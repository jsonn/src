/*	$NetBSD: reg.h,v 1.3.2.2 2000/12/08 09:30:17 bouyer Exp $	*/

#ifndef _POWERPC_REG_H_
#define _POWERPC_REG_H_

struct reg {				/* base registers */
	register_t fixreg[32];
	register_t lr;			/* Link Register */
	int cr;				/* Condition Register */
	int xer;			/* SPR 1 */
	register_t ctr;			/* Count Register */
	register_t pc;			/* Program Counter */
};

struct fpreg {				/* Floating Point registers */
	double fpreg[32];
	double fpscr;			/* Status and Control Register */
};

struct vreg {				/* Vector registers */
	u_int32_t vreg[32][4];
	register_t vscr;		/* Vector Status And Control Register */
	register_t vrsave;		/* SPR 238 */
};

#endif /* _POWERPC_REG_H_ */
