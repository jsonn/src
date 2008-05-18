/*
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)edquota.c	8.3 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: edquota.c,v 1.27.18.1 2008/05/18 12:36:15 yamt Exp $");
#endif
#endif /* not lint */

/*
 * Disk quota editor.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <ufs/ufs/quota.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

const char *qfname = QUOTAFILENAME;
const char *qfextension[] = INITQFNAMES;
const char *quotagroup = QUOTAGROUP;
char tmpfil[] = _PATH_TMP;

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[MAXPATHLEN + 1];
	char	qfname[1];	/* actually longer */
};
#define	FOUND	0x01

#define MAX_TMPSTR	(100+MAXPATHLEN)

int	main __P((int, char **));
void	usage __P((void));
int	getentry __P((const char *, int));
struct quotause *
	getprivs __P((long, int, char *));
void	putprivs __P((long, int, struct quotause *));
int	editit __P((char *));
int	writeprivs __P((struct quotause *, int, char *, int));
int	readprivs __P((struct quotause *, int));
int	writetimes __P((struct quotause *, int, int));
int	readtimes __P((struct quotause *, int));
char *	cvtstoa __P((time_t));
int	cvtatos __P((time_t, char *, time_t *));
void	freeprivs __P((struct quotause *));
int	alldigits __P((const char *));
int	hasquota __P((struct fstab *, int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct quotause *qup, *protoprivs, *curprivs;
	long id, protoid;
	int quotatype, tmpfd;
	char *protoname;
	char *soft = NULL, *hard = NULL;
	char *fs = NULL;
	int ch;
	int tflag = 0, pflag = 0;

	if (argc < 2)
		usage();
	if (getuid())
		errx(1, "permission denied");
	protoname = NULL;
	quotatype = USRQUOTA;
	while ((ch = getopt(argc, argv, "ugtp:s:h:f:")) != -1) {
		switch(ch) {
		case 'p':
			protoname = optarg;
			pflag++;
			break;
		case 'g':
			quotatype = GRPQUOTA;
			break;
		case 'u':
			quotatype = USRQUOTA;
			break;
		case 't':
			tflag++;
			break;
		case 's':
			soft = optarg;
			break;
		case 'h':
			hard = optarg;
			break;
		case 'f':
			fs = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (pflag) {
		if (soft || hard)
			usage();
		if ((protoid = getentry(protoname, quotatype)) == -1)
			exit(1);
		protoprivs = getprivs(protoid, quotatype, fs);
		for (qup = protoprivs; qup; qup = qup->next) {
			qup->dqblk.dqb_btime = 0;
			qup->dqblk.dqb_itime = 0;
		}
		while (argc-- > 0) {
			if ((id = getentry(*argv++, quotatype)) < 0)
				continue;
			putprivs(id, quotatype, protoprivs);
		}
		exit(0);
	}
	if (soft || hard) {
		struct quotause *lqup;
		u_int32_t softb, hardb, softi, hardi;
		if (tflag)
			usage();
		if (soft) {
			if (sscanf(soft, "%d/%d", &softb, &softi) != 2)
				usage();
			softb = btodb((u_quad_t)softb * 1024);
		}
		if (hard) {
			if (sscanf(hard, "%d/%d", &hardb, &hardi) != 2)
				usage();
			hardb = btodb((u_quad_t)hardb * 1024);
		}
		for ( ; argc > 0; argc--, argv++) {
			if ((id = getentry(*argv, quotatype)) == -1)
				continue;
			curprivs = getprivs(id, quotatype, fs);
			for (lqup = curprivs; lqup; lqup = lqup->next) {
				if (soft) {
					if (softb &&
					    lqup->dqblk.dqb_curblocks >= softb &&
					    (lqup->dqblk.dqb_bsoftlimit == 0 ||
					    lqup->dqblk.dqb_curblocks <
					    lqup->dqblk.dqb_bsoftlimit))
						lqup->dqblk.dqb_btime = 0;
					if (softi &&
					    lqup->dqblk.dqb_curinodes >= softi &&
					    (lqup->dqblk.dqb_isoftlimit == 0 ||
					    lqup->dqblk.dqb_curinodes <
					    lqup->dqblk.dqb_isoftlimit))
						lqup->dqblk.dqb_itime = 0;
					lqup->dqblk.dqb_bsoftlimit = softb;
					lqup->dqblk.dqb_isoftlimit = softi;
				}
				if (hard) {
					lqup->dqblk.dqb_bhardlimit = hardb;
					lqup->dqblk.dqb_ihardlimit = hardi;
				}
			}
			putprivs(id, quotatype, curprivs);
			freeprivs(curprivs);
		}
		exit(0);
	}
	tmpfd = mkstemp(tmpfil);
	fchown(tmpfd, getuid(), getgid());
	if (tflag) {
		if (soft || hard)
			usage();
		protoprivs = getprivs(0, quotatype, fs);
		if (writetimes(protoprivs, tmpfd, quotatype) == 0)
			exit(1);
		if (editit(tmpfil) && readtimes(protoprivs, tmpfd))
			putprivs(0, quotatype, protoprivs);
		freeprivs(protoprivs);
		exit(0);
	}
	for ( ; argc > 0; argc--, argv++) {
		if ((id = getentry(*argv, quotatype)) == -1)
			continue;
		curprivs = getprivs(id, quotatype, fs);
		if (writeprivs(curprivs, tmpfd, *argv, quotatype) == 0)
			continue;
		if (editit(tmpfil) && readprivs(curprivs, tmpfd))
			putprivs(id, quotatype, curprivs);
		freeprivs(curprivs);
	}
	close(tmpfd);
	unlink(tmpfil);
	exit(0);
}

void
usage()
{
	fprintf(stderr,
	    "usage: edquota [-u] [-p username] [-f filesystem] username ...\n"
	    "\tedquota -g [-p groupname] [-f filesystem] groupname ...\n"
	    "\tedquota [-u] [-f filesystem] [-s b#/i#] [-h b#/i#] username ...\n"
	    "\tedquota -g [-f filesystem] [-s b#/i#] [-h b#/i#] groupname ...\n"
	    "\tedquota [-u] [-f filesystem] -t\n"
	    "\tedquota -g [-f filesystem] -t\n"
	    );
	exit(1);
}

/*
 * This routine converts a name for a particular quota type to
 * an identifier. This routine must agree with the kernel routine
 * getinoquota as to the interpretation of quota types.
 */
int
getentry(name, quotatype)
	const char *name;
	int quotatype;
{
	struct passwd *pw;
	struct group *gr;

	if (alldigits(name))
		return (atoi(name));
	switch(quotatype) {
	case USRQUOTA:
		if ((pw = getpwnam(name)) != NULL)
			return (pw->pw_uid);
		warnx("%s: no such user", name);
		break;
	case GRPQUOTA:
		if ((gr = getgrnam(name)) != NULL)
			return (gr->gr_gid);
		warnx("%s: no such group", name);
		break;
	default:
		warnx("%d: unknown quota type", quotatype);
		break;
	}
	sleep(1);
	return (-1);
}

/*
 * Collect the requested quota information.
 */
struct quotause *
getprivs(id, quotatype, filesys)
	long id;
	int quotatype;
	char *filesys;
{
	struct fstab *fs;
	struct quotause *qup, *quptail;
	struct quotause *quphead;
	int qcmd, qupsize, fd;
	char *qfpathname;
	static int warned = 0;

	setfsent();
	quptail = NULL;
	quphead = (struct quotause *)0;
	qcmd = QCMD(Q_GETQUOTA, quotatype);
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_vfstype, "ffs"))
			continue;
		if (filesys && strcmp(fs->fs_spec, filesys) != 0 &&
		    strcmp(fs->fs_file, filesys) != 0)
			continue;
		if (!hasquota(fs, quotatype, &qfpathname))
			continue;
		qupsize = sizeof(*qup) + strlen(qfpathname);
		if ((qup = (struct quotause *)malloc(qupsize)) == NULL)
			errx(2, "out of memory");
		if (quotactl(fs->fs_file, qcmd, id, &qup->dqblk) != 0) {
	    		if (errno == EOPNOTSUPP && !warned) {
				warned++;
				warnx(
				    "Quotas are not compiled into this kernel");
				sleep(3);
			}
			if ((fd = open(qfpathname, O_RDONLY)) < 0) {
				fd = open(qfpathname, O_RDWR|O_CREAT, 0640);
				if (fd < 0 && errno != ENOENT) {
					warnx("open `%s'", qfpathname);
					free(qup);
					continue;
				}
				warnx("Creating quota file %s", qfpathname);
				sleep(3);
				(void) fchown(fd, getuid(),
				    getentry(quotagroup, GRPQUOTA));
				(void) fchmod(fd, 0640);
			}
			(void)lseek(fd, (off_t)(id * sizeof(struct dqblk)),
			    SEEK_SET);
			switch (read(fd, &qup->dqblk, sizeof(struct dqblk))) {
			case 0:			/* EOF */
				/*
				 * Convert implicit 0 quota (EOF)
				 * into an explicit one (zero'ed dqblk)
				 */
				memset((caddr_t)&qup->dqblk, 0,
				    sizeof(struct dqblk));
				break;

			case sizeof(struct dqblk):	/* OK */
				break;

			default:		/* ERROR */
				warn("read error in `%s'", qfpathname);
				close(fd);
				free(qup);
				continue;
			}
			close(fd);
		}
		strcpy(qup->qfname, qfpathname);
		strcpy(qup->fsname, fs->fs_file);
		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		qup->next = 0;
	}
	endfsent();
	return (quphead);
}

/*
 * Store the requested quota information.
 */
void
putprivs(id, quotatype, quplist)
	long id;
	int quotatype;
	struct quotause *quplist;
{
	struct quotause *qup;
	int qcmd, fd;

	qcmd = QCMD(Q_SETQUOTA, quotatype);
	for (qup = quplist; qup; qup = qup->next) {
		if (quotactl(qup->fsname, qcmd, id, &qup->dqblk) == 0)
			continue;
		if ((fd = open(qup->qfname, O_WRONLY)) < 0) {
			warnx("open `%s'", qup->qfname);
		} else {
			(void)lseek(fd,
			    (off_t)(id * (long)sizeof (struct dqblk)),
			    SEEK_SET);
			if (write(fd, &qup->dqblk, sizeof (struct dqblk)) !=
			    sizeof (struct dqblk))
				warnx("writing `%s'", qup->qfname);
			close(fd);
		}
	}
}

/*
 * Take a list of privileges and get it edited.
 */
int
editit(ltmpfile)
	char *ltmpfile;
{
	long omask;
	int pid, lst;
	char p[MAX_TMPSTR];

	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
 top:
	if ((pid = fork()) < 0) {

		if (errno == EPROCLIM) {
			warnx("You have too many processes");
			return(0);
		}
		if (errno == EAGAIN) {
			sleep(1);
			goto top;
		}
		warn("fork");
		return (0);
	}
	if (pid == 0) {
		const char *ed;

		sigsetmask(omask);
		setgid(getgid());
		setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = _PATH_VI;
		if (strlen(ed) + strlen(ltmpfile) + 2 >= MAX_TMPSTR) {
			err (1, "%s", "editor or filename too long");
		}
		snprintf (p, MAX_TMPSTR, "%s %s", ed, ltmpfile);
		execlp(_PATH_BSHELL, _PATH_BSHELL, "-c", p, NULL);
		err(1, "%s", ed);
	}
	waitpid(pid, &lst, 0);
	sigsetmask(omask);
	if (!WIFEXITED(lst) || WEXITSTATUS(lst) != 0)
		return (0);
	return (1);
}

/*
 * Convert a quotause list to an ASCII file.
 */
int
writeprivs(quplist, outfd, name, quotatype)
	struct quotause *quplist;
	int outfd;
	char *name;
	int quotatype;
{
	struct quotause *qup;
	FILE *fd;

	ftruncate(outfd, 0);
	(void)lseek(outfd, (off_t)0, SEEK_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		errx(1, "fdopen `%s'", tmpfil);
	fprintf(fd, "Quotas for %s %s:\n", qfextension[quotatype], name);
	for (qup = quplist; qup; qup = qup->next) {
		fprintf(fd, "%s: %s %d, limits (soft = %d, hard = %d)\n",
		    qup->fsname, "blocks in use:",
		    (int)(dbtob((u_quad_t)qup->dqblk.dqb_curblocks) / 1024),
		    (int)(dbtob((u_quad_t)qup->dqblk.dqb_bsoftlimit) / 1024),
		    (int)(dbtob((u_quad_t)qup->dqblk.dqb_bhardlimit) / 1024));
		fprintf(fd, "%s %d, limits (soft = %d, hard = %d)\n",
		    "\tinodes in use:", qup->dqblk.dqb_curinodes,
		    qup->dqblk.dqb_isoftlimit, qup->dqblk.dqb_ihardlimit);
	}
	fclose(fd);
	return (1);
}

/*
 * Merge changes to an ASCII file into a quotause list.
 */
int
readprivs(quplist, infd)
	struct quotause *quplist;
	int infd;
{
	struct quotause *qup;
	FILE *fd;
	int cnt;
	char *cp;
	struct dqblk dqblk;
	char *fsp, line1[BUFSIZ], line2[BUFSIZ];

	(void)lseek(infd, (off_t)0, SEEK_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		warn("Can't re-read temp file");
		return (0);
	}
	/*
	 * Discard title line, then read pairs of lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL &&
	       fgets(line2, sizeof (line2), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			warnx("%s: bad format", line1);
			goto out;
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp,
			    &fsp[strlen(fsp) + 1]);
			goto out;
		}
		cnt = sscanf(cp,
		    " blocks in use: %d, limits (soft = %d, hard = %d)",
		    &dqblk.dqb_curblocks, &dqblk.dqb_bsoftlimit,
		    &dqblk.dqb_bhardlimit);
		if (cnt != 3) {
			warnx("%s:%s: bad format", fsp, cp);
			goto out;
		}
		dqblk.dqb_curblocks = btodb((u_quad_t)
		    dqblk.dqb_curblocks * 1024);
		dqblk.dqb_bsoftlimit = btodb((u_quad_t)
		    dqblk.dqb_bsoftlimit * 1024);
		dqblk.dqb_bhardlimit = btodb((u_quad_t)
		    dqblk.dqb_bhardlimit * 1024);
		if ((cp = strtok(line2, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp, line2);
			goto out;
		}
		cnt = sscanf(cp,
		    "\tinodes in use: %d, limits (soft = %d, hard = %d)",
		    &dqblk.dqb_curinodes, &dqblk.dqb_isoftlimit,
		    &dqblk.dqb_ihardlimit);
		if (cnt != 3) {
			warnx("%s: %s: bad format", fsp, line2);
			goto out;
		}
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			/*
			 * Cause time limit to be reset when the quota
			 * is next used if previously had no soft limit
			 * or were under it, but now have a soft limit
			 * and are over it.
			 */
			if (dqblk.dqb_bsoftlimit &&
			    qup->dqblk.dqb_curblocks >= dqblk.dqb_bsoftlimit &&
			    (qup->dqblk.dqb_bsoftlimit == 0 ||
			     qup->dqblk.dqb_curblocks <
			     qup->dqblk.dqb_bsoftlimit))
				qup->dqblk.dqb_btime = 0;
			if (dqblk.dqb_isoftlimit &&
			    qup->dqblk.dqb_curinodes >= dqblk.dqb_isoftlimit &&
			    (qup->dqblk.dqb_isoftlimit == 0 ||
			     qup->dqblk.dqb_curinodes <
			     qup->dqblk.dqb_isoftlimit))
				qup->dqblk.dqb_itime = 0;
			qup->dqblk.dqb_bsoftlimit = dqblk.dqb_bsoftlimit;
			qup->dqblk.dqb_bhardlimit = dqblk.dqb_bhardlimit;
			qup->dqblk.dqb_isoftlimit = dqblk.dqb_isoftlimit;
			qup->dqblk.dqb_ihardlimit = dqblk.dqb_ihardlimit;
			qup->flags |= FOUND;
			if (dqblk.dqb_curblocks == qup->dqblk.dqb_curblocks &&
			    dqblk.dqb_curinodes == qup->dqblk.dqb_curinodes)
				break;
			warnx("%s: cannot change current allocation", fsp);
			break;
		}
	}
out:
	fclose(fd);
	/*
	 * Disable quotas for any filesystems that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_bsoftlimit = 0;
		qup->dqblk.dqb_bhardlimit = 0;
		qup->dqblk.dqb_isoftlimit = 0;
		qup->dqblk.dqb_ihardlimit = 0;
	}
	return (1);
}

/*
 * Convert a quotause list to an ASCII file of grace times.
 */
int
writetimes(quplist, outfd, quotatype)
	struct quotause *quplist;
	int outfd;
	int quotatype;
{
	struct quotause *qup;
	FILE *fd;

	ftruncate(outfd, 0);
	(void)lseek(outfd, (off_t)0, SEEK_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		err(1, "fdopen `%s'", tmpfil);
	fprintf(fd, "Time units may be: days, hours, minutes, or seconds\n");
	fprintf(fd, "Grace period before enforcing soft limits for %ss:\n",
	    qfextension[quotatype]);
	for (qup = quplist; qup; qup = qup->next) {
		fprintf(fd, "%s: block grace period: %s, ",
		    qup->fsname, cvtstoa(qup->dqblk.dqb_btime));
		fprintf(fd, "file grace period: %s\n",
		    cvtstoa(qup->dqblk.dqb_itime));
	}
	fclose(fd);
	return (1);
}

/*
 * Merge changes of grace times in an ASCII file into a quotause list.
 */
int
readtimes(quplist, infd)
	struct quotause *quplist;
	int infd;
{
	struct quotause *qup;
	FILE *fd;
	int cnt;
	char *cp;
	long litime, lbtime;
	time_t itime, btime, iseconds, bseconds;
	char *fsp, bunits[10], iunits[10], line1[BUFSIZ];

	(void)lseek(infd, (off_t)0, SEEK_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		warnx("Can't re-read temp file!!");
		return (0);
	}
	/*
	 * Discard two title lines, then read lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			warnx("%s: bad format", line1);
			goto bad;
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp,
			    &fsp[strlen(fsp) + 1]);
			goto bad;
		}
		cnt = sscanf(cp,
		    " block grace period: %ld %s file grace period: %ld %s",
		    &lbtime, bunits, &litime, iunits);
		if (cnt != 4) {
			warnx("%s:%s: bad format", fsp, cp);
			goto bad;
		}
		itime = (time_t)litime;
		btime = (time_t)lbtime;
		if (cvtatos(btime, bunits, &bseconds) == 0)
			goto bad;
		if (cvtatos(itime, iunits, &iseconds) == 0) {
bad:
			(void)fclose(fd);
			return (0);
		}
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			qup->dqblk.dqb_btime = bseconds;
			qup->dqblk.dqb_itime = iseconds;
			qup->flags |= FOUND;
			break;
		}
	}
	(void)fclose(fd);
	/*
	 * reset default grace periods for any filesystems
	 * that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_btime = 0;
		qup->dqblk.dqb_itime = 0;
	}
	return (1);
}

/*
 * Convert seconds to ASCII times.
 */
char *
cvtstoa(ltime)
	time_t ltime;
{
	static char buf[20];

	if (ltime % (24 * 60 * 60) == 0) {
		ltime /= 24 * 60 * 60;
		snprintf(buf, sizeof buf, "%ld day%s", (long)ltime,
		    ltime == 1 ? "" : "s");
	} else if (ltime % (60 * 60) == 0) {
		ltime /= 60 * 60;
		sprintf(buf, "%ld hour%s", (long)ltime, ltime == 1 ? "" : "s");
	} else if (ltime % 60 == 0) {
		ltime /= 60;
		sprintf(buf, "%ld minute%s", (long)ltime, ltime == 1 ? "" : "s");
	} else
		sprintf(buf, "%ld second%s", (long)ltime, ltime == 1 ? "" : "s");
	return (buf);
}

/*
 * Convert ASCII input times to seconds.
 */
int
cvtatos(ltime, units, seconds)
	time_t ltime;
	char *units;
	time_t *seconds;
{

	if (memcmp(units, "second", 6) == 0)
		*seconds = ltime;
	else if (memcmp(units, "minute", 6) == 0)
		*seconds = ltime * 60;
	else if (memcmp(units, "hour", 4) == 0)
		*seconds = ltime * 60 * 60;
	else if (memcmp(units, "day", 3) == 0)
		*seconds = ltime * 24 * 60 * 60;
	else {
		printf("%s: bad units, specify %s\n", units,
		    "days, hours, minutes, or seconds");
		return (0);
	}
	return (1);
}

/*
 * Free a list of quotause structures.
 */
void
freeprivs(quplist)
	struct quotause *quplist;
{
	struct quotause *qup, *nextqup;

	for (qup = quplist; qup; qup = nextqup) {
		nextqup = qup->next;
		free(qup);
	}
}

/*
 * Check whether a string is completely composed of digits.
 */
int
alldigits(s)
	const char *s;
{
	int c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while ((c = *s++) != 0);
	return (1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
int
hasquota(fs, type, qfnamep)
	struct fstab *fs;
	int type;
	char **qfnamep;
{
	char *opt;
	char *cp;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	cp = NULL;
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = strchr(opt, '=')) != NULL)
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp) {
		*qfnamep = cp;
		return (1);
	}
	(void) sprintf(buf, "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
	*qfnamep = buf;
	return (1);
}
