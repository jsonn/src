/******************************************************************************
 *
 * Module Name: exconvrt - Object conversion routines
 *              xRevision: 45 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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
__KERNEL_RCSID(0, "$NetBSD: exconvrt.c,v 1.2.4.5 2002/12/29 20:45:50 thorpej Exp $");

#define __EXCONVRT_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exconvrt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToInteger
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to an integer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToInteger (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;
    ACPI_OPERAND_OBJECT     *RetDesc;
    UINT32                  Count;
    UINT8                   *Pointer;
    ACPI_INTEGER            Result;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("ExConvertToInteger", ObjDesc);


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_INTEGER:
        *ResultDesc = ObjDesc;
        return_ACPI_STATUS (AE_OK);

    case ACPI_TYPE_STRING:
        Pointer = (UINT8 *) ObjDesc->String.Pointer;
        Count   = ObjDesc->String.Length;
        break;

    case ACPI_TYPE_BUFFER:
        Pointer = ObjDesc->Buffer.Pointer;
        Count   = ObjDesc->Buffer.Length;
        break;

    default:
        return_ACPI_STATUS (AE_TYPE);
    }

    /*
     * Convert the buffer/string to an integer.  Note that both buffers and
     * strings are treated as raw data - we don't convert ascii to hex for
     * strings.
     *
     * There are two terminating conditions for the loop:
     * 1) The size of an integer has been reached, or
     * 2) The end of the buffer or string has been reached
     */
    Result = 0;

    /* Transfer no more than an integer's worth of data */

    if (Count > AcpiGbl_IntegerByteWidth)
    {
        Count = AcpiGbl_IntegerByteWidth;
    }

    /*
     * String conversion is different than Buffer conversion
     */
    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_STRING:

        /*
         * Convert string to an integer
         * String must be hexadecimal as per the ACPI specification
         */
        Status = AcpiUtStrtoul64 ((char *) Pointer, 16, &Result);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
        break;


    case ACPI_TYPE_BUFFER:

        /*
         * Buffer conversion - we simply grab enough raw data from the
         * buffer to fill an integer
         */
        for (i = 0; i < Count; i++)
        {
            /*
             * Get next byte and shift it into the Result.
             * Little endian is used, meaning that the first byte of the buffer
             * is the LSB of the integer
             */
            Result |= (((ACPI_INTEGER) Pointer[i]) << (i * 8));
        }
        break;


    default:
        /* No other types can get here */
        break;
    }

    /*
     * Create a new integer
     */
    RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!RetDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Save the Result */

    RetDesc->Integer.Value = Result;

    /*
     * If we are about to overwrite the original object on the operand stack,
     * we must remove a reference on the original object because we are
     * essentially removing it from the stack.
     */
    if (*ResultDesc == ObjDesc)
    {
        if (WalkState->Opcode != AML_STORE_OP)
        {
            AcpiUtRemoveReference (ObjDesc);
        }
    }

    *ResultDesc = RetDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToBuffer
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a Buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToBuffer (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *RetDesc;
    UINT32                  i;
    UINT8                   *NewBuf;


    ACPI_FUNCTION_TRACE_PTR ("ExConvertToBuffer", ObjDesc);


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_BUFFER:

        /* No conversion necessary */

        *ResultDesc = ObjDesc;
        return_ACPI_STATUS (AE_OK);


    case ACPI_TYPE_INTEGER:

        /*
         * Create a new Buffer object.
         * Need enough space for one integer 
         */
        RetDesc = AcpiUtCreateBufferObject (AcpiGbl_IntegerByteWidth);
        if (!RetDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the integer to the buffer */

        NewBuf = RetDesc->Buffer.Pointer;
        for (i = 0; i < AcpiGbl_IntegerByteWidth; i++)
        {
            NewBuf[i] = (UINT8) (ObjDesc->Integer.Value >> (i * 8));
        }
        break;


    case ACPI_TYPE_STRING:

        /*
         * Create a new Buffer object
         * Size will be the string length
         */
        RetDesc = AcpiUtCreateBufferObject ((ACPI_SIZE) ObjDesc->String.Length);
        if (!RetDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the string to the buffer */

        NewBuf = RetDesc->Buffer.Pointer;
        ACPI_STRNCPY ((char *) NewBuf, (char *) ObjDesc->String.Pointer, 
            ObjDesc->String.Length);
        break;


    default:
        return_ACPI_STATUS (AE_TYPE);
    }

    /* Mark buffer initialized */

    RetDesc->Common.Flags |= AOPOBJ_DATA_VALID;

    /*
     * If we are about to overwrite the original object on the operand stack,
     * we must remove a reference on the original object because we are
     * essentially removing it from the stack.
     */
    if (*ResultDesc == ObjDesc)
    {
        if (WalkState->Opcode != AML_STORE_OP)
        {
            AcpiUtRemoveReference (ObjDesc);
        }
    }

    *ResultDesc = RetDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertAscii
 *
 * PARAMETERS:  Integer         - Value to be converted
 *              Base            - 10 or 16
 *              String          - Where the string is returned
 *
 * RETURN:      Actual string length
 *
 * DESCRIPTION: Convert an ACPI Integer to a hex or decimal string
 *
 ******************************************************************************/

UINT32
AcpiExConvertToAscii (
    ACPI_INTEGER            Integer,
    UINT32                  Base,
    UINT8                   *String)
{
    UINT32                  i;
    UINT32                  j;
    UINT32                  k = 0;
    char                    HexDigit;
    ACPI_INTEGER            Digit;
    UINT32                  Remainder;
    UINT32                  Length = sizeof (ACPI_INTEGER);
    BOOLEAN                 LeadingZero = TRUE;


    ACPI_FUNCTION_ENTRY ();


    switch (Base)
    {
    case 10:

        Remainder = 0;
        for (i = ACPI_MAX_DECIMAL_DIGITS; i > 0 ; i--)
        {
            /* Divide by nth factor of 10 */

            Digit = Integer;
            for (j = 1; j < i; j++)
            {
                (void) AcpiUtShortDivide (&Digit, 10, &Digit, &Remainder);
            }

            /* Create the decimal digit */

            if (Digit != 0)
            {
                LeadingZero = FALSE;
            }

            if (!LeadingZero)
            {
                String[k] = (UINT8) (ACPI_ASCII_ZERO + Remainder);
                k++;
            }
        }
        break;

    case 16:

        /* Copy the integer to the buffer */

        for (i = 0, j = ((Length * 2) -1); i < (Length * 2); i++, j--)
        {

            HexDigit = AcpiUtHexToAsciiChar (Integer, (j * 4));
            if (HexDigit != ACPI_ASCII_ZERO)
            {
                LeadingZero = FALSE;
            }

            if (!LeadingZero)
            {
                String[k] = (UINT8) HexDigit;
                k++;
            }
        }
        break;

    default:
        break;
    }

    /*
     * Since leading zeros are supressed, we must check for the case where
     * the integer equals 0.
     *
     * Finally, null terminate the string and return the length
     */
    if (!k)
    {
        String [0] = ACPI_ASCII_ZERO;
        k = 1;
    }
    String [k] = 0;

    return (k);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToString
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a string
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToString (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    UINT32                  Base,
    UINT32                  MaxLength,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *RetDesc;
    UINT32                  i;
    UINT32                  Index;
    UINT32                  StringLength;
    UINT8                   *NewBuf;
    UINT8                   *Pointer;


    ACPI_FUNCTION_TRACE_PTR ("ExConvertToString", ObjDesc);


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_STRING:

        if (MaxLength >= ObjDesc->String.Length)
        {
            *ResultDesc = ObjDesc;
            return_ACPI_STATUS (AE_OK);
        }
        else
        {
            /* Must copy the string first and then truncate it */

            return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
        }


    case ACPI_TYPE_INTEGER:

        StringLength = AcpiGbl_IntegerByteWidth * 2;
        if (Base == 10)
        {
            StringLength = ACPI_MAX_DECIMAL_DIGITS;
        }

        /*
         * Create a new String
         */
        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_STRING);
        if (!RetDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Need enough space for one ASCII integer plus null terminator */

        NewBuf = ACPI_MEM_CALLOCATE ((ACPI_SIZE) StringLength + 1);
        if (!NewBuf)
        {
            ACPI_REPORT_ERROR
                (("ExConvertToString: Buffer allocation failure\n"));
            AcpiUtRemoveReference (RetDesc);
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Convert */

        i = AcpiExConvertToAscii (ObjDesc->Integer.Value, Base, NewBuf);

        /* Null terminate at the correct place */

        if (MaxLength < i)
        {
            NewBuf[MaxLength] = 0;
            RetDesc->String.Length = MaxLength;
        }
        else
        {
            NewBuf [i] = 0;
            RetDesc->String.Length = i;
        }

        RetDesc->Buffer.Pointer = NewBuf;
        break;


    case ACPI_TYPE_BUFFER:

        StringLength = ObjDesc->Buffer.Length * 3;
        if (Base == 10)
        {
            StringLength = ObjDesc->Buffer.Length * 4;
        }

        if (MaxLength > ACPI_MAX_STRING_CONVERSION)
        {
            if (StringLength > ACPI_MAX_STRING_CONVERSION)
            {
                return_ACPI_STATUS (AE_AML_STRING_LIMIT);
            }
        }

        /*
         * Create a new string object
         */
        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_STRING);
        if (!RetDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* String length is the lesser of the Max or the actual length */

        if (MaxLength < StringLength)
        {
            StringLength = MaxLength;
        }

        NewBuf = ACPI_MEM_CALLOCATE ((ACPI_SIZE) StringLength + 1);
        if (!NewBuf)
        {
            ACPI_REPORT_ERROR
                (("ExConvertToString: Buffer allocation failure\n"));
            AcpiUtRemoveReference (RetDesc);
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /*
         * Convert each byte of the buffer to two ASCII characters plus a space.
         */
        Pointer = ObjDesc->Buffer.Pointer;
        Index = 0;
        for (i = 0, Index = 0; i < ObjDesc->Buffer.Length; i++)
        {
            Index = AcpiExConvertToAscii ((ACPI_INTEGER) Pointer[i], Base, &NewBuf[Index]);

            NewBuf[Index] = ' ';
            Index++;
        }

        /* Null terminate */

        NewBuf [Index-1] = 0;
        RetDesc->Buffer.Pointer = NewBuf;
        RetDesc->String.Length = (UINT32) ACPI_STRLEN ((char *) NewBuf);
        break;


    default:
        return_ACPI_STATUS (AE_TYPE);
    }


    /*
     * If we are about to overwrite the original object on the operand stack,
     * we must remove a reference on the original object because we are
     * essentially removing it from the stack.
     */
    if (*ResultDesc == ObjDesc)
    {
        if (WalkState->Opcode != AML_STORE_OP)
        {
            AcpiUtRemoveReference (ObjDesc);
        }
    }

    *ResultDesc = RetDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToTargetType
 *
 * PARAMETERS:  DestinationType     - Current type of the destination
 *              SourceDesc          - Source object to be converted.
 *              WalkState           - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Implements "implicit conversion" rules for storing an object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToTargetType (
    ACPI_OBJECT_TYPE        DestinationType,
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("ExConvertToTargetType");


    /* Default behavior */

    *ResultDesc = SourceDesc;

    /*
     * If required by the target,
     * perform implicit conversion on the source before we store it.
     */
    switch (GET_CURRENT_ARG_TYPE (WalkState->OpInfo->RuntimeArgs))
    {
    case ARGI_SIMPLE_TARGET:
    case ARGI_FIXED_TARGET:
    case ARGI_INTEGER_REF:      /* Handles Increment, Decrement cases */

        switch (DestinationType)
        {
        case ACPI_TYPE_LOCAL_REGION_FIELD:
            /*
             * Named field can always handle conversions
             */
            break;

        default:
            /* No conversion allowed for these types */

            if (DestinationType != ACPI_GET_OBJECT_TYPE (SourceDesc))
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                    "Explicit operator, will store (%s) over existing type (%s)\n",
                    AcpiUtGetObjectTypeName (SourceDesc),
                    AcpiUtGetTypeName (DestinationType)));
                Status = AE_TYPE;
            }
        }
        break;


    case ARGI_TARGETREF:

        switch (DestinationType)
        {
        case ACPI_TYPE_INTEGER:
        case ACPI_TYPE_BUFFER_FIELD:
        case ACPI_TYPE_LOCAL_BANK_FIELD:
        case ACPI_TYPE_LOCAL_INDEX_FIELD:
            /*
             * These types require an Integer operand.  We can convert
             * a Buffer or a String to an Integer if necessary.
             */
            Status = AcpiExConvertToInteger (SourceDesc, ResultDesc, WalkState);
            break;


        case ACPI_TYPE_STRING:

            /*
             * The operand must be a String.  We can convert an
             * Integer or Buffer if necessary
             */
            Status = AcpiExConvertToString (SourceDesc, ResultDesc, 16, ACPI_UINT32_MAX, WalkState);
            break;


        case ACPI_TYPE_BUFFER:

            /*
             * The operand must be a Buffer.  We can convert an
             * Integer or String if necessary
             */
            Status = AcpiExConvertToBuffer (SourceDesc, ResultDesc, WalkState);
            break;


        default:
            Status = AE_AML_INTERNAL;
            break;
        }
        break;


    case ARGI_REFERENCE:
        /*
         * CreateXxxxField cases - we are storing the field object into the name
         */
        break;


    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Unknown Target type ID 0x%X Op %s DestType %s\n",
            GET_CURRENT_ARG_TYPE (WalkState->OpInfo->RuntimeArgs),
            WalkState->OpInfo->Name, AcpiUtGetTypeName (DestinationType)));

        Status = AE_AML_INTERNAL;
    }

    /*
     * Source-to-Target conversion semantics:
     *
     * If conversion to the target type cannot be performed, then simply
     * overwrite the target with the new object and type.
     */
    if (Status == AE_TYPE)
    {
        Status = AE_OK;
    }

    return_ACPI_STATUS (Status);
}


