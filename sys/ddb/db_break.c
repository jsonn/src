/*	$NetBSD: db_break.c,v 1.11.2.1 2000/11/20 18:08:46 bouyer Exp $	*/

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
 *
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Breakpoints.
 */
#include <sys/param.h>
#include <sys/proc.h>

#include <machine/db_machdep.h>		/* type definitions */

#include <ddb/db_lex.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_break.h>
#include <ddb/db_output.h>

#define	NBREAKPOINTS	100
struct db_breakpoint	db_break_table[NBREAKPOINTS];
db_breakpoint_t		db_next_free_breakpoint = &db_break_table[0];
db_breakpoint_t		db_free_breakpoints = 0;
db_breakpoint_t		db_breakpoint_list = 0;

db_breakpoint_t
db_breakpoint_alloc()
{
	db_breakpoint_t	bkpt;

	if ((bkpt = db_free_breakpoints) != 0) {
	    db_free_breakpoints = bkpt->link;
	    return (bkpt);
	}
	if (db_next_free_breakpoint == &db_break_table[NBREAKPOINTS]) {
	    db_printf("All breakpoints used.\n");
	    return (0);
	}
	bkpt = db_next_free_breakpoint;
	db_next_free_breakpoint++;

	return (bkpt);
}

void
db_breakpoint_free(bkpt)
	db_breakpoint_t	bkpt;
{
	bkpt->link = db_free_breakpoints;
	db_free_breakpoints = bkpt;
}

void
db_set_breakpoint(map, addr, count)
	vm_map_t	map;
	db_addr_t	addr;
	int		count;
{
	db_breakpoint_t	bkpt;

	if (db_find_breakpoint(map, addr)) {
	    db_printf("Already set.\n");
	    return;
	}

	bkpt = db_breakpoint_alloc();
	if (bkpt == 0) {
	    db_printf("Too many breakpoints.\n");
	    return;
	}

	bkpt->map = map;
	bkpt->address = addr;
	bkpt->flags = 0;
	bkpt->init_count = count;
	bkpt->count = count;

	bkpt->link = db_breakpoint_list;
	db_breakpoint_list = bkpt;
}

void
db_delete_breakpoint(map, addr)
	vm_map_t	map;
	db_addr_t	addr;
{
	db_breakpoint_t	bkpt;
	db_breakpoint_t	*prev;

	for (prev = &db_breakpoint_list;
	     (bkpt = *prev) != 0;
	     prev = &bkpt->link) {
	    if (db_map_equal(bkpt->map, map) &&
		(bkpt->address == addr)) {
		*prev = bkpt->link;
		break;
	    }
	}
	if (bkpt == 0) {
	    db_printf("Not set.\n");
	    return;
	}

	db_breakpoint_free(bkpt);
}

db_breakpoint_t
db_find_breakpoint(map, addr)
	vm_map_t	map;
	db_addr_t	addr;
{
	db_breakpoint_t	bkpt;

	for (bkpt = db_breakpoint_list;
	     bkpt != 0;
	     bkpt = bkpt->link)
	{
	    if (db_map_equal(bkpt->map, map) &&
		(bkpt->address == addr))
		return (bkpt);
	}
	return (0);
}

db_breakpoint_t
db_find_breakpoint_here(addr)
	db_addr_t	addr;
{
    return db_find_breakpoint(db_map_addr(addr), addr);
}

boolean_t	db_breakpoints_inserted = TRUE;

void
db_set_breakpoints()
{
	db_breakpoint_t	bkpt;

	if (!db_breakpoints_inserted) {

	    for (bkpt = db_breakpoint_list;
	         bkpt != 0;
	         bkpt = bkpt->link)
		if (db_map_current(bkpt->map)) {
		    bkpt->bkpt_inst = db_get_value(bkpt->address,
						   BKPT_SIZE,
						   FALSE);
		    db_put_value(bkpt->address,
				 BKPT_SIZE,
				 BKPT_SET(bkpt->bkpt_inst));
		}
	    db_breakpoints_inserted = TRUE;
	}
}

void
db_clear_breakpoints()
{
	db_breakpoint_t	bkpt;

	if (db_breakpoints_inserted) {

	    for (bkpt = db_breakpoint_list;
	         bkpt != 0;
		 bkpt = bkpt->link)
		if (db_map_current(bkpt->map)) {
		    db_put_value(bkpt->address, BKPT_SIZE, bkpt->bkpt_inst);
		}
	    db_breakpoints_inserted = FALSE;
	}
}

/*
 * List breakpoints.
 */
void
db_list_breakpoints()
{
	db_breakpoint_t	bkpt;

	if (db_breakpoint_list == 0) {
	    db_printf("No breakpoints set\n");
	    return;
	}

	db_printf(" Map      Count    Address\n");
	for (bkpt = db_breakpoint_list;
	     bkpt != 0;
	     bkpt = bkpt->link)
	{
	    db_printf("%s%p %5d    ",
		      db_map_current(bkpt->map) ? "*" : " ",
		      bkpt->map, bkpt->init_count);
	    db_printsym(bkpt->address, DB_STGY_PROC, db_printf);
	    db_printf("\n");
	}
}

/* Delete breakpoint */
/*ARGSUSED*/
void
db_delete_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_delete_breakpoint(db_map_addr(addr), (db_addr_t)addr);
}

/* Set breakpoint with skip count */
/*ARGSUSED*/
void
db_breakpoint_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	if (count == -1)
	    count = 1;

	db_set_breakpoint(db_map_addr(addr), (db_addr_t)addr, count);
}

/* list breakpoints */
/*ARGSUSED*/
void
db_listbreak_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_list_breakpoints();
}

#include <uvm/uvm_extern.h>

/*
 *	We want ddb to be usable before most of the kernel has been
 *	initialized.  In particular, current_thread() or kernel_map
 *	(or both) may be null.
 */

boolean_t
db_map_equal(map1, map2)
	vm_map_t	map1, map2;
{
	return ((map1 == map2) ||
		((map1 == NULL) && (map2 == kernel_map)) ||
		((map1 == kernel_map) && (map2 == NULL)));
}

boolean_t
db_map_current(map)
	vm_map_t	map;
{
#if 0
	thread_t	thread;

	return ((map == NULL) ||
		(map == kernel_map) ||
		(((thread = current_thread()) != NULL) &&
		 (map == thread->task->map)));
#else
	return (1);
#endif
}

vm_map_t
db_map_addr(addr)
	vaddr_t addr;
{
#if 0
	thread_t	thread;

	/*
	 *	We want to return kernel_map for all
	 *	non-user addresses, even when debugging
	 *	kernel tasks with their own maps.
	 */

	if ((VM_MIN_ADDRESS <= addr) &&
	    (addr < VM_MAX_ADDRESS) &&
	    ((thread = current_thread()) != NULL))
	    return thread->task->map;
	else
#endif
	    return kernel_map;
}
