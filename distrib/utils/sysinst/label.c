/*	$NetBSD: label.c,v 1.9.2.2 1999/06/24 22:58:28 cgd Exp $	*/

/*
 * Copyright 1997 Jonathan Stone
 * All rights reserved.
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
 *      This product includes software developed for the NetBSD Project by
 *      Jonathan Stone.
 * 4. The name of Jonathan Stone may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JONATHAN STONE ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: label.c,v 1.9.2.2 1999/06/24 22:58:28 cgd Exp $");
#endif

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>

#include "defs.h"
#include "msg_defs.h"
#include "menu_defs.h"

/*
 * local prototypes
 */
static int boringpart __P((partinfo *lp, int i, int rawpart, int bsdpart));

int	checklabel __P((partinfo *lp, int nparts, int rawpart, int bsdpart,
		    int *bad1, int *bad2));
void	translate_partinfo __P((partinfo *lp, struct partition *pp));
void	atofsb __P((const char *, int *, int *));


/*
 * Return 1 iff partition i in lp should be ignored when checking
 * for  overlapping partitions.
 */
static int
boringpart(lp, i, rawpart, bsdpart)
	partinfo *lp;
	int i;
	int rawpart;
	int bsdpart;
{

	if (i == rawpart || i == bsdpart ||
	    lp[i].pi_fstype == FS_UNUSED || lp[i].pi_size == 0)
		return 1;
	return 0;
}



/*
 * Check a sysinst label structure for overlapping partitions.
 * Returns 0 if no overlapping partition found, nonzero otherwise.
 * Sets reference arguments ovly1 and ovly2 to the indices of
 * overlapping partitions if any are found.
 */
int
checklabel(lp, nparts, rawpart, bsdpart, ovly1, ovly2)
	partinfo *lp;
	int nparts;
	int rawpart;
	int bsdpart;
	int *ovly1;
	int *ovly2;
{
	int i;
	int j;

	*ovly1 = -1;
	*ovly2 = -1;

	for (i = 0; i < nparts - 1; i ++ ) {
		partinfo *ip = &lp[i];
		int istart, istop;

		/* skip unused or reserved partitions */
		if (boringpart(lp, i, rawpart, bsdpart))
			continue;

		/*
		 * check succeding partitions for overlap.
		 * O(n^2), but n is small (currently <= 16).
		 */
		istart = ip->pi_offset;
		istop = istart + ip->pi_size;

		for (j = i+1; j < nparts; j++) {
			partinfo *jp = &lp[j];
			int jstart, jstop;

			/* skip unused or reserved partitions */
			if (boringpart(lp, j, rawpart, bsdpart))
				continue;

			jstart = jp->pi_offset;
			jstop = jstart + jp->pi_size;

			/* overlap? */
			if ((istart <= jstart && jstart < istop) ||
			    (jstart <= istart && istart < jstop)) {
				*ovly1 = i;
				*ovly2 = j;
				return (1);
			}
		}
	}

	return (0);
}


/*
 * Check a disklabel.
 * If there are overlapping active parititons,
 * Ask the user if they want to edit the parittion or give up.
 */
int
edit_and_check_label(lp, nparts, rawpart, bsdpart)
	partinfo *lp;
	int nparts;
	int rawpart;
	int bsdpart;
{
	 while (1) {
		 int i, j;

		 /* first give the user the option to edit the label... */
		 process_menu(MENU_fspartok);

		 /* User thinks the label is OK. check for overlaps.*/
		 if (checklabel(lp, nparts, rawpart, bsdpart, &i, &j) == 0) {
			/* partitions are OK. */
			 return (1);
		 }
		 
		 /* partitions overlap. */
		 msg_display(MSG_partitions_overlap, 'a' + i, 'a' + j);
		 /*XXX*/
		 msg_display_add(MSG_edit_partitions_again);
		 process_menu(MENU_yesno);
		 if (!yesno)
			 return(0);
	 }

	/*NOTREACHED*/
}

	
void
emptylabel(lp)
	partinfo *lp;
{
	register int i, maxpart;

	maxpart = getmaxpartitions();

	for (i = 0; i < maxpart; i++) {
		lp[i].pi_fstype = FS_UNUSED;
		lp[i].pi_offset = 0;
		lp[i].pi_size = 0;
		lp[i].pi_bsize = 0;
		lp[i].pi_fsize = 0;
	}
}

int
savenewlabel(lp, nparts)
	partinfo *lp;
	int nparts;
{
	FILE *f;
	int i;

#ifdef DEBUG
	f = fopen("/tmp/disktab", "a");
#else
	/* Create the disktab.preinstall */
	run_prog(0, 0, NULL, "cp /etc/disktab.preinstall /etc/disktab");
	f = fopen("/etc/disktab", "a");
#endif
	if (logging)
		(void)fprintf(log, "Creating disklabel %s\n", bsddiskname);
	if (scripting)
		(void)fprintf(script, "cat <<EOF >>/etc/disktab\n");
	if (f == NULL) {
		endwin();
		(void)fprintf(stderr, "Could not open /etc/disktab");
		if (logging)
			(void)fprintf(log, "Failed to open /etc/diskabel for appending.\n");
		exit (1);
	}
	(void)fprintf(f, "%s|NetBSD installation generated:\\\n", bsddiskname);
	(void)fprintf(f, "\t:dt=%s:ty=winchester:\\\n", disktype);
	(void)fprintf(f, "\t:nc#%d:nt#%d:ns#%d:\\\n", dlcyl, dlhead, dlsec);
	(void)fprintf(f, "\t:sc#%d:su#%d:\\\n", dlhead*dlsec, dlsize);
	(void)fprintf(f, "\t:se#%d:%s\\\n", sectorsize, doessf);
	if (scripting) {
		(void)fprintf(script, "%s|NetBSD installation generated:\\\n", bsddiskname);
		(void)fprintf(script, "\t:dt=%s:ty=winchester:\\\n", disktype);
		(void)fprintf(script, "\t:nc#%d:nt#%d:ns#%d:\\\n", dlcyl, dlhead, dlsec);
		(void)fprintf(script, "\t:sc#%d:su#%d:\\\n", dlhead*dlsec, dlsize);
		(void)fprintf(script, "\t:se#%d:%s\\\n", sectorsize, doessf);
	}
	for (i = 0; i < nparts; i++) {
		(void)fprintf(f, "\t:p%c#%d:o%c#%d:t%c=%s:",
		    'a'+i, bsdlabel[i].pi_size,
		    'a'+i, bsdlabel[i].pi_offset,
		    'a'+i, fstypenames[bsdlabel[i].pi_fstype]);
		if (scripting)
			(void)fprintf(script, "\t:p%c#%d:o%c#%d:t%c=%s:",
			    'a'+i, bsdlabel[i].pi_size,
			    'a'+i, bsdlabel[i].pi_offset,
			    'a'+i, fstypenames[bsdlabel[i].pi_fstype]);
		if (bsdlabel[i].pi_fstype == FS_BSDFFS) {
			(void)fprintf (f, "b%c#%d:f%c#%d:ta=4.2BSD:",
				       'a'+i, bsdlabel[i].pi_bsize,
				       'a'+i, bsdlabel[i].pi_fsize);
			if (scripting)
				(void)fprintf (script, "b%c#%d:f%c#%d:ta=4.2BSD:",
					       'a'+i, bsdlabel[i].pi_bsize,
					       'a'+i, bsdlabel[i].pi_fsize);
		}
		if (i < 7)
			(void)fprintf(f, "\\\n");
		else
			(void)fprintf(f, "\n");
		if (scripting) {
			if (i < 7)
				(void)fprintf(script, "\\\n");
			else
				(void)fprintf(script, "\n");
		}
	}
	fclose (f);
	if (scripting)
		(void)fprintf(script, "EOF\n");
	fflush(NULL);
	return(0);
}


/*
 * XXX MSDOS?
 */
void
translate_partinfo(lp, pp)
	partinfo *lp;
	struct partition *pp;
{

	lp->pi_fstype = pp->p_fstype;

	switch (pp->p_fstype) {

	case FS_UNUSED:				/* XXX */
	case FS_SWAP:
		break;

	case FS_BSDFFS:
		(*lp).pi_offset = 0;
		(*lp).pi_size = 0;
		(*lp).pi_bsize = pp->p_fsize * pp->p_frag;
		(*lp).pi_fsize = pp->p_fsize;
		break;

	case FS_EX2FS:
		(*lp).pi_fstype = FS_UNUSED;	/* XXX why? */
		(*lp).pi_bsize = pp->p_fsize * pp->p_frag;
		(*lp).pi_fsize = pp->p_fsize;
		break;

	default:
		(*lp).pi_fstype = FS_UNUSED;
		break;
	}
}

/*
 * Read a label from disk into a sysist label structure.
 */
int
incorelabel(dkname, lp)
	const char *dkname;
	partinfo *lp;
{
	struct disklabel lab;
	int fd;
	int i, maxpart;
	struct partition *pp;
	char nambuf[STRSIZE];

	fd = opendisk(dkname, O_RDONLY,  nambuf, STRSIZE, 0);

	if (ioctl(fd, DIOCGDINFO, &lab) < 0) {
		/*XXX err(4, "ioctl DIOCGDINFO");*/
		return(errno);
	}
	close(fd);
	touchwin(stdscr);
	maxpart = getmaxpartitions();
	if (maxpart > 16)
		maxpart = 16;


	/* XXX set globals used by MD code to compute disk size? */
	
	pp = &lab.d_partitions[0];
	emptylabel(lp);
	for (i = 0; i < maxpart; i++) {
		translate_partinfo(lp+i, pp+i);
	}

	return (0);
}

/* Ask for a partition offser, check bounds and does the needed roudups */
int
getpartoff(msg_no, defpartstart)
	int msg_no;
	int defpartstart;
{
	char isize[20];
	int i, localsizemult;

	while (1) {
		snprintf (isize, 20, "%d", (defpartstart)/sizemult);
		msg_prompt_add(msg_no, (defpartstart > 0) ? isize : NULL,
		    isize, 20);
		atofsb(isize, &i, &localsizemult);
		if (i < 0)
			continue;
		/* round to cylinder size if localsizemult != 1 */
		i = NUMSEC(i/localsizemult, localsizemult, dlcylsize);
		/* Adjust to start of slice if needed */
		if ((i < ptstart && (ptstart - i) < localsizemult) ||
		    (i > ptstart && (i - ptstart) < localsizemult)) {
			i = ptstart;
		}
		if (i > fsdsize) {
			msg_display(MSG_startoutsidedisk);
			continue;
		}
		return i;
	}
	/* NOTREACHED */
}


/* Ask for a partition size, check bounds and does the needed roudups */
int
getpartsize(msg_no, partstart, defpartsize)
	int msg_no;
	int partstart;
	int defpartsize;
{
	char isize[20];
	int i, partend, localsizemult;
	int fsptend = ptstart + fsptsize;

	while (1) {
		snprintf (isize, 20, "%d", (defpartsize)/sizemult);
		msg_prompt_add (msg_no, (defpartsize != 0) ? isize : 0,
		    isize, 20);
		atofsb(isize, &i, &localsizemult);
		if (i < 0)
			continue;
		/*
		 * partend is aligned to a cylinder if localsizemult
		 * is not 1 sector
		 */
		partend = NUMSEC((partstart + i) / localsizemult,
		    localsizemult, dlcylsize);
		/* Align to end-of-disk or end-of-slice if close enouth */
		if (fsdsize > partend && (fsdsize - partend) < localsizemult)
			partend = fsdsize;
		if (fsptend > partend && (fsptend - partend) < localsizemult)
			partend = fsptend;
		/* sanity checks */
		if (partend > fsdsize) {
			partend = fsdsize;
			msg_display(MSG_endoutsidedisk,
			    (partend - partstart) / sizemult, multname);
			process_menu(MENU_ok);
		}
		/* return value */
		return (partend - partstart);
	}
	/* NOTREACHED */
}

/*
 * convert a string to a number of sectors, with a possible unit
 * 150M = 150 Megabytes
 * 2000c = 2000 cylinders
 * 150256s = 150256 sectors
 * Without units, use the default (sizemult)
 * returns the number of sectors, and the unit used (for roudups).
 */

void
atofsb(str, val, localsizemult)
	const char *str;
	int *val;
	int *localsizemult;
{
	int i;

	*localsizemult = sizemult;
	if (str[0] == '\0') {
		*val = -1;
		return;
	}
	*val = 0;
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			*val = (*val) * 10 + str[i] - '0';
			continue;
		}
		if (str[i + 1] != '\0') {
			/* A non-digit caracter, not at the end */
			*val = -1;
			return;
		}
		if (str[i] == 'M') {
			*localsizemult = MEG / sectorsize;
			break;
		}
		if (str[i] == 'c') {
			*localsizemult = dlcylsize;
			break;
		}
		if (str[i] == 's') {
			*localsizemult = 1;
			break;
		}
		/* not a known unit */
		*val = -1;
		return;
	}
	*val = (*val) * (*localsizemult);
	return;
}
