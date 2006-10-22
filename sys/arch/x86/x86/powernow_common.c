/*	$NetBSD: powernow_common.c,v 1.2.10.1 2006/10/22 06:05:16 yamt Exp $	*/

/*
 *  Copyright (c) 2006 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to the NetBSD Foundation
 *   by Quentin Garnier.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *         This product includes software developed by the NetBSD
 *         Foundation, Inc. and its contributors.
 *  4. Neither the name of The NetBSD Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: powernow_common.c,v 1.2.10.1 2006/10/22 06:05:16 yamt Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <x86/include/powernow.h>

int
powernow_probe(struct cpu_info *ci)
{
	uint32_t regs[4];
	char line[80];

	CPUID(0x80000000, regs[0], regs[1], regs[2], regs[3]);

	/* We need CPUID(0x80000007) */
	if (regs[0] < 0x80000007)
		return 0;
	CPUID(0x80000007, regs[0], regs[1], regs[2], regs[3]);

	bitmask_snprintf(regs[3], "\20\6STC\5TM\4TTP\3VID\2FID\1TS", line,
	    sizeof line);
	aprint_normal("%s: AMD Power Management features: %s\n",
	    device_xname(ci->ci_dev), line);

	/*
	 * For now we're only interested in FID and VID for frequency scaling.
	 */

	return (regs[3] & AMD_PN_FID_VID) == AMD_PN_FID_VID;
}
