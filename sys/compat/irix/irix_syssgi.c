/*	$NetBSD: irix_syssgi.c,v 1.47.4.4 2010/08/11 22:53:03 yamt Exp $ */

/*-
 * Copyright (c) 2001, 2002, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
__KERNEL_RCSID(0, "$NetBSD: irix_syssgi.c,v 1.47.4.4 2010/08/11 22:53:03 yamt Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

#ifndef ELFSIZE
#ifdef _LP64
#define ELFSIZE 64
#else
#define ELFSIZE 32
#endif
#endif

/* round up and down to page boundaries. Borrowed from sys/kern/exec_elf32.c */
#define ELF_ROUND(a, b)         (((a) + (b) - 1) & ~((b) - 1))
#define ELF_TRUNC(a, b)         ((a) & ~((b) - 1))

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <compat/common/compat_util.h>

#include <compat/svr4/svr4_types.h>

#include <compat/irix/irix_types.h>
#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_exec.h>
#include <compat/irix/irix_prctl.h>
#include <compat/irix/irix_syscall.h>
#include <compat/irix/irix_syscallargs.h>
#include <compat/irix/irix_syssgi.h>

/* In sys/kern/exec_elf32.c */
void	ELFNAME(load_psection)(struct exec_vmcmd_set *, struct vnode *,
	    const Elf_Phdr *, Elf_Addr *, u_long *, int *, int);

static int irix_syssgi_mapelf(int, Elf_Phdr *, int,
    struct lwp *, register_t *);
static int irix_syssgi_sysconf(int name, struct lwp *, register_t *);
static int irix_syssgi_pathconf(char *, int, struct lwp *, register_t *);

int
irix_sys_syssgi(struct lwp *l, const struct irix_sys_syssgi_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) request;
		syscallarg(void *) arg1;
		syscallarg(void *) arg2;
		syscallarg(void *) arg3;
		syscallarg(void *) arg4;
		syscallarg(void *) arg5;
	} */
	struct proc *p = l->l_proc;
	int request = SCARG(uap, request);
	void *arg1, *arg2, *arg3;

#ifdef DEBUG_IRIX
	printf("irix_sys_syssgi(): request = %d\n", request);
#endif
	switch(request) {
	case IRIX_SGI_SYSID:	/* Get HostID */
		*retval = (register_t)hostid;
		break;

	case IRIX_SGI_SETGROUPS: {	/* setgroups(2) */
		struct sys_setgroups_args cup;

		SCARG(&cup, gidsetsize) = (intptr_t)SCARG(uap, arg1);
		SCARG(&cup, gidset) = (gid_t *)SCARG(uap, arg2);
		return (sys_setgroups(l, &cup, retval));
		break;
	}

	case IRIX_SGI_GETGROUPS: {	/* getgroups(2) */
		struct sys_getgroups_args cup;

		SCARG(&cup, gidsetsize) = (intptr_t)SCARG(uap, arg1);
		SCARG(&cup, gidset) = (gid_t *)SCARG(uap, arg2);
		return (sys_getgroups(l, &cup, retval));
		break;
	}

	case IRIX_SGI_SETSID: 	/* Set session ID: setsid(2) */
		return (sys_setsid(l, NULL, retval));
		break;

	case IRIX_SGI_GETSID: {	/* Get session ID: getsid(2) */
		struct sys_getsid_args cup;

		SCARG(&cup, pid) = (pid_t)(intptr_t)SCARG(uap, arg1);
		return (sys_getsid(l, &cup, retval));
		break;
	}

	case IRIX_SGI_GETPGID: {/* Get parent process GID: getpgid(2) */
		struct sys_getpgid_args cup;

		SCARG(&cup, pid) = (pid_t)(intptr_t)SCARG(uap, arg1);
		return (sys_getpgid(l, &cup, retval));
		break;
	}

	case IRIX_SGI_SETPGID: {/* Get parent process GID: setpgid(2) */
		struct sys_setpgid_args cup;

		SCARG(&cup, pid) = (pid_t)(intptr_t)SCARG(uap, arg1);
		SCARG(&cup, pgid) = (pid_t)(intptr_t)SCARG(uap, arg2);
		return (sys_setpgid(l, &cup, retval));
		break;
	}

	case IRIX_SGI_PATHCONF: /* Get file limits: pathconf(3) */
		return irix_syssgi_pathconf((char *)SCARG(uap, arg1),
		    (intptr_t)SCARG(uap, arg2), l, retval);
		break;

	case IRIX_SGI_RUSAGE: {	/* BSD getrusage(2) */
		struct compat_50_sys_getrusage_args cup;

		SCARG(&cup, who) = (intptr_t)SCARG(uap, arg1);
		SCARG(&cup, rusage) = (struct rusage50 *)SCARG(uap, arg2);
		return compat_50_sys_getrusage(l, &cup, retval);
	}

	case IRIX_SGI_NUM_MODULES: /* <sys/systeminfo.h> get_num_modules() */
		*retval = 1;
		return 0;
		break;

	case IRIX_SGI_MODULE_INFO: { /* <sys/systeminfo.h> get_module_info() */
		int module_num = (intptr_t)SCARG(uap, arg1);
		struct irix_module_info_s *imip = SCARG(uap, arg2);
		int mss = (intptr_t)SCARG(uap, arg3);
		struct irix_module_info_s imi;
		char *idx;

		if (module_num != 0)
			return EINVAL;

		imi.serial_num = (u_int64_t)hostid;
		imi.mod_num = 0;
		(void)snprintf(imi.serial_str, IRIX_MAX_SERIAL_SIZE,
		    "0800%08x", (u_int32_t)hostid);

		/* Convert to upper case */
		for (idx = imi.serial_str; *idx; idx++)
			if (*idx >= 'a' && *idx <= 'f')
				*idx += ('A' - 'a');

		/* Don't copyout irrelevant data on user request */
		if (mss > sizeof(struct irix_module_info_s))
			mss = sizeof(struct irix_module_info_s);

		return copyout(&imi, imip, mss);
		break;
	}

	case IRIX_SGI_RDNAME: {	/* Read Processes' name */
		struct proc *tp;

		arg1 = SCARG(uap, arg1); /* PID of the process */
		arg2 = SCARG(uap, arg2); /* Address of user buffer */
		arg3 = SCARG(uap, arg3); /* Length of user buffer */
		mutex_enter(proc_lock);
		tp = proc_find((pid_t)(intptr_t)arg1);
		if (tp == NULL ||
		    tp->p_psstr == NULL ||
		    tp->p_psstr->ps_argvstr == NULL ||
		    tp->p_psstr->ps_argvstr[0] == NULL) {
			mutex_exit(proc_lock);
			return 0;
		}
		mutex_exit(proc_lock);

		/* XXX: Unlocked! */
		*retval = (register_t)copyout(tp->p_psstr->ps_argvstr[0],
		    (void *)arg2, (size_t)arg3);
		break;
	}
	case IRIX_SGI_TUNE:	/* Tune system variables */
	case IRIX_SGI_IDBG:	/* Kernel debugging */
	case IRIX_SGI_INVENT:	/* Get system inventory */
	case IRIX_SGI_SETLED:	/* Turn on/off LEDs */
	case IRIX_SGI_SETNVRAM:	/* Sets a NVRAM variable value */
	case IRIX_SGI_GETNVRAM:	/* Gets a NVRAM variable value */
		return EINVAL;
		break;

	case IRIX_SGI_ELFMAP:	/* Maps an ELF image */
		 arg1 = SCARG(uap, arg1); /* file descriptor  */
		 arg2 = SCARG(uap, arg2); /* ptr to ELF program header array */
		 arg3 = SCARG(uap, arg3); /* array's length */
		return irix_syssgi_mapelf((intptr_t)arg1, (Elf_Phdr *)arg2,
		    (intptr_t)arg3, l, retval);
		break;

	case IRIX_SGI_USE_FP_BCOPY:	/* bcopy and bzero can use FPU or not */
		/*
		 * Our kernel does not use FPU, hence we do nothing.
		 */
		break;

	case IRIX_SGI_TOSSTSAVE:	/* Kill saved pregions */
		/*
		 * Our kernel does not seem to save such "pregions",
		 * therefore we do not have to do anything.
		 */
		break;

	case IRIX_SGI_SYSCONF:		/* POSIX sysconf */
		arg1 = SCARG(uap, arg1); /* system variable name */
		return irix_syssgi_sysconf((intptr_t)arg1, l, retval);
		break;

	case IRIX_SGI_SATCTL:		/* control audit stream */
	case IRIX_SGI_RXEV_GET:		/* Trusted IRIX call */
		/* Undocumented (?) and unimplemented */
		return 0;
		break;

	case IRIX_SGI_FDHI:	/* getdtablehi(3): get higher open fd + 1 */
		*retval = (register_t)(p->p_fd->fd_lastfile + 1);
		return 0;
		break;

	default:
		printf("Warning: call to unimplemented syssgi() command %d\n",
		    request);
		    return EINVAL;
		break;
	}

	return 0;
}

static int
irix_syssgi_mapelf(int fd, Elf_Phdr *ph, int count, struct lwp *l, register_t *retval)
{
	Elf_Phdr *kph;
	Elf_Phdr *pht;
	int error = 0;
	int i,j;
	int prot;
	int flags;
	u_long size;
	Elf_Addr uaddr;
	Elf_Addr reloc_offset;
	file_t *fp;
	struct exec_vmcmd_set vcset;
	struct exec_vmcmd *base_vcp = NULL;
	struct vnode *vp;
	struct vm_map_entry *ret;
	struct exec_vmcmd *vcp;
	int need_relocation;

	vcset.evs_cnt = 0;
	vcset.evs_used = 0;

	kph = (Elf_Phdr *)malloc(sizeof(Elf_Phdr) * count, M_TEMP, M_WAITOK);
	error = copyin(ph, kph, sizeof(Elf_Phdr) * count);
	if (error)
		goto bad;

	pht = kph;
	need_relocation = 0;
	for (i = 0; i < count; i++) {
	 	/* Check that each ELF sections is loadable */
		if (pht->p_type != PT_LOAD) {
			error = ENOEXEC;
			goto bad;
		}

	 	/*
		 * Check that the section load addresses are increasing
		 * with the section in the program header array. We do
		 * not support any other situation.
		 */
		if (pht->p_vaddr < kph->p_vaddr) {
#ifdef DEBUG_IRIX
			printf("mapelf: unsupported psection order\n");
#endif
			error = EINVAL;
			goto bad;
		}

	 	/*
		 * Check that the default load addresses are free.
		 * If not, we will have to perform a relocation
		 */
		ret = uvm_map_findspace(&l->l_proc->p_vmspace->vm_map,
		    pht->p_vaddr, pht->p_memsz, (vaddr_t *)(void *)&uaddr,
		    NULL, 0, 0, UVM_FLAG_FIXED);
		if (ret == NULL)
			need_relocation = 1;
		pht++;
	}

	/*
	 * Perform a relocation
	 */
	if (need_relocation) {
		/*
		 * compute the size needed by the section union. This
		 * assumes that the section load addresses are increasing.
		 * (And also that the sections are not overlapping)
		 */
		pht--;
		size = ELF_ROUND((pht->p_vaddr + pht->p_memsz), pht->p_align) -
		    ELF_TRUNC(kph->p_vaddr, kph->p_align);

		/* Find a free place for the sections */
		ret = uvm_map_findspace(&l->l_proc->p_vmspace->vm_map,
		    IRIX_MAPELF_RELOCATE, size, (vaddr_t *)(void *)&uaddr,
			NULL, 0, kph->p_align, 0);

		if (ret == NULL) {
			error = ENOMEM;
			goto bad;
		}

		/*
		 * Relocate the sections, all with the same offset.
		 */
		reloc_offset = uaddr - kph->p_vaddr;
		pht = kph;
		for (i = 0; i < count; i++) {
#ifdef DEBUG_IRIX
			printf("mapelf: relocating section %d from %p to %p\n",
			    i, (void *)pht->p_vaddr,
			    (void *)(pht->p_vaddr + reloc_offset));
#endif
			pht->p_vaddr += reloc_offset;
			pht++;
		}
	}

	/* Find the file's vnode */
	fp = fd_getfile(fd);
	if (fp == NULL) {
		error = EBADF;
		goto bad;
	}
	vp = fp->f_data;

        error = vn_marktext(vp);
        if (error)
                goto bad_unuse;

	/*
	 * Load the sections
	 */
	pht = kph;
	for (i = 0; i < count; i++) {
#ifdef DEBUG_IRIX
		printf("mapelf: loading section %d (len 0x%08lx) at %p\n",
		    i, (long)pht->p_memsz, (void *)pht->p_vaddr);
#endif
		/* Build the vmcmds for loading the section */
		kill_vmcmds(&vcset);
		uaddr = pht->p_vaddr;
		size = 0;
		prot = 0;
		flags = VMCMD_BASE;

		ELFNAME(load_psection)(&vcset, vp, pht, &uaddr,
		    &size, &prot, flags);

		/* Execute the vmcmds */
		for (j = 0; j < vcset.evs_used && !error; j++) {
			vcp = &vcset.evs_cmds[j];
			if (vcp->ev_flags & VMCMD_RELATIVE) {
				if (base_vcp == NULL)
					panic("irix_syssgi_mapelf():  bad vmcmd base");

				vcp->ev_addr += base_vcp->ev_addr;
			}
			IRIX_VM_SYNC(l->l_proc, error = (*vcp->ev_proc)(l, vcp));
			if (error)
				goto bad_unuse;
		}
		pht++;
	}

	*retval = (register_t)kph->p_vaddr;

bad_unuse:
	fd_putfile(fd);
bad:
	free(kph, M_TEMP);
	return error;
}



static int
irix_syssgi_sysconf(int name, struct lwp *l, register_t *retval)
{
	struct proc *p = l->l_proc;
	int error = 0;
	int mib[2], value;
	size_t len = sizeof(value);

	switch (name) {
	case IRIX_SC_ARG_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_ARGMAX;
		break;
	case IRIX_SC_CHILD_MAX:
		*retval = (register_t)p->p_rlimit[RLIMIT_NPROC].rlim_cur;
		return 0;
		break;
	case IRIX_SC_CLK_TCK:
		*retval = hz;
		return 0;
		break;
	case IRIX_SC_NGROUPS_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_NGROUPS;
		break;
	case IRIX_SC_OPEN_MAX:
		*retval = (register_t)p->p_rlimit[RLIMIT_NOFILE].rlim_cur;
		return 0;
		break;
	case IRIX_SC_JOB_CONTROL:
		mib[0] = CTL_KERN;
		mib[1] = KERN_JOB_CONTROL;
		break;
	case IRIX_SC_SAVED_IDS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SAVED_IDS;
		break;
	case IRIX_SC_IP_SECOPTS:/* IP security options */
	/* Trusted IRIX capabilities are unsupported */
	case IRIX_SC_ACL:	/* ACcess Lists */
	case IRIX_SC_AUDIT:	/* Audit */
	case IRIX_SC_INF:	/* Information labels */
	case IRIX_SC_MAC:	/* Mandatory Access Control */
	case IRIX_SC_CAP:	/* Capabilities */
		*retval = 0;
		return 0;
		break;
	case IRIX_SC_PAGESIZE:
		mib[0] = CTL_HW;
		mib[1] = HW_PAGESIZE;
		break;

	case IRIX_SC_PASS_MAX:
	case IRIX_SC_VERSION:
	default:
		printf("Warning: syssgi(SYSCONF) unsupported variable %d\n",
		    name);
		return EINVAL;
		break;
	}

	/*
	 * calling into sysctl with superuser privs, but we don't mind
	 * 'cause we're only querying a value.
	 */
	error = old_sysctl(&mib[0], 2, &value, &len, NULL, 0, NULL);
	if (error == 0)
		*retval = value;
	return (error);
}

static int
irix_syssgi_pathconf(char *path, int name, struct lwp *l, register_t *retval)
{
	struct sys_pathconf_args cup;
	int bname;

	switch (name) {
	case IRIX_PC_LINK_MAX:
	case IRIX_PC_MAX_CANON:
	case IRIX_PC_MAX_INPUT:
	case IRIX_PC_NAME_MAX:
	case IRIX_PC_PATH_MAX:
	case IRIX_PC_PIPE_BUF:
	case IRIX_PC_CHOWN_RESTRICTED:
	case IRIX_PC_NO_TRUNC:
	case IRIX_PC_VDISABLE:
	case IRIX_PC_SYNC_IO:
		bname = name;
		break;
	case IRIX_PC_FILESIZEBITS:
		bname = _PC_FILESIZEBITS;
		break;
	case IRIX_PC_PRIO_IO:
	case IRIX_PC_ASYNC_IO:
	case IRIX_PC_ABI_ASYNC_IO:
	case IRIX_PC_ABI_AIO_XFER_MAX:
	default:
		printf("Warning: unimplemented IRIX pathconf() command %d\n",
		    name);
		*retval = 0;
		return 0;
		break;
	}
	SCARG(&cup, path) = path;
	SCARG(&cup, name) = bname;
	return sys_pathconf(l, &cup, retval);
}
