/*	$NetBSD: ch.c,v 1.37.2.1.2.1 1999/06/21 01:19:10 thorpej Exp $	*/

/*
 * Copyright (c) 1996, 1997, 1998 Jason R. Thorpe <thorpej@and.com>
 * All rights reserved.
 *
 * Partially based on an autochanger driver written by Stefan Grefen
 * and on an autochanger driver written by the Systems Programming Group
 * at the University of Utah Computer Science Department.
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
 *    must display the following acknowledgements:
 *	This product includes software developed by Jason R. Thorpe
 *	for And Communications, http://www.and.com/
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/chio.h> 
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_changer.h>
#include <dev/scsipi/scsiconf.h>

#define CHRETRIES	2
#define CHUNIT(x)	(minor((x)))

struct ch_softc {
	struct device	sc_dev;		/* generic device info */
	struct scsipi_link *sc_link;	/* link in the SCSI bus */

	int		sc_picker;	/* current picker */

	/*
	 * The following information is obtained from the
	 * element address assignment page.
	 */
	int		sc_firsts[4];	/* firsts, indexed by CHET_* */
	int		sc_counts[4];	/* counts, indexed by CHET_* */

	/*
	 * The following mask defines the legal combinations
	 * of elements for the MOVE MEDIUM command.
	 */
	u_int8_t	sc_movemask[4];

	/*
	 * As above, but for EXCHANGE MEDIUM.
	 */
	u_int8_t	sc_exchangemask[4];

	int		flags;		/* misc. info */

	/*
	 * Quirks; see below.
	 */
	int		sc_settledelay;	/* delay for settle */

};

/* sc_flags */
#define CHF_ROTATE	0x01		/* picker can rotate */

/* Autoconfiguration glue */
int	chmatch __P((struct device *, struct cfdata *, void *));
void	chattach __P((struct device *, struct device *, void *));

struct cfattach ch_ca = {
	sizeof(struct ch_softc), chmatch, chattach
};

extern struct cfdriver ch_cd;

struct scsipi_inquiry_pattern ch_patterns[] = {
	{T_CHANGER, T_REMOV,
	 "",		"",		""},
};

/* SCSI glue */
struct scsipi_device ch_switch = {
	NULL, NULL, NULL, NULL
};

int	ch_move __P((struct ch_softc *, struct changer_move *));
int	ch_exchange __P((struct ch_softc *, struct changer_exchange *));
int	ch_position __P((struct ch_softc *, struct changer_position *));
int	ch_ielem __P((struct ch_softc *));
int	ch_usergetelemstatus __P((struct ch_softc *, int, u_int8_t *));
int	ch_getelemstatus __P((struct ch_softc *, int, int, caddr_t, size_t));
int	ch_get_params __P((struct ch_softc *, int));
void	ch_get_quirks __P((struct ch_softc *,
	    struct scsipi_inquiry_pattern *));

/*
 * SCSI changer quirks.
 */
struct chquirk {
	struct	scsipi_inquiry_pattern cq_match; /* device id pattern */
	int	cq_settledelay;	/* settle delay, in seconds */
};

struct chquirk chquirks[] = {
	{{T_CHANGER, T_REMOV,
	  "SPECTRA",	"9000",		"0200"},
	 75},
};

int
chmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct scsipibus_attach_args *sa = aux;
	int priority;

	(void)scsipi_inqmatch(&sa->sa_inqbuf,
	    (caddr_t)ch_patterns, sizeof(ch_patterns) / sizeof(ch_patterns[0]),
	    sizeof(ch_patterns[0]), &priority);

	return (priority);
}

void
chattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ch_softc *sc = (struct ch_softc *)self;
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_link *link = sa->sa_sc_link;

	/* Glue into the SCSI bus */
	sc->sc_link = link;
	link->device = &ch_switch;
	link->device_softc = sc;
	link->openings = 1;

	printf("\n");

	/*
	 * Find out our device's quirks.
	 */
	ch_get_quirks(sc, &sa->sa_inqbuf);

	/*
	 * Some changers require a long time to settle out, to do
	 * tape inventory, for instance.
	 */
	if (sc->sc_settledelay) {
		printf("%s: waiting %d seconds for changer to settle...\n",
		    sc->sc_dev.dv_xname, sc->sc_settledelay);
		delay(1000000 * sc->sc_settledelay);
	}

	/*
	 * Get information about the device.  Note we can't use
	 * interrupts yet.
	 */
	if (ch_get_params(sc, SCSI_AUTOCONF))
		printf("%s: offline\n", sc->sc_dev.dv_xname);
	else {
#define PLURAL(c)	(c) == 1 ? "" : "s"
		printf("%s: %d slot%s, %d drive%s, %d picker%s, %d portal%s\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_counts[CHET_ST], PLURAL(sc->sc_counts[CHET_ST]),
		    sc->sc_counts[CHET_DT], PLURAL(sc->sc_counts[CHET_DT]),
		    sc->sc_counts[CHET_MT], PLURAL(sc->sc_counts[CHET_MT]),
		    sc->sc_counts[CHET_IE], PLURAL(sc->sc_counts[CHET_IE]));
#undef PLURAL
#ifdef CHANGER_DEBUG
		printf("%s: move mask: 0x%x 0x%x 0x%x 0x%x\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_movemask[CHET_MT], sc->sc_movemask[CHET_ST],
		    sc->sc_movemask[CHET_IE], sc->sc_movemask[CHET_DT]);
		printf("%s: exchange mask: 0x%x 0x%x 0x%x 0x%x\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_exchangemask[CHET_MT], sc->sc_exchangemask[CHET_ST],
		    sc->sc_exchangemask[CHET_IE], sc->sc_exchangemask[CHET_DT]);
#endif /* CHANGER_DEBUG */
	}

	/* Default the current picker. */
	sc->sc_picker = sc->sc_firsts[CHET_MT];
}

int
chopen(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	struct ch_softc *sc;
	int unit, error;

	unit = CHUNIT(dev);
	if ((unit >= ch_cd.cd_ndevs) ||
	    ((sc = ch_cd.cd_devs[unit]) == NULL))
		return (ENXIO);

	/*
	 * Only allow one open at a time.
	 */
	if (sc->sc_link->flags & SDEV_OPEN)
		return (EBUSY);

	if ((error = scsipi_adapter_addref(sc->sc_link)) != 0)
		return (error);

	sc->sc_link->flags |= SDEV_OPEN;

	/*
	 * Absorb any unit attention errors.  Ignore "not ready"
	 * since this might occur if e.g. a tape isn't actually
	 * loaded in the drive.
	 */
	error = scsipi_test_unit_ready(sc->sc_link,
	    SCSI_IGNORE_NOT_READY|SCSI_IGNORE_MEDIA_CHANGE);
	if (error)
		goto bad;

	/*
	 * Make sure our parameters are up to date.
	 */
	if ((error = ch_get_params(sc, 0)) != 0)
		goto bad;

	return (0);

 bad:
	scsipi_adapter_delref(sc->sc_link);
	sc->sc_link->flags &= ~SDEV_OPEN;
	return (error);
}

int
chclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	struct ch_softc *sc = ch_cd.cd_devs[CHUNIT(dev)];

	scsipi_wait_drain(sc->sc_link);

	scsipi_adapter_delref(sc->sc_link);
	sc->sc_link->flags &= ~SDEV_OPEN;
	return (0);
}

int
chioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct ch_softc *sc = ch_cd.cd_devs[CHUNIT(dev)];
	int error = 0;

	/*
	 * If this command can change the device's state, we must
	 * have the device open for writing.
	 */
	switch (cmd) {
	case CHIOGPICKER:
	case CHIOGPARAMS:
	case CHIOGSTATUS:
		break;

	default:
		if ((flags & FWRITE) == 0)
			return (EBADF);
	}

	switch (cmd) {
	case CHIOMOVE:
		error = ch_move(sc, (struct changer_move *)data);
		break;

	case CHIOEXCHANGE:
		error = ch_exchange(sc, (struct changer_exchange *)data);
		break;

	case CHIOPOSITION:
		error = ch_position(sc, (struct changer_position *)data);
		break;

	case CHIOGPICKER:
		*(int *)data = sc->sc_picker - sc->sc_firsts[CHET_MT];
		break;

	case CHIOSPICKER:	{
		int new_picker = *(int *)data;

		if (new_picker > (sc->sc_counts[CHET_MT] - 1))
			return (EINVAL);
		sc->sc_picker = sc->sc_firsts[CHET_MT] + new_picker;
		break;		}

	case CHIOGPARAMS:	{
		struct changer_params *cp = (struct changer_params *)data;

		cp->cp_curpicker = sc->sc_picker - sc->sc_firsts[CHET_MT];
		cp->cp_npickers = sc->sc_counts[CHET_MT];
		cp->cp_nslots = sc->sc_counts[CHET_ST];
		cp->cp_nportals = sc->sc_counts[CHET_IE];
		cp->cp_ndrives = sc->sc_counts[CHET_DT];
		break;		}

	case CHIOIELEM:
		error = ch_ielem(sc);
		break;

	case CHIOGSTATUS:	{
		struct changer_element_status *ces =
		    (struct changer_element_status *)data;

		error = ch_usergetelemstatus(sc, ces->ces_type, ces->ces_data);
		break;		}

	/* Implement prevent/allow? */

	default:
		error = scsipi_do_ioctl(sc->sc_link, dev, cmd, data, flags, p);
		break;
	}

	return (error);
}

int
ch_move(sc, cm)
	struct ch_softc *sc;
	struct changer_move *cm;
{
	struct scsi_move_medium cmd;
	u_int16_t fromelem, toelem;

	/*
	 * Check arguments.
	 */
	if ((cm->cm_fromtype > CHET_DT) || (cm->cm_totype > CHET_DT))
		return (EINVAL);
	if ((cm->cm_fromunit > (sc->sc_counts[cm->cm_fromtype] - 1)) ||
	    (cm->cm_tounit > (sc->sc_counts[cm->cm_totype] - 1)))
		return (ENODEV);

	/*
	 * Check the request against the changer's capabilities.
	 */
	if ((sc->sc_movemask[cm->cm_fromtype] & (1 << cm->cm_totype)) == 0)
		return (ENODEV);

	/*
	 * Calculate the source and destination elements.
	 */
	fromelem = sc->sc_firsts[cm->cm_fromtype] + cm->cm_fromunit;
	toelem = sc->sc_firsts[cm->cm_totype] + cm->cm_tounit;

	/*
	 * Build the SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = MOVE_MEDIUM;
	_lto2b(sc->sc_picker, cmd.tea);
	_lto2b(fromelem, cmd.src);
	_lto2b(toelem, cmd.dst);
	if (cm->cm_flags & CM_INVERT)
		cmd.flags |= MOVE_MEDIUM_INVERT;

	/*
	 * Send command to changer.
	 */
	return (scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd), NULL, 0, CHRETRIES,
	    100000, NULL, 0));
}

int
ch_exchange(sc, ce)
	struct ch_softc *sc;
	struct changer_exchange *ce;
{
	struct scsi_exchange_medium cmd;
	u_int16_t src, dst1, dst2;

	/*
	 * Check arguments.
	 */
	if ((ce->ce_srctype > CHET_DT) || (ce->ce_fdsttype > CHET_DT) ||
	    (ce->ce_sdsttype > CHET_DT))
		return (EINVAL);
	if ((ce->ce_srcunit > (sc->sc_counts[ce->ce_srctype] - 1)) ||
	    (ce->ce_fdstunit > (sc->sc_counts[ce->ce_fdsttype] - 1)) ||
	    (ce->ce_sdstunit > (sc->sc_counts[ce->ce_sdsttype] - 1)))
		return (ENODEV);

	/*
	 * Check the request against the changer's capabilities.
	 */
	if (((sc->sc_exchangemask[ce->ce_srctype] &
	     (1 << ce->ce_fdsttype)) == 0) ||
	    ((sc->sc_exchangemask[ce->ce_fdsttype] &
	     (1 << ce->ce_sdsttype)) == 0))
		return (ENODEV);

	/*
	 * Calculate the source and destination elements.
	 */
	src = sc->sc_firsts[ce->ce_srctype] + ce->ce_srcunit;
	dst1 = sc->sc_firsts[ce->ce_fdsttype] + ce->ce_fdstunit;
	dst2 = sc->sc_firsts[ce->ce_sdsttype] + ce->ce_sdstunit;

	/*
	 * Build the SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = EXCHANGE_MEDIUM;
	_lto2b(sc->sc_picker, cmd.tea);
	_lto2b(src, cmd.src);
	_lto2b(dst1, cmd.fdst);
	_lto2b(dst2, cmd.sdst);
	if (ce->ce_flags & CE_INVERT1)
		cmd.flags |= EXCHANGE_MEDIUM_INV1;
	if (ce->ce_flags & CE_INVERT2)
		cmd.flags |= EXCHANGE_MEDIUM_INV2;

	/*
	 * Send command to changer.
	 */
	return (scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd), NULL, 0, CHRETRIES,
	    100000, NULL, 0));
}

int
ch_position(sc, cp)
	struct ch_softc *sc;
	struct changer_position *cp;
{
	struct scsi_position_to_element cmd;
	u_int16_t dst;

	/*
	 * Check arguments.
	 */
	if (cp->cp_type > CHET_DT)
		return (EINVAL);
	if (cp->cp_unit > (sc->sc_counts[cp->cp_type] - 1))
		return (ENODEV);

	/*
	 * Calculate the destination element.
	 */
	dst = sc->sc_firsts[cp->cp_type] + cp->cp_unit;

	/*
	 * Build the SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = POSITION_TO_ELEMENT;
	_lto2b(sc->sc_picker, cmd.tea);
	_lto2b(dst, cmd.dst);
	if (cp->cp_flags & CP_INVERT)
		cmd.flags |= POSITION_TO_ELEMENT_INVERT;

	/*
	 * Send command to changer.
	 */
	return (scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd), NULL, 0, CHRETRIES,
	    100000, NULL, 0));
}

/*
 * Perform a READ ELEMENT STATUS on behalf of the user, and return to
 * the user only the data the user is interested in (i.e. an array of
 * flags bytes).
 */
int
ch_usergetelemstatus(sc, chet, uptr)
	struct ch_softc *sc;
	int chet;
	u_int8_t *uptr;
{
	struct read_element_status_header *st_hdr;
	struct read_element_status_page_header *pg_hdr;
	struct read_element_status_descriptor *desc;
	caddr_t data = NULL;
	size_t size, desclen;
	int avail, i, error = 0;
	u_int8_t *user_data = NULL;

	/*
	 * If there are no elements of the requested type in the changer,
	 * the request is invalid.
	 */
	if (sc->sc_counts[chet] == 0)
		return (EINVAL);

	/*
	 * Request one descriptor for the given element type.  This
	 * is used to determine the size of the descriptor so that
	 * we can allocate enough storage for all of them.  We assume
	 * that the first one can fit into 1k.
	 */
	data = (caddr_t)malloc(1024, M_DEVBUF, M_WAITOK);
	error = ch_getelemstatus(sc, sc->sc_firsts[chet], 1, data, 1024);
	if (error)
		goto done;

	st_hdr = (struct read_element_status_header *)data;
	pg_hdr = (struct read_element_status_page_header *)((u_long)st_hdr +
	    sizeof(struct read_element_status_header));
	desclen = _2btol(pg_hdr->edl);

	size = sizeof(struct read_element_status_header) +
	    sizeof(struct read_element_status_page_header) +
	    (desclen * sc->sc_counts[chet]);

	/*
	 * Reallocate storage for descriptors and get them from the
	 * device.
	 */
	free(data, M_DEVBUF);
	data = (caddr_t)malloc(size, M_DEVBUF, M_WAITOK);
	error = ch_getelemstatus(sc, sc->sc_firsts[chet],
	    sc->sc_counts[chet], data, size);
	if (error)
		goto done;

	/*
	 * Fill in the user status array.
	 */
	st_hdr = (struct read_element_status_header *)data;
	avail = _2btol(st_hdr->count);

	if (avail != sc->sc_counts[chet])
		printf("%s: warning, READ ELEMENT STATUS avail != count\n",
		    sc->sc_dev.dv_xname);

	user_data = (u_int8_t *)malloc(avail, M_DEVBUF, M_WAITOK);

	desc = (struct read_element_status_descriptor *)((u_long)data +
	    sizeof(struct read_element_status_header) +
	    sizeof(struct read_element_status_page_header));
	for (i = 0; i < avail; ++i) {
		user_data[i] = desc->flags1;
		(u_long)desc += desclen;
	}

	/* Copy flags array out to userspace. */
	error = copyout(user_data, uptr, avail);

 done:
	if (data != NULL)
		free(data, M_DEVBUF);
	if (user_data != NULL)
		free(user_data, M_DEVBUF);
	return (error);
}

int
ch_getelemstatus(sc, first, count, data, datalen)
	struct ch_softc *sc;
	int first, count;
	caddr_t data;
	size_t datalen;
{
	struct scsi_read_element_status cmd;

	/*
	 * Build SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = READ_ELEMENT_STATUS;
	_lto2b(first, cmd.sea);
	_lto2b(count, cmd.count);
	_lto3b(datalen, cmd.len);

	/*
	 * Send command to changer.
	 */
	return (scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd),
	    (u_char *)data, datalen, CHRETRIES, 100000, NULL, SCSI_DATA_IN));
}


int
ch_ielem(sc)
	struct ch_softc *sc;
{
	int tmo;
	struct scsi_initialize_element_status cmd;

	/*
	 * Build SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = INITIALIZE_ELEMENT_STATUS;

	/*
	 * Send command to changer.
	 *
	 * The problem is, how long to allow for the command?
	 * It can take a *really* long time, and also depends
	 * on unknowable factors such as whether there are
	 * *almost* readable labels on tapes that a barcode
	 * reader is trying to decipher.
	 *
	 * I'm going to make this long enough to allow 5 minutes
	 * per element plus an initial 10 minute wait.
	 */
	tmo =	sc->sc_counts[CHET_MT] +
		sc->sc_counts[CHET_ST] +
		sc->sc_counts[CHET_IE] +
		sc->sc_counts[CHET_DT];
	tmo *= 5 * 60 * 1000;
	tmo += (10 * 60 * 1000);

	return (scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd),
	    NULL, 0, CHRETRIES, tmo, NULL, 0));
}

/*
 * Ask the device about itself and fill in the parameters in our
 * softc.
 */
int
ch_get_params(sc, scsiflags)
	struct ch_softc *sc;
	int scsiflags;
{
	struct scsi_mode_sense cmd;
	struct scsi_mode_sense_data {
		struct scsi_mode_header header;
		union {
			struct page_element_address_assignment ea;
			struct page_transport_geometry_parameters tg;
			struct page_device_capabilities cap;
		} pages;
	} sense_data;
	int error, from;
	u_int8_t *moves, *exchanges;

	/*
	 * Grab info from the element address assignment page.
	 */
	bzero(&cmd, sizeof(cmd));
	bzero(&sense_data, sizeof(sense_data));
	cmd.opcode = SCSI_MODE_SENSE;
	cmd.byte2 |= 0x08;	/* disable block descriptors */
	cmd.page = 0x1d;
	cmd.length = (sizeof(sense_data) & 0xff);
	error = scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd), (u_char *)&sense_data,
	    sizeof(sense_data), CHRETRIES, 6000, NULL,
	    scsiflags | SCSI_DATA_IN);
	if (error) {
		printf("%s: could not sense element address page\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	sc->sc_firsts[CHET_MT] = _2btol(sense_data.pages.ea.mtea);
	sc->sc_counts[CHET_MT] = _2btol(sense_data.pages.ea.nmte);
	sc->sc_firsts[CHET_ST] = _2btol(sense_data.pages.ea.fsea);
	sc->sc_counts[CHET_ST] = _2btol(sense_data.pages.ea.nse);
	sc->sc_firsts[CHET_IE] = _2btol(sense_data.pages.ea.fieea);
	sc->sc_counts[CHET_IE] = _2btol(sense_data.pages.ea.niee);
	sc->sc_firsts[CHET_DT] = _2btol(sense_data.pages.ea.fdtea);
	sc->sc_counts[CHET_DT] = _2btol(sense_data.pages.ea.ndte);

	/* XXX ask for page trasport geom */

	/*
	 * Grab info from the capabilities page.
	 */
	bzero(&cmd, sizeof(cmd));
	bzero(&sense_data, sizeof(sense_data));
	cmd.opcode = SCSI_MODE_SENSE;
	/*
	 * XXX: Note: not all changers can deal with disabled block descriptors
	 */
	cmd.byte2 = 0x08;	/* disable block descriptors */
	cmd.page = 0x1f;
	cmd.length = (sizeof(sense_data) & 0xff);
	error = scsipi_command(sc->sc_link,
	    (struct scsipi_generic *)&cmd, sizeof(cmd), (u_char *)&sense_data,
	    sizeof(sense_data), CHRETRIES, 6000, NULL,
	    scsiflags | SCSI_DATA_IN);
	if (error) {
		printf("%s: could not sense capabilities page\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	bzero(sc->sc_movemask, sizeof(sc->sc_movemask));
	bzero(sc->sc_exchangemask, sizeof(sc->sc_exchangemask));
	moves = &sense_data.pages.cap.move_from_mt;
	exchanges = &sense_data.pages.cap.exchange_with_mt;
	for (from = CHET_MT; from <= CHET_DT; ++from) {
		sc->sc_movemask[from] = moves[from];
		sc->sc_exchangemask[from] = exchanges[from];
	}

	sc->sc_link->flags |= SDEV_MEDIA_LOADED;
	return (0);
}

void
ch_get_quirks(sc, inqbuf)
	struct ch_softc *sc;
	struct scsipi_inquiry_pattern *inqbuf;
{
	struct chquirk *match;
	int priority;

	sc->sc_settledelay = 0;

	match = (struct chquirk *)scsipi_inqmatch(inqbuf,
	    (caddr_t)chquirks,
	    sizeof(chquirks) / sizeof(chquirks[0]),
	    sizeof(chquirks[0]), &priority);
	if (priority != 0)
		sc->sc_settledelay = match->cq_settledelay;
}
