/* $NetBSD: i386.c,v 1.15.2.1 2004/06/22 07:20:18 tron Exp $ */

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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: i386.c,v 1.15.2.1 2004/06/22 07:20:18 tron Exp $");
#endif /* !__lint */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <md5.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

int
i386_setboot(ib_params *params)
{
	int		retval, i, bpbsize;
	uint8_t		*bootstrapbuf;
	u_int		bootstrapsize;
	ssize_t		rv;
	uint32_t	magic;
	struct x86_boot_params	*bp;
	struct mbr_sector	mbr;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	retval = 0;
	bootstrapbuf = NULL;

	/*
	 * There is only 8k of space in a UFSv1 partition (and ustarfs)
	 * so ensure we don't splat over anything important.
	 */
	if (params->s1stat.st_size > 8192) {
		warnx("stage1 bootstrap `%s' is larger than 8192 bytes",
			params->stage1);
		goto done;
	}

	/*
	 * Read in the existing MBR.
	 */
	rv = pread(params->fsfd, &mbr, sizeof(mbr), MBR_BBSECTOR);
	if (rv == -1) {
		warn("Reading `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof(mbr)) {
		warnx("Reading `%s': short read", params->filesystem);
		goto done;
	}
	if (mbr.mbr_magic != le16toh(MBR_MAGIC)) {
		if (params->flags & IB_VERBOSE) {
			printf(
		    "Ignoring MBR with invalid magic in sector 0 of `%s'\n",
			    params->filesystem);
		}
		memset(&mbr, 0, sizeof(mbr));
	}

	/*
	 * Allocate a buffer, with space to round up the input file
	 * to the next block size boundary, and with space for the boot
	 * block.
	 */
	bootstrapsize = roundup(params->s1stat.st_size, 512);

	bootstrapbuf = malloc(bootstrapsize);
	if (bootstrapbuf == NULL) {
		warn("Allocating %u bytes",  bootstrapsize);
		goto done;
	}
	memset(bootstrapbuf, 0, bootstrapsize);

	/*
	 * Read the file into the buffer.
	 */
	rv = pread(params->s1fd, bootstrapbuf, params->s1stat.st_size, 0);
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		goto done;
	} else if (rv != params->s1stat.st_size) {
		warnx("Reading `%s': short read", params->stage1);
		goto done;
	}

	magic = *(uint32_t *)(bootstrapbuf + 512 * 2 + 4);
	if (magic != htole32(X86_BOOT_MAGIC_1)) {
		warnx("Invalid magic in stage1 boostrap %x != %x",
			magic, htole32(X86_BOOT_MAGIC_1));
		goto done;
	}

	/*
	 * Determine size of BIOS Parameter Block (BPB) to copy from
	 * original MBR to the temporary buffer by examining the first
	 * few instruction in the new bootblock.  Supported values:
	 *	eb 3c 90	jmp ENDOF(mbr_bpbFAT16)+1, nop
	 *	eb 58 90	jmp ENDOF(mbr_bpbFAT32)+1, nop
	 *      (anything else)	; don't preserve
	 */
	bpbsize = 0;
	if (bootstrapbuf[0] == 0xeb && bootstrapbuf[2] == 0x90 &&
	    (bootstrapbuf[1] == 0x3c || bootstrapbuf[1] == 0x58))
		bpbsize = bootstrapbuf[1] + 2 - MBR_BPB_OFFSET;

	/*
	 * Ensure bootxx hasn't got any code or data (i.e, non-zero bytes) in
	 * the partition table.
	 */
	for (i = 0; i < sizeof(mbr.mbr_parts); i++) {
		if (*(uint8_t *)(bootstrapbuf + MBR_PART_OFFSET + i) != 0) {
			warnx(
		    "Partition table has non-zero byte at offset %d in `%s'",
			    MBR_PART_OFFSET + i, params->stage1);
			goto done;
		}
	}

	/*
	 * Copy the BPB and the partition table from the original MBR to the
	 * temporary buffer so that they're written back to the fs.
	 */
	if (bpbsize != 0) {
		if (params->flags & IB_VERBOSE)
			printf("Preserving %d (%#x) bytes of the BPB\n",
			    bpbsize, bpbsize);
		memcpy(bootstrapbuf + MBR_BPB_OFFSET, &mbr.mbr_bpb, bpbsize);
	}
	memcpy(bootstrapbuf + MBR_PART_OFFSET, &mbr.mbr_parts,
	    sizeof(mbr.mbr_parts));

	/*
	 * Fill in any user-specified options into the
	 *	struct x86_boot_params
	 * that's 8 bytes in from the start of the third sector.
	 * See sys/arch/i386/stand/bootxx/bootxx.S for more information.
	 */
	bp = (void *)(bootstrapbuf + 512 * 2 + 8);
	if (le32toh(bp->bp_length) < sizeof *bp) {
		warnx("Patch area in stage1 bootstrap is too small");
		goto done;
	}
	if (params->flags & IB_TIMEOUT)
		bp->bp_timeout = htole32(params->timeout);
	if (params->flags & IB_RESETVIDEO)
		bp->bp_flags |= htole32(X86_BP_FLAGS_RESET_VIDEO);
	if (params->flags & IB_CONSPEED)
		bp->bp_conspeed = htole32(params->conspeed);
	if (params->flags & IB_CONSOLE) {
		static const char *names[] = {
			"pc", "com0", "com1", "com2", "com3",
			"com0kbd", "com1kbd", "com2kbd", "com3kbd",
			NULL };
		for (i = 0; ; i++) {
			if (names[i] == NULL) {
				warnx("invalid console name, valid names are:");
				fprintf(stderr, "\t%s", names[0]);
				for (i = 1; names[i] != NULL; i++)
					fprintf(stderr, ", %s", names[i]);
				fprintf(stderr, "\n");
				goto done;
			}
			if (strcmp(names[i], params->console) == 0)
				break;
		}
		bp->bp_consdev = htole32(i);
	}
	if (params->flags & IB_PASSWORD) {
		MD5_CTX md5ctx;
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, params->password, strlen(params->password));
		MD5Final(bp->bp_password, &md5ctx);
		bp->bp_flags |= htole32(X86_BP_FLAGS_PASSWORD);
	}
	if (params->flags & IB_KEYMAP)
		strlcpy(bp->bp_keymap, params->keymap, sizeof bp->bp_keymap);

	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	/*
	 * Write MBR code to sector zero.
	 */
	rv = pwrite(params->fsfd, bootstrapbuf, 512, 0);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != 512) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	}

	/*
	 * Skip disklabel in sector 1 and write bootxx to sectors 2..N.
	 */
	rv = pwrite(params->fsfd, bootstrapbuf + 512 * 2,
		    bootstrapsize - 512 * 2, 512 * 2);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != bootstrapsize - 512 * 2) {
		warnx("Writing `%s': short write", params->filesystem);
		goto done;
	}

	retval = 1;

 done:
	if (bootstrapbuf)
		free(bootstrapbuf);
	return retval;
}
