/*	$NetBSD: kern_info_43.c,v 1.20.4.2 2006/12/30 20:47:31 yamt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)subr_xxx.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_info_43.c,v 1.20.4.2 2006/12/30 20:47:31 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

int
compat_43_sys_getdtablesize(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	*retval = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	return (0);
}


/* ARGSUSED */
int
compat_43_sys_gethostid(struct lwp *l, void *v,
    register_t *retval)
{

	*(int32_t *)retval = hostid;
	return (0);
}


/*ARGSUSED*/
int
compat_43_sys_gethostname(struct lwp *l, void *v, register_t *retval)
{
	struct compat_43_sys_gethostname_args /* {
		syscallarg(char *) hostname;
		syscallarg(u_int) len;
	} */ *uap = v;
	int name[2];
	size_t sz;

	name[0] = CTL_KERN;
	name[1] = KERN_HOSTNAME;
	sz = SCARG(uap, len);
	return (old_sysctl(&name[0], 2, SCARG(uap, hostname), &sz, 0, 0, l));
}

#define	KINFO_PROC		(0<<8)
#define	KINFO_RT		(1<<8)
#define	KINFO_VNODE		(2<<8)
#define	KINFO_FILE		(3<<8)
#define	KINFO_METER		(4<<8)
#define	KINFO_LOADAVG		(5<<8)
#define	KINFO_CLOCKRATE		(6<<8)
#define	KINFO_BSDI_SYSINFO	(101<<8)


/*
 * The string data is appended to the end of the bsdi_si structure during
 * copyout. The "char *" offsets in the bsdi_si struct are relative to the
 * base of the bsdi_si struct.
 */
struct bsdi_si {
        char    *machine;
        char    *cpu_model;
        long    ncpu;
        long    cpuspeed;
        long    hwflags;
        u_long  physmem;
        u_long  usermem;
        u_long  pagesize;

        char    *ostype;
        char    *osrelease;
        long    os_revision;
        long    posix1_version;
        char    *version;

        long    hz;
        long    profhz;
        int     ngroups_max;
        long    arg_max;
        long    open_max;
        long    child_max;

        struct  timeval boottime;
        char    *hostname;
};

int
compat_43_sys_getkerninfo(struct lwp *l, void *v, register_t *retval)
{
	struct compat_43_sys_getkerninfo_args /* {
		syscallarg(int) op;
		syscallarg(char *) where;
		syscallarg(int *) size;
		syscallarg(int) arg;
	} */ *uap = v;
	int error, name[6];
	size_t size;

	if (SCARG(uap, size) && (error = copyin((caddr_t)SCARG(uap, size),
	    (caddr_t)&size, sizeof(size))))
		return (error);

	switch (SCARG(uap, op) & 0xff00) {

	case KINFO_RT:
		name[0] = CTL_NET;
		name[1] = PF_ROUTE;
		name[2] = 0;
		name[3] = (SCARG(uap, op) & 0xff0000) >> 16;
		name[4] = SCARG(uap, op) & 0xff;
		name[5] = SCARG(uap, arg);
		error = old_sysctl(&name[0], 6, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;

	case KINFO_VNODE:
		name[0] = CTL_KERN;
		name[1] = KERN_VNODE;
		error = old_sysctl(&name[0], 2, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;

	case KINFO_PROC:
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = SCARG(uap, op) & 0xff;
		name[3] = SCARG(uap, arg);
		error = old_sysctl(&name[0], 4, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;

	case KINFO_FILE:
		name[0] = CTL_KERN;
		name[1] = KERN_FILE;
		error = old_sysctl(&name[0], 2, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;

	case KINFO_METER:
		name[0] = CTL_VM;
		name[1] = VM_METER;
		error = old_sysctl(&name[0], 2, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;

	case KINFO_LOADAVG:
		name[0] = CTL_VM;
		name[1] = VM_LOADAVG;
		error = old_sysctl(&name[0], 2, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;

	case KINFO_CLOCKRATE:
		name[0] = CTL_KERN;
		name[1] = KERN_CLOCKRATE;
		error = old_sysctl(&name[0], 2, SCARG(uap, where), &size,
				   NULL, 0, l);
		break;


	case KINFO_BSDI_SYSINFO:
		{
			size_t len;
			struct bsdi_si *usi =
			    (struct bsdi_si *) SCARG(uap, where);
			struct bsdi_si ksi;
			char *us = (char *) &usi[1];

			if (usi == NULL) {
				size = sizeof(ksi) +
				    strlen(ostype) + strlen(cpu_model) +
				    strlen(osrelease) + strlen(machine) +
				    strlen(version) + strlen(hostname) + 6;
				error = 0;
				break;
			}

#define COPY(fld)							\
			ksi.fld = us - (u_long) usi;			\
			if ((error = copyoutstr(fld, us, 1024, &len)) != 0)\
				return error;				\
			us += len

			COPY(machine);
			COPY(cpu_model);
			ksi.ncpu = 1;			/* XXX */
			ksi.cpuspeed = 40;		/* XXX */
			ksi.hwflags = 0;		/* XXX */
			ksi.physmem = ctob(physmem);
			ksi.usermem = ctob(physmem);	/* XXX */
			ksi.pagesize = PAGE_SIZE;

			COPY(ostype);
			COPY(osrelease);
			ksi.os_revision = NetBSD;	/* XXX */
			ksi.posix1_version = _POSIX_VERSION;
			COPY(version);			/* XXX */

			ksi.hz = hz;
			ksi.profhz = profhz;
			ksi.ngroups_max = NGROUPS_MAX;
			ksi.arg_max = ARG_MAX;
			ksi.open_max = OPEN_MAX;
			ksi.child_max = CHILD_MAX;

			ksi.boottime = boottime;
			COPY(hostname);

			size = (us - (char *) &usi[1]) + sizeof(ksi);

			if ((error = copyout(&ksi, usi, sizeof(ksi))) != 0)
				return error;
		}
		break;

	default:
		return (EOPNOTSUPP);
	}
	if (error)
		return (error);
	*retval = size;
	if (SCARG(uap, size))
		error = copyout((caddr_t)&size, (caddr_t)SCARG(uap, size),
		    sizeof(size));
	return (error);
}


/* ARGSUSED */
int
compat_43_sys_sethostid(struct lwp *l, void *v, register_t *retval)
{
	struct compat_43_sys_sethostid_args /* {
		syscallarg(int32_t) hostid;
	} */ *uap = v;
	int error;

	if ((error = kauth_authorize_generic(l->l_cred,
	    KAUTH_GENERIC_ISSUSER, &l->l_acflag)) != 0)
		return (error);
	hostid = SCARG(uap, hostid);
	return (0);
}


/* ARGSUSED */
int
compat_43_sys_sethostname(struct lwp *l, void *v, register_t *retval)
{
	struct compat_43_sys_sethostname_args *uap = v;
	int name[2];

	name[0] = CTL_KERN;
	name[1] = KERN_HOSTNAME;
	return (old_sysctl(&name[0], 2, 0, 0, SCARG(uap, hostname),
			   SCARG(uap, len), l));
}
