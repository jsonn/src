/*	$NetBSD: sa11xx_pcic.c,v 1.1.2.3 2001/03/27 15:30:52 bouyer Exp $	*/

/*
 * Copyright (c) 2001 IWAMOTO Toshihiro.  All rights reserved.
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
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

/*
 * Common code for SA11x0 based PCMCIA modules
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <uvm/uvm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <hpcarm/sa11x0/sa11x0_reg.h>
#include <hpcarm/sa11x0/sa11x0_var.h>
#include <hpcarm/sa11x0/sa11xx_pcicreg.h>
#include <hpcarm/sa11x0/sa11xx_pcicvar.h>

static	void	sapcic_event_thread(void *);

static	int	sapcic_mem_alloc(pcmcia_chipset_handle_t, bus_size_t,
					struct pcmcia_mem_handle *);
static	void	sapcic_mem_free(pcmcia_chipset_handle_t,
				       struct pcmcia_mem_handle *);
static	int	sapcic_mem_map(pcmcia_chipset_handle_t, int, bus_addr_t,
				      bus_size_t, struct pcmcia_mem_handle *,
				      bus_addr_t *, int *);
static	void	sapcic_mem_unmap(pcmcia_chipset_handle_t, int);
static	int	sapcic_io_alloc(pcmcia_chipset_handle_t, bus_addr_t,
				       bus_size_t, bus_size_t,
				       struct pcmcia_io_handle *);
static	void	sapcic_io_free(pcmcia_chipset_handle_t,
				      struct pcmcia_io_handle *);
static	int	sapcic_io_map(pcmcia_chipset_handle_t, int,
				     bus_addr_t, bus_size_t,
				     struct pcmcia_io_handle *, int *);
static	void	sapcic_io_unmap(pcmcia_chipset_handle_t, int);
static	void	*sapcic_intr_establish(pcmcia_chipset_handle_t,
					      struct pcmcia_function *, int,
					      int (*)(void *), void *);
static	void	sapcic_intr_disestablish(pcmcia_chipset_handle_t,
						void *);
static	void	sapcic_socket_enable(pcmcia_chipset_handle_t);
static	void	sapcic_socket_disable(pcmcia_chipset_handle_t);

#ifdef DEBUG
#define DPRINTF(arg)	printf arg
#else
#define DPRINTF(arg)
#endif

struct pcmcia_chip_functions sa11x0_pcmcia_functions = {
	sapcic_mem_alloc,
	sapcic_mem_free,
	sapcic_mem_map,
	sapcic_mem_unmap,

	sapcic_io_alloc,
	sapcic_io_free,
	sapcic_io_map,
	sapcic_io_unmap,

	sapcic_intr_establish,
	sapcic_intr_disestablish,

	sapcic_socket_enable,
	sapcic_socket_disable,
};


void
sapcic_kthread_create(arg)
	void *arg;
{
	struct sapcic_socket *so = arg;

	/* XXX attach card if already present */

	so->laststatus =(so->pcictag->read)(so, SAPCIC_STATUS_CARD);
	if (so->laststatus == SAPCIC_CARD_VALID) {
		printf("%s: card present\n",
		       so->sc->sc_dev.dv_xname);

		pcmcia_card_attach(so->pcmcia);
	}

	if (kthread_create1(sapcic_event_thread, so, &so->event_thread,
			    "%s,%d", so->sc->sc_dev.dv_xname, so->socket)) {
		printf("%s: unable to create event thread for socket %d\n",
		       so->sc->sc_dev.dv_xname, so->socket);
		panic("sapcic_kthread_create");
	}
}

static void
sapcic_event_thread(arg)
	void *arg;
{
	struct sapcic_socket *so = arg;
	int newstatus, s;

	while (so->shutdown == 0) {
		/*
		 * Serialize event processing on the PCIC.  We may
		 * sleep while we hold this lock.
		 */
		(void) lockmgr(&so->sc->sc_lock, LK_EXCLUSIVE, NULL);

		/* sleep .25s to be enqueued chatterling interrupts */
		(void) tsleep((caddr_t)sapcic_event_thread, PWAIT,
			      "pcicss", hz/4);

		s = splhigh();
		so->event = 0;

		/* we don't rely on interrupt type */
		newstatus = (so->pcictag->read)(so, SAPCIC_STATUS_CARD);
		splx(s);

		if (so->laststatus == newstatus) {
			/*
			 * No events to process; release the PCIC lock.
			 */
			(void) lockmgr(&so->sc->sc_lock, LK_RELEASE, NULL);
			(void) tsleep(&so->event, PWAIT, "pcicev", hz);
			continue;
		}

		so->laststatus = newstatus;
		switch (newstatus) {
		case SAPCIC_CARD_VALID:
			printf("%s: insertion event\n",
			       so->sc->sc_dev.dv_xname);

			pcmcia_card_attach(so->pcmcia);
			break;

		case SAPCIC_CARD_INVALID:
			printf("%s: removal event\n",
			       so->sc->sc_dev.dv_xname);

			pcmcia_card_detach(so->pcmcia, DETACH_FORCE);
			break;

		default:
			panic("sapcic_event_thread: unknown status %d",
			    newstatus);
		}

		(void) lockmgr(&so->sc->sc_lock, LK_RELEASE, NULL);
	}

	so->event_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(so->sc);

	kthread_exit(0);
}

int
sapcic_intr(arg)
	void *arg;
{
	struct sapcic_socket *so = arg;

	so->event++;
	(so->pcictag->clear_intr)(so->socket);
	wakeup(&so->event);
	return 1;
}

static int
sapcic_mem_alloc(pch, size, pmh)
	pcmcia_chipset_handle_t pch;
	bus_size_t size;
	struct pcmcia_mem_handle *pmh;
{
	struct sapcic_socket *so = pch;

	/* All we need is bus space tag */
	memset(pmh, 0, sizeof(*pmh));
	pmh->memt = so->sc->sc_iot;
	return (0);
}


static void
sapcic_mem_free(pch, pmh)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_mem_handle *pmh;
{
}

static int
sapcic_mem_map(pch, kind, card_addr, size, pmh, offsetp, windowp)
	pcmcia_chipset_handle_t pch;
	int kind;
	bus_addr_t card_addr;
	bus_size_t size;
	struct pcmcia_mem_handle *pmh;
	bus_addr_t *offsetp;
	int *windowp;
{
	struct sapcic_socket *so = pch;
	int error;
	bus_addr_t pa;

	pa = trunc_page(card_addr);
	*offsetp = card_addr - pa;
	size = round_page(card_addr + size) - pa;

	pa += SAPCIC_BASE_OFFSET;
	pa += SAPCIC_SOCKET_OFFSET * so->socket;

	switch (kind & ~PCMCIA_WIDTH_MEM_MASK) {
	case PCMCIA_MEM_ATTR:
		pa += SAPCIC_ATTR_OFFSET;
		break;
	case PCMCIA_MEM_COMMON:
		pa += SAPCIC_COMMON_OFFSET;
		break;
	default:
		panic("sapcic_mem_map: bogus kind\n");
	}

	error = bus_space_map(so->sc->sc_iot, pa, size, 0, &pmh->memh);
	if (! error)
		*windowp = (int)pmh->memh;
	return (error);
}

static void
sapcic_mem_unmap(pch, window)
	pcmcia_chipset_handle_t pch;
	int window;
{
	struct sapcic_socket *so = pch;

	bus_space_unmap(so->sc->sc_iot, (bus_addr_t)window, 4096); /* XXX */
}

static int
sapcic_io_alloc(pch, start, size, align, pih)
	pcmcia_chipset_handle_t pch;
	bus_addr_t start;
	bus_size_t size;
	bus_size_t align;
	struct pcmcia_io_handle *pih;
{
	struct sapcic_socket *so = pch;

	memset(pih, 0, sizeof(*pih));
	pih->iot = so->sc->sc_iot;
	pih->addr = start;
	pih->size = size;

	/* XXX Are we ignoring alignment constraints? */
	return (0);
}

static void
sapcic_io_free(pch, pih)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_io_handle *pih;
{
}

static int
sapcic_io_map(pch, width, offset, size, pih, windowp)
	pcmcia_chipset_handle_t pch;
	int width;
	bus_addr_t offset;
	bus_size_t size;
	struct pcmcia_io_handle *pih;
	int *windowp;
{
	struct sapcic_socket *so = pch;
	bus_addr_t pa;
	int error;

	pa = pih->addr + offset;
	pa += SAPCIC_BASE_OFFSET;
	pa += SAPCIC_SOCKET_OFFSET * so->socket;

	DPRINTF(("sapcic_io_map: %x %x\n", (unsigned int)pa, (unsigned int)size));
	error = bus_space_map(so->sc->sc_iot, pa, size, 0, &pih->ioh);
	if (! error)
		*windowp = (int)pih->ioh;
	return (error);
}

static void sapcic_io_unmap(pch, window)
	pcmcia_chipset_handle_t pch;
	int window;
{
	struct sapcic_socket *so = pch;

	bus_space_unmap(so->sc->sc_iot, (bus_addr_t)window, 4096); /* XXX */
}

static void *
sapcic_intr_establish(pch, pf, ipl, fct, arg)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_function *pf;
	int ipl;
	int (*fct)(void *);
	void *arg;
{
	struct sapcic_socket *so = pch;

	/* XXX need to check if something should be done here */

	return ((so->pcictag->intr_establish)(so, ipl, fct, arg));
}

static void
sapcic_intr_disestablish(pch, ih)
	pcmcia_chipset_handle_t pch;
	void *ih;
{
	struct sapcic_socket *so = pch;

	((so->pcictag->intr_disestablish)(so, ih));
}

static void
sapcic_socket_enable(pch)
	pcmcia_chipset_handle_t pch;
{
	/* XXX usage of delay() should be decreased */

	struct sapcic_socket *so = pch;
#if defined(DIAGNOSTIC) && defined(notyet)
	if (so->flags & PCIC_FLAG_ENABLED)
		printf("sapcic_socket_enable: enabling twice\n");
#endif

	/* disable interrupts */

	/* power down the socket to reset it, clear the card reset pin */
	(so->pcictag->set_power)(so, SAPCIC_POWER_OFF);

	/* 
	 * wait 300ms until power fails (Tpf).  Then, wait 100ms since
	 * we are changing Vcc (Toff).
	 */
	delay((300 + 100) * 1000);

	/* power up the socket */
	(so->pcictag->set_power)(so, so->socket ? SAPCIC_POWER_3V : SAPCIC_POWER_5V); /* XXX */
	(so->pcictag->write)(so, SAPCIC_CONTROL_POWERSELECT,
			     so->socket ? SAPCIC_POWER_3V : SAPCIC_POWER_5V); /* XXX */

	/* enable PCMCIA control lines */
	(so->pcictag->write)(so, SAPCIC_CONTROL_LINEENABLE, 1);

	/*
	 * wait 100ms until power raise (Tpr) and 20ms to become
	 * stable (Tsu(Vcc)).
	 *
	 * some machines require some more time to be settled
	 * (300ms is added here).
	 */
	delay((100 + 20 + 300) * 1000);

	/* honor nWAIT signal */
	(so->pcictag->write)(so, SAPCIC_CONTROL_WAITENABLE, 1);
	/* now make sure we have reset# active */
	(so->pcictag->write)(so, SAPCIC_CONTROL_RESET, 1);

	/*
	 * hold RESET at least 10us, this is a min allow for slop in
	 * delay routine.
	 */
	delay(20);

	/* clear the reset flag */
	(so->pcictag->write)(so, SAPCIC_CONTROL_RESET, 0);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */
	delay(20 * 1000);

	/* wait for the chip to finish initializing */
/*	pcic_wait_ready(h);*/
	delay(500 * 1000); 	/* XXX wait long enough */

	/* finally enable the interrupt */

}

static void
sapcic_socket_disable(pch)
	pcmcia_chipset_handle_t pch;
{
	/* XXX mask card interrupts */
	/* XXX power down the card */
	/* XXX float controller lines */
}
