/*
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
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
 * %sccs.include.redist.c%
 *
 *	%W% (Berkeley) %G%
 *
 * from: $Header: /cvsroot/src/sys/sys/device.h,v 1.1.2.1 1993/11/29 06:09:18 mycroft Exp $ (LBL)
 */

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
	struct	device *dv_next;	/* next in list of all */
	struct	cfdata *dv_cfdata;	/* config data that found us */
	int	dv_unit;		/* device unit number */
	char	dv_xname[16];		/* external name (name + unit) */
	struct	device *dv_parent;	/* pointer to parent device */
};

/* `event' counters (use zero or more per device instance, as needed) */
struct evcnt {
	struct	evcnt *ev_next;		/* linked list */
	struct	device *ev_dev;		/* associated device */
	int	ev_count;		/* how many have occurred */
	char	ev_name[8];		/* what to call them (systat display) */
};

/*
 * Configuration data (i.e., data placed in ioconf.c).
 */
struct cfdata {
	struct	cfdriver *cf_driver;	/* config driver */
	short	cf_unit;		/* unit number */
	short	cf_fstate;		/* finding state (below) */
	int	*cf_loc;		/* locators (machine dependent) */
	int	cf_flags;		/* flags from config */
	short	*cf_parents;		/* potential parents */
	void	(**cf_ivstubs)();	/* config-generated vectors, if any */
};
#define FSTATE_NOTFOUND	0	/* has not been found */
#define	FSTATE_FOUND	1	/* has been found */
#define	FSTATE_STAR	2	/* duplicable */

typedef int (*cfmatch_t) __P((struct device *, struct cfdata *, void *));

/*
 * `configuration' driver (what the machine-independent autoconf uses).
 * As devices are found, they are applied against all the potential matches.
 * The one with the best match is taken, and a device structure (plus any
 * other data desired) is allocated.  Pointers to these are placed into
 * an array of pointers.  The array itself must be dynamic since devices
 * can be found long after the machine is up and running.
 */
struct cfdriver {
	void	**cd_devs;		/* devices found */
	char	*cd_name;		/* device name */
	cfmatch_t cd_match;		/* returns a match level */
	void	(*cd_attach) __P((struct device *, struct device *, void *));
	enum	devclass cd_class;	/* device classification */
	size_t	cd_devsize;		/* size of dev data (for malloc) */
	void	*cd_aux;		/* additional driver, if any */
	int	cd_ndevs;		/* size of cd_devs array */
};

/*
 * Configuration printing functions, and their return codes.  The second
 * argument is NULL if the device was configured; otherwise it is the name
 * of the parent device.  The return value is ignored if the device was
 * configured, so most functions can return UNCONF unconditionally.
 */
typedef int (*cfprint_t) __P((void *, char *));
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

struct	device *alldevs;	/* head of list of all devices */
struct	evcnt *allevents;	/* head of list of all events */

struct cfdata *config_search __P((cfmatch_t, struct device *, void *));
struct cfdata *config_rootsearch __P((cfmatch_t, char *, void *));
int config_found __P((struct device *, void *, cfprint_t));
int config_rootfound __P((char *, void *));
void config_attach __P((struct device *, struct cfdata *, void *, cfprint_t));
void evcnt_attach __P((struct device *, const char *, struct evcnt *));
struct dkdevice;
void dk_establish __P((struct dkdevice *, struct device *));
