/*	$NetBSD: OsdSynch.c,v 1.2.10.1 2002/06/20 16:31:30 gehenna Exp $	*/

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

/*-     
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

/*
 * OS Services Layer
 *
 * 6.4: Mutual Exclusion and Synchronization
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: OsdSynch.c,v 1.2.10.1 2002/06/20 16:31:30 gehenna Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <dev/acpi/acpica.h>

#define	_COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SYNCH")

/*
 * Simple counting semaphore implemented using a mutex.  This is
 * subsequently used in the OSI code to implement a mutex.  Go figure.
 */
struct acpi_semaphore {
	struct simplelock as_slock;
	UINT32 as_units;
	UINT32 as_maxunits;
};

/*
 * AcpiOsCreateSemaphore:
 *
 *	Create a semaphore.
 */
ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
    ACPI_HANDLE *OutHandle)
{
	struct acpi_semaphore *as;

	ACPI_FUNCTION_TRACE(__FUNCTION__);

	if (OutHandle == NULL)
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	if (InitialUnits > MaxUnits)
		return_ACPI_STATUS(AE_BAD_PARAMETER);

	as = malloc(sizeof(*as), M_DEVBUF, M_NOWAIT);
	if (as == NULL)
		return_ACPI_STATUS(AE_NO_MEMORY);

	simple_lock_init(&as->as_slock);
	as->as_units = InitialUnits;
	as->as_maxunits = MaxUnits;

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "created semaphore %p max %u initial %u\n",
	    as, as->as_maxunits, as->as_units));

	*OutHandle = (ACPI_HANDLE) as;
	return_ACPI_STATUS(AE_OK);
}

/*
 * AcpiOsDeleteSemaphore:
 *
 *	Delete a semaphore.
 */
ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_HANDLE Handle)
{
	struct acpi_semaphore *as = (void *) Handle;

	ACPI_FUNCTION_TRACE(__FUNCTION__);

	if (as == NULL)
		return_ACPI_STATUS(AE_BAD_PARAMETER);

	free(as, M_DEVBUF);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "destroyed semaphre %p\n", as));

	return_ACPI_STATUS(AE_OK);
}

/*
 * AcpiOsWaitSemaphore:
 *
 *	Wait for units from a semaphore.
 */
ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_HANDLE Handle, UINT32 Units, UINT32 Timeout)
{
	struct acpi_semaphore *as = (void *) Handle;
	ACPI_STATUS rv;
	int timo, error;

	/*
	 * This implementation has a bug: It has to stall for the entire
	 * timeout before it will return AE_TIME.  A better implementation
	 * would adjust the amount of time left after being awakened.
	 */

	ACPI_FUNCTION_TRACE(__FUNCTION__);

	if (as == NULL)
		return_ACPI_STATUS(AE_BAD_PARAMETER);

	/* A timeout of -1 means "forever". */
	if (Timeout == -1)
		timo = 0;
	else {
		/* Compute the timeout using uSec per tick. */
		timo = (Timeout * 1000) / (1000000 / hz);
		if (timo <= 0)
			timo = 1;
	}

	simple_lock(&as->as_slock);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "get %d units from semaphore %p (has %d) timeout %d\n",
	    Units, as, as->as_units, Timeout));

	for (;;) {
		if (as->as_units >= Units) {
			as->as_units -= Units;
			rv = AE_OK;
			break;
		}

		ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
		    "semaphore blocked, sleeping %d ticks\n", timo));

		error = ltsleep(as, PVM, "acpisem", timo, &as->as_slock);
		if (error == EWOULDBLOCK) {
			rv = AE_TIME;
			break;
		}
	}

	simple_unlock(&as->as_slock);

	return_ACPI_STATUS(rv);
}

/*
 * AcpiOsSignalSemaphore:
 *
 *	Send units to a semaphore.
 */
ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_HANDLE Handle, UINT32 Units)
{
	struct acpi_semaphore *as = (void *) Handle;

	ACPI_FUNCTION_TRACE(__FUNCTION__);

	if (as == NULL)
		return_ACPI_STATUS(AE_BAD_PARAMETER);

	simple_lock(&as->as_slock);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "return %d units to semaphore %p (has %d)\n",
	    Units, as, as->as_units));

	as->as_units += Units;
	if (as->as_units > as->as_maxunits)
		as->as_units = as->as_maxunits;
	wakeup(as);

	simple_unlock(&as->as_slock);

	return_ACPI_STATUS(AE_OK);
}
