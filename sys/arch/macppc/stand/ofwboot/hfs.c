/*	$NetBSD: hfs.c,v 1.2.22.1 2004/08/03 10:37:31 skrll Exp $	*/

/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
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

#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include <openfirm.h>
#include <hfs.h>

static int OF_fd;	/* XXX */

int
hfs_open(const char *path, struct open_file *f)
{
	int chosen;
	char bootpath[128], *cp;

	if ((chosen = OF_finddevice("/chosen")) == -1)
		return ENXIO;
	memset(bootpath, 0, sizeof bootpath);
	OF_getprop(chosen, "bootpath", bootpath, sizeof bootpath);
	cp = strrchr(bootpath, ',');
	if (cp == NULL)
		return ENXIO;

	strcpy(cp + 1, path);
	OF_fd = OF_open(bootpath);
	if (OF_fd == -1)
		return ENOENT;

	return 0;
}

int
hfs_close(struct open_file *f)
{
	OF_close(OF_fd);
	return 0;
}

int
hfs_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	int len;

	len = OF_read(OF_fd, start, size);
	size -= len;
	if (resid)
		*resid = size;
	return 0;
}

int
hfs_write(struct open_file *f, void *start, size_t size, size_t *resid)
{
	printf("hfs_write\n");
	return ENXIO;
}

off_t
hfs_seek(struct open_file *f, off_t offset, int where)
{
	switch (where) {
	case SEEK_SET:
		return OF_seek(OF_fd, offset);
	case SEEK_CUR:
	case SEEK_END:
	default:
		return -1;
	}
}

int
hfs_stat(struct open_file *f, struct stat *sb)
{
	return 0;
}
