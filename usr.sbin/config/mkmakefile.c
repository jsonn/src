/*	$NetBSD: mkmakefile.c,v 1.44.4.1 2001/02/26 17:07:03 he Exp $	*/

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
 *	from: @(#)mkmakefile.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "sem.h"

/*
 * Make the Makefile.
 */

static const char *srcpath __P((struct files *)); 

static const char *prefix_prologue __P((const char *));

static int emitdefs __P((FILE *));
static int emitfiles __P((FILE *, int, int));

static int emitobjs __P((FILE *));
static int emitcfiles __P((FILE *));
static int emitsfiles __P((FILE *));
static int emitrules __P((FILE *));
static int emitload __P((FILE *));
static int emitincludes __P((FILE *));

int
mkmakefile()
{
	FILE *ifp, *ofp;
	int lineno;
	int (*fn) __P((FILE *));
	char *ifname;
	char line[BUFSIZ], buf[200];

	(void)sprintf(buf, "arch/%s/conf/Makefile.%s", machine, machine);
	ifname = sourcepath(buf);
	if ((ifp = fopen(ifname, "r")) == NULL) {
		(void)fprintf(stderr, "config: cannot read %s: %s\n",
		    ifname, strerror(errno));
		free(ifname);
		return (1);
	}
	if ((ofp = fopen("Makefile", "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write Makefile: %s\n",
		    strerror(errno));
		free(ifname);
		return (1);
	}
	if (emitdefs(ofp) != 0)
		goto wrerror;
	lineno = 0;
	while (fgets(line, sizeof(line), ifp) != NULL) {
		lineno++;
		if (line[0] != '%') {
			if (fputs(line, ofp) < 0)
				goto wrerror;
			continue;
		}
		if (strcmp(line, "%OBJS\n") == 0)
			fn = emitobjs;
		else if (strcmp(line, "%CFILES\n") == 0)
			fn = emitcfiles;
		else if (strcmp(line, "%SFILES\n") == 0)
			fn = emitsfiles;
		else if (strcmp(line, "%RULES\n") == 0)
			fn = emitrules;
		else if (strcmp(line, "%LOAD\n") == 0)
			fn = emitload;
		else if (strcmp(line, "%INCLUDES\n") == 0)
			fn = emitincludes;
		else {
			xerror(ifname, lineno,
			    "unknown %% construct ignored: %s", line);
			continue;
		}
		if ((*fn)(ofp))
			goto wrerror;
	}
	if (ferror(ifp)) {
		(void)fprintf(stderr,
		    "config: error reading %s (at line %d): %s\n",
		    ifname, lineno, strerror(errno));
		goto bad;
		/* (void)unlink("Makefile"); */
		free(ifname);
		return (1);
	}
	if (fclose(ofp)) {
		ofp = NULL;
		goto wrerror;
	}
	(void)fclose(ifp);
	free(ifname);
	return (0);
wrerror:
	(void)fprintf(stderr, "config: error writing Makefile: %s\n",
	    strerror(errno));
bad:
	if (ofp != NULL)
		(void)fclose(ofp);
	/* (void)unlink("Makefile"); */
	free(ifname);
	return (1);
}

/*
 * Return (possibly in a static buffer) the name of the `source' for a
 * file.  If we have `options source', or if the file is marked `always
 * source', this is always the path from the `file' line; otherwise we
 * get the .o from the obj-directory.
 */
static const char *
srcpath(fi)
	struct files *fi;
{
#if 1
	/* Always have source, don't support object dirs for kernel builds. */
	return (fi->fi_path);
#else
	static char buf[MAXPATHLEN];

	if (have_source || (fi->fi_flags & FI_ALWAYSSRC) != 0)
		return (fi->fi_path);
	if (objpath == NULL) {
		error("obj-directory not set");
		return (NULL);
	}
	(void)snprintf(buf, sizeof buf, "%s/%s.o", objpath, fi->fi_base);
	return (buf);
#endif
}

static const char *
prefix_prologue(path)
	const char *path;
{

	if (*path == '/')
		return ("");
	else
		return ("$S/");
}

static int
emitdefs(fp)
	FILE *fp;
{
	struct nvlist *nv;
	char *sp;

	if (fputs("IDENT=", fp) < 0)
		return (1);
	sp = "";
	for (nv = options; nv != NULL; nv = nv->nv_next) {

		/* skip any options output to a header file */
		if (DEFINED_OPTION(nv->nv_name))
			continue;
		if (fprintf(fp, "%s-D%s", sp, nv->nv_name) < 0)
		    return 1;
		if (nv->nv_str)
		    if (fprintf(fp, "=\"%s\"", nv->nv_str) < 0)
			return 1;
		sp = " ";
	}
	if (putc('\n', fp) < 0)
		return (1);
	if (fprintf(fp, "PARAM=-DMAXUSERS=%d\n", maxusers) < 0)
		return (1);
	if (*srcdir == '/' || *srcdir == '.') {
		if (fprintf(fp, "S=\t%s\n", srcdir) < 0)
			return (1);
	} else {
		/*
		 * libkern and libcompat "Makefile.inc"s want relative S
		 * specification to begin with '.'.
		 */
		if (fprintf(fp, "S=\t./%s\n", srcdir) < 0)
			return (1);
	}
	for (nv = mkoptions; nv != NULL; nv = nv->nv_next)
		if (fprintf(fp, "%s=%s\n", nv->nv_name, nv->nv_str) < 0)
			return (1);
	return (0);
}

static int
emitobjs(fp)
	FILE *fp;
{
	struct files *fi;
	struct objects *oi;
	int lpos, len, sp;

	if (fputs("OBJS=", fp) < 0)
		return (1);
	sp = '\t';
	lpos = 7;
	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		len = strlen(fi->fi_base) + 2;
		if (lpos + len > 72) {
			if (fputs(" \\\n", fp) < 0)
				return (1);
			sp = '\t';
			lpos = 7;
		}
		if (fprintf(fp, "%c%s.o", sp, fi->fi_base) < 0)
			return (1);
		lpos += len + 1;
		sp = ' ';
	}
	for (oi = allobjects; oi != NULL; oi = oi->oi_next) {
		if ((oi->oi_flags & OI_SEL) == 0)
			continue;
		len = strlen(oi->oi_path);
		if (*oi->oi_path != '/') {
			len += 3;	/* "$S/" */
			if (oi->oi_prefix != NULL)
				len += strlen(oi->oi_prefix) + 1;
		}
		if (lpos + len > 72) {
			if (fputs(" \\\n", fp) < 0)
				return (1);
			sp = '\t';
			lpos = 7;
		}
		if (*oi->oi_path == '/') {
			if (fprintf(fp, "%c%s", sp, oi->oi_path) < 0)
				return (1);
		} else {
			if (oi->oi_prefix != NULL) {
				if (fprintf(fp, "%c%s%s/%s", sp,
				    prefix_prologue(oi->oi_prefix),
				    oi->oi_prefix, oi->oi_path) < 0)
					return (1);
			} else {
				if (fprintf(fp, "%c$S/%s", sp, oi->oi_path) < 0)
					return (1);
			}
		}
		lpos += len + 1;
		sp = ' ';
	}
	if (putc('\n', fp) < 0)
		return (1);
	return (0);
}

static int
emitcfiles(fp)
	FILE *fp;
{

	return (emitfiles(fp, 'c', 0));
}

static int
emitsfiles(fp)
	FILE *fp;
{

	return (emitfiles(fp, 's', 1));
}

static int
emitfiles(fp, suffix, upper_suffix)
	FILE *fp;
	int suffix;
	int upper_suffix;
{
	struct files *fi;
	struct config *cf;
	int lpos, len, sp;
	const char *fpath;
	char swapname[100];

	if (fprintf(fp, "%cFILES=", toupper(suffix)) < 0)
		return (1);
	sp = '\t';
	lpos = 7;
	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		if ((fpath = srcpath(fi)) == NULL)
                        return (1);
		len = strlen(fpath);
		if (! ((fpath[len - 1] == suffix) ||
		    (upper_suffix && fpath[len - 1] == toupper(suffix))))
			continue;
		if (*fpath != '/') {
			len += 3;	/* "$S/" */
			if (fi->fi_prefix != NULL)
				len += strlen(fi->fi_prefix) + 1;
		}
		if (lpos + len > 72) {
			if (fputs(" \\\n", fp) < 0)
				return (1);
			sp = '\t';
			lpos = 7;
		}
		if (*fi->fi_path == '/') {
			if (fprintf(fp, "%c%s", sp, fi->fi_path) < 0)
				return (1);
		} else {
			if (fi->fi_prefix != NULL) {
				if (fprintf(fp, "%c%s%s/%s", sp,
				    prefix_prologue(fi->fi_prefix),
				    fi->fi_prefix, fi->fi_path) < 0)
					return (1);
			} else {
				if (fprintf(fp, "%c$S/%s", sp, fi->fi_path) < 0)
					return (1);
			}
		}
		lpos += len + 1;
		sp = ' ';
	}
	/*
	 * The allfiles list does not include the configuration-specific
	 * C source files.  These files should be eliminated someday, but
	 * for now, we have to add them to ${CFILES} (and only ${CFILES}).
	 */
	if (suffix == 'c') {
		for (cf = allcf; cf != NULL; cf = cf->cf_next) {
			(void)sprintf(swapname, "swap%s.c", cf->cf_name);
			len = strlen(swapname);
			if (lpos + len > 72) {
				if (fputs(" \\\n", fp) < 0)
					return (1);
				sp = '\t';
				lpos = 7;
			}
			if (fprintf(fp, "%c%s", sp, swapname) < 0)
				return (1);
			lpos += len + 1;
			sp = ' ';
		}
	}
	if (putc('\n', fp) < 0)
		return (1);
	return (0);
}

/*
 * Emit the make-rules.
 */
static int
emitrules(fp)
	FILE *fp;
{
	struct files *fi;
	const char *cp, *fpath;
	int ch;
	char buf[200];

	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		if ((fpath = srcpath(fi)) == NULL)
			return (1);
		if (*fpath == '/') {
			if (fprintf(fp, "%s.o: %s\n", fi->fi_base, fpath) < 0)
				return (1);
		} else {
			if (fi->fi_prefix != NULL) {
				if (fprintf(fp, "%s.o: %s%s/%s\n", fi->fi_base,
				    prefix_prologue(fi->fi_prefix),
				    fi->fi_prefix, fpath) < 0)
					return (1);
			} else {
				if (fprintf(fp, "%s.o: $S/%s\n", fi->fi_base,
				    fpath) < 0)
					return (1);
			}
		}
		if ((cp = fi->fi_mkrule) == NULL) {
			cp = "NORMAL";
			ch = fpath[strlen(fpath) - 1];
			if (islower(ch))
				ch = toupper(ch);
			(void)sprintf(buf, "${%s_%c}", cp, ch);
			cp = buf;
		}
		if (fprintf(fp, "\t%s\n\n", cp) < 0)
			return (1);
	}
	return (0);
}

/*
 * Emit the load commands.
 *
 * This function is not to be called `spurt'.
 */
static int
emitload(fp)
	FILE *fp;
{
	struct config *cf;
	const char *nm, *swname;

	if (fputs(".MAIN: all\nall:", fp) < 0)
		return (1);
	for (cf = allcf; cf != NULL; cf = cf->cf_next) {
		if (fprintf(fp, " %s", cf->cf_name) < 0)
			return (1);
	}
	if (fputs("\n\n", fp) < 0)
		return (1);
	for (cf = allcf; cf != NULL; cf = cf->cf_next) {
		nm = cf->cf_name;
		swname =
		    cf->cf_root != NULL ? cf->cf_name : "generic";
		if (fprintf(fp, "%s: ${SYSTEM_DEP} swap%s.o newvers", nm,
		    swname) < 0)
			return (1);
		if (fprintf(fp, "\n\
\t${SYSTEM_LD_HEAD}\n\
\t${SYSTEM_LD} swap%s.o\n\
\t${SYSTEM_LD_TAIL}\n\
\n\
swap%s.o: ", swname, swname) < 0)
			return (1);
		if (cf->cf_root != NULL) {
			if (fprintf(fp, "swap%s.c\n", nm) < 0)
				return (1);
		} else {
			if (fprintf(fp, "$S/arch/%s/%s/swapgeneric.c\n",
			    machine, machine) < 0)
				return (1);
		}
		if (fputs("\t${NORMAL_C}\n\n", fp) < 0)
			return (1);
	}
	return (0);
}

/*
 * Emit include headers (for any prefixes encountered)
 */
static int
emitincludes(fp)
	FILE *fp;
{
	struct prefix *pf;

	for (pf = allprefixes; pf != NULL; pf = pf->pf_next) {
		if (fprintf(fp, "INCLUDES+=\t-I%s%s\n",
		    prefix_prologue(pf->pf_prefix), pf->pf_prefix) < 0)
			return (1);
	}

	return (0);
}
