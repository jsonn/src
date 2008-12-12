/*	$NetBSD: dm-logging.h,v 1.1.1.1.2.2 2008/12/12 16:32:58 haad Exp $	*/

/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2007 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DM_LOGGING_H
#define _DM_LOGGING_H

#include "libdevmapper.h"

extern dm_log_fn dm_log;

#define plog(l, x...) dm_log(l, __FILE__, __LINE__, ## x)

#include "log.h"

#endif
