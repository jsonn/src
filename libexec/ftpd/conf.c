/*	$NetBSD: conf.c,v 1.31.2.2 2001/03/29 14:14:17 lukem Exp $	*/

/*-
 * Copyright (c) 1997-2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge and Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: conf.c,v 1.31.2.2 2001/03/29 14:14:17 lukem Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#ifdef KERBEROS5
#include <krb5/krb5.h>
#endif

#include "extern.h"
#include "pathnames.h"

static char *strend(const char *, char *);
static int filetypematch(char *, int);


		/* class defaults */
#define DEFAULT_LIMIT		-1		/* unlimited connections */
#define DEFAULT_MAXFILESIZE	-1		/* unlimited file size */
#define DEFAULT_MAXTIMEOUT	7200		/* 2 hours */
#define DEFAULT_TIMEOUT		900		/* 15 minutes */
#define DEFAULT_UMASK		027		/* 15 minutes */

/*
 * Initialise curclass to an `empty' state
 */
void
init_curclass(void)
{
	struct ftpconv	*conv, *cnext;

	for (conv = curclass.conversions; conv != NULL; conv = cnext) {
		REASSIGN(conv->suffix, NULL);
		REASSIGN(conv->types, NULL);
		REASSIGN(conv->disable, NULL);
		REASSIGN(conv->command, NULL);
		cnext = conv->next;
		free(conv);
	}

	memset((char *)&curclass.advertise, 0, sizeof(curclass.advertise));
	curclass.advertise.su_len = 0;		/* `not used' */
	REASSIGN(curclass.chroot, NULL);
	REASSIGN(curclass.classname, NULL);
	curclass.conversions =	NULL;
	REASSIGN(curclass.display, NULL);
	REASSIGN(curclass.homedir, NULL);
	curclass.limit =	DEFAULT_LIMIT;	
	REASSIGN(curclass.limitfile, NULL);
	curclass.maxfilesize =	DEFAULT_MAXFILESIZE;
	curclass.maxrateget =	0;
	curclass.maxrateput =	0;
	curclass.maxtimeout =	DEFAULT_MAXTIMEOUT;
	REASSIGN(curclass.motd, xstrdup(_PATH_FTPLOGINMESG));
	REASSIGN(curclass.notify, NULL);
	curclass.portmin =	0;
	curclass.portmax =	0;
	curclass.rateget =	0;
	curclass.rateput =	0;
	curclass.timeout =	DEFAULT_TIMEOUT;
	    /* curclass.type is set elsewhere */
	curclass.umask =	DEFAULT_UMASK;

	CURCLASS_FLAGS_SET(checkportcmd);
	CURCLASS_FLAGS_SET(modify);
	CURCLASS_FLAGS_SET(passive);
	CURCLASS_FLAGS_CLR(sanenames);
	CURCLASS_FLAGS_SET(upload);
}

/*
 * Parse the configuration file, looking for the named class, and
 * define curclass to contain the appropriate settings.
 */
void
parse_conf(const char *findclass)
{
	FILE		*f;
	char		*buf, *p;
	size_t		 len;
	LLT		 llval;
	int		 none, match;
	char		*endp;
	char		*class, *word, *arg, *template;
	const char	*infile;
	size_t		 line;
	unsigned int	 timeout;
	struct ftpconv	*conv, *cnext;

	init_curclass();
	REASSIGN(curclass.classname, xstrdup(findclass));
			/* set more guest defaults */
	if (strcasecmp(findclass, "guest") == 0) {
		CURCLASS_FLAGS_CLR(modify);
		curclass.umask = 0707;
	}

	infile = conffilename(_PATH_FTPDCONF);
	if ((f = fopen(infile, "r")) == NULL)
		return;

	line = 0;
	template = NULL;
	for (;
	    (buf = fparseln(f, &len, &line, NULL, FPARSELN_UNESCCOMM |
	    		FPARSELN_UNESCCONT | FPARSELN_UNESCESC)) != NULL;
	    free(buf)) {
		none = match = 0;
		p = buf;
		if (len < 1)
			continue;
		if (p[len - 1] == '\n')
			p[--len] = '\0';
		if (EMPTYSTR(p))
			continue;
		
		NEXTWORD(p, word);
		NEXTWORD(p, class);
		NEXTWORD(p, arg);
		if (EMPTYSTR(word) || EMPTYSTR(class))
			continue;
		if (strcasecmp(class, "none") == 0)
			none = 1;
		if (! (strcasecmp(class, findclass) == 0 ||
		       (template != NULL && strcasecmp(class, template) == 0) ||
		       none ||
		       strcasecmp(class, "all") == 0) )
			continue;

#define CONF_FLAG(x) \
	do { \
		if (none || \
		    (!EMPTYSTR(arg) && strcasecmp(arg, "off") == 0)) \
			CURCLASS_FLAGS_CLR(x); \
		else \
			CURCLASS_FLAGS_SET(x); \
	} while (0)

#define CONF_STRING(x) \
	do { \
		if (none || EMPTYSTR(arg)) \
			arg = NULL; \
		else \
			arg = xstrdup(arg); \
		REASSIGN(curclass.x, arg); \
	} while (0)


		if (0)  {
			/* no-op */

		} else if (strcasecmp(word, "advertise") == 0) {
			struct addrinfo	hints, *res;
			int		error;

			memset((char *)&curclass.advertise, 0,
			    sizeof(curclass.advertise));
			curclass.advertise.su_len = 0;
			if (none || EMPTYSTR(arg))
				continue;
			res = NULL;
			memset(&hints, 0, sizeof(hints));
					/*
					 * only get addresses of the family
					 * that we're listening on
					 */
			hints.ai_family = ctrl_addr.su_family;
			hints.ai_socktype = SOCK_STREAM;
			error = getaddrinfo(arg, "0", &hints, &res);
			if (error) {
				syslog(LOG_WARNING, "%s line %d: %s",
				    infile, (int)line, gai_strerror(error));
 advertiseparsefail:
				if (res)
					freeaddrinfo(res);
				continue;
			}
			if (res->ai_next) {
				syslog(LOG_WARNING,
    "%s line %d: multiple addresses returned for `%s'; please be more specific",
				    infile, (int)line, arg);
				goto advertiseparsefail;
			}
			if (sizeof(curclass.advertise) < res->ai_addrlen || (
#ifdef INET6
			    res->ai_family != AF_INET6 &&
#endif
			    res->ai_family != AF_INET)) {
				syslog(LOG_WARNING,
    "%s line %d: unsupported protocol %d for `%s'",
				    infile, (int)line, res->ai_family, arg);
				goto advertiseparsefail;
			}
			memcpy(&curclass.advertise, res->ai_addr,
			    res->ai_addrlen);
			curclass.advertise.su_len = res->ai_addrlen;
			freeaddrinfo(res);

		} else if (strcasecmp(word, "checkportcmd") == 0) {
			CONF_FLAG(checkportcmd);

		} else if (strcasecmp(word, "chroot") == 0) {
			CONF_STRING(chroot);

		} else if (strcasecmp(word, "classtype") == 0) {
			if (!none && !EMPTYSTR(arg)) {
				if (strcasecmp(arg, "GUEST") == 0)
					curclass.type = CLASS_GUEST;
				else if (strcasecmp(arg, "CHROOT") == 0)
					curclass.type = CLASS_CHROOT;
				else if (strcasecmp(arg, "REAL") == 0)
					curclass.type = CLASS_REAL;
				else {
					syslog(LOG_WARNING,
				    "%s line %d: unknown class type `%s'",
					    infile, (int)line, arg);
					continue;
				}
			}

		} else if (strcasecmp(word, "conversion") == 0) {
			char *suffix, *types, *disable, *convcmd;

			if (EMPTYSTR(arg)) {
				syslog(LOG_WARNING,
				    "%s line %d: %s requires a suffix",
				    infile, (int)line, word);
				continue;	/* need a suffix */
			}
			NEXTWORD(p, types);
			NEXTWORD(p, disable);
			convcmd = p;
			if (convcmd)
				convcmd += strspn(convcmd, " \t");
			suffix = xstrdup(arg);
			if (none || EMPTYSTR(types) ||
			    EMPTYSTR(disable) || EMPTYSTR(convcmd)) {
				types = NULL;
				disable = NULL;
				convcmd = NULL;
			} else {
				types = xstrdup(types);
				disable = xstrdup(disable);
				convcmd = xstrdup(convcmd);
			}
			for (conv = curclass.conversions; conv != NULL;
			    conv = conv->next) {
				if (strcmp(conv->suffix, suffix) == 0)
					break;
			}
			if (conv == NULL) {
				conv = (struct ftpconv *)
				    calloc(1, sizeof(struct ftpconv));
				if (conv == NULL) {
					syslog(LOG_WARNING, "can't malloc");
					continue;
				}
				conv->next = NULL;
				for (cnext = curclass.conversions;
				    cnext != NULL; cnext = cnext->next)
					if (cnext->next == NULL)
						break;
				if (cnext != NULL)
					cnext->next = conv;
				else
					curclass.conversions = conv;
			}
			REASSIGN(conv->suffix, suffix);
			REASSIGN(conv->types, types);
			REASSIGN(conv->disable, disable);
			REASSIGN(conv->command, convcmd);

		} else if (strcasecmp(word, "display") == 0) {
			CONF_STRING(display);

		} else if (strcasecmp(word, "homedir") == 0) {
			CONF_STRING(homedir);

		} else if (strcasecmp(word, "limit") == 0) {
			int limit;

			curclass.limit = DEFAULT_LIMIT;
			REASSIGN(curclass.limitfile, NULL);
			if (none || EMPTYSTR(arg))
				continue;
			limit = (int)strtol(arg, &endp, 10);
			if (*endp != 0) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid limit %s",
				    infile, (int)line, arg);
				continue;
			}
			curclass.limit = limit;
			REASSIGN(curclass.limitfile,
			    EMPTYSTR(p) ? NULL : xstrdup(p));

		} else if (strcasecmp(word, "maxfilesize") == 0) {
			curclass.maxfilesize = DEFAULT_MAXFILESIZE;
			if (none || EMPTYSTR(arg))
				continue;
			llval = strsuftoll(arg);
			if (llval == -1) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid maxfilesize %s",
				    infile, (int)line, arg);
				continue;
			}
			curclass.maxfilesize = llval;

		} else if (strcasecmp(word, "maxtimeout") == 0) {
			curclass.maxtimeout = DEFAULT_MAXTIMEOUT;
			if (none || EMPTYSTR(arg))
				continue;
			timeout = (unsigned int)strtoul(arg, &endp, 10);
			if (*endp != 0) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid maxtimeout %s",
				    infile, (int)line, arg);
				continue;
			}
			if (timeout < 30) {
				syslog(LOG_WARNING,
				    "%s line %d: maxtimeout %d < 30 seconds",
				    infile, (int)line, timeout);
				continue;
			}
			if (timeout < curclass.timeout) {
				syslog(LOG_WARNING,
				    "%s line %d: maxtimeout %d < timeout (%d)",
				    infile, (int)line, timeout,
				    curclass.timeout);
				continue;
			}
			curclass.maxtimeout = timeout;

		} else if (strcasecmp(word, "modify") == 0) {
			CONF_FLAG(modify);

		} else if (strcasecmp(word, "motd") == 0) {
			CONF_STRING(motd);

		} else if (strcasecmp(word, "notify") == 0) {
			CONF_STRING(notify);

		} else if (strcasecmp(word, "passive") == 0) {
			CONF_FLAG(passive);

		} else if (strcasecmp(word, "portrange") == 0) {
			int minport, maxport;
			char *min, *max;

			curclass.portmin = 0;
			curclass.portmax = 0;
			if (none || EMPTYSTR(arg))
				continue;
			min = arg;
			NEXTWORD(p, max);
			if (EMPTYSTR(max)) {
				syslog(LOG_WARNING,
				   "%s line %d: missing maxport argument",
				   infile, (int)line);
				continue;
			}
			minport = (int)strtol(min, &endp, 10);
			if (*endp != 0 || minport < IPPORT_RESERVED ||
			    minport > IPPORT_ANONMAX) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid minport %s",
				    infile, (int)line, min);
				continue;
			}
			maxport = (int)strtol(max, &endp, 10);
			if (*endp != 0 || maxport < IPPORT_RESERVED ||
			    maxport > IPPORT_ANONMAX) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid maxport %s",
				    infile, (int)line, max);
				continue;
			}
			if (minport >= maxport) {
				syslog(LOG_WARNING,
				    "%s line %d: minport %d >= maxport %d",
				    infile, (int)line, minport, maxport);
				continue;
			}
			curclass.portmin = minport;
			curclass.portmax = maxport;

		} else if (strcasecmp(word, "rateget") == 0) {
			curclass.maxrateget = 0;
			curclass.rateget = 0;
			if (none || EMPTYSTR(arg))
				continue;
			llval = strsuftoll(arg);
			if (llval == -1) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid rateget %s",
				    infile, (int)line, arg);
				continue;
			}
			curclass.maxrateget = llval;
			curclass.rateget = llval;

		} else if (strcasecmp(word, "rateput") == 0) {
			curclass.maxrateput = 0;
			curclass.rateput = 0;
			if (none || EMPTYSTR(arg))
				continue;
			llval = strsuftoll(arg);
			if (llval == -1) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid rateput %s",
				    infile, (int)line, arg);
				continue;
			}
			curclass.maxrateput = llval;
			curclass.rateput = llval;

		} else if (strcasecmp(word, "sanenames") == 0) {
			CONF_FLAG(sanenames);

		} else if (strcasecmp(word, "timeout") == 0) {
			curclass.timeout = DEFAULT_TIMEOUT;
			if (none || EMPTYSTR(arg))
				continue;
			timeout = (unsigned int)strtoul(arg, &endp, 10);
			if (*endp != 0) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid timeout %s",
				    infile, (int)line, arg);
				continue;
			}
			if (timeout < 30) {
				syslog(LOG_WARNING,
				    "%s line %d: timeout %d < 30 seconds",
				    infile, (int)line, timeout);
				continue;
			}
			if (timeout > curclass.maxtimeout) {
				syslog(LOG_WARNING,
				    "%s line %d: timeout %d > maxtimeout (%d)",
				    infile, (int)line, timeout,
				    curclass.maxtimeout);
				continue;
			}
			curclass.timeout = timeout;

		} else if (strcasecmp(word, "template") == 0) {
			if (none)
				continue;
			REASSIGN(template, EMPTYSTR(arg) ? NULL : xstrdup(arg));

		} else if (strcasecmp(word, "umask") == 0) {
			mode_t umask;

			curclass.umask = DEFAULT_UMASK;
			if (none || EMPTYSTR(arg))
				continue;
			umask = (mode_t)strtoul(arg, &endp, 8);
			if (*endp != 0 || umask > 0777) {
				syslog(LOG_WARNING,
				    "%s line %d: invalid umask %s",
				    infile, (int)line, arg);
				continue;
			}
			curclass.umask = umask;

		} else if (strcasecmp(word, "upload") == 0) {
			CONF_FLAG(upload);
			if (! CURCLASS_FLAGS_ISSET(upload))
				CURCLASS_FLAGS_CLR(modify);

		} else {
			syslog(LOG_WARNING,
			    "%s line %d: unknown directive '%s'",
			    infile, (int)line, word);
			continue;
		}
	}
	REASSIGN(template, NULL);
	fclose(f);
}

/*
 * Show file listed in curclass.display first time in, and list all the
 * files named in curclass.notify in the current directory.
 * Send back responses with the prefix `code' + "-".
 * If code == -1, flush the internal cache of directory names and return.
 */
void
show_chdir_messages(int code)
{
	static StringList *slist = NULL;

	struct stat st;
	struct tm *t;
	glob_t	 gl;
	time_t	 now, then;
	int	 age;
	char	 cwd[MAXPATHLEN];
	char	*cp, **rlist;

	if (code == -1) {
		if (slist != NULL)
			sl_free(slist, 1);
		slist = NULL;
		return;
	}
		
	if (quietmessages)
		return;

		/* Setup list for directory cache */
	if (slist == NULL)
		slist = sl_init();
	if (slist == NULL) {
		syslog(LOG_WARNING, "can't allocate memory for stringlist");
		return;
	}

		/* Check if this directory has already been visited */
	if (getcwd(cwd, sizeof(cwd) - 1) == NULL) {
		syslog(LOG_WARNING, "can't getcwd: %s", strerror(errno));
		return;
	}
	if (sl_find(slist, cwd) != NULL)
		return;	

	cp = xstrdup(cwd);
	if (sl_add(slist, cp) == -1)
		syslog(LOG_WARNING, "can't add `%s' to stringlist", cp);

		/* First check for a display file */
	(void)display_file(curclass.display, code);

		/* Now see if there are any notify files */
	if (EMPTYSTR(curclass.notify))
		return;

	gl.gl_offs = 0;
	if (glob(curclass.notify, GLOB_LIMIT, NULL, &gl) != 0
	    || gl.gl_matchc == 0) {
		globfree(&gl);
		return;
	}
	time(&now);
	for (rlist = gl.gl_pathv; *rlist != NULL; rlist++) {
		if (stat(*rlist, &st) != 0)
			continue;
		if (!S_ISREG(st.st_mode))
			continue;
		then = st.st_mtime;
		if (code != 0) {
			reply(-code, "%s", "");
			code = 0;
		}
		reply(-code, "Please read the file %s", *rlist);
		t = localtime(&now);
		age = 365 * t->tm_year + t->tm_yday;
		t = localtime(&then);
		age -= 365 * t->tm_year + t->tm_yday;
		reply(-code, "  it was last modified on %.24s - %d day%s ago",
		    ctime(&then), age, PLURAL(age));
	}
	globfree(&gl);
}

int
display_file(const char *file, int code)
{
	FILE   *f;
	char   *buf, *p, *cwd;
	size_t	len;
	off_t	lastnum;
	time_t	now;

	lastnum = 0;
	if (quietmessages)
		return (0);

	if (EMPTYSTR(file))
		return(0);
	if ((f = fopen(file, "r")) == NULL)
		return (0);
	reply(-code, "%s", "");

	for (;
	    (buf = fparseln(f, &len, NULL, "\0\0\0", 0)) != NULL; free(buf)) {
		if (len > 0)
			if (buf[len - 1] == '\n')
				buf[--len] = '\0';
		cprintf(stdout, "    ");

		for (p = buf; *p; p++) {
			if (*p == '%') {
				p++;
				switch (*p) {

				case 'c':
					cprintf(stdout, "%s",
					    curclass.classname ?
					    curclass.classname : "<unknown>");
					break;

				case 'C':
					if (getcwd(cwd, sizeof(cwd)-1) == NULL){
						syslog(LOG_WARNING,
						    "can't getcwd: %s",
						    strerror(errno));
						continue;
					}
					cprintf(stdout, "%s", cwd);
					break;

				case 'E':
					if (! EMPTYSTR(emailaddr))
						cprintf(stdout, "%s",
						    emailaddr);
					break;

				case 'L':
					cprintf(stdout, "%s", hostname);
					break;

				case 'M':
					if (curclass.limit == -1) {
						cprintf(stdout, "unlimited");
						lastnum = 0;
					} else {
						cprintf(stdout, "%d",
						    curclass.limit);
						lastnum = curclass.limit;
					}
					break;

				case 'N':
					cprintf(stdout, "%d", connections);
					lastnum = connections;
					break;

				case 'R':
					cprintf(stdout, "%s", remotehost);
					break;

				case 's':
					if (lastnum != 1)
						cprintf(stdout, "s");
					break;

				case 'S':
					if (lastnum != 1)
						cprintf(stdout, "S");
					break;

				case 'T':
					now = time(NULL);
					cprintf(stdout, "%.24s", ctime(&now));
					break;

				case 'U':
					cprintf(stdout, "%s",
					    pw ? pw->pw_name : "<unknown>");
					break;

				case '%':
					CPUTC('%', stdout);
					break;

				}
			} else
				CPUTC(*p, stdout);
		}
		cprintf(stdout, "\r\n");
	}

	(void)fflush(stdout);
	(void)fclose(f);
	return (1);
}

/*
 * Parse src, expanding '%' escapes, into dst (which must be at least
 * MAXPATHLEN long).
 */
void
format_path(char *dst, const char *src)
{
	size_t len;
	const char *p;

	dst[0] = '\0';
	len = 0;
	if (src == NULL)
		return;
	for (p = src; *p && len < MAXPATHLEN; p++) {
		if (*p == '%') {
			p++;
			switch (*p) {

			case 'c':
				len += strlcpy(dst + len, curclass.classname,
				    MAXPATHLEN - len);
				break;

			case 'd':
				len += strlcpy(dst + len, pw->pw_dir,
				    MAXPATHLEN - len);
				break;

			case 'u':
				len += strlcpy(dst + len, pw->pw_name,
				    MAXPATHLEN - len);
				break;

			case '%':
				dst[len++] = '%';
				break;

			}
		} else
			dst[len++] = *p;
	}
	if (len < MAXPATHLEN)
		dst[len] = '\0';
	dst[MAXPATHLEN - 1] = '\0';
}

/*
 * Find s2 at the end of s1.  If found, return a string up to (but
 * not including) s2, otherwise returns NULL.
 */
static char *
strend(const char *s1, char *s2)
{
	static	char buf[MAXPATHLEN];

	char	*start;
	size_t	l1, l2;

	l1 = strlen(s1);
	l2 = strlen(s2);

	if (l2 >= l1)
		return(NULL);
	
	strlcpy(buf, s1, sizeof(buf));
	start = buf + (l1 - l2);

	if (strcmp(start, s2) == 0) {
		*start = '\0';
		return(buf);
	} else
		return(NULL);
}

static int
filetypematch(char *types, int mode)
{
	for ( ; types[0] != '\0'; types++)
		switch (*types) {
		  case 'd':
			if (S_ISDIR(mode))
				return(1);
			break;
		  case 'f':
			if (S_ISREG(mode))
				return(1);
			break;
		}
	return(0);
}

/*
 * Look for a conversion.  If we succeed, return a pointer to the
 * command to execute for the conversion.
 *
 * The command is stored in a static array so there's no memory
 * leak problems, and not too much to change in ftpd.c.  This
 * routine doesn't need to be re-entrant unless we start using a
 * multi-threaded ftpd, and that's not likely for a while...
 */
char **
do_conversion(const char *fname)
{
	struct ftpconv	*cp;
	struct stat	 st;
	int		 o_errno;
	char		*base = NULL;
	char		*cmd, *p, *lp, **argv;
	StringList	*sl;

	o_errno = errno;
	sl = NULL;
	cmd = NULL;
	for (cp = curclass.conversions; cp != NULL; cp = cp->next) {
		if (cp->suffix == NULL) {
			syslog(LOG_WARNING,
			    "cp->suffix==NULL in conv list; SHOULDN'T HAPPEN!");
			continue;
		}
		if ((base = strend(fname, cp->suffix)) == NULL)
			continue;
		if (cp->types == NULL || cp->disable == NULL ||
		    cp->command == NULL)
			continue;
					/* Is it enabled? */
		if (strcmp(cp->disable, ".") != 0 &&
		    stat(cp->disable, &st) == 0)
				continue;
					/* Does the base exist? */
		if (stat(base, &st) < 0)
			continue;
					/* Is the file type ok */
		if (!filetypematch(cp->types, st.st_mode))
			continue;
		break;			/* "We have a winner!" */
	}

	/* If we got through the list, no conversion */
	if (cp == NULL)
		goto cleanup_do_conv;

	/* Split up command into an argv */
	if ((sl = sl_init()) == NULL)
		goto cleanup_do_conv;
	cmd = xstrdup(cp->command);
	p = cmd;
	while (p) {
		NEXTWORD(p, lp);
		if (strcmp(lp, "%s") == 0)
			lp = base;
		if (sl_add(sl, xstrdup(lp)) == -1)
			goto cleanup_do_conv;
	}

	if (sl_add(sl, NULL) == -1)
		goto cleanup_do_conv;
	argv = sl->sl_str;
	free(cmd);
	free(sl);
	return(argv);

 cleanup_do_conv:
	if (sl)
		sl_free(sl, 1);
	free(cmd);
	errno = o_errno;
	return(NULL);
}

/*
 * Convert the string `arg' to a long long, which may have an optional SI suffix
 * (`b', `k', `m', `g', `t'). Returns the number for success, -1 otherwise.
 */
LLT
strsuftoll(const char *arg)
{
	char *cp;
	LLT val;

	if (!isdigit((unsigned char)arg[0]))
		return (-1);

	val = STRTOLL(arg, &cp, 10);
	if (cp != NULL) {
		if (cp[0] != '\0' && cp[1] != '\0')
			 return (-1);
		switch (tolower((unsigned char)cp[0])) {
		case '\0':
		case 'b':
			break;
		case 'k':
			val <<= 10;
			break;
		case 'm':
			val <<= 20;
			break;
		case 'g':
			val <<= 30;
			break;
#ifndef NO_LONG_LONG
		case 't':
			val <<= 40;
			break;
#endif
		default:
			return (-1);
		}
	}
	if (val < 0)
		return (-1);

	return (val);
}

/*
 * Count the number of current connections, reading from
 *	/var/run/ftpd.pids-<class>
 * Does a kill -0 on each pid in that file, and only counts
 * processes that exist (or frees the slot if it doesn't).
 * Adds getpid() to the first free slot. Truncates the file
 * if possible.
 */ 
void
count_users(void)
{
	char	fn[MAXPATHLEN];
	int	fd, i, last;
	size_t	count;
	pid_t  *pids, mypid;
	struct stat sb;

	(void)strlcpy(fn, _PATH_CLASSPIDS, sizeof(fn));
	(void)strlcat(fn, curclass.classname, sizeof(fn));
	pids = NULL;
	connections = 1;

	if ((fd = open(fn, O_RDWR | O_CREAT, 0600)) == -1)
		return;
	if (lockf(fd, F_TLOCK, 0) == -1)
		goto cleanup_count;
	if (fstat(fd, &sb) == -1)
		goto cleanup_count;
	if ((pids = malloc(sb.st_size + sizeof(pid_t))) == NULL)
		goto cleanup_count;
	count = read(fd, pids, sb.st_size);
	if (count < 0 || count != sb.st_size)
		goto cleanup_count;
	count /= sizeof(pid_t);
	mypid = getpid();
	last = 0;
	for (i = 0; i < count; i++) {
		if (pids[i] == 0)
			continue;
		if (kill(pids[i], 0) == -1 && errno != EPERM) {
			if (mypid != 0) {
				pids[i] = mypid;
				mypid = 0;
				last = i;
			}
		} else {
			connections++;
			last = i;
		}
	}
	if (mypid != 0) {
		if (pids[last] != 0)
			last++;
		pids[last] = mypid;
	}
	count = (last + 1) * sizeof(pid_t);
	if (lseek(fd, 0, SEEK_SET) == -1)
		goto cleanup_count;
	if (write(fd, pids, count) == -1)
		goto cleanup_count;
	(void)ftruncate(fd, count);

 cleanup_count:
	if (lseek(fd, 0, SEEK_SET) != -1)
		(void)lockf(fd, F_ULOCK, 0);
	close(fd);
	REASSIGN(pids, NULL);
}
