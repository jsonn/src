/*	$NetBSD: db_command.c,v 1.31.6.1 1999/12/27 18:34:33 wrstuden Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "opt_ddb.h"

/*
 * Command dispatcher.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/pool.h>

#include <machine/db_machdep.h>		/* type definitions */

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_break.h>
#include <ddb/db_watch.h>
#include <ddb/db_run.h>
#include <ddb/db_variables.h>
#include <ddb/db_interface.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>

#include <vm/vm.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_ddb.h>

/*
 * Exported global variables
 */
boolean_t	db_cmd_loop_done;
label_t		*db_recover;

/*
 * if 'ed' style: 'dot' is set at start of last item printed,
 * and '+' points to next line.
 * Otherwise: 'dot' points to next item, '..' points to last.
 */
boolean_t	db_ed_style = TRUE;

/*
 * Utility routine - discard tokens through end-of-line.
 */
void
db_skip_to_eol()
{
	int	t;
	do {
	    t = db_read_token();
	} while (t != tEOL);
}

/*
 * Results of command search.
 */
#define	CMD_UNIQUE	0
#define	CMD_FOUND	1
#define	CMD_NONE	2
#define	CMD_AMBIGUOUS	3
#define	CMD_HELP	4

/*
 * Search for command prefix.
 */
int
db_cmd_search(name, table, cmdp)
	char			*name;
	struct db_command	*table;
	struct db_command	**cmdp;	/* out */
{
	struct db_command	*cmd;
	int			result = CMD_NONE;

	for (cmd = table; cmd->name != 0; cmd++) {
	    register char *lp;
	    register char *rp;
	    register int  c;

	    lp = name;
	    rp = cmd->name;
	    while ((c = *lp) == *rp) {
		if (c == 0) {
		    /* complete match */
		    *cmdp = cmd;
		    return (CMD_UNIQUE);
		}
		lp++;
		rp++;
	    }
	    if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (result == CMD_FOUND) {
		    result = CMD_AMBIGUOUS;
		    /* but keep looking for a full match -
		       this lets us match single letters */
		}
		else {
		    *cmdp = cmd;
		    result = CMD_FOUND;
		}
	    }
	}
	if (result == CMD_NONE) {
	    /* check for 'help' */
		if (name[0] == 'h' && name[1] == 'e'
		    && name[2] == 'l' && name[3] == 'p')
			result = CMD_HELP;
	}
	return (result);
}

void
db_cmd_list(table)
	struct db_command *table;
{
	int	 i, j, w, columns, lines, width=0, items, numcmds;
	char	*p;

	for (numcmds = 0; table[numcmds].name != NULL; numcmds++) {
		w = strlen(table[numcmds].name);
		if (w > width)
			width = w;
	}
	width = DB_NEXT_TAB(width);
	items = 0;

	columns = db_max_width / width;
	if (columns == 0)
		columns = 1;
	lines = (numcmds + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			p = table[j * lines + i].name;
			if (p)
				db_printf("%s", p);
			if (j * lines + i + lines >= numcmds) {
				db_putchar('\n');
				break;
			}
			w = strlen(p);
			while (w < width) {
				w = DB_NEXT_TAB(w);
				db_putchar('\t');
			}
		}
	}
}

void
db_command(last_cmdp, cmd_table)
	struct db_command	**last_cmdp;	/* IN_OUT */
	struct db_command	*cmd_table;
{
	struct db_command	*cmd;
	int		t;
	char		modif[TOK_STRING_SIZE];
	db_expr_t	addr, count;
	boolean_t	have_addr = FALSE;
	int		result;

	t = db_read_token();
	if (t == tEOL) {
	    /* empty line repeats last command, at 'next' */
	    cmd = *last_cmdp;
	    addr = (db_expr_t)db_next;
	    have_addr = FALSE;
	    count = 1;
	    modif[0] = '\0';
	}
	else if (t == tEXCL) {
	    db_fncall(0, 0, 0, NULL);
	    return;
	}
	else if (t != tIDENT) {
	    db_printf("?\n");
	    db_flush_lex();
	    return;
	}
	else {
	    /*
	     * Search for command
	     */
	    while (cmd_table) {
		result = db_cmd_search(db_tok_string,
				       cmd_table,
				       &cmd);
		switch (result) {
		    case CMD_NONE:
			db_printf("No such command\n");
			db_flush_lex();
			return;
		    case CMD_AMBIGUOUS:
			db_printf("Ambiguous\n");
			db_flush_lex();
			return;
		    case CMD_HELP:
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != 0) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    }
		}
	    }

	    if ((cmd->flag & CS_OWN) == 0) {
		/*
		 * Standard syntax:
		 * command [/modifier] [addr] [,count]
		 */
		t = db_read_token();
		if (t == tSLASH) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_printf("Bad modifier\n");
			db_flush_lex();
			return;
		    }
		    db_strcpy(modif, db_tok_string);
		}
		else {
		    db_unread_token(t);
		    modif[0] = '\0';
		}

		if (db_expression(&addr)) {
		    db_dot = (db_addr_t) addr;
		    db_last_addr = db_dot;
		    have_addr = TRUE;
		}
		else {
		    addr = (db_expr_t) db_dot;
		    have_addr = FALSE;
		}
		t = db_read_token();
		if (t == tCOMMA) {
		    if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		    }
		}
		else {
		    db_unread_token(t);
		    count = -1;
		}
		if ((cmd->flag & CS_MORE) == 0) {
		    db_skip_to_eol();
		}
	    }
	}
	*last_cmdp = cmd;
	if (cmd != 0) {
	    /*
	     * Execute the command.
	     */
	    (*cmd->fcn)(addr, have_addr, count, modif);

	    if (cmd->flag & CS_SET_DOT) {
		/*
		 * If command changes dot, set dot to
		 * previous address displayed (if 'ed' style).
		 */
		if (db_ed_style) {
		    db_dot = db_prev;
		}
		else {
		    db_dot = db_next;
		}
	    }
	    else {
		/*
		 * If command does not change dot,
		 * set 'next' location to be the same.
		 */
		db_next = db_dot;
	    }
	}
}

/*ARGSUSED*/
void
db_map_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
        boolean_t full = FALSE;
        
        if (modif[0] == 'f')
                full = TRUE;

        uvm_map_printit((vm_map_t) addr, full, db_printf);
}

/*ARGSUSED*/
void
db_object_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
        boolean_t full = FALSE;
        
        if (modif[0] == 'f')
                full = TRUE;

	uvm_object_printit((struct uvm_object *) addr, full, db_printf);
}

/*ARGSUSED*/
void
db_page_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
        boolean_t full = FALSE;
        
        if (modif[0] == 'f')
                full = TRUE;

	uvm_page_printit((struct vm_page *) addr, full, db_printf);
}

/*ARGSUSED*/
void
db_pool_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{

	pool_printit((struct pool *) addr, modif, db_printf);
}

/*
 * 'show' commands
 */

struct db_command db_show_all_cmds[] = {
	{ "callout",	db_show_callout,	0, NULL },
	{ "procs",	db_show_all_procs,	0, NULL },
	{ NULL, 	NULL, 			0, NULL }
};

struct db_command db_show_cmds[] = {
	{ "all",	NULL,			0,	db_show_all_cmds },
	{ "breaks",	db_listbreak_cmd, 	0,	NULL },
	{ "map",	db_map_print_cmd,	0,	NULL },
	{ "object",	db_object_print_cmd,	0,	NULL },
	{ "page",	db_page_print_cmd,	0,	NULL },
	{ "pool",	db_pool_print_cmd,	0,	NULL },
	{ "registers",	db_show_regs,		0,	NULL },
	{ "watches",	db_listwatch_cmd, 	0,	NULL },
	{ NULL,		NULL,			0,	NULL }
};

struct db_command db_command_table[] = {
	{ "break",	db_breakpoint_cmd,	0,		NULL },
	{ "c",		db_continue_cmd,	0,		NULL },
	{ "call",	db_fncall,		CS_OWN,		NULL },
	{ "callout",	db_show_callout,	0,		NULL },
	{ "continue",	db_continue_cmd,	0,		NULL },
	{ "d",		db_delete_cmd,		0,		NULL },
	{ "delete",	db_delete_cmd,		0,		NULL },
	{ "dwatch",	db_deletewatch_cmd,	0,		NULL },
	{ "examine",	db_examine_cmd,		CS_SET_DOT, 	NULL },
	{ "kill",	db_kill_proc,		CS_OWN,		NULL },
#ifdef DB_MACHINE_COMMANDS
	{ "machine",    NULL,                   0,     		NULL },
#endif
	{ "match",	db_trace_until_matching_cmd,0,		NULL },
	{ "next",	db_trace_until_matching_cmd,0,		NULL },
	{ "print",	db_print_cmd,		0,		NULL },
	{ "ps",		db_show_all_procs,	0,		NULL },
	{ "reboot",	db_reboot_cmd,		CS_OWN,		NULL },
	{ "s",		db_single_step_cmd,	0,		NULL },
	{ "search",	db_search_cmd,		CS_OWN|CS_SET_DOT, NULL },
	{ "set",	db_set_cmd,		CS_OWN,		NULL },
	{ "show",	NULL,			0,		db_show_cmds },
	{ "step",	db_single_step_cmd,	0,		NULL },
	{ "sync",	db_sync_cmd,		CS_OWN,		NULL },
	{ "trace",	db_stack_trace_cmd,	0,		NULL },
	{ "until",	db_trace_until_call_cmd,0,		NULL },
	{ "w",		db_write_cmd,		CS_MORE|CS_SET_DOT, NULL },
	{ "watch",	db_watchpoint_cmd,	CS_MORE,	NULL },
	{ "write",	db_write_cmd,		CS_MORE|CS_SET_DOT, NULL },
	{ "x",		db_examine_cmd,		CS_SET_DOT, 	NULL },
	{ NULL, 	NULL,			0,		NULL }
};

#ifdef DB_MACHINE_COMMANDS

/*
 * this function should be called to install the machine dependent
 * commands. It should be called before the debugger is enabled
 */
void
db_machine_commands_install(ptr)
	struct db_command *ptr;
{
	struct db_command *cmd;

	for (cmd = db_command_table; cmd != 0; cmd++) {
		if (strcmp(cmd->name, "machine") == 0) {
			cmd->more = ptr;
			break;
		}
	}
}

#endif

struct db_command	*db_last_command = 0;

void
db_command_loop()
{
	label_t		db_jmpbuf;
	label_t		*savejmp;
	extern int	db_output_line;

	/*
	 * Initialize 'prev' and 'next' to dot.
	 */
	db_prev = db_dot;
	db_next = db_dot;

	db_cmd_loop_done = 0;

	savejmp = db_recover;
	db_recover = &db_jmpbuf;
	(void) setjmp(&db_jmpbuf);

	while (!db_cmd_loop_done) {
		if (db_print_position() != 0)
			db_printf("\n");
		db_output_line = 0;

		db_printf("db> ");
		(void) db_read_line();

		db_command(&db_last_command, db_command_table);
	}

	db_recover = savejmp;
}

void
db_error(s)
	char *s;
{
	if (s)
	    db_printf(s);
	db_flush_lex();
	longjmp(db_recover);
}


/*
 * Call random function:
 * !expr(arg,arg,arg)
 */
/*ARGSUSED*/
void
db_fncall(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_expr_t	fn_addr;
#define	MAXARGS		11
	db_expr_t	args[MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	db_expr_t	(*func) __P((db_expr_t, ...));
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function\n");
	    db_flush_lex();
	    return;
	}
	func = (db_expr_t (*) __P((db_expr_t, ...))) fn_addr;

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == MAXARGS) {
			db_printf("Too many arguments\n");
			db_flush_lex();
			return;
		    }
		    if (!db_expression(&args[nargs])) {
			db_printf("Argument missing\n");
			db_flush_lex();
			return;
		    }
		    nargs++;
		}
		db_unread_token(t);
	    }
	    if (db_read_token() != tRPAREN) {
		db_printf("?\n");
		db_flush_lex();
		return;
	    }
	}
	db_skip_to_eol();

	while (nargs < MAXARGS) {
	    args[nargs++] = 0;
	}

	retval = (*func)(args[0], args[1], args[2], args[3], args[4],
			 args[5], args[6], args[7], args[8], args[9]);
	db_printf("%#ln\n", retval);
}

void
db_reboot_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_expr_t bootflags;

	/* Flags, default to RB_AUTOBOOT */
	if (!db_expression(&bootflags))
		bootflags = (db_expr_t)RB_AUTOBOOT;
	if (db_read_token() != tEOL) {
	    db_error("?\n");
	    /*NOTREACHED*/
	}
	cpu_reboot((int)bootflags, NULL);
}

void
db_sync_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{

	cpu_reboot(RB_DUMP, NULL);
}
