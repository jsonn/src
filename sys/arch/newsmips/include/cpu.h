/*	$NetBSD: cpu.h,v 1.10.2.2 2002/01/10 19:47:01 thorpej Exp $	*/

#ifndef _NEWSMIPS_CPU_H_
#define _NEWSMIPS_CPU_H_

#include <mips/cpu.h>

#if defined(_KERNEL) || defined(_STANDALONE)
#ifndef _LOCORE
extern int systype;

#define NEWS3400	1
#define NEWS5000	2
#endif
#endif

#endif /* _NEWSMIPS_CPU_H_ */
