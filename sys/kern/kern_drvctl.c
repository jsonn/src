/* $NetBSD: kern_drvctl.c,v 1.14.6.2 2008/04/05 23:33:23 mjf Exp $ */

/*
 * Copyright (c) 2004
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_drvctl.c,v 1.14.6.2 2008/04/05 23:33:23 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/event.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/drvctlio.h>

dev_type_ioctl(drvctlioctl);

const struct cdevsw drvctl_cdevsw = {
	nullopen, nullclose, nullread, nullwrite, drvctlioctl,
	nostop, notty, nopoll, nommap, nokqfilter, D_OTHER
};

void drvctlattach(int);

#define MAXLOCATORS 100

static int drvctl_command(struct lwp *, struct plistref *, u_long, int flag);

static int
pmdevbyname(int cmd, struct devpmargs *a)
{
	struct device *d;

	if ((d = device_find_by_xname(a->devname)) == NULL)
		return ENXIO;

	switch (cmd) {
	case DRVSUSPENDDEV:
		return pmf_device_recursive_suspend(d, PMF_F_NONE) ? 0 : EBUSY;
	case DRVRESUMEDEV:
		if (a->flags & DEVPM_F_SUBTREE) {
			return pmf_device_resume_subtree(d, PMF_F_NONE)
			    ? 0 : EBUSY;
		} else {
			return pmf_device_recursive_resume(d, PMF_F_NONE)
			    ? 0 : EBUSY;
		}
	default:
		return EPASSTHROUGH;
	}
}

static int
listdevbyname(struct devlistargs *l)
{
	device_t d, child;
	deviter_t di;
	int cnt = 0, idx, error = 0;

	if ((d = device_find_by_xname(l->l_devname)) == NULL)
		return ENXIO;

	for (child = deviter_first(&di, 0); child != NULL;
	     child = deviter_next(&di)) {
		if (device_parent(child) != d)
			continue;
		idx = cnt++;
		if (l->l_childname == NULL || idx >= l->l_children)
			continue;
		error = copyoutstr(device_xname(child), l->l_childname[idx],
				sizeof(l->l_childname[idx]), NULL);
		if (error != 0)
			break;
	}
	deviter_release(&di);

	l->l_children = cnt;
	return error;
}

static int
detachdevbyname(const char *devname)
{
	struct device *d;

	if ((d = device_find_by_xname(devname)) == NULL)
		return ENXIO;

#ifndef XXXFULLRISK
	/*
	 * If the parent cannot be notified, it might keep
	 * pointers to the detached device.
	 * There might be a private notification mechanism,
	 * but better play save here.
	 */
	if (d->dv_parent && !d->dv_parent->dv_cfattach->ca_childdetached)
		return (ENOTSUP);
#endif
	return (config_detach(d, 0));
}

static int
rescanbus(const char *busname, const char *ifattr,
	  int numlocators, const int *locators)
{
	int i, rc;
	struct device *d;
	const struct cfiattrdata * const *ap;

	/* XXX there should be a way to get limits and defaults (per device)
	   from config generated data */
	int locs[MAXLOCATORS];
	for (i = 0; i < MAXLOCATORS; i++)
		locs[i] = -1;

	for (i = 0; i < numlocators;i++)
		locs[i] = locators[i];

	if ((d = device_find_by_xname(busname)) == NULL)
		return ENXIO;

	/*
	 * must support rescan, and must have something
	 * to attach to
	 */
	if (!d->dv_cfattach->ca_rescan ||
	    !d->dv_cfdriver->cd_attrs)
		return (ENODEV);

	/* allow to omit attribute if there is exactly one */
	if (!ifattr) {
		if (d->dv_cfdriver->cd_attrs[1])
			return (EINVAL);
		ifattr = d->dv_cfdriver->cd_attrs[0]->ci_name;
	} else {
		/* check for valid attribute passed */
		for (ap = d->dv_cfdriver->cd_attrs; *ap; ap++)
			if (!strcmp((*ap)->ci_name, ifattr))
				break;
		if (!*ap)
			return (EINVAL);
	}

	rc = (*d->dv_cfattach->ca_rescan)(d, ifattr, locs);
	config_deferred(NULL);
	return rc;
}

int
drvctlioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *p)
{
	int res;
	char *ifattr;
	int *locs;

	switch (cmd) {
	case DRVSUSPENDDEV:
	case DRVRESUMEDEV:
#define d ((struct devpmargs *)data)
		res = pmdevbyname(cmd, d);
#undef d
		break;
	case DRVLISTDEV:
		res = listdevbyname((struct devlistargs *)data);
		break;
	case DRVDETACHDEV:
#define d ((struct devdetachargs *)data)
		res = detachdevbyname(d->devname);
#undef d
		break;
	case DRVRESCANBUS:
#define d ((struct devrescanargs *)data)
		d->busname[sizeof(d->busname) - 1] = '\0';

		/* XXX better copyin? */
		if (d->ifattr[0]) {
			d->ifattr[sizeof(d->ifattr) - 1] = '\0';
			ifattr = d->ifattr;
		} else
			ifattr = 0;

		if (d->numlocators) {
			if (d->numlocators > MAXLOCATORS)
				return (EINVAL);
			locs = malloc(d->numlocators * sizeof(int), M_DEVBUF,
				      M_WAITOK);
			res = copyin(d->locators, locs,
				     d->numlocators * sizeof(int));
			if (res) {
				free(locs, M_DEVBUF);
				return (res);
			}
		} else
			locs = 0;
		res = rescanbus(d->busname, ifattr, d->numlocators, locs);
		if (locs)
			free(locs, M_DEVBUF);
#undef d
		break;
	case DRVCTLCOMMAND:
	    	res = drvctl_command(p, (struct plistref *)data, cmd, flag);
	    	break;
	default:
		return (EPASSTHROUGH);
	}
	return (res);
}

void
drvctlattach(int arg)
{
	int maj = cdevsw_lookup_major(&drvctl_cdevsw);
	device_register_name(makedev(maj, 0), NULL, true, DEV_OTHER, "drvctl");
}

/*****************************************************************************
 * Driver control command processing engine
 *****************************************************************************/

static int
drvctl_command_get_properties(struct lwp *l,
			      prop_dictionary_t command_dict,
			      prop_dictionary_t results_dict)
{
	prop_dictionary_t args_dict;
	prop_string_t devname_string;
	device_t dev;
	deviter_t di;
	
	args_dict = prop_dictionary_get(command_dict, "drvctl-arguments");
	if (args_dict == NULL)
		return (EINVAL);

	devname_string = prop_dictionary_get(args_dict, "device-name");
	if (devname_string == NULL)
		return (EINVAL);
	
	for (dev = deviter_first(&di, 0); dev != NULL;
	     dev = deviter_next(&di)) {
		if (prop_string_equals_cstring(devname_string,
					       device_xname(dev))) {
			prop_dictionary_set(results_dict, "drvctl-result-data",
			    device_properties(dev));
			break;
		}
	}

	deviter_release(&di);

	if (dev == NULL)
		return (ESRCH);

	return (0);
}

struct drvctl_command_desc {
	const char *dcd_name;		/* command name */
	int (*dcd_func)(struct lwp *,	/* handler function */
			prop_dictionary_t,
			prop_dictionary_t);
	int dcd_rw;			/* read or write required */
};

static const struct drvctl_command_desc drvctl_command_table[] = {
	{ .dcd_name = "get-properties",
	  .dcd_func = drvctl_command_get_properties,
	  .dcd_rw   = FREAD,
	},

	{ .dcd_name = NULL }
};

static int
drvctl_command(struct lwp *l, struct plistref *pref, u_long ioctl_cmd,
	       int fflag)
{
	prop_dictionary_t command_dict, results_dict;
	prop_string_t command_string;
	const struct drvctl_command_desc *dcd;
	int error;

	error = prop_dictionary_copyin_ioctl(pref, ioctl_cmd, &command_dict);
	if (error)
		return (error);

	results_dict = prop_dictionary_create();
	if (results_dict == NULL) {
		prop_object_release(command_dict);
		return (ENOMEM);
	}
	
	command_string = prop_dictionary_get(command_dict, "drvctl-command");
	if (command_string == NULL) {
		error = EINVAL;
		goto out;
	}

	for (dcd = drvctl_command_table; dcd->dcd_name != NULL; dcd++) {
		if (prop_string_equals_cstring(command_string,
					       dcd->dcd_name))
			break;
	}

	if (dcd->dcd_name == NULL) {
		error = EINVAL;
		goto out;
	}

	if ((fflag & dcd->dcd_rw) == 0) {
		error = EPERM;
		goto out;
	}

	error = (*dcd->dcd_func)(l, command_dict, results_dict);

	prop_dictionary_set_int32(results_dict, "drvctl-error", error);

	error = prop_dictionary_copyout_ioctl(pref, ioctl_cmd, results_dict);
 out:
	prop_object_release(command_dict);
	prop_object_release(results_dict);
	return (error);
}
