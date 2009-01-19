/* $NetBSD: stubs.c,v 1.7.86.1 2009/01/19 13:15:50 skrll Exp $ */
/*
 * stubs.c -- functions I haven't written yet
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: stubs.c,v 1.7.86.1 2009/01/19 13:15:50 skrll Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

int
suibyte(base, c)
	void *base;
	int c;
{
	panic("suibyte not implemented");
}

int
suisword(base, c)
	void *base;
	short c;
{
	panic("suisword not implemented");
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
fuisword(base)
	const void *base;
{
	panic("fuisword not implemented");
}

long
fuiword(base)
	const void *base;
{
	panic("fuiword not implemented");
}
