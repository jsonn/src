/*******************************************************************************
 *
 * Module Name: dbxface - AML Debugger external interfaces
 *              xRevision: 41 $
 *
 ******************************************************************************/

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


#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acparser.h"
#include "acevents.h"
#include "acinterp.h"
#include "acdebug.h"


#ifdef ENABLE_DEBUGGER

#define _COMPONENT          ACPI_DEBUGGER
        MODULE_NAME         ("dbxface")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSingleStep
 *
 * PARAMETERS:  WalkState       - Current walk
 *              Op              - Current executing op
 *              OpType          - Type of the current AML Opcode
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Called just before execution of an AML opcode.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbSingleStep (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   OpType)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  OriginalDebugLevel;
    ACPI_PARSE_OBJECT       *DisplayOp;


    FUNCTION_ENTRY ();

    /* Is there a breakpoint set? */

    if (WalkState->MethodBreakpoint)
    {
        /* Check if the breakpoint has been reached or passed */

        if (WalkState->MethodBreakpoint <= Op->AmlOffset)
        {
            /* Hit the breakpoint, resume single step, reset breakpoint */

            AcpiOsPrintf ("***Break*** at AML offset %X\n", Op->AmlOffset);
            AcpiGbl_CmSingleStep = TRUE;
            AcpiGbl_StepToNextCall = FALSE;
            WalkState->MethodBreakpoint = 0;
        }
    }

    /*
     * Check if this is an opcode that we are interested in --
     * namely, opcodes that have arguments
     */
    if (Op->Opcode == AML_INT_NAMEDFIELD_OP)
    {
        return (AE_OK);
    }

    switch (OpType)
    {
    case OPTYPE_UNDEFINED:
    case OPTYPE_CONSTANT:           /* argument type only */
    case OPTYPE_LITERAL:            /* argument type only */
    case OPTYPE_DATA_TERM:          /* argument type only */
    case OPTYPE_LOCAL_VARIABLE:     /* argument type only */
    case OPTYPE_METHOD_ARGUMENT:    /* argument type only */
        return (AE_OK);
        break;

    case OPTYPE_NAMED_OBJECT:
        switch (Op->Opcode)
        {
        case AML_INT_NAMEPATH_OP:
            return (AE_OK);
            break;
        }
    }

    /*
     * Under certain debug conditions, display this opcode and its operands
     */
    if ((AcpiGbl_DbOutputToFile)            ||
        (AcpiGbl_CmSingleStep)              ||
        (AcpiDbgLevel & ACPI_LV_PARSE))
    {
        if ((AcpiGbl_DbOutputToFile)        ||
            (AcpiDbgLevel & ACPI_LV_PARSE))
        {
            AcpiOsPrintf ("\n[AmlDebug] Next AML Opcode to execute:\n");
        }

        /*
         * Display this op (and only this op - zero out the NEXT field temporarily,
         * and disable parser trace output for the duration of the display because
         * we don't want the extraneous debug output)
         */
        OriginalDebugLevel = AcpiDbgLevel;
        AcpiDbgLevel &= ~(ACPI_LV_PARSE | ACPI_LV_FUNCTIONS);
        Next = Op->Next;
        Op->Next = NULL;


        DisplayOp = Op;
        if (Op->Parent)
        {
            if ((Op->Parent->Opcode == AML_IF_OP) ||
                (Op->Parent->Opcode == AML_WHILE_OP))
            {
                DisplayOp = Op->Parent;
            }
        }

        /* Now we can display it */

        AcpiDbDisplayOp (WalkState, DisplayOp, ACPI_UINT32_MAX);

        if ((Op->Opcode == AML_IF_OP) ||
            (Op->Opcode == AML_WHILE_OP))
        {
            if (WalkState->ControlState->Common.Value)
            {
                AcpiOsPrintf ("Predicate was TRUE, executed block\n");
            }
            else
            {
                AcpiOsPrintf ("Predicate is FALSE, skipping block\n");
            }
        }

        else if (Op->Opcode == AML_ELSE_OP)
        {
            /* TBD */
        }

        /* Restore everything */

        Op->Next = Next;
        AcpiOsPrintf ("\n");
        AcpiDbgLevel = OriginalDebugLevel;
    }

    /* If we are not single stepping, just continue executing the method */

    if (!AcpiGbl_CmSingleStep)
    {
        return (AE_OK);
    }


    /*
     * If we are executing a step-to-call command,
     * Check if this is a method call.
     */
    if (AcpiGbl_StepToNextCall)
    {
        if (Op->Opcode != AML_INT_METHODCALL_OP)
        {
            /* Not a method call, just keep executing */

            return (AE_OK);
        }

        /* Found a method call, stop executing */

        AcpiGbl_StepToNextCall = FALSE;
    }


    /*
     * If the next opcode is a method call, we will "step over" it
     * by default.
     */
    if (Op->Opcode == AML_INT_METHODCALL_OP)
    {
        AcpiGbl_CmSingleStep = FALSE;  /* No more single step while executing called method */

        /* Set the breakpoint on the call, it will stop execution as soon as we return */

        /* TBD: [Future] don't kill the user breakpoint! */

        WalkState->MethodBreakpoint = /* Op->AmlOffset + */ 1;  /* Must be non-zero! */
    }


    /* TBD: [Investigate] what are the namespace locking issues here */

    /* AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE); */

    /* Go into the command loop and await next user command */

    AcpiGbl_MethodExecuting = TRUE;
    Status = AE_CTRL_TRUE;
    while (Status == AE_CTRL_TRUE)
    {
        if (AcpiGbl_DebuggerConfiguration == DEBUGGER_MULTI_THREADED)
        {
            /* Handshake with the front-end that gets user command lines */

            AcpiUtReleaseMutex (ACPI_MTX_DEBUG_CMD_COMPLETE);
            AcpiUtAcquireMutex (ACPI_MTX_DEBUG_CMD_READY);
        }

        else
        {
            /* Single threaded, we must get a command line ourselves */

            /* Force output to console until a command is entered */

            AcpiDbSetOutputDestination (DB_CONSOLE_OUTPUT);

            /* Different prompt if method is executing */

            if (!AcpiGbl_MethodExecuting)
            {
                AcpiOsPrintf ("%1c ", DB_COMMAND_PROMPT);
            }
            else
            {
                AcpiOsPrintf ("%1c ", DB_EXECUTE_PROMPT);
            }

            /* Get the user input line */

            AcpiOsGetLine (AcpiGbl_DbLineBuf);
        }

        Status = AcpiDbCommandDispatch (AcpiGbl_DbLineBuf, WalkState, Op);
    }

    /* AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE); */

    /* User commands complete, continue execution of the interrupted method */

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Init and start debugger
 *
 ******************************************************************************/

int
AcpiDbInitialize (void)
{


    /* Init globals */

    AcpiGbl_DbBuffer = AcpiOsAllocate (ACPI_DEBUG_BUFFER_SIZE);

    /* Initial scope is the root */

    AcpiGbl_DbScopeBuf [0] = '\\';
    AcpiGbl_DbScopeBuf [1] =  0;


    /*
     * If configured for multi-thread support, the debug executor runs in
     * a separate thread so that the front end can be in another address
     * space, environment, or even another machine.
     */
    if (AcpiGbl_DebuggerConfiguration & DEBUGGER_MULTI_THREADED)
    {
        /* These were created with one unit, grab it */

        AcpiUtAcquireMutex (ACPI_MTX_DEBUG_CMD_COMPLETE);
        AcpiUtAcquireMutex (ACPI_MTX_DEBUG_CMD_READY);

        /* Create the debug execution thread to execute commands */

        AcpiOsQueueForExecution (0, AcpiDbExecuteThread, NULL);
    }

    if (!AcpiGbl_DbOpt_verbose)
    {
        AcpiGbl_DbDisasmIndent = "    ";
        AcpiGbl_DbOpt_disasm = TRUE;
        AcpiGbl_DbOpt_stats = FALSE;
    }

    return (0);
}


#endif /* ENABLE_DEBUGGER */
