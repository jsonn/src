/*	$NetBSD: ultrix_misc.c,v 1.35.4.2 1997/10/14 10:21:55 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1997 Jonathan Stone (hereinafter referred to as the author)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
 *
 *	@(#)sun_misc.c	8.1 (Berkeley) 6/18/93
 *
 * from: Header: sun_misc.c,v 1.16 93/04/07 02:46:27 torek Exp 
 */

/*
 * SunOS compatibility module.
 *
 * SunOS system calls that are implemented differently in BSD are
 * handled here.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
/*#include <sys/stat.h>*/
/*#include <sys/ioctl.h>*/
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/ipc.h>

#include <sys/syscallargs.h>

#include <compat/ultrix/ultrix_syscall.h>
#include <compat/ultrix/ultrix_syscallargs.h>
#include <compat/common/compat_util.h>

#include <netinet/in.h>

#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <vm/vm.h>					/* pmap declarations */

#include <sys/conf.h>					/* iszerodev() */
#include <sys/socketvar.h>				/* sosetopt() */


extern struct sysent ultrix_sysent[];
extern char *ultrix_syscallnames[];


/*
 * Select the appropriate setregs callback for the target architecture.
 */
#ifdef mips
#include <machine/ecoff_machdep.h>
#define ULTRIX_EXEC_SETREGS cpu_exec_ecoff_setregs
#endif /* mips */

#ifdef vax
#define ULTRIX_EXEC_SETREGS setregs
#endif /* mips */


extern void ULTRIX_EXEC_SETREGS __P((struct proc *, struct exec_package *,
					u_long));
extern char sigcode[], esigcode[];

struct emul emul_ultrix = {
	"ultrix",
	NULL,
	sendsig,
	ULTRIX_SYS_syscall,
	ULTRIX_SYS_MAXSYSCALL,
	ultrix_sysent,
	ultrix_syscallnames,
	0,
	copyargs,
	ULTRIX_EXEC_SETREGS,
	sigcode,
	esigcode,
};

#define GSI_PROG_ENV 1

int
ultrix_sys_getsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_getsysinfo_args *uap = v;
	static short progenv = 0;

	switch (SCARG(uap, op)) {
		/* operations implemented: */
	case GSI_PROG_ENV:
		if (SCARG(uap, nbytes) < sizeof(short))
			return EINVAL;
		*retval = 1;
		return (copyout(&progenv, SCARG(uap, buffer), sizeof(short)));
	default:
		*retval = 0; /* info unavail */
		return 0;
	}
}

int
ultrix_sys_setsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#ifdef notyet
	struct ultrix_sys_setsysinfo_args *uap = v;
#endif

	*retval = 0;
	return 0;
}

int
ultrix_sys_waitpid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_waitpid_args *uap = v;
	struct sys_wait4_args ua;

	SCARG(&ua, pid) = SCARG(uap, pid);
	SCARG(&ua, status) = SCARG(uap, status);
	SCARG(&ua, options) = SCARG(uap, options);
	SCARG(&ua, rusage) = 0;

	return (sys_wait4(p, &ua, retval));
}

int
ultrix_sys_wait3(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_wait3_args *uap = v;
	struct sys_wait4_args ua;

	SCARG(&ua, pid) = -1;
	SCARG(&ua, status) = SCARG(uap, status);
	SCARG(&ua, options) = SCARG(uap, options);
	SCARG(&ua, rusage) = SCARG(uap, rusage);

	return (sys_wait4(p, &ua, retval));
}

/*
 * Ultrix binaries pass in FD_MAX as the first arg to select().
 * On Ultrix, FD_MAX is 4096, which is more than the NetBSD sys_select()
 * can handle.
 * Since we can't have more than the (native) FD_MAX descriptors open, 
 * limit nfds to at most FD_MAX.
 */
int
ultrix_sys_select(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_select_args *uap = v;
	struct timeval atv;
	int error;

	/* Limit number of FDs selected on to the native maximum */

	if (SCARG(uap, nd) > FD_SETSIZE)
		SCARG(uap, nd) = FD_SETSIZE;

	/* Check for negative timeval */
	if (SCARG(uap, tv)) {
		error = copyin((caddr_t)SCARG(uap, tv), (caddr_t)&atv,
			       sizeof(atv));
		if (error)
			goto done;
#ifdef DEBUG
		/* Ultrix clients sometimes give negative timeouts? */
		if (atv.tv_sec < 0 || atv.tv_usec < 0)
			printf("ultrix select( %ld, %ld): negative timeout\n",
			    atv.tv_sec, atv.tv_usec);
		/*tvp = (timeval *)STACKGAPBASE;*/
#endif

	}
	error = sys_select(p, (void*) uap, retval);
	if (error == EINVAL)
		printf("ultrix select: bad args?\n");

done:
	return error;
}

#if defined(NFS)
int
async_daemon(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_nfssvc_args ouap;

	SCARG(&ouap, flag) = NFSSVC_BIOD;
	SCARG(&ouap, argp) = NULL;

	return (sys_nfssvc(p, &ouap, retval));
}
#endif /* NFS */


#define	SUN__MAP_NEW	0x80000000	/* if not, old mmap & cannot handle */

int
ultrix_sys_mmap(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_mmap_args *uap = v;
	struct sys_mmap_args ouap;
	register struct filedesc *fdp;
	register struct file *fp;
	register struct vnode *vp;

	/*
	 * Verify the arguments.
	 */
	if (SCARG(uap, prot) & ~(PROT_READ|PROT_WRITE|PROT_EXEC))
		return (EINVAL);			/* XXX still needed? */

	if ((SCARG(uap, flags) & SUN__MAP_NEW) == 0)
		return (EINVAL);

	SCARG(&ouap, flags) = SCARG(uap, flags) & ~SUN__MAP_NEW;
	SCARG(&ouap, addr) = SCARG(uap, addr);

	if ((SCARG(&ouap, flags) & MAP_FIXED) == 0 &&
	    SCARG(&ouap, addr) != 0 &&
	    SCARG(&ouap, addr) < (caddr_t)round_page(p->p_vmspace->vm_daddr+MAXDSIZ))
		SCARG(&ouap, addr) = (caddr_t)round_page(p->p_vmspace->vm_daddr+MAXDSIZ);

	SCARG(&ouap, len) = SCARG(uap, len);
	SCARG(&ouap, prot) = SCARG(uap, prot);
	SCARG(&ouap, fd) = SCARG(uap, fd);
	SCARG(&ouap, pos) = SCARG(uap, pos);

	/*
	 * Special case: if fd refers to /dev/zero, map as MAP_ANON.  (XXX)
	 */
	fdp = p->p_fd;
	if ((unsigned)SCARG(&ouap, fd) < fdp->fd_nfiles &&		/*XXX*/
	    (fp = fdp->fd_ofiles[SCARG(&ouap, fd)]) != NULL &&		/*XXX*/
	    fp->f_type == DTYPE_VNODE &&				/*XXX*/
	    (vp = (struct vnode *)fp->f_data)->v_type == VCHR &&	/*XXX*/
	    iszerodev(vp->v_rdev)) {					/*XXX*/
		SCARG(&ouap, flags) |= MAP_ANON;
		SCARG(&ouap, fd) = -1;
	}

	return (sys_mmap(p, &ouap, retval));
}

int
ultrix_sys_setsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_setsockopt_args *uap = v;
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp))  != 0)
		return (error);
#define	SO_DONTLINGER (~SO_LINGER)
	if (SCARG(uap, name) == SO_DONTLINGER) {
		m = m_get(M_WAIT, MT_SOOPTS);
		mtod(m, struct linger *)->l_onoff = 0;
		m->m_len = sizeof(struct linger);
		return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
		    SO_LINGER, m));
	}
	if (SCARG(uap, level) == IPPROTO_IP) {
#define		EMUL_IP_MULTICAST_IF		2
#define		EMUL_IP_MULTICAST_TTL		3
#define		EMUL_IP_MULTICAST_LOOP		4
#define		EMUL_IP_ADD_MEMBERSHIP		5
#define		EMUL_IP_DROP_MEMBERSHIP	6
		static int ipoptxlat[] = {
			IP_MULTICAST_IF,
			IP_MULTICAST_TTL,
			IP_MULTICAST_LOOP,
			IP_ADD_MEMBERSHIP,
			IP_DROP_MEMBERSHIP
		};
		if (SCARG(uap, name) >= EMUL_IP_MULTICAST_IF &&
		    SCARG(uap, name) <= EMUL_IP_DROP_MEMBERSHIP) {
			SCARG(uap, name) =
			    ipoptxlat[SCARG(uap, name) - EMUL_IP_MULTICAST_IF];
		}
	}
	if (SCARG(uap, valsize) > MLEN)
		return (EINVAL);
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		error = copyin(SCARG(uap, val), mtod(m, caddr_t),
		    (u_int)SCARG(uap, valsize));
		if (error) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = SCARG(uap, valsize);
	}
	return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), m));
}

struct ultrix_utsname {
	char    sysname[9];
	char    nodename[9];
	char    nodeext[65-9];
	char    release[9];
	char    version[9];
	char    machine[9];
};

int
ultrix_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_uname_args *uap = v;
	struct ultrix_utsname sut;
	extern char ostype[], machine[], osrelease[];

	bzero(&sut, sizeof(sut));

	bcopy(ostype, sut.sysname, sizeof(sut.sysname) - 1);
	bcopy(hostname, sut.nodename, sizeof(sut.nodename));
	sut.nodename[sizeof(sut.nodename)-1] = '\0';
	bcopy(osrelease, sut.release, sizeof(sut.release) - 1);
	bcopy("1", sut.version, sizeof(sut.version) - 1);
	bcopy(machine, sut.machine, sizeof(sut.machine) - 1);

	return copyout((caddr_t)&sut, (caddr_t)SCARG(uap, name),
	    sizeof(struct ultrix_utsname));
}

int
ultrix_sys_setpgrp(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_setpgrp_args *uap = v;

	/*
	 * difference to our setpgid call is to include backwards
	 * compatibility to pre-setsid() binaries. Do setsid()
	 * instead of setpgid() in those cases where the process
	 * tries to create a new session the old way.
	 */
	if (!SCARG(uap, pgid) &&
	    (!SCARG(uap, pid) || SCARG(uap, pid) == p->p_pid))
		return sys_setsid(p, uap, retval);
	else
		return sys_setpgid(p, uap, retval);
}

#if defined (NFSSERVER)
int
ultrix_sys_nfssvc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#if 0	/* XXX */
	struct ultrix_sys_nfssvc_args *uap = v;
	struct emul *e = p->p_emul;
	struct sys_nfssvc_args outuap;
	struct sockaddr sa;
	int error;

	bzero(&outuap, sizeof outuap);
	SCARG(&outuap, fd) = SCARG(uap, fd);
	SCARG(&outuap, mskval) = STACKGAPBASE;
	SCARG(&outuap, msklen) = sizeof sa;
	SCARG(&outuap, mtchval) = outuap.mskval + sizeof sa;
	SCARG(&outuap, mtchlen) = sizeof sa;

	bzero(&sa, sizeof sa);
	if (error = copyout(&sa, SCARG(&outuap, mskval), SCARG(&outuap, msklen)))
		return (error);
	if (error = copyout(&sa, SCARG(&outuap, mtchval), SCARG(&outuap, mtchlen)))
		return (error);

	return nfssvc(p, &outuap, retval);
#else
	return (ENOSYS);
#endif
}
#endif /* NFSSERVER */

struct ultrix_ustat {
	daddr_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
	char	f_fname[6];	/* filsys name */
	char	f_fpack[6];	/* filsys pack name */
};

int
ultrix_sys_ustat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_ustat_args *uap = v;
	struct ultrix_ustat us;
	int error;

	bzero(&us, sizeof us);

	/*
	 * XXX: should set f_tfree and f_tinode at least
	 * How do we translate dev -> fstat? (and then to ultrix_ustat)
	 */

	if ((error = copyout(&us, SCARG(uap, buf), sizeof us)) != 0)
		return (error);
	return 0;
}

int
ultrix_sys_quotactl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#ifdef notyet
	struct ultrix_sys_quotactl_args *uap = v;
#endif

	return EINVAL;
}

int
ultrix_sys_vhangup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return 0;
}


/*
 * RISC Ultrix cache control syscalls
 */
#ifdef __mips
int
ultrix_sys_cacheflush(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_cacheflush_args /* {
		syscallarg(void *) addr;
		syscallarg(int) nbytes;
		syscallarg(int) flag;
	} */ *uap = v;
	register vm_offset_t va  = (vm_offset_t)SCARG(uap, addr);
	register int nbytes     = SCARG(uap, nbytes);
	register int whichcache = SCARG(uap, whichcache);

	return (mips_user_cacheflush(p, va, nbytes, whichcache));
}


int
ultrix_sys_cachectl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_cachectl_args /* {
		syscallarg(void *) addr;
		syscallarg(int) nbytes;
		syscallarg(int) cacheop;
	} */ *uap = v;
	register vm_offset_t va  = (vm_offset_t)SCARG(uap, addr);
	register int nbytes  = SCARG(uap, nbytes);
	register int cacheop = SCARG(uap, cacheop);

	return mips_user_cachectl(p, va, nbytes, cacheop);
}

#endif	/* __mips */


int
ultrix_sys_exportfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifdef notyet
	struct ultrix_sys_exportfs_args *uap = v;
#endif

	/*
	 * XXX: should perhaps translate into a mount(2)
	 * with MOUNT_EXPORT?
	 */
	return 0;
}

int
ultrix_sys_sigpending(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_sigpending_args *uap = v;
	int mask = p->p_siglist & p->p_sigmask;

	return (copyout((caddr_t)&mask, (caddr_t)SCARG(uap, mask), sizeof(int)));
}

int
ultrix_sys_sigcleanup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_sigcleanup_args *uap = v;

	return sys_sigreturn(p, (struct sys_sigreturn_args *)uap, retval);
}


int
ultrix_sys_shmsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#ifdef SYSVSHM

	/* Ultrix SVSHM weirndess: */
	struct ultrix_sys_shmsys_args *uap = v;
	struct sys_shmat_args shmat_args;
	struct sys_shmctl_args shmctl_args;
	struct sys_shmdt_args shmdt_args;
	struct sys_shmget_args shmget_args;


	switch (SCARG(uap, shmop)) {
	case 0:						/* Ultrix shmat() */
		SCARG(&shmat_args, shmid) = SCARG(uap, a2);
		SCARG(&shmat_args, shmaddr) = (void *)SCARG(uap, a3);
		SCARG(&shmat_args, shmflg) = SCARG(uap, a4);
		return (sys_shmat(p, &shmat_args, retval));

	case 1:						/* Ultrix shmctl() */
		SCARG(&shmctl_args, shmid) = SCARG(uap, a2);
		SCARG(&shmctl_args, cmd) = SCARG(uap, a3);
		SCARG(&shmctl_args, buf) = (struct shmid_ds *)SCARG(uap, a4);
		return (sys_shmctl(p, &shmctl_args, retval));

	case 2:						/* Ultrix shmdt() */
		SCARG(&shmat_args, shmaddr) = (void *)SCARG(uap, a2);
		return (sys_shmdt(p, &shmdt_args, retval));

	case 3:						/* Ultrix shmget() */
		SCARG(&shmget_args, key) = SCARG(uap, a2);
		SCARG(&shmget_args, size) = SCARG(uap, a3);
		SCARG(&shmget_args, shmflg) = SCARG(uap, a4)
		    & (IPC_CREAT|IPC_EXCL|IPC_NOWAIT);
		return (sys_shmget(p, &shmget_args, retval));

	default:
		return (EINVAL);
	}
#else
	return (EOPNOTSUPP);
#endif	/* SYSVSHM */
}
