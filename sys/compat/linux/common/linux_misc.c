/*	$NetBSD: linux_misc.c,v 1.109.4.3 2003/10/22 03:47:43 jmc Exp $	*/

/*-
 * Copyright (c) 1995, 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden and Eric Haszlakiewicz; by Jason R. Thorpe
 * of the Numerical Aerospace Simulation Facility, NASA Ames Research Center.
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

/*
 * Linux compatibility module. Try to deal with various Linux system calls.
 */

/*
 * These functions have been moved to multiarch to allow
 * selection of which machines include them to be 
 * determined by the individual files.linux_<arch> files.
 *
 * Function in multiarch:
 *	linux_sys_break			: linux_break.c
 *	linux_sys_alarm			: linux_misc_notalpha.c
 *	linux_sys_getresgid		: linux_misc_notalpha.c
 *	linux_sys_nice			: linux_misc_notalpha.c
 *	linux_sys_readdir		: linux_misc_notalpha.c
 *	linux_sys_setresgid		: linux_misc_notalpha.c
 *	linux_sys_time			: linux_misc_notalpha.c
 *	linux_sys_utime			: linux_misc_notalpha.c
 *	linux_sys_waitpid		: linux_misc_notalpha.c
 *	linux_sys_old_mmap		: linux_oldmmap.c
 *	linux_sys_oldolduname		: linux_oldolduname.c
 *	linux_sys_oldselect		: linux_oldselect.c
 *	linux_sys_olduname		: linux_olduname.c
 *	linux_sys_pipe			: linux_pipe.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_misc.c,v 1.109.4.3 2003/10/22 03:47:43 jmc Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/swap.h>		/* for SWAP_ON */
#include <sys/sysctl.h>		/* for KERN_DOMAINNAME */

#include <sys/ptrace.h>
#include <machine/ptrace.h>

#include <sys/syscallargs.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>

#include <compat/linux/linux_syscallargs.h>

#include <compat/linux/common/linux_fcntl.h>
#include <compat/linux/common/linux_mmap.h>
#include <compat/linux/common/linux_dirent.h>
#include <compat/linux/common/linux_util.h>
#include <compat/linux/common/linux_misc.h>
#include <compat/linux/common/linux_ptrace.h>
#include <compat/linux/common/linux_reboot.h>
#include <compat/linux/common/linux_emuldata.h>

const int linux_ptrace_request_map[] = {
	LINUX_PTRACE_TRACEME,	PT_TRACE_ME,
	LINUX_PTRACE_PEEKTEXT,	PT_READ_I,
	LINUX_PTRACE_PEEKDATA,	PT_READ_D,
	LINUX_PTRACE_POKETEXT,	PT_WRITE_I,
	LINUX_PTRACE_POKEDATA,	PT_WRITE_D,
	LINUX_PTRACE_CONT,	PT_CONTINUE,
	LINUX_PTRACE_KILL,	PT_KILL,
	LINUX_PTRACE_ATTACH,	PT_ATTACH,
	LINUX_PTRACE_DETACH,	PT_DETACH,
#ifdef PT_STEP
	LINUX_PTRACE_SINGLESTEP,	PT_STEP,
#endif
	-1
};

const static struct mnttypes {
	char *bsd;
	int linux;
} fstypes[] = {
	{ MOUNT_FFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_NFS,		LINUX_NFS_SUPER_MAGIC 		},
	{ MOUNT_MFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_MSDOS,		LINUX_MSDOS_SUPER_MAGIC		},
	{ MOUNT_LFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_FDESC,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_PORTAL,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_NULL,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_OVERLAY,	LINUX_DEFAULT_SUPER_MAGIC	},	
	{ MOUNT_UMAP,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_KERNFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_PROCFS,		LINUX_PROC_SUPER_MAGIC		},
	{ MOUNT_AFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_CD9660,		LINUX_ISOFS_SUPER_MAGIC		},
	{ MOUNT_UNION,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_ADOSFS,		LINUX_ADFS_SUPER_MAGIC		},
	{ MOUNT_EXT2FS,		LINUX_EXT2_SUPER_MAGIC		},
	{ MOUNT_CFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_CODA,		LINUX_CODA_SUPER_MAGIC		},
	{ MOUNT_FILECORE,	LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_NTFS,		LINUX_DEFAULT_SUPER_MAGIC	},
	{ MOUNT_SMBFS,		LINUX_SMB_SUPER_MAGIC		}
};
#define FSTYPESSIZE (sizeof(fstypes) / sizeof(fstypes[0]))

#ifdef DEBUG_LINUX
#define DPRINTF(a)	uprintf a
#else
#define DPRINTF(a)
#endif

/* Local linux_misc.c functions: */
static void bsd_to_linux_statfs __P((struct statfs *, struct linux_statfs *));
static int linux_to_bsd_limit __P((int));

/*
 * The information on a terminated (or stopped) process needs
 * to be converted in order for Linux binaries to get a valid signal
 * number out of it.
 */
void
bsd_to_linux_wstat(st)
	int *st;
{

	int sig;

	if (WIFSIGNALED(*st)) {
		sig = WTERMSIG(*st);
		if (sig >= 0 && sig < NSIG)
			*st= (*st& ~0177) | native_to_linux_signo[sig];
	} else if (WIFSTOPPED(*st)) {
		sig = WSTOPSIG(*st);
		if (sig >= 0 && sig < NSIG)
			*st = (*st & ~0xff00) |
			    (native_to_linux_signo[sig] << 8);
	}
}

/*
 * This is very much the same as waitpid()
 */
int
linux_sys_wait4(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_wait4_args /* {
		syscallarg(int) pid;
		syscallarg(int *) status;
		syscallarg(int) options;
		syscallarg(struct rusage *) rusage;
	} */ *uap = v;
	struct sys_wait4_args w4a;
	int error, *status, tstat, options, linux_options;
	caddr_t sg;

	if (SCARG(uap, status) != NULL) {
		sg = stackgap_init(p, 0);
		status = (int *) stackgap_alloc(p, &sg, sizeof *status);
	} else
		status = NULL;

	linux_options = SCARG(uap, options);
	options = 0;
	if (linux_options &
	    ~(LINUX_WAIT4_WNOHANG|LINUX_WAIT4_WUNTRACED|LINUX_WAIT4_WALL|
	      LINUX_WAIT4_WCLONE))
		return (EINVAL);

	if (linux_options & LINUX_WAIT4_WNOHANG)
		options |= WNOHANG;
	if (linux_options & LINUX_WAIT4_WUNTRACED)
		options |= WUNTRACED;
	if (linux_options & LINUX_WAIT4_WALL)
		options |= WALLSIG;
	if (linux_options & LINUX_WAIT4_WCLONE)
		options |= WALTSIG;

	SCARG(&w4a, pid) = SCARG(uap, pid);
	SCARG(&w4a, status) = status;
	SCARG(&w4a, options) = options;
	SCARG(&w4a, rusage) = SCARG(uap, rusage);

	if ((error = sys_wait4(p, &w4a, retval)))
		return error;

	sigdelset(&p->p_sigctx.ps_siglist, SIGCHLD);

	if (status != NULL) {
		if ((error = copyin(status, &tstat, sizeof tstat)))
			return error;

		bsd_to_linux_wstat(&tstat);
		return copyout(&tstat, SCARG(uap, status), sizeof tstat);
	}

	return 0;
}

/*
 * Linux brk(2). The check if the new address is >= the old one is
 * done in the kernel in Linux. NetBSD does it in the library.
 */
int
linux_sys_brk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_brk_args /* {
		syscallarg(char *) nsize;
	} */ *uap = v;
	char *nbrk = SCARG(uap, nsize);
	struct sys_obreak_args oba;
	struct vmspace *vm = p->p_vmspace;
	struct linux_emuldata *ed = (struct linux_emuldata*)p->p_emuldata;

	SCARG(&oba, nsize) = nbrk;

	if ((caddr_t) nbrk > vm->vm_daddr && sys_obreak(p, &oba, retval) == 0) 
		ed->p_break = (char*)nbrk;
	else 
		nbrk = ed->p_break;

	retval[0] = (register_t)nbrk;

	return 0;
}

/*
 * Convert BSD statfs structure to Linux statfs structure.
 * The Linux structure has less fields, and it also wants
 * the length of a name in a dir entry in a field, which
 * we fake (probably the wrong way).
 */
static void
bsd_to_linux_statfs(bsp, lsp)
	struct statfs *bsp;
	struct linux_statfs *lsp;
{
	int i;

	for (i = 0; i < FSTYPESSIZE; i++)
		if (strcmp(bsp->f_fstypename, fstypes[i].bsd) == 0)
			break;

	if (i == FSTYPESSIZE) {
		DPRINTF(("unhandled fstype in linux emulation: %s\n",
		    bsp->f_fstypename));
		lsp->l_ftype = LINUX_DEFAULT_SUPER_MAGIC;
	} else {
		lsp->l_ftype = fstypes[i].linux;
	}

	lsp->l_fbsize = bsp->f_bsize;
	lsp->l_fblocks = bsp->f_blocks;
	lsp->l_fbfree = bsp->f_bfree;
	lsp->l_fbavail = bsp->f_bavail;
	lsp->l_ffiles = bsp->f_files;
	lsp->l_fffree = bsp->f_ffree;
	/* Linux sets the fsid to 0..., we don't */
	lsp->l_ffsid.val[0] = bsp->f_fsid.val[0];
	lsp->l_ffsid.val[1] = bsp->f_fsid.val[1];
	lsp->l_fnamelen = MAXNAMLEN;	/* XXX */
	(void)memset(lsp->l_fspare, 0, sizeof(lsp->l_fspare));
}

/*
 * Implement the fs stat functions. Straightforward.
 */
int
linux_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_statfs_args /* {
		syscallarg(const char *) path;
		syscallarg(struct linux_statfs *) sp;
	} */ *uap = v;
	struct statfs btmp, *bsp;
	struct linux_statfs ltmp;
	struct sys_statfs_args bsa;
	caddr_t sg;
	int error;

	sg = stackgap_init(p, 0);
	bsp = (struct statfs *) stackgap_alloc(p, &sg, sizeof (struct statfs));

	CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&bsa, path) = SCARG(uap, path);
	SCARG(&bsa, buf) = bsp;

	if ((error = sys_statfs(p, &bsa, retval)))
		return error;

	if ((error = copyin((caddr_t) bsp, (caddr_t) &btmp, sizeof btmp)))
		return error;

	bsd_to_linux_statfs(&btmp, &ltmp);

	return copyout((caddr_t) &ltmp, (caddr_t) SCARG(uap, sp), sizeof ltmp);
}

int
linux_sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct linux_statfs *) sp;
	} */ *uap = v;
	struct statfs btmp, *bsp;
	struct linux_statfs ltmp;
	struct sys_fstatfs_args bsa;
	caddr_t sg;
	int error;

	sg = stackgap_init(p, 0);
	bsp = (struct statfs *) stackgap_alloc(p, &sg, sizeof (struct statfs));

	SCARG(&bsa, fd) = SCARG(uap, fd);
	SCARG(&bsa, buf) = bsp;

	if ((error = sys_fstatfs(p, &bsa, retval)))
		return error;

	if ((error = copyin((caddr_t) bsp, (caddr_t) &btmp, sizeof btmp)))
		return error;

	bsd_to_linux_statfs(&btmp, &ltmp);

	return copyout((caddr_t) &ltmp, (caddr_t) SCARG(uap, sp), sizeof ltmp);
}

/*
 * uname(). Just copy the info from the various strings stored in the
 * kernel, and put it in the Linux utsname structure. That structure
 * is almost the same as the NetBSD one, only it has fields 65 characters
 * long, and an extra domainname field.
 */
int
linux_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_uname_args /* {
		syscallarg(struct linux_utsname *) up;
	} */ *uap = v;
	struct linux_utsname luts;

	strncpy(luts.l_sysname, linux_sysname, sizeof(luts.l_sysname));
	strncpy(luts.l_nodename, hostname, sizeof(luts.l_nodename));
	strncpy(luts.l_release, linux_release, sizeof(luts.l_release));
	strncpy(luts.l_version, linux_version, sizeof(luts.l_version));
	strncpy(luts.l_machine, machine, sizeof(luts.l_machine));
	strncpy(luts.l_domainname, domainname, sizeof(luts.l_domainname));

	return copyout(&luts, SCARG(uap, up), sizeof(luts));
}

/* Used directly on: alpha, mips, ppc, sparc, sparc64 */
/* Used indirectly on: arm, i386, m68k */

/*
 * New type Linux mmap call.
 * Only called directly on machines with >= 6 free regs.
 */
int
linux_sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_mmap_args /* {
		syscallarg(unsigned long) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(linux_off_t) offset;
	} */ *uap = v;
	struct sys_mmap_args cma;
	int flags = 0, fl = SCARG(uap, flags);
	
	if (SCARG(uap, offset) & PAGE_MASK)
		return EINVAL;

	flags |= cvtto_bsd_mask(fl, LINUX_MAP_SHARED, MAP_SHARED);
	flags |= cvtto_bsd_mask(fl, LINUX_MAP_PRIVATE, MAP_PRIVATE);
	flags |= cvtto_bsd_mask(fl, LINUX_MAP_FIXED, MAP_FIXED);
	flags |= cvtto_bsd_mask(fl, LINUX_MAP_ANON, MAP_ANON);
	/* XXX XAX ERH: Any other flags here?  There are more defined... */

	SCARG(&cma, addr) = (void *)SCARG(uap, addr);
	SCARG(&cma, len) = SCARG(uap, len);
	SCARG(&cma, prot) = SCARG(uap, prot);
	if (SCARG(&cma, prot) & VM_PROT_WRITE) /* XXX */
		SCARG(&cma, prot) |= VM_PROT_READ;
	SCARG(&cma, flags) = flags;
	SCARG(&cma, fd) = flags & MAP_ANON ? -1 : SCARG(uap, fd);
	SCARG(&cma, pad) = 0;
	SCARG(&cma, pos) = (off_t)SCARG(uap, offset);

	return sys_mmap(p, &cma, retval);
}

int
linux_sys_mremap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_mremap_args /* {
		syscallarg(void *) old_address;
		syscallarg(size_t) old_size;
		syscallarg(size_t) new_size;
		syscallarg(u_long) flags;
	} */ *uap = v;
	struct sys_munmap_args mua;
	size_t old_size, new_size;
	int error;

	old_size = round_page(SCARG(uap, old_size));
	new_size = round_page(SCARG(uap, new_size));

	/*
	 * Growing mapped region.
	 */
	if (new_size > old_size) {
		/*
		 * XXX Implement me.  What we probably want to do is
		 * XXX dig out the guts of the old mapping, mmap that
		 * XXX object again with the new size, then munmap
		 * XXX the old mapping.
		 */
		*retval = 0;
		return (ENOMEM);
	}

	/*
	 * Shrinking mapped region.
	 */
	if (new_size < old_size) {
		SCARG(&mua, addr) = (caddr_t)SCARG(uap, old_address) +
		    new_size;
		SCARG(&mua, len) = old_size - new_size;
		error = sys_munmap(p, &mua, retval);
		*retval = error ? 0 : (register_t)SCARG(uap, old_address);
		return (error);
	}

	/*
	 * No change.
	 */
	*retval = (register_t)SCARG(uap, old_address);
	return (0);
}

int
linux_sys_msync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_msync_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(int) len;
		syscallarg(int) fl;
	} */ *uap = v;

	struct sys___msync13_args bma;

	/* flags are ignored */
	SCARG(&bma, addr) = SCARG(uap, addr);
	SCARG(&bma, len) = SCARG(uap, len);
	SCARG(&bma, flags) = SCARG(uap, fl);

	return sys___msync13(p, &bma, retval);
}

int
linux_sys_mprotect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_mprotect_args /* {
		syscallarg(const void *) start;
		syscallarg(unsigned long) len;
		syscallarg(int) prot;
	} */ *uap = v;
	unsigned long end, start = (unsigned long)SCARG(uap, start), len;
	int prot = SCARG(uap, prot);
	struct vm_map_entry *entry;
	struct vm_map *map = &p->p_vmspace->vm_map;

	if (start & PAGE_MASK)
		return EINVAL;

	len = round_page(SCARG(uap, len));
	end = start + len;

	if (end < start)
		return EINVAL;
	else if (end == start)
		return 0;

	if (SCARG(uap, prot) & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return EINVAL;

	vm_map_lock(map);
#ifdef notdef
	VM_MAP_RANGE_CHECK(map, start, end);
#endif
	if (!uvm_map_lookup_entry(map, start, &entry) || entry->start > start) {
		vm_map_unlock(map);
		return EFAULT;
	}
	vm_map_unlock(map);
	return uvm_map_protect(map, start, end, prot, FALSE);
}

/*
 * This code is partly stolen from src/lib/libc/compat-43/times.c
 * XXX - CLK_TCK isn't declared in /sys, just in <time.h>, done here
 */

#define CLK_TCK 100
#define	CONVTCK(r)	(r.tv_sec * CLK_TCK + r.tv_usec / (1000000 / CLK_TCK))

int
linux_sys_times(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_times_args /* {
		syscallarg(struct times *) tms;
	} */ *uap = v;
	struct timeval t;
	int error, s;

	if (SCARG(uap, tms)) {
		struct linux_tms ltms;
		struct rusage ru;

		calcru(p, &ru.ru_utime, &ru.ru_stime, NULL);
		ltms.ltms_utime = CONVTCK(ru.ru_utime);
		ltms.ltms_stime = CONVTCK(ru.ru_stime);

		ltms.ltms_cutime = CONVTCK(p->p_stats->p_cru.ru_utime);
		ltms.ltms_cstime = CONVTCK(p->p_stats->p_cru.ru_stime);

		if ((error = copyout(&ltms, SCARG(uap, tms), sizeof ltms)))
			return error;
	}

	s = splclock();
	timersub(&time, &boottime, &t);
	splx(s);

	retval[0] = ((linux_clock_t)(CONVTCK(t)));
	return 0;
}

/*
 * Linux 'readdir' call. This code is mostly taken from the
 * SunOS getdents call (see compat/sunos/sunos_misc.c), though
 * an attempt has been made to keep it a little cleaner (failing
 * miserably, because of the cruft needed if count 1 is passed).
 *
 * The d_off field should contain the offset of the next valid entry,
 * but in Linux it has the offset of the entry itself. We emulate
 * that bug here.
 *
 * Read in BSD-style entries, convert them, and copy them out.
 *
 * Note that this doesn't handle union-mounted filesystems.
 */
int
linux_sys_getdents(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_getdents_args /* {
		syscallarg(int) fd;
		syscallarg(struct linux_dirent *) dent;
		syscallarg(unsigned int) count;
	} */ *uap = v;
	struct dirent *bdp;
	struct vnode *vp;
	caddr_t	inp, buf;		/* BSD-format */
	int len, reclen;		/* BSD-format */
	caddr_t outp;			/* Linux-format */
	int resid, linux_reclen = 0;	/* Linux-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct linux_dirent idb;
	off_t off;		/* true file offset */
	int buflen, error, eofflag, nbytes, oldcall;
	struct vattr va;
	off_t *cookiebuf = NULL, *cookie;
	int ncookies;

	/* getvnode() will use the descriptor for us */
	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out1;
	}

	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR) {
		error = EINVAL;
		goto out1;
	}

	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)))
		goto out1;

	nbytes = SCARG(uap, count);
	if (nbytes == 1) {	/* emulating old, broken behaviour */
		nbytes = sizeof (idb);
		buflen = max(va.va_blocksize, nbytes);
		oldcall = 1;
	} else {
		buflen = min(MAXBSIZE, nbytes);
		if (buflen < va.va_blocksize)
			buflen = va.va_blocksize;
		oldcall = 0;
	}
	buf = malloc(buflen, M_TEMP, M_WAITOK);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	/*
         * First we read into the malloc'ed buffer, then
         * we massage it into user space, one record at a time.
         */
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &cookiebuf,
	    &ncookies);
	if (error)
		goto out;

	inp = buf;
	outp = (caddr_t)SCARG(uap, dent);
	resid = nbytes;
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("linux_readdir");
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			off = *cookie++;
			continue;
		}
		linux_reclen = LINUX_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < linux_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a Linux-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_ino = bdp->d_fileno;
		/*
		 * The old readdir() call misuses the offset and reclen fields.
		 */
		if (oldcall) {
			idb.d_off = (linux_off_t)linux_reclen;
			idb.d_reclen = (u_short)bdp->d_namlen;
		} else {
			if (sizeof (idb.d_off) <= 4 && (off >> 32) != 0) {
				compat_offseterr(vp, "linux_getdents");
				error = EINVAL;
				goto out;
			}
			idb.d_off = (linux_off_t)off;
			idb.d_reclen = (u_short)linux_reclen;
		}
		strcpy(idb.d_name, bdp->d_name);
		if ((error = copyout((caddr_t)&idb, outp, linux_reclen)))
			goto out;
		/* advance past this real entry */
		inp += reclen;
		off = *cookie++;	/* each entry points to itself */
		/* advance output past Linux-shaped entry */
		outp += linux_reclen;
		resid -= linux_reclen;
		if (oldcall)
			break;
	}

	/* if we squished out the whole block, try again */
	if (outp == (caddr_t)SCARG(uap, dent))
		goto again;
	fp->f_offset = off;	/* update the vnode offset */

	if (oldcall)
		nbytes = resid + linux_reclen;

eof:
	*retval = nbytes - resid;
out:
	VOP_UNLOCK(vp, 0);
	if (cookiebuf)
		free(cookiebuf, M_TEMP);
	free(buf, M_TEMP);
out1:
	FILE_UNUSE(fp, p);
	return error;
}

/*
 * Even when just using registers to pass arguments to syscalls you can
 * have 5 of them on the i386. So this newer version of select() does
 * this.
 */
int
linux_sys_select(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_select_args /* {
		syscallarg(int) nfds;
		syscallarg(fd_set *) readfds;
		syscallarg(fd_set *) writefds;
		syscallarg(fd_set *) exceptfds;
		syscallarg(struct timeval *) timeout;
	} */ *uap = v;

	return linux_select1(p, retval, SCARG(uap, nfds), SCARG(uap, readfds),
	    SCARG(uap, writefds), SCARG(uap, exceptfds), SCARG(uap, timeout));
}

/*
 * Common code for the old and new versions of select(). A couple of
 * things are important:
 * 1) return the amount of time left in the 'timeout' parameter
 * 2) select never returns ERESTART on Linux, always return EINTR
 */
int
linux_select1(p, retval, nfds, readfds, writefds, exceptfds, timeout)
	struct proc *p;
	register_t *retval;
	int nfds;
	fd_set *readfds, *writefds, *exceptfds;
	struct timeval *timeout;
{
	struct sys_select_args bsa;
	struct timeval tv0, tv1, utv, *tvp;
	caddr_t sg;
	int error;

	SCARG(&bsa, nd) = nfds;
	SCARG(&bsa, in) = readfds;
	SCARG(&bsa, ou) = writefds;
	SCARG(&bsa, ex) = exceptfds;
	SCARG(&bsa, tv) = timeout;

	/*
	 * Store current time for computation of the amount of
	 * time left.
	 */
	if (timeout) {
		if ((error = copyin(timeout, &utv, sizeof(utv))))
			return error;
		if (itimerfix(&utv)) {
			/*
			 * The timeval was invalid.  Convert it to something
			 * valid that will act as it does under Linux.
			 */
			sg = stackgap_init(p, 0);
			tvp = stackgap_alloc(p, &sg, sizeof(utv));
			utv.tv_sec += utv.tv_usec / 1000000;
			utv.tv_usec %= 1000000;
			if (utv.tv_usec < 0) {
				utv.tv_sec -= 1;
				utv.tv_usec += 1000000;
			}
			if (utv.tv_sec < 0)
				timerclear(&utv);
			if ((error = copyout(&utv, tvp, sizeof(utv))))
				return error;
			SCARG(&bsa, tv) = tvp;
		}
		microtime(&tv0);
	}

	error = sys_select(p, &bsa, retval);
	if (error) {
		/*
		 * See fs/select.c in the Linux kernel.  Without this,
		 * Maelstrom doesn't work.
		 */
		if (error == ERESTART)
			error = EINTR;
		return error;
	}

	if (timeout) {
		if (*retval) {
			/*
			 * Compute how much time was left of the timeout,
			 * by subtracting the current time and the time
			 * before we started the call, and subtracting
			 * that result from the user-supplied value.
			 */
			microtime(&tv1);
			timersub(&tv1, &tv0, &tv1);
			timersub(&utv, &tv1, &utv);
			if (utv.tv_sec < 0)
				timerclear(&utv);
		} else
			timerclear(&utv);
		if ((error = copyout(&utv, timeout, sizeof(utv))))
			return error;
	}

	return 0;
}

/*
 * Get the process group of a certain process. Look it up
 * and return the value.
 */
int
linux_sys_getpgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_getpgid_args /* {
		syscallarg(int) pid;
	} */ *uap = v;
	struct proc *targp;

	if (SCARG(uap, pid) != 0 && SCARG(uap, pid) != p->p_pid) {
		if ((targp = pfind(SCARG(uap, pid))) == 0)
			return ESRCH;
	}
	else
		targp = p;

	retval[0] = targp->p_pgid;
	return 0;
}

/*
 * Set the 'personality' (emulation mode) for the current process. Only
 * accept the Linux personality here (0). This call is needed because
 * the Linux ELF crt0 issues it in an ugly kludge to make sure that
 * ELF binaries run in Linux mode, not SVR4 mode.
 */
int
linux_sys_personality(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_personality_args /* {
		syscallarg(int) per;
	} */ *uap = v;

	if (SCARG(uap, per) != 0)
		return EINVAL;
	retval[0] = 0;
	return 0;
}

#if defined(__i386__) || defined(__m68k__)
/*
 * The calls are here because of type conversions.
 */
int
linux_sys_setreuid16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setreuid16_args /* {
		syscallarg(int) ruid;
		syscallarg(int) euid;
	} */ *uap = v;
	struct sys_setreuid_args bsa;
	
	SCARG(&bsa, ruid) = ((linux_uid_t)SCARG(uap, ruid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, ruid);
	SCARG(&bsa, euid) = ((linux_uid_t)SCARG(uap, euid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, euid);

	return sys_setreuid(p, &bsa, retval);
}

int
linux_sys_setregid16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setregid16_args /* {
		syscallarg(int) rgid;
		syscallarg(int) egid;
	} */ *uap = v;
	struct sys_setregid_args bsa;
	
	SCARG(&bsa, rgid) = ((linux_gid_t)SCARG(uap, rgid) == (linux_gid_t)-1) ?
		(uid_t)-1 : SCARG(uap, rgid);
	SCARG(&bsa, egid) = ((linux_gid_t)SCARG(uap, egid) == (linux_gid_t)-1) ?
		(uid_t)-1 : SCARG(uap, egid);

	return sys_setregid(p, &bsa, retval);
}

int
linux_sys_setresuid16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setresuid16_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
		syscallarg(uid_t) suid;
	} */ *uap = v;
	struct linux_sys_setresuid16_args lsa;

	SCARG(&lsa, ruid) = ((linux_uid_t)SCARG(uap, ruid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, ruid);
	SCARG(&lsa, euid) = ((linux_uid_t)SCARG(uap, euid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, euid);
	SCARG(&lsa, suid) = ((linux_uid_t)SCARG(uap, suid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, suid);

	return linux_sys_setresuid(p, &lsa, retval);
}

int
linux_sys_setresgid16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setresgid16_args /* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
		syscallarg(gid_t) sgid;
	} */ *uap = v;
	struct linux_sys_setresgid16_args lsa;

	SCARG(&lsa, rgid) = ((linux_gid_t)SCARG(uap, rgid) == (linux_gid_t)-1) ?
		(gid_t)-1 : SCARG(uap, rgid);
	SCARG(&lsa, egid) = ((linux_gid_t)SCARG(uap, egid) == (linux_gid_t)-1) ?
		(gid_t)-1 : SCARG(uap, egid);
	SCARG(&lsa, sgid) = ((linux_gid_t)SCARG(uap, sgid) == (linux_gid_t)-1) ?
		(gid_t)-1 : SCARG(uap, sgid);

	return linux_sys_setresgid(p, &lsa, retval);
}

int
linux_sys_getgroups16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_getgroups16_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(linux_gid_t *) gidset;
	} */ *uap = v;
	caddr_t sg; 
	int n, error, i;
	struct sys_getgroups_args bsa;
	gid_t *bset, *kbset;
	linux_gid_t *lset;
	struct pcred *pc = p->p_cred;

	n = SCARG(uap, gidsetsize);
	if (n < 0)
		return EINVAL;
	error = 0;
	bset = kbset = NULL;
	lset = NULL;
	if (n > 0) {
		n = min(pc->pc_ucred->cr_ngroups, n);
		sg = stackgap_init(p, 0);
		bset = stackgap_alloc(p, &sg, n * sizeof (gid_t));
		kbset = malloc(n * sizeof (gid_t), M_TEMP, M_WAITOK);
		lset = malloc(n * sizeof (linux_gid_t), M_TEMP, M_WAITOK);
		if (bset == NULL || kbset == NULL || lset == NULL)
			return ENOMEM;
		SCARG(&bsa, gidsetsize) = n;
		SCARG(&bsa, gidset) = bset;
		error = sys_getgroups(p, &bsa, retval);
		if (error != 0)
			goto out;
		error = copyin(bset, kbset, n * sizeof (gid_t));
		if (error != 0)
			goto out;
		for (i = 0; i < n; i++)
			lset[i] = (linux_gid_t)kbset[i];
		error = copyout(lset, SCARG(uap, gidset),
		    n * sizeof (linux_gid_t));
	} else
		*retval = pc->pc_ucred->cr_ngroups;
out:
	if (kbset != NULL)
		free(kbset, M_TEMP);
	if (lset != NULL)
		free(lset, M_TEMP);
	return error;
}

int
linux_sys_setgroups16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setgroups16_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(linux_gid_t *) gidset;
	} */ *uap = v;
	caddr_t sg;
	int n;
	int error, i;
	struct sys_setgroups_args bsa;
	gid_t *bset, *kbset;
	linux_gid_t *lset;

	n = SCARG(uap, gidsetsize);
	if (n < 0 || n > NGROUPS)
		return EINVAL;
	sg = stackgap_init(p, 0);
	bset = stackgap_alloc(p, &sg, n * sizeof (gid_t));
	lset = malloc(n * sizeof (linux_gid_t), M_TEMP, M_WAITOK);
	kbset = malloc(n * sizeof (gid_t), M_TEMP, M_WAITOK);
	if (lset == NULL || bset == NULL)
		return ENOMEM;
	error = copyin(SCARG(uap, gidset), lset, n * sizeof (linux_gid_t));
	if (error != 0)
		goto out;
	for (i = 0; i < n; i++)
		kbset[i] = (gid_t)lset[i];
	error = copyout(kbset, bset, n * sizeof (gid_t));
	if (error != 0)
		goto out;
	SCARG(&bsa, gidsetsize) = n;
	SCARG(&bsa, gidset) = bset;
	error = sys_setgroups(p, &bsa, retval);
	
out:
	if (lset != NULL)
		free(lset, M_TEMP);
	if (kbset != NULL)
		free(kbset, M_TEMP);

	return error;
}

#endif /* __i386__ || __m68k__ */

/*
 * We have nonexistent fsuid equal to uid.
 * If modification is requested, refuse.
 */
int
linux_sys_setfsuid(p, v, retval)
	 struct proc *p;
	 void *v;
	 register_t *retval;
{
	 struct linux_sys_setfsuid_args /* {
		 syscallarg(uid_t) uid;
	 } */ *uap = v;
	 uid_t uid;

	 uid = SCARG(uap, uid);
	 if (p->p_cred->p_ruid != uid)
		 return sys_nosys(p, v, retval);
	 else
		 return (0);
}

/* XXX XXX XXX */
#ifndef alpha
int
linux_sys_getfsuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return sys_getuid(p, v, retval);
}
#endif

int
linux_sys_setresuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setresuid_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
		syscallarg(uid_t) suid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	uid_t ruid, euid, suid;
	int error;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);
	suid = SCARG(uap, suid);

	/*
	 * Note: These checks are a little different than the NetBSD
	 * setreuid(2) call performs.  This precisely follows the
	 * behavior of the Linux kernel.
	 */
	if (ruid != (uid_t)-1 &&
	    ruid != pc->p_ruid &&
	    ruid != pc->pc_ucred->cr_uid &&
	    ruid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (euid != (uid_t)-1 &&
	    euid != pc->p_ruid &&
	    euid != pc->pc_ucred->cr_uid &&
	    euid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (suid != (uid_t)-1 &&
	    suid != pc->p_ruid &&
	    suid != pc->pc_ucred->cr_uid &&
	    suid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Now assign the new real, effective, and saved UIDs.
	 * Note that Linux, unlike NetBSD in setreuid(2), does not
	 * set the saved UID in this call unless the user specifies
	 * it.
	 */
	if (ruid != (uid_t)-1) {
		(void)chgproccnt(pc->p_ruid, -1);
		(void)chgproccnt(ruid, 1);
		pc->p_ruid = ruid;
	}

	if (euid != (uid_t)-1) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_uid = euid;
	}

	if (suid != (uid_t)-1)
		pc->p_svuid = suid;

	if (ruid != (uid_t)-1 && euid != (uid_t)-1 && suid != (uid_t)-1)
		p->p_flag |= P_SUGID;
	return (0);
}

int
linux_sys_getresuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_getresuid_args /* {
		syscallarg(uid_t *) ruid;
		syscallarg(uid_t *) euid;
		syscallarg(uid_t *) suid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	int error;

	/*
	 * Linux copies these values out to userspace like so:
	 *
	 *	1. Copy out ruid.
	 *	2. If that succeeds, copy out euid.
	 *	3. If both of those succeed, copy out suid.
	 */
	if ((error = copyout(&pc->p_ruid, SCARG(uap, ruid),
			     sizeof(uid_t))) != 0)
		return (error);

	if ((error = copyout(&pc->pc_ucred->cr_uid, SCARG(uap, euid),
			     sizeof(uid_t))) != 0)
		return (error);

	return (copyout(&pc->p_svuid, SCARG(uap, suid), sizeof(uid_t)));
}

int
linux_sys_ptrace(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ptrace_args /* {
		i386, m68k, powerpc: T=int
		alpha: T=long
		syscallarg(T) request;
		syscallarg(T) pid;
		syscallarg(T) addr;
		syscallarg(T) data;
	} */ *uap = v;
	const int *ptr;
	int request;
	int error;

	ptr = linux_ptrace_request_map;
	request = SCARG(uap, request);
	while (*ptr != -1)
		if (*ptr++ == request) {
			struct sys_ptrace_args pta;

			SCARG(&pta, req) = *ptr;
			SCARG(&pta, pid) = SCARG(uap, pid);
			SCARG(&pta, addr) = (caddr_t)SCARG(uap, addr);
			SCARG(&pta, data) = SCARG(uap, data);

			/*
			 * Linux ptrace(PTRACE_CONT, pid, 0, 0) means actually
			 * to continue where the process left off previously.
			 * The same thing is achieved by addr == (caddr_t) 1
			 * on NetBSD, so rewrite 'addr' appropriately.
			 */
			if (request == LINUX_PTRACE_CONT && SCARG(uap, addr)==0)
				SCARG(&pta, addr) = (caddr_t) 1;
			
			error = sys_ptrace(p, &pta, retval);
			if (error) 
				return error;
			switch (request) {
			case LINUX_PTRACE_PEEKTEXT:
			case LINUX_PTRACE_PEEKDATA:
				error = copyout (retval, 
				    (caddr_t)SCARG(uap, data), sizeof *retval);
				*retval = SCARG(uap, data);
				break;
			default:	
				break;
			}
			return error;
		}
		else
			ptr++;

	return LINUX_SYS_PTRACE_ARCH(p, uap, retval);
}

int
linux_sys_reboot(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_reboot_args /* {
		syscallarg(int) magic1;
		syscallarg(int) magic2;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap = v;
	struct sys_reboot_args /* {
		syscallarg(int) opt;
		syscallarg(char *) bootstr;
	} */ sra;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return(error);

	if (SCARG(uap, magic1) != LINUX_REBOOT_MAGIC1)
		return(EINVAL);
	if (SCARG(uap, magic2) != LINUX_REBOOT_MAGIC2 &&
	    SCARG(uap, magic2) != LINUX_REBOOT_MAGIC2A &&
	    SCARG(uap, magic2) != LINUX_REBOOT_MAGIC2B)
		return(EINVAL);

	switch (SCARG(uap, cmd)) {
	case LINUX_REBOOT_CMD_RESTART:
		SCARG(&sra, opt) = RB_AUTOBOOT;
		break;
	case LINUX_REBOOT_CMD_HALT:
		SCARG(&sra, opt) = RB_HALT;
		break;
	case LINUX_REBOOT_CMD_POWER_OFF:
		SCARG(&sra, opt) = RB_HALT|RB_POWERDOWN;
		break;
	case LINUX_REBOOT_CMD_RESTART2:
		/* Reboot with an argument. */
		SCARG(&sra, opt) = RB_AUTOBOOT|RB_STRING;
		SCARG(&sra, bootstr) = SCARG(uap, arg);
		break;
	case LINUX_REBOOT_CMD_CAD_ON:
		return(EINVAL);	/* We don't implement ctrl-alt-delete */
	case LINUX_REBOOT_CMD_CAD_OFF:
		return(0);
	default:
		return(EINVAL);
	}

	return(sys_reboot(p, &sra, retval));
}

/*
 * Copy of compat_12_sys_swapon().
 */
int
linux_sys_swapon(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_swapctl_args ua;
	struct linux_sys_swapon_args /* {
		syscallarg(const char *) name;
	} */ *uap = v;

	SCARG(&ua, cmd) = SWAP_ON;
	SCARG(&ua, arg) = (void *)SCARG(uap, name);
	SCARG(&ua, misc) = 0;	/* priority */
	return (sys_swapctl(p, &ua, retval));
}

/*
 * Stop swapping to the file or block device specified by path.
 */
int
linux_sys_swapoff(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_swapctl_args ua;
	struct linux_sys_swapoff_args /* {
		syscallarg(const char *) path;
	} */ *uap = v;

	SCARG(&ua, cmd) = SWAP_OFF;
	SCARG(&ua, arg) = (void *)SCARG(uap, path);
	return (sys_swapctl(p, &ua, retval));
}

/*
 * Copy of compat_09_sys_setdomainname()
 */
/* ARGSUSED */
int
linux_sys_setdomainname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setdomainname_args /* {
		syscallarg(char *) domainname;
		syscallarg(int) len;
	} */ *uap = v;
	int name;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	name = KERN_DOMAINNAME;
	return (kern_sysctl(&name, 1, 0, 0, SCARG(uap, domainname),
			    SCARG(uap, len), p));
}

/*
 * sysinfo()
 */
/* ARGSUSED */
int
linux_sys_sysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sysinfo_args /* {
		syscallarg(struct linux_sysinfo *) arg;
	} */ *uap = v;
	struct linux_sysinfo si;
	struct loadavg *la;

	si.uptime = time.tv_sec - boottime.tv_sec;
	la = &averunnable;
	si.loads[0] = la->ldavg[0] * LINUX_SYSINFO_LOADS_SCALE / la->fscale;
	si.loads[1] = la->ldavg[1] * LINUX_SYSINFO_LOADS_SCALE / la->fscale;
	si.loads[2] = la->ldavg[2] * LINUX_SYSINFO_LOADS_SCALE / la->fscale;
	si.totalram = ctob(physmem);
	si.freeram = uvmexp.free * uvmexp.pagesize;
	si.sharedram = 0;	/* XXX */
	si.bufferram = uvmexp.filepages * uvmexp.pagesize;
	si.totalswap = uvmexp.swpages * uvmexp.pagesize;
	si.freeswap = (uvmexp.swpages - uvmexp.swpginuse) * uvmexp.pagesize;
	si.procs = nprocs;

	/* The following are only present in newer Linux kernels. */
	si.totalbig = 0;
	si.freebig = 0;
	si.mem_unit = 1;

	return (copyout(&si, SCARG(uap, arg), sizeof si));
}

#define bsd_to_linux_rlimit1(l, b, f) \
    (l)->f = ((b)->f == RLIM_INFINITY || ((b)->f & 0xffffffff00000000) != 0) ? \
    LINUX_RLIM_INFINITY : (int32_t)(b)->f
#define bsd_to_linux_rlimit(l, b) \
    bsd_to_linux_rlimit1(l, b, rlim_cur); \
    bsd_to_linux_rlimit1(l, b, rlim_max)

#define linux_to_bsd_rlimit1(b, l, f) \
    (b)->f = (l)->f == LINUX_RLIM_INFINITY ? RLIM_INFINITY : (l)->f
#define linux_to_bsd_rlimit(b, l) \
    linux_to_bsd_rlimit1(b, l, rlim_cur); \
    linux_to_bsd_rlimit1(b, l, rlim_max)

static int
linux_to_bsd_limit(lim)
	int lim;
{
	switch (lim) {
	case LINUX_RLIMIT_CPU:
		return RLIMIT_CPU;
	case LINUX_RLIMIT_FSIZE:
		return RLIMIT_FSIZE;
	case LINUX_RLIMIT_DATA:
		return RLIMIT_DATA;
	case LINUX_RLIMIT_STACK:
		return RLIMIT_STACK;
	case LINUX_RLIMIT_CORE:
		return RLIMIT_CORE;
	case LINUX_RLIMIT_RSS:
		return RLIMIT_RSS;
	case LINUX_RLIMIT_NPROC:
		return RLIMIT_NPROC;
	case LINUX_RLIMIT_NOFILE:
		return RLIMIT_NOFILE;
	case LINUX_RLIMIT_MEMLOCK:
		return RLIMIT_MEMLOCK;
	case LINUX_RLIMIT_AS:
	case LINUX_RLIMIT_LOCKS:
		return -EOPNOTSUPP;
	default:
		return -EINVAL;
	}
}


int
linux_sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_getrlimit_args /* {
		syscallarg(int) which;
		syscallarg(struct orlimit *) rlp;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p, 0);
	struct sys_getrlimit_args ap;
	struct rlimit rl;
	struct orlimit orl;
	int error;

	SCARG(&ap, which) = linux_to_bsd_limit(SCARG(uap, which));
	if ((error = SCARG(&ap, which)) < 0)
		return -error;
	SCARG(&ap, rlp) = stackgap_alloc(p, &sg, sizeof rl);
	if ((error = sys_getrlimit(p, &ap, retval)) != 0)
		return error;
	if ((error = copyin(SCARG(&ap, rlp), &rl, sizeof(rl))) != 0)
		return error;
	bsd_to_linux_rlimit(&orl, &rl);
	return copyout(&orl, SCARG(uap, rlp), sizeof(orl));
}

int
linux_sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setrlimit_args /* {
		syscallarg(int) which;
		syscallarg(struct orlimit *) rlp;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p, 0);
	struct sys_setrlimit_args ap;
	struct rlimit rl;
	struct orlimit orl;
	int error;

	SCARG(&ap, which) = linux_to_bsd_limit(SCARG(uap, which));
	SCARG(&ap, rlp) = stackgap_alloc(p, &sg, sizeof rl);
	if ((error = SCARG(&ap, which)) < 0)
		return -error;
	if ((error = copyin(SCARG(uap, rlp), &orl, sizeof(orl))) != 0)
		return error;
	linux_to_bsd_rlimit(&rl, &orl);
	/* XXX: alpha complains about this */
	if ((error = copyout(&rl, (void *)SCARG(&ap, rlp), sizeof(rl))) != 0)
		return error;
	return sys_setrlimit(p, &ap, retval);
}

#ifndef __mips__
/* XXX: this doesn't look 100% common, at least mips doesn't have it */
int
linux_sys_ugetrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return linux_sys_getrlimit(p, v, retval);
}
#endif

/*
 * This gets called for unsupported syscalls. The difference to sys_nosys()
 * is that process does not get SIGSYS, the call just returns with ENOSYS.
 * This is the way Linux does it and glibc depends on this behaviour.
 */
int
linux_sys_nosys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return (ENOSYS);
}
