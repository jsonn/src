/*
 * Copyright (C) 2005-2007 Red Hat, Inc. All rights reserved.
 *
 * This file is part of the device-mapper userspace tools.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Note that this file is released only as part of a technology preview
 * and its contents may change in future updates in ways that do not
 * preserve compatibility.
 */

#ifndef LIB_DMEVENT_H
#define LIB_DMEVENT_H

#include <stdint.h>

/*
 * Event library interface.
 */

enum dm_event_mask {
	DM_EVENT_SETTINGS_MASK  = 0x0000FF,
	DM_EVENT_SINGLE		= 0x000001, /* Report multiple errors just once. */
	DM_EVENT_MULTI		= 0x000002, /* Report all of them. */

	DM_EVENT_ERROR_MASK     = 0x00FF00,
	DM_EVENT_SECTOR_ERROR	= 0x000100, /* Failure on a particular sector. */
	DM_EVENT_DEVICE_ERROR	= 0x000200, /* Device failure. */
	DM_EVENT_PATH_ERROR	= 0x000400, /* Failure on an io path. */
	DM_EVENT_ADAPTOR_ERROR	= 0x000800, /* Failure of a host adaptor. */

	DM_EVENT_STATUS_MASK    = 0xFF0000,
	DM_EVENT_SYNC_STATUS	= 0x010000, /* Mirror synchronization completed/failed. */
	DM_EVENT_TIMEOUT	= 0x020000, /* Timeout has occured */

	DM_EVENT_REGISTRATION_PENDING = 0x1000000, /* Monitor thread is setting-up/shutting-down */
};

#define DM_EVENT_ALL_ERRORS DM_EVENT_ERROR_MASK

struct dm_event_handler;

struct dm_event_handler *dm_event_handler_create(void);
void dm_event_handler_destroy(struct dm_event_handler *dmevh);

/*
 * Path of shared library to handle events.
 *
 * All of dso, device_name and uuid strings are duplicated, you do not
 * need to keep the pointers valid after the call succeeds. Thes may
 * return -ENOMEM though.
 */
int dm_event_handler_set_dso(struct dm_event_handler *dmevh, const char *path);

/*
 * Identify the device to monitor by exactly one of device_name, uuid or
 * device number. String arguments are duplicated, see above.
 */
int dm_event_handler_set_dev_name(struct dm_event_handler *dmevh, const char *device_name);

int dm_event_handler_set_uuid(struct dm_event_handler *dmevh, const char *uuid);

void dm_event_handler_set_major(struct dm_event_handler *dmevh, int major);
void dm_event_handler_set_minor(struct dm_event_handler *dmevh, int minor);
void dm_event_handler_set_timeout(struct dm_event_handler *dmevh, int timeout);

/*
 * Specify mask for events to monitor.
 */
void dm_event_handler_set_event_mask(struct dm_event_handler *dmevh,
				     enum dm_event_mask evmask);

const char *dm_event_handler_get_dso(const struct dm_event_handler *dmevh);
const char *dm_event_handler_get_dev_name(const struct dm_event_handler *dmevh);
const char *dm_event_handler_get_uuid(const struct dm_event_handler *dmevh);
int dm_event_handler_get_major(const struct dm_event_handler *dmevh);
int dm_event_handler_get_minor(const struct dm_event_handler *dmevh);
int dm_event_handler_get_timeout(const struct dm_event_handler *dmevh);
enum dm_event_mask dm_event_handler_get_event_mask(const struct dm_event_handler *dmevh);

/* FIXME Review interface (what about this next thing?) */
int dm_event_get_registered_device(struct dm_event_handler *dmevh, int next);

/*
 * Initiate monitoring using dmeventd.
 */
int dm_event_register_handler(const struct dm_event_handler *dmevh);
int dm_event_unregister_handler(const struct dm_event_handler *dmevh);

/* Prototypes for DSO interface, see dmeventd.c, struct dso_data for
   detailed descriptions. */
void process_event(struct dm_task *dmt, enum dm_event_mask evmask, void **user);
int register_device(const char *device_name, const char *uuid, int major, int minor, void **user);
int unregister_device(const char *device_name, const char *uuid, int major,
		      int minor, void **user);

#endif
