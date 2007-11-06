/* $NetBSD: sysmon_envsys_events.c,v 1.19.6.1 2007/11/06 23:30:19 matt Exp $ */

/*-
 * Copyright (c) 2007 Juan Romero Pardines.
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
 */

/*
 * sysmon_envsys(9) events framework.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_envsys_events.c,v 1.19.6.1 2007/11/06 23:30:19 matt Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/kmem.h>
#include <sys/callout.h>

/* #define ENVSYS_DEBUG */
#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>

struct sme_sensor_event {
	int		state;
	int		event;
};

static const struct sme_sensor_event sme_sensor_event[] = {
	{ ENVSYS_SVALID, 	PENVSYS_EVENT_NORMAL },
	{ ENVSYS_SCRITICAL, 	PENVSYS_EVENT_CRITICAL },
	{ ENVSYS_SCRITOVER, 	PENVSYS_EVENT_CRITOVER },
	{ ENVSYS_SCRITUNDER, 	PENVSYS_EVENT_CRITUNDER },
	{ ENVSYS_SWARNOVER, 	PENVSYS_EVENT_WARNOVER },
	{ ENVSYS_SWARNUNDER,	PENVSYS_EVENT_WARNUNDER },
	{ -1, 			-1 }
};

static struct workqueue *seewq;
static struct callout seeco;
static bool sme_events_initialized = false;
static bool sysmon_low_power = false;
kmutex_t sme_mtx, sme_event_init_mtx;
kcondvar_t sme_cv;

/* 10 seconds of timeout for the callout */
static int sme_events_timeout = 10;
static int sme_events_timeout_sysctl(SYSCTLFN_PROTO);
#define SME_EVTIMO 	(sme_events_timeout * hz)

static bool sme_event_check_low_power(void);
static bool sme_battery_critical(envsys_data_t *);

/*
 * sysctl(9) stuff to handle the refresh value in the callout
 * function.
 */
static int
sme_events_timeout_sysctl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int timo, error;

	node = *rnode;
	timo = sme_events_timeout;
	node.sysctl_data = &timo;

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || !newp)
		return error;

	/* min 1s */
	if (timo < 1)
		return EINVAL;

	sme_events_timeout = timo;
	return 0;
}

SYSCTL_SETUP(sysctl_kern_envsys_timeout_setup, "sysctl kern.envsys subtree")
{
	const struct sysctlnode *node, *envsys_node;

	sysctl_createv(clog, 0, NULL, &node,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "kern", NULL,
			NULL, 0, NULL, 0,
			CTL_KERN, CTL_EOL);

	sysctl_createv(clog, 0, &node, &envsys_node,
			0,
			CTLTYPE_NODE, "envsys", NULL,
			NULL, 0, NULL, 0,
			CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &envsys_node, &node,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "refresh_value",
			SYSCTL_DESCR("wait time in seconds to refresh "
			    "sensors being monitored"),
			sme_events_timeout_sysctl, 0, &sme_events_timeout, 0,
			CTL_CREATE, CTL_EOL);
}

/*
 * sme_event_register:
 *
 * 	+ Registers a new sysmon envsys event or updates any event
 * 	  already in the queue.
 */
int
sme_event_register(prop_dictionary_t sdict, envsys_data_t *edata,
		   const char *drvn, const char *objkey,
		   int32_t critval, int crittype, int powertype)
{
	sme_event_t *see = NULL;
	prop_object_t obj;
	bool critvalup = false;
	int error = 0;

	KASSERT(sdict != NULL || edata != NULL);

	mutex_enter(&sme_mtx);
	/* 
	 * check if the event is already on the list and return
	 * EEXIST if value provided hasn't been changed.
	 */
	LIST_FOREACH(see, &sme_events_list, see_list) {
		if (strcmp(edata->desc, see->pes.pes_sensname) == 0)
			if (crittype == see->type) {
				if (see->critval == critval) {
					DPRINTF(("%s: dev=%s sensor=%s type=%d "
				    	    "(already exists)\n", __func__,
				    	    see->pes.pes_dvname,
				    	    see->pes.pes_sensname, see->type));
					mutex_exit(&sme_mtx);
					return EEXIST;
				}
				critvalup = true;
				break;
			}
	}

	/* 
	 * Critical condition operation requested by userland.
	 */
	if (objkey && critval && critvalup) {
		obj = prop_dictionary_get(sdict, objkey);
		if (obj) {
			/* 
			 * object is already in dictionary and value
			 * provided is not the same than we have
			 * currently,  update the critical value.
			 */
			see->critval = critval;
			DPRINTF(("%s: sensor=%s type=%d (critval updated)\n",
				 __func__, edata->desc, see->type));
			error = sme_sensor_upint32(sdict, objkey, critval);
			mutex_exit(&sme_mtx);
			return error;
		}
	}

	/* 
	 * The event is not in on the list or in a dictionary, create a new
	 * sme event, assign required members and update the object in
	 * the dictionary.
	 */
	see = NULL;
	see = kmem_zalloc(sizeof(*see), KM_NOSLEEP);
	if (see == NULL) {
		mutex_exit(&sme_mtx);
		return ENOMEM;
	}

	see->critval = critval;
	see->type = crittype;
	(void)strlcpy(see->pes.pes_dvname, drvn,
	    sizeof(see->pes.pes_dvname));
	see->pes.pes_type = powertype;
	(void)strlcpy(see->pes.pes_sensname, edata->desc,
	    sizeof(see->pes.pes_sensname));
	see->snum = edata->sensor;

	LIST_INSERT_HEAD(&sme_events_list, see, see_list);
	if (objkey && critval) {
		error = sme_sensor_upint32(sdict, objkey, critval);
		if (error) {
			mutex_exit(&sme_mtx);
			goto out;
		}
	}
	DPRINTF(("%s: registering dev=%s sensor=%s snum=%d type=%d "
	    "critval=%" PRIu32 "\n", __func__,
	    see->pes.pes_dvname, see->pes.pes_sensname,
	    see->snum, see->type, see->critval));
	/*
	 * Initialize the events framework if it wasn't initialized
	 * before.
	 */
	mutex_enter(&sme_event_init_mtx);
	mutex_exit(&sme_mtx);
	if (sme_events_initialized == false)
		error = sme_events_init();
	mutex_exit(&sme_event_init_mtx);
out:
	if (error)
		kmem_free(see, sizeof(*see));
	return error;
}

/*
 * sme_event_unregister_all:
 *
 * 	+ Unregisters all sysmon envsys events associated with a
 * 	  sysmon envsys device.
 */
void
sme_event_unregister_all(const char *sme_name)
{
	sme_event_t *see;
	int evcounter = 0;

	KASSERT(mutex_owned(&sme_mtx));
	KASSERT(sme_name != NULL);

	LIST_FOREACH(see, &sme_events_list, see_list) {
		if (strcmp(see->pes.pes_dvname, sme_name) == 0)
			evcounter++;
	}

	DPRINTF(("%s: total events %d (%s)\n", __func__,
	    evcounter, sme_name));

	while ((see = LIST_FIRST(&sme_events_list))) {
		if (evcounter == 0)
			break;

		if (strcmp(see->pes.pes_dvname, sme_name) == 0) {
			DPRINTF(("%s: event %s %d removed (%s)\n", __func__,
			    see->pes.pes_sensname, see->type, sme_name));

			while (see->see_flags & SME_EVENT_WORKING)
				cv_wait(&sme_cv, &sme_mtx);

			LIST_REMOVE(see, see_list);
			kmem_free(see, sizeof(*see));
			evcounter--;
		}
	}

	if (LIST_EMPTY(&sme_events_list)) {
		mutex_enter(&sme_event_init_mtx);
		if (sme_events_initialized)
			sme_events_destroy();
		mutex_exit(&sme_event_init_mtx);
	}
}

/*
 * sme_event_unregister:
 *
 * 	+ Unregisters a sysmon envsys event.
 */
int
sme_event_unregister(const char *sensor, int type)
{
	sme_event_t *see;
	bool found = false;

	KASSERT(mutex_owned(&sme_mtx));
	KASSERT(sensor != NULL);

	LIST_FOREACH(see, &sme_events_list, see_list) {
		if (strcmp(see->pes.pes_sensname, sensor) == 0) {
			if (see->type == type) {
				found = true;
				break;
			}
		}
	}

	if (!found)
		return EINVAL;

	while (see->see_flags & SME_EVENT_WORKING)
		cv_wait(&sme_cv, &sme_mtx);

	DPRINTF(("%s: removing dev=%s sensor=%s type=%d\n",
	    __func__, see->pes.pes_dvname, sensor, type));
	LIST_REMOVE(see, see_list);
	/*
	 * So the events list is empty, we'll do the following:
	 *
	 * 	- stop and destroy the callout.
	 * 	- destroy the workqueue.
	 */
	if (LIST_EMPTY(&sme_events_list)) {
		mutex_enter(&sme_event_init_mtx);
		sme_events_destroy();
		mutex_exit(&sme_event_init_mtx);
	}

	kmem_free(see, sizeof(*see));
	return 0;
}

/*
 * sme_event_drvadd:
 *
 * 	+ Adds a new sysmon envsys event for a driver if a sensor
 * 	  has set any accepted monitoring flag.
 */
void
sme_event_drvadd(void *arg)
{
	sme_event_drv_t *sed_t = arg;
	int error = 0;

	KASSERT(sed_t != NULL);

#define SEE_REGEVENT(a, b, c)						\
do {									\
	if (sed_t->edata->flags & (a)) {				\
		char str[ENVSYS_DESCLEN] = "monitoring-state-";		\
									\
		error = sme_event_register(sed_t->sdict,		\
				      sed_t->edata,			\
				      sed_t->sme->sme_name,		\
				      NULL,				\
				      0,				\
				      (b),				\
				      sed_t->powertype);		\
		if (error && error != EEXIST)				\
			printf("%s: failed to add event! "		\
			    "error=%d sensor=%s event=%s\n",		\
			    __func__, error, sed_t->edata->desc, (c));	\
		else {							\
			(void)strlcat(str, (c), sizeof(str));		\
			prop_dictionary_set_bool(sed_t->sdict,		\
						 str,			\
						 true);			\
		}							\
	}								\
} while (/* CONSTCOND */ 0)

	SEE_REGEVENT(ENVSYS_FMONCRITICAL,
		     PENVSYS_EVENT_CRITICAL,
		     "critical");

	SEE_REGEVENT(ENVSYS_FMONCRITUNDER,
		     PENVSYS_EVENT_CRITUNDER,
		     "critunder");

	SEE_REGEVENT(ENVSYS_FMONCRITOVER,
		     PENVSYS_EVENT_CRITOVER,
		     "critover");

	SEE_REGEVENT(ENVSYS_FMONWARNUNDER,
		     PENVSYS_EVENT_WARNUNDER,
		     "warnunder");

	SEE_REGEVENT(ENVSYS_FMONWARNOVER,
		     PENVSYS_EVENT_WARNOVER,
		     "warnover");

	SEE_REGEVENT(ENVSYS_FMONSTCHANGED,
		     PENVSYS_EVENT_STATE_CHANGED,
		     "state-changed");

	/* we are done, free memory now */
	kmem_free(sed_t, sizeof(*sed_t));
}

/*
 * sme_events_init:
 *
 * 	+ Initializes the events framework.
 */
int
sme_events_init(void)
{
	int error;

	KASSERT(mutex_owned(&sme_event_init_mtx));

	error = workqueue_create(&seewq, "envsysev",
	    sme_events_worker, NULL, PRI_NONE, IPL_SOFTCLOCK, WQ_MPSAFE);
	if (error)
		goto out;

	callout_init(&seeco, 0);
	callout_setfunc(&seeco, sme_events_check, NULL);
	callout_schedule(&seeco, SME_EVTIMO);
	sme_events_initialized = true;
	DPRINTF(("%s: events framework initialized\n", __func__));

out:
	return error;
}

/*
 * sme_events_destroy:
 *
 * 	+ Destroys the events framework: the workqueue and the
 * 	  callout are stopped and destroyed because there are not
 * 	  events in the queue.
 */
void
sme_events_destroy(void)
{
	KASSERT(mutex_owned(&sme_event_init_mtx));

	callout_stop(&seeco);
	sme_events_initialized = false;
	DPRINTF(("%s: events framework destroyed\n", __func__));
	callout_destroy(&seeco);
	workqueue_destroy(seewq);
}

/*
 * sme_events_check:
 *
 * 	+ Runs the work on each sysmon envsys event in our
 * 	  workqueue periodically with callout.
 */
void
sme_events_check(void *arg)
{
	sme_event_t *see;

	LIST_FOREACH(see, &sme_events_list, see_list) {
		DPRINTFOBJ(("%s: dev=%s sensor=%s type=%d\n",
		    __func__,
		    see->pes.pes_dvname,
		    see->pes.pes_sensname,
		    see->type));
		workqueue_enqueue(seewq, &see->see_wk, NULL);
	}
	/*
	 * Now that the events list was checked, reset the refresh value.
	 */
	LIST_FOREACH(see, &sme_events_list, see_list)
		see->see_flags &= ~SME_EVENT_REFRESHED;

	if (!sysmon_low_power)
		callout_schedule(&seeco, SME_EVTIMO);
}

/*
 * sme_events_worker:
 *
 * 	+ workqueue thread that checks if there's a critical condition
 * 	  and sends an event if the condition was triggered.
 */
void
sme_events_worker(struct work *wk, void *arg)
{
	const struct sme_description_table *sdt = NULL;
	const struct sme_sensor_event *sse = sme_sensor_event;
	sme_event_t *see = (void *)wk;
	struct sysmon_envsys *sme;
	envsys_data_t *edata;
	int i, state, error;

	KASSERT(wk == &see->see_wk);

	state = error = 0;

	mutex_enter(&sme_mtx);
	see->see_flags |= SME_EVENT_WORKING;

	/*
	 * We have to find the sme device by looking
	 * at the power envsys device name.
	 */
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list)
		if (strcmp(sme->sme_name, see->pes.pes_dvname) == 0)
			break;
	if (!sme)
		goto out;

	/* get the sensor with the index specified in see->snum */
	edata = &sme->sme_sensor_data[see->snum];

	/* 
	 * refresh the sensor that was marked with a critical event
	 * only if it wasn't refreshed before or if the driver doesn't
	 * use its own method for refreshing.
	 */
	if ((sme->sme_flags & SME_DISABLE_GTREDATA) == 0) {
		if ((see->see_flags & SME_EVENT_REFRESHED) == 0) {
			error = (*sme->sme_gtredata)(sme, edata);
			if (error)
				goto out;
			see->see_flags |= SME_EVENT_REFRESHED;
		}
	}

	DPRINTFOBJ(("%s: desc=%s sensor=%d units=%d value_cur=%d\n",
	    __func__, edata->desc, edata->sensor,
	    edata->units, edata->value_cur));

#define SME_SEND_NORMALEVENT()						\
do {									\
	see->evsent = false;						\
	sysmon_penvsys_event(&see->pes, PENVSYS_EVENT_NORMAL);		\
} while (/* CONSTCOND */ 0)

#define SME_SEND_EVENT(type)						\
do {									\
	see->evsent = true;						\
	sysmon_penvsys_event(&see->pes, (type));			\
} while (/* CONSTCOND */ 0)


	switch (see->type) {
	/*
	 * if state is the same than the one that matches sse[i].state,
	 * send the event...
	 */
	case PENVSYS_EVENT_CRITICAL:
	case PENVSYS_EVENT_CRITUNDER:
	case PENVSYS_EVENT_CRITOVER:
	case PENVSYS_EVENT_WARNUNDER:
	case PENVSYS_EVENT_WARNOVER:
		for (i = 0; sse[i].state != -1; i++)
			if (sse[i].event == see->type)
				break;

		if (see->evsent && edata->state == ENVSYS_SVALID)
			SME_SEND_NORMALEVENT();

		if (!see->evsent && edata->state == sse[i].state)
			SME_SEND_EVENT(see->type);

		break;
	/*
	 * if value_cur is lower than the limit, send the event...
	 */
	case PENVSYS_EVENT_BATT_USERCAP:
	case PENVSYS_EVENT_USER_CRITMIN:
		if (see->evsent && edata->value_cur > see->critval)
			SME_SEND_NORMALEVENT();

		if (!see->evsent && edata->value_cur < see->critval)
			SME_SEND_EVENT(see->type);

		break;
	/*
	 * if value_cur is higher than the limit, send the event...
	 */
	case PENVSYS_EVENT_USER_CRITMAX:
		if (see->evsent && edata->value_cur < see->critval)
			SME_SEND_NORMALEVENT();

		if (!see->evsent && edata->value_cur > see->critval)
			SME_SEND_EVENT(see->type);

		break;
	/*
	 * if value_cur is not normal (battery) or online (drive),
	 * send the event...
	 */
	case PENVSYS_EVENT_STATE_CHANGED:
		/* the state has not been changed, just ignore the event */
		if (edata->value_cur == see->evsent)
			break;

		switch (edata->units) {
		case ENVSYS_DRIVE:
			sdt = sme_get_description_table(SME_DESC_DRIVE_STATES);
			state = ENVSYS_DRIVE_ONLINE;
			break;
		case ENVSYS_BATTERY_CAPACITY:
			sdt =
			    sme_get_description_table(SME_DESC_BATTERY_CAPACITY);
			state = ENVSYS_BATTERY_CAPACITY_NORMAL;
			break;
		default:
			panic("%s: invalid units for ENVSYS_FMONSTCHANGED",
			    __func__);
		}

		for (i = 0; sdt[i].type != -1; i++)
			if (sdt[i].type == edata->value_cur)
				break;

		/* copy current state description  */
		(void)strlcpy(see->pes.pes_statedesc, sdt[i].desc,
		    sizeof(see->pes.pes_statedesc));

		/* state is ok again... send a normal event */
		if (see->evsent && edata->value_cur == state)
			SME_SEND_NORMALEVENT();

		/* state has been changed... send event */
		if (see->evsent || edata->value_cur != state) {
			/* save current drive state */
			see->evsent = edata->value_cur;
			sysmon_penvsys_event(&see->pes, see->type);
		}

		/*
		 * Check if the system is running in low power and send the
		 * event to powerd (if running) or shutdown the system
		 * otherwise.
		 */
		if (!sysmon_low_power && sme_event_check_low_power()) {
			struct penvsys_state pes;

			pes.pes_type = PENVSYS_TYPE_BATTERY;
			sysmon_penvsys_event(&pes, PENVSYS_EVENT_LOW_POWER);
			sysmon_low_power = true;
			callout_stop(&seeco);
		}

		break;
	}
out:
	see->see_flags &= ~SME_EVENT_WORKING;
	cv_broadcast(&sme_cv);
	mutex_exit(&sme_mtx);
}

static bool
sme_event_check_low_power(void)
{
	struct sysmon_envsys *sme;
	envsys_data_t *edata;
	int i;
	bool battery, batterycap, batterycharge;

	KASSERT(mutex_owned(&sme_mtx));

	battery = batterycap = batterycharge = false;

	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list)
		if (sme->sme_class == SME_CLASS_ACADAPTER)
			break;

	/*
	 * No AC Adapter devices were found, do nothing.
	 */
	if (!sme)
		return false;

	/*
	 * If there's an AC Adapter connected, there's no need
	 * to continue...
	 */
	for (i = 0; i < sme->sme_nsensors; i++) {
		edata = &sme->sme_sensor_data[i];
		if (edata->units == ENVSYS_INDICATOR) {
			if (edata->value_cur)
				return false;
		}
	}

	/*
	 * Check for battery devices and its state.
	 */
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		if (sme->sme_class != SME_CLASS_BATTERY)
			continue;

		/*
		 * We've found a battery device...
		 */
		battery = true;
		for (i = 0; i < sme->sme_nsensors; i++) {
			edata = &sme->sme_sensor_data[i];
			if (edata->units == ENVSYS_BATTERY_CAPACITY) {
				batterycap = true;
				if (!sme_battery_critical(edata))
					return false;
			} else if (edata->units == ENVSYS_BATTERY_CHARGE) {
				batterycharge = true;
				if (edata->value_cur)
					return false;
			}
		}
	}
	if (!battery || !batterycap || !batterycharge)
		return false;

	/*
	 * All batteries in low/critical capacity and discharging.
	 */
	return true;
}

static bool
sme_battery_critical(envsys_data_t *edata)
{
	if (edata->value_cur == ENVSYS_BATTERY_CAPACITY_CRITICAL ||
	    edata->value_cur == ENVSYS_BATTERY_CAPACITY_LOW)
		return true;

	return false;
}
