/*	$NetBSD: dp8390var.h,v 1.5.2.1 1997/11/05 18:41:44 thorpej Exp $	*/

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

#define INTERFACE_NAME_LEN	32

/*
 * dp8390_softc: per line info and status
 */
struct dp8390_softc {
	struct device	sc_dev;
	void	*sc_ih;
	int	sc_flags;		/* interface flags, from config */

	struct ethercom sc_ec;		/* ethernet common */
	struct ifmedia sc_media;	/* supported media information */

	bus_space_tag_t	sc_regt;	/* NIC register space tag */
	bus_space_handle_t sc_regh;	/* NIC register space handle */
	bus_space_tag_t	sc_buft;	/* Buffer space tag */
	bus_space_handle_t sc_bufh;	/* Buffer space handle */

	bus_size_t sc_reg_map[16];	/* register map (offsets) */

	int	is790;		/* NIC is a 790 */

	u_int8_t cr_proto;	/* values always set in CR */
	u_int8_t dcr_reg;	/* override DCR iff LS is set */

	int	mem_start;	/* offset of NIC memory */
	int	mem_end;	/* offset of NIC memory end */
	int	mem_size;	/* total shared memory size */
	int	mem_ring;	/* offset of start of RX ring-buffer */

	u_short	txb_cnt;	/* Number of transmit buffers */
	u_short	txb_inuse;	/* number of transmit buffers active */

	u_short	txb_new;	/* pointer to where new buffer will be added */
	u_short	txb_next_tx;	/* pointer to next buffer ready to xmit */
	u_short	txb_len[8];	/* buffered xmit buffer lengths */
	u_short	tx_page_start;	/* first page of TX buffer area */
	u_short	rec_page_start; /* first page of RX ring-buffer */
	u_short	rec_page_stop;	/* last page of RX ring-buffer */
	u_short	next_packet;	/* pointer to next unread RX packet */

	u_int8_t sc_enaddr[6];	/* storage for MAC address */

	int	sc_enabled;	/* boolean; power enabled on interface */

	int	(*test_mem) __P((struct dp8390_softc *));
	void	(*init_card) __P((struct dp8390_softc *));
	void	(*read_hdr) __P((struct dp8390_softc *,
		    int, struct dp8390_ring *));
	void	(*recv_int) __P((struct dp8390_softc *));
	int	(*ring_copy) __P((struct dp8390_softc *,
		    int, caddr_t, u_short));
	int	(*write_mbuf) __P((struct dp8390_softc *, struct mbuf *, int));

	int	(*sc_enable) __P((struct dp8390_softc *));
	void	(*sc_disable) __P((struct dp8390_softc *));

	int	(*sc_mediachange) __P((struct dp8390_softc *));
	void	(*sc_mediastatus) __P((struct dp8390_softc *,
		    struct ifmediareq *));
};

/*
 * Vendor types
 */
#define DP8390_VENDOR_UNKNOWN	0xff	/* Unknown network card */
#define DP8390_VENDOR_WD_SMC	0x00	/* Western Digital/SMC */
#define DP8390_VENDOR_3COM	0x01	/* 3Com */
#define DP8390_VENDOR_NOVELL	0x02	/* Novell */
#define DP8390_VENDOR_APPLE	0x10	/* Apple Ethernet card */
#define DP8390_VENDOR_INTERLAN	0x11	/* Interlan A310 card (GatorCard) */
#define DP8390_VENDOR_DAYNA	0x12	/* DaynaPORT E/30s (and others?) */
#define DP8390_VENDOR_ASANTE	0x13	/* Asante MacCon II/E */
#define DP8390_VENDOR_FARALLON	0x14	/* Farallon EtherMac II-TP */
#define DP8390_VENDOR_FOCUS	0x15	/* FOCUS Enhancements EtherLAN */
#define DP8390_VENDOR_KINETICS	0x16	/* Kinetics EtherPort SE/30 */

/*
 * Compile-time config flags
 */
/*
 * This sets the default for enabling/disablng the tranceiver.
 */
#define DP8390_DISABLE_TRANCEIVER	0x0001

/*
 * This forces the board to be used in 8/16-bit mode even if it autoconfigs
 * differently.
 */
#define DP8390_FORCE_8BIT_MODE		0x0002
#define DP8390_FORCE_16BIT_MODE		0x0004

/*
 * This disables the use of multiple transmit buffers.
 */
#define DP8390_NO_MULTI_BUFFERING	0x0008

/*
 * This forces all operations with the NIC memory to use Programmed I/O (i.e.
 * not via shared memory).
 */
#define DP8390_FORCE_PIO		0x0010

/*
 * NIC register access macros
 */
#define NIC_GET(t, h, reg)	bus_space_read_1(t, h,			\
				    ((sc)->sc_reg_map[reg]))
#define NIC_PUT(t, h, reg, val)	bus_space_write_1(t, h,			\
				    ((sc)->sc_reg_map[reg]), (val))

int	dp8390_config __P((struct dp8390_softc *, int *, int, int));
int	dp8390_intr __P((void *));
int	dp8390_ioctl __P((struct ifnet *, u_long, caddr_t));
void	dp8390_start __P((struct ifnet *));
void	dp8390_watchdog __P((struct ifnet *));
void	dp8390_reset __P((struct dp8390_softc *));
void	dp8390_init __P((struct dp8390_softc *));
void	dp8390_stop __P((struct dp8390_softc *));

void	dp8390_rint __P((struct dp8390_softc *));

void	dp8390_getmcaf __P((struct ethercom *, u_int8_t *));
struct mbuf *dp8390_get __P((struct dp8390_softc *, int, u_short));
void	dp8390_read __P((struct dp8390_softc *, int, u_short));
