/*	$NetBSD: w.c,v 1.36.2.4 2000/09/28 18:47:47 jdolecek Exp $	*/

/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)w.c	8.6 (Berkeley) 6/30/94";
#else
__RCSID("$NetBSD: w.c,v 1.36.2.4 2000/09/28 18:47:47 jdolecek Exp $");
#endif
#endif /* not lint */

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>

#include "extern.h"

#define	max(a,b)	(((a)>(b))?(a):(b))

struct timeval	boottime;
struct utmp	utmp;
struct winsize	ws;
kvm_t	       *kd;
time_t		now;		/* the current time of day */
time_t		uptime;		/* time of last reboot & elapsed time since */
int		ttywidth;	/* width of tty */
int		argwidth;	/* width of tty left to print process args */
int		header = 1;	/* true if -h flag: don't print heading */
int		nflag;		/* true if -n flag: don't convert addrs */
int		sortidle;	/* sort bu idle time */
char	       *sel_user;	/* login of particular user selected */
char		domain[MAXHOSTNAMELEN + 1];

/*
 * One of these per active utmp entry.
 */
struct	entry {
	struct	entry *next;
	struct	utmp utmp;
	dev_t	tdev;			/* dev_t of terminal */
	time_t	idle;			/* idle time of terminal in seconds */
	struct	kinfo_proc2 *kp;	/* `most interesting' proc */
#ifdef SUPPORT_FTPD_UTMP
	pid_t	ftpd_pid;		/* pid as extracted from ftpd's entry */
#endif
} *ep, *ehead = NULL, **nextp = &ehead;

static void	 pr_args __P((struct kinfo_proc2 *));
static void	 pr_header __P((time_t *, int));
static struct stat
		*ttystat __P((char *));
static void	 usage __P((int));
int	main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *__progname;
	struct kinfo_proc2 *kp;
	struct hostent *hp;
	struct stat *stp;
	FILE *ut;
	struct in_addr l;
	int ch, i, nentries, nusers, wcmd, lognamelen;
	char *memf, *nlistf, *p, *x;
	char buf[MAXHOSTNAMELEN], errbuf[_POSIX2_LINE_MAX];

	/* Are we w(1) or uptime(1)? */
	p = __progname;
	if (*p == '-')
		p++;
	if (*p == 'u') {
		wcmd = 0;
		p = "";
	} else {
		wcmd = 1;
		p = "hiflM:N:nsuw";
	}

	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, p)) != -1)
		switch (ch) {
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle = 1;
			break;
		case 'M':
			header = 0;
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'f': case 'l': case 's': case 'u': case 'w':
			warnx("[-flsuw] no longer supported");
			/* FALLTHROUGH */
		case '?':
		default:
			usage(wcmd);
		}
	argc -= optind;
	argv += optind;

	if ((kd = kvm_openfiles(nlistf, memf, NULL,
	    memf == NULL ? KVM_NO_FILES : O_RDONLY, errbuf)) == NULL)
		errx(1, "%s", errbuf);

	(void)time(&now);
	if ((ut = fopen(_PATH_UTMP, "r")) == NULL && wcmd)
		err(1, "%s", _PATH_UTMP);

	if (*argv)
		sel_user = *argv;

	for (nusers = 0; ut && fread(&utmp, sizeof(utmp), 1, ut);) {
		if (utmp.ut_name[0] == '\0')
			continue;
		++nusers;
		if (wcmd == 0 || (sel_user &&
		    strncmp(utmp.ut_name, sel_user, UT_NAMESIZE) != 0))
			continue;
		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			err(1, NULL);
		*nextp = ep;
		nextp = &(ep->next);
		memmove(&(ep->utmp), &utmp, sizeof(struct utmp));
		if (!(stp = ttystat(ep->utmp.ut_line))) {
#ifdef SUPPORT_FTPD_UTMP
			/*
			 * Hack to recognize and correctly parse
			 * utmp entry made by ftpd. The "tty" used
			 * by ftpd is not a real tty, just identifier in
			 * form ftpPROCESS_ID. Pid parsed from the "tty name"
			 * is used later to match corresponding process.
			 */
			if (strncmp(ep->utmp.ut_line, "ftp", 3) == 0)
				ep->ftpd_pid =
					strtol(ep->utmp.ut_line + 3, NULL, 10);
#endif /* SUPPORT_FTPD_UTMP */
			
			continue;
		}
		ep->tdev = stp->st_rdev;
		/*
		 * If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			int mib[2];
			size_t size;

			mib[0] = CTL_KERN;
			mib[1] = KERN_CONSDEV;
			size = sizeof(dev_t);
			(void) sysctl(mib, 2, &ep->tdev, &size, NULL, 0);
		}
		if ((ep->idle = now - stp->st_atime) < 0)
			ep->idle = 0;
	}
	if (ut)
		(void)fclose(ut);
	else
		nusers = 1;

	if (header || wcmd == 0) {
		pr_header(&now, nusers);
		if (wcmd == 0)
			exit (0);
	}

	if ((kp = kvm_getproc2(kd, KERN_PROC_ALL, 0,
	    sizeof(struct kinfo_proc2), &nentries)) == NULL)
		errx(1, "%s", kvm_geterr(kd));

	/* Include trailing space because TTY header starts one column early. */
	lognamelen = sizeof("USER ") - 1 /* NUL */;
	for (i = 0; i < nentries; i++, kp++) {

		if (kp->p_stat == SIDL || kp->p_stat == SZOMB)
			continue;

		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == kp->p_tdev
				&& kp->p__pgid == kp->p_tpgid) {
				/*
				 * Proc is in foreground of this terminal
				 */
				if (proc_compare(ep->kp, kp)) {
					ep->kp = kp;
					lognamelen = max(lognamelen,
					    strlen(kp->p_login));
				}
				break;
			}
#ifdef SUPPORT_FTPD_UTMP
			/*
			 * Hack to match process to ftp utmp entry.
			 */
			else if (ep->tdev == 0 && kp->p_tdev == NODEV
				 && ep->ftpd_pid == kp->p_pid) {
				ep->kp = kp;
				lognamelen = max(lognamelen,
					    strlen(kp->p_login));
			}
#endif /* SUPPORT_FTPD_UTMP */
		}
	}

	argwidth = printf("%-*sTTY %-*s %*s  IDLE WHAT\n",
	    lognamelen, "USER", UT_HOSTSIZE, "FROM",
	    7 /* "dddhhXm" */, "LOGIN@");
	argwidth -= sizeof("WHAT\n") - 1 /* NUL */;

	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
	       ttywidth = 79;
        else
	       ttywidth = ws.ws_col - 1;
	argwidth = ttywidth - argwidth;
	if (argwidth < 4)
		argwidth = 8;
	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from = ehead, *save;
		
		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead;
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				continue;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}
			
	if (!nflag) {
		int	rv;

		rv = gethostname(domain, sizeof(domain));
		domain[sizeof(domain) - 1] = '\0';
		if (rv < 0 || (p = strchr(domain, '.')) == 0)
			domain[0] = '\0';
		else
			memmove(domain, p, strlen(p) + 1);
	}

	for (ep = ehead; ep != NULL; ep = ep->next) {
		p = *ep->utmp.ut_host ? ep->utmp.ut_host : "-";
		for (x = p; x < p + UT_HOSTSIZE; x++)
			if (*x == '\0' || *x == ':')
				break;
		if (x == p + UT_HOSTSIZE || *x != ':')
			x = NULL;
		else
			*x++ = '\0';

		if (!nflag && inet_aton(p, &l) &&
		    (hp = gethostbyaddr((char *)&l, sizeof(l), AF_INET))) {
			if (domain[0] != '\0') {
				p = hp->h_name;
				p += strlen(hp->h_name);
				p -= strlen(domain);
				if (p > hp->h_name && strcmp(p, domain) == 0)
					*p = '\0';
			}
			p = hp->h_name;
		}
		if (x) {
			(void)snprintf(buf, sizeof(buf), "%s:%.*s", p,
			    (int)(ep->utmp.ut_host + UT_HOSTSIZE - x), x);
			p = buf;
		}
		if (ep->kp == NULL) {
		    warnx("Stale utmp entry: %.*s %.*s %.*s",
			UT_NAMESIZE, ep->utmp.ut_name,
			UT_LINESIZE, ep->utmp.ut_line,
			UT_HOSTSIZE, ep->utmp.ut_host);
		    continue;
		}
		(void)printf("%-*s %-2.2s %-*.*s ",
		    lognamelen, ep->kp->p_login,
		    (strncmp(ep->utmp.ut_line, "tty", 3) &&
		    strncmp(ep->utmp.ut_line, "dty", 3)) ?
		    ep->utmp.ut_line : ep->utmp.ut_line + 3,
		    UT_HOSTSIZE, UT_HOSTSIZE, *p ? p : "-");
		pr_attime(&ep->utmp.ut_time, &now);
		pr_idle(ep->idle);
		pr_args(ep->kp);
		(void)printf("\n");
	}
	exit(0);
}

static void
pr_args(kp)
	struct kinfo_proc2 *kp;
{
	char **argv;
	int left;

	if (kp == 0)
		goto nothing;
	left = argwidth;
	argv = kvm_getargv2(kd, kp, argwidth);
	if (argv == 0)
		goto nothing;
	while (*argv) {
		fmt_puts(*argv, &left);
		argv++;
		fmt_putc(' ', &left);
	}
	return;
nothing:
	putchar('-');
}

static void
pr_header(nowp, nusers)
	time_t *nowp;
	int nusers;
{
	double avenrun[3];
	time_t uptime;
	int days, hrs, i, mins;
	int mib[2];
	size_t size;
	char buf[256];

	/*
	 * Print time of day.
	 *
	 * SCCS forces the string manipulation below, as it replaces
	 * %, M, and % in a character string with the file name.
	 */
	(void)strftime(buf, sizeof(buf),
	    __CONCAT("%l:%","M%p"), localtime(nowp));
	buf[sizeof(buf) - 1] = '\0';
	(void)printf("%s ", buf);

	/*
	 * Print how long system has been up.
	 * (Found by looking getting "boottime" from the kernel)
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0) {
		uptime = now - boottime.tv_sec;
		uptime += 30;
		if (uptime > SECSPERMIN) {
			days = uptime / SECSPERDAY;
			uptime %= SECSPERDAY;
			hrs = uptime / SECSPERHOUR;
			uptime %= SECSPERHOUR;
			mins = uptime / SECSPERMIN;
			(void)printf(" up");
			if (days > 0)
				(void)printf(" %d day%s,", days,
				    days > 1 ? "s" : "");
			if (hrs > 0 && mins > 0)
				(void)printf(" %2d:%02d,", hrs, mins);
			else {
				if (hrs > 0)
					(void)printf(" %d hr%s,",
					    hrs, hrs > 1 ? "s" : "");
				if (mins > 0)
					(void)printf(" %d min%s,",
					    mins, mins > 1 ? "s" : "");
			}
		}
	}

	/* Print number of users logged in to system */
	(void)printf(" %d user%s", nusers, nusers != 1 ? "s" : "");

	/*
	 * Print 1, 5, and 15 minute load averages.
	 */
	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) == -1)
		(void)printf(", no load average information available\n");
	else {
		(void)printf(", load averages:");
		for (i = 0; i < (sizeof(avenrun) / sizeof(avenrun[0])); i++) {
			if (i > 0)
				(void)printf(",");
			(void)printf(" %.2f", avenrun[i]);
		}
		(void)printf("\n");
	}
}

static struct stat *
ttystat(line)
	char *line;
{
	static struct stat sb;
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s/%s", _PATH_DEV, line);
	if (stat(ttybuf, &sb))
		return (NULL);
	return (&sb);
}

static void
usage(wcmd)
	int wcmd;
{
	if (wcmd)
		(void)fprintf(stderr,
		    "usage: w: [-hin] [-M core] [-N system] [user]\n");
	else
		(void)fprintf(stderr, "uptime\n");
	exit(1);
}
