/* $NetBSD: user.c,v 1.75.4.2 2006/10/15 15:52:26 bouyer Exp $ */

/*
 * Copyright (c) 1999 Alistair G. Crooks.  All rights reserved.
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
 *	This product includes software developed by Alistair G. Crooks.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1999 \
	        The NetBSD Foundation, Inc.  All rights reserved.");
__RCSID("$NetBSD: user.c,v 1.75.4.2 2006/10/15 15:52:26 bouyer Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#include "defs.h"
#include "usermgmt.h"


/* this struct describes a uid range */
typedef struct range_t {
	int	r_from;		/* low uid */
	int	r_to;		/* high uid */
} range_t;

/* this struct encapsulates the user information */
typedef struct user_t {
	int		u_flags;		/* see below */
	int		u_uid;			/* uid of user */
	char	       *u_password;		/* encrypted password */
	char	       *u_comment;		/* comment field */
	char	       *u_home;			/* home directory */
	char	       *u_primgrp;		/* primary group */
	int		u_groupc;		/* # of secondary groups */
	const char     *u_groupv[NGROUPS_MAX];	/* secondary groups */
	char	       *u_shell;		/* user's shell */
	char	       *u_basedir;		/* base directory for home */
	char	       *u_expire;		/* when password will expire */
	char	       *u_inactive;		/* when account will expire */
	char	       *u_skeldir;		/* directory for startup files */
	char	       *u_class;		/* login class */
	unsigned	u_rsize;		/* size of range array */
	unsigned	u_rc;			/* # of ranges */
	range_t	       *u_rv;			/* the ranges */
	unsigned	u_defrc;		/* # of ranges in defaults */
	int		u_preserve;		/* preserve uids on deletion */
	int		u_allow_samba;		/* allow trailing '$' for samba login names */
} user_t;

/* flags for which fields of the user_t replace the passwd entry */
enum {
	F_COMMENT	= 0x0001,
	F_DUPUID  	= 0x0002,
	F_EXPIRE	= 0x0004,
	F_GROUP		= 0x0008,
	F_HOMEDIR	= 0x0010,
	F_MKDIR		= 0x0020,
	F_INACTIVE	= 0x0040,
	F_PASSWORD	= 0x0080,
	F_SECGROUP	= 0x0100,
	F_SHELL 	= 0x0200,
	F_UID		= 0x0400,
	F_USERNAME	= 0x0800,
	F_CLASS		= 0x1000
};

#define CONFFILE	"/etc/usermgmt.conf"

#ifndef DEF_GROUP
#define DEF_GROUP	"users"
#endif

#ifndef DEF_BASEDIR
#define DEF_BASEDIR	"/home"
#endif

#ifndef DEF_SKELDIR
#define DEF_SKELDIR	"/etc/skel"
#endif

#ifndef DEF_SHELL
#define DEF_SHELL	_PATH_CSHELL
#endif

#ifndef DEF_COMMENT
#define DEF_COMMENT	""
#endif

#ifndef DEF_LOWUID
#define DEF_LOWUID	1000
#endif

#ifndef DEF_HIGHUID
#define DEF_HIGHUID	60000
#endif

#ifndef DEF_INACTIVE
#define DEF_INACTIVE	0
#endif

#ifndef DEF_EXPIRE
#define DEF_EXPIRE	NULL
#endif

#ifndef DEF_CLASS
#define DEF_CLASS	""
#endif

#ifndef WAITSECS
#define WAITSECS	10
#endif

#ifndef NOBODY_UID
#define NOBODY_UID	32767
#endif

/* some useful constants */
enum {
	MaxShellNameLen = 256,
	MaxFileNameLen = MAXPATHLEN,
	MaxUserNameLen = 32,
	MaxFieldNameLen = 32,
	MaxCommandLen = 2048,
	MaxEntryLen = 2048,
	PasswordLength = 2048,

	DES_Len = 13,

	LowGid = DEF_LOWUID,
	HighGid = DEF_HIGHUID
};

/* Full paths of programs used here */
#define CHMOD		"/bin/chmod"
#define CHOWN		"/usr/sbin/chown"
#define MKDIR		"/bin/mkdir"
#define MV		"/bin/mv"
#define NOLOGIN		"/sbin/nologin"
#define PAX		"/bin/pax"
#define RM		"/bin/rm"

#define UNSET_INACTIVE	"Null (unset)"
#define UNSET_EXPIRY	"Null (unset)"

static int asystem(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2)));
	
static int	verbose;

/* if *cpp is non-null, free it, then assign `n' chars of `s' to it */
static void
memsave(char **cpp, const char *s, size_t n)
{
	if (*cpp != NULL) {
		FREE(*cpp);
	}
	NEWARRAY(char, *cpp, n + 1, exit(1));
	(void) memcpy(*cpp, s, n);
	(*cpp)[n] = '\0';
}

/* a replacement for system(3) */
static int
asystem(const char *fmt, ...)
{
	va_list	vp;
	char	buf[MaxCommandLen];
	int	ret;

	va_start(vp, fmt);
	(void) vsnprintf(buf, sizeof(buf), fmt, vp);
	va_end(vp);
	if (verbose) {
		(void) printf("Command: %s\n", buf);
	}
	if ((ret = system(buf)) != 0) {
		warnx("[Warning] can't system `%s'", buf);
	}
	return ret;
}

/* remove a users home directory, returning 1 for success (ie, no problems encountered) */
static int
removehomedir(const char *user, int uid, const char *dir)
{
	struct stat st;

	/* userid not root? */
	if (uid == 0) {
		warnx("Not deleting home directory `%s'; userid is 0", dir);
		return 0;
	}

	/* directory exists (and is a directory!) */
	if (stat(dir, &st) < 0) {
		warnx("Home directory `%s' doesn't exist", dir);
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		warnx("Home directory `%s' is not a directory", dir);
		return 0;
	}

	/* userid matches directory owner? */
	if (st.st_uid != uid) {
		warnx("User `%s' doesn't own directory `%s', not removed", user, dir);
		return 0;
	}

	(void) seteuid(uid);
	/* we add the "|| true" to keep asystem() quiet if there is a non-zero exit status. */
	(void) asystem("%s -rf %s > /dev/null 2>&1 || true", RM, dir);
	(void) seteuid(0);
	if (rmdir(dir) < 0) {
		warnx("Unable to remove all files in `%s'", dir);
		return 0;
	}
	return 1;
}

#define NetBSD_1_4_K	104110000

#if defined(__NetBSD_Version__) && (__NetBSD_Version__ < NetBSD_1_4_K)
/* bounds checking strncpy */
static int
strlcpy(char *to, char *from, size_t tosize)
{
	size_t	n;
	int	fromsize;

	fromsize = strlen(from);
	n = MIN(tosize - 1, fromsize);
	(void) memcpy(to, from, n);
	to[n] = '\0';
	return fromsize;
}
#endif /* NetBSD < 1.4K */

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * research has shown that NetBSD 1.3H was the first version of -current
 * with asprintf in libc. agc
 */
#define NetBSD_1_3_H	103080000

#if defined(__NetBSD_Version__) && (__NetBSD_Version__ < NetBSD_1_3_H)

int
asprintf(char **str, char const *fmt, ...)
{
	int ret;
	va_list ap;
	FILE f;
	unsigned char *_base;

	f._flags = __SWR | __SSTR | __SALC;
	f._bf._base = f._p = (unsigned char *)malloc(128);
	if (f._bf._base == NULL)
		goto err;
	f._bf._size = f._w = 127;		/* Leave room for the NUL */
	va_start(ap, fmt);
	ret = vfprintf(&f, fmt, ap);
	va_end(ap);
	if (ret == -1)
		goto err;
	*f._p = '\0';
	_base = realloc(f._bf._base, (size_t)(ret + 1));
	if (_base == NULL)
		goto err;
	*str = (char *)_base;
	return (ret);

err:
	if (f._bf._base)
		free(f._bf._base);
	*str = NULL;
	return (-1);
}
#endif /* NetBSD < 1.3H */

/* return 1 if all of `s' is numeric */
static int
is_number(char *s)
{
	for ( ; *s ; s++) {
		if (!isdigit((unsigned char) *s)) {
			return 0;
		}
	}
	return 1;
}

/*
 * check that the effective uid is 0 - called from funcs which will
 * modify data and config files.
 */
static void
checkeuid(void)
{
	if (geteuid() != 0) {
		errx(EXIT_FAILURE, "Program must be run as root");
	}
}

/* copy any dot files into the user's home directory */
static int
copydotfiles(char *skeldir, int uid, int gid, char *dir)
{
	struct dirent	*dp;
	DIR		*dirp;
	int		n;

	if ((dirp = opendir(skeldir)) == NULL) {
		warn("can't open source . files dir `%s'", skeldir);
		return 0;
	}
	for (n = 0; (dp = readdir(dirp)) != NULL && n == 0 ; ) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0) {
			continue;
		}
		n = 1;
	}
	(void) closedir(dirp);
	if (n == 0) {
		warnx("No \"dot\" initialisation files found");
	} else {
		(void) asystem("cd %s && %s -rw -pe %s . %s", 
				skeldir, PAX, (verbose) ? "-v" : "", dir);
	}
	(void) asystem("%s -R -h %d:%d %s", CHOWN, uid, gid, dir);
	(void) asystem("%s -R u+w %s", CHMOD, dir);
	return n;
}

/* create a group entry with gid `gid' */
static int
creategid(char *group, int gid, const char *name)
{
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	int		fd;
	int		cc;

	if (getgrnam(group) != NULL) {
		warnx("group `%s' already exists", group);
		return 0;
	}
	if ((from = fopen(_PATH_GROUP, "r")) == NULL) {
		warn("can't create gid for `%s': can't open `%s'", name, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		(void) fclose(from);
		warn("can't create gid: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(f);
		warn("can't create gid: fdopen `%s' failed", f);
		return 0;
	}
	while ((cc = fread(buf, sizeof(char), sizeof(buf), from)) > 0) {
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			(void) fclose(from);
			(void) close(fd);
			(void) unlink(f);
			warn("can't create gid: short write to `%s'", f);
			return 0;
		}
	}
	(void) fprintf(to, "%s:*:%d:%s\n", group, gid, name);
	(void) fclose(from);
	(void) fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		(void) unlink(f);
		warn("can't create gid: can't rename `%s' to `%s'", f, _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	syslog(LOG_INFO, "new group added: name=%s, gid=%d", group, gid);
	return 1;
}

/* modify the group entry with name `group' to be newent */
static int
modify_gid(char *group, char *newent)
{
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	char		*colon;
	int		groupc;
	int		entc;
	int		fd;
	int		cc;

	if ((from = fopen(_PATH_GROUP, "r")) == NULL) {
		warn("can't create gid for `%s': can't open `%s'", group, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		(void) fclose(from);
		warn("can't create gid: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(f);
		warn("can't create gid: fdopen `%s' failed", f);
		return 0;
	}
	groupc = strlen(group);
	while (fgets(buf, sizeof(buf), from) != NULL) {
		cc = strlen(buf);
		if ((colon = strchr(buf, ':')) == NULL) {
			warn("badly formed entry `%s'", buf);
			continue;
		}
		entc = (int)(colon - buf);
		if (entc == groupc && strncmp(group, buf, (unsigned) entc) == 0) {
			if (newent == NULL) {
				continue;
			} else {
				cc = strlen(newent);
				(void) strlcpy(buf, newent, sizeof(buf));
			}
		}
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			(void) fclose(from);
			(void) close(fd);
			(void) unlink(f);
			warn("can't create gid: short write to `%s'", f);
			return 0;
		}
	}
	(void) fclose(from);
	(void) fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		(void) unlink(f);
		warn("can't create gid: can't rename `%s' to `%s'", f, _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	if (newent == NULL) {
		syslog(LOG_INFO, "group deleted: name=%s", group);
	} else {
		syslog(LOG_INFO, "group information modified: name=%s", group);
	}
	return 1;
}

/* modify the group entries for all `groups', by adding `user' */
static int
append_group(char *user, int ngroups, const char **groups)
{
	struct group	*grp;
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	char		*colon;
	int		groupc;
	int		entc;
	int		fd;
	int		nc;
	int		cc;
	int		i;
	int		j;

	for (i = 0 ; i < ngroups ; i++) {
		if ((grp = getgrnam(groups[i])) == NULL) {
			warnx("can't append group `%s' for user `%s'", groups[i], user);
		} else {
			for (j = 0 ; grp->gr_mem[j] ; j++) {
				if (strcmp(user, grp->gr_mem[j]) == 0) {
					/* already in it */
					groups[i] = "";
				}
			}
		}
	}
	if ((from = fopen(_PATH_GROUP, "r")) == NULL) {
		warn("can't append group for `%s': can't open `%s'", user, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		(void) fclose(from);
		warn("can't create gid: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(f);
		warn("can't create gid: fdopen `%s' failed", f);
		return 0;
	}
	while (fgets(buf, sizeof(buf), from) != NULL) {
		cc = strlen(buf);
		if ((colon = strchr(buf, ':')) == NULL) {
			warn("badly formed entry `%s'", buf);
			continue;
		}
		entc = (int)(colon - buf);
		for (i = 0 ; i < ngroups ; i++) {
			if ((groupc = strlen(groups[i])) == 0) {
				continue;
			}
			if (entc == groupc && strncmp(groups[i], buf, (unsigned) entc) == 0) {
				if ((nc = snprintf(&buf[cc - 1],
						sizeof(buf) - cc + 1,
						"%s%s\n",
						(buf[cc - 2] == ':') ? "" : ",",
						user)) < 0) {
					warnx("Warning: group `%s' entry too long", groups[i]);
				}
				cc += nc - 1;
			}
		}
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			(void) fclose(from);
			(void) close(fd);
			(void) unlink(f);
			warn("can't create gid: short write to `%s'", f);
			return 0;
		}
	}
	(void) fclose(from);
	(void) fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		(void) unlink(f);
		warn("can't create gid: can't rename `%s' to `%s'", f, _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	return 1;
}

/* return 1 if `login' is a valid login name */
static int
valid_login(char *login_name, int allow_samba)
{
	unsigned char	*cp;

	if (strlen(login_name) >= LOGIN_NAME_MAX) {
		return 0;
	}
	for (cp = login_name ; *cp ; cp++) {
		if (!isalnum(*cp) && *cp != '.' && *cp != '_' && *cp != '-') {
#ifdef EXTENSIONS
			/* check for a trailing '$' in a Samba user name */
			if (allow_samba && *cp == '$' && *(cp + 1) == 0x0) {
				return 1;
			}	
#endif
			return 0;
		}
	}
	return 1;
}

/* return 1 if `group' is a valid group name */
static int
valid_group(char *group)
{
	unsigned char	*cp;

	for (cp = group ; *cp ; cp++) {
		if (!isalnum(*cp)) {
			return 0;
		}
	}
	return 1;
}

/* find the next gid in the range lo .. hi */
static int
getnextgid(int *gidp, int lo, int hi)
{
	for (*gidp = lo ; *gidp < hi ; *gidp += 1) {
		if (getgrgid((gid_t)*gidp) == NULL) {
			return 1;
		}
	}
	return 0;
}

#ifdef EXTENSIONS
/* save a range of uids */
static int
save_range(user_t *up, char *cp)
{
	int	from;
	int	to;
	int	i;

	if (up->u_rsize == 0) {
		up->u_rsize = 32;
		NEWARRAY(range_t, up->u_rv, up->u_rsize, return(0));
	} else if (up->u_rc == up->u_rsize) {
		up->u_rsize *= 2;
		RENEW(range_t, up->u_rv, up->u_rsize, return(0));
	}
	if (up->u_rv && sscanf(cp, "%d..%d", &from, &to) == 2) {
		for (i = up->u_defrc ; i < up->u_rc ; i++) {
			if (up->u_rv[i].r_from == from && up->u_rv[i].r_to == to) {
				break;
			}
		}
		if (i == up->u_rc) {
			up->u_rv[up->u_rc].r_from = from;
			up->u_rv[up->u_rc].r_to = to;
			up->u_rc += 1;
		}
	} else {
		warnx("Bad range `%s'", cp);
		return 0;
	}
	return 1;
}
#endif

/* set the defaults in the defaults file */
static int
setdefaults(user_t *up)
{
	char	template[MaxFileNameLen];
	FILE	*fp;
	int	ret;
	int	fd;
#ifdef EXTENSIONS
	int	i;
#endif

	(void) snprintf(template, sizeof(template), "%s.XXXXXX", CONFFILE);
	if ((fd = mkstemp(template)) < 0) {
		warnx("can't mkstemp `%s' for writing", CONFFILE);
		return 0;
	}
	if ((fp = fdopen(fd, "w")) == NULL) {
		warn("can't fdopen `%s' for writing", CONFFILE);
		return 0;
	}
	ret = 1;
	if (fprintf(fp, "group\t\t%s\n", up->u_primgrp) <= 0 ||
	    fprintf(fp, "base_dir\t%s\n", up->u_basedir) <= 0 ||
	    fprintf(fp, "skel_dir\t%s\n", up->u_skeldir) <= 0 ||
	    fprintf(fp, "shell\t\t%s\n", up->u_shell) <= 0 ||
#ifdef EXTENSIONS
	    fprintf(fp, "class\t\t%s\n", up->u_class) <= 0 ||
#endif
	    fprintf(fp, "inactive\t%s\n", (up->u_inactive == NULL) ?  UNSET_INACTIVE : up->u_inactive) <= 0 ||
	    fprintf(fp, "expire\t\t%s\n", (up->u_expire == NULL) ?  UNSET_EXPIRY : up->u_expire) <= 0 ||
	    fprintf(fp, "preserve\t%s\n", (up->u_preserve == 0) ? "false" : "true") <= 0) {
		warn("can't write to `%s'", CONFFILE);
		ret = 0;
	}
#ifdef EXTENSIONS
	for (i = (up->u_defrc != up->u_rc) ? up->u_defrc : 0 ; i < up->u_rc ; i++) {
		if (fprintf(fp, "range\t\t%d..%d\n", up->u_rv[i].r_from, up->u_rv[i].r_to) <= 0) {
			warn("can't write to `%s'", CONFFILE);
			ret = 0;
		}
	}
#endif
	(void) fclose(fp);
	if (ret) {
		ret = ((rename(template, CONFFILE) == 0) && (chmod(CONFFILE, 0644) == 0));
	}
	return ret;
}

/* read the defaults file */
static void
read_defaults(user_t *up)
{
	struct stat	st;
	size_t		lineno;
	size_t		len;
	FILE		*fp;
	unsigned char	*cp;
	unsigned char	*s;

	memsave(&up->u_primgrp, DEF_GROUP, strlen(DEF_GROUP));
	memsave(&up->u_basedir, DEF_BASEDIR, strlen(DEF_BASEDIR));
	memsave(&up->u_skeldir, DEF_SKELDIR, strlen(DEF_SKELDIR));
	memsave(&up->u_shell, DEF_SHELL, strlen(DEF_SHELL));
	memsave(&up->u_comment, DEF_COMMENT, strlen(DEF_COMMENT));
#ifdef EXTENSIONS
	memsave(&up->u_class, DEF_CLASS, strlen(DEF_CLASS));
#endif
	up->u_rsize = 16;
	up->u_defrc = 0;
	NEWARRAY(range_t, up->u_rv, up->u_rsize, exit(1));
	up->u_inactive = DEF_INACTIVE;
	up->u_expire = DEF_EXPIRE;
	if ((fp = fopen(CONFFILE, "r")) == NULL) {
		if (stat(CONFFILE, &st) < 0 && !setdefaults(up)) {
			warn("can't create `%s' defaults file", CONFFILE);
		}
		fp = fopen(CONFFILE, "r");
	}
	if (fp != NULL) {
		while ((s = fparseln(fp, &len, &lineno, NULL, 0)) != NULL) {
			if (strncmp(s, "group", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_primgrp, cp, strlen(cp));
			} else if (strncmp(s, "base_dir", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_basedir, cp, strlen(cp));
			} else if (strncmp(s, "skel_dir", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_skeldir, cp, strlen(cp));
			} else if (strncmp(s, "shell", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_shell, cp, strlen(cp));
#ifdef EXTENSIONS
			} else if (strncmp(s, "class", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_class, cp, strlen(cp));
#endif
			} else if (strncmp(s, "inactive", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				if (strcmp(cp, UNSET_INACTIVE) == 0) {
					if (up->u_inactive) {
						FREE(up->u_inactive);
					}
					up->u_inactive = NULL;
				} else {
					memsave(&up->u_inactive, cp, strlen(cp));
				}
#ifdef EXTENSIONS
			} else if (strncmp(s, "range", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				(void) save_range(up, cp);
#endif
#ifdef EXTENSIONS
			} else if (strncmp(s, "preserve", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				up->u_preserve = (strncmp(cp, "true", 4) == 0) ? 1 :
						  (strncmp(cp, "yes", 3) == 0) ? 1 :
						   atoi(cp);
#endif
			} else if (strncmp(s, "expire", 6) == 0) {
				for (cp = s + 6 ; *cp && isspace(*cp) ; cp++) {
				}
				if (strcmp(cp, UNSET_EXPIRY) == 0) {
					if (up->u_expire) {
						FREE(up->u_expire);
					}
					up->u_expire = NULL;
				} else {
					memsave(&up->u_expire, cp, strlen(cp));
				}
			}
			(void) free(s);
		}
		(void) fclose(fp);
	}
	if (up->u_rc == 0) {
		up->u_rv[up->u_rc].r_from = DEF_LOWUID;
		up->u_rv[up->u_rc].r_to = DEF_HIGHUID;
		up->u_rc += 1;
	}
	up->u_defrc = up->u_rc;
}

/* return the next valid unused uid */
static int
getnextuid(int sync_uid_gid, int *uid, int low_uid, int high_uid)
{
	for (*uid = low_uid ; *uid <= high_uid ; (*uid)++) {
		if (getpwuid((uid_t)(*uid)) == NULL && *uid != NOBODY_UID) {
			if (sync_uid_gid) {
				if (getgrgid((gid_t)(*uid)) == NULL) {
					return 1;
				}
			} else {
				return 1;
			}
		}
	}
	return 0;
}

/* structure which defines a password type */
typedef struct passwd_type_t {
	const char     *type;		/* optional type descriptor */
	int		desc_length;	/* length of type descriptor */
	int		length;		/* length of password */
	const char     *regex;		/* regexp to output the password */
	int		re_sub;		/* subscript of regexp to use */
} passwd_type_t;

static passwd_type_t	passwd_types[] = {
	{ "$2a",	3,	54,	"\\$[^$]+\\$[^$]+\\$(.*)",	1 },	/* Blowfish */
	{ "$1",		2,	34,	NULL,				0 },	/* MD5 */
	{ "",		0,	DES_Len,NULL,				0 },	/* standard DES */
	{ NULL,		-1,	-1,	NULL,				0 }	/* none - terminate search */
};

/* return non-zero if it's a valid password - check length for cipher type */
static int
valid_password_length(char *newpasswd)
{
	passwd_type_t  *pwtp;
	regmatch_t	matchv[10];
	regex_t		r;

	for (pwtp = passwd_types ; pwtp->desc_length >= 0 ; pwtp++) {
		if (strncmp(newpasswd, pwtp->type, pwtp->desc_length) == 0) {
			if (pwtp->regex == NULL) {
				return strlen(newpasswd) == pwtp->length;
			}
			(void) regcomp(&r, pwtp->regex, REG_EXTENDED);
			if (regexec(&r, newpasswd, 10, matchv, 0) == 0) {
				regfree(&r);
				return (int)(matchv[pwtp->re_sub].rm_eo - matchv[pwtp->re_sub].rm_so + 1) == pwtp->length;
			}
			regfree(&r);
		}
	}
	return 0;
}

/* look for a valid time, return 0 if it was specified but bad */
static int
scantime(time_t *tp, char *s)
{
	struct tm	tm;

	*tp = 0;
	if (s != NULL) {
		(void) memset(&tm, 0, sizeof(tm));
		if (strptime(s, "%c", &tm) != NULL) {
			*tp = mktime(&tm);
		} else if (strptime(s, "%B %d %Y", &tm) != NULL) {
			*tp = mktime(&tm);
		} else if (isdigit((unsigned char) *s)) {
			*tp = atoi(s);
		} else {
			return 0;
		}
	}
	return 1;
}

/* add a user */
static int
adduser(char *login_name, user_t *up)
{
	struct group	*grp;
	struct stat	st;
	time_t		expire;
	time_t		inactive;
	char		password[PasswordLength + 1];
	char		home[MaxFileNameLen];
	char		buf[MaxFileNameLen];
	int		sync_uid_gid;
	int		masterfd;
	int		ptmpfd;
	int		gid;
	int		cc;
	int		i;

	if (!valid_login(login_name, up->u_allow_samba)) {
		errx(EXIT_FAILURE, "`%s' is not a valid login name", login_name);
	}
	if ((masterfd = open(_PATH_MASTERPASSWD, O_RDONLY)) < 0) {
		err(EXIT_FAILURE, "can't open `%s'", _PATH_MASTERPASSWD);
	}
	if (flock(masterfd, LOCK_EX | LOCK_NB) < 0) {
		err(EXIT_FAILURE, "can't lock `%s'", _PATH_MASTERPASSWD);
	}
	pw_init();
	if ((ptmpfd = pw_lock(WAITSECS)) < 0) {
		(void) close(masterfd);
		err(EXIT_FAILURE, "can't obtain pw_lock");
	}
	while ((cc = read(masterfd, buf, sizeof(buf))) > 0) {
		if (write(ptmpfd, buf, (size_t)(cc)) != cc) {
			(void) close(masterfd);
			(void) close(ptmpfd);
			pw_abort();
			err(EXIT_FAILURE, "short write to /etc/ptmp (not %d chars)", cc);
		}
	}
	/* if no uid was specified, get next one in [low_uid..high_uid] range */
	sync_uid_gid = (strcmp(up->u_primgrp, "=uid") == 0);
	if (up->u_uid == -1) {
		int	got_id = 0;

		/*
		 * Look for a free UID in the command line ranges (if any).
		 * These start after the ranges specified in the config file.
		 */
		for (i = up->u_defrc; !got_id && i < up->u_rc ; i++) {
			got_id = getnextuid(sync_uid_gid, &up->u_uid,
					up->u_rv[i].r_from, up->u_rv[i].r_to);
		}
		/*
		 * If there were no free UIDs in the command line ranges,
		 * try the ranges from the config file (there will always
		 * be at least one default).
		 */
		for (i = 0; !got_id && i < up->u_defrc; i++) {
			got_id = getnextuid(sync_uid_gid, &up->u_uid,
					up->u_rv[i].r_from, up->u_rv[i].r_to);
		}
		if (!got_id) {
			(void) close(ptmpfd);
			pw_abort();
			errx(EXIT_FAILURE, "can't get next uid for %d", up->u_uid);
		}
	}
	/* check uid isn't already allocated */
	if (!(up->u_flags & F_DUPUID) && getpwuid((uid_t)(up->u_uid)) != NULL) {
		(void) close(ptmpfd);
		pw_abort();
		errx(EXIT_FAILURE, "uid %d is already in use", up->u_uid);
	}
	/* if -g=uid was specified, check gid is unused */
	if (sync_uid_gid) {
		if (getgrgid((gid_t)(up->u_uid)) != NULL) {
			(void) close(ptmpfd);
			pw_abort();
			errx(EXIT_FAILURE, "gid %d is already in use", up->u_uid);
		}
		gid = up->u_uid;
	} else if ((grp = getgrnam(up->u_primgrp)) != NULL) {
		gid = grp->gr_gid;
	} else if (is_number(up->u_primgrp) &&
		   (grp = getgrgid((gid_t)atoi(up->u_primgrp))) != NULL) {
		gid = grp->gr_gid;
	} else {
		(void) close(ptmpfd);
		pw_abort();
		errx(EXIT_FAILURE, "group %s not found", up->u_primgrp);
	}
	/* check name isn't already in use */
	if (!(up->u_flags & F_DUPUID) && getpwnam(login_name) != NULL) {
		(void) close(ptmpfd);
		pw_abort();
		errx(EXIT_FAILURE, "already a `%s' user", login_name);
	}
	if (up->u_flags & F_HOMEDIR) {
		(void) strlcpy(home, up->u_home, sizeof(home));
	} else {
		/* if home directory hasn't been given, make it up */
		(void) snprintf(home, sizeof(home), "%s/%s", up->u_basedir, login_name);
	}
	if (!scantime(&inactive, up->u_inactive)) {
		warnx("Warning: inactive time `%s' invalid, account expiry off",
				up->u_inactive);
	}
	if (!scantime(&expire, up->u_expire)) {
		warnx("Warning: expire time `%s' invalid, password expiry off",
				up->u_expire);
	}
	if (lstat(home, &st) < 0 && !(up->u_flags & F_MKDIR)) {
		warnx("Warning: home directory `%s' doesn't exist, and -m was not specified",
		    home);
	}
	password[sizeof(password) - 1] = '\0';
	if (up->u_password != NULL && valid_password_length(up->u_password)) {
		(void) strlcpy(password, up->u_password, sizeof(password));
	} else {
		(void) memset(password, '*', DES_Len);
		password[DES_Len] = 0;
		if (up->u_password != NULL) {
			warnx("Password `%s' is invalid: setting it to `%s'",
				up->u_password, password);
		}
	}
	cc = snprintf(buf, sizeof(buf), "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
			login_name,
			password,
			up->u_uid,
			gid,
#ifdef EXTENSIONS
			up->u_class,
#else
			"",
#endif
			(long) inactive,
			(long) expire,
			up->u_comment,
			home,
			up->u_shell);
	if (write(ptmpfd, buf, (size_t) cc) != cc) {
		(void) close(ptmpfd);
		pw_abort();
		err(EXIT_FAILURE, "can't add `%s'", buf);
	}
	if (up->u_flags & F_MKDIR) {
		if (lstat(home, &st) == 0) {
			(void) close(ptmpfd);
			pw_abort();
			errx(EXIT_FAILURE, "home directory `%s' already exists", home);
		} else {
			if (asystem("%s -p %s", MKDIR, home) != 0) {
				(void) close(ptmpfd);
				pw_abort();
				err(EXIT_FAILURE, "can't mkdir `%s'", home);
			}
			(void) copydotfiles(up->u_skeldir, up->u_uid, gid, home);
		}
	}
	if (strcmp(up->u_primgrp, "=uid") == 0 &&
	    getgrnam(login_name) == NULL &&
	    !creategid(login_name, gid, login_name)) {
		(void) close(ptmpfd);
		pw_abort();
		errx(EXIT_FAILURE, "can't create gid %d for login name %s", gid, login_name);
	}
	if (up->u_groupc > 0 && !append_group(login_name, up->u_groupc, up->u_groupv)) {
		(void) close(ptmpfd);
		pw_abort();
		errx(EXIT_FAILURE, "can't append `%s' to new groups", login_name);
	}
	(void) close(ptmpfd);
#if PW_MKDB_ARGC == 2
	if (pw_mkdb(login_name, 0) < 0) {
		pw_abort();
		err(EXIT_FAILURE, "pw_mkdb failed");
	}
#else
	if (pw_mkdb() < 0) {
		pw_abort();
		err(EXIT_FAILURE, "pw_mkdb failed");
	}
#endif
	syslog(LOG_INFO, "new user added: name=%s, uid=%d, gid=%d, home=%s, shell=%s", 
		login_name, up->u_uid, gid, home, up->u_shell);
	return 1;
}

/* remove a user from the groups file */
static int
rm_user_from_groups(char *login_name)
{
	struct stat	st;
	regmatch_t	matchv[10];
	regex_t		r;
	FILE		*from;
	FILE		*to;
	char		line[MaxEntryLen];
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	int		fd;
	int		cc;
	int		sc;

	(void) snprintf(line, sizeof(line), "(:|,)(%s)(,|$)", login_name);
	if (regcomp(&r, line, REG_EXTENDED|REG_NEWLINE) != 0) {
		warn("can't compile regular expression `%s'", line);
		return 0;
	}
	if ((from = fopen(_PATH_GROUP, "r")) == NULL) {
		warn("can't remove gid for `%s': can't open `%s'", login_name, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		(void) fclose(from);
		warn("can't create gid: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(f);
		warn("can't create gid: fdopen `%s' failed", f);
		return 0;
	}
	while (fgets(buf, sizeof(buf), from) > 0) {
		cc = strlen(buf);
		if (regexec(&r, buf, 10, matchv, 0) == 0) {
			if (buf[(int)matchv[1].rm_so] == ',') {
				matchv[2].rm_so = matchv[1].rm_so;
			} else if (matchv[2].rm_eo != matchv[3].rm_eo) {
				matchv[2].rm_eo = matchv[3].rm_eo;
			}
			cc -= (int) matchv[2].rm_eo;
			sc = (int) matchv[2].rm_so;
			if (fwrite(buf, sizeof(char), sc, to) != sc ||
			    fwrite(&buf[(int)matchv[2].rm_eo], sizeof(char), cc, to) != cc) {
				(void) fclose(from);
				(void) close(fd);
				(void) unlink(f);
				warn("can't create gid: short write to `%s'", f);
				return 0;
			}
		} else if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			(void) fclose(from);
			(void) close(fd);
			(void) unlink(f);
			warn("can't create gid: short write to `%s'", f);
			return 0;
		}
	}
	(void) fclose(from);
	(void) fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		(void) unlink(f);
		warn("can't create gid: can't rename `%s' to `%s'", f, _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	return 1;
}

/* check that the user or group is local, not from YP/NIS */
static int
is_local(char *name, const char *file)
{
	FILE	       *fp;
	char		buf[MaxEntryLen];
	int		len;
	int		ret;

	if ((fp = fopen(file, "r")) == NULL) {
		err(EXIT_FAILURE, "can't open `%s'", file);
	}
	len = strlen(name);
	for (ret = 0 ; fgets(buf, sizeof(buf), fp) != NULL ; ) {
		if (strncmp(buf, name, len) == 0 && buf[len] == ':') {
			ret = 1;
			break;
		}
	}
	(void) fclose(fp);
	return ret;
}

/* modify a user */
static int
moduser(char *login_name, char *newlogin, user_t *up, int allow_samba)
{
	struct passwd  *pwp;
	struct group   *grp;
	const char     *homedir;
	size_t		colonc;
	size_t		loginc;
	size_t		len;
	size_t		cc;
	FILE	       *master;
	char		newdir[MaxFileNameLen];
	char	        buf[MaxEntryLen];
	char	       *colon;
	int		masterfd;
	int		ptmpfd;
	int		error;

	if (!valid_login(newlogin, allow_samba)) {
		errx(EXIT_FAILURE, "`%s' is not a valid login name", login_name);
	}
	if ((pwp = getpwnam(login_name)) == NULL) {
		errx(EXIT_FAILURE, "No such user `%s'", login_name);
	}
	if (!is_local(login_name, _PATH_MASTERPASSWD)) {
		errx(EXIT_FAILURE, "User `%s' must be a local user", login_name);
	}
	/* keep dir name in case we need it for '-m' */
	homedir = pwp->pw_dir;

	if ((masterfd = open(_PATH_MASTERPASSWD, O_RDONLY)) < 0) {
		err(EXIT_FAILURE, "can't open `%s'", _PATH_MASTERPASSWD);
	}
	if (flock(masterfd, LOCK_EX | LOCK_NB) < 0) {
		err(EXIT_FAILURE, "can't lock `%s'", _PATH_MASTERPASSWD);
	}
	pw_init();
	if ((ptmpfd = pw_lock(WAITSECS)) < 0) {
		(void) close(masterfd);
		err(EXIT_FAILURE, "can't obtain pw_lock");
	}
	if ((master = fdopen(masterfd, "r")) == NULL) {
		(void) close(masterfd);
		(void) close(ptmpfd);
		pw_abort();
		err(EXIT_FAILURE, "can't fdopen fd for %s", _PATH_MASTERPASSWD);
	}
	if (up != NULL) {
		if (up->u_flags & F_USERNAME) {
			/* if changing name, check new name isn't already in use */
			if (strcmp(login_name, newlogin) != 0 && getpwnam(newlogin) != NULL) {
				(void) close(ptmpfd);
				pw_abort();
				errx(EXIT_FAILURE, "already a `%s' user", newlogin);
			}
			pwp->pw_name = newlogin;

			/*
			 * Provide a new directory name in case the
			 * home directory is to be moved.
			 */
			if (up->u_flags & F_MKDIR) {
				snprintf(newdir, sizeof(newdir), "%s/%s", up->u_basedir, newlogin);
				pwp->pw_dir = newdir;
			}
		}
		if (up->u_flags & F_PASSWORD) {
			if (up->u_password != NULL) {
				if (!valid_password_length(up->u_password)) {
					(void) close(ptmpfd);
					pw_abort();
					errx(EXIT_FAILURE, "Invalid password: `%s'",
						up->u_password);
				}
				pwp->pw_passwd = up->u_password;
			}
		}
		if (up->u_flags & F_UID) {
			/* check uid isn't already allocated */
			if (!(up->u_flags & F_DUPUID) && getpwuid((uid_t)(up->u_uid)) != NULL) {
				(void) close(ptmpfd);
				pw_abort();
				errx(EXIT_FAILURE, "uid %d is already in use", up->u_uid);
			}
			pwp->pw_uid = up->u_uid;
		}
		if (up->u_flags & F_GROUP) {
			/* if -g=uid was specified, check gid is unused */
			if (strcmp(up->u_primgrp, "=uid") == 0) {
				if (getgrgid((gid_t)(up->u_uid)) != NULL) {
					(void) close(ptmpfd);
					pw_abort();
					errx(EXIT_FAILURE, "gid %d is already in use", up->u_uid);
				}
				pwp->pw_gid = up->u_uid;
			} else if ((grp = getgrnam(up->u_primgrp)) != NULL) {
				pwp->pw_gid = grp->gr_gid;
			} else if (is_number(up->u_primgrp) &&
				   (grp = getgrgid((gid_t)atoi(up->u_primgrp))) != NULL) {
				pwp->pw_gid = grp->gr_gid;
			} else {
				(void) close(ptmpfd);
				pw_abort();
				errx(EXIT_FAILURE, "group %s not found", up->u_primgrp);
			}
		}
		if (up->u_flags & F_INACTIVE) {
			if (!scantime(&pwp->pw_change, up->u_inactive)) {
				warnx("Warning: inactive time `%s' invalid, password expiry off",
					up->u_inactive);
			}
		}
		if (up->u_flags & F_EXPIRE) {
			if (!scantime(&pwp->pw_expire, up->u_expire)) {
				warnx("Warning: expire time `%s' invalid, password expiry off",
					up->u_expire);
			}
		}
		if (up->u_flags & F_COMMENT)
			pwp->pw_gecos = up->u_comment;
		if (up->u_flags & F_HOMEDIR)
			pwp->pw_dir = up->u_home;
		if (up->u_flags & F_SHELL)
			pwp->pw_shell = up->u_shell;
#ifdef EXTENSIONS
		if (up->u_flags & F_CLASS)
			pwp->pw_class = up->u_class;
#endif
	}
	loginc = strlen(login_name);
	while (fgets(buf, sizeof(buf), master) != NULL) {
		if ((colon = strchr(buf, ':')) == NULL) {
			warnx("Malformed entry `%s'. Skipping", buf);
			continue;
		}
		colonc = (size_t)(colon - buf);
		if (strncmp(login_name, buf, loginc) == 0 && loginc == colonc) {
			if (up != NULL) {
				len = snprintf(buf, sizeof(buf), "%s:%s:%d:%d:"
#ifdef EXTENSIONS
									 "%s" 
#endif
									      ":%ld:%ld:%s:%s:%s\n",
					newlogin,
					pwp->pw_passwd,
					pwp->pw_uid,
					pwp->pw_gid,
#ifdef EXTENSIONS
					pwp->pw_class,
#endif
					(long)pwp->pw_change,
					(long)pwp->pw_expire,
					pwp->pw_gecos,
					pwp->pw_dir,
					pwp->pw_shell);
				if (write(ptmpfd, buf, len) != len) {
					(void) close(ptmpfd);
					pw_abort();
					err(EXIT_FAILURE, "can't add `%s'", buf);
				}
			}
		} else {
			len = strlen(buf);
			if ((cc = write(ptmpfd, buf, len)) != len) {
				(void) close(masterfd);
				(void) close(ptmpfd);
				pw_abort();
				err(EXIT_FAILURE, "short write to /etc/ptmp (%lld not %lld chars)",
					(long long)cc,
					(long long)len);
			}
		}
	}
	if (up != NULL) {
		if ((up->u_flags & F_MKDIR) &&
		    asystem("%s %s %s", MV, homedir, pwp->pw_dir) != 0) {
			(void) close(ptmpfd);
			pw_abort();
			err(EXIT_FAILURE, "can't move `%s' to `%s'",
				homedir, pwp->pw_dir);
		}
		if (up->u_groupc > 0 &&
		    !append_group(newlogin, up->u_groupc, up->u_groupv)) {
			(void) close(ptmpfd);
			pw_abort();
			errx(EXIT_FAILURE, "can't append `%s' to new groups",
				newlogin);
		}
	}
	(void) close(ptmpfd);
#if PW_MKDB_ARGC == 2
	if (up != NULL && strcmp(login_name, newlogin) == 0) {
		error = pw_mkdb(login_name, 0);
	} else {
		error = pw_mkdb(NULL, 0);
	}
#else
	error = pw_mkdb();
#endif
	if (error < 0) {
		pw_abort();
		err(EXIT_FAILURE, "pw_mkdb failed");
	}
	if (up == NULL) {
		syslog(LOG_INFO, "user removed: name=%s", login_name);
	} else if (strcmp(login_name, newlogin) == 0) {
		syslog(LOG_INFO, "user information modified: name=%s, uid=%d, gid=%d, home=%s, shell=%s", 
			login_name, pwp->pw_uid, pwp->pw_gid, pwp->pw_dir, pwp->pw_shell);
	} else {
		syslog(LOG_INFO, "user information modified: name=%s, new name=%s, uid=%d, gid=%d, home=%s, shell=%s", 
			login_name, newlogin, pwp->pw_uid, pwp->pw_gid, pwp->pw_dir, pwp->pw_shell);
	}
	return 1;
}


#ifdef EXTENSIONS
/* see if we can find out the user struct */
static struct passwd *
find_user_info(char *name)
{
	struct passwd	*pwp;

	if ((pwp = getpwnam(name)) != NULL) {
		return pwp;
	}
	if (is_number(name) && (pwp = getpwuid((uid_t)atoi(name))) != NULL) {
		return pwp;
	}
	return NULL;
}
#endif

#ifdef EXTENSIONS
/* see if we can find out the group struct */
static struct group *
find_group_info(char *name)
{
	struct group	*grp;

	if ((grp = getgrnam(name)) != NULL) {
		return grp;
	}
	if (is_number(name) && (grp = getgrgid((gid_t)atoi(name))) != NULL) {
		return grp;
	}
	return NULL;
}
#endif

/* print out usage message, and then exit */
void
usermgmt_usage(const char *prog)
{
	if (strcmp(prog, "useradd") == 0) {
		(void) fprintf(stderr, "usage: %s -D [-b basedir] [-e expiry] "
		    "[-f inactive] [-g group]\n\t[-r lowuid..highuid] "
		    "[-s shell] [-L class]\n", prog);
		(void) fprintf(stderr, "usage: %s [-G group] [-b basedir] "
		    "[-c comment] [-d homedir] [-e expiry]\n\t[-f inactive] "
		    "[-g group] [-k skeletondir] [-m] [-o] [-p password]\n"
		    "\t[-r lowuid..highuid] [-s shell]\n\t[-u uid] [-v] user\n",
		    prog);
	} else if (strcmp(prog, "usermod") == 0) {
		(void) fprintf(stderr, "usage: %s [-G group] [-c comment] "
		    "[-d homedir] [-e expire] [-f inactive]\n\t[-g group] "
		    "[-l newname] [-m] [-o] [-p password] [-s shell] [-u uid]\n"
		    "\t[-L class] [-v] user\n", prog);
	} else if (strcmp(prog, "userdel") == 0) {
		(void) fprintf(stderr, "usage: %s -D [-p preserve]\n", prog);
		(void) fprintf(stderr,
		    "usage: %s [-p preserve] [-r] [-v] user\n", prog);
#ifdef EXTENSIONS
	} else if (strcmp(prog, "userinfo") == 0) {
		(void) fprintf(stderr, "usage: %s [-e] [-v] user\n", prog);
#endif
	} else if (strcmp(prog, "groupadd") == 0) {
		(void) fprintf(stderr, "usage: %s [-g gid] [-o] [-v] group\n",
		    prog);
	} else if (strcmp(prog, "groupdel") == 0) {
		(void) fprintf(stderr, "usage: %s [-v] group\n", prog);
	} else if (strcmp(prog, "groupmod") == 0) {
		(void) fprintf(stderr,
		    "usage: %s [-g gid] [-o] [-n newname] [-v] group\n", prog);
	} else if (strcmp(prog, "user") == 0 || strcmp(prog, "group") == 0) {
		(void) fprintf(stderr,
		    "usage: %s ( add | del | mod | info ) ...\n", prog);
#ifdef EXTENSIONS
	} else if (strcmp(prog, "groupinfo") == 0) {
		(void) fprintf(stderr, "usage: %s [-e] [-v] group\n", prog);
#endif
	}
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

#ifdef EXTENSIONS
#define ADD_OPT_EXTENSIONS	"p:r:vL:S"
#else
#define ADD_OPT_EXTENSIONS	
#endif

int
useradd(int argc, char **argv)
{
	user_t	u;
	int	defaultfield;
	int	bigD;
	int	c;
#ifdef EXTENSIONS
	int	i;
#endif

	(void) memset(&u, 0, sizeof(u));
	read_defaults(&u);
	u.u_uid = -1;
	defaultfield = bigD = 0;
	while ((c = getopt(argc, argv, "DG:b:c:d:e:f:g:k:mou:s:" ADD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'D':
			bigD = 1;
			break;
		case 'G':
			while ((u.u_groupv[u.u_groupc] = strsep(&optarg, ",")) != NULL &&
			       u.u_groupc < NGROUPS_MAX) {
				if (u.u_groupv[u.u_groupc][0] != 0) {
					u.u_groupc++;
				}
			}
			if (optarg != NULL) {
				warnx("Truncated list of secondary groups to %d entries", NGROUPS_MAX);
			}
			break;
#ifdef EXTENSIONS
		case 'S':
			u.u_allow_samba = 1;
			break;
#endif
		case 'b':
			defaultfield = 1;
			memsave(&u.u_basedir, optarg, strlen(optarg));
			break;
		case 'c':
			memsave(&u.u_comment, optarg, strlen(optarg));
			break;
		case 'd':
			memsave(&u.u_home, optarg, strlen(optarg));
			u.u_flags |= F_HOMEDIR;
			break;
		case 'e':
			defaultfield = 1;
			memsave(&u.u_expire, optarg, strlen(optarg));
			break;
		case 'f':
			defaultfield = 1;
			memsave(&u.u_inactive, optarg, strlen(optarg));
			break;
		case 'g':
			defaultfield = 1;
			memsave(&u.u_primgrp, optarg, strlen(optarg));
			break;
		case 'k':
			defaultfield = 1;
			memsave(&u.u_skeldir, optarg, strlen(optarg));
			break;
#ifdef EXTENSIONS
		case 'L':
			defaultfield = 1;
			memsave(&u.u_class, optarg, strlen(optarg));
			break;
#endif
		case 'm':
			u.u_flags |= F_MKDIR;
			break;
		case 'o':
			u.u_flags |= F_DUPUID;
			break;
#ifdef EXTENSIONS
		case 'p':
			memsave(&u.u_password, optarg, strlen(optarg));
			break;
#endif
#ifdef EXTENSIONS
		case 'r':
			defaultfield = 1;
			(void) save_range(&u, optarg);
			break;
#endif
		case 's':
			defaultfield = 1;
			memsave(&u.u_shell, optarg, strlen(optarg));
			break;
		case 'u':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-u uid], the uid must be numeric");
			}
			u.u_uid = atoi(optarg);
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("useradd");
			/* NOTREACHED */
		}
	}
	if (bigD) {
		if (defaultfield) {
			checkeuid();
			return setdefaults(&u) ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		(void) printf("group\t\t%s\n", u.u_primgrp);
		(void) printf("base_dir\t%s\n", u.u_basedir);
		(void) printf("skel_dir\t%s\n", u.u_skeldir);
		(void) printf("shell\t\t%s\n", u.u_shell);
#ifdef EXTENSIONS
		(void) printf("class\t\t%s\n", u.u_class);
#endif
		(void) printf("inactive\t%s\n", (u.u_inactive == NULL) ? UNSET_INACTIVE : u.u_inactive);
		(void) printf("expire\t\t%s\n", (u.u_expire == NULL) ? UNSET_EXPIRY : u.u_expire);
#ifdef EXTENSIONS
		for (i = 0 ; i < u.u_rc ; i++) {
			(void) printf("range\t\t%d..%d\n", u.u_rv[i].r_from, u.u_rv[i].r_to);
		}
#endif
		return EXIT_SUCCESS;
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("useradd");
	}
	checkeuid();
	openlog("useradd", LOG_PID, LOG_USER);
	return adduser(*argv, &u) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define MOD_OPT_EXTENSIONS	"p:vL:S"
#else
#define MOD_OPT_EXTENSIONS	
#endif

int
usermod(int argc, char **argv)
{
	user_t	u;
	char	newuser[MaxUserNameLen + 1];
	int	c, have_new_user;

	(void) memset(&u, 0, sizeof(u));
	(void) memset(newuser, 0, sizeof(newuser));
	read_defaults(&u);
	have_new_user = 0;
	while ((c = getopt(argc, argv, "G:c:d:e:f:g:l:mos:u:" MOD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'G':
			while ((u.u_groupv[u.u_groupc] = strsep(&optarg, ",")) != NULL &&
			       u.u_groupc < NGROUPS_MAX) {
				if (u.u_groupv[u.u_groupc][0] != 0) {
					u.u_groupc++;
				}
			}
			if (optarg != NULL) {
				warnx("Truncated list of secondary groups to %d entries", NGROUPS_MAX);
			}
			u.u_flags |= F_SECGROUP;
			break;
#ifdef EXTENSIONS
		case 'S':
			u.u_allow_samba = 1;
			break;
#endif
		case 'c':
			memsave(&u.u_comment, optarg, strlen(optarg));
			u.u_flags |= F_COMMENT;
			break;
		case 'd':
			memsave(&u.u_home, optarg, strlen(optarg));
			u.u_flags |= F_HOMEDIR;
			break;
		case 'e':
			memsave(&u.u_expire, optarg, strlen(optarg));
			u.u_flags |= F_EXPIRE;
			break;
		case 'f':
			memsave(&u.u_inactive, optarg, strlen(optarg));
			u.u_flags |= F_INACTIVE;
			break;
		case 'g':
			memsave(&u.u_primgrp, optarg, strlen(optarg));
			u.u_flags |= F_GROUP;
			break;
		case 'l':
			(void) strlcpy(newuser, optarg, sizeof(newuser));
			have_new_user = 1;
			u.u_flags |= F_USERNAME;
			break;
#ifdef EXTENSIONS
		case 'L':
			memsave(&u.u_class, optarg, strlen(optarg));
			u.u_flags |= F_CLASS;
			break;
#endif
		case 'm':
			u.u_flags |= F_MKDIR;
			break;
		case 'o':
			u.u_flags |= F_DUPUID;
			break;
#ifdef EXTENSIONS
		case 'p':
			memsave(&u.u_password, optarg, strlen(optarg));
			u.u_flags |= F_PASSWORD;
			break;
#endif
		case 's':
			memsave(&u.u_shell, optarg, strlen(optarg));
			u.u_flags |= F_SHELL;
			break;
		case 'u':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-u uid], the uid must be numeric");
			}
			u.u_uid = atoi(optarg);
			u.u_flags |= F_UID;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("usermod");
			/* NOTREACHED */
		}
	}
	if ((u.u_flags & F_MKDIR) && !(u.u_flags & F_HOMEDIR) &&
	    !(u.u_flags & F_USERNAME)) {
		warnx("option 'm' useless without 'd' or 'l' -- ignored");
		u.u_flags &= ~F_MKDIR;
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("usermod");
	}
	checkeuid();
	openlog("usermod", LOG_PID, LOG_USER);
	return moduser(*argv, (have_new_user) ? newuser : *argv, &u, u.u_allow_samba) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define DEL_OPT_EXTENSIONS	"Dp:vS"
#else
#define DEL_OPT_EXTENSIONS	
#endif

int
userdel(int argc, char **argv)
{
	struct passwd	*pwp;
	user_t		u;
	char		password[PasswordLength + 1];
	int		defaultfield;
	int		rmhome;
	int		bigD;
	int		c;

	(void) memset(&u, 0, sizeof(u));
	read_defaults(&u);
	defaultfield = bigD = rmhome = 0;
	while ((c = getopt(argc, argv, "r" DEL_OPT_EXTENSIONS)) != -1) {
		switch(c) {
#ifdef EXTENSIONS
		case 'D':
			bigD = 1;
			break;
#endif
#ifdef EXTENSIONS
		case 'S':
			u.u_allow_samba = 1;
			break;
#endif
#ifdef EXTENSIONS
		case 'p':
			defaultfield = 1;
			u.u_preserve = (strcmp(optarg, "true") == 0) ? 1 :
					(strcmp(optarg, "yes") == 0) ? 1 :
					 atoi(optarg);
			break;
#endif
		case 'r':
			rmhome = 1;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("userdel");
			/* NOTREACHED */
		}
	}
#ifdef EXTENSIONS
	if (bigD) {
		if (defaultfield) {
			checkeuid();
			return setdefaults(&u) ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		(void) printf("preserve\t%s\n", (u.u_preserve) ? "true" : "false");
		return EXIT_SUCCESS;
	}
#endif
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("userdel");
	}
	checkeuid();
	if ((pwp = getpwnam(*argv)) == NULL) {
		warnx("No such user `%s'", *argv);
		return EXIT_FAILURE;
	}
	if (rmhome)
		(void)removehomedir(pwp->pw_name, pwp->pw_uid, pwp->pw_dir);
	if (u.u_preserve) {
		u.u_flags |= F_SHELL;
		memsave(&u.u_shell, NOLOGIN, strlen(NOLOGIN));
		(void) memset(password, '*', DES_Len);
		password[DES_Len] = 0;
		memsave(&u.u_password, password, strlen(password));
		u.u_flags |= F_PASSWORD;
		openlog("userdel", LOG_PID, LOG_USER);
		return moduser(*argv, *argv, &u, u.u_allow_samba) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	if (!rm_user_from_groups(*argv)) {
		return 0;
	}
	openlog("userdel", LOG_PID, LOG_USER);
	return moduser(*argv, *argv, NULL, u.u_allow_samba) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define GROUP_ADD_OPT_EXTENSIONS	"v"
#else
#define GROUP_ADD_OPT_EXTENSIONS	
#endif

/* add a group */
int
groupadd(int argc, char **argv)
{
	int	dupgid;
	int	gid;
	int	c;

	gid = -1;
	dupgid = 0;
	while ((c = getopt(argc, argv, "g:o" GROUP_ADD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'g':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-g gid], the gid must be numeric");
			}
			gid = atoi(optarg);
			break;
		case 'o':
			dupgid = 1;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("groupadd");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupadd");
	}
	checkeuid();
	if (gid < 0 && !getnextgid(&gid, LowGid, HighGid)) {
		err(EXIT_FAILURE, "can't add group: can't get next gid");
	}
	if (!dupgid && getgrgid((gid_t) gid) != NULL) {
		errx(EXIT_FAILURE, "can't add group: gid %d is a duplicate", gid);
	}
	if (!valid_group(*argv)) {
		warnx("warning - invalid group name `%s'", *argv);
	}
	openlog("groupadd", LOG_PID, LOG_USER);
	if (!creategid(*argv, gid, "")) {
		errx(EXIT_FAILURE, "can't add group: problems with %s file", _PATH_GROUP);
	}
	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
#define GROUP_DEL_OPT_EXTENSIONS	"v"
#else
#define GROUP_DEL_OPT_EXTENSIONS	
#endif

/* remove a group */
int
groupdel(int argc, char **argv)
{
	int	c;

	while ((c = getopt(argc, argv, "" GROUP_DEL_OPT_EXTENSIONS)) != -1) {
		switch(c) {
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("groupdel");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupdel");
	}
	if (getgrnam(*argv) == NULL) {
		errx(EXIT_FAILURE, "No such group `%s'", *argv);
	}
	checkeuid();
	openlog("groupdel", LOG_PID, LOG_USER);
	if (!modify_gid(*argv, NULL)) {
		err(EXIT_FAILURE, "can't change %s file", _PATH_GROUP);
	}
	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
#define GROUP_MOD_OPT_EXTENSIONS	"v"
#else
#define GROUP_MOD_OPT_EXTENSIONS	
#endif

/* modify a group */
int
groupmod(int argc, char **argv)
{
	struct group	*grp;
	char		buf[MaxEntryLen];
	char		*newname;
	char		**cpp;
	int		dupgid;
	int		gid;
	int		cc;
	int		c;

	gid = -1;
	dupgid = 0;
	newname = NULL;
	while ((c = getopt(argc, argv, "g:on:" GROUP_MOD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'g':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-g gid], the gid must be numeric");
			}
			gid = atoi(optarg);
			break;
		case 'o':
			dupgid = 1;
			break;
		case 'n':
			memsave(&newname, optarg, strlen(optarg));
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("groupmod");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupmod");
	}
	checkeuid();
	if (gid < 0 && newname == NULL) {
		err(EXIT_FAILURE, "Nothing to change");
	}
	if (dupgid && gid < 0) {
		err(EXIT_FAILURE, "Duplicate which gid?");
	}
	if ((grp = getgrnam(*argv)) == NULL) {
		errx(EXIT_FAILURE, "can't find group `%s' to modify", *argv);
	}
	if (!is_local(*argv, _PATH_GROUP)) {
		errx(EXIT_FAILURE, "Group `%s' must be a local group", *argv);
	}
	if (newname != NULL && !valid_group(newname)) {
		warn("warning - invalid group name `%s'", newname);
	}
	cc = snprintf(buf, sizeof(buf), "%s:%s:%d:",
			(newname) ? newname : grp->gr_name,
			grp->gr_passwd,
			(gid < 0) ? grp->gr_gid : gid);
	for (cpp = grp->gr_mem ; *cpp && cc < sizeof(buf) ; cpp++) {
		cc += snprintf(&buf[cc], sizeof(buf) - cc, "%s%s", *cpp,
			(cpp[1] == NULL) ? "" : ",");
	}
	cc += snprintf(&buf[cc], sizeof(buf) - cc, "\n");
	openlog("groupmod", LOG_PID, LOG_USER);
	if (!modify_gid(*argv, buf)) {
		err(EXIT_FAILURE, "can't change %s file", _PATH_GROUP);
	}
	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
/* display user information */
int
userinfo(int argc, char **argv)
{
	struct passwd	*pwp;
	struct group	*grp;
	char		buf[MaxEntryLen];
	char		**cpp;
	int		exists;
	int		cc;
	int		i;

	exists = 0;
	buf[0] = '\0';
	while ((i = getopt(argc, argv, "ev")) != -1) {
		switch(i) {
		case 'e':
			exists = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usermgmt_usage("userinfo");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("userinfo");
	}
	pwp = find_user_info(*argv);
	if (exists) {
		exit((pwp) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (pwp == NULL) {
		errx(EXIT_FAILURE, "can't find user `%s'", *argv);
	}
	(void) printf("login\t%s\n", pwp->pw_name);
	(void) printf("passwd\t%s\n", pwp->pw_passwd);
	(void) printf("uid\t%d\n", pwp->pw_uid);
	for (cc = 0 ; (grp = getgrent()) != NULL ; ) {
		for (cpp = grp->gr_mem ; *cpp ; cpp++) {
			if (strcmp(*cpp, *argv) == 0 && grp->gr_gid != pwp->pw_gid) {
				cc += snprintf(&buf[cc], sizeof(buf) - cc, "%s ", grp->gr_name);
			}
		}
	}
	if ((grp = getgrgid(pwp->pw_gid)) == NULL) {
		(void) printf("groups\t%d %s\n", pwp->pw_gid, buf);
	} else {
		(void) printf("groups\t%s %s\n", grp->gr_name, buf);
	}
	(void) printf("change\t%s", pwp->pw_change ? ctime(&pwp->pw_change) : "NEVER\n");
#ifdef EXTENSIONS
	(void) printf("class\t%s\n", pwp->pw_class);
#endif
	(void) printf("gecos\t%s\n", pwp->pw_gecos);
	(void) printf("dir\t%s\n", pwp->pw_dir);
	(void) printf("shell\t%s\n", pwp->pw_shell);
	(void) printf("expire\t%s", pwp->pw_expire ? ctime(&pwp->pw_expire) : "NEVER\n");
	return EXIT_SUCCESS;
}
#endif

#ifdef EXTENSIONS
/* display user information */
int
groupinfo(int argc, char **argv)
{
	struct group	*grp;
	char		**cpp;
	int		exists;
	int		i;

	exists = 0;
	while ((i = getopt(argc, argv, "ev")) != -1) {
		switch(i) {
		case 'e':
			exists = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usermgmt_usage("groupinfo");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupinfo");
	}
	grp = find_group_info(*argv);
	if (exists) {
		exit((grp) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (grp == NULL) {
		errx(EXIT_FAILURE, "can't find group `%s'", *argv);
	}
	(void) printf("name\t%s\n", grp->gr_name);
	(void) printf("passwd\t%s\n", grp->gr_passwd);
	(void) printf("gid\t%d\n", grp->gr_gid);
	(void) printf("members\t");
	for (cpp = grp->gr_mem ; *cpp ; cpp++) {
		(void) printf("%s ", *cpp);
	}
	(void) fputc('\n', stdout);
	return EXIT_SUCCESS;
}
#endif
