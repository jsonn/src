/*
 *   Copyright (c) 1998 Martin Husemann. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	isic_pcmcia.h - common definitions for pcmcia isic cards
 *	--------------------------------------------------------
 *
 *	$Id: isic_pcmcia.h,v 1.1.4.2 2001/03/12 13:31:20 bouyer Exp $ 
 *
 *      last edit-date: [Sun Feb 14 10:29:33 1999]
 *
 *	-mh	original implementation
 *
 *---------------------------------------------------------------------------*/

struct pcmcia_l1_softc {
	struct l1_softc sc_isic;	/* parent class */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handler */
};

typedef int (*isic_pcmcia_attach_func)(struct pcmcia_l1_softc *sc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa);

extern int isic_attach_fritzpcmcia(struct pcmcia_l1_softc *sc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa);
extern int isic_attach_elsaisdnmc(struct pcmcia_l1_softc *sc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa);
extern int isic_attach_elsamcall(struct pcmcia_l1_softc *sc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa);
extern int isic_attach_sbspeedstar2(struct pcmcia_l1_softc *sc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa);

