/*	$NetBSD: fdvar.h,v 1.11.2.3 2004/09/21 13:22:02 skrll Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#define	FD_BSIZE(fd)	(128 * (1 << fd->sc_type->secsize))
#define	FDC_MAXIOSIZE	NBPG	/* XXX should be MAXBSIZE */

#define FDC_NSTATUS	10

#ifndef _LOCORE
struct fdcio {
	bus_space_handle_t	fdcio_handle;
	/*
	 * 82072 (sun4c) and 82077 (sun4m) controllers have different
	 * register layout; so we cache offsets to the registers here.
	 */
	u_int	fdcio_reg_msr;
	u_int	fdcio_reg_fifo;
	u_int	fdcio_reg_dor;		/* 82077 only */
	u_int	fdcio_reg_dir;		/* 82077 only */

	/*
	 * Interrupt state.
	 */
	int	fdcio_itask;
	int	fdcio_istatus;

	/*
	 * IO state.
	 */
	char	*fdcio_data;		/* pseudo-DMA data */
	int	fdcio_tc;		/* pseudo-DMA Terminal Count */
	u_char	fdcio_status[FDC_NSTATUS];	/* copy of registers */
	int	fdcio_nstat;		/* # of valid status bytes */

	/*
	 * Statictics.
	 */
	struct	evcnt	fdcio_intrcnt;
};
#endif /* _LOCORE */

/* itask values */
#define FDC_ITASK_NONE		0	/* No HW interrupt expected */
#define FDC_ITASK_SENSEI	1	/* Do SENSEI on next HW interrupt */
#define FDC_ITASK_DMA		2	/* Do Pseudo-DMA */
#define FDC_ITASK_RESULT	3	/* Pick up command results */

/* istatus values */
#define FDC_ISTATUS_NONE	0	/* No status available */
#define FDC_ISTATUS_SPURIOUS	1	/* Spurious HW interrupt detected */
#define FDC_ISTATUS_ERROR	2	/* Operation completed abnormally */
#define FDC_ISTATUS_DONE	3	/* Operation completed normally */


#define FD_MAX_NSEC 36		/* highest known number of spt - allow for */
				/* 2.88 MB drives */

#ifndef _LOCORE
struct ne7_fd_formb {
	int cyl, head;
	int transfer_rate;	/* fdreg.h: FDC_???KBPS */

	union {
		struct fd_form_data {
			/*
			 * DO NOT CHANGE THE LAYOUT OF THIS STRUCTS
			 * it is hardware-dependant since it exactly
			 * matches the byte sequence to write to FDC
			 * during its `format track' operation
			 */
			u_char secshift; /* 0 -> 128, ...; usually 2 -> 512 */
			u_char nsecs;	/* must be <= FD_MAX_NSEC */
			u_char gaplen;	/* GAP 3 length; usually 84 */
			u_char fillbyte; /* usually 0xf6 */
			struct fd_idfield_data {
				/*
				 * data to write into id fields;
				 * for obscure formats, they mustn't match
				 * the real values (but mostly do)
				 */
				u_char cylno;	/* 0 thru 79 (or 39) */
				u_char headno;	/* 0, or 1 */
				u_char secno;	/* starting at 1! */
				u_char secsize;	/* usually 2 */
			} idfields[FD_MAX_NSEC]; /* 0 <= idx < nsecs used */
		} structured;
		u_char raw[1];	/* to have continuous indexed access */
	} format_info;
};

/* make life easier */
#define fd_formb_secshift   format_info.structured.secshift
#define fd_formb_nsecs      format_info.structured.nsecs
#define fd_formb_gaplen     format_info.structured.gaplen
#define fd_formb_fillbyte   format_info.structured.fillbyte
/* these data must be filled in for(i = 0; i < fd_formb_nsecs; i++) */
#define fd_formb_cylno(i)   format_info.structured.idfields[i].cylno
#define fd_formb_headno(i)  format_info.structured.idfields[i].headno
#define fd_formb_secno(i)   format_info.structured.idfields[i].secno
#define fd_formb_secsize(i) format_info.structured.idfields[i].secsize
#endif /* _LOCORE */
