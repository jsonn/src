/*	$NetBSD: sun3_startup.c,v 1.70.2.1 1997/10/30 05:33:48 mellon Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/user.h>
#include <sys/exec_aout.h>

#include <vm/vm.h>

#include <machine/control.h>
#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/dvma.h>
#include <machine/mon.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/idprom.h>
#include <machine/obio.h>
#include <machine/machdep.h>

#include <sun3/sun3/interreg.h>
#include <sun3/sun3/vector.h>

/* This is defined in locore.s */
extern char kernel_text[];

/* These are defined by the linker */
extern char etext[], edata[], end[];
char *esym;	/* DDB */

/* Basically a flag: "Do we have a VAC?" */
int cache_size;

/*
 * XXX: m68k common code needs these...
 * ... but this port does not need to deal with anything except
 * an mc68020, so these two variables are always ignored.
 * XXX: Need to do something about <m68k/include/cpu.h>
 */
int cputype = 0;	/* CPU_68020 */
int mmutype = 2;	/* MMU_SUN */

/*
 * Now our own stuff.
 */

unsigned char cpu_machine_id = 0;
char *cpu_string = NULL;
int cpu_has_vme = 0;

/*
 * XXX - Should empirically estimate the divisor...
 * Note that the value of delay_divisor is roughly
 * 2048 / cpuclock	(where cpuclock is in MHz).
 */
int delay_divisor = 82;		/* assume the fastest (3/260) */

extern int physmem;

struct user *proc0paddr;	/* proc[0] pcb address (u-area VA) */
extern struct pcb *curpcb;

/* First C code called by locore.s */
void _bootstrap __P((struct exec));

static void _verify_hardware __P((void));
static void _vm_init __P((struct exec *kehp));

#if defined(DDB) && !defined(SYMTAB_SPACE)
static void _save_symtab __P((struct exec *kehp));

/*
 * Preserve DDB symbols and strings by setting esym.
 */
static void
_save_symtab(kehp)
	struct exec *kehp;	/* kernel exec header */
{
	int x, *symsz, *strsz;
	char *endp;
	char *errdesc = "?";

	/*
	 * First, sanity-check the exec header.
	 */
	if ((kehp->a_midmag & 0xFFF0) != 0x0100) {
		errdesc = "magic";
		goto err;
	}
	/* Boundary between text and data varries a little. */
	x = kehp->a_text + kehp->a_data;
	if (x != (edata - kernel_text)) {
		errdesc = "a_text+a_data";
		goto err;
	}
	if (kehp->a_bss != (end - edata)) {
		errdesc = "a_bss";
		goto err;
	}
	if (kehp->a_entry != (int)kernel_text) {
		errdesc = "a_entry";
		goto err;
	}
	if (kehp->a_trsize || kehp->a_drsize) {
		errdesc = "a_Xrsize";
		goto err;
	}
	/* The exec header looks OK... */

	/* Check the symtab length word. */
	endp = end;
	symsz = (int*)endp;
	if (kehp->a_syms != *symsz) {
		errdesc = "a_syms";
		goto err;
	}
	endp += sizeof(int);	/* past length word */
	endp += *symsz;			/* past nlist array */

	/* Sanity-check the string table length. */
	strsz = (int*)endp;
	if ((*strsz < 4) || (*strsz > 0x80000)) {
		errdesc = "strsize";
		goto err;
	}

	/* Success!  We have a valid symbol table! */
	endp += *strsz;			/* past strings */
	esym = endp;
	return;

 err:
	mon_printf("_save_symtab: bad %s\n", errdesc);
}
#endif	/* DDB && !SYMTAB_SPACE */

/*
 * This function is called from _bootstrap() to initialize
 * pre-vm-sytem virtual memory.  All this really does is to
 * set virtual_avail to the first page following preloaded
 * data (i.e. the kernel and its symbol table) and special
 * things that may be needed very early (proc0 upages).
 * Once that is done, pmap_bootstrap() is called to do the
 * usual preparations for our use of the MMU.
 */
static void
_vm_init(kehp)
	struct exec *kehp;	/* kernel exec header */
{
	vm_offset_t nextva;

	/*
	 * First preserve our symbol table, which might have been
	 * loaded after our BSS area by the boot loader.  However,
	 * if DDB is not part of this kernel, ignore the symbols.
	 */
	esym = end;
#if defined(DDB) && !defined(SYMTAB_SPACE)
	/* This will advance esym past the symbols. */
	_save_symtab(kehp);
#endif

	/*
	 * Steal some special-purpose, already mapped pages.
	 */
	nextva = m68k_round_page(esym);

	/*
	 * Setup the u-area pages (stack, etc.) for proc0.
	 * This is done very early (here) to make sure the
	 * fault handler works in case we hit an early bug.
	 * (The fault handler may reference proc0 stuff.)
	 */
	proc0paddr = (struct user *) nextva;
	nextva += USPACE;
	bzero((caddr_t)proc0paddr, USPACE);
	proc0.p_addr = proc0paddr;

	/*
	 * Now that proc0 exists, make it the "current" one.
	 */
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	/* This does most of the real work. */
	pmap_bootstrap(nextva);
}

/*
 * Determine which Sun3 model we are running on.
 * We have to do this very early on the Sun3 because
 * pmap_bootstrap() needs to know if it should avoid
 * the video memory on the Sun3/50.
 */
static void
_verify_hardware()
{
	unsigned char machtype;
	int cpu_match = 0;

	idprom_init();

	machtype = identity_prom.idp_machtype;
	if ((machtype & CPU_ARCH_MASK) != SUN3_ARCH) {
		mon_printf("not a sun3?\n");
		sunmon_abort();
	}

	cpu_machine_id = machtype & SUN3_IMPL_MASK;
	switch (cpu_machine_id) {

	case SUN3_MACH_50 :
		cpu_match++;
		cpu_string = "50";
		delay_divisor = 128;	/* 16 MHz */
		break;

	case SUN3_MACH_60 :
		cpu_match++;
		cpu_string = "60";
		delay_divisor = 102;	/* 20 MHz */
		break;

	case SUN3_MACH_110:
		cpu_match++;
		cpu_string = "110";
		delay_divisor = 120;	/* 17 MHz */
		cpu_has_vme = TRUE;
		break;

	case SUN3_MACH_160:
		cpu_match++;
		cpu_string = "160";
		delay_divisor = 120;	/* 17 MHz */
		cpu_has_vme = TRUE;
		break;

	case SUN3_MACH_260:
		cpu_match++;
		cpu_string = "260";
		delay_divisor = 82; 	/* 25 MHz */
		cpu_has_vme = TRUE;
#ifdef	HAVECACHE
		cache_size = 0x10000;	/* 64K */
#endif
		break;

	case SUN3_MACH_E  :
		cpu_match++;
		cpu_string = "E";
		delay_divisor = 102;	/* 20 MHz  XXX: Correct? */
		cpu_has_vme = TRUE;
		break;

	default:
		mon_printf("unknown sun3 model\n");
		sunmon_abort();
	}
	if (!cpu_match) {
		mon_printf("kernel not configured for the Sun 3 model\n");
		sunmon_abort();
	}
}

/*
 * This is called from locore.s just after the kernel is remapped
 * to its proper address, but before the call to main().  The work
 * done here corresponds to various things done in locore.s on the
 * hp300 port (and other m68k) but which we prefer to do in C code.
 * Also do setup specific to the Sun PROM monitor and IDPROM here.
 */
void
_bootstrap(keh)
	struct exec keh;	/* kernel exec header */
{

	/* First, Clear BSS. */
	bzero(edata, end - edata);

	/* Set v_handler, get boothowto. */
	sunmon_init();

	/* Determine the Sun3 model. */
	_verify_hardware();

	/* handle kernel mapping, pmap_bootstrap(), etc. */
	_vm_init(&keh);

	/*
	 * Find and save OBIO mappings needed early,
	 * and call some init functions.
	 */
	obio_init();

	/* We now may enable the console.  (yea!) */
	cninit();

	/*
	 * Point interrupts/exceptions to our vector table.
	 * (Until now, we use the one setup by the PROM.)
	 *
	 * This is done after obio_init() / intreg_init() finds
	 * the interrupt register and disables the NMI clock so
	 * it will not cause "spurrious level 7" complaints.
	 * Done after _vm_init so the PROM can debug that.
	 */
	setvbr((void **)vector_table);

	/* Interrupts are enabled later, after autoconfig. */
}
