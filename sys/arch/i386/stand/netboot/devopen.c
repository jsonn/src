/*	$NetBSD: devopen.c,v 1.3.2.1 1997/09/22 06:31:49 thorpej Exp $	 */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 */

/*
 * bootfile from tftp overrides! TODO: pass (net) device to net_open
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#include <lib/libsa/net.h>	/* global "bootfile" */

#include <libi386.h>
#include <bootinfo.h>
#include "dev_net.h"

struct btinfo_bootpath bibp;

int
devopen(f, fname, file)
	struct open_file *f;
	const char     *fname;
	char          **file;
{
	struct devsw   *dp;
	int             error = 0;

	dp = &devsw[0];
	f->f_dev = dp;

	if(fname)
		strncpy(bootfile, fname, FNAME_SIZE);

	error = net_open(f);

	if (bootfile[0])
		*file = bootfile;
	else
		*file = (char *) fname;

	strncpy(bibp.bootpath, *file, sizeof(bibp.bootpath));
	BI_ADD(&bibp, BTINFO_BOOTPATH, sizeof(bibp));

	return (error);
}
