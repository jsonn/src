/*	$NetBSD: i2o.h,v 1.1.2.2 2000/11/22 17:34:20 bouyer Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * I2O structures and constants, as presented by the I2O specification
 * revision 1.5 (obtainable from http://www.i2osig.org/).  Currently, only
 * what's useful to us is defined in this file.
 */

#ifndef	_I2O_I2O_H_
#define	_I2O_I2O_H_

/*
 * ================= Miscellenous definitions =================
 */

/* Macros to assist in building message headers */
#define	I2O_MSGFLAGS(s)		(I2O_VERSION_11 | (sizeof(struct s) << 14))
#define	I2O_MSGFUNC(t, f)	((t) | (I2O_TID_HOST << 12) | ((f) << 24))

/* Common message function codes with no payload or an undefined payload */
#define	I2O_UTIL_NOP			0x00
#define	I2O_EXEC_IOP_CLEAR		0xbe
#define	I2O_EXEC_SYS_QUIESCE		0xc3
#define	I2O_EXEC_SYS_ENABLE		0xd1
#define	I2O_PRIVATE_MESSAGE		0xff

/* Device class codes */
#define	I2O_CLASS_EXECUTIVE			0x00
#define	I2O_CLASS_DDM				0x01
#define	I2O_CLASS_RANDOM_BLOCK_STORAGE		0x10
#define	I2O_CLASS_SEQUENTIAL_STORAGE		0x11
#define	I2O_CLASS_LAN				0x20
#define	I2O_CLASS_WAN				0x30
#define	I2O_CLASS_FIBRE_CHANNEL_PORT		0x40
#define	I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL	0x41
#define	I2O_CLASS_SCSI_PERIPHERAL		0x51
#define	I2O_CLASS_ATE_PORT			0x60
#define	I2O_CLASS_ATE_PERIPHERAL		0x61
#define	I2O_CLASS_FLOPPY_CONTROLLER		0x70
#define	I2O_CLASS_FLOPPY_DEVICE			0x71
#define	I2O_CLASS_BUS_ADAPTER_PORT		0x80

#define	I2O_CLASS_ANY				0xffffffff

/* Reply status codes */
#define	I2O_STATUS_SUCCESS			0x00
#define	I2O_STATUS_ABORT_DIRTY			0x01
#define	I2O_STATUS_ABORT_NO_DATA_XFER		0x02
#define	I2O_STATUS_ABORT_PARTIAL_XFER		0x03
#define	I2O_STATUS_ERROR_DIRTY			0x04
#define	I2O_STATUS_ERROR_NO_DATA_XFER		0x05
#define	I2O_STATUS_ERROR_PARTIAL_XFER		0x06
#define	I2O_STATUS_PROCESS_ABORT_DIRTY        	0x08
#define	I2O_STATUS_PROCESS_ABORT_NO_DATA_XFER	0x09
#define	I2O_STATUS_PROCESS_ABORT_PARTIAL_XFER	0x0a
#define	I2O_STATUS_TRANSACTION_ERROR		0x0b
#define	I2O_STATUS_PROGRESS_REPORT		0x80

/* Message versions */
#define	I2O_VERSION_10			0x00
#define	I2O_VERSION_11			0x01
#define	I2O_VERSION_20			0x02

/* Commonly used TIDs */
#define	I2O_TID_IOP			0
#define	I2O_TID_HOST			1
#define	I2O_TID_NONE			4095

/* SGL flags.  This list covers only a fraction of the possibilities. */
#define	I2O_SGL_DATA_OUT		0x04000000
#define	I2O_SGL_SIMPLE			0x10000000
#define	I2O_SGL_END_BUFFER		0x40000000
#define	I2O_SGL_END			0x80000000

/* Serial number formats */
#define	I2O_SNFMT_UNKNOWN		0
#define	I2O_SNFMT_BINARY		1
#define	I2O_SNFMT_ASCII			2
#define	I2O_SNFMT_UNICODE		3
#define	I2O_SNFMT_LAN_MAC		4
#define	I2O_SNFMT_WAN_MAC		5

/*
 * ================= Common structures =================
 */

/*
 * Standard I2O message frame.  All message frames begin with this.
 *
 * Bits  Field          Meaning
 * ----  -------------  ----------------------------------------------------
 * 0-2   msgflags       Message header version. Must be 001 (little endian).
 * 3     msgflags	Reserved.
 * 4-7   msgflags       Offset to SGLs expressed as # of 32-bit words.
 * 8-15  msgflags       Control flags.
 * 16-31 msgflags       Message frame size expressed as # of 32-bit words.
 * 0-11  msgfunc	TID of target.
 * 12-23 msgfunc        TID of initiator.
 * 24-31 msgfunc        Function (i.e., type of message).
 */
struct i2o_msg {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;	/* Initiator context */
	u_int32_t	msgtctx;	/* Transaction context */

	/* Message payload */

} __attribute__ ((__packed__));

#define	I2O_MSGFLAGS_STATICMF		0x0100
#define	I2O_MSGFLAGS_64BIT		0x0200
#define	I2O_MSGFLAGS_MULTI		0x1000
#define	I2O_MSGFLAGS_CANT_PROCESS	0x2000
#define	I2O_MSGFLAGS_LAST_REPLY		0x4000
#define	I2O_MSGFLAGS_REPLY		0x8000

/*
 * Standard reply frame.  msgflags, msgfunc, msgictx and msgtctx have the
 * same meaning as in `struct i2o_msg'.
 *
 * Bits  Field          Meaning
 * ----  -------------  ----------------------------------------------------
 * 0-15  status         Detailed status code.  Specific to device class.
 * 16-23 status         Reserved.
 * 24-31 status         Request status code.
 */
struct i2o_reply {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int16_t	detail;
	u_int8_t	reserved;
	u_int8_t	reqstatus;

	/* Reply payload */

} __attribute__ ((__packed__));

/*
 * Hardware resource table.  Not documented here.
 */
struct i2o_hrt_entry {
	u_int32_t	adapterid;
	u_int32_t	controllingtid;
	u_int8_t	businfo[8];
} __attribute__ ((__packed__));

struct i2o_hrt {
	u_int16_t	nentries;
	u_int8_t	entrysize;
	u_int8_t	hrtversion;
	u_int32_t	changeindicator;
	struct i2o_hrt_entry	entry[1];
} __attribute__ ((__packed__));

/*
 * Logical configuration table entry.  Bitfields are broken down as follows:
 *
 * Bits   Field           Meaning
 * -----  --------------  ---------------------------------------------------
 *  0-11  classid         Class ID.
 * 12-15  classid         Class version.
 *  0-11  usertid         User TID
 * 12-23  usertid         Parent TID.
 * 24-31  usertid         BIOS info.
 */
struct i2o_lct_entry {
	u_int16_t	entrysize;
	u_int16_t	localtid;		/* Bits 0-12 only */
	u_int32_t	changeindicator;
	u_int32_t	deviceflags;
	u_int16_t	classid;
	u_int16_t	orgid;
	u_int32_t	subclassinfo;
	u_int32_t	usertid;
	u_int8_t	identitytag[8];
	u_int32_t	eventcaps;
} __attribute__ ((__packed__));

/*
 * Logical configuration table header.
 */
struct i2o_lct {
	u_int16_t	tablesize;
	u_int8_t	boottid;
	u_int8_t	lctversion;
	u_int32_t	iopflags;
	u_int32_t	changeindicator;
	struct i2o_lct_entry	entry[1];
} __attribute__ ((__packed__));

/*
 * IOP system table entry.  Bitfields are broken down as follows:
 *
 * Bits   Field           Meaning
 * -----  --------------  ---------------------------------------------------
 *  0-11  iopid           IOP ID.
 * 12-31  iopid           Reserved.
 *  0-11  segnumber       Segment number.
 * 12-15  segnumber       I2O version.
 * 16-23  segnumber       IOP state.
 * 24-31  segnumber       Messenger type.
 */
struct i2o_iop_entry {
	u_int16_t	orgid;
	u_int16_t	reserved0;
	u_int32_t	iopid;
	u_int32_t	segnumber;			/* Bitfields */
	u_int16_t	inboundmsgframesize;
	u_int16_t	reserved1;
	u_int32_t	lastchanged;
	u_int32_t	iopcaps;
	u_int32_t	inboundmsgportaddresslow;
	u_int32_t	inboundmsgportaddresshigh;
} __attribute__ ((__packed__));

/*
 * IOP status record.  Bitfields are broken down as follows:
 *
 * Bits   Field           Meaning
 * -----  --------------  ---------------------------------------------------
 *  0-11  iopid           IOP ID.
 * 12-15  iopid           Reserved.
 * 16-31  iopid           Host unit ID.
 *  0-11  segnumber       Segment number.
 * 12-15  segnumber       I2O version.
 * 16-23  segnumber       IOP state.
 * 24-31  segnumber       Messenger type.
 */
struct i2o_status {
	u_int16_t	orgid;
	u_int16_t	reserved0;
	u_int32_t	iopid;			/* Bitfields */
	u_int32_t	segnumber;		/* Bitfields */
	u_int16_t	inboundmframesize;
	u_int8_t	initcode;
	u_int8_t	reserved1;
	u_int32_t	maxinboundmframes;
	u_int32_t	currentinboundmframes;
	u_int32_t	maxoutboundmframes;
	u_int8_t	productid[24];
	u_int32_t	expectedlctsize;
	u_int32_t	iopcaps;
	u_int32_t	desiredprivmemsize;
	u_int32_t	currentprivmemsize;
	u_int32_t	currentprivmembase;
	u_int32_t	desiredpriviosize;
	u_int32_t	currentpriviosize;
	u_int32_t	currentpriviobase;
	u_int8_t	reserved2[3];
	u_int8_t	syncbyte;
} __attribute__ ((__packed__));

#define	I2O_IOP_STATE_INITIALIZING		0x01
#define	I2O_IOP_STATE_RESET			0x02
#define	I2O_IOP_STATE_HOLD			0x04
#define	I2O_IOP_STATE_READY			0x05
#define	I2O_IOP_STATE_OPERATIONAL		0x08
#define	I2O_IOP_STATE_FAILED			0x10
#define	I2O_IOP_STATE_FAULTED			0x11

/*
 * ================= Executive class messages =================
 */

/*
 * Retrieve adapter status.
 */
#define	I2O_EXEC_STATUS_GET		0xa0
struct i2o_exec_status_get {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	reserved[4];
	u_int32_t	addrlow;
	u_int32_t	addrhigh;
	u_int32_t	length;
} __attribute__ ((__packed__));

/*
 * Initalize outbound FIFO.
 */
#define	I2O_EXEC_OUTBOUND_INIT		0xa1
struct i2o_exec_outbound_init {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	pagesize;
	u_int32_t	flags;		/* init code, outbound msg size */
} __attribute__ ((__packed__));

#define	I2O_EXEC_OUTBOUND_INIT_IN_PROGRESS	1
#define	I2O_EXEC_OUTBOUND_INIT_REJECTED		2
#define	I2O_EXEC_OUTBOUND_INIT_FAILED		3
#define	I2O_EXEC_OUTBOUND_INIT_COMPLETE		4

/*
 * Notify host of LCT change.
 */
#define	I2O_EXEC_LCT_NOTIFY		0xa2
struct i2o_exec_lct_notify {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	classid;
	u_int32_t	changeindicator;
} __attribute__ ((__packed__));

/*
 * Set system table.
 */
#define	I2O_EXEC_SYS_TAB_SET		0xa3
struct i2o_exec_sys_tab_set {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	iopid;
	u_int32_t	segnumber;
} __attribute__ ((__packed__));

/*
 * Retrieve hardware resource table.
 */
#define	I2O_EXEC_HRT_GET		0xa8
struct i2o_exec_hrt_get {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
} __attribute__ ((__packed__));

/*
 * Reset IOP.
 */
#define	I2O_EXEC_IOP_RESET		0xbd
struct i2o_exec_iop_reset {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	reserved[4];
	u_int32_t	statuslow;
	u_int32_t	statushigh;
} __attribute__ ((__packed__));

#define	I2O_RESET_IN_PROGRESS		0x01
#define	I2O_RESET_REJECTED		0x02

/*
 * ================= HBA class messages =================
 */

#define	I2O_HBA_BUS_SCAN		0x89
struct i2o_hba_bus_scan {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
};

/*
 * ================= HBA class parameter groups =================
 */

#define	I2O_PARAM_HBA_CTLR_INFO		0x0000
struct i2o_param_hba_ctlr_info {
	u_int8_t	bustype;
	u_int8_t	busstate;
	u_int16_t	reserved;
	u_int8_t	busname[12];
} __attribute__ ((__packed__));

#define	I2O_HBA_BUS_GENERIC		0x00
#define	I2O_HBA_BUS_SCSI		0x01
#define	I2O_HBA_BUS_FCA			0x10

#define	I2O_PARAM_HBA_SCSI_PORT_INFO	0x0001
struct i2o_param_hba_scsi_port_info {
	u_int8_t	physicalif;
	u_int8_t	electricalif;
	u_int8_t	isosynchonrous;
	u_int8_t	connectortype;
	u_int8_t	connectorgender;
	u_int8_t	reserved1;
	u_int16_t	reserved2;
	u_int32_t	maxnumberofdevices;
} __attribute__ ((__packed__));

#define	I2O_PARAM_HBA_SCSI_CTLR_INFO	0x0200
struct i2o_param_hba_scsi_ctlr_info {
	u_int8_t	scsitype;
	u_int8_t	protection;
	u_int8_t	settings;
	u_int8_t	reserved;
	u_int32_t	initiatorid;
	u_int64_t	scanlun0only;
	u_int16_t	disabledevice;
	u_int8_t	maxoffset;
	u_int8_t	maxdatawidth;
	u_int64_t	maxsyncrate;
} __attribute__ ((__packed__));

/*
 * ================= Utility messages =================
 */

/*
 * Get parameter group operation.
 */
#define	I2O_UTIL_PARAMS_GET		0x06
struct i2o_util_params_get {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	flags;
} __attribute__ ((__packed__));

#define	I2O_PARAMS_OP_FIELD_GET		1
#define	I2O_PARAMS_OP_LIST_GET		2
#define	I2O_PARAMS_OP_MORE_GET		3
#define	I2O_PARAMS_OP_SIZE_GET		4
#define	I2O_PARAMS_OP_TABLE_GET		5
#define	I2O_PARAMS_OP_FIELD_SET		6
#define	I2O_PARAMS_OP_LIST_SET		7
#define	I2O_PARAMS_OP_ROW_ADD		8
#define	I2O_PARAMS_OP_ROW_DELETE	9
#define	I2O_PARAMS_OP_TABLE_CLEAR	10

struct i2o_param_op_list_header {
	u_int16_t	count;
	u_int16_t	reserved;
} __attribute__ ((__packed__));

struct i2o_param_op_all_template {
	u_int16_t	operation;
	u_int16_t	group;
	u_int16_t	fieldcount;
} __attribute__ ((__packed__));

struct i2o_param_op_results {
	u_int16_t	count;
	u_int16_t	reserved;
} __attribute__ ((__packed__));

struct i2o_param_read_results {
	u_int16_t	blocksize;
	u_int8_t	blockstatus;
	u_int8_t	errorinfosize;
} __attribute__ ((__packed__));

/*
 * Device claim.
 */
#define	I2O_UTIL_CLAIM			0x09
struct i2o_util_claim {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	flags;
} __attribute__ ((__packed__));

#define	I2O_UTIL_CLAIM_RESET_SENSITIVE		0x00000002
#define	I2O_UTIL_CLAIM_STATE_SENSITIVE		0x00000004
#define	I2O_UTIL_CLAIM_CAPACITY_SENSITIVE	0x00000008
#define	I2O_UTIL_CLAIM_NO_PEER_SERVICE		0x00000010
#define	I2O_UTIL_CLAIM_NO_MANAGEMENT_SERVICE	0x00000020
#define	I2O_UTIL_CLAIM_PRIMARY_USER		0x01000000
#define	I2O_UTIL_CLAIM_AUTHORIZED_USER		0x02000000
#define	I2O_UTIL_CLAIM_SECONDARY_USER		0x03000000
#define	I2O_UTIL_CLAIM_MANAGEMENT_USER		0x04000000

/*
 * ================= Utility parameter groups =================
 */

#define	I2O_PARAM_DEVICE_IDENTITY	0xf100
struct i2o_param_device_identity {
	u_int32_t	classid;
	u_int16_t	ownertid;
	u_int16_t	parenttid;
	u_int8_t	vendorinfo[16];
	u_int8_t	productinfo[16];
	u_int8_t	description[16];
	u_int8_t	revlevel[8];
	u_int8_t	snformat;
	u_int8_t	serialnumber[1];
} __attribute__ ((__packed__));

#define	I2O_PARAM_DDM_IDENTITY		0xf101
struct i2o_param_ddm_identity {
	u_int16_t	ddmtid;
	u_int8_t	name[24];
	u_int8_t	revlevel[8];
	u_int8_t	snformat;
	u_int8_t	serialnumber[12];
} __attribute__ ((__packed__));

/*
 * ================= Block storage class messages =================
 */

#define	I2O_RBS_BLOCK_READ		0x30
struct i2o_rbs_block_read {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	flags;		/* flags, time multipler, read ahead */
	u_int32_t	datasize;
	u_int32_t	lowoffset;
	u_int32_t	highoffset;
} __attribute__ ((__packed__));

#define	I2O_RBS_BLOCK_READ_NO_RETRY	0x01
#define	I2O_RBS_BLOCK_READ_SOLO		0x02
#define	I2O_RBS_BLOCK_READ_CACHE_READ	0x04
#define	I2O_RBS_BLOCK_READ_PREFETCH	0x08
#define	I2O_RBS_BLOCK_READ_CACHE_ONLY	0x10

#define	I2O_RBS_BLOCK_WRITE             0x31
struct i2o_rbs_block_write {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	flags;		/* flags, time multipler */
	u_int32_t	datasize;
	u_int32_t	lowoffset;
	u_int32_t	highoffset;
} __attribute__ ((__packed__));

#define	I2O_RBS_BLOCK_WRITE_NO_RETRY	0x01
#define	I2O_RBS_BLOCK_WRITE_SOLO	0x02
#define	I2O_RBS_BLOCK_WRITE_CACHE_NONE	0x04
#define	I2O_RBS_BLOCK_WRITE_CACHE_WT	0x08
#define	I2O_RBS_BLOCK_WRITE_CACHE_WB	0x10

#define	I2O_RBS_CACHE_FLUSH             0x37
struct i2o_rbs_cache_flush {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	flags;		/* flags, time multipler */
} __attribute__ ((__packed__));

#define	I2O_RBS_MEDIA_MOUNT		0x41
struct i2o_rbs_media_mount {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	mediaid;
	u_int32_t	loadflags;
} __attribute__ ((__packed__));

#define	I2O_RBS_MEDIA_EJECT             0x43
struct i2o_rbs_media_eject {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	mediaid;
} __attribute__ ((__packed__));

#define	I2O_RBS_MEDIA_LOCK		0x49
struct i2o_rbs_media_lock {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	mediaid;
} __attribute__ ((__packed__));

#define	I2O_RBS_MEDIA_UNLOCK		0x4b
struct i2o_rbs_media_unlock {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	mediaid;
} __attribute__ ((__packed__));

/* Standard RBS reply frame. */
struct i2o_rbs_reply {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int16_t	detail;
	u_int8_t	reserved;
	u_int8_t	reqstatus;
	u_int32_t	transfercount;
	u_int64_t	offset;		/* Error replies only */
} __attribute__ ((__packed__));

/*
 * ================= Block storage class parameter groups =================
 */

/*
 * Device information.
 */
#define	I2O_PARAM_RBS_DEVICE_INFO	0x0000
struct i2o_param_rbs_device_info {
	u_int8_t	type;
	u_int8_t	npaths;
	u_int16_t	powerstate;
	u_int32_t	blocksize;
	u_int64_t	capacity;
	u_int32_t	capabilities;
	u_int32_t	state;
} __attribute__ ((__packed__));

#define	I2O_RBS_TYPE_DIRECT		0x00
#define	I2O_RBS_TYPE_WORM		0x04
#define	I2O_RBS_TYPE_CDROM		0x05
#define	I2O_RBS_TYPE_OPTICAL		0x07

#define	I2O_RBS_CAP_CACHING		0x00000001
#define	I2O_RBS_CAP_MULTI_PATH		0x00000002
#define	I2O_RBS_CAP_DYNAMIC_CAPACITY	0x00000004
#define	I2O_RBS_CAP_REMOVEABLE_MEDIA	0x00000008
#define	I2O_RBS_CAP_REMOVEABLE_DEVICE	0x00000010
#define	I2O_RBS_CAP_READ_ONLY		0x00000020
#define	I2O_RBS_CAP_LOCKOUT		0x00000040
#define	I2O_RBS_CAP_BOOT_BYPASS		0x00000080
#define	I2O_RBS_CAP_COMPRESSION		0x00000100
#define	I2O_RBS_CAP_DATA_SECURITY	0x00000200
#define	I2O_RBS_CAP_RAID		0x00000400

#define	I2O_RBS_STATE_CACHING		0x00000001
#define	I2O_RBS_STATE_POWERED_ON	0x00000002
#define	I2O_RBS_STATE_READY		0x00000004
#define	I2O_RBS_STATE_MEDIA_LOADED	0x00000008
#define	I2O_RBS_STATE_DEVICE_LOADED	0x00000010
#define	I2O_RBS_STATE_READ_ONLY		0x00000020
#define	I2O_RBS_STATE_LOCKOUT		0x00000040
#define	I2O_RBS_STATE_BOOT_BYPASS	0x00000080
#define	I2O_RBS_STATE_COMPRESSION	0x00000100
#define	I2O_RBS_STATE_DATA_SECURITY	0x00000200
#define	I2O_RBS_STATE_RAID		0x00000400

/*
 * Cache control.
 */
#define	I2O_PARAM_RBS_CACHE_CONTROL	0x0003
struct i2o_param_rbs_cache_control {
	u_int32_t	totalcachesize;
	u_int32_t	readcachesize;
	u_int32_t	writecachesize;
	u_int8_t	writepolicy;
	u_int8_t	readpolicy;
	u_int8_t	errorcorrection;
	u_int8_t	reserved;
} __attribute__ ((__packed__));

/*
 * ================= SCSI peripheral class messages =================
 */

/*
 * Reset SCSI device. 
 */
#define	I2O_SCSI_DEVICE_RESET		0x27
struct i2o_scsi_device_reset {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
} __attribute__ ((__packed__));

/*
 * Execute SCSI command. 
 */
#define	I2O_SCSI_SCB_EXEC		0x81
struct i2o_scsi_scb_exec {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	flags;		/* CDB length and flags */
	u_int8_t	cdb[16];
	u_int32_t	datalen;
} __attribute__ ((__packed__));

#define	I2O_SCB_FLAG_SENSE_DATA_IN_MESSAGE  0x00200000
#define	I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER   0x00600000
#define	I2O_SCB_FLAG_SIMPLE_QUEUE_TAG       0x00800000
#define	I2O_SCB_FLAG_HEAD_QUEUE_TAG         0x01000000
#define	I2O_SCB_FLAG_ORDERED_QUEUE_TAG      0x01800000
#define	I2O_SCB_FLAG_ACA_QUEUE_TAG          0x02000000
#define	I2O_SCB_FLAG_ENABLE_DISCONNECT      0x20000000
#define	I2O_SCB_FLAG_XFER_FROM_DEVICE       0x40000000
#define	I2O_SCB_FLAG_XFER_TO_DEVICE         0x80000000

/*
 * Abort SCSI command.
 */
#define	I2O_SCSI_SCB_ABORT		0x83
struct i2o_scsi_scb_abort {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	tctxabort;
} __attribute__ ((__packed__));


/*
 * SCSI message reply frame.
 */
struct i2o_scsi_reply {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int16_t	detail;
	u_int8_t	reserved;
	u_int8_t	reqstatus;
	u_int32_t	datalen;
	u_int32_t	senselen;
	u_int8_t	sense[40];
} __attribute__ ((__packed__));

#define	I2O_SCSI_DSC_SUCCESS                0x00
#define	I2O_SCSI_DSC_REQUEST_ABORTED        0x02
#define	I2O_SCSI_DSC_UNABLE_TO_ABORT        0x03
#define	I2O_SCSI_DSC_COMPLETE_WITH_ERROR    0x04
#define	I2O_SCSI_DSC_ADAPTER_BUSY           0x05
#define	I2O_SCSI_DSC_REQUEST_INVALID        0x06
#define	I2O_SCSI_DSC_PATH_INVALID           0x07
#define	I2O_SCSI_DSC_DEVICE_NOT_PRESENT     0x08
#define	I2O_SCSI_DSC_UNABLE_TO_TERMINATE    0x09
#define	I2O_SCSI_DSC_SELECTION_TIMEOUT      0x0a
#define	I2O_SCSI_DSC_COMMAND_TIMEOUT        0x0b
#define	I2O_SCSI_DSC_MR_MESSAGE_RECEIVED    0x0d
#define	I2O_SCSI_DSC_SCSI_BUS_RESET         0x0e
#define	I2O_SCSI_DSC_PARITY_ERROR_FAILURE   0x0f
#define	I2O_SCSI_DSC_AUTOSENSE_FAILED       0x10
#define	I2O_SCSI_DSC_NO_ADAPTER             0x11
#define	I2O_SCSI_DSC_DATA_OVERRUN           0x12
#define	I2O_SCSI_DSC_UNEXPECTED_BUS_FREE    0x13
#define	I2O_SCSI_DSC_SEQUENCE_FAILURE       0x14
#define	I2O_SCSI_DSC_REQUEST_LENGTH_ERROR   0x15
#define	I2O_SCSI_DSC_PROVIDE_FAILURE        0x16
#define	I2O_SCSI_DSC_BDR_MESSAGE_SENT       0x17
#define	I2O_SCSI_DSC_REQUEST_TERMINATED     0x18
#define	I2O_SCSI_DSC_IDE_MESSAGE_SENT       0x33
#define	I2O_SCSI_DSC_RESOURCE_UNAVAILABLE   0x34
#define	I2O_SCSI_DSC_UNACKNOWLEDGED_EVENT   0x35
#define	I2O_SCSI_DSC_MESSAGE_RECEIVED       0x36
#define	I2O_SCSI_DSC_INVALID_CDB            0x37
#define	I2O_SCSI_DSC_LUN_INVALID            0x38
#define	I2O_SCSI_DSC_SCSI_TID_INVALID       0x39
#define	I2O_SCSI_DSC_FUNCTION_UNAVAILABLE   0x3a
#define	I2O_SCSI_DSC_NO_NEXUS               0x3b
#define	I2O_SCSI_DSC_SCSI_IID_INVALID       0x3c
#define	I2O_SCSI_DSC_CDB_RECEIVED           0x3d
#define	I2O_SCSI_DSC_LUN_ALREADY_ENABLED    0x3e
#define	I2O_SCSI_DSC_BUS_BUSY               0x3f
#define	I2O_SCSI_DSC_QUEUE_FROZEN           0x40

/*
 * ================= SCSI peripheral class parameter groups =================
 */

#define	I2O_PARAM_SCSI_DEVICE_INFO	0x0000
struct i2o_param_scsi_device_info {
	u_int8_t	devicetype;
	u_int8_t	flags;
	u_int16_t	reserved0;
	u_int32_t	identifier;
	u_int8_t	luninfo[8];
	u_int32_t	queuedepth;
	u_int8_t	reserved1;
	u_int8_t	negoffset;
	u_int8_t	negdatawidth;
	u_int8_t	reserved2;
	u_int64_t	negsyncrate;
} __attribute__ ((__packed__));

#endif	/* !defined _I2O_I2O_H_ */
