/*	$NetBSD: linux_machdep.c,v 1.24.8.2 2006/08/11 15:43:29 yamt Exp $ */

/*-
 * Copyright (c) 1995, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden and Emmanuel Dreyfus.
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
__KERNEL_RCSID(0, "$NetBSD: linux_machdep.c,v 1.24.8.2 2006/08/11 15:43:29 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>
#include <sys/filedesc.h>
#include <sys/exec_elf.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <miscfs/specfs/specdev.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_util.h>
#include <compat/linux/common/linux_ioctl.h>
#include <compat/linux/common/linux_hdio.h>
#include <compat/linux/common/linux_exec.h>
#include <compat/linux/common/linux_machdep.h>

#include <compat/linux/linux_syscallargs.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/regnum.h>
#include <machine/vmparam.h>
#include <machine/locore.h>

#include <mips/cache.h>

/*
 * To see whether wscons is configured (for virtual console ioctl calls).
 */
#if defined(_KERNEL_OPT)
#include "wsdisplay.h"
#endif
#if (NWSDISPLAY > 0)
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplay_usl_io.h>
#endif

/*
 * Set set up registers on exec.
 * XXX not used at the moment since in sys/kern/exec_conf, LINUX_COMPAT
 * entry uses NetBSD's native setregs instead of linux_setregs
 */
void
linux_setregs(l, pack, stack)
	struct lwp *l;
	struct exec_package *pack;
	u_long stack;
{
	setregs(l, pack, stack);
	return;
}

/*
 * Send an interrupt to process.
 *
 * Adapted from sys/arch/mips/mips/mips_machdep.c
 *
 * XXX Does not work well yet with RT signals
 *
 */

void
linux_sendsig(ksi, mask)
	const ksiginfo_t *ksi;
	const sigset_t *mask;
{
	const int sig = ksi->ksi_signo;
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct linux_sigframe *fp;
	struct frame *f;
	int i,onstack;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct linux_sigframe sf;

#ifdef DEBUG_LINUX
	printf("linux_sendsig()\n");
#endif /* DEBUG_LINUX */
	f = (struct frame *)l->l_md.md_regs;

	/*
	 * Do we need to jump onto the signal stack?
	 */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/*
	 * Signal stack is broken (see at the end of linux_sigreturn), so we do
	 * not use it yet. XXX fix this.
	 */
	onstack=0;

	/*
	 * Allocate space for the signal handler context.
	 */
	if (onstack)
		fp = (struct linux_sigframe *)
		    ((caddr_t)p->p_sigctx.ps_sigstk.ss_sp
		    + p->p_sigctx.ps_sigstk.ss_size);
	else
		/* cast for _MIPS_BSD_API == _MIPS_BSD_API_LP32_64CLEAN case */
		fp = (struct linux_sigframe *)(u_int32_t)f->f_regs[_R_SP];

	/*
	 * Build stack frame for signal trampoline.
	 */
	memset(&sf, 0, sizeof sf);

	/*
	 * This is the signal trampoline used by Linux, we don't use it,
	 * but we set it up in case an application expects it to be there
	 */
	sf.lsf_code[0] = 0x24020000;	/* li	v0, __NR_sigreturn	*/
	sf.lsf_code[1] = 0x0000000c;	/* syscall			*/

	native_to_linux_sigset(&sf.lsf_mask, mask);
	for (i=0; i<32; i++) {
		sf.lsf_sc.lsc_regs[i] = f->f_regs[i];
	}
	sf.lsf_sc.lsc_mdhi = f->f_regs[_R_MULHI];
	sf.lsf_sc.lsc_mdlo = f->f_regs[_R_MULLO];
	sf.lsf_sc.lsc_pc = f->f_regs[_R_PC];
	sf.lsf_sc.lsc_status = f->f_regs[_R_SR];
	sf.lsf_sc.lsc_cause = f->f_regs[_R_CAUSE];
	sf.lsf_sc.lsc_badvaddr = f->f_regs[_R_BADVADDR];

	/*
	 * Save signal stack.  XXX broken
	 */
	/* kregs.sc_onstack = p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK; */

	/*
	 * Install the sigframe onto the stack
	 */
	fp -= sizeof(struct linux_sigframe);
	if (copyout(&sf, fp, sizeof(sf)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG_LINUX
		printf("linux_sendsig: stack trashed\n");
#endif /* DEBUG_LINUX */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	/* Set up the registers to return to sigcode. */
	f->f_regs[_R_A0] = native_to_linux_signo[sig];
	f->f_regs[_R_A1] = 0;
	f->f_regs[_R_A2] = (unsigned long)&fp->lsf_sc;

#ifdef DEBUG_LINUX
	printf("sigcontext is at %p\n", &fp->lsf_sc);
#endif /* DEBUG_LINUX */

	f->f_regs[_R_SP] = (unsigned long)fp;
	/* Signal trampoline code is at base of user stack. */
	f->f_regs[_R_RA] = (unsigned long)p->p_sigctx.ps_sigcode;
	f->f_regs[_R_T9] = (unsigned long)catcher;
	f->f_regs[_R_PC] = (unsigned long)catcher;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;

	return;
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 */
int
linux_sys_sigreturn(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigreturn_args /* {
		syscallarg(struct linux_sigframe *) sf;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct linux_sigframe *sf, ksf;
	struct frame *f;
	sigset_t mask;
	int i, error;

#ifdef DEBUG_LINUX
	printf("linux_sys_sigreturn()\n");
#endif /* DEBUG_LINUX */

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	sf = SCARG(uap, sf);

	if ((error = copyin(sf, &ksf, sizeof(ksf))) != 0)
		return (error);

	/* Restore the register context. */
	f = (struct frame *)l->l_md.md_regs;
	for (i=0; i<32; i++)
		f->f_regs[i] = ksf.lsf_sc.lsc_regs[i];
	f->f_regs[_R_MULLO] = ksf.lsf_sc.lsc_mdlo;
	f->f_regs[_R_MULHI] = ksf.lsf_sc.lsc_mdhi;
	f->f_regs[_R_PC] = ksf.lsf_sc.lsc_pc;
	f->f_regs[_R_BADVADDR] = ksf.lsf_sc.lsc_badvaddr;
	f->f_regs[_R_CAUSE] = ksf.lsf_sc.lsc_cause;

	/* Restore signal stack. */
	p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	linux_to_native_sigset(&mask, (linux_sigset_t *)&ksf.lsf_mask);
	(void)sigprocmask1(p, SIG_SETMASK, &mask, 0);

	return (EJUSTRETURN);
}


int
linux_sys_rt_sigreturn(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	return (ENOSYS);
}


#if 0
int
linux_sys_modify_ldt(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	/*
	 * This syscall is not implemented in Linux/Mips: we should not
	 * be here
	 */
#ifdef DEBUG_LINUX
	printf("linux_sys_modify_ldt: should not be here.\n");
#endif /* DEBUG_LINUX */
  return 0;
}
#endif

/*
 * major device numbers remapping
 */
dev_t
linux_fakedev(dev, raw)
	dev_t dev;
	int raw;
{
	/* XXX write me */
	return dev;
}

/*
 * We come here in a last attempt to satisfy a Linux ioctl() call
 */
int
linux_machdepioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return 0;
}

/*
 * See above. If a root process tries to set access to an I/O port,
 * just let it have the whole range.
 */
int
linux_sys_ioperm(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	/*
	 * This syscall is not implemented in Linux/Mips: we should not be here
	 */
#ifdef DEBUG_LINUX
	printf("linux_sys_ioperm: should not be here.\n");
#endif /* DEBUG_LINUX */
	return 0;
}

/*
 * wrapper linux_sys_new_uname() -> linux_sys_uname()
 */
int
linux_sys_new_uname(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
/*
 * Use this if you want to try Linux emulation with a glibc-2.2
 * or higher. Note that signals will not work
 */
#if 0
        struct linux_sys_uname_args /* {
                syscallarg(struct linux_utsname *) up;
        } */ *uap = v;
        struct linux_utsname luts;

        strncpy(luts.l_sysname, linux_sysname, sizeof(luts.l_sysname));
        strncpy(luts.l_nodename, hostname, sizeof(luts.l_nodename));
        strncpy(luts.l_release, "2.4.0", sizeof(luts.l_release));
        strncpy(luts.l_version, linux_version, sizeof(luts.l_version));
        strncpy(luts.l_machine, machine, sizeof(luts.l_machine));
        strncpy(luts.l_domainname, domainname, sizeof(luts.l_domainname));

        return copyout(&luts, SCARG(uap, up), sizeof(luts));
#else
	return linux_sys_uname(l, v, retval);
#endif
}

/*
 * In Linux, cacheflush is currently implemented
 * as a whole cache flush (arguments are ignored)
 * we emulate this broken beahior.
 */
int
linux_sys_cacheflush(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	mips_icache_sync_all();
	mips_dcache_wbinv_all();
	return 0;
}

/*
 * This system call is depecated in Linux, but
 * some binaries and some libraries use it.
 */
int
linux_sys_sysmips(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_sysmips_args {
		syscallarg(int) cmd;
		syscallarg(int) arg1;
		syscallarg(int) arg2;
		syscallarg(int) arg3;
	} *uap = v;
	int error;

	switch (SCARG(uap, cmd)) {
	case LINUX_SETNAME: {
		char nodename [LINUX___NEW_UTS_LEN + 1];
		int name[2];
		size_t len;

		if ((error = kauth_authorize_generic(l->l_cred,
		    KAUTH_GENERIC_ISSUSER, &l->l_acflag)) != 0)
			return error;
		if ((error = copyinstr((char *)SCARG(uap, arg1), nodename,
		    LINUX___NEW_UTS_LEN, &len)) != 0)
			return error;

		name[0] = CTL_KERN;
		name[1] = KERN_HOSTNAME;
		return (old_sysctl(&name[0], 2, 0, 0, nodename, len, NULL));

		break;
	}
	case LINUX_MIPS_ATOMIC_SET: {
		void *addr;
		int s;
		u_int8_t value = 0;

		addr = (void *)SCARG(uap, arg1);

		s = splhigh();
		/*
		 * No error testing here. This is bad, but Linux does
		 * it like this. The source aknowledge "This is broken"
		 * in a comment...
		 */
		(void) copyin(addr, &value, 1);
		*retval = value;
		value = (u_int8_t) SCARG(uap, arg2);
		error = copyout(&value, addr, 1);
		splx(s);

		return 0;
		break;
	}
	case LINUX_MIPS_FIXADE:		/* XXX not implemented */
		break;
	case LINUX_FLUSH_CACHE:
		mips_icache_sync_all();
		mips_dcache_wbinv_all();
		break;
	case LINUX_MIPS_RDNVRAM:
		return EIO;
		break;
	default:
		return EINVAL;
		break;
	}
#ifdef DEBUG_LINUX
	printf("linux_sys_sysmips(): unimplemented command %d\n",
	    SCARG(uap,cmd));
#endif /* DEBUG_LINUX */
	return 0;
}

int
linux_usertrap(struct lwp *l, vaddr_t trapaddr, void *arg)
{
	return 0;
}
