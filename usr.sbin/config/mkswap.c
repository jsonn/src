/*	$NetBSD: mkswap.c,v 1.5.2.2 1997/01/20 05:44:24 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	from: @(#)mkswap.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "sem.h"

static int mkoneswap __P((struct config *));

/*
 * Make the various swap*.c files.  Nothing to do for generic swap.
 */
int
mkswap()
{
	register struct config *cf;

	for (cf = allcf; cf != NULL; cf = cf->cf_next)
		if (mkoneswap(cf))
			return (1);
	return (0);
}

static char *
mkdevstr(d)
dev_t d;
{
	static char buf[32];

	if (d == NODEV)
		(void)sprintf(buf, "NODEV");
	else
		(void)sprintf(buf, "makedev(%d, %d)", major(d), minor(d));
	return buf;
}

static int
mkoneswap(cf)
	register struct config *cf;
{
	register struct nvlist *nv;
	register FILE *fp;
	char fname[200];
	char rootinfo[200];

	(void)sprintf(fname, "swap%s.c", cf->cf_name);
	if ((fp = fopen(fname, "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write %s: %s\n",
		    fname, strerror(errno));
		return (1);
	}
	if (fputs("\
#include <sys/param.h>\n\
#include <sys/conf.h>\n\n", fp) < 0)
		goto wrerror;

	/*
	 * Emit the root device.
	 */
	nv = cf->cf_root;
	if (cf->cf_root->nv_str == s_qmark)
		strcpy(rootinfo, "NULL");
	else
		sprintf(rootinfo, "\"%s\"", cf->cf_root->nv_str);
	if (fprintf(fp, "const char *rootspec = %s;\n", rootinfo) < 0)
		goto wrerror;
	if (fprintf(fp, "dev_t\trootdev = %s;\t/* %s */\n",
	    mkdevstr(nv->nv_int),
	    nv->nv_str == s_qmark ? "wildcarded" : nv->nv_str) < 0)
		goto wrerror;

	/*
	 * Emit the dump device.
	 */
	nv = cf->cf_dump;
	if (fprintf(fp, "dev_t\tdumpdev = %s;\t/* %s */\n",
	    nv ? mkdevstr(nv->nv_int) : "NODEV",
	    nv ? (nv->nv_str ? nv->nv_str : "none") : "unspecified") < 0)
		goto wrerror;

	/*
	 * Emit the swap devices.  If swap is unspecified (i.e. wildcarded
	 * root device), leave room for it to be filled in at run-time.
	 */
	if (fputs("\nstruct\tswdevt swdevt[] = {\n", fp) < 0)
		goto wrerror;
	if (cf->cf_swap == NULL) {
		if (fputs("\t{ NODEV, 0, 0 },\t/* unspecified */\n", fp) < 0)
			goto wrerror;
	} else if (cf->cf_swap->nv_str == NULL) {
		if (fputs("\t{ NODEV, 0, 0 },\t/* none */\n", fp) < 0)
			goto wrerror;
	} else {
		for (nv = cf->cf_swap; nv != NULL; nv = nv->nv_next)
			if (fprintf(fp, "\t{ %s,\t0,\t0 },\t/* %s */\n",
			    mkdevstr(nv->nv_int), nv->nv_str) < 0)
				goto wrerror;
	}
	if (fputs("\t{ NODEV, 0, 0 }\n};\n\n", fp) < 0)
		goto wrerror;

	/*
	 * Emit the root file system.
	 */
	if (cf->cf_fstype == NULL)
		strcpy(rootinfo, "NULL");
	else {
		sprintf(rootinfo, "%s_mountroot", cf->cf_fstype);
		if (fprintf(fp, "extern int %s __P((void));\n", rootinfo) < 0)
			goto wrerror;
	}
	if (fprintf(fp, "int (*mountroot) __P((void)) = %s;\n", rootinfo) < 0)
		goto wrerror;

	if (fclose(fp)) {
		fp = NULL;
		goto wrerror;
	}
	return (0);
wrerror:
	(void)fprintf(stderr, "config: error writing %s: %s\n",
	    fname, strerror(errno));
	if (fp != NULL)
		(void)fclose(fp);
	/* (void)unlink(fname); */
	return (1);
}
