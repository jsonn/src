/*	$NetBSD: delay.c,v 1.2.84.1 2008/01/21 09:37:47 yamt Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "libbug.h"

/* BUG - timing routine */
void
mvmeprom_delay(int msec)
{

	MVMEPROM_ARG1(msec);
	MVMEPROM_CALL(MVMEPROM_DELAY);
}
