/*	$NetBSD: options.c,v 1.25.2.2 1999/11/07 15:57:32 mycroft Exp $	*/

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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
#if 0
static char sccsid[] = "@(#)options.c	8.2 (Berkeley) 4/18/94";
#else
__RCSID("$NetBSD: options.c,v 1.25.2.2 1999/11/07 15:57:32 mycroft Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mtio.h>
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include "pax.h"
#include "options.h"
#include "cpio.h"
#include "tar.h"
#include "extern.h"

/*
 * Routines which handle command line options
 */

int cpio_mode;			/* set if we are in cpio mode */

static int nopids;		/* tar mode: suppress "pids" for -p option */
static char *flgch = FLGCH;	/* list of all possible flags (pax) */
static OPLIST *ophead = NULL;	/* head for format specific options -x */
static OPLIST *optail = NULL;	/* option tail */
static char *firstminusC;	/* first -C argument encountered. */

static int no_op __P((void));
static void printflg __P((unsigned int));
static int c_frmt __P((const void *, const void *));
static off_t str_offt __P((char *));
static void pax_options __P((int, char **));
static void pax_usage __P((void));
static void tar_options __P((int, char **));
static void tar_usage __P((void));
static void cpio_options __P((int, char **));
static void cpio_usage __P((void));

static void checkpositionalminusC __P((char ***, int (*)(char *, int)));

#define GZIP_CMD	"gzip"		/* command to run as gzip */
#define COMPRESS_CMD	"compress"	/* command to run as compress */

/*
 *	Format specific routine table - MUST BE IN SORTED ORDER BY NAME
 *	(see pax.h for description of each function)
 *
 * 	name, blksz, hdsz, udev, hlk, blkagn, inhead, id, st_read,
 *	read, end_read, st_write, write, end_write, trail,
 *	rd_data, wr_data, options
 */

FSUB fsub[] = {
/* 0: OLD BINARY CPIO */
	{ "bcpio", 5120, sizeof(HD_BCPIO), 1, 0, 0, 1, bcpio_id, cpio_strd,
	bcpio_rd, bcpio_endrd, cpio_stwr, bcpio_wr, cpio_endwr, NULL,
	cpio_subtrail, rd_wrfile, wr_rdfile, bad_opt },

/* 1: OLD OCTAL CHARACTER CPIO */
	{ "cpio", 5120, sizeof(HD_CPIO), 1, 0, 0, 1, cpio_id, cpio_strd,
	cpio_rd, cpio_endrd, cpio_stwr, cpio_wr, cpio_endwr, NULL,
	cpio_subtrail, rd_wrfile, wr_rdfile, bad_opt },

/* 2: SVR4 HEX CPIO */
	{ "sv4cpio", 5120, sizeof(HD_VCPIO), 1, 0, 0, 1, vcpio_id, cpio_strd,
	vcpio_rd, vcpio_endrd, cpio_stwr, vcpio_wr, cpio_endwr, NULL,
	cpio_subtrail, rd_wrfile, wr_rdfile, bad_opt },

/* 3: SVR4 HEX CPIO WITH CRC */
	{ "sv4crc", 5120, sizeof(HD_VCPIO), 1, 0, 0, 1, crc_id, crc_strd,
	vcpio_rd, vcpio_endrd, crc_stwr, vcpio_wr, cpio_endwr, NULL,
	cpio_subtrail, rd_wrfile, wr_rdfile, bad_opt },

/* 4: OLD TAR */
	{ "tar", 10240, BLKMULT, 0, 1, BLKMULT, 0, tar_id, no_op,
	tar_rd, tar_endrd, no_op, tar_wr, tar_endwr, tar_trail,
	NULL, rd_wrfile, wr_rdfile, tar_opt },

/* 5: POSIX USTAR */
	{ "ustar", 10240, BLKMULT, 0, 1, BLKMULT, 0, ustar_id, ustar_strd,
	ustar_rd, tar_endrd, ustar_stwr, ustar_wr, tar_endwr, tar_trail,
	NULL, rd_wrfile, wr_rdfile, bad_opt }
};
#define F_BCPIO		0	/* old binary cpio format */
#define F_CPIO		1	/* old octal character cpio format */
#define F_SV4CPIO	2	/* SVR4 hex cpio format */
#define F_SV4CRC	3	/* SVR4 hex with crc cpio format */
#define F_TAR		4	/* old V7 UNIX tar format */
#define F_USTAR		5	/* ustar format */
#define DEFLT		F_USTAR	/* default write format from list above */

/*
 * ford is the archive search order used by get_arc() to determine what kind
 * of archive we are dealing with. This helps to properly id  archive formats
 * some formats may be subsets of others....
 */
int ford[] = {F_USTAR, F_TAR, F_SV4CRC, F_SV4CPIO, F_CPIO, F_BCPIO, -1};

/*
 * options()
 *	figure out if we are pax, tar or cpio. Call the appropriate options
 *	parser
 */

#if __STDC__
void
options(int argc, char **argv)
#else
void
options(argc, argv)
	int argc;
	char **argv;
#endif
{

	/*
	 * Are we acting like pax, tar or cpio (based on argv[0])
	 */
	if ((argv0 = strrchr(argv[0], '/')) != NULL)
		argv0++;
	else
		argv0 = argv[0];

	if (strcmp(NM_TAR, argv0) == 0)
		tar_options(argc, argv);
	else if (strcmp(NM_CPIO, argv0) == 0)
		cpio_options(argc, argv);
	else {
		argv0 = NM_PAX;
		pax_options(argc, argv);
	}
}

/*
 * pax_options()
 *	look at the user specified flags. set globals as required and check if
 *	the user specified a legal set of flags. If not, complain and exit
 */

#if __STDC__
static void
pax_options(int argc, char **argv)
#else
static void
pax_options(argc, argv)
	int argc;
	char **argv;
#endif
{
	int c;
	int i;
	unsigned int flg = 0;
	unsigned int bflg = 0;
	char *pt;
        FSUB tmp;
	extern char *optarg;
	extern int optind;

	/*
	 * process option flags
	 */
	while ((c=getopt(argc,argv,"ab:cdf:iklno:p:rs:tuvwx:zAB:DE:G:HLPT:U:XYZ"))
	    != -1) {
		switch (c) {
		case 'a':
			/*
			 * append
			 */
			flg |= AF;
			break;
		case 'b':
			/*
			 * specify blocksize
			 */
			flg |= BF;
			if ((wrblksz = (int)str_offt(optarg)) <= 0) {
				tty_warn(1, "Invalid block size %s", optarg);
				pax_usage();
			}
			break;
		case 'c':
			/*
			 * inverse match on patterns
			 */
			cflag = 1;
			flg |= CF;
			break;
		case 'd':
			/*
			 * match only dir on extract, not the subtree at dir
			 */
			dflag = 1;
			flg |= DF;
			break;
		case 'f':
			/*
			 * filename where the archive is stored
			 */
			arcname = optarg;
			flg |= FF;
			break;
		case 'i':
			/*
			 * interactive file rename
			 */
			iflag = 1;
			flg |= IF;
			break;
		case 'k':
			/*
			 * do not clobber files that exist
			 */
			kflag = 1;
			flg |= KF;
			break;
		case 'l':
			/*
			 * try to link src to dest with copy (-rw)
			 */
			lflag = 1;
			flg |= LF;
			break;
		case 'n':
			/*
			 * select first match for a pattern only
			 */
			nflag = 1;
			flg |= NF;
			break;
		case 'o':
			/*
			 * pass format specific options
			 */
			flg |= OF;
			if (opt_add(optarg) < 0)
				pax_usage();
			break;
		case 'p':
			/*
			 * specify file characteristic options
			 */
			for (pt = optarg; *pt != '\0'; ++pt) {
				switch(*pt) {
				case 'a':
					/*
					 * do not preserve access time
					 */
					patime = 0;
					break;
				case 'e':
					/*
					 * preserve user id, group id, file
					 * mode, access/modification times
					 * and file flags.
					 */
					pids = 1;
					pmode = 1;
					patime = 1;
					pmtime = 1;
					pfflags = 1;
					break;
#if 0
				case 'f':
					/*
					 * do not preserve file flags
					 */
					pfflags = 0;
					break;
#endif
				case 'm':
					/*
					 * do not preserve modification time
					 */
					pmtime = 0;
					break;
				case 'o':
					/*
					 * preserve uid/gid
					 */
					pids = 1;
					break;
				case 'p':
					/*
					 * preserver file mode bits
					 */
					pmode = 1;
					break;
				default:
					tty_warn(1,
					    "Invalid -p string: %c", *pt);
					pax_usage();
					break;
				}
			}
			flg |= PF;
			break;
		case 'r':
			/*
			 * read the archive
			 */
			flg |= RF;
			break;
		case 's':
			/*
			 * file name substitution name pattern
			 */
			if (rep_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= SF;
			break;
		case 't':
			/*
			 * preserve access time on filesystem nodes we read
			 */
			tflag = 1;
			flg |= TF;
			break;
		case 'u':
			/*
			 * ignore those older files
			 */
			uflag = 1;
			flg |= UF;
			break;
		case 'v':
			/*
			 * verbose operation mode
			 */
			vflag = 1;
			flg |= VF;
			break;
		case 'w':
			/*
			 * write an archive
			 */
			flg |= WF;
			break;
		case 'x':
			/*
			 * specify an archive format on write
			 */
			tmp.name = optarg;
			frmt = (FSUB *)bsearch((void *)&tmp, (void *)fsub,
			    sizeof(fsub)/sizeof(FSUB), sizeof(FSUB), c_frmt);
			if (frmt != NULL) {
				flg |= XF;
				break;
			}
			tty_warn(1, "Unknown -x format: %s", optarg);
			(void)fputs("pax: Known -x formats are:", stderr);
			for (i = 0; i < (sizeof(fsub)/sizeof(FSUB)); ++i)
				(void)fprintf(stderr, " %s", fsub[i].name);
			(void)fputs("\n\n", stderr);
			pax_usage();
			break;
		case 'z':
			/*
			 * use gzip.  Non standard option.
			 */
			zflag = 1;
			gzip_program = GZIP_CMD;
			break;
		case 'A':
			Aflag = 1;
			flg |= CAF;
			break;
		case 'B':
			/*
			 * non-standard option on number of bytes written on a
			 * single archive volume.
			 */
			if ((wrlimit = str_offt(optarg)) <= 0) {
				tty_warn(1, "Invalid write limit %s", optarg);
				pax_usage();
			}
			if (wrlimit % BLKMULT) {
				tty_warn(1,
				    "Write limit is not a %d byte multiple",
				    BLKMULT);
				pax_usage();
			}
			flg |= CBF;
			break;
		case 'D':
			/*
			 * On extraction check file inode change time before the
			 * modification of the file name. Non standard option.
			 */
			Dflag = 1;
			flg |= CDF;
			break;
		case 'E':
			/*
			 * non-standard limit on read faults
			 * 0 indicates stop after first error, values
			 * indicate a limit, "NONE" try forever
			 */
			flg |= CEF;
			if (strcmp(NONE, optarg) == 0)
				maxflt = -1;
			else if ((maxflt = atoi(optarg)) < 0) {
				tty_warn(1,
				    "Error count value must be positive");
				pax_usage();
			}
			break;
		case 'G':
			/*
			 * non-standard option for selecting files within an
			 * archive by group (gid or name)
			 */
			if (grp_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= CGF;
			break;
		case 'H':
			/*
			 * follow command line symlinks only
			 */
			Hflag = 1;
			flg |= CHF;
			break;
		case 'L':
			/*
			 * follow symlinks
			 */
			Lflag = 1;
			flg |= CLF;
			break;
		case 'P':
			/*
			 * do NOT follow symlinks (default)
			 */
			Lflag = 0;
			flg |= CPF;
			break;
		case 'T':
			/*
			 * non-standard option for selecting files within an
			 * archive by modification time range (lower,upper)
			 */
			if (trng_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= CTF;
			break;
		case 'U':
			/*
			 * non-standard option for selecting files within an
			 * archive by user (uid or name)
			 */
			if (usr_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= CUF;
			break;
		case 'X':
			/*
			 * do not pass over mount points in the file system
			 */
			Xflag = 1;
			flg |= CXF;
			break;
		case 'Y':
			/*
			 * On extraction check file inode change time after the
			 * modification of the file name. Non standard option.
			 */
			Yflag = 1;
			flg |= CYF;
			break;
		case 'Z':
			/*
			 * On extraction check modification time after the
			 * modification of the file name. Non standard option.
			 */
			Zflag = 1;
			flg |= CZF;
			break;
		case '?':
		default:
			pax_usage();
			break;
		}
	}

	/*
	 * figure out the operation mode of pax read,write,extract,copy,append
	 * or list. check that we have not been given a bogus set of flags
	 * for the operation mode.
	 */
	if (ISLIST(flg)) {
		act = LIST;
		bflg = flg & BDLIST;
	} else if (ISEXTRACT(flg)) {
		act = EXTRACT;
		bflg = flg & BDEXTR;
	} else if (ISARCHIVE(flg)) {
		act = ARCHIVE;
		bflg = flg & BDARCH;
	} else if (ISAPPND(flg)) {
		act = APPND;
		bflg = flg & BDARCH;
	} else if (ISCOPY(flg)) {
		act = COPY;
		bflg = flg & BDCOPY;
	} else
		pax_usage();
	if (bflg) {
		printflg(flg);
		pax_usage();
	}

	/*
	 * if we are writing (ARCHIVE) we use the default format if the user
	 * did not specify a format. when we write during an APPEND, we will
	 * adopt the format of the existing archive if none was supplied.
	 */
	if (!(flg & XF) && (act == ARCHIVE))
		frmt = &(fsub[DEFLT]);

	/*
	 * process the args as they are interpreted by the operation mode
	 */
	switch (act) {
	case LIST:
	case EXTRACT:
		for (; optind < argc; optind++)
			if (pat_add(argv[optind], 0) < 0)
				pax_usage();
		break;
	case COPY:
		if (optind >= argc) {
			tty_warn(0, "Destination directory was not supplied");
			pax_usage();
		}
		--argc;
		dirptr = argv[argc];
		/* FALLTHROUGH */
	case ARCHIVE:
	case APPND:
		for (; optind < argc; optind++)
			if (ftree_add(argv[optind], 0) < 0)
				pax_usage();
		/*
		 * no read errors allowed on updates/append operation!
		 */
		maxflt = 0;
		break;
	}
}


/*
 * tar_options()
 *	look at the user specified flags. set globals as required and check if
 *	the user specified a legal set of flags. If not, complain and exit
 */

#if __STDC__
static void
tar_options(int argc, char **argv)
#else
static void
tar_options(argc, argv)
	int argc;
	char **argv;
#endif
{
	int c;
	int fstdin = 0;

	/*
	 * process option flags
	 */
	while ((c = getoldopt(argc, argv, "b:cef:hlmoprutvwxzBC:LPX:Z014578")) 
	    != -1)  {
		switch(c) {
		case 'b':
			/*
			 * specify blocksize
			 */
			if ((wrblksz = (int)str_offt(optarg)) <= 0) {
				tty_warn(1, "Invalid block size %s", optarg);
				tar_usage();
			}
			break;
		case 'c':
			/*
			 * create an archive
			 */
			act = ARCHIVE;
			break;
		case 'C':
			/*
			 * chdir here before extracting.
			 * do so lazily, in case it's a list
			 */
			firstminusC = optarg;
			break;
		case 'e':
			/*
			 * stop after first error
			 */
			maxflt = 0;
			break;
		case 'f':
			/*
			 * filename where the archive is stored
			 */
			if ((optarg[0] == '-') && (optarg[1]== '\0')) {
				/*
				 * treat a - as stdin
				 */
				fstdin = 1;
				arcname = (char *)0;
				break;
			}
			fstdin = 0;
			arcname = optarg;
			break;
		case 'h':
			/*
			 * follow command line symlinks only
			 */
			Hflag = 1;
			break;
		case 'l':
			/*
			 * do not pass over mount points in the file system
			 */
			Xflag = 1;
			break;
		case 'm':
			/*
			 * do not preserve modification time
			 */
			pmtime = 0;
			break;
		case 'o':
			/*
			 * This option does several things based on whether
			 * this is a create or extract operation.
			 */
			if (act == ARCHIVE) {
				/* 4.2BSD: don't add directory entries. */
				if (opt_add("write_opt=nodir") < 0)
					tar_usage();

				/* GNU tar: write V7 format archives. */
				frmt = &(fsub[F_TAR]);
			} else {
				/* SUS: don't preserve owner/group. */
				pids = 0;
				nopids = 1;
			}
			break;
		case 'p':
			/*
			 * preserve user id, group id, file
			 * mode, access/modification times
			 */
			if (!nopids)
				pids = 1;
			pmode = 1;
			patime = 1;
			pmtime = 1;
			break;
		case 'r':
		case 'u':
			/*
			 * append to the archive
			 */
			act = APPND;
			break;
		case 't':
			/*
			 * list contents of the tape
			 */
			act = LIST;
			break;
		case 'v':
			/*
			 * verbose operation mode
			 */
			vflag = 1;
			break;
		case 'w':
			/*
			 * interactive file rename
			 */
			iflag = 1;
			break;
		case 'x':
			/*
			 * write an archive
			 */
			act = EXTRACT;
			break;
		case 'z':
			/*
			 * use gzip.  Non standard option.
			 */
			zflag = 1;
			gzip_program = GZIP_CMD;
			break;
		case 'B':
			/*
			 * Nothing to do here, this is pax default
			 */
			break;
		case 'L':
			/*
			 * follow symlinks
			 */
			Lflag = 1;
			break;
		case 'P':
			Aflag = 1;
			break;
		case 'X':
			/*
			 * GNU tar compat: exclude the files listed in optarg
			 */
			if (tar_gnutar_X_compat(optarg) != 0)
				tar_usage();
			break;
		case 'Z':
			/*
			 * use compress.
			 */
			zflag = 1;
			gzip_program = COMPRESS_CMD;
			break;
		case '0':
			arcname = DEV_0;
			break;
		case '1':
			arcname = DEV_1;
			break;
		case '4':
			arcname = DEV_4;
			break;
		case '5':
			arcname = DEV_5;
			break;
		case '7':
			arcname = DEV_7;
			break;
		case '8':
			arcname = DEV_8;
			break;
		default:
			tar_usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (firstminusC && (opt_chdir(firstminusC) < 0))
		tty_warn(1, "can't remember -C directory");

	/*
	 * if we are writing (ARCHIVE) specify tar, otherwise run like pax
	 */
	if (act == ARCHIVE && frmt == NULL)
		frmt = &(fsub[F_USTAR]);

	/*
	 * process the args as they are interpreted by the operation mode
	 */
	switch (act) {
	case LIST:
	default:
		while (*argv != (char *)NULL)
			if (pat_add(*argv++, 0) < 0)
				tar_usage();
		break;
	case EXTRACT:
		checkpositionalminusC(&argv, pat_add);
		break;
	case ARCHIVE:
	case APPND:
		checkpositionalminusC(&argv, ftree_add);
		/*
		 * no read errors allowed on updates/append operation!
		 */
		maxflt = 0;
		break;
	}
	if (!fstdin && ((arcname == (char *)NULL) || (*arcname == '\0'))) {
		arcname = getenv("TAPE");
		if ((arcname == (char *)NULL) || (*arcname == '\0'))
			arcname = DEV_8;
	}
}

/*
 * cpio_options()
 *	look at the user specified flags. set globals as required and check if
 *	the user specified a legal set of flags. If not, complain and exit
 */

#if __STDC__
static void
cpio_options(int argc, char **argv)
#else
static void
cpio_options(argc, argv)
	int argc;
	char **argv;
#endif
{
        FSUB tmp;
	unsigned int flg = 0;
	unsigned int bflg = 0;
	int c, i;

	cpio_mode = uflag = 1;
	/*
	 * process option flags
	 */
	while ((c = getoldopt(argc, argv, "ABC:E:H:I:LM:O:R:SVabcdfiklmoprstuv"))
	    != -1)  {
		switch(c) {
		case 'A':
			/*
			 * append to an archive
			 */
			act = APPND;
			flg |= AF;
			break;
		case 'B':
			/*
			 * set blocksize to 5120
			 */
			blksz = 5120;
			break;
		case 'C':
			/*
			 * specify blocksize
			 */
			if ((blksz = (int)str_offt(optarg)) <= 0) {
				tty_warn(1, "Invalid block size %s", optarg);
				tar_usage();
			}
			break;
#ifdef notyet
		case 'E':
			arg = optarg;
			break;
#endif
		case 'H':
			/*
			 * specify an archive format on write
			 */
			tmp.name = optarg;
			frmt = (FSUB *)bsearch((void *)&tmp, (void *)fsub,
			    sizeof(fsub)/sizeof(FSUB), sizeof(FSUB), c_frmt);
			if (frmt != NULL) {
				flg |= XF;
				break;
			}
			tty_warn(1, "Unknown -H format: %s", optarg);
			(void)fputs("cpio: Known -H formats are:", stderr);
			for (i = 0; i < (sizeof(fsub)/sizeof(FSUB)); ++i)
				(void)fprintf(stderr, " %s", fsub[i].name);
			(void)fputs("\n\n", stderr);
			tar_usage();
			break;
		case 'I':
		case 'O':
			/*
			 * filename where the archive is stored
			 */
			if ((optarg[0] == '-') && (optarg[1]== '\0')) {
				/*
				 * treat a - as stdin
				 */
				arcname = (char *)0;
				break;
			}
			arcname = optarg;
			break;
		case 'L':
			/*
			 * follow symlinks
			 */
			Lflag = 1;
			flg |= CLF;
			break;
#ifdef notyet
		case 'M':
			arg = optarg;
			break;
		case 'R':
			arg = optarg;
			break;
#endif
		case 'S':
			cpio_swp_head = 1;
			break;
#ifdef notyet
		case 'V':
			break;
#endif
		case 'a':
			/*
			 * preserve access time on filesystem nodes we read
			 */
			tflag = 1;
			flg |= TF;
			break;
#ifdef notyet
		case 'b':
			break;
#endif
		case 'c':
			frmt = &fsub[F_SV4CPIO];
			break;
		case 'd':
			/*
			 * pax does this by default ..
			 */
			flg |= RF;
			break;
		case 'f':
			/*
			 * inverse match on patterns
			 */
			cflag = 1;
			flg |= CF;
			break;
		case 'i':
			/*
			 * read the archive
			 */
			flg |= RF;
			break;
#ifdef notyet
		case 'k':
			break;
#endif
		case 'l':
			/*
			 * try to link src to dest with copy (-rw)
			 */
			lflag = 1;
			flg |= LF;
			break;
		case 'm':
			/*
			 * preserve mtime
			 */
			flg |= PF;
			pmtime = 1;
			break;
		case 'o':
			/*
			 * write an archive
			 */
			flg |= WF;
			break;
		case 'p':
			/*
			 * cpio -p is like pax -rw
			 */
			flg |= RF | WF;
			break;
		case 'r':
			/*
			 * interactive file rename
			 */
			iflag = 1;
			flg |= IF;
			break;
#ifdef notyet
		case 's':
			break;
#endif
		case 't':
			act = LIST;
			break;
		case 'u':
			/*
			 * don't ignore those older files
			 */
			uflag = 0;
			flg |= UF;
			break;
		case 'v':
			/*
			 * verbose operation mode
			 */
			vflag = 1;
			flg |= VF;
			break;
		default:
			cpio_usage();
			break;
		}
	}

	/*
	 * figure out the operation mode of cpio. check that we have not been
	 * given a bogus set of flags for the operation mode.
	 */
	if (ISLIST(flg)) {
		act = LIST;
		bflg = flg & BDLIST;
	} else if (ISEXTRACT(flg)) {
		act = EXTRACT;
		bflg = flg & BDEXTR;
	} else if (ISARCHIVE(flg)) {
		act = ARCHIVE;
		bflg = flg & BDARCH;
	} else if (ISAPPND(flg)) {
		act = APPND;
		bflg = flg & BDARCH;
	} else if (ISCOPY(flg)) {
		act = COPY;
		bflg = flg & BDCOPY;
	} else
		cpio_usage();
	if (bflg) {
		cpio_usage();
	}

	/*
	 * if we are writing (ARCHIVE) we use the default format if the user
	 * did not specify a format. when we write during an APPEND, we will
	 * adopt the format of the existing archive if none was supplied.
	 */
	if (!(flg & XF) && (act == ARCHIVE))
		frmt = &(fsub[F_BCPIO]);

	/*
	 * process the args as they are interpreted by the operation mode
	 */
	switch (act) {
	case LIST:
	case EXTRACT:
		for (; optind < argc; optind++)
			if (pat_add(argv[optind], 0) < 0)
				cpio_usage();
		break;
	case COPY:
		if (optind >= argc) {
			tty_warn(0, "Destination directory was not supplied");
			cpio_usage();
		}
		--argc;
		dirptr = argv[argc];
		/* FALLTHROUGH */
	case ARCHIVE:
	case APPND:
		for (; optind < argc; optind++)
			if (ftree_add(argv[optind], 0) < 0)
				cpio_usage();
		/*
		 * no read errors allowed on updates/append operation!
		 */
		maxflt = 0;
		break;
	}
}

/*
 * printflg()
 *	print out those invalid flag sets found to the user
 */

#if __STDC__
static void
printflg(unsigned int flg)
#else
static void
printflg(flg)
	unsigned int flg;
#endif
{
	int nxt;
	int pos = 0;

	(void)fprintf(stderr,"%s: Invalid combination of options:", argv0);
	while ((nxt = ffs(flg)) != 0) {
		flg = flg >> nxt;
		pos += nxt;
		(void)fprintf(stderr, " -%c", flgch[pos-1]);
	}
	(void)putc('\n', stderr);
}

/*
 * c_frmt()
 *	comparison routine used by bsearch to find the format specified
 *	by the user
 */

#if __STDC__
static int
c_frmt(const void *a, const void *b)
#else
static int
c_frmt(a, b)
        void *a;
        void *b;
#endif
{
        return(strcmp(((FSUB *)a)->name, ((FSUB *)b)->name));
}

/*
 * opt_next()
 *	called by format specific options routines to get each format specific
 *	flag and value specified with -o
 * Return:
 *	pointer to next OPLIST entry or NULL (end of list).
 */

#if __STDC__
OPLIST *
opt_next(void)
#else
OPLIST *
opt_next()
#endif
{
	OPLIST *opt;

	if ((opt = ophead) != NULL)
		ophead = ophead->fow;
	return(opt);
}

/*
 * bad_opt()
 *	generic routine used to complain about a format specific options
 *	when the format does not support options.
 */

#if __STDC__
int
bad_opt(void)
#else
int
bad_opt()
#endif
{
	OPLIST *opt;

	if (ophead == NULL)
		return(0);
	/*
	 * print all we were given
	 */
	tty_warn(1,"These format options are not supported");
	while ((opt = opt_next()) != NULL)
		(void)fprintf(stderr, "\t%s = %s\n", opt->name, opt->value);
	pax_usage();
	return(0);
}

/*
 * opt_add()
 *	breaks the value supplied to -o into a option name and value. options
 *	are given to -o in the form -o name-value,name=value
 *	mulltiple -o may be specified.
 * Return:
 *	0 if format in name=value format, -1 if -o is passed junk
 */

#if __STDC__
int
opt_add(const char *str)
#else
int
opt_add(str)
	const char *str;
#endif
{
	OPLIST *opt;
	char *frpt;
	char *pt;
	char *endpt;

	if ((str == NULL) || (*str == '\0')) {
		tty_warn(0, "Invalid option name");
		return(-1);
	}
	frpt = endpt = strdup(str);

	/*
	 * break into name and values pieces and stuff each one into a
	 * OPLIST structure. When we know the format, the format specific
	 * option function will go through this list
	 */
	while ((frpt != NULL) && (*frpt != '\0')) {
		if ((endpt = strchr(frpt, ',')) != NULL)
			*endpt = '\0';
		if ((pt = strchr(frpt, '=')) == NULL) {
			tty_warn(0, "Invalid options format");
			return(-1);
		}
		if ((opt = (OPLIST *)malloc(sizeof(OPLIST))) == NULL) {
			tty_warn(0, "Unable to allocate space for option list");
			return(-1);
		}
		*pt++ = '\0';
		opt->name = frpt;
		opt->value = pt;
		opt->fow = NULL;
		if (endpt != NULL)
			frpt = endpt + 1;
		else
			frpt = NULL;
		if (ophead == NULL) {
			optail = ophead = opt;
			continue;
		}
		optail->fow = opt;
		optail = opt;
	}
	return(0);
}

/*
 * str_offt()
 *	Convert an expression of the following forms to an off_t > 0.
 * 	1) A positive decimal number.
 *	2) A positive decimal number followed by a b (mult by 512).
 *	3) A positive decimal number followed by a k (mult by 1024).
 *	4) A positive decimal number followed by a m (mult by 512).
 *	5) A positive decimal number followed by a w (mult by sizeof int)
 *	6) Two or more positive decimal numbers (with/without k,b or w).
 *	   seperated by x (also * for backwards compatibility), specifying
 *	   the product of the indicated values.
 * Return:
 *	0 for an error, a positive value o.w.
 */

#if __STDC__
static off_t
str_offt(char *val)
#else
static off_t
str_offt(val)
	char *val;
#endif
{
	char *expr;
	off_t num, t;

#	ifdef NET2_STAT
	num = strtol(val, &expr, 0);
	if ((num == LONG_MAX) || (num <= 0) || (expr == val))
#	else
	num = strtoq(val, &expr, 0);
	if ((num == QUAD_MAX) || (num <= 0) || (expr == val))
#	endif
		return(0);

	switch(*expr) {
	case 'b':
		t = num;
		num *= 512;
		if (t > num)
			return(0);
		++expr;
		break;
	case 'k':
		t = num;
		num *= 1024;
		if (t > num)
			return(0);
		++expr;
		break;
	case 'm':
		t = num;
		num *= 1048576;
		if (t > num)
			return(0);
		++expr;
		break;
	case 'w':
		t = num;
		num *= sizeof(int);
		if (t > num)
			return(0);
		++expr;
		break;
	}

	switch(*expr) {
		case '\0':
			break;
		case '*':
		case 'x':
			t = num;
			num *= str_offt(expr + 1);
			if (t > num)
				return(0);
			break;
		default:
			return(0);
	}
	return(num);
}

/*
 * no_op()
 *	for those option functions where the archive format has nothing to do.
 * Return:
 *	0
 */

#if __STDC__
static int
no_op(void)
#else
static int
no_op()
#endif
{
	return(0);
}

/*
 * pax_usage()
 *	print the usage summary to the user
 */

#if __STDC__
void
pax_usage(void)
#else
void
pax_usage()
#endif
{
	(void)fputs("usage: pax [-cdnvz] [-E limit] [-f archive] ", stderr);
	(void)fputs("[-s replstr] ... [-U user] ...", stderr);
	(void)fputs("\n           [-G group] ... ", stderr);
	(void)fputs("[-T [from_date][,to_date]] ... ", stderr);
	(void)fputs("[pattern ...]\n", stderr);
	(void)fputs("       pax -r [-cdiknuvzDYZ] [-E limit] ", stderr);
	(void)fputs("[-f archive] [-o options] ... \n", stderr);
	(void)fputs("           [-p string] ... [-s replstr] ... ", stderr);
	(void)fputs("[-U user] ... [-G group] ...\n           ", stderr);
	(void)fputs("[-T [from_date][,to_date]] ... ", stderr);
	(void)fputs(" [pattern ...]\n", stderr);
	(void)fputs("       pax -w [-dituvzHLPX] [-b blocksize] ", stderr);
	(void)fputs("[[-a] [-f archive]] [-x format] \n", stderr);
	(void)fputs("           [-B bytes] [-o options] ... ", stderr);
	(void)fputs("[-s replstr] ... [-U user] ...", stderr);
	(void)fputs("\n           [-G group] ... ", stderr);
	(void)fputs("[-T [from_date][,to_date][/[c][m]]] ... ", stderr);
	(void)fputs("[file ...]\n", stderr);
	(void)fputs("       pax -r -w [-diklntuvzDHLPXYZ] ", stderr);
	(void)fputs("[-p string] ... [-s replstr] ...", stderr);
	(void)fputs("\n           [-U user] ... [-G group] ... ", stderr);
	(void)fputs("[-T [from_date][,to_date][/[c][m]]] ... ", stderr);
	(void)fputs("\n           [file ...] directory\n", stderr);
	exit(1);
	/* NOTREACHED */
}

/*
 * tar_usage()
 *	print the usage summary to the user
 */

#if __STDC__
void
tar_usage(void)
#else
void
tar_usage()
#endif
{
	(void)fputs("usage: tar -{txru}[cevfbhlmopwBLPX014578] [tapefile] ",
		 stderr);
	(void)fputs("[blocksize] [exclude-file] file1 file2...\n", stderr);
	exit(1);
	/* NOTREACHED */
}

/*
 * cpio_usage()
 *	print the usage summary to the user
 */

#if __STDC__
void
cpio_usage(void)
#else
void
cpio_usage()
#endif
{

#if 1
	(void)fputs(
	    "usage: cpio -i [-BcdfmrStuv] [ -C blksize ] [ -H header ]\n",
	    stderr);
	(void)fputs("  [ -I file ] [ pattern ... ]\n", stderr);
	(void)fputs("usage: cpio -o [-aABcLv] [ -C bufsize ] [ -H header ]\n",
	    stderr);
	(void)fputs("  [ -O file ]\n", stderr);
	(void)fputs("usage: cpio -p [ adlLmuv ] directory\n", stderr);
#else
	/* no E, M, R, V, b, k or s */
	(void)fputs("usage: cpio -i [-bBcdfkmrsStuvV] [ -C bufsize ]\n", stderr);
	(void)fputs("  [ -E file ] [ -H header ] [ -I file [ -M message ] ]\n",
	    stderr);
	(void)fputs("  [ -R id ] [ pattern ... ]\n", stderr);
	(void)fputs("usage: cpio -o [-aABcLvV] [ -C bufsize ] [ -H header ]\n",
	    stderr);
	(void)fputs("  [ -O file [ -M message ] ]\n", stderr);
	(void)fputs("usage: cpio -p [ adlLmuvV ] [ -R id ] directory\n", stderr);
#endif
	exit(1);
	/* NOTREACHED */
}

/*
 * opt_chdir
 *	call ftree_add or pat_add, depending on archive type.
 *	
 * Returns: -1 for listing, else what ftree_add or pat_add returned.
 */

#ifdef __STDC__
int
opt_chdir(char *name)
#else
int
opt_chdir(name)
	char *name;
#endif
{
	switch (act) {
	default:
		return (-1);
		break;
	case ARCHIVE:
	case APPND:
		return (ftree_add(name, 1));
		break;
	case EXTRACT:
		return (pat_add(name, 1));
		break;
	}
}

/*
 * checkpositionalminusC(argvp, addfunc)
 */

#ifdef __STDC__
void
checkpositionalminusC(char ***argvp, int (*addfunc)(char *, int))
#else
void
checkpositionalminusC(argvp, addfunc)
	char ***argvp;
	int (*addfunc)();
#endif
{
	while (**argvp != (char *)NULL) {
		if (!strcmp(**argvp, "-C")) {
			/* XXX should be allow for positional -C/dir, too? */
			if ((*addfunc)(*++*argvp, 1) < 0) {
				tar_usage();
			}
			++*argvp;
			continue;
		}
		if ((*addfunc)(*(*argvp)++, 0) < 0)
			tar_usage();
	}
}
