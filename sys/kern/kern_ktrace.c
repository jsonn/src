/*	$NetBSD: kern_ktrace.c,v 1.74.2.2 2003/08/19 19:40:51 skrll Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_ktrace.c	8.5 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_ktrace.c,v 1.74.2.2 2003/08/19 19:40:51 skrll Exp $");

#include "opt_ktrace.h"
#include "opt_compat_mach.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/ktrace.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>

#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

#ifdef KTRACE

int	ktrace_common(struct lwp *, int, int, int, struct file *);
void	ktrinitheader(struct ktr_header *, struct lwp *, int);
int	ktrops(struct lwp *, struct proc *, int, int, struct file *);
int	ktrsetchildren(struct lwp *, struct proc *, int, int,
	    struct file *);
int	ktrwrite(struct lwp *, struct ktr_header *);
int	ktrcanset(struct proc *, struct proc *);
int	ktrsamefile(struct file *, struct file *);

/*
 * "deep" compare of two files for the purposes of clearing a trace.
 * Returns true if they're the same open file, or if they point at the
 * same underlying vnode/socket.
 */

int
ktrsamefile(f1, f2)
	struct file *f1;
	struct file *f2;
{
	return ((f1 == f2) ||
	    ((f1 != NULL) && (f2 != NULL) &&
		(f1->f_type == f2->f_type) &&
		(f1->f_data == f2->f_data)));
}

void
ktrderef(p)
	struct proc *p;
{
	struct file *fp = p->p_tracep;
	p->p_traceflag = 0;
	if (fp == NULL)
		return;
	simple_lock(&fp->f_slock);
	FILE_USE(fp);

	/*
	 * ktrace file descriptor can't be watched (are not visible to
	 * userspace), so no kqueue stuff here
	 */
	closef(fp, NULL);

	p->p_tracep = NULL;
}

void
ktradref(p)
	struct proc *p;
{
	struct file *fp = p->p_tracep;

	fp->f_count++;
}

void
ktrinitheader(kth, l, type)
	struct ktr_header *kth;
	struct lwp *l;
	int type;
{
	struct proc *p = l->l_proc;

	memset(kth, 0, sizeof(*kth));
	kth->ktr_type = type;
	microtime(&kth->ktr_time);
	kth->ktr_pid = p->p_pid;
	memcpy(kth->ktr_comm, p->p_comm, MAXCOMLEN);
}

void
ktrsyscall(l, code, realcode, callp, args)
	struct lwp *l;
	register_t code;
	register_t realcode;
	const struct sysent *callp;
	register_t args[];
{
	struct proc *p = l->l_proc;
	struct ktr_syscall *ktp;
	struct ktr_header kth;
	register_t *argp;
	int argsize;
	size_t len;
	u_int i;

	if (callp == NULL)
		callp = p->p_emul->e_sysent;
	
	argsize = callp[code].sy_narg * sizeof (register_t);
	len = sizeof(struct ktr_syscall) + argsize;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_SYSCALL);
	ktp = malloc(len, M_TEMP, M_WAITOK);
	ktp->ktr_code = realcode;
	ktp->ktr_argsize = argsize;
	argp = (register_t *)((char *)ktp + sizeof(struct ktr_syscall));
	for (i = 0; i < (argsize / sizeof(*argp)); i++)
		*argp++ = args[i];
	kth.ktr_buf = (caddr_t)ktp;
	kth.ktr_len = len;
	(void) ktrwrite(l, &kth);
	free(ktp, M_TEMP);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrsysret(l, code, error, retval)
	struct lwp *l;
	register_t code; 
	int error; 
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_sysret ktp;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_SYSRET);
	ktp.ktr_code = code;
	ktp.ktr_eosys = 0;			/* XXX unused */
	ktp.ktr_error = error;
	ktp.ktr_retval = retval ? retval[0] : 0;
	ktp.ktr_retval_1 = retval ? retval[1] : 0;

	kth.ktr_buf = (caddr_t)&ktp;
	kth.ktr_len = sizeof(struct ktr_sysret);

	(void) ktrwrite(l, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrnamei(l, path)
	struct lwp *l;
	char *path;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_NAMEI);
	kth.ktr_len = strlen(path);
	kth.ktr_buf = path;

	(void) ktrwrite(l, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktremul(l)
	struct lwp *l;
{
	struct ktr_header kth;
	const char *emul;
	struct proc *p;

	p = l->l_proc;
	emul = p->p_emul->e_name;
	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_EMUL);
	kth.ktr_len = strlen(emul);
	kth.ktr_buf = (caddr_t)emul;

	(void) ktrwrite(l, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrgenio(l, fd, rw, iov, len, error)
	struct lwp *l;
	int fd;
	enum uio_rw rw;
	struct iovec *iov;
	int len;
	int error;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_genio *ktp;
	int resid = len, cnt;
	caddr_t cp;
	int buflen;

	if (error)
		return;

	p->p_traceflag |= KTRFAC_ACTIVE;

	buflen = min(PAGE_SIZE, len + sizeof(struct ktr_genio));

	ktrinitheader(&kth, l, KTR_GENIO);
	ktp = malloc(buflen, M_TEMP, M_WAITOK);
	ktp->ktr_fd = fd;
	ktp->ktr_rw = rw;

	kth.ktr_buf = (caddr_t)ktp;

	cp = (caddr_t)((char *)ktp + sizeof(struct ktr_genio));
	buflen -= sizeof(struct ktr_genio);

	while (resid > 0) {
#if 0 /* XXX NJWLWP */
		KDASSERT(p->p_cpu != NULL);
		KDASSERT(p->p_cpu == curcpu());
#endif
		/* XXX NJWLWP */
		if (curcpu()->ci_schedstate.spc_flags & SPCF_SHOULDYIELD)
			preempt(1);

		cnt = min(iov->iov_len, buflen);
		if (cnt > resid)
			cnt = resid;
		if (copyin(iov->iov_base, cp, cnt))
			break;

		kth.ktr_len = cnt + sizeof(struct ktr_genio);

		if (__predict_false(ktrwrite(l, &kth) != 0))
			break;

		iov->iov_base = (caddr_t)iov->iov_base + cnt;
		iov->iov_len -= cnt;

		if (iov->iov_len == 0)
			iov++;

		resid -= cnt;
	}

	free(ktp, M_TEMP);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrpsig(l, sig, action, mask, code)
	struct lwp *l;
	int sig;
	sig_t action;
	sigset_t *mask;
	int code;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_psig	kp;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_PSIG);
	kp.signo = (char)sig;
	kp.action = action;
	kp.mask = *mask;
	kp.code = code;
	kth.ktr_buf = (caddr_t)&kp;
	kth.ktr_len = sizeof(struct ktr_psig);

	(void) ktrwrite(l, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrcsw(l, out, user)
	struct lwp *l;
	int out;
	int user;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_csw kc;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_CSW);
	kc.out = out;
	kc.user = user;
	kth.ktr_buf = (caddr_t)&kc;
	kth.ktr_len = sizeof(struct ktr_csw);

	(void) ktrwrite(l, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktruser(l, id, addr, len, ustr)
	struct lwp *l;
	const char *id;
	void *addr;
	size_t len;
	int ustr;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_user *ktp;
	caddr_t user_dta;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_USER);
	ktp = malloc(sizeof(struct ktr_user) + len, M_TEMP, M_WAITOK);
	if (ustr) {
		if (copyinstr(id, ktp->ktr_id, KTR_USER_MAXIDLEN, NULL) != 0)
			ktp->ktr_id[0] = '\0';
	} else
		strncpy(ktp->ktr_id, id, KTR_USER_MAXIDLEN);
	ktp->ktr_id[KTR_USER_MAXIDLEN-1] = '\0';

	user_dta = (caddr_t) ((char *)ktp + sizeof(struct ktr_user));
	if (copyin(addr, (void *) user_dta, len) != 0)
		len = 0;

	kth.ktr_buf = (void *)ktp;
	kth.ktr_len = sizeof(struct ktr_user) + len;
	(void) ktrwrite(l, &kth);

	free(ktp, M_TEMP);
	p->p_traceflag &= ~KTRFAC_ACTIVE;

}

void
ktrmmsg(l, msgh, size)
	struct lwp *l;
	const void *msgh;
	size_t size;
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_mmsg	*kp;
	
	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_MMSG);
	
	kp = (struct ktr_mmsg *)msgh;
	kth.ktr_buf = (caddr_t)kp;
	kth.ktr_len = size;
	(void) ktrwrite(l, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrsaupcall(struct lwp *l, int type, int nevent, int nint, void *sas,
    void *ap)
{
	struct proc *p = l->l_proc;
	struct ktr_header kth;
	struct ktr_saupcall *ktp;
	size_t len;
	struct sa_t **sapp;
	int i;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, l, KTR_SAUPCALL);

	len = sizeof(struct ktr_saupcall);
	ktp = malloc(len + sizeof(struct sa_t) * (nevent + nint + 1), M_TEMP,
	    M_WAITOK);

	ktp->ktr_type = type;
	ktp->ktr_nevent = nevent;
	ktp->ktr_nint = nint;
	ktp->ktr_sas = sas;
	ktp->ktr_ap = ap;
	/*
	 *  Copy the sa_t's
	 */
	sapp = (struct sa_t **) sas;

	for (i = nevent + nint; i >= 0; i--) {
		if (copyin(*sapp, (char *)ktp + len, sizeof(struct sa_t)) == 0)
			len += sizeof(struct sa_t);
		sapp++;
	}

	kth.ktr_buf = (void *)ktp;
	kth.ktr_len = len;
	(void) ktrwrite(l, &kth);

	free(ktp, M_TEMP);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

/* Interface and common routines */

int
ktrace_common(l, ops, facs, pid, fp)
	struct lwp *l;
	int ops;
	int facs;
	int pid;
	struct file *fp;
{
	struct proc *curp = l->l_proc;
	struct pgrp *pg;
	struct proc *p;
	int error = 0;
	int ret = 0;
	int one = 1;
	int descend;

	curp->p_traceflag |= KTRFAC_ACTIVE;
	descend = ops & KTRFLAG_DESCEND;
	facs = facs & ~((unsigned) KTRFAC_ROOT);

	/*
	 * Clear all uses of the tracefile
	 */
	if (KTROP(ops) == KTROP_CLEARFILE) {
		proclist_lock_read();
		for (p = LIST_FIRST(&allproc); p != NULL;
		     p = LIST_NEXT(p, p_list)) {
			if (ktrsamefile(p->p_tracep, fp)) {
				if (ktrcanset(curp, p))
					ktrderef(p);
				else
					error = EPERM;
			}
		}
		proclist_unlock_read();
		goto done;
	}

	/*
	 * Mark fp non-blocking, to avoid problems from possible deadlocks.
	 */

	if (fp != NULL) {
		fp->f_flag |= FNONBLOCK;
		(*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&one, l);
	}
	
	/*
	 * need something to (un)trace (XXX - why is this here?)
	 */
	if (!facs) {
		error = EINVAL;
		goto done;
	}
	/* 
	 * do it
	 */
	if (pid < 0) {
		/*
		 * by process group
		 */
		pg = pgfind(-pid);
		if (pg == NULL) {
			error = ESRCH;
			goto done;
		}
		for (p = LIST_FIRST(&pg->pg_members); p != NULL;
		     p = LIST_NEXT(p, p_pglist)) {
			if (descend)
				ret |= ktrsetchildren(l, p, ops, facs, fp);
			else 
				ret |= ktrops(l, p, ops, facs, fp);
		}
					
	} else {
		/*
		 * by pid
		 */
		p = pfind(pid);
		if (p == NULL) {
			error = ESRCH;
			goto done;
		}
		if (descend)
			ret |= ktrsetchildren(l, p, ops, facs, fp);
		else
			ret |= ktrops(l, p, ops, facs, fp);
	}
	if (!ret)
		error = EPERM;
done:
	curp->p_traceflag &= ~KTRFAC_ACTIVE;
	return (error);
}

/*
 * ktrace system call
 */
/* ARGSUSED */
int
sys_fktrace(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_fktrace_args /* {
		syscallarg(int) fd;
		syscallarg(int) ops;
		syscallarg(int) facs;
		syscallarg(int) pid;
	} */ *uap = v;
	struct proc *curp;
	struct file *fp = NULL;
	struct filedesc *fdp = l->l_proc->p_fd;
	int error;

	curp = l->l_proc;
	fdp = curp->p_fd;
	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);

	FILE_USE(fp);

	if ((fp->f_flag & FWRITE) == 0)
		error = EBADF;
	else
		error = ktrace_common(l, SCARG(uap, ops),
		    SCARG(uap, facs), SCARG(uap, pid), fp);

	FILE_UNUSE(fp, l);

	return error;
}

/*
 * ktrace system call
 */
/* ARGSUSED */
int
sys_ktrace(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_ktrace_args /* {
		syscallarg(const char *) fname;
		syscallarg(int) ops;
		syscallarg(int) facs;
		syscallarg(int) pid;
	} */ *uap = v;
	struct proc *curp = l->l_proc;
	struct vnode *vp = NULL;
	struct file *fp = NULL;
	int ops = SCARG(uap, ops);
	struct nameidata nd;
	int error = 0;
	int fd;

	ops = KTROP(ops) | (ops & KTRFLAG_DESCEND);

	curp->p_traceflag |= KTRFAC_ACTIVE;
	if ((ops & KTROP_CLEAR) == 0) {
		/*
		 * an operation which requires a file argument.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, fname),
		    l);
		if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
			curp->p_traceflag &= ~KTRFAC_ACTIVE;
			return (error);
		}
		vp = nd.ni_vp;
		VOP_UNLOCK(vp, 0);
		if (vp->v_type != VREG) {
			(void) vn_close(vp, FREAD|FWRITE, curp->p_ucred, l);
			curp->p_traceflag &= ~KTRFAC_ACTIVE;
			return (EACCES);
		}
		/*
		 * XXX This uses up a file descriptor slot in the
		 * tracing process for the duration of this syscall.
		 * This is not expected to be a problem.  If
		 * falloc(NULL, ...) DTRT we could skip that part, but
		 * that would require changing its interface to allow
		 * the caller to pass in a ucred..
		 *
		 * This will FILE_USE the fp it returns, if any.  
		 * Keep it in use until we return.
		 */
		if ((error = falloc(curp, &fp, &fd)) != 0)
			goto done;
		
		fp->f_flag = FWRITE|FAPPEND;
		fp->f_type = DTYPE_VNODE;
		fp->f_ops = &vnops;
		fp->f_data = (caddr_t)vp;
		FILE_SET_MATURE(fp);
		vp = NULL;
	}
	error = ktrace_common(l, SCARG(uap, ops), SCARG(uap, facs),
	    SCARG(uap, pid), fp);
done:	
	if (vp != NULL)
		(void) vn_close(vp, FWRITE, curp->p_ucred, l);
	if (fp != NULL) {
		FILE_UNUSE(fp, l);	/* release file */
		fdrelease(l, fd); 	/* release fd table slot */
	}
	return (error);
}

int
ktrops(l, p, ops, facs, fp)
	struct lwp *l;
	struct proc *p;
	int ops;
	int facs;
	struct file *fp;
{
	struct proc *curp = l->l_proc;

	if (!ktrcanset(curp, p))
		return (0);
	if (KTROP(ops) == KTROP_SET) {
		if (p->p_tracep != fp) { 
			/*
			 * if trace file already in use, relinquish
			 */
			ktrderef(p);
			p->p_tracep = fp;
			ktradref(p);
		}
		p->p_traceflag |= facs;
		if (curp->p_ucred->cr_uid == 0)
			p->p_traceflag |= KTRFAC_ROOT;
	} else {	
		/* KTROP_CLEAR */
		if (((p->p_traceflag &= ~facs) & KTRFAC_MASK) == 0) {
			/* no more tracing */
			ktrderef(p);
		}
	}

	/*
	 * Emit an emulation record, every time there is a ktrace
	 * change/attach request. 
	 */
	if (KTRPOINT(p, KTR_EMUL))
		ktremul(l);
#ifdef __HAVE_SYSCALL_INTERN
	(*p->p_emul->e_syscall_intern)(p);
#endif

	return (1);
}

int
ktrsetchildren(l, top, ops, facs, fp)
	struct lwp *l;
	struct proc *top;
	int ops;
	int facs;
	struct file *fp;
{
	struct proc *p;
	int ret = 0;

	p = top;
	for (;;) {
		ret |= ktrops(l, p, ops, facs, fp);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (LIST_FIRST(&p->p_children) != NULL)
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top)
				return (ret);
			if (LIST_NEXT(p, p_sibling) != NULL) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
	}
	/*NOTREACHED*/
}

int
ktrwrite(l, kth)
	struct lwp *l;
	struct ktr_header *kth;
{
	struct iovec aiov[2];
	int error, tries;
	struct uio auio;
	struct file *fp;
	struct proc *p;

	p = l->l_proc;
	fp = p->p_tracep;

	if (fp == NULL)
		return 0;
	
	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)kth;
	aiov[0].iov_len = sizeof(struct ktr_header);
	auio.uio_resid = sizeof(struct ktr_header);
	auio.uio_iovcnt = 1;
	auio.uio_lwp = (struct lwp *)0;
	if (kth->ktr_len > 0) {
		auio.uio_iovcnt++;
		aiov[1].iov_base = kth->ktr_buf;
		aiov[1].iov_len = kth->ktr_len;
		auio.uio_resid += kth->ktr_len;
	}
	kth->ktr_buf = (caddr_t)l->l_lid;

	simple_lock(&fp->f_slock);
	FILE_USE(fp);

	tries = 0;
	do {
		error = (*fp->f_ops->fo_write)(fp, &fp->f_offset, &auio,
		    fp->f_cred, FOF_UPDATE_OFFSET);
		if (error == EWOULDBLOCK) 
		  	preempt(1);
		else
			tries++;
	} while ((error == EWOULDBLOCK) && (tries < 3));
	FILE_UNUSE(fp, NULL);

	if (__predict_true(error == 0))
		return (0);
	/*
	 * If error encountered, give up tracing on this vnode.  Don't report
	 * EPIPE as this can easily happen with fktrace()/ktruss.
	 */
	if (error != EPIPE)
		log(LOG_NOTICE,
		    "ktrace write failed, errno %d, tracing stopped\n",
		    error);
	proclist_lock_read();
	for (p = LIST_FIRST(&allproc); p != NULL; p = LIST_NEXT(p, p_list)) {
		if (ktrsamefile(p->p_tracep, fp))
			ktrderef(p);
	}
	proclist_unlock_read();

	return (error);
}

/*
 * Return true if caller has permission to set the ktracing state
 * of target.  Essentially, the target can't possess any
 * more permissions than the caller.  KTRFAC_ROOT signifies that
 * root previously set the tracing status on the target process, and 
 * so, only root may further change it.
 *
 * TODO: check groups.  use caller effective gid.
 */
int
ktrcanset(callp, targetp)
	struct proc *callp;
	struct proc *targetp;
{
	struct pcred *caller = callp->p_cred;
	struct pcred *target = targetp->p_cred;

	if ((caller->pc_ucred->cr_uid == target->p_ruid &&
	     target->p_ruid == target->p_svuid &&
	     caller->p_rgid == target->p_rgid &&	/* XXX */
	     target->p_rgid == target->p_svgid &&
	     (targetp->p_traceflag & KTRFAC_ROOT) == 0 &&
	     (targetp->p_flag & P_SUGID) == 0) ||
	     caller->pc_ucred->cr_uid == 0)
		return (1);

	return (0);
}
#endif /* KTRACE */

/*
 * Put user defined entry to ktrace records.
 */
int
sys_utrace(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
#ifdef KTRACE
	struct sys_utrace_args /* {
		syscallarg(const char *) label;
		syscallarg(void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	if (!KTRPOINT(p, KTR_USER))
		return (0);

	if (SCARG(uap, len) > KTR_USER_MAXLEN)
		return (EINVAL);

	ktruser(l, SCARG(uap, label), SCARG(uap, addr), SCARG(uap, len), 1);

	return (0);
#else /* !KTRACE */
	return ENOSYS;
#endif /* KTRACE */
}
