/*	$NetBSD: lkm.h,v 1.17.8.1 2000/11/20 18:11:31 bouyer Exp $	*/

/*
 * Header file used by loadable kernel modules and loadable kernel module
 * utilities.
 *
 * 23 Jan 93	Terry Lambert		Original
 *
 * Copyright (c) 1992 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_LKM_H_
#define _SYS_LKM_H_

/*
 * Supported module types
 */
typedef enum loadmod {
	LM_SYSCALL,
	LM_VFS,
	LM_DEV,
	LM_STRMOD,
	LM_EXEC,
	LM_MISC
} MODTYPE;


#define	LKM_OLDVERSION	1		/* version of module loader */
#define	LKM_VERSION	1		/* version of module loader */
#define	MAXLKMNAME	32

/****************************************************************************/

#ifdef _KERNEL

/*
 * Loadable system call
 */
struct lkm_syscall {
	MODTYPE	lkm_type;
	int	lkm_ver;
	const char *lkm_name;
	u_long	lkm_offset;		/* save/assign area */
	struct sysent	*lkm_sysent;
	struct sysent	lkm_oldent;	/* save area for unload */
};

/*
 * Loadable file system
 */
struct lkm_vfs {
	MODTYPE	lkm_type;
	int	lkm_ver;
	const char *lkm_name;
	u_long	lkm_offset;
	struct vfsops	*lkm_vfsops;
};

/*
 * Supported device module types
 */
typedef enum devtype {
	LM_DT_BLOCK,
	LM_DT_CHAR
} DEVTYPE;

/*
 * Loadable device driver
 */
struct lkm_dev {
	MODTYPE	lkm_type;
	int	lkm_ver;
	const char *lkm_name;
	u_long	lkm_offset;
	DEVTYPE	lkm_devtype;
	union {
		void	*anon;
		struct bdevsw	*bdev;
		struct cdevsw	*cdev;
	} lkm_dev;
	union {
		struct bdevsw	bdev;
		struct cdevsw	cdev;
	} lkm_olddev;
};

/*
 * Loadable streams module
 */
struct lkm_strmod {
	MODTYPE	lkm_type;
	int	lkm_ver;
	const char *lkm_name;
	u_long	lkm_offset;
	/*
	 * Removed: future release
	 */
};

/*
 * Exec loader
 */
struct lkm_exec {
	MODTYPE	lkm_type;
	int	lkm_ver;
	const char *lkm_name;
	u_long	lkm_offset;
	struct execsw	*lkm_exec;
	struct execsw	lkm_oldexec;
};

/*
 * Miscellaneous module (complex load/unload, potentially complex stat
 */
struct lkm_misc {
	MODTYPE	lkm_type;
	int	lkm_ver;
	char	*lkm_name;
	u_long	lkm_offset;
};

/*
 * Any module (to get type and name info without knowing type)
 */
struct lkm_any {
	MODTYPE	lkm_type;
	int	lkm_ver;
	char	*lkm_name;
	u_long	lkm_offset;
};


/*
 * Generic reference ala XEvent to allow single entry point in the xxxinit()
 * routine.
 */
union lkm_generic {
	struct lkm_any		*lkm_any;
	struct lkm_syscall	*lkm_syscall;
	struct lkm_vfs		*lkm_vfs;
	struct lkm_dev		*lkm_dev;
	struct lkm_strmod	*lkm_strmod;
	struct lkm_exec		*lkm_exec;
	struct lkm_misc		*lkm_misc;
};

/*
 * Per module information structure
 */
struct lkm_table {
	int	type;
	u_long	size;
	u_long	offset;
	u_long	area;
	char	used;

	int	ver;		/* version (INIT) */
	int	refcnt;		/* reference count (INIT) */
	int	depcnt;		/* dependency count (INIT) */
	int	id;		/* identifier (INIT) */

	int	(*entry) __P((struct lkm_table *, int, int));/* entry function */
	union lkm_generic	private;	/* module private data */

				/* ddb support */
        u_long  syms;		/* start of symbol table */
	u_long	sym_size;	/* size of symbol table (syms+strings) */
	u_long	sym_offset;	/* offset of next symbol chunk */
	u_long	sym_symsize;	/* size of symbol part only */
};


#define	LKM_E_LOAD	1
#define	LKM_E_UNLOAD	2
#define	LKM_E_STAT	3


#define	MOD_SYSCALL(name,callslot,sysentp)	\
	static struct lkm_syscall _module = {	\
		LM_SYSCALL,			\
		LKM_VERSION,			\
		name,				\
		callslot,			\
		sysentp				\
	};

#define	MOD_VFS(name,vfsslot,vfsopsp)		\
	static struct lkm_vfs _module = {	\
		LM_VFS,				\
		LKM_VERSION,			\
		name,				\
		vfsslot,			\
		vfsopsp				\
	};

#define	MOD_DEV(name,devtype,devslot,devp)	\
	static struct lkm_dev _module = {	\
		LM_DEV,				\
		LKM_VERSION,			\
		name,				\
		devslot,			\
		devtype,			\
		{ (void *)devp }, 		\
	};

#define	MOD_EXEC(name,execslot,execsw)		\
	static struct lkm_exec _module = {	\
		LM_EXEC,			\
		LKM_VERSION,			\
		name,				\
		execslot,			\
		execsw				\
	};

#define	MOD_MISC(name)				\
	static struct lkm_misc _module = {	\
		LM_MISC,			\
		LKM_VERSION,			\
		name				\
	};


extern int	lkm_nofunc __P((struct lkm_table *lkmtp, int cmd));
extern int	lkmexists __P((struct lkm_table *));
extern int	lkmdispatch __P((struct lkm_table *, int));

/*
 * DISPATCH -- body function for use in module entry point function;
 * generally, the function body will consist entirely of a single
 * DISPATCH line.
 *
 * If load/unload/stat are called on each corresponding entry instance.
 * If no function is desired for load/stat/unload, lkm_nofunc() should
 * be specified.  "cmd" is passed to each function so that a single
 * function can be used if desired.
 */
#define	DISPATCH(lkmtp,cmd,ver,load,unload,stat)			\
	if (ver != LKM_VERSION)						\
		return EINVAL;	/* version mismatch */			\
	switch (cmd) {							\
	int	error;							\
	case LKM_E_LOAD:						\
		lkmtp->private.lkm_any = (struct lkm_any *)&_module;	\
		if ((error = load(lkmtp, cmd)) != 0)			\
			return error;					\
		break;							\
	case LKM_E_UNLOAD:						\
		if ((error = unload(lkmtp, cmd)) != 0)			\
			return error;					\
		break;							\
	case LKM_E_STAT:						\
		if ((error = stat(lkmtp, cmd)) != 0)			\
			return error;					\
		break;							\
	}								\
	return lkmdispatch(lkmtp, cmd);

#endif /* _KERNEL */

/****************************************************************************/

/*
 * IOCTL's recognized by /dev/lkm
 */
#define	LMRESERV_O	_IOWR('K', 0, struct lmc_oresrv)
#define	LMLOADBUF	_IOW('K', 1, struct lmc_loadbuf)
#define	LMUNRESRV	_IO('K', 2)
#define	LMREADY		_IOW('K', 3, int)
#define	LMRESERV	_IOWR('K', 4, struct lmc_resrv)

#define	LMLOAD		_IOW('K', 9, struct lmc_load)
#define	LMUNLOAD	_IOWR('K', 10, struct lmc_unload)
#define	LMSTAT		_IOWR('K', 11, struct lmc_stat)
#define	LMLOADSYMS	_IOW('K', 12, struct lmc_loadbuf)

#define	MODIOBUF	512		/* # of bytes at a time to loadbuf */

/*
 * IOCTL arguments
 */


/*
 * Reserve a page-aligned block of kernel memory for the module
 */
struct lmc_resrv {
	u_long	size;		/* IN: size of module to reserve */
	char	*name;		/* IN: name (must be provided */
	int	slot;		/* OUT: allocated slot (module ID) */
	u_long	addr;		/* OUT: Link-to address */
				/* ddb support */
	u_long  xxx_unused1;	/* unused */
	u_long	sym_size;	/* IN: total size of symbol table */
	u_long	xxx_unused2;	/* unused */
	u_long	sym_symsize;	/* IN: size of symbol portion of symtable */
	u_long	sym_addr;	/* OUT: address of symbol table */
};

/*
 * (Compat with old kernels)
 */
struct lmc_oresrv {
	u_long	size;		/* IN: size of module to reserve */
	char	*name;		/* IN: name (must be provided */
	int	slot;		/* OUT: allocated slot (module ID) */
	u_long	addr;		/* OUT: Link-to address */
};



/*
 * Copy a buffer at a time into the allocated area in the kernel; writes
 * are assumed to occur contiguously.
 */
struct lmc_loadbuf {
	int	cnt;		/* IN: # of chars pointed to by data */
	char	*data;		/* IN: pointer to data buffer */
};


/*
 * Load a module (assumes it's been mmapped to address before call)
 */
struct lmc_load {
	caddr_t	address;	/* IN: user space mmap address */
	int	status;		/* OUT: status of operation */
	int	id;		/* OUT: module ID if loaded */
};

/*
 * Unload a module (by name/id)
 */
struct lmc_unload {
	int	id;		/* IN: module ID to unload */
	char	*name;		/* IN: module name to unload if id -1 */
	int	status;		/* OUT: status of operation */
};


/*
 * Get module information for a given id (or name if id == -1).
 */
struct lmc_stat {
	int	id;			/* IN: module ID to unload */
	char	name[MAXLKMNAME];	/* IN/OUT: name of module */
	u_long	offset;			/* OUT: target table offset */
	MODTYPE	type;			/* OUT: type of module */
	u_long	area;			/* OUT: kernel load addr */
	u_long	size;			/* OUT: module size (pages) */
	u_long	private;		/* OUT: module private data */
	int	ver;			/* OUT: lkm compile version */
};

#endif	/* !_SYS_LKM_H_ */
