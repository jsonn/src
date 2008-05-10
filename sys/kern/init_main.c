/*	$NetBSD: init_main.c,v 1.355.2.1 2008/05/10 23:49:02 wrstuden Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
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
 *	@(#)init_main.c	8.16 (Berkeley) 5/14/95
 */

/*
 * Copyright (c) 1995 Christopher G. Demetriou.  All rights reserved.
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
 *	@(#)init_main.c	8.16 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: init_main.c,v 1.355.2.1 2008/05/10 23:49:02 wrstuden Exp $");

#include "opt_ipsec.h"
#include "opt_ntp.h"
#include "opt_pipe.h"
#include "opt_posix.h"
#include "opt_syscall_debug.h"
#include "opt_sysv.h"
#include "opt_fileassoc.h"
#include "opt_ktrace.h"
#include "opt_pax.h"

#include "rnd.h"
#include "sysmon_envsys.h"
#include "sysmon_power.h"
#include "sysmon_taskq.h"
#include "sysmon_wdog.h"
#include "veriexec.h"

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/fstrans.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/exec.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/percpu.h>
#include <sys/pset.h>
#include <sys/sysctl.h>
#include <sys/reboot.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <sys/event.h>
#include <sys/mbuf.h>
#include <sys/sched.h>
#include <sys/sleepq.h>
#include <sys/iostat.h>
#include <sys/vmem.h>
#include <sys/uuid.h>
#include <sys/extent.h>
#include <sys/disk.h>
#include <sys/mqueue.h>
#include <sys/msgbuf.h>
#include <sys/module.h>
#include <sys/event.h>
#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef P1003_1B_SEMAPHORE
#include <sys/ksem.h>
#endif
#include <sys/domain.h>
#include <sys/namei.h>
#if NRND > 0
#include <sys/rnd.h>
#endif
#include <sys/pipe.h>
#ifdef LKM
#include <sys/lkm.h>
#endif
#if NVERIEXEC > 0
#include <sys/verified_exec.h>
#endif /* NVERIEXEC > 0 */
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/kauth.h>
#include <net80211/ieee80211_netbsd.h>

#include <sys/syscall.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

#if defined(PAX_MPROTECT) || defined(PAX_SEGVGUARD) || defined(PAX_ASLR)
#include <sys/pax.h>
#endif /* PAX_MPROTECT || PAX_SEGVGUARD || PAX_ASLR */

#include <ufs/ufs/quota.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/syncfs/syncfs.h>

#include <sys/cpu.h>

#include <uvm/uvm.h>

#if NSYSMON_TASKQ > 0
#include <dev/sysmon/sysmon_taskq.h>
#endif

#include <dev/cons.h>

#if NSYSMON_ENVSYS > 0 || NSYSMON_POWER > 0 || NSYSMON_WDOG > 0
#include <dev/sysmon/sysmonvar.h>
#endif

#include <net/if.h>
#include <net/raw_cb.h>

#include <secmodel/secmodel.h>

extern struct proc proc0;
extern struct lwp lwp0;
extern struct cwdinfo cwdi0;
extern time_t rootfstime;

#ifndef curlwp
struct	lwp *curlwp = &lwp0;
#endif
struct	proc *initproc;

struct	vnode *rootvp, *swapdev_vp;
int	boothowto;
int	cold = 1;			/* still working on startup */
struct timeval boottime;	        /* time at system startup - will only follow settime deltas */

volatile int start_init_exec;		/* semaphore for start_init() */

static void check_console(struct lwp *l);
static void start_init(void *);
void main(void);
void ssp_init(void);

#if defined(__SSP__) || defined(__SSP_ALL__)
long __stack_chk_guard[8] = {0, 0, 0, 0, 0, 0, 0, 0};
void __stack_chk_fail(void);

void
__stack_chk_fail(void)
{
	panic("stack overflow detected; terminated");
}

void
ssp_init(void)
{
	int s;

#ifdef DIAGNOSTIC
	printf("Initializing SSP:");
#endif
	/*
	 * We initialize ssp here carefully:
	 *	1. after we got some entropy
	 *	2. without calling a function
	 */
	size_t i;
	long guard[__arraycount(__stack_chk_guard)];

	arc4randbytes(guard, sizeof(guard));
	s = splhigh();
	for (i = 0; i < __arraycount(guard); i++)
		__stack_chk_guard[i] = guard[i];
	splx(s);
#ifdef DIAGNOSTIC
	for (i = 0; i < __arraycount(guard); i++)
		printf("%lx ", guard[i]);
	printf("\n");
#endif
}
#else
void
ssp_init(void)
{

}
#endif

void __secmodel_none(void);
__weak_alias(secmodel_start,__secmodel_none);
void
__secmodel_none(void)
{
	return;
}

/*
 * System startup; initialize the world, create process 0, mount root
 * filesystem, and fork to create init and pagedaemon.  Most of the
 * hard work is done in the lower-level initialization routines including
 * startup(), which does memory initialization and autoconfiguration.
 */
void
main(void)
{
	struct timeval time;
	struct lwp *l;
	struct proc *p;
	int s, error;
#ifdef NVNODE_IMPLICIT
	int usevnodes;
#endif
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	l = &lwp0;
#ifndef LWP0_CPU_INFO
	l->l_cpu = curcpu();
#endif

	/*
	 * Attempt to find console and initialize
	 * in case of early panic or other messages.
	 */
	consinit();

	kernel_lock_init();

	uvm_init();

	kmem_init();

	percpu_init();

	/* Initialize lock caches. */
	mutex_obj_init();

	/* Initialize the extent manager. */
	extent_init();

	/* Do machine-dependent initialization. */
	cpu_startup();

	/* Initialize callouts, part 1. */
	callout_startup();

	/*
	 * Initialize the kernel authorization subsystem and start the
	 * default security model, if any. We need to do this early
	 * enough so that subsystems relying on any of the aforementioned
	 * can work properly. Since the security model may dictate the
	 * credential inheritance policy, it is needed at least before
	 * any process is created, specifically proc0.
	 */
	kauth_init();
	secmodel_start();

	/* Initialize the buffer cache */
	bufinit();

	/* Initialize sockets. */
	soinit();

	/*
	 * The following things must be done before autoconfiguration.
	 */
	evcnt_init();		/* initialize event counters */
#if NRND > 0
	rnd_init();		/* initialize RNG */
#endif

	/* Initialize process and pgrp structures. */
	procinit();
	lwpinit();

	/* Initialize signal-related data structures. */
	signal_init();

	/* Initialize resource management. */
	resource_init();

	/* Create process 0 (the swapper). */
	proc0_init();

	/* Initialize the UID hash table. */
	uid_init();

	/* Charge root for one process. */
	(void)chgproccnt(0, 1);

	/* Initialize timekeeping. */
	time_init();

	/* Initialize the run queues, turnstiles and sleep queues. */
	mutex_init(&cpu_lock, MUTEX_DEFAULT, IPL_NONE);
	sched_rqinit();
	turnstile_init();
	sleeptab_init(&sleeptab);

	/* Initialize processor-sets */
	psets_init();

	/* MI initialization of the boot cpu */
	error = mi_cpu_attach(curcpu());
	KASSERT(error == 0);

	/* Initialize timekeeping, part 2. */
	time_init2();

	/*
	 * Initialize mbuf's.  Do this now because we might attempt to
	 * allocate mbufs or mbuf clusters during autoconfiguration.
	 */
	mbinit();

	/* Initialize the sysctl subsystem. */
	sysctl_init();

	/* Initialize I/O statistics. */
	iostat_init();

	/* Initialize the log device. */
	loginit();

	/* Start module system. */
	module_init();

	/* Initialize the file systems. */
#ifdef NVNODE_IMPLICIT
	/*
	 * If maximum number of vnodes in namei vnode cache is not explicitly
	 * defined in kernel config, adjust the number such as we use roughly
	 * 1.0% of memory for vnode cache (but not less than NVNODE vnodes).
	 */
	usevnodes =
	    calc_cache_size(kernel_map, 1, VNODE_VA_MAXPCT) / sizeof(vnode_t);
	if (usevnodes > desiredvnodes)
		desiredvnodes = usevnodes;
#endif
	vfsinit();

	/* Initialize fstrans. */
	fstrans_init();

	/* Initialize the file descriptor system. */
	fd_sys_init();

	/* Initialize kqueue. */
	kqueue_init();

	/* Initialize asynchronous I/O. */
	aio_sysinit();

	/* Initialize message queues. */
	mqueue_sysinit();

	/* Initialize the system monitor subsystems. */
#if NSYSMON_TASKQ > 0
	sysmon_task_queue_preinit();
#endif

#if NSYSMON_ENVSYS > 0
	sysmon_envsys_init();
#endif

#if NSYSMON_POWER > 0
	sysmon_power_init();
#endif

#if NSYSMON_WDOG > 0
	sysmon_wdog_init();
#endif

	inittimecounter();
	ntp_init();

	/* Initialize the device switch tables. */
	devsw_init();

	/* Initialize tty subsystem. */
	tty_init();
	ttyldisc_init();

	/* Initialize the buffer cache, part 2. */
	bufinit2();

	/* Initialize the disk wedge subsystem. */
	dkwedge_init();

	/* Configure the system hardware.  This will enable interrupts. */
	configure();

	ubc_init();		/* must be after autoconfig */

#ifdef SYSVSHM
	/* Initialize System V style shared memory. */
	shminit();
#endif

#ifdef SYSVSEM
	/* Initialize System V style semaphores. */
	seminit();
#endif

#ifdef SYSVMSG
	/* Initialize System V style message queues. */
	msginit();
#endif

#ifdef P1003_1B_SEMAPHORE
	/* Initialize posix semaphores */
	ksem_init();
#endif

#if NVERIEXEC > 0
	/*
	 * Initialise the Veriexec subsystem.
	 */
	veriexec_init();
#endif /* NVERIEXEC > 0 */

#if defined(PAX_MPROTECT) || defined(PAX_SEGVGUARD) || defined(PAX_ASLR)
	pax_init();
#endif /* PAX_MPROTECT || PAX_SEGVGUARD || PAX_ASLR */

#ifdef	FAST_IPSEC
	/* Attach network crypto subsystem */
	ipsec_attach();
#endif

	/*
	 * Initialize protocols.  Block reception of incoming packets
	 * until everything is ready.
	 */
	s = splnet();
	ifinit();
	domaininit();
	if_attachdomain();
	splx(s);

#ifdef GPROF
	/* Initialize kernel profiling. */
	kmstartup();
#endif

	/* Initialize system accounting. */
	acct_init();

#ifndef PIPE_SOCKETPAIR
	/* Initialize pipes. */
	pipe_init();
#endif

#ifdef KTRACE
	/* Initialize ktrace. */
	ktrinit();
#endif

	/* Initialize the UUID system calls. */
	uuid_init();

	/*
	 * Create process 1 (init(8)).  We do this now, as Unix has
	 * historically had init be process 1, and changing this would
	 * probably upset a lot of people.
	 *
	 * Note that process 1 won't immediately exec init(8), but will
	 * wait for us to inform it that the root file system has been
	 * mounted.
	 */
	if (fork1(l, 0, SIGCHLD, NULL, 0, start_init, NULL, NULL, &initproc))
		panic("fork init");

	/*
	 * Load any remaining builtin modules, and hand back temporary
	 * storage to the VM system.
	 */
	module_init_class(MODULE_CLASS_ANY);
	module_jettison();

	/*
	 * Finalize configuration now that all real devices have been
	 * found.  This needs to be done before the root device is
	 * selected, since finalization may create the root device.
	 */
	config_finalize();

	/*
	 * Now that autoconfiguration has completed, we can determine
	 * the root and dump devices.
	 */
	cpu_rootconf();
	cpu_dumpconf();

	/* Mount the root file system. */
	do {
		domountroothook();
		if ((error = vfs_mountroot())) {
			printf("cannot mount root, error = %d\n", error);
			boothowto |= RB_ASKNAME;
			setroot(root_device,
			    (rootdev != NODEV) ? DISKPART(rootdev) : 0);
		}
	} while (error != 0);
	mountroothook_destroy();

	/*
	 * Initialise the time-of-day clock, passing the time recorded
	 * in the root filesystem (if any) for use by systems that
	 * don't have a non-volatile time-of-day device.
	 */
	inittodr(rootfstime);

	CIRCLEQ_FIRST(&mountlist)->mnt_flag |= MNT_ROOTFS;
	CIRCLEQ_FIRST(&mountlist)->mnt_op->vfs_refcount++;

	/*
	 * Get the vnode for '/'.  Set filedesc0.fd_fd.fd_cdir to
	 * reference it.
	 */
	error = VFS_ROOT(CIRCLEQ_FIRST(&mountlist), &rootvnode);
	if (error)
		panic("cannot find root vnode, error=%d", error);
	cwdi0.cwdi_cdir = rootvnode;
	VREF(cwdi0.cwdi_cdir);
	VOP_UNLOCK(rootvnode, 0);
	cwdi0.cwdi_rdir = NULL;

	/*
	 * Now that root is mounted, we can fixup initproc's CWD
	 * info.  All other processes are kthreads, which merely
	 * share proc0's CWD info.
	 */
	initproc->p_cwdi->cwdi_cdir = rootvnode;
	VREF(initproc->p_cwdi->cwdi_cdir);
	initproc->p_cwdi->cwdi_rdir = NULL;

	/*
	 * Now can look at time, having had a chance to verify the time
	 * from the file system.  Reset l->l_rtime as it may have been
	 * munched in mi_switch() after the time got set.
	 */
	getmicrotime(&time);
	boottime = time;
	mutex_enter(proc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		KASSERT((p->p_flag & PK_MARKER) == 0);
		mutex_enter(p->p_lock);
		p->p_stats->p_start = time;
		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			lwp_lock(l);
			memset(&l->l_rtime, 0, sizeof(l->l_rtime));
			lwp_unlock(l);
		}
		mutex_exit(p->p_lock);
	}
	mutex_exit(proc_lock);
	binuptime(&curlwp->l_stime);

	for (CPU_INFO_FOREACH(cii, ci)) {
		ci->ci_schedstate.spc_lastmod = time_second;
	}

	/* Create the pageout daemon kernel thread. */
	uvm_swap_init();
	if (kthread_create(PRI_PGDAEMON, KTHREAD_MPSAFE, NULL, uvm_pageout,
	    NULL, NULL, "pgdaemon"))
		panic("fork pagedaemon");

	/* Create the filesystem syncer kernel thread. */
	if (kthread_create(PRI_IOFLUSH, KTHREAD_MPSAFE, NULL, sched_sync,
	    NULL, NULL, "ioflush"))
		panic("fork syncer");

	/* Create the aiodone daemon kernel thread. */
	if (workqueue_create(&uvm.aiodone_queue, "aiodoned",
	    uvm_aiodone_worker, NULL, PRI_VM, IPL_NONE, WQ_MPSAFE))
		panic("fork aiodoned");

	vmem_rehash_start();

	/* Initialize exec structures */
	exec_init(1);

	/*
	 * Okay, now we can let init(8) exec!  It's off to userland!
	 */
	start_init_exec = 1;
	wakeup(&start_init_exec);

	/* The scheduler is an infinite loop. */
	uvm_scheduler();
	/* NOTREACHED */
}

static void
check_console(struct lwp *l)
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, "/dev/console");
	error = namei(&nd);
	if (error == 0)
		vrele(nd.ni_vp);
	else if (error == ENOENT)
		printf("warning: no /dev/console\n");
	else
		printf("warning: lookup /dev/console: error %d\n", error);
}

/*
 * List of paths to try when searching for "init".
 */
static const char * const initpaths[] = {
	"/sbin/init",
	"/sbin/oinit",
	"/sbin/init.bak",
	NULL,
};

/*
 * Start the initial user process; try exec'ing each pathname in "initpaths".
 * The program is invoked with one argument containing the boot flags.
 */
static void
start_init(void *arg)
{
	struct lwp *l = arg;
	struct proc *p = l->l_proc;
	vaddr_t addr;
	struct sys_execve_args /* {
		syscallarg(const char *) path;
		syscallarg(char * const *) argp;
		syscallarg(char * const *) envp;
	} */ args;
	int options, i, error;
	register_t retval[2];
	char flags[4], *flagsp;
	const char *path, *slash;
	char *ucp, **uap, *arg0, *arg1 = NULL;
	char ipath[129];
	int ipx, len;

	/*
	 * Now in process 1.
	 */
	strncpy(p->p_comm, "init", MAXCOMLEN);

	/*
	 * Wait for main() to tell us that it's safe to exec.
	 */
	while (start_init_exec == 0)
		(void) tsleep(&start_init_exec, PWAIT, "initexec", 0);

	/*
	 * This is not the right way to do this.  We really should
	 * hand-craft a descriptor onto /dev/console to hand to init,
	 * but that's a _lot_ more work, and the benefit from this easy
	 * hack makes up for the "good is the enemy of the best" effect.
	 */
	check_console(l);

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
	addr = (vaddr_t)STACK_ALLOC(USRSTACK, PAGE_SIZE);
	if (uvm_map(&p->p_vmspace->vm_map, &addr, PAGE_SIZE,
                    NULL, UVM_UNKNOWN_OFFSET, 0,
                    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
		    UVM_ADV_NORMAL,
                    UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW)) != 0)
		panic("init: couldn't allocate argument space");
	p->p_vmspace->vm_maxsaddr = (void *)STACK_MAX(addr, PAGE_SIZE);

	ipx = 0;
	while (1) {
		if (boothowto & RB_ASKNAME) {
			printf("init path");
			if (initpaths[ipx])
				printf(" (default %s)", initpaths[ipx]);
			printf(": ");
			len = cngetsn(ipath, sizeof(ipath)-1);
			if (len == 0) {
				if (initpaths[ipx])
					path = initpaths[ipx++];
				else
					continue;
			} else {
				ipath[len] = '\0';
				path = ipath;
			}
		} else {
			if ((path = initpaths[ipx++]) == NULL)
				break;
		}

		ucp = (char *)USRSTACK;

		/*
		 * Construct the boot flag argument.
		 */
		flagsp = flags;
		*flagsp++ = '-';
		options = 0;

		if (boothowto & RB_SINGLE) {
			*flagsp++ = 's';
			options = 1;
		}
#ifdef notyet
		if (boothowto & RB_FASTBOOT) {
			*flagsp++ = 'f';
			options = 1;
		}
#endif

		/*
		 * Move out the flags (arg 1), if necessary.
		 */
		if (options != 0) {
			*flagsp++ = '\0';
			i = flagsp - flags;
#ifdef DEBUG
			printf("init: copying out flags `%s' %d\n", flags, i);
#endif
			arg1 = STACK_ALLOC(ucp, i);
			ucp = STACK_MAX(arg1, i);
			(void)copyout((void *)flags, arg1, i);
		}

		/*
		 * Move out the file name (also arg 0).
		 */
		i = strlen(path) + 1;
#ifdef DEBUG
		printf("init: copying out path `%s' %d\n", path, i);
#else
		if (boothowto & RB_ASKNAME || path != initpaths[0])
			printf("init: trying %s\n", path);
#endif
		arg0 = STACK_ALLOC(ucp, i);
		ucp = STACK_MAX(arg0, i);
		(void)copyout(path, arg0, i);

		/*
		 * Move out the arg pointers.
		 */
		ucp = (void *)STACK_ALIGN(ucp, ALIGNBYTES);
		uap = (char **)STACK_ALLOC(ucp, sizeof(char *) * 3);
		SCARG(&args, path) = arg0;
		SCARG(&args, argp) = uap;
		SCARG(&args, envp) = NULL;
		slash = strrchr(path, '/');
		if (slash)
			(void)suword((void *)uap++,
			    (long)arg0 + (slash + 1 - path));
		else
			(void)suword((void *)uap++, (long)arg0);
		if (options != 0)
			(void)suword((void *)uap++, (long)arg1);
		(void)suword((void *)uap++, 0);	/* terminator */

		/*
		 * Now try to exec the program.  If can't for any reason
		 * other than it doesn't exist, complain.
		 */
		error = sys_execve(l, &args, retval);
		if (error == 0 || error == EJUSTRETURN) {
			KERNEL_UNLOCK_LAST(l);
			return;
		}
		printf("exec %s: error %d\n", path, error);
	}
	printf("init: not found\n");
	panic("no init");
}

/*
 * calculate cache size from physmem and vm_map size.
 */
vaddr_t
calc_cache_size(struct vm_map *map, int pct, int va_pct)
{
	paddr_t t;

	/* XXX should consider competing cache if any */
	/* XXX should consider submaps */
	t = (uintmax_t)physmem * pct / 100 * PAGE_SIZE;
	if (map != NULL) {
		vsize_t vsize;

		vsize = vm_map_max(map) - vm_map_min(map);
		vsize = (uintmax_t)vsize * va_pct / 100;
		if (t > vsize) {
			t = vsize;
		}
	}
	return t;
}
