/*	$NetBSD: cd_link.h,v 1.2.2.3 1997/10/14 10:24:50 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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

#define	CDOUTSTANDING	4
#define	CDRETRIES	1

struct cd_ops;

struct cd_softc {
	struct device sc_dev;
	struct disk sc_dk;

	int flags;
#define	CDF_LOCKED	0x01
#define	CDF_WANTED	0x02
#define	CDF_WLABEL	0x04		/* label is writable */
#define	CDF_LABELLING	0x08		/* writing label */
#define	CDF_ANCIENT	0x10		/* disk is ancient; for minphys */
	struct scsipi_link *sc_link;	/* contains our targ, lun, etc. */
	struct cd_parms {
		int blksize;
		u_long disksize;	/* total number sectors */
	} params;
	struct buf buf_queue;
	const struct cd_ops *sc_ops;	/* our bus-dependent ops vector */

#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
};

struct cd_ops {
	int	(*cdo_setchan) __P((struct cd_softc *, int, int, int, int));
	int	(*cdo_getvol) __P((struct cd_softc *, struct ioc_vol *));
	int	(*cdo_setvol) __P((struct cd_softc *, const struct ioc_vol *));
	int	(*cdo_set_pa_immed) __P((struct cd_softc *));
};

void cdattach __P((struct device *, struct cd_softc *, struct scsipi_link *,
    const struct cd_ops *));
