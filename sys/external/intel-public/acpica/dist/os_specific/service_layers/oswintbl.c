/******************************************************************************
 *
 * Module Name: oswintbl - Windows OSL for obtaining ACPI tables
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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


#ifdef WIN32
#pragma warning(disable:4115)   /* warning C4115: (caused by rpcasync.h) */

#include <windows.h>
#include <winbase.h>

#elif WIN64
#include <windowsx.h>
#endif

#include "acpi.h"
#include "accommon.h"

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("oswintbl")


static char             KeyBuffer[64];
static char             ErrorBuffer[64];


/* Little front-end to win FormatMessage */

char *
OsFormatException (
    LONG                Status)
{

    ErrorBuffer[0] = 0;
    FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, Status, 0,
        ErrorBuffer, 64, NULL);

    return (ErrorBuffer);
}


/******************************************************************************
 *
 * FUNCTION:    OsGetTable
 *
 * PARAMETERS:  Signature       - ACPI Signature for desired table. must be
 *                                  a null terminated string.
 *
 * RETURN:      Pointer to the table. NULL if failure.
 *
 * DESCRIPTION: Get an ACPI table from the Windows registry.
 *
 *****************************************************************************/

ACPI_TABLE_HEADER *
OsGetTable (
    char                *Signature)
{
    HKEY                Handle = NULL;
    ULONG               i;
    LONG                Status;
    ULONG               Type;
    ULONG               NameSize;
    ULONG               DataSize;
    HKEY                SubKey;
    ACPI_TABLE_HEADER   *ReturnTable;


    /* Get a handle to the table key */

    while (1)
    {
        ACPI_STRCPY (KeyBuffer, "HARDWARE\\ACPI\\");
        ACPI_STRCAT (KeyBuffer, Signature);

        Status = RegOpenKeyEx (HKEY_LOCAL_MACHINE, KeyBuffer,
                    0L, KEY_READ, &Handle);

        if (Status != ERROR_SUCCESS)
        {
            /*
             * Somewhere along the way, MS changed the registry entry for
             * the FADT from
             * HARDWARE/ACPI/FACP  to
             * HARDWARE/ACPI/FADT.
             *
             * This code allows for both.
             */
            if (ACPI_COMPARE_NAME (Signature, "FACP"))
            {
                Signature = "FADT";
            }
            else
            {
                AcpiOsPrintf (
                    "Could not find %s in registry at %s: %s (Status=0x%X)\n",
                    Signature, KeyBuffer, OsFormatException (Status), Status);
                return (NULL);
            }
        }
        else
        {
            break;
        }
    }

    /* Actual data for table is down a couple levels */

    for (i = 0; ;)
    {
        Status = RegEnumKey (Handle, i, KeyBuffer, sizeof (KeyBuffer));
        i += 1;
        if (Status == ERROR_NO_MORE_ITEMS)
        {
            break;
        }

        Status = RegOpenKey (Handle, KeyBuffer, &SubKey);
        if (Status != ERROR_SUCCESS)
        {
            AcpiOsPrintf ("Could not open %s entry: %s\n",
                Signature, OsFormatException (Status));
            return (NULL);
        }

        RegCloseKey (Handle);
        Handle = SubKey;
        i = 0;
    }

    /* Find the (binary) table entry */

    for (i = 0; ;)
    {
        NameSize = sizeof (KeyBuffer);
        Status = RegEnumValue (Handle, i, KeyBuffer, &NameSize,
                    NULL, &Type, NULL, 0);
        if (Status != ERROR_SUCCESS)
        {
            AcpiOsPrintf ("Could not get %s registry entry: %s\n",
                Signature, OsFormatException (Status));
            return (NULL);
        }

        if (Type == REG_BINARY)
        {
            break;
        }
        i += 1;
    }

    /* Get the size of the table */

    Status = RegQueryValueEx (Handle, KeyBuffer, NULL, NULL, NULL, &DataSize);
    if (Status != ERROR_SUCCESS)
    {
        AcpiOsPrintf ("Could not read the %s table size: %s\n",
            Signature, OsFormatException (Status));
        return (NULL);
    }

    /* Allocate a new buffer for the table */

    ReturnTable = AcpiOsAllocate (DataSize);
    if (!ReturnTable)
    {
        goto Cleanup;
    }

    /* Get the actual table from the registry */

    Status = RegQueryValueEx (Handle, KeyBuffer, NULL, NULL,
                (UCHAR *) ReturnTable, &DataSize);
    if (Status != ERROR_SUCCESS)
    {
        AcpiOsPrintf ("Could not read %s data: %s\n",
            Signature, OsFormatException (Status));
        AcpiOsFree (ReturnTable);
        return (NULL);
    }

Cleanup:
    RegCloseKey (Handle);
    return (ReturnTable);
}

