/* $NetBSD: subr_autoconf.c,v 1.163.4.2 2009/03/15 19:43:48 snj Exp $ */

/*
 * Copyright (c) 1996, 2000 Christopher G. Demetriou
 * All rights reserved.
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
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
 * --(license Id: LICENSE.proto,v 1.1 2000/06/13 21:40:26 cgd Exp )--
 */

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
 *
 * from: Header: subr_autoconf.c,v 1.12 93/02/01 19:31:48 torek Exp  (LBL)
 *
 *	@(#)subr_autoconf.c	8.3 (Berkeley) 5/17/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_autoconf.c,v 1.163.4.2 2009/03/15 19:43:48 snj Exp $");

#include "opt_ddb.h"
#include "drvctl.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/kthread.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/callout.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/devmon.h>
#include <sys/cpu.h>

#include <sys/disk.h>

#include <machine/limits.h>

#include "opt_userconf.h"
#ifdef USERCONF
#include <sys/userconf.h>
#endif

#ifdef __i386__
#include "opt_splash.h"
#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_PROGRESS)
#include <dev/splash/splash.h>
extern struct splash_progress *splash_progress_state;
#endif
#endif

/*
 * Autoconfiguration subroutines.
 */

typedef struct pmf_private {
	int		pp_nwait;
	int		pp_nlock;
	lwp_t		*pp_holder;
	kmutex_t	pp_mtx;
	kcondvar_t	pp_cv;
} pmf_private_t;

/*
 * ioconf.c exports exactly two names: cfdata and cfroots.  All system
 * devices and drivers are found via these tables.
 */
extern struct cfdata cfdata[];
extern const short cfroots[];

/*
 * List of all cfdriver structures.  We use this to detect duplicates
 * when other cfdrivers are loaded.
 */
struct cfdriverlist allcfdrivers = LIST_HEAD_INITIALIZER(&allcfdrivers);
extern struct cfdriver * const cfdriver_list_initial[];

/*
 * Initial list of cfattach's.
 */
extern const struct cfattachinit cfattachinit[];

/*
 * List of cfdata tables.  We always have one such list -- the one
 * built statically when the kernel was configured.
 */
struct cftablelist allcftables = TAILQ_HEAD_INITIALIZER(allcftables);
static struct cftable initcftable;

#define	ROOT ((device_t)NULL)

struct matchinfo {
	cfsubmatch_t fn;
	struct	device *parent;
	const int *locs;
	void	*aux;
	struct	cfdata *match;
	int	pri;
};

static char *number(char *, int);
static void mapply(struct matchinfo *, cfdata_t);
static device_t config_devalloc(const device_t, const cfdata_t, const int *);
static void config_devdealloc(device_t);
static void config_makeroom(int, struct cfdriver *);
static void config_devlink(device_t);
static void config_devunlink(device_t);

static void pmflock_debug(device_t, const char *, int);
static void pmflock_debug_with_flags(device_t, const char *, int PMF_FN_PROTO);

static device_t deviter_next1(deviter_t *);
static void deviter_reinit(deviter_t *);

struct deferred_config {
	TAILQ_ENTRY(deferred_config) dc_queue;
	device_t dc_dev;
	void (*dc_func)(device_t);
};

TAILQ_HEAD(deferred_config_head, deferred_config);

struct deferred_config_head deferred_config_queue =
	TAILQ_HEAD_INITIALIZER(deferred_config_queue);
struct deferred_config_head interrupt_config_queue =
	TAILQ_HEAD_INITIALIZER(interrupt_config_queue);
int interrupt_config_threads = 8;

static void config_process_deferred(struct deferred_config_head *, device_t);

/* Hooks to finalize configuration once all real devices have been found. */
struct finalize_hook {
	TAILQ_ENTRY(finalize_hook) f_list;
	int (*f_func)(device_t);
	device_t f_dev;
};
static TAILQ_HEAD(, finalize_hook) config_finalize_list =
	TAILQ_HEAD_INITIALIZER(config_finalize_list);
static int config_finalize_done;

/* list of all devices */
struct devicelist alldevs = TAILQ_HEAD_INITIALIZER(alldevs);
kcondvar_t alldevs_cv;
kmutex_t alldevs_mtx;
static int alldevs_nread = 0;
static int alldevs_nwrite = 0;
static lwp_t *alldevs_writer = NULL;

static int config_pending;		/* semaphore for mountroot */
static kmutex_t config_misc_lock;
static kcondvar_t config_misc_cv;

#define	STREQ(s1, s2)			\
	(*(s1) == *(s2) && strcmp((s1), (s2)) == 0)

static int config_initialized;		/* config_init() has been called. */

static int config_do_twiddle;

struct vnode *
opendisk(struct device *dv)
{
	int bmajor, bminor;
	struct vnode *tmpvn;
	int error;
	dev_t dev;
	
	/*
	 * Lookup major number for disk block device.
	 */
	bmajor = devsw_name2blk(device_xname(dv), NULL, 0);
	if (bmajor == -1)
		return NULL;
	
	bminor = minor(device_unit(dv));
	/*
	 * Fake a temporary vnode for the disk, open it, and read
	 * and hash the sectors.
	 */
	dev = device_is_a(dv, "dk") ? makedev(bmajor, bminor) :
	    MAKEDISKDEV(bmajor, bminor, RAW_PART);
	if (bdevvp(dev, &tmpvn))
		panic("%s: can't alloc vnode for %s", __func__,
		    device_xname(dv));
	error = VOP_OPEN(tmpvn, FREAD, NOCRED);
	if (error) {
#ifndef DEBUG
		/*
		 * Ignore errors caused by missing device, partition,
		 * or medium.
		 */
		if (error != ENXIO && error != ENODEV)
#endif
			printf("%s: can't open dev %s (%d)\n",
			    __func__, device_xname(dv), error);
		vput(tmpvn);
		return NULL;
	}

	return tmpvn;
}

int
config_handle_wedges(struct device *dv, int par)
{
	struct dkwedge_list wl;
	struct dkwedge_info *wi;
	struct vnode *vn;
	char diskname[16];
	int i, error;

	if ((vn = opendisk(dv)) == NULL)
		return -1;

	wl.dkwl_bufsize = sizeof(*wi) * 16;
	wl.dkwl_buf = wi = malloc(wl.dkwl_bufsize, M_TEMP, M_WAITOK);

	error = VOP_IOCTL(vn, DIOCLWEDGES, &wl, FREAD, NOCRED);
	VOP_CLOSE(vn, FREAD, NOCRED);
	vput(vn);
	if (error) {
#ifdef DEBUG_WEDGE
		printf("%s: List wedges returned %d\n",
		    device_xname(dv), error);
#endif
		free(wi, M_TEMP);
		return -1;
	}

#ifdef DEBUG_WEDGE
	printf("%s: Returned %u(%u) wedges\n", device_xname(dv),
	    wl.dkwl_nwedges, wl.dkwl_ncopied);
#endif
	snprintf(diskname, sizeof(diskname), "%s%c", device_xname(dv),
	    par + 'a');

	for (i = 0; i < wl.dkwl_ncopied; i++) {
#ifdef DEBUG_WEDGE
		printf("%s: Looking for %s in %s\n", 
		    device_xname(dv), diskname, wi[i].dkw_wname);
#endif
		if (strcmp(wi[i].dkw_wname, diskname) == 0)
			break;
	}

	if (i == wl.dkwl_ncopied) {
#ifdef DEBUG_WEDGE
		printf("%s: Cannot find wedge with parent %s\n",
		    device_xname(dv), diskname);
#endif
		free(wi, M_TEMP);
		return -1;
	}

#ifdef DEBUG_WEDGE
	printf("%s: Setting boot wedge %s (%s) at %llu %llu\n", 
		device_xname(dv), wi[i].dkw_devname, wi[i].dkw_wname,
		(unsigned long long)wi[i].dkw_offset,
		(unsigned long long)wi[i].dkw_size);
#endif
	dkwedge_set_bootwedge(dv, wi[i].dkw_offset, wi[i].dkw_size);
	free(wi, M_TEMP);
	return 0;
}

/*
 * Initialize the autoconfiguration data structures.  Normally this
 * is done by configure(), but some platforms need to do this very
 * early (to e.g. initialize the console).
 */
void
config_init(void)
{
	const struct cfattachinit *cfai;
	int i, j;

	if (config_initialized)
		return;

	mutex_init(&alldevs_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&alldevs_cv, "alldevs");

	mutex_init(&config_misc_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&config_misc_cv, "cfgmisc");

	/* allcfdrivers is statically initialized. */
	for (i = 0; cfdriver_list_initial[i] != NULL; i++) {
		if (config_cfdriver_attach(cfdriver_list_initial[i]) != 0)
			panic("configure: duplicate `%s' drivers",
			    cfdriver_list_initial[i]->cd_name);
	}

	for (cfai = &cfattachinit[0]; cfai->cfai_name != NULL; cfai++) {
		for (j = 0; cfai->cfai_list[j] != NULL; j++) {
			if (config_cfattach_attach(cfai->cfai_name,
						   cfai->cfai_list[j]) != 0)
				panic("configure: duplicate `%s' attachment "
				    "of `%s' driver",
				    cfai->cfai_list[j]->ca_name,
				    cfai->cfai_name);
		}
	}

	initcftable.ct_cfdata = cfdata;
	TAILQ_INSERT_TAIL(&allcftables, &initcftable, ct_list);

	config_initialized = 1;
}

void
config_deferred(device_t dev)
{
	config_process_deferred(&deferred_config_queue, dev);
	config_process_deferred(&interrupt_config_queue, dev);
}

static void
config_interrupts_thread(void *cookie)
{
	struct deferred_config *dc;

	while ((dc = TAILQ_FIRST(&interrupt_config_queue)) != NULL) {
		TAILQ_REMOVE(&interrupt_config_queue, dc, dc_queue);
		(*dc->dc_func)(dc->dc_dev);
		kmem_free(dc, sizeof(*dc));
		config_pending_decr();
	}
	kthread_exit(0);
}

/*
 * Configure the system's hardware.
 */
void
configure(void)
{
	/* Initialize data structures. */
	config_init();
	pmf_init();
#if NDRVCTL > 0
	drvctl_init();
#endif

#ifdef USERCONF
	if (boothowto & RB_USERCONF)
		user_config();
#endif

	if ((boothowto & (AB_SILENT|AB_VERBOSE)) == AB_SILENT) {
		config_do_twiddle = 1;
		printf_nolog("Detecting hardware...");
	}

	/*
	 * Do the machine-dependent portion of autoconfiguration.  This
	 * sets the configuration machinery here in motion by "finding"
	 * the root bus.  When this function returns, we expect interrupts
	 * to be enabled.
	 */
	cpu_configure();
}

void
configure2(void)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int i, s;

	/*
	 * Now that we've found all the hardware, start the real time
	 * and statistics clocks.
	 */
	initclocks();

	cold = 0;	/* clocks are running, we're warm now! */
	s = splsched();
	curcpu()->ci_schedstate.spc_flags |= SPCF_RUNNING;
	splx(s);

	/* Boot the secondary processors. */
	for (CPU_INFO_FOREACH(cii, ci)) {
		uvm_cpu_attach(ci);
	}
	mp_online = true;
#if defined(MULTIPROCESSOR)
	cpu_boot_secondary_processors();
#endif

	/* Setup the runqueues and scheduler. */
	runq_init();
	sched_init();

	/*
	 * Create threads to call back and finish configuration for
	 * devices that want interrupts enabled.
	 */
	for (i = 0; i < interrupt_config_threads; i++) {
		(void)kthread_create(PRI_NONE, 0, NULL,
		    config_interrupts_thread, NULL, NULL, "config");
	}

	/* Get the threads going and into any sleeps before continuing. */
	yield();
}

/*
 * Announce device attach/detach to userland listeners.
 */
static void
devmon_report_device(device_t dev, bool isattach)
{
#if NDRVCTL > 0
	prop_dictionary_t ev;
	const char *parent;
	const char *what;
	device_t pdev = device_parent(dev);

	ev = prop_dictionary_create();
	if (ev == NULL)
		return;

	what = (isattach ? "device-attach" : "device-detach");
	parent = (pdev == NULL ? "root" : device_xname(pdev));
	if (!prop_dictionary_set_cstring(ev, "device", device_xname(dev)) ||
	    !prop_dictionary_set_cstring(ev, "parent", parent)) {
		prop_object_release(ev);
		return;
	}

	devmon_insert(what, ev);
#endif
}

/*
 * Add a cfdriver to the system.
 */
int
config_cfdriver_attach(struct cfdriver *cd)
{
	struct cfdriver *lcd;

	/* Make sure this driver isn't already in the system. */
	LIST_FOREACH(lcd, &allcfdrivers, cd_list) {
		if (STREQ(lcd->cd_name, cd->cd_name))
			return (EEXIST);
	}

	LIST_INIT(&cd->cd_attach);
	LIST_INSERT_HEAD(&allcfdrivers, cd, cd_list);

	return (0);
}

/*
 * Remove a cfdriver from the system.
 */
int
config_cfdriver_detach(struct cfdriver *cd)
{
	int i;

	/* Make sure there are no active instances. */
	for (i = 0; i < cd->cd_ndevs; i++) {
		if (cd->cd_devs[i] != NULL)
			return (EBUSY);
	}

	/* ...and no attachments loaded. */
	if (LIST_EMPTY(&cd->cd_attach) == 0)
		return (EBUSY);

	LIST_REMOVE(cd, cd_list);

	KASSERT(cd->cd_devs == NULL);

	return (0);
}

/*
 * Look up a cfdriver by name.
 */
struct cfdriver *
config_cfdriver_lookup(const char *name)
{
	struct cfdriver *cd;

	LIST_FOREACH(cd, &allcfdrivers, cd_list) {
		if (STREQ(cd->cd_name, name))
			return (cd);
	}

	return (NULL);
}

/*
 * Add a cfattach to the specified driver.
 */
int
config_cfattach_attach(const char *driver, struct cfattach *ca)
{
	struct cfattach *lca;
	struct cfdriver *cd;

	cd = config_cfdriver_lookup(driver);
	if (cd == NULL)
		return (ESRCH);

	/* Make sure this attachment isn't already on this driver. */
	LIST_FOREACH(lca, &cd->cd_attach, ca_list) {
		if (STREQ(lca->ca_name, ca->ca_name))
			return (EEXIST);
	}

	LIST_INSERT_HEAD(&cd->cd_attach, ca, ca_list);

	return (0);
}

/*
 * Remove a cfattach from the specified driver.
 */
int
config_cfattach_detach(const char *driver, struct cfattach *ca)
{
	struct cfdriver *cd;
	device_t dev;
	int i;

	cd = config_cfdriver_lookup(driver);
	if (cd == NULL)
		return (ESRCH);

	/* Make sure there are no active instances. */
	for (i = 0; i < cd->cd_ndevs; i++) {
		if ((dev = cd->cd_devs[i]) == NULL)
			continue;
		if (dev->dv_cfattach == ca)
			return (EBUSY);
	}

	LIST_REMOVE(ca, ca_list);

	return (0);
}

/*
 * Look up a cfattach by name.
 */
static struct cfattach *
config_cfattach_lookup_cd(struct cfdriver *cd, const char *atname)
{
	struct cfattach *ca;

	LIST_FOREACH(ca, &cd->cd_attach, ca_list) {
		if (STREQ(ca->ca_name, atname))
			return (ca);
	}

	return (NULL);
}

/*
 * Look up a cfattach by driver/attachment name.
 */
struct cfattach *
config_cfattach_lookup(const char *name, const char *atname)
{
	struct cfdriver *cd;

	cd = config_cfdriver_lookup(name);
	if (cd == NULL)
		return (NULL);

	return (config_cfattach_lookup_cd(cd, atname));
}

/*
 * Apply the matching function and choose the best.  This is used
 * a few times and we want to keep the code small.
 */
static void
mapply(struct matchinfo *m, cfdata_t cf)
{
	int pri;

	if (m->fn != NULL) {
		pri = (*m->fn)(m->parent, cf, m->locs, m->aux);
	} else {
		pri = config_match(m->parent, cf, m->aux);
	}
	if (pri > m->pri) {
		m->match = cf;
		m->pri = pri;
	}
}

int
config_stdsubmatch(device_t parent, cfdata_t cf, const int *locs, void *aux)
{
	const struct cfiattrdata *ci;
	const struct cflocdesc *cl;
	int nlocs, i;

	ci = cfiattr_lookup(cf->cf_pspec->cfp_iattr, parent->dv_cfdriver);
	KASSERT(ci);
	nlocs = ci->ci_loclen;
	KASSERT(!nlocs || locs);
	for (i = 0; i < nlocs; i++) {
		cl = &ci->ci_locdesc[i];
		/* !cld_defaultstr means no default value */
		if ((!(cl->cld_defaultstr)
		     || (cf->cf_loc[i] != cl->cld_default))
		    && cf->cf_loc[i] != locs[i])
			return (0);
	}

	return (config_match(parent, cf, aux));
}

/*
 * Helper function: check whether the driver supports the interface attribute
 * and return its descriptor structure.
 */
static const struct cfiattrdata *
cfdriver_get_iattr(const struct cfdriver *cd, const char *ia)
{
	const struct cfiattrdata * const *cpp;

	if (cd->cd_attrs == NULL)
		return (0);

	for (cpp = cd->cd_attrs; *cpp; cpp++) {
		if (STREQ((*cpp)->ci_name, ia)) {
			/* Match. */
			return (*cpp);
		}
	}
	return (0);
}

/*
 * Lookup an interface attribute description by name.
 * If the driver is given, consider only its supported attributes.
 */
const struct cfiattrdata *
cfiattr_lookup(const char *name, const struct cfdriver *cd)
{
	const struct cfdriver *d;
	const struct cfiattrdata *ia;

	if (cd)
		return (cfdriver_get_iattr(cd, name));

	LIST_FOREACH(d, &allcfdrivers, cd_list) {
		ia = cfdriver_get_iattr(d, name);
		if (ia)
			return (ia);
	}
	return (0);
}

/*
 * Determine if `parent' is a potential parent for a device spec based
 * on `cfp'.
 */
static int
cfparent_match(const device_t parent, const struct cfparent *cfp)
{
	struct cfdriver *pcd;

	/* We don't match root nodes here. */
	if (cfp == NULL)
		return (0);

	pcd = parent->dv_cfdriver;
	KASSERT(pcd != NULL);

	/*
	 * First, ensure this parent has the correct interface
	 * attribute.
	 */
	if (!cfdriver_get_iattr(pcd, cfp->cfp_iattr))
		return (0);

	/*
	 * If no specific parent device instance was specified (i.e.
	 * we're attaching to the attribute only), we're done!
	 */
	if (cfp->cfp_parent == NULL)
		return (1);

	/*
	 * Check the parent device's name.
	 */
	if (STREQ(pcd->cd_name, cfp->cfp_parent) == 0)
		return (0);	/* not the same parent */

	/*
	 * Make sure the unit number matches.
	 */
	if (cfp->cfp_unit == DVUNIT_ANY ||	/* wildcard */
	    cfp->cfp_unit == parent->dv_unit)
		return (1);

	/* Unit numbers don't match. */
	return (0);
}

/*
 * Helper for config_cfdata_attach(): check all devices whether it could be
 * parent any attachment in the config data table passed, and rescan.
 */
static void
rescan_with_cfdata(const struct cfdata *cf)
{
	device_t d;
	const struct cfdata *cf1;
	deviter_t di;
  

	/*
	 * "alldevs" is likely longer than an LKM's cfdata, so make it
	 * the outer loop.
	 */
	for (d = deviter_first(&di, 0); d != NULL; d = deviter_next(&di)) {

		if (!(d->dv_cfattach->ca_rescan))
			continue;

		for (cf1 = cf; cf1->cf_name; cf1++) {

			if (!cfparent_match(d, cf1->cf_pspec))
				continue;

			(*d->dv_cfattach->ca_rescan)(d,
				cf1->cf_pspec->cfp_iattr, cf1->cf_loc);
		}
	}
	deviter_release(&di);
}

/*
 * Attach a supplemental config data table and rescan potential
 * parent devices if required.
 */
int
config_cfdata_attach(cfdata_t cf, int scannow)
{
	struct cftable *ct;

	ct = kmem_alloc(sizeof(*ct), KM_SLEEP);
	ct->ct_cfdata = cf;
	TAILQ_INSERT_TAIL(&allcftables, ct, ct_list);

	if (scannow)
		rescan_with_cfdata(cf);

	return (0);
}

/*
 * Helper for config_cfdata_detach: check whether a device is
 * found through any attachment in the config data table.
 */
static int
dev_in_cfdata(const struct device *d, const struct cfdata *cf)
{
	const struct cfdata *cf1;

	for (cf1 = cf; cf1->cf_name; cf1++)
		if (d->dv_cfdata == cf1)
			return (1);

	return (0);
}

/*
 * Detach a supplemental config data table. Detach all devices found
 * through that table (and thus keeping references to it) before.
 */
int
config_cfdata_detach(cfdata_t cf)
{
	device_t d;
	int error = 0;
	struct cftable *ct;
	deviter_t di;

	for (d = deviter_first(&di, DEVITER_F_RW); d != NULL;
	     d = deviter_next(&di)) {
		if (!dev_in_cfdata(d, cf))
			continue;
		if ((error = config_detach(d, 0)) != 0)
			break;
	}
	deviter_release(&di);
	if (error) {
		aprint_error_dev(d, "unable to detach instance\n");
		return error;
	}

	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		if (ct->ct_cfdata == cf) {
			TAILQ_REMOVE(&allcftables, ct, ct_list);
			kmem_free(ct, sizeof(*ct));
			return (0);
		}
	}

	/* not found -- shouldn't happen */
	return (EINVAL);
}

/*
 * Invoke the "match" routine for a cfdata entry on behalf of
 * an external caller, usually a "submatch" routine.
 */
int
config_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cfattach *ca;

	ca = config_cfattach_lookup(cf->cf_name, cf->cf_atname);
	if (ca == NULL) {
		/* No attachment for this entry, oh well. */
		return (0);
	}

	return ((*ca->ca_match)(parent, cf, aux));
}

/*
 * Iterate over all potential children of some device, calling the given
 * function (default being the child's match function) for each one.
 * Nonzero returns are matches; the highest value returned is considered
 * the best match.  Return the `found child' if we got a match, or NULL
 * otherwise.  The `aux' pointer is simply passed on through.
 *
 * Note that this function is designed so that it can be used to apply
 * an arbitrary function to all potential children (its return value
 * can be ignored).
 */
cfdata_t
config_search_loc(cfsubmatch_t fn, device_t parent,
		  const char *ifattr, const int *locs, void *aux)
{
	struct cftable *ct;
	cfdata_t cf;
	struct matchinfo m;

	KASSERT(config_initialized);
	KASSERT(!ifattr || cfdriver_get_iattr(parent->dv_cfdriver, ifattr));

	m.fn = fn;
	m.parent = parent;
	m.locs = locs;
	m.aux = aux;
	m.match = NULL;
	m.pri = 0;

	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		for (cf = ct->ct_cfdata; cf->cf_name; cf++) {

			/* We don't match root nodes here. */
			if (!cf->cf_pspec)
				continue;

			/*
			 * Skip cf if no longer eligible, otherwise scan
			 * through parents for one matching `parent', and
			 * try match function.
			 */
			if (cf->cf_fstate == FSTATE_FOUND)
				continue;
			if (cf->cf_fstate == FSTATE_DNOTFOUND ||
			    cf->cf_fstate == FSTATE_DSTAR)
				continue;

			/*
			 * If an interface attribute was specified,
			 * consider only children which attach to
			 * that attribute.
			 */
			if (ifattr && !STREQ(ifattr, cf->cf_pspec->cfp_iattr))
				continue;

			if (cfparent_match(parent, cf->cf_pspec))
				mapply(&m, cf);
		}
	}
	return (m.match);
}

cfdata_t
config_search_ia(cfsubmatch_t fn, device_t parent, const char *ifattr,
    void *aux)
{

	return (config_search_loc(fn, parent, ifattr, NULL, aux));
}

/*
 * Find the given root device.
 * This is much like config_search, but there is no parent.
 * Don't bother with multiple cfdata tables; the root node
 * must always be in the initial table.
 */
cfdata_t
config_rootsearch(cfsubmatch_t fn, const char *rootname, void *aux)
{
	cfdata_t cf;
	const short *p;
	struct matchinfo m;

	m.fn = fn;
	m.parent = ROOT;
	m.aux = aux;
	m.match = NULL;
	m.pri = 0;
	m.locs = 0;
	/*
	 * Look at root entries for matching name.  We do not bother
	 * with found-state here since only one root should ever be
	 * searched (and it must be done first).
	 */
	for (p = cfroots; *p >= 0; p++) {
		cf = &cfdata[*p];
		if (strcmp(cf->cf_name, rootname) == 0)
			mapply(&m, cf);
	}
	return (m.match);
}

static const char * const msgs[3] = { "", " not configured\n", " unsupported\n" };

/*
 * The given `aux' argument describes a device that has been found
 * on the given parent, but not necessarily configured.  Locate the
 * configuration data for that device (using the submatch function
 * provided, or using candidates' cd_match configuration driver
 * functions) and attach it, and return true.  If the device was
 * not configured, call the given `print' function and return 0.
 */
device_t
config_found_sm_loc(device_t parent,
		const char *ifattr, const int *locs, void *aux,
		cfprint_t print, cfsubmatch_t submatch)
{
	cfdata_t cf;

#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_PROGRESS)
	if (splash_progress_state)
		splash_progress_update(splash_progress_state);
#endif

	if ((cf = config_search_loc(submatch, parent, ifattr, locs, aux)))
		return(config_attach_loc(parent, cf, locs, aux, print));
	if (print) {
		if (config_do_twiddle)
			twiddle();
		aprint_normal("%s", msgs[(*print)(aux, device_xname(parent))]);
	}

#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_PROGRESS)
	if (splash_progress_state)
		splash_progress_update(splash_progress_state);
#endif

	return (NULL);
}

device_t
config_found_ia(device_t parent, const char *ifattr, void *aux,
    cfprint_t print)
{

	return (config_found_sm_loc(parent, ifattr, NULL, aux, print, NULL));
}

device_t
config_found(device_t parent, void *aux, cfprint_t print)
{

	return (config_found_sm_loc(parent, NULL, NULL, aux, print, NULL));
}

/*
 * As above, but for root devices.
 */
device_t
config_rootfound(const char *rootname, void *aux)
{
	cfdata_t cf;

	if ((cf = config_rootsearch((cfsubmatch_t)NULL, rootname, aux)) != NULL)
		return (config_attach(ROOT, cf, aux, (cfprint_t)NULL));
	aprint_error("root device %s not configured\n", rootname);
	return (NULL);
}

/* just like sprintf(buf, "%d") except that it works from the end */
static char *
number(char *ep, int n)
{

	*--ep = 0;
	while (n >= 10) {
		*--ep = (n % 10) + '0';
		n /= 10;
	}
	*--ep = n + '0';
	return (ep);
}

/*
 * Expand the size of the cd_devs array if necessary.
 */
static void
config_makeroom(int n, struct cfdriver *cd)
{
	const km_flag_t kmflags = (cold ? KM_NOSLEEP : KM_SLEEP);
	int old, new;
	device_t *nsp;

	if (n < cd->cd_ndevs)
		return;

	/*
	 * Need to expand the array.
	 */
	old = cd->cd_ndevs;
	if (old == 0)
		new = 4;
	else
		new = old * 2;
	while (new <= n)
		new *= 2;
	cd->cd_ndevs = new;
	nsp = kmem_alloc(sizeof(device_t [new]), kmflags);
	if (nsp == NULL)
		panic("config_attach: %sing dev array",
		    old != 0 ? "expand" : "creat");
	memset(nsp + old, 0, sizeof(device_t [new - old]));
	if (old != 0) {
		memcpy(nsp, cd->cd_devs, sizeof(device_t [old]));
		kmem_free(cd->cd_devs, sizeof(device_t [old]));
	}
	cd->cd_devs = nsp;
}

static void
config_devlink(device_t dev)
{
	struct cfdriver *cd = dev->dv_cfdriver;

	/* put this device in the devices array */
	config_makeroom(dev->dv_unit, cd);
	if (cd->cd_devs[dev->dv_unit])
		panic("config_attach: duplicate %s", device_xname(dev));
	cd->cd_devs[dev->dv_unit] = dev;

	/* It is safe to add a device to the tail of the list while
	 * readers are in the list, but not while a writer is in
	 * the list.  Wait for any writer to complete.
	 */
	mutex_enter(&alldevs_mtx);
	while (alldevs_nwrite != 0 && alldevs_writer != curlwp)
		cv_wait(&alldevs_cv, &alldevs_mtx);
	TAILQ_INSERT_TAIL(&alldevs, dev, dv_list);	/* link up */
	cv_signal(&alldevs_cv);
	mutex_exit(&alldevs_mtx);
}

static void
config_devunlink(device_t dev)
{
	struct cfdriver *cd = dev->dv_cfdriver;
	int i;

	/* Unlink from device list. */
	TAILQ_REMOVE(&alldevs, dev, dv_list);

	/* Remove from cfdriver's array. */
	cd->cd_devs[dev->dv_unit] = NULL;

	/*
	 * If the device now has no units in use, deallocate its softc array.
	 */
	for (i = 0; i < cd->cd_ndevs; i++) {
		if (cd->cd_devs[i] != NULL)
			return;
	}
	/* nothing found; deallocate */
	kmem_free(cd->cd_devs, sizeof(device_t [cd->cd_ndevs]));
	cd->cd_devs = NULL;
	cd->cd_ndevs = 0;
}
	
static device_t
config_devalloc(const device_t parent, const cfdata_t cf, const int *locs)
{
	struct cfdriver *cd;
	struct cfattach *ca;
	size_t lname, lunit;
	const char *xunit;
	int myunit;
	char num[10];
	device_t dev;
	void *dev_private;
	const struct cfiattrdata *ia;
	const km_flag_t kmflags = (cold ? KM_NOSLEEP : KM_SLEEP);

	cd = config_cfdriver_lookup(cf->cf_name);
	if (cd == NULL)
		return (NULL);

	ca = config_cfattach_lookup_cd(cd, cf->cf_atname);
	if (ca == NULL)
		return (NULL);

	if ((ca->ca_flags & DVF_PRIV_ALLOC) == 0 &&
	    ca->ca_devsize < sizeof(struct device))
		panic("config_devalloc: %s", cf->cf_atname);

#ifndef __BROKEN_CONFIG_UNIT_USAGE
	if (cf->cf_fstate == FSTATE_STAR) {
		for (myunit = cf->cf_unit; myunit < cd->cd_ndevs; myunit++)
			if (cd->cd_devs[myunit] == NULL)
				break;
		/*
		 * myunit is now the unit of the first NULL device pointer,
		 * or max(cd->cd_ndevs,cf->cf_unit).
		 */
	} else {
		myunit = cf->cf_unit;
		if (myunit < cd->cd_ndevs && cd->cd_devs[myunit] != NULL)
			return (NULL);
	}	
#else
	myunit = cf->cf_unit;
#endif /* ! __BROKEN_CONFIG_UNIT_USAGE */

	/* compute length of name and decimal expansion of unit number */
	lname = strlen(cd->cd_name);
	xunit = number(&num[sizeof(num)], myunit);
	lunit = &num[sizeof(num)] - xunit;
	if (lname + lunit > sizeof(dev->dv_xname))
		panic("config_devalloc: device name too long");

	/* get memory for all device vars */
	KASSERT((ca->ca_flags & DVF_PRIV_ALLOC) || ca->ca_devsize >= sizeof(struct device));
	if (ca->ca_devsize > 0) {
		dev_private = kmem_zalloc(ca->ca_devsize, kmflags);
		if (dev_private == NULL)
			panic("config_devalloc: memory allocation for device softc failed");
	} else {
		KASSERT(ca->ca_flags & DVF_PRIV_ALLOC);
		dev_private = NULL;
	}

	if ((ca->ca_flags & DVF_PRIV_ALLOC) != 0) {
		dev = kmem_zalloc(sizeof(*dev), kmflags);
	} else {
		dev = dev_private;
	}
	if (dev == NULL)
		panic("config_devalloc: memory allocation for device_t failed");

	dev->dv_class = cd->cd_class;
	dev->dv_cfdata = cf;
	dev->dv_cfdriver = cd;
	dev->dv_cfattach = ca;
	dev->dv_unit = myunit;
	dev->dv_activity_count = 0;
	dev->dv_activity_handlers = NULL;
	dev->dv_private = dev_private;
	memcpy(dev->dv_xname, cd->cd_name, lname);
	memcpy(dev->dv_xname + lname, xunit, lunit);
	dev->dv_parent = parent;
	if (parent != NULL)
		dev->dv_depth = parent->dv_depth + 1;
	else
		dev->dv_depth = 0;
	dev->dv_flags = DVF_ACTIVE;	/* always initially active */
	dev->dv_flags |= ca->ca_flags;	/* inherit flags from class */
	if (locs) {
		KASSERT(parent); /* no locators at root */
		ia = cfiattr_lookup(cf->cf_pspec->cfp_iattr,
				    parent->dv_cfdriver);
		dev->dv_locators =
		    kmem_alloc(sizeof(int [ia->ci_loclen + 1]), kmflags);
		*dev->dv_locators++ = sizeof(int [ia->ci_loclen + 1]);
		memcpy(dev->dv_locators, locs, sizeof(int [ia->ci_loclen]));
	}
	dev->dv_properties = prop_dictionary_create();
	KASSERT(dev->dv_properties != NULL);

	prop_dictionary_set_cstring_nocopy(dev->dv_properties,
	    "device-driver", dev->dv_cfdriver->cd_name);
	prop_dictionary_set_uint16(dev->dv_properties,
	    "device-unit", dev->dv_unit);

	return (dev);
}

static void
config_devdealloc(device_t dev)
{
	int priv = (dev->dv_flags & DVF_PRIV_ALLOC);

	KASSERT(dev->dv_properties != NULL);
	prop_object_release(dev->dv_properties);

	if (dev->dv_activity_handlers)
		panic("config_devdealloc with registered handlers");

	if (dev->dv_locators) {
		size_t amount = *--dev->dv_locators;
		kmem_free(dev->dv_locators, amount);
	}

	if (dev->dv_cfattach->ca_devsize > 0)
		kmem_free(dev->dv_private, dev->dv_cfattach->ca_devsize);
	if (priv)
		kmem_free(dev, sizeof(*dev));
}

/*
 * Attach a found device.
 */
device_t
config_attach_loc(device_t parent, cfdata_t cf,
	const int *locs, void *aux, cfprint_t print)
{
	device_t dev;
	struct cftable *ct;
	const char *drvname;

#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_PROGRESS)
	if (splash_progress_state)
		splash_progress_update(splash_progress_state);
#endif

	dev = config_devalloc(parent, cf, locs);
	if (!dev)
		panic("config_attach: allocation of device softc failed");

	/* XXX redundant - see below? */
	if (cf->cf_fstate != FSTATE_STAR) {
		KASSERT(cf->cf_fstate == FSTATE_NOTFOUND);
		cf->cf_fstate = FSTATE_FOUND;
	}
#ifdef __BROKEN_CONFIG_UNIT_USAGE
	  else
		cf->cf_unit++;
#endif

	config_devlink(dev);

	if (config_do_twiddle)
		twiddle();
	else
		aprint_naive("Found ");
	/*
	 * We want the next two printfs for normal, verbose, and quiet,
	 * but not silent (in which case, we're twiddling, instead).
	 */
	if (parent == ROOT) {
		aprint_naive("%s (root)", device_xname(dev));
		aprint_normal("%s (root)", device_xname(dev));
	} else {
		aprint_naive("%s at %s", device_xname(dev), device_xname(parent));
		aprint_normal("%s at %s", device_xname(dev), device_xname(parent));
		if (print)
			(void) (*print)(aux, NULL);
	}

	/*
	 * Before attaching, clobber any unfound devices that are
	 * otherwise identical.
	 * XXX code above is redundant?
	 */
	drvname = dev->dv_cfdriver->cd_name;
	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		for (cf = ct->ct_cfdata; cf->cf_name; cf++) {
			if (STREQ(cf->cf_name, drvname) &&
			    cf->cf_unit == dev->dv_unit) {
				if (cf->cf_fstate == FSTATE_NOTFOUND)
					cf->cf_fstate = FSTATE_FOUND;
#ifdef __BROKEN_CONFIG_UNIT_USAGE
				/*
				 * Bump the unit number on all starred cfdata
				 * entries for this device.
				 */
				if (cf->cf_fstate == FSTATE_STAR)
					cf->cf_unit++;
#endif /* __BROKEN_CONFIG_UNIT_USAGE */
			}
		}
	}
#ifdef __HAVE_DEVICE_REGISTER
	device_register(dev, aux);
#endif

	/* Let userland know */
	devmon_report_device(dev, true);

#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_PROGRESS)
	if (splash_progress_state)
		splash_progress_update(splash_progress_state);
#endif
	(*dev->dv_cfattach->ca_attach)(parent, dev, aux);
#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_PROGRESS)
	if (splash_progress_state)
		splash_progress_update(splash_progress_state);
#endif

	if (!device_pmf_is_registered(dev))
		aprint_debug_dev(dev, "WARNING: power management not supported\n");

	config_process_deferred(&deferred_config_queue, dev);
	return (dev);
}

device_t
config_attach(device_t parent, cfdata_t cf, void *aux, cfprint_t print)
{

	return (config_attach_loc(parent, cf, NULL, aux, print));
}

/*
 * As above, but for pseudo-devices.  Pseudo-devices attached in this
 * way are silently inserted into the device tree, and their children
 * attached.
 *
 * Note that because pseudo-devices are attached silently, any information
 * the attach routine wishes to print should be prefixed with the device
 * name by the attach routine.
 */
device_t
config_attach_pseudo(cfdata_t cf)
{
	device_t dev;

	dev = config_devalloc(ROOT, cf, NULL);
	if (!dev)
		return (NULL);

	/* XXX mark busy in cfdata */

	config_devlink(dev);

#if 0	/* XXXJRT not yet */
#ifdef __HAVE_DEVICE_REGISTER
	device_register(dev, NULL);	/* like a root node */
#endif
#endif
	(*dev->dv_cfattach->ca_attach)(ROOT, dev, NULL);
	config_process_deferred(&deferred_config_queue, dev);
	return (dev);
}

/*
 * Detach a device.  Optionally forced (e.g. because of hardware
 * removal) and quiet.  Returns zero if successful, non-zero
 * (an error code) otherwise.
 *
 * Note that this code wants to be run from a process context, so
 * that the detach can sleep to allow processes which have a device
 * open to run and unwind their stacks.
 */
int
config_detach(device_t dev, int flags)
{
	struct cftable *ct;
	cfdata_t cf;
	const struct cfattach *ca;
	struct cfdriver *cd;
#ifdef DIAGNOSTIC
	device_t d;
#endif
	int rv = 0;

#ifdef DIAGNOSTIC
	cf = dev->dv_cfdata;
	if (cf != NULL && cf->cf_fstate != FSTATE_FOUND &&
	    cf->cf_fstate != FSTATE_STAR)
		panic("config_detach: %s: bad device fstate %d",
		    device_xname(dev), cf ? cf->cf_fstate : -1);
#endif
	cd = dev->dv_cfdriver;
	KASSERT(cd != NULL);

	ca = dev->dv_cfattach;
	KASSERT(ca != NULL);

	KASSERT(curlwp != NULL);
	mutex_enter(&alldevs_mtx);
	if (alldevs_nwrite > 0 && alldevs_writer == NULL)
		;
	else while (alldevs_nread != 0 ||
	       (alldevs_nwrite != 0 && alldevs_writer != curlwp))
		cv_wait(&alldevs_cv, &alldevs_mtx);
	if (alldevs_nwrite++ == 0)
		alldevs_writer = curlwp;
	mutex_exit(&alldevs_mtx);

	/*
	 * Ensure the device is deactivated.  If the device doesn't
	 * have an activation entry point, we allow DVF_ACTIVE to
	 * remain set.  Otherwise, if DVF_ACTIVE is still set, the
	 * device is busy, and the detach fails.
	 */
	if (ca->ca_activate != NULL)
		rv = config_deactivate(dev);

	/*
	 * Try to detach the device.  If that's not possible, then
	 * we either panic() (for the forced but failed case), or
	 * return an error.
	 */
	if (rv == 0) {
		if (ca->ca_detach != NULL)
			rv = (*ca->ca_detach)(dev, flags);
		else
			rv = EOPNOTSUPP;
	}
	if (rv != 0) {
		if ((flags & DETACH_FORCE) == 0)
			goto out;
		else
			panic("config_detach: forced detach of %s failed (%d)",
			    device_xname(dev), rv);
	}

	/*
	 * The device has now been successfully detached.
	 */

	/* Let userland know */
	devmon_report_device(dev, false);

#ifdef DIAGNOSTIC
	/*
	 * Sanity: If you're successfully detached, you should have no
	 * children.  (Note that because children must be attached
	 * after parents, we only need to search the latter part of
	 * the list.)
	 */
	for (d = TAILQ_NEXT(dev, dv_list); d != NULL;
	    d = TAILQ_NEXT(d, dv_list)) {
		if (d->dv_parent == dev) {
			printf("config_detach: detached device %s"
			    " has children %s\n", device_xname(dev), device_xname(d));
			panic("config_detach");
		}
	}
#endif

	/* notify the parent that the child is gone */
	if (dev->dv_parent) {
		device_t p = dev->dv_parent;
		if (p->dv_cfattach->ca_childdetached)
			(*p->dv_cfattach->ca_childdetached)(p, dev);
	}

	/*
	 * Mark cfdata to show that the unit can be reused, if possible.
	 */
	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		for (cf = ct->ct_cfdata; cf->cf_name; cf++) {
			if (STREQ(cf->cf_name, cd->cd_name)) {
				if (cf->cf_fstate == FSTATE_FOUND &&
				    cf->cf_unit == dev->dv_unit)
					cf->cf_fstate = FSTATE_NOTFOUND;
#ifdef __BROKEN_CONFIG_UNIT_USAGE
				/*
				 * Note that we can only re-use a starred
				 * unit number if the unit being detached
				 * had the last assigned unit number.
				 */
				if (cf->cf_fstate == FSTATE_STAR &&
				    cf->cf_unit == dev->dv_unit + 1)
					cf->cf_unit--;
#endif /* __BROKEN_CONFIG_UNIT_USAGE */
			}
		}
	}

	config_devunlink(dev);

	if (dev->dv_cfdata != NULL && (flags & DETACH_QUIET) == 0)
		aprint_normal_dev(dev, "detached\n");

	config_devdealloc(dev);

out:
	mutex_enter(&alldevs_mtx);
	if (--alldevs_nwrite == 0)
		alldevs_writer = NULL;
	cv_signal(&alldevs_cv);
	mutex_exit(&alldevs_mtx);
	return rv;
}

int
config_detach_children(device_t parent, int flags)
{
	device_t dv;
	deviter_t di;
	int error = 0;

	for (dv = deviter_first(&di, DEVITER_F_RW); dv != NULL;
	     dv = deviter_next(&di)) {
		if (device_parent(dv) != parent)
			continue;
		if ((error = config_detach(dv, flags)) != 0)
			break;
	}
	deviter_release(&di);
	return error;
}

int
config_activate(device_t dev)
{
	const struct cfattach *ca = dev->dv_cfattach;
	int rv = 0, oflags = dev->dv_flags;

	if (ca->ca_activate == NULL)
		return (EOPNOTSUPP);

	if ((dev->dv_flags & DVF_ACTIVE) == 0) {
		dev->dv_flags |= DVF_ACTIVE;
		rv = (*ca->ca_activate)(dev, DVACT_ACTIVATE);
		if (rv)
			dev->dv_flags = oflags;
	}
	return (rv);
}

int
config_deactivate(device_t dev)
{
	const struct cfattach *ca = dev->dv_cfattach;
	int rv = 0, oflags = dev->dv_flags;

	if (ca->ca_activate == NULL)
		return (EOPNOTSUPP);

	if (dev->dv_flags & DVF_ACTIVE) {
		dev->dv_flags &= ~DVF_ACTIVE;
		rv = (*ca->ca_activate)(dev, DVACT_DEACTIVATE);
		if (rv)
			dev->dv_flags = oflags;
	}
	return (rv);
}

/*
 * Defer the configuration of the specified device until all
 * of its parent's devices have been attached.
 */
void
config_defer(device_t dev, void (*func)(device_t))
{
	const km_flag_t kmflags = (cold ? KM_NOSLEEP : KM_SLEEP);
	struct deferred_config *dc;

	if (dev->dv_parent == NULL)
		panic("config_defer: can't defer config of a root device");

#ifdef DIAGNOSTIC
	for (dc = TAILQ_FIRST(&deferred_config_queue); dc != NULL;
	     dc = TAILQ_NEXT(dc, dc_queue)) {
		if (dc->dc_dev == dev)
			panic("config_defer: deferred twice");
	}
#endif

	dc = kmem_alloc(sizeof(*dc), kmflags);
	if (dc == NULL)
		panic("config_defer: unable to allocate callback");

	dc->dc_dev = dev;
	dc->dc_func = func;
	TAILQ_INSERT_TAIL(&deferred_config_queue, dc, dc_queue);
	config_pending_incr();
}

/*
 * Defer some autoconfiguration for a device until after interrupts
 * are enabled.
 */
void
config_interrupts(device_t dev, void (*func)(device_t))
{
	const km_flag_t kmflags = (cold ? KM_NOSLEEP : KM_SLEEP);
	struct deferred_config *dc;

	/*
	 * If interrupts are enabled, callback now.
	 */
	if (cold == 0) {
		(*func)(dev);
		return;
	}

#ifdef DIAGNOSTIC
	for (dc = TAILQ_FIRST(&interrupt_config_queue); dc != NULL;
	     dc = TAILQ_NEXT(dc, dc_queue)) {
		if (dc->dc_dev == dev)
			panic("config_interrupts: deferred twice");
	}
#endif

	dc = kmem_alloc(sizeof(*dc), kmflags);
	if (dc == NULL)
		panic("config_interrupts: unable to allocate callback");

	dc->dc_dev = dev;
	dc->dc_func = func;
	TAILQ_INSERT_TAIL(&interrupt_config_queue, dc, dc_queue);
	config_pending_incr();
}

/*
 * Process a deferred configuration queue.
 */
static void
config_process_deferred(struct deferred_config_head *queue,
    device_t parent)
{
	struct deferred_config *dc, *ndc;

	for (dc = TAILQ_FIRST(queue); dc != NULL; dc = ndc) {
		ndc = TAILQ_NEXT(dc, dc_queue);
		if (parent == NULL || dc->dc_dev->dv_parent == parent) {
			TAILQ_REMOVE(queue, dc, dc_queue);
			(*dc->dc_func)(dc->dc_dev);
			kmem_free(dc, sizeof(*dc));
			config_pending_decr();
		}
	}
}

/*
 * Manipulate the config_pending semaphore.
 */
void
config_pending_incr(void)
{

	mutex_enter(&config_misc_lock);
	config_pending++;
	mutex_exit(&config_misc_lock);
}

void
config_pending_decr(void)
{

#ifdef DIAGNOSTIC
	if (config_pending == 0)
		panic("config_pending_decr: config_pending == 0");
#endif
	mutex_enter(&config_misc_lock);
	config_pending--;
	if (config_pending == 0)
		cv_broadcast(&config_misc_cv);
	mutex_exit(&config_misc_lock);
}

/*
 * Register a "finalization" routine.  Finalization routines are
 * called iteratively once all real devices have been found during
 * autoconfiguration, for as long as any one finalizer has done
 * any work.
 */
int
config_finalize_register(device_t dev, int (*fn)(device_t))
{
	struct finalize_hook *f;

	/*
	 * If finalization has already been done, invoke the
	 * callback function now.
	 */
	if (config_finalize_done) {
		while ((*fn)(dev) != 0)
			/* loop */ ;
	}

	/* Ensure this isn't already on the list. */
	TAILQ_FOREACH(f, &config_finalize_list, f_list) {
		if (f->f_func == fn && f->f_dev == dev)
			return (EEXIST);
	}

	f = kmem_alloc(sizeof(*f), KM_SLEEP);
	f->f_func = fn;
	f->f_dev = dev;
	TAILQ_INSERT_TAIL(&config_finalize_list, f, f_list);

	return (0);
}

void
config_finalize(void)
{
	struct finalize_hook *f;
	struct pdevinit *pdev;
	extern struct pdevinit pdevinit[];
	int errcnt, rv;

	/*
	 * Now that device driver threads have been created, wait for
	 * them to finish any deferred autoconfiguration.
	 */
	mutex_enter(&config_misc_lock);
	while (config_pending != 0)
		cv_wait(&config_misc_cv, &config_misc_lock);
	mutex_exit(&config_misc_lock);

	KERNEL_LOCK(1, NULL);

	/* Attach pseudo-devices. */
	for (pdev = pdevinit; pdev->pdev_attach != NULL; pdev++)
		(*pdev->pdev_attach)(pdev->pdev_count);

	/* Run the hooks until none of them does any work. */
	do {
		rv = 0;
		TAILQ_FOREACH(f, &config_finalize_list, f_list)
			rv |= (*f->f_func)(f->f_dev);
	} while (rv != 0);

	config_finalize_done = 1;

	/* Now free all the hooks. */
	while ((f = TAILQ_FIRST(&config_finalize_list)) != NULL) {
		TAILQ_REMOVE(&config_finalize_list, f, f_list);
		kmem_free(f, sizeof(*f));
	}

	KERNEL_UNLOCK_ONE(NULL);

	errcnt = aprint_get_error_count();
	if ((boothowto & (AB_QUIET|AB_SILENT)) != 0 &&
	    (boothowto & AB_VERBOSE) == 0) {
		if (config_do_twiddle) {
			config_do_twiddle = 0;
			printf_nolog("done.\n");
		}
		if (errcnt != 0) {
			printf("WARNING: %d error%s while detecting hardware; "
			    "check system log.\n", errcnt,
			    errcnt == 1 ? "" : "s");
		}
	}
}

/*
 * device_lookup:
 *
 *	Look up a device instance for a given driver.
 */
device_t
device_lookup(cfdriver_t cd, int unit)
{

	if (unit < 0 || unit >= cd->cd_ndevs)
		return (NULL);
	
	return (cd->cd_devs[unit]);
}

/*
 * device_lookup:
 *
 *	Look up a device instance for a given driver.
 */
void *
device_lookup_private(cfdriver_t cd, int unit)
{
	device_t dv;

	if (unit < 0 || unit >= cd->cd_ndevs)
		return NULL;
	
	if ((dv = cd->cd_devs[unit]) == NULL)
		return NULL;

	return dv->dv_private;
}

/*
 * Accessor functions for the device_t type.
 */
devclass_t
device_class(device_t dev)
{

	return (dev->dv_class);
}

cfdata_t
device_cfdata(device_t dev)
{

	return (dev->dv_cfdata);
}

cfdriver_t
device_cfdriver(device_t dev)
{

	return (dev->dv_cfdriver);
}

cfattach_t
device_cfattach(device_t dev)
{

	return (dev->dv_cfattach);
}

int
device_unit(device_t dev)
{

	return (dev->dv_unit);
}

const char *
device_xname(device_t dev)
{

	return (dev->dv_xname);
}

device_t
device_parent(device_t dev)
{

	return (dev->dv_parent);
}

bool
device_is_active(device_t dev)
{
	int active_flags;

	active_flags = DVF_ACTIVE;
	active_flags |= DVF_CLASS_SUSPENDED;
	active_flags |= DVF_DRIVER_SUSPENDED;
	active_flags |= DVF_BUS_SUSPENDED;

	return ((dev->dv_flags & active_flags) == DVF_ACTIVE);
}

bool
device_is_enabled(device_t dev)
{
	return (dev->dv_flags & DVF_ACTIVE) == DVF_ACTIVE;
}

bool
device_has_power(device_t dev)
{
	int active_flags;

	active_flags = DVF_ACTIVE | DVF_BUS_SUSPENDED;

	return ((dev->dv_flags & active_flags) == DVF_ACTIVE);
}

int
device_locator(device_t dev, u_int locnum)
{

	KASSERT(dev->dv_locators != NULL);
	return (dev->dv_locators[locnum]);
}

void *
device_private(device_t dev)
{

	/*
	 * The reason why device_private(NULL) is allowed is to simplify the
	 * work of a lot of userspace request handlers (i.e., c/bdev
	 * handlers) which grab cfdriver_t->cd_units[n].
	 * It avoids having them test for it to be NULL and only then calling
	 * device_private.
	 */
	return dev == NULL ? NULL : dev->dv_private;
}

prop_dictionary_t
device_properties(device_t dev)
{

	return (dev->dv_properties);
}

/*
 * device_is_a:
 *
 *	Returns true if the device is an instance of the specified
 *	driver.
 */
bool
device_is_a(device_t dev, const char *dname)
{

	return (strcmp(dev->dv_cfdriver->cd_name, dname) == 0);
}

/*
 * device_find_by_xname:
 *
 *	Returns the device of the given name or NULL if it doesn't exist.
 */
device_t
device_find_by_xname(const char *name)
{
	device_t dv;
	deviter_t di;

	for (dv = deviter_first(&di, 0); dv != NULL; dv = deviter_next(&di)) {
		if (strcmp(device_xname(dv), name) == 0)
			break;
	}
	deviter_release(&di);

	return dv;
}

/*
 * device_find_by_driver_unit:
 *
 *	Returns the device of the given driver name and unit or
 *	NULL if it doesn't exist.
 */
device_t
device_find_by_driver_unit(const char *name, int unit)
{
	struct cfdriver *cd;

	if ((cd = config_cfdriver_lookup(name)) == NULL)
		return NULL;
	return device_lookup(cd, unit);
}

/*
 * Power management related functions.
 */

bool
device_pmf_is_registered(device_t dev)
{
	return (dev->dv_flags & DVF_POWER_HANDLERS) != 0;
}

bool
device_pmf_driver_suspend(device_t dev PMF_FN_ARGS)
{
	if ((dev->dv_flags & DVF_DRIVER_SUSPENDED) != 0)
		return true;
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) == 0)
		return false;
	if (*dev->dv_driver_suspend != NULL &&
	    !(*dev->dv_driver_suspend)(dev PMF_FN_CALL))
		return false;

	dev->dv_flags |= DVF_DRIVER_SUSPENDED;
	return true;
}

bool
device_pmf_driver_resume(device_t dev PMF_FN_ARGS)
{
	if ((dev->dv_flags & DVF_DRIVER_SUSPENDED) == 0)
		return true;
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) != 0)
		return false;
	if ((flags & PMF_F_SELF) != 0 && !device_is_self_suspended(dev))
		return false;
	if (*dev->dv_driver_resume != NULL &&
	    !(*dev->dv_driver_resume)(dev PMF_FN_CALL))
		return false;

	dev->dv_flags &= ~DVF_DRIVER_SUSPENDED;
	return true;
}

bool
device_pmf_driver_shutdown(device_t dev, int how)
{

	if (*dev->dv_driver_shutdown != NULL &&
	    !(*dev->dv_driver_shutdown)(dev, how))
		return false;
	return true;
}

bool
device_pmf_driver_register(device_t dev,
    bool (*suspend)(device_t PMF_FN_PROTO),
    bool (*resume)(device_t PMF_FN_PROTO),
    bool (*shutdown)(device_t, int))
{
	pmf_private_t *pp;

	if ((pp = kmem_zalloc(sizeof(*pp), KM_NOSLEEP)) == NULL)
		return false;
	mutex_init(&pp->pp_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&pp->pp_cv, "pmfsusp");
	dev->dv_pmf_private = pp;

	dev->dv_driver_suspend = suspend;
	dev->dv_driver_resume = resume;
	dev->dv_driver_shutdown = shutdown;
	dev->dv_flags |= DVF_POWER_HANDLERS;
	return true;
}

static const char *
curlwp_name(void)
{
	if (curlwp->l_name != NULL)
		return curlwp->l_name;
	else
		return curlwp->l_proc->p_comm;
}

void
device_pmf_driver_deregister(device_t dev)
{
	pmf_private_t *pp = dev->dv_pmf_private;

	/* XXX avoid crash in case we are not initialized */
	if (!pp)
		return;

	dev->dv_driver_suspend = NULL;
	dev->dv_driver_resume = NULL;

	mutex_enter(&pp->pp_mtx);
	dev->dv_flags &= ~DVF_POWER_HANDLERS;
	while (pp->pp_nlock > 0 || pp->pp_nwait > 0) {
		/* Wake a thread that waits for the lock.  That
		 * thread will fail to acquire the lock, and then
		 * it will wake the next thread that waits for the
		 * lock, or else it will wake us.
		 */
		cv_signal(&pp->pp_cv);
		pmflock_debug(dev, __func__, __LINE__);
		cv_wait(&pp->pp_cv, &pp->pp_mtx);
		pmflock_debug(dev, __func__, __LINE__);
	}
	dev->dv_pmf_private = NULL;
	mutex_exit(&pp->pp_mtx);

	cv_destroy(&pp->pp_cv);
	mutex_destroy(&pp->pp_mtx);
	kmem_free(pp, sizeof(*pp));
}

bool
device_pmf_driver_child_register(device_t dev)
{
	device_t parent = device_parent(dev);

	if (parent == NULL || parent->dv_driver_child_register == NULL)
		return true;
	return (*parent->dv_driver_child_register)(dev);
}

void
device_pmf_driver_set_child_register(device_t dev,
    bool (*child_register)(device_t))
{
	dev->dv_driver_child_register = child_register;
}

void
device_pmf_self_resume(device_t dev PMF_FN_ARGS)
{
	pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
	if ((dev->dv_flags & DVF_SELF_SUSPENDED) != 0)
		dev->dv_flags &= ~DVF_SELF_SUSPENDED;
	pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
}

bool
device_is_self_suspended(device_t dev)
{
	return (dev->dv_flags & DVF_SELF_SUSPENDED) != 0;
}

void
device_pmf_self_suspend(device_t dev PMF_FN_ARGS)
{
	bool self = (flags & PMF_F_SELF) != 0;

	pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);

	if (!self)
		dev->dv_flags &= ~DVF_SELF_SUSPENDED;
	else if (device_is_active(dev))
		dev->dv_flags |= DVF_SELF_SUSPENDED;

	pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
}

static void
pmflock_debug(device_t dev, const char *func, int line)
{
	pmf_private_t *pp = device_pmf_private(dev);

	aprint_debug_dev(dev, "%s.%d, %s pp_nlock %d pp_nwait %d dv_flags %x\n",
	    func, line, curlwp_name(), pp->pp_nlock, pp->pp_nwait,
	    dev->dv_flags);
}

static void
pmflock_debug_with_flags(device_t dev, const char *func, int line PMF_FN_ARGS)
{
	pmf_private_t *pp = device_pmf_private(dev);

	aprint_debug_dev(dev, "%s.%d, %s pp_nlock %d pp_nwait %d dv_flags %x "
	    "flags " PMF_FLAGS_FMT "\n", func, line, curlwp_name(),
	    pp->pp_nlock, pp->pp_nwait, dev->dv_flags PMF_FN_CALL);
}

static bool
device_pmf_lock1(device_t dev PMF_FN_ARGS)
{
	pmf_private_t *pp = device_pmf_private(dev);

	while (device_pmf_is_registered(dev) &&
	    pp->pp_nlock > 0 && pp->pp_holder != curlwp) {
		pp->pp_nwait++;
		pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
		cv_wait(&pp->pp_cv, &pp->pp_mtx);
		pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
		pp->pp_nwait--;
	}
	if (!device_pmf_is_registered(dev)) {
		pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
		/* We could not acquire the lock, but some other thread may
		 * wait for it, also.  Wake that thread.
		 */
		cv_signal(&pp->pp_cv);
		return false;
	}
	pp->pp_nlock++;
	pp->pp_holder = curlwp;
	pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
	return true;
}

bool
device_pmf_lock(device_t dev PMF_FN_ARGS)
{
	bool rc;
	pmf_private_t *pp = device_pmf_private(dev);

	mutex_enter(&pp->pp_mtx);
	rc = device_pmf_lock1(dev PMF_FN_CALL);
	mutex_exit(&pp->pp_mtx);

	return rc;
}

void
device_pmf_unlock(device_t dev PMF_FN_ARGS)
{
	pmf_private_t *pp = device_pmf_private(dev);

	KASSERT(pp->pp_nlock > 0);
	mutex_enter(&pp->pp_mtx);
	if (--pp->pp_nlock == 0)
		pp->pp_holder = NULL;
	cv_signal(&pp->pp_cv);
	pmflock_debug_with_flags(dev, __func__, __LINE__ PMF_FN_CALL);
	mutex_exit(&pp->pp_mtx);
}

void *
device_pmf_private(device_t dev)
{
	return dev->dv_pmf_private;
}

void *
device_pmf_bus_private(device_t dev)
{
	return dev->dv_bus_private;
}

bool
device_pmf_bus_suspend(device_t dev PMF_FN_ARGS)
{
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) != 0)
		return true;
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) == 0 ||
	    (dev->dv_flags & DVF_DRIVER_SUSPENDED) == 0)
		return false;
	if (*dev->dv_bus_suspend != NULL &&
	    !(*dev->dv_bus_suspend)(dev PMF_FN_CALL))
		return false;

	dev->dv_flags |= DVF_BUS_SUSPENDED;
	return true;
}

bool
device_pmf_bus_resume(device_t dev PMF_FN_ARGS)
{
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) == 0)
		return true;
	if ((flags & PMF_F_SELF) != 0 && !device_is_self_suspended(dev))
		return false;
	if (*dev->dv_bus_resume != NULL &&
	    !(*dev->dv_bus_resume)(dev PMF_FN_CALL))
		return false;

	dev->dv_flags &= ~DVF_BUS_SUSPENDED;
	return true;
}

bool
device_pmf_bus_shutdown(device_t dev, int how)
{

	if (*dev->dv_bus_shutdown != NULL &&
	    !(*dev->dv_bus_shutdown)(dev, how))
		return false;
	return true;
}

void
device_pmf_bus_register(device_t dev, void *priv,
    bool (*suspend)(device_t PMF_FN_PROTO),
    bool (*resume)(device_t PMF_FN_PROTO),
    bool (*shutdown)(device_t, int), void (*deregister)(device_t))
{
	dev->dv_bus_private = priv;
	dev->dv_bus_resume = resume;
	dev->dv_bus_suspend = suspend;
	dev->dv_bus_shutdown = shutdown;
	dev->dv_bus_deregister = deregister;
}

void
device_pmf_bus_deregister(device_t dev)
{
	if (dev->dv_bus_deregister == NULL)
		return;
	(*dev->dv_bus_deregister)(dev);
	dev->dv_bus_private = NULL;
	dev->dv_bus_suspend = NULL;
	dev->dv_bus_resume = NULL;
	dev->dv_bus_deregister = NULL;
}

void *
device_pmf_class_private(device_t dev)
{
	return dev->dv_class_private;
}

bool
device_pmf_class_suspend(device_t dev PMF_FN_ARGS)
{
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) != 0)
		return true;
	if (*dev->dv_class_suspend != NULL &&
	    !(*dev->dv_class_suspend)(dev PMF_FN_CALL))
		return false;

	dev->dv_flags |= DVF_CLASS_SUSPENDED;
	return true;
}

bool
device_pmf_class_resume(device_t dev PMF_FN_ARGS)
{
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) == 0)
		return true;
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) != 0 ||
	    (dev->dv_flags & DVF_DRIVER_SUSPENDED) != 0)
		return false;
	if (*dev->dv_class_resume != NULL &&
	    !(*dev->dv_class_resume)(dev PMF_FN_CALL))
		return false;

	dev->dv_flags &= ~DVF_CLASS_SUSPENDED;
	return true;
}

void
device_pmf_class_register(device_t dev, void *priv,
    bool (*suspend)(device_t PMF_FN_PROTO),
    bool (*resume)(device_t PMF_FN_PROTO),
    void (*deregister)(device_t))
{
	dev->dv_class_private = priv;
	dev->dv_class_suspend = suspend;
	dev->dv_class_resume = resume;
	dev->dv_class_deregister = deregister;
}

void
device_pmf_class_deregister(device_t dev)
{
	if (dev->dv_class_deregister == NULL)
		return;
	(*dev->dv_class_deregister)(dev);
	dev->dv_class_private = NULL;
	dev->dv_class_suspend = NULL;
	dev->dv_class_resume = NULL;
	dev->dv_class_deregister = NULL;
}

bool
device_active(device_t dev, devactive_t type)
{
	size_t i;

	if (dev->dv_activity_count == 0)
		return false;

	for (i = 0; i < dev->dv_activity_count; ++i) {
		if (dev->dv_activity_handlers[i] == NULL)
			break;
		(*dev->dv_activity_handlers[i])(dev, type);
	}

	return true;
}

bool
device_active_register(device_t dev, void (*handler)(device_t, devactive_t))
{
	void (**new_handlers)(device_t, devactive_t);
	void (**old_handlers)(device_t, devactive_t);
	size_t i, old_size, new_size;
	int s;

	old_handlers = dev->dv_activity_handlers;
	old_size = dev->dv_activity_count;

	for (i = 0; i < old_size; ++i) {
		KASSERT(old_handlers[i] != handler);
		if (old_handlers[i] == NULL) {
			old_handlers[i] = handler;
			return true;
		}
	}

	new_size = old_size + 4;
	new_handlers = kmem_alloc(sizeof(void *[new_size]), KM_SLEEP);

	memcpy(new_handlers, old_handlers, sizeof(void *[old_size]));
	new_handlers[old_size] = handler;
	memset(new_handlers + old_size + 1, 0,
	    sizeof(int [new_size - (old_size+1)]));

	s = splhigh();
	dev->dv_activity_count = new_size;
	dev->dv_activity_handlers = new_handlers;
	splx(s);

	if (old_handlers != NULL)
		kmem_free(old_handlers, sizeof(int [old_size]));

	return true;
}

void
device_active_deregister(device_t dev, void (*handler)(device_t, devactive_t))
{
	void (**old_handlers)(device_t, devactive_t);
	size_t i, old_size;
	int s;

	old_handlers = dev->dv_activity_handlers;
	old_size = dev->dv_activity_count;

	for (i = 0; i < old_size; ++i) {
		if (old_handlers[i] == handler)
			break;
		if (old_handlers[i] == NULL)
			return; /* XXX panic? */
	}

	if (i == old_size)
		return; /* XXX panic? */

	for (; i < old_size - 1; ++i) {
		if ((old_handlers[i] = old_handlers[i + 1]) != NULL)
			continue;

		if (i == 0) {
			s = splhigh();
			dev->dv_activity_count = 0;
			dev->dv_activity_handlers = NULL;
			splx(s);
			kmem_free(old_handlers, sizeof(void *[old_size]));
		}
		return;
	}
	old_handlers[i] = NULL;
}

/*
 * Device Iteration
 *
 * deviter_t: a device iterator.  Holds state for a "walk" visiting
 *     each device_t's in the device tree.
 *
 * deviter_init(di, flags): initialize the device iterator `di'
 *     to "walk" the device tree.  deviter_next(di) will return
 *     the first device_t in the device tree, or NULL if there are
 *     no devices.
 *
 *     `flags' is one or more of DEVITER_F_RW, indicating that the
 *     caller intends to modify the device tree by calling
 *     config_detach(9) on devices in the order that the iterator
 *     returns them; DEVITER_F_ROOT_FIRST, asking for the devices
 *     nearest the "root" of the device tree to be returned, first;
 *     DEVITER_F_LEAVES_FIRST, asking for the devices furthest from
 *     the root of the device tree, first; and DEVITER_F_SHUTDOWN,
 *     indicating both that deviter_init() should not respect any
 *     locks on the device tree, and that deviter_next(di) may run
 *     in more than one LWP before the walk has finished.
 *
 *     Only one DEVITER_F_RW iterator may be in the device tree at
 *     once.
 *
 *     DEVITER_F_SHUTDOWN implies DEVITER_F_RW.
 *
 *     Results are undefined if the flags DEVITER_F_ROOT_FIRST and
 *     DEVITER_F_LEAVES_FIRST are used in combination.
 *
 * deviter_first(di, flags): initialize the device iterator `di'
 *     and return the first device_t in the device tree, or NULL
 *     if there are no devices.  The statement
 *
 *         dv = deviter_first(di);
 *
 *     is shorthand for
 *
 *         deviter_init(di);
 *         dv = deviter_next(di);
 *
 * deviter_next(di): return the next device_t in the device tree,
 *     or NULL if there are no more devices.  deviter_next(di)
 *     is undefined if `di' was not initialized with deviter_init() or
 *     deviter_first().
 *
 * deviter_release(di): stops iteration (subsequent calls to
 *     deviter_next() will return NULL), releases any locks and
 *     resources held by the device iterator.
 *
 * Device iteration does not return device_t's in any particular
 * order.  An iterator will never return the same device_t twice.
 * Device iteration is guaranteed to complete---i.e., if deviter_next(di)
 * is called repeatedly on the same `di', it will eventually return
 * NULL.  It is ok to attach/detach devices during device iteration.
 */
void
deviter_init(deviter_t *di, deviter_flags_t flags)
{
	device_t dv;
	bool rw;

	mutex_enter(&alldevs_mtx);
	if ((flags & DEVITER_F_SHUTDOWN) != 0) {
		flags |= DEVITER_F_RW;
		alldevs_nwrite++;
		alldevs_writer = NULL;
		alldevs_nread = 0;
	} else {
		rw = (flags & DEVITER_F_RW) != 0;

		if (alldevs_nwrite > 0 && alldevs_writer == NULL)
			;
		else while ((alldevs_nwrite != 0 && alldevs_writer != curlwp) ||
		       (rw && alldevs_nread != 0))
			cv_wait(&alldevs_cv, &alldevs_mtx);

		if (rw) {
			if (alldevs_nwrite++ == 0)
				alldevs_writer = curlwp;
		} else
			alldevs_nread++;
	}
	mutex_exit(&alldevs_mtx);

	memset(di, 0, sizeof(*di));

	di->di_flags = flags;

	switch (di->di_flags & (DEVITER_F_LEAVES_FIRST|DEVITER_F_ROOT_FIRST)) {
	case DEVITER_F_LEAVES_FIRST:
		TAILQ_FOREACH(dv, &alldevs, dv_list)
			di->di_curdepth = MAX(di->di_curdepth, dv->dv_depth);
		break;
	case DEVITER_F_ROOT_FIRST:
		TAILQ_FOREACH(dv, &alldevs, dv_list)
			di->di_maxdepth = MAX(di->di_maxdepth, dv->dv_depth);
		break;
	default:
		break;
	}

	deviter_reinit(di);
}

static void
deviter_reinit(deviter_t *di)
{
	if ((di->di_flags & DEVITER_F_RW) != 0)
		di->di_prev = TAILQ_LAST(&alldevs, devicelist);
	else
		di->di_prev = TAILQ_FIRST(&alldevs);
}

device_t
deviter_first(deviter_t *di, deviter_flags_t flags)
{
	deviter_init(di, flags);
	return deviter_next(di);
}

static device_t
deviter_next1(deviter_t *di)
{
	device_t dv;

	dv = di->di_prev;

	if (dv == NULL)
		;
	else if ((di->di_flags & DEVITER_F_RW) != 0)
		di->di_prev = TAILQ_PREV(dv, devicelist, dv_list);
	else
		di->di_prev = TAILQ_NEXT(dv, dv_list);

	return dv;
}

device_t
deviter_next(deviter_t *di)
{
	device_t dv = NULL;

	switch (di->di_flags & (DEVITER_F_LEAVES_FIRST|DEVITER_F_ROOT_FIRST)) {
	case 0:
		return deviter_next1(di);
	case DEVITER_F_LEAVES_FIRST:
		while (di->di_curdepth >= 0) {
			if ((dv = deviter_next1(di)) == NULL) {
				di->di_curdepth--;
				deviter_reinit(di);
			} else if (dv->dv_depth == di->di_curdepth)
				break;
		}
		return dv;
	case DEVITER_F_ROOT_FIRST:
		while (di->di_curdepth <= di->di_maxdepth) {
			if ((dv = deviter_next1(di)) == NULL) {
				di->di_curdepth++;
				deviter_reinit(di);
			} else if (dv->dv_depth == di->di_curdepth)
				break;
		}
		return dv;
	default:
		return NULL;
	}
}

void
deviter_release(deviter_t *di)
{
	bool rw = (di->di_flags & DEVITER_F_RW) != 0;

	mutex_enter(&alldevs_mtx);
	if (alldevs_nwrite > 0 && alldevs_writer == NULL)
		--alldevs_nwrite;
	else {

		if (rw) {
			if (--alldevs_nwrite == 0)
				alldevs_writer = NULL;
		} else
			--alldevs_nread;

		cv_signal(&alldevs_cv);
	}
	mutex_exit(&alldevs_mtx);
}
