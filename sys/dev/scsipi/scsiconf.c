/*	$NetBSD: scsiconf.c,v 1.93.2.1 1998/05/05 08:29:32 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include "locators.h"

#if 0
#if NCALS > 0
	{ T_PROCESSOR, T_FIXED, 1,
	  0, 0, 0 },
#endif	/* NCALS */
#if NBLL > 0
	{ T_PROCESSOR, T_FIXED, 1,
	  "AEG     ", "READER          ", "V1.0" },
#endif	/* NBLL */
#if NKIL > 0
	{ T_SCANNER, T_FIXED, 0,
	  "KODAK   ", "IL Scanner 900  ", 0 },
#endif	/* NKIL */
#endif

/*
 * Declarations
 */
void scsi_probedev __P((struct scsibus_softc *, int, int));
int scsi_probe_bus __P((int bus, int target, int lun));

struct scsipi_device probe_switch = {
	NULL,
	NULL,
	NULL,
	NULL,
};

#ifdef __BROKEN_INDIRECT_CONFIG
int scsibusmatch __P((struct device *, void *, void *));
#else
int scsibusmatch __P((struct device *, struct cfdata *, void *));
#endif
void scsibusattach __P((struct device *, struct device *, void *));
#ifdef __BROKEN_INDIRECT_CONFIG
int scsibussubmatch __P((struct device *, void *, void *));
#else
int scsibussubmatch __P((struct device *, struct cfdata *, void *));
#endif

struct cfattach scsibus_ca = {
	sizeof(struct scsibus_softc), scsibusmatch, scsibusattach
};

struct cfdriver scsibus_cd = {
	NULL, "scsibus", DV_DULL
};

int scsibusprint __P((void *, const char *));


int
scsiprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct scsipi_link *l = aux;

	/* only "scsibus"es can attach to "scsi"s; easy. */
	if (pnp)
		printf("scsibus at %s", pnp);

	/* don't print channel if the controller says there can be only one. */
	if (l->scsipi_scsi.channel != SCSI_CHANNEL_ONLY_ONE)
		printf(" channel %d", l->scsipi_scsi.channel);

	return (UNCONF);
}

int
#ifdef __BROKEN_INDIRECT_CONFIG
scsibusmatch(parent, match, aux)
#else
scsibusmatch(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = match;
#endif
	struct scsipi_link *l = aux;
	int channel;

	/*
	 * Allow single-channel controllers to specify their channel
	 * in a special way, so that it's not printed.
	 */
	channel = (l->scsipi_scsi.channel != SCSI_CHANNEL_ONLY_ONE) ?
	    l->scsipi_scsi.channel : 0;

	if (cf->cf_loc[SCSICF_CHANNEL] != channel &&
	    cf->cf_loc[SCSICF_CHANNEL] != SCSICF_CHANNEL_DEFAULT)
		return (0);

	return (1);
}

/*
 * The routine called by the adapter boards to get all their
 * devices configured in.
 */
void
scsibusattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct scsibus_softc *sb = (struct scsibus_softc *)self;
	struct scsipi_link *sc_link_proto = aux;
	size_t nbytes;
	int i;

	sc_link_proto->scsipi_scsi.scsibus = sb->sc_dev.dv_unit;
	sc_link_proto->scsipi_cmd = scsi_scsipi_cmd;
	sc_link_proto->scsipi_interpret_sense = scsi_interpret_sense;
	sc_link_proto->sc_print_addr = scsi_print_addr;

	sb->adapter_link = sc_link_proto;
	sb->sc_maxtarget = sc_link_proto->scsipi_scsi.max_target;
	printf(": %d targets\n", sb->sc_maxtarget + 1);

	nbytes = sb->sc_maxtarget * sizeof(struct scsipi_link **);
	sb->sc_link = (struct scsipi_link ***)malloc(nbytes, M_DEVBUF,
	    M_NOWAIT);
	if (sb->sc_link == NULL)
		panic("scsibusattach: can't allocate target links");

	nbytes = 8 * sizeof(struct scsipi_link *);
	for (i = 0; i <= sb->sc_maxtarget; i++) {
		sb->sc_link[i] = (struct scsipi_link **)malloc(nbytes,
		    M_DEVBUF, M_NOWAIT);
		if (sb->sc_link[i] == NULL)
			panic("scsibusattach: can't allocate lun links");
		bzero(sb->sc_link[i], nbytes);
	}

#if defined(SCSI_DELAY) && SCSI_DELAY > 2
	printf("%s: waiting for scsi devices to settle\n",
	    sb->sc_dev.dv_xname);
#else	/* SCSI_DELAY > 2 */
#undef	SCSI_DELAY
#define SCSI_DELAY 2
#endif	/* SCSI_DELAY */
	delay(1000000 * SCSI_DELAY);

	scsi_probe_bus(sb->sc_dev.dv_unit, -1, -1);
}

int
#ifdef __BROKEN_INDIRECT_CONFIG
scsibussubmatch(parent, match, aux)
#else
scsibussubmatch(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = match;
#endif
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_link *sc_link = sa->sa_sc_link;

	if (cf->cf_loc[SCSIBUSCF_TARGET] != SCSIBUSCF_TARGET_DEFAULT &&
	    cf->cf_loc[SCSIBUSCF_TARGET] != sc_link->scsipi_scsi.target)
		return (0);
	if (cf->cf_loc[SCSIBUSCF_LUN] != SCSIBUSCF_LUN_DEFAULT &&
	    cf->cf_loc[SCSIBUSCF_LUN] != sc_link->scsipi_scsi.lun)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * -1 requests all set up scsi busses.
 * target and lun optionally narrow the search if not -1
 */
int
scsi_probe_busses(bus, target, lun)
	int bus, target, lun;
{

	if (bus == -1) {
		for (bus = 0; bus < scsibus_cd.cd_ndevs; bus++)
			if (scsibus_cd.cd_devs[bus])
				scsi_probe_bus(bus, target, lun);
		return (0);
	} else
		return (scsi_probe_bus(bus, target, lun));
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * target and lun optionally narrow the search if not -1
 */
int
scsi_probe_bus(bus, target, lun)
	int bus, target, lun;
{
	struct scsibus_softc *scsi;
	int maxtarget, mintarget, maxlun, minlun;
	u_int8_t scsi_addr;

	if (bus < 0 || bus >= scsibus_cd.cd_ndevs)
		return (ENXIO);
	scsi = scsibus_cd.cd_devs[bus];
	if (scsi == NULL)
		return (ENXIO);

	scsi_addr = scsi->adapter_link->scsipi_scsi.adapter_target;

	if (target == -1) {
		maxtarget = scsi->sc_maxtarget;
		mintarget = 0;
	} else {
		if (target < 0 || target > scsi->sc_maxtarget)
			return (EINVAL);
		maxtarget = mintarget = target;
	}

	if (lun == -1) {
		maxlun = 7;
		minlun = 0;
	} else {
		if (lun < 0 || lun > 7)
			return (EINVAL);
		maxlun = minlun = lun;
	}

	for (target = mintarget; target <= maxtarget; target++) {
		if (target == scsi_addr)
			continue;
		for (lun = minlun; lun <= maxlun; lun++) {
			/*
			 * See if there's a device present, and configure it.
			 */
			scsi_probedev(scsi, target, lun);
			if ((scsi->moreluns & (1 << target)) == 0)
				break;
			/* otherwise something says we should look further */
		}
	}
	return (0);
}

/*
 * Print out autoconfiguration information for a subdevice.
 *
 * This is a slight abuse of 'standard' autoconfiguration semantics,
 * because 'print' functions don't normally print the colon and
 * device information.  However, in this case that's better than
 * either printing redundant information before the attach message,
 * or having the device driver call a special function to print out
 * the standard device information.
 */
int
scsibusprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_inquiry_pattern *inqbuf;
	u_int8_t type;
	char *dtype, *qtype;
	char vendor[33], product[65], revision[17];
	int target, lun;

	if (pnp != NULL)
		printf("%s", pnp);

	inqbuf = &sa->sa_inqbuf;

	target = sa->sa_sc_link->scsipi_scsi.target;
	lun = sa->sa_sc_link->scsipi_scsi.lun;

	type = inqbuf->type & SID_TYPE;

	/*
	 * Figure out basic device type and qualifier.
	 */
	dtype = 0;
	switch (inqbuf->type & SID_QUAL) {
	case SID_QUAL_LU_OK:
		qtype = "";
		break;

	case SID_QUAL_LU_OFFLINE:
		qtype = " offline";
		break;

	case SID_QUAL_RSVD:
	case SID_QUAL_BAD_LU:
		panic("scsibusprint: impossible qualifier");

	default:
		qtype = "";
		dtype = "vendor-unique";
		break;
	}
	if (dtype == 0)
		dtype = scsipi_dtype(type);

	scsipi_strvis(vendor, inqbuf->vendor, 8);
	scsipi_strvis(product, inqbuf->product, 16);
	scsipi_strvis(revision, inqbuf->revision, 4);

	printf(" targ %d lun %d: <%s, %s, %s> SCSI%d %d/%s %s%s",
	    target, lun, vendor, product, revision,
	    sa->scsipi_info.scsi_version & SID_ANSII, type, dtype,
	    inqbuf->removable ? "removable" : "fixed", qtype);

	return (UNCONF);
}

struct scsi_quirk_inquiry_pattern scsi_quirk_patterns[] = {
	{{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-431  ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "Chinon  ", "CD-ROM CDS-525  ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "DEC     ", "RRD42   (C) DEC ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-535  ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "DENON   ", "DRD-25X         ", "V"},    SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "HP      ", "C4324/C4325     ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "IMS     ", "CDD521/10       ", "2.06"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "MATSHITA", "CD-ROM CR-5XX   ", "1.0b"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "MEDAVIS ", "RENO CD-ROMX2A  ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "MEDIAVIS", "CDR-H93MV       ", "1.3"},  SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:55 ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:83 ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:84 ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:841", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "PIONEER ", "CD-ROM DR-124X  ", "1.01"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-541  ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-55S  ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-8003A", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-8012 ", ""},     SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEAC    ", "CD-ROM          ", "1.06"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEAC    ", "CD-ROM CD-56S   ", "1.0B"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM          ", "1.06"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM DM-XX24 K", "1.10"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TOSHIBA ", "XM-4101TASUNSLCD", "1755"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "JVC     ", "R2626           ", "1.55"}, SDEV_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "ShinaKen", "CD-ROM DM-3x1S", "1.04"}, SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MICROP  ", "1588-15MBSUN0669", ""},     SDEV_AUTOSAVE},
	{{T_OPTICAL, T_REMOV,
	 "EPSON   ", "OMD-5010        ", "3.08"}, SDEV_NOLUNS},

	{{T_DIRECT, T_FIXED,
	 "DEC     ", "RZ55     (C) DEC", ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "EMULEX  ", "MD21/S2     ESDI", "A00"},  SDEV_FORCELUNS|SDEV_AUTOSAVE},
	/* Gives non-media hardware failure in response to start-unit command */
	{{T_DIRECT, T_FIXED,
	 "HITACHI", "DK515C",		"CP16"},  SDEV_NOSTARTUNIT},
	{{T_DIRECT, T_FIXED,
	 "HITACHI", "DK515C",		"CP15"},  SDEV_NOSTARTUNIT},
	{{T_DIRECT, T_FIXED,
	 "IBMRAID ", "0662S",		 ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "0663H",		 ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM",	     "0664",		 ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "H3171-S2",	 ""},	  SDEV_NOLUNS|SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "KZ-C",		 ""},	  SDEV_AUTOSAVE},
	/* Broken IBM disk */
	{{T_DIRECT, T_FIXED,
	 ""	   , "DFRSS2F",		 ""},	  SDEV_AUTOSAVE},
	{{T_DIRECT, T_REMOV,
	 "MPL     ", "MC-DISK-        ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-3280         ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-4380S        ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "MXT-1240S       ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-4170S        ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-8760S",         ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-213S        ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-213S SUN0207", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-200S        ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MEGADRV ", "EV1000",           ""},     SDEV_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "MST     ", "SnapLink        ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "NEC     ", "D3847           ", "0307"}, SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "ELS85S          ", ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "LPS525S         ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "P105S 910-10-94x", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "PD1225S         ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "PD210S   SUN0207", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "RODIME  ", "RO3000S         ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST125N          ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST157N          ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST296           ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST296N          ", ""},     SDEV_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST19171FC", ""},            SDEV_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST34501FC       ", ""},     SDEV_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "TOSHIBA ", "MK538FB         ", "6027"}, SDEV_NOLUNS},
	{{T_DIRECT, T_REMOV,
	 "iomega", "jaz 1GB", 		 ""},	  SDEV_NOMODESENSE},
	{{T_DIRECT, T_REMOV,
	 "IOMEGA", "ZIP 100",		 ""},	  SDEV_NOMODESENSE},
	/* Letting the motor run kills floppy drives and disks quite fast. */
	{{T_DIRECT, T_REMOV,
	 "TEAC", "FC-1",		 ""},	  SDEV_NOSTARTUNIT},

	/* XXX: QIC-36 tape behind Emulex adapter.  Very broken. */
	{{T_SEQUENTIAL, T_REMOV,
	 "        ", "                ", "    "}, SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "CALIPER ", "CP150           ", ""},     SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", ""},     SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "GY-10C          ", ""},     SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-2000        ", "2.09"}, SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5000        ", "3."},   SDEV_NOSYNCWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5200        ", "3."},   SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "TANDBERG", " TDC 3600       ", ""},     SDEV_NOLUNS},
	/* Following entry reported as a Tandberg 3600; ref. PR1933 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 150  21247", ""},     SDEV_NOLUNS},
	/* Following entry for a Cipher ST150S; ref. PR4171 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 1500 21247", "2.2G"}, SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "Python 28454-XXX", ""},     SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5099ES SCSI",      ""},     SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5150ES SCSI",      ""},     SDEV_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 1300      ", "02.4"}, SDEV_NOSYNCWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 2600      ", "01.7"}, SDEV_NOSYNCWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 3200      ", "02.2"}, SDEV_NOSYNCWIDE},

	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "Astra 1200S     ", "V2.9"}, SDEV_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UMAX S-6E       ", "V2.0"}, SDEV_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UMAX S-12       ", "V2.1"}, SDEV_NOLUNS},

	{{T_PROCESSOR, T_FIXED,
	 "LITRONIC", "PCMCIA          ", ""},     SDEV_NOLUNS},
};

/*
 * given a target and lun, ask the device what
 * it is, and find the correct driver table
 * entry.
 */
void
scsi_probedev(scsi, target, lun)
	struct scsibus_softc *scsi;
	int target, lun;
{
	struct scsipi_link *sc_link;
	static struct scsipi_inquiry_data inqbuf;
	struct scsi_quirk_inquiry_pattern *finger;
	int checkdtype, priority;
	struct scsipibus_attach_args sa;
	struct cfdata *cf;

	/* Skip this slot if it is already attached. */
	if (scsi->sc_link[target][lun] != NULL)
		return;

	sc_link = malloc(sizeof(*sc_link), M_DEVBUF, M_NOWAIT);
	*sc_link = *scsi->adapter_link;
	sc_link->scsipi_scsi.target = target;
	sc_link->scsipi_scsi.lun = lun;
	sc_link->device = &probe_switch;

	/*
	 * Ask the device what it is
	 */
#if defined(SCSIDEBUG) && DEBUGTYPE == BUS_SCSI
	if (target == DEBUGTARGET && lun == DEBUGLUN)
		sc_link->flags |= DEBUGLEVEL;
#endif /* SCSIDEBUG */

	(void) scsipi_test_unit_ready(sc_link,
	    SCSI_AUTOCONF | SCSI_IGNORE_ILLEGAL_REQUEST |
	    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);

#ifdef SCSI_2_DEF
	/* some devices need to be told to go to SCSI2 */
	/* However some just explode if you tell them this.. leave it out */
	scsi_change_def(sc_link, SCSI_AUTOCONF | SCSI_SILENT);
#endif /* SCSI_2_DEF */

	/* Now go ask the device all about itself. */
	bzero(&inqbuf, sizeof(inqbuf));
	if (scsipi_inquire(sc_link, &inqbuf, SCSI_AUTOCONF) != 0)
		goto bad;

	{
		int len = inqbuf.additional_length;
		while (len < 3)
			inqbuf.unused[len++] = '\0';
		while (len < 3 + 28)
			inqbuf.unused[len++] = ' ';
	}

	sa.sa_sc_link = sc_link;
	sa.sa_inqbuf.type = inqbuf.device;
	sa.sa_inqbuf.removable = inqbuf.dev_qual2 & SID_REMOVABLE ?
	    T_REMOV : T_FIXED;
	sa.sa_inqbuf.vendor = inqbuf.vendor;
	sa.sa_inqbuf.product = inqbuf.product;
	sa.sa_inqbuf.revision = inqbuf.revision;
	sa.scsipi_info.scsi_version = inqbuf.version;

	finger = (struct scsi_quirk_inquiry_pattern *)scsipi_inqmatch(
	    &sa.sa_inqbuf, (caddr_t)scsi_quirk_patterns,
	    sizeof(scsi_quirk_patterns)/sizeof(scsi_quirk_patterns[0]),
	    sizeof(scsi_quirk_patterns[0]), &priority);
	if (priority != 0)
		sc_link->quirks |= finger->quirks;
	if ((inqbuf.version & SID_ANSII) == 0 &&
	    (sc_link->quirks & SDEV_FORCELUNS) == 0)
		sc_link->quirks |= SDEV_NOLUNS;
	sc_link->scsipi_scsi.scsi_version = inqbuf.version;

	if ((sc_link->quirks & SDEV_NOLUNS) == 0)
		scsi->moreluns |= (1 << target);

	/*
	 * note what BASIC type of device it is
	 */
	if ((inqbuf.dev_qual2 & SID_REMOVABLE) != 0)
		sc_link->flags |= SDEV_REMOVABLE;

	/*
	 * Any device qualifier that has the top bit set (qualifier&4 != 0)
	 * is vendor specific and won't match in this switch.
	 * All we do here is throw out bad/negative responses.
	 */
	checkdtype = 0;
	switch (inqbuf.device & SID_QUAL) {
	case SID_QUAL_LU_OK:
	case SID_QUAL_LU_OFFLINE:
		checkdtype = 1;
		break;

	case SID_QUAL_RSVD:
	case SID_QUAL_BAD_LU:
		goto bad;

	default:
		break;
	}
	if (checkdtype)
		switch (inqbuf.device & SID_TYPE) {
		case T_DIRECT:
		case T_SEQUENTIAL:
		case T_PRINTER:
		case T_PROCESSOR:
		case T_WORM:
		case T_CDROM:
		case T_SCANNER:
		case T_OPTICAL:
		case T_CHANGER:
		case T_COMM:
		case T_IT8_1:
		case T_IT8_2:
		case T_STORARRAY:
		case T_ENCLOSURE:
		default:
			break;
		case T_NODEVICE:
			goto bad;
		}

	if ((cf = config_search(scsibussubmatch, (struct device *)scsi,
	    &sa)) != NULL) {
		scsi->sc_link[target][lun] = sc_link;
		config_attach((struct device *)scsi, cf, &sa, scsibusprint);
	} else {
		scsibusprint(&sa, scsi->sc_dev.dv_xname);
		printf(" not configured\n");
		goto bad;
	}

	return;

bad:
	free(sc_link, M_DEVBUF);
	return;
}
