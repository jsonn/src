/* -*-C++-*-	$NetBSD: arm_console.cpp,v 1.2.24.2 2004/09/18 14:34:39 skrll Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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

#include <arm/arm_console.h>

ARMConsole *ARMConsole::_instance = 0;

ARMConsole *
ARMConsole::Instance(MemoryManager *&mem)
{

	if (!_instance)
		_instance = new ARMConsole(mem);

	return _instance;
}

void
ARMConsole::Destroy(void)
{

	if (_instance)
		delete _instance;
}

BOOL
ARMConsole::init(void)
{

	if (!super::init())
		return FALSE;

	_uart_base = _mem->mapPhysicalPage(0x80050000, 0x100, PAGE_READWRITE);
	if (_uart_base == ~0)
		return FALSE;
	_uart_busy = _uart_base + 0x20;
	_uart_transmit = _uart_base + 0x14;

	return TRUE;
}

void
ARMConsole::print(const TCHAR *fmt, ...)
{
	SETUP_WIDECHAR_BUFFER();

	if (!setupMultibyteBuffer())
		return;

	for (int i = 0; _bufm[i] != '\0' && i < CONSOLE_BUFSIZE; i++) {
		char s = _bufm[i];
		if (s == '\n')
			__putc('\r');
		__putc(s);
	}
}
