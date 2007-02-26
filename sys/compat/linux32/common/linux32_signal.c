/*	$NetBSD: linux32_signal.c,v 1.1.16.3 2007/02/26 09:09:25 yamt Exp $ */

/*-
 * Copyright (c) 2006 Emmanuel Dreyfus, all rights reserved.
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
 *	This product includes software developed by Emmanuel Dreyfus
 * 4. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE THE AUTHOR AND CONTRIBUTORS ``AS IS'' 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/signalvar.h>
#include <sys/lwp.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <compat/netbsd32/netbsd32.h>

#include <compat/linux32/common/linux32_types.h>
#include <compat/linux32/common/linux32_signal.h>
#include <compat/linux32/linux32_syscallargs.h>

#define linux32_sigemptyset(s)    memset((s), 0, sizeof(*(s)))
#define linux32_sigismember(s, n) ((s)->sig[((n) - 1) / LINUX32__NSIG_BPW]  \
				    & (1 << ((n) - 1) % LINUX32__NSIG_BPW))
#define linux32_sigaddset(s, n)   ((s)->sig[((n) - 1) / LINUX32__NSIG_BPW]  \
				    |= (1 << ((n) - 1) % LINUX32__NSIG_BPW))

extern const int native_to_linux32_signo[];
extern const int linux32_to_native_signo[];

void
linux32_to_native_sigset(bss, lss)
	sigset_t *bss;
	const linux32_sigset_t *lss;
{
	int i, newsig;

	sigemptyset(bss);
	for (i = 1; i < LINUX32__NSIG; i++) {
		if (linux32_sigismember(lss, i)) {
			newsig = linux32_to_native_signo[i];
			if (newsig)
				sigaddset(bss, newsig);
		}
	}
}

void
native_to_linux32_sigset(lss, bss)
	linux32_sigset_t *lss;
	const sigset_t *bss;
{
	int i, newsig;

	linux32_sigemptyset(lss);
	for (i = 1; i < NSIG; i++) {
		if (sigismember(bss, i)) {
			newsig = native_to_linux32_signo[i];
			if (newsig)
				linux32_sigaddset(lss, newsig);
		}
	}
}

unsigned int 
native_to_linux32_sigflags(bsf)
	const int bsf;
{
	unsigned int lsf = 0;
	if ((bsf & SA_NOCLDSTOP) != 0)
		lsf |= LINUX32_SA_NOCLDSTOP;
	if ((bsf & SA_NOCLDWAIT) != 0)
		lsf |= LINUX32_SA_NOCLDWAIT;
	if ((bsf & SA_ONSTACK) != 0)
		lsf |= LINUX32_SA_ONSTACK;
	if ((bsf & SA_RESTART) != 0)
		lsf |= LINUX32_SA_RESTART;
	if ((bsf & SA_NODEFER) != 0)
		lsf |= LINUX32_SA_NOMASK;
	if ((bsf & SA_RESETHAND) != 0)
		lsf |= LINUX32_SA_ONESHOT;
	if ((bsf & SA_SIGINFO) != 0)
		lsf |= LINUX32_SA_SIGINFO;
	return lsf; 
}
 
int
linux32_to_native_sigflags(lsf)
	const unsigned long lsf;
{
	int bsf = 0;
	if ((lsf & LINUX32_SA_NOCLDSTOP) != 0)
		bsf |= SA_NOCLDSTOP; 
	if ((lsf & LINUX32_SA_NOCLDWAIT) != 0)
		bsf |= SA_NOCLDWAIT;
	if ((lsf & LINUX32_SA_ONSTACK) != 0)
		bsf |= SA_ONSTACK;
	if ((lsf & LINUX32_SA_RESTART) != 0)
		bsf |= SA_RESTART;
	if ((lsf & LINUX32_SA_ONESHOT) != 0)
		bsf |= SA_RESETHAND;
	if ((lsf & LINUX32_SA_NOMASK) != 0)
		bsf |= SA_NODEFER;
	if ((lsf & LINUX32_SA_SIGINFO) != 0)
		bsf |= SA_SIGINFO;
	if ((lsf & ~LINUX32_SA_ALLBITS) != 0) {
#ifdef DEBUG_LINUX
		printf("linux32_old_to_native_sigflags: "
		    "%lx extra bits ignored\n", lsf);
#endif
	}
	return bsf;
}    

void
linux32_to_native_sigaction(bsa, lsa)
	struct sigaction *bsa;
	const struct linux32_sigaction *lsa;
{
	bsa->sa_handler = NETBSD32PTR64(lsa->linux_sa_handler);
	linux32_to_native_sigset(&bsa->sa_mask, &lsa->linux_sa_mask);
	bsa->sa_flags = linux32_to_native_sigflags(lsa->linux_sa_flags);
}

void
native_to_linux32_sigaction(lsa, bsa)
	struct linux32_sigaction *lsa;
	const struct sigaction *bsa;
{
	lsa->linux_sa_handler = (linux32_handler_t)(long)bsa->sa_handler;
	native_to_linux32_sigset(&lsa->linux_sa_mask, &bsa->sa_mask);
	lsa->linux_sa_flags = native_to_linux32_sigflags(bsa->sa_flags);
	lsa->linux_sa_restorer = (linux32_restorer_t)NULL;
}

void
native_to_linux32_sigaltstack(lss, bss)
	struct linux32_sigaltstack *lss;
	const struct sigaltstack *bss;
{
	lss->ss_sp = (netbsd32_voidp)(long)bss->ss_sp;
	lss->ss_size = bss->ss_size;
	if (bss->ss_flags & SS_ONSTACK)
	    lss->ss_flags = LINUX32_SS_ONSTACK;
	else if (bss->ss_flags & SS_DISABLE)
	    lss->ss_flags = LINUX32_SS_DISABLE;
	else
	    lss->ss_flags = 0;
}


void
native_to_linux32_old_sigset(lss, bss)
	linux32_old_sigset_t *lss;
	const sigset_t *bss;
{
	linux32_sigset_t lsnew;
 
	native_to_linux32_sigset(&lsnew, bss);
 
	/* convert new sigset to old sigset */
	*lss = lsnew.sig[0];
}

void
linux32_old_to_native_sigset(bss, lss)
	sigset_t *bss;
	const linux32_old_sigset_t *lss;
{
	linux32_sigset_t ls;

	bzero(&ls, sizeof(ls));
	ls.sig[0] = *lss;
	
	linux32_to_native_sigset(bss, &ls);
}

int
linux32_sys_rt_sigaction(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux32_sys_rt_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(const linux32_sigactionp_t) nsa;
		syscallarg(linux32_sigactionp_t) osa;
		syscallarg(netbsd32_size_t) sigsetsize;
	} */ *uap = v;
	struct linux32_sigaction nls32;
	struct linux32_sigaction ols32;
	struct sigaction ns;
	struct sigaction os;
	int error;
	int sig;
	int vers = 0;
	void *tramp = NULL;

	if (SCARG(uap, sigsetsize) != sizeof(linux32_sigset_t))
		return EINVAL;

	if (NETBSD32PTR64(SCARG(uap, nsa)) != NULL) {
		if ((error = copyin(NETBSD32PTR64(SCARG(uap, nsa)), 
		    &nls32, sizeof(nls32))) != 0)
			return error;
		linux32_to_native_sigaction(&ns, &nls32);
	}

	sig = SCARG(uap, signum);
	if (sig < 0 || sig >= LINUX32__NSIG)
		return EINVAL;
	if (sig > 0 && !linux32_to_native_signo[sig]) {
		/* unknown signal... */
		os.sa_handler = SIG_IGN;
		sigemptyset(&os.sa_mask);
		os.sa_flags = 0;
	} else {
		if ((error = sigaction1(l, 
		    linux32_to_native_signo[sig],	
		    NETBSD32PTR64(SCARG(uap, nsa)) ? &ns : NULL,
		    NETBSD32PTR64(SCARG(uap, osa)) ? &os : NULL,
		    tramp, vers)) != 0)
			return error;
	}

	if (NETBSD32PTR64(SCARG(uap, osa)) != NULL) {
		native_to_linux32_sigaction(&ols32, &os);

		if ((error = copyout(&ols32, NETBSD32PTR64(SCARG(uap, osa)),
		    sizeof(ols32))) != 0)
			return error;
	}

	return 0;
}

int
linux32_sys_rt_sigprocmask(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux32_sys_rt_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(const linux32_sigsetp_t) set;
		syscallarg(linux32_sigsetp_t) oset;
		syscallarg(netbsd32_size_t) sigsetsize;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	linux32_sigset_t nls32, ols32;
	sigset_t ns, os;
	int error;
	int how;

	if (SCARG(uap, sigsetsize) != sizeof(linux32_sigset_t))
		return EINVAL;

	switch (SCARG(uap, how)) {
	case LINUX32_SIG_BLOCK:
		how = SIG_BLOCK;
		break;
	case LINUX32_SIG_UNBLOCK:
		how = SIG_UNBLOCK;
		break;
	case LINUX32_SIG_SETMASK:
		how = SIG_SETMASK;
		break;
	default:
		return EINVAL;
		break;
	}

	if (NETBSD32PTR64(SCARG(uap, set)) != NULL) {
		if ((error = copyin(NETBSD32PTR64(SCARG(uap, set)), 
		    &nls32, sizeof(nls32))) != 0)
			return error;
		linux32_to_native_sigset(&ns, &nls32);
	}

	mutex_enter(&p->p_smutex);
	error = sigprocmask1(l, how,
	    NETBSD32PTR64(SCARG(uap, set)) ? &ns : NULL,
	    NETBSD32PTR64(SCARG(uap, oset)) ? &os : NULL);
	mutex_exit(&p->p_smutex);
      
        if (error != 0)
		return error;
		
	if (NETBSD32PTR64(SCARG(uap, oset)) != NULL) {
		native_to_linux32_sigset(&ols32, &os);
		if ((error = copyout(&ols32, 
		    NETBSD32PTR64(SCARG(uap, oset)), sizeof(ols32))) != 0)
			return error;
	}

	return 0;
}

int
linux32_sys_kill(l, v, retval)  
	struct lwp *l;
	void *v;
	register_t *retval;
{  
	struct linux32_sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
 
	struct sys_kill_args ka;
	int sig;
 
	SCARG(&ka, pid) = SCARG(uap, pid);
	sig = SCARG(uap, signum);
	if (sig < 0 || sig >= LINUX32__NSIG)
		return (EINVAL);
	SCARG(&ka, signum) = linux32_to_native_signo[sig];
	return sys_kill(l, &ka, retval);
}  

int
linux32_sys_rt_sigsuspend(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{  
	struct linux32_sys_rt_sigsuspend_args /* {
		syscallarg(linux32_sigsetp_t) unewset;
                syscallarg(netbsd32_size_t) sigsetsize;
	} */ *uap = v;
	linux32_sigset_t lss;
	sigset_t bss;
	int error;

	if (SCARG(uap, sigsetsize) != sizeof(linux32_sigset_t))
		return EINVAL;

	if ((error = copyin(NETBSD32PTR64(SCARG(uap, unewset)), 
	    &lss, sizeof(linux32_sigset_t))) != 0)
		return error;

	linux32_to_native_sigset(&bss, &lss);

	return sigsuspend1(l, &bss);
}

int
linux32_sys_signal(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux32_sys_signal_args /* {
		syscallarg(int) signum;
		syscallarg(linux32_handler_t) handler;
	} */ *uap = v;
        struct sigaction nbsa, obsa;
        int error, sig;

        *retval = -1;

        sig = SCARG(uap, signum);
        if (sig < 0 || sig >= LINUX32__NSIG)
                return EINVAL;

        nbsa.sa_handler = NETBSD32PTR64(SCARG(uap, handler));
        sigemptyset(&nbsa.sa_mask);
        nbsa.sa_flags = SA_RESETHAND | SA_NODEFER;

        if ((error = sigaction1(l, linux32_to_native_signo[sig],
            &nbsa, &obsa, NULL, 0)) != 0)
		return error;

        *retval = (int)(long)obsa.sa_handler;
        return 0;
}
