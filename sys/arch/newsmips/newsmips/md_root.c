/*	$NetBSD: md_root.c,v 1.1.2.2 1999/11/15 00:38:49 fvdl Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <dev/md.h>

#include "opt_mdsize.h"

extern int boothowto;

#ifndef MINIROOTSIZE
#define MINIROOTSIZE 512
#endif

#define ROOTBYTES (MINIROOTSIZE << DEV_BSHIFT)

/*
 * This array will be patched to contain a file-system image.
 */
u_int32_t md_root_size = ROOTBYTES;
char md_root_image[ROOTBYTES] = "|This is the root ramdisk!\n";

/*
 * This is called during autoconfig.
 */
void
md_attach_hook(unit, md)
	int unit;
	struct md_conf *md;
{

	if (unit == 0) {
		/* Setup root ramdisk */
		md->md_addr = (caddr_t)md_root_image;
		md->md_size = (size_t)md_root_size;
		md->md_type = MD_KMEM_FIXED;
		printf("md%d: internal %dK image area\n", unit,
		    ROOTBYTES / 1024);
	}
}

/*
 * This is called during open (i.e. mountroot)
 */
void
md_open_hook(unit, md)
	int unit;
	struct md_conf *md;
{

	if (unit == 0) {
		/* The root ramdisk only works single-user. */
		boothowto |= RB_SINGLE;
	}
}
