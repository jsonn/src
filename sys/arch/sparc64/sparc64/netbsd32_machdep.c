
/*
 * Copyright (c) 1998, 2001 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/map.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include <dev/sun/event_var.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>

#include <compat/netbsd32/netbsd32.h>
#include <compat/netbsd32/netbsd32_ioctl.h>
#include <compat/netbsd32/netbsd32_syscallargs.h>

#include <machine/frame.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/vuid_event.h>
#include <machine/netbsd32_machdep.h>

/* Provide a the name of the architecture we're emulating */
char	machine_arch32[] = "sparc";	

static int ev_out32 __P((struct firm_event *, int, struct uio *));

/*
 * Set up registers on exec.
 *
 * XXX this entire mess must be fixed
 */
/* ARGSUSED */
void
netbsd32_setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack; /* XXX */
{
	register struct trapframe64 *tf = p->p_md.md_tf;
	register struct fpstate64 *fs;
	register int64_t tstate;

	/* Don't allow misaligned code by default */
	p->p_md.md_flags &= ~MDP_FIXALIGN;

	/* Mark this as a 32-bit emulation */
	p->p_flag |= P_32;

	/* Setup the ev_out32 hook */
	if (ev_out32_hook == NULL)
		ev_out32_hook = ev_out32;

	/*
	 * Set the registers to 0 except for:
	 *	%o6: stack pointer, built in exec())
	 *	%tstate: (retain icc and xcc and cwp bits)
	 *	%g1: address of p->p_psstr (used by crt0)
	 *	%tpc,%tnpc: entry point of program
	 */
	tstate = ((PSTATE_USER32)<<TSTATE_PSTATE_SHIFT) 
		| (tf->tf_tstate & TSTATE_CWP);
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
	tf->tf_tstate = tstate;
	tf->tf_global[1] = (u_int)(u_long)p->p_psstr;
	tf->tf_pc = pack->ep_entry & ~3;
	tf->tf_npc = tf->tf_pc + 4;

	stack -= sizeof(struct rwindow32);
	tf->tf_out[6] = stack;
	tf->tf_out[7] = NULL;
}

/*
 * NB: since this is a 32-bit address world, sf_scp and sf_sc
 *	can't be a pointer since those are 64-bits wide.
 */
struct sparc32_sigframe {
	int	sf_signo;		/* signal number */
	int	sf_code;		/* code */
	u_int	sf_scp;			/* SunOS user addr of sigcontext */
	int	sf_addr;		/* SunOS compat, always 0 for now */
	struct	netbsd32_sigcontext sf_sc;	/* actual sigcontext */
};

#undef DEBUG
#ifdef DEBUG
extern int sigdebug;
#endif

void
netbsd32_sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct sparc32_sigframe *fp;
	register struct trapframe64 *tf;
	register int addr, onstack; 
	struct rwindow32 *kwin, *oldsp, *newsp;
	struct sparc32_sigframe sf;
	extern char netbsd32_sigcode[], netbsd32_esigcode[];
#define	szsigcode	(netbsd32_esigcode - netbsd32_sigcode)

	tf = p->p_md.md_tf;
	/* Need to attempt to zero extend this 32-bit pointer */
	oldsp = (struct rwindow32 *)(u_long)(u_int)tf->tf_out[6];
	/* Do we need to jump onto the signal stack? */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;
	if (onstack) {
		fp = (struct sparc32_sigframe *)((char *)p->p_sigctx.ps_sigstk.ss_sp +
					p->p_sigctx.ps_sigstk.ss_size);
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sparc32_sigframe *)oldsp;
	fp = (struct sparc32_sigframe *)((u_long)(fp - 1) & ~7);

#ifdef DEBUG
	sigpid = p->p_pid;
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid) {
		printf("sendsig: %s[%d] sig %d newusp %p scp %p oldsp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc, oldsp);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_code = (u_int)code;
#if defined(COMPAT_SUNOS) || defined(LKM)
	sf.sf_scp = (u_long)&fp->sf_sc;
#endif
	sf.sf_addr = 0;			/* XXX */

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = onstack;
	sf.sf_sc.sc_mask = *mask;
	sf.sf_sc.sc_sp = (u_long)oldsp;
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_psr = TSTATECCR_TO_PSR(tf->tf_tstate); /* XXX */
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
	newsp = (struct rwindow32 *)((long)fp - sizeof(struct rwindow32));
	write_user_windows();
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK))
	    printf("sendsig: saving sf to %p, setting stack pointer %p to %p\n",
		   fp, &(((struct rwindow32 *)newsp)->rw_in[6]), oldsp);
#endif
	kwin = (struct rwindow32 *)(((caddr_t)tf)-CCFSZ);
	if (rwindow_save(p) || 
	    copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) || 
	    suword(&(((struct rwindow32 *)newsp)->rw_in[6]), (u_long)oldsp)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
		printf("sendsig: stack was trashed trying to send sig %d, sending SIGILL\n", sig);
		if (sigdebug & SDB_DDB) Debugger();
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
	}
#endif
	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
	addr = (long)p->p_psstr - szsigcode;
	tf->tf_global[1] = (long)catcher;
	tf->tf_pc = addr;
	tf->tf_npc = addr + 4;
	tf->tf_out[6] = (u_int64_t)(u_int)(u_long)newsp;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid) {
		printf("sendsig: about to return to catcher %p thru %p\n", 
		       catcher, addr);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
}

#undef DEBUG

#ifdef COMPAT_13
int
compat_13_netbsd32_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_13_netbsd32_sigreturn_args /* {
		syscallarg(struct netbsd32_sigcontext13 *) sigcntxp;
	} */ *uap = v;
	struct netbsd32_sigcontext13 *scp;
	struct netbsd32_sigcontext13 sc;
	register struct trapframe64 *tf;
	struct rwindow32 *rwstack, *kstack;
	sigset_t mask;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(p)) {
#ifdef DEBUG
		printf("compat_13_netbsd32_sigreturn: rwindow_save(%p) failed, sending SIGILL\n", p);
		Debugger();
#endif
		sigexit(p, SIGILL);
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("compat_13_netbsd32_sigreturn: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
	scp = (struct netbsd32_sigcontext13 *)(u_long)SCARG(uap, sigcntxp);
 	if ((vaddr_t)scp & 3 || (copyin((caddr_t)scp, &sc, sizeof sc) != 0))
#ifdef DEBUG
	{
		printf("compat_13_netbsd32_sigreturn: copyin failed\n");
		Debugger();
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((sc.sc_pc | sc.sc_npc) & 3) != 0)
#ifdef DEBUG
	{
		printf("compat_13_netbsd32_sigreturn: pc %p or npc %p invalid\n", sc.sc_pc, sc.sc_npc);
		Debugger();
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	/* take only psr ICC field */
	tf->tf_tstate = (int64_t)(tf->tf_tstate & ~TSTATE_CCR) | PSRCC_TO_TSTATE(sc.sc_psr);
	tf->tf_pc = (int64_t)sc.sc_pc;
	tf->tf_npc = (int64_t)sc.sc_npc;
	tf->tf_global[1] = (int64_t)sc.sc_g1;
	tf->tf_out[0] = (int64_t)sc.sc_o0;
	tf->tf_out[6] = (int64_t)sc.sc_sp;
	rwstack = (struct rwindow32 *)(u_long)tf->tf_out[6];
	kstack = (struct rwindow32 *)(((caddr_t)tf)-CCFSZ);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("compat_13_netbsd32_sys_sigreturn: return trapframe pc=%p sp=%p tstate=%x\n",
		       (int)tf->tf_pc, (int)tf->tf_out[6], (int)tf->tf_tstate);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
	if (scp->sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask */
	native_sigset13_to_sigset(&scp->sc_mask, &mask);
	(void) sigprocmask1(p, SIG_SETMASK, &mask, 0);
	return (EJUSTRETURN);
}
#endif

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
netbsd32___sigreturn14(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct netbsd32___sigreturn14_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct netbsd32_sigcontext sc, *scp;
	register struct trapframe64 *tf;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(p)) {
#ifdef DEBUG
		printf("netbsd32_sigreturn14: rwindow_save(%p) failed, sending SIGILL\n", p);
		Debugger();
#endif
		sigexit(p, SIGILL);
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("netbsd32_sigreturn14: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
	scp = (struct netbsd32_sigcontext *)(u_long)SCARG(uap, sigcntxp);
 	if ((vaddr_t)scp & 3 || (copyin((caddr_t)scp, &sc, sizeof sc) != 0))
#ifdef DEBUG
	{
		printf("netbsd32_sigreturn14: copyin failed: scp=%p\n", scp);
		Debugger();
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	scp = &sc;

	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((sc.sc_pc | sc.sc_npc) & 3) != 0 || (sc.sc_pc == 0) || (sc.sc_npc == 0))
#ifdef DEBUG
	{
		printf("netbsd32_sigreturn14: pc %p or npc %p invalid\n", sc.sc_pc, sc.sc_npc);
		Debugger();
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	/* take only psr ICC field */
	tf->tf_tstate = (int64_t)(tf->tf_tstate & ~TSTATE_CCR) | PSRCC_TO_TSTATE(sc.sc_psr);
	tf->tf_pc = (int64_t)scp->sc_pc;
	tf->tf_npc = (int64_t)scp->sc_npc;
	tf->tf_global[1] = (int64_t)scp->sc_g1;
	tf->tf_out[0] = (int64_t)scp->sc_o0;
	tf->tf_out[6] = (int64_t)scp->sc_sp;
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("netbsd32_sigreturn14: return trapframe pc=%p sp=%p tstate=%llx\n",
		       (vaddr_t)tf->tf_pc, (vaddr_t)tf->tf_out[6], tf->tf_tstate);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif

	/* Restore signal stack. */
	if (sc.sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &sc.sc_mask, 0);

	return (EJUSTRETURN);
}

#if 0
/* Unfortunately we need to convert v9 trapframe to v8 regs */
int
netbsd32_process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct reg32* regp = (struct reg32*)regs;
	struct trapframe64* tf = p->p_md.md_tf;
	int i;

	/* 
	 * Um, we should only do this conversion for 32-bit emulation
	 * or when running 32-bit mode.  We really need to pass in a
	 * 32-bit emulation flag!
	 */

	regp->r_psr = TSTATECCR_TO_PSR(tf->tf_tstate);
	regp->r_pc = tf->tf_pc;
	regp->r_npc = tf->tf_npc;
	regp->r_y = tf->tf_y;
	for (i = 0; i < 8; i++) {
		regp->r_global[i] = tf->tf_global[i];
		regp->r_out[i] = tf->tf_out[i];
	}
	/* We should also write out the ins and locals.  See signal stuff */
	return (0);
}

int
netbsd32_process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct reg32* regp = (struct reg32*)regs;
	struct trapframe64* tf = p->p_md.md_tf;
	int i;

	tf->tf_pc = regp->r_pc;
	tf->tf_npc = regp->r_npc;
	tf->tf_y = regp->r_pc;
	for (i = 0; i < 8; i++) {
		tf->tf_global[i] = regp->r_global[i];
		tf->tf_out[i] = regp->r_out[i];
	}
	/* We should also read in the ins and locals.  See signal stuff */
	tf->tf_tstate = (int64_t)(tf->tf_tstate & ~TSTATE_CCR) | PSRCC_TO_TSTATE(regp->r_psr);
	return (0);
}

int
netbsd32_process_read_fpregs(p, regs)
struct proc	*p;
struct fpreg	*regs;
{
	extern struct fpstate64	initfpstate;
	struct fpstate64	*statep = &initfpstate;
	struct fpreg32		*regp = (struct fpreg32 *)regs;
	int i;

	/* NOTE: struct fpreg == struct fpstate */
	if (p->p_md.md_fpstate)
		statep = p->p_md.md_fpstate;
	for (i=0; i<32; i++)
		regp->fr_regs[i] = statep->fs_regs[i];
	regp->fr_fsr = statep->fs_fsr;
	regp->fr_qsize = statep->fs_qsize;
	for (i=0; i<statep->fs_qsize; i++)
		regp->fr_queue[i] = statep->fs_queue[i];

	return 0;
}

int
netbsd32_process_write_fpregs(p, regs)
struct proc	*p;
struct fpreg	*regs;
{
	extern struct fpstate	initfpstate;
	struct fpstate64	*statep = &initfpstate;
	struct fpreg32		*regp = (struct fpreg32 *)regs;
	int i;

	/* NOTE: struct fpreg == struct fpstate */
	if (p->p_md.md_fpstate)
		statep = p->p_md.md_fpstate;
	for (i=0; i<32; i++)
		statep->fs_regs[i] = regp->fr_regs[i];
	statep->fs_fsr = regp->fr_fsr;
	statep->fs_qsize = regp->fr_qsize;
	for (i=0; i<regp->fr_qsize; i++)
		statep->fs_queue[i] = regp->fr_queue[i];

	return 0;
}
#endif

/*
 * 32-bit version of cpu_coredump.
 */
int
cpu_coredump32(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core32 *chdr;
{
	int i, error;
	struct md_coredump32 md_core;
	struct coreseg32 cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Fake a v8 trapframe */
	md_core.md_tf.tf_psr = TSTATECCR_TO_PSR(p->p_md.md_tf->tf_tstate);
	md_core.md_tf.tf_pc = p->p_md.md_tf->tf_pc;
	md_core.md_tf.tf_npc = p->p_md.md_tf->tf_npc;
	md_core.md_tf.tf_y = p->p_md.md_tf->tf_y;
	for (i=0; i<8; i++) {
		md_core.md_tf.tf_global[i] = p->p_md.md_tf->tf_global[i];
		md_core.md_tf.tf_out[i] = p->p_md.md_tf->tf_out[i];
	}

	if (p->p_md.md_fpstate) {
		if (p == fpproc) {
			savefpstate(p->p_md.md_fpstate);
			fpproc = NULL;
		}
		/* Copy individual fields */
		for (i=0; i<32; i++)
			md_core.md_fpstate.fs_regs[i] = 
				p->p_md.md_fpstate->fs_regs[i];
		md_core.md_fpstate.fs_fsr = p->p_md.md_fpstate->fs_fsr;
		i = md_core.md_fpstate.fs_qsize = p->p_md.md_fpstate->fs_qsize;
		/* Should always be zero */
		while (i--)
			md_core.md_fpstate.fs_queue[i] = 
				p->p_md.md_fpstate->fs_queue[i];
	} else
		bzero((caddr_t)&md_core.md_fpstate, 
		      sizeof(md_core.md_fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * Write out a series of 32-bit firm_events.
 */
int
ev_out32(e, n, uio)
	struct firm_event *e;
	int n;
	struct uio *uio;
{
	struct firm_event32 e32;
	int error = 0;

	while (n-- && error == 0) {
		e32.id = e->id;
		e32.value = e->value;
		e32.time.tv_sec = e->time.tv_sec;
		e32.time.tv_usec = e->time.tv_usec;
		error = uiomove((caddr_t)&e32, sizeof(e32), uio);
		e++;
	}
	return (error);
}

/*
 * ioctl code
 */

#include <dev/sun/fbio.h>
#include <machine/openpromio.h>

/* from arch/sparc/include/fbio.h */
#if 0
/* unused */
#define	FBIOGINFO	_IOR('F', 2, struct fbinfo)
#endif

struct netbsd32_fbcmap {
	int	index;		/* first element (0 origin) */
	int	count;		/* number of elements */
	netbsd32_u_charp	red;		/* red color map elements */
	netbsd32_u_charp	green;		/* green color map elements */
	netbsd32_u_charp	blue;		/* blue color map elements */
};
#if 1
#define	FBIOPUTCMAP32	_IOW('F', 3, struct netbsd32_fbcmap)
#define	FBIOGETCMAP32	_IOW('F', 4, struct netbsd32_fbcmap)
#endif

struct netbsd32_fbcursor {
	short set;		/* what to set */
	short enable;		/* enable/disable cursor */
	struct fbcurpos pos;	/* cursor's position */
	struct fbcurpos hot;	/* cursor's hot spot */
	struct netbsd32_fbcmap cmap;	/* color map info */
	struct fbcurpos size;	/* cursor's bit map size */
	netbsd32_charp image;	/* cursor's image bits */
	netbsd32_charp mask;	/* cursor's mask bits */
};
#if 1
#define FBIOSCURSOR32	_IOW('F', 24, struct netbsd32_fbcursor)
#define FBIOGCURSOR32	_IOWR('F', 25, struct netbsd32_fbcursor)
#endif

/* from arch/sparc/include/openpromio.h */
struct netbsd32_opiocdesc {
	int	op_nodeid;		/* passed or returned node id */
	int	op_namelen;		/* length of op_name */
	netbsd32_charp op_name;		/* pointer to field name */
	int	op_buflen;		/* length of op_buf (value-result) */
	netbsd32_charp op_buf;		/* pointer to field value */
};
#if 1
#define	OPIOCGET32	_IOWR('O', 1, struct netbsd32_opiocdesc) /* get openprom field */
#define	OPIOCSET32	_IOW('O', 2, struct netbsd32_opiocdesc) /* set openprom field */
#define	OPIOCNEXTPROP32	_IOWR('O', 3, struct netbsd32_opiocdesc) /* get next property */
#endif

/* prototypes for the converters */
static __inline void
netbsd32_to_fbcmap(struct netbsd32_fbcmap *, struct fbcmap *, u_long);
static __inline void
netbsd32_to_fbcursor(struct netbsd32_fbcursor *, struct fbcursor *, u_long);
static __inline void
netbsd32_to_opiocdesc(struct netbsd32_opiocdesc *, struct opiocdesc *, u_long);

static __inline void
netbsd32_from_fbcmap(struct fbcmap *, struct netbsd32_fbcmap *);
static __inline void
netbsd32_from_fbcursor(struct fbcursor *, struct netbsd32_fbcursor *);
static __inline void
netbsd32_from_opiocdesc(struct opiocdesc *, struct netbsd32_opiocdesc *);

/* convert to/from different structures */
static __inline void
netbsd32_to_fbcmap(s32p, p, cmd)
	struct netbsd32_fbcmap *s32p;
	struct fbcmap *p;
	u_long cmd;
{

	p->index = s32p->index;
	p->count = s32p->count;
	p->red = (u_char *)(u_long)s32p->red;
	p->green = (u_char *)(u_long)s32p->green;
	p->blue = (u_char *)(u_long)s32p->blue;
}

static __inline void
netbsd32_to_fbcursor(s32p, p, cmd)
	struct netbsd32_fbcursor *s32p;
	struct fbcursor *p;
	u_long cmd;
{

	p->set = s32p->set;
	p->enable = s32p->enable;
	p->pos = s32p->pos;
	p->hot = s32p->hot;
	netbsd32_to_fbcmap(&s32p->cmap, &p->cmap, cmd);
	p->size = s32p->size;
	p->image = (char *)(u_long)s32p->image;
	p->mask = (char *)(u_long)s32p->mask;
}

static __inline void
netbsd32_to_opiocdesc(s32p, p, cmd)
	struct netbsd32_opiocdesc *s32p;
	struct opiocdesc *p;
	u_long cmd;
{

	p->op_nodeid = s32p->op_nodeid;
	p->op_namelen = s32p->op_namelen;
	p->op_name = (char *)(u_long)s32p->op_name;
	p->op_buflen = s32p->op_buflen;
	p->op_buf = (char *)(u_long)s32p->op_buf;
}

static __inline void
netbsd32_from_fbcmap(p, s32p)
	struct fbcmap *p;
	struct netbsd32_fbcmap *s32p;
{

	s32p->index = p->index;
	s32p->count = p->count;
/* filled in */
#if 0
	s32p->red = (netbsd32_u_charp)p->red;
	s32p->green = (netbsd32_u_charp)p->green;
	s32p->blue = (netbsd32_u_charp)p->blue;
#endif
}

static __inline void
netbsd32_from_fbcursor(p, s32p)
	struct fbcursor *p;
	struct netbsd32_fbcursor *s32p;
{

	s32p->set = p->set;
	s32p->enable = p->enable;
	s32p->pos = p->pos;
	s32p->hot = p->hot;
	netbsd32_from_fbcmap(&p->cmap, &s32p->cmap);
	s32p->size = p->size;
/* filled in */
#if 0
	s32p->image = (netbsd32_charp)p->image;
	s32p->mask = (netbsd32_charp)p->mask;
#endif
}

static __inline void
netbsd32_from_opiocdesc(p, s32p)
	struct opiocdesc *p;
	struct netbsd32_opiocdesc *s32p;
{

	s32p->op_nodeid = p->op_nodeid;
	s32p->op_namelen = p->op_namelen;
	s32p->op_name = (netbsd32_charp)(u_long)p->op_name;
	s32p->op_buflen = p->op_buflen;
	s32p->op_buf = (netbsd32_charp)(u_long)p->op_buf;
}

int
netbsd32_md_ioctl(fp, com, data32, p)
	struct file *fp;
	netbsd32_u_long com;
	caddr_t data32;
	struct proc *p;
{
	u_int size;
	caddr_t data, memp = NULL;
#define STK_PARAMS	128
	u_long stkbuf[STK_PARAMS/sizeof(u_long)];
	int error;

	switch (com) {
	case FBIOPUTCMAP32:
		IOCTL_STRUCT_CONV_TO(FBIOPUTCMAP, fbcmap);
	case FBIOGETCMAP32:
		IOCTL_STRUCT_CONV_TO(FBIOGETCMAP, fbcmap);

	case FBIOSCURSOR32:
		IOCTL_STRUCT_CONV_TO(FBIOSCURSOR, fbcursor);
	case FBIOGCURSOR32:
		IOCTL_STRUCT_CONV_TO(FBIOGCURSOR, fbcursor);

	case OPIOCGET32:
		IOCTL_STRUCT_CONV_TO(OPIOCGET, opiocdesc);
	case OPIOCSET32:
		IOCTL_STRUCT_CONV_TO(OPIOCSET, opiocdesc);
	case OPIOCNEXTPROP32:
		IOCTL_STRUCT_CONV_TO(OPIOCNEXTPROP, opiocdesc);
	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data32, p);
	}
	if (memp)
		free(memp, M_IOCTLOPS);
	return (error);
}
