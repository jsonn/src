/*	$NetBSD: ttyname.c,v 1.9.4.2 1996/09/19 20:04:11 jtc Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)ttyname.c	8.2 (Berkeley) 1/27/94";
#else
static char rcsid[] = "$NetBSD: ttyname.c,v 1.9.4.2 1996/09/19 20:04:11 jtc Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <db.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>

#ifdef __weak_alias
__weak_alias(ttyname,_ttyname);
#endif

static char buf[sizeof(_PATH_DEV) + MAXNAMLEN] = _PATH_DEV;
static char *oldttyname __P((int, struct stat *));

char *
ttyname(fd)
	int fd;
{
	struct stat sb;
	struct termios ttyb;
	DB *db;
	DBT data, key;
	struct {
		mode_t type;
		dev_t dev;
	} bkey;

	/* Must be a terminal. */
	if (tcgetattr(fd, &ttyb) < 0)
		return (NULL);
	/* Must be a character device. */
	if (fstat(fd, &sb) || !S_ISCHR(sb.st_mode))
		return (NULL);

	if (db = dbopen(_PATH_DEVDB, O_RDONLY, 0, DB_HASH, NULL)) {
		memset(&bkey, 0, sizeof(bkey));
		bkey.type = S_IFCHR;
		bkey.dev = sb.st_rdev;
		key.data = &bkey;
		key.size = sizeof(bkey);
		if (!(db->get)(db, &key, &data, 0)) {
			bcopy(data.data,
			    buf + sizeof(_PATH_DEV) - 1, data.size);
			(void)(db->close)(db);
			return (buf);
		}
		(void)(db->close)(db);
	}
	return (oldttyname(fd, &sb));
}

static char *
oldttyname(fd, sb)
	int fd;
	struct stat *sb;
{
	register struct dirent *dirp;
	register DIR *dp;
	struct stat dsb;

	if ((dp = opendir(_PATH_DEV)) == NULL)
		return (NULL);

	while (dirp = readdir(dp)) {
		if (dirp->d_fileno != sb->st_ino)
			continue;
		bcopy(dirp->d_name, buf + sizeof(_PATH_DEV) - 1,
		    dirp->d_namlen + 1);
		if (stat(buf, &dsb) || sb->st_dev != dsb.st_dev ||
		    sb->st_ino != dsb.st_ino)
			continue;
		(void)closedir(dp);
		return (buf);
	}
	(void)closedir(dp);
	return (NULL);
}
