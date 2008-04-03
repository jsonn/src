/*	$NetBSD: zusb.c,v 1.1.2.2 2008/04/03 12:42:31 mjf Exp $	*/

/*
 * Copyright (c) 2008 Christopher Gilbert
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zusb.c,v 1.1.2.2 2008/04/03 12:42:31 mjf Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <zaurus/zaurus/zaurus_reg.h>
#include <zaurus/zaurus/zaurus_var.h>

struct zusb_softc {
	struct device	 sc_dev;
	void		*sc_client_ih;
	void		*sc_host_ih;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

static int	zusb_match(struct device *, struct cfdata *, void *);
static void	zusb_attach(struct device *, struct device *, void *);

CFATTACH_DECL(zusb, sizeof(struct zusb_softc), 
    zusb_match, zusb_attach, NULL, NULL);

static int	zusb_client_intr(void *);
static int	zusb_host_intr(void *);
static void	zusb_test_and_enabled_host_port(struct zusb_softc *);

static int
zusb_match(struct device *parent, struct cfdata *cf, void *aux)
{

	if (ZAURUS_ISC3000)
		return 1;
	return 0;
}

static void
zusb_attach(struct device *parent, struct device *self, void *aux)
{
	struct zusb_softc *sc = (struct zusb_softc *)self;
	struct pxaip_attach_args *pxa = aux;

	sc->sc_iot = pxa->pxa_iot;
	
	/* Map I/O space */
	if (bus_space_map(sc->sc_iot, PXA2X0_USBDC_BASE, PXA270_USBDC_SIZE, 0,
				&sc->sc_ioh)) {
		aprint_error(": couldn't map memory space\n");
		return;
	}

	pxa2x0_gpio_set_function(C3000_USB_DEVICE_PIN, GPIO_IN);
	sc->sc_client_ih = pxa2x0_gpio_intr_establish(C3000_USB_DEVICE_PIN,
	    IST_EDGE_BOTH, IPL_BIO, zusb_client_intr, sc);

	pxa2x0_gpio_set_function(C3000_USB_HOST_PIN, GPIO_IN);
	sc->sc_host_ih = pxa2x0_gpio_intr_establish(C3000_USB_HOST_PIN,
	    IST_EDGE_BOTH, IPL_BIO, zusb_host_intr, sc);

	/* configure port 2 for input */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
			USBDC_UP2OCR_HXS | USBDC_UP2OCR_HXOE | 
			USBDC_UP2OCR_DPPDE | USBDC_UP2OCR_DMPDE);

	zusb_test_and_enabled_host_port(sc);

	printf(": USB Mode detection\n");
}

static int
zusb_client_intr(void *v)
{
	struct zusb_softc *sc = v;
	printf("USB client cable changed\n");
	zusb_test_and_enabled_host_port(sc);

	return 1;
}
	
static int
zusb_host_intr(void *v)
{
	struct zusb_softc *sc = v;

	printf("USB host cable changed\n");
	zusb_test_and_enabled_host_port(sc);

	return 1;
}
	
static void
zusb_test_and_enabled_host_port(struct zusb_softc *sc)
{
	int host_cable = pxa2x0_gpio_get_bit(C3000_USB_HOST_PIN);
	int client_cable = pxa2x0_gpio_get_bit(C3000_USB_DEVICE_PIN);

	printf("USB cable: host %d, client %d\n", host_cable, client_cable);
	if (!host_cable) {
		pxa2x0_gpio_set_function(C3000_USB_HOST_POWER_PIN, GPIO_OUT | GPIO_SET);
		printf("USB host power enabled\n");
	} else {
		pxa2x0_gpio_set_function(C3000_USB_HOST_POWER_PIN, GPIO_OUT | GPIO_CLR);
		printf("USB host power disabled\n");
	}
}

