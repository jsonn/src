/*	$NetBSD: open.c,v 1.20.6.1 2002/02/28 04:14:52 nathanw Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)open.c	8.1 (Berkeley) 6/11/93
 *  
 *
 * Copyright (c) 1989, 1990, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Author: Alessandro Forin
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "stand.h"

/*
 *	File primitives proper
 */

#ifdef HELLO_CTAGS
oopen(){}
#endif

int
#ifndef __INTERNAL_LIBSA_CREAD
open(fname, mode)
#else
oopen(fname, mode)
#endif
	const char *fname;
	int mode;
{
	struct open_file *f;
	int fd, error;
#if !defined(LIBSA_SINGLE_FILESYSTEM)
	int i, besterror;
#endif
	char *file;

	/* find a free file descriptor */
	for (fd = 0, f = files; fd < SOPEN_MAX; fd++, f++)
		if (f->f_flags == 0)
			goto fnd;
	errno = EMFILE;
	return (-1);
fnd:
	/*
	 * Try to open the device.
	 * Convert open mode (0,1,2) to F_READ, F_WRITE.
	 */
	f->f_flags = mode + 1;
#if !defined(LIBSA_SINGLE_DEVICE)
	f->f_dev = (struct devsw *)0;
#endif
#if !defined(LIBSA_SINGLE_FILESYSTEM)
	f->f_ops = (struct fs_ops *)0;
#endif
#if !defined(LIBSA_NO_RAW_ACCESS)
	f->f_offset = 0;
#endif
	file = (char *)0;
	error = devopen(f, fname, &file);
	if (error
#if !defined(LIBSA_SINGLE_DEVICE)
	    || (((f->f_flags & F_NODEV) == 0) &&
		f->f_dev == (struct devsw *)0)
#endif
	    )
		goto err;

#if !defined(LIBSA_NO_RAW_ACCESS)
	/* see if we opened a raw device; otherwise, 'file' is the file name. */
	if (file == (char *)0 || *file == '\0') {
		f->f_flags |= F_RAW;
		return (fd);
	}
#endif

	/* pass file name to the different filesystem open routines */
#if !defined(LIBSA_SINGLE_FILESYSTEM)
	besterror = ENOENT;
	for (i = 0; i < nfsys; i++) {
		error = FS_OPEN(&file_system[i])(file, f);
		if (error == 0) {
			f->f_ops = &file_system[i];
			return (fd);
		}
		if (error != EINVAL)
			besterror = error;
	}
	error = besterror;
#else
	error = FS_OPEN(&file_system[i])(file, f);
	if (error == 0)
		return (fd);
	else if (error == EINVAL)
		error = ENOENT;
#endif

	if ((f->f_flags & F_NODEV) == 0)
#if !defined(LIBSA_SINGLE_DEVICE)
		if (DEV_CLOSE(f->f_dev) != NULL)
#endif
			(void)DEV_CLOSE(f->f_dev)(f);
err:
	f->f_flags = 0;
	errno = error;
	return (-1);
}
