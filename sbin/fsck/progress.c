/*	$NetBSD: progress.c,v 1.1.2.1 2005/05/11 12:22:16 tron Exp $	*/

/*-
 * Copyright (c) 1997-2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn; by Chris Gilbert; and by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#ifndef SMALL
#include <sys/cdefs.h>
__RCSID("$NetBSD: progress.c,v 1.1.2.1 2005/05/11 12:22:16 tron Exp $");

/*
 * File system independent fsck progress bar routines.
 */

#include <sys/param.h>
#include <sys/tty.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "progress.h"

static int	ttywidth = 80;

static int	progress_onoff;
static off_t	progress_current;
static off_t	progress_total;

#define	BUFLEFT		(sizeof(buf) - len)

void
progress_switch(int onoff)
{
	progress_onoff = onoff;
}

void
progress_init(off_t total)
{
	progress_current = 0;
	progress_total = total;
}

void
progress_add(off_t amt)
{
	progress_current += amt;
}

void
progress_bar(const char *dev, const char *label, off_t current, off_t total)
{
	static int lastpercentage = -1;
	char buf[256];
	int len, percentage;
	int barlength;
	int i;
	int lengthextras;

#define	BAROVERHEAD	10	/* non-* portion of progress bar */

	/*
	 * starts should contain at least sizeof(buf) - BAROVERHEAD
	 * entries.
	 */
	static const char stars[] =
"*****************************************************************************"
"*****************************************************************************"
"*****************************************************************************";

	if (progress_onoff == 0)
		return;

	current += progress_current;
	total += progress_total;

	len = 0;
	lengthextras = strlen(dev) + (label != NULL ? strlen(label) : 0);
	percentage = (current * 100) / total;
	percentage = MAX(percentage, 0);
	percentage = MIN(percentage, 100);

	if (percentage == lastpercentage)
		return;
	lastpercentage = percentage;

	len += snprintf(buf + len, BUFLEFT, "%s: ", dev);
	if (label != NULL)
		len += snprintf(buf + len, BUFLEFT, "%s ", label);

	barlength = MIN(sizeof(buf) - 1, ttywidth) - BAROVERHEAD - lengthextras;
	if (barlength > 0) {
		i = barlength * percentage / 100;
		len += snprintf(buf + len, BUFLEFT,
		    "|%.*s%*s| ", i, stars, barlength - i, "");
	}
	len += snprintf(buf + len, BUFLEFT, "%3d%%\r", percentage);
	write(fileno(stdout), buf, len);
}

void
progress_done(void)
{
	char buf[256];
	int len;

	if (progress_onoff == 0)
		return;

	len = MIN(sizeof(buf) - 2, ttywidth);
	memset(buf, ' ', len);
	buf[len] = '\r';
	buf[len + 1] = '\0';
	write(fileno(stdout), buf, len + 1);
}

void
progress_ttywidth(int a)
{
	struct winsize winsize;
	int oerrno = errno;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1 &&
	    winsize.ws_col != 0)
	    	ttywidth = winsize.ws_col;
	else
		ttywidth = 80;
	errno = oerrno;
}

#endif /* ! SMALL */
