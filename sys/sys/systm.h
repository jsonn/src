/*	$NetBSD: systm.h,v 1.63.2.1 1997/01/14 21:27:28 thorpej Exp $	*/

/*-
 * Copyright (c) 1982, 1988, 1991, 1993
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
 *	@(#)systm.h	8.4 (Berkeley) 2/23/94
 */

/*
 * The `securelevel' variable controls the security level of the system.
 * It can only be decreased by process 1 (/sbin/init).
 *
 * Security levels are as follows:
 *   -1	permannently insecure mode - always run system in level 0 mode.
 *    0	insecure mode - immutable and append-only flags make be turned off.
 *	All devices may be read or written subject to permission modes.
 *    1	secure mode - immutable and append-only flags may not be changed;
 *	raw disks of mounted filesystems, /dev/mem, and /dev/kmem are
 *	read-only.
 *    2	highly secure mode - same as (1) plus raw disks are always
 *	read-only whether mounted or not. This level precludes tampering
 *	with filesystems by unmounting them, but also inhibits running
 *	newfs while the system is secured.
 *
 * In normal operation, the system runs in level 0 mode while single user
 * and in level 1 mode while multiuser. If level 2 mode is desired while
 * running multiuser, it can be set in the multiuser startup script
 * (/etc/rc.local) using sysctl(1). If it is desired to run the system
 * in level 0 mode while multiuser, initialize the variable securelevel
 * in /sys/kern/kern_sysctl.c to -1. Note that it is NOT initialized to
 * zero as that would allow the vmunix binary to be patched to -1.
 * Without initialization, securelevel loads in the BSS area which only
 * comes into existence when the kernel is loaded and hence cannot be
 * patched by a stalking hacker.
 */

#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

struct device;
struct proc;
struct uio;
struct tty;
struct vnode;

extern int securelevel;		/* system security level */
extern const char *panicstr;	/* panic message */
extern char version[];		/* system version */
extern char copyright[];	/* system copyright */

extern int autonicetime;        /* time (in seconds) before autoniceval */
extern int autoniceval;         /* proc priority after autonicetime */

extern int nblkdev;		/* number of entries in bdevsw */
extern int nchrdev;		/* number of entries in cdevsw */
extern int nswdev;		/* number of swap devices */
extern int nswap;		/* size of swap space */

extern int selwait;		/* select timeout address */

extern u_char curpriority;	/* priority of current process */

extern int maxmem;		/* max memory per process */
extern int physmem;		/* physical memory */

extern dev_t dumpdev;		/* dump device */
extern long dumplo;		/* offset into dumpdev */
extern int dumpsize;		/* size of dump in pages */

extern dev_t rootdev;		/* root device */
extern struct vnode *rootvp;	/* vnode equivalent to above */
extern struct device *root_device; /* device equivalent to above */
extern const char *rootspec;	/* how root device was specified */

extern dev_t swapdev;		/* swapping device */
extern struct vnode *swapdev_vp;/* vnode equivalent to above */

struct proc;
struct tty;
struct uio;

extern struct sysent {		/* system call table */
	short	sy_narg;	/* number of args */
	short	sy_argsize;	/* total size of arguments */
				/* implementing function */
	int	(*sy_call) __P((struct proc *, void *, register_t *));
} sysent[];
extern int nsysent;
#define	SCARG(p,k)	((p)->k.datum)	/* get arg from args pointer */

extern int boothowto;		/* reboot flags, from console subsystem */

extern void (*v_putc) __P((int)); /* Virtual console putc routine */

extern	void	_insque	__P((void *, void *));
extern	void	_remque	__P((void *));

/* casts to keep lint happy, but it should be happy with void * */
#define	insque(q,p)	_insque(q, p)
#define	remque(q)	_remque(q)

/*
 * General function declarations.
 */
int	nullop __P((void *));
int	enodev __P((void));
int	enosys __P((void));
int	enoioctl __P((void));
int	enxio __P((void));
int	eopnotsupp __P((void));

#ifdef LKM
int	lkmenodev __P((void));
#endif

int	seltrue __P((dev_t dev, int events, struct proc *p));
void	*hashinit __P((int count, int type, u_long *hashmask));
int	sys_nosys __P((struct proc *, void *, register_t *));


void	printf __P((const char *, ...))
    __kprintf_attribute__((__format__(__kprintf__,1,2)));
int	sprintf __P((char *buf, const char *, ...))
    __attribute__((__format__(__printf__,2,3)));

void	panic __P((const char *, ...))
#ifdef __KPRINTF_ATTRIBUTE__
    __kprintf_attribute__((__noreturn__,__format__(__kprintf__,1,2)));
#else
    __attribute__((__noreturn__));
#endif
void	uprintf __P((const char *, ...))
    __kprintf_attribute__((__format__(__kprintf__,1,2)));
void	ttyprintf __P((struct tty *, const char *, ...))
    __kprintf_attribute__((__format__(__kprintf__,2,3)));

char	*bitmask_snprintf __P((u_long, const char *, char *, size_t));

void	tablefull __P((const char *));

void	bcopy __P((const void *, void *, size_t));
void	ovbcopy __P((const void *, void *, size_t));
void	bzero __P((void *, size_t));
int	bcmp __P((const void *, const void *, size_t));

int      memcmp __P((const void *, const void *, size_t));
void    *memcpy __P((void *, const void *, size_t));
void    *memmove __P((void *, const void *, size_t));
void    *memset __P((void *, int, size_t));

#if 0		/* XXX not ready for this yet */
#define bcopy(s,t,n)	memcpy(t,s,n)
#define ovbcopy(s,t,n)	memmove(t,s,n)
#define bzero(s,n)	memset(s,0,n)
#define bcmp(s,t,n)	memcmp(s,t,n)
#endif

int	copystr __P((const void *, void *, size_t, size_t *));
int	copyinstr __P((const void *, void *, size_t, size_t *));
int	copyoutstr __P((const void *, void *, size_t, size_t *));
int	copyin __P((const void *, void *, size_t));
int	copyout __P((const void *, void *, size_t));

int	subyte __P((void *, int));
int	suibyte __P((void *, int));
int	susword __P((void *, short));
int	suisword __P((void *, short));
int	suswintr __P((void *, short));
int	suword __P((void *, long));
int	suiword __P((void *, long));

int	fubyte __P((const void *));
int	fuibyte __P((const void *));
int	fusword __P((const void *));
int	fuisword __P((const void *));
int	fuswintr __P((const void *));
long	fuword __P((const void *));
long	fuiword __P((const void *));

struct timeval;
int	hzto __P((struct timeval *tv));
void	timeout __P((void (*func)(void *), void *arg, int ticks));
void	untimeout __P((void (*func)(void *), void *arg));
void	realitexpire __P((void *));

struct clockframe;
void	hardclock __P((struct clockframe *frame));
void	softclock __P((void));
void	statclock __P((struct clockframe *frame));
#ifdef NTP
void	hardupdate __P((long offset));
#endif

void	initclocks __P((void));
void	inittodr __P((time_t));
void	resettodr __P((void));
void	cpu_initclocks __P((void));

void	startprofclock __P((struct proc *));
void	stopprofclock __P((struct proc *));
void	setstatclockrate __P((int hzrate));

/*
 * Shutdown hooks.  Functions to be run with all interrupts disabled
 * immediately before the system is halted or rebooted.
 */
void	*shutdownhook_establish __P((void (*)(void *), void *));
void	shutdownhook_disestablish __P((void *));
void	doshutdownhooks __P((void));

/*
 * Mountroot hooks.  Device drivers establish these to be executed
 * just before (*mountroot)() if the passed device is selected
 * as the root device.
 */
void	*mountroothook_establish __P((void (*)(struct device *),
	    struct device *));
void	mountroothook_disestablish __P((void *));
void	mountroothook_destroy __P((void));
void	domountroothook __P((void));

int	uiomove __P((void *, int, struct uio *));

int	setjmp	__P((label_t *));
void	longjmp	__P((label_t *));

void	consinit __P((void));

void	cpu_startup __P((void));
void	cpu_set_kpc __P((struct proc *, void (*)(struct proc *)));


#ifdef GPROF
void	kmstartup __P((void));
#endif

#ifdef _KERNEL
#include <lib/libkern/libkern.h>
#endif

#ifdef DDB
/* debugger entry points */
void	Debugger __P((void));	/* in DDB only */
int	read_symtab_from_file __P((struct proc *,struct vnode *,const char *));
extern	int db_onpanic;
#endif	/* DDB */

#endif	/* !_SYS_SYSTM_H_ */
