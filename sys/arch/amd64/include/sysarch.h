/*	$NetBSD: sysarch.h,v 1.1.2.1 2004/08/03 10:31:36 skrll Exp $	*/

#ifndef _AMD64_SYSARCH_H_
#define _AMD64_SYSARCH_H_

/*
 * Architecture specific syscalls (amd64)
 */
#define X86_64_GET_LDT	0
#define X86_64_SET_LDT	1
#define	X86_64_IOPL	2
#define	X86_64_GET_IOPERM	3
#define	X86_64_SET_IOPERM	4
#define	X86_64_VM86	5
#define	X86_64_PMC_INFO	8
#define	X86_64_PMC_STARTSTOP 9
#define	X86_64_PMC_READ	10
#define X86_64_GET_MTRR   11
#define X86_64_SET_MTRR   12

/*
 * XXXfvdl todo.
 */

#if 0

struct x86_64_get_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

struct x86_64_set_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

#endif

struct x86_64_iopl_args {
	int iopl;
};

#if 0

struct x86_64_get_ioperm_args {
	u_long *iomap;
};

struct x86_64_set_ioperm_args {
	u_long *iomap;
};

struct x86_64_pmc_info_args {
	int	type;
	int	flags;
};

#define	PMC_TYPE_NONE		0
#define	PMC_TYPE_I586		1
#define	PMC_TYPE_I686		2

#define	PMC_INFO_HASTSC		0x01

#define	PMC_NCOUNTERS		2

struct x86_64_pmc_startstop_args {
	int counter;
	u_int64_t val;
	u_int8_t event;
	u_int8_t unit;
	u_int8_t compare;
	u_int8_t flags;
};

#define	PMC_SETUP_KERNEL	0x01
#define	PMC_SETUP_USER		0x02
#define	PMC_SETUP_EDGE		0x04
#define	PMC_SETUP_INV		0x08

struct x86_64_pmc_read_args {
	int counter;
	u_int64_t val;
	u_int64_t time;
};

#endif /* todo */

struct x86_64_get_mtrr_args {
	struct mtrr *mtrrp;
	int *n;
};

struct x86_64_set_mtrr_args {
	struct mtrr *mtrrp;
	int *n;
};


#ifdef _KERNEL
int x86_64_iopl __P((struct lwp *, void *, register_t *));
int x86_64_get_mtrr __P((struct lwp *, void *, register_t *));
int x86_64_set_mtrr __P((struct lwp *, void *, register_t *));
#else
#include <sys/cdefs.h>

__BEGIN_DECLS
int x86_64_get_ldt __P((int, union descriptor *, int));
int x86_64_set_ldt __P((int, union descriptor *, int));
int x86_64_iopl __P((int));
int x86_64_get_ioperm __P((u_long *));
int x86_64_set_ioperm __P((u_long *));
int x86_64_pmc_info __P((struct x86_64_pmc_info_args *));
int x86_64_pmc_startstop __P((struct x86_64_pmc_startstop_args *));
int x86_64_pmc_read __P((struct x86_64_pmc_read_args *));
int x86_64_set_mtrr __P((struct mtrr *, int *));
int x86_64_get_mtrr __P((struct mtrr *, int *));
int sysarch __P((int, void *));
__END_DECLS
#endif

#endif /* !_AMD64_SYSARCH_H_ */
