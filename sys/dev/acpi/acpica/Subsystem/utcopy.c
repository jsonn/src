/******************************************************************************
 *
 * Module Name: utcopy - Internal to external object translation utilities
 *              xRevision: 79 $
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
__KERNEL_RCSID(0, "$NetBSD: utcopy.c,v 1.3.2.2 2002/01/10 19:53:35 thorpej Exp $");

#define __UTCOPY_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_UTILITIES
        MODULE_NAME         ("utcopy")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIsimpleToEsimple
 *
 * PARAMETERS:  *InternalObject     - Pointer to the object we are examining
 *              *Buffer             - Where the object is returned
 *              *SpaceUsed          - Where the data length is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to place a simple object in a user
 *              buffer.
 *
 *              The buffer is assumed to have sufficient space for the object.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCopyIsimpleToEsimple (
    ACPI_OPERAND_OBJECT     *InternalObject,
    ACPI_OBJECT             *ExternalObject,
    UINT8                   *DataSpace,
    UINT32                  *BufferSpaceUsed)
{
    UINT32                  Length = 0;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("UtCopyIsimpleToEsimple");


    /*
     * Check for NULL object case (could be an uninitialized
     * package element
     */
    if (!InternalObject)
    {
        *BufferSpaceUsed = 0;
        return_ACPI_STATUS (AE_OK);
    }

    /* Always clear the external object */

    MEMSET (ExternalObject, 0, sizeof (ACPI_OBJECT));

    /*
     * In general, the external object will be the same type as
     * the internal object
     */
    ExternalObject->Type = InternalObject->Common.Type;

    /* However, only a limited number of external types are supported */

    switch (InternalObject->Common.Type)
    {

    case ACPI_TYPE_STRING:

        Length = InternalObject->String.Length + 1;
        ExternalObject->String.Length = InternalObject->String.Length;
        ExternalObject->String.Pointer = (NATIVE_CHAR *) DataSpace;
        MEMCPY ((void *) DataSpace, (void *) InternalObject->String.Pointer, Length);
        break;


    case ACPI_TYPE_BUFFER:

        Length = InternalObject->Buffer.Length;
        ExternalObject->Buffer.Length = InternalObject->Buffer.Length;
        ExternalObject->Buffer.Pointer = DataSpace;
        MEMCPY ((void *) DataSpace, (void *) InternalObject->Buffer.Pointer, Length);
        break;


    case ACPI_TYPE_INTEGER:

        ExternalObject->Integer.Value= InternalObject->Integer.Value;
        break;


    case INTERNAL_TYPE_REFERENCE:

        /*
         * This is an object reference.  Attempt to dereference it.
         */
        switch (InternalObject->Reference.Opcode)
        {
        case AML_ZERO_OP:
            ExternalObject->Type = ACPI_TYPE_INTEGER;
            ExternalObject->Integer.Value = 0;
            break;

        case AML_ONE_OP:
            ExternalObject->Type = ACPI_TYPE_INTEGER;
            ExternalObject->Integer.Value = 1;
            break;

        case AML_ONES_OP:
            ExternalObject->Type = ACPI_TYPE_INTEGER;
            ExternalObject->Integer.Value = ACPI_INTEGER_MAX;
            break;

        case AML_INT_NAMEPATH_OP:
            /*
             * This is a named reference, get the string.  We already know that
             * we have room for it, use max length
             */
            Length = MAX_STRING_LENGTH;
            ExternalObject->Type = ACPI_TYPE_STRING;
            ExternalObject->String.Pointer = (NATIVE_CHAR *) DataSpace;
            Status = AcpiNsHandleToPathname ((ACPI_HANDLE *) InternalObject->Reference.Node,
                        &Length, (char *) DataSpace);

            /* Converted (external) string length is returned from above */

            ExternalObject->String.Length = Length;
            break;

        default:
            /*
             * Use the object type of "Any" to indicate a reference
             * to object containing a handle to an ACPI named object.
             */
            ExternalObject->Type = ACPI_TYPE_ANY;
            ExternalObject->Reference.Handle = InternalObject->Reference.Node;
            break;
        }
        break;


    case ACPI_TYPE_PROCESSOR:

        ExternalObject->Processor.ProcId = InternalObject->Processor.ProcId;
        ExternalObject->Processor.PblkAddress = InternalObject->Processor.Address;
        ExternalObject->Processor.PblkLength = InternalObject->Processor.Length;
        break;


    case ACPI_TYPE_POWER:

        ExternalObject->PowerResource.SystemLevel =
                            InternalObject->PowerResource.SystemLevel;

        ExternalObject->PowerResource.ResourceOrder =
                            InternalObject->PowerResource.ResourceOrder;
        break;


    default:
        /*
         * There is no corresponding external object type
         */
        return_ACPI_STATUS (AE_SUPPORT);
        break;
    }


    *BufferSpaceUsed = (UINT32) ROUND_UP_TO_NATIVE_WORD (Length);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIelementToEelement
 *
 * PARAMETERS:  ACPI_PKG_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Copy one package element to another package element
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCopyIelementToEelement (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PKG_INFO           *Info = (ACPI_PKG_INFO *) Context;
    UINT32                  ObjectSpace;
    UINT32                  ThisIndex;
    ACPI_OBJECT             *TargetObject;


    FUNCTION_ENTRY ();


    ThisIndex    = State->Pkg.Index;
    TargetObject = (ACPI_OBJECT *)
                    &((ACPI_OBJECT *)(State->Pkg.DestObject))->Package.Elements[ThisIndex];

    switch (ObjectType)
    {
    case ACPI_COPY_TYPE_SIMPLE:

        /*
         * This is a simple or null object -- get the size
         */
        Status = AcpiUtCopyIsimpleToEsimple (SourceObject,
                        TargetObject, Info->FreeSpace, &ObjectSpace);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        break;

    case ACPI_COPY_TYPE_PACKAGE:

        /*
         * Build the package object
         */
        TargetObject->Type              = ACPI_TYPE_PACKAGE;
        TargetObject->Package.Count     = SourceObject->Package.Count;
        TargetObject->Package.Elements  = (ACPI_OBJECT *) Info->FreeSpace;

        /*
         * Pass the new package object back to the package walk routine
         */
        State->Pkg.ThisTargetObj = TargetObject;

        /*
         * Save space for the array of objects (Package elements)
         * update the buffer length counter
         */
        ObjectSpace = (UINT32) ROUND_UP_TO_NATIVE_WORD (
                            TargetObject->Package.Count * sizeof (ACPI_OBJECT));
        break;

    default:
        return (AE_BAD_PARAMETER);
    }


    Info->FreeSpace   += ObjectSpace;
    Info->Length      += ObjectSpace;

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIpackageToEpackage
 *
 * PARAMETERS:  *InternalObject     - Pointer to the object we are returning
 *              *Buffer             - Where the object is returned
 *              *SpaceUsed          - Where the object length is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to place a package object in a user
 *              buffer.  A package object by definition contains other objects.
 *
 *              The buffer is assumed to have sufficient space for the object.
 *              The caller must have verified the buffer length needed using the
 *              AcpiUtGetObjectSize function before calling this function.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCopyIpackageToEpackage (
    ACPI_OPERAND_OBJECT     *InternalObject,
    UINT8                   *Buffer,
    UINT32                  *SpaceUsed)
{
    ACPI_OBJECT             *ExternalObject;
    ACPI_STATUS             Status;
    ACPI_PKG_INFO           Info;


    FUNCTION_TRACE ("UtCopyIpackageToEpackage");


    /*
     * First package at head of the buffer
     */
    ExternalObject = (ACPI_OBJECT *) Buffer;

    /*
     * Free space begins right after the first package
     */
    Info.Length      = 0;
    Info.ObjectSpace = 0;
    Info.NumPackages = 1;
    Info.FreeSpace   = Buffer + ROUND_UP_TO_NATIVE_WORD (sizeof (ACPI_OBJECT));


    ExternalObject->Type               = InternalObject->Common.Type;
    ExternalObject->Package.Count      = InternalObject->Package.Count;
    ExternalObject->Package.Elements   = (ACPI_OBJECT *) Info.FreeSpace;


    /*
     * Build an array of ACPI_OBJECTS in the buffer
     * and move the free space past it
     */
    Info.FreeSpace += ExternalObject->Package.Count *
                    ROUND_UP_TO_NATIVE_WORD (sizeof (ACPI_OBJECT));


    Status = AcpiUtWalkPackageTree (InternalObject, ExternalObject,
                            AcpiUtCopyIelementToEelement, &Info);

    *SpaceUsed = Info.Length;

    return_ACPI_STATUS (Status);

}

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIobjectToEobject
 *
 * PARAMETERS:  *InternalObject     - The internal object to be converted
 *              *BufferPtr          - Where the object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to build an API object to be returned to
 *              the caller.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCopyIobjectToEobject (
    ACPI_OPERAND_OBJECT     *InternalObject,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("UtCopyIobjectToEobject");


    if (IS_THIS_OBJECT_TYPE (InternalObject, ACPI_TYPE_PACKAGE))
    {
        /*
         * Package object:  Copy all subobjects (including
         * nested packages)
         */
        Status = AcpiUtCopyIpackageToEpackage (InternalObject,
                        RetBuffer->Pointer, &RetBuffer->Length);
    }

    else
    {
        /*
         * Build a simple object (no nested objects)
         */
        Status = AcpiUtCopyIsimpleToEsimple (InternalObject,
                        (ACPI_OBJECT *) RetBuffer->Pointer,
                        ((UINT8 *) RetBuffer->Pointer +
                        ROUND_UP_TO_NATIVE_WORD (sizeof (ACPI_OBJECT))),
                        &RetBuffer->Length);
        /*
         * build simple does not include the object size in the length
         * so we add it in here
         */
        RetBuffer->Length += sizeof (ACPI_OBJECT);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyEsimpleToIsimple
 *
 * PARAMETERS:  *ExternalObject    - The external object to be converted
 *              *InternalObject    - Where the internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function copies an external object to an internal one.
 *              NOTE: Pointers can be copied, we don't need to copy data.
 *              (The pointers have to be valid in our address space no matter
 *              what we do with them!)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCopyEsimpleToIsimple (
    ACPI_OBJECT             *ExternalObject,
    ACPI_OPERAND_OBJECT     *InternalObject)
{

    FUNCTION_TRACE ("UtCopyEsimpleToIsimple");


    InternalObject->Common.Type = (UINT8) ExternalObject->Type;

    /*
     * Simple types supported are: String, Buffer, Integer
     */
    switch (ExternalObject->Type)
    {

    case ACPI_TYPE_STRING:

        InternalObject->String.Length  = ExternalObject->String.Length;
        InternalObject->String.Pointer = ExternalObject->String.Pointer;
        break;


    case ACPI_TYPE_BUFFER:

        InternalObject->Buffer.Length  = ExternalObject->Buffer.Length;
        InternalObject->Buffer.Pointer = ExternalObject->Buffer.Pointer;
        break;


    case ACPI_TYPE_INTEGER:

        InternalObject->Integer.Value   = ExternalObject->Integer.Value;
        break;

    default:
        /*
         * Whatever other type -- it is not supported
         */
        return_ACPI_STATUS (AE_SUPPORT);
        break;
    }


    return_ACPI_STATUS (AE_OK);
}


#ifdef ACPI_FUTURE_IMPLEMENTATION

/* Code to convert packages that are parameters to control methods */

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyEpackageToIpackage
 *
 * PARAMETERS:  *InternalObject    - Pointer to the object we are returning
 *              *Buffer         - Where the object is returned
 *              *SpaceUsed      - Where the length of the object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to place a package object in a user
 *              buffer.  A package object by definition contains other objects.
 *
 *              The buffer is assumed to have sufficient space for the object.
 *              The caller must have verified the buffer length needed using the
 *              AcpiUtGetObjectSize function before calling this function.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCopyEpackageToIpackage (
    ACPI_OPERAND_OBJECT     *InternalObject,
    UINT8                   *Buffer,
    UINT32                  *SpaceUsed)
{
    UINT8                   *FreeSpace;
    ACPI_OBJECT             *ExternalObject;
    UINT32                  Length = 0;
    UINT32                  ThisIndex;
    UINT32                  ObjectSpace = 0;
    ACPI_OPERAND_OBJECT     *ThisInternalObj;
    ACPI_OBJECT             *ThisExternalObj;


    FUNCTION_TRACE ("UtCopyEpackageToIpackage");


    /*
     * First package at head of the buffer
     */
    ExternalObject = (ACPI_OBJECT *)Buffer;

    /*
     * Free space begins right after the first package
     */
    FreeSpace = Buffer + sizeof(ACPI_OBJECT);


    ExternalObject->Type               = InternalObject->Common.Type;
    ExternalObject->Package.Count      = InternalObject->Package.Count;
    ExternalObject->Package.Elements   = (ACPI_OBJECT *)FreeSpace;


    /*
     * Build an array of ACPI_OBJECTS in the buffer
     * and move the free space past it
     */
    FreeSpace += ExternalObject->Package.Count * sizeof(ACPI_OBJECT);


    /* Call WalkPackage */

}

#endif /* Future implementation */


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyEobjectToIobject
 *
 * PARAMETERS:  *InternalObject    - The external object to be converted
 *              *BufferPtr      - Where the internal object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: Converts an external object to an internal object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCopyEobjectToIobject (
    ACPI_OBJECT             *ExternalObject,
    ACPI_OPERAND_OBJECT     *InternalObject)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("UtCopyEobjectToIobject");


    if (ExternalObject->Type == ACPI_TYPE_PACKAGE)
    {
        /*
         * Package objects contain other objects (which can be objects)
         * buildpackage does it all
         *
         * TBD: Package conversion must be completed and tested
         * NOTE: this code converts packages as input parameters to
         * control methods only.  This is a very, very rare case.
         */
/*
        Status = AcpiUtCopyEpackageToIpackage(InternalObject,
                                                  RetBuffer->Pointer,
                                                  &RetBuffer->Length);
*/
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Packages as parameters not implemented!\n"));

        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
    }

    else
    {
        /*
         * Build a simple object (no nested objects)
         */
        Status = AcpiUtCopyEsimpleToIsimple (ExternalObject, InternalObject);
        /*
         * build simple does not include the object size in the length
         * so we add it in here
         */
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIelementToIelement
 *
 * PARAMETERS:  ACPI_PKG_CALLBACK
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: Copy one package element to another package element
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCopyIelementToIelement (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  ThisIndex;
    ACPI_OPERAND_OBJECT     **ThisTargetPtr;
    ACPI_OPERAND_OBJECT     *TargetObject;


    FUNCTION_ENTRY ();


    ThisIndex     = State->Pkg.Index;
    ThisTargetPtr = (ACPI_OPERAND_OBJECT **)
                        &State->Pkg.DestObject->Package.Elements[ThisIndex];

    switch (ObjectType)
    {
    case 0:

        /*
         * This is a simple object, just copy it
         */
        TargetObject = AcpiUtCreateInternalObject (SourceObject->Common.Type);
        if (!TargetObject)
        {
            return (AE_NO_MEMORY);
        }

        Status = AcpiExStoreObjectToObject (SourceObject, TargetObject,
                        (ACPI_WALK_STATE *) Context);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        *ThisTargetPtr = TargetObject;
        break;


    case 1:
        /*
         * This object is a package - go down another nesting level
         * Create and build the package object
         */
        TargetObject = AcpiUtCreateInternalObject (ACPI_TYPE_PACKAGE);
        if (!TargetObject)
        {
            /* TBD: must delete package created up to this point */

            return (AE_NO_MEMORY);
        }

        TargetObject->Package.Count = SourceObject->Package.Count;

        /*
         * Pass the new package object back to the package walk routine
         */
        State->Pkg.ThisTargetObj = TargetObject;

        /*
         * Store the object pointer in the parent package object
         */
        *ThisTargetPtr = TargetObject;
        break;

    default:
        return (AE_BAD_PARAMETER);
    }


    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCopyIpackageToIpackage
 *
 * PARAMETERS:  *SourceObj      - Pointer to the source package object
 *              *DestObj        - Where the internal object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to copy an internal package object
 *              into another internal package object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCopyIpackageToIpackage (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("UtCopyIpackageToIpackage");


    DestObj->Common.Type    = SourceObj->Common.Type;
    DestObj->Package.Count  = SourceObj->Package.Count;


    /*
     * Create the object array and walk the source package tree
     */
    DestObj->Package.Elements = ACPI_MEM_CALLOCATE ((SourceObj->Package.Count + 1) *
                                                    sizeof (void *));
    DestObj->Package.NextElement = DestObj->Package.Elements;

    if (!DestObj->Package.Elements)
    {
        REPORT_ERROR (
            ("AmlBuildCopyInternalPackageObject: Package allocation failure\n"));
        return_ACPI_STATUS (AE_NO_MEMORY);
    }


    Status = AcpiUtWalkPackageTree (SourceObj, DestObj,
                            AcpiUtCopyIelementToIelement, WalkState);

    return_ACPI_STATUS (Status);
}

