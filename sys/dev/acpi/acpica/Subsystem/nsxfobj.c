/*******************************************************************************
 *
 * Module Name: nsxfobj - Public interfaces to the ACPI subsystem
 *                         ACPI Object oriented interfaces
 *              xRevision: 89 $
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nsxfobj.c,v 1.1.1.1.4.3 2001/11/14 19:13:53 nathanw Exp $");

#define __NSXFOBJ_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_NAMESPACE
        MODULE_NAME         ("nsxfobj")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvaluateObject
 *
 * PARAMETERS:  Handle              - Object handle (optional)
 *              *Pathname           - Object pathname (optional)
 *              **Params            - List of parameters to pass to
 *                                    method, terminated by NULL.
 *                                    Params itself may be NULL
 *                                    if no parameters are being
 *                                    passed.
 *              *ReturnObject       - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and evaluate the given object, passing the given
 *              parameters if necessary.  One of "Handle" or "Pathname" must
 *              be valid (non-null)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvaluateObject (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ParamObjects,
    ACPI_BUFFER             *ReturnBuffer)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **ParamPtr = NULL;
    ACPI_OPERAND_OBJECT     *ReturnObj = NULL;
    ACPI_OPERAND_OBJECT     *ObjectPtr = NULL;
    UINT32                  BufferSpaceNeeded;
    UINT32                  UserBufferLength;
    UINT32                  Count;
    UINT32                  i;
    UINT32                  ParamLength;
    UINT32                  ObjectLength;


    FUNCTION_TRACE ("AcpiEvaluateObject");


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * If there are parameters to be passed to the object
     * (which must be a control method), the external objects
     * must be converted to internal objects
     */
    if (ParamObjects && ParamObjects->Count)
    {
        /*
         * Allocate a new parameter block for the internal objects
         * Add 1 to count to allow for null terminated internal list
         */
        Count           = ParamObjects->Count;
        ParamLength     = (Count + 1) * sizeof (void *);
        ObjectLength    = Count * sizeof (ACPI_OPERAND_OBJECT);

        ParamPtr = ACPI_MEM_CALLOCATE (ParamLength +    /* Parameter List part */
                                    ObjectLength);      /* Actual objects */
        if (!ParamPtr)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        ObjectPtr = (ACPI_OPERAND_OBJECT  *) ((UINT8 *) ParamPtr +
                        ParamLength);

        /*
         * Init the param array of pointers and NULL terminate
         * the list
         */
        for (i = 0; i < Count; i++)
        {
            ParamPtr[i] = &ObjectPtr[i];
            AcpiUtInitStaticObject (&ObjectPtr[i]);
        }
        ParamPtr[Count] = NULL;

        /*
         * Convert each external object in the list to an
         * internal object
         */
        for (i = 0; i < Count; i++)
        {
            Status = AcpiUtCopyEobjectToIobject (&ParamObjects->Pointer[i],
                                                ParamPtr[i]);

            if (ACPI_FAILURE (Status))
            {
                AcpiUtDeleteInternalObjectList (ParamPtr);
                return_ACPI_STATUS (Status);
            }
        }
    }


    /*
     * Three major cases:
     * 1) Fully qualified pathname
     * 2) No handle, not fully qualified pathname (error)
     * 3) Valid handle
     */
    if ((Pathname) &&
        (AcpiNsValidRootPrefix (Pathname[0])))
    {
        /*
         *  The path is fully qualified, just evaluate by name
         */
        Status = AcpiNsEvaluateByName (Pathname, ParamPtr, &ReturnObj);
    }

    else if (!Handle)
    {
        /*
         * A handle is optional iff a fully qualified pathname
         * is specified.  Since we've already handled fully
         * qualified names above, this is an error
         */
        if (!Pathname)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Both Handle and Pathname are NULL\n"));
        }

        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Handle is NULL and Pathname is relative\n"));
        }

        Status = AE_BAD_PARAMETER;
    }

    else
    {
        /*
         * We get here if we have a handle -- and if we have a
         * pathname it is relative.  The handle will be validated
         * in the lower procedures
         */
        if (!Pathname)
        {
            /*
             * The null pathname case means the handle is for
             * the actual object to be evaluated
             */
            Status = AcpiNsEvaluateByHandle (Handle, ParamPtr, &ReturnObj);
        }

        else
        {
           /*
            * Both a Handle and a relative Pathname
            */
            Status = AcpiNsEvaluateRelative (Handle, Pathname, ParamPtr,
                                                &ReturnObj);
        }
    }


    /*
     * If we are expecting a return value, and all went well above,
     * copy the return value to an external object.
     */

    if (ReturnBuffer)
    {
        UserBufferLength = ReturnBuffer->Length;
        ReturnBuffer->Length = 0;

        if (ReturnObj)
        {
            if (VALID_DESCRIPTOR_TYPE (ReturnObj, ACPI_DESC_TYPE_NAMED))
            {
                /*
                 * If we got an Node as a return object,
                 * this means the object we are evaluating
                 * has nothing interesting to return (such
                 * as a mutex, etc.)  We return an error
                 * because these types are essentially
                 * unsupported by this interface.  We
                 * don't check up front because this makes
                 * it easier to add support for various
                 * types at a later date if necessary.
                 */
                Status = AE_TYPE;
                ReturnObj = NULL;   /* No need to delete an Node */
            }

            if (ACPI_SUCCESS (Status))
            {
                /*
                 * Find out how large a buffer is needed
                 * to contain the returned object
                 */
                Status = AcpiUtGetObjectSize (ReturnObj,
                                                &BufferSpaceNeeded);
                if (ACPI_SUCCESS (Status))
                {
                    /*
                     * Check if there is enough room in the
                     * caller's buffer
                     */
                    if (UserBufferLength < BufferSpaceNeeded)
                    {
                        /*
                         * Caller's buffer is too small, can't
                         * give him partial results fail the call
                         * but return the buffer size needed
                         */
                        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                            "Needed buffer size %X, received %X\n",
                            BufferSpaceNeeded, UserBufferLength));

                        ReturnBuffer->Length = BufferSpaceNeeded;
                        Status = AE_BUFFER_OVERFLOW;
                    }

                    else
                    {
                        /*
                         *  We have enough space for the object, build it
                         */
                        Status = AcpiUtCopyIobjectToEobject (ReturnObj,
                                        ReturnBuffer);
                        ReturnBuffer->Length = BufferSpaceNeeded;
                    }
                }
            }
        }
    }


    /* Delete the return and parameter objects */

    if (ReturnObj)
    {
        /*
         * Delete the internal return object. (Or at least
         * decrement the reference count by one)
         */
        AcpiUtRemoveReference (ReturnObj);
    }

    /*
     * Free the input parameter list (if we created one),
     */
    if (ParamPtr)
    {
        /* Free the allocated parameter block */

        AcpiUtDeleteInternalObjectList (ParamPtr);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetNextObject
 *
 * PARAMETERS:  Type            - Type of object to be searched for
 *              Parent          - Parent object whose children we are getting
 *              LastChild       - Previous child that was found.
 *                                The NEXT child will be returned
 *              RetHandle       - Where handle to the next object is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return the next peer object within the namespace.  If Handle is
 *              valid, Scope is ignored.  Otherwise, the first object within
 *              Scope is returned.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetNextObject (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             Parent,
    ACPI_HANDLE             Child,
    ACPI_HANDLE             *RetHandle)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_NAMESPACE_NODE     *ParentNode = NULL;
    ACPI_NAMESPACE_NODE     *ChildNode = NULL;


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Parameter validation */

    if (Type > ACPI_TYPE_MAX)
    {
        return (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    /* If null handle, use the parent */

    if (!Child)
    {
        /* Start search at the beginning of the specified scope */

        ParentNode = AcpiNsConvertHandleToEntry (Parent);
        if (!ParentNode)
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }
    }

    /* Non-null handle, ignore the parent */

    else
    {
        /* Convert and validate the handle */

        ChildNode = AcpiNsConvertHandleToEntry (Child);
        if (!ChildNode)
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }
    }


    /* Internal function does the real work */

    Node = AcpiNsGetNextObject ((ACPI_OBJECT_TYPE8) Type,
                                    ParentNode, ChildNode);
    if (!Node)
    {
        Status = AE_NOT_FOUND;
        goto UnlockAndExit;
    }

    if (RetHandle)
    {
        *RetHandle = AcpiNsConvertEntryToHandle (Node);
    }


UnlockAndExit:

    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetType
 *
 * PARAMETERS:  Handle          - Handle of object whose type is desired
 *              *RetType        - Where the type will be placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This routine returns the type associatd with a particular handle
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetType (
    ACPI_HANDLE             Handle,
    ACPI_OBJECT_TYPE        *RetType)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Parameter Validation */

    if (!RetType)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Special case for the predefined Root Node
     * (return type ANY)
     */
    if (Handle == ACPI_ROOT_OBJECT)
    {
        *RetType = ACPI_TYPE_ANY;
        return (AE_OK);
    }

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    /* Convert and validate the handle */

    Node = AcpiNsConvertHandleToEntry (Handle);
    if (!Node)
    {
        AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        return (AE_BAD_PARAMETER);
    }

    *RetType = Node->Type;


    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetParent
 *
 * PARAMETERS:  Handle          - Handle of object whose parent is desired
 *              RetHandle       - Where the parent handle will be placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Returns a handle to the parent of the object represented by
 *              Handle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetParent (
    ACPI_HANDLE             Handle,
    ACPI_HANDLE             *RetHandle)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status = AE_OK;


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    if (!RetHandle)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Special case for the predefined Root Node (no parent) */

    if (Handle == ACPI_ROOT_OBJECT)
    {
        return (AE_NULL_ENTRY);
    }


    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    /* Convert and validate the handle */

    Node = AcpiNsConvertHandleToEntry (Handle);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }


    /* Get the parent entry */

    *RetHandle =
        AcpiNsConvertEntryToHandle (AcpiNsGetParentObject (Node));

    /* Return exeption if parent is null */

    if (!AcpiNsGetParentObject (Node))
    {
        Status = AE_NULL_ENTRY;
    }


UnlockAndExit:

    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiWalkNamespace
 *
 * PARAMETERS:  Type                - ACPI_OBJECT_TYPE to search for
 *              StartObject         - Handle in namespace where search begins
 *              MaxDepth            - Depth to which search is to reach
 *              UserFunction        - Called when an object of "Type" is found
 *              Context             - Passed to user function
 *              ReturnValue         - Location where return value of
 *                                    UserFunction is put if terminated early
 *
 * RETURNS      Return value from the UserFunction if terminated early.
 *              Otherwise, returns NULL.
 *
 * DESCRIPTION: Performs a modified depth-first walk of the namespace tree,
 *              starting (and ending) at the object specified by StartHandle.
 *              The UserFunction is called whenever an object that matches
 *              the type parameter is found.  If the user function returns
 *              a non-zero value, the search is terminated immediately and this
 *              value is returned to the caller.
 *
 *              The point of this procedure is to provide a generic namespace
 *              walk routine that can be called from multiple places to
 *              provide multiple services;  the User Function can be tailored
 *              to each task, whether it is a print function, a compare
 *              function, etc.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiWalkNamespace (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             StartObject,
    UINT32                  MaxDepth,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AcpiWalkNamespace");


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parameter validation */

    if ((Type > ACPI_TYPE_MAX)  ||
        (!MaxDepth)             ||
        (!UserFunction))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Lock the namespace around the walk.
     * The namespace will be unlocked/locked around each call
     * to the user function - since this function
     * must be allowed to make Acpi calls itself.
     */
    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    Status = AcpiNsWalkNamespace ((ACPI_OBJECT_TYPE8) Type,
                                    StartObject, MaxDepth,
                                    NS_WALK_UNLOCK,
                                    UserFunction, Context,
                                    ReturnValue);

    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetDeviceCallback
 *
 * PARAMETERS:  Callback from AcpiGetDevice
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes callbacks from WalkNamespace and filters out all non-
 *              present devices, or if they specified a HID, it filters based
 *              on that.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsGetDeviceCallback (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    UINT32                  Flags;
    ACPI_DEVICE_ID          DeviceId;
    ACPI_GET_DEVICES_INFO   *Info;


    Info = Context;

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    Node = AcpiNsConvertHandleToEntry (ObjHandle);
    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    if (!Node)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Run _STA to determine if device is present
     */
    Status = AcpiUtExecute_STA (Node, &Flags);
    if (ACPI_FAILURE (Status))
    {
        return (AE_CTRL_DEPTH);
    }

    if (!(Flags & 0x01))
    {
        /* don't return at the device or children of the device if not there */
        return (AE_CTRL_DEPTH);
    }

    /*
     * Filter based on device HID
     */
    if (Info->Hid != NULL)
    {
        Status = AcpiUtExecute_HID (Node, &DeviceId);
        if (Status == AE_NOT_FOUND)
        {
            return (AE_OK);
        }

        else if (ACPI_FAILURE (Status))
        {
            return (AE_CTRL_DEPTH);
        }

        if (STRNCMP (DeviceId.Buffer, Info->Hid, sizeof (DeviceId.Buffer)) != 0)
        {
            return (AE_OK);
        }
    }

    Info->UserFunction (ObjHandle, NestingLevel, Info->Context, ReturnValue);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetDevices
 *
 * PARAMETERS:  HID                 - HID to search for. Can be NULL.
 *              UserFunction        - Called when a matching object is found
 *              Context             - Passed to user function
 *              ReturnValue         - Location where return value of
 *                                    UserFunction is put if terminated early
 *
 * RETURNS      Return value from the UserFunction if terminated early.
 *              Otherwise, returns NULL.
 *
 * DESCRIPTION: Performs a modified depth-first walk of the namespace tree,
 *              starting (and ending) at the object specified by StartHandle.
 *              The UserFunction is called whenever an object that matches
 *              the type parameter is found.  If the user function returns
 *              a non-zero value, the search is terminated immediately and this
 *              value is returned to the caller.
 *
 *              This is a wrapper for WalkNamespace, but the callback performs
 *              additional filtering. Please see AcpiGetDeviceCallback.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetDevices (
    NATIVE_CHAR             *HID,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    ACPI_GET_DEVICES_INFO   Info;


    FUNCTION_TRACE ("AcpiGetDevices");


    /* Ensure that ACPI has been initialized */

    ACPI_IS_INITIALIZATION_COMPLETE (Status);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parameter validation */

    if (!UserFunction)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * We're going to call their callback from OUR callback, so we need
     * to know what it is, and their context parameter.
     */
    Info.Context      = Context;
    Info.UserFunction = UserFunction;
    Info.Hid          = HID;

    /*
     * Lock the namespace around the walk.
     * The namespace will be unlocked/locked around each call
     * to the user function - since this function
     * must be allowed to make Acpi calls itself.
     */
    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    Status = AcpiNsWalkNamespace (ACPI_TYPE_DEVICE,
                                    ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                                    NS_WALK_UNLOCK,
                                    AcpiNsGetDeviceCallback, &Info,
                                    ReturnValue);

    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    return_ACPI_STATUS (Status);
}
