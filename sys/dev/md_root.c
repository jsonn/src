/*	$NetBSD: md_root.c,v 1.1.4.3 2002/01/08 00:29:18 nathanw Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
__KERNEL_RCSID(0, "$NetBSD: md_root.c,v 1.1.4.3 2002/01/08 00:29:18 nathanw Exp $");

#include "opt_md.h"
#include "opt_mdsize.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <dev/md.h>

extern int boothowto;

#ifdef MEMORY_DISK_DYNAMIC
size_t md_root_size;
char *md_root_image;
#else /* MEMORY_DISK_DYNAMIC */

#ifndef MINIROOTSIZE
#define MINIROOTSIZE 512
#endif
#define ROOTBYTES (MINIROOTSIZE << DEV_BSHIFT)

/*
 * This array will be patched to contain a file-system image.
 * See the program mdsetimage(8) for details.
 */
u_int32_t md_root_size = ROOTBYTES;
char md_root_image[ROOTBYTES] = "|This is the root ramdisk!\n";
#endif /* MEMORY_DISK_DYNAMIC */

#ifdef MEMORY_DISK_DYNAMIC
void
md_root_setconf(char *addr, size_t size)
{
	md_root_image = addr;
	md_root_size = size;
}
#endif /* MEMORY_DISK_DYNAMIC */

/*
 * This is called during pseudo-device attachment.
 */
void
md_attach_hook(int unit, struct md_conf *md)
{
	char pbuf[9];

	if (unit == 0) {
		/* Setup root ramdisk */
		md->md_addr = (caddr_t)md_root_image;
		md->md_size = (size_t)md_root_size;
		md->md_type = MD_KMEM_FIXED;
		format_bytes(pbuf, sizeof(pbuf), md->md_size);
		printf("md%d: internal %s image area\n", unit, pbuf);
	}
}

/*
 * This is called during open (i.e. mountroot)
 */
void
md_open_hook(int unit, struct md_conf *md)
{

	if (unit == 0) {
		/* The root ramdisk only works single-user. */
		boothowto |= RB_SINGLE;
	}
}
