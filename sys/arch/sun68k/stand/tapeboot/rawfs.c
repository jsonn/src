/*	$NetBSD: rawfs.c,v 1.5.2.1 2006/02/01 14:51:41 yamt Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
 * All rights reserved.
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
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Raw file system - for stream devices like tapes.
 * No random access, only sequential read allowed.
 * This exists only to allow upper level code to be
 * shielded from the fact that the device must be
 * read only with whole block position and size.
 */

#include <sys/param.h>
#include <stand.h>

#include "rawfs.h"

extern int debug;

/* Our devices are generally willing to do 8K transfers. */
#define	RAWFS_BSIZE	0x2000

/*
 * In-core open file.
 */
struct file {
	daddr_t		fs_nextblk;	/* block number to read next */
	off_t		fs_off;		/* seek offset in file */
	int		fs_len;		/* amount left in f_buf */
	char *		fs_ptr;		/* read pointer into f_buf */
	char		fs_buf[RAWFS_BSIZE];
};

static int rawfs_get_block(struct open_file *);

int 
rawfs_open(const char *path, struct open_file *f)
{
	struct file *fs;

	/*
	 * The actual PROM driver has already been opened.
	 * Just allocate the I/O buffer, etc.
	 */
	fs = alloc(sizeof(struct file));
	fs->fs_nextblk = 0;
	fs->fs_off = 0;
	fs->fs_len = 0;
	fs->fs_ptr = fs->fs_buf;

#ifdef	DEBUG_RAWFS
	printf("rawfs_open: fs=0x%x\n", fs);
#endif

	f->f_fsdata = fs;
	return (0);
}

int 
rawfs_close(struct open_file *f)
{
	struct file *fs;

	fs = (struct file *) f->f_fsdata;
	f->f_fsdata = NULL;

#ifdef	DEBUG_RAWFS
	if (debug) {
		printf("rawfs_close: breakpoint...", fs->fs_buf);
		__asm ("	trap #0");
	}
#endif

	if (fs != NULL)
		dealloc(fs, sizeof(*fs));

	return (0);
}

int 
rawfs_read(struct open_file *f, void *start, u_int size, u_int *resid)
{
	struct file *fs = (struct file *)f->f_fsdata;
	char *addr = start;
	int error = 0;
	size_t csize;

	while (size != 0) {
		if (fs->fs_len == 0)
			if ((error = rawfs_get_block(f)) != 0)
				break;

		if (fs->fs_len <= 0)
			break;	/* EOF */

		csize = size;
		if (csize > fs->fs_len)
			csize = fs->fs_len;

		memcpy(addr, fs->fs_ptr, csize);
		fs->fs_off += csize;
		fs->fs_ptr += csize;
		fs->fs_len -= csize;
		addr += csize;
		size -= csize;
	}
	if (resid)
		*resid = size;
	return (error);
}

int 
rawfs_write(struct open_file *f, void *start, size_t size, size_t *resid)
{
#ifdef	DEBUG_RAWFS
	panic("rawfs_write");
#endif
	return (EROFS);
}

off_t 
rawfs_seek(struct open_file *f, off_t offset, int where)
{
	struct file *fs = (struct file *)f->f_fsdata;
	off_t csize;

	switch (where) {
	case SEEK_SET:
		offset -= fs->fs_off;
		/* FALLTHROUGH */
	case SEEK_CUR:
		if (offset >= 0)
			break;
		/* FALLTHROUGH */
	case SEEK_END:
	default:
		return (-1);
	}

	while (offset != 0) {

		if (fs->fs_len == 0)
			if (rawfs_get_block(f) != 0)
				return (-1);

		if (fs->fs_len <= 0)
			break;	/* EOF */

		csize = offset;
		if (csize > fs->fs_len)
			csize = fs->fs_len;

		fs->fs_off += csize;
		fs->fs_ptr += csize;
		fs->fs_len -= csize;
		offset -= csize;
	}
	return (fs->fs_off);
}

int 
rawfs_stat(struct open_file *f, struct stat *sb)
{
#ifdef	DEBUG_RAWFS
	panic("rawfs_stat");
#endif
	return (EFTYPE);
}


/*
 * Read a block from the underlying stream device
 * (In our case, a tape drive.)
 */
static int 
rawfs_get_block(struct open_file *f)
{
	struct file *fs;
	int error, len;

	fs = (struct file *)f->f_fsdata;
	fs->fs_ptr = fs->fs_buf;

	twiddle();
	error = f->f_dev->dv_strategy(f->f_devdata, F_READ,
		fs->fs_nextblk, RAWFS_BSIZE,	fs->fs_buf, &len);

	if (!error) {
		fs->fs_len = len;
		fs->fs_nextblk += (RAWFS_BSIZE / DEV_BSIZE);
	}

	return (error);
}
