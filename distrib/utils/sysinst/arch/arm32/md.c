/*	$NetBSD: md.c,v 1.23.4.1 2000/10/18 17:51:19 tv Exp $	*/

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
 *
 */

/* md.c -- arm32 machine specific routines */

#include <stdio.h>
#include <curses.h>
#include <unistd.h>
#include <fcntl.h>
#include <util.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"
void backtowin(void);

static int
filecore_checksum(u_char *bootblock);

/*
 * static int filecore_checksum(u_char *bootblock)
 *
 * Calculates the filecore boot block checksum. This is used to validate
 * a filecore boot block on the disk.  If a boot block is validated then
 * it is used to locate the partition table. If the boot block is not
 * validated, it is assumed that the whole disk is NetBSD.
 *
 * The basic algorithm is:
 *
 *	for (each byte in block, excluding checksum) {
 *		sum += byte;
 *		if (sum > 255)
 *			sum -= 255;
 *	}
 *
 * That's equivalent to summing all of the bytes in the block
 * (excluding the checksum byte, of course), then calculating the
 * checksum as "cksum = sum - ((sum - 1) / 255) * 255)".  That
 * expression may or may not yield a faster checksum function,
 * but it's easier to reason about.
 *
 * Note that if you have a block filled with bytes of a single
 * value "X" (regardless of that value!) and calculate the cksum
 * of the block (excluding the checksum byte), you will _always_
 * end up with a checksum of X.  (Do the math; that can be derived
 * from the checksum calculation function!)  That means that
 * blocks which contain bytes which all have the same value will
 * always checksum properly.  That's a _very_ unlikely occurence
 * (probably impossible, actually) for a valid filecore boot block,
 * so we treat such blocks as invalid.
 */

static int
filecore_checksum(bootblock)
	u_char *bootblock;
{  
	u_char byte0, accum_diff;
	u_int sum;
	int i;
 
	sum = 0;
	accum_diff = 0;
	byte0 = bootblock[0];
 
	/*
	 * Sum the contents of the block, keeping track of whether
	 * or not all bytes are the same.  If 'accum_diff' ends up
	 * being zero, all of the bytes are, in fact, the same.
	 */
	for (i = 0; i < 511; ++i) {
		sum += bootblock[i];
		accum_diff |= bootblock[i] ^ byte0;
	}

	/*
	 * Check to see if the checksum byte is the same as the
	 * rest of the bytes, too.  (Note that if all of the bytes
	 * are the same except the checksum, a checksum compare
	 * won't succeed, but that's not our problem.)
	 */
	accum_diff |= bootblock[i] ^ byte0;

	/* All bytes in block are the same; call it invalid. */
	if (accum_diff == 0)
		return (-1);

	return (sum - ((sum - 1) / 255) * 255);
}

int	md_get_info (void)
{	struct disklabel disklabel;
	int fd;
	char devname[100];
	static char bb[DEV_BSIZE];
	struct filecore_bootblock *fcbb = (struct filecore_bootblock *)bb;
	int offset = 0;

	if (strncmp(disk->dd_name, "wd", 2) == 0)
		disktype = "ST506";
	else
		disktype = "SCSI";

	snprintf(devname, 100, "/dev/r%sc", diskdev);

	fd = open(devname, O_RDONLY, 0);
	if (fd < 0) {
		endwin();
		fprintf(stderr, "Can't open %s\n", devname);
		exit(1);
	}
	if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
		endwin();
		fprintf(stderr, "Can't read disklabel on %s.\n", devname);
		close(fd);
		exit(1);
	}

	if (lseek(fd, (off_t)FILECORE_BOOT_SECTOR * DEV_BSIZE, SEEK_SET) < 0
	    || read(fd, bb, sizeof(bb)) < sizeof(bb)) {
		endwin();
		fprintf(stderr, msg_string(MSG_badreadbb));
		close(fd);
		exit(1);
	}

	/* Check if table is valid. */
	if (filecore_checksum(bb) == fcbb->checksum) {
		/*
		 * Check for NetBSD/arm32 (RiscBSD) partition marker.
		 * If found the NetBSD disklabel location is easy.
		 */

		offset = (fcbb->partition_cyl_low +
		    (fcbb->partition_cyl_high << 8)) *
		    fcbb->heads * fcbb->secspertrack;

		if (fcbb->partition_type == PARTITION_FORMAT_RISCBSD)
			;
		else if (fcbb->partition_type == PARTITION_FORMAT_RISCIX) {
			/*
     			 * Ok we need to read the RISCiX partition table and
			 * search for a partition named RiscBSD, NetBSD or
			 * Empty:
			 */

			struct riscix_partition_table *riscix_part =
			    (struct riscix_partition_table *)bb;
			struct riscix_partition *part;
			int loop;

			if (lseek(fd, (off_t)offset * DEV_BSIZE, SEEK_SET) < 0
			    || read(fd, bb, sizeof(bb)) < sizeof(bb)) {
				endwin();
				fprintf(stderr, msg_string(MSG_badreadriscix));
				close(fd);
				exit(1);
			}

			/* Break out as soon as we find a suitable partition */
			for (loop = 0; loop < NRISCIX_PARTITIONS; ++loop) {
				part = &riscix_part->partitions[loop];
				if (strcmp(part->rp_name, "RiscBSD") == 0
				    || strcmp(part->rp_name, "NetBSD") == 0
				    || strcmp(part->rp_name, "Empty:") == 0) {
					offset = part->rp_start;
					break;
				}
			}
			if (loop == NRISCIX_PARTITIONS) {
				/*
				 * Valid filecore boot block, RISCiX partition
				 * table but no NetBSD partition. We should
				 * leave this disc alone.
				 */
				endwin();
				fprintf(stderr, msg_string(MSG_notnetbsdriscix));
				close(fd);
				exit(1);
			}
		} else {
			/*
			 * Valid filecore boot block and no non-ADFS partition.
			 * This means that the whole disc is allocated for ADFS 
			 * so do not trash ! If the user really wants to put a
			 * NetBSD disklabel on the disc then they should remove
			 * the filecore boot block first with dd.
			 */
			endwin();
			fprintf(stderr, msg_string(MSG_notnetbsd));
			close(fd);
			exit(1);
		}
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

	ptstart = offset;
/*	endwin();
	printf("dlcyl=%d\n", dlcyl);
	printf("dlhead=%d\n", dlhead);
	printf("dlsec=%d\n", dlsec);
	printf("secsz=%d\n", sectorsize);
	printf("cylsz=%d\n", dlcylsize);
	printf("dlsz=%d\n", dlsize);
	printf("pstart=%d\n", ptstart);
	printf("pstart=%d\n", partsize);
	printf("secpun=%d\n", disklabel.d_secperunit);
	backtowin();*/

	return 1;
}

int	md_pre_disklabel (void)
{
	return 0;
}

int	md_post_disklabel (void)
{
	return 0;
}

int	md_post_newfs (void)
{
#if 0
	/* XXX boot blocks ... */
	printf(msg_string(MSG_dobootblks), diskdev);
	run_prog(RUN_DISPLAY, NULL, "/sbin/disklabel -B %s /dev/r%sc",
	    "-b /usr/mdec/rzboot -s /usr/mdec/bootrz", diskdev);
#endif
	return 0;
}

int	md_copy_filesystem (void)
{
	return 0;
}

int md_make_bsd_partitions (void)
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

/*	endwin();
	printf("ptsize=%d\n", ptsize);
	printf("fsdsize=%d\n", fsdsize);
	printf("fsptsize=%d\n", fsptsize);
	printf("fsdmb=%d\n", fsdmb);
	backtowin();*/

/*editlab:*/
	/* Ask for layout type -- standard or special */
	msg_display (MSG_layout,
			(1.0*fsptsize*sectorsize)/MEG,
			(1.0*minfsdmb*sectorsize)/MEG,
			(1.0*minfsdmb*sectorsize)/MEG+rammb+XNEEDMB);
	process_menu(MENU_layout);

	if (layoutkind == 3) {
		ask_sizemult(dlcylsize);
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
	bsdlabel[B].pi_fstype = FS_SWAP;
	/* Conventionally, C is whole disk and D in the non NetBSD bit */
	bsdlabel[D].pi_fstype = FS_UNUSED;
	bsdlabel[D].pi_offset = 0;
	bsdlabel[D].pi_size   = ptstart;
/*	if (ptstart > 0)
		bsdlabel[D].pi_fstype = T_FILECORE;*/
	bsdlabel[E].pi_fstype = FS_UNUSED;	/* fill out below */
	bsdlabel[F].pi_fstype = FS_UNUSED;
	bsdlabel[G].pi_fstype = FS_UNUSED;
	bsdlabel[H].pi_fstype = FS_UNUSED;


	switch (layoutkind) {
	case 1: /* standard: a root, b swap, c/d "unused", e /usr */
	case 2: /* standard X: a root, b swap (big), c/d "unused", e /usr */
		partstart = ptstart;

		/* Root */
		i = NUMSEC(24+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC(i/(MEG/sectorsize)+1, MEG/sectorsize,
		    dlcylsize) - partstart;
		bsdlabel[A].pi_offset = partstart;
		bsdlabel[A].pi_size = partsize;
		bsdlabel[A].pi_bsize = 8192;
		bsdlabel[A].pi_fsize = 1024;
		strcpy(fsmount[A], "/");
		partstart += partsize;

		/* swap */
		i = NUMSEC(layoutkind * 2 * (rammb < 32 ? 32 : rammb),
		    MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC(i/(MEG/sectorsize)+1, MEG/sectorsize,
		    dlcylsize) - partstart;
		bsdlabel[B].pi_offset = partstart;
		bsdlabel[B].pi_size = partsize;
		partstart += partsize;

		/* /usr */
		partsize = fsptsize - (partstart - ptstart);
		bsdlabel[E].pi_fstype = FS_BSDFFS;
		bsdlabel[E].pi_offset = partstart;
		bsdlabel[E].pi_size = partsize;
		bsdlabel[E].pi_bsize = 8192;
		bsdlabel[E].pi_fsize = 1024;
		strcpy(fsmount[E], "/usr");

		break;

	case 3: /* custom: ask user for all sizes */
		ask_sizemult(dlcylsize);
		partstart = ptstart;
		remain = fsptsize;

		/* root */
		i = NUMSEC(24+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC(i/(MEG/sectorsize)+1, MEG/sectorsize,
		    dlcylsize) - partstart;
		snprintf(isize, 20, "%d", partsize / sizemult);
		msg_prompt(MSG_askfsroot, isize, isize, 20,
		    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize), sizemult, dlcylsize);
		bsdlabel[A].pi_offset = partstart;
		bsdlabel[A].pi_size = partsize;
		bsdlabel[A].pi_bsize = 8192;
		bsdlabel[A].pi_fsize = 1024;
		strcpy(fsmount[A], "/");
		partstart += partsize;
		remain -= partsize;
	
		/* swap */
		i = NUMSEC(4 * (rammb < 32 ? 32 : rammb),
		    MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC(i/(MEG/sectorsize)+1, MEG/sectorsize,
		    dlcylsize) - partstart;
		snprintf(isize, 20, "%d", partsize/sizemult);
		msg_prompt_add(MSG_askfsswap, isize, isize, 20,
		    remain/sizemult, multname);
		partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
		bsdlabel[B].pi_offset = partstart;
		bsdlabel[B].pi_size = partsize;
		partstart += partsize;
		remain -= partsize;
		
		/* Others E, F, G, H */
		part = E;
		if (remain > 0)
			msg_display(MSG_otherparts);
		while (remain > 0 && part <= H) {
			partsize = remain;
			snprintf (isize, 20, "%d", partsize/sizemult);
			msg_prompt_add(MSG_askfspart, isize, isize, 20,
			    diskdev, partition_name(part), remain/sizemult, multname);
			partsize = NUMSEC(atoi(isize),sizemult, dlcylsize);
			if (partsize > 0) {
				if (remain - partsize < sizemult)
					partsize = remain;
				bsdlabel[part].pi_fstype = FS_BSDFFS;
				bsdlabel[part].pi_offset = partstart;
				bsdlabel[part].pi_size = partsize;
				bsdlabel[part].pi_bsize = 8192;
				bsdlabel[part].pi_fsize = 1024;
				if (part == E)
					strcpy(fsmount[E], "/usr");
				msg_prompt_add(MSG_mountpoint, fsmount[part],
				    fsmount[part], 20);
				partstart += partsize;
				remain -= partsize;
			}
			part++;
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
	msg_prompt(MSG_packname, "mydisk", bsddiskname, DISKNAME_SIZE);

	/* save label to disk for MI code to update. */
	(void) savenewlabel(bsdlabel, 8);	/* save 8-partition label */

	/* Everything looks OK. */
	return (1);
}


/* Upgrade support */
int
md_update(void)
{
	endwin();
	md_copy_filesystem();
	md_post_newfs();
	puts(CL);		/* XXX */
	wclear(stdscr);
	wrefresh(stdscr);
	return 1;
}

void
md_cleanup_install(void)
{
#ifndef DEBUG
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
	run_prog(RUN_FATAL, NULL, "mv -f %s %s", realto, realfrom);
	run_prog(0, NULL, "rm -f %s", target_expand("/sysinst"));
	run_prog(0, NULL, "rm -f %s", target_expand("/.termcap"));
	run_prog(0, NULL, "rm -f %s", target_expand("/.profile"));
#endif
}

int
md_pre_update()
{
	return 1;
}

void
md_init()
{
}
