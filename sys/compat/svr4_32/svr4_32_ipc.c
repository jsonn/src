/*	$NetBSD: svr4_32_ipc.c,v 1.6.2.4 2005/03/04 16:40:42 skrll Exp $	*/

/*-
 * Copyright (c) 1995 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: svr4_32_ipc.c,v 1.6.2.4 2005/03/04 16:40:42 skrll Exp $");

#if defined(_KERNEL_OPT)
#include "opt_sysv.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/stat.h>

#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

#include <compat/svr4_32/svr4_32_types.h>
#include <compat/svr4_32/svr4_32_signal.h>
#include <compat/svr4_32/svr4_32_lwp.h>
#include <compat/svr4_32/svr4_32_ucontext.h>
#include <compat/svr4_32/svr4_32_syscallargs.h>
#include <compat/svr4_32/svr4_32_util.h>
#include <compat/svr4_32/svr4_32_ipc.h>

#if defined(SYSVMSG) || defined(SYSVSHM) || defined(SYSVSEM)
static void svr4_32_to_bsd_ipc_perm __P((const struct svr4_32_ipc_perm *,
				      struct ipc_perm *));
static void bsd_to_svr4_32_ipc_perm __P((const struct ipc_perm *,
				      struct svr4_32_ipc_perm *));
#endif

#ifdef SYSVSEM
static void bsd_to_svr4_32_semid_ds __P((const struct semid_ds *,
				      struct svr4_32_semid_ds *));
static void svr4_32_to_bsd_semid_ds __P((const struct svr4_32_semid_ds *,
				      struct semid_ds *));
static int svr4_32_semop __P((struct lwp *, void *, register_t *));
static int svr4_32_semget __P((struct lwp *, void *, register_t *));
static int svr4_32_semctl __P((struct lwp *, void *, register_t *));
#endif

#ifdef SYSVMSG
static void bsd_to_svr4_32_msqid_ds __P((const struct msqid_ds *,
				      struct svr4_32_msqid_ds *));
static void svr4_32_to_bsd_msqid_ds __P((const struct svr4_32_msqid_ds *,
				      struct msqid_ds *));
static int svr4_32_msgsnd __P((struct lwp *, void *, register_t *));
static int svr4_32_msgrcv __P((struct lwp *, void *, register_t *));
static int svr4_32_msgget __P((struct lwp *, void *, register_t *));
static int svr4_32_msgctl __P((struct lwp *, void *, register_t *));
#endif

#ifdef SYSVSHM
static void bsd_to_svr4_32_shmid_ds __P((const struct shmid_ds *,
				      struct svr4_32_shmid_ds *));
static void svr4_32_to_bsd_shmid_ds __P((const struct svr4_32_shmid_ds *,
				      struct shmid_ds *));
static int svr4_32_shmat __P((struct lwp *, void *, register_t *));
static int svr4_32_shmdt __P((struct lwp *, void *, register_t *));
static int svr4_32_shmget __P((struct lwp *, void *, register_t *));
static int svr4_32_shmctl __P((struct lwp *, void *, register_t *));
#endif

#if defined(SYSVMSG) || defined(SYSVSHM) || defined(SYSVSEM)

static void
svr4_32_to_bsd_ipc_perm(spp, bpp)
	const struct svr4_32_ipc_perm *spp;
	struct ipc_perm *bpp;
{
	bpp->_key = spp->key;
	bpp->uid = spp->uid;
	bpp->gid = spp->gid;
	bpp->cuid = spp->cuid;
	bpp->cgid = spp->cgid;
	bpp->mode = spp->mode;
	bpp->_seq = spp->seq;
}

static void
bsd_to_svr4_32_ipc_perm(bpp, spp)
	const struct ipc_perm *bpp;
	struct svr4_32_ipc_perm *spp;
{
	spp->key = bpp->_key;
	spp->uid = bpp->uid;
	spp->gid = bpp->gid;
	spp->cuid = bpp->cuid;
	spp->cgid = bpp->cgid;
	spp->mode = bpp->mode;
	spp->seq = bpp->_seq;
}
#endif

#ifdef SYSVSEM
static void
bsd_to_svr4_32_semid_ds(bds, sds)
	const struct semid_ds *bds;
	struct svr4_32_semid_ds *sds;
{
	bsd_to_svr4_32_ipc_perm(&bds->sem_perm, &sds->sem_perm);
	sds->sem_base = (svr4_32_semp)(u_long)bds->_sem_base;
	sds->sem_nsems = bds->sem_nsems;
	sds->sem_otime = bds->sem_otime;
	sds->sem_ctime = bds->sem_ctime;
}

static void
svr4_32_to_bsd_semid_ds(sds, bds)
	const struct svr4_32_semid_ds *sds;
	struct semid_ds *bds;
{
	svr4_32_to_bsd_ipc_perm(&sds->sem_perm, &bds->sem_perm);
	bds->_sem_base = (struct __sem *)(u_long)sds->sem_base;
	bds->sem_nsems = sds->sem_nsems;
	bds->sem_otime = sds->sem_otime;
	bds->sem_ctime = sds->sem_ctime;
}

struct svr4_32_sys_semctl_args {
	syscallarg(int) what;
	syscallarg(int) semid;
	syscallarg(int) semnum;
	syscallarg(int) cmd;
	syscallarg(union netbsd32_semun) arg;
};

static int
svr4_32_semctl(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_semctl_args *uap = v;
	struct proc *p = l->l_proc;
	struct semid_ds sembuf;
	struct svr4_32_semid_ds ssembuf;
	int cmd, error;
	void *pass_arg = NULL;

	cmd = SCARG(uap, cmd);

	switch (cmd) {
	case SVR4_IPC_SET:
		pass_arg = &sembuf;
		cmd = IPC_SET;
		break;

	case SVR4_IPC_STAT:
		pass_arg = &sembuf;
		cmd = IPC_STAT;
		break;

	case SVR4_IPC_RMID:
		cmd = IPC_RMID;
		break;

	case SVR4_SEM_GETVAL:
		cmd = GETVAL;
		break;

	case SVR4_SEM_GETPID:
		cmd = GETPID;
		break;

	case SVR4_SEM_GETNCNT:
		cmd = GETNCNT;
		break;

	case SVR4_SEM_GETZCNT:
		cmd = GETZCNT;
		break;

	case SVR4_SEM_GETALL:
		pass_arg = &SCARG(uap, arg);
		cmd = GETALL;
		break;

	case SVR4_SEM_SETVAL:
		pass_arg = &SCARG(uap, arg);
		cmd = SETVAL;
		break;

	case SVR4_SEM_SETALL:
		pass_arg = &SCARG(uap, arg);
		cmd = SETALL;
		break;

	default:
		return (EINVAL);
	}

	if (cmd == IPC_SET) {
		error = copyin((caddr_t)(u_long)SCARG(uap, arg).buf,
			       &ssembuf, sizeof(ssembuf));
		if (error)
			return (error);
		svr4_32_to_bsd_semid_ds(&ssembuf, &sembuf);
	}

	error = semctl1(p, SCARG(uap, semid), SCARG(uap, semnum), cmd,
	    pass_arg, retval);

	if (error == 0 && cmd == IPC_STAT) {
		bsd_to_svr4_32_semid_ds(&sembuf, &ssembuf);
		error = copyout(&ssembuf, (caddr_t)(u_long)SCARG(uap, arg).buf,
				sizeof(ssembuf));
	}

	return (error);
}

struct svr4_32_sys_semget_args {
	syscallarg(int) what;
	syscallarg(svr4_key_t) key;
	syscallarg(int) nsems;
	syscallarg(int) semflg;
};

static int
svr4_32_semget(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_semget_args *uap = v;
	struct sys_semget_args ap;

	SCARG(&ap, key) = SCARG(uap, key);
	SCARG(&ap, nsems) = SCARG(uap, nsems);
	SCARG(&ap, semflg) = SCARG(uap, semflg);

	return sys_semget(l, &ap, retval);
}

struct svr4_32_sys_semop_args {
	syscallarg(int) what;
	syscallarg(int) semid;
	syscallarg(svr4_32_sembufp) sops;
	syscallarg(u_int) nsops;
};

static int
svr4_32_semop(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_semop_args *uap = v;
	struct sys_semop_args ap;

	SCARG(&ap, semid) = SCARG(uap, semid);
	/* These are the same */
	SCARG(&ap, sops) = (struct sembuf *)(u_long)SCARG(uap, sops);
	SCARG(&ap, nsops) = SCARG(uap, nsops);

	return sys_semop(l, &ap, retval);
}

int
svr4_32_sys_semsys(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_semsys_args *uap = v;

	DPRINTF(("svr4_32_semsys(%d)\n", SCARG(uap, what)));

	switch (SCARG(uap, what)) {
	case SVR4_semctl:
		return svr4_32_semctl(l, v, retval);
	case SVR4_semget:
		return svr4_32_semget(l, v, retval);
	case SVR4_semop:
		return svr4_32_semop(l, v, retval);
	default:
		return EINVAL;
	}
}
#endif

#ifdef SYSVMSG
static void
bsd_to_svr4_32_msqid_ds(bds, sds)
	const struct msqid_ds *bds;
	struct svr4_32_msqid_ds *sds;
{
	bsd_to_svr4_32_ipc_perm(&bds->msg_perm, &sds->msg_perm);
	sds->msg_first = (svr4_32_msgp)(u_long)bds->_msg_first;
	sds->msg_last = (svr4_32_msgp)(u_long)bds->_msg_last;
	sds->msg_cbytes = bds->_msg_cbytes;
	sds->msg_qnum = bds->msg_qnum;
	sds->msg_qbytes = bds->msg_qbytes;
	sds->msg_lspid = bds->msg_lspid;
	sds->msg_lrpid = bds->msg_lrpid;
	sds->msg_stime = bds->msg_stime;
	sds->msg_rtime = bds->msg_rtime;
	sds->msg_ctime = bds->msg_ctime;

#if 0
	/* XXX What to put here? */
	sds->msg_cv = 0;
	sds->msg_qnum_cv = 0;
#endif
}

static void
svr4_32_to_bsd_msqid_ds(sds, bds)
	const struct svr4_32_msqid_ds *sds;
	struct msqid_ds *bds;
{
	svr4_32_to_bsd_ipc_perm(&sds->msg_perm, &bds->msg_perm);
	bds->_msg_first = (struct __msg *)(u_long)sds->msg_first;
	bds->_msg_last = (struct __msg *)(u_long)sds->msg_last;
	bds->_msg_cbytes = sds->msg_cbytes;
	bds->msg_qnum = sds->msg_qnum;
	bds->msg_qbytes = sds->msg_qbytes;
	bds->msg_lspid = sds->msg_lspid;
	bds->msg_lrpid = sds->msg_lrpid;
	bds->msg_stime = sds->msg_stime;
	bds->msg_rtime = sds->msg_rtime;
	bds->msg_ctime = sds->msg_ctime;

#if 0
	XXX sds->msg_cv
	XXX sds->msg_qnum_cv
#endif
}

struct svr4_32_sys_msgsnd_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(netbsd32_voidp) msgp;
	syscallarg(netbsd32_size_t) msgsz;
	syscallarg(int) msgflg;
};

static int
svr4_32_msgsnd(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_msgsnd_args *uap = v;
	struct sys_msgsnd_args ap;

	SCARG(&ap, msqid) = SCARG(uap, msqid);
	SCARG(&ap, msgp) = (void *)(u_long)SCARG(uap, msgp);
	SCARG(&ap, msgsz) = SCARG(uap, msgsz);
	SCARG(&ap, msgflg) = SCARG(uap, msgflg);

	return sys_msgsnd(l, &ap, retval);
}

struct svr4_32_sys_msgrcv_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(netbsd32_voidp) msgp;
	syscallarg(netbsd32_size_t) msgsz;
	syscallarg(netbsd32_long) msgtyp;
	syscallarg(int) msgflg;
};

static int
svr4_32_msgrcv(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_msgrcv_args *uap = v;
	struct sys_msgrcv_args ap;

	SCARG(&ap, msqid) = SCARG(uap, msqid);
	SCARG(&ap, msgp) = (void *)(u_long)SCARG(uap, msgp);
	SCARG(&ap, msgsz) = SCARG(uap, msgsz);
	SCARG(&ap, msgtyp) = SCARG(uap, msgtyp);
	SCARG(&ap, msgflg) = SCARG(uap, msgflg);

	return sys_msgrcv(l, &ap, retval);
}

struct svr4_32_sys_msgget_args {
	syscallarg(int) what;
	syscallarg(svr4_key_t) key;
	syscallarg(int) msgflg;
};

static int
svr4_32_msgget(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_msgget_args *uap = v;
	struct sys_msgget_args ap;

	SCARG(&ap, key) = SCARG(uap, key);
	SCARG(&ap, msgflg) = SCARG(uap, msgflg);

	return sys_msgget(l, &ap, retval);
}

struct svr4_32_sys_msgctl_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(int) cmd;
	syscallarg(svr4_32_msqid_dsp) buf;
};

static int
svr4_32_msgctl(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	int error;
	struct svr4_32_sys_msgctl_args *uap = v;
	struct proc *p = l->l_proc;
	struct sys___msgctl13_args ap;
	struct svr4_32_msqid_ds ss;
	struct msqid_ds bs;
	caddr_t sg = stackgap_init(p, 0);

	SCARG(&ap, msqid) = SCARG(uap, msqid);
	SCARG(&ap, cmd) = SCARG(uap, cmd);
	SCARG(&ap, buf) = stackgap_alloc(p, &sg, sizeof(bs));

	switch (SCARG(uap, cmd)) {
	case SVR4_IPC_STAT:
		SCARG(&ap, cmd) = IPC_STAT;
		if ((error = sys___msgctl13(l, &ap, retval)) != 0)
			return error;
		error = copyin(&bs, SCARG(&ap, buf), sizeof bs);
		if (error)
			return error;
		bsd_to_svr4_32_msqid_ds(&bs, &ss);
		return copyout(&ss, (caddr_t)(u_long)SCARG(uap, buf), sizeof ss);

	case SVR4_IPC_SET:
		SCARG(&ap, cmd) = IPC_SET;
		error = copyin((caddr_t)(u_long)SCARG(uap, buf), &ss, sizeof ss);
		if (error)
			return error;
		svr4_32_to_bsd_msqid_ds(&ss, &bs);
		error = copyout(&bs, SCARG(&ap, buf), sizeof bs);
		if (error)
			return error;
		return sys___msgctl13(l, &ap, retval);

	case SVR4_IPC_RMID:
		SCARG(&ap, cmd) = IPC_RMID;
		error = copyin((caddr_t)(u_long)SCARG(uap, buf), &ss, sizeof ss);
		if (error)
			return error;
		svr4_32_to_bsd_msqid_ds(&ss, &bs);
		error = copyout(&bs, SCARG(&ap, buf), sizeof bs);
		if (error)
			return error;
		return sys___msgctl13(l, &ap, retval);

	default:
		return EINVAL;
	}
}

int
svr4_32_sys_msgsys(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_msgsys_args *uap = v;

	DPRINTF(("svr4_32_msgsys(%d)\n", SCARG(uap, what)));

	switch (SCARG(uap, what)) {
	case SVR4_msgsnd:
		return svr4_32_msgsnd(l, v, retval);
	case SVR4_msgrcv:
		return svr4_32_msgrcv(l, v, retval);
	case SVR4_msgget:
		return svr4_32_msgget(l, v, retval);
	case SVR4_msgctl:
		return svr4_32_msgctl(l, v, retval);
	default:
		return EINVAL;
	}
}
#endif

#ifdef SYSVSHM

static void
bsd_to_svr4_32_shmid_ds(bds, sds)
	const struct shmid_ds *bds;
	struct svr4_32_shmid_ds *sds;
{
	bsd_to_svr4_32_ipc_perm(&bds->shm_perm, &sds->shm_perm);
	sds->shm_segsz = bds->shm_segsz;
	sds->shm_lkcnt = 0;
	sds->shm_lpid = bds->shm_lpid;
	sds->shm_cpid = bds->shm_cpid;
	sds->shm_amp = (netbsd32_caddr_t)(u_long)bds->_shm_internal;
	sds->shm_nattch = bds->shm_nattch;
	sds->shm_cnattch = 0;
	sds->shm_atime = bds->shm_atime;
	sds->shm_pad1 = 0;
	sds->shm_dtime = bds->shm_dtime;
	sds->shm_pad2 = 0;
	sds->shm_ctime = bds->shm_ctime;
	sds->shm_pad3 = 0;
}

static void
svr4_32_to_bsd_shmid_ds(sds, bds)
	const struct svr4_32_shmid_ds *sds;
	struct shmid_ds *bds;
{
	svr4_32_to_bsd_ipc_perm(&sds->shm_perm, &bds->shm_perm);
	bds->shm_segsz = sds->shm_segsz;
	bds->shm_lpid = sds->shm_lpid;
	bds->shm_cpid = sds->shm_cpid;
	bds->_shm_internal = (void *)(u_long)sds->shm_amp;
	bds->shm_nattch = sds->shm_nattch;
	bds->shm_atime = sds->shm_atime;
	bds->shm_dtime = sds->shm_dtime;
	bds->shm_ctime = sds->shm_ctime;
}

struct svr4_32_sys_shmat_args {
	syscallarg(int) what;
	syscallarg(int) shmid;
	syscallarg(netbsd32_voidp) shmaddr;
	syscallarg(int) shmflg;
};

static int
svr4_32_shmat(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_shmat_args *uap = v;
	struct sys_shmat_args ap;

	SCARG(&ap, shmid) = SCARG(uap, shmid);
	SCARG(&ap, shmaddr) = (void *)(u_long)SCARG(uap, shmaddr);
	SCARG(&ap, shmflg) = SCARG(uap, shmflg);

	return sys_shmat(l, &ap, retval);
}

struct svr4_32_sys_shmdt_args {
	syscallarg(int) what;
	syscallarg(netbsd32_voidp) shmaddr;
};

static int
svr4_32_shmdt(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_shmdt_args *uap = v;
	struct sys_shmdt_args ap;

	SCARG(&ap, shmaddr) = (void *)(u_long)SCARG(uap, shmaddr);

	return sys_shmdt(l, &ap, retval);
}

struct svr4_32_sys_shmget_args {
	syscallarg(int) what;
	syscallarg(netbsd32_key_t) key;
	syscallarg(int) size;
	syscallarg(int) shmflg;
};

static int
svr4_32_shmget(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_shmget_args *uap = v;
	struct sys_shmget_args ap;

	SCARG(&ap, key) = SCARG(uap, key);
	SCARG(&ap, size) = SCARG(uap, size);
	SCARG(&ap, shmflg) = SCARG(uap, shmflg);

	return sys_shmget(l, &ap, retval);
}

struct svr4_32_sys_shmctl_args {
	syscallarg(int) what;
	syscallarg(int) shmid;
	syscallarg(int) cmd;
	syscallarg(svr4_32_shmid_dsp) buf;
};

int
svr4_32_shmctl(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_shmctl_args *uap = v;
	int error;
	struct proc *p = l->l_proc;
	caddr_t sg = stackgap_init(p, 0);
	struct sys___shmctl13_args ap;
	struct shmid_ds bs;
	struct svr4_32_shmid_ds ss;

	SCARG(&ap, shmid) = SCARG(uap, shmid);

	if (SCARG(uap, buf)) {
		SCARG(&ap, buf) = stackgap_alloc(p, &sg, sizeof (struct shmid_ds));
		switch (SCARG(uap, cmd)) {
		case SVR4_IPC_SET:
		case SVR4_IPC_RMID:
		case SVR4_SHM_LOCK:
		case SVR4_SHM_UNLOCK:
			error = copyin((caddr_t)(u_long)SCARG(uap, buf),
				       (caddr_t)&ss, sizeof ss);
			if (error)
				return error;
			svr4_32_to_bsd_shmid_ds(&ss, &bs);
			error = copyout(&bs, SCARG(&ap, buf), sizeof bs);
			if (error)
				return error;
			break;
		default:
			break;
		}
	}
	else
		SCARG(&ap, buf) = 0;


	switch (SCARG(uap, cmd)) {
	case SVR4_IPC_STAT:
		SCARG(&ap, cmd) = IPC_STAT;
		if ((error = sys___shmctl13(l, &ap, retval)) != 0)
			return error;
		if (!SCARG(uap, buf))
			return 0;
		error = copyin(&bs, SCARG(&ap, buf), sizeof bs);
		if (error)
			return error;
		bsd_to_svr4_32_shmid_ds(&bs, &ss);
		return copyout(&ss, (caddr_t)(u_long)SCARG(uap, buf), sizeof ss);

	case SVR4_IPC_SET:
		SCARG(&ap, cmd) = IPC_SET;
		return sys___shmctl13(l, &ap, retval);

	case SVR4_IPC_RMID:
	case SVR4_SHM_LOCK:
	case SVR4_SHM_UNLOCK:
		switch (SCARG(uap, cmd)) {
		case SVR4_IPC_RMID:
			SCARG(&ap, cmd) = IPC_RMID;
			break;
		case SVR4_SHM_LOCK:
			SCARG(&ap, cmd) = SHM_LOCK;
			break;
		case SVR4_SHM_UNLOCK:
			SCARG(&ap, cmd) = SHM_UNLOCK;
			break;
		default:
			return EINVAL;
		}
		return sys___shmctl13(l, &ap, retval);

	default:
		return EINVAL;
	}
}

int
svr4_32_sys_shmsys(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct svr4_32_sys_shmsys_args *uap = v;

	DPRINTF(("svr4_32_shmsys(%d)\n", SCARG(uap, what)));

	switch (SCARG(uap, what)) {
	case SVR4_shmat:
		return svr4_32_shmat(l, v, retval);
	case SVR4_shmdt:
		return svr4_32_shmdt(l, v, retval);
	case SVR4_shmget:
		return svr4_32_shmget(l, v, retval);
	case SVR4_shmctl:
		return svr4_32_shmctl(l, v, retval);
	default:
		return ENOSYS;
	}
}
#endif /* SYSVSHM */
