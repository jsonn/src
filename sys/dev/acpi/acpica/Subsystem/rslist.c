/*******************************************************************************
 *
 * Module Name: rslist - Linked list utilities
 *              xRevision: 1.52 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2006, Intel Corp.
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
__KERNEL_RCSID(0, "$NetBSD: rslist.c,v 1.12.2.1 2006/02/01 14:51:51 yamt Exp $");

#define __RSLIST_C__

#include "acpi.h"
#include "acresrc.h"

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rslist")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsConvertAmlToResources
 *
 * PARAMETERS:  Aml                 - Pointer to the resource byte stream
 *              AmlLength           - Length of Aml
 *              OutputBuffer        - Pointer to the buffer that will
 *                                    contain the output structures
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes the resource byte stream and parses it, creating a
 *              linked list of resources in the caller's output buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsConvertAmlToResources (
    UINT8                   *Aml,
    UINT32                  AmlLength,
    UINT8                   *OutputBuffer)
{
    ACPI_RESOURCE           *Resource = (void *) OutputBuffer;
    ACPI_STATUS             Status;
    UINT8                   ResourceIndex;
    UINT8                   *EndAml;


    ACPI_FUNCTION_TRACE ("RsConvertAmlToResources");


    EndAml = Aml + AmlLength;

    /* Loop until end-of-buffer or an EndTag is found */

    while (Aml < EndAml)
    {
        /* Validate the Resource Type and Resource Length */

        Status = AcpiUtValidateResource (Aml, &ResourceIndex);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Convert the AML byte stream resource to a local resource struct */

        Status = AcpiRsConvertAmlToResource (
                    Resource, ACPI_CAST_PTR (AML_RESOURCE, Aml),
                    AcpiGbl_GetResourceDispatch[ResourceIndex]);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR ((
                "Could not convert AML resource (Type %X) to resource, %s\n",
                *Aml, AcpiFormatException (Status)));
            return_ACPI_STATUS (Status);
        }

        /* Normal exit on completion of an EndTag resource descriptor */

        if (AcpiUtGetResourceType (Aml) == ACPI_RESOURCE_NAME_END_TAG)
        {
            return_ACPI_STATUS (AE_OK);
        }

        /* Point to the next input AML resource */

        Aml += AcpiUtGetDescriptorLength (Aml);

        /* Point to the next structure in the output buffer */

        Resource = ACPI_ADD_PTR (ACPI_RESOURCE, Resource, Resource->Length);
    }

    /* Did not find an EndTag resource descriptor */

    return_ACPI_STATUS (AE_AML_NO_RESOURCE_END_TAG);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsConvertResourcesToAml
 *
 * PARAMETERS:  Resource            - Pointer to the resource linked list
 *              AmlSizeNeeded       - Calculated size of the byte stream
 *                                    needed from calling AcpiRsGetAmlLength()
 *                                    The size of the OutputBuffer is
 *                                    guaranteed to be >= AmlSizeNeeded
 *              OutputBuffer        - Pointer to the buffer that will
 *                                    contain the byte stream
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes the resource linked list and parses it, creating a
 *              byte stream of resources in the caller's output buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsConvertResourcesToAml (
    ACPI_RESOURCE           *Resource,
    ACPI_SIZE               AmlSizeNeeded,
    UINT8                   *OutputBuffer)
{
    UINT8                   *Aml = OutputBuffer;
    UINT8                   *EndAml = OutputBuffer + AmlSizeNeeded;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsConvertResourcesToAml");


    /* Walk the resource descriptor list, convert each descriptor */

    while (Aml < EndAml)
    {
        /* Validate the (internal) Resource Type */

        if (Resource->Type > ACPI_RESOURCE_TYPE_MAX)
        {
            ACPI_REPORT_ERROR ((
                "Invalid descriptor type (%X) in resource list\n",
                Resource->Type));
            return_ACPI_STATUS (AE_BAD_DATA);
        }

        /* Perform the conversion */

        Status = AcpiRsConvertResourceToAml (Resource,
                    ACPI_CAST_PTR (AML_RESOURCE, Aml),
                    AcpiGbl_SetResourceDispatch[Resource->Type]);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("Could not convert resource (type %X) to AML, %s\n",
                Resource->Type, AcpiFormatException (Status)));
            return_ACPI_STATUS (Status);
        }

        /* Perform final sanity check on the new AML resource descriptor */

        Status = AcpiUtValidateResource (
                    ACPI_CAST_PTR (AML_RESOURCE, Aml), NULL);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Check for end-of-list, normal exit */

        if (Resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
        {
            /* An End Tag indicates the end of the input Resource Template */

            return_ACPI_STATUS (AE_OK);
        }

        /*
         * Extract the total length of the new descriptor and set the
         * Aml to point to the next (output) resource descriptor
         */
        Aml += AcpiUtGetDescriptorLength (Aml);

        /* Point to the next input resource descriptor */

        Resource = ACPI_ADD_PTR (ACPI_RESOURCE, Resource, Resource->Length);
    }

    /* Completed buffer, but did not find an EndTag resource descriptor */

    return_ACPI_STATUS (AE_AML_NO_RESOURCE_END_TAG);
}

