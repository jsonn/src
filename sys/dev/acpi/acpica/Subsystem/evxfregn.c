/******************************************************************************
 *
 * Module Name: evxfregn - External Interfaces, ACPI Operation Regions and
 *                         Address Spaces.
 *              xRevision: 61 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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
 * Redistribution of source code of any substantial prton of the Covered
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
__KERNEL_RCSID(0, "$NetBSD: evxfregn.c,v 1.6.2.2 2004/09/18 14:44:44 skrll Exp $");

#define __EVXFREGN_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acevents.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evxfregn")


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallAddressSpaceHandler
 *
 * PARAMETERS:  Device          - Handle for the device
 *              SpaceId         - The address space ID
 *              Handler         - Address of the handler
 *              Setup           - Address of the setup function
 *              Context         - Value passed to the handler on each access
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for all OpRegions of a given SpaceId.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_OBJECT_TYPE        Type;
    UINT16                  Flags = 0;


    ACPI_FUNCTION_TRACE ("AcpiInstallAddressSpaceHandler");


    /* Parameter validation */

    if (!Device)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsMapHandleToNode (Device);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /*
     * This registration is valid for only the types below
     * and the root.  This is where the default handlers
     * get placed.
     */
    if ((Node->Type != ACPI_TYPE_DEVICE)     &&
        (Node->Type != ACPI_TYPE_PROCESSOR)  &&
        (Node->Type != ACPI_TYPE_THERMAL)    &&
        (Node != AcpiGbl_RootNode))
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    if (Handler == ACPI_DEFAULT_HANDLER)
    {
        Flags = ACPI_ADDR_HANDLER_DEFAULT_INSTALLED;

        switch (SpaceId)
        {
        case ACPI_ADR_SPACE_SYSTEM_MEMORY:
            Handler = AcpiExSystemMemorySpaceHandler;
            Setup   = AcpiEvSystemMemoryRegionSetup;
            break;

        case ACPI_ADR_SPACE_SYSTEM_IO:
            Handler = AcpiExSystemIoSpaceHandler;
            Setup   = AcpiEvIoSpaceRegionSetup;
            break;

        case ACPI_ADR_SPACE_PCI_CONFIG:
            Handler = AcpiExPciConfigSpaceHandler;
            Setup   = AcpiEvPciConfigRegionSetup;
            break;

        case ACPI_ADR_SPACE_CMOS:
            Handler = AcpiExCmosSpaceHandler;
            Setup   = AcpiEvCmosRegionSetup;
            break;

        case ACPI_ADR_SPACE_PCI_BAR_TARGET:
            Handler = AcpiExPciBarSpaceHandler;
            Setup   = AcpiEvPciBarRegionSetup;
            break;

        case ACPI_ADR_SPACE_DATA_TABLE:
            Handler = AcpiExDataTableSpaceHandler;
            Setup   = NULL;
            break;

        default:
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }
    }

    /* If the caller hasn't specified a setup routine, use the default */

    if (!Setup)
    {
        Setup = AcpiEvDefaultRegionSetup;
    }

    /* Check for an existing internal object */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (ObjDesc)
    {
        /*
         * The attached device object already exists.
         * Make sure the handler is not already installed.
         */
        HandlerObj = ObjDesc->Device.Handler;

        /* Walk the handler list for this device */

        while (HandlerObj)
        {
            /* Same SpaceId indicates a handler already installed */

            if(HandlerObj->AddressSpace.SpaceId == SpaceId)
            {
                if (HandlerObj->AddressSpace.Handler == Handler)
                {
                    /*
                     * It is (relatively) OK to attempt to install the SAME
                     * handler twice. This can easily happen with PCI_Config space.
                     */
                    Status = AE_SAME_HANDLER;
                    goto UnlockAndExit;
                }
                else
                {
                    /* A handler is already installed */

                    Status = AE_ALREADY_EXISTS;
                }
                goto UnlockAndExit;
            }

            /* Walk the linked list of handlers */

            HandlerObj = HandlerObj->AddressSpace.Next;
        }
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
            "Creating object on Device %p while installing handler\n", Node));

        /* ObjDesc does not exist, create one */

        if (Node->Type == ACPI_TYPE_ANY)
        {
            Type = ACPI_TYPE_DEVICE;
        }
        else
        {
            Type = Node->Type;
        }

        ObjDesc = AcpiUtCreateInternalObject (Type);
        if (!ObjDesc)
        {
            Status = AE_NO_MEMORY;
            goto UnlockAndExit;
        }

        /* Init new descriptor */

        ObjDesc->Common.Type = (UINT8) Type;

        /* Attach the new object to the Node */

        Status = AcpiNsAttachObject (Node, ObjDesc, Type);

        /* Remove local reference to the object */

        AcpiUtRemoveReference (ObjDesc);

        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "Installing address handler for region %s(%X) on Device %4.4s %p(%p)\n",
        AcpiUtGetRegionName (SpaceId), SpaceId,
        AcpiUtGetNodeName (Node), Node, ObjDesc));

    /*
     * Install the handler
     *
     * At this point there is no existing handler.
     * Just allocate the object for the handler and link it
     * into the list.
     */
    HandlerObj = AcpiUtCreateInternalObject (ACPI_TYPE_LOCAL_ADDRESS_HANDLER);
    if (!HandlerObj)
    {
        Status = AE_NO_MEMORY;
        goto UnlockAndExit;
    }

    /* Init handler obj */

    HandlerObj->AddressSpace.SpaceId     = (UINT8) SpaceId;
    HandlerObj->AddressSpace.Hflags      = Flags;
    HandlerObj->AddressSpace.RegionList  = NULL;
    HandlerObj->AddressSpace.Node        = Node;
    HandlerObj->AddressSpace.Handler     = Handler;
    HandlerObj->AddressSpace.Context     = Context;
    HandlerObj->AddressSpace.Setup       = Setup;

    /* Install at head of Device.AddressSpace list */

    HandlerObj->AddressSpace.Next        = ObjDesc->Device.Handler;

    /*
     * The Device object is the first reference on the HandlerObj.
     * Each region that uses the handler adds a reference.
     */
    ObjDesc->Device.Handler = HandlerObj;

    /*
     * Walk the namespace finding all of the regions this
     * handler will manage.
     *
     * Start at the device and search the branch toward
     * the leaf nodes until either the leaf is encountered or
     * a device is detected that has an address handler of the
     * same type.
     *
     * In either case, back up and search down the remainder
     * of the branch
     */
    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, Device, ACPI_UINT32_MAX,
                    ACPI_NS_WALK_UNLOCK, AcpiEvInstallHandler,
                    HandlerObj, NULL);

    /*
     * Now we can run the _REG methods for all Regions for this
     * space ID.  This is a separate walk in order to handle any
     * interdependencies between regions and _REG methods.  (i.e. handlers
     * must be installed for all regions of this Space ID before we
     * can run any _REG methods.
     */
    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, Device, ACPI_UINT32_MAX,
                    ACPI_NS_WALK_UNLOCK, AcpiEvRegRun,
                    HandlerObj, NULL);

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveAddressSpaceHandler
 *
 * PARAMETERS:  Device          - Handle for the device
 *              SpaceId         - The address space ID
 *              Handler         - Address of the handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a previously installed handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_OPERAND_OBJECT     *RegionObj;
    ACPI_OPERAND_OBJECT     **LastObjPtr;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiRemoveAddressSpaceHandler");


    /* Parameter validation */

    if (!Device)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsMapHandleToNode (Device);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Make sure the internal object exists */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        Status = AE_NOT_EXIST;
        goto UnlockAndExit;
    }

    /* Find the address handler the user requested */

    HandlerObj = ObjDesc->Device.Handler;
    LastObjPtr = &ObjDesc->Device.Handler;
    while (HandlerObj)
    {
        /* We have a handler, see if user requested this one */

        if (HandlerObj->AddressSpace.SpaceId == SpaceId)
        {
            /* Matched SpaceId, first dereference this in the Regions */

            ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
                "Removing address handler %p(%p) for region %s on Device %p(%p)\n",
                HandlerObj, Handler, AcpiUtGetRegionName (SpaceId),
                Node, ObjDesc));

            RegionObj = HandlerObj->AddressSpace.RegionList;

            /* Walk the handler's region list */

            while (RegionObj)
            {
                /*
                 * First disassociate the handler from the region.
                 *
                 * NOTE: this doesn't mean that the region goes away
                 * The region is just inaccessible as indicated to
                 * the _REG method
                 */
                AcpiEvDetachRegion (RegionObj, TRUE);

                /*
                 * Walk the list: Just grab the head because the
                 * DetachRegion removed the previous head.
                 */
                RegionObj = HandlerObj->AddressSpace.RegionList;

            }

            /* Remove this Handler object from the list */

            *LastObjPtr = HandlerObj->AddressSpace.Next;

            /* Now we can delete the handler object */

            AcpiUtRemoveReference (HandlerObj);
            goto UnlockAndExit;
        }

        /* Walk the linked list of handlers */

        LastObjPtr = &HandlerObj->AddressSpace.Next;
        HandlerObj = HandlerObj->AddressSpace.Next;
    }

    /* The handler does not exist */

    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "Unable to remove address handler %p for %s(%X), DevNode %p, obj %p\n",
        Handler, AcpiUtGetRegionName (SpaceId), SpaceId, Node, ObjDesc));

    Status = AE_NOT_EXIST;

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}


