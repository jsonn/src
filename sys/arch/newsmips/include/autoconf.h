/*	$NetBSD: autoconf.h,v 1.6.8.1 2002/10/10 18:34:35 jdolecek Exp $	*/

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
	const char	*ca_name;		/* Device name. */
	int	ca_slot;		/* Device slot (table entry). */
	int	ca_offset;		/* Offset into slot. */
	int	ca_addr;		/* Device address. */
	int	ca_slotpri;		/* Device interrupt "priority" */
};

/* Locator aliases */
#define cf_addr	cf_loc[0]
#define cf_level	cf_loc[1]

int news3400_badaddr __P((void *, u_int));
#define badaddr news3400_badaddr

void *hb_intr_establish __P((int, int, int (*)(void *), void *));
