/*	$NetBSD: cpu.h,v 1.18.4.2 1999/01/22 04:13:50 nisimura Exp $	*/

#ifndef _PMAX_CPU_H_
#define _PMAX_CPU_H_

/*
 * pmax uses standard mips1 convention, wiring FPU to hard interupt 5.
 */
#include <mips/cpu.h>
#include <mips/cpuregs.h>

#define MIPS_INT_MASK_FPU	MIPS_INT_MASK_5

#define	INT_MASK_REAL_DEV	(MIPS_HARD_INT_MASK &~ MIPS_INT_MASK_5)
#define	INT_MASK_FPU_DEAL	MIPS_INT_MASK_5

#endif _PMAX_CPU_H_
