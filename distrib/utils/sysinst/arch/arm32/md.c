/*	$NetBSD: md.c,v 1.1.2.2 1997/12/04 11:44:50 jonathan Exp $	*/

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

static u_int
filecore_checksum(u_char *bootblock);

/*
 * symbolic names for disk partitions
 */
#define PART_ROOT A
#define PART_RAW  C
#define PART_USR  E

/*
 * static u_int filecore_checksum(u_char *bootblock)
 *
 * Calculates the filecore boot block checksum. This is used to validate
 * a filecore boot block on the disc. If a boot block is validated then
 * it is used to locate the partition table. If the boot block is not
 * validated, it is assumed that the whole disc is NetBSD.
 */

/*
 * This can be coded better using add with carry but as it is used rarely
 * there is not much point writing it in assembly.
 */
 
static u_int
filecore_checksum(bootblock)
	u_char *bootblock;
{
	u_int sum;
	u_int loop;
    
	sum = 0;

	for (loop = 0; loop < 512; ++loop)
		sum += bootblock[loop];

	if (sum == 0) return(0xffff);

	sum = 0;
    
	for (loop = 0; loop < 511; ++loop) {
		sum += bootblock[loop];
		if (sum > 255)
			sum -= 255;
	}

	return(sum);
}


int	md_get_info (void)
{	struct disklabel disklabel;
	int fd;
	char devname[100];
	static char bb[DEV_BSIZE];
	struct filecore_bootblock *fcbb = (struct filecore_bootblock *)bb;
	int offset = 0;

	if (strncmp(disk->name, "wd", 2) == 0)
		disktype = "ST506";
	else
		disktype = "SCSI";

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

	if (lseek(fd, (off_t)FILECORE_BOOT_SECTOR * DEV_BSIZE, SEEK_SET) < 0 ||
	    read(fd, bb, sizeof(bb)) < sizeof(bb)) {
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

		offset = (fcbb->partition_cyl_low + (fcbb->partition_cyl_high << 8))
		    * fcbb->heads * fcbb->secspertrack;

		if (fcbb->partition_type == PARTITION_FORMAT_RISCBSD)
			;
		else if (fcbb->partition_type == PARTITION_FORMAT_RISCIX) {
			/*
     			 * Ok we need to read the RISCiX partition table and
			 * search for a partition named RiscBSD, NetBSD or
			 * Empty:
			 */

			struct riscix_partition_table *riscix_part = (struct riscix_partition_table *)bb;
			int loop;

			if (lseek(fd, (off_t)offset * DEV_BSIZE, SEEK_SET) < 0 ||
			    read(fd, bb, sizeof(bb)) < sizeof(bb)) {
				endwin();
				fprintf(stderr, msg_string(MSG_badreadriscix));
				close(fd);
				exit(1);
			}
			/* Break out as soon as we find a suitable partition */

			for (loop = 0; loop < NRISCIX_PARTITIONS; ++loop) {
				if (strcmp(riscix_part->partitions[loop].rp_name, "RiscBSD") == 0
				    || strcmp(riscix_part->partitions[loop].rp_name, "NetBSD") == 0
				    || strcmp(riscix_part->partitions[loop].rp_name, "Empty:") == 0) {
					offset = riscix_part->partitions[loop].rp_start;
					break;
				}
			}
			if (loop == NRISCIX_PARTITIONS) {
				/*
				 * Valid filecore boot block, RISCiX partition table
				 * but no NetBSD partition. We should leave this disc alone.
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

	return 1;
}

void	md_pre_disklabel (void)
{
}

void	md_post_disklabel (void)
{
}

void	md_post_newfs (void)
{
#if 0
	/* XXX boot blocks ... */
	printf (msg_string(MSG_dobootblks), diskdev);
	run_prog_or_continue("/sbin/disklabel -B %s /dev/r%sc",
			"-b /usr/mdec/rzboot -s /usr/mdec/bootrz", diskdev);
#endif
}

void	md_copy_filesystem (void)
{
	/* Copy the instbin(s) to the disk */
	printf ("%s", msg_string(MSG_dotar));
	run_prog ("tar --one-file-system -cf - / |"
		  "(cd /mnt ; tar --unlink -xpf - )");
	run_prog ("/bin/cp /tmp/.hdprofile /mnt/.profile");
}

int md_make_bsd_partitions (void)
{
	FILE *f;
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
	bsdlabel[C][D_FSTYPE] = T_UNUSED;
	bsdlabel[C][D_OFFSET] = 0;
	bsdlabel[C][D_SIZE] = dlsize;
	
	/* Standard fstypes */
	bsdlabel[A][D_FSTYPE] = T_42BSD;
	bsdlabel[B][D_FSTYPE] = T_SWAP;
	/* Conventionally, C is whole disk and D in the non NetBSD bit */
	bsdlabel[D][D_FSTYPE] = T_UNUSED;
	bsdlabel[D][D_OFFSET] = 0;
	bsdlabel[D][D_SIZE]   = ptstart;
	bsdlabel[E][D_FSTYPE] = T_UNUSED;	/* fill out below */
	bsdlabel[F][D_FSTYPE] = T_UNUSED;
	bsdlabel[G][D_FSTYPE] = T_UNUSED;
	bsdlabel[H][D_FSTYPE] = T_UNUSED;


	switch (layoutkind) {
	case 1: /* standard: a root, b swap, c "unused", e /usr */
	case 2: /* standard X: a root, b swap (big), c "unused", e /usr */
		partstart = ptstart;

		/* Root */
#if 0
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
		/* i386 md code uses: */
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
				   dlcylsize) - partstart;
#else
		/* By convention, NetBSD/arm32 uses a 32Mbyte root */
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

		break;

	case 3: /* custom: ask user for all sizes */
		ask_sizemult();
		/* root */
		partstart = ptstart;
		remain = fsdsize - partstart;
#if 0
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
#endif
		partsize = NUMSEC (32, MEG/sectorsize,
				   dlcylsize);
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
		i = NUMSEC(2 * (rammb < 32 ? 32 : rammb),
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
	puts (CL);
	wrefresh(stdscr);
	return 1;
}
