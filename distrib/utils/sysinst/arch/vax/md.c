/*	$NetBSD: md.c,v 1.3.2.3 2000/03/01 12:38:48 he Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Based on code written by Philip A. Nelson for Piermont Information
 * Systems Inc.
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
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
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
 */

/* changes from the i386 version made by mrg */

/* md.c -- vax machine specific routines */
/* This file is in close sync with pmax, sparc, and x68k md.c */

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <stdio.h>
#include <curses.h>
#include <unistd.h>
#include <fcntl.h>
#include <util.h>

#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"

/*
 * Symbolic names for disk partitions.
 */
#define	PART_ROOT	A
#define PART_SWAP	B
#define	PART_RAW	C
#define PART_USR	D	/* Can be after PART_FIRST_FREE */
#define PART_FIRST_FREE	E
#define PART_LAST	H

#define DEFSWAPRAM	16	/* Assume at least this RAM for swap calc */
#define DEFROOTSIZE	32	/* Default root size */
#define STDNEEDMB	80	/* Min space for non X install */

int
md_get_info(void)
{
	struct disklabel disklabel;
	int fd;
	char devname[100];

	snprintf (devname, 100, "/dev/r%sc", diskdev);

	fd = open (devname, O_RDONLY, 0);
	if (fd < 0) {
		if (logging)
			(void)fprintf(log, "Can't open %s\n", devname);
		endwin();
		fprintf (stderr, "Can't open %s\n", devname);
		exit(1);
	}
	if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
		if (logging)
			(void)fprintf(log, "Can't read disklabel on %s.\n",
				devname);
		endwin();
		fprintf (stderr, "Can't read disklabel on %s.\n", devname);
		close(fd);
		exit(1);
	}
	close(fd);

	dlcyl = disklabel.d_ncylinders;
	dlhead = disklabel.d_ntracks;
	dlsec = disklabel.d_nsectors;
	sectorsize = disklabel.d_secsize;
	dlcylsize = disklabel.d_secpercyl;

	/*
	 * Compute whole disk size. Take max of (dlcyl*dlhead*dlsec)
	 * and secperunit,  just in case the disk is already labelled.  
	 * (If our new label's RAW_PART size ends up smaller than the
	 * in-core RAW_PART size  value, updating the label will fail.)
	 */
	dlsize = dlcyl*dlhead*dlsec;
	if (disklabel.d_secperunit > dlsize)
		dlsize = disklabel.d_secperunit;

	/* Compute minimum NetBSD partition sizes (in sectors). */
	minfsdmb = STDNEEDMB * (MEG / sectorsize);

	return 1;
}

/*
 * hook called before writing new disklabel.
 */
int
md_pre_disklabel(void)
{
	return 0;
}

/*
 * hook called after writing disklabel to new target disk.
 */
int
md_post_disklabel(void)
{
	return 0;
}

/*
 * MD hook called after upgrade() or install() has finished setting
 * up the target disk but immediately before the user is given the
 * ``disks are now set up'' message, so that if power fails, they can
 * continue installation by booting the target disk and doing an
 * `upgrade'.
 *
 * On the vax, we use this opportunity to install the boot blocks.
 */
int
md_post_newfs(void)
{

	printf(msg_string(MSG_dobootblks), diskdev);
	run_prog(0, 0, NULL, "/sbin/disklabel -B %s", diskdev);
	return 0;
}

/*
 * some ports use this to copy the MD filesystem, we do not.
 */
int
md_copy_filesystem(void)
{
	return 0;
}

/*
 * md back-end code for menu-driven BSD disklabel editor.
 */
int
md_make_bsd_partitions(void)
{
	int i, part;
	int remain;
	char isize[20];
	int maxpart = getmaxpartitions();

	/*
	 * Initialize global variables that track  space used on this disk.
	 * Standard 4.3BSD 8-partition labels always cover whole disk.
	 */
	ptsize = dlsize - ptstart;
	fsdsize = dlsize;		/* actually means `whole disk' */
	fsptsize = dlsize - ptstart;	/* netbsd partition -- same as above */
	fsdmb = fsdsize / MEG;

	/* Ask for layout type -- standard or special */
	msg_display (MSG_layout,
			(1.0*fsptsize*sectorsize)/MEG,
			(1.0*minfsdmb*sectorsize)/MEG,
			(1.0*minfsdmb*sectorsize)/MEG+rammb+XNEEDMB);
	process_menu (MENU_layout);

	if (layoutkind == 3) {
		ask_sizemult();
	} else {
		sizemult = MEG / sectorsize;
		multname = msg_string(MSG_megname);
	}


	/* Build standard partitions */
	emptylabel(bsdlabel);

	/* Partitions C is predefined (whole  disk). */
	bsdlabel[C].pi_fstype = FS_UNUSED;
	bsdlabel[C].pi_offset = 0;
	bsdlabel[C].pi_size = dlsize;
	
	/* Standard fstypes */
	bsdlabel[A].pi_fstype = FS_BSDFFS;
	bsdlabel[B].pi_fstype = FS_UNUSED;
	/* Conventionally, C is whole disk. */
	bsdlabel[D].pi_fstype = FS_UNUSED;	/* fill out below */
	bsdlabel[E].pi_fstype = FS_UNUSED;
	bsdlabel[F].pi_fstype = FS_UNUSED;
	bsdlabel[G].pi_fstype = FS_UNUSED;
	bsdlabel[H].pi_fstype = FS_UNUSED;


	switch (layoutkind) {
	case 1: /* standard: a root, b swap, c "unused", d /usr */
	case 2: /* standard X: a root, b swap (big), c "unused", d /usr */
		partstart = ptstart;

		/* Root */
		partsize = NUMSEC(DEFROOTSIZE, MEG/sectorsize, dlcylsize);
		bsdlabel[PART_ROOT].pi_offset = partstart;
		bsdlabel[PART_ROOT].pi_size = partsize;
		bsdlabel[PART_ROOT].pi_bsize = 8192;
		bsdlabel[PART_ROOT].pi_fsize = 1024;
		strcpy(fsmount[PART_ROOT], "/");
		partstart += partsize;

		/* swap */
		i = NUMSEC(layoutkind * 2 *
			   (rammb < DEFSWAPRAM ? DEFSWAPRAM : rammb),
			   MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
			   dlcylsize) - partstart;
		bsdlabel[PART_SWAP].pi_fstype = FS_SWAP;
		bsdlabel[PART_SWAP].pi_offset = partstart;
		bsdlabel[PART_SWAP].pi_size = partsize;
		partstart += partsize;

		/* /usr */
		partsize = fsdsize - partstart;
		bsdlabel[PART_USR].pi_fstype = FS_BSDFFS;
		bsdlabel[PART_USR].pi_offset = partstart;
		bsdlabel[PART_USR].pi_size = partsize;
		bsdlabel[PART_USR].pi_bsize = 8192;
		bsdlabel[PART_USR].pi_fsize = 1024;
		strcpy (fsmount[PART_USR], "/usr");

		break;

	case 3: /* custom: ask user for all sizes */
		ask_sizemult();
		/* root */
		partstart = ptstart;
		remain = fsdsize - partstart;
		partsize = NUMSEC(DEFROOTSIZE, MEG/sectorsize, dlcylsize);
		snprintf (isize, 20, "%d", partsize/sizemult);
		msg_prompt (MSG_askfsroot, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
		/* If less than a 'unit' left, also use it */
		if (remain - partsize < sizemult)
			partsize = remain;
		bsdlabel[PART_ROOT].pi_offset = partstart;
		bsdlabel[PART_ROOT].pi_size = partsize;
		bsdlabel[PART_ROOT].pi_bsize = 8192;
		bsdlabel[PART_ROOT].pi_fsize = 1024;
		strcpy(fsmount[PART_ROOT], "/");
		partstart += partsize;
		
		/* swap */
		remain = fsdsize - partstart;
		if (remain > 0) {
			i = NUMSEC(2 *
				   (rammb < DEFSWAPRAM ? DEFSWAPRAM : rammb),
			   MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
			   dlcylsize) - partstart;
			if (partsize > remain)
				partsize = remain;
		snprintf (isize, 20, "%d", partsize/sizemult);
		msg_prompt_add (MSG_askfsswap, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			if (partsize) {
				/* If less than a 'unit' left, also use it */
				if (remain - partsize < sizemult)
					partsize = remain;
				bsdlabel[PART_SWAP].pi_fstype = FS_SWAP;
				bsdlabel[PART_SWAP].pi_offset = partstart;
				bsdlabel[PART_SWAP].pi_size = partsize;
		partstart += partsize;
			}
		}
		
		/* /usr */
		remain = fsdsize - partstart;
		if (remain > 0) {
			partsize = remain;
		snprintf (isize, 20, "%d", partsize/sizemult);
		msg_prompt_add (MSG_askfsusr, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			if (partsize) {
				/* If less than a 'unit' left, also use it */
		if (remain - partsize < sizemult)
			partsize = remain;
		bsdlabel[PART_USR].pi_fstype = FS_BSDFFS;
		bsdlabel[PART_USR].pi_offset = partstart;
		bsdlabel[PART_USR].pi_size = partsize;
		bsdlabel[PART_USR].pi_bsize = 8192;
		bsdlabel[PART_USR].pi_fsize = 1024;
		strcpy (fsmount[PART_USR], "/usr");
		partstart += partsize;
			}
		}

		/* Others ... */
		remain = fsdsize - partstart;
		if (remain > 0)
			msg_display (MSG_otherparts);
		part = PART_FIRST_FREE;
		for (; remain > 0 && part <= PART_LAST; ++part ) {
			if (bsdlabel[part].pi_fstype != FS_UNUSED)
				continue;
			partsize = fsdsize - partstart;
			snprintf (isize, 20, "%d", partsize/sizemult);
			msg_prompt_add (MSG_askfspart, isize, isize, 20,
					diskdev, partname[part],
					remain/sizemult, multname);
			partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			/* If less than a 'unit' left, also use it */
			if (remain - partsize < sizemult)
				partsize = remain;
			bsdlabel[part].pi_fstype = FS_BSDFFS;
			bsdlabel[part].pi_offset = partstart;
			bsdlabel[part].pi_size = partsize;
			bsdlabel[part].pi_bsize = 8192;
			bsdlabel[part].pi_fsize = 1024;
			msg_prompt_add(MSG_mountpoint, NULL, fsmount[part],
				    20);
			partstart += partsize;
			remain = fsdsize - partstart;
		}

		break;
	}

	/*
	 * OK, we have a partition table. Give the user the chance to
	 * edit it and verify it's OK, or abort altogether.
	 */
	if (edit_and_check_label(bsdlabel, maxpart, RAW_PART, RAW_PART) == 0) {
		msg_display(MSG_abort);
		return 0;
	}

	/* Disk name */
	msg_prompt (MSG_packname, "mydisk", bsddiskname, DISKNAME_SIZE);

	/* save label to disk for MI code to update. */
	(void) savenewlabel(bsdlabel, 8);	/* 8 partitions in  label */

	/* Everything looks OK. */
	return (1);
}

/* Upgrade support */
int
md_update(void)
{
	endwin();
	md_copy_filesystem ();
	md_post_newfs();
	puts(CL);		/* XXX */
	wclear(stdscr);
	wrefresh(stdscr);
	return 1;
}

void
md_cleanup_install(void)
{
	char realfrom[STRSIZE];
	char realto[STRSIZE];
	char sedcmd[STRSIZE];

	strncpy(realfrom, target_expand("/etc/rc.conf"), STRSIZE);
	strncpy(realto, target_expand("/etc/rc.conf.install"), STRSIZE);

	sprintf(sedcmd, "sed 's/rc_configured=NO/rc_configured=YES/' < %s > %s",
	    realfrom, realto);
	if (logging)
		(void)fprintf(log, "%s\n", sedcmd);
	if (scripting)
		(void)fprintf(script, "%s\n", sedcmd);
	do_system(sedcmd);

	run_prog(1, 0, NULL, "mv -f %s %s", realto, realfrom);

	run_prog(0, 0, NULL, "rm -f %s", target_expand("/sysinst"));
	run_prog(0, 0, NULL, "rm -f %s", target_expand("/.termcap"));
	run_prog(0, 0, NULL, "rm -f %s", target_expand("/.profile"));
}
