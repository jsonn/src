/* $NetBSD: stubs.c,v 1.6.2.2 2000/11/20 20:02:34 bouyer Exp $ */
/*
 * stubs.c -- functions I haven't written yet
 */

#include <sys/param.h>

__RCSID("$NetBSD: stubs.c,v 1.6.2.2 2000/11/20 20:02:34 bouyer Exp $");

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/user.h>

void
resettodr()
{

	printf("resettodr: not doing anything\n");
}

int
suibyte(base, c)
	void *base;
	int c;
{
	panic("suibyte not implemented");
}

int
susword(base, c)
	void *base;
	short c;
{
	panic("susword not implemented");
}

int
suisword(base, c)
	void *base;
	short c;
{
	panic("suisword not implemented");
}

int
suswintr(base, c)
	void *base;
	short c;
{
	panic("suswintr not implemented");
}

int
suiword(base, c)
	void *base;
	long c;
{
	panic("suiword not implemented");
}

int
fuibyte(base)
	const void *base;
{
	panic("fuibyte not implemented");
}

int
fusword(base)
	const void *base;
{
	panic("fusword not implemented");
}

int
fuisword(base)
	const void *base;
{
	panic("fuisword not implemented");
}

int
fuswintr(base)
	const void *base;
{
	panic("fuswintr not implemented");
}

long
fuiword(base)
	const void *base;
{
	panic("fuiword not implemented");
}

int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	printf("FIXME: cpu_coredump() not implemented.\n");
	return ENOSYS;
}

int
cpu_sysctl(name, namelen, oldval, oldlenp, newval, newlen, p)
	int *name;
	u_int namelen;
	void *oldval, *newval;
	size_t *oldlenp, newlen;
	struct proc *p;
{
	panic("cpu_sysctl not implemented");
}

void
pagemove(foo, bar, len)
	caddr_t foo, bar;
	size_t len;
{
	panic("pagemove not implemented");
}

int sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	panic("sys_sysarch not implemented");
}
