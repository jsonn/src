/*	misc.c,v 1.13 2006/05/21 19:26:43 christos Exp	*/

/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

#include <sys/cdefs.h>
#if !defined(lint) && !defined(LINT)
#if 0
static char rcsid[] = "Id: misc.c,v 2.9 1994/01/15 20:43:43 vixie Exp";
#else
__RCSID("misc.c,v 1.13 2006/05/21 19:26:43 christos Exp");
#endif
#endif

/* vix 26jan87 [RCS has the rest of the log]
 * vix 30dec86 [written]
 */


#include "cron.h"
#if SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#if defined(SYSLOG)
# include <syslog.h>
#endif
#include <ctype.h>
#include <vis.h>

#if defined(LOG_DAEMON) && !defined(LOG_CRON)
#define LOG_CRON LOG_DAEMON
#endif

static int in_file(char *, FILE *);
static void mkprint(char *, unsigned char *, int);

static int		LogFD = ERR;


int
strcmp_until(const char *left, const char *right, int until)
{
	int	diff;

	while (*left && *left != until && *left == *right) {
		left++;
		right++;
	}

	if ((*left=='\0' || *left == until) &&
	    (*right=='\0' || *right == until)) {
		diff = 0;
	} else {
		diff = *left - *right;
	}

	return diff;
}


/* strdtb(s) - delete trailing blanks in string 's' and return new length
 */
int
strdtb(char *s)
{
	char	*x = s;

	/* scan forward to the null
	 */
	while (*x)
		x++;

	/* scan backward to either the first character before the string,
	 * or the last non-blank in the string, whichever comes first.
	 */
	do	{x--;}
	while (x >= s && isspace((unsigned char)*x));

	/* one character beyond where we stopped above is where the null
	 * goes.
	 */
	*++x = '\0';

	/* the difference between the position of the null character and
	 * the position of the first character of the string is the length.
	 */
	return x - s;
}


int
set_debug_flags(char *flags)
{
	/* debug flags are of the form    flag[,flag ...]
	 *
	 * if an error occurs, print a message to stdout and return FALSE.
	 * otherwise return TRUE after setting ERROR_FLAGS.
	 */

#if !DEBUGGING

	printf("this program was compiled without debugging enabled\n");
	return FALSE;

#else /* DEBUGGING */

	char	*pc = flags;

	DebugFlags = 0;

	while (*pc) {
		const char	* const *test;
		int	mask;

		/* try to find debug flag name in our list.
		 */
		for (	test = DebugFlagNames, mask = 1;
			*test && strcmp_until(*test, pc, ',');
			test++, mask <<= 1
		    )
			;

		if (!*test) {
			fprintf(stderr,
				"unrecognized debug flag <%s> <%s>\n",
				flags, pc);
			return FALSE;
		}

		DebugFlags |= mask;

		/* skip to the next flag
		 */
		while (*pc && *pc != ',')
			pc++;
		if (*pc == ',')
			pc++;
	}

	if (DebugFlags) {
		int	flag;

		fprintf(stderr, "debug flags enabled:");

		for (flag = 0;  DebugFlagNames[flag];  flag++)
			if (DebugFlags & (1 << flag))
				fprintf(stderr, " %s", DebugFlagNames[flag]);
		fprintf(stderr, "\n");
	}

	return TRUE;

#endif /* DEBUGGING */
}


void
set_cron_uid(void)
{
#if defined(BSD) || defined(POSIX)
	if (seteuid(ROOT_UID) < OK) {
		warn("cannot seteuid");
		exit(ERROR_EXIT);
	}
#else
	if (setuid(ROOT_UID) < OK) {
		warn("cannot setuid");
		exit(ERROR_EXIT);
	}
#endif
}


void
set_cron_cwd(void)
{
	struct stat	sb;

	/* first check for CRONDIR ("/var/cron" or some such)
	 */
	if (stat(CRONDIR, &sb) < OK && errno == ENOENT) {
		warn("cannot stat %s", CRONDIR);
		if (OK == mkdir(CRONDIR, 0700)) {
			fprintf(stderr, "%s: created\n", CRONDIR);
			stat(CRONDIR, &sb);
		} else {
			warn("cannot create %s", CRONDIR);
			exit(ERROR_EXIT);
		}
	}
	if (!S_ISDIR(sb.st_mode)) {
		warnx("`%s' is not a directory, bailing out", CRONDIR);
		exit(ERROR_EXIT);
	}
	if (chdir(CRONDIR) < OK) {
		warn("cannot chdir(%s), bailing out", CRONDIR);
		exit(ERROR_EXIT);
	}

	/* CRONDIR okay (now==CWD), now look at SPOOL_DIR ("tabs" or some such)
	 */
	if (stat(SPOOL_DIR, &sb) < OK && errno == ENOENT) {
		warn("cannot stat %s", SPOOL_DIR);
		if (OK == mkdir(SPOOL_DIR, 0700)) {
			fprintf(stderr, "%s: created\n", SPOOL_DIR);
			stat(SPOOL_DIR, &sb);
		} else {
			warn("cannot create %s", SPOOL_DIR);
			exit(ERROR_EXIT);
		}
	}
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			SPOOL_DIR);
		exit(ERROR_EXIT);
	}
}


/* acquire_daemonlock() - write our PID into /etc/cron.pid, unless
 *	another daemon is already running, which we detect here.
 *
 * note: main() calls us twice; once before forking, once after.
 *	we maintain static storage of the file pointer so that we
 *	can rewrite our PID into the PIDFILE after the fork.
 *
 * it would be great if fflush() disassociated the file buffer.
 */
void
acquire_daemonlock(int closeflag)
{
	static	FILE	*fp = NULL;

	if (closeflag && fp) {
		fclose(fp);
		fp = NULL;
		return;
	}

	if (!fp) {
		char	pidfile[MAX_FNAME];
		char	buf[MAX_TEMPSTR];
		int	fd, otherpid;

		(void) snprintf(pidfile, sizeof(pidfile), PIDFILE, PIDDIR);
		if ((-1 == (fd = open(pidfile, O_RDWR|O_CREAT, 0644)))
		    || (NULL == (fp = fdopen(fd, "r+")))
		    ) {
			snprintf(buf, sizeof(buf),
				"can't open or create %s: %s",
				pidfile, strerror(errno));
			warnx(buf);
			log_it("CRON", getpid(), "DEATH", buf);
			exit(ERROR_EXIT);
		}

		if (flock(fd, LOCK_EX|LOCK_NB) < OK) {
			int save_errno = errno;

			fscanf(fp, "%d", &otherpid);
			snprintf(buf, sizeof(buf),
				"can't lock %s, otherpid may be %d: %s",
				pidfile, otherpid, strerror(save_errno));
			warnx(buf);
			log_it("CRON", getpid(), "DEATH", buf);
			exit(ERROR_EXIT);
		}

		(void) fcntl(fd, F_SETFD, 1);
	}

	rewind(fp);
	fprintf(fp, "%d\n", getpid());
	fflush(fp);
	(void) ftruncate(fileno(fp), ftell(fp));

	/* abandon fd and fp even though the file is open. we need to
	 * keep it open and locked, but we don't need the handles elsewhere.
	 */
}

/* get_char(file) : like getc() but increment LineNumber on newlines
 */
int
get_char(FILE *file)
{
	int	ch;

	ch = getc(file);
	if (ch == '\n')
		Set_LineNum(LineNumber + 1)
	return ch;
}


/* unget_char(ch, file) : like ungetc but do LineNumber processing
 */
void
unget_char(int ch, FILE *file)
{
	ungetc(ch, file);
	if (ch == '\n')
		Set_LineNum(LineNumber - 1)
}


/* get_string(str, max, file, termstr) : like fgets() but
 *		(1) has terminator string which should include \n
 *		(2) will always leave room for the null
 *		(3) uses get_char() so LineNumber will be accurate
 *		(4) returns EOF or terminating character, whichever
 */
int
get_string(char *string, int size, FILE *file, const char *terms)
{
	int	ch;

	while (EOF != (ch = get_char(file)) && !strchr(terms, ch)) {
		if (size > 1) {
			*string++ = (char) ch;
			size--;
		}
	}

	if (size > 0)
		*string = '\0';

	return ch;
}


/* skip_comments(file) : read past comment (if any)
 */
void
skip_comments(FILE *file)
{
	int	ch;

	while (EOF != (ch = get_char(file))) {
		/* ch is now the first character of a line.
		 */

		while (ch == ' ' || ch == '\t')
			ch = get_char(file);

		if (ch == EOF)
			break;

		/* ch is now the first non-blank character of a line.
		 */

		if (ch != '\n' && ch != '#')
			break;

		/* ch must be a newline or comment as first non-blank
		 * character on a line.
		 */

		while (ch != '\n' && ch != EOF)
			ch = get_char(file);

		/* ch is now the newline of a line which we're going to
		 * ignore.
		 */
	}
	if (ch != EOF)
		unget_char(ch, file);
}


/* int in_file(char *string, FILE *file)
 *	return TRUE if one of the lines in file matches string exactly,
 *	FALSE otherwise.
 */
static int
in_file(char *string, FILE *file)
{
	char line[MAX_TEMPSTR];

	rewind(file);
	while (fgets(line, MAX_TEMPSTR, file)) {
		if (line[0] != '\0')
			line[strlen(line)-1] = '\0';
		if (0 == strcmp(line, string))
			return TRUE;
	}
	return FALSE;
}


/* int allowed(char *username)
 *	returns TRUE if (ALLOW_FILE exists and user is listed)
 *	or (DENY_FILE exists and user is NOT listed)
 *	or (neither file exists but user=="root" so it's okay)
 */
int
allowed(char *username)
{
	static int	init = FALSE;
	static FILE	*allow, *deny;

	if (!init) {
		init = TRUE;
#if defined(ALLOW_FILE) && defined(DENY_FILE)
		allow = fopen(ALLOW_FILE, "r");
		deny = fopen(DENY_FILE, "r");
		Debug(DMISC, ("allow/deny enabled, %d/%d\n", !!allow, !!deny))
		if (allow)
			(void)fcntl(fileno(allow), F_SETFD, FD_CLOEXEC);
		if (deny)
			(void)fcntl(fileno(deny), F_SETFD, FD_CLOEXEC);
#else
		allow = NULL;
		deny = NULL;
#endif
	}

	if (allow)
		return (in_file(username, allow));
	if (deny)
		return (!in_file(username, deny));

#if defined(ALLOW_ONLY_ROOT)
	return (strcmp(username, ROOT_USER) == 0);
#else
	return TRUE;
#endif
}


void
log_it(const char *username, int xpid, const char *event, const char *detail)
{
	PID_T			pid = xpid;
#if defined(LOG_FILE)
	char			*msg;
	size_t			msglen;
	TIME_T			now = time((TIME_T) 0);
	struct tm	*t = localtime(&now);
#endif /*LOG_FILE*/

#if defined(SYSLOG)
	static int		syslog_open = 0;
#endif

#if defined(LOG_FILE)
	/* we assume that MAX_TEMPSTR will hold the date, time, &punctuation.
	 */
	msglen = strlen(username) + strlen(event) + strlen(detail) +
	    MAX_TEMPSTR;
	msg = malloc(msglen);

	if (LogFD < OK) {
		LogFD = open(LOG_FILE, O_WRONLY|O_APPEND|O_CREAT, 0600);
		if (LogFD < OK) {
			warn("can't open log file %s", LOG_FILE);
		} else {
			(void) fcntl(LogFD, F_SETFD, 1);
		}
	}

	/* we have to snprintf() it because fprintf() doesn't always write
	 * everything out in one chunk and this has to be atomically appended
	 * to the log file.
	 */
	snprintf(msg, msglen, "%s (%02d/%02d-%02d:%02d:%02d-%d) %s (%s)\n",
		username,
		t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, pid,
		event, detail);

	/* we have to run strlen() because sprintf() returns (char*) on old BSD
	 */
	if (LogFD < OK || write(LogFD, msg, strlen(msg)) < OK) {
		if (LogFD >= OK)
			warn("can't write to log file");
		write(STDERR, msg, strlen(msg));
	}

	free(msg);
#endif /*LOG_FILE*/

#if defined(SYSLOG)
	if (!syslog_open) {
		/* we don't use LOG_PID since the pid passed to us by
		 * our client may not be our own.  therefore we want to
		 * print the pid ourselves.
		 */
# ifdef LOG_DAEMON
		openlog(getprogname(), LOG_PID, LOG_CRON);
# else
		openlog(getprogname(), LOG_PID);
# endif
		syslog_open = TRUE;		/* assume openlog success */
	}

	syslog(LOG_INFO, "(%s) %s (%s)\n", username, event, detail);

#endif /*SYSLOG*/

#if DEBUGGING
	if (DebugFlags) {
		fprintf(stderr, "log_it: (%s %d) %s (%s)\n",
			username, pid, event, detail);
	}
#endif
}


void
log_close(void) {
	if (LogFD != ERR) {
		close(LogFD);
		LogFD = ERR;
	}
}


/* two warnings:
 *	(1) this routine is fairly slow
 *	(2) it returns a pointer to static storage
 */
char *
first_word(char *s, /* string we want the first word of */
           const char *t  /* terminators, implicitly including \0 */)
{
	static char retbuf[2][MAX_TEMPSTR + 1];	/* sure wish C had GC */
	static int retsel = 0;
	char *rb, *rp;

	/* select a return buffer */
	retsel = 1-retsel;
	rb = &retbuf[retsel][0];
	rp = rb;

	/* skip any leading terminators */
	while (*s && (NULL != strchr(t, *s))) {
		s++;
	}

	/* copy until next terminator or full buffer */
	while (*s && (NULL == strchr(t, *s)) && (rp < &rb[MAX_TEMPSTR])) {
		*rp++ = *s++;
	}

	/* finish the return-string and return it */
	*rp = '\0';
	return rb;
}


/* warning:
 *	heavily ascii-dependent.
 */
static void
mkprint(char *dst, unsigned char *src, int len)
{
	while(len > 0 && isblank((unsigned char) *src))
		len--, src++;

	strvisx(dst, src, len, VIS_TAB|VIS_NL);
}


/* warning:
 *	returns a pointer to malloc'd storage, you must call free yourself.
 */
char *
mkprints(unsigned char *src, unsigned int len)
{
	char *dst = malloc(len*4 + 1);

	mkprint(dst, src, len);

	return dst;
}


#ifdef MAIL_DATE
/* Sat, 27 Feb 1993 11:44:51 -0800 (CST)
 * 1234567890123456789012345678901234567
 */
char *
arpadate(time_t *clock)
{
	static char ret[64];	/* zone name might be >3 chars */
	time_t t = clock ? *clock : time(NULL);
	struct tm *tm = localtime(&t);
	int hours = tm->tm_gmtoff / 3600;
	int minutes = (tp->tm_gmtoff - (hours * 3600)) / 60;

	if (minutes < 0)
		minutes = -minutes;
	
	(void)strftime(ret, sizeof(ret), "%a, %e %b %Y %T ????? (%Z)", &tm);
	(void)snprintf(strchr(ret, '?'), "% .2d%.2d", hours, minutes);
	ret[sizeof(ret) - 1] = '\0';
	return ret;
}
#endif /*MAIL_DATE*/


#ifdef HAVE_SAVED_UIDS
static int save_euid;
int swap_uids(void) { save_euid = geteuid(); return seteuid(getuid()); }
#if 0
int swap_uids_back() { return seteuid(save_euid); }
#endif
#else /*HAVE_SAVED_UIDS*/
int swap_uids(void) { return setreuid(geteuid(), getuid()); }
#if 0
int swap_uids_back() { return swap_uids(); }
#endif
#endif /*HAVE_SAVED_UIDS*/
