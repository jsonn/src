/******************************************************************************
 *
 * Name: acinterp.h - Interpreter subcomponent prototypes and defines
 *       xRevision: 106 $
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

#ifndef __ACINTERP_H__
#define __ACINTERP_H__


#define WALK_OPERANDS       &(WalkState->Operands [WalkState->NumOperands -1])


/* Interpreter constants */

#define AML_END_OF_BLOCK            -1
#define PUSH_PKG_LENGTH             1
#define DO_NOT_PUSH_PKG_LENGTH      0


#define STACK_TOP                   0
#define STACK_BOTTOM                (UINT32) -1

/* Constants for global "WhenToParseMethods" */

#define METHOD_PARSE_AT_INIT        0x0
#define METHOD_PARSE_JUST_IN_TIME   0x1
#define METHOD_DELETE_AT_COMPLETION 0x2


ACPI_STATUS
AcpiExResolveOperands (
    UINT16                  Opcode,
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState);


/*
 * amxface - External interpreter interfaces
 */

ACPI_STATUS
AcpiExLoadTable (
    ACPI_TABLE_TYPE         TableId);

ACPI_STATUS
AcpiExExecuteMethod (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     **Params,
    ACPI_OPERAND_OBJECT     **ReturnObjDesc);


/*
 * amconvrt - object conversion
 */

ACPI_STATUS
AcpiExConvertToInteger (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExConvertToBuffer (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExConvertToString (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    UINT32                  Base,
    UINT32                  MaxLength,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExConvertToTargetType (
    ACPI_OBJECT_TYPE8       DestinationType,
    ACPI_OPERAND_OBJECT     **ObjDesc,
    ACPI_WALK_STATE         *WalkState);


/*
 * amfield - ACPI AML (p-code) execution - field manipulation
 */

ACPI_STATUS
AcpiExExtractFromField (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);

ACPI_STATUS
AcpiExInsertIntoField (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);

ACPI_STATUS
AcpiExSetupField (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  FieldByteOffset);

ACPI_STATUS
AcpiExReadFieldDatum (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  FieldByteOffset,
    UINT32                  *Value);

ACPI_STATUS
AcpiExCommonAccessField (
    UINT32                  Mode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);


ACPI_STATUS
AcpiExAccessIndexField (
    UINT32                  Mode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);

ACPI_STATUS
AcpiExAccessBankField (
    UINT32                  Mode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);

ACPI_STATUS
AcpiExAccessRegionField (
    UINT32                  Mode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);


ACPI_STATUS
AcpiExAccessBufferField (
    UINT32                  Mode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength);

ACPI_STATUS
AcpiExReadDataFromField (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **RetBufferDesc);

ACPI_STATUS
AcpiExWriteDataToField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc);

/*
 * ammisc - ACPI AML (p-code) execution - specific opcodes
 */

ACPI_STATUS
AcpiExTriadic (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);

ACPI_STATUS
AcpiExHexadic (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);

ACPI_STATUS
AcpiExCreateBufferField (
    UINT8                   *AmlPtr,
    UINT32                  AmlLength,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExReconfiguration (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExCreateMutex (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExCreateProcessor (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *ProcessorNode);

ACPI_STATUS
AcpiExCreatePowerResource (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *PowerNode);

ACPI_STATUS
AcpiExCreateRegion (
    UINT8                   *AmlPtr,
    UINT32                  AmlLength,
    UINT8                   RegionSpace,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExCreateEvent (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExCreateAlias (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExCreateMethod (
    UINT8                   *AmlPtr,
    UINT32                  AmlLength,
    UINT32                  MethodFlags,
    ACPI_NAMESPACE_NODE     *Method);


/*
 * ammutex - mutex support
 */

ACPI_STATUS
AcpiExAcquireMutex (
    ACPI_OPERAND_OBJECT     *TimeDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExReleaseMutex (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExReleaseAllMutexes (
    ACPI_OPERAND_OBJECT     *MutexList);

void
AcpiExUnlinkMutex (
    ACPI_OPERAND_OBJECT     *ObjDesc);


/*
 * amprep - ACPI AML (p-code) execution - prep utilities
 */

ACPI_STATUS
AcpiExPrepCommonFieldObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT8                   FieldFlags,
    UINT32                  FieldPosition,
    UINT32                  FieldLength);

ACPI_STATUS
AcpiExPrepRegionFieldValue (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_HANDLE             Region,
    UINT8                   FieldFlags,
    UINT32                  FieldPosition,
    UINT32                  FieldLength);

ACPI_STATUS
AcpiExPrepBankFieldValue (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_NAMESPACE_NODE     *BankRegisterNode,
    UINT32                  BankVal,
    UINT8                   FieldFlags,
    UINT32                  FieldPosition,
    UINT32                  FieldLength);

ACPI_STATUS
AcpiExPrepIndexFieldValue (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_NAMESPACE_NODE     *IndexReg,
    ACPI_NAMESPACE_NODE     *DataReg,
    UINT8                   FieldFlags,
    UINT32                  FieldPosition,
    UINT32                  FieldLength);


/*
 * amsystem - Interface to OS services
 */

ACPI_STATUS
AcpiExSystemDoNotifyOp (
    ACPI_OPERAND_OBJECT     *Value,
    ACPI_OPERAND_OBJECT     *ObjDesc);

void
AcpiExSystemDoSuspend(
    UINT32                  Time);

void
AcpiExSystemDoStall (
    UINT32                  Time);

ACPI_STATUS
AcpiExSystemAcquireMutex(
    ACPI_OPERAND_OBJECT     *Time,
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiExSystemReleaseMutex(
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiExSystemSignalEvent(
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiExSystemWaitEvent(
    ACPI_OPERAND_OBJECT     *Time,
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiExSystemResetEvent(
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiExSystemWaitSemaphore (
    ACPI_HANDLE             Semaphore,
    UINT32                  Timeout);


/*
 * ammonadic - ACPI AML (p-code) execution, monadic operators
 */

ACPI_STATUS
AcpiExMonadic1 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExMonadic2 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);

ACPI_STATUS
AcpiExMonadic2R (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);


/*
 * amdyadic - ACPI AML (p-code) execution, dyadic operators
 */

ACPI_STATUS
AcpiExDyadic1 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExDyadic2 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);

ACPI_STATUS
AcpiExDyadic2R (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);

ACPI_STATUS
AcpiExDyadic2S (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc);


/*
 * amresolv  - Object resolution and get value functions
 */

ACPI_STATUS
AcpiExResolveToValue (
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExResolveNodeToValue (
    ACPI_NAMESPACE_NODE     **StackPtr,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExResolveObjectToValue (
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExGetBufferFieldValue (
    ACPI_OPERAND_OBJECT     *FieldDesc,
    ACPI_OPERAND_OBJECT     *ResultDesc);


/*
 * amdump - Scanner debug output routines
 */

void
AcpiExShowHexValue (
    UINT32                  ByteCount,
    UINT8                   *AmlPtr,
    UINT32                  LeadSpace);


ACPI_STATUS
AcpiExDumpOperand (
    ACPI_OPERAND_OBJECT     *EntryDesc);

void
AcpiExDumpOperands (
    ACPI_OPERAND_OBJECT     **Operands,
    OPERATING_MODE          InterpreterMode,
    NATIVE_CHAR             *Ident,
    UINT32                  NumLevels,
    NATIVE_CHAR             *Note,
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber);

void
AcpiExDumpObjectDescriptor (
    ACPI_OPERAND_OBJECT     *Object,
    UINT32                  Flags);


void
AcpiExDumpNode (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  Flags);


/*
 * amnames - interpreter/scanner name load/execute
 */

NATIVE_CHAR *
AcpiExAllocateNameString (
    UINT32                  PrefixCount,
    UINT32                  NumNameSegs);

UINT32
AcpiExGoodChar (
    UINT32                  Character);

ACPI_STATUS
AcpiExNameSegment (
    UINT8                   **InAmlAddress,
    NATIVE_CHAR             *NameString);

ACPI_STATUS
AcpiExGetNameString (
    ACPI_OBJECT_TYPE8       DataType,
    UINT8                   *InAmlAddress,
    NATIVE_CHAR             **OutNameString,
    UINT32                  *OutNameLength);

ACPI_STATUS
AcpiExDoName (
    ACPI_OBJECT_TYPE        DataType,
    OPERATING_MODE          LoadExecMode);


/*
 * amstore - Object store support
 */

ACPI_STATUS
AcpiExStore (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExStoreObjectToIndex (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExStoreObjectToNode (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExStoreObjectToObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState);


/*
 *
 */

ACPI_STATUS
AcpiExResolveObject (
    ACPI_OPERAND_OBJECT     **SourceDescPtr,
    ACPI_OBJECT_TYPE8       TargetType,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiExStoreObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OBJECT_TYPE8       TargetType,
    ACPI_OPERAND_OBJECT     **TargetDescPtr,
    ACPI_WALK_STATE         *WalkState);


/*
 * amcopy - object copy
 */

ACPI_STATUS
AcpiExCopyBufferToBuffer (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc);

ACPI_STATUS
AcpiExCopyStringToString (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc);

ACPI_STATUS
AcpiExCopyIntegerToIndexField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc);

ACPI_STATUS
AcpiExCopyIntegerToBankField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc);

ACPI_STATUS
AcpiExCopyDataToNamedField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_NAMESPACE_NODE     *Node);

ACPI_STATUS
AcpiExCopyIntegerToBufferField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc);

/*
 * amutils - interpreter/scanner utilities
 */

ACPI_STATUS
AcpiExEnterInterpreter (
    void);

void
AcpiExExitInterpreter (
    void);

void
AcpiExTruncateFor32bitTable (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

BOOLEAN
AcpiExValidateObjectType (
    ACPI_OBJECT_TYPE        Type);

BOOLEAN
AcpiExAcquireGlobalLock (
    UINT32                  Rule);

ACPI_STATUS
AcpiExReleaseGlobalLock (
    BOOLEAN                 Locked);

UINT32
AcpiExDigitsNeeded (
    ACPI_INTEGER            Value,
    UINT32                  Base);

ACPI_STATUS
AcpiExEisaIdToString (
    UINT32                  NumericId,
    NATIVE_CHAR             *OutString);

ACPI_STATUS
AcpiExUnsignedIntegerToString (
    ACPI_INTEGER            Value,
    NATIVE_CHAR             *OutString);


/*
 * amregion - default OpRegion handlers
 */

ACPI_STATUS
AcpiExSystemMemorySpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);

ACPI_STATUS
AcpiExSystemIoSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);

ACPI_STATUS
AcpiExPciConfigSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);

ACPI_STATUS
AcpiExEmbeddedControllerSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);

ACPI_STATUS
AcpiExSmBusSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);


#endif /* __INTERP_H__ */
