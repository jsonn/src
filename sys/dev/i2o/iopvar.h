/*	$NetBSD: iopvar.h,v 1.4.2.2 2001/08/24 00:09:10 nathanw Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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

#ifndef _I2O_IOPVAR_H_
#define	_I2O_IOPVAR_H_

#include "locators.h"

/*
 * Transfer descriptor.
 */
struct iop_xfer {
	bus_dmamap_t	ix_map;
	u_int		ix_size;
	u_int		ix_flags;
};
#define	IX_IN			0x0001	/* Data transfer from IOP */
#define	IX_OUT			0x0002	/* Data transfer to IOP */

/*
 * Message wrapper.
 */
struct iop_msg {
	SLIST_ENTRY(iop_msg)	im_chain;	/* Next free message */
	u_int			im_flags;	/* Control flags */
	u_int			im_tctx;	/* Transaction context */
	void			*im_dvcontext;	/* Un*x device context */
	struct i2o_reply	*im_rb;		/* Reply buffer */
	u_int			im_reqstatus;	/* Status from reply */
	struct iop_xfer		im_xfer[IOP_MAX_MSG_XFERS];
};
#define	IM_SYSMASK		0x00ff
#define	IM_REPLIED		0x0001	/* Message has been replied to */
#define	IM_ALLOCED		0x0002	/* This message wrapper is allocated */
#define	IM_SGLOFFADJ		0x0004	/* S/G list offset adjusted */
#define	IM_FAIL			0x0008	/* Transaction error returned */	

#define	IM_USERMASK		0xff00
#define	IM_WAIT			0x0100	/* Wait (sleep) for completion */
#define	IM_POLL			0x0200	/* Wait (poll) for completion */
#define	IM_NOSTATUS		0x0400	/* Don't check status if waiting */
#define	IM_POLL_INTR		0x0800	/* Do send interrupt when polling */

struct iop_initiator {
	LIST_ENTRY(iop_initiator) ii_list;
	LIST_ENTRY(iop_initiator) ii_hash;

	void	(*ii_intr)(struct device *, struct iop_msg *, void *);
	int	(*ii_reconfig)(struct device *);
	void	(*ii_adjqparam)(struct device *, int);

	struct	device *ii_dv;
	int	ii_flags;
	int	ii_ictx;		/* Initiator context */
	int	ii_tid;
};
#define	II_NOTCTX	0x0001	/* No transaction context */
#define	II_CONFIGURED	0x0002	/* Already configured */
#define	II_UTILITY	0x0004	/* Utility initiator (not a real device) */

#define	IOP_ICTX	0
#define	IOP_INIT_CODE	0x80

/*
 * Parameter group op (for async parameter retrievals).
 */
struct iop_pgop {
	struct	i2o_param_op_list_header olh;
	struct	i2o_param_op_all_template oat;
} __attribute__ ((__packed__));

/*
 * Per-IOP context.
 */
struct iop_softc {
	struct device	sc_dv;		/* Generic device data */
	bus_space_handle_t sc_ioh;	/* Bus space handle */
	bus_space_tag_t	sc_iot;		/* Bus space tag */
	bus_dma_tag_t	sc_dmat;	/* Bus DMA tag */
	void	 	*sc_ih;		/* Interrupt handler cookie */

	struct iop_msg	*sc_ims;	/* Message wrappers */
	SLIST_HEAD(, iop_msg) sc_im_freelist; /* Free wrapper list */

	bus_dmamap_t	sc_rep_dmamap;	/* Reply frames DMA map */
	int		sc_rep_size;	/* Reply frames size */
	bus_addr_t	sc_rep_phys;	/* Reply frames PA */
	caddr_t		sc_rep;		/* Reply frames VA */

	int		sc_maxib;	/* Max inbound (-> IOP) queue depth */
	int		sc_maxob;	/* Max outbound (<- IOP) queue depth */
	int		sc_curib;	/* Current inbound queue depth */

	struct i2o_hrt	*sc_hrt;	/* Hardware resource table */
	struct iop_tidmap *sc_tidmap;	/* TID map (per-LCT-entry flags) */
	struct i2o_lct	*sc_lct;	/* Logical configuration table */
	int		sc_nlctent;	/* Number of LCT entries */
	int		sc_flags;	/* IOP-wide flags */
	u_int32_t	sc_chgind;	/* Configuration change indicator */
	struct lock	sc_conflock;	/* Configuration lock */
	struct proc	*sc_reconf_proc;/* Auto reconfiguration process */
	LIST_HEAD(, iop_initiator) sc_iilist;/* Initiator list */
	int		sc_nii;		/* Total number of initiators */
	int		sc_nuii;	/* Number of utility initiators */

	struct iop_initiator sc_eventii;/* IOP event handler */
	bus_dmamap_t	sc_scr_dmamap;  /* Scratch DMA map */
	bus_dma_segment_t sc_scr_seg[1];/* Scratch DMA segment */
	caddr_t		sc_scr;		/* Scratch memory VA */

	bus_space_tag_t	sc_bus_memt;	/* Parent bus memory tag */
	bus_space_tag_t	sc_bus_iot;	/* Parent but I/O tag */
	bus_addr_t	sc_memaddr;	/* Register window address */
	bus_size_t	sc_memsize;	/* Register window size */

	struct i2o_status sc_status;	/* Last retrieved status record */
};
#define	IOP_OPEN		0x01	/* Device interface open */
#define	IOP_HAVESTATUS		0x02	/* Successfully retrieved status */
#define	IOP_ONLINE		0x04	/* Can use ioctl interface */

struct iop_attach_args {
	int	ia_class;		/* device class */
	int	ia_tid;			/* target ID */
};
#define	iopcf_tid	cf_loc[IOPCF_TID]		/* TID */

void	iop_init(struct iop_softc *, const char *);
int	iop_intr(void *);
int	iop_lct_get(struct iop_softc *);
int	iop_param_op(struct iop_softc *, int, struct iop_initiator *, int,
		     int, void *, int);
int	iop_print_ident(struct iop_softc *, int);
int	iop_simple_cmd(struct iop_softc *, int, int, int, int, int);
void	iop_strvis(struct iop_softc *, const char *, int, char *, int);
int	iop_post(struct iop_softc *, u_int32_t *);

void	iop_initiator_register(struct iop_softc *, struct iop_initiator *);
void	iop_initiator_unregister(struct iop_softc *, struct iop_initiator *);

struct	iop_msg *iop_msg_alloc(struct iop_softc *, int);
void	iop_msg_free(struct iop_softc *, struct iop_msg *);
int	iop_msg_map(struct iop_softc *, struct iop_msg *, u_int32_t *, void *,
		    int, int, struct proc *);
int	iop_msg_map_bio(struct iop_softc *, struct iop_msg *, u_int32_t *,
			void *, int, int);
int	iop_msg_post(struct iop_softc *, struct iop_msg *, void *, int);
void	iop_msg_unmap(struct iop_softc *, struct iop_msg *);

int	iop_util_abort(struct iop_softc *, struct iop_initiator *, int, int,
		      int);
int	iop_util_claim(struct iop_softc *, struct iop_initiator *, int, int);
int	iop_util_eventreg(struct iop_softc *, struct iop_initiator *, int);

#endif	/* !_I2O_IOPVAR_H_ */
