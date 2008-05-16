/*	$NetBSD: saio.h,v 1.1.130.1 2008/05/16 02:23:23 yamt Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file derived from kernel/mach/sun3.md/machMon.h from the
 * sprite distribution.
 *
 * In particular, this file came out of the Walnut Creek cdrom collection
 * which contained no warnings about any possible copyright infringement.
 */

/*
 * machMon.h --
 *
 *     Structures, constants and defines for access to the sun monitor.
 *     These were translated from the sun monitor header files:
 *          mon/sunromvec.h
 *          stand/saio.h
 *
 * Copyright (C) 1985 Regents of the University of California
 * All rights reserved.
 *
 * Header: /sprite/src/boot/sunprom/sun3.md/RCS/machMon.h,v \
 * 1.1 90/09/17 10:57:28 rab Exp Locker: rab $ SPRITE (Berkeley)
 */

/*
 * The table entry that describes a device.  It exists in the PROM; a
 * pointer to it is passed in MachMonBootParam.  It can be used to locate
 * PROM subroutines for opening, reading, and writing the device.
 *
 * When using this interface, only one device can be open at once.
 */
struct boottab {
	char	b_dev[2];		/* The name of the device */
	int	(*b_probe)();		/* probe() --> -1 or found controller 
					   number */
	int	(*b_boot)();		/* boot(bp) --> -1 or start address */
	int	(*b_open)();		/* open(iobp) --> -1 or 0 */
	int	(*b_close)();		/* close(iobp) --> -1 or 0 */
	int	(*b_strategy)();	/* strategy(iobp,rw) --> -1 or 0 */
	char	*b_desc;		/* Printable string describing dev */
	struct devinfo *b_devinfo;	/* Information to configure device */
} __attribute__((packed));

/*
 * This table gives information about the resources needed by a device.  
 */
struct devinfo {
  unsigned int      d_devbytes;   /* Bytes occupied by device in IO space.  */
  unsigned int      d_dmabytes;   /* Bytes needed by device in DMA memory.  */
  unsigned int      d_localbytes; /* Bytes needed by device for local info. */
  unsigned int      d_stdcount;   /* How many standard addresses.           */
  unsigned long     *d_stdaddrs;  /* The vector of standard addresses.      */
  unsigned int      d_devtype;    /* What map space device is in.           */
  unsigned int      d_maxiobytes; /* Size to break big I/O's into.          */
};

/*
 * These are the "page map entry types" specified in the
 * d_devtype field of struct devinfo.
 */
#define MAP_MAINMEM 	0
#define MAP_OBIO		1
#define MAP_MBMEM		2
#define MAP_MBIO		3
#define MAP_VME16A16D	4
#define MAP_VME16A32D	5
#define MAP_VME24A16D	6
#define MAP_VME24A32D	7
#define MAP_VME32A16D	8
#define MAP_VME32A32D	9
#define MAP__NTYPES 	10

/*
 * A "stand alone I/O request", (from SunOS saio.h)
 * This is passed as the main argument to the PROM I/O routines
 * in the MachMonBootDevice structure.
 */
struct saioreq {
	char	si_flgs;
	char	si_pad1;
	struct boottab *si_boottab;	/* Points to boottab entry if any */
	char	*si_devdata;		/* Device-specific data pointer */
	int	si_ctlr;		/* Controller number or address */
	int	si_unit;		/* Unit number within controller */
	long	si_boff;		/* Partition number within unit */
	long	si_cyloff;
	long	si_offset;
	long	si_bn;			/* Block number to R/W */
	char	*si_ma;			/* Memory address to R/W */
	int	si_cc;			/* Character count to R/W */
	struct	saif *si_sif;		/* net if. pointer (set by b_open) */
	char 	*si_devaddr;		/* Points to mapped in device */
	char	*si_dmaaddr;		/* Points to allocated DMA space */
} __attribute__((packed));

#define SAIO_F_READ	0x01
#define SAIO_F_WRITE	0x02
#define SAIO_F_ALLOC	0x04
#define SAIO_F_FILE	0x08
#define	SAIO_F_EOF	0x10	/* EOF on device */
#define SAIO_F_AJAR	0x20	/* Descriptor "ajar" (stopped but not closed) */

/*
 * Ethernet interface descriptor (from SunOS saio.h)
 * First, set: saiop->si_devaddr, saiop->si_dmaaddr, etc.
 * Then:  saiop->si_boottab->b_open()  will set:
 *   saiop->si_sif;
 *   saiop->si_devdata;
 * The latter is the first arg to the following functions.
 * Note that the buffer must be in DVMA space...
 */
struct saif {
	/* Transmit packet, returns zero on success. */
	int	(*sif_xmit)(void *devdata, char *buf, int len);
	/* Receive packet, return zero if none arrived. */
	int	(*sif_poll)(void *devdata, char *buf);
	/* Reset interface, set addresses, etc. */
	int	(*sif_reset)(void *devdata, struct saioreq *sip);
	/*
	 * Later proms have more stuff here, but what versions?
	 * It appears that all V3.X PROMs support this...
	 */
	/* Copy our ethernet address to the passed array. */
	int	(*sif_macaddr)(char *ea);
};

#ifdef	_STANDALONE
/* libsa:promdev.c */
int  prom_iopen (struct saioreq *sip);
void prom_iclose(struct saioreq *sip);
#endif	/* _STANDALONE */
