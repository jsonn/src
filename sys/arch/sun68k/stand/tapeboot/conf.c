/*	$NetBSD: conf.c,v 1.1.8.2 2002/06/20 03:42:03 nathanw Exp $	*/

#include <stand.h>
#include <rawfs.h>
#include <dev_tape.h>

struct fs_ops file_system[] = {
	{
		rawfs_open, rawfs_close, rawfs_read,
		rawfs_write, rawfs_seek, rawfs_stat,
	},
};
int nfsys = 1;

struct devsw devsw[] = {
	{ "tape", tape_strategy, tape_open, tape_close, tape_ioctl },
};
int	ndevs = 1;

#ifdef DEBUG
int debug;
#endif /* DEBUG */
