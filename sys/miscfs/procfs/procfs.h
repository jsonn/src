/*	$NetBSD: procfs.h,v 1.33.2.1 2001/03/05 22:49:51 nathanw Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs.h	8.9 (Berkeley) 5/14/95
 */

/*
 * The different types of node in a procfs filesystem
 */
typedef enum {
	Proot,		/* the filesystem root */
	Pcurproc,	/* symbolic link for curproc */
	Pself,		/* like curproc, but this is the Linux name */
	Pproc,		/* a process-specific sub-directory */
	Pfile,		/* the executable file */
	Pmem,		/* the process's memory image */
	Pregs,		/* the process's register set */
	Pfpregs,	/* the process's FP register set */
	Pctl,		/* process control */
	Pstatus,	/* process status */
	Pnote,		/* process notifier */
	Pnotepg,	/* process group notifier */
	Pmap,		/* memory map */
	Pcmdline,	/* process command line args */
	Pmeminfo,	/* system memory info (if -o linux) */
	Pcpuinfo	/* CPU info (if -o linux) */
} pfstype;

/*
 * control data for the proc file system.
 */
struct pfsnode {
	LIST_ENTRY(pfsnode) pfs_hash;	/* hash chain */
	struct vnode	*pfs_vnode;	/* vnode associated with this pfsnode */
	pfstype		pfs_type;	/* type of procfs node */
	pid_t		pfs_pid;	/* associated process */
	mode_t		pfs_mode;	/* mode bits for stat() */
	u_long		pfs_flags;	/* open flags */
	u_long		pfs_fileno;	/* unique file id */
};

#define PROCFS_NOTELEN	64	/* max length of a note (/proc/$pid/note) */
#define PROCFS_CTLLEN 	8	/* max length of a ctl msg (/proc/$pid/ctl */

struct procfs_args {
	int version;
	int flags;
};

#define PROCFS_ARGSVERSION	1

#define PROCFSMNT_LINUXCOMPAT	0x01

/*
 * Kernel stuff follows
 */
#ifdef _KERNEL
#define CNEQ(cnp, s, len) \
	 ((cnp)->cn_namelen == (len) && \
	  (memcmp((s), (cnp)->cn_nameptr, (len)) == 0))

#define UIO_MX 32

#define PROCFS_FILENO(pid, type) \
	(((type) < Pproc) ? \
			((type) + 2) : \
			((((pid)+1) << 4) + ((int) (type))))

struct procfsmount {
	void *pmnt_exechook;
	int pmnt_flags;
};

#define VFSTOPROC(mp)	((struct procfsmount *)(mp)->mnt_data)

/*
 * Convert between pfsnode vnode
 */
#define VTOPFS(vp)	((struct pfsnode *)(vp)->v_data)
#define PFSTOV(pfs)	((pfs)->pfs_vnode)

typedef struct vfs_namemap vfs_namemap_t;
struct vfs_namemap {
	const char *nm_name;
	int nm_val;
};

int vfs_getuserstr __P((struct uio *, char *, int *));
const vfs_namemap_t *vfs_findname __P((const vfs_namemap_t *, const char *, int));

#define PFIND(pid) ((pid) ? pfind(pid) : &proc0)
int procfs_freevp __P((struct vnode *));
int procfs_allocvp __P((struct mount *, struct vnode **, long, pfstype));
int procfs_donote __P((struct proc *, struct proc *, struct pfsnode *,
    struct uio *));
int procfs_doregs __P((struct proc *, struct lwp *, struct pfsnode *,
    struct uio *));
int procfs_dofpregs __P((struct proc *, struct lwp *, struct pfsnode *,
    struct uio *));
int procfs_domem __P((struct proc *, struct lwp *, struct pfsnode *,
    struct uio *));
int procfs_doctl __P((struct proc *, struct lwp *, struct pfsnode *,
    struct uio *));
int procfs_dostatus __P((struct proc *, struct lwp *, struct pfsnode *,
    struct uio *));
int procfs_domap __P((struct proc *, struct proc *, struct pfsnode *,
    struct uio *));
int procfs_docmdline __P((struct proc *, struct proc *, struct pfsnode *,
    struct uio *));
int procfs_domeminfo __P((struct proc *, struct proc *, struct pfsnode *,
    struct uio *));
int procfs_docpuinfo __P((struct proc *, struct proc *, struct pfsnode *,
    struct uio *));

int procfs_checkioperm __P((struct proc *, struct proc *));
void procfs_revoke_vnodes __P((struct proc *, void *));
void procfs_hashinit __P((void));
void procfs_hashdone __P((void));

/* functions to check whether or not files should be displayed */
int procfs_validfile __P((struct lwp *, struct mount *));
int procfs_validfpregs __P((struct lwp *, struct mount *));
int procfs_validregs __P((struct lwp *, struct mount *));
int procfs_validmap __P((struct lwp *, struct mount *));

int procfs_rw __P((void *));

int procfs_getcpuinfstr __P((char *, int *));

#define PROCFS_LOCKED	0x01
#define PROCFS_WANT	0x02

extern int (**procfs_vnodeop_p) __P((void *));
extern struct vfsops procfs_vfsops;

int	procfs_root __P((struct mount *, struct vnode **));

#endif /* _KERNEL */
