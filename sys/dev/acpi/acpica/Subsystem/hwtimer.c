
/******************************************************************************
 *
 * Name: hwtimer.c - ACPI Power Management Timer Interface
 *              xRevision: 12 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hwtimer.c,v 1.2.2.2 2002/01/10 19:53:24 thorpej Exp $");

#include "acpi.h"
#include "achware.h"

#define _COMPONENT          ACPI_HARDWARE
        MODULE_NAME         ("hwtimer")


/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimerResolution
 *
 * PARAMETERS:  none
 *
 * RETURN:      Number of bits of resolution in the PM Timer (24 or 32).
 *
 * DESCRIPTION: Obtains resolution of the ACPI PM Timer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimerResolution (
    UINT32                  *Resolution)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AcpiGetTimerResolution");


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (!Resolution)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (0 == AcpiGbl_FADT->TmrValExt)
    {
        *Resolution = 24;
    }

    else
    {
        *Resolution = 32;
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimer
 *
 * PARAMETERS:  none
 *
 * RETURN:      Current value of the ACPI PM Timer (in ticks).
 *
 * DESCRIPTION: Obtains current value of ACPI PM Timer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimer (
    UINT32                  *Ticks)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AcpiGetTimer");


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (!Ticks)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiOsReadPort ((ACPI_IO_ADDRESS)
        ACPI_GET_ADDRESS (AcpiGbl_FADT->XPmTmrBlk.Address), Ticks, 32);

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimerDuration
 *
 * PARAMETERS:  StartTicks
 *              EndTicks
 *              TimeElapsed
 *
 * RETURN:      TimeElapsed
 *
 * DESCRIPTION: Computes the time elapsed (in microseconds) between two
 *              PM Timer time stamps, taking into account the possibility of
 *              rollovers, the timer resolution, and timer frequency.
 *
 *              The PM Timer's clock ticks at roughly 3.6 times per
 *              _microsecond_, and its clock continues through Cx state
 *              transitions (unlike many CPU timestamp counters) -- making it
 *              a versatile and accurate timer.
 *
 *              Note that this function accomodates only a single timer
 *              rollover.  Thus for 24-bit timers, this function should only
 *              be used for calculating durations less than ~4.6 seconds
 *              (~20 hours for 32-bit timers).
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimerDuration (
    UINT32                  StartTicks,
    UINT32                  EndTicks,
    UINT32                  *TimeElapsed)
{
    UINT32                  DeltaTicks = 0;
    UINT32                  Seconds = 0;
    UINT32                  Milliseconds = 0;
    UINT32                  Microseconds = 0;
    UINT32                  Remainder = 0;


    FUNCTION_TRACE ("AcpiGetTimerDuration");


    if (!TimeElapsed)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Compute Tick Delta:
     * -------------------
     * Handle (max one) timer rollovers on 24- versus 32-bit timers.
     */
    if (StartTicks < EndTicks)
    {
        DeltaTicks = EndTicks - StartTicks;
    }

    else if (StartTicks > EndTicks)
    {
        /* 24-bit Timer */

        if (0 == AcpiGbl_FADT->TmrValExt)
        {
            DeltaTicks = (((0x00FFFFFF - StartTicks) + EndTicks) & 0x00FFFFFF);
        }

        /* 32-bit Timer */

        else
        {
            DeltaTicks = (0xFFFFFFFF - StartTicks) + EndTicks;
        }
    }

    else
    {
        *TimeElapsed = 0;
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Compute Duration:
     * -----------------
     * Since certain compilers (gcc/Linux, argh!) don't support 64-bit
     * divides in kernel-space we have to do some trickery to preserve
     * accuracy while using 32-bit math.
     *
     * TBD: Change to use 64-bit math when supported.
     *
     * The process is as follows:
     *  1. Compute the number of seconds by dividing Delta Ticks by
     *     the timer frequency.
     *  2. Compute the number of milliseconds in the remainder from step #1
     *     by multiplying by 1000 and then dividing by the timer frequency.
     *  3. Compute the number of microseconds in the remainder from step #2
     *     by multiplying by 1000 and then dividing by the timer frequency.
     *  4. Add the results from steps 1, 2, and 3 to get the total duration.
     *
     * Example: The time elapsed for DeltaTicks = 0xFFFFFFFF should be
     *          1199864031 microseconds.  This is computed as follows:
     *          Step #1: Seconds = 1199; Remainder = 3092840
     *          Step #2: Milliseconds = 864; Remainder = 113120
     *          Step #3: Microseconds = 31; Remainder = <don't care!>
     */

    /* Step #1 */

    Seconds = DeltaTicks / PM_TIMER_FREQUENCY;
    Remainder = DeltaTicks % PM_TIMER_FREQUENCY;

    /* Step #2 */

    Milliseconds = (Remainder * 1000) / PM_TIMER_FREQUENCY;
    Remainder = (Remainder * 1000) % PM_TIMER_FREQUENCY;

    /* Step #3 */

    Microseconds = (Remainder * 1000) / PM_TIMER_FREQUENCY;

    /* Step #4 */

    *TimeElapsed = Seconds * 1000000;
    *TimeElapsed += Milliseconds * 1000;
    *TimeElapsed += Microseconds;

    return_ACPI_STATUS (AE_OK);
}


