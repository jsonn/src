/*	$NetBSD: umass.c,v 1.38.2.3 2001/09/30 12:03:15 he Exp $	*/
/*-
 * Copyright (c) 1999 MAEKAWA Masahide <bishop@rr.iij4u.or.jp>,
 *		      Nick Hibma <n_hibma@freebsd.org>
 * All rights reserved.
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
 *     $FreeBSD: src/sys/dev/usb/umass.c,v 1.13 2000/03/26 01:39:12 n_hibma Exp $
 */

/*
 * Universal Serial Bus Mass Storage Class Bulk-Only Transport
 * http://www.usb.org/developers/usbmassbulk_09.pdf
 * XXX Add URL to CBI spec in www.usb.org
 */

/*
 * Ported to NetBSD by Lennart Augustsson <augustss@netbsd.org>.
 * Parts of the code written my Jason R. Thorpe <thorpej@shagadelic.org>.
 */

/*
 * The driver handles 3 Wire Protocols
 * - Command/Bulk/Interrupt (CBI)
 * - Command/Bulk/Interrupt with Command Completion Interrupt (CBI with CCI)
 * - Mass Storage Bulk-Only (BBB)
 *   (BBB refers Bulk/Bulk/Bulk for Command/Data/Status phases)
 *
 * Over these wire protocols it handles the following command protocols
 * - SCSI
 * - UFI (floppy command set)
 * - 8070 (ATA/ATAPI)
 *
 * UFI and 8070i are transformed versions of the SCSI command set. The
 * sc->transform method is used to convert the commands into the appropriate
 * format (if at all necessary). For example, UFI requires all commands to be
 * 12 bytes in length amongst other things.
 *
 * The source code below is marked and can be split into a number of pieces
 * (in this order):
 *
 * - probe/attach/detach
 * - generic transfer routines
 * - BBB
 * - CBI
 * - CBI_I (in addition to functions from CBI)
 * - CAM (Common Access Method)
 * - SCSI
 * - UFI
 * - 8070i
 *
 * The protocols are implemented using a state machine, for the transfers as
 * well as for the resets. The state machine is contained in umass_*_state.
 * The state machine is started through either umass_*_transfer or
 * umass_*_reset.
 *
 * The reason for doing this is a) CAM performs a lot better this way and b) it
 * avoids using tsleep from interrupt context (for example after a failed
 * transfer).
 */

/*
 * The SCSI related part of this driver has been derived from the
 * dev/ppbus/vpo.c driver, by Nicolas Souchu (nsouch@freebsd.org).
 *
 * The CAM layer uses so called actions which are messages sent to the host
 * adapter for completion. The actions come in through umass_cam_action. The
 * appropriate block of routines is called depending on the transport protocol
 * in use. When the transfer has finished, these routines call
 * umass_cam_cb again to complete the CAM command.
 */

/* XXX Should we split the driver into a number of files?  umass.c,
 *     umass_scsi.c, umass_8070.c, umass_ufi.c, umass_bbb.c, umass_cbi.c or
 *     something similar?
 */

#include "atapibus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#undef KASSERT
#define KASSERT(cond, msg)
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/clock.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#if defined(__FreeBSD__)
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>

#ifdef UMASS_DO_CAM_RESCAN
#include <sys/devicestat.h>
#include <cam/cam_periph.h>
#endif

#elif defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/scsiio.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/scsipi/atapiconf.h>

#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsi_changer.h>

#include <dev/ata/atavar.h>	/* XXX */
#include <sys/disk.h>		/* XXX */
#include <dev/scsipi/sdvar.h>	/* XXX */
#endif

#ifdef UMASS_DEBUG
#define DIF(m, x)	if (umassdebug & (m)) do { x ; } while (0)
#define DPRINTF(m, x)	if (umassdebug & (m)) logprintf x
#define UDMASS_UPPER	0x00008000	/* upper layer */
#define UDMASS_GEN	0x00010000	/* general */
#define UDMASS_SCSI	0x00020000	/* scsi */
#define UDMASS_UFI	0x00040000	/* ufi command set */
#define UDMASS_8070	0x00080000	/* 8070i command set */
#define UDMASS_USB	0x00100000	/* USB general */
#define UDMASS_BBB	0x00200000	/* Bulk-Only transfers */
#define UDMASS_CBI	0x00400000	/* CBI transfers */
#define UDMASS_ALL	0xffff0000	/* all of the above */

#define UDMASS_XFER	0x40000000	/* all transfers */
#define UDMASS_CMD	0x80000000

int umassdebug = 0; //UDMASS_ALL;
#else
#define DIF(m, x)	/* nop */
#define DPRINTF(m, x)	/* nop */
#endif


/* Generic definitions */

#define UFI_COMMAND_LENGTH 12

/* Direction for umass_*_transfer */
#define DIR_NONE	0
#define DIR_IN		1
#define DIR_OUT		2

/* The transfer speed determines the timeout value */
#define UMASS_DEFAULT_TRANSFER_SPEED	150	/* in kb/s, conservative est. */
#define UMASS_FLOPPY_TRANSFER_SPEED	20
#define UMASS_ZIP100_TRANSFER_SPEED	650

#define UMASS_SPINUP_TIME 10000	/* ms */

#ifdef __FreeBSD__
/* device name */
#define DEVNAME		"umass"
#define DEVNAME_SIM	"umass-"

#define UMASS_MAX_TRANSFER_SIZE		65536

/* CAM specific definitions */

/* The bus id, whatever that is */
#define UMASS_SCSI_BUS		0

/* All USB drives are 'connected' to one SIM (SCSI controller). umass3
 * ends up being target 3 on that SIM. When a request for target 3
 * comes in we fetch the softc with devclass_get_softc(target_id).
 *
 * The SIM is the highest target number. This makes sure that umass0 corresponds
 * to target 0 on the USB SCSI bus.
 */
#ifndef UMASS_DEBUG
#define UMASS_SCSIID_MAX	32	/* maximum number of drives expected */
#else
/* while debugging avoid unnecessary clutter in the output at umass_cam_rescan
 * (XPT_PATH_INQ)
 */
#define UMASS_SCSIID_MAX	3	/* maximum number of drives expected */
#endif
#define UMASS_SCSIID_HOST	UMASS_SCSIID_MAX
#endif

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)			      


/* Bulk-Only features */

#define UR_BBB_RESET	0xff		/* Bulk-Only reset */
#define	UR_BBB_GET_MAX_LUN	0xfe

/* Command Block Wrapper */
typedef struct {
	uDWord		dCBWSignature;
#	define CBWSIGNATURE	0x43425355
	uDWord		dCBWTag;
	uDWord		dCBWDataTransferLength;
	uByte		bCBWFlags;
#	define CBWFLAGS_OUT	0x00
#	define CBWFLAGS_IN	0x80
	uByte		bCBWLUN;
	uByte		bCDBLength;
#	define CBWCDBLENGTH	16
	uByte		CBWCDB[CBWCDBLENGTH];
} umass_bbb_cbw_t;
#define UMASS_BBB_CBW_SIZE	31

/* Command Status Wrapper */
typedef struct {
	uDWord		dCSWSignature;
#	define CSWSIGNATURE	0x53425355
	uDWord		dCSWTag;
	uDWord		dCSWDataResidue;
	uByte		bCSWStatus;
#	define CSWSTATUS_GOOD	0x0
#	define CSWSTATUS_FAILED 0x1
#	define CSWSTATUS_PHASE	0x2
} umass_bbb_csw_t;
#define UMASS_BBB_CSW_SIZE	13

/* CBI features */

#define UR_CBI_ADSC	0x00

typedef unsigned char umass_cbi_cbl_t[16];	/* Command block */

typedef union {
	struct {
		unsigned char	type;
		#define IDB_TYPE_CCI		0x00
		unsigned char	value;
		#define IDB_VALUE_PASS		0x00
		#define IDB_VALUE_FAIL		0x01
		#define IDB_VALUE_PHASE		0x02
		#define IDB_VALUE_PERSISTENT	0x03
		#define IDB_VALUE_STATUS_MASK	0x03
	} common;

	struct {
		unsigned char	asc;
		unsigned char	ascq;
	} ufi;
} umass_cbi_sbl_t;



struct umass_softc;		/* see below */

typedef void (*transfer_cb_f)(struct umass_softc *sc, void *priv,
			      int residue, int status);
#define STATUS_CMD_OK		0	/* everything ok */
#define STATUS_CMD_UNKNOWN	1	/* will have to fetch sense */
#define STATUS_CMD_FAILED	2	/* transfer was ok, command failed */
#define STATUS_WIRE_FAILED	3	/* couldn't even get command across */

typedef void (*wire_reset_f)(struct umass_softc *sc, int status);
typedef void (*wire_transfer_f)(struct umass_softc *sc, int lun,
				void *cmd, int cmdlen, void *data, int datalen, 
				int dir, transfer_cb_f cb, void *priv);
typedef void (*wire_state_f)(usbd_xfer_handle xfer,
			     usbd_private_handle priv, usbd_status err);

#if defined(__FreeBSD__)
typedef int (*command_transform_f)(struct umass_softc *sc,
				   u_char *cmd, int cmdlen,
				   u_char **rcmd, int *rcmdlen));
#endif


/* the per device structure */
struct umass_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* device */

	unsigned char		drive;
#	define DRIVE_GENERIC		0	/* use defaults for this one */
#	define ZIP_100			1	/* to be used for quirks */
#	define SHUTTLE_EUSB		2

	unsigned char		quirks;
	/* The drive does not support Test Unit Ready. Convert to
	 * Start Unit.
	 * Y-E Data
	 * ZIP 100
	 */
#	define NO_TEST_UNIT_READY	0x01
	/* The drive does not reset the Unit Attention state after
	 * REQUEST SENSE has been sent. The INQUIRY command does not reset
	 * the UA either, and so CAM runs in circles trying to retrieve the
	 * initial INQUIRY data.
	 * Y-E Data
	 */
#	define RS_NO_CLEAR_UA		0x02	/* no REQUEST SENSE on INQUIRY*/
	/* The drive does not support START_STOP.
	 * Shuttle E-USB
	 */
#	define NO_START_STOP		0x04

	unsigned int		proto;
#	define PROTO_UNKNOWN	0x0000		/* unknown protocol */
#	define PROTO_BBB	0x0001		/* USB wire protocol */
#	define PROTO_CBI	0x0002
#	define PROTO_CBI_I	0x0004
#	define PROTO_WIRE	0x00ff		/* USB wire protocol mask */
#	define PROTO_SCSI	0x0100		/* command protocol */
#	define PROTO_ATAPI	0x0200
#	define PROTO_UFI	0x0400
#	define PROTO_COMMAND	0xff00		/* command protocol mask */

	usbd_interface_handle	iface;		/* Mass Storage interface */
	int			ifaceno;	/* MS iface number */

	u_int8_t		bulkin;		/* bulk-in Endpoint Address */
	u_int8_t		bulkout;	/* bulk-out Endpoint Address */
	u_int8_t		intrin;		/* intr-in Endp. (CBI) */
	usbd_pipe_handle	bulkin_pipe;
	usbd_pipe_handle	bulkout_pipe;
	usbd_pipe_handle	intrin_pipe;

	/* Reset the device in a wire protocol specific way */
	wire_reset_f		reset;

	/* The start of a wire transfer. It prepares the whole transfer (cmd,
	 * data, and status stage) and initiates it. It is up to the state
	 * machine (below) to handle the various stages and errors in these
	 */
	wire_transfer_f		transfer;

	/* The state machine, handling the various states during a transfer */
	wire_state_f		state;

#if defined(__FreeBSD__)
	/* The command transform function is used to conver the SCSI commands
	 * into their derivatives, like UFI, ATAPI, and friends.
	 */
	command_transform_f	transform;	/* command transform */
#endif
	
	/* Bulk specific variables for transfers in progress */
	umass_bbb_cbw_t		cbw;	/* command block wrapper */
	umass_bbb_csw_t		csw;	/* command status wrapper*/
	/* CBI specific variables for transfers in progress */
	umass_cbi_cbl_t		cbl;	/* command block */ 
	umass_cbi_sbl_t		sbl;	/* status block */

	/* generic variables for transfers in progress */
	/* ctrl transfer requests */
	usb_device_request_t	request;

	/* xfer handles
	 * Most of our operations are initiated from interrupt context, so
	 * we need to avoid using the one that is in use. We want to avoid
	 * allocating them in the interrupt context as well.
	 */
	/* indices into array below */
#	define XFER_BBB_CBW		0	/* Bulk-Only */
#	define XFER_BBB_DATA		1
#	define XFER_BBB_DCLEAR		2
#	define XFER_BBB_CSW1		3
#	define XFER_BBB_CSW2		4
#	define XFER_BBB_SCLEAR		5
#	define XFER_BBB_RESET1		6
#	define XFER_BBB_RESET2		7
#	define XFER_BBB_RESET3		8
	
#	define XFER_CBI_CB		0	/* CBI */
#	define XFER_CBI_DATA		1
#	define XFER_CBI_STATUS		2
#	define XFER_CBI_DCLEAR		3
#	define XFER_CBI_SCLEAR		4
#	define XFER_CBI_RESET1		5
#	define XFER_CBI_RESET2		6
#	define XFER_CBI_RESET3		7

#	define XFER_NR			9	/* maximum number */

	usbd_xfer_handle	transfer_xfer[XFER_NR]; /* for ctrl xfers */

	void			*data_buffer;

	int			transfer_dir;		/* data direction */
	void			*transfer_data;		/* data buffer */
	int			transfer_datalen;	/* (maximum) length */
	int			transfer_actlen;	/* actual length */ 
	transfer_cb_f		transfer_cb;		/* callback */
	void			*transfer_priv;		/* for callback */
	int			transfer_status;

	int			transfer_state;
#	define TSTATE_IDLE			0
#	define TSTATE_BBB_COMMAND		1	/* CBW transfer */
#	define TSTATE_BBB_DATA			2	/* Data transfer */
#	define TSTATE_BBB_DCLEAR		3	/* clear endpt stall */
#	define TSTATE_BBB_STATUS1		4	/* clear endpt stall */
#	define TSTATE_BBB_SCLEAR		5	/* clear endpt stall */
#	define TSTATE_BBB_STATUS2		6	/* CSW transfer */
#	define TSTATE_BBB_RESET1		7	/* reset command */
#	define TSTATE_BBB_RESET2		8	/* in clear stall */
#	define TSTATE_BBB_RESET3		9	/* out clear stall */
#	define TSTATE_CBI_COMMAND		10	/* command transfer */
#	define TSTATE_CBI_DATA			11	/* data transfer */
#	define TSTATE_CBI_STATUS		12	/* status transfer */
#	define TSTATE_CBI_DCLEAR		13	/* clear ep stall */
#	define TSTATE_CBI_SCLEAR		14	/* clear ep stall */
#	define TSTATE_CBI_RESET1		15	/* reset command */
#	define TSTATE_CBI_RESET2		16	/* in clear stall */
#	define TSTATE_CBI_RESET3		17	/* out clear stall */
#	define TSTATE_STATES			18	/* # of states above */


	int			transfer_speed;		/* in kb/s */
	int			timeout;		/* in msecs */

	u_int8_t		maxlun;			/* max lun supported */

#if defined(__FreeBSD__)
	/* SCSI/CAM specific variables */
	struct scsi_sense	cam_scsi_sense;

#elif defined(__NetBSD__) || defined(__OpenBSD__)
	union {
		struct scsipi_link	sc_link;
		struct {
			struct ata_atapi_attach	sc_aa;
			struct ata_drive_datas	sc_aa_drive;
		} aa;
	} u;
	struct atapi_adapter	sc_atapi_adapter;
#define sc_adapter sc_atapi_adapter._generic
	int			sc_xfer_flags;
	usbd_status		sc_sync_status;
	struct scsipi_sense	sc_sense_cmd;

	device_ptr_t		sc_child;	/* child device, for detach */
	char			sc_dying;

#endif
};

#ifdef UMASS_DEBUG
char *states[TSTATE_STATES+1] = {
	/* should be kept in sync with the list at transfer_state */
	"Idle",
	"BBB CBW",
	"BBB Data",
	"BBB Data bulk-in/-out clear stall",
	"BBB CSW, 1st attempt",
	"BBB CSW bulk-in clear stall",
	"BBB CSW, 2nd attempt",
	"BBB Reset",
	"BBB bulk-in clear stall",
	"BBB bulk-out clear stall",
	"CBI Command",
	"CBI Data",
	"CBI Status",
	"CBI Data bulk-in/-out clear stall",
	"CBI Status intr-in clear stall",
	"CBI Reset",
	"CBI bulk-in clear stall",
	"CBI bulk-out clear stall",
	NULL
};
#endif

struct cam_sim *umass_sim;	/* SCSI Interface Module */
struct cam_path *umass_path;	/*   and its path */


/* USB device probe/attach/detach functions */
USB_DECLARE_DRIVER(umass);
Static void umass_disco(struct umass_softc *sc);
Static int umass_match_proto(struct umass_softc *sc,
			     usbd_interface_handle iface,
			     usbd_device_handle dev);
Static void umass_init_shuttle(struct umass_softc *sc);

/* generic transfer functions */
Static usbd_status umass_setup_transfer(struct umass_softc *sc,
				usbd_pipe_handle pipe,
				void *buffer, int buflen, int flags,
				usbd_xfer_handle xfer);
Static usbd_status umass_setup_ctrl_transfer(struct umass_softc *sc,
				usbd_device_handle dev,
				usb_device_request_t *req,
				void *buffer, int buflen, int flags, 
				usbd_xfer_handle xfer);
Static void umass_clear_endpoint_stall(struct umass_softc *sc,
				u_int8_t endpt, usbd_pipe_handle pipe,
				int state, usbd_xfer_handle xfer);
#if 0
Static void umass_reset(struct umass_softc *sc,	transfer_cb_f cb, void *priv);
#endif

/* Bulk-Only related functions */
Static void umass_bbb_reset(struct umass_softc *sc, int status);
Static void umass_bbb_transfer(struct umass_softc *sc, int lun,
				void *cmd, int cmdlen,
				void *data, int datalen, int dir,
				transfer_cb_f cb, void *priv);
Static void umass_bbb_state(usbd_xfer_handle xfer,
				usbd_private_handle priv,
				usbd_status err);
usbd_status umass_bbb_get_max_lun(struct umass_softc *sc, u_int8_t *maxlun);


/* CBI related functions */
Static int umass_cbi_adsc(struct umass_softc *sc, char *buffer,int buflen,
				usbd_xfer_handle xfer);
Static void umass_cbi_reset(struct umass_softc *sc, int status);
Static void umass_cbi_transfer(struct umass_softc *sc, int lun,
				void *cmd, int cmdlen,
				void *data, int datalen, int dir,
				transfer_cb_f cb, void *priv);
Static void umass_cbi_state(usbd_xfer_handle xfer,
				usbd_private_handle priv, usbd_status err);

#if defined(__FreeBSD__)
/* CAM related functions */
Static void umass_cam_action(struct cam_sim *sim, union ccb *ccb);
Static void umass_cam_poll(struct cam_sim *sim);

Static void umass_cam_cb(struct umass_softc *sc, void *priv,
				int residue, int status);
Static void umass_cam_sense_cb(struct umass_softc *sc, void *priv,
				int residue, int status);

#ifdef UMASS_DO_CAM_RESCAN
Static void umass_cam_rescan(struct umass_softc *sc);
#endif

Static int umass_cam_attach_sim(void);
Static int umass_cam_attach(struct umass_softc *sc);
Static int umass_cam_detach_sim(void);
Static int umass_cam_detach(struct umass_softc *sc);

#elif defined(__NetBSD__) || defined(__OpenBSD__)

#define UMASS_SCSIID_HOST	0x00
#define UMASS_SCSIID_DEVICE	0x01

#define UMASS_MAX_TRANSFER_SIZE	MAXBSIZE

struct scsipi_device umass_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

Static int umass_scsipi_cmd(struct scsipi_xfer *xs);
Static void umass_scsipi_minphys(struct buf *bp);
Static int umass_scsipi_ioctl(struct scsipi_link *, u_long,
				   caddr_t, int, struct proc *);
Static int umass_scsipi_getgeom(struct scsipi_link *link,
			      struct disk_parms *, u_long sectors);

Static void umass_scsipi_cb(struct umass_softc *sc, void *priv,
				     int residue, int status);
Static void umass_scsipi_sense_cb(struct umass_softc *sc, void *priv,
				       int residue, int status);

Static int scsipiprint(void *aux, const char *pnp);
Static int umass_ufi_transform(struct umass_softc *sc,
			       struct scsipi_generic *cmd, int cmdlen,
			       struct scsipi_generic *rcmd, int *rcmdlen);
#if NATAPIBUS > 0
Static void umass_atapi_probedev(struct atapibus_softc *, int);
#endif
#endif

#if defined(__FreeBSD__)
/* SCSI specific functions */
Static int umass_scsi_transform(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
				unsigned char **rcmd, int *rcmdlen);

/* UFI specific functions */
Static int umass_ufi_transform(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
				unsigned char **rcmd, int *rcmdlen);

/* 8070 specific functions */
Static int umass_8070_transform(struct umass_softc *sc,
				unsigned char *cmd, int cmdlen,
				unsigned char **rcmd, int *rcmdlen);
#endif

#ifdef UMASS_DEBUG
/* General debugging functions */
Static void umass_bbb_dump_cbw(struct umass_softc *sc,
				umass_bbb_cbw_t *cbw);
Static void umass_bbb_dump_csw(struct umass_softc *sc,
				umass_bbb_csw_t *csw);
Static void umass_dump_buffer(struct umass_softc *sc, u_int8_t *buffer,
				int buflen, int printlen);
#endif


void usbd_clear_endpoint_toggle(usbd_pipe_handle pipe);	/* XXXXX */

/*
 * USB device probe/attach/detach
 */

/*
 * Match the device we are seeing with the devices supported. Fill in the
 * proto and drive fields in the softc accordingly.
 * This function is called from both probe and attach.
 */

Static int
umass_match_proto(struct umass_softc *sc, usbd_interface_handle iface,
		  usbd_device_handle dev)
{
	usb_device_descriptor_t *dd;
	usb_interface_descriptor_t *id;

	/*
	 * Fill in sc->drive and sc->proto and return a match
	 * value if both are determined and 0 otherwise.
	 */

	sc->drive = DRIVE_GENERIC;
	sc->proto = PROTO_UNKNOWN;
	sc->transfer_speed = UMASS_DEFAULT_TRANSFER_SPEED;

	sc->sc_udev = dev;
	dd = usbd_get_device_descriptor(dev);

	if (UGETW(dd->idVendor) == USB_VENDOR_SHUTTLE
	    && UGETW(dd->idProduct) == USB_PRODUCT_SHUTTLE_EUSB) {
		sc->drive = SHUTTLE_EUSB;
#if CBI_I
		sc->proto = PROTO_ATAPI | PROTO_CBI_I;
#else
		sc->proto = PROTO_ATAPI | PROTO_CBI;
#endif
		sc->quirks |= NO_TEST_UNIT_READY | NO_START_STOP;
		return (UMATCH_VENDOR_PRODUCT);
	}

	if (UGETW(dd->idVendor) == USB_VENDOR_YEDATA
	    && UGETW(dd->idProduct) == USB_PRODUCT_YEDATA_FLASHBUSTERU) {

		/* Revisions < 1.28 do not handle the interrupt endpoint
		 * very well.
		 */
		if (UGETW(dd->bcdDevice) < 0x128)
			sc->proto = PROTO_UFI | PROTO_CBI;
		else
#if CBI_I
			sc->proto = PROTO_UFI | PROTO_CBI_I;
#else
			sc->proto = PROTO_UFI | PROTO_CBI;
#endif
		/*
		 * Revisions < 1.28 do not have the TEST UNIT READY command
		 * Revisions == 1.28 have a broken TEST UNIT READY
		 */
		if (UGETW(dd->bcdDevice) <= 0x128)
			sc->quirks |= NO_TEST_UNIT_READY;

		sc->quirks |= RS_NO_CLEAR_UA;
		sc->transfer_speed = UMASS_FLOPPY_TRANSFER_SPEED;
		return (UMATCH_VENDOR_PRODUCT_REV);
	}


	id = usbd_get_interface_descriptor(iface);
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return (UMATCH_NONE);

	switch (id->bInterfaceSubClass) {
	case UISUBCLASS_SCSI:
		sc->proto |= PROTO_SCSI;
		break;
	case UISUBCLASS_UFI:
		sc->transfer_speed = UMASS_FLOPPY_TRANSFER_SPEED;
		sc->proto |= PROTO_UFI;
		break;
	case UISUBCLASS_SFF8020I:
	case UISUBCLASS_SFF8070I:
	case UISUBCLASS_QIC157:
		sc->proto |= PROTO_ATAPI;
		break;
	default:
		DPRINTF(UDMASS_GEN, ("%s: Unsupported command protocol %d\n",
			USBDEVNAME(sc->sc_dev), id->bInterfaceSubClass));
		return (UMATCH_NONE);
	}

	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_CBI:
		sc->proto |= PROTO_CBI;
		break;
	case UIPROTO_MASS_CBI_I:
#if CBI_I
		sc->proto |= PROTO_CBI_I;
#else
		sc->proto |= PROTO_CBI;
#endif
		break;
	case UIPROTO_MASS_BBB:
		sc->proto |= PROTO_BBB;
		break;
	case UIPROTO_MASS_BBB_P:
		sc->drive = ZIP_100;
		sc->proto |= PROTO_BBB;
		sc->transfer_speed = UMASS_ZIP100_TRANSFER_SPEED;
		sc->quirks |= NO_TEST_UNIT_READY;
		break;
	default:
		DPRINTF(UDMASS_GEN, ("%s: Unsupported wire protocol %d\n",
			USBDEVNAME(sc->sc_dev), id->bInterfaceProtocol));
		return (UMATCH_NONE);
	}

	return (UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO);
}

USB_MATCH(umass)
{
	USB_MATCH_START(umass, uaa);
#if defined(__FreeBSD__)
	struct umass_softc *sc = device_get_softc(self);
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct umass_softc scs, *sc = &scs;
	memset(sc, 0, sizeof *sc);
#endif

	if (uaa->iface == NULL)
		return(UMATCH_NONE);

	return (umass_match_proto(sc, uaa->iface, uaa->device));
}

USB_ATTACH(umass)
{
	USB_ATTACH_START(umass, sc, uaa);
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	const char *sSubclass, *sProto;
	char devinfo[1024];
	int i, bno;
	int err;

	/*
	 * the softc struct is bzero-ed in device_set_driver. We can safely
	 * call umass_detach without specifically initialising the struct.
	 */

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

	sc->iface = uaa->iface;
	sc->ifaceno = uaa->ifaceno;

	/* initialise the proto and drive values in the umass_softc (again) */
	(void) umass_match_proto(sc, sc->iface, uaa->device);

	/*
	 * The timeout is based on the maximum expected transfer size
	 * divided by the expected transfer speed.
	 * We multiply by 4 to make sure a busy system doesn't make things
	 * fail.
	 */
	sc->timeout = 4 * UMASS_MAX_TRANSFER_SIZE / sc->transfer_speed;
	sc->timeout += UMASS_SPINUP_TIME;	/* allow for spinning up */

	id = usbd_get_interface_descriptor(sc->iface);
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	switch (id->bInterfaceSubClass) {
	case UISUBCLASS_SCSI:
		sSubclass = "SCSI";
		break;
	case UISUBCLASS_UFI:
		sSubclass = "UFI";
		break;
	case UISUBCLASS_SFF8020I:
		sSubclass = "SFF8020i";
		break;
	case UISUBCLASS_SFF8070I:
		sSubclass = "SFF8070i";
		break;
	case UISUBCLASS_QIC157:
		sSubclass = "QIC157";
		break;
	default:
		sSubclass = "unknown";
		break;
	}
	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_CBI:
		sProto = "CBI";
		break;
	case UIPROTO_MASS_CBI_I:
		sProto = "CBI-I";
		break;
	case UIPROTO_MASS_BBB:
		sProto = "BBB";
		break;
	case UIPROTO_MASS_BBB_P:
		sProto = "BBB-P";
		break;
	default:
		sProto = "unknown";
		break;
	}
	printf("%s: using %s over %s\n", USBDEVNAME(sc->sc_dev), sSubclass, 
	       sProto);

	/*
	 * In addition to the Control endpoint the following endpoints
	 * are required:
	 * a) bulk-in endpoint.
	 * b) bulk-out endpoint.
	 * and for Control/Bulk/Interrupt with CCI (CBI_I)
	 * c) intr-in
	 *
	 * The endpoint addresses are not fixed, so we have to read them
	 * from the device descriptors of the current interface.
	 */
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->iface, i);
		if (!ed) {
			printf("%s: could not read endpoint descriptor\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->bulkin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->bulkout = ed->bEndpointAddress;
		} else if (sc->proto & PROTO_CBI_I
		    && UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->intrin = ed->bEndpointAddress;
#ifdef UMASS_DEBUG
			if (UGETW(ed->wMaxPacketSize) > 2) {
				DPRINTF(UDMASS_CBI, ("%s: intr size is %d\n",
					USBDEVNAME(sc->sc_dev),
					UGETW(ed->wMaxPacketSize)));
			}
#endif
		}
	}

	/* check whether we found all the endpoints we need */
	if (!sc->bulkin || !sc->bulkout
	    || (sc->proto & PROTO_CBI_I && !sc->intrin) ) {
		DPRINTF(UDMASS_USB, ("%s: endpoint not found %d/%d/%d\n",
			USBDEVNAME(sc->sc_dev),
			sc->bulkin, sc->bulkout, sc->intrin));
		umass_disco(sc);
		USB_ATTACH_ERROR_RETURN;
	}

	/*
	 * Get the maximum LUN supported by the device.
	 */
	if ((sc->proto & PROTO_WIRE) == PROTO_BBB) {
		err = umass_bbb_get_max_lun(sc, &sc->maxlun);
		if (err) {
			printf("%s: unable to get Max Lun: %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			USB_ATTACH_ERROR_RETURN;
		}
	} else {
		sc->maxlun = 0;
	}

	/* Open the bulk-in and -out pipe */
	err = usbd_open_pipe(sc->iface, sc->bulkout,
				USBD_EXCLUSIVE_USE, &sc->bulkout_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("%s: cannot open %d-out pipe (bulk)\n",
			USBDEVNAME(sc->sc_dev), sc->bulkout));
		umass_disco(sc);
		USB_ATTACH_ERROR_RETURN;
	}
	err = usbd_open_pipe(sc->iface, sc->bulkin,
				USBD_EXCLUSIVE_USE, &sc->bulkin_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("%s: could not open %d-in pipe (bulk)\n",
			USBDEVNAME(sc->sc_dev), sc->bulkin));
		umass_disco(sc);
		USB_ATTACH_ERROR_RETURN;
	}
	/* 
	 * Open the intr-in pipe if the protocol is CBI with CCI.
	 * Note: early versions of the Zip drive do have an interrupt pipe, but
	 * this pipe is unused
	 *
	 * We do not open the interrupt pipe as an interrupt pipe, but as a
	 * normal bulk endpoint. We send an IN transfer down the wire at the
	 * appropriate time, because we know exactly when to expect data on
	 * that endpoint. This saves bandwidth, but more important, makes the
	 * code for handling the data on that endpoint simpler. No data
	 * arriving concurrently.
	 */
	if (sc->proto & PROTO_CBI_I) {
		err = usbd_open_pipe(sc->iface, sc->intrin,
				USBD_EXCLUSIVE_USE, &sc->intrin_pipe);
		if (err) {
			DPRINTF(UDMASS_USB, ("%s: couldn't open %d-in (intr)\n",
				USBDEVNAME(sc->sc_dev), sc->intrin));
			umass_disco(sc);
			USB_ATTACH_ERROR_RETURN;
		}
	}

	/* initialisation of generic part */
	sc->transfer_state = TSTATE_IDLE;

	/* request a sufficient number of xfer handles */
	for (i = 0; i < XFER_NR; i++) {
		sc->transfer_xfer[i] = usbd_alloc_xfer(uaa->device);
		if (sc->transfer_xfer[i] == 0) {
			DPRINTF(UDMASS_USB, ("%s: Out of memory\n",
				USBDEVNAME(sc->sc_dev)));
			umass_disco(sc);
			USB_ATTACH_ERROR_RETURN;
		}
	}
	/* Allocate buffer for data transfer (it's huge). */
	switch (sc->proto & PROTO_WIRE) {
	case PROTO_BBB:
		bno = XFER_BBB_DATA;
		goto dalloc;
	case PROTO_CBI:
		bno = XFER_CBI_DATA;
		goto dalloc;
	case PROTO_CBI_I:
		bno = XFER_CBI_DATA;
	dalloc:
		sc->data_buffer = usbd_alloc_buffer(sc->transfer_xfer[bno], 
						    UMASS_MAX_TRANSFER_SIZE);
		if (sc->data_buffer == NULL) {
			umass_disco(sc);
			USB_ATTACH_ERROR_RETURN;
		}
		break;
	default:
		break;
	}

	/* Initialise the wire protocol specific methods */
	if (sc->proto & PROTO_BBB) {
		sc->reset = umass_bbb_reset;
		sc->transfer = umass_bbb_transfer;
		sc->state = umass_bbb_state;
	} else if ((sc->proto & PROTO_CBI) || (sc->proto & PROTO_CBI_I)) {
		sc->reset = umass_cbi_reset;
		sc->transfer = umass_cbi_transfer;
		sc->state = umass_cbi_state;
#ifdef UMASS_DEBUG
	} else {
		panic("%s:%d: Unknown proto 0x%02x\n",
		      __FILE__, __LINE__, sc->proto);
#endif
	}

	if (sc->drive == SHUTTLE_EUSB)
		umass_init_shuttle(sc);

#if defined(__FreeBSD__)
	if (sc->proto & PROTO_SCSI)
		sc->transform = umass_scsi_transform;
	else if (sc->proto & PROTO_UFI)
		sc->transform = umass_ufi_transform;
	else if (sc->proto & PROTO_ATAPI)
		sc->transform = umass_8070_transform;
#ifdef UMASS_DEBUG
	else
		panic("No transformation defined for command proto 0x%02x\n",
		      sc->proto & PROTO_COMMAND);
#endif

	/* From here onwards the device can be used. */

	if ((sc->proto & PROTO_SCSI) ||
	    (sc->proto & PROTO_ATAPI) ||
	    (sc->proto & PROTO_UFI)) {
		/* Prepare the SCSI command block */
		sc->cam_scsi_sense.opcode = REQUEST_SENSE;

		/* If this is the first device register the SIM */
		if (umass_sim == NULL) {
			err = umass_cam_attach_sim();
			if (err) {
				umass_disco(self);
				USB_ATTACH_ERROR_RETURN;
			}
		}

		/* Attach the new device to our SCSI host controller (SIM) */
		err = umass_cam_attach(sc);
		if (err) {
			umass_disco(self);
			USB_ATTACH_ERROR_RETURN;
		}
	} else {
		panic("%s:%d: Unknown proto 0x%02x\n",
		      __FILE__, __LINE__, sc->proto);
	}
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	/*
	 * Fill in the adapter.
	 */
	sc->sc_adapter.scsipi_cmd = umass_scsipi_cmd;
	sc->sc_adapter.scsipi_minphys = umass_scsipi_minphys;
	sc->sc_adapter.scsipi_ioctl = umass_scsipi_ioctl;
	sc->sc_adapter.scsipi_getgeom = umass_scsipi_getgeom;
	
	/*
	 * fill in the prototype scsipi_link.
	 */
	switch (sc->proto & PROTO_COMMAND) {
	case PROTO_UFI:
		sc->u.sc_link.quirks |= SDEV_ONLYBIG;
		/* fall into */
	case PROTO_SCSI:
		sc->u.sc_link.type = BUS_SCSI;
		sc->u.sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
		sc->u.sc_link.adapter_softc = sc;
		sc->u.sc_link.scsipi_scsi.adapter_target = UMASS_SCSIID_HOST;
		sc->u.sc_link.adapter = &sc->sc_adapter;
		sc->u.sc_link.device = &umass_dev;
		sc->u.sc_link.openings = 1;
		sc->u.sc_link.scsipi_scsi.max_target = UMASS_SCSIID_DEVICE;
		sc->u.sc_link.scsipi_scsi.max_lun = sc->maxlun;

		if (sc->quirks & NO_TEST_UNIT_READY)
			sc->u.sc_link.quirks |= ADEV_NOTUR;
		break;

#if NATAPIBUS > 0
	case PROTO_ATAPI:
		sc->u.aa.sc_aa.aa_type = T_ATAPI;
		sc->u.aa.sc_aa.aa_channel = 0;
		sc->u.aa.sc_aa.aa_openings = 1;
		sc->u.aa.sc_aa.aa_drv_data = &sc->u.aa.sc_aa_drive;
		sc->u.aa.sc_aa.aa_bus_private = &sc->sc_atapi_adapter;
		sc->sc_atapi_adapter.atapi_probedev = umass_atapi_probedev;
		sc->sc_atapi_adapter.atapi_kill_pending = scsi_kill_pending;
		break;
#endif

	default:
		printf("%s: proto=0x%x not supported yet\n", 
		       USBDEVNAME(sc->sc_dev), sc->proto);
		umass_disco(sc);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_child = config_found(&sc->sc_dev, &sc->u, scsipiprint);
	if (sc->sc_child == NULL) {
		umass_disco(sc);
		/* Not an error, just not a complete success. */
		USB_ATTACH_SUCCESS_RETURN;
	}
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	DPRINTF(UDMASS_GEN, ("%s: Attach finished\n", USBDEVNAME(sc->sc_dev)));

	USB_ATTACH_SUCCESS_RETURN;
}

Static int
scsipiprint(void *aux, const char *pnp)
{
	struct scsipi_link *l = aux;

	if (l->type == BUS_SCSI)
		return (scsiprint(aux, pnp));
	else {
#if NATAPIBUS > 0
		extern int atapi_print(void *aux, const char *pnp);
		return (atapi_print(aux, pnp));
#else
		if (pnp)
			printf("atapibus at %s", pnp);
		return (UNCONF);
#endif
	}
}

USB_DETACH(umass)
{
	USB_DETACH_START(umass, sc);
	int rv = 0;

	DPRINTF(UDMASS_USB, ("%s: detached\n", USBDEVNAME(sc->sc_dev)));

	/* Abort the pipes to wake up any waiting processes. */
	if (sc->bulkout_pipe != NULL)
		usbd_abort_pipe(sc->bulkout_pipe);
	if (sc->bulkin_pipe != NULL)
		usbd_abort_pipe(sc->bulkin_pipe);
	if (sc->intrin_pipe != NULL)
		usbd_abort_pipe(sc->intrin_pipe);

#if 0
	/* Do we really need referebce counting?  Perhaps in ioctl() */
	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);
#endif

#if defined(__FreeBSD__)
	if ((sc->proto & PROTO_SCSI) ||
	    (sc->proto & PROTO_ATAPI) ||
	    (sc->proto & PROTO_UFI))
		/* detach the device from the SCSI host controller (SIM) */
		rv = umass_cam_detach(sc);
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);
#endif
	if (rv != 0)
		return (rv);

	umass_disco(sc);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
umass_activate(struct device *self, enum devact act)
{
	struct umass_softc *sc = (struct umass_softc *) self;
	int rv = 0;

	DPRINTF(UDMASS_USB, ("%s: umass_activate: %d\n",
	    USBDEVNAME(sc->sc_dev), act));

	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_child == NULL)
			break;
		rv = config_deactivate(sc->sc_child);
		DPRINTF(UDMASS_USB, ("%s: umass_activate: child "
		    "returned %d\n", USBDEVNAME(sc->sc_dev), rv));
		if (rv == 0)
			sc->sc_dying = 1;
		break;
	}
	return (rv);
}
#endif

Static void
umass_disco(struct umass_softc *sc)
{ 
	int i;

	DPRINTF(UDMASS_GEN, ("umass_disco\n"));

	/* Free the xfers. */
	for (i = 0; i < XFER_NR; i++)
		if (sc->transfer_xfer[i] != NULL) {
			usbd_free_xfer(sc->transfer_xfer[i]);
			sc->transfer_xfer[i] = NULL;
		}

	/* Remove all the pipes. */
	if (sc->bulkout_pipe != NULL)
		usbd_close_pipe(sc->bulkout_pipe);
	if (sc->bulkin_pipe != NULL)
		usbd_close_pipe(sc->bulkin_pipe);
	if (sc->intrin_pipe != NULL)
		usbd_close_pipe(sc->intrin_pipe);
}

Static void
umass_init_shuttle(struct umass_softc *sc)
{
	usb_device_request_t req;
	u_char status[2];

	/* The Linux driver does this */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->ifaceno);
	USETW(req.wLength, sizeof status);
	(void)usbd_do_request(sc->sc_udev, &req, &status);
}

/*
 * Generic functions to handle transfers
 */

Static usbd_status
umass_setup_transfer(struct umass_softc *sc, usbd_pipe_handle pipe,
			void *buffer, int buflen, int flags,
			usbd_xfer_handle xfer)
{
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	/* Initialiase a USB transfer and then schedule it */

	usbd_setup_xfer(xfer, pipe, (void *)sc, buffer, buflen,
	    flags | sc->sc_xfer_flags, sc->timeout, sc->state);

	err = usbd_transfer(xfer);
	DPRINTF(UDMASS_XFER,("%s: start xfer buffer=%p buflen=%d flags=0x%x "
	    "timeout=%d\n", USBDEVNAME(sc->sc_dev),
	    buffer, buflen, flags | sc->sc_xfer_flags, sc->timeout));
	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup transfer, %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}


Static usbd_status
umass_setup_ctrl_transfer(struct umass_softc *sc, usbd_device_handle dev,
	 usb_device_request_t *req,
	 void *buffer, int buflen, int flags,
	 usbd_xfer_handle xfer)
{
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	/* Initialiase a USB control transfer and then schedule it */

	usbd_setup_default_xfer(xfer, dev, (void *) sc,
	    sc->timeout, req, buffer, buflen, flags, sc->state);

	err = usbd_transfer(xfer);
	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup ctrl transfer, %s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(err)));

		/* do not reset, as this would make us loop */
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static void
umass_clear_endpoint_stall(struct umass_softc *sc,
	u_int8_t endpt, usbd_pipe_handle pipe,
	int state, usbd_xfer_handle xfer)
{
	usbd_device_handle dev;

	if (sc->sc_dying)
		return;

	DPRINTF(UDMASS_BBB, ("%s: Clear endpoint 0x%02x stall\n",
		USBDEVNAME(sc->sc_dev), endpt));

	usbd_interface2device_handle(sc->iface, &dev);

	sc->transfer_state = state;

	usbd_clear_endpoint_toggle(pipe);

	sc->request.bmRequestType = UT_WRITE_ENDPOINT;
	sc->request.bRequest = UR_CLEAR_FEATURE;
	USETW(sc->request.wValue, UF_ENDPOINT_HALT);
	USETW(sc->request.wIndex, endpt);
	USETW(sc->request.wLength, 0);
	umass_setup_ctrl_transfer(sc, dev, &sc->request, NULL, 0, 0, xfer);
}

#if 0
Static void
umass_reset(struct umass_softc *sc, transfer_cb_f cb, void *priv)
{
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;

	/* The reset is a forced reset, so no error (yet) */
	sc->reset(sc, STATUS_CMD_OK);
}
#endif

/*
 * Bulk protocol specific functions
 */

Static void
umass_bbb_reset(struct umass_softc *sc, int status)
{
	usbd_device_handle dev;

	KASSERT(sc->proto & PROTO_BBB,
		("sc->proto == 0x%02x wrong for umass_bbb_reset\n", sc->proto));

	if (sc->sc_dying)
		return;

	/*
	 * Reset recovery (5.3.4 in Universal Serial Bus Mass Storage Class)
	 *
	 * For Reset Recovery the host shall issue in the following order:
	 * a) a Bulk-Only Mass Storage Reset
	 * b) a Clear Feature HALT to the Bulk-In endpoint
	 * c) a Clear Feature HALT to the Bulk-Out endpoint
	 *
	 * This is done in 3 steps, states:
	 * TSTATE_BBB_RESET1
	 * TSTATE_BBB_RESET2
	 * TSTATE_BBB_RESET3
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 */

	DPRINTF(UDMASS_BBB, ("%s: Bulk Reset\n",
		USBDEVNAME(sc->sc_dev)));
	
	sc->transfer_state = TSTATE_BBB_RESET1;
	sc->transfer_status = status;

	usbd_interface2device_handle(sc->iface, &dev);

	/* reset is a class specific interface write */
	sc->request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	sc->request.bRequest = UR_BBB_RESET;
	USETW(sc->request.wValue, 0);
	USETW(sc->request.wIndex, sc->ifaceno);
	USETW(sc->request.wLength, 0);
	umass_setup_ctrl_transfer(sc, dev, &sc->request, NULL, 0, 0,
				  sc->transfer_xfer[XFER_BBB_RESET1]);
}

Static void
umass_bbb_transfer(struct umass_softc *sc, int lun, void *cmd, int cmdlen,
		    void *data, int datalen, int dir,
		    transfer_cb_f cb, void *priv)
{
	static int dCBWtag = 42;	/* unique for CBW of transfer */

	DPRINTF(UDMASS_BBB,("%s: umass_bbb_transfer cmd=0x%02x\n",
		USBDEVNAME(sc->sc_dev), *(u_char*)cmd));

	KASSERT(sc->proto & PROTO_BBB,
		("sc->proto == 0x%02x wrong for umass_bbb_transfer\n",
		sc->proto));

	/*
	 * Do a Bulk-Only transfer with cmdlen bytes from cmd, possibly
	 * a data phase of datalen bytes from/to the device and finally a
	 * csw read phase.
	 * If the data direction was inbound a maximum of datalen bytes
	 * is stored in the buffer pointed to by data.
	 *
	 * umass_bbb_transfer initialises the transfer and lets the state
	 * machine in umass_bbb_state handle the completion. It uses the 
	 * following states:
	 * TSTATE_BBB_COMMAND
	 *   -> TSTATE_BBB_DATA
	 *   -> TSTATE_BBB_STATUS
	 *   -> TSTATE_BBB_STATUS2
	 *   -> TSTATE_BBB_IDLE
	 *
	 * An error in any of those states will invoke
	 * umass_bbb_reset.
	 */

	/* check the given arguments */
	KASSERT(datalen == 0 || data != NULL,
		("%s: datalen > 0, but no buffer",USBDEVNAME(sc->sc_dev)));
	KASSERT(cmdlen <= CBWCDBLENGTH,
		("%s: cmdlen exceeds CDB length in CBW (%d > %d)",
			USBDEVNAME(sc->sc_dev), cmdlen, CBWCDBLENGTH));
	KASSERT(dir == DIR_NONE || datalen > 0,
		("%s: datalen == 0 while direction is not NONE\n",
			USBDEVNAME(sc->sc_dev)));
	KASSERT(datalen == 0 || dir != DIR_NONE,
		("%s: direction is NONE while datalen is not zero\n",
			USBDEVNAME(sc->sc_dev)));
	KASSERT(sizeof(umass_bbb_cbw_t) == UMASS_BBB_CBW_SIZE,
		("%s: CBW struct does not have the right size (%d vs. %d)\n",
			USBDEVNAME(sc->sc_dev),
			sizeof(umass_bbb_cbw_t), UMASS_BBB_CBW_SIZE));
	KASSERT(sizeof(umass_bbb_csw_t) == UMASS_BBB_CSW_SIZE,
		("%s: CSW struct does not have the right size (%d vs. %d)\n",
			USBDEVNAME(sc->sc_dev),
			sizeof(umass_bbb_csw_t), UMASS_BBB_CSW_SIZE));

	/*
	 * Determine the direction of the data transfer and the length.
	 *
	 * dCBWDataTransferLength (datalen) :
	 *   This field indicates the number of bytes of data that the host
	 *   intends to transfer on the IN or OUT Bulk endpoint(as indicated by
	 *   the Direction bit) during the execution of this command. If this
	 *   field is set to 0, the device will expect that no data will be
	 *   transferred IN or OUT during this command, regardless of the value
	 *   of the Direction bit defined in dCBWFlags.
	 *
	 * dCBWFlags (dir) :
	 *   The bits of the Flags field are defined as follows:
	 *     Bits 0-6	 reserved
	 *     Bit  7	 Direction - this bit shall be ignored if the
	 *			     dCBWDataTransferLength field is zero.
	 *		 0 = data Out from host to device
	 *		 1 = data In from device to host
	 */

	/* Fill in the Command Block Wrapper */
	USETDW(sc->cbw.dCBWSignature, CBWSIGNATURE);
	USETDW(sc->cbw.dCBWTag, dCBWtag);
	dCBWtag++;	/* cannot be done in macro (it will be done 4 times) */
	USETDW(sc->cbw.dCBWDataTransferLength, datalen);
	/* DIR_NONE is treated as DIR_OUT (0x00) */
	sc->cbw.bCBWFlags = (dir == DIR_IN? CBWFLAGS_IN:CBWFLAGS_OUT);
	sc->cbw.bCBWLUN = lun;
	sc->cbw.bCDBLength = cmdlen;
	bcopy(cmd, sc->cbw.CBWCDB, cmdlen);

	DIF(UDMASS_BBB, umass_bbb_dump_cbw(sc, &sc->cbw));

	/* store the details for the data transfer phase */
	sc->transfer_dir = dir;
	sc->transfer_data = data;
	sc->transfer_datalen = datalen;
	sc->transfer_actlen = 0;
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;
	sc->transfer_status = STATUS_CMD_OK;

	/* move from idle to the command state */
	sc->transfer_state = TSTATE_BBB_COMMAND;

	/* Send the CBW from host to device via bulk-out endpoint. */
	if (umass_setup_transfer(sc, sc->bulkout_pipe,
			&sc->cbw, UMASS_BBB_CBW_SIZE, 0,
			sc->transfer_xfer[XFER_BBB_CBW])) {
		umass_bbb_reset(sc, STATUS_WIRE_FAILED);
	}
}


Static void
umass_bbb_state(usbd_xfer_handle xfer, usbd_private_handle priv,
		usbd_status err)
{
	struct umass_softc *sc = (struct umass_softc *) priv;
	usbd_xfer_handle next_xfer;

	KASSERT(sc->proto & PROTO_BBB,
		("sc->proto == 0x%02x wrong for umass_bbb_state\n",sc->proto));

	if (sc->sc_dying)
		return;

	/*
	 * State handling for BBB transfers.
	 *
	 * The subroutine is rather long. It steps through the states given in
	 * Annex A of the Bulk-Only specification.
	 * Each state first does the error handling of the previous transfer
	 * and then prepares the next transfer.
	 * Each transfer is done asynchroneously so after the request/transfer
	 * has been submitted you will find a 'return;'.
	 */

	DPRINTF(UDMASS_BBB, ("%s: Handling BBB state %d (%s), xfer=%p, %s\n",
		USBDEVNAME(sc->sc_dev), sc->transfer_state,
		states[sc->transfer_state], xfer, usbd_errstr(err)));

	switch (sc->transfer_state) {

	/***** Bulk Transfer *****/
	case TSTATE_BBB_COMMAND:
		/* Command transport phase, error handling */
		if (err) {
			DPRINTF(UDMASS_BBB, ("%s: failed to send CBW\n",
				USBDEVNAME(sc->sc_dev)));
			/* If the device detects that the CBW is invalid, then
			 * the device may STALL both bulk endpoints and require
			 * a Bulk-Reset
			 */
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		/* Data transport phase, setup transfer */
		sc->transfer_state = TSTATE_BBB_DATA;
		if (sc->transfer_dir == DIR_IN) {
			if (umass_setup_transfer(sc, sc->bulkin_pipe,
					sc->data_buffer, sc->transfer_datalen,
					USBD_SHORT_XFER_OK | USBD_NO_COPY,
					sc->transfer_xfer[XFER_BBB_DATA]))
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else if (sc->transfer_dir == DIR_OUT) {
			memcpy(sc->data_buffer, sc->transfer_data, 
			       sc->transfer_datalen);
			if (umass_setup_transfer(sc, sc->bulkout_pipe,
					sc->data_buffer, sc->transfer_datalen,
					USBD_NO_COPY,/* fixed length transfer */
					sc->transfer_xfer[XFER_BBB_DATA]))
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else {
			DPRINTF(UDMASS_BBB, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
		}

		/* FALLTHROUGH if no data phase, err == 0 */
	case TSTATE_BBB_DATA:
		/* Command transport phase, error handling (ignored if no data
		 * phase (fallthrough from previous state)) */
		if (sc->transfer_dir != DIR_NONE) {
			/* retrieve the length of the transfer that was done */
			usbd_get_xfer_status(xfer, NULL, NULL,
					     &sc->transfer_actlen, NULL);

			if (err) {
				DPRINTF(UDMASS_BBB, ("%s: Data-%s %db failed, "
					"%s\n", USBDEVNAME(sc->sc_dev),
					(sc->transfer_dir == DIR_IN?"in":"out"),
					sc->transfer_datalen,usbd_errstr(err)));

				if (err == USBD_STALLED) {
					umass_clear_endpoint_stall(sc,
					  (sc->transfer_dir == DIR_IN?
					    sc->bulkin:sc->bulkout),
					  (sc->transfer_dir == DIR_IN?
					    sc->bulkin_pipe:sc->bulkout_pipe),
					  TSTATE_BBB_DCLEAR,
					  sc->transfer_xfer[XFER_BBB_DCLEAR]);
					return;
				} else {
					/* Unless the error is a pipe stall the
					 * error is fatal.
					 */
					umass_bbb_reset(sc,STATUS_WIRE_FAILED);
					return;
				}
			}
		}

		if (sc->transfer_dir == DIR_IN)
			memcpy(sc->transfer_data, sc->data_buffer,
			       sc->transfer_actlen);

		DIF(UDMASS_BBB, if (sc->transfer_dir == DIR_IN)
					umass_dump_buffer(sc, sc->transfer_data,
						sc->transfer_datalen, 48));

		/* FALLTHROUGH, err == 0 (no data phase or successfull) */
	case TSTATE_BBB_DCLEAR: /* stall clear after data phase */
	case TSTATE_BBB_SCLEAR: /* stall clear after status phase */
		/* Reading of CSW after bulk stall condition in data phase
		 * (TSTATE_BBB_DATA2) or bulk-in stall condition after
		 * reading CSW (TSTATE_BBB_SCLEAR).
		 * In the case of no data phase or successfull data phase,
		 * err == 0 and the following if block is passed.
		 */
		if (err) {	/* should not occur */
			/* try the transfer below, even if clear stall failed */
			DPRINTF(UDMASS_BBB, ("%s: bulk-%s stall clear failed"
				", %s\n", USBDEVNAME(sc->sc_dev),
				(sc->transfer_dir == DIR_IN? "in":"out"),
				usbd_errstr(err)));
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}
	
		/* Status transport phase, setup transfer */
		if (sc->transfer_state == TSTATE_BBB_COMMAND ||
		    sc->transfer_state == TSTATE_BBB_DATA ||
		    sc->transfer_state == TSTATE_BBB_DCLEAR) {
			/* After no data phase, successfull data phase and
			 * after clearing bulk-in/-out stall condition
			 */
			sc->transfer_state = TSTATE_BBB_STATUS1;
			next_xfer = sc->transfer_xfer[XFER_BBB_CSW1];
		} else {
			/* After first attempt of fetching CSW */
			sc->transfer_state = TSTATE_BBB_STATUS2;
			next_xfer = sc->transfer_xfer[XFER_BBB_CSW2];
		}

		/* Read the Command Status Wrapper via bulk-in endpoint. */
		if (umass_setup_transfer(sc, sc->bulkin_pipe,
				&sc->csw, UMASS_BBB_CSW_SIZE, 0,
				next_xfer)) {
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		return;
	case TSTATE_BBB_STATUS1:	/* first attempt */
	case TSTATE_BBB_STATUS2:	/* second attempt */
		/* Status transfer, error handling */
		if (err) {
			DPRINTF(UDMASS_BBB, ("%s: Failed to read CSW, %s%s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err),
				(sc->transfer_state == TSTATE_BBB_STATUS1?
					", retrying":"")));

			/* If this was the first attempt at fetching the CSW
			 * retry it, otherwise fail.
			 */
			if (sc->transfer_state == TSTATE_BBB_STATUS1) {
				umass_clear_endpoint_stall(sc,
						sc->bulkin, sc->bulkin_pipe,
						TSTATE_BBB_SCLEAR,
						sc->transfer_xfer[XFER_BBB_SCLEAR]);
				return;
			} else {
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);
				return;
			}
		}

		DIF(UDMASS_BBB, umass_bbb_dump_csw(sc, &sc->csw));

		/* Check CSW and handle any error */
		if (UGETDW(sc->csw.dCSWSignature) != CSWSIGNATURE) {
			/* Invalid CSW: Wrong signature or wrong tag might
			 * indicate that the device is confused -> reset it.
			 */
			printf("%s: Invalid CSW: sig 0x%08x should be 0x%08x\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWSignature),
				CSWSIGNATURE);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		} else if (UGETDW(sc->csw.dCSWTag)
				!= UGETDW(sc->cbw.dCBWTag)) {
			printf("%s: Invalid CSW: tag %d should be %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWTag),
				UGETDW(sc->cbw.dCBWTag));

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		/* CSW is valid here */
		} else if (sc->csw.bCSWStatus > CSWSTATUS_PHASE) {
			printf("%s: Invalid CSW: status %d > %d\n",
				USBDEVNAME(sc->sc_dev),
				sc->csw.bCSWStatus,
				CSWSTATUS_PHASE);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		} else if (sc->csw.bCSWStatus == CSWSTATUS_PHASE) {
			printf("%s: Phase Error, residue = %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWDataResidue));
				
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		} else if (sc->transfer_actlen > sc->transfer_datalen) {
			/* Buffer overrun! Don't let this go by unnoticed */
			panic("%s: transferred %d bytes instead of %d bytes\n",
				USBDEVNAME(sc->sc_dev),
				sc->transfer_actlen, sc->transfer_datalen);
		} else if (sc->transfer_datalen - sc->transfer_actlen
			   != UGETDW(sc->csw.dCSWDataResidue)) {
			DPRINTF(UDMASS_BBB, ("%s: actlen=%d != residue=%d\n",
				USBDEVNAME(sc->sc_dev),
				sc->transfer_datalen - sc->transfer_actlen,
				UGETDW(sc->csw.dCSWDataResidue)));

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		} else if (sc->csw.bCSWStatus == CSWSTATUS_FAILED) {
			DPRINTF(UDMASS_BBB, ("%s: Command Failed, res = %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWDataResidue)));

			/* SCSI command failed but transfer was succesful */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					UGETDW(sc->csw.dCSWDataResidue),
					STATUS_CMD_FAILED);

			return;

		} else {	/* success */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					UGETDW(sc->csw.dCSWDataResidue),
					STATUS_CMD_OK);

			return;
		}

	/***** Bulk Reset *****/
	case TSTATE_BBB_RESET1:
		if (err)
			printf("%s: BBB reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		umass_clear_endpoint_stall(sc,
			sc->bulkin, sc->bulkin_pipe, TSTATE_BBB_RESET2,
			sc->transfer_xfer[XFER_BBB_RESET2]);

		return;
	case TSTATE_BBB_RESET2:
		if (err)	/* should not occur */
			printf("%s: BBB bulk-in clear stall failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		umass_clear_endpoint_stall(sc,
			sc->bulkout, sc->bulkout_pipe, TSTATE_BBB_RESET3,
			sc->transfer_xfer[XFER_BBB_RESET3]);

		return;
	case TSTATE_BBB_RESET3:
		if (err)	/* should not occur */
			printf("%s: BBB bulk-out clear stall failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_IDLE;
		if (sc->transfer_priv) {
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					sc->transfer_status);
		}

		return;

	/***** Default *****/
	default:
		panic("%s: Unknown state %d\n",
		      USBDEVNAME(sc->sc_dev), sc->transfer_state);
	}
}

/*
 * Command/Bulk/Interrupt (CBI) specific functions
 */

Static int
umass_cbi_adsc(struct umass_softc *sc, char *buffer, int buflen,
	       usbd_xfer_handle xfer)
{
	usbd_device_handle dev;

	KASSERT(sc->proto & (PROTO_CBI|PROTO_CBI_I),
		("sc->proto == 0x%02x wrong for umass_cbi_adsc\n",sc->proto));

	usbd_interface2device_handle(sc->iface, &dev);

	sc->request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	sc->request.bRequest = UR_CBI_ADSC;
	USETW(sc->request.wValue, 0);
	USETW(sc->request.wIndex, sc->ifaceno);
	USETW(sc->request.wLength, buflen);
	return umass_setup_ctrl_transfer(sc, dev, &sc->request, buffer,
					 buflen, 0, xfer);
}


Static void
umass_cbi_reset(struct umass_softc *sc, int status)
{
	int i;
#	define SEND_DIAGNOSTIC_CMDLEN	12

	KASSERT(sc->proto & (PROTO_CBI|PROTO_CBI_I),
		("sc->proto == 0x%02x wrong for umass_cbi_reset\n",sc->proto));

	if (sc->sc_dying)
		return;

	/*
	 * Command Block Reset Protocol
	 * 
	 * First send a reset request to the device. Then clear
	 * any possibly stalled bulk endpoints.

	 * This is done in 3 steps, states:
	 * TSTATE_CBI_RESET1
	 * TSTATE_CBI_RESET2
	 * TSTATE_CBI_RESET3
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 */

	DPRINTF(UDMASS_CBI, ("%s: CBI Reset\n",
		USBDEVNAME(sc->sc_dev)));
	
	KASSERT(sizeof(sc->cbl) >= SEND_DIAGNOSTIC_CMDLEN,
		("%s: CBL struct is too small (%d < %d)\n",
			USBDEVNAME(sc->sc_dev),
			sizeof(sc->cbl), SEND_DIAGNOSTIC_CMDLEN));

	sc->transfer_state = TSTATE_CBI_RESET1;
	sc->transfer_status = status;

	/* The 0x1d code is the SEND DIAGNOSTIC command. To distingiush between
	 * the two the last 10 bytes of the cbl is filled with 0xff (section
	 * 2.2 of the CBI spec).
	 */
	sc->cbl[0] = 0x1d;	/* Command Block Reset */
	sc->cbl[1] = 0x04;
	for (i = 2; i < SEND_DIAGNOSTIC_CMDLEN; i++)
		sc->cbl[i] = 0xff;

	umass_cbi_adsc(sc, sc->cbl, SEND_DIAGNOSTIC_CMDLEN,
		       sc->transfer_xfer[XFER_CBI_RESET1]);
	/* XXX if the command fails we should reset the port on the bub */
}

Static void
umass_cbi_transfer(struct umass_softc *sc, int lun,
		void *cmd, int cmdlen, void *data, int datalen, int dir,
		transfer_cb_f cb, void *priv)
{
	DPRINTF(UDMASS_CBI,("%s: umass_cbi_transfer cmd=0x%02x, len=%d\n",
		USBDEVNAME(sc->sc_dev), *(u_char*)cmd, datalen));

	KASSERT(sc->proto & (PROTO_CBI|PROTO_CBI_I),
		("sc->proto == 0x%02x wrong for umass_cbi_transfer\n",
		sc->proto));

	if (sc->sc_dying)
		return;

	/*
	 * Do a CBI transfer with cmdlen bytes from cmd, possibly
	 * a data phase of datalen bytes from/to the device and finally a
	 * csw read phase.
	 * If the data direction was inbound a maximum of datalen bytes
	 * is stored in the buffer pointed to by data.
	 *
	 * umass_cbi_transfer initialises the transfer and lets the state
	 * machine in umass_cbi_state handle the completion. It uses the 
	 * following states:
	 * TSTATE_CBI_COMMAND
	 *   -> XXX fill in
	 *
	 * An error in any of those states will invoke
	 * umass_cbi_reset.
	 */

	/* check the given arguments */
	KASSERT(datalen == 0 || data != NULL,
		("%s: datalen > 0, but no buffer",USBDEVNAME(sc->sc_dev)));
	KASSERT(datalen == 0 || dir != DIR_NONE,
		("%s: direction is NONE while datalen is not zero\n",
			USBDEVNAME(sc->sc_dev)));

	/* store the details for the data transfer phase */
	sc->transfer_dir = dir;
	sc->transfer_data = data;
	sc->transfer_datalen = datalen;
	sc->transfer_actlen = 0;
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;
	sc->transfer_status = STATUS_CMD_OK;

	/* move from idle to the command state */
	sc->transfer_state = TSTATE_CBI_COMMAND;

	/* Send the Command Block from host to device via control endpoint. */
	if (umass_cbi_adsc(sc, cmd, cmdlen, sc->transfer_xfer[XFER_CBI_CB]))
		umass_cbi_reset(sc, STATUS_WIRE_FAILED);
}

Static void
umass_cbi_state(usbd_xfer_handle xfer, usbd_private_handle priv,
		usbd_status err)
{
	struct umass_softc *sc = (struct umass_softc *) priv;

	KASSERT(sc->proto & (PROTO_CBI|PROTO_CBI_I),
		("sc->proto == 0x%02x wrong for umass_cbi_state\n", sc->proto));

	if (sc->sc_dying)
		return;

	/*
	 * State handling for CBI transfers.
	 */

	DPRINTF(UDMASS_CBI, ("%s: Handling CBI state %d (%s), xfer=%p, %s\n",
		USBDEVNAME(sc->sc_dev), sc->transfer_state,
		states[sc->transfer_state], xfer, usbd_errstr(err)));

	switch (sc->transfer_state) {

	/***** CBI Transfer *****/
	case TSTATE_CBI_COMMAND:
		if (err == USBD_STALLED) {
			DPRINTF(UDMASS_CBI, ("%s: Command Transport failed\n",
				USBDEVNAME(sc->sc_dev)));
			/* Status transport by control pipe (section 2.3.2.1).
			 * The command contained in the command block failed.
			 *
			 * The control pipe has already been unstalled by the
			 * USB stack.
			 * Section 2.4.3.1.1 states that the bulk in endpoints
			 * should not stalled at this point.
			 */

			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					STATUS_CMD_FAILED);

			return;
		} else if (err) {
			DPRINTF(UDMASS_CBI, ("%s: failed to send ADSC\n",
				USBDEVNAME(sc->sc_dev)));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);

			return;
		}
		
		sc->transfer_state = TSTATE_CBI_DATA;
		if (sc->transfer_dir == DIR_IN) {
			if (umass_setup_transfer(sc, sc->bulkin_pipe,
					sc->transfer_data, sc->transfer_datalen,
					USBD_SHORT_XFER_OK | USBD_NO_COPY,
					sc->transfer_xfer[XFER_CBI_DATA]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);

		} else if (sc->transfer_dir == DIR_OUT) {
			memcpy(sc->data_buffer, sc->transfer_data, 
			       sc->transfer_datalen);
			if (umass_setup_transfer(sc, sc->bulkout_pipe,
					sc->transfer_data, sc->transfer_datalen,
					USBD_NO_COPY,/* fixed length transfer */
					sc->transfer_xfer[XFER_CBI_DATA]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);

		} else if (sc->proto & PROTO_CBI_I) {
			DPRINTF(UDMASS_CBI, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
			sc->transfer_state = TSTATE_CBI_STATUS;
			if (umass_setup_transfer(sc, sc->intrin_pipe,
					&sc->sbl, sizeof(sc->sbl),
					0,	/* fixed length transfer */
					sc->transfer_xfer[XFER_CBI_STATUS])){
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
		} else {
			DPRINTF(UDMASS_CBI, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
			/* No command completion interrupt. Request
			 * sense data.
			 */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
			       0, STATUS_CMD_UNKNOWN);
		}

		return;

	case TSTATE_CBI_DATA:
		/* retrieve the length of the transfer that was done */
		usbd_get_xfer_status(xfer,NULL,NULL,&sc->transfer_actlen,NULL);
		DPRINTF(UDMASS_CBI, ("%s: CBI_DATA actlen=%d\n",
			USBDEVNAME(sc->sc_dev), sc->transfer_actlen));

		if (err) {
			DPRINTF(UDMASS_CBI, ("%s: Data-%s %db failed, "
				"%s\n", USBDEVNAME(sc->sc_dev),
				(sc->transfer_dir == DIR_IN?"in":"out"),
				sc->transfer_datalen,usbd_errstr(err)));

			if (err == USBD_STALLED) {
				umass_clear_endpoint_stall(sc,
					sc->bulkin, sc->bulkin_pipe,
					TSTATE_CBI_DCLEAR,
					sc->transfer_xfer[XFER_CBI_DCLEAR]);
			} else {
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
			return;
		}

		if (sc->transfer_dir == DIR_IN)
			memcpy(sc->transfer_data, sc->data_buffer,
			       sc->transfer_actlen);

		DIF(UDMASS_CBI, if (sc->transfer_dir == DIR_IN)
					umass_dump_buffer(sc, sc->transfer_data,
						sc->transfer_actlen, 48));

		if (sc->proto & PROTO_CBI_I) {
			sc->transfer_state = TSTATE_CBI_STATUS;
			memset(&sc->sbl, 0, sizeof(sc->sbl));
			if (umass_setup_transfer(sc, sc->intrin_pipe,
				    &sc->sbl, sizeof(sc->sbl),
				    0,	/* fixed length transfer */
				    sc->transfer_xfer[XFER_CBI_STATUS])){
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
		} else {
			/* No command completion interrupt. Request
			 * sense to get status of command.
			 */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
				sc->transfer_datalen - sc->transfer_actlen,
				STATUS_CMD_UNKNOWN);
		}
		return;

	case TSTATE_CBI_STATUS:
		if (err) {
			DPRINTF(UDMASS_CBI, ("%s: Status Transport failed\n",
				USBDEVNAME(sc->sc_dev)));
			/* Status transport by interrupt pipe (section 2.3.2.2).
			 */

			if (err == USBD_STALLED) {
				umass_clear_endpoint_stall(sc,
					sc->intrin, sc->intrin_pipe,
					TSTATE_CBI_SCLEAR,
					sc->transfer_xfer[XFER_CBI_SCLEAR]);
			} else {
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
			return;
		}

		/* Dissect the information in the buffer */

		if (sc->proto & PROTO_UFI) {
			int status;
			
			/* Section 3.4.3.1.3 specifies that the UFI command
			 * protocol returns an ASC and ASCQ in the interrupt
			 * data block.
			 */

			DPRINTF(UDMASS_CBI, ("%s: UFI CCI, ASC = 0x%02x, "
				"ASCQ = 0x%02x\n",
				USBDEVNAME(sc->sc_dev),
				sc->sbl.ufi.asc, sc->sbl.ufi.ascq));

			if (sc->sbl.ufi.asc == 0 && sc->sbl.ufi.ascq == 0)
				status = STATUS_CMD_OK;
			else
				status = STATUS_CMD_FAILED;

			/* No sense, command successfull */
		} else {
			/* Command Interrupt Data Block */
			DPRINTF(UDMASS_CBI, ("%s: type=0x%02x, value=0x%02x\n",
				USBDEVNAME(sc->sc_dev),
				sc->sbl.common.type, sc->sbl.common.value));

			if (sc->sbl.common.type == IDB_TYPE_CCI) {
				int err;

				if ((sc->sbl.common.value&IDB_VALUE_STATUS_MASK)
							== IDB_VALUE_PASS) {
					err = STATUS_CMD_OK;
				} else if ((sc->sbl.common.value & IDB_VALUE_STATUS_MASK)
							== IDB_VALUE_FAIL ||
					   (sc->sbl.common.value & IDB_VALUE_STATUS_MASK)
						== IDB_VALUE_PERSISTENT) {
					err = STATUS_CMD_FAILED;
				} else {
					err = STATUS_WIRE_FAILED;
				}

				sc->transfer_state = TSTATE_IDLE;
				sc->transfer_cb(sc, sc->transfer_priv,
						sc->transfer_datalen,
						err);
			}
		}
		return;

	case TSTATE_CBI_DCLEAR:
		if (err) {	/* should not occur */
			printf("%s: CBI bulk-in/out stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);
		}

		sc->transfer_state = TSTATE_IDLE;
		sc->transfer_cb(sc, sc->transfer_priv,
				sc->transfer_datalen,
				STATUS_CMD_FAILED);
		return;

	case TSTATE_CBI_SCLEAR:
		if (err)	/* should not occur */
			printf("%s: CBI intr-in stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		/* Something really bad is going on. Reset the device */
		umass_cbi_reset(sc, STATUS_CMD_FAILED);
		return;

	/***** CBI Reset *****/
	case TSTATE_CBI_RESET1:
		if (err)
			printf("%s: CBI reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		umass_clear_endpoint_stall(sc,
			sc->bulkin, sc->bulkin_pipe, TSTATE_CBI_RESET2,
			sc->transfer_xfer[XFER_CBI_RESET2]);

		return;
	case TSTATE_CBI_RESET2:
		if (err)	/* should not occur */
			printf("%s: CBI bulk-in stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		umass_clear_endpoint_stall(sc,
			sc->bulkout, sc->bulkout_pipe, TSTATE_CBI_RESET3,
			sc->transfer_xfer[XFER_CBI_RESET3]);

		return;
	case TSTATE_CBI_RESET3:
		if (err)	/* should not occur */
			printf("%s: CBI bulk-out stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_IDLE;
		if (sc->transfer_priv) {
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					sc->transfer_status);
		}

		return;


	/***** Default *****/
	default:
		panic("%s: Unknown state %d\n",
		      USBDEVNAME(sc->sc_dev), sc->transfer_state);
	}
}

usbd_status
umass_bbb_get_max_lun(struct umass_softc *sc, u_int8_t *maxlun)
{
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;
	usb_interface_descriptor_t *id;

	*maxlun = 0;		/* Default to 0. */

	DPRINTF(UDMASS_BBB, ("%s: Get Max Lun\n", USBDEVNAME(sc->sc_dev)));

	usbd_interface2device_handle(sc->iface, &dev);
	id = usbd_get_interface_descriptor(sc->iface);

	/* The Get Max Lun command is a class-specific request. */
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_BBB_GET_MAX_LUN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 1);

	err = usbd_do_request(dev, &req, maxlun);
	switch (err) {
	case USBD_NORMAL_COMPLETION:
		DPRINTF(UDMASS_BBB, ("%s: Max Lun %d\n",
		    USBDEVNAME(sc->sc_dev), *maxlun));
		break;

	case USBD_STALLED:
		/*
		 * Device doesn't support Get Max Lun request.
		 */
		err = USBD_NORMAL_COMPLETION;
		DPRINTF(UDMASS_BBB, ("%s: Get Max Lun not supported\n",
		    USBDEVNAME(sc->sc_dev)));
		break;

	case USBD_SHORT_XFER:
		/*
		 * XXX This must mean Get Max Lun is not supported, too!
		 */
		err = USBD_NORMAL_COMPLETION;
		DPRINTF(UDMASS_BBB, ("%s: Get Max Lun SHORT_XFER\n",
		    USBDEVNAME(sc->sc_dev)));
		break;

	default:
		printf("%s: Get Max Lun failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		/* XXX Should we port_reset the device? */
		break;
	}

	return (err);
}



#if defined(__FreeBSD__)
/*
 * CAM specific functions (used by SCSI, UFI, 8070)
 */

Static int
umass_cam_attach_sim(void)
{
	struct cam_devq *devq;		/* Per device Queue */

	/* A HBA is attached to the CAM layer.
	 *
	 * The CAM layer will then after a while start probing for
	 * devices on the bus. The number of devices is limitted to one.
	 */

	/* SCSI transparent command set */

	devq = cam_simq_alloc(1 /*maximum openings*/);
	if (devq == NULL)
		return(ENOMEM);

	umass_sim = cam_sim_alloc(umass_cam_action, umass_cam_poll, DEVNAME,
				NULL /*priv*/, 0 /*unit number*/,
				1 /*maximum device openings*/,
				0 /*maximum tagged device openings*/,
				devq);
	if (umass_sim == NULL) {
		cam_simq_free(devq);
		return(ENOMEM);
	}

	if(xpt_bus_register(umass_sim, 0) != CAM_SUCCESS)
		return(ENOMEM);

	if (xpt_create_path(&umass_path, NULL, cam_sim_path(umass_sim),
			    UMASS_SCSIID_HOST, 0)
	    != CAM_REQ_CMP)
		return(ENOMEM);

	return(0);
}

#ifdef UMASS_DO_CAM_RESCAN
/* this function is only used from umass_cam_rescan, so mention
 * prototype down here.
 */
Static void umass_cam_rescan_callback(struct cam_periph *periph,union ccb *ccb);

Static void
umass_cam_rescan_callback(struct cam_periph *periph, union ccb *ccb)
{
#ifdef UMASS_DEBUG
	struct umass_softc *sc = devclass_get_softc(umass_devclass,
					       ccb->ccb_h.target_id);

	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d: Rescan failed, 0x%04x\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccb->ccb_h.status));
	} else {
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d: Rescan succeeded, freeing resources.\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));
	}
#endif

	xpt_free_path(ccb->ccb_h.path);
	free(ccb, M_USBDEV);
}

Static void
umass_cam_rescan(struct umass_softc *sc)
{
	struct cam_path *path;
	union ccb *ccb = malloc(sizeof(union ccb), M_USBDEV, M_WAITOK);

	memset(ccb, 0, sizeof(union ccb));

	DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d: scanning bus for new device %d\n",
		USBDEVNAME(sc->sc_dev),	 cam_sim_path(umass_sim),
		device_get_unit(sc->sc_dev), 0,
		device_get_unit(sc->sc_dev)));

	if (xpt_create_path(&path, xpt_periph, cam_sim_path(umass_sim),
		    device_get_unit(sc->sc_dev), 0)
	    != CAM_REQ_CMP)
		return;

	xpt_setup_ccb(&ccb->ccb_h, path, 5/*priority (low)*/);
	ccb->ccb_h.func_code = XPT_SCAN_BUS;
	ccb->ccb_h.cbfcnp = umass_cam_rescan_callback;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);

	/* The scan is in progress now. */
}
#endif

Static int
umass_cam_attach(struct umass_softc *sc)
{
	/* SIM already attached at module load. The device is a target on the
	 * one SIM we registered: target device_get_unit(self).
	 */

	/* The artificial limit UMASS_SCSIID_MAX is there because CAM expects
	 * a limit to the number of targets that are present on a SIM.
	 */
	if (device_get_unit(sc->sc_dev) > UMASS_SCSIID_MAX) {
		printf("%s: Increase UMASS_SCSIID_MAX (currently %d) in %s "
			"and try again.\n", USBDEVNAME(sc->sc_dev),
			UMASS_SCSIID_MAX, __FILE__);
		return(1);
	}
		
#ifdef UMASS_DO_CAM_RESCAN
	if (!cold) {
		/* Notify CAM of the new device. Any failure is benign, as the
		 * user can still do it by hand (camcontrol rescan <busno>).
		 * Only do this if we are not booting, because CAM does a scan
		 * after booting has completed, when interrupts have been
		 * enabled.
		 */
		umass_cam_rescan(sc);
	}
#endif

	return(0);	/* always succesful */
}

/* umass_cam_detach
 *	detach from the CAM layer
 */

Static int
umass_cam_detach_sim(void)
{
	if (umass_sim)
		return(EBUSY);	/* XXX CAM can't handle disappearing SIMs yet */

	if (umass_path) {
		/* XXX do we need to send an asynchroneous event for the SIM?
		xpt_async(AC_LOST_DEVICE, umass_path, NULL);
		 */
		xpt_free_path(umass_path);
		umass_path = NULL;
	}

	if (umass_sim) {
		if (xpt_bus_deregister(cam_sim_path(umass_sim)))
			cam_sim_free(umass_sim, /*free_devq*/TRUE);
		else
			return(EBUSY);

		umass_sim = NULL;
	}

	return(0);
}

Static int
umass_cam_detach(struct umass_softc *sc)
{
	struct cam_path *path;

	/* detach of sim not done until module unload */
	DPRINTF(UDMASS_SCSI, ("%s: losing CAM device entry\n",
		USBDEVNAME(sc->sc_dev)));

	if (xpt_create_path(&path, NULL, cam_sim_path(umass_sim),
		    device_get_unit(sc->sc_dev), CAM_LUN_WILDCARD)
	    != CAM_REQ_CMP)
		return(ENOMEM);
	xpt_async(AC_LOST_DEVICE, path, NULL);
	xpt_free_path(path);

	return(0);
}



/* umass_cam_action
 *	CAM requests for action come through here
 */

Static void
umass_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct umass_softc *sc = devclass_get_softc(umass_devclass,
					       ccb->ccb_h.target_id);

	/* The softc is still there, but marked as going away. umass_cam_detach
	 * has not yet notified CAM of the lost device however.
	 */
	if (sc && sc->sc_dying) {
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:func_code 0x%04x: "
			"Invalid target (gone)\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccb->ccb_h.func_code));
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return;
	}

	/* Verify, depending on the operation to perform, that we either got a
	 * valid sc, because an existing target was referenced, or otherwise
	 * the SIM is addressed.
	 *
	 * This avoids bombing out at a printf and does give the CAM layer some
	 * sensible feedback on errors.
	 */
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	case XPT_RESET_DEV:
	case XPT_GET_TRAN_SETTINGS:
	case XPT_SET_TRAN_SETTINGS:
	case XPT_CALC_GEOMETRY:
		/* the opcodes requiring a target. These should never occur. */
		if (sc == NULL) {
			printf("%s:%d:%d:%d:func_code 0x%04x: "
				"Invalid target\n",
				DEVNAME_SIM, UMASS_SCSI_BUS,
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				ccb->ccb_h.func_code);

			ccb->ccb_h.status = CAM_TID_INVALID;
			xpt_done(ccb);
			return;
		}
		break;
	case XPT_PATH_INQ:
	case XPT_NOOP:
		/* The opcodes sometimes aimed at a target (sc is valid),
		 * sometimes aimed at the SIM (sc is invalid and target is
		 * CAM_TARGET_WILDCARD)
		 */
		if (sc == NULL && ccb->ccb_h.target_id != CAM_TARGET_WILDCARD) {
			DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:func_code 0x%04x: "
				"Invalid target\n",
				DEVNAME_SIM, UMASS_SCSI_BUS,
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				ccb->ccb_h.func_code));

			ccb->ccb_h.status = CAM_TID_INVALID;
			xpt_done(ccb);
			return;
		}
		break;
	default:
		/* XXX Hm, we should check the input parameters */
	}

	/* Perform the requested action */
	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio = &ccb->csio;	/* deref union */
		int dir;
		unsigned char *cmd;
		int cmdlen;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_SCSI_IO: "
			"cmd: 0x%02x, flags: 0x%02x, "
			"%db cmd/%db data/%db sense\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			csio->cdb_io.cdb_bytes[0],
			ccb->ccb_h.flags & CAM_DIR_MASK,
			csio->cdb_len, csio->dxfer_len,
			csio->sense_len));

		/* clear the end of the buffer to make sure we don't send out
		 * garbage.
		 */
		DIF(UDMASS_SCSI, if ((ccb->ccb_h.flags & CAM_DIR_MASK)
				     == CAM_DIR_OUT)
					umass_dump_buffer(sc, csio->data_ptr,
						csio->dxfer_len, 48));

		if (sc->transfer_state != TSTATE_IDLE) {
			DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_SCSI_IO: "
				"I/O requested while busy (state %d, %s)\n",
				USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				sc->transfer_state,states[sc->transfer_state]));
			ccb->ccb_h.status = CAM_SCSI_BUSY;
			xpt_done(ccb);
			return;
		}

		switch(ccb->ccb_h.flags&CAM_DIR_MASK) {
		case CAM_DIR_IN:
			dir = DIR_IN;
			break;
		case CAM_DIR_OUT:
			dir = DIR_OUT;
			break;
		default:
			dir = DIR_NONE;
		}

		ccb->ccb_h.status = CAM_REQ_INPROG | CAM_SIM_QUEUED;
		if (sc->transform(sc, csio->cdb_io.cdb_bytes, csio->cdb_len,
				  &cmd, &cmdlen)) {
			sc->transfer(sc, ccb->ccb_h.target_lun, cmd, cmdlen,
				     csio->data_ptr,
				     csio->dxfer_len, dir,
				     umass_cam_cb, (void *) ccb);
		} else {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
		}

		break;
	}
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_PATH_INQ:.\n",
			(sc == NULL? DEVNAME_SIM:USBDEVNAME(sc->sc_dev)),
			UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		/* host specific information */
		cpi->version_num = 1;
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = UMASS_SCSIID_MAX;	/* one target */
		cpi->max_lun = 0;	/* no LUN's supported */
		cpi->initiator_id = UMASS_SCSIID_HOST;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "USB SCSI", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = UMASS_SCSI_BUS;
		if (sc) {
			cpi->base_transfer_speed = sc->transfer_speed;
			cpi->max_lun = sc->maxlun;
		}

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_DEV:
	{
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_RESET_DEV:.\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		ccb->ccb_h.status = CAM_REQ_INPROG;
		umass_reset(sc, umass_cam_cb, (void *) ccb);
		break;
	} 
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts = &ccb->cts;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_GET_TRAN_SETTINGS:.\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		cts->valid = 0;
		cts->flags = 0;		/* no disconnection, tagging */

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_SET_TRAN_SETTINGS:.\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg = &ccb->ccg;

		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_CALC_GEOMETRY: "
			"Volume size = %d\n",
			USBDEVNAME(sc->sc_dev), UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccg->volume_size));

		/* XXX We should probably ask the drive for the details
		 *     instead of cluching them up ourselves
		 */
		if (sc->drive == ZIP_100) {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
			ccg->cylinders = ccg->volume_size / ccg->heads
					  / ccg->secs_per_track;
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		} else if (sc->proto & PROTO_UFI) {
			ccg->heads = 2;
			if (ccg->volume_size == 2880)
				ccg->secs_per_track = 18;
			else
				ccg->secs_per_track = 9;
			ccg->cylinders = 80;
			break;
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		}

		xpt_done(ccb);
		break;
	}
	case XPT_NOOP:
	{
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:XPT_NOOP:.\n",
			(sc == NULL? DEVNAME_SIM:USBDEVNAME(sc->sc_dev)),
			UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun));

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		DPRINTF(UDMASS_SCSI, ("%s:%d:%d:%d:func_code 0x%04x: "
			"Not implemented\n",
			(sc == NULL? DEVNAME_SIM:USBDEVNAME(sc->sc_dev)),
			UMASS_SCSI_BUS,
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccb->ccb_h.func_code));

		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		break;
	}
}

/* umass_cam_poll
 *	all requests are handled through umass_cam_action, requests
 *	are never pending. So, nothing to do here.
 */
Static void
umass_cam_poll(struct cam_sim *sim)
{
#ifdef UMASS_DEBUG
	struct umass_softc *sc = (struct umass_softc *) sim->softc;

	DPRINTF(UDMASS_SCSI, ("%s: CAM poll\n",
		USBDEVNAME(sc->sc_dev)));
#endif

	/* nop */
}


/* umass_cam_cb
 *	finalise a completed CAM command
 */

Static void
umass_cam_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	union ccb *ccb = (union ccb *) priv;
	struct ccb_scsiio *csio = &ccb->csio;		/* deref union */

	csio->resid = residue;

	switch (status) {
	case STATUS_CMD_OK:
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case STATUS_CMD_UNKNOWN:
	case STATUS_CMD_FAILED:
		switch (ccb->ccb_h.func_code) {
		case XPT_SCSI_IO:
		{
			unsigned char *cmd;
			int cmdlen;

			/* fetch sense data */
			DPRINTF(UDMASS_SCSI,("%s: Fetching %db sense data\n",
			        USBDEVNAME(sc->sc_dev),
			        sc->cam_scsi_sense.length));

			sc->cam_scsi_sense.length = csio->sense_len;

			if (sc->transform(sc, (char *) &sc->cam_scsi_sense,
				      sizeof(sc->cam_scsi_sense),
				      &cmd, &cmdlen)) {
				sc->transfer(sc, ccb->ccb_h.target_lun,
					     cmd, cmdlen,
					     &csio->sense_data,
					     csio->sense_len, DIR_IN,
					     umass_cam_sense_cb, (void *) ccb);
			} else {
#ifdef UMASS_DEBUG
				panic("transform(REQUEST_SENSE) failed\n");
#else 
				csio->resid = sc->transfer_datalen;
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				xpt_done(ccb);
#endif
			}
			break;
		}
		case XPT_RESET_DEV: /* Reset failed */
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			break;
		default:
			panic("umass_cam_cb called for func_code %d\n",
			      ccb->ccb_h.func_code);
		}
		break;

	case STATUS_WIRE_FAILED:
		/* the wire protocol failed and will have recovered
		 * (hopefully).	 We return an error to CAM and let CAM retry
		 * the command if necessary.
		 */
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		xpt_done(ccb);
		break;

	default:
		panic("%s: Unknown status %d in umass_cam_cb\n",
			USBDEVNAME(sc->sc_dev), status);
	}
}

/* Finalise a completed autosense operation
 */
Static void
umass_cam_sense_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	union ccb *ccb = (union ccb *) priv;
	struct ccb_scsiio *csio = &ccb->csio;		/* deref union */

	switch (status) {
	case STATUS_CMD_OK:
	case STATUS_CMD_UNKNOWN:
		/* Getting sense data succeeded. The length of the sense data
		 * is not returned in any way. The sense data itself contains
		 * the length of the sense data that is valid.
		 */
		if (sc->quirks & RS_NO_CLEAR_UA
		    && csio->cdb_io.cdb_bytes[0] == INQUIRY
		    && (csio->sense_data.flags & SSD_KEY)
						== SSD_KEY_UNIT_ATTENTION) {
			/* Ignore unit attention errors in the case where
			 * the Unit Attention state is not cleared on
			 * REQUEST SENSE. They will appear again at the next
			 * command.
			 */
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else if ((csio->sense_data.flags & SSD_KEY)
						== SSD_KEY_NO_SENSE) {
			/* No problem after all (in the case of CBI without
			 * CCI)
			 */
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else {
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR
					    | CAM_AUTOSNS_VALID;
			csio->scsi_status = SCSI_STATUS_CHECK_COND;
		}
		xpt_done(ccb);
		break;

	default:
		DPRINTF(UDMASS_SCSI, ("%s: Autosense failed, status %d\n",
			USBDEVNAME(sc->sc_dev), status));
		ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
		xpt_done(ccb);
	}
}


Static int
umass_driver_load(module_t mod, int what, void *arg)
{
	int err;

	switch (what) {
	case MOD_UNLOAD:
		err = umass_cam_detach_sim();
		if (err)
			return(err);
		return(usbd_driver_load(mod, what, arg));
	case MOD_LOAD:
		/* We don't attach to CAM at this point, because it will try
		 * and malloc memory for it. This is not possible when the
		 * boot loader loads umass as a module before the kernel
		 * has been bootstrapped.
		 */
	default:
		return(usbd_driver_load(mod, what, arg));
	}
}



/* (even the comment is missing) */

DRIVER_MODULE(umass, uhub, umass_driver, umass_devclass, umass_driver_load, 0);


/*
 * SCSI specific functions
 */

Static int
umass_scsi_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		     unsigned char **rcmd, int *rcmdlen)
{
	*rcmd = cmd;		/* trivial copy */
	*rcmdlen = cmdlen;

	switch (cmd[0]) {
	case TEST_UNIT_READY:
		if (sc->quirks & NO_TEST_UNIT_READY) {
			DPRINTF(UDMASS_SCSI, ("%s: Converted TEST_UNIT_READY "
				"to START_UNIT\n", USBDEVNAME(sc->sc_dev)));
			cmd[0] = START_STOP_UNIT;
			cmd[4] = SSS_START;
		}
		break;
	}

	return 1;		/* success */
}

/*
 * UFI specific functions
 */

Static int
umass_ufi_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		    unsigned char **rcmd, int *rcmdlen)
{
	*rcmd = cmd;
	/* A UFI command is always 12 bytes in length */
	/* XXX cmd[(cmdlen+1)..12] contains garbage */
	*rcmdlen = 12;

	switch (cmd[0]) {
	case TEST_UNIT_READY:
		if (sc->quirks &  NO_TEST_UNIT_READY) {
			DPRINTF(UDMASS_UFI, ("%s: Converted TEST_UNIT_READY "
				"to START_UNIT\n", USBDEVNAME(sc->sc_dev)));
			cmd[0] = START_STOP_UNIT;
			cmd[4] = SSS_START;
		}
		return 1;
	case INQUIRY:
	case START_STOP_UNIT:
	case MODE_SENSE:
	case PREVENT_ALLOW:
	case READ_10:
	case READ_12:
	case READ_CAPACITY:
	case REQUEST_SENSE:
	case REZERO_UNIT:
	case POSITION_TO_ELEMENT:	/* SEEK_10 */
	case SEND_DIAGNOSTIC:
	case WRITE_10:
	case WRITE_12:
	/* FORMAT_UNIT */
	/* MODE_SELECT */
	/* READ_FORMAT_CAPACITY */
	/* VERIFY */
	/* WRITE_AND_VERIFY */
		return 1;	/* success */
	default:
		return 0;	/* success */
	}
}

/*
 * 8070 specific functions
 */
Static int
umass_8070_transform(struct umass_softc *sc, unsigned char *cmd, int cmdlen,
		     unsigned char **rcmd, int *rcmdlen)
{
	return 0;	/* failure */
}

#endif /* __FreeBSD__ */


#ifdef UMASS_DEBUG
Static void
umass_bbb_dump_cbw(struct umass_softc *sc, umass_bbb_cbw_t *cbw)
{
	int clen = cbw->bCDBLength;
	int dlen = UGETDW(cbw->dCBWDataTransferLength);
	u_int8_t *c = cbw->CBWCDB;
	int tag = UGETDW(cbw->dCBWTag);
	int flags = cbw->bCBWFlags;

	DPRINTF(UDMASS_BBB, ("%s: CBW %d: cmd = %db "
		"(0x%02x%02x%02x%02x%02x%02x%s), "
		"data = %d bytes, dir = %s\n",
		USBDEVNAME(sc->sc_dev), tag, clen,
		c[0], c[1], c[2], c[3], c[4], c[5], (clen > 6? "...":""),
		dlen, (flags == CBWFLAGS_IN? "in":
		       (flags == CBWFLAGS_OUT? "out":"<invalid>"))));
}

Static void
umass_bbb_dump_csw(struct umass_softc *sc, umass_bbb_csw_t *csw)
{
	int sig = UGETDW(csw->dCSWSignature);
	int tag = UGETW(csw->dCSWTag);
	int res = UGETDW(csw->dCSWDataResidue);
	int status = csw->bCSWStatus;

	DPRINTF(UDMASS_BBB, ("%s: CSW %d: sig = 0x%08x (%s), tag = %d, "
		"res = %d, status = 0x%02x (%s)\n", USBDEVNAME(sc->sc_dev),
		tag, sig, (sig == CSWSIGNATURE?	 "valid":"invalid"),
		tag, res,
		status, (status == CSWSTATUS_GOOD? "good":
			 (status == CSWSTATUS_FAILED? "failed":
			  (status == CSWSTATUS_PHASE? "phase":"<invalid>")))));
}

Static void
umass_dump_buffer(struct umass_softc *sc, u_int8_t *buffer, int buflen,
		  int printlen)
{
	int i, j;
	char s1[40];
	char s2[40];
	char s3[5];

	s1[0] = '\0';
	s3[0] = '\0';

	sprintf(s2, " buffer=%p, buflen=%d", buffer, buflen);
	for (i = 0; i < buflen && i < printlen; i++) {
		j = i % 16;
		if (j == 0 && i != 0) {
			DPRINTF(UDMASS_GEN, ("%s: 0x %s%s\n",
				USBDEVNAME(sc->sc_dev), s1, s2));
			s2[0] = '\0';
		}
		sprintf(&s1[j*2], "%02x", buffer[i] & 0xff);
	}
	if (buflen > printlen)
		sprintf(s3, " ...");
	DPRINTF(UDMASS_GEN, ("%s: 0x %s%s%s\n",
		USBDEVNAME(sc->sc_dev), s1, s2, s3));
}
#endif








#if defined(__NetBSD__) || defined(__OpenBSD__)
Static int
umass_scsipi_cmd(struct scsipi_xfer *xs)
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct umass_softc *sc = sc_link->adapter_softc;
	struct scsipi_generic *cmd, trcmd;
	int cmdlen;
	int dir;

	DIF(UDMASS_UPPER, sc_link->flags |= DEBUGLEVEL);

	DPRINTF(UDMASS_CMD, ("%s: umass_scsi_cmd:  %d:%d xs=%p cmd=0x%02x "
	    "(quirks=0x%x, poll=%d)\n", USBDEVNAME(sc->sc_dev),
	    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun,
	    xs, xs->cmd->opcode, sc_link->quirks, 
	    xs->xs_control & XS_CTL_POLL));
#if defined(USB_DEBUG) && defined(SCSIDEBUG)
	if (umassdebug & UDMASS_SCSI)
		show_scsipi_xs(xs);
	else if (umassdebug & ~UDMASS_CMD)
		show_scsipi_cmd(xs);
#endif

	if (sc->sc_dying) {
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

#ifdef UMASS_DEBUG
	if ((sc_link->type == BUS_ATAPI ? 
	     sc_link->scsipi_atapi.drive : sc_link->scsipi_scsi.target) 
	    != UMASS_SCSIID_DEVICE) {
		DPRINTF(UDMASS_SCSI, ("%s: wrong SCSI ID %d\n",
		    USBDEVNAME(sc->sc_dev),
		    sc_link->scsipi_scsi.target));
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	if (xs->cmd->opcode == SCSI_MODE_SENSE &&
	    (sc_link->quirks & SDEV_NOMODESENSE)) {
		/*printf("%s: SCSI_MODE_SENSE\n", USBDEVNAME(sc->sc_dev));*/
		xs->error = XS_TIMEOUT;
		goto done;
	}

	if (xs->cmd->opcode == START_STOP &&
	    (sc->quirks & NO_START_STOP)) {
		/*printf("%s: START_STOP\n", USBDEVNAME(sc->sc_dev));*/
		xs->error = XS_NOERROR;
		goto done;
	}

	dir = DIR_NONE;
	if (xs->datalen) {
		switch (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		case XS_CTL_DATA_IN:
			dir = DIR_IN;
			break;
		case XS_CTL_DATA_OUT:
			dir = DIR_OUT;
			break;
		}
	}

	if (xs->datalen > UMASS_MAX_TRANSFER_SIZE) {
		printf("umass_cmd: large datalen, %d\n", xs->datalen);
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

	cmd = xs->cmd;
	cmdlen = xs->cmdlen;
	if (sc->proto & PROTO_UFI) {
		if (!umass_ufi_transform(sc, cmd, cmdlen, &trcmd, &cmdlen)) {
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}
		cmd = &trcmd;

	}

	if (xs->xs_control & XS_CTL_POLL) {
		/* Use sync transfer. XXX Broken! */
		DPRINTF(UDMASS_SCSI, ("umass_scsi_cmd: sync dir=%d\n", dir));
		sc->sc_xfer_flags = USBD_SYNCHRONOUS;
		sc->sc_sync_status = USBD_INVAL;
		sc->transfer(sc, sc_link->scsipi_scsi.lun, cmd, cmdlen,
			     xs->data, xs->datalen, dir, 0, xs);
		sc->sc_xfer_flags = 0;
		DPRINTF(UDMASS_SCSI, ("umass_scsi_cmd: done err=%d\n", 
				      sc->sc_sync_status));
		switch (sc->sc_sync_status) {
		case USBD_NORMAL_COMPLETION:
			xs->error = XS_NOERROR;
			break;
		case USBD_TIMEOUT:
			xs->error = XS_TIMEOUT;
			break;
		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		goto done;
	} else {
		DPRINTF(UDMASS_SCSI, ("umass_scsi_cmd: async dir=%d, cmdlen=%d"
				      " datalen=%d\n",
				      dir, cmdlen, xs->datalen));
		sc->transfer(sc, sc_link->scsipi_scsi.lun, cmd, cmdlen,
		    xs->data, xs->datalen, dir, umass_scsipi_cb, xs);
		return (SUCCESSFULLY_QUEUED);
	}

	/* Return if command finishes early. */
 done:
	xs->xs_status |= XS_STS_DONE;
	scsipi_done(xs);
	if (xs->xs_control & XS_CTL_POLL)
		return (COMPLETE);
	else
		return (SUCCESSFULLY_QUEUED);
}

Static void
umass_scsipi_minphys(struct buf *bp)
{
	if (bp->b_bcount > UMASS_MAX_TRANSFER_SIZE)
		bp->b_bcount = UMASS_MAX_TRANSFER_SIZE;
	minphys(bp);
}

int
umass_scsipi_ioctl(struct scsipi_link *link, u_long cmd, caddr_t arg,
		   int flag, struct proc *p)
{
	/*struct umass_softc *sc = link->adapter_softc;*/

	switch (cmd) {
#if 0
	case SCBUSIORESET:
		ccb->ccb_h.status = CAM_REQ_INPROG;
		umass_reset(sc, umass_cam_cb, (void *) ccb);
		return (0);
#endif
	default:
		return (ENOTTY);
	}
}

Static int
umass_scsipi_getgeom(struct scsipi_link *sc_link, struct disk_parms *dp,
		     u_long sectors)
{
	struct umass_softc *sc = sc_link->adapter_softc;

	/* If it's not a floppy, we don't know what to do. */
	if (!(sc->proto & PROTO_UFI))
		return (0);

	switch (sectors) {
	case 1440:
		/* Most likely a single density 3.5" floppy. */
		dp->heads = 2;
		dp->sectors = 9;
		dp->cyls = 80;
		return (1);
	case 2880:
		/* Most likely a double density 3.5" floppy. */
		dp->heads = 2;
		dp->sectors = 18;
		dp->cyls = 80;
		return (1);
	default:
		return (0);
	}
}

Static void
umass_scsipi_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	struct scsipi_xfer *xs = priv;
	struct scsipi_link *sc_link = xs->sc_link;
	int cmdlen;
	int s;

	DPRINTF(UDMASS_CMD,("umass_scsipi_cb: xs=%p residue=%d status=%d\n",
		xs, residue, status));

	xs->resid = residue;

	switch (status) {
	case STATUS_CMD_OK:
		xs->error = XS_NOERROR;
		break;

	case STATUS_CMD_UNKNOWN:
	case STATUS_CMD_FAILED:
		/* fetch sense data */
		memset(&sc->sc_sense_cmd, 0, sizeof(sc->sc_sense_cmd));
		sc->sc_sense_cmd.opcode = REQUEST_SENSE;
		sc->sc_sense_cmd.byte2 = sc_link->scsipi_scsi.lun <<
		    SCSI_CMD_LUN_SHIFT;
		sc->sc_sense_cmd.length = sizeof(xs->sense);

		cmdlen = sizeof(sc->sc_sense_cmd);
		if (sc->proto & PROTO_UFI) /* XXX */
			cmdlen = UFI_COMMAND_LENGTH;
		sc->transfer(sc, sc_link->scsipi_scsi.lun,
			     &sc->sc_sense_cmd, cmdlen,
			     &xs->sense, sizeof(xs->sense), DIR_IN,
			     umass_scsipi_sense_cb, xs);
		return;

	case STATUS_WIRE_FAILED:
		xs->error = XS_RESET;
		break;

	default:
		panic("%s: Unknown status %d in umass_scsipi_cb\n",
			USBDEVNAME(sc->sc_dev), status);
	}

	xs->xs_status |= XS_STS_DONE;

	DPRINTF(UDMASS_CMD,("umass_scsipi_cb: return xs->error=%d, "
		"xs->xs_status=0x%x xs->resid=%d\n", xs->error, xs->xs_status,
		xs->resid));

	s = splbio();
	scsipi_done(xs);
	splx(s);
}

/* 
 * Finalise a completed autosense operation
 */
Static void
umass_scsipi_sense_cb(struct umass_softc *sc, void *priv, int residue,
		      int status)
{
	struct scsipi_xfer *xs = priv;
	int s;

	DPRINTF(UDMASS_CMD,("umass_scsipi_sense_cb: xs=%p residue=%d "
		"status=%d\n", xs, residue, status));

	switch (status) {
	case STATUS_CMD_OK:
	case STATUS_CMD_UNKNOWN:
		/* getting sense data succeeded */
		if (xs->cmd->opcode == INQUIRY && (xs->resid < xs->datalen
		    || ((sc->quirks & RS_NO_CLEAR_UA) /* XXX */) )) {
			/*
			 * Some drivers return SENSE errors even after INQUIRY.
			 * The upper layer doesn't like that.
			 */
			xs->error = XS_NOERROR;
			break;
		}
		/* XXX look at residue */
		if (residue == 0 || residue == 14)/* XXX */
			xs->error = XS_SENSE;
		else
			xs->error = XS_SHORTSENSE;
		break;
	default:
		DPRINTF(UDMASS_SCSI, ("%s: Autosense failed, status %d\n",
			USBDEVNAME(sc->sc_dev), status));
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	xs->xs_status |= XS_STS_DONE;

	DPRINTF(UDMASS_CMD,("umass_scsipi_sense_cb: return xs->error=%d, "
		"xs->xs_status=0x%x xs->resid=%d\n", xs->error, xs->xs_status,
		xs->resid));

	s = splbio();
	scsipi_done(xs);
	splx(s);
}

/*
 * UFI specific functions
 */

Static int
umass_ufi_transform(struct umass_softc *sc, struct scsipi_generic *cmd, 
		    int cmdlen, struct scsipi_generic *rcmd, int *rcmdlen)
{
	*rcmdlen = UFI_COMMAND_LENGTH;
	memset(rcmd, 0, sizeof *rcmd);

	/* Handle any quirks */
	if (cmd->opcode == TEST_UNIT_READY
	    && (sc->quirks & NO_TEST_UNIT_READY)) {
		/*
		 * Some devices do not support this command.
		 * Start Stop Unit should give the same results
		 */
		DPRINTF(UDMASS_UFI, ("%s: Converted TEST_UNIT_READY "
			"to START_UNIT\n", USBDEVNAME(sc->sc_dev)));
		cmd->opcode = START_STOP;
		cmd->bytes[3] = SSS_START;
		return 1;
	} 

	switch (cmd->opcode) {
	/* Commands of which the format has been verified. They should work. */
	case TEST_UNIT_READY:
	case SCSI_REZERO_UNIT:
	case REQUEST_SENSE:
	case INQUIRY:
	case START_STOP:
	/*case SEND_DIAGNOSTIC: ??*/
	case PREVENT_ALLOW:
	case READ_CAPACITY:
	case READ_BIG:
	case WRITE_BIG:
	case POSITION_TO_ELEMENT:	/* SEEK_10 */
	case SCSI_MODE_SELECT_BIG:
	case SCSI_MODE_SENSE_BIG:
		/* Copy the command into the (zeroed out) destination buffer */
		memcpy(rcmd, cmd, cmdlen);
		return (1);	/* success */

	/* 
	 * Other UFI commands: FORMAT_UNIT, MODE_SELECT, READ_FORMAT_CAPACITY,
	 * VERIFY, WRITE_AND_VERIFY.
	 * These should be checked whether they somehow can be made to fit.
	 */

	/* These commands are known _not_ to work. They should be converted. */
	case SCSI_READ_COMMAND:
	case SCSI_WRITE_COMMAND:
	case SCSI_MODE_SENSE:
	case SCSI_MODE_SELECT:
	default:
		printf("%s: Unsupported UFI command 0x%02x",
			USBDEVNAME(sc->sc_dev), cmd->opcode);
		if (cmdlen == 6)
			printf(", 6 byte command should have been converted");
		printf("\n");
		return (0);	/* failure */
	}
}

#if NATAPIBUS > 0
Static void
umass_atapi_probedev(struct atapibus_softc *atapi, int target)
{
	struct scsipi_link *sc_link;
	struct scsipibus_attach_args sa;
	struct ata_drive_datas *drvp = &atapi->sc_drvs[target];
	char vendor[33], product[65], revision[17];
	struct scsipi_inquiry_data inqbuf;

	DPRINTF(UDMASS_SCSI,("umass_atapi_probedev: atapi=%p target=%d\n",
			     atapi, target));

	if (target != 0)	/* only probe drive 0 */
		return;

	if (atapi->sc_link[target])
		return;

	sc_link = malloc(sizeof(*sc_link), M_DEVBUF, M_NOWAIT); 
	if (sc_link == NULL) {
		printf("%s: can't allocate link for drive %d\n",
		       atapi->sc_dev.dv_xname, target);
		return;       
	}
	*sc_link = *atapi->adapter_link;

	DIF(UDMASS_UPPER, sc_link->flags |= DEBUGLEVEL);

	/* Fill generic parts of the link. */
	sc_link->active = 0;
	sc_link->scsipi_atapi.drive = target;
	sc_link->device = &umass_dev;
	TAILQ_INIT(&sc_link->pending_xfers);

	DPRINTF(UDMASS_SCSI, ("umass_atapi_probedev: doing inquiry\n"));
	/* Now go ask the device all about itself. */
	memset(&inqbuf, 0, sizeof(inqbuf));
	if (scsipi_inquire(sc_link, &inqbuf, XS_CTL_DISCOVERY) != 0)
		goto bad;

	scsipi_strvis(vendor, 33, inqbuf.vendor, 8);
	scsipi_strvis(product, 65, inqbuf.product, 16);
	scsipi_strvis(revision, 17, inqbuf.revision, 4);

	sa.sa_sc_link = sc_link;
	sa.sa_inqbuf.type = inqbuf.device;
	sa.sa_inqbuf.removable = inqbuf.dev_qual2 & SID_REMOVABLE ?
	    T_REMOV : T_FIXED;
	if (sa.sa_inqbuf.removable)
		sc_link->flags |= SDEV_REMOVABLE;
	/* XXX how? sc_link->scsipi_atapi.cap |= ACAP_LEN;*/
	sa.sa_inqbuf.vendor = vendor;
	sa.sa_inqbuf.product = product;
	sa.sa_inqbuf.revision = revision;
	sa.sa_inqptr = NULL;

	drvp->drv_softc = atapi_probedev(atapi, target, sc_link, &sa);
	/* atapi_probedev() frees the scsipi_link when there is no device. */
	return;

bad:
	free(sc_link, M_DEVBUF);
	return;
}
#endif
#endif
