/*	$NetBSD: autoconf.c,v 1.64.2.1 1997/01/14 21:26:19 thorpej Exp $ */

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <net/if.h>

#include <dev/cons.h>

#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#ifdef SUN4
#include <machine/oldmon.h>
#include <machine/idprom.h>
#include <sparc/sparc/memreg.h>
#endif
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/pmap.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/timerreg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif


/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;		/* if 1, still working on cold-start */
int	fbnode;		/* node ID of ROM's console frame buffer */
int	optionsnode;	/* node ID of ROM's options */
int	mmu_3l;		/* SUN4_400 models have a 3-level MMU */

#ifdef KGDB
extern	int kgdb_debug_panic;
#endif

static	int rootnode;
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
static	void bootpath_fake __P((struct bootpath *, char *));
static	void bootpath_print __P((struct bootpath *));
int	search_prom __P((int, char *));

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
	struct confargs *ca = aux;

	if (CPU_ISSUN4) {
		printf("WARNING: matchbyname not valid on sun4!");
		printf("%s\n", cf->cf_driver->cd_name);
		return (0);
	}
	return (strcmp(cf->cf_driver->cd_name, ca->ca_ra.ra_name) == 0);
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

#ifdef SUN4
struct promvec promvecdat;
struct om_vector *oldpvec = (struct om_vector *)PROM_BASE;

struct idprom idprom;
void	getidprom __P((struct idprom *, int size));
#endif

/*
 * locore.s code calls bootstrap() just before calling main(), after double
 * mapping the kernel to high memory and setting up the trap base register.
 * We must finish mapping the kernel properly and glean any bootstrap info.
 */
void
bootstrap()
{
	int nregion = 0, nsegment = 0, ncontext = 0;
	extern int msgbufmapped;

#if defined(SUN4)
	if (CPU_ISSUN4) {
		extern void oldmon_w_cmd __P((u_long, char *));
		extern struct msgbuf *msgbufp;
		/*
		 * XXX
		 * Some boot programs mess up physical page 0, which
		 * is where we want to put the msgbuf. There's some
		 * room, so shift it over half a page.
		 */
		msgbufp = (struct msgbuf *)((caddr_t) msgbufp + 4096);

		/*
		 * XXX:
		 * The promvec is bogus. We need to build a
		 * fake one from scratch as soon as possible.
		 */
		bzero(&promvecdat, sizeof promvecdat);
		promvec = &promvecdat;

		promvec->pv_stdin = oldpvec->inSource;
		promvec->pv_stdout = oldpvec->outSink;
		promvec->pv_putchar = oldpvec->putChar;
		promvec->pv_putstr = oldpvec->fbWriteStr;
		promvec->pv_nbgetchar = oldpvec->mayGet;
		promvec->pv_getchar = oldpvec->getChar;
		promvec->pv_romvec_vers = 0;		/* eek! */
		promvec->pv_reboot = oldpvec->reBoot;
		promvec->pv_abort = oldpvec->abortEntry;
		promvec->pv_setctxt = oldpvec->setcxsegmap;
		promvec->pv_v0bootargs = (struct v0bootargs **)(oldpvec->bootParam);
		promvec->pv_halt = oldpvec->exitToMon;

		/*
		 * Discover parts of the machine memory organization
		 * that we need this early.
		 */
		if (oldpvec->romvecVersion >= 2)
			*oldpvec->vector_cmd = oldmon_w_cmd;
		getidprom(&idprom, sizeof(idprom));
		switch (cpumod = idprom.id_machine) {
		case SUN4_100:
			nsegment = 256;
			ncontext = 8;
			break;
		case SUN4_200:
			nsegment = 512;
			ncontext = 16;
			break;
		case SUN4_300:
			nsegment = 256;
			ncontext = 16;
			break;
		case SUN4_400:
			nsegment = 1024;
			ncontext = 64;
			nregion = 256;
			mmu_3l = 1;
			break;
		default:
			printf("bootstrap: sun4 machine type %2x unknown!\n",
			    idprom.id_machine);
			callrom();
		}
	}
#endif /* SUN4 */

#if defined(SUN4C)
	if (CPU_ISSUN4C) {
		register int node = findroot();
		nsegment = getpropint(node, "mmu-npmg", 128);
		ncontext = getpropint(node, "mmu-nctx", 8);
	}
#endif /* SUN4C */

#if defined (SUN4M)
	if (CPU_ISSUN4M) {
		nsegment = 0;
		cpumod = (u_int) getpsr() >> 24;
		mmumod = (u_int) lda(SRMMU_PCR, ASI_SRMMU) >> 28;
		/*
		 * We use the max. number of contexts on the micro and
		 * hyper SPARCs. The SuperSPARC would let us use up to 65536
		 * contexts (by powers of 2), but we keep it at 4096 since
		 * the table must be aligned to #context*4. With 4K contexts,
		 * we waste at most 16K of memory. Note that the context
		 * table is *always* page-aligned, so there can always be
		 * 1024 contexts without sacrificing memory space (given
		 * that the chip supports 1024 contexts).
		 *
		 * Currently known limits: MS2=256, HS=4096, SS=65536
		 * 	some old SS's=4096
		 *
		 * XXX Should this be a tuneable parameter?
		 */
		switch (mmumod) {
		case SUN4M_MMU_MS1:
			ncontext = 64;
			break;
		case SUN4M_MMU_MS:
			ncontext = 256;
			break;
		default:
			ncontext = 4096;
			break;
		}
	}
#endif /* SUN4M */

	pmap_bootstrap(ncontext, nregion, nsegment);
	msgbufmapped = 1;	/* enable message buffer */
#ifdef KGDB
	zs_kgdb_init();		/* XXX */
#endif
#ifdef DDB
	db_machine_init();
	ddb_init();
#endif

	/*
	 * On sun4ms we have to do some nasty stuff here. We need to map
	 * in the interrupt registers (since we need to find out where
	 * they are from the PROM, since they aren't in a fixed place), and
	 * disable all interrupts. We can't do this easily from locore
	 * since the PROM is ugly to use from assembly. We also need to map
	 * in the counter registers because we can't disable the level 14
	 * (statclock) interrupt, so we need a handler early on (ugh).
	 *
	 * NOTE: We *demand* the psl to stay at splhigh() at least until
	 * we get here. The system _cannot_ take interrupts until we map
	 * the interrupt registers.
	 */

#if defined(SUN4M)
#define getpte4m(va)	lda(((va) & 0xFFFFF000) | ASI_SRMMUFP_L3, ASI_SRMMUFP)

	/* First we'll do the interrupt registers */
	if (CPU_ISSUN4M) {
		register int node;
		struct romaux ra;
		register u_int pte;
		register int i;
		extern void setpte4m __P((u_int, u_int));
		extern struct timer_4m *timerreg_4m;
		extern struct counter_4m *counterreg_4m;

		if ((node = opennode("/obio/interrupt")) == 0)
		    if ((node=search_prom(findroot(),"interrupt"))==0)
			panic("bootstrap: could not get interrupt "
			      "node from prom");

		if (!romprop(&ra, "interrupt", node))
		    panic("bootstrap: could not get interrupt properties");
		if (ra.ra_nvaddrs < 2)
		    panic("bootstrap: less than 2 interrupt regs. available");
		if (ra.ra_nvaddrs > 5)
		    panic("bootstrap: cannot support capability of > 4 CPUs");

		for (i = 0; i < ra.ra_nvaddrs - 1; i++) {

			pte = getpte4m((u_int)ra.ra_vaddrs[i]);
			if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
			    panic("bootstrap: PROM has invalid mapping for "
				  "processor interrupt register %d",i);
			pte |= PPROT_S;

			/* Duplicate existing mapping */

			setpte4m(PI_INTR_VA + (_MAXNBPG * i), pte);
		}

		/*
		 * That was the processor register...now get system register;
		 * it is the last returned by the PROM
		 */
		pte = getpte4m((u_int)ra.ra_vaddrs[i]);
		if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
		    panic("bootstrap: PROM has invalid mapping for system "
			  "interrupt register");
		pte |= PPROT_S;

		setpte4m(SI_INTR_VA, pte);

		/* Now disable interrupts */
		ienab_bis(SINTR_MA);

		/* Send all interrupts to primary processor */
		*((u_int *)ICR_ITR) = 0;

#ifdef DEBUG
/*		printf("SINTR: mask: 0x%x, pend: 0x%x\n", *(int*)ICR_SI_MASK,
		       *(int*)ICR_SI_PEND);
*/
#endif

		/*
		 * Now map in the counters
		 * (XXX: fix for multiple CPUs! We assume 1)
		 * The processor register is the first; the system is the last.
		 * See also timerattach() in clock.c.
		 * This shouldn't be necessary; we ought to keep interrupts off
		 * and/or disable the (level 14) counter...
		 */

		if ((node = opennode("/obio/counter")) == 0)
		    if ((node=search_prom(findroot(),"counter"))==0)
			panic("bootstrap: could not find counter in OPENPROM");

		if (!romprop(&ra, "counter", node))
			panic("bootstrap: could not find counter properties");

		counterreg_4m = (struct counter_4m *)ra.ra_vaddrs[0];
		timerreg_4m = (struct timer_4m *)ra.ra_vaddrs[ra.ra_nvaddrs-1];
	}
#endif /* SUN4M */
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
 * }
 *
 */

static void
bootpath_build()
{
	register char *cp, *pp;
	register struct bootpath *bp;

	/*
	 * On SS1s, promvec->pv_v0bootargs->ba_argv[1] contains the flags
	 * that were given after the boot command.  On SS2s, pv_v0bootargs
	 * is NULL but *promvec->pv_v2bootargs.v2_bootargs points to
	 * "vmunix -s" or whatever.
	 * XXX	DO THIS BEFORE pmap_boostrap?
	 */
	bzero(bootpath, sizeof(bootpath));
	bp = bootpath;
	if (promvec->pv_romvec_vers < 2) {
		/*
		 * Grab boot device name and values.  build fake bootpath.
		 */
		cp = (*promvec->pv_v0bootargs)->ba_argv[0];

		if (cp != NULL)
			bootpath_fake(bp, cp);

		bootpath_print(bootpath);

		/* Setup pointer to boot flags */
		cp = (*promvec->pv_v0bootargs)->ba_argv[1];
		if (cp == NULL || *cp != '-')
			return;
	} else {
		/*
		 * Grab boot path from PROM
		 */
		cp = *promvec->pv_v2bootargs.v2_bootpath;
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
		cp = *promvec->pv_v2bootargs.v2_bootargs;
		if (cp == NULL)
			return;
		while (*cp != '-')
			if (*cp++ == '\0')
				return;
	}
	for (;;) {
		switch (*++cp) {

		case '\0':
			return;

		case 'a':
			boothowto |= RB_ASKNAME;
			break;

		case 'b':
			boothowto |= RB_DFLTROOT;
			break;

		case 'd':	/* kgdb - always on zs	XXX */
#ifdef KGDB
			boothowto |= RB_KDB;	/* XXX unused */
			kgdb_debug_panic = 1;
			kgdb_connect(1);
#else
			printf("kernel not compiled with KGDB\n");
#endif
			break;

		case 's':
			boothowto |= RB_SINGLE;
			break;
		}
	}
}

/*
 * Fake a ROM generated bootpath.
 * The argument `cp' points to a string such as "xd(0,0,0)netbsd"
 */

static void
bootpath_fake(bp, cp)
	struct bootpath *bp;
	char *cp;
{
	register char *pp;
	int v0val[3];

#define BP_APPEND(BP,N,V0,V1,V2) { \
	strcpy((BP)->name, N); \
	(BP)->val[0] = (V0); \
	(BP)->val[1] = (V1); \
	(BP)->val[2] = (V2); \
	(BP)++; \
	nbootpath++; \
}

#if defined(SUN4)
	if (CPU_ISSUN4M) {
		printf("twas brillig..\n");
		return;
	}
#endif

	pp = cp + 2;
	v0val[0] = v0val[1] = v0val[2] = 0;
	if (*pp == '(' 					/* for vi: ) */
 	    && *(pp = str2hex(++pp, &v0val[0])) == ','
	    && *(pp = str2hex(++pp, &v0val[1])) == ',')
		(void)str2hex(++pp, &v0val[2]);

#if defined(SUN4)
	if (CPU_ISSUN4) {
		char tmpname[8];

		/*
		 *  xylogics VME dev: xd, xy, xt
		 *  fake looks like: /vmel0/xdc0/xd@1,0
		 */
		if (cp[0] == 'x') {
			if (cp[1] == 'd') {/* xd? */
				BP_APPEND(bp, "vmel", -1, 0, 0);
			} else {
				BP_APPEND(bp, "vmes", -1, 0, 0);
			}
			sprintf(tmpname,"x%cc", cp[1]); /* e.g. xdc */
			BP_APPEND(bp, tmpname,-1, v0val[0], 0);
			sprintf(tmpname,"%c%c", cp[0], cp[1]);
			BP_APPEND(bp, tmpname,v0val[1], v0val[2], 0); /* e.g. xd */
			return;
		}

		/*
		 * ethernet: ie, le (rom supports only obio?)
		 * fake looks like: /obio0/le0
		 */
		if ((cp[0] == 'i' || cp[0] == 'l') && cp[1] == 'e')  {
			BP_APPEND(bp, "obio", -1, 0, 0);
			sprintf(tmpname,"%c%c", cp[0], cp[1]);
			BP_APPEND(bp, tmpname, -1, 0, 0);
			return;
		}

		/*
		 * scsi: sd, st, sr
		 * assume: 4/100 = sw: /obio0/sw0/sd@0,0:a
		 * 4/200 & 4/400 = si/sc: /vmes0/si0/sd@0,0:a
 		 * 4/300 = esp: /obio0/esp0/sd@0,0:a
		 * (note we expect sc to mimic an si...)
		 */
		if (cp[0] == 's' &&
			(cp[1] == 'd' || cp[1] == 't' || cp[1] == 'r')) {

			int  target, lun;

			switch (cpumod) {
			case SUN4_200:
			case SUN4_400:
				BP_APPEND(bp, "vmes", -1, 0, 0);
				BP_APPEND(bp, "si", -1, v0val[0], 0);
				break;
			case SUN4_100:
				BP_APPEND(bp, "obio", -1, 0, 0);
				BP_APPEND(bp, "sw", -1, v0val[0], 0);
				break;
			case SUN4_300:
				BP_APPEND(bp, "obio", -1, 0, 0);
				BP_APPEND(bp, "esp", -1, v0val[0], 0);
				break;
			default:
				panic("bootpath_fake: unknown cpumod %d",
				      cpumod);
			}
			/*
			 * Deal with target/lun encodings.
			 * Note: more special casing in dk_establish().
			 */
			if (oldpvec->monId[0] > '1') {
				target = v0val[1] >> 3; /* new format */
				lun    = v0val[1] & 0x7;
			} else {
				target = v0val[1] >> 2; /* old format */
				lun    = v0val[1] & 0x3;
			}
			sprintf(tmpname, "%c%c", cp[0], cp[1]);
			BP_APPEND(bp, tmpname, target, lun, v0val[2]);
			return;
		}

		return; /* didn't grok bootpath, no change */
	}
#endif /* SUN4 */

#if defined(SUN4C)
	/*
	 * sun4c stuff
	 */

	/*
	 * floppy: fd
	 * fake looks like: /fd@0,0:a
	 */
	if (cp[0] == 'f' && cp[1] == 'd') {
		/*
		 * Assume `fd(c,u,p)' means:
		 * partition `p' on floppy drive `u' on controller `c'
		 */
		BP_APPEND(bp, "fd", v0val[0], v0val[1], v0val[2]);
		return;
	}

	/*
	 * ethernet: le
	 * fake looks like: /sbus0/le0
	 */
	if (cp[0] == 'l' && cp[1] == 'e') {
		BP_APPEND(bp, "sbus", -1, 0, 0);
		BP_APPEND(bp, "le", -1, v0val[0], 0);
		return;
	}

	/*
	 * scsi: sd, st, sr
	 * fake looks like: /sbus0/esp0/sd@3,0:a
	 */
	if (cp[0] == 's' && (cp[1] == 'd' || cp[1] == 't' || cp[1] == 'r')) {
		char tmpname[8];
		int  target, lun;

		BP_APPEND(bp, "sbus", -1, 0, 0);
		BP_APPEND(bp, "esp", -1, v0val[0], 0);
		if (cp[1] == 'r')
			sprintf(tmpname, "cd"); /* netbsd uses 'cd', not 'sr'*/
		else
			sprintf(tmpname,"%c%c", cp[0], cp[1]);
		/* XXX - is TARGET/LUN encoded in v0val[1]? */
		target = v0val[1];
		lun = 0;
		BP_APPEND(bp, tmpname, target, lun, v0val[2]);
		return;
	}
#endif /* SUN4C */


	/*
	 * unknown; return
	 */

#undef BP_APPEND
}

/*
 * print out the bootpath
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
	char *propval;

	if (!CPU_ISSUN4 && promvec->pv_romvec_vers < 2) {
		/*
		 * Machines with real v0 proms have an `s[dt]-targets' property
		 * which contains the mapping for us to use. v2 proms donot
		 * require remapping.
		 */
		propval = getpropstring(optionsnode, prop);
		if (propval == NULL || strlen(propval) != 8) {
 build_default_map:
			printf("WARNING: %s map is bogus, using default\n",
				prop);
			for (i = 0; i < 8; ++i)
				map[i] = i;
			i = map[0];
			map[0] = map[3];
			map[3] = i;
			return;
		}
		for (i = 0; i < 8; ++i) {
			map[i] = propval[i] - '0';
			if (map[i] < 0 ||
			    map[i] >= 8)
				goto build_default_map;
		}
	} else {
		/*
		 * Set up the identity mapping for old sun4 monitors
		 * and v[2-] OpenPROMs. Note: dkestablish() does the
		 * SCSI-target juggling for sun4 monitors.
		 */
		for (i = 0; i < 8; ++i)
			map[i] = i;
	}
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

struct devnametobdevmaj sparc_nam2blk[] = {
	{ "xy",		3 },
	{ "sd",		7 },
	{ "xd",		10 },
	{ "st",		11 },
	{ "fd",		16 },
	{ "cd",		18 },
	{ NULL,		0 },
};

/*
 * Determine mass storage and memory configuration for a machine.
 * We get the PROM's root device and make sure we understand it, then
 * attach it as `mainbus0'.  We also set up to handle the PROM `sync'
 * command.
 */
void
configure()
{
	struct confargs oca;
	register int node = 0;
	register char *cp;
	struct bootpath *bp;
	struct device *bootdv;
	int bootpartition;

	/* build the bootpath */
	bootpath_build();

#if defined(SUN4)
	if (CPU_ISSUN4) {
		extern struct cfdata cfdata[];
		extern struct cfdriver memreg_cd, obio_cd;
		struct cfdata *cf, *memregcf = NULL;
		register short *p;
		struct rom_reg rr;

		for (cf = cfdata; memregcf==NULL && cf->cf_driver; cf++) {
			if (cf->cf_driver != &memreg_cd ||
				cf->cf_loc[0] == -1) /* avoid sun4m memreg0 */
				continue;
			/*
			 * On the 4/100 obio addresses must be mapped at
			 * 0x0YYYYYYY, but alias higher up (we avoid the
			 * alias condition because it causes pmap difficulties)
			 * XXX: We also assume that 4/[23]00 obio addresses
			 * must be 0xZYYYYYYY, where (Z != 0)
			 * make sure we get the correct memreg cfdriver!
			 */
			if (cpumod==SUN4_100 && (cf->cf_loc[0] & 0xf0000000))
				continue;
			if (cpumod!=SUN4_100 && !(cf->cf_loc[0] & 0xf0000000))
				continue;
			for (p = cf->cf_parents; memregcf==NULL && *p >= 0; p++)
				if (cfdata[*p].cf_driver == &obio_cd)
					memregcf = cf;
		}
		if (memregcf==NULL)
			panic("configure: no memreg found!");

		rr.rr_iospace = BUS_OBIO;
		rr.rr_paddr = (void *)memregcf->cf_loc[0];
		rr.rr_len = NBPG;
		par_err_reg = (u_int *)bus_map(&rr, NBPG, BUS_OBIO);
		if (par_err_reg == NULL)
			panic("configure: ROM hasn't mapped memreg!");
	}
#endif
#if defined(SUN4C)
	if (CPU_ISSUN4C) {
		node = findroot();
		cp = getpropstring(node, "device_type");
		if (strcmp(cp, "cpu") != 0)
			panic("PROM root device type = %s (need CPU)\n", cp);
	}
#endif
#if defined(SUN4M)
	if (CPU_ISSUN4M)
		node = findroot();
#endif

	*promvec->pv_synchook = sync_crash;

	oca.ca_ra.ra_node = node;
	oca.ca_ra.ra_name = cp = "mainbus";
	if (config_rootfound(cp, (void *)&oca) == NULL)
		panic("mainbus not configured");
	(void)spl0();

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */

	bp = nbootpath == 0 ? NULL : &bootpath[nbootpath-1];
	bootdv = bp == NULL ? NULL : bp->dev;
	bootpartition = bp == NULL ? 0 : bp->val[2];

	setroot(bootdv, bootpartition, sparc_nam2blk);
	swapconf();
	dumpconf();
	cold = 0;
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
	register int freq;
{
	register char *p;
	static char buf[10];

	freq /= 1000;
	sprintf(buf, "%d", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = buf + strlen(buf);
		sprintf(p, "%d", freq);
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
	register struct confargs *ca = aux;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	if (ca->ca_ra.ra_paddr)
		printf(" %saddr 0x%x", ca->ca_ra.ra_iospace ? "io" : "",
		    (int)ca->ca_ra.ra_paddr);
	return (UNCONF);
}

int
findroot()
{
	register int node;

	if ((node = rootnode) == 0 && (node = nextsibling(0)) == 0)
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
	register int node;

	for (node = first; node; node = nextsibling(node))
		if (strcmp(getpropstring(node, "name"), name) == 0)
			return (node);
	return (0);
}

/*
 * Fill in a romaux.  Returns 1 on success, 0 if the register property
 * was not the right size.
 */
int
romprop(rp, cp, node)
	register struct romaux *rp;
	const char *cp;
	register int node;
{
	register int len;
	union { char regbuf[256]; struct rom_reg rr[RA_MAXREG]; } u;
	static const char pl[] = "property length";

	bzero(u.regbuf, sizeof u);
	len = getprop(node, "reg", (void *)u.regbuf, sizeof(u.regbuf));
	if (len == -1 &&
	    node_has_property(node, "device_type") &&
	    strcmp(getpropstring(node, "device_type"), "hierarchical") == 0)
		len = 0;
	if (len % sizeof(struct rom_reg)) {
		printf("%s \"reg\" %s = %d (need multiple of %d)\n",
			cp, pl, len, sizeof(struct rom_reg));
		return (0);
	}
	if (len > RA_MAXREG * sizeof(struct rom_reg))
		printf("warning: %s \"reg\" %s %d > %d, excess ignored\n",
		    cp, pl, len, RA_MAXREG * sizeof(struct rom_reg));
	rp->ra_node = node;
	rp->ra_name = cp;
	rp->ra_nreg = len / sizeof(struct rom_reg);
	bcopy(u.rr, rp->ra_reg, len);

	len = getprop(node, "address", (void *)rp->ra_vaddrs,
		      sizeof(rp->ra_vaddrs));
	if (len == -1) {
		rp->ra_vaddr = 0;	/* XXX - driver compat */
		len = 0;
	}
	if (len & 3) {
		printf("%s \"address\" %s = %d (need multiple of 4)\n",
		    cp, pl, len);
		len = 0;
	}
	rp->ra_nvaddrs = len >> 2;

	len = getprop(node, "intr", (void *)&rp->ra_intr, sizeof rp->ra_intr);
	if (len == -1)
		len = 0;
	if (len & 7) {
		printf("%s \"intr\" %s = %d (need multiple of 8)\n",
		    cp, pl, len);
		len = 0;
	}
	rp->ra_nintr = len >>= 3;
	/* SPARCstation interrupts are not hardware-vectored */
	while (--len >= 0) {
		if (rp->ra_intr[len].int_vec) {
			printf("WARNING: %s interrupt %d has nonzero vector\n",
			    cp, len);
			break;
		}
#if defined(SUN4M)
		if (CPU_ISSUN4M) {
			/* What's in these high bits anyway? */
			rp->ra_intr[len].int_pri &= 0xf;
			/* Look at "interrupts" property too? */
		}
#endif

	}
#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		len = getprop(node, "ranges", (void *)&rp->ra_range,
			      sizeof rp->ra_range);
		if (len == -1)
			len = 0;
		rp->ra_nrange = len / sizeof(struct rom_range);
	} else
#endif
		rp->ra_nrange = 0;

	return (1);
}

int
mainbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
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
	struct confargs oca;
	register const char *const *ssp, *sp = NULL;
#if defined(SUN4C) || defined(SUN4M)
	struct confargs *ca = aux;
	register int node0, node;
	const char *const *openboot_special;
#define L1A_HACK		/* XXX hack to allow L1-A during autoconf */
#ifdef L1A_HACK
	int audio = 0;
#endif
#endif
#if defined(SUN4)
	static const char *const oldmon_special[] = {
		"vmel",
		"vmes",
		NULL
	};
#endif /* SUN4 */

#if defined(SUN4C)
	static const char *const openboot_special4c[] = {
		/* find these first (end with empty string) */
		"memory-error",	/* as early as convenient, in case of error */
		"eeprom",
		"counter-timer",
		"auxiliary-io",
		"",

		/* ignore these (end with NULL) */
		"aliases",
		"interrupt-enable",
		"memory",
		"openprom",
		"options",
		"packages",
		"virtual-memory",
		NULL
	};
#else
#define openboot_special4c	((void *)0)
#endif
#if defined(SUN4M)
	static const char *const openboot_special4m[] = {
		/* find these first */
		"obio",		/* smart enough to get eeprom/etc mapped */
		"",

		/* ignore these (end with NULL) */
		/*
		 * These are _root_ devices to ignore. Others must be handled
		 * elsewhere.
		 */
		"SUNW,sx",		/* XXX: no driver for SX yet */
		"eccmemctl",
		"virtual-memory",
		"aliases",
		"memory",
		"openprom",
		"options",
		"packages",
		/* we also skip any nodes with device_type == "cpu" */
		NULL
	};
#else
#define openboot_special4m	((void *)0)
#endif

#if defined(SUN4M)
	if (CPU_ISSUN4M)
		printf(": %s", getpropstring(ca->ca_ra.ra_node, "name"));
#endif
	printf("\n");

	/*
	 * Locate and configure the ``early'' devices.  These must be
	 * configured before we can do the rest.  For instance, the
	 * EEPROM contains the Ethernet address for the LANCE chip.
	 * If the device cannot be located or configured, panic.
	 */

#if defined(SUN4)
	if (CPU_ISSUN4) {
		/* Configure the CPU. */
		bzero(&oca, sizeof(oca));
		oca.ca_ra.ra_name = "cpu";
		(void)config_found(dev, (void *)&oca, mbprint);

		/* Start at the beginning of the bootpath */
		bzero(&oca, sizeof(oca));
		oca.ca_ra.ra_bp = bootpath;

		oca.ca_bustype = BUS_MAIN;
		oca.ca_ra.ra_name = "obio";
		if (config_found(dev, (void *)&oca, mbprint) == NULL)
			panic("obio missing");

		for (ssp = oldmon_special; (sp = *ssp) != NULL; ssp++) {
			oca.ca_bustype = BUS_MAIN;
			oca.ca_ra.ra_name = sp;
			(void)config_found(dev, (void *)&oca, mbprint);
		}
		return;
	}
#endif

/*
 * The rest of this routine is for OBP machines exclusively.
 */
#if defined(SUN4C) || defined(SUN4M)

	openboot_special = CPU_ISSUN4M
				? openboot_special4m
				: openboot_special4c;

	node = ca->ca_ra.ra_node;	/* i.e., the root node */

	/* the first early device to be configured is the cpu */
#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		/* XXX - what to do on multiprocessor machines? */
		register const char *cp;

		for (node = firstchild(node); node; node = nextsibling(node)) {
			cp = getpropstring(node, "device_type");
			if (strcmp(cp, "cpu") == 0)
				break;
		}
		if (node == 0)
			panic("None of the CPUs found\n");
	}
#endif

	oca.ca_ra.ra_node = node;
	oca.ca_ra.ra_name = "cpu";
	oca.ca_ra.ra_paddr = 0;
	oca.ca_ra.ra_nreg = 0;
	config_found(dev, (void *)&oca, mbprint);

	node = ca->ca_ra.ra_node;	/* re-init root node */

	if (promvec->pv_romvec_vers <= 2)
		/* remember which frame buffer, if any, is to be /dev/fb */
		fbnode = getpropint(node, "fb", 0);

	/* Find the "options" node */
	node0 = firstchild(node);
	optionsnode = findnode(node0, "options");
	if (optionsnode == 0)
		panic("no options in OPENPROM");

	/* Start at the beginning of the bootpath */
	oca.ca_ra.ra_bp = bootpath;

	for (ssp = openboot_special; *(sp = *ssp) != 0; ssp++) {
		if ((node = findnode(node0, sp)) == 0) {
			printf("could not find %s in OPENPROM\n", sp);
			panic(sp);
		}
		oca.ca_bustype = BUS_MAIN;
		if (!romprop(&oca.ca_ra, sp, node) ||
		    (config_found(dev, (void *)&oca, mbprint) == NULL))
			panic(sp);
	}

	/*
	 * Configure the rest of the devices, in PROM order.  Skip
	 * PROM entries that are not for devices, or which must be
	 * done before we get here.
	 */
	for (node = node0; node; node = nextsibling(node)) {
		register const char *cp;

#if defined(SUN4M)
		if (CPU_ISSUN4M) /* skip the CPUs */
			if (node_has_property(node, "device_type") &&
			    !strcmp(getpropstring(node, "device_type"), "cpu"))
				continue;
#endif
		cp = getpropstring(node, "name");
		for (ssp = openboot_special; (sp = *ssp) != NULL; ssp++)
			if (strcmp(cp, sp) == 0)
				break;
		if (sp == NULL && romprop(&oca.ca_ra, cp, node)) {
#ifdef L1A_HACK
			if (strcmp(cp, "audio") == 0)
				audio = 1;
			if (strcmp(cp, "zs") == 0)
				autoconf_nzs++;
			if (/*audio &&*/ autoconf_nzs >= 2)	/*XXX*/
				(void) splx(11 << 8);		/*XXX*/
#endif
			oca.ca_bustype = BUS_MAIN;
			(void) config_found(dev, (void *)&oca, mbprint);
		}
	}
#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		/* Enable device interrupts */
		ienab_bic(SINTR_MA);
	}
#endif
#endif /* SUN4C || SUN4M */
}

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

/*
 * findzs() is called from the zs driver (which is, at least in theory,
 * generic to any machine with a Zilog ZSCC chip).  It should return the
 * address of the corresponding zs channel.  It may not fail, and it
 * may be called before the VM code can be used.  Here we count on the
 * FORTH PROM to map in the required zs chips.
 */
void *
findzs(zs)
	int zs;
{

#if defined(SUN4)
#define ZS0_PHYS	0xf1000000
#define ZS1_PHYS	0xf0000000
#define ZS2_PHYS	0xe0000000

	if (CPU_ISSUN4) {
		struct rom_reg rr;
		register void *vaddr;

		switch (zs) {
		case 0:
			rr.rr_paddr = (void *)ZS0_PHYS;
			break;
		case 1:
			rr.rr_paddr = (void *)ZS1_PHYS;
			break;
		case 2:
			rr.rr_paddr = (void *)ZS2_PHYS;
			break;
		default:
			panic("findzs: unknown zs device %d", zs);
		}

		rr.rr_iospace = BUS_OBIO;
		rr.rr_len = NBPG;
		vaddr = bus_map(&rr, NBPG, BUS_OBIO);
		if (vaddr)
			return (vaddr);
	}
#endif

#if defined(SUN4C) || defined(SUN4M)
	if (CPU_ISSUN4COR4M) {
		register int node, addr;

		node = firstchild(findroot());
		if (CPU_ISSUN4M) { /* zs is in "obio" tree on Sun4M */
			node = findnode(node, "obio");
			if (!node)
			    panic("findzs: no obio node");
			node = firstchild(node);
		}
		while ((node = findnode(node, "zs")) != 0) {
			if (getpropint(node, "slave", -1) == zs) {
				if ((addr = getpropint(node, "address", 0)) == 0)
					panic("findzs: zs%d not mapped by PROM", zs);
				return ((void *)addr);
			}
			node = nextsibling(node);
		}
	}
#endif
	panic("findzs: cannot find zs%d", zs);
	/* NOTREACHED */
}

int
makememarr(ap, max, which)
	register struct memarr *ap;
	int max, which;
{
#if defined(SUN4C) || defined(SUN4M)
	struct v2rmi {
		int	zero;
		int	addr;
		int	len;
	} v2rmi[200];		/* version 2 rom meminfo layout */
#define	MAXMEMINFO (sizeof(v2rmi) / sizeof(*v2rmi))
	register struct v0mlist *mp;
	register int i, node, len;
	char *prop;
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		switch (which) {
		case MEMARR_AVAILPHYS:
			ap[0].addr = 0;
			ap[0].len = *oldpvec->memoryAvail;
			break;
		case MEMARR_TOTALPHYS:
			ap[0].addr = 0;
			ap[0].len = *oldpvec->memorySize;
			break;
		default:
			printf("pre_panic: makememarr");
			break;
		}
		return (1);
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	switch (i = promvec->pv_romvec_vers) {

	case 0:
		/*
		 * Version 0 PROMs use a linked list to describe these
		 * guys.
		 */
		switch (which) {

		case MEMARR_AVAILPHYS:
			mp = *promvec->pv_v0mem.v0_physavail;
			break;

		case MEMARR_TOTALPHYS:
			mp = *promvec->pv_v0mem.v0_phystot;
			break;

		default:
			panic("makememarr");
		}
		for (i = 0; mp != NULL; mp = mp->next, i++) {
			if (i >= max)
				goto overflow;
			ap->addr = (u_int)mp->addr;
			ap->len = mp->nbytes;
			ap++;
		}
		break;

	default:
		printf("makememarr: hope version %d PROM is like version 2\n",
		    i);
		/* FALLTHROUGH */

        case 3:
	case 2:
		/*
		 * Version 2 PROMs use a property array to describe them.
		 */
		if (max > MAXMEMINFO) {
			printf("makememarr: limited to %d\n", MAXMEMINFO);
			max = MAXMEMINFO;
		}
		if ((node = findnode(firstchild(findroot()), "memory")) == 0)
			panic("makememarr: cannot find \"memory\" node");
		switch (which) {

		case MEMARR_AVAILPHYS:
			prop = "available";
			break;

		case MEMARR_TOTALPHYS:
			prop = "reg";
			break;

		default:
			panic("makememarr");
		}
		len = getprop(node, prop, (void *)v2rmi, sizeof v2rmi) /
		    sizeof(struct v2rmi);
		for (i = 0; i < len; i++) {
			if (i >= max)
				goto overflow;
			ap->addr = v2rmi[i].addr;
			ap->len = v2rmi[i].len;
			ap++;
		}
		break;
	}

	/*
	 * Success!  (Hooray)
	 */
	if (i == 0)
		panic("makememarr: no memory found");
	return (i);

overflow:
	/*
	 * Oops, there are more things in the PROM than our caller
	 * provided space for.  Truncate any extras.
	 */
	printf("makememarr: WARNING: lost some memory\n");
	return (i);
#endif
}

/*
 * Internal form of getprop().  Returns the actual length.
 */
int
getprop(node, name, buf, bufsiz)
	int node;
	char *name;
	void *buf;
	register int bufsiz;
{
#if defined(SUN4C) || defined(SUN4M)
	register struct nodeops *no;
	register int len;
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		printf("WARNING: getprop not valid on sun4! %s\n", name);
		return (0);
	}
#endif

#if defined(SUN4C) || defined(SUN4M)
	no = promvec->pv_nodeops;
	len = no->no_proplen(node, name);
	if (len > bufsiz) {
		printf("node %x property %s length %d > %d\n",
		    node, name, len, bufsiz);
#ifdef DEBUG
		panic("getprop");
#else
		return (0);
#endif
	}
	no->no_getprop(node, name, buf);
	return (len);
#endif
}

/*
 * Return a string property.  There is a (small) limit on the length;
 * the string is fetched into a static buffer which is overwritten on
 * subsequent calls.
 */
char *
getpropstring(node, name)
	int node;
	char *name;
{
	register int len;
	static char stringbuf[32];

	len = getprop(node, name, (void *)stringbuf, sizeof stringbuf - 1);
	if (len == -1)
		len = 0;
	stringbuf[len] = '\0';	/* usually unnecessary */
	return (stringbuf);
}

/*
 * Fetch an integer (or pointer) property.
 * The return value is the property, or the default if there was none.
 */
int
getpropint(node, name, deflt)
	int node;
	char *name;
	int deflt;
{
	register int len;
	char intbuf[16];

	len = getprop(node, name, (void *)intbuf, sizeof intbuf);
	if (len != 4)
		return (deflt);
	return (*(int *)intbuf);
}

/*
 * OPENPROM functions.  These are here mainly to hide the OPENPROM interface
 * from the rest of the kernel.
 */
int
firstchild(node)
	int node;
{

	return (promvec->pv_nodeops->no_child(node));
}

int
nextsibling(node)
	int node;
{

	return (promvec->pv_nodeops->no_nextnode(node));
}

char    *strchr __P((const char *, int));
u_int      hexatoi __P((const char *));

/* The following recursively searches a PROM tree for a given node */
int
search_prom(rootnode, name)
        register int rootnode;
        register char *name;
{
        register int rtnnode;
        register int node = rootnode;

        if (node == findroot() || !strcmp("hierarchical",
                                          getpropstring(node, "device_type")))
            node = firstchild(node);

        if (!node)
            panic("search_prom: null node");

        do {
                if (strcmp(getpropstring(node, "name"),name) == 0)
                    return node;

                if (node_has_property(node,"device_type") &&
                    (!strcmp(getpropstring(node, "device_type"),"hierarchical")
                     || !strcmp(getpropstring(node, "name"),"iommu"))
                    && (rtnnode = search_prom(node, name)) != 0)
                        return rtnnode;

        } while ((node = nextsibling(node)));

        return 0;
}

/* The following are used primarily in consinit() */

int
opennode(path)		/* translate phys. device path to node */
	register char *path;
{
	register int fd;

	if (promvec->pv_romvec_vers < 2) {
		printf("WARNING: opennode not valid on sun4! %s\n", path);
		return (0);
	}
	fd = promvec->pv_v2devops.v2_open(path);
	if (fd == 0)
		return 0;
	return promvec->pv_v2devops.v2_fd_phandle(fd);
}

int
node_has_property(node, prop)	/* returns 1 if node has given property */
	register int node;
	register const char *prop;
{

	return ((*promvec->pv_nodeops->no_proplen)(node, (caddr_t)prop) != -1);
}

#ifdef RASTERCONSOLE
/* Pass a string to the FORTH PROM to be interpreted */
void
rominterpret(s)
	register char *s;
{

	if (promvec->pv_romvec_vers < 2)
		promvec->pv_fortheval.v0_eval(strlen(s), s);
	else
		promvec->pv_fortheval.v2_eval(s);
}

/*
 * Try to figure out where the PROM stores the cursor row & column
 * variables.  Returns nonzero on error.
 */
int
romgetcursoraddr(rowp, colp)
	register int **rowp, **colp;
{
	char buf[100];

	/*
	 * line# and column# are global in older proms (rom vector < 2)
	 * and in some newer proms.  They are local in version 2.9.  The
	 * correct cutoff point is unknown, as yet; we use 2.9 here.
	 */
	if (promvec->pv_romvec_vers < 2 || promvec->pv_printrev < 0x00020009)
		sprintf(buf,
		    "' line# >body >user %lx ! ' column# >body >user %lx !",
		    (u_long)rowp, (u_long)colp);
	else
		sprintf(buf,
		    "stdout @ is my-self addr line# %lx ! addr column# %lx !",
		    (u_long)rowp, (u_long)colp);
	*rowp = *colp = NULL;
	rominterpret(buf);
	return (*rowp == NULL || *colp == NULL);
}
#endif

void
romhalt()
{
	if (CPU_ISSUN4COR4M)
		*promvec->pv_synchook = NULL;

	promvec->pv_halt();
	panic("PROM exit failed");
}

void
romboot(str)
	char *str;
{
	if (CPU_ISSUN4COR4M)
		*promvec->pv_synchook = NULL;

	promvec->pv_reboot(str);
	panic("PROM boot failed");
}

void
callrom()
{

#if 0			/* sun4c FORTH PROMs do this for us */
	if (CPU_ISSUN4)
		fb_unblank();
#endif
	promvec->pv_abort();
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
 * The $#!@$&%# kernel library doesn't have strchr or atoi. Ugh. We duplicate
 * here.
 */

char *
strchr(p, ch)			/* cribbed from libc */
	register const char *p, ch;
{
	for (;; ++p) {
		if (*p == ch)
			return((char *)p);
		if (!*p)
			return((char *)NULL);
	}
	/* NOTREACHED */
}

u_int
hexatoi(nptr)			/* atoi assuming hex, no 0x */
	const char *nptr;
{
	u_int retval;
	str2hex((char *)nptr, &retval);
	return retval;
}

