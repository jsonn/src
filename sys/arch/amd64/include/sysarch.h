/*	$NetBSD: sysarch.h,v 1.3.2.1 2006/06/21 14:48:25 yamt Exp $	*/

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

struct x86_64_get_ldt_args {
	void *desc;
	unsigned len;
};

struct x86_64_set_ldt_args {
	void *desc;
	unsigned len;
};

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

#else
/* Silence the build */
struct x86_64_pmc_info_args;
struct x86_64_pmc_startstop_args;
struct x86_64_pmc_read_args;

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
int x86_64_iopl(struct lwp *, void *, register_t *);
int x86_64_get_mtrr(struct lwp *, void *, register_t *);
int x86_64_set_mtrr(struct lwp *, void *, register_t *);
#else
#include <sys/cdefs.h>

__BEGIN_DECLS
int x86_64_get_ldt(void *, int);
int x86_64_set_ldt(void *, int);
int x86_64_iopl(int);
int x86_64_get_ioperm(u_long *);
int x86_64_set_ioperm(u_long *);
int x86_64_pmc_info(struct x86_64_pmc_info_args *);
int x86_64_pmc_startstop(struct x86_64_pmc_startstop_args *);
int x86_64_pmc_read(struct x86_64_pmc_read_args *);
int x86_64_set_mtrr(struct mtrr *, int *);
int x86_64_get_mtrr(struct mtrr *, int *);
int sysarch(int, void *);
__END_DECLS
#endif

#endif /* !_AMD64_SYSARCH_H_ */
