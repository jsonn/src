/*	$NetBSD: autoconf.c,v 1.113.2.1 2006/04/01 12:06:30 yamt Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: autoconf.c,v 1.113.2.1 2006/04/01 12:06:30 yamt Exp $");

#include "opt_ddb.h"
#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/msgbuf.h>
#include <sys/boot_flag.h>
#include <sys/ksyms.h>

#include <net/if.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>
#include <machine/sparc64.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/bootinfo.h>
#include <sparc64/sparc64/timerreg.h>

#include <dev/ata/atavar.h>
#include <dev/pci/pcivar.h>
#include <dev/sbus/sbusvar.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#ifdef RASTERCONSOLE
#error options RASTERCONSOLE is obsolete for sparc64 - remove it from your config file
#endif

#include "ksyms.h"

struct evcnt intr_evcnts[] = {
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "spur"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev1"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev2"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev3"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev4"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev5"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev6"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev7"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr",  "lev8"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev9"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "clock"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev11"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev12"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "lev13"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr", "prof"),
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "intr",  "lev15")
};

int printspl = 0;
void *bootinfo = 0;

#ifdef KGDB
extern	int kgdb_debug_panic;
#endif

char	machine_model[100];

static	char *str2hex(register char *, register int *);
static	int mbprint(void *, const char *);
static	void crazymap(const char *, int *);
int	st_crazymap(int);
void	sync_crash(void);
int	mainbus_match(struct device *, struct cfdata *, void *);
static	void mainbus_attach(struct device *, struct device *, void *);
static  void get_ncpus(void);

struct	bootpath bootpath[8];
int	nbootpath;
static	void bootpath_build(void);
static	void bootpath_print(struct bootpath *);

/*
 * Kernel 4MB mappings.
 */
struct tlb_entry *kernel_tlbs;
int kernel_tlb_slots;

/* Global interrupt mappings for all device types.  Match against the OBP
 * 'device_type' property. 
 */
struct intrmap intrmap[] = {
	{ "block",	PIL_FD },	/* Floppy disk */
	{ "serial",	PIL_SER },	/* zs */
	{ "scsi",	PIL_SCSI },
	{ "scsi-2",	PIL_SCSI },
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
matchbyname(struct device *parent, struct cfdata *cf, void *aux)
{
	printf("%s: WARNING: matchbyname\n", cf->cf_name);
	return (0);
}

/*
 * Convert hex ASCII string to a value.  Returns updated pointer.
 * Depends on ASCII order (this *is* machine-dependent code, you know).
 */
static char *
str2hex(register char *str, register int *vp)
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


static void
get_ncpus()
{
	int node;
	char sbuf[32];

	node = findroot();

	sparc_ncpus = 0;
	for (node = OF_child(node); node; node = OF_peer(node)) {
		if (OF_getprop(node, "device_type", sbuf, sizeof(sbuf)) <= 0)
			continue;
		if (strcmp(sbuf, "cpu") != 0)
			continue;
		sparc_ncpus++;
	}
}

/*
 * lookup_bootinfo:
 * Look up information in bootinfo of boot loader.
 */
void *
lookup_bootinfo(int type)
{
	struct btinfo_common *bt;
	char *help = bootinfo;

	/* Check for a bootinfo record first. */
	if (help == NULL)
		return (NULL);

	do {
		bt = (struct btinfo_common *)help;
		if (bt->type == type)
			return ((void *)help);
		help += bt->next;
	} while (bt->next != 0 &&
		(size_t)help < (size_t)bootinfo + BOOTINFO_SIZE);

	return (NULL);
}

/*
 * locore.s code calls bootstrap() just before calling main().
 *
 * What we try to do is as follows:
 * - Initialize PROM and the console
 * - Read in part of information provided by a bootloader and find out
 *   kernel load and end addresses
 * - Initialize ksyms
 * - Find out number of active CPUs
 * - Finalize the bootstrap by calling pmap_bootstrap() 
 *
 * We will try to run out of the prom until we get out of pmap_bootstrap().
 */
void
bootstrap(void *o0, void *bootargs, void *bootsize, void *o3, void *ofw)
{
	void *bi;
	long bmagic;

	struct btinfo_symtab *bi_sym;
	struct btinfo_count *bi_count;
	struct btinfo_kernend *bi_kend;
	struct btinfo_tlb *bi_tlb;

	extern void *romtba;
	extern void* get_romtba(void);
	extern void  OF_val2sym32(void *);
	extern void OF_sym2val32(void *);

	/* Save OpenFrimware entry point */
	romp   = ofw;
	romtba = get_romtba();

	prom_init();

	/* Initialize the PROM console so printf will not panic */
	(*cn_tab->cn_init)(cn_tab);

	printf("sparc64_init(%p, %p, %p, %p, %p)\n", o0, bootargs, bootsize,
			o3, ofw);

	/* Extract bootinfo pointer */
	if ((long)bootsize >= (4 * sizeof(uint64_t))) {
		/* Loaded by 64-bit bootloader */
		bi = (void*)(u_long)(((uint64_t*)bootargs)[3]);
		bmagic = (long)(((uint64_t*)bootargs)[0]);
	} else if ((long)bootsize >= (4 * sizeof(uint32_t))) {
		/* Loaded by 32-bit bootloader */
		bi = (void*)(u_long)(((uint32_t*)bootargs)[3]);
		bmagic = (long)(((uint32_t*)bootargs)[0]);
	} else {
		printf("Bad bootinfo size.\n"
				"This kernel requires NetBSD boot loader.\n");
		panic("sparc64_init.");
	}

	printf("sparc64_init: bmagic=%lx, bi=%p\n", bmagic, bi);

	/* Read in the information provided by NetBSD boot loader */
	if (SPARC_MACHINE_OPENFIRMWARE != bmagic) {
		printf("No bootinfo information.\n"
				"This kernel requires NetBSD boot loader.\n");
		panic("sparc64_init.");
	}

	bootinfo = (void*)(u_long)((uint64_t*)bi)[1];
	LOOKUP_BOOTINFO(bi_kend, BTINFO_KERNEND);

	if (bi_kend->addr == (vaddr_t)0) {
		panic("Kernel end address is not found in bootinfo.\n");
	}

#if NKSYMS || defined(DDB) || defined(LKM)
	LOOKUP_BOOTINFO(bi_sym, BTINFO_SYMTAB);
	ksyms_init(bi_sym->nsym, (int *)(u_long)bi_sym->ssym,
			(int *)(u_long)bi_sym->esym);
#ifdef DDB
#ifdef __arch64__
	/* This can only be installed on an 64-bit system cause otherwise our stack is screwed */
	OF_set_symbol_lookup(OF_sym2val, OF_val2sym);
#else
	OF_set_symbol_lookup(OF_sym2val32, OF_val2sym32);
#endif
#endif
#endif

	LOOKUP_BOOTINFO(bi_count, BTINFO_DTLB_SLOTS);
	kernel_tlb_slots = bi_count->count;
	LOOKUP_BOOTINFO(bi_tlb, BTINFO_DTLB);
	kernel_tlbs = &bi_tlb->tlb[0];

	get_ncpus();
	pmap_bootstrap(KERNBASE, bi_kend->addr);
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
	char sbuf[128];

	memset(bootpath, 0, sizeof(bootpath));
	bp = bootpath;

	/*
	 * Grab boot path from PROM
	 */
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "bootpath", sbuf, sizeof(sbuf));
	cp = sbuf;
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
					    sbus offset/address */
		}
		++bp;
		++nbootpath;
	}
	bp->name[0] = 0;
	
	bootpath_print(bootpath);
	
	/* Setup pointer to boot flags */
	OF_getprop(chosen, "bootargs", sbuf, sizeof(sbuf));
	cp = sbuf;

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
bootpath_print(struct bootpath *bp)
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
bootpath_store(int storep, struct bootpath *bp)
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
crazymap(const char *prop, int *map)
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
sd_crazymap(int n)
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
st_crazymap(int n)
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
clockfreq(long freq)
{
	char *p;
	static char sbuf[10];

	freq /= 1000;
	sprintf(sbuf, "%ld", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = sbuf + strlen(sbuf);
		sprintf(p, "%ld", freq);
		*p = '.';	/* now sbuf = %d.%3d */
	}
	return (sbuf);
}

/* ARGSUSED */
static int
mbprint(void *aux, const char *name)
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
mainbus_match(struct device *parent, struct cfdata *cf, void *aux)
{

	return (1);
}

/*
 * Attach the mainbus.
 *
 * Our main job is to attach the CPU (the root node we got in configure())
 * and iterate down the list of `mainbus devices' (children of that node).
 * We also record the `node id' of the default frame buffer, if any.
 */
static void
mainbus_attach(struct device *parent, struct device *dev, void *aux)
{
extern struct sparc_bus_dma_tag mainbus_dma_tag;
extern struct sparc_bus_space_tag mainbus_space_tag;

	struct mainbus_attach_args ma;
	char sbuf[32];
	const char *const *ssp, *sp = NULL;
	int node0, node, rv, i;

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
	prom_getidprom();
	printf(": %s: hostid %lx\n", machine_model, hostid);

	/*
	 * Locate and configure the ``early'' devices.  These must be
	 * configured before we can do the rest.  For instance, the
	 * EEPROM contains the Ethernet address for the LANCE chip.
	 * If the device cannot be located or configured, panic.
	 */
	if (sparc_ncpus == 0)
		panic("None of the CPUs found");

	/*
	 * Init static interrupt eventcounters
	 */
	for (i = 0; i < sizeof(intr_evcnts)/sizeof(intr_evcnts[0]); i++)
		evcnt_attach_static(&intr_evcnts[i]);

	node = findroot();

	/* Establish the first component of the boot path */
	bootpath_store(1, bootpath);

	/* first early device to be configured is the CPU */
	for (node = OF_child(node); node; node = OF_peer(node)) {
		if (OF_getprop(node, "device_type", sbuf, sizeof(sbuf)) <= 0)
			continue;
		if (strcmp(sbuf, "cpu") != 0)
			continue;
		memset(&ma, 0, sizeof(ma));
		ma.ma_bustag = &mainbus_space_tag;
		ma.ma_dmatag = &mainbus_dma_tag;
		ma.ma_node = node;
		ma.ma_name = "cpu";
		config_found(dev, &ma, mbprint);
	}

	node = findroot();	/* re-init root node */

	/* Find the "options" node */
	node0 = OF_child(node);

	/*
	 * Configure the devices, in PROM order.  Skip
	 * PROM entries that are not for devices, or which must be
	 * done before we get here.
	 */
	for (node = node0; node; node = OF_peer(node)) {
		int portid;

		DPRINTF(ACDB_PROBE, ("Node: %x", node));
		if ((OF_getprop(node, "device_type", sbuf, sizeof(sbuf)) > 0) &&
		    strcmp(sbuf, "cpu") == 0)
			continue;
		OF_getprop(node, "name", sbuf, sizeof(sbuf));
		DPRINTF(ACDB_PROBE, (" name %s\n", sbuf));
		for (ssp = openboot_special; (sp = *ssp) != NULL; ssp++)
			if (strcmp(sbuf, sp) == 0)
				break;
		if (sp != NULL)
			continue; /* an "early" device already configured */

		memset(&ma, 0, sizeof ma);
		ma.ma_bustag = &mainbus_space_tag;
		ma.ma_dmatag = &mainbus_dma_tag;
		ma.ma_name = sbuf;
		ma.ma_node = node;
		if (OF_getprop(node, "upa-portid", &portid, sizeof(portid)) !=
		    sizeof(portid)) 
			portid = -1;
		ma.ma_upaid = portid;

		if (prom_getprop(node, "reg", sizeof(*ma.ma_reg), 
				 &ma.ma_nreg, &ma.ma_reg) != 0)
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
		rv = prom_getprop(node, "interrupts", sizeof(*ma.ma_interrupts),
			&ma.ma_ninterrupts, &ma.ma_interrupts);
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
		rv = prom_getprop(node, "address", sizeof(*ma.ma_address), 
			&ma.ma_naddress, &ma.ma_address);
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
	memset(&ma, 0, sizeof ma);
	ma.ma_name = "pcons";
	(void) config_found(dev, (void *)&ma, mbprint);
}

CFATTACH_DECL(mainbus, sizeof(struct device),
    mainbus_match, mainbus_attach, NULL, NULL);


/*
 * Try to figure out where the PROM stores the cursor row & column
 * variables.  Returns nonzero on error.
 */
int
romgetcursoraddr(int **rowp, int **colp)
{
	cell_t row = 0UL, col = 0UL;

	OF_interpret("stdout @ is my-self addr line# addr column# ", 0, 2,
		&col, &row);
	/*
	 * We are running on a 64-bit machine, so these things point to
	 * 64-bit values.  To convert them to pointers to integers, add
	 * 4 to the address.
	 */
	*rowp = (int *)(intptr_t)(row+4);
	*colp = (int *)(intptr_t)(col+4);
	return (row == 0UL || col == 0UL);
}

#if 0
void callrom()
{

	__asm volatile("wrpr	%%g0, 0, %%tl" : );
	OF_enter();
}
#endif

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(const char *name, int unit)
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

static int bus_class(struct device *);
static int dev_compatible(struct device *, void *, char *);
static int instance_match(struct device *, void *, struct bootpath *);
static void nail_bootdev(struct device *, struct bootpath *);

static struct {
	const char *name;
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
	{ "isp",	BUSCLASS_PCI },
	{ "pciide",	BUSCLASS_PCI },
	{ "cmdide",	BUSCLASS_PCI },
	{ "aceride",	BUSCLASS_PCI },
	{ "siop",	BUSCLASS_PCI },
	{ "esiop",	BUSCLASS_PCI },
	{ "pci",	BUSCLASS_PCI },
	{ "fdc",	BUSCLASS_FDC },
};

/*
 * A list of driver names may have differently named PROM nodes.
 */
static struct {
	const char *name;
	const char *compat[6];
} dev_compat_tab[] = {
	{ "dma",	{ "espdma", NULL }},
	{ "isp",	{ "QLGC,isp", "PTI,isp", "ptiisp", "scsi",
			  "SUNW,isptwo", NULL }},
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
dev_compatible(struct device *dev, void *aux, char *bpname)
{
	int i, j;

	/*
	 * Step 1:
	 *
	 * If this is a PCI device, find it's device class and try that.
	 */
	if ((bus_class(device_parent(dev))) == BUSCLASS_PCI) {
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
			if (strcmp(bpname, "ethernet") == 0)
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
		if (device_is_a(dev, dev_compat_tab[i].name)) {
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
bus_class(struct device *dev)
{
	int i, class;

	class = BUSCLASS_NONE;
	if (dev == NULL)
		return (class);

	for (i = sizeof(bus_class_tab)/sizeof(bus_class_tab[0]); i-- > 0;) {
		if (device_is_a(dev, bus_class_tab[i].name)) {
			class = bus_class_tab[i].class;
			break;
		}
	}

	return (class);
}

int
instance_match(struct device *dev, void *aux, struct bootpath *bp)
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
	switch (bus_class(device_parent(dev))) {
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

	if (bp->val[0] == -1 && bp->val[1] == device_unit(dev))
		return (1);

	return (0);
}

void
nail_bootdev(struct device *dev, struct bootpath *bp)
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
device_register(struct device *dev, void *aux)
{
	struct bootpath *bp = bootpath_store(0, NULL);
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
	DPRINTF(ACDB_BOOTDEV,
	    ("\n%s: device_register: dvname %s(%s) bpname %s\n",
	    dev->dv_xname, device_cfdata(dev)->cf_name, dev->dv_xname, bpname));

	/* First, match by name */
	if (!device_is_a(dev, bpname)) {
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
	} else if (device_is_a(dev, "le") ||
		   device_is_a(dev, "hme") ||
		   device_is_a(dev, "tlp")) {

		/*
		 * ethernet devices.
		 */
		if (instance_match(dev, aux, bp) != 0) {
			nail_bootdev(dev, bp);
			DPRINTF(ACDB_BOOTDEV, ("\t-- found ethernet controller %s\n",
			    dev->dv_xname));
			return;
		}
	} else if (device_is_a(dev, "sd") ||
		   device_is_a(dev, "cd")) {
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
			(struct scsibus_softc *)device_parent(dev);
		u_int target = bp->val[0];
		u_int lun = bp->val[1];

		/* Check the controller that this scsibus is on */
		if ((bp-1)->dev != device_parent(&sbsc->sc_dev))
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
	} else if (device_is_a(dev, "wd")) {
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
