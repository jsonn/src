/*	$NetBSD: md.c,v 1.3.2.4 1997/11/18 19:22:33 thorpej Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
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
 *      This product includes software develooped for the NetBSD Project by
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
 *
 */

/* md.c -- pmax machine specific routines */

#include <stdio.h>
#include <curses.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"

/*
 * symbolic names for disk partitions
 */
#define PART_ROOT A
#define PART_RAW  C
#define PART_USR  D

int	md_get_info (void)
{	struct disklabel disklabel;
	int fd;
	char devname[100];

	snprintf (devname, 100, "/dev/r%sc", diskdev);

	fd = open (devname, O_RDONLY, 0);
	if (fd < 0) {
		endwin();
		fprintf (stderr, "Can't open %s\n", devname);
		exit(1);
	}
	if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
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
	minfsdmb = (80 + 4*rammb) * (MEG / sectorsize);

	return 1;
}


/* 
 * hook called before editing new disklabel.
 */
void	md_pre_disklabel (void)
{
}


/* 
 * hook called after writing  disklabel to new target disk.
 */
void	md_post_disklabel (void)

{
}

/*
 * md back-end code for menu-driven  BSD disklabel editor.
 */
 void md_make_bsd_partitions (void)
{
	FILE *f;
	int i, part;
	int remain;
	char isize[20];

	/*
	 * Initialize global variables that track  space used on this disk.
	 * Standard 4.3BSD 8-partition labels always cover whole disk.
	 */
	ptstart = 0;
	ptsize = dlsize;
	fsdsize = dlsize;	/* actually means `whole disk' */
	fsptsize = dlsize;	/* netbsd partition -- same as above */
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

	/* Partitions C is predefined (whole  disk). */
	bsdlabel[C][D_FSTYPE] = T_UNUSED;
	bsdlabel[C][D_OFFSET] = 0;
	bsdlabel[C][D_SIZE] = dlsize;
	
	/* Standard fstypes */
	bsdlabel[A][D_FSTYPE] = T_42BSD;
	bsdlabel[B][D_FSTYPE] = T_SWAP;
	/* Conventionally, C is whole disk. */
	bsdlabel[D][D_FSTYPE] = T_UNUSED;	/* fill out below */
	bsdlabel[E][D_FSTYPE] = T_UNUSED;
	bsdlabel[F][D_FSTYPE] = T_UNUSED;
	bsdlabel[G][D_FSTYPE] = T_UNUSED;
	bsdlabel[H][D_FSTYPE] = T_UNUSED;


	switch (layoutkind) {
	case 1: /* standard: a root, b swap, c "unused", d /usr */
	case 2: /* standard X: a root, b swap (big), c "unused", d /usr */
		partstart = ptstart;

		/* Root */
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
#if 0
		/* i386 md code uses: */
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
				   dlcylsize) - partstart;
#else
		/* By convention, NetBSD/pmax uses a 32Mbyte root */
		partsize= NUMSEC(32, MEG/sectorsize, dlcylsize);
#endif
		bsdlabel[A][D_OFFSET] = partstart;
		bsdlabel[A][D_SIZE] = partsize;
		bsdlabel[A][D_BSIZE] = 8192;
		bsdlabel[A][D_FSIZE] = 1024;
		strcpy (fsmount[A], "/");
		partstart += partsize;

		/* swap */
		i = NUMSEC(layoutkind * 2 * (rammb < 32 ? 32 : rammb),
			   MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
			   dlcylsize) - partstart - swapadj;
		bsdlabel[B][D_OFFSET] = partstart;
		bsdlabel[B][D_SIZE] = partsize;
		partstart += partsize;

		/* /usr */
		partsize = fsdsize - partstart;
		bsdlabel[PART_USR][D_FSTYPE] = T_42BSD;
		bsdlabel[PART_USR][D_OFFSET] = partstart;
		bsdlabel[PART_USR][D_SIZE] = partsize;
		bsdlabel[PART_USR][D_BSIZE] = 8192;
		bsdlabel[PART_USR][D_FSIZE] = 1024;
		strcpy (fsmount[PART_USR], "/usr");


		/* Verify Partitions. */
		process_menu (MENU_fspartok);
		break;

	case 3: /* custom: ask user for all sizes */
		ask_sizemult();
		/* root */
		partstart = ptstart;
		remain = fsdsize - partstart;
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
				   dlcylsize) - partstart;
		snprintf (isize, 20, "%d", partsize/sizemult);
		msg_prompt (MSG_askfsroot, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
		bsdlabel[A][D_OFFSET] = partstart;
		bsdlabel[A][D_SIZE] = partsize;
		bsdlabel[A][D_BSIZE] = 8192;
		bsdlabel[A][D_FSIZE] = 1024;
		strcpy (fsmount[A], "/");
		partstart += partsize;
		
		/* swap */
		remain = fsdsize - partstart;
		i = NUMSEC(layoutkind * 2 * (rammb < 32 ? 32 : rammb),
			   MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
			   dlcylsize) - partstart - swapadj;
		snprintf (isize, 20, "%d", partsize/sizemult);
		msg_prompt_add (MSG_askfsswap, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize) - swapadj;
		bsdlabel[B][D_OFFSET] = partstart;
		bsdlabel[B][D_SIZE] = partsize;
		partstart += partsize;
		
		/* /usr */
		remain = fsdsize - partstart;
		partsize = fsdsize - partstart;
		snprintf (isize, 20, "%d", partsize/sizemult);
		msg_prompt_add (MSG_askfsusr, isize, isize, 20,
			    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
		if (remain - partsize < sizemult)
			partsize = remain;
		bsdlabel[PART_USR][D_FSTYPE] = T_42BSD;
		bsdlabel[PART_USR][D_OFFSET] = partstart;
		bsdlabel[PART_USR][D_SIZE] = partsize;
		bsdlabel[PART_USR][D_BSIZE] = 8192;
		bsdlabel[PART_USR][D_FSIZE] = 1024;
		strcpy (fsmount[PART_USR], "/usr");
		partstart += partsize;

		/* Others ... */
		remain = fsdsize - partstart;
		part = F;
		if (remain > 0)
			msg_display (MSG_otherparts);
		while (remain > 0 && part <= H) {
			partsize = fsdsize - partstart;
			snprintf (isize, 20, "%d", partsize/sizemult);
			msg_prompt_add (MSG_askfspart, isize, isize, 20,
					diskdev, partname[part],
					remain/sizemult, multname);
			partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			if (remain - partsize < sizemult)
				partsize = remain;
			bsdlabel[part][D_FSTYPE] = T_42BSD;
			bsdlabel[part][D_OFFSET] = partstart;
			bsdlabel[part][D_SIZE] = partsize;
			bsdlabel[part][D_BSIZE] = 8192;
			bsdlabel[part][D_FSIZE] = 1024;
			msg_prompt_add (MSG_mountpoint, NULL,
					fsmount[part], 20);
			partstart += partsize;
			remain = fsdsize - partstart;
			part++;
		}
		

		/* Verify Partitions. */
		process_menu(MENU_fspartok);
		break;
	}

	/* Disk name */
	msg_prompt (MSG_packname, "mydisk", bsddiskname, 80);

	/* Create the disktab.preinstall */
	run_prog ("cp /etc/disktab.preinstall /etc/disktab");
#ifdef DEBUG
	f = fopen ("/tmp/disktab", "a");
#else
	f = fopen ("/etc/disktab", "a");
#endif
	if (f == NULL) {
		endwin();
		(void) fprintf (stderr, "Could not open /etc/disktab");
		exit (1);
	}
	(void)fprintf (f, "%s|NetBSD installation generated:\\\n", bsddiskname);
	(void)fprintf (f, "\t:dt=%s:ty=winchester:\\\n", disktype);
	(void)fprintf (f, "\t:nc#%d:nt#%d:ns#%d:\\\n", dlcyl, dlhead, dlsec);
	(void)fprintf (f, "\t:sc#%d:su#%d:\\\n", dlhead*dlsec, dlsize);
	(void)fprintf (f, "\t:se#%d:%s\\\n", sectorsize, doessf);
	for (i=0; i<8; i++) {
		(void)fprintf (f, "\t:p%c#%d:o%c#%d:t%c=%s:",
			       'a'+i, bsdlabel[i][D_SIZE],
			       'a'+i, bsdlabel[i][D_OFFSET],
			       'a'+i, fstype[bsdlabel[i][D_FSTYPE]]);
		if (bsdlabel[i][D_FSTYPE] == T_42BSD)
			(void)fprintf (f, "b%c#%d:f%c#%d",
				       'a'+i, bsdlabel[i][D_BSIZE],
				       'a'+i, bsdlabel[i][D_FSIZE]);
		if (i < 7)
			(void)fprintf (f, "\\\n");
		else
			(void)fprintf (f, "\n");
	}
	fclose (f);

}



/*
 * md_copy_filesystem() -- MD hook called  after the target
 * disk's filesystems are newfs'ed (install) or /fsck'ed (upgrade)
 * and mounted.
 * Gives MD code an opportunity to copy data from the install-tools
 * boot disk to the  target disk.  (e.g., on i386, put a copy of the 
 * complete install ramdisk onto the hard disk, so it's at least
 * minimally bootable.)
 *
 * On pmax, we're probably running off a release diskimage.
 * Copy the diskimage to the target disk, since it's probably
 * the  same as the  install sets and it makes the target bootable
 * to standalone.  But don't do anything if the target is
 * already  the current root: we'd clobber the files we're trying to copy.
 */

void	md_copy_filesystem (void)
{
	/*
	 * Make sure any binaries in a diskimage /usr.install get copied 
	 * into the current root's /usr/bin. (may be same as target /usr/bin.)
	 * The rest of sysinst uses /usr/bin/{tar,ftp,chgrp}.
	 * We cannot ship those in /usr/bin, because if we did
	 * an install with target root == current root, they'd
	 * be be hidden under the  target's /usr filesystem.
	 *
	 * Now copy them into the standard  location under /usr.
	 * (the target /usr is already mounted so they always end
	 * up in the correct place.
	 */

	/*  diskimage location of  /usr subset  -- patchable. */
	const char *diskimage_usr = "/usr.install";
	int dir_exists;


	/* test returns 0  on success */
	dir_exists = (run_prog("test -d %s", diskimage_usr) == 0);
	if (dir_exists) {
		run_prog (
		  "tar --one-file-system -cf - -C %s . | tar -xpf - -C /usr",
		  diskimage_usr);
	}

	if (target_already_root()) {

	  	/* The diskimage /usr subset has served its purpose. */
	  	/* (but leave it for now, in case of errors.) */
#if 0
		run_prog("rm -fr %s 2> /dev/null", diskimage_usr);
#endif
		return;
	}

	/* Copy all the diskimage/ramdisk binaries to the target disk. */
	printf ("%s", msg_string(MSG_dotar));
	run_prog ("tar --one-file-system -cf - -C / . |"
		  "(cd /mnt ; tar --unlink -xpf - )");

	/* Make sure target has a copy of install kernel. */
	dup_file_into_target("/netbsd");

	/* Copy next-stage profile into target /.profile. */
	dup_file_into_target ("/tmp/.hdprofile" "/.profile");
}


/*
 * MD hook called after upgrade() or install() hasve finished the 
 * setting up the target disk but immediately before the user is
 * given thte ``disks are now set up'' message, that if power fails,
 * they can continue installation by  booting the target  disk and
 * doing an `upgrade'.
 *
 * On pmax, this is a convenient place to write up-to-date bootblocks 
 * to the target root filesystem.
 */
void	md_post_newfs (void)
{
	/* XXX boot blocks ... */
	printf (msg_string(MSG_dobootblks), diskdev);
	run_prog_or_continue("/sbin/disklabel -B %s /dev/r%sc",
			"-b /usr/mdec/rzboot -s /usr/mdec/bootrz", diskdev);
}



/* Upgrade support */
int
md_update(void)
{
	/* stolen from i386 -- untested */
	endwin();
	md_copy_filesystem ();
	md_post_newfs();
	puts (CL);
	wrefresh(stdscr);

	return 1;
}
