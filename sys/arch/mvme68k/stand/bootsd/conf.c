/*	$NetBSD: conf.c,v 1.1.68.1 2005/11/10 13:57:53 skrll Exp $	*/

#include <sys/types.h>
#include <machine/prom.h>

#include <lib/libsa/stand.h>
#include <ufs.h>
#include "libsa.h"

struct fs_ops file_system[] = {
	FS_OPS(ufs),
};
int nfsys = sizeof(file_system)/sizeof(struct fs_ops);

struct devsw devsw[] = {
        { "bugsc", bugscstrategy, bugscopen, bugscclose, bugscioctl },
};
int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));


