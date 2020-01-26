# Quake Notes

## Global Variables and cvars

* common
	* `char* com_cmdline` (cvar) - all command line arguments that can fit within 255 characters, space delimited
	* `int com_argc` - MAX_NUM_ARGVS or the total number of arguments, whichever is smaller
	* `char** com_argv` - basically a copy of argv, with " " as the final element



## Important Prefixes

* COM - common
* Q - mostly library replacements
    * memset, memcpy, memcmp
    * strcpy, strcat, strcmp, strcasecmp, atoi, atof
    * log2 (in mathlib)
* Host - host methods
* SV - server methods
* Cbuf - command buffer
* Cmd - command execution
* Key - key binding and event handling
* IN - mouse, or other input devices
* Con - console
* V - view (player eye positioning)
* Chase - chase camera mode
* W - wad
* PR - edicts?
* Mod - models
* NET - network?
* Con - console
* M - menu
* SCR - screen
* Memory
	* Z - zone management
	* Hunk - hunk management
	* Cache - cache management
* SZ - sizebuf handling
* MSG - message handling?  for networking?

## Memory Management
is this right?

--- top ---
video buffer
z buffer
surface cache
<-- high hunk reset buffer held by video
high hunk allocations
<-- high hunk used
cacheable memory
<-- low hunk used
client and server low hunk allocations
<-- reset point held by host
startup hunk allocations
zone block (48k)
--- bottom ---

### Hunk
```
typedef struct {
	int sentinel;
	int size;  // including sizeof(hunk_t), -1 = not allocated
	char name[8];
} hunk_t;
```

hunk_base - pointer to the memory allocated to the hunk
hunk_size - size in bytes of hunk_base
hunk_low_used - amount of low hunk used in bytes
hunk_high_used - amount of high hunk used in bytes

Low and high allocatable spaces behave the same.

Items can be temporarily allocated to the high hunk, but they will be deallocated as soon as anything tries to allocate to the high hunk

### Zone
```
typedef struct memblock_s {
	int size;  // including the header and possibly tiny fragments
	int tag;  // a tag of 0 is a free block
	int id;  // should be ZONEID
	struct memblock_s *next;
	struct memblock_s *prev;
	int pad;  // pad to 64 bit boundary
} memblock_t;
```

Doubly linked list of memblock_t nodes, living within the first allocated lower portion of the hunk.  The data is stored in the space in memory just after the memblock_t for that data.

### Cache
```
typedef struct cache_system_s {
	int size;  // including this header
	cache_user_t *user;
	char name[16];
	struct cache_system_s *prev;
	struct cache_system_s *next;
	struct cache_system_s *lru_prev;  // for LRU flushing
	struct cache_system_s *lru_next;  // for LRU flushing
} cache_system_t;
```

Lives between the high and low hunk portions, can be freed from to provide additional space on either end
Doubly linked list, LRU

TODO: investigate further



## Filesystem
```
// in memory
typedef struct {
	char name[MAX_QPATH];
	int filepos;
	int filelen;
} packfile_t;

typedef struct pack_s {
	char filename[MAX_OSPATH];
	int handle;
	int numfiles;
	packfile_t *files;
} pack_t;

// on disk
typedef struct {
	char name[56];
	int filepos;
	int filelen;
} dpackfile_t;

typedef struct {
	char id[4];
	int dirofs;
	int dirlen;
} dpackheader_t;

typedef struct searchpath_s {
	char    filename[MAX_OSPATH];
	pack_t  *pack;  // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;
```

pak file on disk begins with `dpackheader_t`, followed by `dpackfile_t`s at `header.dirofs`
when read into memory, the pak is represented by `pack_t`, which has many `packfile_t`s

* `COM_Path_f` - path command, prints all of `com_searchpaths`
* `COM_WriteFile`
* `COM_CreatePath` - how does this work?
* `COM_CopyFile` - copies a file over the network?
* `COM_FindFile` - searches for a file in `com_searchpaths`
	* if the current item is a pakfile, it finds or creates the file within it
	* otherwise, ???
	* if file not found, all -1s returned
* `COM_OpenFile` - calls `COM_FindFile` with fd
	* `COM_LoadFile` - calls `COM_OpenFile`
		* `COM_LoadHunkFile` - calls `COM_LoadFile` and puts it in the hunk
		* `COM_LoadTempFile` - calls `COM_LoadFile` and puts it in the hunk as a temporary
		* `COM_LoadCacheFile` - calls `COM_LoadFile` and puts it in the cache
		* `COM_LoadStackFile` - calls `COM_LoadFile` and puts it in the zone (if small enough)
* `COM_FOpenFile` - falls `COM_FindFile` with FILE
* `COM_CloseFile`
* `COM_LoadPackFile`
	* opens the packfile
	* reads its header, makes sure it starts with "PACK"
	* allocates the `packfile_t`s on the hunk
	* verifies the file integrity with CRC (?) (only matters for unregistered versions)
	* allocates the `pack_t` on the hunk, pointing at the `packfile_t`s
* `COM_AddGameDirectory`
	* adds the directory to `com_searchpaths`
	* calls `COM_LoadPackFile` for each pakfile in the directory
* `COM_InitFilesystem`
	* checks for `basedir` argv, loads that or the default
	* does the same for `cachedir`, if none, sets none
	* `COM_AddGameDirectory` basedir
		* does the same for `hipnotic`, `rogue`, and `game` flags
	* allows player to override `com_searchpaths` with `path` flag



## The Flow of Control

* See `notes_init.md` for initialization
* See `notes_loop.md` for the game loop



## areas of further interest:

* cvars
* id386 - 32 bit x86 code
* FPS_20 - experimental 20 fps server

```c
// common.h
#if !defined BYTE_DEFINED
typedef unsigned char	   byte;
#define BYTE_DEFINED 1
#endif

#undef true
#undef false

typedef enum {false, true}  qboolean;
```

```c
// zone.c
#define HUNK_SENTINEL   0x1df001ed

typedef struct
{
	int	 sentinel;
	int	 size;	   // including sizeof(hunk_t), -1 = not allocated
	char	name[8];
} hunk_t;

byte	*hunk_base;
int	 hunk_size;

int	 hunk_low_used;
int	 hunk_high_used;
```

```c
// cvar.h
typedef struct cvar_s
{
	char	*name;
	char	*string;
	qboolean archive;	   // set to true to cause it to be saved to vars.rc
	qboolean server;		// notifies players when changed
	float   value;
	struct cvar_s *next;
} cvar_t;
```

```c
// quakedef.h
typedef struct
{
	char	*basedir;
	char	*cachedir;	  // for development over ISDN lines
	int	 argc;
	char	**argv;
	void	*membase;
	int	 memsize;
} quakeparms_t;
```

```c
// zone.c
// what the hell is this doing
size = sizeof(hunk_t) + ((size+15)&~15);
```

```c
// host.c
// does this even do anything? sets the hunk low mark?
Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
```
