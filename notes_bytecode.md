# QuakeC Bytecode Interpreter

* globals - global variables, as defined in `progdefs.h` `globalvars_t`, plus anything else that's specified as a global variable?
	* What happens when the version number of the QuakeC payload changes? (i.e. I'm running 1.06, the latest I'm aware of is 1.09)
* fields/ents - all the entities, as defined by `entvars_t`?

## Headers
* `pr_comp.h` - all the types for bytecode execution, except `eval_t`
* `progdefs.h` - `globalvars_t` and `entvars_t`, the types shared with QuakeC
* `progs.h` - `eval_t`, `edict_t`, and all edict and execution function definitions

## Source Files
* `pr_edict.c` - program setup and entity handling
* `pr_cmds.c` - definitions of builtin functions (see `pr_builtin` for the whole list)
* `pr_exec.c` - execution of functions

## Types
* `dprograms_t` - header storing all of the offsets and counts for statements, global definitions, field definitions, functions, strings, and global values
* `dfunction_t` - function info
	* `first_statement` - index into `pr_statements`
		* negative values indicate builtins (see `pr_builtin` for the whole list)
	* `parm_start` - index of first parameter in `pr_globals`
	* `locals` - number of parameters + local variables
	* `profile` - number of statements executed in this function, for debugging
		* set during execution
	* `s_name` - index of function name in `pr_strings`
	* `s_file` - index of source file name in `pr_strings`
	* `numparms` - number of parameters
* `ddef_t` - a variable?
	* `type` - `etype_t`
	* `ofs` - seemingly an offset, but never used as such?
		* instead used simply by comparing values with the `ofs` in `pr_globaldefs` or `pr_fielddefs`?
	* `s_name` - index to `pr_strings`
* `dstatement_t` - a single statement within a function
	* `op` - op values from the enum above this type
	* `a`, `b`, `c`, - index into `pr_globals`, which in turn contains `eval_t` values at those indices
* `eval_t` - union of various types:
	* `_float`, `_int`
	* `string` - signed int index, generally to `pr_strings`
		* this value can overflow, leading to segfaults
	* `vector` - three floats
	* `function` - signed int index into `pr_functions`
	* `edict` - byte offset find the right `edict_t` value in `sv.edicts`
* `globalvars_t` - global values shared between QC and C

## Important globals
* `progs` - `dprograms_t*` pointing to `progs.dat` in the hunk
* `pr_functions` - `dfunction_t*` pointing to `progs->ofs_functions` bytes into `progs`
	*
* `pr_strings` - `char*` pointing to `progs->ofs_strings` bytes within `progs`
	* contains all string values the game cares about (over 88k??)
	* first character is null
* `pr_globaldefs` - `ddef_t*` pointing to `progs->ofs_globaldefs` bytes within `progs`
* `pr_fielddefs` - `ddef_t*` pointing to `progs->ofs_fielddefs` bytes within `progs`
* `pr_statements` - `dstatement_t*` pointing to `progs->ofs_statements` bytes within `progs`
* `pr_global_struct` - `globalvars_t*` pointing to `progs->ofs_globals` bytes within `progs`
* `pr_globals` - `pr_global_struct` cast as `float*`
	* cast as `eval_t*` when executing, indexed with statement `a`, `b`, and `c`

## Important functions
* `void PR_LoadProgs()` - loads `progs.dat` into the hunk, sets most or all of the global values, does integrity checking and byte swapping
* `void PR_Init(void)` - registers commands and cvars:
	* commands:
		* `edict` - print edict
		* `edicts` - print al edicts
		* `edictcount` - print the count of edicts, divided into active (not free), view (models), touch (solids), and step (?)
		* `profile` - prints top 10 functions
	* cvars: none of them appear to actually be used

### edict handling functions
* edict manipulation:
	* `edict_t* ED_Alloc()` - finds or allocates a new edict within `sv.edicts`
	* `void ED_Free(edict_t* ed)` - marks edict as free
		* interesting comment: "FIXME: walk all entities and NULL out references to this entity"
	* `char* ED_NewString(char* string)` - allocates the string on the hunk

* finding values:
	* `ED_FindField`, `ED_FindGlobal`, and `ED_FindFunction` - finds the item by walking the relevant `pr_{items}` list comparing with `item->s_name`
	* `eval_t* GetEdictFieldValue(edict_t* ed, char* field)` - basically a cached version of `ED_FindField`

* writing edicts to save file:
	* `void ED_Write(FILE* save_file, edict_t* ed)` - writes an edict to a file
	* `void ED_WriteGlobals(FILE* save_file)` - write all globals in `pr_globaldefs` to a file if `DEF_SAVEGLOBAL` is set on `global_item->type`

* parsing edicts:
	* `void ED_ParseGlobals(char* data)` - called in `Host_Loadgame_f`
	* `qboolean ED_ParseEpair(void* dest_start, ddef_t* key, char* raw_value)` - parses the `raw_value` to the correct `eval_t` type and stores it at `dest_start + key->ofs`
	* `char* ED_ParseEdict(char* data, edict_t* ent)` - like `ED_ParseGlobals`, but
	* `void ED_LoadFromFile(char* data)` - called from `SV_SpawnServer`

## Memory layout

dprograms_t* progs
	the `progs.dat` file loaded into the hunk
dfunction_t* pr_functions
char* pr_strings
ddef_t* pr_globaldefs
ddef_t* pr_fielddefs
dstatement_t* pr_statements
globalvars_t* pr_global_struct, float* pr_globals
int pr_edict_size (in bytes)

## Open Questions
* what is the layout of memory?
	* where are `pr_strings` et al in relation to the hunk?
		* i.e. how does `ED_ParseEpair` work for strings?



typedef struct {
	int ofs_statements;
	int ofs_globaldefs;
	int ofs_fielddefs;
	int ofs_functions;
	int ofs_strings;
	int ofs_globals;
	int entityfields;
} dprograms_t;
