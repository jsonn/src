/*	$NetBSD: ubavar.h,v 1.25 1999/06/06 19:14:49 ragge Exp $	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ubavar.h	7.7 (Berkeley) 6/28/90
 */

/*
 * This file contains definitions related to the kernel structures
 * for dealing with the unibus adapters.
 *
 * Each uba has a uba_softc structure.
 * Each unibus controller which is not a device has a uba_ctlr structure.
 * Each unibus device has a uba_device structure.
 */

/*
 * Per-uba structure.
 *
 * This structure holds the interrupt vector for the uba,
 * and its address in physical and virtual space.  At boot time
 * we determine the devices attached to the uba's and their
 * interrupt vectors, filling in uh_vec.  We free the map
 * register and bdp resources of the uba into the structures
 * defined here.
 *
 * During normal operation, resources are allocated and returned
 * to the structures here.  We watch the number of passive releases
 * on each uba, and if the number is excessive may reset the uba.
 * 
 * When uba resources are needed and not available, or if a device
 * which can tolerate no other uba activity (rk07) gets on the bus,
 * then device drivers may have to wait to get to the bus and are
 * queued here.  It is also possible for processes to block in
 * the unibus driver in resource wait (mrwant, bdpwant); these
 * wait states are also recorded here.
 */
struct	uba_softc {
	struct	device uh_dev;		/* Device struct, autoconfig */
	SIMPLEQ_HEAD(, uba_unit) uh_resq;	/* resource wait chain */
	void	(**uh_reset) __P((int));/* UBA reset function array */
	int	*uh_resarg;		/* array of ubareset args */
	int	uh_resno;		/* Number of devices to reset */
	int	uh_lastiv;		/* last free interrupt vector */
	int	(*uh_errchk) __P((struct uba_softc *));
	void	(*uh_beforescan) __P((struct uba_softc *));
	void	(*uh_afterscan) __P((struct uba_softc *));
	void	(*uh_ubainit) __P((struct uba_softc *));
	void	(*uh_ubapurge) __P((struct uba_softc *, int));
	short	uh_nr;			/* Unibus sequential number */
	bus_space_tag_t	uh_iot;		/* Tag for this Unibus */
	bus_space_handle_t uh_ioh;	/* Handle for I/O space */
	bus_dma_tag_t	uh_dmat;
};

/*
 * Per-controller structure.
 * The unit struct is common to both the adapter and the controller
 * to which it belongs. It is only used on controllers that handles
 * BDP's, and calls the adapter queueing subroutines.
 */
struct	uba_unit {
	SIMPLEQ_ENTRY(uba_unit) uu_resq;/* Queue while waiting for resources */
	void	*uu_softc;	/* Pointer to units softc */
	int	uu_bdp;		/* for controllers that hang on to bdp's */
	int    (*uu_ready) __P((struct uba_unit *));
	void	*uu_ref;	/* Buffer this is related to */
	short   uu_xclu;        /* want exclusive use of bdp's */
	short   uu_keepbdp;     /* hang on to bdp's once allocated */
};

/*
 * uba_attach_args is used during autoconfiguration. It is sent
 * from ubascan() to each (possible) device.
 */
struct uba_attach_args {
	bus_space_tag_t	ua_iot;		/* Tag for this bus I/O-space */
	bus_addr_t	ua_ioh;		/* I/O regs addr */
	bus_dma_tag_t	ua_dmat;
		    /* Pointer to int routine, filled in by probe*/
	void		(*ua_ivec) __P((int));
		    /* UBA reset routine, filled in by probe */
	void		(*ua_reset) __P((int));
	int		ua_iaddr;	/* Full CSR address of device */
	int		ua_br;		/* IPL this dev interrupted on */
	int		ua_cvec;	/* Vector for this device */
};

/*
 * Flags to UBA map/bdp allocation routines
 */
#define	UBA_NEEDBDP	0x01		/* transfer needs a bdp */
#define	UBA_CANTWAIT	0x02		/* don't block me */
#define	UBA_NEED16	0x04		/* need 16 bit addresses only */
#define	UBA_HAVEBDP	0x08		/* use bdp specified in high bits */
#define	UBA_DONTQUE	0x10		/* Do not enqueue xfer */

/*
 * Some common defines for all subtypes of U/Q-buses/adapters.
 */
#define	MAXUBAXFER	(63*1024)	/* Max transfer size in bytes */
#define	UBAIOSIZE	(8*1024)	/* 8K I/O space */
#define ubdevreg(addr) ((addr) & 017777)

#ifdef _KERNEL
#define b_forw  b_hash.le_next	/* Nice to have when handling uba queues */

void	uba_attach __P((struct uba_softc *, unsigned long));
void	uba_enqueue __P((struct uba_unit *));
void	uba_done __P((struct uba_softc *));
void	ubareset __P((int));

#endif /* _KERNEL */
