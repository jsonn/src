/*	$NetBSD: longsize.c,v 1.2.4.2 2015/01/07 10:10:28 msaitoh Exp $	*/

#include <stdio.h>

main()
{
	if (sizeof(long) == 8) { 
		printf("-DLONG8\n");
	} else if (sizeof(long) == 4) {
		printf("-DLONG4\n");
	}
	exit(0);
}
