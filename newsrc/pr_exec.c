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

#include "quakedef.h"


/*

*/

typedef struct {
	int s;
	dfunction_t* f;
} prstack_t;

#define MAX_STACK_DEPTH 32
prstack_t pr_stack[MAX_STACK_DEPTH];
int pr_depth;

#define LOCALSTACK_SIZE 2048
int localstack[LOCALSTACK_SIZE];
int localstack_used;


qboolean pr_trace;  // whether to print statements with PR_PrintStatement()
dfunction_t* pr_xfunction;
int pr_xstatement;


int pr_argc;

char* pr_opnames[] = {
	"DONE",

	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",

	"DIV",

	"ADD_F",
	"ADD_V",

	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_E",
	"EQ_FNC",

	"NE_F",
	"NE_V",
	"NE_S",
	"NE_E",
	"NE_FNC",

	"LE",
	"GE",
	"LT",
	"GT",

	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",

	"ADDRESS",

	"STORE_F",
	"STORE_V",
	"STORE_S",
	"STORE_ENT",
	"STORE_FLD",
	"STORE_FNC",

	"STOREP_F",
	"STOREP_V",
	"STOREP_S",
	"STOREP_ENT",
	"STOREP_FLD",
	"STOREP_FNC",

	"RETURN",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_ENT",
	"NOT_FNC",

	"IF",
	"IFNOT",

	"CALL0",
	"CALL1",
	"CALL2",
	"CALL3",
	"CALL4",
	"CALL5",
	"CALL6",
	"CALL7",
	"CALL8",

	"STATE",

	"GOTO",

	"AND",
	"OR",

	"BITAND",
	"BITOR"
};

char* PR_GlobalString(int ofs);
char* PR_GlobalStringNoContents(int ofs);


//=============================================================================

/*
=================
PR_PrintStatement

pretty-print a single statement from a function
=================
*/
void PR_PrintStatement(dstatement_t* statement) {
	if ((unsigned)statement->op < sizeof(pr_opnames) / sizeof(pr_opnames[0])) {
		Con_Printf("%s ", pr_opnames[statement->op]);

		for (int i = strlen(pr_opnames[statement->op]); i<10 ; i++) {
			Con_Printf(" ");
		}
	}

	if (statement->op == OP_IF || statement->op == OP_IFNOT) {
		Con_Printf("a: %sbranch b: %i", PR_GlobalString(statement->a), statement->b);
	} else if (statement->op == OP_GOTO) {
		Con_Printf("branch %i", statement->a);
	} else if ((unsigned)(statement->op - OP_STORE_F) < 6) {
		// op is one of OP_STORE_(F|V|S|ENT|FLD|FNC)
		Con_Printf("a: %s", PR_GlobalString(statement->a));
		Con_Printf("b: %s", PR_GlobalStringNoContents(statement->b));
	} else {
		if (statement->a) {
			Con_Printf("a: %s", PR_GlobalString(statement->a));
		}

		if (statement->b) {
			Con_Printf("b: %s", PR_GlobalString(statement->b));
		}

		if (statement->c) {
			Con_Printf("c: %s", PR_GlobalStringNoContents(statement->c));
		}
	}

	Con_Printf("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace(void) {
	if (pr_depth == 0) {
		Con_Printf("<NO STACK>\n");
		return;
	}

	pr_stack[pr_depth].f = pr_xfunction;

	for (int i = pr_depth; i >= 0; i--) {
		dfunction_t* function = pr_stack[i].f;

		if (!function) {
			Con_Printf("<NO FUNCTION>\n");
		} else {
			Con_Printf(
					"%12s : %s\n",
					pr_strings + function->s_file,
					pr_strings + function->s_name);
		}
	}
}


/*
============
PR_Profile_f
============
*/
void PR_Profile_f(void) {
	int num = 0;
	dfunction_t* best;

	do {
		int max = 0;
		best = NULL;

		for (int i = 0; i < progs->numfunctions; i++) {
			dfunction_t* function = &pr_functions[i];

			if (function->profile > max) {
				max = function->profile;
				best = function;
			}
		}

		if (best) {
			if (num < 10) {
				Con_Printf("%7i %s\n", best->profile, pr_strings + best->s_name);
			}

			num++;
			best->profile = 0;
		}
	} while (best);
}


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError(char *error, ...) {
	va_list argptr;
	char string[1024];

	va_start(argptr,error);
	vsprintf(string,error,argptr);
	va_end(argptr);

	PR_PrintStatement(pr_statements + pr_xstatement);
	PR_StackTrace();
	Con_Printf("%s\n", string);

	pr_depth = 0;  // dump the stack so host_error can shutdown functions

	Host_Error("Program error");
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
int PR_EnterFunction(dfunction_t* function) {
	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;

	if (pr_depth >= MAX_STACK_DEPTH) {
		PR_RunError("stack overflow");
	}

	// save off any locals that the new function steps on
	int num_function_locals = function->locals;

	if (localstack_used + num_function_locals > LOCALSTACK_SIZE) {
		PR_RunError("PR_ExecuteProgram: locals stack overflow\n");
	}

	for (int i = 0; i < num_function_locals; i++) {
		localstack[localstack_used + i] = ((int*)pr_globals)[function->parm_start + i];
	}

	localstack_used += num_function_locals;

	// copy parameters
	int function_parm_offset = function->parm_start;

	for (int i = 0; i < function->numparms; i++) {
		for (int j = 0; j < function->parm_size[i]; j++) {
			size_t offset = OFS_PARM0 + i * 3 + j;
			((int*)pr_globals)[function_parm_offset] = ((int*)pr_globals)[offset];
			function_parm_offset++;
		}
	}

	pr_xfunction = function;

	// offset the statement++ in PR_ExecuteProgram
	return function->first_statement - 1;
}

/*
====================
PR_LeaveFunction
====================
*/
int PR_LeaveFunction(void) {
	if (pr_depth <= 0) {
		Sys_Error ("prog stack underflow");
	}

// restore locals from the stack
	int num_function_locals = pr_xfunction->locals;
	localstack_used -= num_function_locals;

	if (localstack_used < 0) {
		PR_RunError("PR_ExecuteProgram: locals stack underflow\n");
	}

	for (int i = 0; i < num_function_locals; i++) {
		((int*)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used + i];
	}

	// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;

	return pr_stack[pr_depth].s;
}


/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteProgram(func_t func_index) {
	dfunction_t* new_function;
	int i;
	edict_t* ed;
	eval_t* ptr;

	if (!func_index || func_index >= progs->numfunctions) {
		if (pr_global_struct->self) {
			ED_Print(PROG_TO_EDICT(pr_global_struct->self));
		}

		Host_Error("PR_ExecuteProgram: NULL function");
	}

	dfunction_t* function = &pr_functions[func_index];

	int runaway = 100000;
	pr_trace = false;

	// make a stack frame
	int exitdepth = pr_depth;
	int statement_counter = PR_EnterFunction(function);

	while (1) {
		statement_counter++;	// next statement

		dstatement_t* statement = &pr_statements[statement_counter];
		eval_t* a = (eval_t*)&pr_globals[statement->a];
		eval_t* b = (eval_t*)&pr_globals[statement->b];
		eval_t* c = (eval_t*)&pr_globals[statement->c];

		if (!--runaway) {
			PR_RunError("runaway loop error");
		}

		pr_xfunction->profile++;
		pr_xstatement = statement_counter;

		if (pr_trace) {
			PR_PrintStatement(statement);
		}

		switch (statement->op) {
			case OP_ADD_F:
				c->_float = a->_float + b->_float;
				break;

			case OP_ADD_V:
				c->vector[0] = a->vector[0] + b->vector[0];
				c->vector[1] = a->vector[1] + b->vector[1];
				c->vector[2] = a->vector[2] + b->vector[2];
				break;

			case OP_SUB_F:
				c->_float = a->_float - b->_float;
				break;

			case OP_SUB_V:
				c->vector[0] = a->vector[0] - b->vector[0];
				c->vector[1] = a->vector[1] - b->vector[1];
				c->vector[2] = a->vector[2] - b->vector[2];
				break;

			case OP_MUL_F:
				c->_float = a->_float * b->_float;
				break;

			case OP_MUL_V:
				c->_float = a->vector[0] * b->vector[0]
						+ a->vector[1] * b->vector[1]
						+ a->vector[2] * b->vector[2];
				break;

			case OP_MUL_FV:
				c->vector[0] = a->_float * b->vector[0];
				c->vector[1] = a->_float * b->vector[1];
				c->vector[2] = a->_float * b->vector[2];
				break;

			case OP_MUL_VF:
				c->vector[0] = b->_float * a->vector[0];
				c->vector[1] = b->_float * a->vector[1];
				c->vector[2] = b->_float * a->vector[2];
				break;

			case OP_DIV_F:
				c->_float = a->_float / b->_float;
				break;

			case OP_BITAND:
				c->_float = (int)a->_float & (int)b->_float;
				break;

			case OP_BITOR:
				c->_float = (int)a->_float | (int)b->_float;
				break;

			case OP_GE:
				c->_float = a->_float >= b->_float;
				break;
			case OP_LE:
				c->_float = a->_float <= b->_float;
				break;
			case OP_GT:
				c->_float = a->_float > b->_float;
				break;
			case OP_LT:
				c->_float = a->_float < b->_float;
				break;
			case OP_AND:
				c->_float = a->_float && b->_float;
				break;
			case OP_OR:
				c->_float = a->_float || b->_float;
				break;

			case OP_NOT_F:
				c->_float = !a->_float;
				break;
			case OP_NOT_V:
				c->_float = !a->vector[0] && !a->vector[1] && !a->vector[2];
				break;
			case OP_NOT_S:
				c->_float = !a->string || !pr_strings[a->string];
				break;
			case OP_NOT_FNC:
				c->_float = !a->function;
				break;
			case OP_NOT_ENT:
				c->_float = (PROG_TO_EDICT(a->edict) == sv.edicts);
				break;

			case OP_EQ_F:
				c->_float = a->_float == b->_float;
				break;
			case OP_EQ_V:
				c->_float = (a->vector[0] == b->vector[0]) &&
							(a->vector[1] == b->vector[1]) &&
							(a->vector[2] == b->vector[2]);
				break;
			case OP_EQ_S:
				// there's a bug here, where the offset stored in a->string is invalid due
				// overflow, for example when the world model is set in SV_SpawnServer
				// line ~1160:
				//   ent->v.model = sv.worldmodel->name - pr_strings;
				// ent->v.model (and thus a->string) should be negative, but it can be a
				// positive value, causing a segfault when attempting to read
				// pr_strings + a->string
				c->_float = !strcmp(pr_strings + a->string, pr_strings + b->string);
				break;
			case OP_EQ_E:
				c->_float = a->_int == b->_int;
				break;
			case OP_EQ_FNC:
				c->_float = a->function == b->function;
				break;


			case OP_NE_F:
				c->_float = a->_float != b->_float;
				break;
			case OP_NE_V:
				c->_float = (a->vector[0] != b->vector[0]) ||
							(a->vector[1] != b->vector[1]) ||
							(a->vector[2] != b->vector[2]);
				break;
			case OP_NE_S:
				c->_float = strcmp(pr_strings + a->string, pr_strings + b->string);
				break;
			case OP_NE_E:
				c->_float = a->_int != b->_int;
				break;
			case OP_NE_FNC:
				c->_float = a->function != b->function;
				break;

		//==================
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:  // integers
			case OP_STORE_S:
			case OP_STORE_FNC:  // pointers
				b->_int = a->_int;
				break;
			case OP_STORE_V:
				b->vector[0] = a->vector[0];
				b->vector[1] = a->vector[1];
				b->vector[2] = a->vector[2];
				break;

			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:  // integers
			case OP_STOREP_S:
			case OP_STOREP_FNC:  // pointers
				ptr = (eval_t*)((byte*)sv.edicts + b->_int);
				ptr->_int = a->_int;
				break;
			case OP_STOREP_V:
				ptr = (eval_t*)((byte*)sv.edicts + b->_int);
				ptr->vector[0] = a->vector[0];
				ptr->vector[1] = a->vector[1];
				ptr->vector[2] = a->vector[2];
				break;

			case OP_ADDRESS:
				ed = PROG_TO_EDICT(a->edict);
#ifdef PARANOID
				NUM_FOR_EDICT(ed);  // make sure it's in range
#endif
				if (ed == (edict_t*)sv.edicts && sv.state == ss_active) {
					PR_RunError("assignment to world entity");
				}

				c->_int = (byte*)((int*)&ed->v + b->_int) - (byte*)sv.edicts;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
				ed = PROG_TO_EDICT(a->edict);
#ifdef PARANOID
				NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
				a = (eval_t*)((int*)&ed->v + b->_int);
				c->_int = a->_int;
				break;

			case OP_LOAD_V:
				ed = PROG_TO_EDICT(a->edict);
#ifdef PARANOID
				NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
				a = (eval_t*)((int*)&ed->v + b->_int);
				c->vector[0] = a->vector[0];
				c->vector[1] = a->vector[1];
				c->vector[2] = a->vector[2];
				break;

		//==================

			case OP_IFNOT:
				if (!a->_int) {
					statement_counter += statement->b - 1;	// offset the s++
				}
				break;

			case OP_IF:
				if (a->_int) {
					statement_counter += statement->b - 1;	// offset the s++
				}
				break;

			case OP_GOTO:
				statement_counter += statement->a - 1;	// offset the s++
				break;

			case OP_CALL0:
			case OP_CALL1:
			case OP_CALL2:
			case OP_CALL3:
			case OP_CALL4:
			case OP_CALL5:
			case OP_CALL6:
			case OP_CALL7:
			case OP_CALL8:
				pr_argc = statement->op - OP_CALL0;

				if (!a->function) {
					PR_RunError("NULL function");
				}

				new_function = &pr_functions[a->function];

				if (new_function->first_statement < 0) {
					// negative statements are built in functions
					i = -new_function->first_statement;

					if (i >= pr_numbuiltins) {
						PR_RunError("Bad builtin call number");
					}

					pr_builtins[i]();
					break;
				}

				statement_counter = PR_EnterFunction(new_function);
				break;

			case OP_DONE:
			case OP_RETURN:
				pr_globals[OFS_RETURN] = pr_globals[statement->a];
				pr_globals[OFS_RETURN+1] = pr_globals[statement->a+1];
				pr_globals[OFS_RETURN+2] = pr_globals[statement->a+2];

				statement_counter = PR_LeaveFunction();

				if (pr_depth == exitdepth) {
					// all done
					return;
				}
				break;

			case OP_STATE:
				ed = PROG_TO_EDICT(pr_global_struct->self);
#ifdef FPS_20
				ed->v.nextthink = pr_global_struct->time + 0.05;
#else
				ed->v.nextthink = pr_global_struct->time + 0.1;
#endif
				if (a->_float != ed->v.frame) {
					ed->v.frame = a->_float;
				}

				ed->v.think = b->function;
				break;

			default:
				PR_RunError("Bad opcode %i", statement->op);
		}
	}

	// never gets here? always exits in OP_DONE or OP_RETURN?
	printf("PR_ExecuteProgram done?\n");
}
