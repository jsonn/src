/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)stand.h	8.1 (Berkeley) 6/11/93
 * 	     $Id: stand.h,v 1.4.2.2 1994/08/22 21:56:14 brezak Exp $
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include "saioctl.h"
#include "saerrno.h"

#ifndef NULL
#define	NULL	0
#endif

struct open_file;

/*
 * This structure is used to define file system operations in a file system
 * independent way.
 */
struct fs_ops {
	int	(*open) __P((char *path, struct open_file *f));
	int	(*close) __P((struct open_file *f));
	int	(*read) __P((struct open_file *f, char *buf,
			u_int size, u_int *resid));
	int	(*write) __P((struct open_file *f, char *buf,
			u_int size, u_int *resid));
	off_t	(*seek) __P((struct open_file *f, off_t offset, int where));
	int	(*stat) __P((struct open_file *f, struct stat *sb));
};

extern struct fs_ops file_system[];

/* where values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

/* Device switch */
struct devsw {
	char	*dv_name;
	int	(*dv_strategy) __P((void *devdata, int rw,
			daddr_t blk, u_int size, char *buf, u_int *rsize));
	int	(*dv_open) __P((struct open_file *f, ...));
	int	(*dv_close) __P((struct open_file *f));
	int	(*dv_ioctl) __P((struct open_file *f, int cmd, void *data));
};

extern struct devsw devsw[];	/* device array */
extern int ndevs;		/* number of elements in devsw[] */

struct open_file {
	int		f_flags;	/* see F_* below */
	struct devsw	*f_dev;		/* pointer to device operations */
	void		*f_devdata;	/* device specific data */
	struct fs_ops	*f_ops;		/* pointer to file system operations */
	void		*f_fsdata;	/* file system specific data */
};

#define	SOPEN_MAX	4
extern struct open_file files[SOPEN_MAX];
extern int nfsys;

/* f_flags values */
#define	F_READ		0x0001	/* file opened for reading */
#define	F_WRITE		0x0002	/* file opened for writing */
#define	F_RAW		0x0004	/* raw device open - no file system */
#define F_NODEV		0x0008	/* network open - no device */

#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define tolower(c)	((c) - 'A' + 'a')
#define isspace(c)	((c) == ' ' || (c) == '\t')
#define isdigit(c)	((c) >= '0' && (c) <= '9')

int	devopen __P((struct open_file *f, char *fname, char **file));
void	*alloc __P((unsigned size));
void	free __P((void *ptr, unsigned size));
struct	disklabel;
char	*getdisklabel __P((const char *buf, struct disklabel *lp));

void	printf __P((const char *, ...));
void	gets __P((char *));
void	panic __P((const char *, ...));
int	getchar __P((void));
int	exec __P((char *, char *, int));
int	open __P((char *,int));
int	close __P((int));
int	read __P((int, void *, u_int));
int	write __P((int, void *, u_int));
    
int	nodev(), noioctl();
void	nullsys();

int	null_open __P((char *path, struct open_file *f));
int	null_close __P((struct open_file *f));
int	null_read __P((struct open_file *f, char *buf,
		u_int size, u_int *resid));
int	null_write __P((struct open_file *f, char *buf,
		u_int size, u_int *resid));
off_t	null_seek __P((struct open_file *f, off_t offset, int where));
int	null_stat __P((struct open_file *f, struct stat *sb));

/* Machine dependent functions */
void	machdep_start __P((char *, int, char *, char *, char *));
int	machdep_exec __P((char *, char *, int));
int	getchar __P((void));
void	putchar __P((int));    
