/*	$NetBSD: iwm_mod.c,v 1.6.6.1 2004/08/03 10:53:58 skrll Exp $	*/

/*
 * Copyright (c) 1997, 1998 Hauke Fath.  All rights reserved.
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

/*
 * Sony (floppy disk) driver for Macintosh m68k, module entry.
 * This is derived from Terry Lambert's LKM examples.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iwm_mod.c,v 1.6.6.1 2004/08/03 10:53:58 skrll Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include <sys/errno.h>

/* The module entry */
int iwmfd_lkmentry(struct lkm_table *lkmtp, int cmd, int ver);

static int iwmfd_load(struct lkm_table *lkmtp, int cmd);
static int iwmfd_unload(struct lkm_table *lkmtp, int cmd);

extern int fd_mod_init(void);
extern void fd_mod_free(void);

extern const struct bdevsw fd_bdevsw;
extern const struct cdevsw fd_cdevsw;

MOD_DEV("iwmfd", "fd", &fd_bdevsw, -1, &fd_cdevsw, -1)

/*
 * iwmfd_lkmentry
 *
 * External entry point; should generally match name of .o file.
 */
int
iwmfd_lkmentry (struct lkm_table *lkmtp, int cmd, int ver)
{

	DISPATCH(lkmtp, cmd, ver, iwmfd_load, iwmfd_unload, lkm_nofunc);
}

static int
iwmfd_load(struct lkm_table *lkmtp, int cmd)
{

	return fd_mod_init();
}

static int
iwmfd_unload(struct lkm_table *lkmtp, int cmd)
{

	fd_mod_free();
	return (0);
}
