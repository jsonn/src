/*	$NetBSD: autoconf.h,v 1.9.2.4 1999/03/15 03:58:27 nisimura Exp $ */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

/*
 * Machine-dependent structures of autoconfiguration
 */

struct confargs {
	char	*ca_name;		/* device name */
	int	ca_slot;		/* CPU "slot" number */
};

typedef void	*intr_arg_t;		/* pointer to some softc */
typedef int	(*intr_handler_t) __P((intr_arg_t));

int	badaddr	__P((void *, u_int));
void	makebootdev __P((char *));
