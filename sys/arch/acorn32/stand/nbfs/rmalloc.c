/* $NetBSD: rmalloc.c,v 1.1.2.3 2006/08/11 15:40:59 yamt Exp $ */

#include <lib/libsa/stand.h>
#include <riscoscalls.h>

void *
alloc(size_t size)
{
	void *ret;

	if (xosmodule_alloc(size, &ret) != NULL)
		return NULL;
	return ret;
}

void dealloc(void *ptr, size_t size)
{

	xosmodule_free(ptr);
}
