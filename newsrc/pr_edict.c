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
// sv_edict.c -- entity dictionary

#include "quakedef.h"

dprograms_t* progs;
dfunction_t* pr_functions;
char* pr_strings;
ddef_t* pr_fielddefs;
ddef_t* pr_globaldefs;
dstatement_t* pr_statements;
globalvars_t* pr_global_struct;
float* pr_globals;  // same as pr_global_struct
int pr_edict_size;  // in bytes

unsigned short pr_crc;

ddef_t* ED_FieldAtOfs(int ofs);
qboolean ED_ParseEpair(void* dest_start, ddef_t* key, char* raw_value);

cvar_t nomonsters = {"nomonsters", "0"};
cvar_t gamecfg = {"gamecfg", "0"};
cvar_t scratch1 = {"scratch1", "0"};
cvar_t scratch2 = {"scratch2", "0"};
cvar_t scratch3 = {"scratch3", "0"};
cvar_t scratch4 = {"scratch4", "0"};
cvar_t savedgamecfg = {"savedgamecfg", "0", true};
cvar_t saved1 = {"saved1", "0", true};
cvar_t saved2 = {"saved2", "0", true};
cvar_t saved3 = {"saved3", "0", true};
cvar_t saved4 = {"saved4", "0", true};

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
void ED_ClearEdict(edict_t* e) {
	memset(&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t* ED_Alloc(void) {
	int i = svs.maxclients + 1;
	edict_t* e;

	for (; i < sv.num_edicts; i++) {
		e = EDICT_NUM(i);

		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || sv.time - e->freetime > 0.5)) {
			ED_ClearEdict(e);
			return e;
		}
	}

	if (i == MAX_EDICTS) {
		Sys_Error("ED_Alloc: no free edicts");
	}

	sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict(e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free(edict_t* ed) {
	// unlink from world bsp
	SV_UnlinkEdict(ed);

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy(vec3_origin, ed->v.origin);
	VectorCopy(vec3_origin, ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs

This is used in PR_PrintStatement() for logging
============
*/
ddef_t* ED_GlobalAtOfs(int ofs) {
	for (int i = 0; i < progs->numglobaldefs; i++) {
		ddef_t* def = &pr_globaldefs[i];

		if (def->ofs == ofs) {
			return def;
		}
	}

	return NULL;
}

/*
============
ED_FieldAtOfs

This is used in PR_PrintStatement() for logging
============
*/
ddef_t* ED_FieldAtOfs (int ofs) {
	for (int i = 0; i < progs->numfielddefs; i++) {
		ddef_t* def = &pr_fielddefs[i];

		if (def->ofs == ofs) {
			return def;
		}
	}

	return NULL;
}

/*
============
ED_FindField
============
*/
ddef_t* ED_FindField(char* name) {
	for (int i = 0; i < progs->numfielddefs; i++) {
		ddef_t* def = &pr_fielddefs[i];

		if (!strcmp(pr_strings + def->s_name, name)) {
			return def;
		}
	}

	return NULL;
}


/*
============
ED_FindGlobal
============
*/
ddef_t* ED_FindGlobal(char* name) {
	for (int i = 0; i < progs->numglobaldefs; i++) {
		ddef_t* def = &pr_globaldefs[i];

		if (!strcmp(pr_strings + def->s_name, name)) {
			return def;
		}
	}

	return NULL;
}


/*
============
ED_FindFunction
============
*/
dfunction_t* ED_FindFunction(char* name) {
	for (int i = 0; i < progs->numfunctions; i++) {
		dfunction_t* func = &pr_functions[i];

		if (!strcmp(pr_strings + func->s_name, name)) {
			return func;
		}
	}

	return NULL;
}



#define MAX_FIELD_LEN 64
#define GEFV_CACHESIZE 2

typedef struct {
	ddef_t* pcache;
	char field[MAX_FIELD_LEN];
} gefv_cache_t;

static gefv_cache_t gefvCache[GEFV_CACHESIZE] = {
	{NULL, ""},
	{NULL, ""}
};

/*
============
GetEdictFieldValue
============
*/
eval_t* GetEdictFieldValue(edict_t* ed, char* field) {
	ddef_t *def = NULL;

	// first, check if the variable is in the cache
	for (int i = 0; i < GEFV_CACHESIZE; i++) {
		if (!strcmp(field, gefvCache[i].field)) {
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	// if it's not, find it the hard way
	def = ED_FindField(field);
	static int rep = 0;

	if (strlen(field) < MAX_FIELD_LEN) {
		gefvCache[rep].pcache = def;
		strcpy(gefvCache[rep].field, field);
		rep ^= 1;
	}

Done:
	if (!def) {
		return NULL;
	}

	return (eval_t*)((char*)&ed->v + def->ofs * 4);
}


/*
============
PR_ValueString

For debugging
Returns a string describing *data in a type specific manner
=============
*/
char* PR_ValueString(etype_t type, eval_t* val) {
	static char line[256];
	ddef_t* def;
	dfunction_t* function;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			sprintf(line, "%s", pr_strings + val->string);
			break;

		case ev_entity:
			sprintf(line, "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
			break;

		case ev_function:
			function = pr_functions + val->function;
			sprintf(line, "%s()", pr_strings + function->s_name);
			break;

		case ev_field:
			def = ED_FieldAtOfs ( val->_int );
			sprintf(line, ".%s", pr_strings + def->s_name);
			break;

		case ev_void:
			sprintf(line, "void");
			break;

		case ev_float:
			sprintf(line, "%5.1f", val->_float);
			break;

		case ev_vector:
			sprintf(
					line,
					"'%5.1f %5.1f %5.1f'",
					val->vector[0],
					val->vector[1],
					val->vector[2]);
			break;

		case ev_pointer:
			sprintf(line, "pointer");
			break;

		default:
			sprintf(line, "bad type %i", type);
			break;
	}

	return line;
}

/*
============
PR_UglyValueString

For debugging
Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char* PR_UglyValueString(etype_t type, eval_t* val) {
	static char line[256];
	ddef_t* def;
	dfunction_t* f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			sprintf(line, "%s", pr_strings + val->string);
			break;

		case ev_entity:
			sprintf(line, "%i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
			break;

		case ev_function:
			f = pr_functions + val->function;
			sprintf(line, "%s", pr_strings + f->s_name);
			break;

		case ev_field:
			def = ED_FieldAtOfs(val->_int);
			sprintf(line, "%s", pr_strings + def->s_name);
			break;

		case ev_void:
			sprintf(line, "void");
			break;

		case ev_float:
			sprintf(line, "%f", val->_float);
			break;

		case ev_vector:
			sprintf(line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
			break;

		default:
			sprintf(line, "bad type %i", type);
			break;
	}

	return line;
}

/*
============
PR_GlobalString

For debugging
Returns a string with a description and the contents of a global,
padded to 30 field width
============
*/
char* PR_GlobalString(int ofs) {
	char* value_string;
	static char line[128];

	void* val = (void*)&pr_globals[ofs];
	ddef_t* def = ED_GlobalAtOfs(ofs);

	if (!def) {
		sprintf(line, "%i (?!?)", ofs);
	} else {
		value_string = PR_ValueString(def->type, val);
		sprintf(line, "%i (%s)%s", ofs, pr_strings + def->s_name, value_string);
	}

	int i = strlen(line);

	for (; i < 30; i++) {
		strcat(line, " ");
	}

	strcat(line, " ");

	return line;
}

char* PR_GlobalStringNoContents(int ofs) {
	static char line[128];

	ddef_t* def = ED_GlobalAtOfs(ofs);

	if (!def) {
		sprintf(line, "%i(?!?)", ofs);
	} else {
		sprintf(line, "%i(%s)", ofs, pr_strings + def->s_name);
	}

	int i = strlen(line);

	for (; i < 20; i++) {
		strcat(line, " ");
	}

	strcat(line, " ");

	return line;
}



int type_size[8] = {
	1,                    // void
	sizeof(string_t) / 4, // string
	1,                    // float
	3,                    // vector
	1,                    // entity
	1,                    // field
	sizeof(func_t) / 4,   // function
	sizeof(void*) / 4     // pointer
};

/*
=============
ED_Print

For debugging
=============
*/
void ED_Print(edict_t* ed) {
	if (ed->free) {
		Con_Printf("FREE\n");
		return;
	}

	Con_Printf("\nEDICT %i:\n", NUM_FOR_EDICT(ed));

	for (int i = 1; i < progs->numfielddefs; i++) {
		ddef_t* d = &pr_fielddefs[i];
		char* name = pr_strings + d->s_name;

		if (name[strlen(name) - 2] == '_') {
			// skip _x, _y, _z vars
			continue;
		}

		int* v = (int*)((char*)&ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		int type = d->type & ~DEF_SAVEGLOBAL;
		int j = 0;

		for (; j < type_size[type]; j++) {
			if (v[j]) {
				break;
			}
		}

		if (j == type_size[type]) {
			continue;
		}

		Con_Printf("%s", name);

		int l = strlen(name);

		while (l++ < 15) {
			Con_Printf(" ");
		}

		Con_Printf("%s\n", PR_ValueString(d->type, (eval_t*)v));
	}
}

void ED_PrintNum(int ent) {
	ED_Print(EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts(void) {
	Con_Printf("%i entities\n", sv.num_edicts);

	for (int i = 0; i < sv.num_edicts; i++) {
		ED_PrintNum(i);
	}
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edict
=============
*/
void ED_PrintEdict_f(void) {
	int i = Q_atoi(Cmd_Argv(1));

	if (i >= sv.num_edicts) {
		Con_Printf("Bad edict number\n");
		return;
	}

	ED_PrintNum(i);
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count(void) {
	int active = 0;
	int models = 0;
	int solid = 0;
	int step = 0;

	for (int i = 0; i < sv.num_edicts; i++) {
		edict_t* ent = EDICT_NUM(i);

		if (ent->free) {
			continue;
		}

		active++;

		if (ent->v.solid) {
			solid++;
		}

		if (ent->v.model) {
			models++;
		}

		if (ent->v.movetype == MOVETYPE_STEP) {
			step++;
		}
	}

	Con_Printf("num_edicts: %3i\n", sv.num_edicts);
	Con_Printf("active:     %3i\n", active);
	Con_Printf("view:       %3i\n", models);
	Con_Printf("touch:      %3i\n", solid);
	Con_Printf("step:       %3i\n", step);
}

/*
=============
ED_Write

For savegames
=============
*/
void ED_Write(FILE* save_file, edict_t* ed) {
	fprintf(save_file, "{\n");

	if (ed->free) {
		fprintf(save_file, "}\n");
		return;
	}

	for (int i = 1; i < progs->numfielddefs; i++) {
		ddef_t* d = &pr_fielddefs[i];
		char* name = pr_strings + d->s_name;

		if (name[strlen(name)-2] == '_') {
			// skip _x, _y, _z vars
			continue;
		}

		int* v = (int *)((char*)&ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		int type = d->type & ~DEF_SAVEGLOBAL;
		int j = 0;

		for (; j < type_size[type]; j++) {
			if (v[j]) {
				break;
			}
		}

		if (j == type_size[type]) {
			continue;
		}

		fprintf(save_file, "\"%s\" ", name);
		fprintf(save_file, "\"%s\"\n", PR_UglyValueString(d->type, (eval_t*)v));
	}

	fprintf(save_file, "}\n");
}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void ED_WriteGlobals(FILE* save_file) {
	fprintf(save_file, "{\n");

	for (int i = 0; i < progs->numglobaldefs; i++) {
		ddef_t* def = &pr_globaldefs[i];
		int type = def->type;

		if (!(def->type & DEF_SAVEGLOBAL)) {
			continue;
		}

		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string &&
				type != ev_float &&
				type != ev_entity) {
			continue;
		}

		char* name = pr_strings + def->s_name;

		fprintf(save_file, "\"%s\" ", name);
		fprintf(save_file, "\"%s\"\n", PR_UglyValueString(type, (eval_t*)&pr_globals[def->ofs]));
	}

	fprintf(save_file, "}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals(char* data) {
	char keyname[64];

	while (1) {
		// parse key
		data = COM_Parse(data);

		// finished reading globals
		if (com_token[0] == '}') {
			break;
		}

		if (!data) {
			Sys_Error("ED_ParseEntity: EOF without closing brace");
		}

		strcpy(keyname, com_token);

		// parse value
		data = COM_Parse(data);

		if (!data) {
			Sys_Error("ED_ParseEntity: EOF without closing brace");
		}

		if (com_token[0] == '}') {
			Sys_Error("ED_ParseEntity: closing brace without data");
		}

		ddef_t* key = ED_FindGlobal(keyname);

		if (!key) {
			Con_Printf("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair((void*)pr_globals, key, com_token)) {
			Host_Error("ED_ParseGlobals: parse error");
		}
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
char* ED_NewString(char* string) {
	int length = strlen(string) + 1;
	char* new_string = Hunk_Alloc(length);
	char* position = new_string;

	for (int i = 0; i < length; i++) {
		if (string[i] == '\\' && i < length - 1) {
			i++;

			if (string[i] == 'n') {
				*position++ = '\n';
			} else {
				*position++ = '\\';
			}
		} else {
			*position++ = string[i];
		}
	}

	return new_string;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
qboolean ED_ParseEpair(void* dest_start, ddef_t* key, char* raw_value) {
	char vector_buffer[128];
	char* vector_float_start;
	char* vector_float_end;
	ddef_t* def;
	dfunction_t* func;

	void* destination = (void*)((int*)dest_start + key->ofs);

	switch (key->type & ~DEF_SAVEGLOBAL) {
		case ev_string:
			*(string_t*)destination = ED_NewString(raw_value) - pr_strings;
			break;

		case ev_float:
			*(float*)destination = atof(raw_value);
			break;

		case ev_vector:
			// raw_value is three floats separated by spaces
			strcpy(vector_buffer, raw_value);
			vector_float_start = vector_buffer;
			vector_float_end = vector_buffer;

			for (int i = 0; i < 3; i++) {
				// read float until end of value
				while (*vector_float_end && *vector_float_end != ' ') {
					vector_float_end++;
				}

				// vector_float_end is now at the space, put a null there
				*vector_float_end = 0;

				// w points at the start of the value, which is now null terminated
				((float *)destination)[i] = atof(vector_float_start);

				// move on to the next value
				vector_float_start = vector_float_end + 1;
				vector_float_end = vector_float_end + 1;
			}

			break;

		case ev_entity:
			// raw_value is an index to an edict
			*(int*)destination = EDICT_TO_PROG(EDICT_NUM(atoi(raw_value)));
			break;

		case ev_field:
			// raw_value is an index to a field
			def = ED_FindField(raw_value);

			if (!def) {
				Con_Printf("Can't find field %s\n", raw_value);
				return false;
			}

			*(int*)destination = G_INT(def->ofs);
			break;

		case ev_function:
			// raw_value is an index to a function
			func = ED_FindFunction(raw_value);

			if (!func) {
				Con_Printf("Can't find function %s\n", raw_value);
				return false;
			}

			*(func_t*)destination = func - pr_functions;
			break;

		default:
			break;
	}

	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char* ED_ParseEdict(char* data, edict_t* ent) {
	char keyname[256];

	qboolean init = false;
	qboolean anglehack;

	// clear it
	if (ent != sv.edicts) {
		// hack?
		ED_ClearEdict(ent);
	}

	// go through all the dictionary pairs
	while (1) {
		// parse key
		data = COM_Parse(data);

		if (com_token[0] == '}') {
			break;
		}

		if (!data) {
			Sys_Error("ED_ParseEntity: EOF without closing brace");
		}

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(com_token, "angle")) {
			strcpy(com_token, "angles");
			anglehack = true;
		} else {
			anglehack = false;
		}

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light")) {
			// hack for single light def
			strcpy(com_token, "light_lev");
		}

		strcpy(keyname, com_token);

		// another hack to fix heynames with trailing spaces
		int n = strlen(keyname);

		while (n && keyname[n - 1] == ' ') {
			keyname[n - 1] = 0;
			n--;
		}

		// parse value
		data = COM_Parse(data);

		if (!data) {
			Sys_Error("ED_ParseEntity: EOF without closing brace");
		}

		if (com_token[0] == '}') {
			Sys_Error("ED_ParseEntity: closing brace without data");
		}

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_') {
			continue;
		}

		ddef_t* key = ED_FindField(keyname);

		if (!key) {
			Con_Printf("'%s' is not a field\n", keyname);
			continue;
		}

		if (anglehack) {
			char temp[32];
			strcpy(temp, com_token);
			sprintf(com_token, "0 %s 0", temp);
		}

		if (!ED_ParseEpair((void *)&ent->v, key, com_token)) {
			Host_Error("ED_ParseEdict: parse error");
		}
	}

	if (!init) {
		ent->free = true;
	}

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/

void ED_LoadFromFile(char* data) {
	edict_t* ent = NULL;
	int inhibit = 0;
	pr_global_struct->time = sv.time;

	// parse ents
	while (1) {
		// parse the opening brace
		data = COM_Parse(data);

		if (!data) {
			break;
		}

		if (com_token[0] != '{') {
			Sys_Error("ED_LoadFromFile: found %s when expecting {", com_token);
		}

		if (!ent) {
			ent = EDICT_NUM(0);
		} else {
			ent = ED_Alloc();
		}

		data = ED_ParseEdict(data, ent);

		// remove things from different skill levels or deathmatch
		if (deathmatch.value) {
			if (((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) {
				ED_Free(ent);
				inhibit++;
				continue;
			}
		} else if ((current_skill == 0 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD))) {
			ED_Free(ent);
			inhibit++;
			continue;
		}

		// immediately call spawn function
		if (!ent->v.classname) {
			Con_Printf("No classname for:\n");
			ED_Print(ent);
			ED_Free(ent);
			continue;
		}

		// look for the spawn function
		dfunction_t* function = ED_FindFunction(pr_strings + ent->v.classname);

		if (!function) {
			Con_Printf("No spawn function for:\n");
			ED_Print(ent);
			ED_Free(ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram(function - pr_functions);
	}

	Con_DPrintf("%i entities inhibited\n", inhibit);
}


/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs(void) {
	// flush the non-C variable lookup cache
	for (int i = 0; i < GEFV_CACHESIZE; i++) {
		gefvCache[i].field[0] = 0;
	}

	CRC_Init(&pr_crc);

	progs = (dprograms_t*)COM_LoadHunkFile("progs.dat");

	if (!progs) {
		Sys_Error("PR_LoadProgs: couldn't load progs.dat");
	}

	Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

	for (int i = 0; i < com_filesize; i++) {
		CRC_ProcessByte(&pr_crc, ((byte *)progs)[i]);
	}

	// byte swap the header
	for (int i = 0; i < sizeof(*progs) / 4; i++) {
		((int*)progs)[i] = LittleLong(((int*)progs)[i]);
	}

	if (progs->version != PROG_VERSION) {
		Sys_Error(
				"progs.dat has wrong version number (%i should be %i)",
				progs->version,
				PROG_VERSION);
	}

	if (progs->crc != PROGHEADER_CRC) {
		Sys_Error("progs.dat system vars have been modified, progdefs.h is out of date");
	}

	printf("progs layout: \n");
	printf("  ofs_statements: %d\n", progs->ofs_statements);
	printf("  ofs_globaldefs: %d\n", progs->ofs_globaldefs);
	printf("  ofs_fielddefs: %d\n", progs->ofs_fielddefs);
	printf("  ofs_functions: %d\n", progs->ofs_functions);
	printf("  ofs_strings: %d\n", progs->ofs_strings);
	printf("  ofs_globals: %d\n", progs->ofs_globals);
	printf("  entityfields: %d\n", progs->entityfields);

	pr_functions = (dfunction_t*)((byte*)progs + progs->ofs_functions);
	pr_strings = (char*)progs + progs->ofs_strings;
	pr_globaldefs = (ddef_t*)((byte*)progs + progs->ofs_globaldefs);
	pr_fielddefs = (ddef_t*)((byte*)progs + progs->ofs_fielddefs);
	pr_statements = (dstatement_t*)((byte*)progs + progs->ofs_statements);

	pr_global_struct = (globalvars_t*)((byte*)progs + progs->ofs_globals);
	pr_globals = (float*)pr_global_struct;

	pr_edict_size = progs->entityfields * 4 + sizeof(edict_t) - sizeof(entvars_t);

	printf("pr pointers layout:\n");
	printf("  progs: %lu\n", progs);
	printf("  pr_functions: %lu\n", pr_functions);
	printf("  pr_strings: %lu\n", pr_strings);
	printf("  pr_globaldefs: %lu\n", pr_globaldefs);
	printf("  pr_fielddefs: %lu\n", pr_fielddefs);
	printf("  pr_statements: %lu\n", pr_statements);
	printf("  pr_global_struct: %lu\n", pr_global_struct);
	printf("  pr_globals: %lu\n", pr_globals);
	printf("  pr_edict_size: %d\n", pr_edict_size);

	// byte swap the lumps
	for (int i = 0; i < progs->numstatements; i++) {
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (int i = 0; i < progs->numfunctions; i++) {
		pr_functions[i].first_statement = LittleLong(pr_functions[i].first_statement);
		pr_functions[i].parm_start = LittleLong(pr_functions[i].parm_start);
		pr_functions[i].s_name = LittleLong(pr_functions[i].s_name);
		pr_functions[i].s_file = LittleLong(pr_functions[i].s_file);
		pr_functions[i].numparms = LittleLong(pr_functions[i].numparms);
		pr_functions[i].locals = LittleLong(pr_functions[i].locals);
	}

	for (int i = 0; i < progs->numglobaldefs; i++) {
		pr_globaldefs[i].type = LittleShort(pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort(pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong(pr_globaldefs[i].s_name);
	}

	for (int i = 0; i < progs->numfielddefs; i++) {
		pr_fielddefs[i].type = LittleShort(pr_fielddefs[i].type);

		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL) {
			Sys_Error("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		}

		pr_fielddefs[i].ofs = LittleShort(pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong(pr_fielddefs[i].s_name);
	}

	for (int i = 0; i < progs->numglobals; i++) {
		((int*)pr_globals)[i] = LittleLong(((int*)pr_globals)[i]);
	}
}


/*
===============
PR_Init
===============
*/
void PR_Init(void) {
	Cmd_AddCommand("edict", ED_PrintEdict_f);
	Cmd_AddCommand("edicts", ED_PrintEdicts);
	Cmd_AddCommand("edictcount", ED_Count);
	Cmd_AddCommand("profile", PR_Profile_f);
	Cvar_RegisterVariable(&nomonsters);
	Cvar_RegisterVariable(&gamecfg);
	Cvar_RegisterVariable(&scratch1);
	Cvar_RegisterVariable(&scratch2);
	Cvar_RegisterVariable(&scratch3);
	Cvar_RegisterVariable(&scratch4);
	Cvar_RegisterVariable(&savedgamecfg);
	Cvar_RegisterVariable(&saved1);
	Cvar_RegisterVariable(&saved2);
	Cvar_RegisterVariable(&saved3);
	Cvar_RegisterVariable(&saved4);
}


edict_t* EDICT_NUM(int n) {
	if (n < 0 || n >= sv.max_edicts) {
		Sys_Error("EDICT_NUM: bad number %i", n);
	}

	return (edict_t*)((byte*)sv.edicts + (n) * pr_edict_size);
}

int NUM_FOR_EDICT(edict_t* e) {
	int b;

	b = (byte*)e - (byte*)sv.edicts;
	b = b / pr_edict_size;

	if (b < 0 || b >= sv.num_edicts) {
		Sys_Error("NUM_FOR_EDICT: bad pointer");
	}

	return b;
}
