/*	$NetBSD: cardslot.c,v 1.2.2.4 2001/03/27 15:31:50 bouyer Exp $	*/

/*
 * Copyright (c) 1999 and 2000
 *       HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_cardslot.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/kthread.h>

#include <machine/bus.h>

#include <dev/cardbus/cardslotvar.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>
#include <dev/ic/i82365var.h>


#if defined CARDSLOT_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#define STATIC static
#define DPRINTF(a)
#endif



STATIC void cardslotattach __P((struct device *, struct device *, void *));

STATIC int cardslotmatch __P((struct device *, struct cfdata *, void *));
static void create_slot_manager __P((void *));
static void cardslot_event_thread __P((void *arg));

STATIC int cardslot_cb_print __P((void *aux, const char *pcic));
static int cardslot_16_print __P((void *, const char *));
static int cardslot_16_submatch __P((struct device *, struct cfdata *,void *));

struct cfattach cardslot_ca = {
	sizeof(struct cardslot_softc), cardslotmatch, cardslotattach
};


STATIC int
cardslotmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct cardslot_attach_args *caa = aux;

	if (caa->caa_cb_attach == NULL && caa->caa_16_attach == NULL) {
		/* Neither CardBus nor 16-bit PCMCIA are defined. */
		return 0;
	}

	return 1;
}



STATIC void
cardslotattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct cardslot_softc *sc = (struct cardslot_softc *)self;
	struct cardslot_attach_args *caa = aux;

	struct cbslot_attach_args *cba = caa->caa_cb_attach;
	struct pcmciabus_attach_args *pa = caa->caa_16_attach;

	struct cardbus_softc *csc;
	struct pcmcia_softc *psc;

	sc->sc_slot = sc->sc_dev.dv_unit;
	sc->sc_cb_softc = NULL;
	sc->sc_16_softc = NULL;
	SIMPLEQ_INIT(&sc->sc_events);
	sc->sc_th_enable = 0;

	printf(" slot %d flags %x\n", sc->sc_slot, sc->sc_dev.dv_cfdata->cf_flags);

	DPRINTF(("%s attaching CardBus bus...\n", sc->sc_dev.dv_xname));
	if (cba != NULL) {
		if (NULL != (csc = (void *)config_found(self, cba, cardslot_cb_print))) {
			/* cardbus found */
			DPRINTF(("cardslotattach: found cardbus on %s\n", sc->sc_dev.dv_xname));
			sc->sc_cb_softc = csc;
		}
	}

	if (pa != NULL) {
		if (NULL != (psc = (void *)config_found_sm(self, pa,
		    cardslot_16_print, cardslot_16_submatch))) {
			/* pcmcia 16-bit bus found */
			DPRINTF(("cardslotattach: found 16-bit pcmcia bus\n"));
			sc->sc_16_softc = psc;
			/*
			 * XXX:
			 * dirty.  This code should be removed to achieve MI.
			 */
			caa->caa_ph->pcmcia = (struct device *)psc;
		}
	}

	if (csc != NULL || psc != NULL) {
		config_pending_incr();
		kthread_create(create_slot_manager, (void *)sc);
	}

	if (csc && (csc->sc_cf->cardbus_ctrl)(csc->sc_cc, CARDBUS_CD)) {
		DPRINTF(("cardslotattach: CardBus card found\n"));
		/* attach deferred */
		cardslot_event_throw(sc, CARDSLOT_EVENT_INSERTION_CB);
	}

	if (psc && (psc->pct->card_detect)(psc->pch)) {
		DPRINTF(("cardbusattach: 16-bit card found\n"));
		/* attach deferred */
		cardslot_event_throw(sc, CARDSLOT_EVENT_INSERTION_16);
	}
}



STATIC int
cardslot_cb_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct cbslot_attach_args *cba = aux;

	if (pnp) {
		printf("cardbus at %s subordinate bus %d", pnp, cba->cba_bus);
	}

	return UNCONF;
}


static int
cardslot_16_submatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{

	if (cf->cf_loc[PCMCIABUSCF_CONTROLLER] != PCMCIABUSCF_CONTROLLER_DEFAULT
	    && cf->cf_loc[PCMCIABUSCF_CONTROLLER] != 0) {
		return 0;
	}

	if ((cf->cf_loc[PCMCIABUSCF_CONTROLLER] == PCMCIABUSCF_CONTROLLER_DEFAULT)) {
		return ((*cf->cf_attach->ca_match)(parent, cf, aux));
	}

	return 0;
}



static int
cardslot_16_print(arg, pnp)
	void *arg;
	const char *pnp;
{

	if (pnp) {
		printf("pcmciabus at %s", pnp);
	}

	return UNCONF;
}




static void
create_slot_manager(arg)
	void *arg;
{
	struct cardslot_softc *sc = (struct cardslot_softc *)arg;

	sc->sc_th_enable = 1;

	if (kthread_create1(cardslot_event_thread, sc, &sc->sc_event_thread,
	    "%s", sc->sc_dev.dv_xname)) {
		printf("%s: unable to create event thread for slot %d\n",
		    sc->sc_dev.dv_xname, sc->sc_slot);
		panic("create_slot_manager");
	}
}




/*
 * void cardslot_event_throw(struct cardslot_softc *sc, int ev)
 *
 *   This function throws an event to the event handler.  If the state
 *   of a slot is changed, it should be noticed using this function.
 */
void
cardslot_event_throw(sc, ev)
	struct cardslot_softc *sc;
	int ev;
{
	struct cardslot_event *ce;

	DPRINTF(("cardslot_event_throw: an event %s comes\n",
	    ev == CARDSLOT_EVENT_INSERTION_CB ? "CardBus Card inserted" :
	    ev == CARDSLOT_EVENT_INSERTION_16 ? "16-bit Card inserted" :
	    ev == CARDSLOT_EVENT_REMOVAL_CB ? "CardBus Card removed" :
	    ev == CARDSLOT_EVENT_REMOVAL_16 ? "16-bit Card removed" : "???"));

	if (NULL == (ce = (struct cardslot_event *)malloc(sizeof (struct cardslot_event), M_TEMP, M_NOWAIT))) {
		panic("cardslot_enevt");
	}

	ce->ce_type = ev;

	{
		int s = spltty();
		SIMPLEQ_INSERT_TAIL(&sc->sc_events, ce, ce_q);
		splx(s);
	}

	wakeup(&sc->sc_events);

	return;
}


/*
 * static void cardslot_event_thread(void *arg)
 *
 *   This function is the main routine handing cardslot events such as
 *   insertions and removals.
 *
 */
static void
cardslot_event_thread(arg)
	void *arg;
{
	struct cardslot_softc *sc = arg;
	struct cardslot_event *ce;
	int s, first = 1;
	static int antonym_ev[4] = {
		CARDSLOT_EVENT_REMOVAL_16, CARDSLOT_EVENT_INSERTION_16,
		CARDSLOT_EVENT_REMOVAL_CB, CARDSLOT_EVENT_INSERTION_CB
	};

	while (sc->sc_th_enable) {
		s = spltty();
		if ((ce = SIMPLEQ_FIRST(&sc->sc_events)) == NULL) {
			splx(s);
			if (first) {
				first = 0;
				config_pending_decr();
			}
			(void) tsleep(&sc->sc_events, PWAIT, "cardslotev", 0);
			continue;
		}
		SIMPLEQ_REMOVE_HEAD(&sc->sc_events, ce, ce_q);
		splx(s);

		if (IS_CARDSLOT_INSERT_REMOVE_EV(ce->ce_type)) {
			/* Chattering supression */
			s = spltty();
			while (1) {
				struct cardslot_event *ce1, *ce2;

				if ((ce1 = SIMPLEQ_FIRST(&sc->sc_events)) == NULL) {
					break;
				}
				if (ce1->ce_type != antonym_ev[ce->ce_type]) {
					break;
				}
				if ((ce2 = SIMPLEQ_NEXT(ce1, ce_q)) == NULL) {
					break;
				}
				if (ce2->ce_type == ce->ce_type) {
					SIMPLEQ_REMOVE_HEAD(&sc->sc_events, ce1, ce_q);
					free(ce1, M_TEMP);
					SIMPLEQ_REMOVE_HEAD(&sc->sc_events, ce2, ce_q);
					free(ce2, M_TEMP);
				}
			}
			splx(s);
		}

		switch (ce->ce_type) {
		case CARDSLOT_EVENT_INSERTION_CB:
			if ((CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_CB)
			    || (CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_16)) {
				if (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING) {
					/*
					 * A card has already been
					 * inserted and works.
					 */
					break;
				}
			}

			if (sc->sc_cb_softc) {
				CARDSLOT_SET_CARDTYPE(sc->sc_status,
				    CARDSLOT_STATUS_CARD_CB);
				if (cardbus_attach_card(sc->sc_cb_softc) > 0) {
					/* at least one function works */
					CARDSLOT_SET_WORK(sc->sc_status, CARDSLOT_STATUS_WORKING);
				} else {
					/*
					 * no functions work or this
					 * card is not known
					 */
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_NOTWORK);
				}
			} else {
				panic("no cardbus on %s", sc->sc_dev.dv_xname);
			}

			break;

		case CARDSLOT_EVENT_INSERTION_16:
			if ((CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_CB)
			    || (CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_16)) {
				if (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING) {
					/*
					 * A card has already been
					 * inserted and work.
					 */
					break;
				}
			}
			if (sc->sc_16_softc) {
				CARDSLOT_SET_CARDTYPE(sc->sc_status, CARDSLOT_STATUS_CARD_16);
				if (pcmcia_card_attach((struct device *)sc->sc_16_softc)) {
					/* Do not attach */
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_NOTWORK);
				} else {
					/* working */
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_WORKING);
				}
			} else {
				panic("no 16-bit pcmcia on %s", sc->sc_dev.dv_xname);
			}

			break;

		case CARDSLOT_EVENT_REMOVAL_CB:
			if (CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_CB) {
				/* CardBus card has not been inserted. */
				if (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING) {
					cardbus_detach_card(sc->sc_cb_softc);
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_NOTWORK);
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_CARD_NONE);
				}
				CARDSLOT_SET_CARDTYPE(sc->sc_status,
				    CARDSLOT_STATUS_CARD_NONE);
			} else if (CARDSLOT_CARDTYPE(sc->sc_status) != CARDSLOT_STATUS_CARD_16) {
				/* Unknown card... */
				CARDSLOT_SET_CARDTYPE(sc->sc_status,
				    CARDSLOT_STATUS_CARD_NONE);
			}
			CARDSLOT_SET_WORK(sc->sc_status,
			    CARDSLOT_STATUS_NOTWORK);
			break;

		case CARDSLOT_EVENT_REMOVAL_16:
			DPRINTF(("%s: removal event\n", sc->sc_dev.dv_xname));
			if (CARDSLOT_CARDTYPE(sc->sc_status) != CARDSLOT_STATUS_CARD_16) {
				/* 16-bit card has not been inserted. */
				break;
			}
			if ((sc->sc_16_softc != NULL)
			    && (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING)) {
				struct pcmcia_softc *psc = sc->sc_16_softc;

				pcmcia_card_deactivate((struct device *)psc);
				pcmcia_chip_socket_disable(psc->pct, psc->pch);
				pcmcia_card_detach((struct device *)psc, DETACH_FORCE);
			}
			CARDSLOT_SET_CARDTYPE(sc->sc_status, CARDSLOT_STATUS_CARD_NONE);
			CARDSLOT_SET_WORK(sc->sc_status, CARDSLOT_STATUS_NOTWORK);
			break;

		default:
			panic("cardslot_event_thread: unknown event %d", ce->ce_type);
		}
		free(ce, M_TEMP);
	}

	sc->sc_event_thread = NULL;

	/* In case the parent device is waiting for us to exit. */
	wakeup(sc);

	kthread_exit(0);
}
