/* $NetBSD: procfs_machdep.c,v 1.1.8.2 2001/05/14 18:23:04 drochner Exp $ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>

/*
 * Linux-style /proc/cpuinfo.
 * Only used when procfs is mounted with -o linux.
 */
int
procfs_getcpuinfstr(char *buf, int *len)
{

	*len = 0;
	return 0;
}
