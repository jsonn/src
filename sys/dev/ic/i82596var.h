/* $NetBSD: i82596var.h,v 1.1.4.4 2004/09/21 13:27:57 skrll Exp $ */

/*
 * Copyright (c) 2003 Jochen Kunz.
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
 * 3. The name of Jochen Kunz may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOCHEN KUNZ
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JOCHEN KUNZ
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* All definitions are for a Intel 82596 DX/SX / CA in linear 32 bit mode. */



/* Supported chip variants */
extern char *i82596_typenames[];
enum i82596_types { I82596_UNKNOWN, I82596_DX, I82596_CA };



/* System Configuration Pointer */
struct iee_scp {
	volatile u_int16_t scp_pad1;
	volatile u_int16_t scp_sysbus;		/* Sysbus Byte */
	volatile u_int32_t scp_pad2;
	volatile u_int32_t scp_iscp_addr;	/* Int. Sys. Conf. Pointer */
};



/* Intermediate System Configuration Pointer */
struct iee_iscp {
	volatile u_int16_t iscp_bussy;		/* Even Word, bits 0..15 */
	volatile u_int16_t iscp_pad;		/* Odd Word, bits 16..32 */
	volatile u_int32_t iscp_scb_addr;	/* address of SCB */
};



/* System Control Block */
struct iee_scb {
	volatile u_int16_t scb_status;		/* Status Bits */
	volatile u_int16_t scb_cmd;		/* Command Bits */
	volatile u_int32_t scb_cmd_blk_addr;	/* Command Block Address */
	volatile u_int32_t scb_rfa_addr;	/* Receive Frame Area Address */
	volatile u_int32_t scb_crc_err;		/* CRC Errors */
	volatile u_int32_t scb_align_err;	/* Alignment Errors */
	volatile u_int32_t scb_resource_err;	/* Resource Errors [1] */
	volatile u_int32_t scb_overrun_err;	/* Overrun Errors [1] */
	volatile u_int32_t scb_rcvcdt_err;	/* RCVCDT Errors [1] */
	volatile u_int32_t scb_short_fr_err;	/* Short Frame Errors */
	volatile u_int16_t scb_tt_off;		/* Bus Throtle Off Timer */
	volatile u_int16_t scb_tt_on;		/* Bus Throtle On Timer */
};
/* [1] In MONITOR mode these counters change function. */



/* Command Block */
struct iee_cb {
	volatile u_int16_t cb_status;		/* Status Bits */
	volatile u_int16_t cb_cmd;		/* Command Bits */
	volatile u_int32_t cb_link_addr;	/* Link Address to next CMD */
	union {
		volatile u_int8_t cb_ind_addr[8];/* Individual Address */
		volatile u_int8_t cb_cf[16];	/* Configuration Bytes */
		struct {
			volatile u_int16_t mc_size;/* Num bytes of Mcast Addr.*/
			volatile u_int8_t mc_addrs[6]; /* List of Mcast Addr. */
		} cb_mcast;
		struct {
			volatile u_int32_t tx_tbd_addr;/* TX Buf. Descr. Addr.*/
			volatile u_int16_t tx_tcb_count; /* Len. of opt. data */
			volatile u_int16_t tx_pad;
			volatile u_int8_t tx_dest_addr[6]; /* Dest. Addr. */
			volatile u_int16_t tx_length; /* Length of data */
			/* u_int8_t data;	 Data to send, optional */
		} cb_transmit;
		volatile u_int32_t cb_tdr;	/* Time & Flags from TDR CMD */
		volatile u_int32_t cb_dump_addr;/* Address of Dump buffer */
	};
};



/* Transmit Buffer Descriptor */
struct iee_tbd {
	volatile u_int16_t tbd_size;		/* Size of buffer & Flags */
	volatile u_int16_t tbd_pad;
	volatile u_int32_t tbd_link_addr;	/* Link Address to next RFD */
	volatile u_int32_t tbd_tb_addr;		/* Transmit Buffer Address */
};



/* Receive Frame Descriptor */
struct iee_rfd {
	volatile u_int16_t rfd_status;		/* Status Bits */
	volatile u_int16_t rfd_cmd;		/* Command Bits */
	volatile u_int32_t rfd_link_addr;	/* Link Address to next RFD */
	volatile u_int32_t rfd_rbd_addr;	/* Address of first free RBD */
	volatile u_int16_t rfd_count;		/* Actual Count */
	volatile u_int16_t rfd_size;		/* Size */
	volatile u_int8_t rfd_dest_addr[6];	/* Destiantion Address */
	volatile u_int8_t rfd_src_addr[6];	/* Source Address */
	volatile u_int16_t rfd_length;		/* Length Field */
	volatile u_int16_t rfd_pad;		/* Optional Data */
};



/* Receive Buffer Descriptor */
struct iee_rbd {
	volatile u_int16_t rbd_count;		/* Actual Cont of bytes */
	volatile u_int16_t rbd_pad1;
	volatile u_int32_t rbd_next_rbd;	/* Address of Next RBD */
	volatile u_int32_t rbd_rb_addr;		/* Receive Buffer Address */
	volatile u_int16_t rbd_size;		/* Size of Receive Buffer */
	volatile u_int16_t rbd_pad2;
};



#define IEE_NRFD	32	/* Number of RFDs == length of receive queue */
#define IEE_NCB		32	/* Number of Command Blocks == transmit queue */
#define IEE_NTBD	16	/* Number of TBDs per CB */



struct iee_softc {
	struct device sc_dev;		/* common device data */
	struct ifmedia sc_ifmedia;	/* media interface */
	struct ethercom sc_ethercom;	/* ethernet speciffic stuff */
	enum i82596_types sc_type;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_shmem_map;
	bus_dma_segment_t sc_dma_segs;
	bus_dmamap_t sc_rx_map[IEE_NRFD];
	bus_dmamap_t sc_tx_map[IEE_NCB];
	struct mbuf *sc_rx_mbuf[IEE_NRFD];
	struct mbuf *sc_tx_mbuf[IEE_NCB];
	caddr_t sc_shmem_addr;
	int sc_next_cb;
	int sc_next_tbd;
	int sc_rx_done;
	u_int8_t sc_cf[14];
	int sc_flags;
	int sc_cl_align;
	u_int32_t sc_crc_err;
	u_int32_t sc_align_err;
	u_int32_t sc_resource_err;
	u_int32_t sc_overrun_err;
	u_int32_t sc_rcvcdt_err;
	u_int32_t sc_short_fr_err;
	u_int32_t sc_receive_err;
	u_int32_t sc_tx_col;
	u_int32_t sc_rx_err;
	u_int32_t sc_cmd_err;
	u_int32_t sc_tx_timeout;
	u_int32_t sc_setup_timeout;
	int (*sc_iee_cmd)(struct iee_softc *, u_int32_t);
	int (*sc_iee_reset)(struct iee_softc *);
	void (*sc_mediastatus)(struct ifnet *, struct ifmediareq *);
	int (*sc_mediachange)(struct ifnet *);
};



/* Flags */
#define IEE_NEED_SWAP	0x01
#define	IEE_WANT_MCAST	0x02

#define IEE_SWAP(x)	((sc->sc_flags & IEE_NEED_SWAP) == 0 ? x : 	\
			(((x) << 16) | ((x) >> 16)))
#define IEE_PHYS_SHMEM(x) ((u_int32_t) (sc->sc_shmem_map->dm_segs[0].ds_addr \
			+ (x)))


/* Offsets in shared memory */
#define IEE_SCP_SZ	(((sizeof(struct iee_scp) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_SCP_OFF	0

#define IEE_ISCP_SZ	(((sizeof(struct iee_iscp) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_ISCP_OFF	IEE_SCP_SZ

#define IEE_SCB_SZ	(((sizeof(struct iee_scb) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_SCB_OFF	(IEE_SCP_SZ + IEE_ISCP_SZ)

#define IEE_RFD_SZ	(((sizeof(struct iee_rfd) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_RFD_LIST_SZ	(IEE_RFD_SZ * IEE_NRFD)
#define IEE_RFD_OFF	(IEE_SCP_SZ + IEE_ISCP_SZ + IEE_SCB_SZ)

#define IEE_RBD_SZ	(((sizeof(struct iee_rbd) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_RBD_LIST_SZ	(IEE_RBD_SZ * IEE_NRFD)
#define IEE_RBD_OFF	(IEE_SCP_SZ + IEE_ISCP_SZ + IEE_SCB_SZ		\
			+ IEE_RFD_SZ * IEE_NRFD)

#define IEE_CB_SZ	(((sizeof(struct iee_cb) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_CB_LIST_SZ	(IEE_CB_SZ * IEE_NCB)
#define IEE_CB_OFF	(IEE_SCP_SZ + IEE_ISCP_SZ + IEE_SCB_SZ		\
			+ IEE_RFD_SZ * IEE_NRFD + IEE_RBD_SZ * IEE_NRFD)

#define IEE_TBD_SZ	(((sizeof(struct iee_tbd) - 1) / (sc)->sc_cl_align + 1)\
			* (sc)->sc_cl_align)
#define IEE_TBD_LIST_SZ	(IEE_TBD_SZ * IEE_NTBD * IEE_NCB)
#define IEE_TBD_OFF	(IEE_SCP_SZ + IEE_ISCP_SZ + IEE_SCB_SZ		\
			+ IEE_RFD_SZ * IEE_NRFD + IEE_RBD_SZ * IEE_NRFD	\
			+ IEE_CB_SZ * IEE_NCB)

#define IEE_SHMEM_MAX	(IEE_SCP_SZ + IEE_ISCP_SZ + IEE_SCB_SZ		\
			+ IEE_RFD_SZ * IEE_NRFD + IEE_RBD_SZ * IEE_NRFD	\
			+ IEE_CB_SZ * IEE_NCB + IEE_TBD_SZ * IEE_NTBD * IEE_NCB)


#define SC_SCP		((struct iee_scp*)((sc)->sc_shmem_addr + IEE_SCP_OFF))
#define SC_ISCP		((struct iee_iscp*)((sc)->sc_shmem_addr + IEE_ISCP_OFF))
#define SC_SCB		((struct iee_scb*)((sc)->sc_shmem_addr + IEE_SCB_OFF))
#define SC_RFD(n)	((struct iee_rfd*)((sc)->sc_shmem_addr + IEE_RFD_OFF \
				+ (n) * IEE_RFD_SZ))
#define SC_RBD(n)	((struct iee_rbd*)((sc)->sc_shmem_addr + IEE_RBD_OFF \
				+ (n) * IEE_RBD_SZ))
#define SC_CB(n)	((struct iee_cb*)((sc)->sc_shmem_addr + IEE_CB_OFF \
				+ (n) * IEE_CB_SZ))
#define SC_TBD(n)	((struct iee_tbd*)((sc)->sc_shmem_addr + IEE_TBD_OFF \
				+ (n) * IEE_TBD_SZ))



void iee_attach(struct iee_softc *, u_int8_t *, int *, int, int);
void iee_detach(struct iee_softc *, int);
int iee_intr(void *);




