/*	$NetBSD: db_break.h,v 1.13.2.1 2000/11/20 18:08:46 bouyer Exp $	*/

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

#ifndef	_DDB_DB_BREAK_H_
#define	_DDB_DB_BREAK_H_

#include <uvm/uvm_extern.h>

/*
 * Breakpoints.
 */
typedef struct db_breakpoint {
	vm_map_t map;			/* in this map */
	db_addr_t address;		/* set here */
	int	init_count;		/* number of times to skip bkpt */
	int	count;			/* current count */
	int	flags;			/* flags: */
#define	BKPT_SINGLE_STEP	0x2	    /* to simulate single step */
#define	BKPT_TEMP		0x4	    /* temporary */
	db_expr_t bkpt_inst;		/* saved instruction at bkpt */
	struct db_breakpoint *link;	/* link in in-use or free chain */
} *db_breakpoint_t;

db_breakpoint_t db_breakpoint_alloc __P((void));
void db_breakpoint_free __P((db_breakpoint_t));
void db_set_breakpoint __P((vm_map_t, db_addr_t, int));
void db_delete_breakpoint __P((vm_map_t, db_addr_t));
db_breakpoint_t db_find_breakpoint __P((vm_map_t, db_addr_t));
db_breakpoint_t db_find_breakpoint_here __P((db_addr_t));
void db_set_breakpoints __P((void));
void db_clear_breakpoints __P((void));
void db_list_breakpoints __P((void));
void db_delete_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_breakpoint_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_listbreak_cmd __P((db_expr_t, int, db_expr_t, char *));
boolean_t db_map_equal __P((vm_map_t, vm_map_t));
boolean_t db_map_current __P((vm_map_t));
vm_map_t db_map_addr __P((vaddr_t));

#endif	/* _DDB_DB_BREAK_H_ */
