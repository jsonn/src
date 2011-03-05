/*	$NetBSD: proc.h,v 1.5.2.1 2011/03/05 20:50:49 rmind Exp $	*/

#ifndef _IA64_PROC_H_
#define _IA64_PROC_H_

#include <machine/frame.h>

/*
 * Machine-dependent part of the lwp structure for ia64
 */
struct mdlwp {
	u_long	md_flags;
	struct	trapframe *md_tf;	/* trap/syscall registers */
	__volatile int md_astpending;	/* AST pending for this process */
};

/*
 * md_flags usage
 * --------------
 * XXX:
 */

struct mdproc {
  /* XXX: Todo */
	void	(*md_syscall)(struct lwp *, u_int64_t, struct trapframe *);
					/* Syscall handling function */
};

#endif /* _IA64_PROC_H_ */
