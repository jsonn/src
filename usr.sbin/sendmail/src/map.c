/*
 * Copyright (c) 1992, 1995-1997 Eric P. Allman.
 * Copyright (c) 1992, 1993
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

#ifndef lint
static char sccsid[] = "@(#)map.c	8.186 (Berkeley) 10/21/97";
#endif /* not lint */

#include "sendmail.h"

#ifdef NDBM
# include <ndbm.h>
# ifdef R_FIRST
  ERROR README:	You are running the Berkeley DB version of ndbm.h.  See
  ERROR README:	the READ_ME file about tweaking Berkeley DB so it can
  ERROR README:	coexist with NDBM, or delete -DNDBM from the Makefile
  ERROR README: and use -DNEWDB instead.
# endif
#endif
#ifdef NEWDB
# include <db.h>
#endif
#ifdef NIS
  struct dom_binding;	/* forward reference needed on IRIX */
# include <rpcsvc/ypclnt.h>
# ifdef NDBM
#  define NDBM_YP_COMPAT	/* create YP-compatible NDBM files */
# endif
#endif

/*
**  MAP.C -- implementations for various map classes.
**
**	Each map class implements a series of functions:
**
**	bool map_parse(MAP *map, char *args)
**		Parse the arguments from the config file.  Return TRUE
**		if they were ok, FALSE otherwise.  Fill in map with the
**		values.
**
**	char *map_lookup(MAP *map, char *key, char **args, int *pstat)
**		Look up the key in the given map.  If found, do any
**		rewriting the map wants (including "args" if desired)
**		and return the value.  Set *pstat to the appropriate status
**		on error and return NULL.  Args will be NULL if called
**		from the alias routines, although this should probably
**		not be relied upon.  It is suggested you call map_rewrite
**		to return the results -- it takes care of null termination
**		and uses a dynamically expanded buffer as needed.
**
**	void map_store(MAP *map, char *key, char *value)
**		Store the key:value pair in the map.
**
**	bool map_open(MAP *map, int mode)
**		Open the map for the indicated mode.  Mode should
**		be either O_RDONLY or O_RDWR.  Return TRUE if it
**		was opened successfully, FALSE otherwise.  If the open
**		failed an the MF_OPTIONAL flag is not set, it should
**		also print an error.  If the MF_ALIAS bit is set
**		and this map class understands the @:@ convention, it
**		should call aliaswait() before returning.
**
**	void map_close(MAP *map)
**		Close the map.
**
**	This file also includes the implementation for getcanonname.
**	It is currently implemented in a pretty ad-hoc manner; it ought
**	to be more properly integrated into the map structure.
*/

#define DBMMODE		0644

#ifndef EX_NOTFOUND
# define EX_NOTFOUND	EX_NOHOST
#endif

extern bool	aliaswait __P((MAP *, char *, int));
extern bool	extract_canonname __P((char *, char *, char[], int));

#if O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL
# define LOCK_ON_OPEN	1	/* we can open/create a locked file */
#else
# define LOCK_ON_OPEN	0	/* no such luck -- bend over backwards */
#endif

#ifndef O_ACCMODE
# define O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif
/*
**  MAP_PARSEARGS -- parse config line arguments for database lookup
**
**	This is a generic version of the map_parse method.
**
**	Parameters:
**		map -- the map being initialized.
**		ap -- a pointer to the args on the config line.
**
**	Returns:
**		TRUE -- if everything parsed OK.
**		FALSE -- otherwise.
**
**	Side Effects:
**		null terminates the filename; stores it in map
*/

bool
map_parseargs(map, ap)
	MAP *map;
	char *ap;
{
	register char *p = ap;

	map->map_mflags |= MF_TRY0NULL | MF_TRY1NULL;
	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'N':
			map->map_mflags |= MF_INCLNULL;
			map->map_mflags &= ~MF_TRY0NULL;
			break;

		  case 'O':
			map->map_mflags &= ~MF_TRY1NULL;
			break;

		  case 'o':
			map->map_mflags |= MF_OPTIONAL;
			break;

		  case 'f':
			map->map_mflags |= MF_NOFOLDCASE;
			break;

		  case 'm':
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 'A':
			map->map_mflags |= MF_APPEND;
			break;

		  case 'q':
			map->map_mflags |= MF_KEEPQUOTES;
			break;

		  case 'a':
			map->map_app = ++p;
			break;

		  case 'k':
			while (isascii(*++p) && isspace(*p))
				continue;
			map->map_keycolnm = p;
			break;

		  case 'v':
			while (isascii(*++p) && isspace(*p))
				continue;
			map->map_valcolnm = p;
			break;

		  case 'z':
			if (*++p != '\\')
				map->map_coldelim = *p;
			else
			{
				switch (*++p)
				{
				  case 'n':
					map->map_coldelim = '\n';
					break;

				  case 't':
					map->map_coldelim = '\t';
					break;

				  default:
					map->map_coldelim = '\\';
				}
			}
			break;

		  case 't':
			map->map_mflags |= MF_NODEFER;
			break;

#ifdef RESERVED_FOR_SUN
		  case 'd':
			map->map_mflags |= MF_DOMAIN_WIDE;
			break;

		  case 's':
			/* info type */
			break;
#endif
		}
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}
	if (map->map_app != NULL)
		map->map_app = newstr(map->map_app);
	if (map->map_keycolnm != NULL)
		map->map_keycolnm = newstr(map->map_keycolnm);
	if (map->map_valcolnm != NULL)
		map->map_valcolnm = newstr(map->map_valcolnm);

	if (*p != '\0')
	{
		map->map_file = p;
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
		map->map_file = newstr(map->map_file);
	}

	while (*p != '\0' && isascii(*p) && isspace(*p))
		p++;
	if (*p != '\0')
		map->map_rebuild = newstr(p);

	if (map->map_file == NULL &&
	    !bitset(MCF_OPTFILE, map->map_class->map_cflags))
	{
		syserr("No file name for %s map %s",
			map->map_class->map_cname, map->map_mname);
		return FALSE;
	}
	return TRUE;
}
/*
**  MAP_REWRITE -- rewrite a database key, interpolating %n indications.
**
**	It also adds the map_app string.  It can be used as a utility
**	in the map_lookup method.
**
**	Parameters:
**		map -- the map that causes this.
**		s -- the string to rewrite, NOT necessarily null terminated.
**		slen -- the length of s.
**		av -- arguments to interpolate into buf.
**
**	Returns:
**		Pointer to rewritten result.  This is static data that
**		should be copied if it is to be saved!
**
**	Side Effects:
**		none.
*/

char *
map_rewrite(map, s, slen, av)
	register MAP *map;
	register const char *s;
	int slen;
	char **av;
{
	register char *bp;
	register char c;
	char **avp;
	register char *ap;
	int i;
	int len;
	static int buflen = -1;
	static char *buf = NULL;

	if (tTd(39, 1))
	{
		printf("map_rewrite(%.*s), av =", slen, s);
		if (av == NULL)
			printf(" (nullv)");
		else
		{
			for (avp = av; *avp != NULL; avp++)
				printf("\n\t%s", *avp);
		}
		printf("\n");
	}

	/* count expected size of output (can safely overestimate) */
	i = len = slen;
	if (av != NULL)
	{
		const char *sp = s;

		for (i = slen; --i >= 0 && (c = *sp++) != 0; )
		{
			if (c != '%')
				continue;
			if (--i < 0)
				break;
			c = *sp++;
			if (!(isascii(c) && isdigit(c)))
				continue;
			for (avp = av; --c >= '0' && *avp != NULL; avp++)
				continue;
			if (*avp == NULL)
				continue;
			len += strlen(*avp);
		}
	}
	if (map->map_app != NULL)
		len += strlen(map->map_app);
	if (buflen < ++len)
	{
		/* need to malloc additional space */
		buflen = len;
		if (buf != NULL)
			free(buf);
		buf = xalloc(buflen);
	}

	bp = buf;
	if (av == NULL)
	{
		bcopy(s, bp, slen);
		bp += slen;
	}
	else
	{
		while (--slen >= 0 && (c = *s++) != '\0')
		{
			if (c != '%')
			{
  pushc:
				*bp++ = c;
				continue;
			}
			if (--slen < 0 || (c = *s++) == '\0')
				c = '%';
			if (c == '%')
				goto pushc;
			if (!(isascii(c) && isdigit(c)))
			{
				*bp++ = '%';
				goto pushc;
			}
			for (avp = av; --c >= '0' && *avp != NULL; avp++)
				continue;
			if (*avp == NULL)
				continue;

			/* transliterate argument into output string */
			for (ap = *avp; (c = *ap++) != '\0'; )
				*bp++ = c;
		}
	}
	if (map->map_app != NULL)
		strcpy(bp, map->map_app);
	else
		*bp = '\0';
	if (tTd(39, 1))
		printf("map_rewrite => %s\n", buf);
	return buf;
}
/*
**  INITMAPS -- initialize for aliasing
**
**	Parameters:
**		rebuild -- if TRUE, this rebuilds the cached versions.
**		e -- current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		initializes aliases:
**		if alias database:  opens the database.
**		if no database available: reads aliases into the symbol table.
*/

void
initmaps(rebuild, e)
	bool rebuild;
	register ENVELOPE *e;
{
	extern void map_init();

#if XDEBUG
	checkfd012("entering initmaps");
#endif
	CurEnv = e;

	stabapply(map_init, 0);
	stabapply(map_init, rebuild ? 2 : 1);
#if XDEBUG
	checkfd012("exiting initmaps");
#endif
}

void
map_init(s, pass)
	register STAB *s;
	int pass;
{
	bool rebuildable;
	register MAP *map;

	/* has to be a map */
	if (s->s_type != ST_MAP)
		return;

	map = &s->s_map;
	if (!bitset(MF_VALID, map->map_mflags))
		return;

	if (tTd(38, 2))
		printf("map_init(%s:%s, %s, %d)\n",
			map->map_class->map_cname == NULL ? "NULL" :
				map->map_class->map_cname,
			map->map_mname == NULL ? "NULL" : map->map_mname,
			map->map_file == NULL ? "NULL" : map->map_file,
			pass);

	/*
	** Pass 0 opens all non-rebuildable maps.
	** Pass 1 opens all rebuildable maps for read.
	** Pass 2 rebuilds all rebuildable maps.
	*/

	rebuildable = (bitset(MF_ALIAS, map->map_mflags) &&
		       bitset(MCF_REBUILDABLE, map->map_class->map_cflags));

	if ((pass == 0 && rebuildable) ||
	    ((pass == 1 || pass == 2) && !rebuildable))
	{
		if (tTd(38, 3))
			printf("\twrong pass (pass = %d, rebuildable = %d)\n",
			       pass, rebuildable);
		return;
	}

	/* if already open, close it (for nested open) */
	if (bitset(MF_OPEN, map->map_mflags))
	{
		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
	}
		
	if (pass == 2)
	{
		rebuildaliases(map, FALSE);
		return;
	}

	if (map->map_class->map_open(map, O_RDONLY))
	{
		if (tTd(38, 4))
			printf("\t%s:%s %s: valid\n",
				map->map_class->map_cname == NULL ? "NULL" :
					map->map_class->map_cname,
				map->map_mname == NULL ? "NULL" :
					map->map_mname,
				map->map_file == NULL ? "NULL" :
					map->map_file);
		map->map_mflags |= MF_OPEN;
	}
	else
	{
		if (tTd(38, 4))
			printf("\t%s:%s %s: invalid: %s\n",
				map->map_class->map_cname == NULL ? "NULL" :
					map->map_class->map_cname,
				map->map_mname == NULL ? "NULL" :
					map->map_mname,
				map->map_file == NULL ? "NULL" :
					map->map_file,
				errstring(errno));
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			extern MAPCLASS BogusMapClass;

			map->map_class = &BogusMapClass;
			map->map_mflags |= MF_OPEN;
		}
	}
}
/*
**  GETCANONNAME -- look up name using service switch
**
**	Parameters:
**		host -- the host name to look up.
**		hbsize -- the size of the host buffer.
**		trymx -- if set, try MX records.
**
**	Returns:
**		TRUE -- if the host was found.
**		FALSE -- otherwise.
*/

bool
getcanonname(host, hbsize, trymx)
	char *host;
	int hbsize;
	bool trymx;
{
	int nmaps;
	int mapno;
	bool found = FALSE;
	bool got_tempfail = FALSE;
	auto int stat;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];

	nmaps = switch_map_find("hosts", maptype, mapreturn);
	for (mapno = 0; mapno < nmaps; mapno++)
	{
		int i;

		if (tTd(38, 20))
			printf("getcanonname(%s), trying %s\n",
				host, maptype[mapno]);
		if (strcmp("files", maptype[mapno]) == 0)
		{
			extern bool text_getcanonname __P((char *, int, int *));

			found = text_getcanonname(host, hbsize, &stat);
		}
#ifdef NIS
		else if (strcmp("nis", maptype[mapno]) == 0)
		{
			extern bool nis_getcanonname __P((char *, int, int *));

			found = nis_getcanonname(host, hbsize, &stat);
		}
#endif
#ifdef NISPLUS
		else if (strcmp("nisplus", maptype[mapno]) == 0)
		{
			extern bool nisplus_getcanonname __P((char *, int, int *));

			found = nisplus_getcanonname(host, hbsize, &stat);
		}
#endif
#if NAMED_BIND
		else if (strcmp("dns", maptype[mapno]) == 0)
		{
			extern bool dns_getcanonname __P((char *, int, bool, int *));

			found = dns_getcanonname(host, hbsize, trymx, &stat);
		}
#endif
#if NETINFO
		else if (strcmp("netinfo", maptype[mapno]) == 0)
		{
			extern bool ni_getcanonname __P((char *, int, int *));

			found = ni_getcanonname(host, hbsize, &stat);
		}
#endif
		else
		{
			found = FALSE;
			stat = EX_UNAVAILABLE;
		}

		/*
		**  Heuristic: if $m is not set, we are running during system
		**  startup.  In this case, when a name is apparently found
		**  but has no dot, treat is as not found.  This avoids
		**  problems if /etc/hosts has no FQDN but is listed first
		**  in the service switch.
		*/

		if (found &&
		    (macvalue('m', CurEnv) != NULL || strchr(host, '.') != NULL))
			break;

		/* see if we should continue */
		if (stat == EX_TEMPFAIL)
		{
			i = MA_TRYAGAIN;
			got_tempfail = TRUE;
		}
		else if (stat == EX_NOTFOUND)
			i = MA_NOTFOUND;
		else
			i = MA_UNAVAIL;
		if (bitset(1 << mapno, mapreturn[i]))
			break;
	}

	if (found)
	{
		char *d;

		if (tTd(38, 20))
			printf("getcanonname(%s), found\n", host);

		/*
		**  If returned name is still single token, compensate
		**  by tagging on $m.  This is because some sites set
		**  up their DNS or NIS databases wrong.
		*/

		if ((d = strchr(host, '.')) == NULL || d[1] == '\0')
		{
			d = macvalue('m', CurEnv);
			if (d != NULL &&
			    hbsize > (int) (strlen(host) + strlen(d) + 1))
			{
				if (host[strlen(host) - 1] != '.')
					strcat(host, ".");
				strcat(host, d);
			}
			else
			{
				return FALSE;
			}
		}
		return TRUE;
	}

	if (tTd(38, 20))
		printf("getcanonname(%s), failed, stat=%d\n", host, stat);

#if NAMED_BIND
	if (got_tempfail)
		h_errno = TRY_AGAIN;
	else
		h_errno = HOST_NOT_FOUND;
#endif

	return FALSE;
}
/*
**  EXTRACT_CANONNAME -- extract canonical name from /etc/hosts entry
**
**	Parameters:
**		name -- the name against which to match.
**		line -- the /etc/hosts line.
**		cbuf -- the location to store the result.
**		cbuflen -- the size of cbuf.
**
**	Returns:
**		TRUE -- if the line matched the desired name.
**		FALSE -- otherwise.
*/

bool
extract_canonname(name, line, cbuf, cbuflen)
	char *name;
	char *line;
	char cbuf[];
	int cbuflen;
{
	int i;
	char *p;
	bool found = FALSE;
	extern char *get_column __P((char *, int, char, char *, int));

	cbuf[0] = '\0';
	if (line[0] == '#')
		return FALSE;

	for (i = 1; ; i++)
	{
		char nbuf[MAXNAME + 1];

		p = get_column(line, i, '\0', nbuf, sizeof nbuf);
		if (p == NULL)
			break;
		if (*p == '\0')
			continue;
		if (cbuf[0] == '\0' ||
		    (strchr(cbuf, '.') == NULL && strchr(p, '.') != NULL))
		{
			snprintf(cbuf, cbuflen, "%s", p);
		}
		if (strcasecmp(name, p) == 0)
			found = TRUE;
	}
	if (found && strchr(cbuf, '.') == NULL)
	{
		/* try to add a domain on the end of the name */
		char *domain = macvalue('m', CurEnv);

		if (domain != NULL &&
		    strlen(domain) + strlen(cbuf) + 1 < cbuflen)
		{
			p = &cbuf[strlen(cbuf)];
			*p++ = '.';
			strcpy(p, domain);
		}
	}
	return found;
}
/*
**  NDBM modules
*/

#ifdef NDBM

/*
**  NDBM_MAP_OPEN -- DBM-style map open
*/

bool
ndbm_map_open(map, mode)
	MAP *map;
	int mode;
{
	register DBM *dbm;
	struct stat st;
	int dfd;
	int pfd;
	int sff;
	int ret;
	int smode = S_IREAD;
	char dirfile[MAXNAME + 1];
	char pagfile[MAXNAME + 1];
	struct stat std, stp;

	if (tTd(38, 2))
		printf("ndbm_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);
	map->map_lockfd = -1;
	mode &= O_ACCMODE;

	/* do initial file and directory checks */
	snprintf(dirfile, sizeof dirfile, "%s.dir", map->map_file);
	snprintf(pagfile, sizeof pagfile, "%s.pag", map->map_file);
	sff = SFF_ROOTOK|SFF_REGONLY;
	if (mode == O_RDWR)
	{
		sff |= SFF_NOLINK|SFF_CREAT;
		smode = S_IWRITE;
	}
	else
	{
		sff |= SFF_NOWLINK;
	}
	if (FatalWritableDirs)
		sff |= SFF_SAFEDIRPATH;
	if ((ret = safefile(dirfile, RunAsUid, RunAsGid, RunAsUserName,
			    sff, smode, &std)) != 0 ||
	    (ret = safefile(pagfile, RunAsUid, RunAsGid, RunAsUserName,
			    sff, smode, &stp)) != 0)
	{
		/* cannot open this map */
		if (tTd(38, 2))
			printf("\tunsafe map file: %d\n", ret);
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("dbm map \"%s\": unsafe map file %s",
				map->map_mname, map->map_file);
		return FALSE;
	}
	if (std.st_mode == ST_MODE_NOFILE)
		mode |= O_CREAT|O_EXCL;

#if LOCK_ON_OPEN
	if (mode == O_RDONLY)
		mode |= O_SHLOCK;
	else
		mode |= O_TRUNC|O_EXLOCK;
#else
	if ((mode & O_ACCMODE) == O_RDWR)
	{
# if NOFTRUNCATE
		/*
		**  Warning: race condition.  Try to lock the file as
		**  quickly as possible after opening it.
		**	This may also have security problems on some systems,
		**	but there isn't anything we can do about it.
		*/

		mode |= O_TRUNC;
# else
		/*
		**  This ugly code opens the map without truncating it,
		**  locks the file, then truncates it.  Necessary to
		**  avoid race conditions.
		*/

		int dirfd;
		int pagfd;

		dirfd = safeopen(dirfile, mode, DBMMODE,
				 SFF_NOLINK|SFF_CREAT|SFF_OPENASROOT);
		pagfd = safeopen(pagfile, mode, DBMMODE,
				 SFF_NOLINK|SFF_CREAT|SFF_OPENASROOT);

		if (dirfd < 0 || pagfd < 0)
		{
			int save_errno = errno;

			if (dirfd >= 0)
				(void) close(dirfd);
			if (pagfd >= 0)
				(void) close(pagfd);
			errno = save_errno;
			syserr("ndbm_map_open: cannot create database %s",
				map->map_file);
			return FALSE;
		}
		if (ftruncate(dirfd, (off_t) 0) < 0 ||
		    ftruncate(pagfd, (off_t) 0) < 0)
		{
			int save_errno = errno;

			(void) close(dirfd);
			(void) close(pagfd);
			errno = save_errno;
			syserr("ndbm_map_open: cannot truncate %s.{dir,pag}",
				map->map_file);
			return FALSE;
		}

		/* if new file, get "before" bits for later filechanged check */
		if (std.st_mode == ST_MODE_NOFILE &&
		    (fstat(dirfd, &std) < 0 || fstat(pagfd, &stp) < 0))
		{
			int save_errno = errno;

			(void) close(dirfd);
			(void) close(pagfd);
			errno = save_errno;
			syserr("ndbm_map_open(%s.{dir,pag}): cannot fstat pre-opened file",
				map->map_file);
			return FALSE;
		}

		/* have to save the lock for the duration (bletch) */
		map->map_lockfd = dirfd;
		close(pagfd);

		/* twiddle bits for dbm_open */
		mode &= ~(O_CREAT|O_EXCL);
# endif
	}
#endif

	/* open the database */
	dbm = dbm_open(map->map_file, mode, DBMMODE);
	if (dbm == NULL)
	{
		int save_errno = errno;

		if (bitset(MF_ALIAS, map->map_mflags) &&
		    aliaswait(map, ".pag", FALSE))
			return TRUE;
#if !LOCK_ON_OPEN && !NOFTRUNCATE
		if (map->map_lockfd >= 0)
			close(map->map_lockfd);
#endif
		errno = save_errno;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("Cannot open DBM database %s", map->map_file);
		return FALSE;
	}
	dfd = dbm_dirfno(dbm);
	pfd = dbm_pagfno(dbm);
	if (dfd == pfd)
	{
		/* heuristic: if files are linked, this is actually gdbm */
		dbm_close(dbm);
#if !LOCK_ON_OPEN && !NOFTRUNCATE
		if (map->map_lockfd >= 0)
			close(map->map_lockfd);
#endif
		errno = 0;
		syserr("dbm map \"%s\": cannot support GDBM",
			map->map_mname);
		return FALSE;
	}

	if (filechanged(dirfile, dfd, &std, sff) ||
	    filechanged(pagfile, pfd, &stp, sff))
	{
		int save_errno = errno;

		dbm_close(dbm);
#if !LOCK_ON_OPEN && !NOFTRUNCATE
		if (map->map_lockfd >= 0)
			close(map->map_lockfd);
#endif
		errno = save_errno;
		syserr("ndbm_map_open(%s): file changed after open",
			map->map_file);
		return FALSE;
	}

	map->map_db1 = (ARBPTR_T) dbm;
	if (mode == O_RDONLY)
	{
#if LOCK_ON_OPEN
		if (dfd >= 0)
			(void) lockfile(dfd, map->map_file, ".dir", LOCK_UN);
		if (pfd >= 0)
			(void) lockfile(pfd, map->map_file, ".pag", LOCK_UN);
#endif
		if (bitset(MF_ALIAS, map->map_mflags) &&
		    !aliaswait(map, ".pag", TRUE))
			return FALSE;
	}
	else
	{
		map->map_mflags |= MF_LOCKED;
	}
	if (fstat(dfd, &st) >= 0)
		map->map_mtime = st.st_mtime;
	return TRUE;
}


/*
**  NDBM_MAP_LOOKUP -- look up a datum in a DBM-type map
*/

char *
ndbm_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	datum key, val;
	int fd;
	char keybuf[MAXNAME + 1];
	struct stat stbuf;

	if (tTd(38, 20))
		printf("ndbm_map_lookup(%s, %s)\n",
			map->map_mname, name);

	key.dptr = name;
	key.dsize = strlen(name);
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		if (key.dsize > sizeof keybuf - 1)
			key.dsize = sizeof keybuf - 1;
		bcopy(key.dptr, keybuf, key.dsize);
		keybuf[key.dsize] = '\0';
		makelower(keybuf);
		key.dptr = keybuf;
	}
lockdbm:
	fd = dbm_dirfno((DBM *) map->map_db1);
	if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(fd, map->map_file, ".dir", LOCK_SH);
	if (fd < 0 || fstat(fd, &stbuf) < 0 || stbuf.st_mtime > map->map_mtime) 
	{
		/* Reopen the database to sync the cache */
		int omode = bitset(map->map_mflags, MF_WRITABLE) ? O_RDWR
								 : O_RDONLY;

		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
		if (map->map_class->map_open(map, omode)) 
		{
			map->map_mflags |= MF_OPEN;
			if ((omode && O_ACCMODE) == O_RDWR)
				map->map_mflags |= MF_WRITABLE;
			goto lockdbm;
		}
		else
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags)) 
			{
				extern MAPCLASS BogusMapClass;

				*statp = EX_TEMPFAIL;
				map->map_class = &BogusMapClass;
				map->map_mflags |= MF_OPEN;
				syserr("Cannot reopen NDBM database %s",
					map->map_file);
			}
			return NULL;
		}
	}
	val.dptr = NULL;
	if (bitset(MF_TRY0NULL, map->map_mflags))
	{
		val = dbm_fetch((DBM *) map->map_db1, key);
		if (val.dptr != NULL)
			map->map_mflags &= ~MF_TRY1NULL;
	}
	if (val.dptr == NULL && bitset(MF_TRY1NULL, map->map_mflags))
	{
		key.dsize++;
		val = dbm_fetch((DBM *) map->map_db1, key);
		if (val.dptr != NULL)
			map->map_mflags &= ~MF_TRY0NULL;
	}
	if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(fd, map->map_file, ".dir", LOCK_UN);
	if (val.dptr == NULL)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, val.dptr, val.dsize, av);
}


/*
**  NDBM_MAP_STORE -- store a datum in the database
*/

void
ndbm_map_store(map, lhs, rhs)
	register MAP *map;
	char *lhs;
	char *rhs;
{
	datum key;
	datum data;
	int stat;
	char keybuf[MAXNAME + 1];

	if (tTd(38, 12))
		printf("ndbm_map_store(%s, %s, %s)\n",
			map->map_mname, lhs, rhs);

	key.dsize = strlen(lhs);
	key.dptr = lhs;
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		if (key.dsize > sizeof keybuf - 1)
			key.dsize = sizeof keybuf - 1;
		bcopy(key.dptr, keybuf, key.dsize);
		keybuf[key.dsize] = '\0';
		makelower(keybuf);
		key.dptr = keybuf;
	}

	data.dsize = strlen(rhs);
	data.dptr = rhs;

	if (bitset(MF_INCLNULL, map->map_mflags))
	{
		key.dsize++;
		data.dsize++;
	}

	stat = dbm_store((DBM *) map->map_db1, key, data, DBM_INSERT);
	if (stat > 0)
	{
		if (!bitset(MF_APPEND, map->map_mflags))
			message("050 Warning: duplicate alias name %s", lhs);
		else
		{
			static char *buf = NULL;
			static int bufsiz = 0;
			auto int xstat;
			datum old;

			old.dptr = ndbm_map_lookup(map, key.dptr, NULL, &xstat);
			if (old.dptr != NULL && *(char *) old.dptr != '\0')
			{
				old.dsize = strlen(old.dptr);
				if (data.dsize + old.dsize + 2 > bufsiz)
				{
					if (buf != NULL)
						(void) free(buf);
					bufsiz = data.dsize + old.dsize + 2;
					buf = xalloc(bufsiz);
				}
				snprintf(buf, bufsiz, "%s,%s",
					data.dptr, old.dptr);
				data.dsize = data.dsize + old.dsize + 1;
				data.dptr = buf;
				if (tTd(38, 9))
					printf("ndbm_map_store append=%s\n", data.dptr);
			}
		}
		stat = dbm_store((DBM *) map->map_db1, key, data, DBM_REPLACE);
	}
	if (stat != 0)
		syserr("readaliases: dbm put (%s)", lhs);
}


/*
**  NDBM_MAP_CLOSE -- close the database
*/

void
ndbm_map_close(map)
	register MAP  *map;
{
	if (tTd(38, 9))
		printf("ndbm_map_close(%s, %s, %x)\n",
			map->map_mname, map->map_file, map->map_mflags);

	if (bitset(MF_WRITABLE, map->map_mflags))
	{
#ifdef NDBM_YP_COMPAT
		bool inclnull;
		char buf[200];

		inclnull = bitset(MF_INCLNULL, map->map_mflags);
		map->map_mflags &= ~MF_INCLNULL;

		if (strstr(map->map_file, "/yp/") != NULL)
		{
			long save_mflags = map->map_mflags;

			map->map_mflags |= MF_NOFOLDCASE;

			(void) snprintf(buf, sizeof buf, "%010ld", curtime());
			ndbm_map_store(map, "YP_LAST_MODIFIED", buf);

			(void) gethostname(buf, sizeof buf);
			ndbm_map_store(map, "YP_MASTER_NAME", buf);

			map->map_mflags = save_mflags;
		}

		if (inclnull)
			map->map_mflags |= MF_INCLNULL;
#endif

		/* write out the distinguished alias */
		ndbm_map_store(map, "@", "@");
	}
	dbm_close((DBM *) map->map_db1);

	/* release lock (if needed) */
#if !LOCK_ON_OPEN
	if (map->map_lockfd >= 0)
		(void) close(map->map_lockfd);
#endif
}

#endif
/*
**  NEWDB (Hash and BTree) Modules
*/

#ifdef NEWDB

/*
**  BT_MAP_OPEN, HASH_MAP_OPEN -- database open primitives.
**
**	These do rather bizarre locking.  If you can lock on open,
**	do that to avoid the condition of opening a database that
**	is being rebuilt.  If you don't, we'll try to fake it, but
**	there will be a race condition.  If opening for read-only,
**	we immediately release the lock to avoid freezing things up.
**	We really ought to hold the lock, but guarantee that we won't
**	be pokey about it.  That's hard to do.
*/

extern bool	db_map_open __P((MAP *, int, char *, DBTYPE, const void *));

/* these should be K line arguments */
#ifndef DB_CACHE_SIZE
# define DB_CACHE_SIZE	(1024 * 1024)	/* database memory cache size */
#endif
#ifndef DB_HASH_NELEM
# define DB_HASH_NELEM	4096		/* (starting) size of hash table */
#endif

bool
bt_map_open(map, mode)
	MAP *map;
	int mode;
{
	BTREEINFO btinfo;

	if (tTd(38, 2))
		printf("bt_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	bzero(&btinfo, sizeof btinfo);
	btinfo.cachesize = DB_CACHE_SIZE;
	return db_map_open(map, mode, "btree", DB_BTREE, &btinfo);
}

bool
hash_map_open(map, mode)
	MAP *map;
	int mode;
{
	HASHINFO hinfo;

	if (tTd(38, 2))
		printf("hash_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	bzero(&hinfo, sizeof hinfo);
	hinfo.nelem = DB_HASH_NELEM;
	hinfo.cachesize = DB_CACHE_SIZE;
	return db_map_open(map, mode, "hash", DB_HASH, &hinfo);
}

bool
db_map_open(map, mode, mapclassname, dbtype, openinfo)
	MAP *map;
	int mode;
	char *mapclassname;
	DBTYPE dbtype;
	const void *openinfo;
{
	DB *db;
	int i;
	int omode;
	int smode = S_IREAD;
	int fd;
	int sff;
	int saveerrno;
	struct stat st;
	char buf[MAXNAME + 1];

	/* do initial file and directory checks */
	snprintf(buf, sizeof buf - 3, "%s", map->map_file);
	i = strlen(buf);
	if (i < 3 || strcmp(&buf[i - 3], ".db") != 0)
		(void) strcat(buf, ".db");

	mode &= O_ACCMODE;
	omode = mode;

	sff = SFF_ROOTOK|SFF_REGONLY;
	if (mode == O_RDWR)
	{
		sff |= SFF_NOLINK|SFF_CREAT;
		smode = S_IWRITE;
	}
	else
	{
		sff |= SFF_NOWLINK;
	}
	if (FatalWritableDirs)
		sff |= SFF_SAFEDIRPATH;
	if ((i = safefile(buf, RunAsUid, RunAsGid, RunAsUserName,
			  sff, smode, &st)) != 0)
	{
		/* cannot open this map */
		if (tTd(38, 2))
			printf("\tunsafe map file: %s\n", errstring(i));
		errno = i;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("%s map \"%s\": unsafe map file %s",
				mapclassname, map->map_mname, map->map_file);
		return FALSE;
	}
	if (st.st_mode == ST_MODE_NOFILE)
		omode |= O_CREAT|O_EXCL;

	map->map_lockfd = -1;

#if LOCK_ON_OPEN
	if (mode == O_RDWR)
		omode |= O_TRUNC|O_EXLOCK;
# if !OLD_NEWDB
	else
		omode |= O_SHLOCK;
# endif
#else
	/*
	**  Pre-lock the file to avoid race conditions.  In particular,
	**  since dbopen returns NULL if the file is zero length, we
	**  must have a locked instance around the dbopen.
	*/

	fd = open(buf, omode, DBMMODE);
	if (fd < 0)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("db_map_open: cannot pre-open database %s", buf);
		return FALSE;
	}

	/* make sure no baddies slipped in just before the open... */
	if (filechanged(buf, fd, &st, sff))
	{
		int save_errno = errno;

		(void) close(fd);
		errno = save_errno;
		syserr("db_map_open(%s): file changed after pre-open", buf);
		return FALSE;
	}

	/* if new file, get the "before" bits for later filechanged check */
	if (st.st_mode == ST_MODE_NOFILE && fstat(fd, &st) < 0)
	{
		int save_errno = errno;

		(void) close(fd);
		errno = save_errno;
		syserr("db_map_open(%s): cannot fstat pre-opened file",
			buf);
		return FALSE;
	}

	/* actually lock the pre-opened file */
	if (!lockfile(fd, buf, NULL, mode == O_RDONLY ? LOCK_SH : LOCK_EX))
		syserr("db_map_open: cannot lock %s", buf);

	/* set up mode bits for dbopen */
	if (mode == O_RDWR)
		omode |= O_TRUNC;
	omode &= ~(O_EXCL|O_CREAT);
#endif

	db = dbopen(buf, omode, DBMMODE, dbtype, openinfo);
	saveerrno = errno;

#if !LOCK_ON_OPEN
	if (mode == O_RDWR)
		map->map_lockfd = fd;
	else
		(void) close(fd);
#endif

	if (db == NULL)
	{
		if (mode == O_RDONLY && bitset(MF_ALIAS, map->map_mflags) &&
		    aliaswait(map, ".db", FALSE))
			return TRUE;
#if !LOCK_ON_OPEN
		if (map->map_lockfd >= 0)
			(void) close(map->map_lockfd);
#endif
		errno = saveerrno;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("Cannot open %s database %s",
				mapclassname, map->map_file);
		return FALSE;
	}

	if (filechanged(buf, db->fd(db), &st, sff))
	{
		int save_errno = errno;

		db->close(db);
#if !LOCK_ON_OPEN
		if (map->map_lockfd >= 0)
			close(map->map_lockfd);
#endif
		errno = save_errno;
		syserr("db_map_open(%s): file changed after open", buf);
		return FALSE;
	}

	if (mode == O_RDWR)
		map->map_mflags |= MF_LOCKED;
#if !OLD_NEWDB
	fd = db->fd(db);
# if LOCK_ON_OPEN
	if (fd >= 0 && mode == O_RDONLY)
	{
		(void) lockfile(fd, buf, NULL, LOCK_UN);
	}
# endif
#endif

	/* try to make sure that at least the database header is on disk */
	if (mode == O_RDWR)
#if OLD_NEWDB
		(void) db->sync(db);
#else
		(void) db->sync(db, 0);

	if (fd >= 0 && fstat(fd, &st) >= 0)
		map->map_mtime = st.st_mtime;
#endif

	map->map_db2 = (ARBPTR_T) db;
	if (mode == O_RDONLY && bitset(MF_ALIAS, map->map_mflags) &&
	    !aliaswait(map, ".db", TRUE))
		return FALSE;
	return TRUE;
}


/*
**  DB_MAP_LOOKUP -- look up a datum in a BTREE- or HASH-type map
*/

char *
db_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	DBT key, val;
	register DB *db = (DB *) map->map_db2;
	int i;
	int st;
	int saveerrno;
	int fd;
	struct stat stbuf;
	char keybuf[MAXNAME + 1];
	char buf[MAXNAME + 1];

	if (tTd(38, 20))
		printf("db_map_lookup(%s, %s)\n",
			map->map_mname, name);

	i = strlen(map->map_file);
	if (i > MAXNAME)
		i = MAXNAME;
	strncpy(buf, map->map_file, i);
	buf[i] = '\0';
	if (i > 3 && strcmp(&buf[i - 3], ".db") == 0)
		buf[i - 3] = '\0';

	key.size = strlen(name);
	if (key.size > sizeof keybuf - 1)
		key.size = sizeof keybuf - 1;
	key.data = keybuf;
	bcopy(name, keybuf, key.size);
	keybuf[key.size] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(keybuf);
#if !OLD_NEWDB
  lockdb:
	fd = db->fd(db);
	if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(fd, buf, ".db", LOCK_SH);
	if (fd < 0 || fstat(fd, &stbuf) < 0 || stbuf.st_mtime > map->map_mtime) 
	{
		/* Reopen the database to sync the cache */
		int omode = bitset(map->map_mflags, MF_WRITABLE) ? O_RDWR
								 : O_RDONLY;

		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
		if (map->map_class->map_open(map, omode)) 
		{
			map->map_mflags |= MF_OPEN;
			if ((omode && O_ACCMODE) == O_RDWR)
				map->map_mflags |= MF_WRITABLE;
			db = (DB *) map->map_db2;
			goto lockdb;
		}
		else
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags)) 
			{
				extern MAPCLASS BogusMapClass;

				*statp = EX_TEMPFAIL;
				map->map_class = &BogusMapClass;
				map->map_mflags |= MF_OPEN;
				syserr("Cannot reopen DB database %s",
					map->map_file);
			}
			return NULL;
		}
	}
#endif
	
	st = 1;
	if (bitset(MF_TRY0NULL, map->map_mflags))
	{
		st = db->get(db, &key, &val, 0);
		if (st == 0)
			map->map_mflags &= ~MF_TRY1NULL;
	}
	if (st != 0 && bitset(MF_TRY1NULL, map->map_mflags))
	{
		key.size++;
		st = db->get(db, &key, &val, 0);
		if (st == 0)
			map->map_mflags &= ~MF_TRY0NULL;
	}
	saveerrno = errno;
#if !OLD_NEWDB
	if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(fd, buf, ".db", LOCK_UN);
#endif
	if (st != 0)
	{
		errno = saveerrno;
		if (st < 0)
			syserr("db_map_lookup: get (%s)", name);
		return NULL;
	}
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, val.data, val.size, av);
}


/*
**  DB_MAP_STORE -- store a datum in the NEWDB database
*/

void
db_map_store(map, lhs, rhs)
	register MAP *map;
	char *lhs;
	char *rhs;
{
	int stat;
	DBT key;
	DBT data;
	register DB *db = map->map_db2;
	char keybuf[MAXNAME + 1];

	if (tTd(38, 12))
		printf("db_map_store(%s, %s, %s)\n",
			map->map_mname, lhs, rhs);

	key.size = strlen(lhs);
	key.data = lhs;
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		if (key.size > sizeof keybuf - 1)
			key.size = sizeof keybuf - 1;
		bcopy(key.data, keybuf, key.size);
		keybuf[key.size] = '\0';
		makelower(keybuf);
		key.data = keybuf;
	}

	data.size = strlen(rhs);
	data.data = rhs;

	if (bitset(MF_INCLNULL, map->map_mflags))
	{
		key.size++;
		data.size++;
	}

	stat = db->put(db, &key, &data, R_NOOVERWRITE);
	if (stat > 0)
	{
		if (!bitset(MF_APPEND, map->map_mflags))
			message("050 Warning: duplicate alias name %s", lhs);
		else
		{
			static char *buf = NULL;
			static int bufsiz = 0;
			DBT old;

			old.data = db_map_lookup(map, key.data, NULL, &stat);
			if (old.data != NULL)
			{
				old.size = strlen(old.data);
				if (data.size + old.size + 2 > bufsiz)
				{
					if (buf != NULL)
						(void) free(buf);
					bufsiz = data.size + old.size + 2;
					buf = xalloc(bufsiz);
				}
				snprintf(buf, bufsiz, "%s,%s",
					data.data, old.data);
				data.size = data.size + old.size + 1;
				data.data = buf;
				if (tTd(38, 9))
					printf("db_map_store append=%s\n",
					       (char *) data.data);
			}
		}
		stat = db->put(db, &key, &data, 0);
	}
	if (stat != 0)
		syserr("readaliases: db put (%s)", lhs);
}


/*
**  DB_MAP_CLOSE -- add distinguished entries and close the database
*/

void
db_map_close(map)
	MAP *map;
{
	register DB *db = map->map_db2;

	if (tTd(38, 9))
		printf("db_map_close(%s, %s, %lx)\n",
			map->map_mname, map->map_file, map->map_mflags);

	if (bitset(MF_WRITABLE, map->map_mflags))
	{
		/* write out the distinguished alias */
		db_map_store(map, "@", "@");
	}

#if OLD_NEWDB
	(void) db->sync(db);
#else
	(void) db->sync(db, 0);
#endif

#if !LOCK_ON_OPEN
	if (map->map_lockfd >= 0)
		(void) close(map->map_lockfd);
#endif

	if (db->close(db) != 0)
		syserr("readaliases: db close failure");
}

#endif
/*
**  NIS Modules
*/

# ifdef NIS

# ifndef YPERR_BUSY
#  define YPERR_BUSY	16
# endif

/*
**  NIS_MAP_OPEN -- open DBM map
*/

bool
nis_map_open(map, mode)
	MAP *map;
	int mode;
{
	int yperr;
	register char *p;
	auto char *vp;
	auto int vsize;

	if (tTd(38, 2))
		printf("nis_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
#ifdef ENOSYS
		errno = ENOSYS;
#else
# ifdef EFTYPE
		errno = EFTYPE;
# else
		errno = ENXIO;
# endif
#endif
		return FALSE;
	}

	p = strchr(map->map_file, '@');
	if (p != NULL)
	{
		*p++ = '\0';
		if (*p != '\0')
			map->map_domain = p;
	}

	if (*map->map_file == '\0')
		map->map_file = "mail.aliases";

	if (map->map_domain == NULL)
	{
		yperr = yp_get_default_domain(&map->map_domain);
		if (yperr != 0)
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags))
				syserr("421 NIS map %s specified, but NIS not running",
					map->map_file);
			return FALSE;
		}
	}

	/* check to see if this map actually exists */
	yperr = yp_match(map->map_domain, map->map_file, "@", 1,
			&vp, &vsize);
	if (tTd(38, 10))
		printf("nis_map_open: yp_match(@, %s, %s) => %s\n",
			map->map_domain, map->map_file, yperr_string(yperr));
	if (yperr == 0 || yperr == YPERR_KEY || yperr == YPERR_BUSY)
	{
		/*
		**  We ought to be calling aliaswait() here if this is an
		**  alias file, but powerful HP-UX NIS servers  apparently
		**  don't insert the @:@ token into the alias map when it
		**  is rebuilt, so aliaswait() just hangs.  I hate HP-UX.
		*/

#if 0
		if (!bitset(MF_ALIAS, map->map_mflags) ||
		    aliaswait(map, NULL, TRUE))
#endif
			return TRUE;
	}

	if (!bitset(MF_OPTIONAL, map->map_mflags))
	{
		syserr("421 Cannot bind to map %s in domain %s: %s",
			map->map_file, map->map_domain, yperr_string(yperr));
	}

	return FALSE;
}


/*
**  NIS_MAP_LOOKUP -- look up a datum in a NIS map
*/

char *
nis_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *vp;
	auto int vsize;
	int buflen;
	int yperr;
	char keybuf[MAXNAME + 1];

	if (tTd(38, 20))
		printf("nis_map_lookup(%s, %s)\n",
			map->map_mname, name);

	buflen = strlen(name);
	if (buflen > sizeof keybuf - 1)
		buflen = sizeof keybuf - 1;
	bcopy(name, keybuf, buflen);
	keybuf[buflen] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(keybuf);
	yperr = YPERR_KEY;
	if (bitset(MF_TRY0NULL, map->map_mflags))
	{
		yperr = yp_match(map->map_domain, map->map_file, keybuf, buflen,
			     &vp, &vsize);
		if (yperr == 0)
			map->map_mflags &= ~MF_TRY1NULL;
	}
	if (yperr == YPERR_KEY && bitset(MF_TRY1NULL, map->map_mflags))
	{
		buflen++;
		yperr = yp_match(map->map_domain, map->map_file, keybuf, buflen,
			     &vp, &vsize);
		if (yperr == 0)
			map->map_mflags &= ~MF_TRY0NULL;
	}
	if (yperr != 0)
	{
		if (yperr != YPERR_KEY && yperr != YPERR_BUSY)
			map->map_mflags &= ~(MF_VALID|MF_OPEN);
		return NULL;
	}
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, vp, vsize, av);
}


/*
**  NIS_GETCANONNAME -- look up canonical name in NIS
*/

bool
nis_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	char *vp;
	auto int vsize;
	int keylen;
	int yperr;
	static bool try0null = TRUE;
	static bool try1null = TRUE;
	static char *yp_domain = NULL;
	char host_record[MAXLINE];
	char cbuf[MAXNAME];
	char nbuf[MAXNAME + 1];

	if (tTd(38, 20))
		printf("nis_getcanonname(%s)\n", name);

	if (strlen(name) >= sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strcpy(nbuf, name);
	shorten_hostname(nbuf);
	keylen = strlen(nbuf);

	if (yp_domain == NULL)
		yp_get_default_domain(&yp_domain);
	makelower(nbuf);
	yperr = YPERR_KEY;
	if (try0null)
	{
		yperr = yp_match(yp_domain, "hosts.byname", nbuf, keylen,
			     &vp, &vsize);
		if (yperr == 0)
			try1null = FALSE;
	}
	if (yperr == YPERR_KEY && try1null)
	{
		keylen++;
		yperr = yp_match(yp_domain, "hosts.byname", nbuf, keylen,
			     &vp, &vsize);
		if (yperr == 0)
			try0null = FALSE;
	}
	if (yperr != 0)
	{
		if (yperr == YPERR_KEY)
			*statp = EX_NOHOST;
		else if (yperr == YPERR_BUSY)
			*statp = EX_TEMPFAIL;
		else
			*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	if (vsize >= sizeof host_record)
		vsize = sizeof host_record - 1;
	strncpy(host_record, vp, vsize);
	host_record[vsize] = '\0';
	if (tTd(38, 44))
		printf("got record `%s'\n", host_record);
	if (!extract_canonname(nbuf, host_record, cbuf, sizeof cbuf))
	{
		/* this should not happen, but.... */
		*statp = EX_NOHOST;
		return FALSE;
	}
	if (hbsize < strlen(cbuf))
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	strcpy(name, cbuf);
	*statp = EX_OK;
	return TRUE;
}

#endif
/*
**  NISPLUS Modules
**
**	This code donated by Sun Microsystems.
*/

#ifdef NISPLUS

#undef NIS		/* symbol conflict in nis.h */
#undef T_UNSPEC		/* symbol conflict in nis.h -> ... -> sys/tiuser.h */
#include <rpcsvc/nis.h>
#include <rpcsvc/nislib.h>

#define EN_col(col)	zo_data.objdata_u.en_data.en_cols.en_cols_val[(col)].ec_value.ec_value_val
#define COL_NAME(res,i)	((res->objects.objects_val)->TA_data.ta_cols.ta_cols_val)[i].tc_name
#define COL_MAX(res)	((res->objects.objects_val)->TA_data.ta_cols.ta_cols_len)
#define PARTIAL_NAME(x)	((x)[strlen(x) - 1] != '.')

/*
**  NISPLUS_MAP_OPEN -- open nisplus table
*/

bool
nisplus_map_open(map, mode)
	MAP *map;
	int mode;
{
	nis_result *res = NULL;
	int retry_cnt, max_col, i;
	char qbuf[MAXLINE + NIS_MAXNAMELEN];

	if (tTd(38, 2))
		printf("nisplus_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		errno = ENODEV;
		return FALSE;
	}

	if (*map->map_file == '\0')
		map->map_file = "mail_aliases.org_dir";

	if (PARTIAL_NAME(map->map_file) && map->map_domain == NULL)
	{
		/* set default NISPLUS Domain to $m */
		extern char *nisplus_default_domain();

		map->map_domain = newstr(nisplus_default_domain());
		if (tTd(38, 2))
			printf("nisplus_map_open(%s): using domain %s\n",
				 map->map_file, map->map_domain);
	}
	if (!PARTIAL_NAME(map->map_file))
	{
		map->map_domain = newstr("");
		snprintf(qbuf, sizeof qbuf, "%s", map->map_file);
	}
	else
	{
		/* check to see if this map actually exists */
		snprintf(qbuf, sizeof qbuf, "%s.%s",
			map->map_file, map->map_domain);
	}

	retry_cnt = 0;
	while (res == NULL || res->status != NIS_SUCCESS)
	{
		res = nis_lookup(qbuf, FOLLOW_LINKS);
		switch (res->status)
		{
		  case NIS_SUCCESS:
			break;

		  case NIS_TRYAGAIN:
		  case NIS_RPCERROR:
		  case NIS_NAMEUNREACHABLE:
			if (retry_cnt++ > 4)
			{
				errno = EBADR;
				return FALSE;
			}
			/* try not to overwhelm hosed server */
			sleep(2);
			break;

		  default:		/* all other nisplus errors */
#if 0
			if (!bitset(MF_OPTIONAL, map->map_mflags))
				syserr("421 Cannot find table %s.%s: %s",
					map->map_file, map->map_domain,
					nis_sperrno(res->status));
#endif
			errno = EBADR;
			return FALSE;
		}
	}

	if (NIS_RES_NUMOBJ(res) != 1 ||
	    (NIS_RES_OBJECT(res)->zo_data.zo_type != TABLE_OBJ))
	{
		if (tTd(38, 10))
			printf("nisplus_map_open: %s is not a table\n", qbuf);
#if 0
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("421 %s.%s: %s is not a table",
				map->map_file, map->map_domain,
				nis_sperrno(res->status));
#endif
		errno = EBADR;
		return FALSE;
	}
	/* default key column is column 0 */
	if (map->map_keycolnm == NULL)
		map->map_keycolnm = newstr(COL_NAME(res,0));

	max_col = COL_MAX(res);
	
	/* verify the key column exist */
	for (i=0; i< max_col; i++)
	{
		if (!strcmp(map->map_keycolnm, COL_NAME(res,i)))
			break;
	}
	if (i == max_col)
	{
		if (tTd(38, 2))
			printf("nisplus_map_open(%s): can not find key column %s\n",
				map->map_file, map->map_keycolnm);
		errno = EBADR;
		return FALSE;
	}

	/* default value column is the last column */
	if (map->map_valcolnm == NULL)
	{
		map->map_valcolno = max_col - 1;
		return TRUE;
	}

	for (i=0; i< max_col; i++)
	{
		if (strcmp(map->map_valcolnm, COL_NAME(res,i)) == 0)
		{
			map->map_valcolno = i;
			return TRUE;
		}
	}

	if (tTd(38, 2))
		printf("nisplus_map_open(%s): can not find column %s\n",
			 map->map_file, map->map_keycolnm);
	errno = EBADR;
	return FALSE;
}


/*
**  NISPLUS_MAP_LOOKUP -- look up a datum in a NISPLUS table
*/

char *
nisplus_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *p;
	auto int vsize;
	char *skp;
	int skleft;
	char search_key[MAXNAME + 4];
	char qbuf[MAXLINE + NIS_MAXNAMELEN];
	nis_result *result;

	if (tTd(38, 20))
		printf("nisplus_map_lookup(%s, %s)\n",
			map->map_mname, name);

	if (!bitset(MF_OPEN, map->map_mflags))
	{
		if (nisplus_map_open(map, O_RDONLY))
			map->map_mflags |= MF_OPEN;
		else
		{
			*statp = EX_UNAVAILABLE;
			return NULL;
		}
	}
		
	/*
	**  Copy the name to the key buffer, escaping double quote characters
	**  by doubling them and quoting "]" and "," to avoid having the
	**  NIS+ parser choke on them.
	*/

	skleft = sizeof search_key - 4;
	skp = search_key;
	for (p = name; *p != '\0' && skleft > 0; p++)
	{
		switch (*p)
		{
		  case ']':
		  case ',':
			/* quote the character */
			*skp++ = '"';
			*skp++ = *p;
			*skp++ = '"';
			skleft -= 3;
			break;

		  case '"':
			/* double the quote */
			*skp++ = '"';
			skleft--;
			/* fall through... */

		  default:
			*skp++ = *p;
			skleft--;
			break;
		}
	}
	*skp = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(search_key);

	/* construct the query */
	if (PARTIAL_NAME(map->map_file))
		snprintf(qbuf, sizeof qbuf, "[%s=%s],%s.%s",
			map->map_keycolnm, search_key, map->map_file,
			map->map_domain);
	else
		snprintf(qbuf, sizeof qbuf, "[%s=%s],%s",
			map->map_keycolnm, search_key, map->map_file);

	if (tTd(38, 20))
		printf("qbuf=%s\n", qbuf);
	result = nis_list(qbuf, FOLLOW_LINKS | FOLLOW_PATH, NULL, NULL);
	if (result->status == NIS_SUCCESS)
	{
		int count;
		char *str;

		if ((count = NIS_RES_NUMOBJ(result)) != 1)
		{
			if (LogLevel > 10)
				sm_syslog(LOG_WARNING, CurEnv->e_id,
				  "%s: lookup error, expected 1 entry, got %d",
				    map->map_file, count);

			/* ignore second entry */
			if (tTd(38, 20))
				printf("nisplus_map_lookup(%s), got %d entries, additional entries ignored\n",
					name, count);
		}

		p = ((NIS_RES_OBJECT(result))->EN_col(map->map_valcolno));
		/* set the length of the result */
		if (p == NULL)
			p = "";
		vsize = strlen(p);
		if (tTd(38, 20))
			printf("nisplus_map_lookup(%s), found %s\n",
				name, p);
		if (bitset(MF_MATCHONLY, map->map_mflags))
			str = map_rewrite(map, name, strlen(name), NULL);
		else
			str = map_rewrite(map, p, vsize, av);
		nis_freeresult(result);
		*statp = EX_OK;
		return str;
	}
	else
	{
		if (result->status == NIS_NOTFOUND)
			*statp = EX_NOTFOUND;
		else if (result->status == NIS_TRYAGAIN)
			*statp = EX_TEMPFAIL;
		else
		{
			*statp = EX_UNAVAILABLE;
			map->map_mflags &= ~(MF_VALID|MF_OPEN);
		}
	}
	if (tTd(38, 20))
		printf("nisplus_map_lookup(%s), failed\n", name);
	nis_freeresult(result);
	return NULL;
}



/*
**  NISPLUS_GETCANONNAME -- look up canonical name in NIS+
*/

bool
nisplus_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	char *vp;
	auto int vsize;
	nis_result *result;
	char *p;
	char nbuf[MAXNAME + 1];
	char qbuf[MAXLINE + NIS_MAXNAMELEN];

	if (strlen(name) >= sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strcpy(nbuf, name);
	shorten_hostname(nbuf);

	p = strchr(nbuf, '.');
	if (p == NULL)
	{
		/* single token */
		snprintf(qbuf, sizeof qbuf, "[name=%s],hosts.org_dir", nbuf);
	}
	else if (p[1] != '\0')
	{
		/* multi token -- take only first token in nbuf */
		*p = '\0';
		snprintf(qbuf, sizeof qbuf, "[name=%s],hosts.org_dir.%s",
			nbuf, &p[1]);
	}
	else
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	if (tTd(38, 20))
		printf("\nnisplus_getcanoname(%s), qbuf=%s\n",
			 name, qbuf);

	result = nis_list(qbuf, EXPAND_NAME|FOLLOW_LINKS|FOLLOW_PATH,
		NULL, NULL);

	if (result->status == NIS_SUCCESS)
	{
		int count;
		char *domain;

		if ((count = NIS_RES_NUMOBJ(result)) != 1)
		{
			if (LogLevel > 10)
				sm_syslog(LOG_WARNING, CurEnv->e_id,
				       "nisplus_getcanonname: lookup error, expected 1 entry, got %d",
				       count);

			/* ignore second entry */
			if (tTd(38, 20))
				printf("nisplus_getcanoname(%s), got %d entries, all but first ignored\n",
					name, count);
		}

		if (tTd(38, 20))
			printf("nisplus_getcanoname(%s), found in directory \"%s\"\n",
			       name, (NIS_RES_OBJECT(result))->zo_domain);


		vp = ((NIS_RES_OBJECT(result))->EN_col(0));
		vsize = strlen(vp);
		if (tTd(38, 20))
			printf("nisplus_getcanonname(%s), found %s\n",
				name, vp);
		if (strchr(vp, '.') != NULL)
		{
			domain = "";
		}
		else
		{
			domain = macvalue('m', CurEnv);
			if (domain == NULL)
				domain = "";
		}
		if (hbsize > vsize + (int) strlen(domain) + 1)
		{
			if (domain[0] == '\0')
				strcpy(name, vp);
			else
				snprintf(name, hbsize, "%s.%s", vp, domain);
			*statp = EX_OK;
		}
		else
			*statp = EX_NOHOST;
		nis_freeresult(result);
		return TRUE;
	}
	else
	{
		if (result->status == NIS_NOTFOUND)
			*statp = EX_NOHOST;
		else if (result->status == NIS_TRYAGAIN)
			*statp = EX_TEMPFAIL;
		else
			*statp = EX_UNAVAILABLE;
	}
	if (tTd(38, 20))
		printf("nisplus_getcanonname(%s), failed, status=%d, nsw_stat=%d\n",
			name, result->status, *statp);
	nis_freeresult(result);
	return FALSE;
}


char *
nisplus_default_domain()
{
	static char default_domain[MAXNAME + 1] = "";
	char *p;

	if (default_domain[0] != '\0')
		return(default_domain);
	
	p = nis_local_directory();
	snprintf(default_domain, sizeof default_domain, "%s", p);
	return default_domain;
}

#endif /* NISPLUS */
/*
**  LDAP Modules
**
**	Contributed by Booker C. Bense <bbense@networking.stanford.edu>.
**	Get your support from him.
*/

#ifdef LDAPMAP

# undef NEEDGETOPT		/* used for something else in LDAP */

# include <lber.h>
# include <ldap.h>
# include "ldap_map.h"

/*
**  LDAP_MAP_OPEN -- open LDAP map
**
**	Since LDAP is TCP-based there is not much we can or should do
**	here.  It might be a good idea to attempt an open/close here.
*/

bool
ldap_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		printf("ldap_map_open(%s, %d)\n", map->map_mname, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
#ifdef ENOSYS
		errno = ENOSYS;
#else
# ifdef EFTYPE
		errno = EFTYPE;
# else
		errno = ENXIO;
# endif
#endif
		return FALSE;
	}
	return TRUE;
}


/*
**  LDAP_MAP_START -- actually open LDAP map
**
**	Caching should be investigated.
*/

bool
ldap_map_start(map)
	MAP *map;
{
	LDAP_MAP_STRUCT *lmap;
	LDAP *ld;

	if (tTd(38, 2))
		printf("ldap_map_start(%s)\n", map->map_mname);

	lmap = (LDAP_MAP_STRUCT *) map->map_db1;

	if (tTd(38,9))
		printf("ldap_open(%s, %d)\n", lmap->ldaphost, lmap->ldapport);

	if ((ld = ldap_open(lmap->ldaphost,lmap->ldapport)) == NULL)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			syserr("ldapopen failed to %s in map %s",
				lmap->ldaphost, map->map_mname);
		}
		return FALSE;
	}

	ld->ld_deref = lmap->deref;
	ld->ld_timelimit = lmap->timelimit;
	ld->ld_sizelimit = lmap->sizelimit;
	ld->ld_options = lmap->ldap_options;

	if (ldap_bind_s(ld, lmap->binddn,lmap->passwd,lmap->method) != LDAP_SUCCESS)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			syserr("421 Cannot bind to map %s in ldap server %s",
				map->map_mname, lmap->ldaphost);
		}
	}
	else
	{
		/* We need to cast ld into the map structure */
		lmap->ld = ld;
		return TRUE;
	}

	return FALSE;
}


/*
** LDAP_MAP_CLOSE -- close ldap map
*/

void
ldap_map_close(map)
	MAP *map;
{
	LDAP_MAP_STRUCT *lmap ;
	lmap = (LDAP_MAP_STRUCT *) map->map_db1;
	if (lmap->ld != NULL)
		ldap_unbind(lmap->ld);
}


#ifdef SUNET_ID
/*
** SUNET_ID_HASH -- Convert a string to it's Sunet_id canonical form
** This only makes sense at Stanford University.
*/

char *
sunet_id_hash(str)
	char *str;
{
	char *p, *p_last;

	p = str;
	p_last = p;
	while (*p != '\0')
	{
		if (islower(*p) || isdigit(*p))
		{
			*p_last = *p;
			p_last++;
		}
		else if (isupper(*p))
		{
			*p_last = tolower(*p);
			p_last++;
		}
		++p;
	}
	if (*p_last != '\0')
		*p_last = '\0';
	return (str);
}



#endif /* SUNET_ID */
/*
** LDAP_MAP_LOOKUP -- look up a datum in a LDAP map
*/

char *
ldap_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	LDAP_MAP_STRUCT *lmap = NULL;
	LDAPMessage *entry;
	char *vp;
	auto int vsize;
	char keybuf[MAXNAME + 1];
	char filter[LDAP_MAP_MAX_FILTER + 1];
	char **attr_values = NULL;
	char *result;
	int name_len;

	if (tTd(38, 20))
		printf("ldap_map_lookup(%s, %s)\n", map->map_mname, name);

	/* actually open the map */
	if (!ldap_map_start(map))
	{
		result = NULL;
		*statp = EX_TEMPFAIL;
		goto quick_exit;
	}

	/* Get ldap struct pointer from map */
	lmap = (LDAP_MAP_STRUCT *) map->map_db1;

	name_len = strlen(name);
	if (name_len > MAXNAME)
		name_len = MAXNAME;
	strncpy(keybuf, name, name_len);
	keybuf[name_len] = '\0';

	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
#ifdef SUNET_ID
		sunet_id_hash(keybuf);
#else
		makelower(keybuf);
#endif /*SUNET_ID */

	/* sprintf keybuf into filter */
	snprintf(filter, sizeof filter, lmap->filter, keybuf);

	if (ldap_search_st(lmap->ld, lmap->base,lmap->scope,filter,
			   lmap->attr, lmap->attrsonly, &(lmap->timeout),
			   &(lmap->res)) != LDAP_SUCCESS)
	{
		/* try close/opening map */
		ldap_map_close(map);
		if (!ldap_map_start(map))
		{
			result = NULL;
			*statp = EX_TEMPFAIL;
			goto quick_exit;
		}
		if (ldap_search_st(lmap->ld, lmap->base, lmap->scope, filter,
				   lmap->attr, lmap->attrsonly,
				   &(lmap->timeout), &(lmap->res))
			!= LDAP_SUCCESS)
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags))
			{
				syserr("Error in ldap_search_st using %s in map %s",
					filter, map->map_mname);
			}
			result = NULL;
			*statp = EX_TEMPFAIL;
			goto quick_exit;
		}
	}

	entry = ldap_first_entry(lmap->ld,lmap->res);
	if (entry == NULL)
	{
	        result = NULL;
		*statp = EX_NOTFOUND;
		goto quick_exit;
	}

	/* Need to build the args for map_rewrite here */
	attr_values = ldap_get_values(lmap->ld,entry,lmap->attr[0]);
	if (attr_values == NULL)
	{
		/* bad things happened */
		result = NULL;
		*statp = EX_NOTFOUND;
		goto quick_exit;
	}

	*statp = EX_OK;

	/* If there is more that one use the first */
	vp = attr_values[0];
	vsize = strlen(vp);

	if (LogLevel > 9)
		sm_syslog(LOG_INFO, CurEnv->e_id,
			"ldap %.100s => %s",
			name, vp);
	if (bitset(MF_MATCHONLY, map->map_mflags))
		result = map_rewrite(map, name, strlen(name), NULL);
	else
		result = map_rewrite(map, vp, vsize, av);

  quick_exit:
	if (attr_values != NULL)
		ldap_value_free(attr_values);
	if (lmap != NULL)
		ldap_msgfree(lmap->res);
	ldap_map_close(map);
	return result ;
}


/*
** LDAP_MAP_DEQUOTE - helper routine for ldap_map_parseargs
*/

char *
ldap_map_dequote(str)
	char *str;
{
	char *p;
	char *start;
	p = str;

	if (*p == '"')
	{
		start = ++p;
		/* Should probably swallow initial whitespace here */
	}
	else
	{
		return(str);
	}
	while (*p != '"' && *p != '\0')
	{
		p++;
	}
	if (*p != '\0')
		*p = '\0';
	return start;
}

/*
** LDAP_MAP_PARSEARGS -- parse ldap map definition args.
*/

bool
ldap_map_parseargs(map,args)
	MAP *map;
	char *args;
{
	register char *p = args;
	register int done;
	LDAP_MAP_STRUCT *lmap;

	/* We need to alloc an LDAP_MAP_STRUCT struct */
	lmap  = (LDAP_MAP_STRUCT *) xalloc(sizeof(LDAP_MAP_STRUCT));

	/* Set default int's here , default strings below */
	lmap->ldapport =  DEFAULT_LDAP_MAP_PORT;
	lmap->deref = DEFAULT_LDAP_MAP_DEREF;
	lmap->timelimit = DEFAULT_LDAP_MAP_TIMELIMIT;
	lmap->sizelimit = DEFAULT_LDAP_MAP_SIZELIMIT;
	lmap->ldap_options = DEFAULT_LDAP_MAP_LDAP_OPTIONS;
	lmap->method = DEFAULT_LDAP_MAP_METHOD;
	lmap->scope = DEFAULT_LDAP_MAP_SCOPE;
	lmap->attrsonly = DEFAULT_LDAP_MAP_ATTRSONLY;
	lmap->timeout.tv_sec = DEFAULT_LDAP_MAP_TIMELIMIT;
	lmap->timeout.tv_usec = 0;

	/* Default char ptrs to NULL */
	lmap->binddn = NULL;
	lmap->passwd = NULL;
	lmap->base   = NULL;
	lmap->ldaphost = NULL;
	map->map_mflags |= MF_TRY0NULL | MF_TRY1NULL;
	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'N':
			map->map_mflags |= MF_INCLNULL;
			map->map_mflags &= ~MF_TRY0NULL;
			break;

		  case 'O':
			map->map_mflags &= ~MF_TRY1NULL;
			break;

		  case 'o':
			map->map_mflags |= MF_OPTIONAL;
			break;

		  case 'f':
			map->map_mflags |= MF_NOFOLDCASE;
			break;

		  case 'm':
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 'A':
			map->map_mflags |= MF_APPEND;
			break;

		  case 'q':
			map->map_mflags |= MF_KEEPQUOTES;
			break;

		  case 't':
			map->map_mflags |= MF_NODEFER;
			break;

		  case 'a':
			map->map_app = ++p;
			break;

			/* Start of ldap_map specific args */
		  case 'k':		/* search field */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->filter = p;
			break;

		  case 'v':		/* attr to return */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->attr[0] = p;
			lmap->attr[1] = NULL;
			break;

			/* args stolen from ldapsearch.c */
		  case 'R':		/* don't auto chase referrals */
#ifdef LDAP_REFERRALS
			lmap->ldap_options &= ~LDAP_OPT_REFERRALS;
#else  /* LDAP_REFERRALS */
			syserr("compile with -DLDAP_REFERRALS for referral support\n");
#endif /* LDAP_REFERRALS */
			break;

		  case 'n':		/* retrieve attribute names only -- no values */
			lmap->attrsonly += 1;
			break;

		  case 's':		/* search scope */
			if (strncasecmp(p, "base", 4) == 0)
			{
				lmap->scope = LDAP_SCOPE_BASE;
			}
			else if (strncasecmp(p, "one", 3) == 0)
			{
				lmap->scope = LDAP_SCOPE_ONELEVEL;
			}
			else if (strncasecmp(p, "sub", 3) == 0)
			{
				lmap->scope = LDAP_SCOPE_SUBTREE;
			}
			else
			{		/* bad config line */
				if (!bitset(MCF_OPTFILE, map->map_class->map_cflags))
				{
					syserr("Scope must be [base|one|sub] not %s in map %s",
						p, map->map_mname);
					return FALSE;
				}
			}
			break;

		  case 'h':		/* ldap host */
			while (isascii(*++p) && isspace(*p))
				continue;
			map->map_domain = p;
			lmap->ldaphost = p;
			break;

		  case 'b':		/* search base */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->base = p;
			break;

		  case 'p':		/* ldap port */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldapport = atoi(p);
			break;

		  case 'l':		/* time limit */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->timelimit = atoi(p);
			break;

		}

		/* need to account for quoted strings here arggg... */
		done =  isascii(*p) && isspace(*p);
		while (*p != '\0' && !done)
		{
			if (*p == '"')
			{
				while (*++p != '"' && *p != '\0')
				{
					continue;
				}
				if (*p != '\0')
					p++;
			}
			else
			{
				p++;
			}
			done = isascii(*p) && isspace(*p);
		}

		if (*p != '\0')
			*p++ = '\0';
	}

	if (map->map_app != NULL)
		map->map_app = newstr(ldap_map_dequote(map->map_app));

	if (map->map_domain != NULL)
		map->map_domain = newstr(ldap_map_dequote(map->map_domain));

	/*
	** We need to swallow up all the stuff into a struct
	** and dump it into map->map_dbptr1
	*/

	if (lmap->ldaphost != NULL)
		lmap->ldaphost = newstr(ldap_map_dequote(lmap->ldaphost));
	else
	{
		syserr("LDAP map: -h flag is required");
		return FALSE;
	}

	if (lmap->binddn != NULL)
		lmap->binddn = newstr(ldap_map_dequote(lmap->binddn));
	else
		lmap->binddn = DEFAULT_LDAP_MAP_BINDDN;


	if (lmap->passwd != NULL)
		lmap->passwd = newstr(ldap_map_dequote(lmap->passwd));
	else
		lmap->passwd = DEFAULT_LDAP_MAP_PASSWD;

	if (lmap->base != NULL)
		lmap->base = newstr(ldap_map_dequote(lmap->base));
	else
	{
		syserr("LDAP map: -b flag is required");
		return FALSE;
	}


	if (lmap->filter != NULL)
		lmap->filter = newstr(ldap_map_dequote(lmap->filter));
	else
	{
		if (!bitset(MCF_OPTFILE, map->map_class->map_cflags))
		{
			syserr("No filter given in map %s", map->map_mname);
			return FALSE;
		}
	}
	if (lmap->attr[0] != NULL)
		lmap->attr[0] = newstr(ldap_map_dequote(lmap->attr[0]));
	else
	{
		if (!bitset(MCF_OPTFILE, map->map_class->map_cflags))
		{
			syserr("No return attribute in %s", map->map_mname);
			return FALSE;
		}
	}

	map->map_db1 = (ARBPTR_T) lmap;
	return TRUE;
}

#endif /* LDAP Modules */
/*
** syslog map
*/

#if _FFR_SYSLOG_MAP

#define map_prio	map_lockfd	/* overload field */

/*
** SYSLOG_MAP_PARSEARGS -- check for priority level to syslog messages.
*/

bool
syslog_map_parseargs(map, args)
	MAP *map;
	char *args;
{
	char *p = args;
	char *priority = NULL;

	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		if (*++p == 'L')
			priority = ++p;
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}

	if (priority == NULL)
		map->map_prio = LOG_INFO;
	else
	{
		if (strncasecmp("LOG_", priority, 4) == 0)
			priority += 4;
		
#ifdef LOG_EMERG
		if (strcasecmp("EMERG", priority) == 0)
			map->map_prio = LOG_EMERG;
		else
#endif
#ifdef LOG_ALERT
		if (strcasecmp("ALERT", priority) == 0)
			map->map_prio = LOG_ALERT;
		else
#endif
#ifdef LOG_CRIT
		if (strcasecmp("CRIT", priority) == 0)
			map->map_prio = LOG_CRIT;
		else
#endif
#ifdef LOG_ERR
		if (strcasecmp("ERR", priority) == 0)
			map->map_prio = LOG_ERR;
		else
#endif
#ifdef LOG_WARNING
		if (strcasecmp("WARNING", priority) == 0)
			map->map_prio = LOG_WARNING;
		else
#endif
#ifdef LOG_NOTICE
		if (strcasecmp("NOTICE", priority) == 0)
			map->map_prio = LOG_NOTICE;
		else
#endif
#ifdef LOG_INFO
		if (strcasecmp("INFO", priority) == 0)
			map->map_prio = LOG_INFO;
		else
#endif
#ifdef LOG_DEBUG
		if (strcasecmp("DEBUG", priority) == 0)
			map->map_prio = LOG_DEBUG;
		else
#endif
		{
			syserr("syslog_map_parseargs: Unknown priority %s\n",
			       priority);
			return FALSE;
		}
	}
	return TRUE;
}

/*
** SYSLOG_MAP_LOOKUP -- rewrite and syslog message.  Always return empty string
*/

char *
syslog_map_lookup(map, string, args, statp)
	MAP *map;
	char *string;
	char **args;
	int *statp;
{
	char *ptr = map_rewrite(map, string, strlen(string), args);

	if (ptr != NULL)
	{
		if (tTd(38, 20))
			printf("syslog_map_lookup(%s (priority %d): %s\n",
			       map->map_mname, map->map_prio, ptr);

		sm_syslog(map->map_prio, CurEnv->e_id, "%s", ptr);
	}
	
	*statp = EX_OK;
	return "";
}

#endif /* _FFR_SYSLOG_MAP */
/*
**  HESIOD Modules
*/

#ifdef HESIOD

#include <hesiod.h>

bool
hes_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		printf("hes_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
#ifdef ENOSYS
		errno = ENOSYS;
#else
# ifdef EFTYPE
		errno = EFTYPE;
# else
		errno = ENXIO;
# endif
#endif
		return FALSE;
	}

	if (hes_error() == HES_ER_UNINIT)
		hes_init();
	switch (hes_error())
	{
	  case HES_ER_OK:
	  case HES_ER_NOTFOUND:
		return TRUE;
	}

	if (!bitset(MF_OPTIONAL, map->map_mflags))
		syserr("421 cannot initialize Hesiod map (%d)", hes_error());

	return FALSE;
}

char *
hes_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char **hp;

	if (tTd(38, 20))
		printf("hes_map_lookup(%s, %s)\n", map->map_file, name);

	if (name[0] == '\\')
	{
		char *np;
		int nl;
		char nbuf[MAXNAME];

		nl = strlen(name);
		if (nl < sizeof nbuf - 1)
			np = nbuf;
		else
			np = xalloc(strlen(name) + 2);
		np[0] = '\\';
		strcpy(&np[1], name);
		hp = hes_resolve(np, map->map_file);
		if (np != nbuf)
			free(np);
	}
	else
	{
		hp = hes_resolve(name, map->map_file);
	}
	if (hp == NULL || hp[0] == NULL)
	{
		switch (hes_error())
		{
		  case HES_ER_OK:
			*statp = EX_OK;
			break;

		  case HES_ER_NOTFOUND:
			*statp = EX_NOTFOUND;
			break;

		  case HES_ER_CONFIG:
			*statp = EX_UNAVAILABLE;
			break;

		  case HES_ER_NET:
			*statp = EX_TEMPFAIL;
			break;
		}
		return NULL;
	}
	
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, hp[0], strlen(hp[0]), av);
}

#endif
/*
**  NeXT NETINFO Modules
*/

#if NETINFO

# define NETINFO_DEFAULT_DIR		"/aliases"
# define NETINFO_DEFAULT_PROPERTY	"members"

extern char	*ni_propval __P((char *, char *, char *, char *, int));


/*
**  NI_MAP_OPEN -- open NetInfo Aliases
*/

bool
ni_map_open(map, mode)
	MAP *map;
	int mode;
{
	char *p;

	if (tTd(38, 2))
		printf("ni_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);
	mode &= O_ACCMODE;

	if (*map->map_file == '\0')
		map->map_file = NETINFO_DEFAULT_DIR;

	if (map->map_valcolnm == NULL)
		map->map_valcolnm = NETINFO_DEFAULT_PROPERTY;

	if (map->map_coldelim == '\0' && bitset(MF_ALIAS, map->map_mflags))
		map->map_coldelim = ',';

	return TRUE;
}


/*
**  NI_MAP_LOOKUP -- look up a datum in NetInfo
*/

char *
ni_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *res;
	char *propval;

	if (tTd(38, 20))
		printf("ni_map_lookup(%s, %s)\n", map->map_mname, name);

	propval = ni_propval(map->map_file, map->map_keycolnm, name,
			     map->map_valcolnm, map->map_coldelim);

	if (propval == NULL)
		return NULL;

	if (bitset(MF_MATCHONLY, map->map_mflags))
		res = map_rewrite(map, name, strlen(name), NULL);
	else
		res = map_rewrite(map, propval, strlen(propval), av);
	free(propval);
	return res;
}


bool
ni_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	char *vptr;
	char nbuf[MAXNAME + 1];

	if (tTd(38, 20))
		printf("ni_getcanonname(%s)\n", name);

	if (strlen(name) >= sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strcpy(nbuf, name);
	shorten_hostname(nbuf);

	/* we only accept single token search key */
	if (strchr(nbuf, '.'))
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	/* Do the search */
	vptr = ni_propval("/machines", NULL, nbuf, "name", '\0');

	if (vptr == NULL)
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	if (hbsize >= strlen(vptr))
	{
		strcpy(name, vptr);
		*statp = EX_OK;
		return TRUE;
	}
	*statp = EX_UNAVAILABLE;
	free(vptr);
	return FALSE;
}


/*
**  NI_PROPVAL -- NetInfo property value lookup routine
**
**	Parameters:
**		keydir -- the NetInfo directory name in which to search
**			for the key.
**		keyprop -- the name of the property in which to find the
**			property we are interested.  Defaults to "name".
**		keyval -- the value for which we are really searching.
**		valprop -- the property name for the value in which we
**			are interested.
**		sepchar -- if non-nil, this can be multiple-valued, and
**			we should return a string separated by this
**			character.
**
**	Returns:
**		NULL -- if:
**			1. the directory is not found
**			2. the property name is not found
**			3. the property contains multiple values
**			4. some error occured
**		else -- the value of the lookup.
**
**	Example:
**		To search for an alias value, use:
**		  ni_propval("/aliases", "name", aliasname, "members", ',')
**
**	Notes:
**      	Caller should free the return value of ni_proval
*/

# include <netinfo/ni.h>

# define LOCAL_NETINFO_DOMAIN    "."
# define PARENT_NETINFO_DOMAIN   ".."
# define MAX_NI_LEVELS           256

char *
ni_propval(keydir, keyprop, keyval, valprop, sepchar)
	char *keydir;
	char *keyprop;
	char *keyval;
	char *valprop;
	int sepchar;
{
	char *propval = NULL;
	int i;
	int j, alen;
	void *ni = NULL;
	void *lastni = NULL;
	ni_status nis;
	ni_id nid;
	ni_namelist ninl;
	register char *p;
	char keybuf[1024];

	/*
	**  Create the full key from the two parts.
	**
	**	Note that directory can end with, e.g., "name=" to specify
	**	an alternate search property.
	*/

	i = strlen(keydir) + strlen(keyval) + 2;
	if (keyprop != NULL)
		i += strlen(keyprop) + 1;
	if (i > sizeof keybuf)
		return NULL;
	strcpy(keybuf, keydir);
	strcat(keybuf, "/");
	if (keyprop != NULL)
	{
		strcat(keybuf, keyprop);
		strcat(keybuf, "=");
	}
	strcat(keybuf, keyval);

	if (tTd(38, 21))
		printf("ni_propval(%s, %s, %s, %s, %d) keybuf='%s'\n",
			keydir, keyprop, keyval, valprop, sepchar, keybuf);
	/*
	**  If the passed directory and property name are found
	**  in one of netinfo domains we need to search (starting
	**  from the local domain moving all the way back to the
	**  root domain) set propval to the property's value
	**  and return it.
	*/

	for (i = 0; i < MAX_NI_LEVELS && propval == NULL; i++)
	{
		if (i == 0)
		{
			nis = ni_open(NULL, LOCAL_NETINFO_DOMAIN, &ni);
			if (tTd(38, 20))
				printf("ni_open(LOCAL) = %d\n", nis);
		}
		else
		{
			if (lastni != NULL)
				ni_free(lastni);
			lastni = ni;
			nis = ni_open(lastni, PARENT_NETINFO_DOMAIN, &ni);
			if (tTd(38, 20))
				printf("ni_open(PARENT) = %d\n", nis);
		}

		/*
		**  Don't bother if we didn't get a handle on a
		**  proper domain.  This is not necessarily an error.
		**  We would get a positive ni_status if, for instance
		**  we never found the directory or property and tried
		**  to open the parent of the root domain!
		*/

		if (nis != 0)
			break;

		/*
		**  Find the path to the server information.
		*/

		if (ni_pathsearch(ni, &nid, keybuf) != 0)
			continue;

		/*
		**  Find associated value information.
		*/

		if (ni_lookupprop(ni, &nid, valprop, &ninl) != 0)
			continue;

		if (tTd(38, 20))
			printf("ni_lookupprop: len=%d\n", ninl.ni_namelist_len);
		/*
		**  See if we have an acceptable number of values.
		*/

		if (ninl.ni_namelist_len <= 0)
			continue;

		if (sepchar == '\0' && ninl.ni_namelist_len > 1)
		{
			ni_namelist_free(&ninl);
			continue;
		}

		/*
		**  Calculate number of bytes needed and build result
		*/

		alen = 1;
		for (j = 0; j < ninl.ni_namelist_len; j++)
			alen += strlen(ninl.ni_namelist_val[j]) + 1;
		propval = p = xalloc(alen);
		for (j = 0; j < ninl.ni_namelist_len; j++)
		{
			strcpy(p, ninl.ni_namelist_val[j]);
			p += strlen(p);
			*p++ = sepchar;
		}
		*--p = '\0';

		ni_namelist_free(&ninl);
	}

	/*
	**  Clean up.
	*/

	if (ni != NULL)
		ni_free(ni);
	if (lastni != NULL && ni != lastni)
		ni_free(lastni);
	if (tTd(38, 20))
		printf("ni_propval returns: '%s'\n", propval);

	return propval;
}

#endif
/*
**  TEXT (unindexed text file) Modules
**
**	This code donated by Sun Microsystems.
*/

#define map_sff		map_lockfd	/* overload field */


/*
**  TEXT_MAP_OPEN -- open text table
*/

bool
text_map_open(map, mode)
	MAP *map;
	int mode;
{
	int sff;
	int i;

	if (tTd(38, 2))
		printf("text_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		errno = ENODEV;
		return FALSE;
	}

	if (*map->map_file == '\0')
	{
		syserr("text map \"%s\": file name required",
			map->map_mname);
		return FALSE;
	}

	if (map->map_file[0] != '/')
	{
		syserr("text map \"%s\": file name must be fully qualified",
			map->map_mname);
		return FALSE;
	}

	sff = SFF_ROOTOK|SFF_REGONLY|SFF_NOWLINK;
	if (FatalWritableDirs)
		sff |= SFF_SAFEDIRPATH;
	if ((i = safefile(map->map_file, RunAsUid, RunAsGid, RunAsUserName,
			  sff, S_IRUSR, NULL)) != 0)
	{
		/* cannot open this map */
		if (tTd(38, 2))
			printf("\tunsafe map file: %d\n", i);
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("text map \"%s\": unsafe map file %s",
				map->map_mname, map->map_file);
		return FALSE;
	}

	if (map->map_keycolnm == NULL)
		map->map_keycolno = 0;
	else
	{
		if (!isdigit(*map->map_keycolnm))
		{
			syserr("text map \"%s\", file %s: -k should specify a number, not %s",
				map->map_mname, map->map_file,
				map->map_keycolnm);
			return FALSE;
		}
		map->map_keycolno = atoi(map->map_keycolnm);
	}

	if (map->map_valcolnm == NULL)
		map->map_valcolno = 0;
	else
	{
		if (!isdigit(*map->map_valcolnm))
		{
			syserr("text map \"%s\", file %s: -v should specify a number, not %s",
					map->map_mname, map->map_file,
					map->map_valcolnm);
			return FALSE;
		}
		map->map_valcolno = atoi(map->map_valcolnm);
	}

	if (tTd(38, 2))
	{
		printf("text_map_open(%s, %s): delimiter = ",
			map->map_mname, map->map_file);
		if (map->map_coldelim == '\0')
			printf("(white space)\n");
		else
			printf("%c\n", map->map_coldelim);
	}

	map->map_sff = sff;
	return TRUE;
}


/*
**  TEXT_MAP_LOOKUP -- look up a datum in a TEXT table
*/

char *
text_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *vp;
	auto int vsize;
	int buflen;
	FILE *f;
	char delim;
	int key_idx;
	bool found_it;
	int sff = map->map_sff;
	char search_key[MAXNAME + 1];
	char linebuf[MAXLINE];
	char buf[MAXNAME + 1];
	extern char *get_column __P((char *, int, char, char *, int));

	found_it = FALSE;
	if (tTd(38, 20))
		printf("text_map_lookup(%s, %s)\n", map->map_mname,  name);

	buflen = strlen(name);
	if (buflen > sizeof search_key - 1)
		buflen = sizeof search_key - 1;
	bcopy(name, search_key, buflen);
	search_key[buflen] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(search_key);

	f = safefopen(map->map_file, O_RDONLY, FileMode, sff);
	if (f == NULL)
	{
		map->map_mflags &= ~(MF_VALID|MF_OPEN);
		*statp = EX_UNAVAILABLE;
		return NULL;
	}
	key_idx = map->map_keycolno;
	delim = map->map_coldelim;
	while (fgets(linebuf, MAXLINE, f) != NULL)
	{
		char *p;

		/* skip comment line */
		if (linebuf[0] == '#')
			continue;
		p = strchr(linebuf, '\n');
		if (p != NULL)
			*p = '\0';
		p = get_column(linebuf, key_idx, delim, buf, sizeof buf);
		if (p != NULL && strcasecmp(search_key, p) == 0)
		{
			found_it = TRUE;
			break;
		}
	}
	fclose(f);
	if (!found_it)
	{
		*statp = EX_NOTFOUND;
		return NULL;
	}
	vp = get_column(linebuf, map->map_valcolno, delim, buf, sizeof buf);
	vsize = strlen(vp);
	*statp = EX_OK;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, vp, vsize, av);
}


/*
**  TEXT_GETCANONNAME -- look up canonical name in hosts file
*/

bool
text_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	bool found;
	FILE *f;
	char linebuf[MAXLINE];
	char cbuf[MAXNAME + 1];
	char nbuf[MAXNAME + 1];

	if (tTd(38, 20))
		printf("text_getcanonname(%s)\n", name);

	if (strlen(name) >= (SIZE_T) sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strcpy(nbuf, name);
	shorten_hostname(nbuf);

	f = fopen(HostsFile, "r");
	if (f == NULL)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	found = FALSE;
	while (!found && fgets(linebuf, MAXLINE, f) != NULL)
	{
		char *p = strpbrk(linebuf, "#\n");

		if (p != NULL)
			*p = '\0';
		if (linebuf[0] != '\0')
			found = extract_canonname(nbuf, linebuf, cbuf, sizeof cbuf);
	}
	fclose(f);
	if (!found)
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	if ((SIZE_T) hbsize >= strlen(cbuf))
	{
		strcpy(name, cbuf);
		*statp = EX_OK;
		return TRUE;
	}
	*statp = EX_UNAVAILABLE;
	return FALSE;
}
/*
**  STAB (Symbol Table) Modules
*/


/*
**  STAB_MAP_LOOKUP -- look up alias in symbol table
*/

char *
stab_map_lookup(map, name, av, pstat)
	register MAP *map;
	char *name;
	char **av;
	int *pstat;
{
	register STAB *s;

	if (tTd(38, 20))
		printf("stab_lookup(%s, %s)\n",
			map->map_mname, name);

	s = stab(name, ST_ALIAS, ST_FIND);
	if (s != NULL)
		return (s->s_alias);
	return (NULL);
}


/*
**  STAB_MAP_STORE -- store in symtab (actually using during init, not rebuild)
*/

void
stab_map_store(map, lhs, rhs)
	register MAP *map;
	char *lhs;
	char *rhs;
{
	register STAB *s;

	s = stab(lhs, ST_ALIAS, ST_ENTER);
	s->s_alias = newstr(rhs);
}


/*
**  STAB_MAP_OPEN -- initialize (reads data file)
**
**	This is a wierd case -- it is only intended as a fallback for
**	aliases.  For this reason, opens for write (only during a
**	"newaliases") always fails, and opens for read open the
**	actual underlying text file instead of the database.
*/

bool
stab_map_open(map, mode)
	register MAP *map;
	int mode;
{
	FILE *af;
	int sff;
	struct stat st;

	if (tTd(38, 2))
		printf("stab_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		errno = ENODEV;
		return FALSE;
	}

	sff = SFF_ROOTOK|SFF_REGONLY|SFF_NOWLINK;
	if (FatalWritableDirs)
		sff |= SFF_SAFEDIRPATH;
	af = safefopen(map->map_file, O_RDONLY, 0444, sff);
	if (af == NULL)
		return FALSE;
	readaliases(map, af, FALSE, FALSE);

	if (fstat(fileno(af), &st) >= 0)
		map->map_mtime = st.st_mtime;
	fclose(af);

	return TRUE;
}
/*
**  Implicit Modules
**
**	Tries several types.  For back compatibility of aliases.
*/


/*
**  IMPL_MAP_LOOKUP -- lookup in best open database
*/

char *
impl_map_lookup(map, name, av, pstat)
	MAP *map;
	char *name;
	char **av;
	int *pstat;
{
	if (tTd(38, 20))
		printf("impl_map_lookup(%s, %s)\n",
			map->map_mname, name);

#ifdef NEWDB
	if (bitset(MF_IMPL_HASH, map->map_mflags))
		return db_map_lookup(map, name, av, pstat);
#endif
#ifdef NDBM
	if (bitset(MF_IMPL_NDBM, map->map_mflags))
		return ndbm_map_lookup(map, name, av, pstat);
#endif
	return stab_map_lookup(map, name, av, pstat);
}

/*
**  IMPL_MAP_STORE -- store in open databases
*/

void
impl_map_store(map, lhs, rhs)
	MAP *map;
	char *lhs;
	char *rhs;
{
	if (tTd(38, 12))
		printf("impl_map_store(%s, %s, %s)\n",
			map->map_mname, lhs, rhs);
#ifdef NEWDB
	if (bitset(MF_IMPL_HASH, map->map_mflags))
		db_map_store(map, lhs, rhs);
#endif
#ifdef NDBM
	if (bitset(MF_IMPL_NDBM, map->map_mflags))
		ndbm_map_store(map, lhs, rhs);
#endif
	stab_map_store(map, lhs, rhs);
}

/*
**  IMPL_MAP_OPEN -- implicit database open
*/

bool
impl_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		printf("impl_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
#ifdef NEWDB
	map->map_mflags |= MF_IMPL_HASH;
	if (hash_map_open(map, mode))
	{
# ifdef NDBM_YP_COMPAT
		if (mode == O_RDONLY || strstr(map->map_file, "/yp/") == NULL)
# endif
			return TRUE;
	}
	else
		map->map_mflags &= ~MF_IMPL_HASH;
#endif
#ifdef NDBM
	map->map_mflags |= MF_IMPL_NDBM;
	if (ndbm_map_open(map, mode))
	{
		return TRUE;
	}
	else
		map->map_mflags &= ~MF_IMPL_NDBM;
#endif

#if defined(NEWDB) || defined(NDBM)
	if (Verbose)
		message("WARNING: cannot open alias database %s", map->map_file);
#else
	if (mode != O_RDONLY)
		usrerr("Cannot rebuild aliases: no database format defined");
#endif

	return stab_map_open(map, mode);
}


/*
**  IMPL_MAP_CLOSE -- close any open database(s)
*/

void
impl_map_close(map)
	MAP *map;
{
	if (tTd(38, 9))
		printf("impl_map_close(%s, %s, %lx)\n",
			map->map_mname, map->map_file, map->map_mflags);
#ifdef NEWDB
	if (bitset(MF_IMPL_HASH, map->map_mflags))
	{
		db_map_close(map);
		map->map_mflags &= ~MF_IMPL_HASH;
	}
#endif

#ifdef NDBM
	if (bitset(MF_IMPL_NDBM, map->map_mflags))
	{
		ndbm_map_close(map);
		map->map_mflags &= ~MF_IMPL_NDBM;
	}
#endif
}
/*
**  User map class.
**
**	Provides access to the system password file.
*/

/*
**  USER_MAP_OPEN -- open user map
**
**	Really just binds field names to field numbers.
*/

bool
user_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		printf("user_map_open(%s, %d)\n",
			map->map_mname, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
#ifdef ENOSYS
		errno = ENOSYS;
#else
# ifdef EFTYPE
		errno = EFTYPE;
# else
		errno = ENXIO;
# endif
#endif
		return FALSE;
	}
	if (map->map_valcolnm == NULL)
		/* nothing */ ;
	else if (strcasecmp(map->map_valcolnm, "name") == 0)
		map->map_valcolno = 1;
	else if (strcasecmp(map->map_valcolnm, "passwd") == 0)
		map->map_valcolno = 2;
	else if (strcasecmp(map->map_valcolnm, "uid") == 0)
		map->map_valcolno = 3;
	else if (strcasecmp(map->map_valcolnm, "gid") == 0)
		map->map_valcolno = 4;
	else if (strcasecmp(map->map_valcolnm, "gecos") == 0)
		map->map_valcolno = 5;
	else if (strcasecmp(map->map_valcolnm, "dir") == 0)
		map->map_valcolno = 6;
	else if (strcasecmp(map->map_valcolnm, "shell") == 0)
		map->map_valcolno = 7;
	else
	{
		syserr("User map %s: unknown column name %s",
			map->map_mname, map->map_valcolnm);
		return FALSE;
	}
	return TRUE;
}


/*
**  USER_MAP_LOOKUP -- look up a user in the passwd file.
*/

char *
user_map_lookup(map, key, av, statp)
	MAP *map;
	char *key;
	char **av;
	int *statp;
{
	struct passwd *pw;
	auto bool fuzzy;

	if (tTd(38, 20))
		printf("user_map_lookup(%s, %s)\n",
			map->map_mname, key);

	pw = finduser(key, &fuzzy);
	if (pw == NULL)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, key, strlen(key), NULL);
	else
	{
		char *rwval = NULL;
		char buf[30];

		switch (map->map_valcolno)
		{
		  case 0:
		  case 1:
			rwval = pw->pw_name;
			break;

		  case 2:
			rwval = pw->pw_passwd;
			break;

		  case 3:
			snprintf(buf, sizeof buf, "%d", pw->pw_uid);
			rwval = buf;
			break;

		  case 4:
			snprintf(buf, sizeof buf, "%d", pw->pw_gid);
			rwval = buf;
			break;

		  case 5:
			rwval = pw->pw_gecos;
			break;

		  case 6:
			rwval = pw->pw_dir;
			break;

		  case 7:
			rwval = pw->pw_shell;
			break;
		}
		return map_rewrite(map, rwval, strlen(rwval), av);
	}
}
/*
**  Program map type.
**
**	This provides access to arbitrary programs.  It should be used
**	only very sparingly, since there is no way to bound the cost
**	of invoking an arbitrary program.
*/

char *
prog_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int i;
	register char *p;
	int fd;
	auto pid_t pid;
	char *rval;
	int stat;
	char *argv[MAXPV + 1];
	char buf[MAXLINE];

	if (tTd(38, 20))
		printf("prog_map_lookup(%s, %s) %s\n",
			map->map_mname, name, map->map_file);

	i = 0;
	argv[i++] = map->map_file;
	if (map->map_rebuild != NULL)
	{
		snprintf(buf, sizeof buf, "%s", map->map_rebuild);
		for (p = strtok(buf, " \t"); p != NULL; p = strtok(NULL, " \t"))
		{
			if (i >= MAXPV - 1)
				break;
			argv[i++] = p;
		}
	}
	argv[i++] = name;
	argv[i] = NULL;
	if (tTd(38, 21))
	{
		printf("prog_open:");
		for (i = 0; argv[i] != NULL; i++)
			printf(" %s", argv[i]);
		printf("\n");
	}
	(void) blocksignal(SIGCHLD);
	pid = prog_open(argv, &fd, CurEnv);
	if (pid < 0)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("prog_map_lookup(%s) failed (%s) -- closing",
				map->map_mname, errstring(errno));
		else if (tTd(38, 9))
			printf("prog_map_lookup(%s) failed (%s) -- closing",
				map->map_mname, errstring(errno));
		map->map_mflags &= ~(MF_VALID|MF_OPEN);
		*statp = EX_OSFILE;
		return NULL;
	}
	i = read(fd, buf, sizeof buf - 1);
	if (i < 0)
	{
		syserr("prog_map_lookup(%s): read error %s\n",
			map->map_mname, errstring(errno));
		rval = NULL;
	}
	else if (i == 0)
	{
		if (tTd(38, 20))
			printf("prog_map_lookup(%s): empty answer\n",
				map->map_mname);
		rval = NULL;
	}
	else
	{
		buf[i] = '\0';
		p = strchr(buf, '\n');
		if (p != NULL)
			*p = '\0';

		/* collect the return value */
		if (bitset(MF_MATCHONLY, map->map_mflags))
			rval = map_rewrite(map, name, strlen(name), NULL);
		else
			rval = map_rewrite(map, buf, strlen(buf), NULL);

		/* now flush any additional output */
		while ((i = read(fd, buf, sizeof buf)) > 0)
			continue;
	}

	/* wait for the process to terminate */
	close(fd);
	stat = waitfor(pid);
	(void) releasesignal(SIGCHLD);

	if (stat == -1)
	{
		syserr("prog_map_lookup(%s): wait error %s\n",
			map->map_mname, errstring(errno));
		*statp = EX_SOFTWARE;
		rval = NULL;
	}
	else if (WIFEXITED(stat))
	{
		if ((*statp = WEXITSTATUS(stat)) != EX_OK)
			rval = NULL;
	}
	else
	{
		syserr("prog_map_lookup(%s): child died on signal %d",
			map->map_mname, stat);
		*statp = EX_UNAVAILABLE;
		rval = NULL;
	}
	return rval;
}
/*
**  Sequenced map type.
**
**	Tries each map in order until something matches, much like
**	implicit.  Stores go to the first map in the list that can
**	support storing.
**
**	This is slightly unusual in that there are two interfaces.
**	The "sequence" interface lets you stack maps arbitrarily.
**	The "switch" interface builds a sequence map by looking
**	at a system-dependent configuration file such as
**	/etc/nsswitch.conf on Solaris or /etc/svc.conf on Ultrix.
**
**	We don't need an explicit open, since all maps are
**	opened during startup, including underlying maps.
*/

/*
**  SEQ_MAP_PARSE -- Sequenced map parsing
*/

bool
seq_map_parse(map, ap)
	MAP *map;
	char *ap;
{
	int maxmap;

	if (tTd(38, 2))
		printf("seq_map_parse(%s, %s)\n", map->map_mname, ap);
	maxmap = 0;
	while (*ap != '\0')
	{
		register char *p;
		STAB *s;

		/* find beginning of map name */
		while (isascii(*ap) && isspace(*ap))
			ap++;
		for (p = ap; isascii(*p) && isalnum(*p); p++)
			continue;
		if (*p != '\0')
			*p++ = '\0';
		while (*p != '\0' && (!isascii(*p) || !isalnum(*p)))
			p++;
		if (*ap == '\0')
		{
			ap = p;
			continue;
		}
		s = stab(ap, ST_MAP, ST_FIND);
		if (s == NULL)
		{
			syserr("Sequence map %s: unknown member map %s",
				map->map_mname, ap);
		}
		else if (maxmap == MAXMAPSTACK)
		{
			syserr("Sequence map %s: too many member maps (%d max)",
				map->map_mname, MAXMAPSTACK);
			maxmap++;
		}
		else if (maxmap < MAXMAPSTACK)
		{
			map->map_stack[maxmap++] = &s->s_map;
		}
		ap = p;
	}
	return TRUE;
}


/*
**  SWITCH_MAP_OPEN -- open a switched map
**
**	This looks at the system-dependent configuration and builds
**	a sequence map that does the same thing.
**
**	Every system must define a switch_map_find routine in conf.c
**	that will return the list of service types associated with a
**	given service class.
*/

bool
switch_map_open(map, mode)
	MAP *map;
	int mode;
{
	int mapno;
	int nmaps;
	char *maptype[MAXMAPSTACK];

	if (tTd(38, 2))
		printf("switch_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	nmaps = switch_map_find(map->map_file, maptype, map->map_return);
	if (tTd(38, 19))
	{
		printf("\tswitch_map_find => %d\n", nmaps);
		for (mapno = 0; mapno < nmaps; mapno++)
			printf("\t\t%s\n", maptype[mapno]);
	}
	if (nmaps <= 0 || nmaps > MAXMAPSTACK)
		return FALSE;

	for (mapno = 0; mapno < nmaps; mapno++)
	{
		register STAB *s;
		char nbuf[MAXNAME + 1];

		if (maptype[mapno] == NULL)
			continue;
		(void) snprintf(nbuf, sizeof nbuf, "%s.%s",
			map->map_mname, maptype[mapno]);
		s = stab(nbuf, ST_MAP, ST_FIND);
		if (s == NULL)
		{
			syserr("Switch map %s: unknown member map %s",
				map->map_mname, nbuf);
		}
		else
		{
			map->map_stack[mapno] = &s->s_map;
			if (tTd(38, 4))
				printf("\tmap_stack[%d] = %s:%s\n",
					mapno, s->s_map.map_class->map_cname,
					nbuf);
		}
	}
	return TRUE;
}


/*
**  SEQ_MAP_CLOSE -- close all underlying maps
*/

void
seq_map_close(map)
	MAP *map;
{
	int mapno;

	if (tTd(38, 9))
		printf("seq_map_close(%s)\n", map->map_mname);

	for (mapno = 0; mapno < MAXMAPSTACK; mapno++)
	{
		MAP *mm = map->map_stack[mapno];

		if (mm == NULL || !bitset(MF_OPEN, mm->map_mflags))
			continue;
		mm->map_class->map_close(mm);
		mm->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
	}
}


/*
**  SEQ_MAP_LOOKUP -- sequenced map lookup
*/

char *
seq_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	int mapno;
	int mapbit = 0x01;
	bool tempfail = FALSE;

	if (tTd(38, 20))
		printf("seq_map_lookup(%s, %s)\n", map->map_mname, key);

	for (mapno = 0; mapno < MAXMAPSTACK; mapbit <<= 1, mapno++)
	{
		MAP *mm = map->map_stack[mapno];
		char *rv;

		if (mm == NULL)
			continue;
		if (!bitset(MF_OPEN, mm->map_mflags))
		{
			if (bitset(mapbit, map->map_return[MA_UNAVAIL]))
			{
				*pstat = EX_UNAVAILABLE;
				return NULL;
			}
			continue;
		}
		*pstat = EX_OK;
		rv = mm->map_class->map_lookup(mm, key, args, pstat);
		if (rv != NULL)
			return rv;
		if (*pstat == EX_TEMPFAIL)
		{
			if (bitset(mapbit, map->map_return[MA_TRYAGAIN]))
				return NULL;
			tempfail = TRUE;
		}
		else if (bitset(mapbit, map->map_return[MA_NOTFOUND]))
			break;
	}
	if (tempfail)
		*pstat = EX_TEMPFAIL;
	else if (*pstat == EX_OK)
		*pstat = EX_NOTFOUND;
	return NULL;
}


/*
**  SEQ_MAP_STORE -- sequenced map store
*/

void
seq_map_store(map, key, val)
	MAP *map;
	char *key;
	char *val;
{
	int mapno;

	if (tTd(38, 12))
		printf("seq_map_store(%s, %s, %s)\n",
			map->map_mname, key, val);

	for (mapno = 0; mapno < MAXMAPSTACK; mapno++)
	{
		MAP *mm = map->map_stack[mapno];

		if (mm == NULL || !bitset(MF_WRITABLE, mm->map_mflags))
			continue;

		mm->map_class->map_store(mm, key, val);
		return;
	}
	syserr("seq_map_store(%s, %s, %s): no writable map",
		map->map_mname, key, val);
}
/*
**  NULL stubs
*/

bool
null_map_open(map, mode)
	MAP *map;
	int mode;
{
	return TRUE;
}

void
null_map_close(map)
	MAP *map;
{
	return;
}

char *
null_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	*pstat = EX_NOTFOUND;
	return NULL;
}

void
null_map_store(map, key, val)
	MAP *map;
	char *key;
	char *val;
{
	return;
}


/*
**  BOGUS stubs
*/

char *
bogus_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	*pstat = EX_TEMPFAIL;
	return NULL;
}

MAPCLASS	BogusMapClass =
{
	"bogus-map",		NULL,		0,
	NULL,		bogus_map_lookup,	null_map_store,
	null_map_open,	null_map_close,
};
