/*	$NetBSD: tx39timerreg.h,v 1.4.132.1 2008/05/16 02:22:29 yamt Exp $ */

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 * Toshiba TX3912/3922 Timer module
 */
#define TX39_TIMERRTCHI_REG		0x140
#define TX39_TIMERRTCLO_REG		0x144
#define TX39_TIMERALARMHI_REG		0x148
#define TX39_TIMERALARMLO_REG		0x14C
#define TX39_TIMERCONTROL_REG		0x150
#define	TX39_TIMERPERIODIC_REG		0x154

/* Periodic timer (1.15MHz) */
#ifdef TX391X
/*
 * TX3912 base clock is 36.864MHz
 */
#define TX39_TIMERCLK			1152000
#endif
#ifdef TX392X 
/*
 * TX3922 base clock seems to be 32.25MHz (Telios)
 */
#define TX39_TIMERCLK			1007812
#endif

/* Real timer clock (32.768kHz) */
#define TX39_RTCLOCK			32768
#define TX39_MSEC2RTC(m)		((TX39_RTCLOCK * (m)) / 1000)

/*
 *	RTC Register High/Low
 */
/* R */
#define TX39_TIMERRTCHI_SHIFT		0
#ifdef TX391X
#define TX39_TIMERRTCHI_MASK		0xff
#endif /* TX391X */
#ifdef TX392X
#define TX39_TIMERRTCHI_MASK		0x7ff
#endif /* TX392X */

#define TX39_TIMERRTCHI(cr)		((cr) & TX39_TIMERRTCHI_MASK)

/*
 *	Alarm Register High/Low
 */
/* R/W */
#ifdef TX391X /* 40bit */
#define TX39_TIMERALARMHI_SHIFT		0
#define TX39_TIMERALARMHI_MASK		0xff
#endif /* TX391X */
#ifdef TX392X /* 43bit */
#define TX39_TIMERALARMHI_SHIFT		0
#define TX39_TIMERALARMHI_MASK		0x7ff
#endif /* TX392X */

#define TX39_TIMERALARMHI(cr)		((cr) & TX39_TIMERALARMHI_MASK)

/*
 *	Timer Control Register
 */
#define TX39_TIMERCONTROL_FREEZEPRE	0x00000080
#define TX39_TIMERCONTROL_FREEZERTC	0x00000040
#define TX39_TIMERCONTROL_FREEZETIMER	0x00000020
#define TX39_TIMERCONTROL_ENPERTIMER	0x00000010
#define TX39_TIMERCONTROL_RTCCLR	0x00000008
#define TX39_TIMERCONTROL_TESTCMS	0x00000004	/* Don't set */
#define TX39_TIMERCONTROL_ENTESTCLK	0x00000002	/* Don't set */
#define TX39_TIMERCONTROL_ENRTCTST	0x00000001

/*
 *	Periodic Timer Register
 */
/* R */
#define TX39_TIMERPERIODIC_PERCNT_SHIFT 15
#define TX39_TIMERPERIODIC_PERCNT_MASK	0xffff
#define TX39_TIMERPERIODIC_PERCNT(cr)					\
	(((cr) >> TX39_TIMERPERIODIC_PERCNT_SHIFT) &			\
	TX39_TIMERPERIODIC_PERCNT_MASK)
/* R/W */
#define TX39_TIMERPERIODIC_PERVAL_SHIFT 0
#define TX39_TIMERPERIODIC_PERVAL_MASK	0xffff
#define TX39_TIMERPERIODIC_PERVAL(cr)					\
	(((cr) >> TX39_TIMERPERIODIC_PERVAL_SHIFT) &			\
	TX39_TIMERPERIODIC_PERVAL_MASK)
#define TX39_TIMERPERIODIC_PERVAL_SET(cr, val)				\
	((cr) | (((val) << TX39_TIMERPERIODIC_PERVAL_SHIFT) &		\
	(TX39_TIMERPERIODIC_PERVAL_MASK << TX39_TIMERPERIODIC_PERVAL_SHIFT)))
#define TX39_TIMERPERIODIC_PERVAL_CLR(cr) ((cr) &=			\
	 ~(TX39_TIMERPERIODIC_PERVAL_MASK << TX39_TIMERPERIODIC_PERVAL_SHIFT))
#define TX39_TIMERPERIODIC_INTRRATE(val)				\
	((val) + 1)/TX39_TIMERCLK /* unit:Hz */

