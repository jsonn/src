/*      $NetBSD: clockctl.h,v 1.8.8.1 2005/04/29 11:29:37 kent Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 */

#ifndef _SYS_CLOCKCTL_H_
#define _SYS_CLOCKCTL_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/ioctl.h>
#include <sys/mount.h>	/* For fhandle_t */
#include <sys/sa.h>	/* for sa_upcall_t */
#include <sys/syscallargs.h>

#if defined(NTP) || !defined(_KERNEL)
struct clockctl_ntp_adjtime_args {
	struct sys_ntp_adjtime_args uas;
	register_t retval;
};
#define CLOCKCTL_NTP_ADJTIME _IOWR('C', 0x4, struct clockctl_ntp_adjtime_args)
#endif

#define CLOCKCTL_SETTIMEOFDAY _IOW('C', 0x1, struct sys_settimeofday_args)
#define CLOCKCTL_ADJTIME _IOWR('C', 0x2, struct sys_adjtime_args)
#define CLOCKCTL_CLOCK_SETTIME _IOW('C', 0x3, struct sys_clock_settime_args)

#ifdef _KERNEL
void    clockctlattach(int);
int     clockctlopen(dev_t, int, int, struct proc *);
int     clockctlclose(dev_t, int, int, struct proc *);
int     clockctlioctl(dev_t, u_long, caddr_t, int, struct proc *);
#endif

#endif /* _SYS_CLOCKCTL_H_ */
