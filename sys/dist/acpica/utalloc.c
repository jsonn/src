/******************************************************************************
 *
 * Module Name: utalloc - local memory allocation routines
 *              xRevision: 1.154 $
 *
 *****************************************************************************/

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
__KERNEL_RCSID(0, "$NetBSD: utalloc.c,v 1.1.4.2 2006/04/11 12:20:53 yamt Exp $");

#define __UTALLOC_C__

#include "acpi.h"

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utalloc")

/* Local prototypes */

#ifdef ACPI_DBG_TRACK_ALLOCATIONS

static ACPI_DEBUG_MEM_BLOCK *
AcpiUtFindAllocation (
    void                    *Allocation);

static ACPI_STATUS
AcpiUtTrackAllocation (
    ACPI_DEBUG_MEM_BLOCK    *Address,
    ACPI_SIZE               Size,
    UINT8                   AllocType,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

static ACPI_STATUS
AcpiUtRemoveAllocation (
    ACPI_DEBUG_MEM_BLOCK    *Address,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

static ACPI_STATUS
AcpiUtCreateList (
    const char              *ListName,
    UINT16                  ObjectSize,
    ACPI_MEMORY_LIST        **ReturnCache);
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateCaches
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create all local caches
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCreateCaches (
    void)
{
    ACPI_STATUS             Status;


#ifdef ACPI_DBG_TRACK_ALLOCATIONS

    /* Memory allocation lists */

    Status = AcpiUtCreateList ("Acpi-Global", 0,
                &AcpiGbl_GlobalList);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiUtCreateList ("Acpi-Namespace", sizeof (ACPI_NAMESPACE_NODE),
                &AcpiGbl_NsNodeList);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
#endif

    /* Object Caches, for frequently used objects */

    Status = AcpiOsCreateCache ("Acpi-State", sizeof (ACPI_GENERIC_STATE),
                ACPI_MAX_STATE_CACHE_DEPTH, &AcpiGbl_StateCache);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiOsCreateCache ("Acpi-Parse", sizeof (ACPI_PARSE_OBJ_COMMON),
                ACPI_MAX_PARSE_CACHE_DEPTH, &AcpiGbl_PsNodeCache);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiOsCreateCache ("Acpi-ParseExt", sizeof (ACPI_PARSE_OBJ_NAMED),
                ACPI_MAX_EXTPARSE_CACHE_DEPTH, &AcpiGbl_PsNodeExtCache);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiOsCreateCache ("Acpi-Operand", sizeof (ACPI_OPERAND_OBJECT),
                ACPI_MAX_OBJECT_CACHE_DEPTH, &AcpiGbl_OperandCache);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteCaches
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Purge and delete all local caches
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtDeleteCaches (
    void)
{

    (void) AcpiOsDeleteCache (AcpiGbl_StateCache);
    AcpiGbl_StateCache = NULL;

    (void) AcpiOsDeleteCache (AcpiGbl_OperandCache);
    AcpiGbl_OperandCache = NULL;

    (void) AcpiOsDeleteCache (AcpiGbl_PsNodeCache);
    AcpiGbl_PsNodeCache = NULL;

    (void) AcpiOsDeleteCache (AcpiGbl_PsNodeExtCache);
    AcpiGbl_PsNodeExtCache = NULL;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidateBuffer
 *
 * PARAMETERS:  Buffer              - Buffer descriptor to be validated
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform parameter validation checks on an ACPI_BUFFER
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtValidateBuffer (
    ACPI_BUFFER             *Buffer)
{

    /* Obviously, the structure pointer must be valid */

    if (!Buffer)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Special semantics for the length */

    if ((Buffer->Length == ACPI_NO_BUFFER)              ||
        (Buffer->Length == ACPI_ALLOCATE_BUFFER)        ||
        (Buffer->Length == ACPI_ALLOCATE_LOCAL_BUFFER))
    {
        return (AE_OK);
    }

    /* Length is valid, the buffer pointer must be also */

    if (!Buffer->Pointer)
    {
        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtInitializeBuffer
 *
 * PARAMETERS:  Buffer              - Buffer to be validated
 *              RequiredLength      - Length needed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Validate that the buffer is of the required length or
 *              allocate a new buffer.  Returned buffer is always zeroed.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtInitializeBuffer (
    ACPI_BUFFER             *Buffer,
    ACPI_SIZE               RequiredLength)
{
    ACPI_STATUS             Status = AE_OK;


    switch (Buffer->Length)
    {
    case ACPI_NO_BUFFER:

        /* Set the exception and returned the required length */

        Status = AE_BUFFER_OVERFLOW;
        break;


    case ACPI_ALLOCATE_BUFFER:

        /* Allocate a new buffer */

        Buffer->Pointer = AcpiOsAllocate (RequiredLength);
        if (!Buffer->Pointer)
        {
            return (AE_NO_MEMORY);
        }

        /* Clear the buffer */

        ACPI_MEMSET (Buffer->Pointer, 0, RequiredLength);
        break;


    case ACPI_ALLOCATE_LOCAL_BUFFER:

        /* Allocate a new buffer with local interface to allow tracking */

        Buffer->Pointer = ACPI_MEM_CALLOCATE (RequiredLength);
        if (!Buffer->Pointer)
        {
            return (AE_NO_MEMORY);
        }
        break;


    default:

        /* Existing buffer: Validate the size of the buffer */

        if (Buffer->Length < RequiredLength)
        {
            Status = AE_BUFFER_OVERFLOW;
            break;
        }

        /* Clear the buffer */

        ACPI_MEMSET (Buffer->Pointer, 0, RequiredLength);
        break;
    }

    Buffer->Length = RequiredLength;
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAllocate
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: The subsystem's equivalent of malloc.
 *
 ******************************************************************************/

void *
AcpiUtAllocate (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    void                    *Allocation;


    ACPI_FUNCTION_TRACE_U32 ("UtAllocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        ACPI_ERROR ((Module, Line,
            "UtAllocate: Attempt to allocate zero bytes, allocating 1 byte"));
        Size = 1;
    }

    Allocation = AcpiOsAllocate (Size);
    if (!Allocation)
    {
        /* Report allocation error */

        ACPI_ERROR ((Module, Line,
            "UtAllocate: Could not allocate size %X", (UINT32) Size));

        return_PTR (NULL);
    }

    return_PTR (Allocation);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCallocate
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: Subsystem equivalent of calloc.
 *
 ******************************************************************************/

void *
AcpiUtCallocate (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    void                    *Allocation;


    ACPI_FUNCTION_TRACE_U32 ("UtCallocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        ACPI_ERROR ((Module, Line,
            "Attempt to allocate zero bytes, allocating 1 byte"));
        Size = 1;
    }

    Allocation = AcpiOsAllocate (Size);
    if (!Allocation)
    {
        /* Report allocation error */

        ACPI_ERROR ((Module, Line,
            "Could not allocate size %X", (UINT32) Size));
        return_PTR (NULL);
    }

    /* Clear the memory block */

    ACPI_MEMSET (Allocation, 0, Size);
    return_PTR (Allocation);
}


#ifdef ACPI_DBG_TRACK_ALLOCATIONS
/*
 * These procedures are used for tracking memory leaks in the subsystem, and
 * they get compiled out when the ACPI_DBG_TRACK_ALLOCATIONS is not set.
 *
 * Each memory allocation is tracked via a doubly linked list.  Each
 * element contains the caller's component, module name, function name, and
 * line number.  AcpiUtAllocate and AcpiUtCallocate call
 * AcpiUtTrackAllocation to add an element to the list; deletion
 * occurs in the body of AcpiUtFree.
 */

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateList
 *
 * PARAMETERS:  CacheName       - Ascii name for the cache
 *              ObjectSize      - Size of each cached object
 *              ReturnCache     - Where the new cache object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a local memory list for tracking purposed
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCreateList (
    const char              *ListName,
    UINT16                  ObjectSize,
    ACPI_MEMORY_LIST        **ReturnCache)
{
    ACPI_MEMORY_LIST        *Cache;


    Cache = AcpiOsAllocate (sizeof (ACPI_MEMORY_LIST));
    if (!Cache)
    {
        return (AE_NO_MEMORY);
    }

    ACPI_MEMSET (Cache, 0, sizeof (ACPI_MEMORY_LIST));

    Cache->ListName   = ListName;
    Cache->ObjectSize = ObjectSize;

    *ReturnCache = Cache;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAllocateAndTrack
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: The subsystem's equivalent of malloc.
 *
 ******************************************************************************/

void *
AcpiUtAllocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *Allocation;
    ACPI_STATUS             Status;


    Allocation = AcpiUtAllocate (Size + sizeof (ACPI_DEBUG_MEM_HEADER),
                    Component, Module, Line);
    if (!Allocation)
    {
        return (NULL);
    }

    Status = AcpiUtTrackAllocation (Allocation, Size,
                    ACPI_MEM_MALLOC, Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Allocation);
        return (NULL);
    }

    AcpiGbl_GlobalList->TotalAllocated++;
    AcpiGbl_GlobalList->CurrentTotalSize += (UINT32) Size;

    return ((void *) &Allocation->UserSpace);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCallocateAndTrack
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: Subsystem equivalent of calloc.
 *
 ******************************************************************************/

void *
AcpiUtCallocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *Allocation;
    ACPI_STATUS             Status;


    Allocation = AcpiUtCallocate (Size + sizeof (ACPI_DEBUG_MEM_HEADER),
                    Component, Module, Line);
    if (!Allocation)
    {
        /* Report allocation error */

        ACPI_ERROR ((Module, Line,
            "Could not allocate size %X", (UINT32) Size));
        return (NULL);
    }

    Status = AcpiUtTrackAllocation (Allocation, Size,
                ACPI_MEM_CALLOC, Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Allocation);
        return (NULL);
    }

    AcpiGbl_GlobalList->TotalAllocated++;
    AcpiGbl_GlobalList->CurrentTotalSize += (UINT32) Size;

    return ((void *) &Allocation->UserSpace);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtFreeAndTrack
 *
 * PARAMETERS:  Allocation          - Address of the memory to deallocate
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      None
 *
 * DESCRIPTION: Frees the memory at Allocation
 *
 ******************************************************************************/

void
AcpiUtFreeAndTrack (
    void                    *Allocation,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *DebugBlock;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("UtFree", Allocation);


    if (NULL == Allocation)
    {
        ACPI_ERROR ((Module, Line,
            "Attempt to delete a NULL address"));

        return_VOID;
    }

    DebugBlock = ACPI_CAST_PTR (ACPI_DEBUG_MEM_BLOCK,
                    (((char *) Allocation) - sizeof (ACPI_DEBUG_MEM_HEADER)));

    AcpiGbl_GlobalList->TotalFreed++;
    AcpiGbl_GlobalList->CurrentTotalSize -= DebugBlock->Size;

    Status = AcpiUtRemoveAllocation (DebugBlock,
                    Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "Could not free memory"));
    }

    AcpiOsFree (DebugBlock);
    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "%p freed\n", Allocation));
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtFindAllocation
 *
 * PARAMETERS:  Allocation              - Address of allocated memory
 *
 * RETURN:      A list element if found; NULL otherwise.
 *
 * DESCRIPTION: Searches for an element in the global allocation tracking list.
 *
 ******************************************************************************/

static ACPI_DEBUG_MEM_BLOCK *
AcpiUtFindAllocation (
    void                    *Allocation)
{
    ACPI_DEBUG_MEM_BLOCK    *Element;


    ACPI_FUNCTION_ENTRY ();


    Element = AcpiGbl_GlobalList->ListHead;

    /* Search for the address. */

    while (Element)
    {
        if (Element == Allocation)
        {
            return (Element);
        }

        Element = Element->Next;
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtTrackAllocation
 *
 * PARAMETERS:  Allocation          - Address of allocated memory
 *              Size                - Size of the allocation
 *              AllocType           - MEM_MALLOC or MEM_CALLOC
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtTrackAllocation (
    ACPI_DEBUG_MEM_BLOCK    *Allocation,
    ACPI_SIZE               Size,
    UINT8                   AllocType,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    ACPI_MEMORY_LIST        *MemList;
    ACPI_DEBUG_MEM_BLOCK    *Element;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR ("UtTrackAllocation", Allocation);


    MemList = AcpiGbl_GlobalList;
    Status = AcpiUtAcquireMutex (ACPI_MTX_MEMORY);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Search list for this address to make sure it is not already on the list.
     * This will catch several kinds of problems.
     */
    Element = AcpiUtFindAllocation (Allocation);
    if (Element)
    {
        ACPI_ERROR ((AE_INFO,
            "UtTrackAllocation: Allocation already present in list! (%p)",
            Allocation));

        ACPI_ERROR ((AE_INFO, "Element %p Address %p",
            Element, Allocation));

        goto UnlockAndExit;
    }

    /* Fill in the instance data. */

    Allocation->Size      = (UINT32) Size;
    Allocation->AllocType = AllocType;
    Allocation->Component = Component;
    Allocation->Line      = Line;

    ACPI_STRNCPY (Allocation->Module, Module, ACPI_MAX_MODULE_NAME);
    Allocation->Module[ACPI_MAX_MODULE_NAME-1] = 0;

    /* Insert at list head */

    if (MemList->ListHead)
    {
        ((ACPI_DEBUG_MEM_BLOCK *)(MemList->ListHead))->Previous = Allocation;
    }

    Allocation->Next = MemList->ListHead;
    Allocation->Previous = NULL;

    MemList->ListHead = Allocation;


UnlockAndExit:
    Status = AcpiUtReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtRemoveAllocation
 *
 * PARAMETERS:  Allocation          - Address of allocated memory
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:
 *
 * DESCRIPTION: Deletes an element from the global allocation tracking list.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtRemoveAllocation (
    ACPI_DEBUG_MEM_BLOCK    *Allocation,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line)
{
    ACPI_MEMORY_LIST        *MemList;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("UtRemoveAllocation");


    MemList = AcpiGbl_GlobalList;
    if (NULL == MemList->ListHead)
    {
        /* No allocations! */

        ACPI_ERROR ((Module, Line,
            "Empty allocation list, nothing to free!"));

        return_ACPI_STATUS (AE_OK);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_MEMORY);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Unlink */

    if (Allocation->Previous)
    {
        (Allocation->Previous)->Next = Allocation->Next;
    }
    else
    {
        MemList->ListHead = Allocation->Next;
    }

    if (Allocation->Next)
    {
        (Allocation->Next)->Previous = Allocation->Previous;
    }

    /* Mark the segment as deleted */

    ACPI_MEMSET (&Allocation->UserSpace, 0xEA, Allocation->Size);

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "Freeing size 0%X\n",
        Allocation->Size));

    Status = AcpiUtReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDumpAllocationInfo
 *
 * PARAMETERS:
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print some info about the outstanding allocations.
 *
 ******************************************************************************/

void
AcpiUtDumpAllocationInfo (
    void)
{
/*
    ACPI_MEMORY_LIST        *MemList;
*/

    ACPI_FUNCTION_TRACE ("UtDumpAllocationInfo");

/*
    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current allocations",
                    MemList->CurrentCount,
                    ROUND_UP_TO_1K (MemList->CurrentSize)));

    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max concurrent allocations",
                    MemList->MaxConcurrentCount,
                    ROUND_UP_TO_1K (MemList->MaxConcurrentSize)));


    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Total (all) internal objects",
                    RunningObjectCount,
                    ROUND_UP_TO_1K (RunningObjectSize)));

    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Total (all) allocations",
                    RunningAllocCount,
                    ROUND_UP_TO_1K (RunningAllocSize)));


    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current Nodes",
                    AcpiGbl_CurrentNodeCount,
                    ROUND_UP_TO_1K (AcpiGbl_CurrentNodeSize)));

    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max Nodes",
                    AcpiGbl_MaxConcurrentNodeCount,
                    ROUND_UP_TO_1K ((AcpiGbl_MaxConcurrentNodeCount *
                        sizeof (ACPI_NAMESPACE_NODE)))));
*/
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDumpAllocations
 *
 * PARAMETERS:  Component           - Component(s) to dump info for.
 *              Module              - Module to dump info for.  NULL means all.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a list of all outstanding allocations.
 *
 ******************************************************************************/

void
AcpiUtDumpAllocations (
    UINT32                  Component,
    const char              *Module)
{
    ACPI_DEBUG_MEM_BLOCK    *Element;
    ACPI_DESCRIPTOR         *Descriptor;
    UINT32                  NumOutstanding = 0;


    ACPI_FUNCTION_TRACE ("UtDumpAllocations");


    /*
     * Walk the allocation list.
     */
    if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_MEMORY)))
    {
        return;
    }

    Element = AcpiGbl_GlobalList->ListHead;
    while (Element)
    {
        if ((Element->Component & Component) &&
            ((Module == NULL) || (0 == ACPI_STRCMP (Module, Element->Module))))
        {
            /* Ignore allocated objects that are in a cache */

            Descriptor = ACPI_CAST_PTR (ACPI_DESCRIPTOR, &Element->UserSpace);
            if (Descriptor->DescriptorId != ACPI_DESC_TYPE_CACHED)
            {
                AcpiOsPrintf ("%p Len %04X %9.9s-%d [%s] ",
                    Descriptor, Element->Size, Element->Module,
                    Element->Line, AcpiUtGetDescriptorName (Descriptor));

                /* Most of the elements will be Operand objects. */

                switch (ACPI_GET_DESCRIPTOR_TYPE (Descriptor))
                {
                case ACPI_DESC_TYPE_OPERAND:
                    AcpiOsPrintf ("%12.12s R%hd",
                        AcpiUtGetTypeName (Descriptor->Object.Common.Type),
                        Descriptor->Object.Common.ReferenceCount);
                    break;

                case ACPI_DESC_TYPE_PARSER:
                    AcpiOsPrintf ("AmlOpcode %04hX",
                        Descriptor->Op.Asl.AmlOpcode);
                    break;

                case ACPI_DESC_TYPE_NAMED:
                    AcpiOsPrintf ("%4.4s",
                        AcpiUtGetNodeName (&Descriptor->Node));
                    break;

                default:
                    break;
                }

                AcpiOsPrintf ( "\n");
                NumOutstanding++;
            }
        }
        Element = Element->Next;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_MEMORY);

    /* Print summary */

    if (!NumOutstanding)
    {
        ACPI_INFO ((AE_INFO,
            "No outstanding allocations"));
    }
    else
    {
        ACPI_ERROR ((AE_INFO,
            "%d(%X) Outstanding allocations",
            NumOutstanding, NumOutstanding));
    }

    return_VOID;
}

#endif  /* #ifdef ACPI_DBG_TRACK_ALLOCATIONS */

