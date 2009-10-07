/*	$NetBSD: sdmmc_cis.c,v 1.1.8.2 2009/10/07 15:41:13 sborrill Exp $	*/
/*	$OpenBSD: sdmmc_cis.c,v 1.1 2006/06/01 21:53:41 uwe Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Routines to decode the Card Information Structure of SD I/O cards */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdmmc_cis.c,v 1.1.8.2 2009/10/07 15:41:13 sborrill Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmc_ioreg.h>
#include <dev/sdmmc/sdmmcdevs.h>
#include <dev/sdmmc/sdmmcvar.h>

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

static uint32_t sdmmc_cisptr(struct sdmmc_function *);
static uint32_t
sdmmc_cisptr(struct sdmmc_function *sf)
{
	uint32_t cisptr = 0;

	/* XXX where is the per-function CIS pointer register? */
	if (sf->number != 0)
		return SD_IO_CIS_START;

	/* XXX is the CIS pointer stored in little-endian format? */
	cisptr |= sdmmc_io_read_1(sf, SD_IO_CCCR_CISPTR + 0) << 0;
	cisptr |= sdmmc_io_read_1(sf, SD_IO_CCCR_CISPTR + 1) << 8;
	cisptr |= sdmmc_io_read_1(sf, SD_IO_CCCR_CISPTR + 2) << 16;
	return cisptr;
}

int
sdmmc_read_cis(struct sdmmc_function *sf, struct sdmmc_cis *cis)
{
	device_t dev = sf->sc->sc_dev;
	uint32_t reg;
	uint8_t tplcode;
	uint8_t tpllen;
	int start, ch, count;
	int i;

	memset(cis, 0, sizeof *cis);

	/* XXX read per-function CIS */
	if (sf->number != 0)
		return 1;

	reg = sdmmc_cisptr(sf);
	if (reg < SD_IO_CIS_START ||
	    reg >= (SD_IO_CIS_START + SD_IO_CIS_SIZE - 16)) {
		aprint_error_dev(dev, "bad CIS ptr %#x\n", reg);
		return 1;
	}

	for (;;) {
		tplcode = sdmmc_io_read_1(sf, reg++);
		tpllen = sdmmc_io_read_1(sf, reg++);

		if (tplcode == 0xff || tpllen == 0) {
			if (tplcode != 0xff)
				aprint_error_dev(dev, "CIS parse error at %d, "
				    "tuple code %#x, length %d\n",
				    reg, tplcode, tpllen);
			break;
		}

		switch (tplcode) {
		case SD_IO_CISTPL_FUNCID:
			if (tpllen < 2) {
				aprint_error_dev(dev,
				    "bad CISTPL_FUNCID length\n");
				reg += tpllen;
				break;
			}
			cis->function = sdmmc_io_read_1(sf, reg);
			reg += tpllen;
			break;

		case SD_IO_CISTPL_MANFID:
			if (tpllen < 4) {
				aprint_error_dev(dev,
				    "bad CISTPL_MANFID length\n");
				reg += tpllen;
				break;
			}
			cis->manufacturer = sdmmc_io_read_1(sf, reg++);
			cis->manufacturer |= sdmmc_io_read_1(sf, reg++) << 8;
			cis->product = sdmmc_io_read_1(sf, reg++);
			cis->product |= sdmmc_io_read_1(sf, reg++) << 8;
			break;

		case SD_IO_CISTPL_VERS_1:
			if (tpllen < 2) {
				aprint_error_dev(dev,
				    "CISTPL_VERS_1 too short\n");
				reg += tpllen;
				break;
			}

			cis->cis1_major = sdmmc_io_read_1(sf, reg++);
			cis->cis1_minor = sdmmc_io_read_1(sf, reg++);

			for (count = 0, start = 0, i = 0;
			     (count < 4) && ((i + 4) < 256); i++) {
				ch = sdmmc_io_read_1(sf, reg + i);
				if (ch == 0xff)
					break;
				cis->cis1_info_buf[i] = ch;
				if (ch == 0) {
					cis->cis1_info[count] =
					    cis->cis1_info_buf + start;
					start = i + 1;
					count++;
				}
			}

			reg += tpllen - 2;
			break;

		default:
			aprint_error_dev(dev,
			    "unknown tuple code %#x, length %d\n",
			    tplcode, tpllen);
			reg += tpllen;
			break;
		}
	}

	return 0;
}

void
sdmmc_print_cis(struct sdmmc_function *sf)
{
	device_t dev = sf->sc->sc_dev;
	struct sdmmc_cis *cis = &sf->cis;
	int i;

	printf("%s: CIS version %u.%u\n", device_xname(dev), cis->cis1_major,
	    cis->cis1_minor);

	printf("%s: CIS info: ", device_xname(dev));
	for (i = 0; i < 4; i++) {
		if (cis->cis1_info[i] == NULL)
			break;
		if (i != 0)
			aprint_verbose(", ");
		printf("%s", cis->cis1_info[i]);
	}
	printf("\n");

	printf("%s: Manufacturer code 0x%x, product 0x%x\n", device_xname(dev),
	    cis->manufacturer, cis->product);

	printf("%s: function %d: ", device_xname(dev), sf->number);
	switch (sf->cis.function) {
	case SDMMC_FUNCTION_WLAN:
		printf("wireless network adapter");
		break;

	default:
		printf("unknown function (%d)", sf->cis.function);
		break;
	}
	printf("\n");
}

void
sdmmc_check_cis_quirks(struct sdmmc_function *sf)
{
	char *p;
	int i;

	if (sf->cis.manufacturer == SDMMC_VENDOR_SPECTEC &&
	    sf->cis.product == SDMMC_PRODUCT_SPECTEC_SDW820) {
		/* This card lacks the VERS_1 tuple. */
		static const char cis1_info[] = 
		    "Spectec\0SDIO WLAN Card\0SDW-820\0\0";

		sf->cis.cis1_major = 0x01;
		sf->cis.cis1_minor = 0x00;

		p = sf->cis.cis1_info_buf;
		strlcpy(p, cis1_info, sizeof(sf->cis.cis1_info_buf));
		for (i = 0; i < 4; i++) {
			sf->cis.cis1_info[i] = p;
			p += strlen(p) + 1;
		}
	}
}
