/*	$NetBSD: devopen.c,v 1.4.2.1 2004/08/03 10:38:28 skrll Exp $	*/

/*-
 * Copyright (C) 1999 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <lib/libsa/ustarfs.h>

#include <machine/romcall.h>

#ifdef BOOT_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

int dkopen(struct open_file *, ...);
int dkclose(struct open_file *);
int dkstrategy(void *, int, daddr_t, size_t, void *, size_t *);

struct devsw devsw[] = {
	{ "dk", dkstrategy, dkopen, dkclose, noioctl }
};
int ndevs = sizeof(devsw) / sizeof(devsw[0]);

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
	{ ustarfs_open, ustarfs_close, ustarfs_read, ustarfs_write,
	    ustarfs_seek, ustarfs_stat }
};
int nfsys = sizeof(file_system) / sizeof(file_system[0]);

struct romdev {
	int fd;
} romdev;

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;	/* out */
{
	int fd;
	char devname[32];
	char *cp;

	DPRINTF("devopen: %s\n", fname);

	strcpy(devname, fname);
	cp = strchr(devname, ')') + 1;
	*cp = 0;
	fd = rom_open(devname, 0);

	DPRINTF("devname = %s, fd = %d\n", devname, fd);
	if (fd == -1)
		return -1;

	romdev.fd = fd;

	f->f_dev = devsw;
	f->f_devdata = &romdev;
	*file = strchr(fname, ')') + 1;

	return 0;
}

int
dkopen(struct open_file *f, ...)
{
	DPRINTF("dkopen\n");
	return 0;
}

int
dkclose(f)
	struct open_file *f;
{
	struct romdev *dev = f->f_devdata;

	DPRINTF("dkclose\n");
	rom_close(dev->fd);
	return 0;
}

int
dkstrategy(devdata, rw, blk, size, buf, rsize)
	void *devdata;
	int rw;
	daddr_t blk;
	size_t size;
	void *buf;
	size_t *rsize;	/* out: number of bytes transfered */
{
	struct romdev *dev = devdata;

	/* XXX should use partition offset */

	rom_lseek(dev->fd, blk * 512, 0);
	rom_read(dev->fd, buf, size);
	*rsize = size;
	return 0;
}
