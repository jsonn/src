/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1983 Eric P. Allman.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1983, 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n\
     Copyright (c) 1983 Eric P. Allman.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)Id: vacation.c,v 8.68.4.4 2000/07/18 05:10:29 gshapiro Exp";
#endif /* ! lint */

#include <ctype.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#ifdef EX_OK
# undef EX_OK		/* unistd.h may have another use for this */
#endif /* EX_OK */
#include <sysexits.h>

#include "sendmail/sendmail.h"
#include "libsmdb/smdb.h"

#if defined(__hpux) && !defined(HPUX11)
# undef syslog		/* Undo hard_syslog conf.h change */
#endif /* defined(__hpux) && !defined(HPUX11) */

#ifndef _PATH_SENDMAIL
# define _PATH_SENDMAIL "/usr/lib/sendmail"
#endif /* ! _PATH_SENDMAIL */

#define ONLY_ONCE	((time_t) 0)	/* send at most one reply */
#define INTERVAL_UNDEF	((time_t) (-1))	/* no value given */

uid_t	RealUid;
gid_t	RealGid;
char	*RealUserName;
uid_t	RunAsUid;
uid_t	RunAsGid;
char	*RunAsUserName;
int	Verbose = 2;
bool	DontInitGroups = FALSE;
uid_t	TrustedUid = 0;
BITMAP256 DontBlameSendmail;

/*
**  VACATION -- return a message to the sender when on vacation.
**
**	This program is invoked as a message receiver.  It returns a
**	message specified by the user to whomever sent the mail, taking
**	care not to return a message too often to prevent "I am on
**	vacation" loops.
*/

#define	VDB	".vacation"		/* vacation database */
#define	VMSG	".vacation.msg"		/* vacation message */
#define SECSPERDAY	(60 * 60 * 24)
#define DAYSPERWEEK	7

#ifndef TRUE
# define TRUE	1
# define FALSE	0
#endif /* ! TRUE */

#ifndef __P
# ifdef __STDC__
#  define __P(protos)	protos
# else /* __STDC__ */
#  define __P(protos)	()
#  define const
# endif /* __STDC__ */
#endif /* ! __P */

typedef struct alias
{
	char *name;
	struct alias *next;
} ALIAS;

ALIAS *Names = NULL;

SMDB_DATABASE *Db;

char From[MAXLINE];

#if _FFR_DEBUG
void (*msglog)(int, const char *, ...) = &syslog;
static void debuglog __P((int, const char *, ...));
#else /* _FFR_DEBUG */
# define msglog		syslog
#endif /* _FFR_DEBUG */

int
main(argc, argv)
	int argc;
	char **argv;
{
	bool iflag, emptysender, exclude;
#if _FFR_LISTDB
	bool lflag = FALSE;
#endif /* _FFR_LISTDB */
	int mfail = 0, ufail = 0;
	int ch;
	int result;
	time_t interval;
	struct passwd *pw;
	ALIAS *cur;
	char *dbfilename = VDB;
	char *msgfilename = VMSG;
	char *name;
	SMDB_USER_INFO user_info;
	static char rnamebuf[MAXNAME];
	extern int optind, opterr;
	extern char *optarg;
	extern void usage __P((void));
	extern void setinterval __P((time_t));
	extern void readheaders __P((void));
	extern bool recent __P((void));
	extern void setreply __P((char *, time_t));
	extern void sendmessage __P((char *, char *, bool));
	extern void xclude __P((FILE *));

	/* Vars needed to link with smutil */
	clrbitmap(DontBlameSendmail);
	RunAsUid = RealUid = getuid();
	RunAsGid = RealGid = getgid();
	pw = getpwuid(RealUid);
	if (pw != NULL)
	{
		if (strlen(pw->pw_name) > MAXNAME - 1)
			pw->pw_name[MAXNAME] = '\0';
		snprintf(rnamebuf, sizeof rnamebuf, "%s", pw->pw_name);
	}
	else
		snprintf(rnamebuf, sizeof rnamebuf,
			 "Unknown UID %d", (int) RealUid);
	RunAsUserName = RealUserName = rnamebuf;

#ifdef LOG_MAIL
	openlog("vacation", LOG_PID, LOG_MAIL);
#else /* LOG_MAIL */
	openlog("vacation", LOG_PID);
#endif /* LOG_MAIL */

	opterr = 0;
	iflag = FALSE;
	emptysender = FALSE;
	exclude = FALSE;
	interval = INTERVAL_UNDEF;
	*From = '\0';

#if _FFR_DEBUG && _FFR_LISTDB
# define OPTIONS		"a:df:Iilm:r:s:t:xz"
#else /* _FFR_DEBUG && _FFR_LISTDB */
# if _FFR_DEBUG
#  define OPTIONS		"a:df:Iim:r:s:t:xz"
# else /* _FFR_DEBUG */
#  if _FFR_LISTDB
#   define OPTIONS		"a:f:Iilm:r:s:t:xz"
#  else /* _FFR_LISTDB */
#   define OPTIONS		"a:f:Iim:r:s:t:xz"
#  endif /* _FFR_LISTDB */
# endif /* _FFR_DEBUG */
#endif /* _FFR_DEBUG && _FFR_LISTDB */

	while (mfail == 0 && ufail == 0 &&
	       (ch = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch((char)ch)
		{
		  case 'a':			/* alias */
			cur = (ALIAS *)malloc((u_int)sizeof(ALIAS));
			if (cur == NULL)
			{
				mfail++;
				break;
			}
			cur->name = optarg;
			cur->next = Names;
			Names = cur;
			break;

#if _FFR_DEBUG
		case 'd':			/* debug mode */
			msglog = &debuglog;
			break;
#endif /* _FFR_DEBUG */


		  case 'f':		/* alternate database */
			dbfilename = optarg;
			break;

		  case 'I':			/* backward compatible */
		  case 'i':			/* init the database */
			iflag = TRUE;
			break;

#if _FFR_LISTDB
		  case 'l':
			lflag = TRUE;		/* list the database */
			break;
#endif /* _FFR_LISTDB */

		  case 'm':		/* alternate message file */
			msgfilename = optarg;
			break;

		  case 'r':
			if (isascii(*optarg) && isdigit(*optarg))
			{
				interval = atol(optarg) * SECSPERDAY;
				if (interval < 0)
					ufail++;
			}
			else
				interval = ONLY_ONCE;
			break;

		  case 's':		/* alternate sender name */
			(void) strlcpy(From, optarg, sizeof From);
			break;

		  case 't':		/* SunOS: -t1d (default expire) */
			break;

		  case 'x':
			exclude = TRUE;
			break;

		  case 'z':
			emptysender = TRUE;
			break;

		  case '?':
		  default:
			ufail++;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (mfail != 0)
	{
		msglog(LOG_NOTICE,
		       "vacation: can't allocate memory for alias.\n");
		exit(EX_TEMPFAIL);
	}
	if (ufail != 0)
		usage();

	if (argc != 1)
	{
		if (!iflag &&
#if _FFR_LISTDB
		    !lflag &&
#endif /* _FFR_LISTDB */
		    !exclude)
			usage();
		if ((pw = getpwuid(getuid())) == NULL)
		{
			msglog(LOG_ERR,
			       "vacation: no such user uid %u.\n", getuid());
			exit(EX_NOUSER);
		}
	}
#if _FFR_BLACKBOX
	name = *argv;
#else /* _FFR_BLACKBOX */
	else if ((pw = getpwnam(*argv)) == NULL)
	{
		msglog(LOG_ERR, "vacation: no such user %s.\n", *argv);
		exit(EX_NOUSER);
	}
	name = pw->pw_name;
	if (chdir(pw->pw_dir) != 0)
	{
		msglog(LOG_NOTICE,
		       "vacation: no such directory %s.\n", pw->pw_dir);
		exit(EX_NOINPUT);
	}
#endif /* _FFR_BLACKBOX */
	user_info.smdbu_id = pw->pw_uid;
	user_info.smdbu_group_id = pw->pw_gid;
	(void) strlcpy(user_info.smdbu_name, pw->pw_name,
		       SMDB_MAX_USER_NAME_LEN);

	result = smdb_open_database(&Db, dbfilename,
				    O_CREAT|O_RDWR | (iflag ? O_TRUNC : 0),
				    S_IRUSR|S_IWUSR, SFF_CREAT,
				    SMDB_TYPE_DEFAULT, &user_info, NULL);
	if (result != SMDBE_OK)
	{
		msglog(LOG_NOTICE, "vacation: %s: %s\n", dbfilename,
		       errstring(result));
		exit(EX_DATAERR);
	}

#if _FFR_LISTDB
	if (lflag)
	{
		static void listdb __P((void));

		listdb();
		(void)Db->smdb_close(Db);
		exit(EX_OK);
	}
#endif /* _FFR_LISTDB */

	if (interval != INTERVAL_UNDEF)
		setinterval(interval);

	if (iflag)
	{
		result = Db->smdb_close(Db);
		if (!exclude)
			exit(EX_OK);
	}

	if (exclude)
	{
		xclude(stdin);
		result = Db->smdb_close(Db);
		exit(EX_OK);
	}

	if ((cur = (ALIAS *)malloc((u_int)sizeof(ALIAS))) == NULL)
	{
		msglog(LOG_NOTICE,
		       "vacation: can't allocate memory for username.\n");
		exit(EX_OSERR);
	}
	cur->name = name;
	cur->next = Names;
	Names = cur;

	readheaders();
	if (!recent())
	{
		time_t now;

		(void) time(&now);
		setreply(From, now);
		result = Db->smdb_close(Db);
		sendmessage(name, msgfilename, emptysender);
	}
	else
		result = Db->smdb_close(Db);
	exit(EX_OK);
	/* NOTREACHED */
	return EX_OK;
}

/*
** READHEADERS -- read mail headers
**
**	Parameters:
**		none.
**
**	Returns:
**		nothing.
**
*/
void
readheaders()
{
	bool tome, cont;
	register char *p;
	register ALIAS *cur;
	char buf[MAXLINE];
	extern bool junkmail __P((char *));
	extern bool nsearch __P((char *, char *));

	cont = tome = FALSE;
	while (fgets(buf, sizeof(buf), stdin) && *buf != '\n')
	{
		switch(*buf)
		{
		  case 'F':		/* "From " */
			cont = FALSE;
			if (strncmp(buf, "From ", 5) == 0)
			{
				bool quoted = FALSE;

				p = buf + 5;
				while (*p != '\0')
				{
					/* escaped character */
					if (*p == '\\')
					{
						p++;
						if (*p == '\0')
						{
							msglog(LOG_NOTICE,
							       "vacation: badly formatted \"From \" line.\n");
							exit(EX_DATAERR);
						}
					}
					else if (*p == '"')
						quoted = !quoted;
					else if (*p == '\r' || *p == '\n')
						break;
					else if (*p == ' ' && !quoted)
						break;
					p++;
				}
				if (quoted)
				{
					msglog(LOG_NOTICE,
					       "vacation: badly formatted \"From \" line.\n");
					exit(EX_DATAERR);
				}
				*p = '\0';

				/* ok since both strings have MAXLINE length */
				if (*From == '\0')
					(void)strlcpy(From, buf + 5,
						      sizeof From);
				if ((p = strchr(buf + 5, '\n')) != NULL)
					*p = '\0';
				if (junkmail(buf + 5))
					exit(EX_OK);
			}
			break;

		  case 'P':		/* "Precedence:" */
		  case 'p':
			cont = FALSE;
			if (strlen(buf) <= 10 ||
			    strncasecmp(buf, "Precedence", 10) != 0 ||
			    (buf[10] != ':' && buf[10] != ' ' &&
			     buf[10] != '\t'))
				break;
			if ((p = strchr(buf, ':')) == NULL)
				break;
			while (*++p != '\0' && isascii(*p) && isspace(*p));
			if (*p == '\0')
				break;
			if (strncasecmp(p, "junk", 4) == 0 ||
			    strncasecmp(p, "bulk", 4) == 0 ||
			    strncasecmp(p, "list", 4) == 0)
				exit(EX_OK);
			break;

		  case 'C':		/* "Cc:" */
		  case 'c':
			if (strncasecmp(buf, "Cc:", 3) != 0)
				break;
			cont = TRUE;
			goto findme;

		  case 'T':		/* "To:" */
		  case 't':
			if (strncasecmp(buf, "To:", 3) != 0)
				break;
			cont = TRUE;
			goto findme;

		  default:
			if (!isascii(*buf) || !isspace(*buf) || !cont || tome)
			{
				cont = FALSE;
				break;
			}
findme:
			for (cur = Names;
			     !tome && cur != NULL;
			     cur = cur->next)
				tome = nsearch(cur->name, buf);
		}
	}
	if (!tome)
		exit(EX_OK);
	if (*From == '\0')
	{
		msglog(LOG_NOTICE, "vacation: no initial \"From \" line.\n");
		exit(EX_DATAERR);
	}
}

/*
** NSEARCH --
**	do a nice, slow, search of a string for a substring.
**
**	Parameters:
**		name -- name to search.
**		str -- string in which to search.
**
**	Returns:
**		is name a substring of str?
**
*/
bool
nsearch(name, str)
	register char *name, *str;
{
	register size_t len;
	register char *s;

	len = strlen(name);

	for (s = str; *s != '\0'; ++s)
	{
		/*
		**  Check to make sure that the string matches and
		**  the previous character is not an alphanumeric and
		**  the next character after the match is not an alphanumeric.
		**
		**  This prevents matching "eric" to "derick" while still
		**  matching "eric" to "<eric+detail>".
		*/

		if (tolower(*s) == tolower(*name) &&
		    strncasecmp(name, s, len) == 0 &&
		    (s == str || !isascii(*(s - 1)) || !isalnum(*(s - 1))) &&
		    (!isascii(*(s + len)) || !isalnum(*(s + len))))
			return TRUE;
	}
	return FALSE;
}

/*
** JUNKMAIL --
**	read the header and return if automagic/junk/bulk/list mail
**
**	Parameters:
**		from -- sender address.
**
**	Returns:
**		is this some automated/junk/bulk/list mail?
**
*/
bool
junkmail(from)
	char *from;
{
	register size_t len;
	register char *p;
	register struct ignore *cur;
	static struct ignore
	{
		char	*name;
		size_t	len;
	} ignore[] =
	{
		{ "-request",		8	},
		{ "postmaster",		10	},
		{ "uucp",		4	},
		{ "mailer-daemon",	13	},
		{ "mailer",		6	},
		{ "-relay",		6	},
		{ NULL,			0	}
	};

	/*
	 * This is mildly amusing, and I'm not positive it's right; trying
	 * to find the "real" name of the sender, assuming that addresses
	 * will be some variant of:
	 *
	 * From site!site!SENDER%site.domain%site.domain@site.domain
	 */
	if ((p = strchr(from, '%')) == NULL &&
	    (p = strchr(from, '@')) == NULL)
	{
		if ((p = strrchr(from, '!')) != NULL)
			++p;
		else
			p = from;
		for (; *p; ++p)
			continue;
	}
	len = p - from;
	for (cur = ignore; cur->name != NULL; ++cur)
	{
		if (len >= cur->len &&
		    strncasecmp(cur->name, p - cur->len, cur->len) == 0)
			return TRUE;
	}
	return FALSE;
}

#define	VIT	"__VACATION__INTERVAL__TIMER__"

/*
** RECENT --
**	find out if user has gotten a vacation message recently.
**
**	Parameters:
**		none.
**
**	Returns:
**		TRUE iff user has gotten a vacation message recently.
**
*/
bool
recent()
{
	SMDB_DBENT key, data;
	time_t then, next;
	bool trydomain = FALSE;
	int st;
	char *domain;

	memset(&key, '\0', sizeof key);
	memset(&data, '\0', sizeof data);

	/* get interval time */
	key.data.data = VIT;
	key.data.size = sizeof(VIT);

	st = Db->smdb_get(Db, &key, &data, 0);
	if (st != SMDBE_OK)
		next = SECSPERDAY * DAYSPERWEEK;
	else
		memmove(&next, data.data.data, sizeof(next));

	memset(&data, '\0', sizeof data);

	/* get record for this address */
	key.data.data = From;
	key.data.size = strlen(From);

	do
	{
		st = Db->smdb_get(Db, &key, &data, 0);
		if (st == SMDBE_OK)
		{
			memmove(&then, data.data.data, sizeof(then));
			if (next == ONLY_ONCE || then == ONLY_ONCE ||
			    then + next > time(NULL))
				return TRUE;
		}
		if ((trydomain = !trydomain) &&
		    (domain = strchr(From, '@')) != NULL)
		{
			key.data.data = domain;
			key.data.size = strlen(domain);
		}
	} while (trydomain);
	return FALSE;
}

/*
** SETINTERVAL --
**	store the reply interval
**
**	Parameters:
**		interval -- time interval for replies.
**
**	Returns:
**		nothing.
**
**	Side Effects:
**		stores the reply interval in database.
*/
void
setinterval(interval)
	time_t interval;
{
	SMDB_DBENT key, data;

	memset(&key, '\0', sizeof key);
	memset(&data, '\0', sizeof data);

	key.data.data = VIT;
	key.data.size = sizeof(VIT);
	data.data.data = (char*) &interval;
	data.data.size = sizeof(interval);
	(void)(Db->smdb_put)(Db, &key, &data, 0);
}

/*
** SETREPLY --
**	store that this user knows about the vacation.
**
**	Parameters:
**		from -- sender address.
**		when -- last reply time.
**
**	Returns:
**		nothing.
**
**	Side Effects:
**		stores user/time in database.
*/
void
setreply(from, when)
	char *from;
	time_t when;
{
	SMDB_DBENT key, data;

	memset(&key, '\0', sizeof key);
	memset(&data, '\0', sizeof data);

	key.data.data = from;
	key.data.size = strlen(from);
	data.data.data = (char*) &when;
	data.data.size = sizeof(when);
	(void)(Db->smdb_put)(Db, &key, &data, 0);
}

/*
** XCLUDE --
**	add users to vacation db so they don't get a reply.
**
**	Parameters:
**		f -- file pointer with list of address to exclude
**
**	Returns:
**		nothing.
**
**	Side Effects:
**		stores users in database.
*/
void
xclude(f)
	FILE *f;
{
	char buf[MAXLINE], *p;

	if (f == NULL)
		return;
	while (fgets(buf, sizeof buf, f))
	{
		if ((p = strchr(buf, '\n')) != NULL)
			*p = '\0';
		setreply(buf, ONLY_ONCE);
	}
}

/*
** SENDMESSAGE --
**	exec sendmail to send the vacation file to sender
**
**	Parameters:
**		myname -- user name.
**		msgfn -- name of file with vacation message.
**		emptysender -- use <> as sender address?
**
**	Returns:
**		nothing.
**
**	Side Effects:
**		sends vacation reply.
*/
void
sendmessage(myname, msgfn, emptysender)
	char *myname;
	char *msgfn;
	bool emptysender;
{
	FILE *mfp, *sfp;
	int i;
	int pvect[2];
	char buf[MAXLINE];

	mfp = fopen(msgfn, "r");
	if (mfp == NULL)
	{
		if (msgfn[0] == '/')
			msglog(LOG_NOTICE, "vacation: no %s file.\n", msgfn);
		else
			msglog(LOG_NOTICE, "vacation: no ~%s/%s file.\n",
			       myname, msgfn);
		exit(EX_NOINPUT);
	}
	if (pipe(pvect) < 0)
	{
		msglog(LOG_ERR, "vacation: pipe: %s", errstring(errno));
		exit(EX_OSERR);
	}
	i = fork();
	if (i < 0)
	{
		msglog(LOG_ERR, "vacation: fork: %s", errstring(errno));
		exit(EX_OSERR);
	}
	if (i == 0)
	{
		(void) dup2(pvect[0], 0);
		(void) close(pvect[0]);
		(void) close(pvect[1]);
		(void) fclose(mfp);
		if (emptysender)
			myname = "<>";
		(void) execl(_PATH_SENDMAIL, "sendmail", "-f", myname, "--",
		      From, NULL);
		msglog(LOG_ERR, "vacation: can't exec %s: %s",
			_PATH_SENDMAIL, errstring(errno));
		exit(EX_UNAVAILABLE);
	}
	/* check return status of the following calls? XXX */
	(void) close(pvect[0]);
	if ((sfp = fdopen(pvect[1], "w")) != NULL)
	{
		(void) fprintf(sfp, "To: %s\n", From);
		(void) fprintf(sfp, "Auto-Submitted: auto-generated\n");
		while (fgets(buf, sizeof buf, mfp))
			(void) fputs(buf, sfp);
		(void) fclose(mfp);
		(void) fclose(sfp);
	}
	else
	{
		(void) fclose(mfp);
		msglog(LOG_ERR, "vacation: can't open pipe to sendmail");
		exit(EX_UNAVAILABLE);
	}
}

void
usage()
{
	msglog(LOG_NOTICE, "uid %u: usage: vacation [-i] [-a alias]%s [-f db]%s [-m msg] [-r interval] [-s sender] [-t time] [-x] [-z] login\n",
	       getuid(),
#if _FFR_DEBUG
	       " [-d]",
#else /* _FFR_DEBUG */
	       "",
#endif /* _FFR_DEBUG */
#if _FFR_LISTDB
	       " [-l]"
#else /* _FFR_LISTDB */
	       ""
#endif /* _FFR_LISTDB */
	       );
	exit(EX_USAGE);
}

#if _FFR_LISTDB
/*
** LISTDB -- list the contents of the vacation database
**
**	Parameters:
**		none.
**
**	Returns:
**		nothing.
*/

static void
listdb()
{
	int result;
	time_t t;
	SMDB_CURSOR *cursor = NULL;
	SMDB_DBENT db_key, db_value;

	memset(&db_key, '\0', sizeof db_key);
	memset(&db_value, '\0', sizeof db_value);

	result = Db->smdb_cursor(Db, &cursor, 0);
	if (result != SMDBE_OK)
	{
		fprintf(stderr, "vacation: set cursor: %s\n",
			errstring(result));
		return;
	}

	while ((result = cursor->smdbc_get(cursor, &db_key, &db_value,
					   SMDB_CURSOR_GET_NEXT)) == SMDBE_OK)
	{
		/* skip magic VIT entry */
		if ((int)db_key.data.size -1 == strlen(VIT) &&
		    strncmp((char *)db_key.data.data, VIT,
			    (int)db_key.data.size - 1) == 0)
			continue;

		/* skip bogus values */
		if (db_value.data.size != sizeof t)
		{
			fprintf(stderr, "vacation: %.*s invalid time stamp\n",
				(int) db_key.data.size,
				(char *) db_key.data.data);
			continue;
		}

		memcpy(&t, db_value.data.data, sizeof t);

		if (db_key.data.size > 40)
			db_key.data.size = 40;

		printf("%-40.*s %-10s",
		       (int) db_key.data.size, (char *) db_key.data.data,
		       ctime(&t));

		memset(&db_key, '\0', sizeof db_key);
		memset(&db_value, '\0', sizeof db_value);
	}

	if (result != SMDBE_OK && result != SMDBE_LAST_ENTRY)
	{
		fprintf(stderr,	"vacation: get value at cursor: %s\n",
			errstring(result));
		if (cursor != NULL)
		{
			(void) cursor->smdbc_close(cursor);
			cursor = NULL;
		}
		return;
	}
	(void) cursor->smdbc_close(cursor);
	cursor = NULL;
}
#endif /* _FFR_LISTDB */

#if _FFR_DEBUG
/*
** DEBUGLOG -- write message to standard error
**
**	Append a message to the standard error for the convenience of
**	end-users debugging without access to the syslog messages.
**
**	Parameters:
**		i -- syslog log level
**		fmt -- string format
**
**	Returns:
**		nothing.
*/

/*VARARGS2*/
static void
#ifdef __STDC__
debuglog(int i, const char *fmt, ...)
#else /* __STDC__ */
debuglog(i, fmt, va_alist)
	int i;
	const char *fmt;
	va_dcl
#endif /* __STDC__ */

{
	VA_LOCAL_DECL

	VA_START(fmt);
	vfprintf(stderr, fmt, ap);
	VA_END;
}
#endif /* _FFR_DEBUG */

/*VARARGS1*/
void
#ifdef __STDC__
message(const char *msg, ...)
#else /* __STDC__ */
message(msg, va_alist)
	const char *msg;
	va_dcl
#endif /* __STDC__ */
{
	const char *m;
	VA_LOCAL_DECL

	m = msg;
	if (isascii(m[0]) && isdigit(m[0]) &&
	    isascii(m[1]) && isdigit(m[1]) &&
	    isascii(m[2]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	(void) vfprintf(stderr, m, ap);
	VA_END;
	(void) fprintf(stderr, "\n");
}

/*VARARGS1*/
void
#ifdef __STDC__
syserr(const char *msg, ...)
#else /* __STDC__ */
syserr(msg, va_alist)
	const char *msg;
	va_dcl
#endif /* __STDC__ */
{
	const char *m;
	VA_LOCAL_DECL

	m = msg;
	if (isascii(m[0]) && isdigit(m[0]) &&
	    isascii(m[1]) && isdigit(m[1]) &&
	    isascii(m[2]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	(void) vfprintf(stderr, m, ap);
	VA_END;
	(void) fprintf(stderr, "\n");
}

void
dumpfd(fd, printclosed, logit)
	int fd;
	bool printclosed;
	bool logit;
{
	return;
}
