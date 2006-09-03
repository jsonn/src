/*	$NetBSD: boot1.c,v 1.1.2.2 2006/09/03 15:23:20 yamt Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
__RCSID("$NetBSD: boot1.c,v 1.1.2.2 2006/09/03 15:23:20 yamt Exp $");

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <sys/param.h>
#include <sys/bootblock.h>
#include <sys/disklabel.h>
#include <dev/raidframe/raidframevar.h>	/* For RF_PROTECTED_SECTORS */

#include "biosdisk_ll.h"

#define	XSTR(x)	#x
#define	STR(x)	XSTR(x)

static uint32_t bios_sector;

const char *boot1(uint32_t *);
void putstr(const char *str);
int raise(int sig);

extern struct disklabel ptn_disklabel;

const char *
boot1(uint32_t *sector)
{
        struct stat sb;
	int fd;

	bios_sector = *sector;

	putstr("\r\nNetBSD/" MACHINE " " STR(FS) " Primary Bootstrap\r\n");

	do {
		/*
		 * We default to the filesystem at the start of the
		 * MBR partition
		 */
		fd = open("boot", 0);
		if (fd != -1)
			break;

		/*
		 * Maybe the filesystem is enclosed in a raid set.
		 * add in size of raidframe header and try again.
		 * (Maybe this should only be done if the filesystem
		 * magic number is absent.)
		 */
		bios_sector += RF_PROTECTED_SECTORS;
		fd = open("boot", 0);
		if (fd != -1)
			break;

		/*
		 * Nothing at the start of the MBR partition, fallback on
		 * partition 'a' from the disklabel in this MBR partition.
		 */
		if (ptn_disklabel.d_magic != DISKMAGIC)
			break;
		if (ptn_disklabel.d_magic2 != DISKMAGIC)
			break;
		if (ptn_disklabel.d_partitions[0].p_fstype == FS_UNUSED)
			break;

		bios_sector = ptn_disklabel.d_partitions[0].p_offset;
		*sector = bios_sector;
		if (ptn_disklabel.d_partitions[0].p_fstype == FS_RAID)
			bios_sector += RF_PROTECTED_SECTORS;
		fd = open("boot", 0);
	} while (0);

	if (fd == -1 || fstat(fd, &sb) == -1)
		return "Can't open /boot.\r\n";

#if 0
	if (sb.st_size > SECONDARY_MAX_LOAD)
		return "/boot too large.\r\n";
#endif

	if (read(fd, (void *)SECONDARY_LOAD_ADDRESS, sb.st_size) != sb.st_size)
		return "/boot load failed.\r\n";

	if (*(uint32_t *)(SECONDARY_LOAD_ADDRESS + 4) != LANDISK_BOOT_MAGIC_2)
		return "Invalid /boot file format.\r\n";

	return 0;
}

int
blkdevstrategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{

	if (flag != F_READ)
		return EROFS;

	if (size & (BIOSDISK_SECSIZE - 1))
		return EINVAL;

	if (rsize)
		*rsize = size;

	if (size != 0 && readsects(0x40, bios_sector + dblk, buf,
				   size / BIOSDISK_SECSIZE) != 0)
		return EIO;

	return 0;
}

/*ARGUSED*/
int
raise(int sig)
{

	return 0;
}
