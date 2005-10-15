/*	$NetBSD: pkill.c,v 1.9.2.3 2005/10/15 16:33:15 riz Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
__RCSID("$NetBSD: pkill.c,v 1.9.2.3 2005/10/15 16:33:15 riz Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <kvm.h>
#include <err.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <paths.h>

#define	STATUS_MATCH	0
#define	STATUS_NOMATCH	1
#define	STATUS_BADUSAGE	2
#define	STATUS_ERROR	3

enum listtype {
	LT_GENERIC,
	LT_USER,
	LT_GROUP,
	LT_TTY,
	LT_PGRP,
	LT_SID
};

struct list {
	SLIST_ENTRY(list) li_chain;
	long	li_number;
};

SLIST_HEAD(listhead, list);

static struct kinfo_proc2	*plist;
static char	*selected;
static const char *delim = "\n";
static int	nproc;
static int	pgrep;
static int	signum = SIGTERM;
static int	newest;
static int	inverse;
static int	longfmt;
static int	matchargs;
static int	fullmatch;
static int	cflags = REG_EXTENDED;
static kvm_t	*kd;
static pid_t	mypid;

static struct listhead euidlist = SLIST_HEAD_INITIALIZER(list);
static struct listhead ruidlist = SLIST_HEAD_INITIALIZER(list);
static struct listhead rgidlist = SLIST_HEAD_INITIALIZER(list);
static struct listhead pgrplist = SLIST_HEAD_INITIALIZER(list);
static struct listhead ppidlist = SLIST_HEAD_INITIALIZER(list);
static struct listhead tdevlist = SLIST_HEAD_INITIALIZER(list);
static struct listhead sidlist = SLIST_HEAD_INITIALIZER(list);

int	main(int, char **);
static void	usage(void) __attribute__((__noreturn__));
static int	killact(struct kinfo_proc2 *);
static int	grepact(struct kinfo_proc2 *);
static void	makelist(struct listhead *, enum listtype, char *);

int
main(int argc, char **argv)
{
	char buf[_POSIX2_LINE_MAX], *mstr, **pargv, *p, *q;
	int i, j, ch, bestidx, rv, criteria;
	int (*action)(struct kinfo_proc2 *);
	struct kinfo_proc2 *kp;
	struct list *li;
	u_int32_t bestsec, bestusec;
	regex_t reg;
	regmatch_t regmatch;

	setprogname(argv[0]);

	if (strcmp(getprogname(), "pgrep") == 0) {
		action = grepact;
		pgrep = 1;
	} else {
		action = killact;
		p = argv[1];

		if (argc > 1 && p[0] == '-') {
			p++;
			i = (int)strtol(p, &q, 10);
			if (*q == '\0') {
				signum = i;
				argv++;
				argc--;
			} else {
				if (strncasecmp(p, "sig", 3) == 0)
					p += 3;
				for (i = 1; i < NSIG; i++)
					if (strcasecmp(sys_signame[i], p) == 0)
						break;
				if (i != NSIG) {
					signum = i;
					argv++;
					argc--;
				}
			}
		}
	}

	criteria = 0;

	while ((ch = getopt(argc, argv, "G:P:U:d:fg:ilns:t:u:vx")) != -1)
		switch (ch) {
		case 'G':
			makelist(&rgidlist, LT_GROUP, optarg);
			criteria = 1;
			break;
		case 'P':
			makelist(&ppidlist, LT_GENERIC, optarg);
			criteria = 1;
			break;
		case 'U':
			makelist(&ruidlist, LT_USER, optarg);
			criteria = 1;
			break;
		case 'd':
			if (!pgrep)
				usage();
			delim = optarg;
			break;
		case 'f':
			matchargs = 1;
			break;
		case 'g':
			makelist(&pgrplist, LT_PGRP, optarg);
			criteria = 1;
			break;
		case 'i':
			cflags |= REG_ICASE;
			break;
		case 'l':
			if (!pgrep)
				usage();
			longfmt = 1;
			break;
		case 'n':
			newest = 1;
			criteria = 1;
			break;
		case 's':
			makelist(&sidlist, LT_SID, optarg);
			criteria = 1;
			break;
		case 't':
			makelist(&tdevlist, LT_TTY, optarg);
			criteria = 1;
			break;
		case 'u':
			makelist(&euidlist, LT_USER, optarg);
			criteria = 1;
			break;
		case 'v':
			inverse = 1;
			break;
		case 'x':
			fullmatch = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;
	if (argc != 0)
		criteria = 1;
	if (!criteria)
		usage();

	mypid = getpid();

	/*
	 * Retrieve the list of running processes from the kernel.
	 */
	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, buf);
	if (kd == NULL)
		errx(STATUS_ERROR, "Cannot open kernel files (%s)", buf);

	plist = kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(*plist), &nproc);
	if (plist == NULL)
		errx(STATUS_ERROR, "Cannot get process list (%s)",
		    kvm_geterr(kd));

	/*
	 * Allocate memory which will be used to keep track of the
	 * selection.
	 */
	if ((selected = calloc((size_t)1, (size_t)nproc)) == NULL)
		err(STATUS_ERROR, "Cannot allocate memory for %d processes",
		    nproc);

	/*
	 * Refine the selection.
	 */
	for (; *argv != NULL; argv++) {
		if ((rv = regcomp(&reg, *argv, cflags)) != 0) {
			(void)regerror(rv, &reg, buf, sizeof(buf));
			errx(STATUS_BADUSAGE,
			    "Cannot compile regular expression `%s' (%s)",
			    *argv, buf);
		}

		for (i = 0, kp = plist; i < nproc; i++, kp++) {
			if ((kp->p_flag & P_SYSTEM) != 0 || kp->p_pid == mypid)
				continue;

			if (matchargs) {
				if ((pargv = kvm_getargv2(kd, kp, 0)) == NULL)
					continue;

				j = 0;
				while (j < sizeof(buf) && *pargv != NULL) {
					j += snprintf(buf + j, sizeof(buf) - j,
					    pargv[1] != NULL ? "%s " : "%s",
					    pargv[0]);
					pargv++;
				}

				mstr = buf;
			} else
				mstr = kp->p_comm;

			rv = regexec(&reg, mstr, 1, &regmatch, 0);
			if (rv == 0) {
				if (fullmatch) {
					if (regmatch.rm_so == 0 &&
					    regmatch.rm_eo == strlen(mstr))
						selected[i] = 1;
				} else
					selected[i] = 1;
			} else if (rv != REG_NOMATCH) {
				(void)regerror(rv, &reg, buf, sizeof(buf));
				errx(STATUS_ERROR,
				    "Regular expression evaluation error (%s)",
				    buf);
			}
		}

		regfree(&reg);
	}

	for (i = 0, kp = plist; i < nproc; i++, kp++) {
		if ((kp->p_flag & P_SYSTEM) != 0)
			continue;

		SLIST_FOREACH(li, &ruidlist, li_chain)
			if (kp->p_ruid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&ruidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}
	
		SLIST_FOREACH(li, &rgidlist, li_chain)
			if (kp->p_rgid == (gid_t)li->li_number)
				break;
		if (SLIST_FIRST(&rgidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &euidlist, li_chain)
			if (kp->p_uid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&euidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &ppidlist, li_chain)
			if (kp->p_ppid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&ppidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &pgrplist, li_chain)
			if (kp->p__pgid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&pgrplist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &tdevlist, li_chain) {
			if (li->li_number == -1 &&
			    (kp->p_flag & P_CONTROLT) == 0)
				break;
			if (kp->p_tdev == (uid_t)li->li_number)
				break;
		}
		if (SLIST_FIRST(&tdevlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &sidlist, li_chain)
			if (kp->p_sid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&sidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		if (argc == 0)
			selected[i] = 1;
	}

	if (newest) {
		bestsec = 0;
		bestusec = 0;
		bestidx = -1;

		for (i = 0, kp = plist; i < nproc; i++, kp++) {
			if (!selected[i])
				continue;

			if (kp->p_ustart_sec > bestsec ||
			    (kp->p_ustart_sec == bestsec
			    && kp->p_ustart_usec > bestusec)) {
			    	bestsec = kp->p_ustart_sec;
			    	bestusec = kp->p_ustart_usec;
				bestidx = i;
			}
		}

		(void)memset(selected, 0, (size_t)nproc);
		if (bestidx != -1)
			selected[bestidx] = 1;
	}

	/*
	 * Take the appropriate action for each matched process, if any.
	 */
	for (i = 0, rv = 0, kp = plist; i < nproc; i++, kp++) {
		if (kp->p_pid == mypid)
			continue;
		if (selected[i]) {
			if (inverse)
				continue;
		} else if (!inverse)
			continue;

		if ((kp->p_flag & P_SYSTEM) != 0)
			continue;

		rv |= (*action)(kp);
	}

	return rv ? STATUS_MATCH : STATUS_NOMATCH;
}

static void
usage(void)
{
	const char *ustr;

	if (pgrep)
		ustr = "[-filnvx] [-d delim]";
	else
		ustr = "[-signal] [-finvx]";

	(void)fprintf(stderr,
		"Usage: %s %s [-G gid] [-P ppid] [-U uid] [-g pgrp] [-s sid]\n"
		"             [-t tty] [-u euid] pattern ...\n", getprogname(),
		ustr);

	exit(STATUS_ERROR);
}

static int
killact(struct kinfo_proc2 *kp)
{
	if (kill(kp->p_pid, signum) == -1) {
		if (errno == ESRCH)
			/*
			 * The process disappeared between us matching
			 * it and us signalling it.  Return 0 to
			 * indicate that the process should not be
			 * considered a match.
			 */
			return 0;

		err(STATUS_ERROR, "signalling pid %d", (int)kp->p_pid);
	}

	return 1;
}

static int
grepact(struct kinfo_proc2 *kp)
{
	char **argv;

	if (longfmt && matchargs) {
		if ((argv = kvm_getargv2(kd, kp, 0)) == NULL)
			/*
			 * The process disappeared?  Return 0 to
			 * indicate that the process should not be
			 * considered a match.
			 */
			return 0;

		(void)printf("%d ", (int)kp->p_pid);
		for (; *argv != NULL; argv++) {
			(void)printf("%s", *argv);
			if (argv[1] != NULL)
				(void)putchar(' ');
		}
	} else if (longfmt)
		(void)printf("%d %s", (int)kp->p_pid, kp->p_comm);
	else
		(void)printf("%d", (int)kp->p_pid);

	(void)printf("%s", delim);

	return 1;
}

static void
makelist(struct listhead *head, enum listtype type, char *src)
{
	struct list *li;
	struct passwd *pw;
	struct group *gr;
	struct stat st;
	char *sp, *ep, buf[MAXPATHLEN];
	const char *p;
	int empty;
	const char *prefix = _PATH_DEV;

	empty = 1;

	while ((sp = strsep(&src, ",")) != NULL) {
		if (*sp == '\0')
			usage();

		if ((li = malloc(sizeof(*li))) == NULL)
			err(STATUS_ERROR, "Cannot allocate %zd bytes",
			    sizeof(*li));
		SLIST_INSERT_HEAD(head, li, li_chain);
		empty = 0;

		li->li_number = (uid_t)strtol(sp, &ep, 0);
		if (*ep == '\0') {
			switch (type) {
			case LT_PGRP:
				if (li->li_number == 0)
					li->li_number = getpgrp();
				break;
			case LT_SID:
				if (li->li_number == 0)
					li->li_number = getsid(mypid);
				break;
			case LT_TTY:
				usage();
				/*NOTREACHED*/
			default:
				break;
			}
			continue;
		}

		switch (type) {
		case LT_USER:
			if ((pw = getpwnam(sp)) == NULL)
				errx(STATUS_BADUSAGE, "Unknown user `%s'",
				    sp);
			li->li_number = pw->pw_uid;
			break;
		case LT_GROUP:
			if ((gr = getgrnam(sp)) == NULL)
				errx(STATUS_BADUSAGE, "Unknown group `%s'",
				    sp);
			li->li_number = gr->gr_gid;
			break;
		case LT_TTY:
			if (strcmp(sp, "-") == 0) {
				li->li_number = -1;
				break;
			} else if (strcmp(sp, "co") == 0)
				p = "console";
			else if (strncmp(sp, "tty", 3) == 0)
				p = sp;
			else {
				p = sp;
				prefix = _PATH_TTY;
			}

			(void)snprintf(buf, sizeof(buf), "%s%s", prefix, p);

			if (stat(buf, &st) == -1) {
				if (errno == ENOENT)
					errx(STATUS_BADUSAGE,
					    "No such tty: `%s'", sp);
				err(STATUS_ERROR, "Cannot access `%s'", sp);
			}

			if ((st.st_mode & S_IFCHR) == 0)
				errx(STATUS_BADUSAGE, "Not a tty: `%s'", sp);

			li->li_number = st.st_rdev;
			break;
		default:
			usage();
		}
	}

	if (empty)
		usage();
}
