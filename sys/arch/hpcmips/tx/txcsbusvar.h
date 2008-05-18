/*	$NetBSD: txcsbusvar.h,v 1.4.76.1 2008/05/18 12:32:04 yamt Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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
 *	Chip Select bus attach arguments.
 */
struct csbus_attach_args {
	const char *cba_busname;
	tx_chipset_tag_t cba_tc;
};

/*
 *	Information for Chip Select CS[0:3], MCS[0:3], CARD[1:2]
 */
struct cs_handle {
	int		cs;		/* Chip Select. see tx39biuvar.h */
	u_int32_t	csbase;		/* base offset from CS start addr */
	u_int32_t	cssize;		/* map size */
	int		cswidth;	/* CS bus-width */
	bus_space_tag_t cstag;		/* bus_space tag for this CS */
};

/*
 *	Chip Select attach arguments.
 */
struct cs_attach_args {
	tx_chipset_tag_t ca_tc;
	struct cs_handle ca_csreg;	/* Register space */
	struct cs_handle ca_csio;	/* I/O space */
	struct cs_handle ca_csmem;	/* Memory space */
	int ca_irq1;			/* Interrupt request */
	int ca_irq2;			/* 2nd interrupt request */
	int ca_irq3;			/* 3rd interrupt request */
};
