/*	$NetBSD: sel_subs.c,v 1.7.2.1 1998/01/29 11:10:24 mellon Exp $	*/

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
static char sccsid[] = "@(#)sel_subs.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: sel_subs.c,v 1.7.2.1 1998/01/29 11:10:24 mellon Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <tzfile.h>

#include "pax.h"
#include "sel_subs.h"
#include "extern.h"

static int str_sec __P((char *, time_t *));
static int usr_match __P((ARCHD *));
static int grp_match __P((ARCHD *));
static int trng_match __P((ARCHD *));

static TIME_RNG *trhead = NULL;		/* time range list head */
static TIME_RNG *trtail = NULL;		/* time range list tail */
static USRT **usrtb = NULL;		/* user selection table */
static GRPT **grptb = NULL;		/* group selection table */

/*
 * Routines for selection of archive members
 */

/*
 * sel_chk()
 *	check if this file matches a specfied uid, gid or time range
 * Return:
 *	0 if this archive member should be processed, 1 if it should be skipped
 */

#if __STDC__
int
sel_chk(ARCHD *arcn)
#else
int
sel_chk(arcn)
	ARCHD *arcn;
#endif
{
	if (((usrtb != NULL) && usr_match(arcn)) ||
	    ((grptb != NULL) && grp_match(arcn)) ||
	    ((trhead != NULL) && trng_match(arcn)))
		return(1);
	return(0);
}

/*
 * User/group selection routines
 *
 * Routines to handle user selection of files based on the file uid/gid. To
 * add an entry, the user supplies either then name or the uid/gid starting with
 * a # on the command line. A \# will eascape the #.
 */

/*
 * usr_add()
 *	add a user match to the user match hash table
 * Return:
 *	0 if added ok, -1 otherwise;
 */

#if __STDC__
int
usr_add(char *str)
#else
int
usr_add(str)
	char *str;
#endif
{
	u_int indx;
	USRT *pt;
	struct passwd *pw;
	uid_t uid;

	/*
	 * create the table if it doesn't exist
	 */
	if ((str == NULL) || (*str == '\0'))
		return(-1);
	if ((usrtb == NULL) &&
 	    ((usrtb = (USRT **)calloc(USR_TB_SZ, sizeof(USRT *))) == NULL)) {
                tty_warn(1,
		    "Unable to allocate memory for user selection table");
                return(-1);
	}

	/*
	 * figure out user spec
	 */
	if (str[0] != '#') {
		/*
		 * it is a user name, \# escapes # as first char in user name
		 */
		if ((str[0] == '\\') && (str[1] == '#'))
			++str;
		if ((pw = getpwnam(str)) == NULL) {
                	tty_warn(1, "Unable to find uid for user: %s", str);
                	return(-1);
		}
		uid = (uid_t)pw->pw_uid;
        } else
#		ifdef NET2_STAT
		uid = (uid_t)atoi(str+1);
#		else
		uid = (uid_t)strtoul(str+1, (char **)NULL, 10);
#		endif
	endpwent();

	/*
	 * hash it and go down the hash chain (if any) looking for it
	 */
	indx = ((unsigned)uid) % USR_TB_SZ;
	if ((pt = usrtb[indx]) != NULL) {
                while (pt != NULL) {
                        if (pt->uid == uid)
				return(0);
                        pt = pt->fow;
                }
	}

	/*
	 * uid is not yet in the table, add it to the front of the chain
	 */
	if ((pt = (USRT *)malloc(sizeof(USRT))) != NULL) {
		pt->uid = uid;
		pt->fow = usrtb[indx];
		usrtb[indx] = pt;
		return(0);
	}
        tty_warn(1, "User selection table out of memory");
        return(-1);
}

/*
 * usr_match()
 *	check if this files uid matches a selected uid.
 * Return:
 *	0 if this archive member should be processed, 1 if it should be skipped
 */

#if __STDC__
static int
usr_match(ARCHD *arcn)
#else
static int
usr_match(arcn)
	ARCHD *arcn;
#endif
{
	USRT *pt;

	/*
	 * hash and look for it in the table
	 */
	pt = usrtb[((unsigned)arcn->sb.st_uid) % USR_TB_SZ];
	while (pt != NULL) {
		if (pt->uid == arcn->sb.st_uid)
			return(0);
		pt = pt->fow;
	}

	/*
	 * not found
	 */
	return(1);
}

/*
 * grp_add()
 *	add a group match to the group match hash table
 * Return:
 *	0 if added ok, -1 otherwise;
 */

#if __STDC__
int
grp_add(char *str)
#else
int
grp_add(str)
	char *str;
#endif
{
	u_int indx;
	GRPT *pt;
	struct group *gr;
	gid_t gid;

	/*
	 * create the table if it doesn't exist
	 */
	if ((str == NULL) || (*str == '\0'))
		return(-1);
	if ((grptb == NULL) &&
 	    ((grptb = (GRPT **)calloc(GRP_TB_SZ, sizeof(GRPT *))) == NULL)) {
                tty_warn(1,
		    "Unable to allocate memory fo group selection table");
                return(-1);
	}

	/*
	 * figure out user spec
	 */
	if (str[0] != '#') {
		/*
		 * it is a group name, \# escapes # as first char in group name
		 */
		if ((str[0] == '\\') && (str[1] == '#'))
			++str;
		if ((gr = getgrnam(str)) == NULL) {
                	tty_warn(1,
			    "Cannot determine gid for group name: %s", str);
                	return(-1);
		}
		gid = (gid_t)gr->gr_gid;
        } else
#		ifdef NET2_STAT
		gid = (gid_t)atoi(str+1);
#		else
		gid = (gid_t)strtoul(str+1, (char **)NULL, 10);
#		endif
	endgrent();

	/*
	 * hash it and go down the hash chain (if any) looking for it
	 */
	indx = ((unsigned)gid) % GRP_TB_SZ;
	if ((pt = grptb[indx]) != NULL) {
                while (pt != NULL) {
                        if (pt->gid == gid)
				return(0);
                        pt = pt->fow;
                }
	}

	/*
	 * gid not in the table, add it to the front of the chain
	 */
	if ((pt = (GRPT *)malloc(sizeof(GRPT))) != NULL) {
		pt->gid = gid;
		pt->fow = grptb[indx];
		grptb[indx] = pt;
		return(0);
	}
        tty_warn(1, "Group selection table out of memory");
        return(-1);
}

/*
 * grp_match()
 *	check if this files gid matches a selected gid.
 * Return:
 *	0 if this archive member should be processed, 1 if it should be skipped
 */

#if __STDC__
static int
grp_match(ARCHD *arcn)
#else
static int
grp_match(arcn)
	ARCHD *arcn;
#endif
{
	GRPT *pt;

	/*
	 * hash and look for it in the table
	 */
	pt = grptb[((unsigned)arcn->sb.st_gid) % GRP_TB_SZ];
	while (pt != NULL) {
		if (pt->gid == arcn->sb.st_gid)
			return(0);
		pt = pt->fow;
	}

	/*
	 * not found
	 */
	return(1);
}

/*
 * Time range selection routines
 *
 * Routines to handle user selection of files based on the modification and/or
 * inode change time falling within a specified time range (the non-standard
 * -T flag). The user may specify any number of different file time ranges.
 * Time ranges are checked one at a time until a match is found (if at all).
 * If the file has a mtime (and/or ctime) which lies within one of the time
 * ranges, the file is selected. Time ranges may have a lower and/or a upper
 * value. These ranges are inclusive. When no time ranges are supplied to pax
 * with the -T option, all members in the archive will be selected by the time
 * range routines. When only a lower range is supplied, only files with a
 * mtime (and/or ctime) equal to or younger are selected. When only a upper
 * range is supplied, only files with a mtime (and/or ctime) equal to or older
 * are selected. When the lower time range is equal to the upper time range,
 * only files with a mtime (or ctime) of exactly that time are selected.
 */

/*
 * trng_add()
 *	add a time range match to the time range list.
 *	This is a non-standard pax option. Lower and upper ranges are in the
 *	format: [yy[mm[dd[hh]]]]mm[.ss] and are comma separated.
 *	Time ranges are based on current time, so 1234 would specify a time of
 *	12:34 today.
 * Return:
 *	0 if the time range was added to the list, -1 otherwise
 */

#if __STDC__
int
trng_add(char *str)
#else
int
trng_add(str)
	char *str;
#endif
{
	TIME_RNG *pt;
	char *up_pt = NULL;
	char *stpt;
	char *flgpt;
	int dot = 0;

	/*
	 * throw out the badly formed time ranges
	 */
	if ((str == NULL) || (*str == '\0')) {
		tty_warn(1, "Empty time range string");
		return(-1);
	}

	/*
	 * locate optional flags suffix /{cm}.
	 */
	if ((flgpt = strrchr(str, '/')) != NULL)
		*flgpt++ = '\0';

	for (stpt = str; *stpt != '\0'; ++stpt) {
		if ((*stpt >= '0') && (*stpt <= '9'))
			continue;
		if ((*stpt == ',') && (up_pt == NULL)) {
			*stpt = '\0';
			up_pt = stpt + 1;
			dot = 0;
			continue;
		}

		/*
		 * allow only one dot per range (secs)
		 */
		if ((*stpt == '.') && (!dot)) {
			++dot;
			continue;
		}
		tty_warn(1, "Improperly specified time range: %s", str);
		goto out;
	}

	/*
	 * allocate space for the time range and store the limits
	 */
	if ((pt = (TIME_RNG *)malloc(sizeof(TIME_RNG))) == NULL) {
		tty_warn(1, "Unable to allocate memory for time range");
		return(-1);
	}

	/*
	 * by default we only will check file mtime, but usee can specify
	 * mtime, ctime (inode change time) or both.
	 */
	if ((flgpt == NULL) || (*flgpt == '\0'))
		pt->flgs = CMPMTME;
	else {
		pt->flgs = 0;
		while (*flgpt != '\0') {
			switch(*flgpt) {
			case 'M':
			case 'm':
				pt->flgs |= CMPMTME;
				break;
			case 'C':
			case 'c':
				pt->flgs |= CMPCTME;
				break;
			default:
				tty_warn(1, "Bad option %c with time range %s",
				    *flgpt, str);
				goto out;
			}
			++flgpt;
		}
	}

	/*
	 * start off with the current time
	 */
	pt->low_time = pt->high_time = time((time_t *)NULL);
	if (*str != '\0') {
		/*
		 * add lower limit
		 */
		if (str_sec(str, &(pt->low_time)) < 0) {
			tty_warn(1, "Illegal lower time range %s", str);
			(void)free((char *)pt);
			goto out;
		}
		pt->flgs |= HASLOW;
	}

	if ((up_pt != NULL) && (*up_pt != '\0')) {
		/*
		 * add upper limit
		 */
		if (str_sec(up_pt, &(pt->high_time)) < 0) {
			tty_warn(1, "Illegal upper time range %s", up_pt);
			(void)free((char *)pt);
			goto out;
		}
		pt->flgs |= HASHIGH;

		/*
		 * check that the upper and lower do not overlap
		 */
		if (pt->flgs & HASLOW) {
			if (pt->low_time > pt->high_time) {
				tty_warn(1,
				    "Upper %s and lower %s time overlap",
				    up_pt, str);
				(void)free((char *)pt);
				return(-1);
			}
		}
	}

	pt->fow = NULL;
	if (trhead == NULL) {
		trtail = trhead = pt;
		return(0);
	}
	trtail->fow = pt;
	trtail = pt;
	return(0);

    out:
	tty_warn(1, "Time range format is: [yy[mm[dd[hh]]]]mm[.ss][/[c][m]]");
	return(-1);
}

/*
 * trng_match()
 *	check if this files mtime/ctime falls within any supplied time range.
 * Return:
 *	0 if this archive member should be processed, 1 if it should be skipped
 */

#if __STDC__
static int
trng_match(ARCHD *arcn)
#else
static int
trng_match(arcn)
	ARCHD *arcn;
#endif
{
	TIME_RNG *pt;

	/*
	 * have to search down the list one at a time looking for a match.
	 * remember time range limits are inclusive.
	 */
	pt = trhead;
	while (pt != NULL) {
		switch(pt->flgs & CMPBOTH) {
		case CMPBOTH:
			/*
			 * user wants both mtime and ctime checked for this
			 * time range
			 */
			if (((pt->flgs & HASLOW) &&
			    (arcn->sb.st_mtime < pt->low_time) &&
			    (arcn->sb.st_ctime < pt->low_time)) ||
			    ((pt->flgs & HASHIGH) &&
			    (arcn->sb.st_mtime > pt->high_time) &&
			    (arcn->sb.st_ctime > pt->high_time))) {
				pt = pt->fow;
				continue;
			}
			break;
		case CMPCTME:
			/*
			 * user wants only ctime checked for this time range
			 */
			if (((pt->flgs & HASLOW) &&
			    (arcn->sb.st_ctime < pt->low_time)) ||
			    ((pt->flgs & HASHIGH) &&
			    (arcn->sb.st_ctime > pt->high_time))) {
				pt = pt->fow;
				continue;
			}
			break;
		case CMPMTME:
		default:
			/*
			 * user wants only mtime checked for this time range
			 */
			if (((pt->flgs & HASLOW) &&
			    (arcn->sb.st_mtime < pt->low_time)) ||
			    ((pt->flgs & HASHIGH) &&
			    (arcn->sb.st_mtime > pt->high_time))) {
				pt = pt->fow;
				continue;
			}
			break;
		}
		break;
	}

	if (pt == NULL)
		return(1);
	return(0);
}

/*
 * str_sec()
 *	Convert a time string in the format of [yy[mm[dd[hh]]]]mm[.ss] to gmt
 *	seconds. Tval already has current time loaded into it at entry.
 * Return:
 *	0 if converted ok, -1 otherwise
 */

#define ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

#if __STDC__
static int
str_sec(char *str, time_t *tval)
#else
static int
str_sec(str, tval)
	char *str;
	time_t *tval;
#endif
{
	struct tm *lt;
	char *dot = NULL;
	int yearset;

	lt = localtime(tval);
	if ((dot = strchr(str, '.')) != NULL) {
		/*
		 * seconds (.ss)
		 */
		*dot++ = '\0';
		if (strlen(dot) != 2)
			return(-1);
		lt->tm_sec = ATOI2(dot);
	} else
		lt->tm_sec = 0;

	yearset = 0;
	switch (strlen(str)) {
	case 12:
		lt->tm_year = ATOI2(str) * 100 - TM_YEAR_BASE;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:
		if (yearset) {
			lt->tm_year += ATOI2(str);
		} else {
			yearset = ATOI2(str);
			if (yearset < 69)
				lt->tm_year = yearset + 2000 - TM_YEAR_BASE;
			else
				lt->tm_year = yearset + 1900 - TM_YEAR_BASE;
		}
		/* FALLTHROUGH */
	case 8:
		lt->tm_mon = ATOI2(str);
		--lt->tm_mon;
		/* FALLTHROUGH */
	case 6:
		lt->tm_mday = ATOI2(str);
		/* FALLTHROUGH */
	case 4:
		lt->tm_hour = ATOI2(str);
		/* FALLTHROUGH */
	case 2:
		lt->tm_min = ATOI2(str);
		break;
	default:
		return(-1);
	}

	/*
	 * convert broken-down time to GMT clock time seconds
	 */
	if ((*tval = mktime(lt)) == -1)
		return(-1);
	return(0);
}
