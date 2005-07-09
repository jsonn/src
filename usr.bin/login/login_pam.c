/*     $NetBSD: login_pam.c,v 1.4.2.4 2005/07/09 22:42:12 tron Exp $       */

/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)login.c	8.4 (Berkeley) 4/2/94";
#endif
__RCSID("$NetBSD: login_pam.c,v 1.4.2.4 2005/07/09 22:42:12 tron Exp $");
#endif /* not lint */

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>
#include <login_cap.h>
#include <vis.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

#include "pathnames.h"

void	 badlogin (char *);
static void	 update_db (int);
void	 getloginname (void);
int	 main (int, char *[]);
void	 motd (char *);
int	 rootterm (char *);
void	 sigint (int);
void	 sleepexit (int);
const	 char *stypeof (const char *);
void	 timedout (int);
void	 decode_ss (const char *);
void	 usage (void);

static struct pam_conv pamc = { openpam_ttyconv, NULL };

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

#define DEFAULT_BACKOFF 3
#define DEFAULT_RETRIES 10

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
u_int	timeout = 300;

struct	passwd *pwd, pwres;
char	pwbuf[1024];
struct	group *gr;
int	failures, have_ss;
char	term[64], *envinit[1], *hostname, *username, *tty, *nested;
struct timeval now;
struct sockaddr_storage ss;

extern const char copyrightstr[];

int
main(int argc, char *argv[])
{
	extern char **environ;
	struct stat st;
	int ask, ch, cnt, fflag, hflag, pflag, sflag, quietlog, rootlogin;
	int auth_passed;
	int Fflag;
	uid_t uid, saved_uid;
	gid_t saved_gid, saved_gids[NGROUPS_MAX];
	int nsaved_gids;
	char *domain, *p, *ttyn, *pwprompt;
	char tbuf[MAXPATHLEN + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[MAXHOSTNAMELEN + 1];
	int need_chpass, require_chpass;
	int login_retries = DEFAULT_RETRIES, 
	    login_backoff = DEFAULT_BACKOFF;
	char *shell = NULL;
	login_cap_t *lc = NULL;
	pam_handle_t *pamh = NULL;
	int pam_err;
	void *oint;
	void *oabrt;
	const void *newuser;
	int pam_silent = PAM_SILENT;
	pid_t xpid, pid;
	int status;
	char *saved_term;
	char **pamenv;

	tbuf[0] = '\0';
	pwprompt = NULL;
	nested = NULL;
	need_chpass = require_chpass = 0;

	(void)signal(SIGALRM, timedout);
	(void)alarm(timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", 0, LOG_AUTH);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote host to
	 *    login so that it may be placed in utmp/utmpx and wtmp/wtmpx
	 * -a in addition to -h, a server my supply -a to pass the actual
	 *    server address.
	 * -s is used to force use of S/Key or equivalent.
	 */
	domain = NULL;
	if (gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");
	else
		domain = strchr(localhost, '.');
	localhost[sizeof(localhost) - 1] = '\0';

	Fflag = fflag = hflag = pflag = sflag = 0;
	have_ss = 0;
	uid = getuid();
	while ((ch = getopt(argc, argv, "a:Ffh:ps")) != -1)
		switch (ch) {
		case 'a':
			if (uid) {
				errno = EPERM;
				err(EXIT_FAILURE, "-a option");
			}
			decode_ss(optarg);
#ifdef notdef
			(void)sockaddr_snprintf(optarg,
			    sizeof(struct sockaddr_storage), "%a", (void *)&ss);
#endif
			break;
		case 'F':
			Fflag = 1;
			/* FALLTHROUGH */
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid) {
				errno = EPERM;
				err(EXIT_FAILURE, "-h option");
			}
			hflag = 1;
			if (domain && (p = strchr(optarg, '.')) != NULL &&
			    strcasecmp(p, domain) == 0)
				*p = '\0';
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		default:
		case '?':
			usage();
			break;
		}

	setproctitle(NULL);
	argc -= optind;
	argv += optind;

	if (*argv) {
		username = *argv;
		ask = 0;
	} else
		ask = 1;

#ifdef F_CLOSEM
	(void)fcntl(3, F_CLOSEM, 0);
#else
	for (cnt = getdtablesize(); cnt > 2; cnt--)
		(void)close(cnt);
#endif

	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if ((tty = strstr(ttyn, "/pts/")) != NULL)
		++tty;
	else if ((tty = strrchr(ttyn, '/')) != NULL)
		++tty;
	else
		tty = ttyn;

	if (issetugid()) {
		nested = strdup(user_from_uid(getuid(), 0));
		if (nested == NULL) {
                	syslog(LOG_ERR, "strdup: %m");
                	sleepexit(EXIT_FAILURE);
		}
	}

	/* Get "login-retries" and "login-backoff" from default class */
	if ((lc = login_getclass(NULL)) != NULL) {
		login_retries = (int)login_getcapnum(lc, "login-retries",
		    DEFAULT_RETRIES, DEFAULT_RETRIES);
		login_backoff = (int)login_getcapnum(lc, "login-backoff", 
		    DEFAULT_BACKOFF, DEFAULT_BACKOFF);
		login_close(lc);
		lc = NULL;
	}


	for (cnt = 0;; ask = 1) {
		if (ask) {
			fflag = 0;
			getloginname();
		}
		rootlogin = 0;
		auth_passed = 0;
		if (strlen(username) > MAXLOGNAME)
			username[MAXLOGNAME] = '\0';

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(tbuf, username)) {
			if (failures > (pwd ? 0 : 1))
				badlogin(tbuf);
			failures = 0;
		}

#define PAM_END(msg) do { 						\
	syslog(LOG_ERR, "%s: %s", msg, pam_strerror(pamh, pam_err)); 	\
	warnx("%s: %s", msg, pam_strerror(pamh, pam_err));		\
	pam_end(pamh, pam_err);						\
	sleepexit(EXIT_FAILURE);					\
} while (/*CONSTCOND*/0)

		pam_err = pam_start("login", username, &pamc, &pamh);
		if (pam_err != PAM_SUCCESS) {
			if (pamh != NULL)
				PAM_END("pam_start");
			/* Things went really bad... */
			syslog(LOG_ERR, "pam_start failed: %s",
			    pam_strerror(pamh, pam_err));
			errx(EXIT_FAILURE, "pam_start failed");
		}

#define PAM_SET_ITEM(item, var)	do {					\
	pam_err = pam_set_item(pamh, (item), (var));			\
	if (pam_err != PAM_SUCCESS)					\
		PAM_END("pam_set_item(" # item ")");			\
} while (/*CONSTCOND*/0)

		/* 
		 * Fill hostname tty, and nested user
		 */
		PAM_SET_ITEM(PAM_RHOST, hostname);
		PAM_SET_ITEM(PAM_TTY, tty); 
		if (nested)
			PAM_SET_ITEM(PAM_NUSER, nested); 
		if (have_ss)
			PAM_SET_ITEM(PAM_SOCKADDR, &ss); 

		/*
		 * Don't check for errors, because we don't want to give
		 * out any information.
		 */
		pwd = NULL;
		(void)getpwnam_r(username, &pwres, pwbuf, sizeof(pwbuf), &pwd);

		/*
		 * Establish the class now, before we might goto
		 * within the next block. pwd can be NULL since it
		 * falls back to the "default" class if it is.
		 */
		lc = login_getclass(pwd ? pwd->pw_class : NULL);

		/*
		 * if we have a valid account name, and it doesn't have a
		 * password, or the -f option was specified and the caller
		 * is root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		if (pwd) {
			if (pwd->pw_uid == 0)
				rootlogin = 1;

			if (fflag && (uid == 0 || uid == pwd->pw_uid)) {
				/* already authenticated */
				auth_passed = 1;
				goto skip_auth;
			}
		}

		(void)setpriority(PRIO_PROCESS, 0, -4);

		switch(pam_err = pam_authenticate(pamh, pam_silent)) {
		case PAM_SUCCESS:
			/*
			 * PAM can change the user, refresh
			 * username, pwd, and lc.
			 */
			pam_err = pam_get_item(pamh, PAM_USER, &newuser);
			if (pam_err != PAM_SUCCESS)
				PAM_END("pam_get_item(PAM_USER)");

			username = (char *)newuser;
			/*
			 * Don't check for errors, because we don't want to give
			 * out any information.
			 */
			pwd = NULL;
			(void)getpwnam_r(username, &pwres, pwbuf, sizeof(pwbuf),
			    &pwd);
			lc = login_getpwclass(pwd);
			auth_passed = 1;

			switch (pam_err = pam_acct_mgmt(pamh, pam_silent)) {
			case PAM_SUCCESS:
				break;

			case PAM_NEW_AUTHTOK_REQD:
				pam_err = pam_chauthtok(pamh, 
				    pam_silent|PAM_CHANGE_EXPIRED_AUTHTOK);

				if (pam_err != PAM_SUCCESS)
					PAM_END("pam_chauthtok");
				break;

			default:
				PAM_END("pam_acct_mgmt");
				break;
			}
			break;
					
		case PAM_AUTH_ERR:
		case PAM_USER_UNKNOWN:
		case PAM_MAXTRIES:
			auth_passed = 0;
			break;

		default:
			PAM_END("pam_authenticate");
			break;
		}

		(void)setpriority(PRIO_PROCESS, 0, 0);

skip_auth:
		/*
		 * If the user exists and authentication passed, 
		 * get out of the loop and login the user.
		 */
		if (pwd && auth_passed)
			break;

		(void)printf("Login incorrect\n");
		failures++;
		cnt++;
		/* we allow 10 tries, but after 3 we start backing off */
		if (cnt > login_backoff) {
			if (cnt >= login_retries) {
				badlogin(username);
				pam_end(pamh, PAM_SUCCESS);
				sleepexit(EXIT_FAILURE);
			}
			sleep((u_int)((cnt - 3) * 5));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	endpwent();

        quietlog = login_getcapbool(lc, "hushlogin", 0);

	/* 
	 * Temporarily give up special privileges so we can change
	 * into NFS-mounted homes that are exported for non-root
	 * access and have mode 7x0 
	 */
	saved_uid = geteuid();
	saved_gid = getegid();
	nsaved_gids = getgroups(NGROUPS_MAX, saved_gids);
	
	(void)setegid(pwd->pw_gid);
	initgroups(username, pwd->pw_gid);
	(void)seteuid(pwd->pw_uid);
	
	if (chdir(pwd->pw_dir) != 0) {
                if (login_getcapbool(lc, "requirehome", 0)) {
			(void)printf("Home directory %s required\n",
			    pwd->pw_dir);
			pam_end(pamh, PAM_SUCCESS);
                        exit(EXIT_FAILURE);
		}

		(void)printf("No home directory %s!\n", pwd->pw_dir);
		if (chdir("/")) {
			pam_end(pamh, PAM_SUCCESS);
			exit(EXIT_SUCCESS);
		}
		pwd->pw_dir = "/";
		(void)printf("Logging in with home = \"/\".\n");
	}

	if (!quietlog) {
		quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;
		pam_silent = 0;
	}

	/* regain special privileges */
	setegid(saved_gid);
	setgroups(nsaved_gids, saved_gids);
	seteuid(saved_uid);

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);

	if (ttyaction(ttyn, "login", pwd->pw_name))
		(void)printf("Warning: ttyaction failed.\n");

	/* Nothing else left to fail -- really log in. */
        update_db(quietlog);

	if (nested == NULL && setusercontext(lc, pwd, pwd->pw_uid,
	    LOGIN_SETLOGIN) != 0) {
		syslog(LOG_ERR, "setusercontext failed");
		pam_end(pamh, PAM_SUCCESS);
		exit(EXIT_FAILURE);
	}

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0) {
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			    username, tty, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s",
			    username, tty);
	}

	/*
	 * Establish groups
	 */
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGROUP) != 0) {
		syslog(LOG_ERR, "setusercontext failed");
		pam_end(pamh, PAM_SUCCESS);
		exit(EXIT_FAILURE);
	}

	pam_err = pam_setcred(pamh, pam_silent|PAM_ESTABLISH_CRED);
	if (pam_err != PAM_SUCCESS)
		PAM_END("pam_setcred");

	pam_err = pam_open_session(pamh, pam_silent);
	if (pam_err != PAM_SUCCESS)
		PAM_END("pam_open_session");
	
	/*
	 * Fork because we need to call pam_closesession as root.
	 * Make sure signals cannot kill the parent.
	 * This is copied from crontab(8), which has to
         * cope with a similar situation.
	 */
	oint = signal(SIGINT, SIG_IGN);
	oabrt = signal(SIGABRT, SIG_IGN);

	switch(pid = fork()) {
	case -1:
		pam_err = pam_close_session(pamh, 0);
		if (pam_err != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_close_session: %s",
			    pam_strerror(pamh, pam_err));
			warnx("pam_close_session: %s",
			    pam_strerror(pamh, pam_err));
		}
		syslog(LOG_ERR, "fork failed: %m");
		warn("fork failed");
		pam_end(pamh, pam_err);		
		exit(EXIT_FAILURE);
		break;

	case 0: /* Child */
		break;

	default:
		/*
		 * Parent: wait for the child to terminate
		 * and call pam_close_session.
		 */
		if ((xpid = waitpid(pid, &status, 0)) != pid) {
			pam_err = pam_close_session(pamh, 0);
			if (pam_err != PAM_SUCCESS) {
				syslog(LOG_ERR,
				    "pam_close_session: %s",
				    pam_strerror(pamh, pam_err));
				warnx("pam_close_session: %s",
				    pam_strerror(pamh, pam_err));
			}
			pam_end(pamh, pam_err);
			if (xpid != -1)
				warnx("wrong PID: %d != %d", pid, xpid);
			else
				warn("wait for pid %d failed", pid);
			exit(EXIT_FAILURE);
		}
		
		(void)signal(SIGINT, oint);
		(void)signal(SIGABRT, oabrt);
		if ((pam_err = pam_close_session(pamh, 0)) != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_close_session: %s",
			    pam_strerror(pamh, pam_err));
			warnx("pam_close_session: %s",
			    pam_strerror(pamh, pam_err));
		}
		pam_end(pamh, PAM_SUCCESS);
		exit(EXIT_SUCCESS);	
		break;
	}

	/* 
	 * The child: starting here, we don't have to care about
	 * handling PAM issues if we exit, the parent will do the
	 * job when we exit.
         * 
	 * Destroy environment unless user has requested its preservation. 
	 * Try to preserve TERM anyway.
	 */
	saved_term = getenv("TERM");
	if (!pflag) {
		environ = envinit;
		if (saved_term)
			setenv("TERM", saved_term, 0);
	}

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;

	shell = login_getcapstr(lc, "shell", pwd->pw_shell, pwd->pw_shell);
	if (*shell == '\0')
		shell = pwd->pw_shell;

	if ((pwd->pw_shell = strdup(shell)) == NULL) {
		syslog(LOG_ERR, "Cannot alloc mem");
		exit(EXIT_FAILURE);
	}
	
	(void)setenv("HOME", pwd->pw_dir, 1);
	(void)setenv("SHELL", pwd->pw_shell, 1);
	if (term[0] == '\0') {
		char *tt = (char *)stypeof(tty);

		if (tt == NULL)
			tt = login_getcapstr(lc, "term", NULL, NULL);

		/* unknown term -> "su" */
		(void)strlcpy(term, tt != NULL ? tt : "su", sizeof(term));
	}
	(void)setenv("TERM", term, 0);
	(void)setenv("LOGNAME", pwd->pw_name, 1);
	(void)setenv("USER", pwd->pw_name, 1);

	/*
	 * Add PAM environement
	 */
	if ((pamenv = pam_getenvlist(pamh)) != NULL) {
		char **envitem;

		for (envitem = pamenv; *envitem; envitem++) {
			putenv(*envitem);
			free(*envitem);
		}

		free(pamenv);
	}

	/* This drops root privs */
	if (setusercontext(lc, pwd, pwd->pw_uid,
	    (LOGIN_SETALL & ~LOGIN_SETLOGIN)) != 0) {
		syslog(LOG_ERR, "setusercontext failed");
		exit(EXIT_FAILURE);
	}

	if (!quietlog) {
		char *fname;

		fname = login_getcapstr(lc, "copyright", NULL, NULL);
		if (fname != NULL && access(fname, F_OK) == 0)
			motd(fname);
		else
			(void)printf("%s", copyrightstr);

                fname = login_getcapstr(lc, "welcome", NULL, NULL);
                if (fname == NULL || access(fname, F_OK) != 0)
                        fname = _PATH_MOTDFILE;
                motd(fname);

		(void)snprintf(tbuf,
		    sizeof(tbuf), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			(void)printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

	login_close(lc);

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	(void)strlcpy(tbuf + 1, (p = strrchr(pwd->pw_shell, '/')) ?
	    p + 1 : pwd->pw_shell, sizeof(tbuf) - 1);

	execlp(pwd->pw_shell, tbuf, NULL);
	err(EXIT_FAILURE, "%s", pwd->pw_shell);
}

#define	NBUFSIZ		(MAXLOGNAME + 1)


void
getloginname(void)
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				exit(EXIT_SUCCESS);
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf) {
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
		}
	}
}

int
rootterm(char *ttyn)
{
	struct ttyent *t;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

jmp_buf motdinterrupt;

void
motd(char *fname)
{
	int fd, nchars;
	sig_t oldint;
	char tbuf[8192];

	if ((fd = open(fname ? fname : _PATH_MOTDFILE, O_RDONLY, 0)) < 0)
		return;
	oldint = signal(SIGINT, sigint);
	if (setjmp(motdinterrupt) == 0)
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
	(void)signal(SIGINT, oldint);
	(void)close(fd);
}

/* ARGSUSED */
void
sigint(int signo)
{

	longjmp(motdinterrupt, 1);
}

/* ARGSUSED */
void
timedout(int signo)
{

	(void)fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(EXIT_SUCCESS);
}

static void
update_db(int quietlog)
{
	if (nested != NULL) {
		if (hostname != NULL)
			syslog(LOG_NOTICE, "%s to %s on tty %s from %s",
			    nested, pwd->pw_name, tty, hostname);
		else
			syslog(LOG_NOTICE, "%s to %s on tty %s", nested,
			    pwd->pw_name, tty);

	}
	if (hostname != NULL && have_ss == 0) {
		socklen_t len = sizeof(ss);
		(void)getpeername(STDIN_FILENO, (struct sockaddr *)&ss, &len);
	}
	(void)gettimeofday(&now, NULL);
}

void
badlogin(char *name)
{

	if (failures == 0)
		return;
	if (hostname) {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s",
		    failures, failures > 1 ? "S" : "", hostname);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", hostname, name);
	} else {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
}

const char *
stypeof(const char *ttyid)
{
	struct ttyent *t;

	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : NULL);
}

void
sleepexit(int eval)
{

	(void)sleep(5);
	exit(eval);
}

void
decode_ss(const char *arg)
{
	struct sockaddr_storage *ssp;
	size_t len = strlen(arg);
	
	if (len > sizeof(*ssp) * 4 + 1 || len < sizeof(*ssp))
		errx(EXIT_FAILURE, "Bad argument");

	if ((ssp = malloc(len)) == NULL)
		err(EXIT_FAILURE, NULL);

	if (strunvis((char *)ssp, arg) != sizeof(*ssp))
		errx(EXIT_FAILURE, "Decoding error");

	(void)memcpy(&ss, ssp, sizeof(ss));
	have_ss = 1;
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-Ffps] [-a address] [-h hostname] [username]\n",
	    getprogname());
	exit(EXIT_FAILURE);
}
