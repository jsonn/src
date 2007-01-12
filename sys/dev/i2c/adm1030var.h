/*	$NetBSD: adm1030var.h,v 1.2.22.1 2007/01/12 00:57:35 ad Exp $	*/

/*-
 * Copyright (C) 2005 Michael Lorenz.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * a driver fot the ADM1030 environmental controller found in some iBook G3 
 * and probably other Apple machines 
 */

#ifndef ADM1030VAR_H
#define ADM1030VAR_H

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adm1030var.h,v 1.2.22.1 2007/01/12 00:57:35 ad Exp $");

#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>
#include "sysmon_envsys.h"

struct adm1030c_softc {
	struct device sc_dev;
	struct device *parent;
	struct sysmon_envsys *sc_sysmon_cookie;
	struct i2c_controller *sc_i2c;
	int sc_node, address;
	uint8_t regs[3];
};

void adm1030c_setup(struct adm1030c_softc *);

#endif
