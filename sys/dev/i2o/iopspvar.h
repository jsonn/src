/*	$NetBSD: iopspvar.h,v 1.2.2.3 2001/01/22 18:00:43 bouyer Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef _I2O_IOPSPVAR_H_
#define	_I2O_IOPSPVAR_H_

#define	IOPSP_MAX_LUN		8
#define	IOPSP_MAX_SCSI_TARGET	15
#define	IOPSP_MAX_FCAL_TARGET	127

#define	IOPSP_TIDMAP(map, t, l)	(map[(t) * (IOPSP_MAX_LUN + 1) + (l)])
#define	IOPSP_TID_ABSENT	0x0000	/* Device is absent */
#define	IOPSP_TID_INUSE		0xffff	/* Device in use by another module */

#ifdef I2OVERBOSE
struct iopsp_target {
	u_int8_t	it_width;
	u_int8_t	it_syncrate;
	u_int8_t	it_offset;
	u_int8_t	it_flags;
};
#define	IT_PRESENT		0x01	/* Target is present */
#endif

struct iopsp_softc {
	struct	device sc_dv;			/* Generic device data */
	struct	scsipi_adapter sc_adapter;	/* scsipi adapter */
	struct	scsipi_channel sc_channel;	/* Prototype link */
	struct	iop_initiator sc_ii;		/* I2O initiator state */
	u_int	sc_tid;				/* Bus port TID */
	u_short	*sc_tidmap;			/* Target/LUN -> TID map */
	u_int	sc_chgindicator;		/* Last LCT change # */
#ifdef I2OVERBOSE
	struct	iopsp_target *sc_targetmap;	/* Target information */
#endif
};

#endif	/* !_I2O_IOPSPVAR_H_ */
