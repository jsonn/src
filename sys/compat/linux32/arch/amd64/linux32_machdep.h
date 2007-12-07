/*	$NetBSD: linux32_machdep.h,v 1.1.16.3 2007/12/07 17:28:40 yamt Exp $ */

#ifndef _AMD64_LINUX32_MACHDEP_H
#define _AMD64_LINUX32_MACHDEP_H

#include <compat/linux32/arch/amd64/linux32_missing.h>

#ifdef _KERNEL
__BEGIN_DECLS
void linux32_syscall_intern(struct proc *);
__END_DECLS
#endif /* !_KERNEL */

#endif /* _AMD64_LINUX32_MACHDEP_H */
