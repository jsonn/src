/*	$NetBSD: gayle.c,v 1.4.16.1 2005/11/10 13:51:35 skrll Exp $	*/

/* public domain */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gayle.c,v 1.4.16.1 2005/11/10 13:51:35 skrll Exp $");

/*
 * Gayle management routines
 *
 *   Any module that uses gayle should call gayle_init() before using anything
 *   related to gayle. gayle_init() can be called multiple times.
 */

#include <amiga/amiga/gayle.h>
#include <amiga/dev/zbusvar.h>

struct gayle_struct *gayle_base_virtual_address = 0;

#define GAYLE_PHYS_ADDRESS      0xda8000

void
gayle_init(void) {

	if (gayle_base_virtual_address != 0)
		return;

	gayle_base_virtual_address =
	    (struct gayle_struct *) __UNVOLATILE(ztwomap(GAYLE_PHYS_ADDRESS));
}
