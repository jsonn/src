/*	$NetBSD: devnodes.c,v 1.1.2.2 2009/07/23 23:32:54 jym Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: devnodes.c,v 1.1.2.2 2009/07/23 23:32:54 jym Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/vfs_syscalls.h>

#include "rump_dev_private.h"

/* realqvik(tm) "devfs" */
int
rump_dev_makenodes(dev_t devtype, const char *basename, char minchar,
	devmajor_t maj, devminor_t minnum, int nnodes)
{
	int error;
	char *devname, *p;
	size_t devlen;
	register_t retval;

	error = do_sys_mkdir("/dev", 0777);
	if (error != 0 && error != EEXIST)
		return error;

	devlen = strlen("/dev/") + strlen(basename) + 1 + 1; /* +letter +0 */
	devname = kmem_zalloc(devlen, KM_SLEEP);
	strlcpy(devname, "/dev/", devlen);
	strlcat(devname, basename, devlen);
	p = devname + devlen-2;

	for (; nnodes > 0; nnodes--, minchar++, minnum++) {
		KASSERT(minchar <= 'z');
		*p = minchar;

		if ((error = do_sys_mknod(curlwp, devname, 0666 | devtype,
		    makedev(maj, minnum), &retval)))
			goto out;
	}

 out:
	kmem_free(devname, devlen);
	return error;
}
