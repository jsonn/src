/*	$NetBSD: boot.c,v 1.1.8.2 2001/06/14 12:57:17 fredette Exp $ */

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/mon.h>

#include <stand.h>
#include <loadfile.h>
#include "libsa.h"

/*
 * Default the name (really tape segment number).
 * The defaults assume the following tape layout:
 *   segment 0:  tapeboot
 *   segment 1:  netbsd.sun3  (RAMDISK3)
 *   segment 2:  netbsd.sun3x (RAMDISK3X)
 *   segment 3:  miniroot image
 * Therefore, the default name is "1" or "2"
 * for sun3 and sun3x respectively.
 */

char	defname[32] = "1";
char	line[80];

int
main()
{
	char *cp, *file;
	void *entry;
	u_long marks[MARK_MAX];
	int fd;

	printf(">> %s tapeboot [%s]\n", bootprog_name, bootprog_rev);
	prom_get_boot_info();

	/*
	 * Can not hold open the tape device as is done
	 * in the other boot programs because it does
	 * its position-to-segment on open.
	 */

	/* If running on a Sun3X, use segment 2. */
	if (_is3x)
		defname[0] = '2';
	file = defname;

	cp = prom_bootfile;
	if (cp && *cp)
		file = cp;

	for (;;) {
		if (prom_boothow & RB_ASKNAME) {
			printf("tapeboot: segment? [%s]: ", defname);
			gets(line);
			if (line[0])
				file = line;
			else
				file = defname;
		} else
			printf("tapeboot: loading segment %s\n", file);

		marks[MARK_START] = KERN_LOADADDR;
		if ((fd = loadfile(file, marks, LOAD_KERNEL)) != -1) {
			break;
		}
		printf("tapeboot: segment %s: %s\n", file, strerror(errno));
		prom_boothow |= RB_ASKNAME;
	}
	close(fd);

	entry = (void *)marks[MARK_ENTRY];
	printf("Starting program at 0x%x\n", entry);
	chain_to(entry);
}
