/*	$NetBSD: ypdb.c,v 1.4.2.1 1997/11/28 09:30:41 mellon Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * This code is derived from ndbm module of BSD4.4 db (hash) by
 * Mats O Jansson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ypdb.c,v 1.4.2.1 1997/11/28 09:30:41 mellon Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>

#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <rpcsvc/yp.h>

#include "ypdb.h"

/*
 * Returns:
 * 	*DBM on success
 *	 NULL on failure
 */

DBM *
ypdb_open(file, flags, mode)
	const char *file;
	int flags, mode;
{
	char path[MAXPATHLEN], *cp;
	DBM *db;
	BTREEINFO info;

	cp = strrchr(file, '.');
	snprintf(path, sizeof(path), "%s%s", file,
	    (cp != NULL && strcmp(cp, ".db") == 0) ? "" : YPDB_SUFFIX);

		/* try our btree format first */
	info.flags = 0;
	info.cachesize = 0;
	info.maxkeypage = 0;
	info.minkeypage = 0;
	info.psize = 0;
	info.compare = NULL;
	info.prefix = NULL;
	info.lorder = 0;
	db = (DBM *)dbopen(path, flags, mode, DB_BTREE, (void *)&info);
	if (db != NULL || errno != EFTYPE)
		return (db);

		/* fallback to standard hash (for sendmail's aliases.db) */
	db = (DBM *)dbopen(path, flags, mode, DB_HASH, NULL);
	return (db);
}

void
ypdb_close(db)
	DBM *db;
{
	(void)(db->close)(db);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */

datum
ypdb_fetch(db, key)
	DBM *db;
	datum key;
{
	datum retval;
	int status;

	status = (db->get)(db, (DBT *)&key, (DBT *)&retval, 0);
	if (status) {
		retval.dptr = NULL;
		retval.dsize = 0;
	}
	return (retval);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */

datum
ypdb_firstkey(db)
	DBM *db;
{
	int status;
	datum retdata, retkey;

	status = (db->seq)(db, (DBT *)&retkey, (DBT *)&retdata, R_FIRST);
	if (status)
		retkey.dptr = NULL;
	return (retkey);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */

datum
ypdb_nextkey(db)
	DBM *db;
{
	int status;
	datum retdata, retkey;

	status = (db->seq)(db, (DBT *)&retkey, (DBT *)&retdata, R_NEXT);
	if (status)
		retkey.dptr = NULL;
	return (retkey);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */

datum
ypdb_setkey(db, key)
	DBM *db;
        datum key;
{
	int status;
	datum retdata;
	status = (db->seq)(db, (DBT *)&key, (DBT *)&retdata, R_CURSOR);
	if (status)
		key.dptr = NULL;
	return (key);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 */

int
ypdb_delete(db, key)
	DBM *db;
	datum key;
{
	int status;

	status = (db->del)(db, (DBT *)&key, 0);
	if (status)
		return (-1);
	else
		return (0);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 *	 1 if YPDB_INSERT and entry exists
 */

int
ypdb_store(db, key, content, flags)
	DBM *db;
	datum key, content;
	int flags;
{
	if (key.dsize > YPMAXRECORD || content.dsize > YPMAXRECORD)
		return -1;
	return ((db->put)(db, (DBT *)&key, (DBT *)&content,
	    (flags == YPDB_INSERT) ? R_NOOVERWRITE : 0));
}
