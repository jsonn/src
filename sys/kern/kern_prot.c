/*	$NetBSD: kern_prot.c,v 1.63.2.5 2002/02/28 19:59:35 nathanw Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_prot.c	8.9 (Berkeley) 2/14/95
 */

/*
 * System calls related to processes and protection
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_prot.c,v 1.63.2.5 2002/02/28 19:59:35 nathanw Exp $");

#include "opt_compat_43.h"

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/malloc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

int	sys_getpid(struct lwp *, void *, register_t *);
int	sys_getpid_with_ppid(struct lwp *, void *, register_t *);
int	sys_getuid(struct lwp *, void *, register_t *);
int	sys_getuid_with_euid(struct lwp *, void *, register_t *);
int	sys_getgid(struct lwp *, void *, register_t *);
int	sys_getgid_with_egid(struct lwp *, void *, register_t *);

/* ARGSUSED */
int
sys_getpid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	*retval = p->p_pid;
	return (0);
}

/* ARGSUSED */
int
sys_getpid_with_ppid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	retval[0] = p->p_pid;
	retval[1] = p->p_pptr->p_pid;
	return (0);
}

/* ARGSUSED */
int
sys_getppid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	*retval = p->p_pptr->p_pid;
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
int
sys_getpgrp(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	*retval = p->p_pgrp->pg_id;
	return (0);
}

/*
 * Return the process group ID of the session leader (session ID)
 * for the specified process.
 */
int
sys_getsid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_getsid_args /* {
		syscalldarg(pid_t) pid;
	} */ *uap = v;
	struct proc *p = l->l_proc;

	if (SCARG(uap, pid) == 0)
		goto found;
	if ((p = pfind(SCARG(uap, pid))) == 0)
		return (ESRCH);
found:
	*retval = p->p_session->s_sid;
	return (0);
}

int
sys_getpgid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_getpgid_args /* {
		syscallarg(pid_t) pid;
	} */ *uap = v;
	struct proc *p = l->l_proc;

	if (SCARG(uap, pid) == 0)
		goto found;
	if ((p = pfind(SCARG(uap, pid))) == 0)
		return (ESRCH);
found:
	*retval = p->p_pgid;
	return (0);
}

/* ARGSUSED */
int
sys_getuid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	*retval = p->p_cred->p_ruid;
	return (0);
}

/* ARGSUSED */
int
sys_getuid_with_euid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	retval[0] = p->p_cred->p_ruid;
	retval[1] = p->p_ucred->cr_uid;
	return (0);
}

/* ARGSUSED */
int
sys_geteuid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	*retval = p->p_ucred->cr_uid;
	return (0);
}

/* ARGSUSED */
int
sys_getgid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	*retval = p->p_cred->p_rgid;
	return (0);
}

/* ARGSUSED */
int
sys_getgid_with_egid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	retval[0] = p->p_cred->p_rgid;
	retval[1] = p->p_ucred->cr_gid;
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
/* ARGSUSED */
int
sys_getegid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	*retval = p->p_ucred->cr_gid;
	return (0);
}

int
sys_getgroups(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_getgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(gid_t *) gidset;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	int ngrp;
	int error;

	ngrp = SCARG(uap, gidsetsize);
	if (ngrp == 0) {
		*retval = pc->pc_ucred->cr_ngroups;
		return (0);
	}
	if (ngrp < pc->pc_ucred->cr_ngroups)
		return (EINVAL);
	ngrp = pc->pc_ucred->cr_ngroups;
	error = copyout((caddr_t)pc->pc_ucred->cr_groups,
			(caddr_t)SCARG(uap, gidset), ngrp * sizeof(gid_t));
	if (error)
		return (error);
	*retval = ngrp;
	return (0);
}

/* ARGSUSED */
int
sys_setsid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;

	if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
		return (EPERM);
	} else {
		(void)enterpgrp(p, p->p_pid, 1);
		*retval = p->p_pid;
		return (0);
	}
}

/*
 * set process group (setpgid/old setpgrp)
 *
 * caller does setpgid(targpid, targpgid)
 *
 * pgid must be in valid range (EINVAL)
 * pid must be caller or child of caller (ESRCH)
 * if a child
 *	pid must be in same session (EPERM)
 *	pid can't have done an exec (EACCES)
 * if pgid != pid
 * 	there must exist some pid in same session having pgid (EPERM)
 * pid must not be session leader (EPERM)
 */
/* ARGSUSED */
int
sys_setpgid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setpgid_args /* {
		syscallarg(int) pid;
		syscallarg(int) pgid;
	} */ *uap = v;
	struct proc *curp = l->l_proc;
	struct proc *targp;			/* target process */
	struct pgrp *pgrp;			/* target pgrp */

	if (SCARG(uap, pgid) < 0)
		return (EINVAL);

	if (SCARG(uap, pid) != 0 && SCARG(uap, pid) != curp->p_pid) {
		if ((targp = pfind(SCARG(uap, pid))) == 0
		    || !inferior(targp, curp))
			return (ESRCH);
		if (targp->p_session != curp->p_session)
			return (EPERM);
		if (targp->p_flag & P_EXEC)
			return (EACCES);
	} else
		targp = curp;
	if (SESS_LEADER(targp))
		return (EPERM);
	if (SCARG(uap, pgid) == 0)
		SCARG(uap, pgid) = targp->p_pid;
	else if (SCARG(uap, pgid) != targp->p_pid)
		if ((pgrp = pgfind(SCARG(uap, pgid))) == 0 ||
	            pgrp->pg_session != curp->p_session)
			return (EPERM);
	return (enterpgrp(targp, SCARG(uap, pgid), 0));
}

/* ARGSUSED */
int
sys_setuid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setuid_args /* {
		syscallarg(uid_t) uid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	uid_t uid;
	int error;

	uid = SCARG(uap, uid);
	if (uid != pc->p_ruid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	/*
	 * Check if we are all set, and this is a no-op.
	 */
	if (pc->p_ruid == uid && pc->p_svuid == uid &&
	    pc->pc_ucred->cr_uid == uid)
		return (0);
	/*
	 * Everything's okay, do it.
	 * Transfer proc count to new user.
	 * Copy credentials so other references do not see our changes.
	 */
	(void)chgproccnt(pc->p_ruid, -1);
	(void)chgproccnt(uid, 1);
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = uid;
	pc->p_ruid = uid;
	pc->p_svuid = uid;
	p_sugid(p);
	return (0);
}

/* ARGSUSED */
int
sys_seteuid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_seteuid_args /* {
		syscallarg(uid_t) euid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	uid_t euid;
	int error;

	euid = SCARG(uap, euid);
	if (euid != pc->p_ruid && euid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	/*
	 * Check if we are all set, and this is a no-op.
	 */
	if (pc->pc_ucred->cr_uid == euid)
		return (0);

	/*
	 * Everything's okay, do it.  Copy credentials so other references do
	 * not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = euid;
	p_sugid(p);
	return (0);
}

int
sys_setreuid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setreuid_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	uid_t ruid, euid;
	int error, changed = 0;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);

	if (ruid != (uid_t)-1 &&
	    ruid != pc->p_ruid && ruid != pc->pc_ucred->cr_uid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (euid != (uid_t)-1 &&
	    euid != pc->p_ruid && euid != pc->pc_ucred->cr_uid &&
	    euid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (euid != (uid_t)-1 && euid != pc->pc_ucred->cr_uid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_uid = euid;
		changed++;
	}

	if (ruid != (uid_t)-1 &&
	    (pc->p_ruid != ruid || pc->p_svuid != pc->pc_ucred->cr_uid)) {
		(void)chgproccnt(pc->p_ruid, -1);
		(void)chgproccnt(ruid, 1);
		pc->p_ruid = ruid;
		pc->p_svuid = pc->pc_ucred->cr_uid;
		changed++;
	}

	if (changed)
		p_sugid(p);
	return (0);
}

/* ARGSUSED */
int
sys_setgid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setgid_args /* {
		syscallarg(gid_t) gid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	gid_t gid;
	int error;

	gid = SCARG(uap, gid);
	if (gid != pc->p_rgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	/*
	 * Check if we are all set, and this is a no-op.
	 */
	if (pc->pc_ucred->cr_gid == gid && pc->p_rgid == gid &&
	    pc->p_svgid == gid)
		return (0);

	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = gid;
	pc->p_rgid = gid;
	pc->p_svgid = gid;
	p_sugid(p);
	return (0);
}

/* ARGSUSED */
int
sys_setegid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setegid_args /* {
		syscallarg(gid_t) egid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	gid_t egid;
	int error;

	egid = SCARG(uap, egid);
	if (egid != pc->p_rgid && egid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	/*
	 * Check if we are all set, and this is a no-op.
	 */
	if (pc->pc_ucred->cr_gid == egid)
		return (0);

	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = egid;
	p_sugid(p);
	return (0);
}

int
sys_setregid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setregid_args /* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	gid_t rgid, egid;
	int error, changed = 0;

	rgid = SCARG(uap, rgid);
	egid = SCARG(uap, egid);

	if (rgid != (gid_t)-1 &&
	    rgid != pc->p_rgid && rgid != pc->pc_ucred->cr_gid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (egid != (gid_t)-1 &&
	    egid != pc->p_rgid && egid != pc->pc_ucred->cr_gid &&
	    egid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (egid != (gid_t)-1 && pc->pc_ucred->cr_gid != egid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_gid = egid;
		changed++;
	}

	if (rgid != (gid_t)-1 &&
	    (pc->p_rgid != rgid || pc->p_svgid != pc->pc_ucred->cr_gid)) {
		pc->p_rgid = rgid;
		pc->p_svgid = pc->pc_ucred->cr_gid;
		changed++;
	}

	if (changed)
		p_sugid(p);
	return (0);
}

int
sys_issetugid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use P_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	*retval = (p->p_flag & P_SUGID) != 0;
	return (0);
}

/* ARGSUSED */
int
sys_setgroups(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_setgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(const gid_t *) gidset;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct pcred *pc = p->p_cred;
	int ngrp;
	int error;
	gid_t grp[NGROUPS];
	size_t grsize;

	if ((error = suser(pc->pc_ucred, &p->p_acflag)) != 0)
		return (error);

	ngrp = SCARG(uap, gidsetsize);
	if ((u_int)ngrp > NGROUPS)
		return (EINVAL);

	grsize = ngrp * sizeof(gid_t);
	error = copyin(SCARG(uap, gidset), grp, grsize);
	if (error)
		return (error);
	/*
	 * Check if this is a no-op.
	 */
	if (pc->pc_ucred->cr_ngroups == ngrp &&
	    memcmp(grp, pc->pc_ucred->cr_groups, grsize) == 0)
		return (0);

	pc->pc_ucred = crcopy(pc->pc_ucred);
	(void)memcpy(pc->pc_ucred->cr_groups, grp, grsize);
	pc->pc_ucred->cr_ngroups = ngrp;
	p_sugid(p);
	return (0);
}

/*
 * Check if gid is a member of the group set.
 */
int
groupmember(gid, cred)
	gid_t gid;
	struct ucred *cred;
{
	gid_t *gp;
	gid_t *egp;

	egp = &(cred->cr_groups[cred->cr_ngroups]);
	for (gp = cred->cr_groups; gp < egp; gp++)
		if (*gp == gid)
			return (1);
	return (0);
}

/*
 * Test whether the specified credentials imply "super-user"
 * privilege; if so, and we have accounting info, set the flag
 * indicating use of super-powers.
 * Returns 0 or error.
 */
int
suser(cred, acflag)
	struct ucred *cred;
	u_short *acflag;
{
	if (cred->cr_uid == 0) {
		if (acflag)
			*acflag |= ASU;
		return (0);
	}
	return (EPERM);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget()
{
	struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK);
	memset((caddr_t)cr, 0, sizeof(*cr));
	cr->cr_ref = 1;
	return (cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 */
void
crfree(cr)
	struct ucred *cr;
{

	if (--cr->cr_ref == 0)
		FREE((caddr_t)cr, M_CRED);
}

/*
 * Copy cred structure to a new one and free the old one.
 */
struct ucred *
crcopy(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	if (cr->cr_ref == 1)
		return (cr);
	newcr = crget();
	*newcr = *cr;
	crfree(cr);
	newcr->cr_ref = 1;
	return (newcr);
}

/*
 * Dup cred struct to a new held one.
 */
struct ucred *
crdup(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	newcr = crget();
	*newcr = *cr;
	newcr->cr_ref = 1;
	return (newcr);
}

/*
 * convert from userland credentials to kernel one
 */
void
crcvt(uc, uuc)
	struct ucred *uc;
	const struct uucred *uuc;
{
	uc->cr_ref = 0;
	uc->cr_uid = uuc->cr_uid;
	uc->cr_gid = uuc->cr_gid;
	uc->cr_ngroups = uuc->cr_ngroups;
	(void)memcpy(uc->cr_groups, uuc->cr_groups, sizeof(uuc->cr_groups));
}

/*
 * Get login name, if available.
 */
/* ARGSUSED */
int
sys___getlogin(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys___getlogin_args /* {
		syscallarg(char *) namebuf;
		syscallarg(size_t) namelen;
	} */ *uap = v;
	struct proc *p = l->l_proc;

	if (SCARG(uap, namelen) > sizeof(p->p_pgrp->pg_session->s_login))
		SCARG(uap, namelen) = sizeof(p->p_pgrp->pg_session->s_login);
	return (copyout((caddr_t) p->p_pgrp->pg_session->s_login,
	    (caddr_t) SCARG(uap, namebuf), SCARG(uap, namelen)));
}

/*
 * Set login name.
 */
/* ARGSUSED */
int
sys___setlogin(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys___setlogin_args /* {
		syscallarg(const char *) namebuf;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	error = copyinstr(SCARG(uap, namebuf), p->p_pgrp->pg_session->s_login,
	    sizeof(p->p_pgrp->pg_session->s_login) - 1, (size_t *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	return (error);
}
