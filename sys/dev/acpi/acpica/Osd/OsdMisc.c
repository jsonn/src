/*	$NetBSD: OsdMisc.c,v 1.2.10.1 2002/06/20 16:31:29 gehenna Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * OS Services Layer
 *
 * 6.10: Miscellaneous
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: OsdMisc.c,v 1.2.10.1 2002/06/20 16:31:29 gehenna Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_extern.h>
#include <ddb/db_output.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpi_osd.h>

#include <dev/acpi/acpica/Subsystem/acdebug.h>

int acpi_indebugger;

/*
 * AcpiOsSignal:
 *
 *	Break to the debugger or display a breakpoint message.
 */
ACPI_STATUS
AcpiOsSignal(UINT32 Function, void *Info)
{

	switch (Function) {
	case ACPI_SIGNAL_FATAL:
	    {
		ACPI_SIGNAL_FATAL_INFO *info = Info;

		panic("ACPI fatal signal: "
		    "Type 0x%08x, Code 0x%08x, Argument 0x%08x",
		    info->Type, info->Code, info->Argument);
		/* NOTREACHED */
		break;
	    }

	case ACPI_SIGNAL_BREAKPOINT:
	    {
		const char *info = Info;

		printf("%s\n", info);
#if defined(DDB)
		Debugger();
#else
		printf("ACPI: WARNING: DDB not configured into kernel.\n");
		return (AE_NOT_EXIST);
#endif
		break;
	    }

	default:
		return (AE_BAD_PARAMETER);
	}

	return (AE_OK);
}

ACPI_STATUS
AcpiOsGetLine(char *Buffer)
{
#if defined(DDB)
	char *cp;

	db_readline(Buffer, 80);
	for (cp = Buffer; *cp != 0; cp++)
		if (*cp == '\n' || *cp == '\r')
			*cp = 0;
	db_output_line = 0;
	return (AE_OK);
#else
	printf("ACPI: WARNING: DDB not configured into kernel.\n");
	return (AE_NOT_EXIST);
#endif
}

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
		    ACPI_TABLE_HEADER **NewTable)
{
	/* XXX TBD */
	return (AE_OK);
}

/*
 * acpi_osd_debugger:
 *
 *	Enter the ACPICA debugger.
 */
void
acpi_osd_debugger(void)
{
#ifdef ENABLE_DEBUGGER
	static int beenhere;
	ACPI_PARSE_OBJECT obj;
#ifdef DDB
	label_t	acpi_jmpbuf;
	label_t	*savejmp;
#endif

	if (beenhere == 0) {
		printf("Initializing ACPICA debugger...\n");
		AcpiDbInitialize();
		beenhere = 1;
	}

	printf("Entering ACPICA debugger...\n");
#ifdef DDB
	savejmp = db_recover;
	setjmp(&acpi_jmpbuf);
	db_recover = &acpi_jmpbuf;
#endif
	acpi_indebugger = 1;
	AcpiDbUserCommands('A', &obj);
	acpi_indebugger = 0;
#ifdef DDB
	db_recover = savejmp;
#endif
#else
	printf("ACPI: WARNING: ACPCICA debugger not present.\n");
#endif
}
