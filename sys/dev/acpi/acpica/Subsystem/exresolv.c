
/******************************************************************************
 *
 * Module Name: exresolv - AML Interpreter object resolution
 *              xRevision: 97 $
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
__KERNEL_RCSID(0, "$NetBSD: exresolv.c,v 1.2.2.2 2002/01/10 19:53:20 thorpej Exp $");

#define __EXRESOLV_C__

#include "acpi.h"
#include "amlcode.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"
#include "acevents.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exresolv")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExGetBufferFieldValue
 *
 * PARAMETERS:  *ObjDesc            - Pointer to a BufferField
 *              *ResultDesc         - Pointer to an empty descriptor which will
 *                                    become an Integer with the field's value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieve the value from a BufferField
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExGetBufferFieldValue (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     *ResultDesc)
{
    ACPI_STATUS             Status;
    UINT32                  Mask;
    UINT8                   *Location;


    FUNCTION_TRACE ("ExGetBufferFieldValue");


    /*
     * Parameter validation
     */
    if (!ObjDesc)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - null field pointer\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    if (!(ObjDesc->Common.Flags & AOPOBJ_DATA_VALID))
    {
        Status = AcpiDsGetBufferFieldArguments (ObjDesc);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    if (!ObjDesc->BufferField.BufferObj)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - null container pointer\n"));
        return_ACPI_STATUS (AE_AML_INTERNAL);
    }

    if (ACPI_TYPE_BUFFER != ObjDesc->BufferField.BufferObj->Common.Type)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - container is not a Buffer\n"));
        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    if (!ResultDesc)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - null result pointer\n"));
        return_ACPI_STATUS (AE_AML_INTERNAL);
    }


    /* Field location is (base of buffer) + (byte offset) */

    Location = ObjDesc->BufferField.BufferObj->Buffer.Pointer
                + ObjDesc->BufferField.BaseByteOffset;

    /*
     * Construct Mask with as many 1 bits as the field width
     *
     * NOTE: Only the bottom 5 bits are valid for a shift operation, so
     *  special care must be taken for any shift greater than 31 bits.
     *
     * TBD: [Unhandled] Fields greater than 32 bits will not work.
     */
    if (ObjDesc->BufferField.BitLength < 32)
    {
        Mask = ((UINT32) 1 << ObjDesc->BufferField.BitLength) - (UINT32) 1;
    }
    else
    {
        Mask = ACPI_UINT32_MAX;
    }

    ResultDesc->Integer.Type = (UINT8) ACPI_TYPE_INTEGER;

    /* Get the 32 bit value at the location */

    MOVE_UNALIGNED32_TO_32 (&ResultDesc->Integer.Value, Location);

    /*
     * Shift the 32-bit word containing the field, and mask off the
     * resulting value
     */
    ResultDesc->Integer.Value =
        (ResultDesc->Integer.Value >> ObjDesc->BufferField.StartFieldBitOffset) & Mask;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "** Read from buffer %p byte %ld bit %d width %d addr %p mask %08lx val %08lx\n",
        ObjDesc->BufferField.BufferObj->Buffer.Pointer,
        ObjDesc->BufferField.BaseByteOffset,
        ObjDesc->BufferField.StartFieldBitOffset,
        ObjDesc->BufferField.BitLength,
        Location, Mask, ResultDesc->Integer.Value));

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExResolveToValue
 *
 * PARAMETERS:  **StackPtr          - Points to entry on ObjStack, which can
 *                                    be either an (ACPI_OPERAND_OBJECT *)
 *                                    or an ACPI_HANDLE.
 *              WalkState           - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert Reference objects to values
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExResolveToValue (
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("ExResolveToValue", StackPtr);


    if (!StackPtr || !*StackPtr)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - null pointer\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }


    /*
     * The entity pointed to by the StackPtr can be either
     * 1) A valid ACPI_OPERAND_OBJECT, or
     * 2) A ACPI_NAMESPACE_NODE (NamedObj)
     */
    if (VALID_DESCRIPTOR_TYPE (*StackPtr, ACPI_DESC_TYPE_INTERNAL))
    {
        Status = AcpiExResolveObjectToValue (StackPtr, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Object on the stack may have changed if AcpiExResolveObjectToValue()
     * was called (i.e., we can't use an _else_ here.)
     */
    if (VALID_DESCRIPTOR_TYPE (*StackPtr, ACPI_DESC_TYPE_NAMED))
    {
        Status = AcpiExResolveNodeToValue ((ACPI_NAMESPACE_NODE **) StackPtr,
                        WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }


    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Resolved object %p\n", *StackPtr));
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExResolveObjectToValue
 *
 * PARAMETERS:  StackPtr        - Pointer to a stack location that contains a
 *                                ptr to an internal object.
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieve the value from an internal object.  The Reference type
 *              uses the associated AML opcode to determine the value.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExResolveObjectToValue (
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *StackDesc;
    void                    *TempNode;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT16                  Opcode;


    FUNCTION_TRACE ("ExResolveObjectToValue");


    StackDesc = *StackPtr;

    /* This is an ACPI_OPERAND_OBJECT  */

    switch (StackDesc->Common.Type)
    {

    case INTERNAL_TYPE_REFERENCE:

        Opcode = StackDesc->Reference.Opcode;

        switch (Opcode)
        {

        case AML_NAME_OP:

            /*
             * Convert indirect name ptr to a direct name ptr.
             * Then, AcpiExResolveNodeToValue can be used to get the value
             */
            TempNode = StackDesc->Reference.Object;

            /* Delete the Reference Object */

            AcpiUtRemoveReference (StackDesc);

            /* Put direct name pointer onto stack and exit */

            (*StackPtr) = TempNode;
            break;


        case AML_LOCAL_OP:
        case AML_ARG_OP:

            /*
             * Get the local from the method's state info
             * Note: this increments the local's object reference count
             */
            Status = AcpiDsMethodDataGetValue (Opcode,
                            StackDesc->Reference.Offset, WalkState, &ObjDesc);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            /*
             * Now we can delete the original Reference Object and
             * replace it with the resolve value
             */
            AcpiUtRemoveReference (StackDesc);
            *StackPtr = ObjDesc;

            ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "[Arg/Local %d] ValueObj is %p\n",
                StackDesc->Reference.Offset, ObjDesc));
            break;


        /*
         * TBD: [Restructure] These next three opcodes change the type of
         * the object, which is actually a no-no.
         */
        case AML_ZERO_OP:

            StackDesc->Common.Type = (UINT8) ACPI_TYPE_INTEGER;
            StackDesc->Integer.Value = 0;
            break;


        case AML_ONE_OP:

            StackDesc->Common.Type = (UINT8) ACPI_TYPE_INTEGER;
            StackDesc->Integer.Value = 1;
            break;


        case AML_ONES_OP:

            StackDesc->Common.Type = (UINT8) ACPI_TYPE_INTEGER;
            StackDesc->Integer.Value = ACPI_INTEGER_MAX;

            /* Truncate value if we are executing from a 32-bit ACPI table */

            AcpiExTruncateFor32bitTable (StackDesc, WalkState);
            break;


        case AML_INDEX_OP:

            switch (StackDesc->Reference.TargetType)
            {
            case ACPI_TYPE_BUFFER_FIELD:

                /* Just return - leave the Reference on the stack */
                break;


            case ACPI_TYPE_PACKAGE:
                ObjDesc = *StackDesc->Reference.Where;
                if (ObjDesc)
                {
                    /*
                     * Valid obj descriptor, copy pointer to return value
                     * (i.e., dereference the package index)
                     * Delete the ref object, increment the returned object
                     */
                    AcpiUtRemoveReference (StackDesc);
                    AcpiUtAddReference (ObjDesc);
                    *StackPtr = ObjDesc;
                }

                else
                {
                    /*
                     * A NULL object descriptor means an unitialized element of
                     * the package, can't dereference it
                     */
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Attempt to deref an Index to NULL pkg element Idx=%p\n",
                        StackDesc));
                    Status = AE_AML_UNINITIALIZED_ELEMENT;
                }
                break;

            default:
                /* Invalid reference object */

                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Unknown TargetType %X in Index/Reference obj %p\n",
                    StackDesc->Reference.TargetType, StackDesc));
                Status = AE_AML_INTERNAL;
                break;
            }

            break;


        case AML_DEBUG_OP:

            /* Just leave the object as-is */
            break;


        default:

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown Reference object subtype %02X in %p\n",
                Opcode, StackDesc));
            Status = AE_AML_INTERNAL;
            break;

        }   /* switch (Opcode) */

        break; /* case INTERNAL_TYPE_REFERENCE */


    case ACPI_TYPE_BUFFER_FIELD:

        ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_ANY);
        if (!ObjDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        Status = AcpiExGetBufferFieldValue (StackDesc, ObjDesc);
        if (ACPI_FAILURE (Status))
        {
            AcpiUtRemoveReference (ObjDesc);
            ObjDesc = NULL;
        }

        *StackPtr = (void *) ObjDesc;
        break;


    case INTERNAL_TYPE_BANK_FIELD:

        ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_ANY);
        if (!ObjDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* TBD: WRONG! */

        Status = AcpiExGetBufferFieldValue (StackDesc, ObjDesc);
        if (ACPI_FAILURE (Status))
        {
            AcpiUtRemoveReference (ObjDesc);
            ObjDesc = NULL;
        }

        *StackPtr = (void *) ObjDesc;
        break;


    /* TBD: [Future] - may need to handle IndexField, and DefField someday */

    default:

        break;

    }   /* switch (StackDesc->Common.Type) */


    return_ACPI_STATUS (Status);
}


