/* $NetBSD: isp_target.h,v 1.8.2.4 2001/03/27 15:31:59 bouyer Exp $ */
/*
 * This driver, which is contained in NetBSD in the files:
 *
 *	sys/dev/ic/isp.c
 *	sys/dev/ic/isp_inline.h
 *	sys/dev/ic/isp_netbsd.c
 *	sys/dev/ic/isp_netbsd.h
 *	sys/dev/ic/isp_target.c
 *	sys/dev/ic/isp_target.h
 *	sys/dev/ic/isp_tpublic.h
 *	sys/dev/ic/ispmbox.h
 *	sys/dev/ic/ispreg.h
 *	sys/dev/ic/ispvar.h
 *	sys/microcode/isp/asm_sbus.h
 *	sys/microcode/isp/asm_1040.h
 *	sys/microcode/isp/asm_1080.h
 *	sys/microcode/isp/asm_12160.h
 *	sys/microcode/isp/asm_2100.h
 *	sys/microcode/isp/asm_2200.h
 *	sys/pci/isp_pci.c
 *	sys/sbus/isp_sbus.c
 *
 * Is being actively maintained by Matthew Jacob (mjacob@netbsd.org).
 * This driver also is shared source with FreeBSD, OpenBSD, Linux, Solaris,
 * Linux versions. This tends to be an interesting maintenance problem.
 *
 * Please coordinate with Matthew Jacob on changes you wish to make here.
 */
/*
 * Qlogic Target Mode Structure and Flag Definitions
 *
 * Copyright (c) 1997, 1998
 * Patrick Stirling
 * pms@psconsult.com
 * All rights reserved.
 *
 * Additional Copyright (c) 1999, 2000, 2001
 * Matthew Jacob
 * mjacob@feral.com
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef	_ISP_TARGET_H
#define	_ISP_TARGET_H

/*
 * Defines for all entry types
 */
#define QLTM_SVALID	0x80
#define	QLTM_SENSELEN	18

/*
 * Structure for Enable Lun and Modify Lun queue entries
 */
typedef struct {
	isphdr_t	le_header;
	u_int32_t	le_reserved;
	u_int8_t	le_lun;
	u_int8_t	le_rsvd;
	u_int8_t	le_ops;		/* Modify LUN only */
	u_int8_t	le_tgt;		/* Not for FC */
	u_int32_t	le_flags;	/* Not for FC */
	u_int8_t	le_status;
	u_int8_t	le_reserved2;
	u_int8_t	le_cmd_count;
	u_int8_t	le_in_count;
	u_int8_t	le_cdb6len;	/* Not for FC */
	u_int8_t	le_cdb7len;	/* Not for FC */
	u_int16_t	le_timeout;
	u_int16_t	le_reserved3[20];
} lun_entry_t;

/*
 * le_flags values
 */
#define LUN_TQAE	0x00000001	/* bit1  Tagged Queue Action Enable */
#define LUN_DSSM	0x01000000	/* bit24 Disable Sending SDP Message */
#define	LUN_DISAD	0x02000000	/* bit25 Disable autodisconnect */
#define LUN_DM		0x40000000	/* bit30 Disconnects Mandatory */

/*
 * le_ops values
 */
#define LUN_CCINCR	0x01	/* increment command count */
#define LUN_CCDECR	0x02	/* decrement command count */
#define LUN_ININCR	0x40	/* increment immed. notify count */
#define LUN_INDECR	0x80	/* decrement immed. notify count */

/*
 * le_status values
 */
#define	LUN_OK		0x01	/* we be rockin' */
#define LUN_ERR		0x04	/* request completed with error */
#define LUN_INVAL	0x06	/* invalid request */
#define LUN_NOCAP	0x16	/* can't provide requested capability */
#define LUN_ENABLED	0x3E	/* LUN already enabled */

/*
 * Immediate Notify Entry structure
 */
#define IN_MSGLEN	8	/* 8 bytes */
#define IN_RSVDLEN	8	/* 8 words */
typedef struct {
	isphdr_t	in_header;
	u_int32_t	in_reserved;
	u_int8_t	in_lun;		/* lun */
	u_int8_t	in_iid;		/* initiator */
	u_int8_t	in_reserved2;
	u_int8_t	in_tgt;		/* target */
	u_int32_t	in_flags;
	u_int8_t	in_status;
	u_int8_t	in_rsvd2;
	u_int8_t	in_tag_val;	/* tag value */
	u_int8_t	in_tag_type;	/* tag type */
	u_int16_t	in_seqid;	/* sequence id */
	u_int8_t	in_msg[IN_MSGLEN];	/* SCSI message bytes */
	u_int16_t	in_reserved3[IN_RSVDLEN];
	u_int8_t	in_sense[QLTM_SENSELEN];/* suggested sense data */
} in_entry_t;

typedef struct {
	isphdr_t	in_header;
	u_int32_t	in_reserved;
	u_int8_t	in_lun;		/* lun */
	u_int8_t	in_iid;		/* initiator */
	u_int16_t	in_scclun;
	u_int32_t	in_reserved2;
	u_int16_t	in_status;
	u_int16_t	in_task_flags;
	u_int16_t	in_seqid;	/* sequence id */
} in_fcentry_t;

/*
 * Values for the in_status field
 */
#define	IN_REJECT	0x0D	/* Message Reject message received */
#define IN_RESET	0x0E	/* Bus Reset occurred */
#define IN_NO_RCAP	0x16	/* requested capability not available */
#define IN_IDE_RECEIVED	0x33	/* Initiator Detected Error msg received */
#define IN_RSRC_UNAVAIL	0x34	/* resource unavailable */
#define IN_MSG_RECEIVED	0x36	/* SCSI message received */
#define	IN_ABORT_TASK	0x20	/* task named in RX_ID is being aborted (FC) */
#define	IN_PORT_LOGOUT	0x29	/* port has logged out (FC) */
#define	IN_PORT_CHANGED	0x2A	/* port changed */
#define	IN_GLOBAL_LOGO	0x2E	/* all ports logged out */
#define	IN_NO_NEXUS	0x3B	/* Nexus not established */

/*
 * Values for the in_task_flags field- should only get one at a time!
 */
#define	TASK_FLAGS_ABORT_TASK		(1<<9)
#define	TASK_FLAGS_CLEAR_TASK_SET	(1<<10)
#define	TASK_FLAGS_TARGET_RESET		(1<<13)
#define	TASK_FLAGS_CLEAR_ACA		(1<<14)
#define	TASK_FLAGS_TERMINATE_TASK	(1<<15)

#ifndef	MSG_ABORT_TAG
#define	MSG_ABORT_TAG		0x06
#endif
#ifndef	MSG_CLEAR_QUEUE
#define	MSG_CLEAR_QUEUE		0x0e
#endif
#ifndef	MSG_BUS_DEV_RESET
#define	MSG_BUS_DEV_RESET	0x0b
#endif
#ifndef	MSG_REL_RECOVERY
#define	MSG_REL_RECOVERY	0x10
#endif
#ifndef	MSG_TERM_IO_PROC
#define	MSG_TERM_IO_PROC	0x11
#endif


/*
 * Notify Acknowledge Entry structure
 */
#define NA_RSVDLEN	22
typedef struct {
	isphdr_t	na_header;
	u_int32_t	na_reserved;
	u_int8_t	na_lun;		/* lun */
	u_int8_t	na_iid;		/* initiator */
	u_int8_t	na_reserved2;
	u_int8_t	na_tgt;		/* target */
	u_int32_t	na_flags;
	u_int8_t	na_status;
	u_int8_t	na_event;
	u_int16_t	na_seqid;	/* sequence id */
	u_int16_t	na_reserved3[NA_RSVDLEN];
} na_entry_t;

/*
 * Value for the na_event field
 */
#define NA_RST_CLRD	0x80	/* Clear an async event notification */
#define	NA_OK		0x01	/* Notify Acknowledge Succeeded */
#define	NA_INVALID	0x06	/* Invalid Notify Acknowledge */

#define	NA2_RSVDLEN	21
typedef struct {
	isphdr_t	na_header;
	u_int32_t	na_reserved;
	u_int8_t	na_lun;		/* lun */
	u_int8_t	na_iid;		/* initiator */
	u_int16_t	na_scclun;
	u_int16_t	na_flags;
	u_int16_t	na_reserved2;
	u_int16_t	na_status;
	u_int16_t	na_task_flags;
	u_int16_t	na_seqid;	/* sequence id */
	u_int16_t	na_reserved3[NA2_RSVDLEN];
} na_fcentry_t;
#define	NAFC_RCOUNT	0x80	/* increment resource count */
#define NAFC_RST_CLRD	0x20	/* Clear LIP Reset */
/*
 * Accept Target I/O Entry structure
 */
#define ATIO_CDBLEN	26

typedef struct {
	isphdr_t	at_header;
	u_int16_t	at_reserved;
	u_int16_t	at_handle;
	u_int8_t	at_lun;		/* lun */
	u_int8_t	at_iid;		/* initiator */
	u_int8_t	at_cdblen; 	/* cdb length */
	u_int8_t	at_tgt;		/* target */
	u_int32_t	at_flags;
	u_int8_t	at_status;	/* firmware status */
	u_int8_t	at_scsi_status;	/* scsi status */
	u_int8_t	at_tag_val;	/* tag value */
	u_int8_t	at_tag_type;	/* tag type */
	u_int8_t	at_cdb[ATIO_CDBLEN];	/* received CDB */
	u_int8_t	at_sense[QLTM_SENSELEN];/* suggested sense data */
} at_entry_t;

/*
 * at_flags values
 */
#define AT_NODISC	0x00008000	/* disconnect disabled */
#define AT_TQAE		0x00000001	/* Tagged Queue Action enabled */

/*
 * at_status values
 */
#define AT_PATH_INVALID	0x07	/* ATIO sent to firmware for disabled lun */
#define	AT_RESET	0x0E	/* SCSI Bus Reset Occurred */
#define AT_PHASE_ERROR	0x14	/* Bus phase sequence error */
#define AT_NOCAP	0x16	/* Requested capability not available */
#define AT_BDR_MSG	0x17	/* Bus Device Reset msg received */
#define AT_CDB		0x3D	/* CDB received */

/*
 * Accept Target I/O Entry structure, Type 2
 */
#define ATIO2_CDBLEN	16

typedef struct {
	isphdr_t	at_header;
	u_int32_t	at_reserved;
	u_int8_t	at_lun;		/* lun or reserved */
	u_int8_t	at_iid;		/* initiator */
	u_int16_t	at_rxid; 	/* response ID */
	u_int16_t	at_flags;
	u_int16_t	at_status;	/* firmware status */
	u_int8_t	at_reserved1;
	u_int8_t	at_taskcodes;
	u_int8_t	at_taskflags;
	u_int8_t	at_execodes;
	u_int8_t	at_cdb[ATIO2_CDBLEN];	/* received CDB */
	u_int32_t	at_datalen;		/* allocated data len */
	u_int16_t	at_scclun;	/* SCC Lun or reserved */
	u_int16_t	at_reserved2[10];
	u_int16_t	at_oxid;
} at2_entry_t;

#define	ATIO2_WWPN_OFFSET	0x2A
#define	ATIO2_OXID_OFFSET	0x3E

#define	ATIO2_TC_ATTR_MASK	0x7
#define	ATIO2_TC_ATTR_SIMPLEQ	0
#define	ATIO2_TC_ATTR_HEADOFQ	1
#define	ATIO2_TC_ATTR_ORDERED	2
#define	ATIO2_TC_ATTR_ACAQ	4
#define	ATIO2_TC_ATTR_UNTAGGED	5

/*
 * Continue Target I/O Entry structure
 * Request from driver. The response from the
 * ISP firmware is the same except that the last 18
 * bytes are overwritten by suggested sense data if
 * the 'autosense valid' bit is set in the status byte.
 */
typedef struct {
	isphdr_t	ct_header;
	u_int16_t	ct_reserved;
#define	ct_syshandle	ct_reserved	/* we use this */
	u_int16_t	ct_fwhandle;	/* required by f/w */
	u_int8_t	ct_lun;	/* lun */
	u_int8_t	ct_iid;	/* initiator id */
	u_int8_t	ct_reserved2;
	u_int8_t	ct_tgt;	/* our target id */
	u_int32_t	ct_flags;
	u_int8_t 	ct_status;	/* isp status */
	u_int8_t 	ct_scsi_status;	/* scsi status */
	u_int8_t 	ct_tag_val;	/* tag value */
	u_int8_t 	ct_tag_type;	/* tag type */
	u_int32_t	ct_xfrlen;	/* transfer length */
	u_int32_t	ct_resid;	/* residual length */
	u_int16_t	ct_timeout;
	u_int16_t	ct_seg_count;
	ispds_t		ct_dataseg[ISP_RQDSEG];
} ct_entry_t;

/*
 * For some of the dual port SCSI adapters, port (bus #) is reported
 * in the MSbit of ct_iid. Bit fields are a bit too awkward here.
 *
 * Note that this does not apply to FC adapters at all which can and
 * do report IIDs between 129 && 255 (these represent devices that have
 * logged in across a SCSI fabric).
 */
#define	GET_IID_VAL(x)		(x & 0x3f)
#define	GET_BUS_VAL(x)		((x >> 7) & 0x1)
#define	SET_IID_VAL(y, x)	(y | (x & 0x3f))
#define	SET_BUS_VAL(y, x)	(y | ((x & 0x1) << 7))

/*
 * ct_flags values
 */
#define CT_TQAE		0x00000001	/* bit  1, Tagged Queue Action enable */
#define CT_DATA_IN	0x00000040	/* bits 6&7, Data direction */
#define CT_DATA_OUT	0x00000080	/* bits 6&7, Data direction */
#define CT_NO_DATA	0x000000C0	/* bits 6&7, Data direction */
#define	CT_CCINCR	0x00000100	/* bit 8, autoincrement atio count */
#define CT_DATAMASK	0x000000C0	/* bits 6&7, Data direction */
#define	CT_INISYNCWIDE	0x00004000	/* bit 14, Do Sync/Wide Negotiation */
#define CT_NODISC	0x00008000	/* bit 15, Disconnects disabled */
#define CT_DSDP		0x01000000	/* bit 24, Disable Save Data Pointers */
#define CT_SENDRDP	0x04000000	/* bit 26, Send Restore Pointers msg */
#define CT_SENDSTATUS	0x80000000	/* bit 31, Send SCSI status byte */

/*
 * ct_status values
 * - set by the firmware when it returns the CTIO
 */
#define CT_OK		0x01	/* completed without error */
#define CT_ABORTED	0x02	/* aborted by host */
#define CT_ERR		0x04	/* see sense data for error */
#define CT_INVAL	0x06	/* request for disabled lun */
#define CT_NOPATH	0x07	/* invalid ITL nexus */
#define	CT_INVRXID	0x08	/* (FC only) Invalid RX_ID */
#define CT_RSELTMO	0x0A	/* reselection timeout after 2 tries */
#define CT_TIMEOUT	0x0B	/* timed out */
#define CT_RESET	0x0E	/* SCSI Bus Reset occurred */
#define	CT_PARITY	0x0F	/* Uncorrectable Parity Error */
#define	CT_PANIC	0x13	/* Unrecoverable Error */
#define CT_PHASE_ERROR	0x14	/* Bus phase sequence error */
#define CT_BDR_MSG	0x17	/* Bus Device Reset msg received */
#define CT_TERMINATED	0x19	/* due to Terminate Transfer mbox cmd */
#define	CT_PORTNOTAVAIL	0x28	/* port not available */
#define	CT_LOGOUT	0x29	/* port logout */
#define	CT_PORTCHANGED	0x2A	/* port changed */
#define	CT_IDE		0x33	/* Initiator Detected Error */
#define CT_NOACK	0x35	/* Outstanding Immed. Notify. entry */

/*
 * When the firmware returns a CTIO entry, it may overwrite the last
 * part of the structure with sense data. This starts at offset 0x2E
 * into the entry, which is in the middle of ct_dataseg[1]. Rather
 * than define a new struct for this, I'm just using the sense data
 * offset.
 */
#define CTIO_SENSE_OFFSET	0x2E

/*
 * Entry length in u_longs. All entries are the same size so
 * any one will do as the numerator.
 */
#define UINT32_ENTRY_SIZE	(sizeof(at_entry_t)/sizeof(u_int32_t))

/*
 * QLA2100 CTIO (type 2) entry
 */
#define	MAXRESPLEN	26
typedef struct {
	isphdr_t	ct_header;
	u_int16_t	ct_reserved;
	u_int16_t	ct_fwhandle;	/* just to match CTIO */
	u_int8_t	ct_lun;	/* lun */
	u_int8_t	ct_iid;	/* initiator id */
	u_int16_t	ct_rxid; /* response ID */
	u_int16_t	ct_flags;
	u_int16_t 	ct_status;	/* isp status */
	u_int16_t	ct_timeout;
	u_int16_t	ct_seg_count;
	u_int32_t	ct_reloff;	/* relative offset */
	int32_t		ct_resid;	/* residual length */
	union {
		/*
		 * The three different modes that the target driver
		 * can set the CTIO2 up as.
		 *
		 * The first is for sending FCP_DATA_IUs as well as
		 * (optionally) sending a terminal SCSI status FCP_RSP_IU.
		 *
		 * The second is for sending SCSI sense data in an FCP_RSP_IU.
		 * Note that no FCP_DATA_IUs will be sent.
		 *
		 * The third is for sending FCP_RSP_IUs as built specifically
		 * in system memory as located by the isp_dataseg.
		 */
		struct {
			u_int32_t _reserved;
			u_int16_t _reserved2;
			u_int16_t ct_scsi_status;
			u_int32_t ct_xfrlen;
			ispds_t ct_dataseg[ISP_RQDSEG_T2];
		} m0;
		struct {
			u_int16_t _reserved;
			u_int16_t _reserved2;
			u_int16_t ct_senselen;
			u_int16_t ct_scsi_status;
			u_int16_t ct_resplen;
			u_int8_t  ct_resp[MAXRESPLEN];
		} m1;
		struct {
			u_int32_t _reserved;
			u_int16_t _reserved2;
			u_int16_t _reserved3;
			u_int32_t ct_datalen;
			ispds_t ct_fcp_rsp_iudata;
		} m2;
		/*
		 * CTIO2 returned from F/W...
		 */
		struct {
			u_int32_t _reserved[4];
			u_int16_t ct_scsi_status;
			u_int8_t  ct_sense[QLTM_SENSELEN];
		} fw;
	} rsp;
} ct2_entry_t;

/*
 * ct_flags values for CTIO2
 */
#define	CT2_FLAG_MMASK	0x0003
#define	CT2_FLAG_MODE0	0x0000
#define	CT2_FLAG_MODE1	0x0001
#define	CT2_FLAG_MODE2	0x0002
#define CT2_DATA_IN	CT_DATA_IN
#define CT2_DATA_OUT	CT_DATA_OUT
#define CT2_NO_DATA	CT_NO_DATA
#define CT2_DATAMASK	CT_DATAMASK
#define	CT2_CCINCR	0x0100
#define	CT2_FASTPOST	0x0200
#define CT2_SENDSTATUS	0x8000

/*
 * ct_status values are (mostly) the same as that for ct_entry.
 */

/*
 * ct_scsi_status values- the low 8 bits are the normal SCSI status
 * we know and love. The upper 8 bits are validity markers for FCP_RSP_IU
 * fields.
 */
#define	CT2_RSPLEN_VALID	0x0100
#define	CT2_SNSLEN_VALID	0x0200
#define	CT2_DATA_OVER		0x0400
#define	CT2_DATA_UNDER		0x0800

/*
 * Macros for packing/unpacking the above structures
 */

#ifdef	__sparc__
#define	ISP_SBUS_SWOZZLE(isp, src, dst, taga, tagb)	\
	if (isp->isp_bustype == ISP_BT_SBUS) {	\
		u_int8_t tmp = src -> taga;	\
		dst -> taga =  dst -> tagb;	\
		src -> tagb =  tmp;		\
	} else { \
		dst -> taga =  src -> taga;	\
		dst -> tagb =  src -> taga;	\
	}
#else
#define	ISP_SBUS_SWOZZLE(isp, src, dst, taga, tagb)	\
		dst -> taga =  src -> taga;	\
		dst -> tagb =  src -> taga
#endif

#define	MCIDF(d, s)	if ((void *) d != (void *)s) MEMCPY(d, s, QENTRY_LEN)

/* This is really only for SBus cards on a sparc */
#ifdef	__sparc__
#define	ISP_SWIZ_ATIO(isp, vdst, vsrc)					\
{									\
	at_entry_t *src = (at_entry_t *) vsrc;				\
	at_entry_t *dst = (at_entry_t *) vdst;				\
	dst->at_header = src->at_header;				\
	dst->at_reserved = src->at_reserved;				\
	dst->at_handle = src->at_handle;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_lun, at_iid);		\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_cdblen, at_tgt);		\
	dst->at_flags = src->at_flags;					\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_status, at_scsi_status);	\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_tag_val, at_tag_type);	\
	MEMCPY(dst->at_cdb, src->at_cdb, ATIO_CDBLEN);			\
	MEMCPY(dst->at_sense, src->at_sense, QLTM_SENSELEN);		\
}
#define	ISP_SWIZ_ATIO2(isp, vdst, vsrc)					\
{									\
	at2_entry_t *src = (at2_entry_t *) vsrc;			\
	at2_entry_t *dst = (at2_entry_t *) vdst;			\
	dst->at_reserved = src->at_reserved;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_lun, at_iid);		\
	dst->at_rxid = src->at_rxid;					\
	dst->at_flags = src->at_flags;					\
	dst->at_status = src->at_status;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_reserved1, at_taskcodes);	\
	ISP_SBUS_SWOZZLE(isp, src, dst, at_taskflags, at_execodes);	\
	MEMCPY(dst->at_cdb, src->at_cdb, ATIO2_CDBLEN);			\
	dst->at_datalen = src->at_datalen;				\
	dst->at_scclun = src->at_scclun;				\
	MEMCPY(dst->at_reserved2, src->at_reserved2, sizeof dst->at_reserved2);\
	dst->at_oxid = src->at_oxid;					\
}
#define	ISP_SWIZ_CTIO(isp, vdst, vsrc)					\
{									\
	ct_entry_t *src = (ct_entry_t *) vsrc;				\
	ct_entry_t *dst = (ct_entry_t *) vdst;				\
	dst->ct_header = src->ct_header;				\
	dst->ct_syshandle = src->ct_syshandle;				\
	dst->ct_fwhandle = src->ct_fwhandle;				\
	dst->ct_fwhandle = src->ct_fwhandle;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, ct_lun, ct_iid);		\
	ISP_SBUS_SWOZZLE(isp, src, dst, ct_reserved2, ct_tgt);		\
	dst->ct_flags = src->ct_flags;					\
	ISP_SBUS_SWOZZLE(isp, src, dst, ct_status, ct_scsi_status);	\
	ISP_SBUS_SWOZZLE(isp, src, dst, ct_tag_val, ct_tag_type);	\
	dst->ct_xfrlen = src->ct_xfrlen;				\
	dst->ct_resid = src->ct_resid;					\
	dst->ct_timeout = src->ct_timeout;				\
	dst->ct_seg_count = src->ct_seg_count;				\
	MEMCPY(dst->ct_dataseg, src->ct_dataseg, sizeof (dst->ct_dataseg)); \
}
#define	ISP_SWIZ_CTIO2(isp, vdst, vsrc)					\
{									\
	ct2_entry_t *src = (ct2_entry_t *) vsrc;			\
	ct2_entry_t *dst = (ct2_entry_t *) vdst;			\
	dst->ct_header = src->ct_header;				\
	dst->ct_syshandle = src->ct_syshandle;				\
	dst->ct_fwhandle = src->ct_fwhandle;				\
	dst->ct_fwhandle = src->ct_fwhandle;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, ct_lun, ct_iid);		\
	dst->ct_rxid = src->ct_rxid;					\
	dst->ct_flags = src->ct_flags;					\
	dst->ct_status = src->ct_status;				\
	dst->ct_timeout = src->ct_timeout;				\
	dst->ct_seg_count = src->ct_seg_count;				\
	dst->ct_reloff = src->ct_reloff;				\
	dst->ct_resid = src->ct_resid;					\
	dst->rsp = src->rsp;						\
}
#define	ISP_SWIZ_ENABLE_LUN(isp, vdst, vsrc)				\
{									\
	lun_entry_t *src = (lun_entry_t *)vsrc;				\
	lun_entry_t *dst = (lun_entry_t *)vdst;				\
	dst->le_header = src->le_header;				\
	dst->le_reserved2 = src->le_reserved2;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, le_lun, le_rsvd);		\
	ISP_SBUS_SWOZZLE(isp, src, dst, le_ops, le_tgt);		\
	dst->le_flags = src->le_flags;					\
	ISP_SBUS_SWOZZLE(isp, src, dst, le_status, le_reserved2);	\
	ISP_SBUS_SWOZZLE(isp, src, dst, le_cmd_count, le_in_count);	\
	ISP_SBUS_SWOZZLE(isp, src, dst, le_cdb6len, le_cdb7len);	\
	dst->le_timeout = src->le_timeout;				\
	dst->le_reserved = src->le_reserved;				\
}
#define	ISP_SWIZ_NOTIFY(isp, vdst, vsrc)				\
{									\
	in_entry_type *src = (in_entry_t *)vsrc;			\
	in_entry_type *dst = (in_entry_t *)vdst;			\
	dst->in_header = src->in_header;				\
	dst->in_reserved2 = src->in_reserved2;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, in_lun, in_iid);		\
	ISP_SBUS_SWOZZLE(isp, src, dst, in_reserved2, in_tgt);		\
	dst->in_flags = src->in_flags;					\
	ISP_SBUS_SWOZZLE(isp, src, dst, in_status, in_rsvd2);		\
	ISP_SBUS_SWOZZLE(isp, src, dst, in_tag_val, in_tag_type);	\
	dst->in_seqid = src->in_seqid;					\
	MEMCPY(dst->in_msg, src->in_msg, IN_MSGLEN);			\
	MEMCPY(dst->in_reserved, src->in_reserved, IN_RESERVED);	\
	MEMCPY(dst->in_sense, src->in_sense, QLTM_SENSELEN);		\
}
#define	ISP_SWIZ_NOTIFY_FC(isp, vdst, vsrc)				\
{									\
	in_fcentry_type *src = (in_fcentry_t *)vsrc;			\
	in_fcentry_type *dst = (in_fcentry_t *)vdst;			\
	dst->in_header = src->in_header;				\
	dst->in_reserved2 = src->in_reserved2;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, in_lun, in_iid);		\
	dst->in_scclun = src->in_scclun;				\
	dst->in_reserved2 = src->in_reserved2;				\
	dst->in_status = src->in_status;				\
	dst->in_task_flags = src->in_task_flags;			\
	dst->in_seqid = src->in_seqid;					\
}
#define	ISP_SWIZ_NOT_ACK(isp, vdst, vsrc)				\
{									\
	na_entry_t *src = (na_entry_t *)vsrc;				\
	na_entry_t *dst = (na_entry_t *)vdst;				\
	dst->na_header = src->na_header;				\
	dst->na_reserved = src->na_reserved;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, na_lun, na_iid);		\
	dst->na_reserved2 = src->na_reserved2;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, na_reserved, na_tgt);		\
	dst->na_flags = src->na_flags;					\
	ISP_SBUS_SWOZZLE(isp, src, dst, na_status, na_event);		\
	dst->na_seqid = src->na_seqid;					\
	MEMCPY(dst->na_reserved3, src->na_reserved3, NA_RSVDLEN);	\
}
#define	ISP_SWIZ_NOT_ACK_FC(isp, vdst, vsrc)				\
{									\
	na_fcentry_t *src = (na_fcentry_t *)vsrc;			\
	na_fcentry_t *dst = (na_fcentry_t *)vdst;			\
	dst->na_header = src->na_header;				\
	dst->na_reserved = src->na_reserved;				\
	ISP_SBUS_SWOZZLE(isp, src, dst, na_lun, na_iid);		\
	dst->na_scclun = src->na_scclun;				\
	dst->na_flags = src->na_flags;					\
	dst->na_reserved2 = src->na_reserved2;				\
	dst->na_status = src->na_status;				\
	dst->na_task_flags = src->na_task_flags;			\
	dst->na_seqid = src->na_seqid;					\
	MEMCPY(dst->na_reserved3, src->na_reserved3, NA2_RSVDLEN);	\
}
#else
#define	ISP_SWIZ_ATIO(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_ATIO2(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_CTIO(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_CTIO2(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_ENABLE_LUN(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_ATIO2(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_CTIO2(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_NOTIFY(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_NOTIFY_FC(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_NOT_ACK(isp, d, s)	MCIDF(d, s)
#define	ISP_SWIZ_NOT_ACK_FC(isp, d, s)	MCIDF(d, s)
#endif

/*
 * Debug macros
 */

#define	ISP_TDQE(isp, msg, idx, arg)	\
    if (isp->isp_dblev & ISP_LOGTDEBUG2) isp_print_qentry(isp, msg, idx, arg)

/*
 * The functions below are target mode functions that
 * are generally internal to the Qlogic driver.
 */

/*
 * This function handles new response queue entry appropriate for target mode.
 */
int isp_target_notify(struct ispsoftc *, void *, u_int16_t *);

/*
 * Enable/Disable/Modify a logical unit.
 */
#define	DFLT_CMD_CNT	32	/* XX */
#define	DFLT_INOTIFY	(4)
int isp_lun_cmd(struct ispsoftc *, int, int, int, int, u_int32_t);

/*
 * General request queue 'put' routine for target mode entries.
 */
int isp_target_put_entry __P((struct ispsoftc *isp, void *));

/*
 * General routine to put back an ATIO entry-
 * used for replenishing f/w resource counts.
 */
int
isp_target_put_atio(struct ispsoftc *, int, int, int, int, int);

/*
 * General routine to send a final CTIO for a command- used mostly for
 * local responses.
 */
int
isp_endcmd(struct ispsoftc *, void *, u_int32_t, u_int16_t);
#define	ECMD_SVALID	0x100

/*
 * Handle an asynchronous event
 */

void isp_target_async(struct ispsoftc *, int, int);

#endif	/* _ISP_TARGET_H */
