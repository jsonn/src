/*	$NetBSD: db_watch.h,v 1.12.2.1 2001/03/12 13:30:00 bouyer Exp $	*/

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
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	10/90
 */

#ifndef	_DDB_DB_WATCH_
#define	_DDB_DB_WATCH_

/*
 * Watchpoint.
 */
typedef struct db_watchpoint {
	vm_map_t map;			/* in this map */
	db_addr_t loaddr;		/* from this address */
	db_addr_t hiaddr;		/* to this address */
	struct db_watchpoint *link;	/* link in in-use or free chain */
} *db_watchpoint_t;

db_watchpoint_t db_watchpoint_alloc __P((void));
void db_watchpoint_free __P((db_watchpoint_t));
void db_set_watchpoint __P((vm_map_t, db_addr_t, vsize_t));
void db_delete_watchpoint __P((vm_map_t, db_addr_t));
void db_list_watchpoints __P((void));
void db_deletewatch_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_watchpoint_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_listwatch_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_set_watchpoints __P((void));
void db_clear_watchpoints __P((void));
boolean_t db_find_watchpoint __P((vm_map_t, db_addr_t, db_regs_t *));

#endif	/* _DDB_DB_WATCH_ */
