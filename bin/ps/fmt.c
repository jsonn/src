/*	$NetBSD: fmt.c,v 1.13 1998/02/03 03:16:01 perry Exp $	*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "ps.h"
#include "extern.h"

void
fmt_puts(s, leftp)
	char *s;
	int *leftp;
{
	static char *v = 0, *nv;
	static int maxlen = 0;
	int len;

	if (*leftp == 0)
		return;
	len = strlen(s) * 4 + 1;
	if (len > maxlen) {
		if (maxlen == 0)
			maxlen = getpagesize();
		while (len > maxlen)
			maxlen *= 2;
		nv = realloc(v, maxlen);
		if (nv == 0)
			return;
		v = nv;
	}
	strvis(v, s, VIS_TAB | VIS_NL | VIS_CSTYLE);
	if (*leftp != -1) {
		len = strlen(v);
		if (len > *leftp) {
			v[*leftp] = '\0';
			*leftp = 0;
		} else
			*leftp -= len;
	}
	printf("%s", v);
}

void
fmt_putc(c, leftp)
	int c;
	int *leftp;
{

	if (*leftp == 0)
		return;
	if (*leftp != -1)
		*leftp -= 1;
	putchar(c);
}
