/*	$NetBSD: machdep.c,v 1.117.2.1 1998/08/08 03:06:42 eeh Exp $ */

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 *	@(#)machdep.c	8.6 (Berkeley) 1/14/94
 */

#include "opt_uvm.h"
#include "opt_compat_sunos.h"

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/extent.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#if defined(UVM)
#include <uvm/uvm.h> /* XXX: not _extern ... need vm_map_create */
#endif

#include <sys/sysctl.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/oldmon.h>
#include <machine/bsd_openprom.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>

#include "fb.h"
#include "power.h"

#if NPOWER > 0
#include <sparc/dev/power.h>
#endif

#if defined(UVM)
vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;
#else
vm_map_t buffer_map;
#endif
extern vm_offset_t avail_end;

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

int	physmem;

extern	caddr_t msgbufaddr;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

/*
 * dvmamap is used to manage DVMA memory. Note: this coincides with
 * the memory range in `phys_map' (which is mostly a place-holder).
 */
vm_offset_t dvma_base, dvma_end;
struct map *dvmamap;
static int ndvmamap;	/* # of entries in dvmamap */
struct extent *dvmamap24;

caddr_t allocsys __P((caddr_t));
void	dumpsys __P((void));
void	stackdump __P((void));

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	register unsigned i;
	register caddr_t v;
	register int sz;
	int base, residual;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
#endif
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
	extern struct user *proc0paddr;

#ifdef DEBUG
	pmapdebug = 0;
#endif

	/*
	 * Map the message buffer (physical location 0).
	 */
	pmap_enter(pmap_kernel(), MSGBUF_VA, 0x0,
		   VM_PROT_READ|VM_PROT_WRITE, 1);

	/*
	 * XXX - sun4
	 * Some boot programs mess up physical page 0, which
	 * is where we want to put the msgbuf. There's some
	 * room, so shift it over half a page.
	 */
	initmsgbuf((caddr_t)(MSGBUF_VA + (CPU_ISSUN4 ? 4096 : 0)), MSGBUFSIZE);

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	/*identifycpu();*/
#if !defined(MACHINE_NEW_NONCONTIG) && !defined(MACHINE_NONCONTIG)
	physmem = btoc(avail_end);
#endif
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);

#if defined(UVM)
	if ((v = (caddr_t)uvm_km_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
#else
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
#endif

	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

#if defined(UVM)
        /*
         * allocate virtual and physical memory for the buffers.
         */
        size = MAXBSIZE * nbuf;         /* # bytes for buffers */

        /* allocate VM for buffers... area is not managed by VM system */
        if (uvm_map(kernel_map, (vm_offset_t *) &buffers, round_page(size),
                    NULL, UVM_UNKNOWN_OFFSET,
                    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
                                UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
        	panic("cpu_startup: cannot allocate VM for buffers");

        minaddr = (vm_offset_t) buffers;
        if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
        	bufpages = btoc(MAXBSIZE) * nbuf; /* do not overallocate RAM */
        }
        base = bufpages / nbuf;
        residual = bufpages % nbuf;

        /* now allocate RAM for buffers */
	for (i = 0 ; i < nbuf ; i++) {
		vm_offset_t curbuf;
		vm_size_t curbufsize;
		struct vm_page *pg;

		/*
		 * each buffer has MAXBSIZE bytes of VM space allocated.  of
		 * that MAXBSIZE space we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vm_offset_t) buffers + (i * MAXBSIZE);
		curbufsize = CLBYTES * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL);
			if (pg == NULL)
				panic("cpu_startup: "
				    "not enough RAM for buffer cache");
			pmap_enter(kernel_map->pmap, curbuf,
			    VM_PAGE_TO_PHYS(pg), VM_PROT_ALL, TRUE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
#else
	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;

	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
	    &maxaddr, size, TRUE);

	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE) {
		/* don't want to alloc more physical mem than needed */
		base = MAXBSIZE;
		residual = 0;
	}

	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
#endif

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
#if defined(UVM)
        exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
                                 16*NCARGS, TRUE, FALSE, NULL);
#else
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);
#endif

	/*
	 * Allocate a map for physio.  Others use a submap of the kernel
	 * map, but we want one completely separate, even though it uses
	 * the same pmap.
	 */
	dvma_base = CPU_ISSUN4M ? DVMA4M_BASE : DVMA_BASE;
	dvma_end = CPU_ISSUN4M ? DVMA4M_END : DVMA_END;
#if defined(UVM)
	phys_map = uvm_map_create(pmap_kernel(), dvma_base, dvma_end, 1);
#else
	phys_map = vm_map_create(pmap_kernel(), dvma_base, dvma_end, 1);
#endif
	if (phys_map == NULL)
		panic("unable to create DVMA map");
	/*
	 * Allocate DVMA space and dump into a privately managed
	 * resource map for double mappings which is usable from
	 * interrupt contexts.
	 */
#if defined(UVM)
	if (uvm_km_valloc_wait(phys_map, (dvma_end-dvma_base)) != dvma_base)
		panic("unable to allocate from DVMA map");
#else
	if (kmem_alloc_wait(phys_map, (dvma_end-dvma_base)) != dvma_base)
		panic("unable to allocate from DVMA map");
#endif
	rminit(dvmamap, btoc((dvma_end-dvma_base)),
		vtorc(dvma_base), "dvmamap", ndvmamap);

	if (CPU_ISSUN4OR4C) {
		/*
		 * Allocate dma map for 24-bit devices (le, ie)
		 * [dvma_base - dvma_end] is for VME devices..
		 */
		dvmamap24 = extent_create("dvmamap24", 0xff000000, dvma_base,
					  M_DEVBUF, 0, 0, EX_NOWAIT);
		if (dvmamap24 == NULL)
			panic("unable to allocate DVMA map");
	}

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
#if defined(UVM)
        mb_map = uvm_km_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
            VM_MBUF_SIZE, FALSE, FALSE, NULL);
#else
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
#endif
	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
#if defined(UVM)
	printf("avail mem = %ld\n", ptoa(uvmexp.free));
#else
	printf("avail mem = %ld\n", ptoa(cnt.v_free_count));
#endif
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.  The cpu code will turn on the cache.
	 */
	configure();

	/*
	 * Re-zero proc0's user area, to nullify the effect of the
	 * stack running into it during auto-configuration.
	 * XXX - should fix stack usage.
	 * XXX - there's a race here, as interrupts are enabled
	 */
	bzero(proc0paddr, sizeof(struct user));

	pmap_redzone();
}

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * You call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
	valloc(callout, struct callout, ncallout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16 and at
	 * most 1/2 of available kernel virtual memory).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0) {
		int bmax = btoc(VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
			   (MAXBSIZE/NBPG) / 2;
		bufpages = (physmem / 20) / CLSIZE;
		if (nbuf == 0 && bufpages > bmax)
			bufpages = bmax;
		/*
		 * XXX stopgap measure to prevent wasting too much KVM on
		 * the sparsely filled buffer cache.
		 */
		if (CPU_ISSUN4C && bufpages > (128 * (65536/MAXBSIZE)))
			bufpages = (128 * (65536/MAXBSIZE));
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
#if !defined(UVM)
	valloc(swbuf, struct buf, nswbuf);
#endif
	valloc(buf, struct buf, nbuf);
	/*
	 * Allocate DVMA slots for 1/4 of the number of i/o buffers
	 * and one for each process too (PHYSIO).
	 */
	valloc(dvmamap, struct map, ndvmamap = maxproc + ((nbuf / 4) &~ 1));
	return (v);
}

/*
 * Set up registers on exec.
 *
 * XXX this entire mess must be fixed
 */
/* ARGSUSED */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	register struct trapframe *tf = p->p_md.md_tf;
	register struct fpstate *fs;
	register int psr;

	/* Don't allow misaligned code by default */
	p->p_md.md_flags &= ~MDP_FIXALIGN;

	/*
	 * Set the registers to 0 except for:
	 *	%o6: stack pointer, built in exec())
	 *	%psr: (retain CWP and PSR_S bits)
	 *	%g1: address of PS_STRINGS (used by crt0)
	 *	%pc,%npc: entry point of program
	 */
	psr = tf->tf_psr & (PSR_S | PSR_CWP);
	if ((fs = p->p_md.md_fpstate) != NULL) {
		/*
		 * We hold an FPU state.  If we own *the* FPU chip state
		 * we must get rid of it, and the only way to do that is
		 * to save it.  In any case, get rid of our FPU state.
		 */
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
		p->p_md.md_fpstate = NULL;
	}
	bzero((caddr_t)tf, sizeof *tf);
	tf->tf_psr = psr;
	tf->tf_global[1] = (int)PS_STRINGS;
	tf->tf_pc = pack->ep_entry & ~3;
	tf->tf_npc = tf->tf_pc + 4;
	stack -= sizeof(struct rwindow);
	tf->tf_out[6] = stack;
}

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

struct sigframe {
	int	sf_signo;		/* signal number */
	int	sf_code;		/* code */
	struct	sigcontext *sf_scp;	/* SunOS user addr of sigcontext */
	int	sf_addr;		/* SunOS compat, always 0 for now */
	struct	sigcontext sf_sc;	/* actual sigcontext */
};

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct sigacts *psp = p->p_sigacts;
	register struct sigframe *fp;
	register struct trapframe *tf;
	register int addr, oonstack, oldsp, newsp;
	struct sigframe sf;
	extern char sigcode[], esigcode[];
#define	szsigcode	(esigcode - sigcode)

	tf = p->p_md.md_tf;
	oldsp = tf->tf_out[6];
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Compute new user stack addresses, subtract off
	 * one signal frame, and align.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)((caddr_t)psp->ps_sigstk.ss_sp +
		                                  psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)oldsp;
	fp = (struct sigframe *)((int)(fp - 1) & ~7);

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: %s[%d] sig %d newusp %p scp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc);
#endif
	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_code = code;
#ifdef COMPAT_SUNOS
	sf.sf_scp = &fp->sf_sc;
#endif
	sf.sf_addr = 0;			/* XXX */

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;
	sf.sf_sc.sc_sp = oldsp;
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_psr = tf->tf_psr;
	sf.sf_sc.sc_g1 = tf->tf_global[1];
	sf.sf_sc.sc_o0 = tf->tf_out[0];

	/*
	 * Put the stack in a consistent state before we whack away
	 * at it.  Note that write_user_windows may just dump the
	 * registers into the pcb; we need them in the process's memory.
	 * We also need to make sure that when we start the signal handler,
	 * its %i6 (%fp), which is loaded from the newly allocated stack area,
	 * joins seamlessly with the frame it was in when the signal occurred,
	 * so that the debugger and _longjmp code can back up through it.
	 */
	newsp = (int)fp - sizeof(struct rwindow);
	write_user_windows();
	if (rwindow_save(p) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) ||
	    suword(&((struct rwindow *)newsp)->rw_in[6], oldsp)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
#endif
	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
#ifdef COMPAT_SUNOS
	if (psp->ps_usertramp & sigmask(sig)) {
		addr = (int)catcher;	/* user does his own trampolining */
	} else
#endif
	{
		addr = (int)PS_STRINGS - szsigcode;
		tf->tf_global[1] = (int)catcher;
	}
	tf->tf_pc = addr;
	tf->tf_npc = addr + 4;
	tf->tf_out[6] = newsp;
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: about to return to catcher\n");
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct trapframe *tf;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
#endif
	scp = SCARG(uap, sigcntxp);
#if defined(UVM)
	if ((int)scp & 3 || uvm_useracc((caddr_t)scp,sizeof *scp, B_WRITE) == 0)
		return (EINVAL);
#else
	if ((int)scp & 3 || useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0)
		return (EINVAL);
#endif
	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((scp->sc_pc | scp->sc_npc) & 3) != 0)
		return (EINVAL);
	/* take only psr ICC field */
	tf->tf_psr = (tf->tf_psr & ~PSR_ICC) | (scp->sc_psr & PSR_ICC);
	tf->tf_pc = scp->sc_pc;
	tf->tf_npc = scp->sc_npc;
	tf->tf_global[1] = scp->sc_g1;
	tf->tf_out[0] = scp->sc_o0;
	tf->tf_out[6] = scp->sc_sp;
	if (scp->sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;
	return (EJUSTRETURN);
}

int	waittime = -1;

void
cpu_reboot(howto, user_boot_string)
	register int howto;
	char *user_boot_string;
{
	int i;
	static char str[128];
	extern int cold;

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

#if NFB > 0
	fb_unblank();
#endif
	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;

		/* XXX protect against curproc->p_stats.foo refs in sync() */
		if (curproc == NULL)
			curproc = &proc0;
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	(void) splhigh();

	/* If rebooting and a dump is requested, do it. */
#if 0
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
#else
	if (howto & RB_DUMP)
#endif
		dumpsys();

 haltsys:

	/* Run any shutdown hooks. */
	doshutdownhooks();

	/* If powerdown was requested, do it. */
	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NPOWER > 0
		powerdown();
		printf("WARNING: powerdown failed!\n");
#endif
		/*
		 * RB_POWERDOWN implies RB_HALT... fall into it...
		 */
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");
		romhalt();
	}

	printf("rebooting\n\n");
	if (user_boot_string && *user_boot_string) {
		i = strlen(user_boot_string);
		if (i > sizeof(str))
			romboot(user_boot_string);	/* XXX */
		bcopy(user_boot_string, str, i);
	} else {
		i = 1;
		str[0] = '\0';
	}

	if (howto & RB_SINGLE)
		str[i++] = 's';
	if (howto & RB_KDB)
		str[i++] = 'd';
	if (i > 1) {
		if (str[0] == '\0')
			str[0] = '-';
		str[i] = 0;
	} else
		str[0] = 0;
	romboot(str);
	/*NOTREACHED*/
}

u_long	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
cpu_dumpconf()
{
	register int nblks, dumpblks;

	if (dumpdev == NODEV || bdevsw[major(dumpdev)].d_psize == 0)
		/* No usable dump device */
		return;

	nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);

	dumpblks = ctod(physmem) + pmap_dumpsize();
	if (dumpblks > (nblks - ctod(1)))
		/*
		 * dump size is too big for the partition.
		 * Note, we safeguard a click at the front for a
		 * possible disk label.
		 */
		return;

	/* Put the dump at the end of the partition */
	dumplo = nblks - dumpblks;

	/*
	 * savecore(8) expects dumpsize to be the number of pages
	 * of actual core dumped (i.e. excluding the MMU stuff).
	 */
	dumpsize = physmem;
}

#define	BYTES_PER_DUMP	(32 * 1024)	/* must be a multiple of pagesize */
static vm_offset_t dumpspace;

caddr_t
reserve_dumppages(p)
	caddr_t p;
{

	dumpspace = (vm_offset_t)p;
	return (p + BYTES_PER_DUMP);
}

/*
 * Write a crash dump.
 */
void
dumpsys()
{
	register int psize;
	daddr_t blkno;
	register int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
	int error = 0;
	register struct memarr *mp;
	register int nmem;
	extern struct memarr pmemarr[];
	extern int npmemarr;

	/* copy registers to memory */
	snapshot(cpcb);
	stackdump();

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;

	error = pmap_dumpmmu(dump, blkno);
	blkno += pmap_dumpsize();

	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0 && error == 0; mp++) {
		unsigned i = 0, n;
		int maddr = mp->addr;

		if (maddr == 0) {
			/* Skip first page at physical address 0 */
			maddr += NBPG;
			i += NBPG;
			blkno += btodb(NBPG);
		}

		for (; i < mp->len; i += n) {
			n = mp->len - i;
			if (n > BYTES_PER_DUMP)
				 n = BYTES_PER_DUMP;

			/* print out how many MBs we have dumped */
			if (i && (i % (1024*1024)) == 0)
				printf("%d ", i / (1024*1024));

			(void) pmap_map(dumpspace, maddr, maddr + n,
					VM_PROT_READ);
			error = (*dump)(dumpdev, blkno,
					(caddr_t)dumpspace, (int)n);
			pmap_remove(pmap_kernel(), dumpspace, dumpspace + n);
			if (error)
				break;
			maddr += n;
			blkno += btodb(n);
		}
	}

	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
}

/*
 * get the fp and dump the stack as best we can.  don't leave the
 * current stack page
 */
void
stackdump()
{
	struct frame *fp = getfp(), *sfp;

	sfp = fp;
	printf("Frame pointer is at %p\n", fp);
	printf("Call traceback:\n");
	while (fp && ((u_long)fp >> PGSHIFT) == ((u_long)sfp >> PGSHIFT)) {
		printf("  pc = 0x%x  args = (0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) fp = %p\n",
		    fp->fr_pc, fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6],
		    fp->fr_fp);
		fp = fp->fr_fp;
	}
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return (ENOEXEC);
}

#ifdef SUN4
void
oldmon_w_trace(va)
	u_long va;
{
	u_long stop;
	struct frame *fp;

	if (curproc)
		printf("curproc = %p, pid %d\n", curproc, curproc->p_pid);
	else
		printf("no curproc\n");

#if defined(UVM)
	printf("uvm: swtch %d, trap %d, sys %d, intr %d, soft %d, faults %d\n",
	    uvmexp.swtch, uvmexp.traps, uvmexp.syscalls, uvmexp.intrs,
		uvmexp.softs, uvmexp.faults);
#else
	printf("cnt: swtch %d, trap %d, sys %d, intr %d, soft %d, faults %d\n",
	    cnt.v_swtch, cnt.v_trap, cnt.v_syscall, cnt.v_intr, cnt.v_soft,
	    cnt.v_faults);
#endif
	write_user_windows();

#define round_up(x) (( (x) + (NBPG-1) ) & (~(NBPG-1)) )

	printf("\nstack trace with sp = 0x%lx\n", va);
	stop = round_up(va);
	printf("stop at 0x%lx\n", stop);
	fp = (struct frame *) va;
	while (round_up((u_long) fp) == stop) {
		printf("  0x%x(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) fp %p\n", fp->fr_pc,
		    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2], fp->fr_arg[3],
		    fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6], fp->fr_fp);
		fp = fp->fr_fp;
		if (fp == NULL)
			break;
	}
	printf("end of stack trace\n");
}

void
oldmon_w_cmd(va, ar)
	u_long va;
	char *ar;
{
	switch (*ar) {
	case '\0':
		switch (va) {
		case 0:
			panic("g0 panic");
		case 4:
			printf("w: case 4\n");
			break;
		default:
			printf("w: unknown case %ld\n", va);
			break;
		}
		break;
	case 't':
		oldmon_w_trace(va);
		break;
	default:
		printf("w: arg not allowed\n");
	}
}
#endif /* SUN4 */

int
ldcontrolb(addr)
caddr_t addr;
{
	struct pcb *xpcb;
	extern struct user *proc0paddr;
	u_long saveonfault;
	int res;
	int s;

	if (CPU_ISSUN4M) {
		printf("warning: ldcontrolb called in sun4m\n");
		return 0;
	}

	s = splhigh();
	if (curproc == NULL)
		xpcb = (struct pcb *)proc0paddr;
	else
		xpcb = &curproc->p_addr->u_pcb;

	saveonfault = (u_long)xpcb->pcb_onfault;
        res = xldcontrolb(addr, xpcb);
	xpcb->pcb_onfault = (caddr_t)saveonfault;

	splx(s);
	return (res);
}

void
wzero(vb, l)
	void *vb;
	u_int l;
{
	u_char *b = vb;
	u_char *be = b + l;
	u_short *sp;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b & 1)
		*b++ = 0;

	/* back, */
	if (b != be && ((u_long)be & 1) != 0) {
		be--;
		*be = 0;
	}

	/* and middle. */
	sp = (u_short *)b;
	while (sp != (u_short *)be)
		*sp++ = 0;
}

void
wcopy(vb1, vb2, l)
	const void *vb1;
	void *vb2;
	u_int l;
{
	const u_char *b1e, *b1 = vb1;
	u_char *b2 = vb2;
	u_short *sp;
	int bstore = 0;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b1 & 1) {
		*b2++ = *b1++;
		l--;
	}

	/* middle, */
	sp = (u_short *)b1;
	b1e = b1 + l;
	if (l & 1)
		b1e--;
	bstore = (u_long)b2 & 1;

	while (sp < (u_short *)b1e) {
		if (bstore) {
			b2[1] = *sp & 0xff;
			b2[0] = *sp >> 8;
		} else
			*((short *)b2) = *sp;
		sp++;
		b2 += 2;
	}

	/* and back. */
	if (l & 1)
		*b2 = *b1e;
}

static bus_addr_t dvmamap_alloc __P((int, int));

static __inline__ bus_addr_t
dvmamap_alloc(size, flags)
	int size;
	int flags;
{
	int s, pn, npf;

	npf = btoc(size);
	s = splimp();
	for (;;) {
		pn = rmalloc(dvmamap, npf);
		if (pn != 0)
			break;

		if (flags & BUS_DMA_WAITOK) {
			(void)tsleep(dvmamap, PRIBIO+1, "dvma", 0);
			continue;
		}
		splx(s);
		return ((bus_addr_t)-1);
	}
	splx(s);

	return ((bus_addr_t)rctov(pn));
}

static void dvmamap_free __P((bus_addr_t, bus_size_t));

static __inline__ void
dvmamap_free (addr, size)
	bus_addr_t addr;
	bus_size_t size;
{
	int s, pn, npf;

	npf = btoc(size);
	pn = vtorc(addr);
	s = splimp();
	rmfree(dvmamap, npf, pn);
	wakeup(dvmamap);
	splx(s);
}



/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_bus_dmamap_create(t, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct sparc_bus_dmamap *map;
	void *mapstore;
	size_t mapsize;

	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(struct sparc_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	if ((mapstore = malloc(mapsize, M_DMAMAP,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	bzero(mapstore, mapsize);
	map = (struct sparc_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->dm_mapsize = 0;		/* no valid mappings */
	map->dm_nsegs = 0;

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
_bus_dmamap_destroy(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	free(map, M_DMAMAP);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_bus_dmamap_load_mbuf(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{

	panic("_bus_dmamap_load: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
_bus_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{

	panic("_bus_dmamap_load_uio: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_bus_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{

	panic("_bus_dmamap_load_raw: not implemented");
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_bus_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
}

int
_bus_dmamem_alloc_common(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	vm_offset_t low, high;
	struct pglist *mlist;
	int error;

	/* Always round the size. */
	size = round_page(size);
	low = vm_first_phys;
	high = vm_first_phys + vm_num_phys - PAGE_SIZE;

	if ((mlist = malloc(sizeof(*mlist), M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	/*
	 * Allocate pages from the VM system.
	 */
	TAILQ_INIT(mlist);
#if defined(UVM)
	error = uvm_pglistalloc(size, low, high,
	    alignment, boundary, mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
#else
	error = vm_page_alloc_memory(size, low, high,
	    alignment, boundary, mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
#endif
	if (error)
		return (error);

	/*
	 * Simply keep a pointer around to the linked list, so
	 * bus_dmamap_free() can return it.
	 *
	 * NOBODY SHOULD TOUCH THE pageq FIELDS WHILE THESE PAGES
	 * ARE IN OUR CUSTODY.
	 */
	segs[0]._ds_mlist = mlist;
	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_bus_dmamem_free_common(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{

	if (nsegs != 1)
		panic("bus_dmamem_free: nsegs = %d", nsegs);

	/*
	 * Return the list of pages back to the VM system.
	 */
#if defined(UVM)
	uvm_pglistfree(segs[0]._ds_mlist);
#else
	vm_page_free_memory(segs[0]._ds_mlist);
#endif
	free(segs[0]._ds_mlist, M_DEVBUF);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_bus_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("_bus_dmamem_unmap");
#endif

	size = round_page(size);
#if defined(UVM)
	uvm_unmap(kernel_map, (vm_offset_t)kva, (vm_offset_t)kva + size, 0);
#else
	vm_map_remove(kernel_map, (vm_offset_t)kva, (vm_offset_t)kva + size);
#endif
}

/*
 * Common functin for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
int
_bus_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs, off, prot, flags;
{

	panic("_bus_dmamem_mmap: not implemented");
}

/* sun4/sun4c dma map functions */
int	sun4_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t, void *,
				bus_size_t, struct proc *, int));
void	sun4_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
int	sun4_dmamem_alloc __P((bus_dma_tag_t tag, bus_size_t size, 
				bus_size_t alignment, bus_size_t boundary,
				bus_dma_segment_t *segs, int nsegs, int *rsegs,
				int flags));
void	sun4_dmamem_free __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
				int nsegs));
int	sun4_dmamem_map __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,  
				int nsegs, size_t size, caddr_t *kvap,
				int flags));

/*
 * sun4/sun4c: load DMA map with a linear buffer.
 */
int
sun4_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
#if notyet
	bus_size_t sgsize;
	caddr_t vaddr = buf;
	bus_addr_t dvmaddr, curaddr;
	pmap_t pmap;
#endif

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size)
		return (EINVAL);

	/*
	 * XXX Need to implement "don't dma across this boundry".
	 */
	if (map->_dm_boundary != 0)
		panic("bus_dmamap_load: boundaries not implemented");

	cpuinfo.cache_flush(buf, buflen);

	if (p == NULL) {
		map->dm_mapsize = buflen;
		map->dm_nsegs = 1;
		map->dm_segs[0].ds_addr = (bus_addr_t)buf;
		map->dm_segs[0].ds_len = buflen;
		return (0);
	}

#if notyet
	sgsize = round_page(buflen + ((int)vaddr & PGOFSET));

	dvmaddr = dvmamap_alloc(sgsize, flags);
	if (dvmaddr != 0)
		return (ENOMEM);

	/*
	 * We always use just one segment.
	 */
	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	map->dm_segs[0].ds_addr = dvmaddr + (vaddr & PGOFSET);
	map->dm_segs[0].ds_len = buflen;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	for (; buflen > 0; ) {
		/*
		 * Get the physical address for this page.
		 */
		curaddr = (bus_addr_t)pmap_extract(pmap, (vm_offset_t)vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = NBPG - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

#ifdef notyet
#if defined(SUN4)
		if (have_iocache)
			curaddr |= PG_IOC;
#endif
#endif
		pmap_enter(pmap_kernel(), dvmaddr,
			   (curaddr & ~(NBPG-1))| PMAP_NC,
			   VM_PROT_READ|VM_PROT_WRITE, 1);

		dvmaddr += PAGE_SIZE;
		vaddr += sgsize;
		buflen -= sgsize;
	}
#else
	panic("sun4_dmamap_load: not implemented");
#endif
	return (0);
}

/*
 * sun4/sun4c function for unloading a DMA map.
 */
void
sun4_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	bus_addr_t addr;
	bus_size_t len;

	if (map->dm_nsegs != 1)
		panic("_bus_dmamap_unload: nsegs = %d", map->dm_nsegs);

	if (1) {
		map->dm_mapsize = 0;
		map->dm_nsegs = 0;
		return;
	}
	addr = map->dm_segs[0].ds_addr & ~PGOFSET;
	len = map->dm_segs[0].ds_len;

	pmap_remove(pmap_kernel(), addr, addr + len);

	dvmamap_free(addr, len);

	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
sun4_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	vm_offset_t va;
	bus_addr_t dvmaddr;
	vm_page_t m;
	struct pglist *mlist;
	int error;

	if ((flags & BUS_DMA_24BIT) == 0) {
		/* Any memory will do */
		va = uvm_km_kmemalloc(kernel_map, uvm.kernel_object, size,
				      (flags & BUS_DMA_NOWAIT) != 0
						? UVM_KMF_NOWAIT
						: 0);
		if (va == NULL)
			return (ENOMEM);

		kvm_uncache((caddr_t)va, btoc(size));
		segs[0].ds_addr = (bus_addr_t)va;
		segs[0].ds_len = size;
		segs[0]._ds_mlist = NULL;
		*rsegs = 1;
		return (0);
	}

	error = _bus_dmamem_alloc_common(t, size, alignment, boundary,
					 segs, nsegs, rsegs, flags);
	if (error != 0)
		return (error);

	if (extent_alloc(dvmamap24, round_page(size), NBPG, EX_NOBOUNDARY,
            EX_NOWAIT, (u_long *)&dvmaddr) != 0)
		return (ENOMEM);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	segs[0].ds_addr = dvmaddr;
	segs[0].ds_len = size;
	*rsegs = 1;

	mlist = segs[0]._ds_mlist;

	/* Map memory into DVMA space */
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {
		va = VM_PAGE_TO_PHYS(m);

#ifdef notyet
#if defined(SUN4)
		if (have_iocache)
			va |= PG_IOC;
#endif
#endif
		pmap_enter(pmap_kernel(), dvmaddr,
			   va | PMAP_NC,
			   VM_PROT_READ|VM_PROT_WRITE, 1);
		dvmaddr += PAGE_SIZE;
	}

	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
sun4_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{
	bus_addr_t addr;
	bus_size_t len;

	if (segs[0]._ds_mlist == NULL) {
		vm_offset_t kva = (vm_offset_t)segs[0].ds_addr;
		vm_offset_t size = round_page(segs[0].ds_len);
		uvm_unmap(kernel_map, kva, kva + size, 0);
		return;
	}

	addr = segs[0].ds_addr;
	len = segs[0].ds_len;

	if (extent_free(dvmamap24, addr, round_page(len), EX_NOWAIT) != 0)
		printf("warning: %ld of DVMA space lost\n", len);

	pmap_remove(pmap_kernel(), addr, addr + len);

	/*
	 * Return the list of pages back to the VM system.
	 */
	_bus_dmamem_free_common(t, segs, nsegs);
}

/*
 * Common function for mapping DMA-safe memory.  May be called by
 * bus-specific DMA memory map functions.
 */
int
sun4_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vm_page_t m;
	vm_offset_t va;
	bus_addr_t addr;
	struct pglist *mlist;

	if (nsegs != 1)
		panic("sun4_dmamem_map: nsegs = %d", nsegs);

	if (segs[0]._ds_mlist == NULL) {
		*kvap = (caddr_t)segs[0].ds_addr;
		return (0);
	}

	size = round_page(size);

	va = uvm_km_valloc(kernel_map, size);
	if (va == 0)
		return (ENOMEM);

	*kvap = (caddr_t)va;
	mlist = segs[0]._ds_mlist;

	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {

		if (size == 0)
			panic("sun4_dmamem_map: size botch");

		addr = VM_PAGE_TO_PHYS(m);
		pmap_enter(pmap_kernel(), va, addr | PMAP_NC,
			   VM_PROT_READ | VM_PROT_WRITE, TRUE);

		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return (0);
}


struct sparc_bus_dma_tag mainbus_dma_tag = {
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	sun4_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	sun4_dmamap_unload,
	_bus_dmamap_sync,

	sun4_dmamem_alloc,
	sun4_dmamem_free,
	sun4_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap
};


/*
 * Base bus space handlers.
 */
static int	sparc_bus_map __P(( bus_space_tag_t, bus_type_t, bus_addr_t,
				    bus_size_t, int, vm_offset_t,
				    bus_space_handle_t *));
static int	sparc_bus_unmap __P((bus_space_tag_t, bus_space_handle_t,
				     bus_size_t));
static int	sparc_bus_mmap __P((bus_space_tag_t, bus_type_t,
				    bus_addr_t, int, bus_space_handle_t *));
static void	*sparc_mainbus_intr_establish __P((bus_space_tag_t, int, int,
						   int (*) __P((void *)),
						   void *));
static void     sparc_bus_barrier __P(( bus_space_tag_t, bus_space_handle_t,
					bus_size_t, bus_size_t, int));


int
sparc_bus_map(t, iospace, addr, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t	iospace;
	bus_addr_t	addr;
	bus_size_t	size;
	vm_offset_t	vaddr;
	bus_space_handle_t *hp;
{
	vm_offset_t v;
	vm_offset_t pa;
	unsigned int pmtype;
static	vm_offset_t iobase;


	if (iobase == NULL)
		iobase = IODEV_BASE;

	size = round_page(size);
	if (size == 0) {
		printf("sparc_bus_map: zero size\n");
		return (EINVAL);
	}

	if (vaddr)
		v = trunc_page(vaddr);
	else {
		v = iobase;
		iobase += size;
		if (iobase > IODEV_END)	/* unlikely */
			panic("sparc_bus_map: iobase=0x%lx", iobase);
	}

	/* note: preserve page offset */
	*hp = (bus_space_handle_t)(v | ((u_long)addr & PGOFSET));

	pa = trunc_page(addr);
	pmtype = PMAP_IOENC(iospace);

	do {
		pmap_enter(pmap_kernel(), v, pa | pmtype | PMAP_NC,
			   VM_PROT_READ | VM_PROT_WRITE, 1);
		v += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	return (0);
}

int
sparc_bus_unmap(t, bh, size)
	bus_space_tag_t t;
	bus_size_t	size;
	bus_space_handle_t bh;
{
	vm_offset_t va = trunc_page((vm_offset_t)bh);
	vm_offset_t endva = va + round_page(size);

	pmap_remove(pmap_kernel(), va, endva);
	return (0);
}

int
sparc_bus_mmap(t, iospace, paddr, flags, hp)
	bus_space_tag_t t;
	bus_type_t	iospace;
	bus_addr_t	paddr;
	int		flags;
	bus_space_handle_t *hp;
{
	*hp = (bus_space_handle_t)(paddr | PMAP_IOENC(iospace) | PMAP_NC);
	return (0);
}

/*
 * Establish a temporary bus mapping for device probing.
 */
int
bus_space_probe(tag, btype, paddr, size, offset, flags, callback, arg)
	bus_space_tag_t tag;
	bus_type_t	btype;
	bus_addr_t	paddr;
	bus_size_t	size;
	size_t		offset;
	int		flags;
	int		(*callback) __P((void *, void *));
	void		*arg;
{
	bus_space_handle_t bh;
	caddr_t tmp;
	int result;

	if (bus_space_map2(tag, btype, paddr, size, flags, TMPMAP_VA, &bh) != 0)
		return (0);

	tmp = (caddr_t)bh;
	result = (probeget(tmp + offset, size) != -1);
	if (result && callback != NULL)
		result = (*callback)(tmp, arg);
	bus_space_unmap(tag, bh, size);
	return (result);
}


void *
sparc_mainbus_intr_establish(t, level, flags, handler, arg)
	bus_space_tag_t t;
	int	level;
	int	flags;
	int	(*handler)__P((void *));
	void	*arg;
{
	struct intrhand *ih;

	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	if ((flags & BUS_INTR_ESTABLISH_FASTTRAP) != 0)
		intr_fasttrap(level, (void (*)__P((void)))handler);
	else
		intr_establish(level, ih);
	return (ih);
}

void sparc_bus_barrier (t, h, offset, size, flags)
	bus_space_tag_t	t;
	bus_space_handle_t h;
	bus_size_t	offset;
	bus_size_t	size;
	int		flags;
{
	/* No default barrier action defined */
	return;
}

struct sparc_bus_space_tag mainbus_space_tag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	sparc_bus_map,			/* bus_space_map */
	sparc_bus_unmap,		/* bus_space_unmap */
	NULL,				/* bus_space_subregion */
	sparc_bus_barrier,		/* bus_space_barrier */
	sparc_bus_mmap,			/* bus_space_mmap */
	sparc_mainbus_intr_establish	/* bus_intr_establish */
};
