/*	$NetBSD: tc_3min.c,v 1.1 1998/04/19 07:59:13 jonathan Exp $	*/

/*
 * Copyright (c) 1998 Jonathan Stone.  All rights reserved.
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
 *	This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: tc_3min.c,v 1.1 1998/04/19 07:59:13 jonathan Exp $ ");

#include <sys/types.h>
#include <sys/device.h>
#include <dev/cons.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>

#include <pmax/pmax/kmin.h>

int	tcbus_match_3min(struct device *, void *, void *);
void	tcbus_attach_3min(struct device *, struct device *, void *);

/* 3MIN slot addreseses */
static struct tc_slotdesc tc_kmin_slots [] = {
       	{ TC_KV(KMIN_PHYS_TC_0_START), TC_C(0) },   /* 0 - tc option slot 0 */
	{ TC_KV(KMIN_PHYS_TC_1_START), TC_C(1) },   /* 1 - tc option slot 1 */
	{ TC_KV(KMIN_PHYS_TC_2_START), TC_C(2) },   /* 2 - tc option slot 2 */
	{ TC_KV(KMIN_PHYS_TC_3_START), TC_C(3) }    /* 3 - IO asic on b'board */
};

/*
 * The only builtin Turbochannel device on the kn03 (and kmin)
 * is the IOCTL asic, which is mapped into TC slot 3.
 */
const struct tc_builtin tc_kn02ba_builtins[] = {
	{ "IOCTL   ",	3, 0x0, TC_C(3), /*TC_C(3)*/ }
};

int tc_kmin_nslots =
    sizeof(tc_kmin_slots) / sizeof(tc_kmin_slots[0]);

/* 3MIN turbochannel autoconfiguration table */
struct tcbus_attach_args kmin_tc_desc =
{
	"tc",				/* XXX common substructure */
	0,				/* XXX bus_space_tag */
	TC_SPEED_12_5_MHZ,
	KMIN_TC_NSLOTS, tc_kmin_slots,
	1, tc_kn02ba_builtins, /*XXX*/
	0 /*tc_ds_ioasic_intr_establish*/ ,
	0 /**tc_ds_ioasic_intr_disestablish*/,
};
