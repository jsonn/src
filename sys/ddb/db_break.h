/*	$NetBSD: db_break.h,v 1.18.26.1 2007/02/27 16:53:43 yamt Exp $	*/

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
	struct vm_map *map;			/* in this map */
	db_addr_t address;		/* set here */
	int	init_count;		/* number of times to skip bkpt */
	int	count;			/* current count */
	int	flags;			/* flags: */
#define	BKPT_SINGLE_STEP	0x2	    /* to simulate single step */
#define	BKPT_TEMP		0x4	    /* temporary */
	db_expr_t bkpt_inst;		/* saved instruction at bkpt */
	struct db_breakpoint *link;	/* link in in-use or free chain */
} *db_breakpoint_t;

db_breakpoint_t	db_find_breakpoint_here(db_addr_t);
void		db_set_breakpoints(void);
void		db_clear_breakpoints(void);
void		db_delete_cmd(db_expr_t, bool, db_expr_t, const char *);
void		db_breakpoint_cmd(db_expr_t, bool, db_expr_t, const char *);
void		db_listbreak_cmd(db_expr_t, bool, db_expr_t, const char *);
bool		db_map_equal(struct vm_map *, struct vm_map *);
bool		db_map_current(struct vm_map *);
struct vm_map  *db_map_addr(vaddr_t);

#endif	/* _DDB_DB_BREAK_H_ */
