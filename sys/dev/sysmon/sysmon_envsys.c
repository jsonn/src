/*	$NetBSD: sysmon_envsys.c,v 1.17.2.3 2007/08/20 18:37:49 ad Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juan Romero Pardines.
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
 *      This product includes software developed by Juan Romero Pardines
 *      for the NetBSD Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 2000 Zembu Labs, Inc.
 * All rights reserved.
 *
 * Author: Jason R. Thorpe <thorpej@zembu.com>
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
 *	This product includes software developed by Zembu Labs, Inc.
 * 4. Neither the name of Zembu Labs nor the names of its employees may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ZEMBU LABS, INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAR-
 * RANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DIS-
 * CLAIMED.  IN NO EVENT SHALL ZEMBU LABS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Environmental sensor framework for sysmon, exported to userland
 * with proplib(3).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_envsys.c,v 1.17.2.3 2007/08/20 18:37:49 ad Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/kmem.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>
#include <dev/sysmon/sysmon_taskq.h>

#include "opt_compat_netbsd.h"

struct sme_sensor_type {
	int		type;
	int		crittype;
	const char	*desc;
};

static const struct sme_sensor_type sme_sensor_type[] = {
	{ ENVSYS_STEMP,		PENVSYS_TYPE_TEMP,	"Temperature" },
	{ ENVSYS_SFANRPM,	PENVSYS_TYPE_FAN,	"Fan" },
	{ ENVSYS_SVOLTS_AC,	PENVSYS_TYPE_VOLTAGE,	"Voltage AC" },
	{ ENVSYS_SVOLTS_DC,	PENVSYS_TYPE_VOLTAGE,	"Voltage DC" },
	{ ENVSYS_SOHMS,		PENVSYS_TYPE_RESISTANCE,"Ohms" },
	{ ENVSYS_SWATTS,	PENVSYS_TYPE_POWER,	"Watts" },
	{ ENVSYS_SAMPS,		PENVSYS_TYPE_POWER,	"Ampere" },
	{ ENVSYS_SWATTHOUR,	PENVSYS_TYPE_BATTERY,	"Watt hour" },
	{ ENVSYS_SAMPHOUR,	PENVSYS_TYPE_BATTERY,	"Ampere hour" },
	{ ENVSYS_INDICATOR,	PENVSYS_TYPE_INDICATOR,	"Indicator" },
	{ ENVSYS_INTEGER,	PENVSYS_TYPE_INDICATOR,	"Integer" },
	{ ENVSYS_DRIVE,		PENVSYS_TYPE_DRIVE,	"Drive" },
	{ -1,			-1,			"unknown" }
};

struct sme_sensor_state {
	int		type;
	const char 	*desc;
};

static const struct sme_sensor_state sme_sensor_state[] = {
	{ ENVSYS_SVALID,	"valid" },
	{ ENVSYS_SINVALID,	"invalid" },
	{ ENVSYS_SCRITICAL,	"critical" },
	{ ENVSYS_SCRITUNDER,	"critical-under" },
	{ ENVSYS_SCRITOVER,	"critical-over" },
	{ ENVSYS_SWARNUNDER,	"warning-under" },
	{ ENVSYS_SWARNOVER,	"warning-over" },
	{ -1,			"unknown" }
};

static const struct sme_sensor_state sme_sensor_drive_state[] = {
	{ ENVSYS_DRIVE_EMPTY,		"drive state is unknown" },
	{ ENVSYS_DRIVE_READY,		"drive is ready" },
	{ ENVSYS_DRIVE_POWERUP,		"drive is powering up" },
	{ ENVSYS_DRIVE_ONLINE,		"drive is online" },
	{ ENVSYS_DRIVE_IDLE,		"drive is idle" },
	{ ENVSYS_DRIVE_ACTIVE,		"drive is active" },
	{ ENVSYS_DRIVE_REBUILD,		"drive is rebuilding" },
	{ ENVSYS_DRIVE_POWERDOWN,	"drive is powering down" },
	{ ENVSYS_DRIVE_FAIL,		"drive failed" },
	{ ENVSYS_DRIVE_PFAIL,		"drive degraded" },
	{ -1,				"unknown" }
};

struct sme_sensor_names {
	SLIST_ENTRY(sme_sensor_names) sme_names;
	int	assigned;
	char	desc[ENVSYS_DESCLEN];
};

static SLIST_HEAD(, sme_sensor_names) sme_names_list;
static prop_dictionary_t sme_propd;
static kcondvar_t sme_list_cv;
static int sme_uniqsensors = 0;

kmutex_t sme_mtx, sme_list_mtx, sme_event_mtx;
kcondvar_t sme_event_cv;

#ifdef COMPAT_40
static u_int sysmon_envsys_next_sensor_index = 0;
static struct sysmon_envsys *sysmon_envsys_find_40(u_int);
#endif

static void sysmon_envsys_release(struct sysmon_envsys *);
static int sme_register_sensorname(envsys_data_t *);

/*
 * sysmon_envsys_init:
 *
 * 	+ Initialize global mutexes, dictionary and the linked lists.
 */
void
sysmon_envsys_init(void)
{
	LIST_INIT(&sysmon_envsys_list);
	LIST_INIT(&sme_events_list);
	mutex_init(&sme_mtx, MUTEX_DRIVER, IPL_NONE);
	mutex_init(&sme_list_mtx, MUTEX_DRIVER, IPL_NONE);
	mutex_init(&sme_event_mtx, MUTEX_DRIVER, IPL_NONE);
	mutex_init(&sme_event_init_mtx, MUTEX_DRIVER, IPL_NONE);
	cv_init(&sme_list_cv, "smefind");
	cv_init(&sme_event_cv, "smeevent");
	sme_propd = prop_dictionary_create();
}

/*
 * sysmonopen_envsys:
 *
 *	+ Open the system monitor device.
 */
int
sysmonopen_envsys(dev_t dev, int flag, int mode, struct lwp *l)
{
	return 0;
}

/*
 * sysmonclose_envsys:
 *
 *	+ Close the system monitor device.
 */
int
sysmonclose_envsys(dev_t dev, int flag, int mode, struct lwp *l)
{
	/* Nothing to do */
	return 0;
}

/*
 * sysmonioctl_envsys:
 *
 *	+ Perform an envsys control request.
 */
int
sysmonioctl_envsys(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct sysmon_envsys *sme = NULL;
	int error = 0;
#ifdef COMPAT_40
	u_int oidx;
#endif

	switch (cmd) {
	case ENVSYS_GETDICTIONARY:
	    {
		struct plistref *plist = (struct plistref *)data;
	
		/*
		 * Update all sysmon envsys devices dictionaries with
		 * new data if it's different than we have currently
		 * in the dictionary.
		 */		
		mutex_enter(&sme_list_mtx);
		LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
			if (sme == NULL)
				continue;

			mutex_enter(&sme_mtx);
			error = sme_update_dictionary(sme);
			if (error) {
				DPRINTF(("%s: sme_update_dictionary, "
				    "error=%d\n", __func__, error));
				mutex_exit(&sme_list_mtx);
				mutex_exit(&sme_mtx);
				return error;
			}
			mutex_exit(&sme_mtx);
		}
		mutex_exit(&sme_list_mtx);
		/*
		 * Copy global dictionary to userland.
		 */
		error = prop_dictionary_copyout_ioctl(plist, cmd, sme_propd);
		break;
	    }
	case ENVSYS_SETDICTIONARY:
	    {
		const struct plistref *plist = (const struct plistref *)data;
		prop_dictionary_t udict;
		prop_object_t obj;
		const char *devname = NULL;

		if ((flag & FWRITE) == 0)
			return EPERM;

		/*
		 * Get dictionary from userland.
		 */
		error = prop_dictionary_copyin_ioctl(plist, cmd, &udict);
		if (error) {
			DPRINTF(("%s: copyin_ioctl error=%d\n",
			    __func__, error));
			break;
		}

		/*
		 * Parse "driver-name" key to obtain the driver we
		 * are searching for.
		 */
		obj = prop_dictionary_get(udict, "driver-name");
		if (obj == NULL || prop_object_type(obj) != PROP_TYPE_STRING) {
			DPRINTF(("%s: driver-name failed\n", __func__));
			prop_object_release(udict);
			error = EINVAL;
			break;
		}

		/* driver name */
		devname = prop_string_cstring_nocopy(obj);

		/* find the correct sme device */
		sme = sysmon_envsys_find(devname);
		if (sme == NULL) {
			DPRINTF(("%s: NULL sme\n", __func__));
			prop_object_release(udict);
			error = EINVAL;
			break;
		}

		/*
		 * Find the correct array object with the string supplied
		 * by the userland dictionary.
		 */
		obj = prop_dictionary_get(sme_propd, devname);
		if (prop_object_type(obj) != PROP_TYPE_ARRAY) {
			DPRINTF(("%s: array device failed\n", __func__));
			sysmon_envsys_release(sme);
			prop_object_release(udict);
			error = EINVAL;
			break;
		}

		/* do the real work now */
		error = sme_userset_dictionary(sme, udict, obj);
		sysmon_envsys_release(sme);
		break;
	    }
	    /*
	     * Compatibility functions with the old interface, only
	     * implemented ENVSYS_GTREDATA and ENVSYS_GTREINFO; enough
	     * to make old applications work.
	     */
#ifdef COMPAT_40
	case ENVSYS_GTREDATA:
	    {
		struct envsys_tre_data *tred = (void *)data;
		envsys_data_t *edata = NULL;

		tred->validflags = 0;

		sme = sysmon_envsys_find_40(tred->sensor);
		if (sme == NULL)
			break;

		mutex_enter(&sme_mtx);
		oidx = tred->sensor;
		tred->sensor = SME_SENSOR_IDX(sme, tred->sensor);

		DPRINTFOBJ(("%s: sensor=%d oidx=%d dev=%s nsensors=%d\n",
		    __func__, tred->sensor, oidx, sme->sme_name,
		    sme->sme_nsensors));

		edata = &sme->sme_sensor_data[tred->sensor];

		if (tred->sensor < sme->sme_nsensors) {
			if ((sme->sme_flags & SME_DISABLE_GTREDATA) == 0) {
				error = (*sme->sme_gtredata)(sme, edata);
				if (error) {
					DPRINTF(("%s: sme_gtredata failed\n",
				    	    __func__));
					mutex_exit(&sme_mtx);
					return error;
				}
			}

			/* copy required values to the old interface */
			tred->sensor = edata->sensor;
			tred->cur.data_us = edata->value_cur;
			tred->cur.data_s = edata->value_cur;
			tred->max.data_us = edata->value_max;
			tred->max.data_s = edata->value_max;
			tred->min.data_us = edata->value_min;
			tred->min.data_s = edata->value_min;
			tred->avg.data_us = edata->value_avg;
			tred->avg.data_s = edata->value_avg;
			tred->units = edata->units;

			tred->validflags |= ENVSYS_FVALID;
			tred->validflags |= ENVSYS_FCURVALID;

			if (edata->flags & ENVSYS_FPERCENT) {
				tred->validflags |= ENVSYS_FMAXVALID;
				tred->validflags |= ENVSYS_FFRACVALID;
			}

			if (edata->state == ENVSYS_SINVALID ||
			    edata->flags & ENVSYS_FNOTVALID) {
				tred->validflags &= ~ENVSYS_FCURVALID;
				tred->cur.data_us = tred->cur.data_s = 0;
			}

			DPRINTFOBJ(("%s: sensor=%s tred->cur.data_s=%d\n",
			    __func__, edata->desc, tred->cur.data_s));
			DPRINTFOBJ(("%s: tred->validflags=%d tred->units=%d"
			    " tred->sensor=%d\n", __func__, tred->validflags,
			    tred->units, tred->sensor));
		}
		tred->sensor = oidx;
		mutex_exit(&sme_mtx);

		break;
	    }
	case ENVSYS_GTREINFO:
	    {
		struct envsys_basic_info *binfo = (void *)data;
		envsys_data_t *edata = NULL;

		binfo->validflags = 0;

		sme = sysmon_envsys_find_40(binfo->sensor);
		if (sme == NULL)
			break;

		mutex_enter(&sme_mtx);
		oidx = binfo->sensor;
		binfo->sensor = SME_SENSOR_IDX(sme, binfo->sensor);

		edata = &sme->sme_sensor_data[binfo->sensor];

		binfo->validflags |= ENVSYS_FVALID;

		if (binfo->sensor < sme->sme_nsensors) {
			binfo->units = edata->units;
			(void)strlcpy(binfo->desc, edata->desc,
			    sizeof(binfo->desc));
		}

		DPRINTFOBJ(("%s: binfo->units=%d binfo->validflags=%d\n",
		    __func__, binfo->units, binfo->validflags));
		DPRINTFOBJ(("%s: binfo->desc=%s binfo->sensor=%d\n",
		    __func__, binfo->desc, binfo->sensor));

		binfo->sensor = oidx;
		mutex_exit(&sme_mtx);

		break;
	    }
#endif /* COMPAT_40 */
	default:
		error = ENOTTY;
		break;
	}

	return error;
}

/*
 * sysmon_envsys_register:
 *
 *	+ Register an envsys device.
 *	+ Create device dictionary.
 */
int
sysmon_envsys_register(struct sysmon_envsys *sme)
{
	struct sysmon_envsys *lsme;
	int error = 0;

	/* 
	 * sanity check 1, make sure the driver has initialized
	 * the sensors count or the value is not too high.
	 */
	if (!sme->sme_nsensors || sme->sme_nsensors > ENVSYS_MAXSENSORS)
		return EINVAL;

	/* 
	 * sanity check 2, make sure that sme->sme_name and
	 * sme->sme_data are not NULL.
	 */
	if (sme->sme_name == NULL || sme->sme_sensor_data == NULL)
		return EINVAL;

	/*
	 * sanity check 3, if SME_DISABLE_GTREDATA is not set
	 * the sme_gtredata function callback must be non NULL.
	 */
	if ((sme->sme_flags & SME_DISABLE_GTREDATA) == 0) {
		if (sme->sme_gtredata == NULL)
			return EINVAL;
	}

	/*
	 * - check if requested sysmon_envsys device is valid
	 *   and does not exist already in the list.
	 * - add device into the list.
	 * - create the plist structure.
	 */
	mutex_enter(&sme_list_mtx);
	LIST_FOREACH(lsme, &sysmon_envsys_list, sme_list) {
	       if (strcmp(lsme->sme_name, sme->sme_name) == 0) {
		       error = EEXIST;
		       goto out;
	       }
	}

	/* initialize the singly linked list for sensor descriptions */
	SLIST_INIT(&sme_names_list);

#ifdef COMPAT_40
	sme->sme_fsensor = sysmon_envsys_next_sensor_index;
	sysmon_envsys_next_sensor_index += sme->sme_nsensors;
#endif
	error = sysmon_envsys_createplist(sme);
	if (!error) {
		LIST_INSERT_HEAD(&sysmon_envsys_list, sme, sme_list);
		sme_uniqsensors = 0;
	}

out:
	mutex_exit(&sme_list_mtx);
	return error;
}

/*
 * sysmon_envsys_unregister:
 *
 *	+ Unregister an envsys device.
 *	+ Unregister all events associated with this device.
 *	+ Remove device dictionary.
 */
void
sysmon_envsys_unregister(struct sysmon_envsys *sme)
{
	struct sme_sensor_names *snames;

	KASSERT(sme != NULL);

	mutex_enter(&sme_list_mtx);
	while (sme->sme_flags & SME_FLAG_BUSY) {
		sme->sme_flags |= SME_FLAG_WANTED;
		cv_wait(&sme_list_cv, &sme_list_mtx);
	}
	prop_dictionary_remove(sme_propd, sme->sme_name);
	/*
	 * Unregister all events associated with this device.
	 */
	mutex_exit(&sme_list_mtx);
	sme_event_unregister_all(sme);
	mutex_enter(&sme_list_mtx);

	/*
	 * Remove all elements from the sensor names singly linked list.
	 */
	while (!SLIST_EMPTY(&sme_names_list)) {
		snames = SLIST_FIRST(&sme_names_list);
		SLIST_REMOVE_HEAD(&sme_names_list, sme_names);
		mutex_exit(&sme_list_mtx);
		kmem_free(snames, sizeof(*snames));
		mutex_enter(&sme_list_mtx);
	}
	LIST_REMOVE(sme, sme_list);
	mutex_exit(&sme_list_mtx);
}

/*
 * sysmon_envsys_find:
 *
 *	+ Find an envsys device.
 */
struct sysmon_envsys *
sysmon_envsys_find(const char *name)
{
	struct sysmon_envsys *sme;

	mutex_enter(&sme_list_mtx);
again:
	for (sme = LIST_FIRST(&sysmon_envsys_list); sme != NULL;
	     sme = LIST_NEXT(sme, sme_list)) {
			if (strcmp(sme->sme_name, name) == 0) {
				if (sme->sme_flags & SME_FLAG_BUSY) {
					sme->sme_flags |= SME_FLAG_WANTED;
					cv_wait(&sme_list_cv, &sme_list_mtx);
					goto again;
				}
				sme->sme_flags |= SME_FLAG_BUSY;
				break;
			}
	}
	mutex_exit(&sme_list_mtx);
	return sme;
}

/*
 * sysmon_envsys_release:
 *
 * 	+ Release an envsys device.
 */
void
sysmon_envsys_release(struct sysmon_envsys *sme)
{
	mutex_enter(&sme_list_mtx);
	if (sme->sme_flags & SME_FLAG_WANTED)
		cv_broadcast(&sme_list_cv);
	sme->sme_flags &= ~(SME_FLAG_BUSY | SME_FLAG_WANTED);
	mutex_exit(&sme_list_mtx);
}

/* compatibility function */
#ifdef COMPAT_40
struct sysmon_envsys *
sysmon_envsys_find_40(u_int idx)
{
	struct sysmon_envsys *sme;

	mutex_enter(&sme_list_mtx);
	for (sme = LIST_FIRST(&sysmon_envsys_list); sme != NULL;
	     sme = LIST_NEXT(sme, sme_list)) {
		if (idx >= sme->sme_fsensor &&
	    	    idx < (sme->sme_fsensor + sme->sme_nsensors))
			break;
	}
	mutex_exit(&sme_list_mtx);
	return sme;
}
#endif

/*
 * sysmon_envsys_createplist:
 *
 * 	+ Create the property list structure for a device.
 */
int
sysmon_envsys_createplist(struct sysmon_envsys *sme)
{
	envsys_data_t *edata;
	prop_array_t array;
	int i, nsens, error;

	nsens = error = 0;

	KASSERT(mutex_owned(&sme_list_mtx));
	mutex_exit(&sme_list_mtx);

	/* create the sysmon envsys device array. */
	array = prop_array_create();
	if (array == NULL)
		return EINVAL;

	/*
	 * Iterate over all sensors and create a dictionary with all
	 * values specified by the sysmon envsys driver.
	 */
	for (i = 0; i < sme->sme_nsensors; i++) {
		/*
		 * XXX:
		 *
		 * workaround for LKMs. First sensor used in a LKM,
		 * gets the index in sensor from all edata structures
		 * allocated in kernel. It is unknown to me why this
		 * value is not 0 when using a LKM.
		 *
		 * For now, overwrite its index to 0.
		 */
		if (i == 0)
			sme->sme_sensor_data[0].sensor = 0;

		edata = &sme->sme_sensor_data[i];
		sme_make_dictionary(sme, array, edata);
	}

	if (prop_array_count(array) == 0) {
		error = EINVAL;
		prop_object_release(array);
		return error;
	}

	/*
	 * <dict>
	 * 	<key>foo0</key>
	 * 	<array>
	 * 		...
	 */
	mutex_enter(&sme_mtx);
	if (!prop_dictionary_set(sme_propd, sme->sme_name, array)) {
		DPRINTF(("%s: prop_dictionary_set\n", __func__));
		error = EINVAL;
	}
	mutex_exit(&sme_mtx);


	prop_object_release(array);
	mutex_enter(&sme_list_mtx);
	return error;
}

/*
 * sme_register_sensorname:
 *
 * 	+ Registers a sensor name into the list maintained per device.
 */
static int
sme_register_sensorname(envsys_data_t *edata)
{
	struct sme_sensor_names *snames, *snames2 = NULL;

	KASSERT(edata != NULL);

	snames = kmem_zalloc(sizeof(*snames), KM_SLEEP);

	mutex_enter(&sme_list_mtx);
	SLIST_FOREACH(snames2, &sme_names_list, sme_names) {
		/* 
		 * Match sensors with empty and duplicate description.
		 */
		if (strlen(edata->desc) == 0 ||
		    strcmp(snames2->desc, edata->desc) == 0)
			if (snames2->assigned) {
				edata->flags |= ENVSYS_FNOTVALID;
				mutex_exit(&sme_list_mtx);
				DPRINTF(("%s: duplicate name "
				    "(%s)\n", __func__, edata->desc));
				kmem_free(snames, sizeof(*snames));
				return EEXIST;
			}
	}

	snames->assigned = true;
	(void)strlcpy(snames->desc, edata->desc, sizeof(snames->desc));
	DPRINTF(("%s: registering sensor name=%s\n", __func__, edata->desc));
	SLIST_INSERT_HEAD(&sme_names_list, snames, sme_names);
	sme_uniqsensors++;
	mutex_exit(&sme_list_mtx);

	return 0;
}

/*
 * sme_make_dictionary:
 *
 * 	+ Create sensor's dictionary in device's array.
 */
void
sme_make_dictionary(struct sysmon_envsys *sme, prop_array_t array,
		    envsys_data_t *edata)
{
	const struct sme_sensor_type *est = sme_sensor_type;
	const struct sme_sensor_state *ess = sme_sensor_state;
	const struct sme_sensor_state *esds = sme_sensor_drive_state;
	sme_event_drv_t *sme_evdrv_t = NULL;
	prop_dictionary_t dict;
	int i, j;

	i = j = 0;


	/*
	 * <array>
	 * 	<dict>
	 * 		...
	 */
	dict = prop_dictionary_create();
	if (dict == NULL) {
		DPRINTF(("%s: couldn't allocate a dictionary\n", __func__));
		goto invalidate_sensor;
	}

	/* find the correct unit for this sensor. */
	for (i = 0; est[i].type != -1; i++)
		if (est[i].type == edata->units)
			break;

	if (strcmp(est[i].desc, "unknown") == 0) {
		DPRINTF(("%s: invalid units type for sensor=%d\n",
		    __func__, edata->sensor));
		goto invalidate_sensor;
	}

	/*
	 * 		...
	 * 		<key>type</key>
	 * 		<string>foo</string>
	 * 		<key>description</key>
	 * 		<string>blah blah</string>
	 * 		...
	 */
	if (sme_sensor_upstring(dict, "type", est[i].desc))
		goto invalidate_sensor;

	if (strlen(edata->desc) == 0) {
		DPRINTF(("%s: invalid description for sensor=%d\n",
		    __func__, edata->sensor));
		goto invalidate_sensor;
	}

	/*
	 * Check if description was already assigned in other sensor.
	 */
	if (sme_register_sensorname(edata))
		goto out;

	if (sme_sensor_upstring(dict, "description", edata->desc))
		goto invalidate_sensor;

	/*
	 * Add sensor's state description.
	 *
	 * 		...
	 * 		<key>state</key>
	 * 		<string>valid</string>
	 * 		...
	 */
	for (j = 0; ess[j].type != -1; j++)
		if (ess[j].type == edata->state) 
			break;

	if (strcmp(ess[j].desc, "unknown") == 0) {
		DPRINTF(("%s: invalid state for sensor=%d\n",
		    __func__, edata->sensor));
		goto invalidate_sensor;
	}

	DPRINTF(("%s: sensor desc=%s type=%d state=%d\n",
	    __func__, edata->desc, edata->units, edata->state));

	if (sme_sensor_upstring(dict, "state", ess[j].desc))
		goto invalidate_sensor;

	/*
	 * add the percentage boolean object:
	 *
	 * 		...
	 * 		<key>want-percentage</key>
	 * 		<true/>
	 * 		...
	 */
	if (edata->flags & ENVSYS_FPERCENT)
		if (sme_sensor_upbool(dict, "want-percentage", true))
			goto out;

	/*
	 * Add the monitoring boolean object:
	 *
	 * 		...
	 * 		<key>monitoring-supported</key>
	 * 		<true/>
	 *		...
	 * 
	 * always false on Drive and Indicator types, they
	 * cannot be monitored.
	 *
	 */
	if ((edata->flags & ENVSYS_FMONNOTSUPP) ||
	    (edata->units == ENVSYS_INDICATOR) ||
	    (edata->units == ENVSYS_DRIVE)) {
		if (sme_sensor_upbool(dict, "monitoring-supported", false))
			goto out;
	} else {
		if (sme_sensor_upbool(dict, "monitoring-supported", true))
			goto out;
	}

	/*
	 * Add the drive-state object for drive sensors:
	 *
	 * 		...
	 * 		<key>drive-state</key>
	 * 		<string>drive is online</string>
	 * 		...
	 */
	if (edata->units == ENVSYS_DRIVE) {
		for (j = 0; esds[j].type != -1; j++)
			if (esds[j].type == edata->value_cur)
				break;

		if (sme_sensor_upstring(dict, "drive-state", esds[j].desc))
			goto out;
	}

	/* if sensor is enabled, add the following properties... */	
	if (edata->state == ENVSYS_SVALID) {
		/*
		 * 	...
		 * 	<key>rpms</key>
		 * 	<integer>2500</integer>
		 * 	<key>rfact</key>
		 * 	<integer>10000</integer>
		 * 	<key>cur-value</key>
		 * 	<integer>1250</integer>
		 * 	<key>min-value</key>
		 * 	<integer>800</integer>
		 * 	<key>max-value</integer>
		 * 	<integer>3000</integer>
		 * 	<key>avg-value</integer>
		 * 	<integer>1400</integer>
		 * </dict>
		 */
		if (edata->units == ENVSYS_SFANRPM)
			if (sme_sensor_upuint32(dict, "rpms", edata->rpms))
				goto out;

		if (edata->units == ENVSYS_SVOLTS_AC ||
		    edata->units == ENVSYS_SVOLTS_DC)
			if (sme_sensor_upint32(dict, "rfact", edata->rfact))
				goto out;

		if (sme_sensor_upint32(dict, "cur-value", edata->value_cur))
			goto out;

		if (edata->flags & ENVSYS_FVALID_MIN) {
			if (sme_sensor_upint32(dict,
					       "min-value",
					       edata->value_min))
				goto out;
		}

		if (edata->flags & ENVSYS_FVALID_MAX) {
			if (sme_sensor_upint32(dict,
					       "max-value",
					       edata->value_max))
				goto out;
		}

		if (edata->flags & ENVSYS_FVALID_AVG) {
			if (sme_sensor_upint32(dict,
					       "avg-value",
					        edata->value_avg))
				goto out;
		}
	}

	/*
	 * 	...
	 * </array>
	 */

	mutex_enter(&sme_mtx);
	if (!prop_array_set(array, sme_uniqsensors - 1, dict)) {
		mutex_exit(&sme_mtx);
		DPRINTF(("%s: prop_array_add\n", __func__));
		goto invalidate_sensor;
	}
	mutex_exit(&sme_mtx);

	/*
	 * Add a new event if a monitoring flag was set.
	 */
	if (edata->monitor) {
		sme_evdrv_t = kmem_zalloc(sizeof(*sme_evdrv_t), KM_SLEEP);

		sme_evdrv_t->sdict = dict;
		sme_evdrv_t->edata = edata;
		sme_evdrv_t->sme = sme;
		sme_evdrv_t->powertype = est[i].crittype;

		sysmon_task_queue_init();
		sysmon_task_queue_sched(0, sme_event_drvadd, sme_evdrv_t);
	}

out:
	prop_object_release(dict);
	return;

invalidate_sensor:
	mutex_enter(&sme_mtx);
	edata->flags |= ENVSYS_FNOTVALID;
	mutex_exit(&sme_mtx);
	prop_object_release(dict);
}

/*
 * sme_update_dictionary:
 *
 * 	+ Update per-sensor dictionaries with new values if there were
 * 	  changes, otherwise the object in dictionary is untouched.
 * 	+ Send a critical event if any sensor is in a critical condition.
 */
int
sme_update_dictionary(struct sysmon_envsys *sme)
{
	const struct sme_sensor_type *est = sme_sensor_type;
	const struct sme_sensor_state *ess = sme_sensor_state;
	const struct sme_sensor_state *esds = sme_sensor_drive_state;
	envsys_data_t *edata = NULL;
	prop_object_t array, dict;
	int i, j, error, invalid;

	error = invalid = 0;
	array = dict = NULL;

	/* retrieve the array of dictionaries in device. */
	array = prop_dictionary_get(sme_propd, sme->sme_name);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		DPRINTF(("%s: not an array (%s)\n", __func__, sme->sme_name));
		return EINVAL;
	}

	/* 
	 * - iterate over all sensors.
	 * - fetch new data.
	 * - check if data in dictionary is different than new data.
	 * - update dictionary if there were changes.
	 */
	for (i = 0; i < sme->sme_nsensors; i++) {
		edata = &sme->sme_sensor_data[i];
		/* skip invalid sensors */
		if (edata->flags & ENVSYS_FNOTVALID) {
			invalid++;
			continue;
		}

		/* 
		 * refresh sensor data via sme_gtredata only if the
		 * flag is not set.
		 */
		if ((sme->sme_flags & SME_DISABLE_GTREDATA) == 0) {
			error = (*sme->sme_gtredata)(sme, edata);
			if (error) {
				DPRINTF(("%s: gtredata[%d] failed\n",
				    __func__, i));
				return error;
			}
		}

		/* retrieve sensor's dictionary. */
		dict = prop_array_get(array, i - invalid);
		if (prop_object_type(dict) != PROP_TYPE_DICTIONARY) {
			DPRINTF(("%s: not a dictionary (%d:%s)\n",
			    __func__, edata->sensor, sme->sme_name));
			return EINVAL;
		}

		for (j = 0; ess[j].type != -1; j++)
			if (ess[j].type == edata->state)
				break;

		DPRINTFOBJ(("%s: state=%s type=%d flags=%d "
		    "units=%d sensor=%d\n", __func__, ess[j].desc,
		    ess[j].type, edata->flags, edata->units, edata->sensor));

		/* update sensor state */
		error = sme_sensor_upstring(dict, "state", ess[j].desc);
		if (error)
			break;

		for (j = 0; est[j].type != -1; j++)
			if (est[j].type == edata->units)
				break;

		/* update sensor type */
		error = sme_sensor_upstring(dict, "type", est[j].desc);
		if (error)
			break;

		/* update sensor current value */
		error = sme_sensor_upint32(dict,
					   "cur-value",
					   edata->value_cur);
		if (error)
			break;

		/*
		 * Integer and Indicator types do not the following
		 * values, so skip them.
		 */
		if (edata->units == ENVSYS_INTEGER ||
		    edata->units == ENVSYS_INDICATOR)
			continue;

		/* update sensor flags */
		if (edata->flags & ENVSYS_FPERCENT) {
			error = sme_sensor_upbool(dict,
						  "want-percentage",
						  true);
			if (error)
				break;
		}

		if (edata->flags & ENVSYS_FVALID_MAX) {
			error = sme_sensor_upint32(dict,
						   "max-value",
						   edata->value_max);
			if (error)
				break;
		}
						   
		if (edata->flags & ENVSYS_FVALID_MIN) {
			error = sme_sensor_upint32(dict,
						   "min-value",
						   edata->value_min);
			if (error)
				break;
		}

		if (edata->flags & ENVSYS_FVALID_AVG) {
			error = sme_sensor_upint32(dict,
						   "avg-value",
						   edata->value_avg);
			if (error)
				break;
		}

		/* update 'rpms' only in ENVSYS_SFANRPM. */
		if (edata->units == ENVSYS_SFANRPM) {
			error = sme_sensor_upuint32(dict,
						    "rpms",
						    edata->rpms);
			if (error)
				break;
		}

		/* update 'rfact' only in ENVSYS_SVOLTS_[AD]C. */
		if (edata->units == ENVSYS_SVOLTS_AC ||
		    edata->units == ENVSYS_SVOLTS_DC) {
			error = sme_sensor_upint32(dict,
						   "rfact",
						   edata->rfact);
			if (error)
				break;
		}
		
		/* update 'drive-state' only in ENVSYS_DRIVE. */
		if (edata->units == ENVSYS_DRIVE) {
			for (j = 0; esds[j].type != -1; j++)
				if (esds[j].type == edata->value_cur)
					break;

			error = sme_sensor_upstring(dict,
						    "drive-state",
						    esds[j].desc);
			if (error)
				break;
		}
	}

	return error;
}

/*
 * sme_userset_dictionary:
 *
 * 	+ Parses the userland dictionary and run the appropiate
 * 	  tasks that were requested.
 */
int
sme_userset_dictionary(struct sysmon_envsys *sme, prop_dictionary_t udict,
		       prop_array_t array)
{
	const struct sme_sensor_type *sst = sme_sensor_type;
	envsys_data_t *edata, *nedata;
	prop_dictionary_t dict;
	prop_object_t obj, obj1, obj2;
	int32_t critval;
	int i, invalid, error;
	const char *blah, *sname;
	bool targetfound = false;

	error = invalid = 0;
	blah = sname = NULL;

	/* get sensor's name from userland dictionary. */
	obj = prop_dictionary_get(udict, "sensor-name");
	if (prop_object_type(obj) != PROP_TYPE_STRING) {
		DPRINTF(("%s: sensor-name failed\n", __func__));
		return EINVAL;
	}

	/* iterate over the sensors to find the right one */
	for (i = 0; i < sme->sme_nsensors; i++) {
		edata = &sme->sme_sensor_data[i];
		/* skip sensors with duplicate description */
		if (edata->flags & ENVSYS_FNOTVALID) {
			invalid++;
			continue;
		}

		dict = prop_array_get(array, i - invalid);
		obj1 = prop_dictionary_get(dict, "description");

		/* is it our sensor? */
		if (!prop_string_equals(obj1, obj))
			continue;

		/*
		 * Check if a new description operation was
		 * requested by the user and set new description.
		 */
		if ((obj2 = prop_dictionary_get(udict, "new-description"))) {
			targetfound = true;
			blah = prop_string_cstring_nocopy(obj2);
			
			for (i = 0; i < sme->sme_nsensors; i++) {
				if (i == edata->sensor)
					continue;

				nedata = &sme->sme_sensor_data[i];
				if (strcmp(blah, nedata->desc) == 0) {
					error = EEXIST;
					break;
				}
			}

			if (error)
				break;

			error = sme_sensor_upstring(dict,
						    "description",
						    blah);
			if (!error)
				(void)strlcpy(edata->desc,
					      blah,
					      sizeof(edata->desc));

			break;
		}

		/* did the user want to remove a critical capacity limit? */
		obj2 = prop_dictionary_get(udict, "remove-critical-cap");
		if (obj2 != NULL) {
			targetfound = true;
			if ((edata->flags & ENVSYS_FMONNOTSUPP) ||
			    (edata->flags & ENVSYS_FPERCENT) == 0) {
				    error = ENOTSUP;
				    break;
			}

			sname = prop_string_cstring_nocopy(obj);
			error = sme_event_unregister(sname,
			    PENVSYS_EVENT_BATT_USERCAP);
			if (error)
				break;

			prop_dictionary_remove(dict, "critical-capacity");
			break;
		}

		/* did the user want to remove a critical min limit? */
		obj2 = prop_dictionary_get(udict, "remove-cmin-limit");
		if (obj2 != NULL) {
			targetfound = true;
			sname = prop_string_cstring_nocopy(obj);
			error = sme_event_unregister(sname,
			    PENVSYS_EVENT_USER_CRITMIN);
			if (error)
				break;

			prop_dictionary_remove(dict, "critical-min-limit");
			break;
		}

		/* did the user want to remove a critical max limit? */
		obj2 = prop_dictionary_get(udict, "remove-cmax-limit");
		if (obj2 != NULL) {
			targetfound = true;
			sname = prop_string_cstring_nocopy(obj);
			error = sme_event_unregister(sname,
			    PENVSYS_EVENT_USER_CRITMAX);
			if (error)
				break;

			prop_dictionary_remove(dict, "critical-max-limit");
			break;
		}

		/* did the user want to change rfact? */
		obj2 = prop_dictionary_get(udict, "new-rfact");
		if (obj2 != NULL) {
			targetfound = true;
			if (edata->flags & ENVSYS_FCHANGERFACT)
				edata->rfact = prop_number_integer_value(obj2);
			else
				error = ENOTSUP;

			break;
		}

		for (i = 0; sst[i].type != -1; i++)
			if (sst[i].type == edata->units)
				break;

		/* did the user want to set a critical capacity event? */
		obj2 = prop_dictionary_get(udict, "critical-capacity");
		if (obj2 != NULL) {
			targetfound = true;
			if ((edata->flags & ENVSYS_FMONNOTSUPP) ||
			    (edata->flags & ENVSYS_FPERCENT) == 0) {
				error = ENOTSUP;
				break;
			}

			critval = prop_number_integer_value(obj2);
			error = sme_event_add(dict,
					      edata,
					      sme->sme_name,
					      "critical-capacity",
					      critval,
					      PENVSYS_EVENT_BATT_USERCAP,
					      sst[i].crittype);
			break;
		}

		/* did the user want to set a critical max event? */
		obj2 = prop_dictionary_get(udict, "critical-max-limit");
		if (obj2 != NULL) {
			targetfound = true;
			if (edata->units == ENVSYS_INDICATOR ||
			    edata->flags & ENVSYS_FMONNOTSUPP) {
				error = ENOTSUP;
				break;
			}

			critval = prop_number_integer_value(obj2);
			error = sme_event_add(dict,
					      edata,
					      sme->sme_name,
					      "critical-max-limit",
					      critval,
					      PENVSYS_EVENT_USER_CRITMAX,
					      sst[i].crittype);
			break;
		}

		/* did the user want to set a critical min event? */
		obj2 = prop_dictionary_get(udict, "critical-min-limit");
		if (obj2 != NULL) {
			targetfound = true;
			if (edata->units == ENVSYS_INDICATOR ||
			    edata->flags & ENVSYS_FMONNOTSUPP) {
				error = ENOTSUP;
				break;
			}

			critval = prop_number_integer_value(obj2);
			error = sme_event_add(dict,
					      edata,
					      sme->sme_name,
					      "critical-min-limit",
					      critval,
					      PENVSYS_EVENT_USER_CRITMIN,
					      sst[i].crittype);
			break;
		}
	}

	/* invalid target? return the error */
	if (!targetfound)
		error = EINVAL;

	return error;
}
