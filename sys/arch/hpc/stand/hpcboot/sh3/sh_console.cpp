/*	$NetBSD: sh_console.cpp,v 1.1.2.3 2001/03/27 15:30:50 bouyer Exp $	*/

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

#include <hpcmenu.h>
#include <sh3/sh_console.h>
#include <sh3/hd64461.h>

#define BI_CNUSE_SCI		2
#define BI_CNUSE_SCIF		3
#define BI_CNUSE_HD64461COM	4

SHConsole *SHConsole::_instance = 0;

struct SHConsole::console_info
SHConsole::_console_info[] = {
	{ PLATID_CPU_SH_3        , PLATID_MACH_HP                          , SCIFPrint       , BI_CNUSE_SCIF },
	{ PLATID_CPU_SH_3_7709   , PLATID_MACH_HITACHI                     , HD64461COMPrint , BI_CNUSE_HD64461COM },
	{ PLATID_CPU_SH_3_7709   , PLATID_MACH_CASIO_CASSIOPEIAA_A55V      , 0               , BI_CNUSE_BUILTIN },
	{ 0, 0, 0 } // terminator.
};

SHConsole::SHConsole()
{
	_print = 0;
}

SHConsole::~SHConsole()
{
}

SHConsole *
SHConsole::Instance()
{
	if (!_instance)
		_instance = new SHConsole();

	return _instance;
}

BOOL
SHConsole::init()
{
	HpcMenuInterface &menu = HpcMenuInterface::Instance();
	struct console_info *tab = _console_info;
	platid_mask_t target, entry;
	
	_kmode = SetKMode(1);
	
	target.dw.dw0 = menu._pref.platid_hi;
	target.dw.dw1 = menu._pref.platid_lo;

	// search apriori setting if any.
	for (; tab->cpu; tab++) {
		entry.dw.dw0 = tab->cpu;
		entry.dw.dw1 = tab->machine;
		if (platid_match(&target, &entry)) {
			_print = tab->print;
			_boot_console = tab->boot_console;
			break;
		}
	}

	// always open COM1 to supply clock and power for the
	// sake of kernel serial driver 
	return openCOM1();
}

void
SHConsole::print(const TCHAR *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	wvsprintf(_bufw, fmt, ap);
	va_end(ap);

	if (!setupBuffer())
		return;

	if (_print == 0)
		SerialConsole::genericPrint(_bufm);
	else
		_print(_bufm);
}

void
SHConsole::SCIPrint(const char *buf)
{
	SCI_PRINT(buf);
}

void
SHConsole::SCIFPrint(const char *buf)
{
	SCIF_PRINT(buf);
}

void
SHConsole::HD64461COMPrint(const char *buf)
{
	HD64461COM_PRINT(buf);
}


