/*	$NetBSD: panic.c,v 1.2.2.1 2002/07/15 01:22:24 gehenna Exp $	*/


#include <machine/stdarg.h>
#include <stand.h>
#include "libsa.h"

__dead void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	putchar('\n');
	va_end(ap);
	breakpoint();
	exit(0);
}
