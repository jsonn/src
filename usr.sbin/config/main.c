/*	$NetBSD: main.c,v 1.22.2.1 1997/03/02 16:05:25 mrg Exp $	*/

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
 *	from: @(#)main.c	8.1 (Berkeley) 6/6/93
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "sem.h"

int	firstfile __P((const char *));
int	yyparse __P((void));

extern char *optarg;
extern int optind;

static struct hashtab *mkopttab;
static struct nvlist **nextopt;
static struct nvlist **nextdefopt;
static struct nvlist **nextmkopt;
static struct nvlist **nextfsopt;

static __dead void stop __P((void));
static int do_option __P((struct hashtab *, struct nvlist ***,
			const char *, const char *, const char *));
static int crosscheck __P((void));
static int badstar __P((void));
static int mksymlinks __P((void));
static int hasparent __P((struct devi *));
static int cfcrosscheck __P((struct config *, const char *, struct nvlist *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *p;
	const char *last_component;
	int pflag, ch;

	pflag = 0;
	while ((ch = getopt(argc, argv, "gpb:s:")) != EOF) {
		switch (ch) {

		case 'g':
			/*
			 * In addition to DEBUG, you probably wanted to
			 * set "options KGDB" and maybe others.  We could
			 * do that for you, but you really should just
			 * put them in the config file.
			 */
			(void)fputs(
			    "-g is obsolete (use makeoptions DEBUG=\"-g\")\n",
			    stderr);
			goto usage;

		case 'p':
			/*
			 * Essentially the same as makeoptions PROF="-pg",
			 * but also changes the path from ../../compile/FOO
			 * to ../../compile/FOO.PROF; i.e., compile a
			 * profiling kernel based on a typical "regular"
			 * kernel.
			 *
			 * Note that if you always want profiling, you
			 * can (and should) use a "makeoptions" line.
			 */
			pflag = 1;
			break;

		case 'b':
			builddir = optarg;
			break;

		case 's':
			srcdir = optarg;
			break;

		case '?':
		default:
			goto usage;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 1) {
usage:
		(void)fputs("usage: config [-p] [-s srcdir] [-b builddir] sysname\n", stderr);
		exit(1);
	}
	conffile = (argc == 1) ? argv[0] : "CONFIG";
	if (firstfile(conffile)) {
		(void)fprintf(stderr, "config: cannot read %s: %s\n",
		    conffile, strerror(errno));
		exit(2);
	}

	/*
	 * Init variables.
	 */
	minmaxusers = 1;
	maxmaxusers = 10000;
	initintern();
	initfiles();
	initsem();
	devbasetab = ht_new();
	devatab = ht_new();
	selecttab = ht_new();
	needcnttab = ht_new();
	opttab = ht_new();
	mkopttab = ht_new();
	fsopttab = ht_new();
	defopttab = ht_new();
	nextopt = &options;
	nextmkopt = &mkoptions;
	nextfsopt = &fsoptions;
	nextdefopt = &defoptions;

	/*
	 * Handle profiling (must do this before we try to create any
	 * files).
	 */
	last_component = strrchr(conffile, '/');
	last_component = (last_component) ? last_component + 1 : conffile;
	if (pflag) {
		p  = emalloc(strlen(last_component) + 17);
		(void)sprintf(p, "../compile/%s.PROF", last_component);
		(void)addmkoption(intern("PROF"), "-pg");
		(void)addoption(intern("GPROF"), NULL);
	} else {
		p = emalloc(strlen(last_component) + 13);
		(void)sprintf(p, "../compile/%s", last_component);
	}
	defbuilddir = (argc == 0) ? "." : p;

	/*
	 * Parse config file (including machine definitions).
	 */
	if (yyparse())
		stop();

	/*
	 * Fix (as in `set firmly in place') files.
	 */
	if (fixfiles())
		stop();

	/*
	 * Perform cross-checking.
	 */
	if (maxusers == 0) {
		if (defmaxusers) {
			(void)printf("maxusers not specified; %d assumed\n",
			    defmaxusers);
			maxusers = defmaxusers;
		} else {
			(void)fprintf(stderr,
			    "config: need \"maxusers\" line\n");
			errors++;
		}
	}
	if (fsoptions == NULL) {
		(void)fprintf(stderr,
		    "config: need at least one \"file-system\" line\n");
		errors++;
	}
	if (crosscheck() || errors)
		stop();

	/*
	 * Squeeze things down and finish cross-checks (STAR checks must
	 * run after packing).
	 */
	pack();
	if (badstar())
		stop();

	/*
	 * Ready to go.  Build all the various files.
	 */
	if (mksymlinks() || mkmakefile() || mkheaders() || mkswap() ||
	    mkioconf())
		stop();
	(void)printf("Don't forget to run \"make depend\"\n");
	exit(0);
}

/*
 * Make a symlink for "machine" so that "#include <machine/foo.h>" works,
 * and for the machine's CPU architecture, so that works as well.
 */
static int
mksymlinks()
{
	int ret;
	char *p, buf[MAXPATHLEN];
	const char *q;

	sprintf(buf, "arch/%s/include", machine);
	p = sourcepath(buf);
	(void)unlink("machine");
	ret = symlink(p, "machine");
	if (ret)
		(void)fprintf(stderr, "config: symlink(machine -> %s): %s\n",
		    p, strerror(errno));

	if (machinearch != NULL) {
		sprintf(buf, "arch/%s/include", machinearch);
		p = sourcepath(buf);
		q = machinearch;
	} else {
		p = strdup("machine");
		q = machine;
	}
	(void)unlink(q);
	ret = symlink(p, q);
	if (ret)
		(void)fprintf(stderr, "config: symlink(%s -> %s): %s\n",
		    q, p, strerror(errno));
	free(p);

	return (ret);
}

static __dead void
stop()
{
	(void)fprintf(stderr, "*** Stop.\n");
	exit(1);
}

/*
 * Define a standard option, for which a header file will be generated.
 */
void
defoption(name)
	const char *name;
{
	register const char *n;
	register char *p, c;
	char low[500];

	/*
	 * Convert to lower case.  The header file name will be
	 * in lower case, so we store the lower case version in
	 * the hash table to detect option name collisions.  The
	 * original string will be stored in the nvlist for use
	 * in the header file.
	 */
	for (n = name, p = low; (c = *n) != '\0'; n++)
		*p++ = isupper(c) ? tolower(c) : c;
	*p = 0;

	n = intern(low);
	(void)do_option(defopttab, &nextdefopt, n, name, "defopt");

	/*
	 * Insert a verbatum copy of the option name, as well,
	 * to speed lookups when creating the Makefile.
	 */
	(void)ht_insert(defopttab, name, (void *)name);
}

/*
 * Add an option from "options FOO".  Note that this selects things that
 * are "optional foo".
 */
void
addoption(name, value)
	const char *name, *value;
{
	register const char *n;
	register char *p, c;
	char low[500];

	if (do_option(opttab, &nextopt, name, value, "options"))
		return;

	/* make lowercase, then add to select table */
	for (n = name, p = low; (c = *n) != '\0'; n++)
		*p++ = isupper(c) ? tolower(c) : c;
	*p = 0;
	n = intern(low);
	(void)ht_insert(selecttab, n, (void *)n);
}

/*
 * Add a file system option.  This routine simply inserts the name into
 * a list of valid file systems, which is used to validate the root
 * file system type.  The name is then treated like a standard option.
 */
void
addfsoption(name)
	const char *name;
{
	register struct nvlist *nv;
	register const char *n; 
	register char *p, c;
	char buf[500];

	/* Convert to lowercase. */
	for (n = name, p = buf; (c = *n) != '\0'; n++)
		*p++ = isupper(c) ? tolower(c) : c;
	*p = 0;

	n = intern(buf);

	if (do_option(fsopttab, &nextfsopt, n, NULL, "file-system"))
		return;

	/* Convert to uppercase. */
	for (n = name, p = buf; (c = *n) != '\0'; n++)
		*p++ = islower(c) ? toupper(c) : c;
	*p = 0;

	n = intern(buf);

	addoption(n, NULL);
}

/*
 * Add a "make" option.
 */
void
addmkoption(name, value)
	const char *name, *value;
{

	(void)do_option(mkopttab, &nextmkopt, name, value, "mkoptions");
}

/*
 * Add a name=value pair to an option list.  The value may be NULL.
 */
static int
do_option(ht, nppp, name, value, type)
	struct hashtab *ht;
	struct nvlist ***nppp;
	const char *name, *value, *type;
{
	register struct nvlist *nv;

	/* assume it will work */
	nv = newnv(name, value, NULL, 0, NULL);
	if (ht_insert(ht, name, nv) == 0) {
		**nppp = nv;
		*nppp = &nv->nv_next;
		return (0);
	}

	/* oops, already got that option */
	nvfree(nv);
	if ((nv = ht_lookup(ht, name)) == NULL)
		panic("do_option");
	if (nv->nv_str != NULL)
		error("already have %s `%s=%s'", type, name, nv->nv_str);
	else
		error("already have %s `%s'", type, name);
	return (1);
}

/*
 * Return true if there is at least one instance of the given unit
 * on the given device attachment (or any units, if unit == WILD).
 */
int
deva_has_instances(deva, unit)
	register struct deva *deva;
	int unit;
{
	register struct devi *i;

	if (unit == WILD)
		return (deva->d_ihead != NULL);
	for (i = deva->d_ihead; i != NULL; i = i->i_asame)
		if (unit == i->i_unit)
			return (1);
	return (0);
}

/*
 * Return true if there is at least one instance of the given unit
 * on the given base (or any units, if unit == WILD).
 */
int
devbase_has_instances(dev, unit)
	register struct devbase *dev;
	int unit;
{
	register struct deva *da;

	for (da = dev->d_ahead; da != NULL; da = da->d_bsame)
		if (deva_has_instances(da, unit))
			return (1);
	return (0);
}

static int
hasparent(i)
	register struct devi *i;
{
	register struct nvlist *nv;
	int atunit = i->i_atunit;

	/*
	 * We determine whether or not a device has a parent in in one
	 * of two ways:
	 *	(1) If a parent device was named in the config file,
	 *	    i.e. cases (2) and (3) in sem.c:adddev(), then
	 *	    we search its devbase for a matching unit number.
	 *	(2) If the device was attach to an attribute, then we
	 *	    search all attributes the device can be attached to
	 *	    for parents (with appropriate unit numebrs) that
	 *	    may be able to attach the device.
	 */

	/*
	 * Case (1): A parent was named.  Either it's configured, or not.
	 */
	if (i->i_atdev != NULL)
		return (devbase_has_instances(i->i_atdev, atunit));

	/*
	 * Case (2): No parent was named.  Look for devs that provide the attr.
	 */
	if (i->i_atattr != NULL)
		for (nv = i->i_atattr->a_refs; nv != NULL; nv = nv->nv_next)
			if (devbase_has_instances(nv->nv_ptr, atunit))
				return (1);
	return (0);
}

static int
cfcrosscheck(cf, what, nv)
	register struct config *cf;
	const char *what;
	register struct nvlist *nv;
{
	register struct devbase *dev;
	register struct devi *pd;
	int errs, devminor;

	if (maxpartitions <= 0)
		panic("cfcrosscheck");

	for (errs = 0; nv != NULL; nv = nv->nv_next) {
		if (nv->nv_name == NULL)
			continue;
		dev = ht_lookup(devbasetab, nv->nv_name);
		if (dev == NULL)
			panic("cfcrosscheck(%s)", nv->nv_name);
		devminor = minor(nv->nv_int) / maxpartitions;
		if (devbase_has_instances(dev, devminor))
			continue;
		if (devbase_has_instances(dev, STAR) &&
		    devminor >= dev->d_umax)
			continue;
		for (pd = allpseudo; pd != NULL; pd = pd->i_next)
			if (pd->i_base == dev && devminor < dev->d_umax &&
			    devminor >= 0)
				goto loop;
		(void)fprintf(stderr,
		    "%s%d: %s says %s on %s, but there's no %s\n",
		    conffile, cf->cf_lineno,
		    cf->cf_name, what, nv->nv_str, nv->nv_str);
		errs++;
loop:
	}
	return (errs);
}

/*
 * Cross-check the configuration: make sure that each target device or
 * attribute (`at foo[0*?]') names at least one real device.  Also see
 * that the root, and dump devices for all configurations are there.
 */
int
crosscheck()
{
	register struct devi *i;
	register struct config *cf;
	int errs;

	errs = 0;
	for (i = alldevi; i != NULL; i = i->i_next) {
		if (i->i_at == NULL || hasparent(i))
			continue;
		xerror(conffile, i->i_lineno,
		    "%s at %s is orphaned", i->i_name, i->i_at);
		(void)fprintf(stderr, " (%s %s declared)\n",
		    i->i_atunit == WILD ? "nothing matching" : "no",
		    i->i_at);
		errs++;
	}
	if (allcf == NULL) {
		(void)fprintf(stderr, "%s has no configurations!\n",
		    conffile);
		errs++;
	}
	for (cf = allcf; cf != NULL; cf = cf->cf_next) {
		if (cf->cf_root->nv_str != s_qmark) {
			errs += cfcrosscheck(cf, "root", cf->cf_root);
			errs += cfcrosscheck(cf, "dumps", cf->cf_dump);
		}
	}
	return (errs);
}

/*
 * Check to see if there is a *'d unit with a needs-count file.
 */
int
badstar()
{
	register struct devbase *d;
	register struct deva *da;
	register struct devi *i;
	register int errs, n;

	errs = 0;
	for (d = allbases; d != NULL; d = d->d_next) {
		for (da = d->d_ahead; da != NULL; da = da->d_bsame)
			for (i = da->d_ihead; i != NULL; i = i->i_asame) {
				if (i->i_unit == STAR)
					goto foundstar;
			}
		continue;
	foundstar:
		if (ht_lookup(needcnttab, d->d_name)) {
			(void)fprintf(stderr,
		    "config: %s's cannot be *'d until its driver is fixed\n",
			    d->d_name);
			errs++;
			continue;
		}
		for (n = 0; i != NULL; i = i->i_alias)
			if (!i->i_collapsed)
				n++;
		if (n < 1)
			panic("badstar() n<1");
	}
	return (errs);
}

/*
 * Verify/create builddir if necessary, change to it, and verify srcdir.
 * This will be called when we see the first include.
 */
void
setupdirs()
{
	struct stat st;
	char *prof;

	/* srcdir must be specified if builddir is not specified or if
	 * no configuration filename was specified. */
	if ((builddir || strcmp(defbuilddir, ".") == 0) && !srcdir) {
		error("source directory must be specified");
		exit(1);
	}

	if (srcdir == NULL)
		srcdir = "../../../..";
	if (builddir == NULL)
		builddir = defbuilddir;

	if (stat(builddir, &st) != 0) {
		if (mkdir(builddir, 0777)) {
			(void)fprintf(stderr, "config: cannot create %s: %s\n",
			    builddir, strerror(errno));
			exit(2);
		}
	} else if (!S_ISDIR(st.st_mode)) {
		(void)fprintf(stderr, "config: %s is not a directory\n",
			      builddir);
		exit(2);
	}
	if (chdir(builddir) != 0) {
		(void)fprintf(stderr, "config: cannot change to %s\n",
			      builddir);
		exit(2);
	}
	if (stat(srcdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
		(void)fprintf(stderr, "config: %s is not a directory\n",
			      srcdir);
		exit(2);
	}
}
