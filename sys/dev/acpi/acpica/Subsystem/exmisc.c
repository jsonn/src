
/******************************************************************************
 *
 * Module Name: exmisc - ACPI AML (p-code) execution - specific opcodes
 *              xRevision: 83 $
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
__KERNEL_RCSID(0, "$NetBSD: exmisc.c,v 1.2.2.2 2002/01/10 19:53:18 thorpej Exp $");

#define __EXMISC_C__

#include "acpi.h"
#include "acparser.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exmisc")



/*******************************************************************************
 *
 * FUNCTION:    AcpiExTriadic
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *              WalkState           - Current walk state
 *              ReturnDesc          - Where to store the return object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Triadic operator (3 operands)
 *
 * ALLOCATION:  Deletes one operand descriptor -- other remains on stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExTriadic (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_OPERAND_OBJECT     *TmpDesc;
    ACPI_SIGNAL_FATAL_INFO  *Fatal;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("ExTriadic");


#define ObjDesc1            Operand[0]
#define ObjDesc2            Operand[1]
#define ResDesc             Operand[2]


    switch (Opcode)
    {

    case AML_FATAL_OP:

        /* DefFatal    :=  FatalOp  FatalType   FatalCode   FatalArg    */

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
            "FatalOp: Type %x Code %x Arg %x <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n",
            (UINT32) ObjDesc1->Integer.Value, (UINT32) ObjDesc2->Integer.Value,
            (UINT32) ResDesc->Integer.Value));


        Fatal = ACPI_MEM_ALLOCATE (sizeof (ACPI_SIGNAL_FATAL_INFO));
        if (Fatal)
        {
            Fatal->Type = (UINT32) ObjDesc1->Integer.Value;
            Fatal->Code = (UINT32) ObjDesc2->Integer.Value;
            Fatal->Argument = (UINT32) ResDesc->Integer.Value;
        }

        /*
         * Signal the OS
         */
        AcpiOsSignal (ACPI_SIGNAL_FATAL, Fatal);

        /* Might return while OS is shutting down */

        ACPI_MEM_FREE (Fatal);
        break;


    case AML_MID_OP:

        /* DefMid       := MidOp Source Index  Length Result */

        /* Create the internal return object (string or buffer) */

        break;


    case AML_INDEX_OP:

        /* DefIndex     := IndexOp Source Index Destination */

        /* Create the internal return object */

        RetDesc = AcpiUtCreateInternalObject (INTERNAL_TYPE_REFERENCE);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /*
         * At this point, the ObjDesc1 operand is either a Package or a Buffer
         */
        if (ObjDesc1->Common.Type == ACPI_TYPE_PACKAGE)
        {
            /* Object to be indexed is a Package */

            if (ObjDesc2->Integer.Value >= ObjDesc1->Package.Count)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Index value beyond package end\n"));
                Status = AE_AML_PACKAGE_LIMIT;
                goto Cleanup;
            }

            if ((ResDesc->Common.Type == INTERNAL_TYPE_REFERENCE) &&
                (ResDesc->Reference.Opcode == AML_ZERO_OP))
            {
                /*
                 * There is no actual result descriptor (the ZeroOp Result
                 * descriptor is a placeholder), so just delete the placeholder and
                 * return a reference to the package element
                 */
                AcpiUtRemoveReference (ResDesc);
            }

            else
            {
                /*
                 * Each element of the package is an internal object.  Get the one
                 * we are after.
                 */
                TmpDesc                       = ObjDesc1->Package.Elements[ObjDesc2->Integer.Value];
                RetDesc->Reference.Opcode     = AML_INDEX_OP;
                RetDesc->Reference.TargetType = TmpDesc->Common.Type;
                RetDesc->Reference.Object     = TmpDesc;

                Status = AcpiExStore (RetDesc, ResDesc, WalkState);
                RetDesc->Reference.Object     = NULL;
            }

            /*
             * The local return object must always be a reference to the package element,
             * not the element itself.
             */
            RetDesc->Reference.Opcode     = AML_INDEX_OP;
            RetDesc->Reference.TargetType = ACPI_TYPE_PACKAGE;
            RetDesc->Reference.Where      = &ObjDesc1->Package.Elements[ObjDesc2->Integer.Value];
        }

        else
        {
            /* Object to be indexed is a Buffer */

            if (ObjDesc2->Integer.Value >= ObjDesc1->Buffer.Length)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Index value beyond end of buffer\n"));
                Status = AE_AML_BUFFER_LIMIT;
                goto Cleanup;
            }

            RetDesc->Reference.Opcode       = AML_INDEX_OP;
            RetDesc->Reference.TargetType   = ACPI_TYPE_BUFFER_FIELD;
            RetDesc->Reference.Object       = ObjDesc1;
            RetDesc->Reference.Offset       = (UINT32) ObjDesc2->Integer.Value;

            Status = AcpiExStore (RetDesc, ResDesc, WalkState);
        }
        break;
    }


Cleanup:

    /* Always delete operands */

    AcpiUtRemoveReference (ObjDesc1);
    AcpiUtRemoveReference (ObjDesc2);

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ResDesc);

        if (RetDesc)
        {
            AcpiUtRemoveReference (RetDesc);
            RetDesc = NULL;
        }
    }

    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExHexadic
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *              WalkState           - Current walk state
 *              ReturnDesc          - Where to store the return object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Match operator
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExHexadic (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Index;
    UINT32                  MatchValue = (UINT32) -1;


    FUNCTION_TRACE ("ExHexadic");

#define PkgDesc             Operand[0]
#define Op1Desc             Operand[1]
#define V1Desc              Operand[2]
#define Op2Desc             Operand[3]
#define V2Desc              Operand[4]
#define StartDesc           Operand[5]



    switch (Opcode)
    {

        case AML_MATCH_OP:

        /* Validate match comparison sub-opcodes */

        if ((Op1Desc->Integer.Value > MAX_MATCH_OPERATOR) ||
            (Op2Desc->Integer.Value > MAX_MATCH_OPERATOR))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "operation encoding out of range\n"));
            Status = AE_AML_OPERAND_VALUE;
            goto Cleanup;
        }

        Index = (UINT32) StartDesc->Integer.Value;
        if (Index >= (UINT32) PkgDesc->Package.Count)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Start position value out of range\n"));
            Status = AE_AML_PACKAGE_LIMIT;
            goto Cleanup;
        }

        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;

        }

        /*
         * Examine each element until a match is found.  Within the loop,
         * "continue" signifies that the current element does not match
         * and the next should be examined.
         * Upon finding a match, the loop will terminate via "break" at
         * the bottom.  If it terminates "normally", MatchValue will be -1
         * (its initial value) indicating that no match was found.  When
         * returned as a Number, this will produce the Ones value as specified.
         */
        for ( ; Index < PkgDesc->Package.Count; ++Index)
        {
            /*
             * Treat any NULL or non-numeric elements as non-matching.
             * TBD [Unhandled] - if an element is a Name,
             *      should we examine its value?
             */
            if (!PkgDesc->Package.Elements[Index] ||
                ACPI_TYPE_INTEGER != PkgDesc->Package.Elements[Index]->Common.Type)
            {
                continue;
            }

            /*
             * Within these switch statements:
             *      "break" (exit from the switch) signifies a match;
             *      "continue" (proceed to next iteration of enclosing
             *          "for" loop) signifies a non-match.
             */
            switch (Op1Desc->Integer.Value)
            {

            case MATCH_MTR:   /* always true */

                break;


            case MATCH_MEQ:   /* true if equal   */

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     != V1Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MLE:   /* true if less than or equal  */

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     > V1Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MLT:   /* true if less than   */

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     >= V1Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MGE:   /* true if greater than or equal   */

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     < V1Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MGT:   /* true if greater than    */

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     <= V1Desc->Integer.Value)
                {
                    continue;
                }
                break;


            default:    /* undefined   */

                continue;
            }


            switch(Op2Desc->Integer.Value)
            {

            case MATCH_MTR:

                break;


            case MATCH_MEQ:

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     != V2Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MLE:

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     > V2Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MLT:

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     >= V2Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MGE:

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     < V2Desc->Integer.Value)
                {
                    continue;
                }
                break;


            case MATCH_MGT:

                if (PkgDesc->Package.Elements[Index]->Integer.Value
                     <= V2Desc->Integer.Value)
                {
                    continue;
                }
                break;


            default:

                continue;
            }

            /* Match found: exit from loop */

            MatchValue = Index;
            break;
        }

        /* MatchValue is the return value */

        RetDesc->Integer.Value = MatchValue;
        break;

    }


Cleanup:

    /* Free the operands */

    AcpiUtRemoveReference (StartDesc);
    AcpiUtRemoveReference (V2Desc);
    AcpiUtRemoveReference (Op2Desc);
    AcpiUtRemoveReference (V1Desc);
    AcpiUtRemoveReference (Op1Desc);
    AcpiUtRemoveReference (PkgDesc);


    /* Delete return object on error */

    if (ACPI_FAILURE (Status) &&
        (RetDesc))
    {
        AcpiUtRemoveReference (RetDesc);
        RetDesc = NULL;
    }


    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}
