/*	$NetBSD: kvm_private.h,v 1.10.2.1 2002/04/23 20:10:20 nathanw Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 *	@(#)kvm_private.h	8.1 (Berkeley) 6/4/93
 */

struct __kvm {
	/*
	 * a string to be prepended to error messages
	 * provided for compatibility with sun's interface
	 * if this value is null, errors are saved in errbuf[]
	 */
	const char *program;
	char	*errp;		/* XXX this can probably go away */
	char	errbuf[_POSIX2_LINE_MAX];
	DB	*db;
	int	pmfd;		/* physical memory file (or crashdump) */
	int	vmfd;		/* virtual memory file (-1 if crashdump) */
	int	swfd;		/* swap file (e.g., /dev/drum) */
	int	nlfd;		/* namelist file (e.g., /vmunix) */
	char	alive;		/* live kernel? */
	struct kinfo_proc *procbase;
	struct kinfo_proc2 *procbase2;
	struct kinfo_lwp *lwpbase;
	u_long	usrstack;		/* address of end of user stack */
	u_long	min_uva, max_uva;	/* min/max user virtual address */
	int	nbpg;		/* page size */
	char	*swapspc;	/* (dynamic) storage for swapped pages */
	char	*argspc, *argbuf; /* (dynamic) storage for argv strings */
	int	arglen;		/* length of the above */
	char	**argv;		/* (dynamic) storage for argv pointers */
	int	argc;		/* length of above (not actual # present) */

	/*
	 * Header structures for kernel dumps. Only gets filled in for
	 * dead kernels.
	 */
	struct kcore_hdr	*kcore_hdr;
	size_t	cpu_dsize;
	void	*cpu_data;
	off_t	dump_off;	/* Where the actual dump starts	*/

	/*
	 * Kernel virtual address translation state.  This only gets filled
	 * in for dead kernels; otherwise, the running kernel (i.e. kmem)
	 * will do the translations for us.  It could be big, so we
	 * only allocate it if necessary.
	 */
	struct vmstate *vmst; /* XXX: should become obsoleted */
	/*
	 * These kernel variables are used for looking up user addresses,
	 * and are cached for efficiency.
	 */
	struct pglist *vm_page_buckets;
	int vm_page_hash_mask;
};

/* Levels of aliveness */
#define	KVM_ALIVE_DEAD		0	/* dead, working from core file */
#define	KVM_ALIVE_FILES		1	/* alive, working from open kmem/drum */
#define	KVM_ALIVE_SYSCTL	2	/* alive, sysctl-type calls only */

#define	ISALIVE(kd)	((kd)->alive != KVM_ALIVE_DEAD)
#define	ISKMEM(kd)	((kd)->alive == KVM_ALIVE_FILES)
#define	ISSYSCTL(kd)	((kd)->alive == KVM_ALIVE_SYSCTL || ISKMEM(kd))

/*
 * Functions used internally by kvm, but across kvm modules.
 */
void	 _kvm_err __P((kvm_t *kd, const char *program, const char *fmt, ...))
	__attribute__((__format__(__printf__, 3, 4)));
int	 _kvm_dump_mkheader __P((kvm_t *kd_live, kvm_t *kd_dump));
void	 _kvm_freeprocs __P((kvm_t *kd));
void	 _kvm_freevtop __P((kvm_t *));
int	 _kvm_mdopen __P((kvm_t *));
int	 _kvm_initvtop __P((kvm_t *));
int	 _kvm_kvatop __P((kvm_t *, u_long, u_long *));
void	*_kvm_malloc __P((kvm_t *kd, size_t));
off_t	 _kvm_pa2off __P((kvm_t *, u_long));
void	*_kvm_realloc __P((kvm_t *kd, void *, size_t));
void	 _kvm_syserr
	    __P((kvm_t *kd, const char *program, const char *fmt, ...))
	    __attribute__((__format__(__printf__, 3, 4)));

