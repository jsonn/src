/*	$NetBSD: autoconf.c,v 1.54.4.8 2003/01/07 21:23:32 thorpej Exp $ */

/*
 * Copyright (c) 1996
 *    The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

#include "opt_ddb.h"
#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/msgbuf.h>
#include <sys/boot_flag.h>

#include <net/if.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>
#include <machine/sparc64.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <sparc64/sparc64/timerreg.h>

#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>
#include <dev/pci/pcivar.h>
#include <dev/sbus/sbusvar.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif


int printspl = 0;

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	stdinnode;	/* node ID of ROM's console input device */
int	fbnode;		/* node ID of ROM's console output device */
int	optionsnode;	/* node ID of ROM's options */

#ifdef KGDB
extern	int kgdb_debug_panic;
#endif

static	int rootnode;
char	machine_model[100];

static	char *str2hex __P((char *, int *));
static	int mbprint __P((void *, const char *));
static	void crazymap __P((char *, int *));
int	st_crazymap __P((int));
void	sync_crash __P((void));
int	mainbus_match __P((struct device *, struct cfdata *, void *));
static	void mainbus_attach __P((struct device *, struct device *, void *));

struct	bootpath bootpath[8];
int	nbootpath;
static	void bootpath_build __P((void));
static	void bootpath_print __P((struct bootpath *));

/* Global interrupt mappings for all device types.  Match against the OBP
 * 'device_type' property. 
 */
struct intrmap intrmap[] = {
	{ "block",	PIL_FD },	/* Floppy disk */
	{ "serial",	PIL_SER },	/* zs */
	{ "scsi",	PIL_SCSI },
	{ "network",	PIL_NET },
	{ "display",	PIL_VIDEO },
	{ "audio",	PIL_AUD },
	{ "ide",	PIL_SCSI },
/* The following devices don't have device types: */
	{ "SUNW,CS4231",	PIL_AUD },
	{ NULL,		0 }
};

#ifdef DEBUG
#define ACDB_BOOTDEV	0x1
#define	ACDB_PROBE	0x2
int autoconf_debug = 0x0;
#define DPRINTF(l, s)   do { if (autoconf_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

/*
 * Most configuration on the SPARC is done by matching OPENPROM Forth
 * device names with our internal names.
 */
int
matchbyname(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	printf("%s: WARNING: matchbyname\n", cf->cf_name);
	return (0);
}

/*
 * Convert hex ASCII string to a value.  Returns updated pointer.
 * Depends on ASCII order (this *is* machine-dependent code, you know).
 */
static char *
str2hex(str, vp)
	register char *str;
	register int *vp;
{
	register int v, c;

	for (v = 0;; v = v * 16 + c, str++) {
		c = *(u_char *)str;
		if (c <= '9') {
			if ((c -= '0') < 0)
				break;
		} else if (c <= 'F') {
			if ((c -= 'A' - 10) < 10)
				break;
		} else if (c <= 'f') {
			if ((c -= 'a' - 10) < 10)
				break;
		} else
			break;
	}
	*vp = v;
	return (str);
}

/*
 * locore.s code calls bootstrap() just before calling main().
 *
 * What we try to do is as follows:
 *
 * 1) We will try to re-allocate the old message buffer.
 *
 * 2) We will then get the list of the total and available
 *	physical memory and available virtual memory from the
 *	prom.
 *
 * 3) We will pass the list to pmap_bootstrap to manage them.
 *
 * We will try to run out of the prom until we get to cpu_init().
 */
void
bootstrap(nctx)
	int nctx;
{
	extern int end;	/* End of kernel */
#if defined(DDB) && defined(DB_ELF_SYMBOLS)
	extern void *ssym, *esym;
#endif
#ifndef	__arch64__
	/* Assembly glue for the PROM */
	extern void OF_sym2val32 __P((void *));
	extern void OF_val2sym32 __P((void *));
#endif

	/* 
	 * Initialize ddb first and register OBP callbacks.
	 * We can do this because ddb_init() does not allocate anything,
	 * just initialze some pointers to important things
	 * like the symtab.
	 *
	 * By doing this first and installing the OBP callbacks
	 * we get to do symbolic debugging of pmap_bootstrap().
	 */
#ifdef KGDB
/* Moved zs_kgdb_init() to dev/zs.c:consinit(). */
	zs_kgdb_init();		/* XXX */
#endif
	/* Initialize the PROM console so printf will not panic */
	(*cn_tab->cn_init)(cn_tab);
#ifdef DDB
#ifdef DB_ELF_SYMBOLS
	ddb_init((int)((caddr_t)esym - (caddr_t)ssym), ssym, esym); 
#else
	ddb_init();
#endif
#ifdef __arch64__
	/* This can only be installed on an 64-bit system cause otherwise our stack is screwed */
	OF_set_symbol_lookup(OF_sym2val, OF_val2sym);
#else
#if 1
	OF_set_symbol_lookup(OF_sym2val32, OF_val2sym32);
#endif
#endif
#endif

	pmap_bootstrap(KERNBASE, (u_long)&end, nctx);
}

/*
 * bootpath_build: build a bootpath. Used when booting a generic
 * kernel to find our root device.  Newer proms give us a bootpath,
 * for older proms we have to create one.  An element in a bootpath
 * has 4 fields: name (device name), val[0], val[1], and val[2]. Note that:
 * Interpretation of val[] is device-dependent. Some examples:
 *
 * if (val[0] == -1) {
 *	val[1] is a unit number    (happens most often with old proms)
 * } else {
 *	[sbus device] val[0] is a sbus slot, and val[1] is an sbus offset
 *	[scsi disk] val[0] is target, val[1] is lun, val[2] is partition
 *	[scsi tape] val[0] is target, val[1] is lun, val[2] is file #
 *	[pci device] val[0] is device, val[1] is function, val[2] might be partition
 * }
 *
 */

static void
bootpath_build()
{
	register char *cp, *pp;
	register struct bootpath *bp;
	register long chosen;
	char buf[128];

	bzero((void*)bootpath, sizeof(bootpath));
	bp = bootpath;

	/*
	 * Grab boot path from PROM
	 */
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "bootpath", buf, sizeof(buf));
	cp = buf;
	while (cp != NULL && *cp == '/') {
		/* Step over '/' */
		++cp;
		/* Extract name */
		pp = bp->name;
		while (*cp != '@' && *cp != '/' && *cp != '\0')
			*pp++ = *cp++;
		*pp = '\0';
		if (*cp == '@') {
			cp = str2hex(++cp, &bp->val[0]);
			if (*cp == ',')
				cp = str2hex(++cp, &bp->val[1]);
			if (*cp == ':')
				/* XXX - we handle just one char */
				bp->val[2] = *++cp - 'a', ++cp;
		} else {
			bp->val[0] = -1; /* no #'s: assume unit 0, no
					    sbus offset/adddress */
		}
		++bp;
		++nbootpath;
	}
	bp->name[0] = 0;
	
	bootpath_print(bootpath);
	
	/* Setup pointer to boot flags */
	OF_getprop(chosen, "bootargs", buf, sizeof(buf));
	cp = buf;

	/* Find start of boot flags */
	while (*cp) {
		while(*cp == ' ' || *cp == '\t') cp++;
		if (*cp == '-' || *cp == '\0')
			break;
		while(*cp != ' ' && *cp != '\t' && *cp != '\0') cp++;
		
	}
	if (*cp != '-')
		return;

	for (;*++cp;) {
		int fl;

		fl = 0;
		BOOT_FLAG(*cp, fl);
		if (!fl) {
			printf("unknown option `%c'\n", *cp);
			continue;
		}
		boothowto |= fl;

		/* specialties */
		if (*cp == 'd') {
#if defined(KGDB)
			kgdb_debug_panic = 1;
			kgdb_connect(1);
#elif defined(DDB)
			Debugger();
#else
			printf("kernel has no debugger\n");
#endif
		} else if (*cp == 't') {
			/* turn on traptrace w/o breaking into kdb */
			extern int trap_trace_dis;

			trap_trace_dis = 0;
		}
	}
}

/*
 * print out the bootpath
 * the %x isn't 0x%x because the Sun EPROMs do it this way, and
 * consistency with the EPROMs is probably better here.
 */

static void
bootpath_print(bp)
	struct bootpath *bp;
{
	printf("bootpath: ");
	while (bp->name[0]) {
		if (bp->val[0] == -1)
			printf("/%s%x", bp->name, bp->val[1]);
		else
			printf("/%s@%x,%x", bp->name, bp->val[0], bp->val[1]);
		if (bp->val[2] != 0)
			printf(":%c", bp->val[2] + 'a');
		bp++;
	}
	printf("\n");
}


/*
 * save or read a bootpath pointer from the boothpath store.
 *
 * XXX. required because of SCSI... we don't have control over the "sd"
 * device, so we can't set boot device there.   we patch in with
 * dk_establish(), and use this to recover the bootpath.
 */
struct bootpath *
bootpath_store(storep, bp)
	int storep;
	struct bootpath *bp;
{
	static struct bootpath *save;
	struct bootpath *retval;

	retval = save;
	if (storep)
		save = bp;

	return (retval);
}

/*
 * Set up the sd target mappings for non SUN4 PROMs.
 * Find out about the real SCSI target, given the PROM's idea of the
 * target of the (boot) device (i.e., the value in bp->v0val[0]).
 */
static void
crazymap(prop, map)
	char *prop;
	int *map;
{
	int i;

	/*
	 * Set up the identity mapping for old sun4 monitors
	 * and v[2-] OpenPROMs. Note: dkestablish() does the
	 * SCSI-target juggling for sun4 monitors.
	 */
	for (i = 0; i < 8; ++i)
		map[i] = i;
}

int
sd_crazymap(n)
	int	n;
{
	static int prom_sd_crazymap[8]; /* static: compute only once! */
	static int init = 0;

	if (init == 0) {
		crazymap("sd-targets", prom_sd_crazymap);
		init = 1;
	}
	return prom_sd_crazymap[n];
}

int
st_crazymap(n)
	int	n;
{
	static int prom_st_crazymap[8]; /* static: compute only once! */
	static int init = 0;

	if (init == 0) {
		crazymap("st-targets", prom_st_crazymap);
		init = 1;
	}
	return prom_st_crazymap[n];
}


/*
 * Determine mass storage and memory configuration for a machine.
 * We get the PROM's root device and make sure we understand it, then
 * attach it as `mainbus0'.  We also set up to handle the PROM `sync'
 * command.
 */
void
cpu_configure()
{

	/* build the bootpath */
	bootpath_build();

#if notyet
        /* FIXME FIXME FIXME  This is probably *WRONG!!!**/
        OF_set_callback(sync_crash);
#endif

	/* block clock interrupts and anything below */
	splclock();
	/* Enable device interrupts */
        setpstate(getpstate()|PSTATE_IE);

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("mainbus not configured");

	/* Enable device interrupts */
        setpstate(getpstate()|PSTATE_IE);

	(void)spl0();
}


void
cpu_rootconf()
{
	struct bootpath *bp;
	int bootpartition;

	bp = nbootpath == 0 ? NULL : &bootpath[nbootpath-1];
	if (bp == NULL)
		bootpartition = 0;
	else if (booted_device != bp->dev)
		bootpartition = 0;
	else
		bootpartition = bp->val[2];

	setroot(booted_device, bootpartition);
}

/*
 * Console `sync' command.  SunOS just does a `panic: zero' so I guess
 * no one really wants anything fancy...
 */
void
sync_crash()
{

	panic("PROM sync command");
}

char *
clockfreq(freq)
	long freq;
{
	char *p;
	static char buf[10];

	freq /= 1000;
	sprintf(buf, "%ld", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = buf + strlen(buf);
		sprintf(p, "%ld", freq);
		*p = '.';	/* now buf = %d.%3d */
	}
	return (buf);
}

/* ARGSUSED */
static int
mbprint(aux, name)
	void *aux;
	const char *name;
{
	struct mainbus_attach_args *ma = aux;

	if (name)
		aprint_normal("%s at %s", ma->ma_name, name);
	if (ma->ma_address)
		aprint_normal(" addr 0x%08lx", (u_long)ma->ma_address[0]);
	if (ma->ma_pri)
		aprint_normal(" ipl %d", ma->ma_pri);
	return (UNCONF);
}

int
findroot()
{
	register int node;

	if ((node = rootnode) == 0 && (node = OF_peer(0)) == 0)
		panic("no PROM root device");
	rootnode = node;
	return (node);
}

/*
 * Given a `first child' node number, locate the node with the given name.
 * Return the node number, or 0 if not found.
 */
int
findnode(first, name)
	int first;
	register const char *name;
{
	int node;
	char buf[32];

	for (node = first; node; node = OF_peer(node)) {
		if ((OF_getprop(node, "name", buf, sizeof(buf)) > 0) &&
			(strcmp(buf, name) == 0))
			return (node);
	}
	return (0);
}

int
mainbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{

	return (1);
}

int autoconf_nzs = 0;	/* must be global so obio.c can see it */

/*
 * Attach the mainbus.
 *
 * Our main job is to attach the CPU (the root node we got in configure())
 * and iterate down the list of `mainbus devices' (children of that node).
 * We also record the `node id' of the default frame buffer, if any.
 */
static void
mainbus_attach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
extern struct sparc_bus_dma_tag mainbus_dma_tag;
extern struct sparc_bus_space_tag mainbus_space_tag;

	struct mainbus_attach_args ma;
	char buf[32];
	const char *const *ssp, *sp = NULL;
	int node0, node, rv;

	static const char *const openboot_special[] = {
		/* ignore these (end with NULL) */
		/*
		 * These are _root_ devices to ignore. Others must be handled
		 * elsewhere.
		 */
		"virtual-memory",
		"aliases",
		"memory",
		"openprom",
		"options",
		"packages",
		"chosen",
		NULL
	};

	OF_getprop(findroot(), "name", machine_model, sizeof machine_model);
	printf(": %s\n", machine_model);

	/*
	 * Locate and configure the ``early'' devices.  These must be
	 * configured before we can do the rest.  For instance, the
	 * EEPROM contains the Ethernet address for the LANCE chip.
	 * If the device cannot be located or configured, panic.
	 */

	node = findroot();

	/* Establish the first component of the boot path */
	bootpath_store(1, bootpath);

	/* first early device to be configured is the cpu */
	for (node = OF_child(node); node; node = OF_peer(node)) {
		if (OF_getprop(node, "device_type", buf, sizeof(buf)) <= 0)
			continue;
		if (strcmp(buf, "cpu") != 0)
			continue;
		bzero(&ma, sizeof(ma));
		ma.ma_bustag = &mainbus_space_tag;
		ma.ma_dmatag = &mainbus_dma_tag;
		ma.ma_node = node;
		ma.ma_name = "cpu";
		config_found(dev, &ma, mbprint);
		break;
	}
	if (node == 0)
		panic("None of the CPUs found");

	node = findroot();	/* re-init root node */

	/* Find the "options" node */
	node0 = OF_child(node);
	optionsnode = findnode(node0, "options");
	if (optionsnode == 0)
		panic("no options in OPENPROM");

	/*
	 * Configure the devices, in PROM order.  Skip
	 * PROM entries that are not for devices, or which must be
	 * done before we get here.
	 */
	for (node = node0; node; node = OF_peer(node)) {
		int portid;

		DPRINTF(ACDB_PROBE, ("Node: %x", node));
		if ((OF_getprop(node, "device_type", buf, sizeof(buf)) > 0) &&
		    strcmp(buf, "cpu") == 0)
			continue;
		OF_getprop(node, "name", buf, sizeof(buf));
		DPRINTF(ACDB_PROBE, (" name %s\n", buf));
		for (ssp = openboot_special; (sp = *ssp) != NULL; ssp++)
			if (strcmp(buf, sp) == 0)
				break;
		if (sp != NULL)
			continue; /* an "early" device already configured */

		bzero(&ma, sizeof ma);
		ma.ma_bustag = &mainbus_space_tag;
		ma.ma_dmatag = &mainbus_dma_tag;
		ma.ma_name = buf;
		ma.ma_node = node;
		if (OF_getprop(node, "upa-portid", &portid, sizeof(portid)) !=
		    sizeof(portid)) 
			portid = -1;
		ma.ma_upaid = portid;

		if (PROM_getprop(node, "reg", sizeof(*ma.ma_reg), 
				 &ma.ma_nreg, (void**)&ma.ma_reg) != 0)
			continue;
#ifdef DEBUG
		if (autoconf_debug & ACDB_PROBE) {
			if (ma.ma_nreg)
				printf(" reg %08lx.%08lx\n",
					(long)ma.ma_reg->ur_paddr, 
					(long)ma.ma_reg->ur_len);
			else
				printf(" no reg\n");
		}
#endif
		rv = PROM_getprop(node, "interrupts", sizeof(*ma.ma_interrupts),
			&ma.ma_ninterrupts, (void**)&ma.ma_interrupts);
		if (rv != 0 && rv != ENOENT) {
			free(ma.ma_reg, M_DEVBUF);
			continue;
		}
#ifdef DEBUG
		if (autoconf_debug & ACDB_PROBE) {
			if (ma.ma_interrupts)
				printf(" interrupts %08x\n", *ma.ma_interrupts);
			else
				printf(" no interrupts\n");
		}
#endif
		rv = PROM_getprop(node, "address", sizeof(*ma.ma_address), 
			&ma.ma_naddress, (void**)&ma.ma_address);
		if (rv != 0 && rv != ENOENT) {
			free(ma.ma_reg, M_DEVBUF);
			if (ma.ma_ninterrupts)
				free(ma.ma_interrupts, M_DEVBUF);
			continue;
		}
#ifdef DEBUG
		if (autoconf_debug & ACDB_PROBE) {
			if (ma.ma_naddress)
				printf(" address %08x\n", *ma.ma_address);
			else
				printf(" no address\n");
		}
#endif
		(void) config_found(dev, (void *)&ma, mbprint);
		free(ma.ma_reg, M_DEVBUF);
		if (ma.ma_ninterrupts)
			free(ma.ma_interrupts, M_DEVBUF);
		if (ma.ma_naddress)
			free(ma.ma_address, M_DEVBUF);
	}
	/* Try to attach PROM console */
	bzero(&ma, sizeof ma);
	ma.ma_name = "pcons";
	(void) config_found(dev, (void *)&ma, mbprint);
}

CFATTACH_DECL(mainbus, sizeof(struct device),
    mainbus_match, mainbus_attach, NULL, NULL);

int
PROM_getprop(node, name, size, nitem, bufp)
	int	node;
	char	*name;
	size_t	size;
	int	*nitem;
	void	**bufp;
{
	void	*buf;
	long	len;

	*nitem = 0;
	len = PROM_getproplen(node, name);
	if (len <= 0)
		return (ENOENT);

	if ((len % size) != 0)
		return (EINVAL);

	buf = *bufp;
	if (buf == NULL) {
		/* No storage provided, so we allocate some */
		buf = malloc(len, M_DEVBUF, M_NOWAIT);
		if (buf == NULL)
			return (ENOMEM);
	}

	OF_getprop(node, name, buf, len);
	*bufp = buf;
	*nitem = len / size;
	return (0);
}


/*
 * Internal form of proplen().  Returns the property length.
 */
long
PROM_getproplen(node, name)
	int node;
	char *name;
{
	return (OF_getproplen(node, name));
}

/*
 * Return a string property.  There is a (small) limit on the length;
 * the string is fetched into a static buffer which is overwritten on
 * subsequent calls.
 */
char *
PROM_getpropstring(node, name)
	int node;
	char *name;
{
	static char stringbuf[32];

	return (PROM_getpropstringA(node, name, stringbuf));
}

/* Alternative PROM_getpropstring(), where caller provides the buffer */
char *
PROM_getpropstringA(node, name, buffer)
	int node;
	char *name;
	char *buffer;
{
	int blen;

	if (PROM_getprop(node, name, 1, &blen, (void **)&buffer) != 0)
		blen = 0;

	buffer[blen] = '\0';	/* usually unnecessary */
	return (buffer);
}

/*
 * Fetch an integer (or pointer) property.
 * The return value is the property, or the default if there was none.
 */
int
PROM_getpropint(node, name, deflt)
	int node;
	char *name;
	int deflt;
{
	int intbuf;

	

	if (OF_getprop(node, name, &intbuf, sizeof(intbuf)) != sizeof(intbuf))
		return (deflt);

	return (intbuf);
}

/*
 * OPENPROM functions.  These are here mainly to hide the OPENPROM interface
 * from the rest of the kernel.
 */
int
firstchild(node)
	int node;
{

	return OF_child(node);
}

int
nextsibling(node)
	int node;
{

	return OF_peer(node);
}

/* The following are used primarily in consinit() */

int
node_has_property(node, prop)	/* returns 1 if node has given property */
	register int node;
	register const char *prop;
{
	return (OF_getproplen(node, (caddr_t)prop) != -1);
}

#ifdef RASTERCONSOLE
/*
 * Try to figure out where the PROM stores the cursor row & column
 * variables.  Returns nonzero on error.
 */
int
romgetcursoraddr(rowp, colp)
	register int **rowp, **colp;
{
	cell_t row = NULL, col = NULL;

	OF_interpret("stdout @ is my-self addr line# addr column# ", 0, 2,
		&col, &row);
	/*
	 * We are running on a 64-bit machine, so these things point to
	 * 64-bit values.  To convert them to pointers to integers, add
	 * 4 to the address.
	 */
	*rowp = (int *)(row+4);
	*colp = (int *)(col+4);
	return (row == NULL || col == NULL);
}
#endif /* RASTERCONSOLE */

void
callrom()
{

	__asm __volatile("wrpr	%%g0, 0, %%tl" : );
	OF_enter();
}

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(name, unit)
	char *name;
	int unit;
{
	struct device *dev = alldevs.tqh_first;
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	sprintf(num, "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strcpy(fullname, name);
	strcat(fullname, num);

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = dev->dv_list.tqe_next) == NULL)
			return NULL;
	}
	return dev;
}


/*
 * Device registration used to determine the boot device.
 * 
 * Copied from the sparc port.
 */
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#define BUSCLASS_NONE		0
#define BUSCLASS_MAINBUS	1
#define BUSCLASS_IOMMU		2
#define BUSCLASS_OBIO		3
#define BUSCLASS_SBUS		4
#define BUSCLASS_VME		5
#define BUSCLASS_PCI		6
#define BUSCLASS_XDC		7
#define BUSCLASS_XYC		8
#define BUSCLASS_FDC		9

static int bus_class __P((struct device *));
static int dev_compatible __P((struct device *, void *, char *));
static int instance_match __P((struct device *, void *, struct bootpath *));
static void nail_bootdev __P((struct device *, struct bootpath *));

static struct {
	char	*name;
	int	class;
} bus_class_tab[] = {
	{ "mainbus",	BUSCLASS_MAINBUS },
	{ "upa",	BUSCLASS_MAINBUS },
	{ "psycho",	BUSCLASS_MAINBUS },
	{ "obio",	BUSCLASS_OBIO },
	{ "iommu",	BUSCLASS_IOMMU },
	{ "sbus",	BUSCLASS_SBUS },
	{ "xbox",	BUSCLASS_SBUS },
	{ "esp",	BUSCLASS_SBUS },
	{ "dma",	BUSCLASS_SBUS },
	{ "espdma",	BUSCLASS_SBUS },
	{ "ledma",	BUSCLASS_SBUS },
	{ "simba",	BUSCLASS_PCI },
	{ "ppb",	BUSCLASS_PCI },
	{ "pciide",	BUSCLASS_PCI },
	{ "siop",	BUSCLASS_PCI },
	{ "pci",	BUSCLASS_PCI },
	{ "fdc",	BUSCLASS_FDC },
};

/*
 * A list of driver names may have differently named PROM nodes.
 */
static struct {
	char	*name;
	char	*compat[6];
} dev_compat_tab[] = {
	{ "dma",	{ "espdma", NULL }},
	{ "isp",	{ "QLGC,isp", "PTI,isp", "ptiisp", "scsi", NULL }},
	{ "fdc",	{ "SUNW,fdtwo",	NULL }},
	{ "psycho",	{ "pci", NULL }},
	{ "wd",		{ "disk", "ide-disk", NULL }},
	{ "sd",		{ "disk", NULL }},
	{ "hme",	{ "SUNW,hme", "network", NULL }},
	{ "esp",	{ "SUNW,fas", "fas", NULL }},
	{ "siop",	{ "glm",  "SUNW,glm", NULL }},
	{ NULL,		{ NULL }},
};

int
dev_compatible(dev, aux, bpname)
	struct device *dev;
	void *aux;
	char *bpname;
{
	int i, j;

	/*
	 * Step 1:
	 *
	 * If this is a PCI device, find it's device class and try that.
	 */
	if ((bus_class(dev->dv_parent)) == BUSCLASS_PCI) {
		struct pci_attach_args *pa = aux;

		DPRINTF(ACDB_BOOTDEV,
			("\n%s: dev_compatible: checking PCI class %x\n",
				dev->dv_xname, pa->pa_class));

		switch (PCI_CLASS(pa->pa_class)) {
			/*
			 * We can only really have pci-pci bridges,
			 * disk controllers, or NICs on the bootpath.
			 */
		case PCI_CLASS_BRIDGE:
			if (PCI_SUBCLASS(pa->pa_class) != 
				PCI_SUBCLASS_BRIDGE_PCI)
				break;
			DPRINTF(ACDB_BOOTDEV,
				("\n%s: dev_compatible: comparing %s with %s\n",
					dev->dv_xname, bpname, "pci"));
			if (strcmp(bpname, "pci") == 0)
				return (0);
			break;
		case PCI_CLASS_MASS_STORAGE:
			if (PCI_SUBCLASS(pa->pa_class) == 
				PCI_SUBCLASS_MASS_STORAGE_IDE) {
				DPRINTF(ACDB_BOOTDEV,
					("\n%s: dev_compatible: "
						"comparing %s with %s\n",
						dev->dv_xname, bpname, "ide"));
				if (strcmp(bpname, "ide") == 0)
					return (0);
			}
			if (PCI_SUBCLASS(pa->pa_class) == 
				PCI_SUBCLASS_MASS_STORAGE_SCSI) {
				DPRINTF(ACDB_BOOTDEV,
					("\n%s: dev_compatible: "
						"comparing %s with %s\n",
						dev->dv_xname, bpname, "scsi"));
				if (strcmp(bpname, "scsi") == 0)
					return (0);
			}
			break;
		case PCI_CLASS_NETWORK:
			DPRINTF(ACDB_BOOTDEV,
				("\n%s: dev_compatible: comparing %s with %s\n",
					dev->dv_xname, bpname, "network"));
			if (strcmp(bpname, "network") == 0)
				return (0);
			break;
		default:
			break;
		}
	}

	/*
	 * Step 2:
	 * 
	 * Look through the list of equivalent names and see if any of them
	 * match.  This is a nasty O(n^2) operation.
	 */
	for (i = 0; dev_compat_tab[i].name != NULL; i++) {
		if (strcmp(dev->dv_cfdata->cf_name, 
			dev_compat_tab[i].name) == 0) {
			DPRINTF(ACDB_BOOTDEV,
				("\n%s: dev_compatible: translating %s\n",
					dev->dv_xname, dev_compat_tab[i].name));
			for (j = 0; dev_compat_tab[i].compat[j] != NULL; j++) {
				DPRINTF(ACDB_BOOTDEV,
					("\n%s: dev_compatible: "
						"comparing %s to %s\n",
						dev->dv_xname, bpname,
						dev_compat_tab[i].compat[j]));
				if (strcmp(bpname, 
					dev_compat_tab[i].compat[j]) == 0)
					return (0);
			}
		}
	}
	DPRINTF(ACDB_BOOTDEV,
		("\n%s: dev_compatible: no match\n",
			dev->dv_xname));
	return (1);
}

static int
bus_class(dev)
	struct device *dev;
{
	const char *name;
	int i, class;

	class = BUSCLASS_NONE;
	if (dev == NULL)
		return (class);

	name = dev->dv_cfdata->cf_name;
	for (i = sizeof(bus_class_tab)/sizeof(bus_class_tab[0]); i-- > 0;) {
		if (strcmp(name, bus_class_tab[i].name) == 0) {
			class = bus_class_tab[i].class;
			break;
		}
	}

	return (class);
}

int
instance_match(dev, aux, bp)
	struct device *dev;
	void *aux;
	struct bootpath *bp;
{
	struct mainbus_attach_args *ma;
	struct sbus_attach_args *sa;
	struct pci_attach_args *pa;

	/*
	 * Several devices are represented on bootpaths in one of
	 * two formats, e.g.:
	 *	(1) ../sbus@.../esp@<offset>,<slot>/sd@..  (PROM v3 style)
	 *	(2) /sbus0/esp0/sd@..                      (PROM v2 style)
	 *
	 * hence we fall back on a `unit number' check if the bus-specific
	 * instance parameter check does not produce a match.
	 *
	 * For PCI devices, we get:
	 *	../pci@../xxx@<dev>,<fn>/...
	 */

	/*
	 * Rank parent bus so we know which locators to check.
	 */
	switch (bus_class(dev->dv_parent)) {
	case BUSCLASS_MAINBUS:
		ma = aux;
		DPRINTF(ACDB_BOOTDEV,
		    ("instance_match: mainbus device, want %#x have %#x\n",
		    ma->ma_upaid, bp->val[0]));
		if (bp->val[0] == ma->ma_upaid)
			return (1);
		break;
	case BUSCLASS_SBUS:
		sa = aux;
		DPRINTF(ACDB_BOOTDEV, ("instance_match: sbus device, "
		    "want slot %#x offset %#x have slot %#x offset %#x\n",
		     bp->val[0], bp->val[1], sa->sa_slot, sa->sa_offset));
		if (bp->val[0] == sa->sa_slot && bp->val[1] == sa->sa_offset)
			return (1);
		break;
	case BUSCLASS_PCI:
		pa = aux;
		DPRINTF(ACDB_BOOTDEV, ("instance_match: pci device, "
		    "want dev %#x fn %#x have dev %#x fn %#x\n",
		     bp->val[0], bp->val[1], pa->pa_device, pa->pa_function));
		if (bp->val[0] == pa->pa_device &&
		    bp->val[1] == pa->pa_function)
			return (1);
		break;
	default:
		break;
	}

	if (bp->val[0] == -1 && bp->val[1] == dev->dv_unit)
		return (1);

	return (0);
}

struct device *booted_device;

void
nail_bootdev(dev, bp)
	struct device *dev;
	struct bootpath *bp;
{

	if (bp->dev != NULL)
		panic("device_register: already got a boot device: %s",
			bp->dev->dv_xname);

	/*
	 * Mark this bootpath component by linking it to the matched
	 * device. We pick up the device pointer in cpu_rootconf().
	 */
	booted_device = bp->dev = dev;

	/*
	 * Then clear the current bootpath component, so we don't spuriously
	 * match similar instances on other busses, e.g. a disk on
	 * another SCSI bus with the same target.
	 */
	bootpath_store(1, NULL);
}

void
device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	struct bootpath *bp = bootpath_store(0, NULL);
	const char *dvname;
	char *bpname;

	/*
	 * If device name does not match current bootpath component
	 * then there's nothing interesting to consider.
	 */
	if (bp == NULL)
		return;

	/*
	 * Translate device name to device class name in case the prom uses
	 * that.
	 */
	bpname = bp->name;
	dvname = dev->dv_cfdata->cf_name;
	DPRINTF(ACDB_BOOTDEV,
	    ("\n%s: device_register: dvname %s(%s) bpname %s\n",
	    dev->dv_xname, dvname, dev->dv_xname, bpname));

	/* First, match by name */
	if (strcmp(dvname, bpname) != 0) {
		if (dev_compatible(dev, aux, bpname) != 0)
			return;
	}

	if (bus_class(dev) != BUSCLASS_NONE) {
		/*
		 * A bus or controller device of sorts. Check instance
		 * parameters and advance boot path on match.
		 */
		if (instance_match(dev, aux, bp) != 0) {
			bp->dev = dev;
			bootpath_store(1, bp + 1);
			DPRINTF(ACDB_BOOTDEV, ("\t-- found bus controller %s\n",
			    dev->dv_xname));
			return;
		}
	} else if (strcmp(dvname, "le") == 0 ||
		   strcmp(dvname, "hme") == 0) {
		/*
		 * ethernet devices.
		 */
		if (instance_match(dev, aux, bp) != 0) {
			nail_bootdev(dev, bp);
			DPRINTF(ACDB_BOOTDEV, ("\t-- found ethernet controller %s\n",
			    dev->dv_xname));
			return;
		}
	} else if (strcmp(dvname, "sd") == 0 || strcmp(dvname, "cd") == 0) {
		/*
		 * A SCSI disk or cd; retrieve target/lun information
		 * from parent and match with current bootpath component.
		 * Note that we also have look back past the `scsibus'
		 * device to determine whether this target is on the
		 * correct controller in our boot path.
		 */
		struct scsipibus_attach_args *sa = aux;
		struct scsipi_periph *periph = sa->sa_periph;
		struct scsibus_softc *sbsc =
			(struct scsibus_softc *)dev->dv_parent;
		u_int target = bp->val[0];
		u_int lun = bp->val[1];

		/* Check the controller that this scsibus is on */
		if ((bp-1)->dev != sbsc->sc_dev.dv_parent)
			return;

		/*
		 * Bounds check: we know the target and lun widths.
		 */
		if (target >= periph->periph_channel->chan_ntargets ||
		    lun >= periph->periph_channel->chan_nluns) {
			printf("SCSI disk bootpath component not accepted: "
			       "target %u; lun %u\n", target, lun);
			return;
		}

		if (periph->periph_target == target &&
		    periph->periph_lun == lun) {
			nail_bootdev(dev, bp);
			DPRINTF(ACDB_BOOTDEV, ("\t-- found [cs]d disk %s\n",
			    dev->dv_xname));
			return;
		}
	} else if (strcmp("wd", dvname) == 0) {
		/* IDE disks. */
		struct ata_device *adev = aux;

		/*
		 * The PROM gives you names like "disk@1,0", where the first value
		 * appears to be both the drive & channel combined (channel * 2 +
		 * drive), and the second value we don't use (what is it anyway?)
		 */
		if ((adev->adev_channel * 2) + adev->adev_drv_data->drive ==
		    bp->val[0]) {
			nail_bootdev(dev, bp);
			DPRINTF(ACDB_BOOTDEV, ("\t-- found wd disk %s\n",
			    dev->dv_xname));
			return;
		}
	} else {
		/*
		 * Generic match procedure.
		 */
		if (instance_match(dev, aux, bp) != 0) {
			nail_bootdev(dev, bp);
			DPRINTF(ACDB_BOOTDEV, ("\t-- found generic device %s\n",
			    dev->dv_xname));
			return;
		}
	}
}
