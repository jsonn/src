/*	$NetBSD: libdm-compat.h,v 1.1.1.1.2.2 2008/12/13 14:39:36 haad Exp $	*/

/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2006 Red Hat, Inc. All rights reserved.
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

#ifndef _LINUX_LIBDM_COMPAT_H
#define _LINUX_LIBDM_COMPAT_H

#include "kdev_t.h"
#include "dm-ioctl.h"
#include <inttypes.h>
#include <sys/ioctl.h>

struct dm_task;
struct dm_info;

/*
 * Old versions of structures for backwards compatibility.
 */

struct dm_ioctl_v1 {
	uint32_t version[3];	/* in/out */
	uint32_t data_size;	/* total size of data passed in
				 * including this struct */

	uint32_t data_start;	/* offset to start of data
				 * relative to start of this struct */

	int32_t target_count;	/* in/out */
	int32_t open_count;	/* out */
	uint32_t flags;		/* in/out */

	__kernel_dev_t dev;	/* in/out */

	char name[DM_NAME_LEN];	/* device name */
	char uuid[DM_UUID_LEN];	/* unique identifier for
				 * the block device */
};

struct dm_target_spec_v1 {
	int32_t status;		/* used when reading from kernel only */
	uint64_t sector_start;
	uint32_t length;
	uint32_t next;

	char target_type[DM_MAX_TYPE_NAME];

};

struct dm_target_deps_v1 {
	uint32_t count;

	__kernel_dev_t dev[0];	/* out */
};

enum {
	/* Top level cmds */
	DM_VERSION_CMD_V1 = 0,
	DM_REMOVE_ALL_CMD_V1,

	/* device level cmds */
	DM_DEV_CREATE_CMD_V1,
	DM_DEV_REMOVE_CMD_V1,
	DM_DEV_RELOAD_CMD_V1,
	DM_DEV_RENAME_CMD_V1,
	DM_DEV_SUSPEND_CMD_V1,
	DM_DEV_DEPS_CMD_V1,
	DM_DEV_STATUS_CMD_V1,

	/* target level cmds */
	DM_TARGET_STATUS_CMD_V1,
	DM_TARGET_WAIT_CMD_V1,
};

#define DM_VERSION_V1       _IOWR(DM_IOCTL, DM_VERSION_CMD_V1, struct dm_ioctl)
#define DM_REMOVE_ALL_V1    _IOWR(DM_IOCTL, DM_REMOVE_ALL_CMD_V1, struct dm_ioctl)

#define DM_DEV_CREATE_V1    _IOWR(DM_IOCTL, DM_DEV_CREATE_CMD_V1, struct dm_ioctl)
#define DM_DEV_REMOVE_V1    _IOWR(DM_IOCTL, DM_DEV_REMOVE_CMD_V1, struct dm_ioctl)
#define DM_DEV_RELOAD_V1    _IOWR(DM_IOCTL, DM_DEV_RELOAD_CMD_V1, struct dm_ioctl)
#define DM_DEV_SUSPEND_V1   _IOWR(DM_IOCTL, DM_DEV_SUSPEND_CMD_V1, struct dm_ioctl)
#define DM_DEV_RENAME_V1    _IOWR(DM_IOCTL, DM_DEV_RENAME_CMD_V1, struct dm_ioctl)
#define DM_DEV_DEPS_V1      _IOWR(DM_IOCTL, DM_DEV_DEPS_CMD_V1, struct dm_ioctl)
#define DM_DEV_STATUS_V1    _IOWR(DM_IOCTL, DM_DEV_STATUS_CMD_V1, struct dm_ioctl)

#define DM_TARGET_STATUS_V1 _IOWR(DM_IOCTL, DM_TARGET_STATUS_CMD_V1, struct dm_ioctl)
#define DM_TARGET_WAIT_V1   _IOWR(DM_IOCTL, DM_TARGET_WAIT_CMD_V1, struct dm_ioctl)

/* *INDENT-OFF* */
static struct cmd_data _cmd_data_v1[] = {
	{ "create",	DM_DEV_CREATE_V1,	{1, 0, 0} },
	{ "reload",	DM_DEV_RELOAD_V1,	{1, 0, 0} },
	{ "remove",	DM_DEV_REMOVE_V1,	{1, 0, 0} },
	{ "remove_all",	DM_REMOVE_ALL_V1,	{1, 0, 0} },
	{ "suspend",	DM_DEV_SUSPEND_V1,	{1, 0, 0} },
	{ "resume",	DM_DEV_SUSPEND_V1,	{1, 0, 0} },
	{ "info",	DM_DEV_STATUS_V1,	{1, 0, 0} },
	{ "deps",	DM_DEV_DEPS_V1,		{1, 0, 0} },
	{ "rename",	DM_DEV_RENAME_V1,	{1, 0, 0} },
	{ "version",	DM_VERSION_V1,		{1, 0, 0} },
	{ "status",	DM_TARGET_STATUS_V1,	{1, 0, 0} },
	{ "table",	DM_TARGET_STATUS_V1,	{1, 0, 0} },
	{ "waitevent",	DM_TARGET_WAIT_V1,	{1, 0, 0} },
	{ "names",	0,			{4, 0, 0} },
	{ "clear",	0,			{4, 0, 0} },
	{ "mknodes",	0,			{4, 0, 0} },
	{ "versions",	0,			{4, 1, 0} },
	{ "message",	0,			{4, 2, 0} },
	{ "setgeometry",0,			{4, 6, 0} },
};
/* *INDENT-ON* */

#endif
