/*	$NetBSD: ibm4xx_autoconf.c,v 1.2.2.2 2005/01/17 19:30:09 skrll Exp $	*/
/*	Original Tag: ibm4xxgpx_autoconf.c,v 1.2 2004/10/23 17:12:22 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ibm4xx_autoconf.c,v 1.2.2.2 2005/01/17 19:30:09 skrll Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/cpu.h>

void
ibm4xx_device_register(struct device *dev, void *aux)
{
	struct device *parent = dev->dv_parent;

	if (strcmp(dev->dv_cfdata->cf_name, "emac") == 0 &&
	    strcmp(parent->dv_cfdata->cf_name, "opb") == 0) {
		/* Set the mac-addr of the on-chip Ethernet. */
		/* XXX 405GP/405GPr only has one; what about CPUs with two? */
		/* XXX board_data is for IBM ROM Monitor 1.x with 405GP */
		if (prop_set(dev_propdb, dev, "mac-addr",
			     &board_data.mac_address_local,
			     sizeof(board_data.mac_address_local),
			     PROP_CONST, 0) != 0)
			printf("WARNING: unable to set mac-addr "
			    "property for %s\n", dev->dv_xname);
		return;
	}
}
