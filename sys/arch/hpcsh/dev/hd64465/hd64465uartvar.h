/*	$NetBSD: hd64465uartvar.h,v 1.2.124.1 2008/05/16 02:22:31 yamt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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

#ifndef _LOCORE
int hd64465uart_kgdb_init(void);
#endif

#define HD64465COM_TX_BUSY()						\
	while ((VOLATILE_REF8(HD64465_ULSR_REG8) & LSR_TXRDY) == 0)

#define HD64465COM_PUTC(c)						\
do {									\
	HD64465COM_TX_BUSY();						\
	VOLATILE_REF8(HD64465_UTBR_REG8) = (c);				\
	HD64465COM_TX_BUSY();						\
} while (/*CONSTCOND*/0)

#define HD64465COM_PRINT(s)						\
do {									\
	char *__s =(char *)(s);						\
	int __i;							\
	for (__i = 0; __s[__i] != '\0'; __i++) {			\
		char __c = __s[__i];					\
		if (__c == '\n')					\
			HD64465COM_PUTC('\r');				\
		HD64465COM_PUTC(__c);					\
	}								\
} while (/*CONSTCOND*/0)

