/*
 * fake swapgeneric.c -- should do this differently.
 *
 *	@(#)swapgeneric.c	8.1 (Berkeley) 7/19/93
 * $Id: swapgeneric.c,v 1.4.2.1 1994/07/26 18:21:40 cgd Exp $
 */

#include <sys/param.h>
#include <sys/conf.h>

extern int ffs_mountroot();
int (*mountroot)() = ffs_mountroot;

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

struct	swdevt swdevt[] = {
	{ makedev(7, 1), 0, 0 },	/* sd0b */
	{ makedev(7, 9), 0, 0 },	/* sd1b */
	{ makedev(7, 17), 0, 0 },	/* sd2b */
	{ makedev(7, 25), 0, 0 },	/* sd3b */
	{ makedev(7, 33), 0, 0 },	/* sd4b */
	{ makedev(7, 41), 0, 0 },	/* sd5b */
	{ makedev(7, 49), 0, 0 },	/* sd6b */
	{ makedev(7, 57), 0, 0 },	/* sd7b */
	{ NODEV, 0, 0 }
};
