/*	$NetBSD: autoconf.h,v 1.8.2.1 2005/01/24 08:33:58 skrll Exp $	*/
/*	$OpenBSD: autoconf.h,v 1.2 1997/03/12 19:16:54 pefo Exp $	*/
/*	NetBSD: autoconf.h,v 1.1 1995/02/13 23:07:31 cgd Exp 	*/

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
 * Machine-dependent structures for autoconfiguration
 */

#ifndef _ARC_AUTOCONF_H_
#define _ARC_AUTOCONF_H_

struct confargs;

typedef int (*intr_handler_t)(void *);

struct abus {
	struct	device *ab_dv;		/* back-pointer to device */
	int	ab_type;		/* bus type (see below) */
	void	(*ab_intr_establish)	/* bus's set-handler function */
		   (struct confargs *, intr_handler_t, void *);
	void	(*ab_intr_disestablish)	/* bus's unset-handler function */
		   (struct confargs *);
	caddr_t	(*ab_cvtaddr)		/* convert slot/offset to address */
		   (struct confargs *);
	int	(*ab_matchname)		/* see if name matches driver */
		   (struct confargs *, char *);
};

#define	BUS_MAIN	1		/* mainbus */
#define	BUS_PICA	2		/* PICA Bus */
#define	BUS_ISABR	3		/* ISA Bridge Bus */

#define	BUS_INTR_ESTABLISH(ca, handler, val)				\
	    (*(ca)->ca_bus->ab_intr_establish)((ca), (handler), (val))
#define	BUS_INTR_DISESTABLISH(ca)					\
	    (*(ca)->ca_bus->ab_intr_establish)(ca)
#define	BUS_CVTADDR(ca)							\
	    (*(ca)->ca_bus->ab_cvtaddr)(ca)
#define	BUS_MATCHNAME(ca, name)						\
	    (*(ca)->ca_bus->ab_matchname)((ca), (name))

struct confargs {
	char	*ca_name;		/* Device name. */
	int	ca_slot;		/* Device slot. */
	int	ca_offset;		/* Offset into slot. */
	struct	abus *ca_bus;		/* bus device resides on. */
};

void	makebootdev(char *cp);

/* serial console related variables */
extern int com_freq;
extern int com_console;
extern int com_console_address;
extern int com_console_speed;
extern int com_console_mode;
#endif /* _ARC_AUTOCONF_H_ */
