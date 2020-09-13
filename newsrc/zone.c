/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// Z_zone.c

#include "quakedef.h"

// 0xc000: 16^3 = 4096; 4096 * 12 = 49152
// i.e. about 48k
#define DYNAMIC_SIZE 0xc000

void Cache_FreeLow(int new_low_hunk);
void Cache_FreeHigh(int new_high_hunk);


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

#define ZONEID 0x1d4a11
#define MINFRAGMENT	64

typedef struct memblock_s {
	int size;  // including the header and possibly tiny fragments
	int tag;  // a tag of 0 is a free block
	int id;  // should be ZONEID
	struct memblock_s *next;
	struct memblock_s *prev;
	int pad;  // pad to 64 bit boundary
} memblock_t;

// only used for the main zone
typedef struct {
	int size;  // total bytes malloced, including header
	memblock_t blocklist;  // start / end cap for linked list
	memblock_t *rover;
} memzone_t;

memzone_t *mainzone;

/*
========================
Z_ClearZone

Creates a memblock and sets up a doubly linked list, where the block points to
the memzone's blocklist, which points to the block
========================
*/
void Z_ClearZone(memzone_t *zone, int size) {
	memblock_t *block;

	// sets the position of the block pointer to just after the memzone object
	block = (memblock_t *)((byte *)zone + sizeof(memzone_t));

	// set the entire zone to one free block
	zone->blocklist.next = block;
	zone->blocklist.prev = block;
	zone->blocklist.tag = 1;	// mark as in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;

	block->prev = &zone->blocklist;
	block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}


/*
========================
Z_Free
========================
*/
void Z_Free(void *ptr) {
	memblock_t *current;
	memblock_t *previous;
	memblock_t *next;

	if (!ptr) {
		Sys_Error("Z_Free: NULL pointer");
	}

	// ptr points to the free space for the memblock (see end of Z_TagMalloc), so
	// we need to include the memblock itself too
	current = (memblock_t *)((byte *)ptr - sizeof(memblock_t));

	if (current->id != ZONEID) {
		Sys_Error("Z_Free: tried to free a pointer without ZONEID");
	}

	if (current->tag == 0) {
		Sys_Error("Z_Free: tried to free an already freed pointer");
	}

	current->tag = 0;  // mark as free

	previous = current->prev;

	if (!previous->tag) {
		// merge with previous free block
		previous->size += current->size;
		previous->next = current->next;
		previous->next->prev = previous;

		if (current == mainzone->rover) {
			mainzone->rover = previous;
		}

		current = previous;
	}

	next = current->next;

	if (!next->tag) {
		// merge the next free block onto the end
		current->size += next->size;
		current->next = next->next;
		current->next->prev = current;

		if (next == mainzone->rover) {
			mainzone->rover = current;
		}
	}
}


/*
========================
Z_Malloc
========================
*/
void *Z_Malloc(int size) {
	void *buf;

	Z_CheckHeap();  // DEBUG
	buf = Z_TagMalloc(size, 1);

	if (!buf) {
		Sys_Error ("Z_Malloc: failed on allocation of %i bytes",size);
	}

	Q_memset(buf, 0, size);

	return buf;
}

void *Z_TagMalloc(int block_size, int tag) {
	int remaining_space;
	memblock_t *base;
	memblock_t *start;
	memblock_t *rover;
	memblock_t *new_block;

	if (!tag) {
		Sys_Error("Z_TagMalloc: tried to use a 0 tag");
	}

	block_size += sizeof(memblock_t);  // account for size of block header
	block_size += 4;  // space for memory trash tester
	block_size = (block_size + 7) & ~7;  // align to 8-byte boundary

	// scan through the block list looking for the first free block
	// of sufficient size
	base = rover = mainzone->rover;
	start = base->prev;

	do {
		if (rover == start) {
			// scanned all the way around the list
			return NULL;
		}

		if (rover->tag) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while (base->tag || base->size < block_size);

	// found a block big enough
	// by default, the size of the last memblock on the list is the remaining
	// amount of the zone
	remaining_space = base->size - block_size;

	if (remaining_space > MINFRAGMENT) {
		// there will be a free fragment after the allocated block
		new_block = (memblock_t *)((byte *)base + block_size );
		new_block->size = remaining_space;
		new_block->tag = 0;			// free block
		new_block->prev = base;
		new_block->id = ZONEID;
		new_block->next = base->next;
		new_block->next->prev = new_block;

		base->next = new_block;
		base->size = block_size;
	}

	base->tag = tag;  // no longer a free block

	mainzone->rover = base->next; // next allocation will start looking here

	base->id = ZONEID;

	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = ZONEID;

	// returns the location in memory of the memblock's free space
	return (void *)((byte *)base + sizeof(memblock_t));
}


/*
========================
Z_Print

unused
========================
*/
void Z_Print(memzone_t *zone) {
	memblock_t *block;

	Con_Printf("zone size: %i  location: %p\n",mainzone->size,mainzone);

	for (block = zone->blocklist.next; ; block = block->next){
		Con_Printf(
				"block: %p    size: %7i    tag: %3i\n",
				block, block->size, block->tag);

		if (block->next == &zone->blocklist) {
			// all blocks have been hit
			break;
		}

		if ( (byte *)block + block->size != (byte *)block->next) {
			Con_Printf("ERROR: block size does not touch the next block\n");
		}

		if ( block->next->prev != block) {
			Con_Printf("ERROR: next block doesn't have proper back link\n");
		}

		if (!block->tag && !block->next->tag) {
			Con_Printf("ERROR: two consecutive free blocks\n");
		}
	}
}


/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap(void) {
	memblock_t *block;

	for (block = mainzone->blocklist.next; ; block = block->next) {
		if (block->next == &mainzone->blocklist) {
			break;  // all blocks have been hit
		}

		if ( (byte *)block + block->size != (byte *)block->next) {
			Sys_Error("Z_CheckHeap: block size does not touch the next block\n");
		}

		if ( block->next->prev != block) {
			Sys_Error("Z_CheckHeap: next block doesn't have proper back link\n");
		}

		if (!block->tag && !block->next->tag) {
			Sys_Error("Z_CheckHeap: two consecutive free blocks\n");
		}
	}
}



/*
==============================================================================

						HUNK

==============================================================================
*/

#define HUNK_SENTINEL 0x1df001ed

typedef struct {
	int sentinel;
	int size;  // including sizeof(hunk_t), -1 = not allocated
	char name[8];
} hunk_t;

byte *hunk_base;
int hunk_size;
int hunk_low_used;
int hunk_high_used;

qboolean hunk_tempactive;
int hunk_tempmark;


void Hunk_Init(void *buf, int size) {
  hunk_base = buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;
}

/*
==============
Hunk_Check

Run consistency and sentinel trashing checks
==============
*/
void Hunk_Check(void) {
	hunk_t *hunk;
	hunk = (hunk_t *)hunk_base;

	while ((byte *)hunk != hunk_base + hunk_low_used) {
		if (hunk->sentinel != HUNK_SENTINEL) {
			Sys_Error("Hunk_Check: trashed sentinel");
		}

		if (hunk->size < 16 || hunk->size + (byte *)hunk - hunk_base > hunk_size) {
			Sys_Error("Hunk_Check: bad size");
		}

		hunk = (hunk_t *)((byte *)hunk + hunk->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print(qboolean all) {
	hunk_t *h;
  hunk_t *next;
  hunk_t *endlow;
  hunk_t *starthigh;
  hunk_t *endhigh;
	int count;
  int sum;
	int totalblocks;
	char name[9];

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunk_t *)hunk_base;
	endlow = (hunk_t *)(hunk_base + hunk_low_used);
	starthigh = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *)(hunk_base + hunk_size);

	Con_Printf("          :%8i total hunk size\n", hunk_size);
	Con_Printf("-------------------------\n");

	while (1) {
		// skip to the high hunk if done with low hunk
		if (h == endlow) {
			Con_Printf("-------------------------\n");
			Con_Printf("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
			Con_Printf("-------------------------\n");
			h = starthigh;
		}

		// if totally done, break
		if (h == endhigh) {
			break;
		}

		// run consistency checks
		if (h->sentinel != HUNK_SENTINEL) {
			Sys_Error("Hunk_Check: trahsed sentinel");
		}

		if (h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size) {
			Sys_Error("Hunk_Check: bad size");
		}

		next = (hunk_t *)((byte *)h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		// print the single block
		memcpy(name, h->name, 8);

		if (all) {
			Con_Printf("%8p :%8i %8s\n",h, h->size, name);
		}

		// print the total
		if (next == endlow || next == endhigh || strncmp(h->name, next->name, 8)) {
			if (!all) {
				Con_Printf("          :%8i %8s (TOTAL)\n",sum, name);
			}

			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName(int size, char *name) {
	hunk_t *h;

#ifdef PARANOID
	Hunk_Check();
#endif

	if (size < 0) {
		Sys_Error("Hunk_Alloc: bad size: %i", size);
	}

	size += sizeof(hunk_t);  // account for size of hunk header
	size = (size + 15) &~ 15;  // align to 16-byte boundary

	if ((hunk_size - hunk_low_used - hunk_high_used) < size) {
		Sys_Error("Hunk_Alloc: failed on %i bytes",size);
	}

	h = (hunk_t *)(hunk_base + hunk_low_used);
	hunk_low_used += size;

	Cache_FreeLow(hunk_low_used);

	memset(h, 0, size);

	h->size = size;
	h->sentinel = HUNK_SENTINEL;
	Q_strncpy(h->name, name, 8);

	return (void *)(h + 1);
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc(int size) {
	return Hunk_AllocName(size, "unknown");
}

int	Hunk_LowMark(void) {
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark) {
	if (mark < 0 || mark > hunk_low_used) {
		Sys_Error("Hunk_FreeToLowMark: bad mark %i", mark);
	}

	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int	Hunk_HighMark(void) {
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark(int mark) {
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	if (mark < 0 || mark > hunk_high_used) {
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);
	}

	memset(hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}


/*
===================
Hunk_HighAllocName
===================
*/
void *Hunk_HighAllocName(int size, char *name) {
	hunk_t *h;

	if (size < 0) {
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);
	}


	if (hunk_tempactive) {
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

#ifdef PARANOID
	Hunk_Check();
#endif

	size += sizeof(hunk_t);  // account for size of hunk header
	size = (size + 15) &~ 15;  // align to 16-byte boundary

	if (hunk_size - hunk_low_used - hunk_high_used < size) {
		Con_Printf("Hunk_HighAlloc: failed on %i bytes\n",size);
		return NULL;
	}

	hunk_high_used += size;
	Cache_FreeHigh(hunk_high_used);

	h = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);

	memset(h, 0, size);
	h->size = size;
	h->sentinel = HUNK_SENTINEL;
	Q_strncpy(h->name, name, 8);

	return (void *)(h + 1);
}


/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void *Hunk_TempAlloc(int size) {
	void *buf;

	size = (size + 15) &~ 15;

	// this is redundant, since Hunk_HighMark does it as well
	// if (hunk_tempactive) {
	// 	Hunk_FreeToHighMark(hunk_tempmark);
	// 	hunk_tempactive = false;
	// }

	hunk_tempmark = Hunk_HighMark();

	buf = Hunk_HighAllocName(size, "temp");

	hunk_tempactive = true;

	return buf;
}



/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s {
	int size;  // including this header
	cache_user_t *user;
	char name[16];
	struct cache_system_s *prev;
	struct cache_system_s *next;
	struct cache_system_s *lru_prev;  // for LRU flushing
	struct cache_system_s *lru_next;  // for LRU flushing
} cache_system_t;

cache_system_t *Cache_TryAlloc(int size, qboolean nobottom);

cache_system_t cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move(cache_system_t *c) {
	cache_system_t *new;

	// we are clearing up space at the bottom, so only allocate it late
	new = Cache_TryAlloc(c->size, true);

	if (new) {
		// Con_Printf("cache_move ok\n");
		Q_memcpy(new + 1, c + 1, c->size - sizeof(cache_system_t));
		new->user = c->user;
		Q_memcpy(new->name, c->name, sizeof(new->name));
		Cache_Free(c->user);
		new->user->data = (void *)(new + 1);
	} else {
		// Con_Printf("cache_move failed\n");
		Cache_Free(c->user);  // tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow(int new_low_hunk) {
	cache_system_t *c;

	while (1) {
		c = cache_head.next;

		if (c == &cache_head) {
			return;  // nothing in cache at all
		}

		if ((byte *)c >= hunk_base + new_low_hunk) {
			return;  // there is space to grow the hunk
		}

		Cache_Move(c);	// reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh(int new_high_hunk) {
	cache_system_t *c;
	cache_system_t *prev;

	prev = NULL;

	while (1) {
		c = cache_head.prev;
		if (c == &cache_head) {
			return;  // nothing in cache at all
		}

		if ((byte *)c + c->size <= hunk_base + hunk_size - new_high_hunk) {
			return;  // there is space to grow the hunk
		}

		if (c == prev) {
			Cache_Free(c->user);	// didn't move out of the way
		} else {
			Cache_Move(c);	// try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU(cache_system_t *cs) {
	if (!cs->lru_next || !cs->lru_prev) {
		Sys_Error("Cache_UnlinkLRU: NULL link");
	}

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = NULL;
}

void Cache_MakeLRU(cache_system_t *cs) {
	if (cs->lru_next || cs->lru_prev) {
		Sys_Error("Cache_MakeLRU: active link");
	}

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t *Cache_TryAlloc(int size, qboolean nobottom) {
	cache_system_t *cs;
	cache_system_t *new;

	// is the cache completely empty?
	if (!nobottom && cache_head.prev == &cache_head) {
		if (hunk_size - hunk_high_used - hunk_low_used < size) {
			Sys_Error("Cache_TryAlloc: %i is greater then free hunk", size);
		}

		new = (cache_system_t *)(hunk_base + hunk_low_used);
		memset(new, 0, sizeof(*new));
		new->size = size;

		cache_head.prev = cache_head.next = new;
		new->prev = new->next = &cache_head;

		Cache_MakeLRU(new);
		return new;
	}

	// search from the bottom up for space
	new = (cache_system_t *)(hunk_base + hunk_low_used);
	cs = cache_head.next;

	do {
		if (!nobottom || cs != cache_head.next) {
			if ((byte *)cs - (byte *)new >= size) {
				// found space
				memset(new, 0, sizeof(*new));
				new->size = size;

				new->next = cs;
				new->prev = cs->prev;
				cs->prev->next = new;
				cs->prev = new;

				Cache_MakeLRU(new);

				return new;
			}
		}

		// continue looking
		new = (cache_system_t *)((byte *)cs + cs->size);
		cs = cs->next;

	} while (cs != &cache_head);

	// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - (byte *)new >= size) {
		memset(new, 0, sizeof(*new));
		new->size = size;

		new->next = &cache_head;
		new->prev = cache_head.prev;
		cache_head.prev->next = new;
		cache_head.prev = new;

		Cache_MakeLRU(new);

		return new;
	}

	return NULL;  // couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush(void) {
	while (cache_head.next != &cache_head) {
		// reclaim the space
		Cache_Free(cache_head.next->user);
	}
}


/*
============
Cache_Print

============
*/
void Cache_Print(void) {
	cache_system_t *cd;

	for (cd = cache_head.next; cd != &cache_head; cd = cd->next) {
		Con_Printf("%8i : %s\n", cd->size, cd->name);
	}
}

/*
============
Cache_Report

============
*/
void Cache_Report(void) {
	Con_DPrintf(
			"%4.1f megabyte data cache\n",
			(hunk_size - hunk_high_used - hunk_low_used) / (float)(1024*1024));
}

/*
============
Cache_Compact

============
*/
void Cache_Compact(void) {
}

/*
============
Cache_Init

============
*/
void Cache_Init(void) {
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand("flush", Cache_Flush);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free(cache_user_t *c) {
	cache_system_t *cs;

	if (!c->data) {
		Sys_Error("Cache_Free: not allocated");
	}

	cs = ((cache_system_t *)c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU(cs);
}



/*
==============
Cache_Check
==============
*/
void* Cache_Check(cache_user_t* c) {
	cache_system_t* cs;

	if (!c->data) {
		return NULL;
	}

	cs = ((cache_system_t*)c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU(cs);
	Cache_MakeLRU(cs);

	return c->data;
}


/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc(cache_user_t *c, int size, char *name) {
	cache_system_t *cs;

	if (c->data) {
		Sys_Error("Cache_Alloc: allready allocated");
	}

	if (size <= 0) {
		Sys_Error("Cache_Alloc: size %i", size);
	}

	size = (size + sizeof(cache_system_t) + 15) & ~15;

	// find memory for it
	while (1) {
		cs = Cache_TryAlloc(size, false);
		if (cs) {
			strncpy (cs->name, name, sizeof(cs->name)-1);
			c->data = (void *)(cs+1);
			cs->user = c;
			break;
		}

		// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head) {
			Sys_Error("Cache_Alloc: out of memory");
		}

		// not enough memory at all
		Cache_Free(cache_head.lru_prev->user);
	}

	return Cache_Check(c);
}



//============================================================================



void Zone_Init() {
	int zone_parm;
	int zonesize = DYNAMIC_SIZE;

	zone_parm = COM_CheckParm("-zone");

	if (zone_parm) {
		if (zone_parm < com_argc - 1) {
			// assume whatever follows -zone is a valid size value
			zonesize = Q_atoi(com_argv[zone_parm + 1]) * 1024;
		} else {
			Sys_Error("Memory_Init: you must specify a size in KB after -zone");
		}
	}

	mainzone = Hunk_AllocName(zonesize, "zone" );
	Z_ClearZone(mainzone, zonesize);
}



/*
========================
Memory_Init
========================
*/
void Memory_Init(void *buf, int size) {
	Hunk_Init(buf, size);
	Cache_Init();
	Zone_Init();
}
