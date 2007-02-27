/*	$NetBSD: kern_prot.c,v 1.97.2.1 2007/02/27 16:54:23 yamt Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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
__KERNEL_RCSID(0, "$NetBSD: kern_prot.c,v 1.97.2.1 2007/02/27 16:54:23 yamt Exp $");

#include "opt_compat_43.h"

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/pool.h>
#include <sys/syslog.h>
#include <sys/resourcevar.h>
#include <sys/kauth.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <sys/malloc.h>

int	sys_getpid(struct lwp *, void *, register_t *);
int	sys_getpid_with_ppid(struct lwp *, void *, register_t *);
int	sys_getuid(struct lwp *, void *, register_t *);
int	sys_getuid_with_euid(struct lwp *, void *, register_t *);
int	sys_getgid(struct lwp *, void *, register_t *);
int	sys_getgid_with_egid(struct lwp *, void *, register_t *);

static int grsortu(gid_t *, int);

/* ARGSUSED */
int
sys_getpid(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	*retval = p->p_pid;
	return (0);
}

/* ARGSUSED */
int
sys_getpid_with_ppid(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	retval[0] = p->p_pid;
	rw_enter(&proclist_lock, RW_READER);
	retval[1] = p->p_pptr->p_pid;
	rw_exit(&proclist_lock);
	return (0);
}

/* ARGSUSED */
int
sys_getppid(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	rw_enter(&proclist_lock, RW_READER);
	*retval = p->p_pptr->p_pid;
	rw_exit(&proclist_lock);
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
int
sys_getpgrp(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	rw_enter(&proclist_lock, RW_READER);
	*retval = p->p_pgrp->pg_id;
	rw_exit(&proclist_lock);
	return (0);
}

/*
 * Return the process group ID of the session leader (session ID)
 * for the specified process.
 */
int
sys_getsid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_getsid_args /* {
		syscalldarg(pid_t) pid;
	} */ *uap = v;
	pid_t pid = SCARG(uap, pid);
	struct proc *p;
	int error = 0;

	rw_enter(&proclist_lock, RW_READER);
	if (pid == 0)
		*retval = l->l_proc->p_session->s_sid;
	else if ((p = p_find(pid, PFIND_LOCKED)) != NULL)
		*retval = p->p_session->s_sid;
	else
		error = ESRCH;
	rw_exit(&proclist_lock);

	return error;
}

int
sys_getpgid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_getpgid_args /* {
		syscallarg(pid_t) pid;
	} */ *uap = v;
	pid_t pid = SCARG(uap, pid);
	struct proc *p;
	int error = 0;

	rw_enter(&proclist_lock, RW_READER);
	if (pid == 0)
		*retval = l->l_proc->p_pgid;
	else if ((p = p_find(pid, PFIND_LOCKED)) != NULL)
		*retval = p->p_pgid;
	else
		error = ESRCH;
	rw_exit(&proclist_lock);

	return error;
}

/* ARGSUSED */
int
sys_getuid(struct lwp *l, void *v, register_t *retval)
{

	*retval = kauth_cred_getuid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_getuid_with_euid(struct lwp *l, void *v, register_t *retval)
{

	retval[0] = kauth_cred_getuid(l->l_cred);
	retval[1] = kauth_cred_geteuid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_geteuid(struct lwp *l, void *v, register_t *retval)
{

	*retval = kauth_cred_geteuid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_getgid(struct lwp *l, void *v, register_t *retval)
{

	*retval = kauth_cred_getgid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_getgid_with_egid(struct lwp *l, void *v, register_t *retval)
{

	retval[0] = kauth_cred_getgid(l->l_cred);
	retval[1] = kauth_cred_getegid(l->l_cred);
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
/* ARGSUSED */
int
sys_getegid(struct lwp *l, void *v, register_t *retval)
{

	*retval = kauth_cred_getegid(l->l_cred);
	return (0);
}

int
sys_getgroups(struct lwp *l, void *v, register_t *retval)
{
	struct sys_getgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(gid_t *) gidset;
	} */ *uap = v;
	kauth_cred_t cred = l->l_cred;
	u_int ngrp;
	int error;
	gid_t *grbuf;

	if (SCARG(uap, gidsetsize) == 0) {
		*retval = kauth_cred_ngroups(cred);
		return (0);
	} else if (SCARG(uap, gidsetsize) < 0)
		return (EINVAL);
	ngrp = SCARG(uap, gidsetsize);
	if (ngrp < kauth_cred_ngroups(cred))
		return (EINVAL);
	ngrp = kauth_cred_ngroups(cred);

	grbuf = malloc(ngrp * sizeof(*grbuf), M_TEMP, M_WAITOK);
	kauth_cred_getgroups(cred, grbuf, ngrp);
	error = copyout(grbuf, (caddr_t)SCARG(uap, gidset),
			ngrp * sizeof(gid_t));
	free(grbuf, M_TEMP);
	if (error)
		return (error);
	*retval = ngrp;
	return (0);
}

/* ARGSUSED */
int
sys_setsid(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;
	int error;

	error = enterpgrp(p, p->p_pid, p->p_pid, 1);
	*retval = p->p_pid;
	return (error);
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
 *
 * Permission checks now in enterpgrp()
 */
/* ARGSUSED */
int
sys_setpgid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setpgid_args /* {
		syscallarg(int) pid;
		syscallarg(int) pgid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	pid_t targp, pgid;

	if (SCARG(uap, pgid) < 0)
		return EINVAL;
	if ((targp = SCARG(uap, pid)) == 0)
		targp = p->p_pid;
	if ((pgid = SCARG(uap, pgid)) == 0)
		pgid = targp;

	return enterpgrp(p, targp, pgid, 0);
}

/*
 * Set real, effective and saved uids to the requested values.
 * non-root callers can only ever change uids to values that match
 * one of the processes current uid values.
 * This is further restricted by the flags argument.
 */

int
do_setresuid(struct lwp *l, uid_t r, uid_t e, uid_t sv, u_int flags)
{
	struct proc *p = l->l_proc;
	kauth_cred_t cred, ncred;

	ncred = kauth_cred_alloc();

	/* Get a write lock on the process credential. */
	proc_crmod_enter();
	cred = p->p_cred;

	/*
	 * Check that the new value is one of the allowed existing values,
	 * or that we have root privilege.
	 */
	if ((r != -1
	    && !((flags & ID_R_EQ_R) && r == kauth_cred_getuid(cred))
	    && !((flags & ID_R_EQ_E) && r == kauth_cred_geteuid(cred))
	    && !((flags & ID_R_EQ_S) && r == kauth_cred_getsvuid(cred))) ||
	    (e != -1
	    && !((flags & ID_E_EQ_R) && e == kauth_cred_getuid(cred))
	    && !((flags & ID_E_EQ_E) && e == kauth_cred_geteuid(cred))
	    && !((flags & ID_E_EQ_S) && e == kauth_cred_getsvuid(cred))) ||
	    (sv != -1
	    && !((flags & ID_S_EQ_R) && sv == kauth_cred_getuid(cred))
	    && !((flags & ID_S_EQ_E) && sv == kauth_cred_geteuid(cred))
	    && !((flags & ID_S_EQ_S) && sv == kauth_cred_getsvuid(cred)))) {
		int error;

		error = kauth_authorize_process(cred, KAUTH_PROCESS_SETID,
		    p, NULL, NULL, NULL);
		if (error != 0) {
		 	proc_crmod_leave(cred, ncred, false);
			return error;
		}
	}

	/* If nothing has changed, short circuit the request */
	if ((r == -1 || r == kauth_cred_getuid(cred))
	    && (e == -1 || e == kauth_cred_geteuid(cred))
	    && (sv == -1 || sv == kauth_cred_getsvuid(cred))) {
		proc_crmod_leave(cred, ncred, false);
		return 0;
	}

	kauth_cred_clone(cred, ncred);

	if (r != -1 && r != kauth_cred_getuid(ncred)) {
		/* Update count of processes for this user */
		(void)chgproccnt(kauth_cred_getuid(ncred), -1);
		(void)chgproccnt(r, 1);
		kauth_cred_setuid(ncred, r);
	}
	if (sv != -1)
		kauth_cred_setsvuid(ncred, sv);
	if (e != -1)
		kauth_cred_seteuid(ncred, e);

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, cred, true);

	return 0;
}

/*
 * Set real, effective and saved gids to the requested values.
 * non-root callers can only ever change gids to values that match
 * one of the processes current gid values.
 * This is further restricted by the flags argument.
 */

int
do_setresgid(struct lwp *l, gid_t r, gid_t e, gid_t sv, u_int flags)
{
	struct proc *p = l->l_proc;
	kauth_cred_t cred, ncred;

	ncred = kauth_cred_alloc();

	/* Get a write lock on the process credential. */
	proc_crmod_enter();
	cred = p->p_cred;

	/*
	 * check new value is one of the allowed existing values.
	 * otherwise, check if we have root privilege.
	 */
	if ((r != -1
	    && !((flags & ID_R_EQ_R) && r == kauth_cred_getgid(cred))
	    && !((flags & ID_R_EQ_E) && r == kauth_cred_getegid(cred))
	    && !((flags & ID_R_EQ_S) && r == kauth_cred_getsvgid(cred))) ||
	    (e != -1
	    && !((flags & ID_E_EQ_R) && e == kauth_cred_getgid(cred))
	    && !((flags & ID_E_EQ_E) && e == kauth_cred_getegid(cred))
	    && !((flags & ID_E_EQ_S) && e == kauth_cred_getsvgid(cred))) ||
	    (sv != -1
	    && !((flags & ID_S_EQ_R) && sv == kauth_cred_getgid(cred))
	    && !((flags & ID_S_EQ_E) && sv == kauth_cred_getegid(cred))
	    && !((flags & ID_S_EQ_S) && sv == kauth_cred_getsvgid(cred)))) {
		int error;

		error = kauth_authorize_process(cred, KAUTH_PROCESS_SETID,
		    p, NULL, NULL, NULL);
		if (error != 0) {
		 	proc_crmod_leave(cred, ncred, false);
			return error;
		}
	}

	/* If nothing has changed, short circuit the request */
	if ((r == -1 || r == kauth_cred_getgid(cred))
	    && (e == -1 || e == kauth_cred_getegid(cred))
	    && (sv == -1 || sv == kauth_cred_getsvgid(cred))) {
	 	proc_crmod_leave(cred, ncred, false);
		return 0;
	}

	kauth_cred_clone(cred, ncred);

	if (r != -1)
		kauth_cred_setgid(ncred, r);
	if (sv != -1)
		kauth_cred_setsvgid(ncred, sv);
	if (e != -1)
		kauth_cred_setegid(ncred, e);

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, cred, true);

	return 0;
}

/* ARGSUSED */
int
sys_setuid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setuid_args /* {
		syscallarg(uid_t) uid;
	} */ *uap = v;
	uid_t uid = SCARG(uap, uid);

	return do_setresuid(l, uid, uid, uid,
			    ID_R_EQ_R | ID_E_EQ_R | ID_S_EQ_R);
}

/* ARGSUSED */
int
sys_seteuid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_seteuid_args /* {
		syscallarg(uid_t) euid;
	} */ *uap = v;

	return do_setresuid(l, -1, SCARG(uap, euid), -1, ID_E_EQ_R | ID_E_EQ_S);
}

int
sys_setreuid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setreuid_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
	} */ *uap = v;
	kauth_cred_t cred = l->l_cred;
	uid_t ruid, euid, svuid;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);

	if (ruid == -1)
		ruid = kauth_cred_getuid(cred);
	if (euid == -1)
		euid = kauth_cred_geteuid(cred);

	/* Saved uid is set to the new euid if the ruid changed */
	svuid = (ruid == kauth_cred_getuid(cred)) ? -1 : euid;

	return do_setresuid(l, ruid, euid, svuid,
			    ID_R_EQ_R | ID_R_EQ_E |
			    ID_E_EQ_R | ID_E_EQ_E | ID_E_EQ_S |
			    ID_S_EQ_R | ID_S_EQ_E | ID_S_EQ_S);
}

/* ARGSUSED */
int
sys_setgid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setgid_args /* {
		syscallarg(gid_t) gid;
	} */ *uap = v;
	gid_t gid = SCARG(uap, gid);

	return do_setresgid(l, gid, gid, gid,
			    ID_R_EQ_R | ID_E_EQ_R | ID_S_EQ_R);
}

/* ARGSUSED */
int
sys_setegid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setegid_args /* {
		syscallarg(gid_t) egid;
	} */ *uap = v;

	return do_setresgid(l, -1, SCARG(uap, egid), -1, ID_E_EQ_R | ID_E_EQ_S);
}

int
sys_setregid(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setregid_args /* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
	} */ *uap = v;
	kauth_cred_t cred = l->l_cred;
	gid_t rgid, egid, svgid;

	rgid = SCARG(uap, rgid);
	egid = SCARG(uap, egid);

	if (rgid == -1)
		rgid = kauth_cred_getgid(cred);
	if (egid == -1)
		egid = kauth_cred_getegid(cred);

	/* Saved gid is set to the new egid if the rgid changed */
	svgid = rgid == kauth_cred_getgid(cred) ? -1 : egid;

	return do_setresgid(l, rgid, egid, svgid,
			ID_R_EQ_R | ID_R_EQ_E |
			ID_E_EQ_R | ID_E_EQ_E | ID_E_EQ_S |
			ID_S_EQ_R | ID_S_EQ_E | ID_S_EQ_S);
}

int
sys_issetugid(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use PK_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	*retval = (p->p_flag & PK_SUGID) != 0;
	return (0);
}

/*
 * sort -u for groups.
 */
static int
grsortu(gid_t *grp, int ngrp)
{
	const gid_t *src, *end;
	gid_t *dst;
	gid_t group;
	int i, j;

	/* bubble sort */
	for (i = 0; i < ngrp; i++)
		for (j = i + 1; j < ngrp; j++)
			if (grp[i] > grp[j]) {
				gid_t tmp = grp[i];
				grp[i] = grp[j];
				grp[j] = tmp;
			}

	/* uniq */
	end = grp + ngrp;
	src = grp;
	dst = grp;
	while (src < end) {
		group = *src++;
		while (src < end && *src == group)
			src++;
		*dst++ = group;
	}

#ifdef DIAGNOSTIC
	/* zero out the rest of the array */
	(void)memset(dst, 0, sizeof(*grp) * (end - dst));
#endif

	return dst - grp;
}

/* ARGSUSED */
int
sys_setgroups(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(const gid_t *) gidset;
	} */ *uap = v;
	kauth_cred_t cred, ncred;
	struct proc *p = l->l_proc;
	int ngrp;
	int error;
	gid_t grp[NGROUPS];
	size_t grsize;

	ngrp = SCARG(uap, gidsetsize);
	if ((u_int)ngrp > NGROUPS)
		return EINVAL;

	grsize = ngrp * sizeof(gid_t);
	error = copyin(SCARG(uap, gidset), grp, grsize);
	if (error)
		return error;

	ncred = kauth_cred_alloc();
	proc_crmod_enter();
	cred = p->p_cred;

	if ((error = kauth_authorize_process(cred, KAUTH_PROCESS_SETID,
	    p, NULL, NULL, NULL)) != 0)
		goto bad;

	ngrp = grsortu(grp, ngrp);
	kauth_cred_clone(cred, ncred);
	kauth_cred_setgroups(ncred, grp, ngrp, -1);

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, cred, true);

	return (0);
  bad:
  	proc_crmod_leave(cred, ncred, false); 
	return (error);
}

/*
 * Get login name, if available.
 */
/* ARGSUSED */
int
sys___getlogin(struct lwp *l, void *v, register_t *retval)
{
	struct sys___getlogin_args /* {
		syscallarg(char *) namebuf;
		syscallarg(size_t) namelen;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	char login[sizeof(p->p_session->s_login)];
	int namelen = SCARG(uap, namelen);

	if (namelen > sizeof(login))
		namelen = sizeof(login);
	rw_enter(&proclist_lock, RW_READER);
	memcpy(login, p->p_session->s_login, namelen);
	rw_exit(&proclist_lock);
	return (copyout(login, (void *)SCARG(uap, namebuf), namelen));
}

/*
 * Set login name.
 */
/* ARGSUSED */
int
sys___setlogin(struct lwp *l, void *v, register_t *retval)
{
	struct sys___setlogin_args /* {
		syscallarg(const char *) namebuf;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct session *sp;
	char newname[sizeof sp->s_login + 1];
	int error;

	if ((error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_SETID,
	    p, NULL, NULL, NULL)) != 0)
		return (error);
	error = copyinstr(SCARG(uap, namebuf), &newname, sizeof newname, NULL);
	if (error != 0)
		return (error == ENAMETOOLONG ? EINVAL : error);

	rw_enter(&proclist_lock, RW_WRITER);
	sp = p->p_session;
	if (sp->s_flags & S_LOGIN_SET && p->p_pid != sp->s_sid &&
	    strncmp(newname, sp->s_login, sizeof sp->s_login) != 0)
		log(LOG_WARNING, "%s (pid %d) changing logname from "
		    "%.*s to %s\n", p->p_comm, p->p_pid,
		    (int)sizeof sp->s_login, sp->s_login, newname);
	sp->s_flags |= S_LOGIN_SET;
	strncpy(sp->s_login, newname, sizeof sp->s_login);
	rw_exit(&proclist_lock);
	return (0);
}

