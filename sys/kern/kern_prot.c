/*	$NetBSD: kern_prot.c,v 1.16.2.1 1994/10/06 04:23:17 mycroft Exp $	*/

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
 *	@(#)kern_prot.c	8.6 (Berkeley) 1/21/94
 */

/*
 * System calls related to processes and protection
 */

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/malloc.h>

/* ARGSUSED */
getpid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_pid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	retval[1] = p->p_pptr->p_pid;
#endif
	return (0);
}

/* ARGSUSED */
getppid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_pptr->p_pid;
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
getpgrp(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_pgrp->pg_id;
	return (0);
}

/* ARGSUSED */
getuid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_cred->p_ruid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	retval[1] = p->p_ucred->cr_uid;
#endif
	return (0);
}

/* ARGSUSED */
geteuid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_ucred->cr_uid;
	return (0);
}

/* ARGSUSED */
getgid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_cred->p_rgid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	retval[1] = p->p_ucred->cr_groups[0];
#endif
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
/* ARGSUSED */
getegid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_ucred->cr_groups[0];
	return (0);
}

struct getgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
getgroups(p, uap, retval)
	struct proc *p;
	register struct	getgroups_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register u_int ngrp;
	int error;

	if ((ngrp = uap->gidsetsize) == 0) {
		*retval = pc->pc_ucred->cr_ngroups;
		return (0);
	}
	if (ngrp < pc->pc_ucred->cr_ngroups)
		return (EINVAL);
	ngrp = pc->pc_ucred->cr_ngroups;
	if (error = copyout((caddr_t)pc->pc_ucred->cr_groups,
	    (caddr_t)uap->gidset, ngrp * sizeof(gid_t)))
		return (error);
	*retval = ngrp;
	return (0);
}

/* ARGSUSED */
setsid(p, uap, retval)
	register struct proc *p;
	void *uap;
	int *retval;
{

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
 * pid must be caller or child of caller (ESRCH)
 * if a child
 *	pid must be in same session (EPERM)
 *	pid can't have done an exec (EACCES)
 * if pgid != pid
 * 	there must exist some pid in same session having pgid (EPERM)
 * pid must not be session leader (EPERM)
 */
struct setpgid_args {
	int	pid;	/* target process id */
	int	pgid;	/* target pgrp id */
};
/* ARGSUSED */
setpgid(curp, uap, retval)
	struct proc *curp;
	register struct setpgid_args *uap;
	int *retval;
{
	register struct proc *targp;		/* target process */
	register struct pgrp *pgrp;		/* target pgrp */

#ifdef COMPAT_09
	uap->pid  = (short) uap->pid;		/* XXX */
	uap->pgid = (short) uap->pgid;		/* XXX */
#endif

	if (uap->pid != 0 && uap->pid != curp->p_pid) {
		if ((targp = pfind(uap->pid)) == 0 || !inferior(targp))
			return (ESRCH);
		if (targp->p_session != curp->p_session)
			return (EPERM);
		if (targp->p_flag & P_EXEC)
			return (EACCES);
	} else
		targp = curp;
	if (SESS_LEADER(targp))
		return (EPERM);
	if (uap->pgid == 0)
		uap->pgid = targp->p_pid;
	else if (uap->pgid != targp->p_pid)
		if ((pgrp = pgfind(uap->pgid)) == 0 ||
	            pgrp->pg_session != curp->p_session)
			return (EPERM);
	return (enterpgrp(targp, uap->pgid, 0));
}

struct setuid_args {
	uid_t	uid;
};
/* ARGSUSED */
setuid(p, uap, retval)
	struct proc *p;
	struct setuid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register uid_t uid;
	int error;

#ifdef COMPAT_09				/* XXX */
	uid = (u_short)uap->uid;
#else
	uid = uap->uid;
#endif
	if (uid != pc->p_ruid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
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
	p->p_flag |= P_SUGID;
	return (0);
}

struct seteuid_args {
	uid_t	euid;
};
/* ARGSUSED */
seteuid(p, uap, retval)
	struct proc *p;
	struct seteuid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register uid_t euid;
	int error;

#ifdef COMPAT_09				/* XXX */
	euid = (u_short)uap->euid;
#else
	euid = uap->euid;
#endif
	if (euid != pc->p_ruid && euid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	/*
	 * Everything's okay, do it.  Copy credentials so other references do
	 * not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = euid;
	p->p_flag |= P_SUGID;
	return (0);
}

struct setgid_args {
	gid_t	gid;
};
/* ARGSUSED */
setgid(p, uap, retval)
	struct proc *p;
	struct setgid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register gid_t gid;
	int error;

#ifdef COMPAT_09				/* XXX */
	gid = (u_short)uap->gid;
#else
	gid = uap->gid;
#endif
	if (gid != pc->p_rgid && (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_groups[0] = gid;
	pc->p_rgid = gid;
	pc->p_svgid = gid;		/* ??? */
	p->p_flag |= P_SUGID;
	return (0);
}

struct setegid_args {
	gid_t	egid;
};
/* ARGSUSED */
setegid(p, uap, retval)
	struct proc *p;
	struct setegid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register gid_t egid;
	int error;

#ifdef COMPAT_09				/* XXX */
	egid = (u_short)uap->egid;
#else
	egid = uap->egid;
#endif
	if (egid != pc->p_rgid && egid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_groups[0] = egid;
	p->p_flag |= P_SUGID;
	return (0);
}

struct setgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
/* ARGSUSED */
setgroups(p, uap, retval)
	struct proc *p;
	struct setgroups_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register u_int ngrp;
	int error;

	if (error = suser(pc->pc_ucred, &p->p_acflag))
		return (error);
	ngrp = uap->gidsetsize;
	if (ngrp < 1 || ngrp > NGROUPS)
		return (EINVAL);
	pc->pc_ucred = crcopy(pc->pc_ucred);
	if (error = copyin((caddr_t)uap->gidset,
	    (caddr_t)pc->pc_ucred->cr_groups, ngrp * sizeof(gid_t)))
		return (error);
	pc->pc_ucred->cr_ngroups = ngrp;
	p->p_flag |= P_SUGID;
	return (0);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
struct setreuid_args {
	int	ruid;
	int	euid;
};
/* ARGSUSED */
osetreuid(p, uap, retval)
	register struct proc *p;
	struct setreuid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	struct seteuid_args args;

	/*
	 * we assume that the intent of setting ruid is to be able to get
	 * back ruid priviledge. So we make sure that we will be able to
	 * do so, but do not actually set the ruid.
	 */
	if (uap->ruid != (uid_t)-1 && uap->ruid != pc->p_ruid &&
	    uap->ruid != pc->p_svuid)
		return (EPERM);
	if (uap->euid == (uid_t)-1)
		return (0);
	args.euid = uap->euid;
	return (seteuid(p, &args, retval));
}

struct setregid_args {
	int	rgid;
	int	egid;
};
/* ARGSUSED */
osetregid(p, uap, retval)
	register struct proc *p;
	struct setregid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	struct setegid_args args;

	/*
	 * we assume that the intent of setting rgid is to be able to get
	 * back rgid priviledge. So we make sure that we will be able to
	 * do so, but do not actually set the rgid.
	 */
	if (uap->rgid != (gid_t)-1 && uap->rgid != pc->p_rgid &&
	    uap->rgid != pc->p_svgid)
		return (EPERM);
	if (uap->egid == (gid_t)-1)
		return (0);
	args.egid = uap->egid;
	return (setegid(p, &args, retval));
}
#endif /* defined(COMPAT_43) || defined(COMPAT_SUNOS) */

/*
 * Check if gid is a member of the group set.
 */
groupmember(gid, cred)
	gid_t gid;
	register struct ucred *cred;
{
	register gid_t *gp;
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
	register struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK);
	bzero((caddr_t)cr, sizeof(*cr));
	cr->cr_ref = 1;
	return (cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 */
crfree(cr)
	struct ucred *cr;
{
	int s;

	s = splimp();				/* ??? */
	if (--cr->cr_ref == 0)
		FREE((caddr_t)cr, M_CRED);
	(void) splx(s);
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
 * Get login name, if available.
 */
struct getlogin_args {
	char	*namebuf;
	u_int	namelen;
};
/* ARGSUSED */
getlogin(p, uap, retval)
	struct proc *p;
	struct getlogin_args *uap;
	int *retval;
{

	if (uap->namelen > sizeof (p->p_pgrp->pg_session->s_login))
		uap->namelen = sizeof (p->p_pgrp->pg_session->s_login);
	return (copyout((caddr_t) p->p_pgrp->pg_session->s_login,
	    (caddr_t) uap->namebuf, uap->namelen));
}

/*
 * Set login name.
 */
struct setlogin_args {
	char	*namebuf;
};
/* ARGSUSED */
setlogin(p, uap, retval)
	struct proc *p;
	struct setlogin_args *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	error = copyinstr((caddr_t) uap->namebuf,
	    (caddr_t) p->p_pgrp->pg_session->s_login,
	    sizeof (p->p_pgrp->pg_session->s_login) - 1, (u_int *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	return (error);
}
