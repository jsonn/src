/*	$NetBSD: idprom.c,v 1.19.4.1 1997/10/14 10:18:59 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*
 * Machine ID PROM - system type and serial number
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/autoconf.h>
#include <machine/control.h>
#include <machine/idprom.h>
#include <machine/machdep.h>
#include <machine/mon.h>

/*
 * This structure is what this driver is all about.
 * It is copied from the device early in startup.
 */
struct idprom identity_prom;

/*
 * This is called very early during startup to
 * get a copy of the idprom from control space.
 */
void
idprom_init()
{
	struct idprom *idp;
	char *dst;
	vm_offset_t src;
	int len, x, xorsum;
	union {
		long l;
		char c[4];
	} hid;

	/* Copy the IDPROM contents and do the checksum. */
	idp = &identity_prom;
	dst = (char*)idp;
	src = IDPROM_BASE;
	len = IDPROM_SIZE;
	xorsum = 0;	/* calculated as xor of data */

	do {
		x = get_control_byte(src++);
		*dst++ = x;
		xorsum ^= x;
	} while (--len > 0);

	if (xorsum != 0)
		mon_printf("idprom: bad checksum=%d\n", xorsum);
	if (idp->idp_format < 1)
		mon_printf("idprom: bad version=%d\n", idp->idp_format);

	/*
	 * Construct the hostid from the idprom contents.
	 * This appears to be the way SunOS does it.
	 */
	hid.c[0] = idp->idp_machtype;
	hid.c[1] = idp->idp_serialnum[0];
	hid.c[2] = idp->idp_serialnum[1];
	hid.c[3] = idp->idp_serialnum[2];
	hostid = hid.l;
}

void
idprom_etheraddr(eaddrp)
	u_char *eaddrp;
{
	u_char *src, *dst;
	int len = 6;

	src = identity_prom.idp_etheraddr;
	dst = eaddrp;

	do *dst++ = *src++;
	while (--len > 0);
}
