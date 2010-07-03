/******************************************************************************
 *
 * Module Name: evgpeinit - System GPE initialization and update
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


#include "acpi.h"
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evgpeinit")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the GPE data structures and the FADT GPE 0/1 blocks
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvGpeInitialize (
    void)
{
    UINT32                  RegisterCount0 = 0;
    UINT32                  RegisterCount1 = 0;
    UINT32                  GpeNumberMax = 0;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (EvGpeInitialize);


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Initialize the GPE Block(s) defined in the FADT
     *
     * Why the GPE register block lengths are divided by 2:  From the ACPI
     * Spec, section "General-Purpose Event Registers", we have:
     *
     * "Each register block contains two registers of equal length
     *  GPEx_STS and GPEx_EN (where x is 0 or 1). The length of the
     *  GPE0_STS and GPE0_EN registers is equal to half the GPE0_LEN
     *  The length of the GPE1_STS and GPE1_EN registers is equal to
     *  half the GPE1_LEN. If a generic register block is not supported
     *  then its respective block pointer and block length values in the
     *  FADT table contain zeros. The GPE0_LEN and GPE1_LEN do not need
     *  to be the same size."
     */

    /*
     * Determine the maximum GPE number for this machine.
     *
     * Note: both GPE0 and GPE1 are optional, and either can exist without
     * the other.
     *
     * If EITHER the register length OR the block address are zero, then that
     * particular block is not supported.
     */
    if (AcpiGbl_FADT.Gpe0BlockLength &&
        AcpiGbl_FADT.XGpe0Block.Address)
    {
        /* GPE block 0 exists (has both length and address > 0) */

        RegisterCount0 = (UINT16) (AcpiGbl_FADT.Gpe0BlockLength / 2);

        GpeNumberMax = (RegisterCount0 * ACPI_GPE_REGISTER_WIDTH) - 1;

        /* Install GPE Block 0 */

        Status = AcpiEvCreateGpeBlock (AcpiGbl_FadtGpeDevice,
                    &AcpiGbl_FADT.XGpe0Block, RegisterCount0, 0,
                    AcpiGbl_FADT.SciInterrupt, &AcpiGbl_GpeFadtBlocks[0]);

        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "Could not create GPE Block 0"));
        }
    }

    if (AcpiGbl_FADT.Gpe1BlockLength &&
        AcpiGbl_FADT.XGpe1Block.Address)
    {
        /* GPE block 1 exists (has both length and address > 0) */

        RegisterCount1 = (UINT16) (AcpiGbl_FADT.Gpe1BlockLength / 2);

        /* Check for GPE0/GPE1 overlap (if both banks exist) */

        if ((RegisterCount0) &&
            (GpeNumberMax >= AcpiGbl_FADT.Gpe1Base))
        {
            ACPI_ERROR ((AE_INFO,
                "GPE0 block (GPE 0 to %u) overlaps the GPE1 block "
                "(GPE %u to %u) - Ignoring GPE1",
                GpeNumberMax, AcpiGbl_FADT.Gpe1Base,
                AcpiGbl_FADT.Gpe1Base +
                ((RegisterCount1 * ACPI_GPE_REGISTER_WIDTH) - 1)));

            /* Ignore GPE1 block by setting the register count to zero */

            RegisterCount1 = 0;
        }
        else
        {
            /* Install GPE Block 1 */

            Status = AcpiEvCreateGpeBlock (AcpiGbl_FadtGpeDevice,
                        &AcpiGbl_FADT.XGpe1Block, RegisterCount1,
                        AcpiGbl_FADT.Gpe1Base,
                        AcpiGbl_FADT.SciInterrupt, &AcpiGbl_GpeFadtBlocks[1]);

            if (ACPI_FAILURE (Status))
            {
                ACPI_EXCEPTION ((AE_INFO, Status,
                    "Could not create GPE Block 1"));
            }

            /*
             * GPE0 and GPE1 do not have to be contiguous in the GPE number
             * space. However, GPE0 always starts at GPE number zero.
             */
            GpeNumberMax = AcpiGbl_FADT.Gpe1Base +
                            ((RegisterCount1 * ACPI_GPE_REGISTER_WIDTH) - 1);
        }
    }

    /* Exit if there are no GPE registers */

    if ((RegisterCount0 + RegisterCount1) == 0)
    {
        /* GPEs are not required by ACPI, this is OK */

        ACPI_DEBUG_PRINT ((ACPI_DB_INIT,
            "There are no GPE blocks defined in the FADT\n"));
        Status = AE_OK;
        goto Cleanup;
    }

    /* Check for Max GPE number out-of-range */

    if (GpeNumberMax > ACPI_GPE_MAX)
    {
        ACPI_ERROR ((AE_INFO,
            "Maximum GPE number from FADT is too large: 0x%X",
            GpeNumberMax));
        Status = AE_BAD_VALUE;
        goto Cleanup;
    }

Cleanup:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvUpdateGpes
 *
 * PARAMETERS:  TableOwnerId        - ID of the newly-loaded ACPI table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check for new GPE methods (_Lxx/_Exx) made available as a
 *              result of a Load() or LoadTable() operation. If new GPE
 *              methods have been installed, register the new methods and
 *              enable and runtime GPEs that are associated with them. Also,
 *              run any newly loaded _PRW methods in order to discover any
 *              new CAN_WAKE GPEs.
 *
 ******************************************************************************/

void
AcpiEvUpdateGpes (
    ACPI_OWNER_ID           TableOwnerId)
{
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo;
    ACPI_GPE_BLOCK_INFO     *GpeBlock;
    ACPI_GPE_WALK_INFO      WalkInfo;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  NewWakeGpeCount = 0;


    /* We will examine only _PRW/_Lxx/_Exx methods owned by this table */

    WalkInfo.OwnerId = TableOwnerId;
    WalkInfo.ExecuteByOwnerId = TRUE;
    WalkInfo.Count = 0;

    if (AcpiGbl_LeaveWakeGpesDisabled)
    {
        /*
         * 1) Run any newly-loaded _PRW methods to find any GPEs that
         * can now be marked as CAN_WAKE GPEs. Note: We must run the
         * _PRW methods before we process the _Lxx/_Exx methods because
         * we will enable all runtime GPEs associated with the new
         * _Lxx/_Exx methods at the time we process those methods.
         *
         * Unlock interpreter so that we can run the _PRW methods.
         */
        WalkInfo.GpeBlock = NULL;
        WalkInfo.GpeDevice = NULL;

        AcpiExExitInterpreter ();

        Status = AcpiNsWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
                    ACPI_UINT32_MAX, ACPI_NS_WALK_NO_UNLOCK,
                    AcpiEvMatchPrwAndGpe, NULL, &WalkInfo, NULL);
        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "While executing _PRW methods"));
        }

        AcpiExEnterInterpreter ();
        NewWakeGpeCount = WalkInfo.Count;
    }

    /*
     * 2) Find any _Lxx/_Exx GPE methods that have just been loaded.
     *
     * Any GPEs that correspond to new _Lxx/_Exx methods and are not
     * marked as CAN_WAKE are immediately enabled.
     *
     * Examine the namespace underneath each GpeDevice within the
     * GpeBlock lists.
     */
    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    WalkInfo.Count = 0;
    WalkInfo.EnableThisGpe = TRUE;

    /* Walk the interrupt level descriptor list */

    GpeXruptInfo = AcpiGbl_GpeXruptListHead;
    while (GpeXruptInfo)
    {
        /* Walk all Gpe Blocks attached to this interrupt level */

        GpeBlock = GpeXruptInfo->GpeBlockListHead;
        while (GpeBlock)
        {
            WalkInfo.GpeBlock = GpeBlock;
            WalkInfo.GpeDevice = GpeBlock->Node;

            Status = AcpiNsWalkNamespace (ACPI_TYPE_METHOD,
                        WalkInfo.GpeDevice, ACPI_UINT32_MAX,
                        ACPI_NS_WALK_NO_UNLOCK, AcpiEvMatchGpeMethod,
                        NULL, &WalkInfo, NULL);
            if (ACPI_FAILURE (Status))
            {
                ACPI_EXCEPTION ((AE_INFO, Status,
                    "While decoding _Lxx/_Exx methods"));
            }

            GpeBlock = GpeBlock->Next;
        }

        GpeXruptInfo = GpeXruptInfo->Next;
    }

    if (WalkInfo.Count || NewWakeGpeCount)
    {
        ACPI_INFO ((AE_INFO,
            "Enabled %u new runtime GPEs, added %u new wakeup GPEs",
            WalkInfo.Count, NewWakeGpeCount));
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvMatchGpeMethod
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Called from AcpiWalkNamespace. Expects each object to be a
 *              control method under the _GPE portion of the namespace.
 *              Extract the name and GPE type from the object, saving this
 *              information for quick lookup during GPE dispatch. Allows a
 *              per-OwnerId evaluation if ExecuteByOwnerId is TRUE in the
 *              WalkInfo parameter block.
 *
 *              The name of each GPE control method is of the form:
 *              "_Lxx" or "_Exx", where:
 *                  L      - means that the GPE is level triggered
 *                  E      - means that the GPE is edge triggered
 *                  xx     - is the GPE number [in HEX]
 *
 * If WalkInfo->ExecuteByOwnerId is TRUE, we only execute examine GPE methods
 *    with that owner.
 * If WalkInfo->EnableThisGpe is TRUE, the GPE that is referred to by a GPE
 *    method is immediately enabled (Used for Load/LoadTable operators)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvMatchGpeMethod (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *MethodNode = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle);
    ACPI_GPE_WALK_INFO      *WalkInfo = ACPI_CAST_PTR (ACPI_GPE_WALK_INFO, Context);
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_NAMESPACE_NODE     *GpeDevice;
    ACPI_STATUS             Status;
    UINT32                  GpeNumber;
    char                    Name[ACPI_NAME_SIZE + 1];
    UINT8                   Type;


    ACPI_FUNCTION_TRACE (EvMatchGpeMethod);


    /* Check if requested OwnerId matches this OwnerId */

    if ((WalkInfo->ExecuteByOwnerId) &&
        (MethodNode->OwnerId != WalkInfo->OwnerId))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Match and decode the _Lxx and _Exx GPE method names
     *
     * 1) Extract the method name and null terminate it
     */
    ACPI_MOVE_32_TO_32 (Name, &MethodNode->Name.Integer);
    Name[ACPI_NAME_SIZE] = 0;

    /* 2) Name must begin with an underscore */

    if (Name[0] != '_')
    {
        return_ACPI_STATUS (AE_OK); /* Ignore this method */
    }

    /*
     * 3) Edge/Level determination is based on the 2nd character
     *    of the method name
     *
     * NOTE: Default GPE type is RUNTIME only. Later, if a _PRW object is
     * found that points to this GPE, the ACPI_GPE_CAN_WAKE flag is set.
     */
    switch (Name[1])
    {
    case 'L':
        Type = ACPI_GPE_LEVEL_TRIGGERED;
        break;

    case 'E':
        Type = ACPI_GPE_EDGE_TRIGGERED;
        break;

    default:
        /* Unknown method type, just ignore it */

        ACPI_DEBUG_PRINT ((ACPI_DB_LOAD,
            "Ignoring unknown GPE method type: %s "
            "(name not of form _Lxx or _Exx)", Name));
        return_ACPI_STATUS (AE_OK);
    }

    /* 4) The last two characters of the name are the hex GPE Number */

    GpeNumber = ACPI_STRTOUL (&Name[2], NULL, 16);
    if (GpeNumber == ACPI_UINT32_MAX)
    {
        /* Conversion failed; invalid method, just ignore it */

        ACPI_DEBUG_PRINT ((ACPI_DB_LOAD,
            "Could not extract GPE number from name: %s "
            "(name is not of form _Lxx or _Exx)", Name));
        return_ACPI_STATUS (AE_OK);
    }

    /* Ensure that we have a valid GPE number for this GPE block */

    GpeEventInfo = AcpiEvLowGetGpeInfo (GpeNumber, WalkInfo->GpeBlock);
    if (!GpeEventInfo)
    {
        /*
         * This GpeNumber is not valid for this GPE block, just ignore it.
         * However, it may be valid for a different GPE block, since GPE0
         * and GPE1 methods both appear under \_GPE.
         */
        return_ACPI_STATUS (AE_OK);
    }

    if ((GpeEventInfo->Flags & ACPI_GPE_DISPATCH_MASK) ==
            ACPI_GPE_DISPATCH_HANDLER)
    {
        /* If there is already a handler, ignore this GPE method */

        return_ACPI_STATUS (AE_OK);
    }

    if ((GpeEventInfo->Flags & ACPI_GPE_DISPATCH_MASK) ==
            ACPI_GPE_DISPATCH_METHOD)
    {
        /*
         * If there is already a method, ignore this method. But check
         * for a type mismatch (if both the _Lxx AND _Exx exist)
         */
        if (Type != (GpeEventInfo->Flags & ACPI_GPE_XRUPT_TYPE_MASK))
        {
            ACPI_ERROR ((AE_INFO,
                "For GPE 0x%.2X, found both _L%2.2X and _E%2.2X methods",
                GpeNumber, GpeNumber, GpeNumber));
        }
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Add the GPE information from above to the GpeEventInfo block for
     * use during dispatch of this GPE.
     */
    GpeEventInfo->Flags |= (UINT8) (Type | ACPI_GPE_DISPATCH_METHOD);
    GpeEventInfo->Dispatch.MethodNode = MethodNode;

    /*
     * Enable this GPE if requested. This only happens when during the
     * execution of a Load or LoadTable operator. We have found a new
     * GPE method and want to immediately enable the GPE if it is a
     * runtime GPE.
     */
    if (WalkInfo->EnableThisGpe)
    {
        /* Ignore GPEs that can wake the system */

        if (!(GpeEventInfo->Flags & ACPI_GPE_CAN_WAKE) ||
            !AcpiGbl_LeaveWakeGpesDisabled)
        {
            WalkInfo->Count++;
            GpeDevice = WalkInfo->GpeDevice;

            if (GpeDevice == AcpiGbl_FadtGpeDevice)
            {
                GpeDevice = NULL;
            }

            Status = AcpiEnableGpe (GpeDevice, GpeNumber,
                        ACPI_GPE_TYPE_RUNTIME);
            if (ACPI_FAILURE (Status))
            {
                ACPI_EXCEPTION ((AE_INFO, Status,
                    "Could not enable GPE 0x%02X", GpeNumber));
            }
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_LOAD,
        "Registered GPE method %s as GPE number 0x%.2X\n",
        Name, GpeNumber));
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvMatchPrwAndGpe
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status. NOTE: We ignore errors so that the _PRW walk is
 *              not aborted on a single _PRW failure.
 *
 * DESCRIPTION: Called from AcpiWalkNamespace. Expects each object to be a
 *              Device. Run the _PRW method. If present, extract the GPE
 *              number and mark the GPE as a CAN_WAKE GPE. Allows a
 *              per-OwnerId execution if ExecuteByOwnerId is TRUE in the
 *              WalkInfo parameter block.
 *
 * If WalkInfo->ExecuteByOwnerId is TRUE, we only execute _PRWs with that
 *    owner.
 * If WalkInfo->GpeDevice is NULL, we execute every _PRW found. Otherwise,
 *    we only execute _PRWs that refer to the input GpeDevice.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvMatchPrwAndGpe (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_GPE_WALK_INFO      *WalkInfo = ACPI_CAST_PTR (ACPI_GPE_WALK_INFO, Context);
    ACPI_NAMESPACE_NODE     *GpeDevice;
    ACPI_GPE_BLOCK_INFO     *GpeBlock;
    ACPI_NAMESPACE_NODE     *TargetGpeDevice;
    ACPI_NAMESPACE_NODE     *PrwNode;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_OPERAND_OBJECT     *PkgDesc;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  GpeNumber;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (EvMatchPrwAndGpe);


    /* Check for a _PRW method under this device */

    Status = AcpiNsGetNode (ObjHandle, METHOD_NAME__PRW,
                ACPI_NS_NO_UPSEARCH, &PrwNode);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Check if requested OwnerId matches this OwnerId */

    if ((WalkInfo->ExecuteByOwnerId) &&
        (PrwNode->OwnerId != WalkInfo->OwnerId))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Execute the _PRW */

    Status = AcpiUtEvaluateObject (PrwNode, NULL,
                ACPI_BTYPE_PACKAGE, &PkgDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* The returned _PRW package must have at least two elements */

    if (PkgDesc->Package.Count < 2)
    {
        goto Cleanup;
    }

    /* Extract pointers from the input context */

    GpeDevice = WalkInfo->GpeDevice;
    GpeBlock = WalkInfo->GpeBlock;

    /*
     * The _PRW object must return a package, we are only interested
     * in the first element
     */
    ObjDesc = PkgDesc->Package.Elements[0];

    if (ObjDesc->Common.Type == ACPI_TYPE_INTEGER)
    {
        /* Use FADT-defined GPE device (from definition of _PRW) */

        TargetGpeDevice = NULL;
        if (GpeDevice)
        {
            TargetGpeDevice = AcpiGbl_FadtGpeDevice;
        }

        /* Integer is the GPE number in the FADT described GPE blocks */

        GpeNumber = (UINT32) ObjDesc->Integer.Value;
    }
    else if (ObjDesc->Common.Type == ACPI_TYPE_PACKAGE)
    {
        /* Package contains a GPE reference and GPE number within a GPE block */

        if ((ObjDesc->Package.Count < 2) ||
            ((ObjDesc->Package.Elements[0])->Common.Type !=
                ACPI_TYPE_LOCAL_REFERENCE) ||
            ((ObjDesc->Package.Elements[1])->Common.Type !=
                ACPI_TYPE_INTEGER))
        {
            goto Cleanup;
        }

        /* Get GPE block reference and decode */

        TargetGpeDevice = ObjDesc->Package.Elements[0]->Reference.Node;
        GpeNumber = (UINT32) ObjDesc->Package.Elements[1]->Integer.Value;
    }
    else
    {
        /* Unknown type, just ignore it */

        goto Cleanup;
    }

    /* Get the GpeEventInfo for this GPE */

    if (GpeDevice)
    {
        /*
         * Is this GPE within this block?
         *
         * TRUE if and only if these conditions are true:
         *     1) The GPE devices match.
         *     2) The GPE index(number) is within the range of the Gpe Block
         *          associated with the GPE device.
         */
        if (GpeDevice != TargetGpeDevice)
        {
            goto Cleanup;
        }

        GpeEventInfo = AcpiEvLowGetGpeInfo (GpeNumber, GpeBlock);
    }
    else
    {
        /* GpeDevice is NULL, just match the TargetDevice and GpeNumber */

        GpeEventInfo = AcpiEvGetGpeEventInfo (TargetGpeDevice, GpeNumber);
    }

    if (GpeEventInfo)
    {
        if (!(GpeEventInfo->Flags & ACPI_GPE_CAN_WAKE))
        {
            /* This GPE can wake the system */

            GpeEventInfo->Flags |= ACPI_GPE_CAN_WAKE;
            WalkInfo->Count++;
        }
    }

Cleanup:
    AcpiUtRemoveReference (PkgDesc);
    return_ACPI_STATUS (AE_OK);
}

