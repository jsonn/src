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
 *	i4b_isic.c - global isic stuff
 *	==============================
 *
 *	$Id: isic.c,v 1.1.2.3 2002/04/01 07:45:27 nathanw Exp $ 
 *
 *      last edit-date: [Fri Jan  5 11:36:10 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic.c,v 1.1.2.3 2002/04/01 07:45:27 nathanw Exp $");

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_global.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/ipac.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

isdn_link_t *isic_ret_linktab(void*, int channel);
void isic_set_link(void*, int channel, const struct isdn_l4_driver_functions *l4_driver, void *l4_driver_softc);
void n_connect_request(u_int cdid);
void n_connect_response(u_int cdid, int response, int cause);
void n_disconnect_request(u_int cdid, int cause);
void n_alert_request(u_int cdid);
void n_mgmt_command(int bri, int cmd, void *parm);

const struct isdn_l3_driver_functions
isic_l3_driver = {
	isic_ret_linktab,
	isic_set_link,
	n_connect_request,
	n_connect_response,
	n_disconnect_request,
	n_alert_request,
	NULL,
	NULL,
	n_mgmt_command
};

/*---------------------------------------------------------------------------*
 *	isic - device driver interrupt routine
 *---------------------------------------------------------------------------*/
int
isicintr(void *arg)
{
	struct isic_softc *sc = arg;

	if (sc->sc_dying)
		return 0;

	if(sc->sc_ipac == 0)	/* HSCX/ISAC interupt routine */
	{
		u_char was_hscx_irq = 0;
		u_char was_isac_irq = 0;

		register u_char hscx_irq_stat;
		register u_char isac_irq_stat;

		for(;;)
		{
			/* get hscx irq status from hscx b ista */
			hscx_irq_stat =
	 	    	    HSCX_READ(HSCX_CH_B, H_ISTA) & ~HSCX_B_IMASK;
	
			/* get isac irq status */
			isac_irq_stat = ISAC_READ(I_ISTA);
	
			/* do as long as there are pending irqs in the chips */
			if(!hscx_irq_stat && !isac_irq_stat)
				break;
	
			if(hscx_irq_stat & (HSCX_ISTA_RME | HSCX_ISTA_RPF |
					    HSCX_ISTA_RSC | HSCX_ISTA_XPR |
					    HSCX_ISTA_TIN | HSCX_ISTA_EXB))
			{
				isic_hscx_irq(sc, hscx_irq_stat,
						HSCX_CH_B,
						hscx_irq_stat & HSCX_ISTA_EXB);
				was_hscx_irq = 1;			
			}
			
			if(hscx_irq_stat & (HSCX_ISTA_ICA | HSCX_ISTA_EXA))
			{
				isic_hscx_irq(sc,
				    HSCX_READ(HSCX_CH_A, H_ISTA) & ~HSCX_A_IMASK,
				    HSCX_CH_A,
				    hscx_irq_stat & HSCX_ISTA_EXA);
				was_hscx_irq = 1;
			}
	
			if(isac_irq_stat)
			{
				if (isic_isac_irq(sc, isac_irq_stat)) /* isac handler */
					break;	/* bad IRQ */
				was_isac_irq = 1;
			}
		}
	
		HSCX_WRITE(0, H_MASK, 0xff);
		ISAC_WRITE(I_MASK, 0xff);
		HSCX_WRITE(1, H_MASK, 0xff);
	
		if (sc->clearirq)
		{
			DELAY(80);
			sc->clearirq(sc);
		} else
			DELAY(100);
	
		HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
		ISAC_WRITE(I_MASK, ISAC_IMASK);
		HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);

		return(was_hscx_irq || was_isac_irq);
	}
	else	/* IPAC interrupt routine */
	{
		register u_char ipac_irq_stat;
		register u_char was_ipac_irq = 0;

		for(;;)
		{
			/* get global irq status */
			
			ipac_irq_stat = (IPAC_READ(IPAC_ISTA)) & 0x3f;
			
			/* check hscx a */
			
			if(ipac_irq_stat & (IPAC_ISTA_ICA | IPAC_ISTA_EXA))
			{
				/* HSCX A interrupt */
				isic_hscx_irq(sc, HSCX_READ(HSCX_CH_A, H_ISTA),
						HSCX_CH_A,
						ipac_irq_stat & IPAC_ISTA_EXA);
				was_ipac_irq = 1;			
			}
			if(ipac_irq_stat & (IPAC_ISTA_ICB | IPAC_ISTA_EXB))
			{
				/* HSCX B interrupt */
				isic_hscx_irq(sc, HSCX_READ(HSCX_CH_B, H_ISTA),
						HSCX_CH_B,
						ipac_irq_stat & IPAC_ISTA_EXB);
				was_ipac_irq = 1;			
			}
			if(ipac_irq_stat & IPAC_ISTA_ICD)
			{
				/* ISAC interrupt */
				isic_isac_irq(sc, ISAC_READ(I_ISTA));
				was_ipac_irq = 1;
			}
			if(ipac_irq_stat & IPAC_ISTA_EXD)
			{
				/* force ISAC interrupt handling */
				if (isic_isac_irq(sc, ISAC_ISTA_EXI))
					break;	/* bad IRQ */
				was_ipac_irq = 1;
			}
	
			/* do as long as there are pending irqs in the chip */
			if(!ipac_irq_stat)
				break;
		}

		IPAC_WRITE(IPAC_MASK, 0xff);
		DELAY(50);
		IPAC_WRITE(IPAC_MASK, 0xc0);

		return(was_ipac_irq);
	}		
}

int
isic_attach_bri(struct isic_softc *sc, const char *cardname, const struct isdn_layer1_bri_driver *dchan_driver)
{
	struct isdn_l3_driver * drv;

	drv = isdn_attach_bri(sc->sc_dev.dv_xname, cardname, &sc->sc_l2, &isic_l3_driver);
	sc->sc_l3token = drv;
	sc->sc_l2.driver = dchan_driver;
	sc->sc_l2.l1_token = sc;
	sc->sc_l2.bri = drv->bri;
	isdn_layer2_status_ind(&sc->sc_l2, STI_ATTACH, 1);
	return 1;
}

int
isic_detach_bri(struct isic_softc *sc)
{
	isdn_layer2_status_ind(&sc->sc_l2, STI_ATTACH, 0);
	isdn_detach_bri(sc->sc_l3token);
	sc->sc_l3token = NULL;
	return 1;
}
