/*******************************************************************************
 *
 * Module Name: nsaccess - Top-level functions for accessing ACPI namespace
 *              $Revision: 1.1.1.1.4.4 $
 *
 ******************************************************************************/

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
__KERNEL_RCSID(0, "$NetBSD: nsaccess.c,v 1.1.1.1.4.4 2002/06/20 03:44:01 nathanw Exp $");

#define __NSACCESS_C__

#include "acpi.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsaccess")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRootInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Allocate and initialize the default root named objects
 *
 * MUTEX:       Locks namespace for entire execution
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsRootInitialize (void)
{
    ACPI_STATUS                 Status;
    const ACPI_PREDEFINED_NAMES *InitVal = NULL;
    ACPI_NAMESPACE_NODE         *NewNode;
    ACPI_OPERAND_OBJECT         *ObjDesc;


    ACPI_FUNCTION_TRACE ("NsRootInitialize");


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * The global root ptr is initially NULL, so a non-NULL value indicates
     * that AcpiNsRootInitialize() has already been called; just return.
     */
    if (AcpiGbl_RootNode)
    {
        Status = AE_OK;
        goto UnlockAndExit;
    }

    /*
     * Tell the rest of the subsystem that the root is initialized
     * (This is OK because the namespace is locked)
     */
    AcpiGbl_RootNode = &AcpiGbl_RootNodeStruct;

    /* Enter the pre-defined names in the name table */

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "Entering predefined entries into namespace\n"));

    for (InitVal = AcpiGbl_PreDefinedNames; InitVal->Name; InitVal++)
    {
        Status = AcpiNsLookup (NULL, InitVal->Name, InitVal->Type,
                        ACPI_IMODE_LOAD_PASS2, ACPI_NS_NO_UPSEARCH, NULL, &NewNode);

        if (ACPI_FAILURE (Status) || (!NewNode)) /* Must be on same line for code converter */
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Could not create predefined name %s, %s\n",
                InitVal->Name, AcpiFormatException (Status)));
        }

        /*
         * Name entered successfully.
         * If entry in PreDefinedNames[] specifies an
         * initial value, create the initial value.
         */
        if (InitVal->Val)
        {
            /*
             * Entry requests an initial value, allocate a
             * descriptor for it.
             */
            ObjDesc = AcpiUtCreateInternalObject (InitVal->Type);
            if (!ObjDesc)
            {
                Status = AE_NO_MEMORY;
                goto UnlockAndExit;
            }

            /*
             * Convert value string from table entry to
             * internal representation. Only types actually
             * used for initial values are implemented here.
             */
            switch (InitVal->Type)
            {
            case ACPI_TYPE_INTEGER:

                ObjDesc->Integer.Value =
                        (ACPI_INTEGER) ACPI_STRTOUL (InitVal->Val, NULL, 10);
                break;


            case ACPI_TYPE_STRING:

                /*
                 * Build an object around the static string
                 */
                ObjDesc->String.Length = ACPI_STRLEN (InitVal->Val);
                ObjDesc->String.Pointer = InitVal->Val;
                ObjDesc->Common.Flags |= AOPOBJ_STATIC_POINTER;
                break;


            case ACPI_TYPE_MUTEX:

                ObjDesc->Mutex.SyncLevel =
                            (UINT16) ACPI_STRTOUL (InitVal->Val, NULL, 10);

                if (ACPI_STRCMP (InitVal->Name, "_GL_") == 0)
                {
                    /*
                     * Create a counting semaphore for the
                     * global lock
                     */
                    Status = AcpiOsCreateSemaphore (ACPI_NO_UNIT_LIMIT,
                                            1, &ObjDesc->Mutex.Semaphore);
                    if (ACPI_FAILURE (Status))
                    {
                        goto UnlockAndExit;
                    }

                    /*
                     * We just created the mutex for the
                     * global lock, save it
                     */
                    AcpiGbl_GlobalLockSemaphore = ObjDesc->Mutex.Semaphore;
                }
                else
                {
                    /* Create a mutex */

                    Status = AcpiOsCreateSemaphore (1, 1,
                                        &ObjDesc->Mutex.Semaphore);
                    if (ACPI_FAILURE (Status))
                    {
                        goto UnlockAndExit;
                    }
                }
                break;


            default:
                ACPI_REPORT_ERROR (("Unsupported initial type value %X\n",
                    InitVal->Type));
                AcpiUtRemoveReference (ObjDesc);
                ObjDesc = NULL;
                continue;
            }

            /* Store pointer to value descriptor in the Node */

            Status = AcpiNsAttachObject (NewNode, ObjDesc, ACPI_GET_OBJECT_TYPE (ObjDesc));

            /* Remove local reference to the object */

            AcpiUtRemoveReference (ObjDesc);
        }
    }


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsLookup
 *
 * PARAMETERS:  PrefixNode      - Search scope if name is not fully qualified
 *              Pathname        - Search pathname, in internal format
 *                                (as represented in the AML stream)
 *              Type            - Type associated with name
 *              InterpreterMode - IMODE_LOAD_PASS2 => add name if not found
 *              Flags           - Flags describing the search restrictions
 *              WalkState       - Current state of the walk
 *              ReturnNode      - Where the Node is placed (if found
 *                                or created successfully)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find or enter the passed name in the name space.
 *              Log an error if name not found in Exec mode.
 *
 * MUTEX:       Assumes namespace is locked.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsLookup (
    ACPI_GENERIC_STATE      *ScopeInfo,
    NATIVE_CHAR             *Pathname,
    ACPI_OBJECT_TYPE        Type,
    ACPI_INTERPRETER_MODE   InterpreterMode,
    UINT32                  Flags,
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     **ReturnNode)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *PrefixNode;
    ACPI_NAMESPACE_NODE     *CurrentNode = NULL;
    ACPI_NAMESPACE_NODE     *ThisNode = NULL;
    UINT32                  NumSegments;
    ACPI_NAME               SimpleName;
    ACPI_OBJECT_TYPE        TypeToCheckFor;
    ACPI_OBJECT_TYPE        ThisSearchType;
    UINT32                  LocalFlags = Flags & ~ACPI_NS_ERROR_IF_FOUND;


    ACPI_FUNCTION_TRACE ("NsLookup");


    if (!ReturnNode)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiGbl_NsLookupCount++;
    *ReturnNode = ACPI_ENTRY_NOT_FOUND;

    if (!AcpiGbl_RootNode)
    {
        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    /*
     * Get the prefix scope.
     * A null scope means use the root scope
     */
    if ((!ScopeInfo) ||
        (!ScopeInfo->Scope.Node))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
            "Null scope prefix, using root node (%p)\n",
            AcpiGbl_RootNode));

        PrefixNode = AcpiGbl_RootNode;
    }
    else
    {
        PrefixNode = ScopeInfo->Scope.Node;
    }

    /*
     * This check is explicitly split to relax the TypeToCheckFor
     * conditions for BankFieldDefn.  Originally, both BankFieldDefn and
     * DefFieldDefn caused TypeToCheckFor to be set to ACPI_TYPE_REGION,
     * but the BankFieldDefn may also check for a Field definition as well
     * as an OperationRegion.
     */
    if (INTERNAL_TYPE_FIELD_DEFN == Type)
    {
        /* DefFieldDefn defines fields in a Region */

        TypeToCheckFor = ACPI_TYPE_REGION;
    }
    else if (INTERNAL_TYPE_BANK_FIELD_DEFN == Type)
    {
        /* BankFieldDefn defines data fields in a Field Object */

        TypeToCheckFor = ACPI_TYPE_ANY;
    }
    else
    {
        TypeToCheckFor = Type;
    }

    /*
     * Begin examination of the actual pathname
     */
    if (!Pathname)
    {
        /* A Null NamePath is allowed and refers to the root */

        NumSegments  = 0;
        ThisNode     = AcpiGbl_RootNode;
        Pathname     = "";

        ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
            "Null Pathname (Zero segments), Flags=%X\n", Flags));
    }
    else
    {
        /*
         * Name pointer is valid (and must be in internal name format)
         *
         * Check for scope prefixes:
         *
         * As represented in the AML stream, a namepath consists of an
         * optional scope prefix followed by a name segment part.
         *
         * If present, the scope prefix is either a Root Prefix (in
         * which case the name is fully qualified), or one or more
         * Parent Prefixes (in which case the name's scope is relative
         * to the current scope).
         */
        if (*Pathname == (UINT8) AML_ROOT_PREFIX)
        {
            /* Pathname is fully qualified, start from the root */

            ThisNode = AcpiGbl_RootNode;

            /* Point to name segment part */

            Pathname++;

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Searching from root [%p]\n",
                ThisNode));
        }
        else
        {
            /* Pathname is relative to current scope, start there */

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Searching relative to pfx scope [%p]\n",
                PrefixNode));

            /*
             * Handle multiple Parent Prefixes (carat) by just getting
             * the parent node for each prefix instance.
             */
            ThisNode = PrefixNode;
            while (*Pathname == (UINT8) AML_PARENT_PREFIX)
            {
                /*
                 * Point past this prefix to the name segment
                 * part or the next Parent Prefix
                 */
                Pathname++;

                /* Backup to the parent node */

                ThisNode = AcpiNsGetParentNode (ThisNode);
                if (!ThisNode)
                {
                    /* Current scope has no parent scope */

                    ACPI_REPORT_ERROR (
                        ("ACPI path has too many parent prefixes (^) - reached beyond root node\n"));
                    return_ACPI_STATUS (AE_NOT_FOUND);
                }
            }
        }

        /*
         * Determine the number of ACPI name segments in this pathname.
         *
         * The segment part consists of either:
         *  - A Null name segment (0)
         *  - A DualNamePrefix followed by two 4-byte name segments
         *  - A MultiNamePrefix followed by a byte indicating the
         *      number of segments and the segments themselves.
         *  - A single 4-byte name segment
         *
         * Examine the name prefix opcode, if any, to determine the number of
         * segments.
         */
        switch (*Pathname)
        {
        case 0:
            /*
             * Null name after a root or parent prefixes. We already
             * have the correct target node and there are no name segments.
             */
            NumSegments  = 0;

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Prefix-only Pathname (Zero name segments), Flags=%X\n", Flags));
            break;

        case AML_DUAL_NAME_PREFIX:

            /* Two segments, point to first name segment */

            NumSegments = 2;
            Pathname++;

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Dual Pathname (2 segments, Flags=%X)\n", Flags));
            break;

        case AML_MULTI_NAME_PREFIX_OP:

            /* Extract segment count, point to first name segment */

            Pathname++;
            NumSegments = (UINT32) (UINT8) *Pathname;
            Pathname++;

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Multi Pathname (%d Segments, Flags=%X) \n",
                NumSegments, Flags));
            break;

        default:
            /*
             * Not a Null name, no Dual or Multi prefix, hence there is
             * only one name segment and Pathname is already pointing to it.
             */
            NumSegments = 1;

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Simple Pathname (1 segment, Flags=%X)\n", Flags));
            break;
        }

        ACPI_DEBUG_EXEC (AcpiNsPrintPathname (NumSegments, Pathname));
    }

    /*
     * Search namespace for each segment of the name.  Loop through and
     * verify/add each name segment.
     */
    CurrentNode = ThisNode;
    while (NumSegments && CurrentNode)
    {
        /*
         * Search for the current name segment under the current
         * named object.  The Type is significant only at the last name
         * segment.  (We don't care about the types along the path, only
         * the type of the final target object.)
         */
        ThisSearchType = ACPI_TYPE_ANY;
        NumSegments--;
        if (!NumSegments)
        {
            ThisSearchType = Type;
            LocalFlags = Flags;
        }

        /* Extract one ACPI name from the front of the pathname */

        ACPI_MOVE_UNALIGNED32_TO_32 (&SimpleName, Pathname);

        /* Try to find the ACPI name */

        Status = AcpiNsSearchAndEnter (SimpleName, WalkState, CurrentNode,
                        InterpreterMode, ThisSearchType, LocalFlags, &ThisNode);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_NOT_FOUND)
            {
                /* Name not found in ACPI namespace */

                ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                    "Name [%4.4s] not found in scope [%4.4s] %p\n",
                    (char *) &SimpleName, (char *) &CurrentNode->Name, CurrentNode));
            }

            return_ACPI_STATUS (Status);
        }

        /*
         * Sanity typecheck of the target object:
         *
         * If 1) This is the last segment (NumSegments == 0)
         *    2) And we are looking for a specific type
         *       (Not checking for TYPE_ANY)
         *    3) Which is not an alias
         *    4) Which is not a local type (TYPE_DEF_ANY)
         *    5) Which is not a local type (TYPE_SCOPE)
         *    6) Which is not a local type (TYPE_INDEX_FIELD_DEFN)
         *    7) And the type of target object is known (not TYPE_ANY)
         *    8) And target object does not match what we are looking for
         *
         * Then we have a type mismatch.  Just warn and ignore it.
         */
        if ((NumSegments        == 0)                               &&
            (TypeToCheckFor     != ACPI_TYPE_ANY)                   &&
            (TypeToCheckFor     != INTERNAL_TYPE_ALIAS)             &&
            (TypeToCheckFor     != INTERNAL_TYPE_DEF_ANY)           &&
            (TypeToCheckFor     != INTERNAL_TYPE_SCOPE)             &&
            (TypeToCheckFor     != INTERNAL_TYPE_INDEX_FIELD_DEFN)  &&
            (ThisNode->Type     != ACPI_TYPE_ANY)                   &&
            (ThisNode->Type     != TypeToCheckFor))
        {
            /* Complain about a type mismatch */

            ACPI_REPORT_WARNING (
                ("NsLookup: %4.4s, type %X, checking for type %X\n",
                (char *) &SimpleName, ThisNode->Type, TypeToCheckFor));
        }

        /*
         * If this is the last name segment and we are not looking for a
         * specific type, but the type of found object is known, use that type
         * to see if it opens a scope.
         */
        if ((NumSegments == 0) && (Type == ACPI_TYPE_ANY))
        {
            Type = ThisNode->Type;
        }

        /* Point to next name segment and make this node current */

        Pathname += ACPI_NAME_SIZE;
        CurrentNode = ThisNode;
    }

    /*
     * Always check if we need to open a new scope
     */
    if (!(Flags & ACPI_NS_DONT_OPEN_SCOPE) && (WalkState))
    {
        /*
         * If entry is a type which opens a scope, push the new scope on the
         * scope stack.
         */
        if (AcpiNsOpensScope (TypeToCheckFor))
        {
            Status = AcpiDsScopeStackPush (ThisNode, Type, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Setting current scope to [%4.4s] (%p)\n", 
                ThisNode->Name.Ascii, ThisNode));
        }
    }

    *ReturnNode = ThisNode;
    return_ACPI_STATUS (AE_OK);
}

