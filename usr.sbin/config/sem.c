/*	$NetBSD: sem.c,v 1.10.2.1 1997/01/14 21:29:00 thorpej Exp $	*/

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
 *	from: @(#)sem.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "sem.h"

/*
 * config semantics.
 */

#define	NAMESIZE	100	/* local name buffers */

static const char *s_ifnet;		/* magic attribute */
const char *s_nfs;
const char *s_qmark;

static struct hashtab *attrtab;		/* for attribute lookup */
static struct hashtab *cfhashtab;	/* for config lookup */
static struct hashtab *devitab;		/* etc */

static struct attr errattr;
static struct devbase errdev;
static struct deva errdeva;
static struct devbase **nextbase;
static struct deva **nextdeva;
static struct config **nextcf;
static struct devi **nextdevi;
static struct devi **nextpseudo;

static int has_errobj __P((struct nvlist *, void *));
static int has_attr __P((struct nvlist *, const char *));
static struct nvlist *addtoattr __P((struct nvlist *, struct devbase *));
static int exclude __P((struct nvlist *, const char *, const char *));
static int resolve __P((struct nvlist **, const char *, const char *,
			struct nvlist *, int));
static int lresolve __P((struct nvlist **, const char *, const char *,
			struct nvlist *, int));
static struct devi *newdevi __P((const char *, int, struct devbase *d));
static struct devi *getdevi __P((const char *));
static const char *concat __P((const char *, int));
static int split __P((const char *, size_t, char *, size_t, int *));
static void selectbase __P((struct devbase *, struct deva *));
static int onlist __P((struct nvlist *, void *));
static const char **fixloc __P((const char *, struct attr *, struct nvlist *));

void
initsem()
{

	attrtab = ht_new();
	errattr.a_name = "<internal>";

	allbases = NULL;
	nextbase = &allbases;

	alldevas = NULL;
	nextdeva = &alldevas;

	cfhashtab = ht_new();
	allcf = NULL;
	nextcf = &allcf;

	devitab = ht_new();
	alldevi = NULL;
	nextdevi = &alldevi;
	errdev.d_name = "<internal>";

	allpseudo = NULL;
	nextpseudo = &allpseudo;

	s_ifnet = intern("ifnet");
	s_nfs = intern("nfs");
	s_qmark = intern("?");
}

/* Name of include file just ended (set in scan.l) */
extern const char *lastfile;

void
enddefs()
{
	register struct devbase *dev;

	for (dev = allbases; dev != NULL; dev = dev->d_next) {
		if (!dev->d_isdef) {
			(void)fprintf(stderr,
			    "%s: device `%s' used but not defined\n",
			    lastfile, dev->d_name);
			errors++;
			continue;
		}
	}
	if (errors) {
		(void)fprintf(stderr, "*** Stop.\n");
		exit(1);
	}
}

void
setdefmaxusers(min, def, max)
	int min, def, max;
{

	if (min < 1 || min > def || def > max)
		error("maxusers must have 1 <= min <= default <= max");
	else {
		minmaxusers = min;
		defmaxusers = def;
		maxmaxusers = max;
	}
}

void
setmaxusers(n)
	int n;
{

	if (maxusers != 0) {
		error("duplicate maxusers parameter");
		return;
	}
	maxusers = n;
	if (n < minmaxusers) {
		error("warning: minimum of %d maxusers assumed\n", minmaxusers);
		errors--;	/* take it away */
		maxusers = minmaxusers;
	} else if (n > maxmaxusers) {
		error("warning: maxusers (%d) > %d", n, maxmaxusers);
		errors--;
	}
}

/*
 * Define an attribute, optionally with an interface (a locator list).
 * Since an empty locator list is logically different from "no interface",
 * all locator lists include a dummy head node, which we discard here.
 */
int
defattr(name, locs)
	const char *name;
	struct nvlist *locs;
{
	register struct attr *a;
	register struct nvlist *nv;
	register int len;

	a = emalloc(sizeof *a);
	if (ht_insert(attrtab, name, a)) {
		free(a);
		error("attribute `%s' already defined", name);
		nvfreel(locs);
		return (1);
	}
	a->a_name = name;
	if (locs != NULL) {
		a->a_iattr = 1;
		a->a_locs = locs->nv_next;
		nvfree(locs);
	} else {
		a->a_iattr = 0;
		a->a_locs = NULL;
	}
	len = 0;
	for (nv = a->a_locs; nv != NULL; nv = nv->nv_next)
		len++;
	a->a_loclen = len;
	a->a_devs = NULL;
	a->a_refs = NULL;
	return (0);
}

/*
 * Return true if the given `error object' is embedded in the given
 * pointer list.
 */
static int
has_errobj(nv, obj)
	register struct nvlist *nv;
	register void *obj;
{

	for (; nv != NULL; nv = nv->nv_next)
		if (nv->nv_ptr == obj)
			return (1);
	return (0);
}

/*
 * Return true if the given attribute is embedded in the given
 * pointer list.
 */
static int
has_attr(nv, attr)
	register struct nvlist *nv;
	register const char *attr;
{
	register struct attr *a;

	if ((a = getattr(attr)) == NULL)
		return (0);

	for (; nv != NULL; nv = nv->nv_next)
		if (nv->nv_ptr == a)
			return (1);
	return (0);
}

/*
 * Add a device base to a list in an attribute (actually, to any list).
 * Note that this does not check for duplicates, and does reverse the
 * list order, but no one cares anyway.
 */
static struct nvlist *
addtoattr(l, dev)
	register struct nvlist *l;
	register struct devbase *dev;
{
	register struct nvlist *n;

	n = newnv(NULL, NULL, dev, 0, l);
	return (n);
}

/*
 * Define a device.  This may (or may not) also define an interface
 * attribute and/or refer to existing attributes.
 */
void
defdev(dev, ispseudo, loclist, attrs)
	register struct devbase *dev;
	int ispseudo;
	struct nvlist *loclist, *attrs;
{
	register struct nvlist *nv;
	register struct attr *a;

	if (dev == &errdev)
		goto bad;
	if (dev->d_isdef) {
		error("redefinition of `%s'", dev->d_name);
		goto bad;
	}
	dev->d_isdef = 1;
	if (has_errobj(attrs, &errattr))
		goto bad;

	/*
	 * Handle implicit attribute definition from locator list.  Do
	 * this before scanning the `at' list so that we can have, e.g.:
	 *	device foo at other, foo { slot = -1 }
	 * (where you can plug in a foo-bus extender to a foo-bus).
	 */
	if (loclist != NULL) {
		nv = loclist;
		loclist = NULL;	/* defattr disposes of them for us */
		if (defattr(dev->d_name, nv))
			goto bad;
		attrs = newnv(dev->d_name, NULL, getattr(dev->d_name), 0,
		    attrs);
	}

	/* Committed!  Set up fields. */
	dev->d_ispseudo = ispseudo;
	dev->d_attrs = attrs;

	/*
	 * For each interface attribute this device refers to, add this
	 * device to its reference list.  This makes, e.g., finding all
	 * "scsi"s easier.
	 */
	for (nv = attrs; nv != NULL; nv = nv->nv_next) {
		a = nv->nv_ptr;
		if (a->a_iattr)
			a->a_refs = addtoattr(a->a_refs, dev);
	}
	return;
bad:
	nvfreel(loclist);
	nvfreel(attrs);
}

/*
 * Look up a devbase.  Also makes sure it is a reasonable name,
 * i.e., does not end in a digit or contain special characters.
 */
struct devbase *
getdevbase(name)
	const char *name;
{
	register u_char *p;
	register struct devbase *dev;

	p = (u_char *)name;
	if (!isalpha(*p))
		goto badname;
	while (*++p) {
		if (!isalnum(*p) && *p != '_')
			goto badname;
	}
	if (isdigit(*--p)) {
badname:
		error("bad device base name `%s'", name);
		return (&errdev);
	}
	dev = ht_lookup(devbasetab, name);
	if (dev == NULL) {
		dev = emalloc(sizeof *dev);
		dev->d_name = name;
		dev->d_next = NULL;
		dev->d_isdef = 0;
		dev->d_major = NODEV;
		dev->d_attrs = NULL;
		dev->d_ihead = NULL;
		dev->d_ipp = &dev->d_ihead;
		dev->d_ahead = NULL;
		dev->d_app = &dev->d_ahead;
		dev->d_umax = 0;
		*nextbase = dev;
		nextbase = &dev->d_next;
		if (ht_insert(devbasetab, name, dev))
			panic("getdevbase(%s)", name);
	}
	return (dev);
}

/*
 * Define some of a device's allowable parent attachments.
 * There may be a list of (plain) attributes.
 */
void
defdevattach(deva, dev, atlist, attrs)
	register struct deva *deva;
	struct devbase *dev;
	struct nvlist *atlist, *attrs;
{
	register struct nvlist *nv;
	register struct attr *a;
	register struct deva *da;

	if (dev == &errdev)
		goto bad;
	if (deva == NULL)
		deva = getdevattach(dev->d_name);
	if (deva == &errdeva)
		goto bad;
	if (!dev->d_isdef) {
		error("attaching undefined device `%s'", dev->d_name);
		goto bad;
	}
	if (deva->d_isdef) {
		error("redefinition of `%s'", deva->d_name);
		goto bad;
	}
	if (dev->d_ispseudo) {
		error("pseudo-devices can't attach");
		goto bad;
	}

	deva->d_isdef = 1;
	if (has_errobj(attrs, &errattr))
		goto bad;
	for (nv = attrs; nv != NULL; nv = nv->nv_next) {
		a = nv->nv_ptr;
		if (a == &errattr)
			continue;		/* already complained */
		if (a->a_iattr)
			error("`%s' is not a plain attribute", a->a_name);
	}

	/* Committed!  Set up fields. */
	deva->d_attrs = attrs;
	deva->d_atlist = atlist;
	deva->d_devbase = dev;

	/*
	 * Turn the `at' list into interface attributes (map each
	 * nv_name to an attribute, or to NULL for root), and add
	 * this device to those attributes, so that children can
	 * be listed at this particular device if they are supported
	 * by that attribute.
	 */
	for (nv = atlist; nv != NULL; nv = nv->nv_next) {
		if (nv->nv_name == NULL)
			nv->nv_ptr = a = NULL;	/* at root */
		else
			nv->nv_ptr = a = getattr(nv->nv_name);
		if (a == &errattr)
			continue;		/* already complained */

		/*
		 * Make sure that an attachment spec doesn't
		 * already say how to attach to this attribute.
		 */
		for (da = dev->d_ahead; da != NULL; da = da->d_bsame)
			if (onlist(da->d_atlist, a))
				error("attach at `%s' already done by `%s'",
				     a ? a->a_name : "root", da->d_name);

		if (a == NULL)
			continue;		/* at root; don't add */
		if (!a->a_iattr)
			error("%s cannot be at plain attribute `%s'",
			    dev->d_name, a->a_name);
		else
			a->a_devs = addtoattr(a->a_devs, dev);
	}

	/* attach to parent */
	*dev->d_app = deva;
	dev->d_app = &deva->d_bsame;
	return;
bad:
	nvfreel(atlist);
	nvfreel(attrs);
}

/*
 * Look up a device attachment.  Also makes sure it is a reasonable
 * name, i.e., does not contain digits or special characters.
 */
struct deva *
getdevattach(name)
	const char *name;
{
	register u_char *p;
	register struct deva *deva;

	p = (u_char *)name;
	if (!isalpha(*p))
		goto badname;
	while (*++p) {
		if (!isalnum(*p) && *p != '_')
			goto badname;
	}
	if (isdigit(*--p)) {
badname:
		error("bad device attachment name `%s'", name);
		return (&errdeva);
	}
	deva = ht_lookup(devatab, name);
	if (deva == NULL) {
		deva = emalloc(sizeof *deva);
		deva->d_name = name;
		deva->d_next = NULL;
		deva->d_bsame = NULL;
		deva->d_isdef = 0;
		deva->d_devbase = NULL;
		deva->d_atlist = NULL;
		deva->d_attrs = NULL;
		deva->d_ihead = NULL;
		deva->d_ipp = &deva->d_ihead;
		*nextdeva = deva;
		nextdeva = &deva->d_next;
		if (ht_insert(devatab, name, deva))
			panic("getdeva(%s)", name);
	}
	return (deva);
}

/*
 * Look up an attribute.
 */
struct attr *
getattr(name)
	const char *name;
{
	struct attr *a;

	if ((a = ht_lookup(attrtab, name)) == NULL) {
		error("undefined attribute `%s'", name);
		a = &errattr;
	}
	return (a);
}

/*
 * Set the major device number for a device, so that it can be used
 * as a root/swap/dumps "on" device in a configuration.
 */
void
setmajor(d, n)
	struct devbase *d;
	int n;
{

	if (d != &errdev && d->d_major != NODEV)
		error("device `%s' is already major %d",
		    d->d_name, d->d_major);
	else
		d->d_major = n;
}

#define ABS(x) ((x) < 0 ? -(x) : (x))

static int
exclude(nv, name, what)
	struct nvlist *nv;
	const char *name, *what;
{

	if (nv != NULL) {
		error("%s: wildcarded root must not specify %s", name, what);
		return (1);
	}
	return (0);
}

/*
 * Map things like "ra0b" => makedev(major("ra"), 0*maxpartitions + 'b'-'a').
 * Handle the case where the device number is given but there is no
 * corresponding name, and map NULL to the default.
 */
static int
resolve(nvp, name, what, dflt, part)
	register struct nvlist **nvp;
	const char *name, *what;
	struct nvlist *dflt;
	register int part;
{
	register struct nvlist *nv, *anv;
	register struct devbase *dev;
	register const char *cp;
	register int maj, min, i, l;
	register struct attr *a;
	int unit;
	char buf[NAMESIZE], *s_ifnet;;

	if ((u_int)(part -= 'a') >= maxpartitions)
		panic("resolve");
	if ((nv = *nvp) == NULL) {
		dev_t	d = NODEV;
		/*
		 * Apply default.  Easiest to do this by number.
		 * Make sure to retain NODEVness, if this is dflt's disposition.
		 */
		if (dflt->nv_int != NODEV) {
			maj = major(dflt->nv_int);
			min = (minor(dflt->nv_int) / maxpartitions) + part;
			d = makedev(maj, min);
		}
		*nvp = nv = newnv(NULL, NULL, NULL, d, NULL);
	}
	if (nv->nv_int != NODEV) {
		/*
		 * By the numbers.  Find the appropriate major number
		 * to make a name.
		 */
		maj = major(nv->nv_int);
		min = minor(nv->nv_int);
		for (dev = allbases; dev != NULL; dev = dev->d_next)
			if (dev->d_major == maj)
				break;
		if (dev == NULL)
			(void)sprintf(buf, "<%d/%d>", maj, min);
		else
			(void)sprintf(buf, "%s%d%c", dev->d_name,
			    min / maxpartitions, (min % maxpartitions) + 'a');
		nv->nv_str = intern(buf);
		return (0);
	}

	if (nv->nv_str == NULL || nv->nv_str == s_qmark)
		/*
		 * Wildcarded or unspecified; leave it as NODEV.
		 */
		return (0);

	/*
	 * The normal case: things like "ra2b".  Check for partition
	 * suffix, remove it if there, and split into name ("ra") and
	 * unit (2).
	 */
	l = i = strlen(nv->nv_str);
	cp = &nv->nv_str[l];
	if (l > 1 && *--cp >= 'a' && *cp <= 'a'+maxpartitions &&
	    isdigit(cp[-1])) {
		l--;
		part = *cp - 'a';
	}
	cp = nv->nv_str;
	if (split(cp, l, buf, sizeof buf, &unit)) {
		error("%s: invalid %s device name `%s'", name, what, cp);
		return (1);
	}
	dev = ht_lookup(devbasetab, intern(buf));
	if (dev == NULL) {
		error("%s: device `%s' does not exist", buf);
		return (1);
	}

	/*
	 * Check for the magic network interface attribute, and
	 * don't bother making a device number.
	 */
	if (has_attr(dev->d_attrs, s_ifnet))
		nv->nv_int = NODEV;
	else {
		if (dev->d_major == NODEV) {
			error("%s: can't make %s device from `%s'",
			    name, what, nv->nv_str);
			return (1);
		}
		nv->nv_int =
		    makedev(dev->d_major, unit * maxpartitions + part);
	}

	nv->nv_name = dev->d_name;
	return (0);
}

static int
lresolve(nvp, name, what, dflt, part)
	register struct nvlist **nvp;
	const char *name, *what;
	struct nvlist *dflt;
	int part;
{
	int err;

	while ((err = resolve(nvp, name, what, dflt, part)) == 0 &&
	    (*nvp)->nv_next != NULL)
		nvp = &(*nvp)->nv_next;
	return (err);
}

/*
 * Add a completed configuration to the list.
 */
void
addconf(cf0)
	register struct config *cf0;
{
	register struct config *cf;
	register struct nvlist *nv;
	const char *name;

	name = cf0->cf_name;
	cf = emalloc(sizeof *cf);
	if (ht_insert(cfhashtab, name, cf)) {
		error("configuration `%s' already defined", name);
		free(cf);
		goto bad;
	}
	*cf = *cf0;

	/*
	 * Check for wildcarded root device.
	 */
	if (cf->cf_root->nv_str == s_qmark) {
		/*
		 * Make sure no swap or dump device specified.
		 * Note single | here (check all).
		 */
		if (exclude(cf->cf_swap, name, "swap devices") |
		    exclude(cf->cf_dump, name, "dump device"))
			goto bad;
	} else {
		nv = cf->cf_root;
		if (nv == NULL) {
			error("%s: no root device specified", name);
			goto bad;
		}
		if (resolve(&cf->cf_root, name, "root", nv, 'a') |
		    lresolve(&cf->cf_swap, name, "swap", nv, 'b') |
		    resolve(&cf->cf_dump, name, "dumps", nv, 'b'))
			goto bad;
	}

	/* Wildcarded fstype is `unspecified'. */
	if (cf->cf_fstype == s_qmark)
		cf->cf_fstype = NULL;

	*nextcf = cf;
	nextcf = &cf->cf_next;
	return;
bad:
	nvfreel(cf0->cf_root);
	nvfreel(cf0->cf_swap);
	nvfreel(cf0->cf_dump);
}

void
setconf(npp, what, v)
	register struct nvlist **npp;
	const char *what;
	struct nvlist *v;
{

	if (*npp != NULL) {
		error("duplicate %s specification", what);
		nvfreel(v);
	} else
		*npp = v;
}

void
setfstype(fstp, v)
	const char **fstp;
	const char *v;
{

	if (*fstp != NULL) {
		error("multiple fstype specifications");
		return;
	}

	if (v != s_qmark && ht_lookup(fsopttab, v) == NULL) {
		error("\"%s\" is not a configured file system", v);
		return;
	}

	*fstp = v;
}

static struct devi *
newdevi(name, unit, d)
	const char *name;
	int unit;
	struct devbase *d;
{
	register struct devi *i;

	i = emalloc(sizeof *i);
	i->i_name = name;
	i->i_unit = unit;
	i->i_base = d;
	i->i_next = NULL;
	i->i_bsame = NULL;
	i->i_asame = NULL;
	i->i_alias = NULL;
	i->i_at = NULL;
	i->i_atattr = NULL;
	i->i_atdev = NULL;
	i->i_atdeva = NULL;
	i->i_locs = NULL;
	i->i_cfflags = 0;
	i->i_lineno = currentline();
	if (unit >= d->d_umax)
		d->d_umax = unit + 1;
	return (i);
}

/*
 * Add the named device as attaching to the named attribute (or perhaps
 * another device instead) plus unit number.
 */
void
adddev(name, at, loclist, flags)
	const char *name, *at;
	struct nvlist *loclist;
	int flags;
{
	register struct devi *i;	/* the new instance */
	register struct attr *attr;	/* attribute that allows attach */
	register struct devbase *ib;	/* i->i_base */
	register struct devbase *ab;	/* not NULL => at another dev */
	register struct nvlist *nv;
	register struct deva *iba;	/* devbase attachment used */
	const char *cp;
	int atunit;
	char atbuf[NAMESIZE];
	int hit;

	ab = NULL;
	iba = NULL;
	if (at == NULL) {
		/* "at root" */
		if ((i = getdevi(name)) == NULL)
			goto bad;
		/*
		 * Must warn about i_unit > 0 later, after taking care of
		 * the STAR cases (we could do non-star's here but why
		 * bother?).  Make sure this device can be at root.
		 */
		ib = i->i_base;
		hit = 0;
		for (iba = ib->d_ahead; iba != NULL; iba = iba->d_bsame)
			if (onlist(iba->d_atlist, NULL)) {
				hit = 1;
				break;
			}
		if (!hit) {
			error("%s's cannot attach to the root", ib->d_name);
			goto bad;
		}
		attr = &errattr;	/* a convenient "empty" attr */
	} else {
		if (split(at, strlen(at), atbuf, sizeof atbuf, &atunit)) {
			error("invalid attachment name `%s'", at);
			/* (void)getdevi(name); -- ??? */
			goto bad;
		}
		if ((i = getdevi(name)) == NULL)
			goto bad;
		ib = i->i_base;
		cp = intern(atbuf);

		/*
		 * Devices can attach to two types of things: Attributes,
		 * and other devices (which have the appropriate attributes
		 * to allow attachment).
		 *
		 * (1) If we're attached to an attribute, then we don't need
		 *     look at the parent base device to see what attributes
		 *     it has, and make sure that we can attach to them.    
		 *
		 * (2) If we're attached to a real device (i.e. named in
		 *     the config file), we want to remember that so that
		 *     at cross-check time, if the device we're attached to
		 *     is missing but other devices which also provide the
		 *     attribute are present, we don't get a false "OK."
		 *
		 * (3) If the thing we're attached to is an attribute
		 *     but is actually named in the config file, we still
		 *     have to remember its devbase.
		 */

		/* Figure out parent's devbase, to satisfy case (3). */
		ab = ht_lookup(devbasetab, cp);

		/* Find out if it's an attribute. */
		attr = ht_lookup(attrtab, cp);

		/* Make sure we're _really_ attached to the attr.  Case (1). */
		if (attr != NULL && onlist(attr->a_devs, ib))
			goto findattachment;

		/*
		 * Else a real device, and not just an attribute.  Case (2).
		 *
		 * Have to work a bit harder to see whether we have
		 * something like "tg0 at esp0" (where esp is merely
		 * not an attribute) or "tg0 at nonesuch0" (where
		 * nonesuch is not even a device).
		 */
		if (ab == NULL) {
			error("%s at %s: `%s' unknown",
			    name, at, atbuf);
			goto bad;
		}

		/*
		 * See if the named parent carries an attribute
		 * that allows it to supervise device ib.
		 */
		for (nv = ab->d_attrs; nv != NULL; nv = nv->nv_next) {
			attr = nv->nv_ptr;
			if (onlist(attr->a_devs, ib))
				goto findattachment;
		}
		error("%s's cannot attach to %s's", ib->d_name, atbuf);
		goto bad;

findattachment:
		/* find out which attachment it uses */
		hit = 0;
		for (iba = ib->d_ahead; iba != NULL; iba = iba->d_bsame)
			if (onlist(iba->d_atlist, attr)) {
				hit = 1;
				break;
			}
		if (!hit)
			panic("adddev: can't figure out attachment");
	}
ok:
	if ((i->i_locs = fixloc(name, attr, loclist)) == NULL)
		goto bad;
	i->i_at = at;
	i->i_atattr = attr;
	i->i_atdev = ab;
	i->i_atdeva = iba;
	i->i_atunit = atunit;
	i->i_cfflags = flags;

	*iba->d_ipp = i;
	iba->d_ipp = &i->i_asame;

	selectbase(ib, iba);
	/* all done, fall into ... */
bad:
	nvfreel(loclist);
	return;
}

void
addpseudo(name, number)
	const char *name;
	int number;
{
	register struct devbase *d;
	register struct devi *i;

	d = ht_lookup(devbasetab, name);
	if (d == NULL) {
		error("undefined pseudo-device %s", name);
		return;
	}
	if (!d->d_ispseudo) {
		error("%s is a real device, not a pseudo-device", name);
		return;
	}
	if (ht_lookup(devitab, name) != NULL) {
		error("`%s' already defined", name);
		return;
	}
	i = newdevi(name, number - 1, d);	/* foo 16 => "foo0..foo15" */
	if (ht_insert(devitab, name, i))
		panic("addpseudo(%s)", name);
	selectbase(d, NULL);
	*nextpseudo = i;
	nextpseudo = &i->i_next;
	npseudo++;
}

/*
 * Define a new instance of a specific device.
 */
static struct devi *
getdevi(name)
	const char *name;
{
	register struct devi *i, *firsti;
	register struct devbase *d;
	int unit;
	char base[NAMESIZE];

	if (split(name, strlen(name), base, sizeof base, &unit)) {
		error("invalid device name `%s'", name);
		return (NULL);
	}
	d = ht_lookup(devbasetab, intern(base));
	if (d == NULL) {
		error("%s: unknown device `%s'", name, base);
		return (NULL);
	}
	if (d->d_ispseudo) {
		error("%s: %s is a pseudo-device", name, base);
		return (NULL);
	}
	firsti = ht_lookup(devitab, name);
	i = newdevi(name, unit, d);
	if (firsti == NULL) {
		if (ht_insert(devitab, name, i))
			panic("getdevi(%s)", name);
		*d->d_ipp = i;
		d->d_ipp = &i->i_bsame;
	} else {
		while (firsti->i_alias)
			firsti = firsti->i_alias;
		firsti->i_alias = i;
	}
	*nextdevi = i;
	nextdevi = &i->i_next;
	ndevi++;
	return (i);
}

static const char *
concat(name, c)
	const char *name;
	int c;
{
	register int len;
	char buf[NAMESIZE];

	len = strlen(name);
	if (len + 2 > sizeof(buf)) {
		error("device name `%s%c' too long", name, c);
		len = sizeof(buf) - 2;
	}
	bcopy(name, buf, len);
	buf[len] = c;
	buf[len + 1] = 0;
	return (intern(buf));
}

const char *
starref(name)
	const char *name;
{

	return (concat(name, '*'));
}

const char *
wildref(name)
	const char *name;
{

	return (concat(name, '?'));
}

/*
 * Split a name like "foo0" into base name (foo) and unit number (0).
 * Return 0 on success.  To make this useful for names like "foo0a",
 * the length of the "foo0" part is one of the arguments.
 */
static int
split(name, nlen, base, bsize, aunit)
	register const char *name;
	size_t nlen;
	char *base;
	size_t bsize;
	int *aunit;
{
	register const char *cp;
	register int c, l;

	l = nlen;
	if (l < 2 || l >= bsize || isdigit(*name))
		return (1);
	c = (u_char)name[--l];
	if (!isdigit(c)) {
		if (c == '*')
			*aunit = STAR;
		else if (c == '?')
			*aunit = WILD;
		else
			return (1);
	} else {
		cp = &name[l];
		while (isdigit(cp[-1]))
			l--, cp--;
		*aunit = atoi(cp);
	}
	bcopy(name, base, l);
	base[l] = 0;
	return (0);
}

/*
 * We have an instance of the base foo, so select it and all its
 * attributes for "optional foo".
 */
static void
selectbase(d, da)
	register struct devbase *d;
	register struct deva *da;
{
	register struct attr *a;
	register struct nvlist *nv;

	(void)ht_insert(selecttab, d->d_name, (char *)d->d_name);
	for (nv = d->d_attrs; nv != NULL; nv = nv->nv_next) {
		a = nv->nv_ptr;
		(void)ht_insert(selecttab, a->a_name, (char *)a->a_name);
	}
	if (da != NULL) {
		(void)ht_insert(selecttab, da->d_name, (char *)da->d_name);
		for (nv = da->d_attrs; nv != NULL; nv = nv->nv_next) {
			a = nv->nv_ptr;
			(void)ht_insert(selecttab, a->a_name,
			    (char *)a->a_name);
		}
	}
}

/*
 * Is the given pointer on the given list of pointers?
 */
static int
onlist(nv, ptr)
	register struct nvlist *nv;
	register void *ptr;
{
	for (; nv != NULL; nv = nv->nv_next)
		if (nv->nv_ptr == ptr)
			return (1);
	return (0);
}

static char *
extend(p, name)
	register char *p;
	const char *name;
{
	register int l;

	l = strlen(name);
	bcopy(name, p, l);
	p += l;
	*p++ = ',';
	*p++ = ' ';
	return (p);
}

/*
 * Check that we got all required locators, and default any that are
 * given as "?" and have defaults.  Return 0 on success.
 */
static const char **
fixloc(name, attr, got)
	const char *name;
	register struct attr *attr;
	register struct nvlist *got;
{
	register struct nvlist *m, *n;
	register int ord;
	register const char **lp;
	int nmissing, nextra, nnodefault;
	char *mp, *ep, *ndp;
	char missing[1000], extra[1000], nodefault[1000];
	static const char *nullvec[1];

	/*
	 * Look for all required locators, and number the given ones
	 * according to the required order.  While we are numbering,
	 * set default values for defaulted locators.
	 */
	if (attr->a_loclen == 0)	/* e.g., "at root" */
		lp = nullvec;
	else
		lp = emalloc((attr->a_loclen + 1) * sizeof(const char *));
	for (n = got; n != NULL; n = n->nv_next)
		n->nv_int = -1;
	nmissing = 0;
	mp = missing;
	/* yes, this is O(mn), but m and n should be small */
	for (ord = 0, m = attr->a_locs; m != NULL; m = m->nv_next, ord++) {
		for (n = got; n != NULL; n = n->nv_next) {
			if (n->nv_name == m->nv_name) {
				n->nv_int = ord;
				break;
			}
		}
		if (n == NULL && m->nv_int == 0) {
			nmissing++;
			mp = extend(mp, m->nv_name);
		}
		lp[ord] = m->nv_str;
	}
	if (ord != attr->a_loclen)
		panic("fixloc");
	lp[ord] = NULL;
	nextra = 0;
	ep = extra;
	nnodefault = 0;
	ndp = nodefault;
	for (n = got; n != NULL; n = n->nv_next) {
		if (n->nv_int >= 0) {
			if (n->nv_str != NULL)
				lp[n->nv_int] = n->nv_str;
			else if (lp[n->nv_int] == NULL) {
				nnodefault++;
				ndp = extend(ndp, n->nv_name);
			}
		} else {
			nextra++;
			ep = extend(ep, n->nv_name);
		}
	}
	if (nextra) {
		ep[-2] = 0;	/* kill ", " */
		error("%s: extraneous locator%s: %s",
		    name, nextra > 1 ? "s" : "", extra);
	}
	if (nmissing) {
		mp[-2] = 0;
		error("%s: must specify %s", name, missing);
	}
	if (nnodefault) {
		ndp[-2] = 0;
		error("%s: cannot wildcard %s", name, nodefault);
	}
	if (nmissing || nnodefault) {
		free(lp);
		lp = NULL;
	}
	return (lp);
}
