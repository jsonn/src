/*	$NetBSD: linux32_syscall.h,v 1.1.2.2 2006/02/18 15:38:59 yamt Exp $ */

#ifndef _LINUX32_SYSCALL_H
#define _LINUX32_SYSCALL_H

#if defined(__amd64__)
#include <compat/linux32/arch/amd64/linux32_syscall.h>
#else
#define	LINUX32_SYS_MAXSYSCALL	0
#endif

#endif /* !_LINUX32_SYSCALL_H */
