/*	$NetBSD: device.h,v 1.21.4.1 1997/09/22 06:34:18 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)device.h	8.2 (Berkeley) 2/17/94
 */

#ifndef _SYS_DEVICE_H_
#define	_SYS_DEVICE_H_

#include <sys/queue.h>

/*
 * Minimal device structures.
 * Note that all ``system'' device types are listed here.
 */
enum devclass {
	DV_DULL,		/* generic, no special info */
	DV_CPU,			/* CPU (carries resource utilization) */
	DV_DISK,		/* disk drive (label, etc) */
	DV_IFNET,		/* network interface */
	DV_TAPE,		/* tape device */
	DV_TTY			/* serial line interface (???) */
};

struct device {
	enum	devclass dv_class;	/* this device's classification */
	TAILQ_ENTRY(device) dv_list;	/* entry on list of all devices */
	struct	cfdata *dv_cfdata;	/* config data that found us */
	int	dv_unit;		/* device unit number */
	char	dv_xname[16];		/* external name (name + unit) */
	struct	device *dv_parent;	/* pointer to parent device */
};
TAILQ_HEAD(devicelist, device);

/* `event' counters (use zero or more per device instance, as needed) */
struct evcnt {
	TAILQ_ENTRY(evcnt) ev_list;	/* entry on list of all counters */
	struct	device *ev_dev;		/* associated device */
	int	ev_count;		/* how many have occurred */
	char	ev_name[8];		/* what to call them (systat display) */
};
TAILQ_HEAD(evcntlist, evcnt);

/*
 * Configuration data (i.e., data placed in ioconf.c).
 */
struct cfdata {
	struct	cfattach *cf_attach;	/* config attachment */
	struct	cfdriver *cf_driver;	/* config driver */
	short	cf_unit;		/* unit number */
	short	cf_fstate;		/* finding state (below) */
	int	*cf_loc;		/* locators (machine dependent) */
	int	cf_flags;		/* flags from config */
	short	*cf_parents;		/* potential parents */
	const char **cf_locnames;	/* locator names (machine dependent) */
};
#define FSTATE_NOTFOUND	0	/* has not been found */
#define	FSTATE_FOUND	1	/* has been found */
#define	FSTATE_STAR	2	/* duplicable */

#ifdef __BROKEN_INDIRECT_CONFIG
typedef int (*cfmatch_t) __P((struct device *, void *, void *));
typedef void (*cfscan_t) __P((struct device *, void *));
#else
typedef int (*cfmatch_t) __P((struct device *, struct cfdata *, void *));
#endif

/*
 * `configuration' attachment and driver (what the machine-independent
 * autoconf uses).  As devices are found, they are applied against all
 * the potential matches.  The one with the best match is taken, and a
 * device structure (plus any other data desired) is allocated.  Pointers
 * to these are placed into an array of pointers.  The array itself must
 * be dynamic since devices can be found long after the machine is up
 * and running.
 *
 * Devices can have multiple configuration attachments if they attach
 * to different attributes (busses, or whatever), to allow specification
 * of multiple match and attach functions.  There is only one configuration
 * driver per driver, so that things like unit numbers and the device
 * structure array will be shared.
 */
struct cfattach {
	size_t	  ca_devsize;		/* size of dev data (for malloc) */
	cfmatch_t ca_match;		/* returns a match level */
	void	(*ca_attach) __P((struct device *, struct device *, void *));
	/* XXX should have detach */
};

struct cfdriver {
	void	**cd_devs;		/* devices found */
	char	*cd_name;		/* device name */
	enum	devclass cd_class;	/* device classification */
#ifdef __BROKEN_INDIRECT_CONFIG
	int	cd_indirect;		/* indirectly configure subdevices */
#else
	/* XXX TEMPORARY */
	void	*cd_lossage_prevention;	/* keep 'cd_ndevs' from being initted */
#endif
	int	cd_ndevs;		/* size of cd_devs array */
};

/*
 * Configuration printing functions, and their return codes.  The second
 * argument is NULL if the device was configured; otherwise it is the name
 * of the parent device.  The return value is ignored if the device was
 * configured, so most functions can return UNCONF unconditionally.
 */
typedef int (*cfprint_t) __P((void *, const char *));
#define	QUIET	0		/* print nothing */
#define	UNCONF	1		/* print " not configured\n" */
#define	UNSUPP	2		/* print " not supported\n" */

/*
 * Pseudo-device attach information (function + number of pseudo-devs).
 */
struct pdevinit {
	void	(*pdev_attach) __P((int));
	int	pdev_count;
};

#ifdef _KERNEL

extern struct devicelist alldevs;	/* list of all devices */
extern struct evcntlist allevents;	/* list of all event counters */

void config_init __P((void));
#ifdef __BROKEN_INDIRECT_CONFIG
void *config_search __P((cfmatch_t, struct device *, void *));
void *config_rootsearch __P((cfmatch_t, char *, void *));
#else /* __BROKEN_INDIRECT_CONFIG */
struct cfdata *config_search __P((cfmatch_t, struct device *, void *));
struct cfdata *config_rootsearch __P((cfmatch_t, char *, void *));
#endif /* __BROKEN_INDIRECT_CONFIG */
struct device *config_found_sm __P((struct device *, void *, cfprint_t,
    cfmatch_t));
struct device *config_rootfound __P((char *, void *));
#ifdef __BROKEN_INDIRECT_CONFIG
void config_scan __P((cfscan_t, struct device *));
struct device *config_attach __P((struct device *, void *, void *, cfprint_t));
#else /* __BROKEN_INDIRECT_CONFIG */
struct device *config_attach __P((struct device *, struct cfdata *, void *,
    cfprint_t));
#endif /* __BROKEN_INDIRECT_CONFIG */
#if defined(__alpha__) || defined(hp300) || defined(__i386__)
void device_register __P((struct device *, void *));
#endif
void evcnt_attach __P((struct device *, const char *, struct evcnt *));

/* compatibility definitions */
#define config_found(d, a, p)	config_found_sm((d), (a), (p), NULL)
#endif /* _KERNEL */

#endif /* !_SYS_DEVICE_H_ */
