/*	$NetBSD: sysarch.h,v 1.16.12.1 2006/05/24 15:47:58 tron Exp $	*/

#ifndef _I386_SYSARCH_H_
#define _I386_SYSARCH_H_

/*
 * Architecture specific syscalls (i386)
 */
#define I386_GET_LDT	0
#define I386_SET_LDT	1
#define	I386_IOPL	2
#define	I386_GET_IOPERM	3
#define	I386_SET_IOPERM	4
#define	I386_OLD_VM86	5
#define	I386_PMC_INFO	8
#define	I386_PMC_STARTSTOP 9
#define	I386_PMC_READ	10
#define I386_GET_MTRR	11
#define I386_SET_MTRR	12
#define	I386_VM86	13

struct i386_get_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

struct i386_set_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

struct i386_get_mtrr_args {
	struct mtrr *mtrrp;
	int *n;
};

struct i386_set_mtrr_args {
	struct mtrr *mtrrp;
	int *n;
};

struct i386_iopl_args {
	int iopl;
};

struct i386_get_ioperm_args {
	u_long *iomap;
};

struct i386_set_ioperm_args {
	u_long *iomap;
};

struct i386_pmc_info_args {
	int	type;
	int	flags;
};

#define	PMC_TYPE_NONE		0
#define	PMC_TYPE_I586		1
#define	PMC_TYPE_I686		2
#define	PMC_TYPE_K7		3

#define	PMC_INFO_HASTSC		0x01

#define	PMC_NCOUNTERS		4

struct i386_pmc_startstop_args {
	int counter;
	uint64_t val;
	uint8_t event;
	uint8_t unit;
	uint8_t compare;
	uint8_t flags;
};

#define	PMC_SETUP_KERNEL	0x01
#define	PMC_SETUP_USER		0x02
#define	PMC_SETUP_EDGE		0x04
#define	PMC_SETUP_INV		0x08

struct i386_pmc_read_args {
	int counter;
	uint64_t val;
	uint64_t time;
};

struct mtrr;

#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int i386_get_ldt(int, union descriptor *, int);
int i386_set_ldt(int, union descriptor *, int);
int i386_iopl(int);
int i386_pmc_info(struct i386_pmc_info_args *);
int i386_pmc_startstop(struct i386_pmc_startstop_args *);
int i386_pmc_read(struct i386_pmc_read_args *);
int i386_set_mtrr(struct mtrr *, int *);
int i386_get_mtrr(struct mtrr *, int *);
int sysarch(int, void *);
__END_DECLS
#endif

#endif /* !_I386_SYSARCH_H_ */
