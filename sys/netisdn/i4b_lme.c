/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_lme.c - layer management entity
 *	-------------------------------------
 *
 *	$Id: i4b_lme.c,v 1.2.2.1 2002/01/10 20:03:39 thorpej Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jan  5 11:33:47 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i4b_lme.c,v 1.2.2.1 2002/01/10 20:03:39 thorpej Exp $");

#ifdef __FreeBSD__
#include "i4bq921.h"
#else
#define	NI4BQ921	1
#endif
#if NI4BQ921 > 0

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#endif

#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_isdnq931.h>
#include <netisdn/i4b_mbuf.h>

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l2fsm.h>

/*---------------------------------------------------------------------------*
 *	mdl assign indication handler
 *---------------------------------------------------------------------------*/
void
i4b_mdl_assign_ind(l2_softc_t *l2sc)
{
	NDBGL2(L2_PRIM, "bri %d", l2sc->bri);
	
	i4b_l1_activate(l2sc);
	
	if(l2sc->tei_valid == TEI_VALID)
	{
		l2sc->T202func = (void(*)(void*))i4b_tei_verify;
		l2sc->N202 = N202DEF;
		i4b_tei_verify(l2sc);
	}
	else
	{
		l2sc->T202func = (void(*)(void*))i4b_tei_assign;
		l2sc->N202 = N202DEF;
		i4b_tei_assign(l2sc);
	}		
}

/*---------------------------------------------------------------------------*
 *	i4b_mdl_error_ind handler (Q.921 01/94 pp 156)
 *---------------------------------------------------------------------------*/
void
i4b_mdl_error_ind(l2_softc_t *l2sc, char *where, int errorcode)
{
#if DO_I4B_DEBUG
	static char *error_text[] = {
		"MDL_ERR_A: rx'd unsolicited response - supervisory (F=1)",
		"MDL_ERR_B: rx'd unsolicited response - DM (F=1)",
		"MDL_ERR_C: rx'd unsolicited response - UA (F=1)",
		"MDL_ERR_D: rx'd unsolicited response - UA (F=0)",
		"MDL_ERR_E: rx'd unsolicited response - DM (F=0)",
		"MDL_ERR_F: peer initiated re-establishment - SABME",
		"MDL_ERR_G: unsuccessful transmission N200times - SABME",
		"MDL_ERR_H: unsuccessful transmission N200times - DIS",
		"MDL_ERR_I: unsuccessful transmission N200times - Status ENQ",
		"MDL_ERR_J: other error - N(R) error",
		"MDL_ERR_K: other error - rx'd FRMR response",
		"MDL_ERR_L: other error - rx'd undefined frame",
		"MDL_ERR_M: other error - receipt of I field not permitted",
		"MDL_ERR_N: other error - rx'd frame with wrong size",
		"MDL_ERR_O: other error - N201 error",
		"MDL_ERR_MAX: i4b_mdl_error_ind called with wrong parameter!!!"
	};
#endif

	if(errorcode > MDL_ERR_MAX)
		errorcode = MDL_ERR_MAX;
		
	NDBGL2(L2_ERROR, "bri = %d, location = %s", l2sc->bri, where);
	NDBGL2(L2_ERROR, "error = %s", error_text[errorcode]);

	switch(errorcode)
	{	
		case MDL_ERR_A:
		case MDL_ERR_B:
			break;

		case MDL_ERR_C:
		case MDL_ERR_D:
			i4b_tei_verify(l2sc);
			break;

		case MDL_ERR_E:
		case MDL_ERR_F:
			break;

		case MDL_ERR_G:
		case MDL_ERR_H:
			i4b_tei_verify(l2sc);
			break;

		case MDL_ERR_I:
		case MDL_ERR_J:
		case MDL_ERR_K:
		case MDL_ERR_L:
		case MDL_ERR_M:
		case MDL_ERR_N:
		case MDL_ERR_O:
			break;

		default:
			break;
	}
}
 
#endif /* NI4BQ921 > 0 */
