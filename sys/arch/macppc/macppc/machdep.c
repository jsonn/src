/*	$NetBSD: machdep.c,v 1.127.2.2 2004/09/18 14:37:08 skrll Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.127.2.2 2004/09/18 14:37:08 skrll Exp $");

#include "opt_compat_netbsd.h"
#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_ipkdb.h"
#include "opt_altivec.h"
#include "opt_multiprocessor.h"
#include "adb.h"
#include "zsc.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/user.h>
#include <sys/boot_flag.h>
#include <sys/ksyms.h>

#include <uvm/uvm_extern.h>

#include <net/netisr.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#ifdef KGDB
#include <sys/kgdb.h>
#endif
 
#ifdef IPKDB
#include <ipkdb/ipkdb.h>
#endif

#include <machine/autoconf.h>
#include <machine/powerpc.h>
#include <machine/trap.h>
#include <machine/bus.h>
#include <machine/fpu.h>
#include <powerpc/oea/bat.h>
#ifdef ALTIVEC
#include <powerpc/altivec.h>
#endif

#include <dev/cons.h>
#include <dev/ofw/openfirm.h>

#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wscons_callbacks.h>

#include <dev/usb/ukbdvar.h>

#include <macppc/dev/adbvar.h>

#if NZSC > 0
#include <machine/z8530var.h>
#endif

#include "ksyms.h"

extern int ofmsr;

char bootpath[256];
static int chosen;
struct pmap ofw_pmap;
int ofkbd_ihandle;

#if NKSYMS || defined(DDB) || defined(LKM)
void *startsym, *endsym;
#endif

struct ofw_translations {
	vaddr_t va;
	int len;
	paddr_t pa;
	int mode;
};

int ofkbd_cngetc(dev_t);
void cninit_kd(void);
int lcsplx(int);
int save_ofmap(struct ofw_translations *, int);
void restore_ofmap(struct ofw_translations *, int);
static void dumpsys(void);

void
initppc(startkernel, endkernel, args)
	u_int startkernel, endkernel;
	char *args;
{
	struct ofw_translations *ofmap;
	int ofmaplen;

	oea_batinit(0x80000000, BAT_BL_256M, 0xf0000000, BAT_BL_256M,
	    0x90000000, BAT_BL_256M, 0xa0000000, BAT_BL_256M,
	    0xb0000000, BAT_BL_256M, 0);
	oea_init(ext_intr);

	chosen = OF_finddevice("/chosen");

	ofmaplen = save_ofmap(NULL, 0);
	ofmap = alloca(ofmaplen);
	save_ofmap(ofmap, ofmaplen);

#ifdef	__notyet__		/* Needs some rethinking regarding real/virtual OFW */
	OF_set_callback(callback);
#endif

	ofmsr &= ~PSL_IP;

	/*
	 * Parse arg string.
	 */
#if NKSYMS || defined(DDB) || defined(LKM)
	memcpy(&startsym, args + strlen(args) + 1, sizeof(startsym));
	memcpy(&endsym, args + strlen(args) + 5, sizeof(endsym));
	if (startsym == NULL || endsym == NULL)
		startsym = endsym = NULL;
#endif

	strcpy(bootpath, args);
	args = bootpath;
	while (*++args && *args != ' ');
	if (*args) {
		*args++ = 0;
		while (*args)
			BOOT_FLAG(*args++, boothowto);
	}

	/*
	 * i386 port says, that this shouldn't be here,
	 * but I really think the console should be initialized
	 * as early as possible.
	 */
	consinit();

	/*
	 * Set the page size.
	 */
	uvm_setpagesize();

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);

	restore_ofmap(ofmap, ofmaplen);
}

int
save_ofmap(ofmap, maxlen)
	struct ofw_translations *ofmap;
	int maxlen;
{
	int mmui, mmu, len;

	OF_getprop(chosen, "mmu", &mmui, sizeof mmui);
	mmu = OF_instance_to_package(mmui);

	if (ofmap) {
		memset(ofmap, 0, maxlen);	/* to be safe */
		len = OF_getprop(mmu, "translations", ofmap, maxlen);
	} else
		len = OF_getproplen(mmu, "translations");

	return len;
}

void
restore_ofmap(ofmap, len)
	struct ofw_translations *ofmap;
	int len;
{
	int n = len / sizeof(struct ofw_translations);
	int i;

	pmap_pinit(&ofw_pmap);

	ofw_pmap.pm_sr[KERNEL_SR] = KERNEL_SEGMENT;
#ifdef KERNEL2_SR
	ofw_pmap.pm_sr[KERNEL2_SR] = KERNEL2_SEGMENT;
#endif

	for (i = 0; i < n; i++) {
		paddr_t pa = ofmap[i].pa;
		vaddr_t va = ofmap[i].va;
		int len = ofmap[i].len;

		if (va < 0xf0000000)			/* XXX */
			continue;

		while (len > 0) {
			pmap_enter(&ofw_pmap, va, pa, VM_PROT_ALL,
			    VM_PROT_ALL|PMAP_WIRED);
			pa += PAGE_SIZE;
			va += PAGE_SIZE;
			len -= PAGE_SIZE;
		}
	}
	pmap_update(&ofw_pmap);
}

/*
 * Machine dependent startup code.
 */
void
cpu_startup()
{
	oea_startup(NULL);
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	/*
	 * Initialize soft interrupt framework.
	 */
	softintr__init();
#endif
}

/*
 * consinit
 * Initialize system console.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;
	cninit();

#if NKSYMS || defined(DDB) || defined(LKM)
	ksyms_init((int)((u_int)endsym - (u_int)startsym), startsym, endsym);
#endif
#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif

#ifdef IPKDB
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif

#ifdef KGDB
#if NZSC > 0
	zs_kgdb_init();
#endif
	if (boothowto & RB_KDB)
		kgdb_connect(1);
#endif
}

/*
 * Crash dump handling.
 */

void
dumpsys()
{
	printf("dumpsys: TBD\n");
}

#ifndef __HAVE_GENERIC_SOFT_INTERRUPTS
#include "zsc.h"
#include "com.h"
/*
 * Soft tty interrupts.
 */
void
softserial()
{
#if NZSC > 0
	zssoft(NULL);
#endif
#if NCOM > 0
	comsoft();
#endif
}
#endif

#if 0
/*
 * Stray interrupts.
 */
void
strayintr(irq)
	int irq;
{
	log(LOG_ERR, "stray interrupt %d\n", irq);
}
#endif

/*
 * Halt or reboot the machine after syncing/dumping according to howto.
 */
void
cpu_reboot(howto, what)
	int howto;
	char *what;
{
	static int syncing;
	static char str[256];
	char *ap = str, *ap1 = ap;

	boothowto = howto;
	if (!cold && !(howto & RB_NOSYNC) && !syncing) {
		syncing = 1;
		vfs_shutdown();		/* sync */
		resettodr();		/* set wall clock */
	}

#ifdef MULTIPROCESSOR
	/* Halt other CPU.  XXX for now... */
	macppc_send_ipi(&cpu_info[1 - cpu_number()], MACPPC_IPI_HALT);
	delay(100000);	/* XXX */
#endif

	splhigh();

	if (!cold && (howto & RB_DUMP))
		dumpsys();

	doshutdownhooks();

	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NADB > 0
		delay(1000000);
		adb_poweroff();
		printf("WARNING: powerdown failed!\n");
#endif
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");

		/* flush cache for msgbuf */
		__syncicache((void *)msgbuf_paddr, round_page(MSGBUFSIZE));

		ppc_exit();
	}

	printf("rebooting\n\n");
	if (what && *what) {
		if (strlen(what) > sizeof str - 5)
			printf("boot string too large, ignored\n");
		else {
			strcpy(str, what);
			ap1 = ap = str + strlen(str);
			*ap++ = ' ';
		}
	}
	*ap++ = '-';
	if (howto & RB_SINGLE)
		*ap++ = 's';
	if (howto & RB_KDB)
		*ap++ = 'd';
	*ap++ = 0;
	if (ap[-2] == '-')
		*ap1 = 0;

	/* flush cache for msgbuf */
	__syncicache((void *)msgbuf_paddr, round_page(MSGBUFSIZE));

#if NADB > 0
	adb_restart();	/* not return */
#endif
	ppc_exit();
}

#if 0
/*
 * OpenFirmware callback routine
 */
void
callback(p)
	void *p;
{
	panic("callback");	/* for now			XXX */
}
#endif

int
lcsplx(ipl)
	int ipl;
{
	return spllower(ipl); 	/* XXX */
}

#include "akbd.h"
#include "ukbd.h"
#include "ofb.h"
#include "zstty.h"

void
cninit()
{
#if (NZSTTY > 0)
	struct consdev *cp;
#endif /* (NZSTTY > 0) */
	int stdout, node;
	char type[16];

	if (OF_getprop(chosen, "stdout", &stdout, sizeof(stdout))
	    != sizeof(stdout))
		goto nocons;

	node = OF_instance_to_package(stdout);
	memset(type, 0, sizeof(type));
	if (OF_getprop(node, "device_type", type, sizeof(type)) == -1)
		goto nocons;

#if NOFB > 0
	if (strcmp(type, "display") == 0) {
		cninit_kd();
		return;
	}
#endif /* NOFB > 0 */

#if NZSTTY > 0
	if (strcmp(type, "serial") == 0) {
		extern struct consdev consdev_zs;

		cp = &consdev_zs;
		(*cp->cn_probe)(cp);
		(*cp->cn_init)(cp);
		cn_tab = cp;

		return;
	}
#endif

nocons:
	return;
}

#if NOFB > 0
struct usb_kbd_ihandles {
	struct usb_kbd_ihandles *next;
	int ihandle;
};

void
cninit_kd()
{
	int stdin, node;
	char name[16];
#if NAKBD > 0
	int akbd;
#endif
#if NUKBD > 0
	struct usb_kbd_ihandles *ukbds;
	int ukbd;
#endif

	/*
	 * Attach the console output now (so we can see debugging messages,
	 * if any).
	 */
	ofb_cnattach();

	/*
	 * We must determine which keyboard type we have.
	 */
	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin))
	    != sizeof(stdin)) {
		printf("WARNING: no `stdin' property in /chosen\n");
		return;
	}

	node = OF_instance_to_package(stdin);
	memset(name, 0, sizeof(name));
	OF_getprop(node, "name", name, sizeof(name));
	if (strcmp(name, "keyboard") != 0) {
		printf("WARNING: stdin is not a keyboard: %s\n", name);
		return;
	}

#if NAKBD > 0
	memset(name, 0, sizeof(name));
	OF_getprop(OF_parent(node), "name", name, sizeof(name));
	if (strcmp(name, "adb") == 0) {
		printf("console keyboard type: ADB\n");
		akbd_cnattach();
		goto kbd_found;
	}
#endif

	/*
	 * It is not obviously an ADB keyboard. Could be USB,
	 * or ADB on some firmware versions (e.g.: iBook G4)
	 * This is not enough, we have a few more problems:
	 *
	 *	(1) The stupid Macintosh firmware uses a
	 *	    `psuedo-hid' (no typo) or `pseudo-hid',  
	 *	    which apparently merges all keyboards 
	 *	    input into a single input stream.  
	 *	    Because of this, we can't actually 
	 *	    determine which controller or keyboard 
	 *	    is really the console keyboard!
	 *
	 *	(2) Even if we could, the keyboard can be USB,
	 *	    and this requires a lot of the kernel to 
	 *	    be running in order for it to work.
	 *
	 * So, what we do is this:
	 *
	 *	(1) First check for OpenFirmware implementation
	 *	    that will not let us distinguish between 
	 *	    USB and ADB. In that situation, try attaching 
	 *	    anything as we can, and hope things get better 
	 *	    at autoconfiguration time.
	 *
	 *	(2) Assume the keyboard is USB.
	 *	    Tell the ukbd driver that it is the console.
	 *	    At autoconfiguration time, it will attach the
	 *	    first USB keyboard instance as the console
	 *	    keyboard.
	 *
	 *	(3) Until then, so that we have _something_, we
	 *	    use the OpenFirmware I/O facilities to read
	 *	    the keyboard.
	 */

	/*
	 * stdin is /pseudo-hid/keyboard.  There is no 
	 * `adb-kbd-ihandle or `usb-kbd-ihandles methods
	 * available. Try attaching as ADB.
	 *
	 * XXX This must be called before pmap_bootstrap().
	 */
	if (strcmp(name, "pseudo-hid") == 0) {
		printf("console keyboard type: unknown, assuming ADB\n");
#if NAKBD > 0
		akbd_cnattach();
#endif
		goto kbd_found;
	}

	/*
	 * stdin is /psuedo-hid/keyboard.  Test `adb-kbd-ihandle and
	 * `usb-kbd-ihandles to figure out the real keyboard(s).
	 *
	 * XXX This must be called before pmap_bootstrap().
	 */

#if NUKBD > 0
	if (OF_call_method("`usb-kbd-ihandles", stdin, 0, 1, &ukbds) >= 0 &&
	    ukbds != NULL && ukbds->ihandle != 0 &&
	    OF_instance_to_package(ukbds->ihandle) != -1) {
		printf("usb-kbd-ihandles matches\n");
		printf("console keyboard type: USB\n");
		ukbd_cnattach();
		goto kbd_found;
	}
	/* Try old method name. */
	if (OF_call_method("`usb-kbd-ihandle", stdin, 0, 1, &ukbd) >= 0 &&
	    ukbd != 0 &&
	    OF_instance_to_package(ukbd) != -1) {
		printf("usb-kbd-ihandle matches\n");
		printf("console keyboard type: USB\n");
		stdin = ukbd;
		ukbd_cnattach();
		goto kbd_found;
	}
#endif

#if NAKBD > 0
	if (OF_call_method("`adb-kbd-ihandle", stdin, 0, 1, &akbd) >= 0 &&
	    akbd != 0 &&
	    OF_instance_to_package(akbd) != -1) {
		printf("adb-kbd-ihandle matches\n");
		printf("console keyboard type: ADB\n");
		stdin = akbd;
		akbd_cnattach();
		goto kbd_found;
	}
#endif

#if NUKBD > 0
	/*
	 * XXX Old firmware does not have `usb-kbd-ihandles method.  Assume
	 * XXX USB keyboard anyway.
	 */
	printf("defaulting to USB...");
	printf("console keyboard type: USB\n");
	ukbd_cnattach();
	goto kbd_found;
#endif

	/*
	 * No keyboard is found.  Just return.
	 */
	printf("no console keyboard\n");
	return;

#if NAKBD + NUKBD > 0
kbd_found:
	/*
	 * XXX This is a little gross, but we don't get to call
	 * XXX wskbd_cnattach() twice.
	 */
	ofkbd_ihandle = stdin;
	wsdisplay_set_cons_kbd(ofkbd_cngetc, NULL, NULL);
#endif
}
#endif

/*
 * Bootstrap console keyboard routines, using OpenFirmware I/O.
 */
int
ofkbd_cngetc(dev)
	dev_t dev;
{
	u_char c = '\0';
	int len;

	do {
		len = OF_read(ofkbd_ihandle, &c, 1);
	} while (len != 1);

	return c;
}

#ifdef MULTIPROCESSOR
/*
 * Save a process's FPU state to its PCB.  The state is in another CPU
 * (though by the time our IPI is processed, it may have been flushed already).
 */
void
mp_save_fpu_lwp(l)
	struct lwp *l;
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct cpu_info *fpcpu;
	int i;

	/*
	 * Send an IPI to the other CPU with the data and wait for that CPU
	 * to flush the data.  Note that the other CPU might have switched
	 * to a different proc's FPU state by the time it receives the IPI,
	 * but that will only result in an unnecessary reload.
	 */

	fpcpu = pcb->pcb_fpcpu;
	if (fpcpu == NULL) {
		return;
	}
	macppc_send_ipi(fpcpu, MACPPC_IPI_FLUSH_FPU);

	/* Wait for flush. */
#if 0
	while (pcb->pcb_fpcpu)
		;
#else
	for (i = 0; i < 0x3fffffff; i++) {
		if (pcb->pcb_fpcpu == NULL)
			return;
	}
	printf("mp_save_fpu_proc{%d} pid = %d.%d, fpcpu->ci_cpuid = %d\n",
	    cpu_number(), l->l_proc->p_pid, l->l_lid, fpcpu->ci_cpuid);
	panic("mp_save_fpu_proc");
#endif
}

#ifdef ALTIVEC
/*
 * Save a process's AltiVEC state to its PCB.  The state may be in any CPU.
 * The process must either be curproc or traced by curproc (and stopped).
 * (The point being that the process must not run on another CPU during
 * this function).
 */
void
mp_save_vec_lwp(l)
	struct lwp *l;
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct cpu_info *veccpu;
	int i;

	/*
	 * Send an IPI to the other CPU with the data and wait for that CPU
	 * to flush the data.  Note that the other CPU might have switched
	 * to a different proc's AltiVEC state by the time it receives the IPI,
	 * but that will only result in an unnecessary reload.
	 */

	veccpu = pcb->pcb_veccpu;
	if (veccpu == NULL) {
		return;
	}
	macppc_send_ipi(veccpu, MACPPC_IPI_FLUSH_VEC);

	/* Wait for flush. */
#if 0
	while (pcb->pcb_veccpu)
		;
#else
	for (i = 0; i < 0x3fffffff; i++) {
		if (pcb->pcb_veccpu == NULL)
			return;
	}
	printf("mp_save_vec_lwp{%d} pid = %d.%d, veccpu->ci_cpuid = %d\n",
	    cpu_number(), l->l_proc->p_pid, l->l_lid, veccpu->ci_cpuid);
	panic("mp_save_vec_lwp");
#endif
}
#endif /* ALTIVEC */
#endif /* MULTIPROCESSOR */
