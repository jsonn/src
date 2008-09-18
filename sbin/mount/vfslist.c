/*	$NetBSD: vfslist.c,v 1.6.24.1 2008/09/18 04:28:26 wrstuden Exp $	*/

/*
 * Copyright (c) 1995
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)vfslist.c	8.1 (Berkeley) 5/8/95";
#else
__RCSID("$NetBSD: vfslist.c,v 1.6.24.1 2008/09/18 04:28:26 wrstuden Exp $");
#endif
#endif /* not lint */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mountprog.h"

static int	  skipvfs;

int
checkvfsname(const char *vfsname, const char **vfslist)
{

	if (vfslist == NULL)
		return (0);
	while (*vfslist != NULL) {
		if (strcmp(vfsname, *vfslist) == 0)
			return (skipvfs);
		++vfslist;
	}
	return (!skipvfs);
}

const char **
makevfslist(const char *fslist)
{
	const char **av;
	size_t i;
	char *nextcp, *fsl;

	if (fslist == NULL)
		return NULL;

	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		skipvfs = 1;
	}
	if ((fsl = strdup(fslist)) == NULL) {
		warn("strdup");
		return NULL;
	}
	for (i = 0, nextcp = fsl; *nextcp; nextcp++)
		if (*nextcp == ',')
			i++;
	if ((av = malloc((i + 2) * sizeof(char *))) == NULL) {
		warn("malloc");
		free(fsl);
		return NULL;
	}
	nextcp = fsl;
	i = 0;
	av[i++] = nextcp;
	while ((nextcp = strchr(nextcp, ',')) != NULL) {
		*nextcp++ = '\0';
		av[i++] = nextcp;
	}
	av[i++] = NULL;
	return av;
}
