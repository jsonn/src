/*	$NetBSD: irix_signal.c,v 1.31.4.1 2005/03/19 08:33:34 yamt Exp $ */

/*-
 * Copyright (c) 1994, 2001-2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Emmanuel Dreyfus.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irix_signal.c,v 1.31.4.1 2005/03/19 08:33:34 yamt Exp $");

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/ptrace.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <machine/regnum.h>
#include <machine/trap.h>

#include <compat/common/compat_util.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_wait.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_lwp.h>
#include <compat/svr4/svr4_ucontext.h>
#include <compat/svr4/svr4_syscallargs.h>

#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_errno.h>
#include <compat/irix/irix_exec.h>
#include <compat/irix/irix_syscallargs.h>

extern const int native_to_svr4_signo[];
extern const int svr4_to_native_signo[];

static int irix_wait_siginfo __P((struct proc *, int,
    struct irix_irix5_siginfo *));
static void irix_signal_siginfo __P((struct irix_irix5_siginfo *,
    int, u_long, caddr_t));
static void irix_set_ucontext __P((struct irix_ucontext*, const sigset_t *,
    int, struct lwp *));
static void irix_set_sigcontext __P((struct irix_sigcontext*, const sigset_t *,
    int, struct lwp *));
static void irix_get_ucontext __P((struct irix_ucontext*, struct lwp *));
static void irix_get_sigcontext __P((struct irix_sigcontext*, struct lwp *));

#define irix_sigmask(n)	 (1 << (((n) - 1) & 31))
#define irix_sigword(n)	 (((n) - 1) >> 5)
#define irix_sigemptyset(s)     memset((s), 0, sizeof(*(s)))
#define irix_sigismember(s, n)  ((s)->bits[irix_sigword(n)] & irix_sigmask(n))
#define irix_sigaddset(s, n)    ((s)->bits[irix_sigword(n)] |= irix_sigmask(n))

/*
 * Build a struct siginfo wor waitsys/waitid
 * This is ripped from svr4_setinfo. See irix_sys_waitsys...
 */
static int
irix_wait_siginfo(p, st, s)
	struct proc *p;
	int st;
	struct irix_irix5_siginfo *s;
{
	struct irix_irix5_siginfo i;
	int sig;

	memset(&i, 0, sizeof(i));

	i.isi_signo = SVR4_SIGCHLD;
	i.isi_errno = 0; /* XXX? */

	if (p) {
		i.isi_pid = p->p_pid;
		if (p->p_stat == SZOMB) {
			i.isi_stime = p->p_ru->ru_stime.tv_sec;
			i.isi_utime = p->p_ru->ru_utime.tv_sec;
		}
		else {
			i.isi_stime = p->p_stats->p_ru.ru_stime.tv_sec;
			i.isi_utime = p->p_stats->p_ru.ru_utime.tv_sec;
		}
	}

	if (WIFEXITED(st)) {
		i.isi_status = WEXITSTATUS(st);
		i.isi_code = SVR4_CLD_EXITED;
	} else if (WIFSTOPPED(st)) {
		sig = WSTOPSIG(st);
		if (sig >= 0 && sig < NSIG)
			i.isi_status = native_to_svr4_signo[sig];

		if (i.isi_status == SVR4_SIGCONT)
			i.isi_code = SVR4_CLD_CONTINUED;
		else
			i.isi_code = SVR4_CLD_STOPPED;
	} else {
		sig = WTERMSIG(st);
		if (sig >= 0 && sig < NSIG)
			i.isi_status = native_to_svr4_signo[sig];

		if (WCOREDUMP(st))
			i.isi_code = SVR4_CLD_DUMPED;
		else
			i.isi_code = SVR4_CLD_KILLED;
	}

	return copyout(&i, s, sizeof(i));
}

/*
 * Build a struct siginfo for signal delivery
 */
static void
irix_signal_siginfo(isi, sig, code, addr)
	struct irix_irix5_siginfo *isi;
	int sig;
	u_long code;
	caddr_t addr;
{
	if (sig < 0 || sig >= SVR4_NSIG) {
		isi->isi_errno = IRIX_EINVAL;
		return;
	}
	isi->isi_signo = native_to_svr4_signo[sig];
	isi->isi_errno = 0;
	isi->isi_addr = (irix_app32_ptr_t)addr;

	switch (code) {
	case T_TLB_MOD:
	case T_TLB_LD_MISS:
	case T_TLB_ST_MISS:
		switch (sig) {
		case SIGSEGV:
			isi->isi_code = IRIX_SEGV_MAPERR;
			isi->isi_errno = IRIX_EFAULT;
			break;
		case SIGBUS:
			isi->isi_code = IRIX_BUS_ADRERR;
			isi->isi_errno = IRIX_EACCES;
			break;
		case SIGKILL:
			isi->isi_code = IRIX_SEGV_MAPERR;
			isi->isi_errno = IRIX_ENOMEM;
			break;
		default:
			isi->isi_code = 0;
		}
		break;

	case T_ADDR_ERR_LD:
	case T_ADDR_ERR_ST:
	case T_BUS_ERR_IFETCH:
	case T_BUS_ERR_LD_ST:
		/* NetBSD issues a SIGSEGV here, IRIX rather uses SIGBUS */
		isi->isi_code = IRIX_SEGV_MAPERR;
		isi->isi_errno = IRIX_EFAULT;
		break;

	case T_BREAK:
		isi->isi_code = IRIX_TRAP_BRKPT;
		break;

	case T_RES_INST:
	case T_COP_UNUSABLE:
		/* NetBSD issues SIGSEGV here, IRIX rather uses SIGILL */
		isi->isi_code = IRIX_SEGV_MAPERR;
		isi->isi_errno = IRIX_EFAULT;
		break;

	case T_OVFLOW:
		isi->isi_errno = IRIX_EOVERFLOW;
	case T_TRAP:
		isi->isi_code = IRIX_FPE_INTOVF;
		break;

	case T_FPE:
		isi->isi_code = IRIX_FPE_FLTINV;
		break;

	case T_WATCH:
	case T_VCEI:
	case T_VCED:
	case T_INT:
	case T_SYSCALL:
	default:
		isi->isi_code = 0;
#ifdef DEBUG_IRIX
		printf("irix_signal_siginfo: sig %d code %ld\n", sig, code);
#endif
		break;
	}
}

void
native_to_irix_sigset(bss, sss)
	 const sigset_t *bss;
	 irix_sigset_t *sss;
{
	 int i, newsig;

	 irix_sigemptyset(sss);
	 for (i = 1; i < NSIG; i++) {
		 if (sigismember(bss, i)) {
			 newsig = native_to_svr4_signo[i];
			 if (newsig)
			 	irix_sigaddset(sss, newsig);
		 }
	 }
}

void
irix_to_native_sigset(sss, bss)
	const irix_sigset_t *sss;
	sigset_t *bss;
{
	int i, newsig;

	sigemptyset(bss);
	for (i = 1; i < SVR4_NSIG; i++) {
		if (irix_sigismember(sss, i)) {
			newsig = svr4_to_native_signo[i];
			if (newsig)
				sigaddset(bss, newsig);
		}
	}
}

void
irix_sendsig(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	void *sp;
	struct frame *f;
	int onstack;
	int error;
	sig_t catcher = SIGACTION(p, ksi->ksi_signo).sa_handler;
	struct irix_sigframe sf;

	f = (struct frame *)l->l_md.md_regs;
#ifdef DEBUG_IRIX
	printf("irix_sendsig()\n");
	printf("catcher = %p, sig = %d, code = 0x%x\n",
	    (void *)catcher, ksi->ksi_signo, ksi->ksi_trap);
	printf("irix_sendsig(): starting [PC=%p SP=%p SR=0x%08lx]\n",
	    (void *)f->f_regs[_R_PC], (void *)f->f_regs[_R_SP],
	    f->f_regs[_R_SR]);
#endif /* DEBUG_IRIX */

	/*
	 * Do we need to jump onto the signal stack?
	 */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0
		&& (SIGACTION(p, ksi->ksi_signo).sa_flags & SA_ONSTACK) != 0;
#ifdef DEBUG_IRIX
	if (onstack)
		printf("irix_sendsig: using signal stack\n");
#endif
	/*
	 * Allocate space for the signal handler context.
	 */
	if (onstack)
		sp = (void *)((caddr_t)p->p_sigctx.ps_sigstk.ss_sp
		    + p->p_sigctx.ps_sigstk.ss_size);
	else
		/* cast for _MIPS_BSD_API == _MIPS_BSD_API_LP32_64CLEAN case */
		sp = (void *)(u_int32_t)f->f_regs[_R_SP];

	/*
	 * Build the signal frame
	 */
	bzero(&sf, sizeof(sf));
	if (SIGACTION(p, ksi->ksi_signo).sa_flags & SA_SIGINFO) {
		irix_set_ucontext(&sf.isf_ctx.iss.iuc, mask, ksi->ksi_trap, l);
		irix_signal_siginfo(&sf.isf_ctx.iss.iis, ksi->ksi_signo,
		    ksi->ksi_trap, (caddr_t)f->f_regs[_R_BADVADDR]);
	} else {
		irix_set_sigcontext(&sf.isf_ctx.isc, mask, ksi->ksi_trap, l);
	}

	/*
	 * Compute the new stack address after copying sigframe
	 */
	sp = (void *)((unsigned long)sp - sizeof(sf.isf_ctx));
	sp = (void *)((unsigned long)sp & ~0xfUL); /* 16 bytes alignement */

	/*
	 * Install the sigframe onto the stack
	 */
	error = copyout(&sf.isf_ctx, sp, sizeof(sf.isf_ctx));
	if (error != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG_IRIX
		printf("irix_sendsig: stack trashed\n");
#endif /* DEBUG_IRIX */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}


	/*
	 * Set up signal trampoline arguments.
	 */
	f->f_regs[_R_A0] = native_to_svr4_signo[ksi->ksi_signo];/* signo */
	f->f_regs[_R_A1] = 0;			/* NULL */
	f->f_regs[_R_A2] = (unsigned long)sp;	/* ucontext/sigcontext */
	f->f_regs[_R_A3] = (unsigned long)catcher;/* signal handler address */

	/*
	 * When siginfo is selected, the higher bit of A0 is set
	 * This is how the signal trampoline is able to discover if A2
	 * points to a struct irix_sigcontext or struct irix_ucontext.
	 * Also, A1 points to struct siginfo instead of being NULL.
	 */
	if (SIGACTION(p, ksi->ksi_signo).sa_flags & SA_SIGINFO) {
		f->f_regs[_R_A0] |= 0x80000000;
		f->f_regs[_R_A1] = (u_long)sp +
		    ((u_long)&sf.isf_ctx.iss.iis - (u_long)&sf);
	}

	/*
	 * Set up the new stack pointer
	 */
	f->f_regs[_R_SP] = (unsigned long)sp;
#ifdef DEBUG_IRIX
	printf("stack pointer at %p, A1 = %p\n", sp, (void *)f->f_regs[_R_A1]);
#endif /* DEBUG_IRIX */

	/*
	 * Set up the registers to jump to the signal trampoline
	 * on return to userland.
	 * see irix_sys_sigaction for details about how we get
	 * the signal trampoline address.
	 */
	f->f_regs[_R_PC] = (unsigned long)
	    (((struct irix_emuldata *)(p->p_emuldata))->ied_sigtramp[ksi->ksi_signo]);

	/*
	 * Remember that we're now on the signal stack.
	 */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;

#ifdef DEBUG_IRIX
	printf("returning from irix_sendsig()\n");
#endif
	return;
}

static void
irix_set_sigcontext (scp, mask, code, l)
	struct irix_sigcontext *scp;
	const sigset_t *mask;
	int code;
	struct lwp *l;
{
	struct proc *p = l->l_proc;
	int i;
	struct frame *f;

#ifdef DEBUG_IRIX
	printf("irix_set_sigcontext()\n");
#endif
	f = (struct frame *)l->l_md.md_regs;
	/*
	 * Build stack frame for signal trampoline.
	 */
	native_to_irix_sigset(mask, &scp->isc_sigset);
	for (i = 1; i < 32; i++) { /* save gpr1 - gpr31 */
		scp->isc_regs[i] = f->f_regs[i];
	}
	scp->isc_regs[0] = 0;
	scp->isc_fp_rounded_result = 0;
	scp->isc_regmask = ~0x1UL;
	scp->isc_mdhi = f->f_regs[_R_MULHI];
	scp->isc_mdlo = f->f_regs[_R_MULLO];
	scp->isc_pc = f->f_regs[_R_PC];
	scp->isc_badvaddr = f->f_regs[_R_BADVADDR];
	scp->isc_cause = f->f_regs[_R_CAUSE];

	/*
	 * Save the floating-pointstate, if necessary, then copy it.
	 */
#ifndef SOFTFLOAT
	scp->isc_ownedfp = l->l_md.md_flags & MDP_FPUSED;
	if (scp->isc_ownedfp) {
		/* if FPU has current state, save it first */
		if (l == fpcurlwp)
			savefpregs(l);
		(void)memcpy(&scp->isc_fpregs, &l->l_addr->u_pcb.pcb_fpregs,
		    sizeof(scp->isc_fpregs));
		scp->isc_fpc_csr = l->l_addr->u_pcb.pcb_fpregs.r_regs[32];
	}
#else
	(void)memcpy(&scp->isc_fpregs, &l->l_addr->u_pcb.pcb_fpregs,
	    sizeof(scp->isc_fpregs));
#endif
	/*
	 * Save signal stack
	 */
	scp->isc_ssflags =
	    (p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK) ? IRIX_SS_ONSTACK : 0;

	return;
}

void
irix_set_ucontext(ucp, mask, code, l)
	struct irix_ucontext *ucp;
	const sigset_t *mask;
	int code;
	struct lwp *l;
{
	struct proc *p = l->l_proc;
	struct frame *f;

#ifdef DEBUG_IRIX
	printf("irix_set_ucontext()\n");
#endif
	f = (struct frame *)l->l_md.md_regs;
	/*
	 * Save general purpose registers
	 */
	native_to_irix_sigset(mask, &ucp->iuc_sigmask);
	memcpy(&ucp->iuc_mcontext.svr4___gregs,
	    &f->f_regs, 32 * sizeof(mips_reg_t));
	/* Theses registers have different order on NetBSD and IRIX */
	ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_MDLO] = f->f_regs[_R_MULLO];
	ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_MDHI] = f->f_regs[_R_MULHI];
	ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_EPC] = f->f_regs[_R_PC];
	ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_CAUSE] = f->f_regs[_R_CAUSE];

	/*
	 * Save the floating-pointstate, if necessary, then copy it.
	 */
#ifndef SOFTFLOAT
	if (l->l_md.md_flags & MDP_FPUSED) {
		/* if FPU has current state, save it first */
		if (l == fpcurlwp)
			savefpregs(l);
		(void)memcpy(&ucp->iuc_mcontext.svr4___fpregs,
		    &l->l_addr->u_pcb.pcb_fpregs,
		    sizeof(ucp->iuc_mcontext.svr4___fpregs));
		ucp->iuc_mcontext.svr4___fpregs.svr4___fp_csr =
		    l->l_addr->u_pcb.pcb_fpregs.r_regs[32];
	}
#else
	(void)memcpy(&ucp->iuc_mcontext.svr4___fpregs,
	    &l->l_addr->u_pcb.pcb_fpregs,
	    sizeof(ucp->iuc_mcontext.svr4___fpregs));
#endif
	/*
	 * Save signal stack
	 */
	ucp->iuc_stack.ss_sp = p->p_sigctx.ps_sigstk.ss_sp;
	ucp->iuc_stack.ss_size = p->p_sigctx.ps_sigstk.ss_size;

	if (p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK)
		ucp->iuc_stack.ss_flags |= IRIX_SS_ONSTACK;
	else
		ucp->iuc_stack.ss_flags &= ~IRIX_SS_ONSTACK;

	if (p->p_sigctx.ps_sigstk.ss_flags & SS_DISABLE)
		ucp->iuc_stack.ss_flags |= IRIX_SS_DISABLE;
	else
		ucp->iuc_stack.ss_flags &= ~IRIX_SS_DISABLE;

	/*
	 * Used fields in irix_ucontext: all
	 */
	ucp->iuc_flags = IRIX_UC_ALL;

	return;
}

int
irix_sys_sigreturn(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_sigreturn_args /* {
		syscallarg(struct irix_sigcontext *) scp;
		syscallarg(struct irix_ucontext *) ucp;
		syscallarg(int) signo;
	} */ *uap = v;
	void *usf;
	struct irix_sigframe ksf;
	int error;

#ifdef DEBUG_IRIX
	printf("irix_sys_sigreturn()\n");
	printf("scp = %p, ucp = %p, sig = %d\n",
	    (void *)SCARG(uap, scp), (void *)SCARG(uap, ucp),
	    SCARG(uap, signo));
#endif /* DEBUG_IRIX */

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	usf = (void *)SCARG(uap, scp);
	if (usf == NULL) {
		usf = (void *)SCARG(uap, ucp);

		if ((error = copyin(usf, &ksf.isf_ctx.iss.iuc,
		    sizeof(ksf.isf_ctx))) != 0)
			return error;

		irix_get_ucontext(&ksf.isf_ctx.iss.iuc, l);
	} else {
		if ((error = copyin(usf, &ksf.isf_ctx.isc,
		    sizeof(ksf.isf_ctx))) != 0)
			return error;

		irix_get_sigcontext(&ksf.isf_ctx.isc, l);
	}

#ifdef DEBUG_IRIX
	printf("irix_sys_sigreturn(): returning [PC=%p SP=%p SR=0x%08lx]\n",
	    (void *)((struct frame *)(l->l_md.md_regs))->f_regs[_R_PC],
	    (void *)((struct frame *)(l->l_md.md_regs))->f_regs[_R_SP],
	    ((struct frame *)(l->l_md.md_regs))->f_regs[_R_SR]);
#endif

	return EJUSTRETURN;
}

static void
irix_get_ucontext(ucp, l)
	struct irix_ucontext *ucp;
	struct lwp *l;
{
	struct proc *p = l->l_proc;
	struct frame *f;
	sigset_t mask;

	/* Restore the register context. */
	f = (struct frame *)l->l_md.md_regs;

	if (ucp->iuc_flags & IRIX_UC_CPU) {
		(void)memcpy(&f->f_regs, &ucp->iuc_mcontext.svr4___gregs,
		    32 * sizeof(mips_reg_t));
		/* Theses registers have different order on NetBSD and IRIX */
		f->f_regs[_R_MULLO] =
		    ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_MDLO];
		f->f_regs[_R_MULHI] =
		    ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_MDHI];
		f->f_regs[_R_PC] =
		    ucp->iuc_mcontext.svr4___gregs[IRIX_CTX_EPC];
	}

	if (ucp->iuc_flags & IRIX_UC_MAU) {
#ifndef SOFTFLOAT
		/* Disable the FPU to fault in FP registers. */
		f->f_regs[_R_SR] &= ~MIPS_SR_COP_1_BIT;
		if (l == fpcurlwp)
			fpcurlwp = NULL;
		(void)memcpy(&l->l_addr->u_pcb.pcb_fpregs,
		    &ucp->iuc_mcontext.svr4___fpregs,
		    sizeof(l->l_addr->u_pcb.pcb_fpregs));
		l->l_addr->u_pcb.pcb_fpregs.r_regs[32] =
		     ucp->iuc_mcontext.svr4___fpregs.svr4___fp_csr;
#else
		(void)memcpy(&l->l_addr->u_pcb.pcb_fpregs,
		    &ucp->iuc_mcontext.svr4___fpregs,
		    sizeof(l->l_addr->u_pcb.pcb_fpregs));
#endif
	}

	/*
	 * Restore stack
	 */
	if (ucp->iuc_flags & IRIX_UC_STACK) {
		p->p_sigctx.ps_sigstk.ss_sp = ucp->iuc_stack.ss_sp;
		p->p_sigctx.ps_sigstk.ss_size = ucp->iuc_stack.ss_size;

		if (ucp->iuc_stack.ss_flags & IRIX_SS_ONSTACK)
			p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
		else
			p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

		if (ucp->iuc_stack.ss_flags & IRIX_SS_DISABLE)
			p->p_sigctx.ps_sigstk.ss_flags |= IRIX_SS_DISABLE;
		else
			p->p_sigctx.ps_sigstk.ss_flags &= ~IRIX_SS_DISABLE;
	}

	/*
	 * Restore signal mask
	 */
	if (ucp->iuc_flags & IRIX_UC_SIGMASK) {
		/* Restore signal mask. */
		irix_to_native_sigset(&ucp->iuc_sigmask, &mask);
		(void)sigprocmask1(p, SIG_SETMASK, &mask, 0);
	}

	return;
}

static void
irix_get_sigcontext(scp, l)
	struct irix_sigcontext *scp;
	struct lwp *l;
{
	struct proc *p = l->l_proc;
	int i;
	struct frame *f;
	sigset_t mask;

	/* Restore the register context. */
	f = (struct frame *)l->l_md.md_regs;

	for (i = 1; i < 32; i++) /* restore gpr1 to gpr31 */
		f->f_regs[i] = scp->isc_regs[i];
	f->f_regs[_R_MULLO] = scp->isc_mdlo;
	f->f_regs[_R_MULHI] = scp->isc_mdhi;
	f->f_regs[_R_PC] = scp->isc_pc;

#ifndef SOFTFLOAT
	if (scp->isc_ownedfp) {
		/* Disable the FPU to fault in FP registers. */
		f->f_regs[_R_SR] &= ~MIPS_SR_COP_1_BIT;
		if (l == fpcurlwp)
			fpcurlwp = NULL;
		(void)memcpy(&l->l_addr->u_pcb.pcb_fpregs, &scp->isc_fpregs,
		    sizeof(scp->isc_fpregs));
		l->l_addr->u_pcb.pcb_fpregs.r_regs[32] = scp->isc_fpc_csr;
	}
#else
	(void)memcpy(&l->l_addr->u_pcb.pcb_fpregs, &scp->isc_fpregs,
	    sizeof(l->l_addr->u_pcb.pcb_fpregs));
#endif

	/* Restore signal stack. */
	if (scp->isc_ssflags & IRIX_SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;


	/* Restore signal mask. */
	irix_to_native_sigset(&scp->isc_sigset, &mask);
	(void)sigprocmask1(p, SIG_SETMASK, &mask, 0);

	return;
}


int
irix_sys_sginap(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_sginap_args /* {
		syscallarg(long) ticks;
	} */ *uap = v;
	int rticks = SCARG(uap, ticks);
	struct timeval tvb, tve, tvd;
	long long delta;
	int dontcare;

	if (rticks != 0)
		microtime(&tvb);

	if ((tsleep(&dontcare, PZERO|PCATCH, 0, rticks) != 0) &&
	    (rticks != 0)) {
		microtime(&tve);
		timersub(&tve, &tvb, &tvd);
		delta = ((tvd.tv_sec * 1000000) + tvd.tv_usec); /* XXX */
		*retval = (register_t)(rticks - (delta / tick));
	}

	return 0;
}

/*
 * XXX Untested. Expect bugs and security problems here
 */
int
irix_sys_getcontext(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_getcontext_args /* {
		syscallarg(struct irix_ucontext *) ucp;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct frame *f;
	struct irix_ucontext kucp;
	int i, error;

	f = (struct frame *)l->l_md.md_regs;

	kucp.iuc_flags = IRIX_UC_ALL;
	kucp.iuc_link = NULL;		/* XXX */
	native_to_irix_sigset(&p->p_sigctx.ps_sigmask, &kucp.iuc_sigmask);
	kucp.iuc_stack.ss_sp = p->p_sigctx.ps_sigstk.ss_sp;
	kucp.iuc_stack.ss_size = p->p_sigctx.ps_sigstk.ss_size;
	kucp.iuc_stack.ss_flags = 0;
	if (p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK)
		kucp.iuc_stack.ss_flags &= IRIX_SS_ONSTACK;
	if (p->p_sigctx.ps_sigstk.ss_flags & SS_DISABLE)
		kucp.iuc_stack.ss_flags &= IRIX_SS_DISABLE;

	for (i = 0; i < 36; i++) /* Is order correct? */
		kucp.iuc_mcontext.svr4___gregs[i] = f->f_regs[i];
	for (i = 0; i < 32; i++)
		kucp.iuc_mcontext.svr4___fpregs.svr4___fp_r.svr4___fp_regs[i]
		    = 0; /* XXX where are FP registers? */
	for (i = 0; i < 47; i++)
		kucp.iuc_filler[i] = 0;	/* XXX */
	kucp.iuc_triggersave = 0;	/* XXX */

	error = copyout(&kucp, SCARG(uap, ucp), sizeof(kucp));

	return error;
}

/*
 * XXX Untested. Expect bugs and security problems here
 */
int
irix_sys_setcontext(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_setcontext_args /* {
		syscallarg(struct irix_ucontext *) ucp;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct frame *f;
	struct irix_ucontext kucp;
	int i, error;

	error = copyin(SCARG(uap, ucp), &kucp, sizeof(kucp));
	if (error)
		goto out;

	f = (struct frame *)l->l_md.md_regs;

	if (kucp.iuc_flags & IRIX_UC_SIGMASK)
		irix_to_native_sigset(&kucp.iuc_sigmask,
		    &p->p_sigctx.ps_sigmask);

	if (kucp.iuc_flags & IRIX_UC_STACK) {
		p->p_sigctx.ps_sigstk.ss_sp = kucp.iuc_stack.ss_sp;
		p->p_sigctx.ps_sigstk.ss_size =
		    (unsigned long)kucp.iuc_stack.ss_sp;
		p->p_sigctx.ps_sigstk.ss_flags = 0;
		if (kucp.iuc_stack.ss_flags & IRIX_SS_ONSTACK)
			p->p_sigctx.ps_sigstk.ss_flags &= SS_ONSTACK;
		if (kucp.iuc_stack.ss_flags & IRIX_SS_DISABLE)
			p->p_sigctx.ps_sigstk.ss_flags &= SS_DISABLE;
	}

	if (kucp.iuc_flags & IRIX_UC_CPU)
		for (i = 0; i < 36; i++) /* Is register order right? */
			f->f_regs[i] = kucp.iuc_mcontext.svr4___gregs[i];

	if (kucp.iuc_flags & IRIX_UC_MAU) { /* XXX */
#ifdef DEBUG_IRIX
	printf("irix_sys_setcontext(): IRIX_UC_MAU requested\n");
#endif
	}

out:
	return error;
}


/*
 * The following code is from svr4_sys_waitsys(), with a few lines added
 * for supporting the rusage argument which is present in the IRIX version
 * and not in the SVR4 version.
 * Both version could be merged by creating a svr4_sys_waitsys1() with the
 * rusage argument, and by calling it with NULL from svr4_sys_waitsys().
 * irix_wait_siginfo is here because 1) svr4_setinfo is static and cannot be
 * used here and 2) because struct irix_irix5_siginfo is quite different
 * from svr4_siginfo. In order to merge, we need to include irix_signal.h
 * from svr4_misc.c, or push the irix_irix5_siginfo into svr4_siginfo.h
 */
int
irix_sys_waitsys(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_waitsys_args /* {
		syscallarg(int) type;
		syscallarg(int) pid;
		syscallarg(struct irix_irix5_siginfo *) info;
		syscallarg(int) options;
		syscallarg(struct rusage *) ru;
	} */ *uap = v;
	struct proc *parent = l->l_proc;
	int error;
	struct proc *child;
	int options;

	switch (SCARG(uap, type)) {
	case SVR4_P_PID:
		break;

	case SVR4_P_PGID:
		SCARG(uap, pid) = -parent->p_pgid;
		break;

	case SVR4_P_ALL:
		SCARG(uap, pid) = WAIT_ANY;
		break;

	default:
		return EINVAL;
	}

#ifdef DEBUG_IRIX
	printf("waitsys(%d, %d, %p, %x, %p)\n",
		 SCARG(uap, type), SCARG(uap, pid),
		 SCARG(uap, info), SCARG(uap, options), SCARG(uap, ru));
#endif

	/* Translate options */
	options = 0;
	if (SCARG(uap, options) & SVR4_WNOWAIT)
		options |= WNOWAIT;
	if (SCARG(uap, options) & SVR4_WNOHANG)
		options |= WNOHANG;
	if ((SCARG(uap, options) & (SVR4_WEXITED|SVR4_WTRAPPED)) == 0)
		options |= WNOZOMBIE;
	if (SCARG(uap, options) & (SVR4_WSTOPPED|SVR4_WCONTINUED))
		options |= WUNTRACED;

	error = find_stopped_child(parent, SCARG(uap,pid), options, &child);
	if (error != 0)
		return error;
	*retval = 0;
	if (child == NULL)
		return irix_wait_siginfo(NULL, 0, SCARG(uap, info));

	if (child->p_stat == SZOMB) {
#ifdef DEBUG_IRIX
		printf("irix_sys_wait(): found %d\n", child->p_pid);
#endif
		if ((error = irix_wait_siginfo(child, child->p_xstat,
						  SCARG(uap, info))) != 0)
			return error;


		if ((SCARG(uap, options) & SVR4_WNOWAIT)) {
#ifdef DEBUG_IRIX
			printf(("irix_sys_wait(): Don't wait\n"));
#endif
			return 0;
		}
		if (SCARG(uap, ru) &&
		    /* XXX (dsl) is this copying out the right data???
		       child->p_ru would seem more appropriate! */
		    (error = copyout(&(parent->p_stats->p_ru),
		    (caddr_t)SCARG(uap, ru), sizeof(struct rusage))))
			return error;

		proc_free(child);
		return 0;
	}

	/* Child state must be SSTOP */

#ifdef DEBUG_IRIX
	printf("jobcontrol %d\n", child->p_pid);
#endif
	return irix_wait_siginfo(child, W_STOPCODE(child->p_xstat),
				    SCARG(uap, info));
}

int
irix_sys_sigprocmask(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(irix_sigset_t *) set;
		syscallarg(irix_sigset_t *) oset;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct svr4_sys_sigprocmask_args cup;
	int error;
	sigset_t *obss;
	irix_sigset_t niss, oiss;
	caddr_t sg;

	SCARG(&cup, how) = SCARG(uap, how);
	SCARG(&cup, set) = (svr4_sigset_t *)SCARG(uap, set);
	SCARG(&cup, oset) = (svr4_sigset_t *)SCARG(uap, oset);

	if (SCARG(uap, how) == IRIX_SIG_SETMASK32) {
		sg = stackgap_init(p, 0);
		if ((error = copyin(SCARG(uap, set), &niss, sizeof(niss))) != 0)
			return error;
		SCARG(&cup, set) = stackgap_alloc(p, &sg, sizeof(niss));

		obss = &p->p_sigctx.ps_sigmask;
		native_to_irix_sigset(obss, &oiss);
		/* preserve the higher 32 bits */
		niss.bits[3] = oiss.bits[3];

		if ((error = copyout(&niss, (void *)SCARG(&cup, set),
		    sizeof(niss))) != 0)
			return error;

		SCARG(&cup, how) = SVR4_SIG_SETMASK;
	}
	return svr4_sys_sigprocmask(l, &cup, retval);
}

int
irix_sys_sigaction(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(const struct svr4_sigaction *) nsa;
		syscallarg(struct svr4_sigaction *) osa;
		syscallarg(void *) sigtramp;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int signum;
	struct svr4_sys_sigaction_args cup;
	struct irix_emuldata *ied;
#ifdef DEBUG_IRIX
	void *sigtramp;
#endif

	/*
	 * On IRIX, the sigaction() system call has a fourth argument, which
	 * is a pointer to the signal trampoline code. The kernel does not
	 * seems to provide a signal trampoline, the user process has to
	 * embed it. Of course, the sigaction() stub in libc only has three
	 * argument. The fourth argument to the system call is filled by the
	 * libc.
	 *
	 * The signal trampoline does the following job:
	 *   - holds extra bytes on the stack (48 on IRIX 6, 24 on IRIX 5)
	 *     for the signal frame. See struct irix_sigframe in irix_signal.h
	 *     for the details of the signal frame fields for IRIX 6.
	 *   - checks if the higher bit of a0 is set (the kernel sets this
	 *     when SA_SIGINFO is set)
	 *   - if so, stores in a2 sf.isf_ucp, and NULL in sf.isf_scp
	 *     SA_SIGACTION is set, we are using a struct irix_ucontext in a2
	 *   - if not, stores a2 in sf.isf_scp. Here SA_SIGACTION is clear
	 *     and we are using a struct irix_sigcontext in a2.
	 *   - finds the address of errno, and stores it in sf.isf_uep (IRIX 6
	 *     only). This is done by looking up the Global Offset Table and
	 *     assuming that the errnoaddr symbol is at a fixed offset from
	 *     the signal trampoline.
	 *   - invoke the signal handler
	 *   - sets errno using sf.isf_uep and sf.isf_errno (IRIX 6 only)
	 *   - calls sigreturn(sf.isf_scp, sf.isf_ucp, sf.isf_signo) on IRIX 6
	 *     and sigreturn(sf.isf_scp, sf.isf_ucp) on IRIX 5.  Note that if
	 *     SA_SIGINFO was set, then the higher bit of sf.isf_signo is
	 *     still set.
	 *
	 * The signal trampoline is hence saved in the p_emuldata field
	 * of struct proc, in an array (one element for each signal)
	 */
	signum = SCARG(uap, signum);
	if (signum < 0 || signum >= SVR4_NSIG)
		return EINVAL;
	signum = svr4_to_native_signo[signum];
	ied = (struct irix_emuldata *)(p->p_emuldata);

#ifdef DEBUG_IRIX
	sigtramp = ied->ied_sigtramp[signum];

	if (sigtramp != NULL && sigtramp != SCARG(uap, sigtramp))
		printf("Warning: sigtramp changed from %p to %p for sig. %d\n",
			sigtramp, SCARG(uap, sigtramp), signum);
#endif

	ied->ied_sigtramp[signum] = SCARG(uap, sigtramp);

	SCARG(&cup, signum) = signum;
	SCARG(&cup, nsa) = SCARG(uap, nsa);
	SCARG(&cup, osa) = SCARG(uap, osa);

	return svr4_sys_sigaction(l, &cup, retval);
}
