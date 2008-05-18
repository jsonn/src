/*	$NetBSD: apmsubr.c,v 1.4.44.1 2008/05/18 12:36:14 yamt Exp $ */

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John Kohl.
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

#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <machine/apmvar.h>
#include <err.h>
#include "apm-proto.h"

const char *
battstate(int state)
{
    switch (state) {
    case APM_BATT_HIGH:
	return "high";
    case APM_BATT_LOW:
	return "low";
    case APM_BATT_CRITICAL:
	return "CRITICAL";
    case APM_BATT_CHARGING:
	return "charging";
    case APM_BATT_ABSENT:
	return "absent";
    case APM_BATT_UNKNOWN:
	return "unknown (absent?)";
    default:
	return "invalid battery state";
    }
}

const char *
ac_state(int state)
{
    switch (state) {
    case APM_AC_OFF:
	return "not connected";
    case APM_AC_ON:
	return "connected";
    case APM_AC_BACKUP:
	return "backup power source";
    case APM_AC_UNKNOWN:
	return "not known";
    default:
	return "invalid AC status";
    }
}
