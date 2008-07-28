/*	$NetBSD: tunefs.c,v 1.34.2.2 2008/07/28 12:40:06 simonb Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tunefs.c	8.3 (Berkeley) 5/3/95";
#else
__RCSID("$NetBSD: tunefs.c,v 1.34.2.2 2008/07/28 12:40:06 simonb Exp $");
#endif
#endif /* not lint */

/*
 * tunefs: change layout parameters to an existing file system.
 */
#include <sys/param.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_wapbl.h>

#include <machine/bswap.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

/* the optimization warning string template */
#define	OPTWARN	"should optimize for %s with minfree %s %d%%"

union {
	struct	fs sb;
	char pad[MAXBSIZE];
} sbun;
#define	sblock sbun.sb
char buf[MAXBSIZE];

int	fi;
long	dev_bsize = 512;
int	needswap = 0;
int	is_ufs2 = 0;
off_t	sblockloc;

static off_t sblock_try[] = SBLOCKSEARCH;

static	void	bwrite(daddr_t, char *, int, const char *);
static	void	bread(daddr_t, char *, int, const char *);
static	void	change_log_info(long long);
static	void	getsb(struct fs *, const char *);
static	int	openpartition(const char *, int, char *, size_t);
static	void	show_log_info(void);
static	void	usage(void);

int
main(int argc, char *argv[])
{
#define	OPTSTRINGBASE	"AFNe:g:h:l:m:o:"
#ifdef TUNEFS_SOFTDEP
	int		softdep;
#define	OPTSTRING	OPTSTRINGBASE ## "n:"
#else
#define	OPTSTRING	OPTSTRINGBASE
#endif
	int		i, ch, Aflag, Fflag, Nflag, openflags;
	const char	*special, *chg[2];
	char		device[MAXPATHLEN];
	int		maxbpg, minfree, optim;
	int		avgfilesize, avgfpdir;
	long long	logfilesize;

	Aflag = Fflag = Nflag = 0;
	maxbpg = minfree = optim = -1;
	avgfilesize = avgfpdir = -1;
	logfilesize = -1;
#ifdef TUNEFS_SOFTDEP
	softdep = -1;
#endif
	chg[FS_OPTSPACE] = "space";
	chg[FS_OPTTIME] = "time";

	while ((ch = getopt(argc, argv, OPTSTRING)) != -1) {
		switch (ch) {

		case 'A':
			Aflag++;
			break;

		case 'F':
			Fflag++;
			break;

		case 'N':
			Nflag++;
			break;

		case 'e':
			maxbpg = strsuftoll(
			    "maximum blocks per file in a cylinder group",
			    optarg, 1, INT_MAX);
			break;

		case 'g':
			avgfilesize = strsuftoll("average file size", optarg,
			    1, INT_MAX);
			break;

		case 'h':
			avgfpdir = strsuftoll(
			    "expected number of files per directory",
			    optarg, 1, INT_MAX);
			break;

		case 'l':
			logfilesize = strsuftoll("journal log file size",
			    optarg, 0, INT_MAX);
			break;

		case 'm':
			minfree = strsuftoll("minimum percentage of free space",
			    optarg, 0, 99);
			break;

#ifdef TUNEFS_SOFTDEP
		case 'n':
			if (strcmp(optarg, "enable") == 0)
				softdep = 1;
			else if (strcmp(optarg, "disable") == 0)
				softdep = 0;
			else {
				errx(10, "bad soft dependencies "
					"(options are `enable' or `disable')");
			}
			break;
#endif

		case 'o':
			if (strcmp(optarg, chg[FS_OPTSPACE]) == 0)
				optim = FS_OPTSPACE;
			else if (strcmp(optarg, chg[FS_OPTTIME]) == 0)
				optim = FS_OPTTIME;
			else
				errx(10,
				    "bad %s (options are `space' or `time')",
				    "optimization preference");
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind; 
	if (argc != 1)
		usage();

	special = argv[0];
	openflags = Nflag ? O_RDONLY : O_RDWR;
	if (Fflag)
		fi = open(special, openflags);
	else {
		fi = openpartition(special, openflags, device, sizeof(device));
		special = device;
	}
	if (fi == -1)
		err(1, "%s", special);
	getsb(&sblock, special);

#define CHANGEVAL(old, new, type, suffix) do				\
	if ((new) != -1) {						\
		if ((new) == (old))					\
			warnx("%s remains unchanged at %d%s",		\
			    (type), (old), (suffix));			\
		else {							\
			warnx("%s changes from %d%s to %d%s",		\
			    (type), (old), (suffix), (new), (suffix));	\
			(old) = (new);					\
		}							\
	} while (/* CONSTCOND */0)

	warnx("tuning %s", special);
	CHANGEVAL(sblock.fs_maxbpg, maxbpg,
	    "maximum blocks per file in a cylinder group", "");
	CHANGEVAL(sblock.fs_minfree, minfree,
	    "minimum percentage of free space", "%");
	if (minfree != -1) {
		if (minfree >= MINFREE &&
		    sblock.fs_optim == FS_OPTSPACE)
			warnx(OPTWARN, "time", ">=", MINFREE);
		if (minfree < MINFREE &&
		    sblock.fs_optim == FS_OPTTIME)
			warnx(OPTWARN, "space", "<", MINFREE);
	}
#ifdef TUNEFS_SOFTDEP
	if (softdep == 1) {
		sblock.fs_flags |= FS_DOSOFTDEP;
		warnx("soft dependencies set");
	} else if (softdep == 0) {
		sblock.fs_flags &= ~FS_DOSOFTDEP;
		warnx("soft dependencies cleared");
	}
#endif
	if (optim != -1) {
		if (sblock.fs_optim == optim) {
			warnx("%s remains unchanged as %s",
			    "optimization preference",
			    chg[optim]);
		} else {
			warnx("%s changes from %s to %s",
			    "optimization preference",
			    chg[sblock.fs_optim], chg[optim]);
			sblock.fs_optim = optim;
			if (sblock.fs_minfree >= MINFREE &&
			    optim == FS_OPTSPACE)
				warnx(OPTWARN, "time", ">=", MINFREE);
			if (sblock.fs_minfree < MINFREE &&
			    optim == FS_OPTTIME)
				warnx(OPTWARN, "space", "<", MINFREE);
		}
	}
	CHANGEVAL(sblock.fs_avgfilesize, avgfilesize,
	    "average file size", "");
	CHANGEVAL(sblock.fs_avgfpdir, avgfpdir,
	    "expected number of files per directory", "");

	if (logfilesize >= 0)
		change_log_info(logfilesize);

	if (Nflag) {
		fprintf(stdout, "tunefs: current settings of %s\n", special);
		fprintf(stdout, "\tmaximum contiguous block count %d\n",
		    sblock.fs_maxcontig);
		fprintf(stdout,
		    "\tmaximum blocks per file in a cylinder group %d\n",
		    sblock.fs_maxbpg);
		fprintf(stdout, "\tminimum percentage of free space %d%%\n",
		    sblock.fs_minfree);
#ifdef TUNEFS_SOFTDEP
		fprintf(stdout, "\tsoft dependencies: %s\n",
		    (sblock.fs_flags & FS_DOSOFTDEP) ? "on" : "off");
#endif
		fprintf(stdout, "\toptimization preference: %s\n",
		    chg[sblock.fs_optim]);
		fprintf(stdout, "\taverage file size: %d\n",
		    sblock.fs_avgfilesize);
		fprintf(stdout,
		    "\texpected number of files per directory: %d\n",
		    sblock.fs_avgfpdir);
		show_log_info();
		fprintf(stdout, "tunefs: no changes made\n");
		exit(0);
	}

	memcpy(buf, (char *)&sblock, SBLOCKSIZE);
	if (needswap)
		ffs_sb_swap((struct fs*)buf, (struct fs*)buf);
	bwrite(sblockloc, buf, SBLOCKSIZE, special);
	if (Aflag)
		for (i = 0; i < sblock.fs_ncg; i++)
			bwrite(fsbtodb(&sblock, cgsblock(&sblock, i)),
			    buf, SBLOCKSIZE, special);
	close(fi);
	exit(0);
}

static void
show_log_info(void)
{
	const char *loc;
	uint64_t size, blksize;
	int print;

	switch (sblock.fs_journal_location) {
	case UFS_WAPBL_JOURNALLOC_NONE:
		print = blksize = 0;
		/* nothing */
		break;
	case UFS_WAPBL_JOURNALLOC_END_PARTITION:
		loc = "end of partition";
		size = sblock.fs_journallocs[UFS_WAPBL_EPART_COUNT];
		blksize = sblock.fs_journallocs[UFS_WAPBL_EPART_BLKSZ];
		print = 1;
		break;
	case UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM:
		loc = "in filesystem";
		size = sblock.fs_journallocs[UFS_WAPBL_INFS_COUNT];
		blksize = sblock.fs_journallocs[UFS_WAPBL_INFS_BLKSZ];
		print = 1;
		break;
	default:
		loc = "unknown";
		size = blksize = 0;
		print = 1;
		break;
	}

	if (print) {
		fprintf(stdout, "\tjournal log file location: %s\n", loc);
		fprintf(stdout, "\tjournal log file size: %" PRIu64 "\n",
		    size * blksize);
		fprintf(stdout, "\tjournal log flags:");
		if (sblock.fs_journal_flags & UFS_WAPBL_FLAGS_CREATE_LOG)
			fprintf(stdout, " clear-log");
		if (sblock.fs_journal_flags & UFS_WAPBL_FLAGS_CLEAR_LOG)
			fprintf(stdout, " clear-log");
		fprintf(stdout, "\n");
	}
}

static void
change_log_info(long long logfilesize)
{
	/*
	 * NOTES:
	 *  - only operate on in-filesystem log sizes
	 *  - can't change size of existing log
	 *  - if current is same, no action
	 *  - if current is zero and new is non-zero, set flag to create log
	 *    on next mount
	 *  - if current is non-zero and new is zero, set flag to clear log
	 *    on next mount
	 */
	int in_fs_log;
	uint64_t old_size;

	old_size = 0;
	switch (sblock.fs_journal_location) {
	case UFS_WAPBL_JOURNALLOC_END_PARTITION:
		in_fs_log = 0;
		old_size = sblock.fs_journallocs[UFS_WAPBL_EPART_COUNT] *
		    sblock.fs_journallocs[UFS_WAPBL_EPART_BLKSZ];
		break;

	case UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM:
		in_fs_log = 1;
		old_size = sblock.fs_journallocs[UFS_WAPBL_INFS_COUNT] *
		    sblock.fs_journallocs[UFS_WAPBL_INFS_BLKSZ];
		break;

	case UFS_WAPBL_JOURNALLOC_NONE:
	default:
		in_fs_log = 0;
		old_size = 0;
		break;
	}

	if (!in_fs_log)
		errx(1, "Can't change size of non-in-filesystem log");

	if (old_size == logfilesize && logfilesize > 0) {
		/* no action */
		warnx("log file size remains unchanged at %lld", logfilesize);
		return;
	}

	if (logfilesize == 0) {
		/*
		 * Don't clear out the locators - the kernel might need
		 * these to find the log!  Just set the "clear the log"
		 * flag and let the kernel do the rest.
		 */
		sblock.fs_journal_flags |= UFS_WAPBL_FLAGS_CLEAR_LOG;
		sblock.fs_journal_flags &= ~UFS_WAPBL_FLAGS_CREATE_LOG;
		warnx("log file size cleared from %" PRIu64 "", old_size);
		return;
	}

	if (old_size == 0) {
		/* create new log of desired size next mount */
		sblock.fs_journal_location = UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM;
		sblock.fs_journallocs[UFS_WAPBL_INFS_ADDR] = 0;
		sblock.fs_journallocs[UFS_WAPBL_INFS_COUNT] = logfilesize;
		sblock.fs_journallocs[UFS_WAPBL_INFS_BLKSZ] = 0;
		sblock.fs_journallocs[UFS_WAPBL_INFS_INO] = 0;
		sblock.fs_journal_flags |= UFS_WAPBL_FLAGS_CREATE_LOG;
		sblock.fs_journal_flags &= ~UFS_WAPBL_FLAGS_CLEAR_LOG;
		warnx("log file size set to %lld", logfilesize);
	} else {
		errx(1, "Can't change existing log size from %" PRIu64 " to %d",
		     old_size, logfilesize);
	} 
}

static void
usage(void)
{

	fprintf(stderr, "usage: tunefs [-AFN] tuneup-options special-device\n");
	fprintf(stderr, "where tuneup-options are:\n");
	fprintf(stderr, "\t-e maximum blocks per file in a cylinder group\n");
	fprintf(stderr, "\t-g average file size\n");
	fprintf(stderr, "\t-h expected number of files per directory\n");
	fprintf(stderr, "\t-l journal log file size (`0' to clear journal)\n");
	fprintf(stderr, "\t-m minimum percentage of free space\n");
#ifdef TUNEFS_SOFTDEP
	fprintf(stderr, "\t-n soft dependencies (`enable' or `disable')\n");
#endif
	fprintf(stderr, "\t-o optimization preference (`space' or `time')\n");
	exit(2);
}

static void
getsb(struct fs *fs, const char *file)
{
	int i;

	for (i = 0; ; i++) {
		if (sblock_try[i] == -1)
			errx(5, "cannot find filesystem superblock");
		bread(sblock_try[i] / dev_bsize, (char *)fs, SBLOCKSIZE, file);
		switch(fs->fs_magic) {
		case FS_UFS2_MAGIC:
			is_ufs2 = 1;
			/*FALLTHROUGH*/
		case FS_UFS1_MAGIC:
			break;
		case FS_UFS2_MAGIC_SWAPPED:
			is_ufs2 = 1;
			/*FALLTHROUGH*/
		case FS_UFS1_MAGIC_SWAPPED:
			warnx("%s: swapping byte order", file);
			needswap = 1;
			ffs_sb_swap(fs, fs);
			break;
		default:
			continue;
		}
		if (!is_ufs2 && sblock_try[i] == SBLOCK_UFS2)
			continue;
		if ((is_ufs2 || fs->fs_old_flags & FS_FLAGS_UPDATED)
		    && fs->fs_sblockloc != sblock_try[i])
			continue;
		break;
	}

	dev_bsize = fs->fs_fsize / fsbtodb(fs, 1);
	sblockloc = sblock_try[i] / dev_bsize;
}

static void
bwrite(daddr_t blk, char *buffer, int size, const char *file)
{
	off_t	offset;

	offset = (off_t)blk * dev_bsize;
	if (lseek(fi, offset, SEEK_SET) == -1)
		err(6, "%s: seeking to %lld", file, (long long)offset);
	if (write(fi, buffer, size) != size)
		err(7, "%s: writing %d bytes", file, size);
}

static void
bread(daddr_t blk, char *buffer, int cnt, const char *file)
{
	off_t	offset;
	int	i;

	offset = (off_t)blk * dev_bsize;
	if (lseek(fi, offset, SEEK_SET) == -1)
		err(4, "%s: seeking to %lld", file, (long long)offset);
	if ((i = read(fi, buffer, cnt)) != cnt)
		errx(5, "%s: short read", file);
}

static int
openpartition(const char *name, int flags, char *device, size_t devicelen)
{
	char		rawspec[MAXPATHLEN], *p;
	struct fstab	*fs;
	int		fd, oerrno;

	fs = getfsfile(name);
	if (fs) {
		if ((p = strrchr(fs->fs_spec, '/')) != NULL) {
			snprintf(rawspec, sizeof(rawspec), "%.*s/r%s",
			    (int)(p - fs->fs_spec), fs->fs_spec, p + 1);
			name = rawspec;
		} else
			name = fs->fs_spec;
	}
	fd = opendisk(name, flags, device, devicelen, 0);
	if (fd == -1 && errno == ENOENT) {
		oerrno = errno;
		strlcpy(device, name, devicelen);
		errno = oerrno;
	}
	return (fd);
}
