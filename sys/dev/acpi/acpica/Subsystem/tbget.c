/******************************************************************************
 *
 * Module Name: tbget - ACPI Table get* routines
 *              $Revision: 1.2.2.3 $
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
__KERNEL_RCSID(0, "$NetBSD: tbget.c,v 1.2.2.3 2002/06/23 17:45:46 jdolecek Exp $");

#define __TBGET_C__

#include "acpi.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbget")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbTableOverride
 *
 * PARAMETERS:  *TableInfo          - Info for current table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Attempts override of current table with a new one if provided
 *              by the host OS.
 *
 ******************************************************************************/

void
AcpiTbTableOverride (
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_HEADER       *NewTable;
    ACPI_STATUS             Status;
    ACPI_POINTER            Address;
    ACPI_TABLE_DESC         NewTableInfo;


    ACPI_FUNCTION_TRACE ("AcpiTbTableOverride");


    Status = AcpiOsTableOverride (TableInfo->Pointer, &NewTable);
    if (ACPI_FAILURE (Status))
    {
        /* Some severe error from the OSL, but we basically ignore it */

        ACPI_REPORT_ERROR (("Could not override ACPI table, %s\n", 
            AcpiFormatException (Status)));
        return_VOID;
    }

    if (!NewTable)
    {
        /* No table override */

        return_VOID;
    }

    /* 
     * We have a new table to override the old one.  Get a copy of 
     * the new one.  We know that the new table has a logical pointer.
     */
    Address.PointerType     = ACPI_LOGICAL_POINTER;
    Address.Pointer.Logical = NewTable;

    Status = AcpiTbGetTable (&Address, &NewTableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not copy ACPI table override\n"));
        return_VOID;
    }

    /*
     * Delete the original table
     */
    AcpiTbDeleteSingleTable (TableInfo);

    /* Copy the table info */

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Successful table override [%4.4s]\n", 
        ((ACPI_TABLE_HEADER *) NewTableInfo.Pointer)->Signature));

    ACPI_MEMCPY (TableInfo, &NewTableInfo, sizeof (ACPI_TABLE_DESC));
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableWithOverride
 *
 * PARAMETERS:  Address             - Physical or logical address of table
 *              *TableInfo          - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Gets and installs the table with possible table override by OS.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTableWithOverride (
    ACPI_POINTER            *Address,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiTbGetTableWithOverride");


    Status = AcpiTbGetTable (Address, TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not get ACPI table, %s\n", 
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /*
     * Attempt override.  It either happens or it doesn't, no status
     */
    AcpiTbTableOverride (TableInfo);

    /* Install the table */

    Status = AcpiTbInstallTable (TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not install ACPI table, %s\n", 
            AcpiFormatException (Status)));
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTablePtr
 *
 * PARAMETERS:  TableType       - one of the defined table types
 *              Instance        - Which table of this type
 *              TablePtrLoc     - pointer to location to place the pointer for
 *                                return
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the pointer to an ACPI table.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTablePtr (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **TablePtrLoc)
{
    ACPI_TABLE_DESC         *TableDesc;
    UINT32                  i;


    ACPI_FUNCTION_TRACE ("TbGetTablePtr");


    if (!AcpiGbl_DSDT)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    if (TableType > ACPI_TABLE_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * For all table types (Single/Multiple), the first
     * instance is always in the list head.
     */
    if (Instance == 1)
    {
        /*
         * Just pluck the pointer out of the global table!
         * Will be null if no table is present
         */
        *TablePtrLoc = AcpiGbl_AcpiTables[TableType].Pointer;
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Check for instance out of range
     */
    if (Instance > AcpiGbl_AcpiTables[TableType].Count)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Walk the list to get the desired table
     * Since the if (Instance == 1) check above checked for the
     * first table, setting TableDesc equal to the .Next member
     * is actually pointing to the second table.  Therefore, we
     * need to walk from the 2nd table until we reach the Instance
     * that the user is looking for and return its table pointer.
     */
    TableDesc = AcpiGbl_AcpiTables[TableType].Next;
    for (i = 2; i < Instance; i++)
    {
        TableDesc = TableDesc->Next;
    }

    /* We are now pointing to the requested table's descriptor */

    *TablePtrLoc = TableDesc->Pointer;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTable
 *
 * PARAMETERS:  Address             - Physical address of table to retrieve
 *              *TableInfo          - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Maps the physical address of table into a logical address
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_HEADER       *TableHeader = NULL;
    ACPI_TABLE_HEADER       *FullTable = NULL;
    ACPI_SIZE               Size;
    UINT8                   Allocation;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("TbGetTable");


    if (!TableInfo || !Address)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    switch (Address->PointerType)
    {
    case ACPI_LOGICAL_POINTER:

        /*
         * Getting data from a buffer, not BIOS tables
         */
        TableHeader = Address->Pointer.Logical;

        /* Allocate buffer for the entire table */

        FullTable = ACPI_MEM_ALLOCATE (TableHeader->Length);
        if (!FullTable)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the entire table (including header) to the local buffer */

        Size = (ACPI_SIZE) TableHeader->Length;
        ACPI_MEMCPY (FullTable, TableHeader, Size);

        /* Save allocation type */

        Allocation = ACPI_MEM_ALLOCATED;
        break;


    case ACPI_PHYSICAL_POINTER:

        /*
         * Not reading from a buffer, just map the table's physical memory
         * into our address space.
         */
        Size = SIZE_IN_HEADER;

        Status = AcpiTbMapAcpiTable (Address->Pointer.Physical, &Size, &FullTable);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Save allocation type */

        Allocation = ACPI_MEM_MAPPED;
        break;


    default:
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Return values */

    TableInfo->Pointer      = FullTable;
    TableInfo->Length       = Size;
    TableInfo->Allocation   = Allocation;
    TableInfo->BasePointer  = FullTable;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, 
        "Found table [%4.4s] at %8.8X%8.8X, mapped/copied to %p\n", 
        FullTable->Signature, 
        ACPI_HIDWORD (Address->Pointer.Physical),
        ACPI_LODWORD (Address->Pointer.Physical), FullTable));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetAllTables
 *
 * PARAMETERS:  NumberOfTables      - Number of tables to get
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate tables other than the RSDT.  The RSDT must
 *              already be loaded and validated.
 *
 *              Get the minimum set of ACPI tables, namely:
 *
 *              1) FADT (via RSDT in loop below)
 *              2) FACS (via FADT)
 *              3) DSDT (via FADT)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetAllTables (
    UINT32                  NumberOfTables)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Index;
    ACPI_TABLE_DESC         TableInfo;
    ACPI_POINTER            Address;     


    ACPI_FUNCTION_TRACE ("TbGetAllTables");

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Number of tables: %d\n", NumberOfTables));


    /*
     * Loop through all table pointers found in RSDT.
     * This will NOT include the FACS and DSDT - we must get
     * them after the loop.
     *
     * The ONLY table we are interested in getting here is the FADT.
     */
    for (Index = 0; Index < NumberOfTables; Index++)
    {
        /* Clear the TableInfo each time */

        ACPI_MEMSET (&TableInfo, 0, sizeof (ACPI_TABLE_DESC));

        /* Get the table via the XSDT */

        Address.PointerType   = AcpiGbl_TableFlags;
        Address.Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_XSDT->TableOffsetEntry[Index]);

        Status = AcpiTbGetTable (&Address, &TableInfo);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Recognize and install the table */

        Status = AcpiTbInstallTable (&TableInfo);
        if (ACPI_FAILURE (Status))
        {
            /*
             * Unrecognized or unsupported table, delete it and ignore the
             * error.  Just get as many tables as we can, later we will
             * determine if there are enough tables to continue.
             */
            (void) AcpiTbUninstallTable (&TableInfo);
            Status = AE_OK;
        }
    }

    if (!AcpiGbl_FADT)
    {
        ACPI_REPORT_ERROR (("No FADT present in R/XSDT\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /*
     * Convert the FADT to a common format.  This allows earlier revisions of the
     * table to coexist with newer versions, using common access code.
     */
    Status = AcpiTbConvertTableFadt ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not convert FADT to internal common format\n"));
        return_ACPI_STATUS (Status);
    }

    /*
     * Get the FACS (must have the FADT first, from loop above)
     * AcpiTbGetTableFacs will fail if FADT pointer is not valid
     */
    Address.PointerType   = AcpiGbl_TableFlags;
    Address.Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_FADT->XFirmwareCtrl);

    Status = AcpiTbGetTable (&Address, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not get the FACS, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /* Install the FACS */

    Status = AcpiTbInstallTable (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not install the FACS, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /*
     * Create the common FACS pointer table
     * (Contains pointers to the original table)
     */
    Status = AcpiTbBuildCommonFacs (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Get/install the DSDT (We know that the FADT is valid now)
     */
    Address.PointerType   = AcpiGbl_TableFlags;
    Address.Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_FADT->XDsdt);

    Status = AcpiTbGetTableWithOverride (&Address, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not get the DSDT\n"));
        return_ACPI_STATUS (Status);
    }

    /* Set Integer Width (32/64) based upon DSDT revision */

    AcpiUtSetIntegerWidth (AcpiGbl_DSDT->Revision);

    /* Dump the entire DSDT */

    ACPI_DEBUG_PRINT ((ACPI_DB_TABLES,
        "Hex dump of entire DSDT, size %d (0x%X), Integer width = %d\n",
        AcpiGbl_DSDT->Length, AcpiGbl_DSDT->Length, AcpiGbl_IntegerBitWidth));
    ACPI_DUMP_BUFFER ((UINT8 *) AcpiGbl_DSDT, AcpiGbl_DSDT->Length);

    /* Always delete the RSDP mapping, we are done with it */

    AcpiTbDeleteAcpiTable (ACPI_TABLE_RSDP);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbVerifyRsdp
 *
 * PARAMETERS:  NumberOfTables      - Where the table count is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate the RSDP (ptr) and RSDT (table)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbVerifyRsdp (
    ACPI_POINTER            *Address)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;
    RSDP_DESCRIPTOR         *Rsdp;


    ACPI_FUNCTION_TRACE ("TbVerifyRsdp");


    switch (Address->PointerType)
    {
    case ACPI_LOGICAL_POINTER:

        Rsdp = Address->Pointer.Logical;
        break;

    case ACPI_PHYSICAL_POINTER:
        /*
         * Obtain access to the RSDP structure
         */
        Status = AcpiOsMapMemory (Address->Pointer.Physical, sizeof (RSDP_DESCRIPTOR),
                                    (void **) &Rsdp);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
        break;
    
    default:
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     *  The signature and checksum must both be correct
     */
    if (ACPI_STRNCMP ((NATIVE_CHAR *) Rsdp, RSDP_SIG, sizeof (RSDP_SIG)-1) != 0)
    {
        /* Nope, BAD Signature */

        Status = AE_BAD_SIGNATURE;
        goto Cleanup;
    }

    /* Check the standard checksum */

    if (AcpiTbChecksum (Rsdp, ACPI_RSDP_CHECKSUM_LENGTH) != 0)
    {
        Status = AE_BAD_CHECKSUM;
        goto Cleanup;
    }

    /* Check extended checksum if table version >= 2 */

    if (Rsdp->Revision >= 2)
    {
        if (AcpiTbChecksum (Rsdp, ACPI_RSDP_XCHECKSUM_LENGTH) != 0)
        {
            Status = AE_BAD_CHECKSUM;
            goto Cleanup;
        }
    }

    /* The RSDP supplied is OK */

    TableInfo.Pointer      = ACPI_CAST_PTR (ACPI_TABLE_HEADER, Rsdp);
    TableInfo.Length       = sizeof (RSDP_DESCRIPTOR);
    TableInfo.Allocation   = ACPI_MEM_MAPPED;
    TableInfo.BasePointer  = Rsdp;

    /* Save the table pointers and allocation info */

    Status = AcpiTbInitTableDescriptor (ACPI_TABLE_RSDP, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Save the RSDP in a global for easy access */

    AcpiGbl_RSDP = ACPI_CAST_PTR (RSDP_DESCRIPTOR, TableInfo.Pointer);
    return_ACPI_STATUS (Status);


    /* Error exit */
Cleanup:

    if (AcpiGbl_TableFlags & ACPI_PHYSICAL_POINTER)
    {
        AcpiOsUnmapMemory (Rsdp, sizeof (RSDP_DESCRIPTOR));
    }
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetRsdtAddress
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDT physical address
 *
 * DESCRIPTION: Extract the address of the RSDT or XSDT, depending on the
 *              version of the RSDP
 *
 ******************************************************************************/

void
AcpiTbGetRsdtAddress (
    ACPI_POINTER            *OutAddress)
{

    ACPI_FUNCTION_ENTRY ();


    OutAddress->PointerType = AcpiGbl_TableFlags;

    /*
     * For RSDP revision 0 or 1, we use the RSDT.
     * For RSDP revision 2 (and above), we use the XSDT
     */
    if (AcpiGbl_RSDP->Revision < 2)
    {
        OutAddress->Pointer.Value = AcpiGbl_RSDP->RsdtPhysicalAddress;
    }
    else
    {
        OutAddress->Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_RSDP->XsdtPhysicalAddress);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbValidateRsdt
 *
 * PARAMETERS:  TablePtr        - Addressable pointer to the RSDT.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Validate signature for the RSDT or XSDT
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbValidateRsdt (
    ACPI_TABLE_HEADER       *TablePtr)
{
    int                     NoMatch;


    ACPI_FUNCTION_NAME ("TbValidateRsdt");


    /*
     * For RSDP revision 0 or 1, we use the RSDT.
     * For RSDP revision 2 and above, we use the XSDT
     */
    if (AcpiGbl_RSDP->Revision < 2)
    {
        NoMatch = ACPI_STRNCMP ((char *) TablePtr, RSDT_SIG,
                        sizeof (RSDT_SIG) -1);
    }
    else
    {
        NoMatch = ACPI_STRNCMP ((char *) TablePtr, XSDT_SIG,
                        sizeof (XSDT_SIG) -1);
    }

    if (NoMatch)
    {
        /* Invalid RSDT or XSDT signature */

        ACPI_REPORT_ERROR (("Invalid signature where RSDP indicates RSDT/XSDT should be located\n"));

        ACPI_DUMP_BUFFER (AcpiGbl_RSDP, 20);

        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_ERROR,
            "RSDT/XSDT signature at %X (%p) is invalid\n",
            AcpiGbl_RSDP->RsdtPhysicalAddress,
            (void *) (NATIVE_UINT) AcpiGbl_RSDP->RsdtPhysicalAddress));

        return (AE_BAD_SIGNATURE);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTablePointer
 *
 * PARAMETERS:  PhysicalAddress     - Address from RSDT
 *              Flags               - virtual or physical addressing
 *              TablePtr            - Addressable address (output)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an addressable pointer to an ACPI table
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTablePointer (
    ACPI_POINTER            *Address,
    UINT32                  Flags,
    ACPI_SIZE               *Size,
    ACPI_TABLE_HEADER       **TablePtr)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_ENTRY ();


    /*
     * What mode is the processor in? (Virtual or Physical addressing)
     */
    if ((Flags & ACPI_MEMORY_MODE) == ACPI_LOGICAL_ADDRESSING)
    {
        /* Incoming pointer can be either logical or physical */

        switch (Address->PointerType)
        {
        case ACPI_PHYSICAL_POINTER:

            *Size = SIZE_IN_HEADER;
            Status = AcpiTbMapAcpiTable (Address->Pointer.Physical, Size, TablePtr);
            break;

        case ACPI_LOGICAL_POINTER:

            *TablePtr = Address->Pointer.Logical;
            *Size = 0;
            break;

        default:
            return (AE_BAD_PARAMETER);
        }
    }
    else
    {
        /* In Physical addressing mode, all pointers must be physical */

        switch (Address->PointerType)
        {
        case ACPI_PHYSICAL_POINTER:
            *Size = 0;
            *TablePtr = Address->Pointer.Logical;
            break;

        case ACPI_LOGICAL_POINTER:

            Status = AE_BAD_PARAMETER;
            break;

        default:
            return (AE_BAD_PARAMETER);
        }
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableRsdt
 *
 * PARAMETERS:  NumberOfTables      - Where the table count is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate the RSDP (ptr) and RSDT (table)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTableRsdt (
    UINT32                  *NumberOfTables)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;
    ACPI_POINTER            Address;     


    ACPI_FUNCTION_TRACE ("TbGetTableRsdt");


    /* Get the RSDT/XSDT from the RSDP */

    AcpiTbGetRsdtAddress (&Address);
    Status = AcpiTbGetTable (&Address, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Could not get the R/XSDT, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "RSDP located at %p, RSDT physical=%8.8X%8.8X \n",
        AcpiGbl_RSDP,
        ACPI_HIDWORD (Address.Pointer.Value),
        ACPI_LODWORD (Address.Pointer.Value)));

    /* Check the RSDT or XSDT signature */

    Status = AcpiTbValidateRsdt (TableInfo.Pointer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Valid RSDT signature, verify the checksum.  If it fails, just
     * print a warning and ignore it.
     */
    Status = AcpiTbVerifyTableChecksum (TableInfo.Pointer);

    /* Convert and/or copy to an XSDT structure */

    Status = AcpiTbConvertToXsdt (&TableInfo, NumberOfTables);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Save the table pointers and allocation info */

    Status = AcpiTbInitTableDescriptor (ACPI_TABLE_XSDT, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    AcpiGbl_XSDT = (XSDT_DESCRIPTOR *) TableInfo.Pointer;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "XSDT located at %p\n", AcpiGbl_XSDT));
    return_ACPI_STATUS (Status);
}


