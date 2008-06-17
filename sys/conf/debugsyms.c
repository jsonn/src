/*	$NetBSD: debugsyms.c,v 1.1.16.1 2008/06/17 09:14:24 yamt Exp $	*/
/*
 * This file is in the public domain.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: debugsyms.c,v 1.1.16.1 2008/06/17 09:14:24 yamt Exp $");

#define	_CALLOUT_PRIVATE
#define	__MUTEX_PRIVATE

#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sleepq.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/user.h>
#include <sys/vnode.h>

/*
 * Without a dummy function referencing some of the types, gcc will
 * not emit any debug information.
 */
proc_t	*_debugsym_dummyfunc(vnode_t *vp);

proc_t *
_debugsym_dummyfunc(vnode_t *vp)
{

	return ((lwp_t *)vp->v_mount)->l_proc;
}
