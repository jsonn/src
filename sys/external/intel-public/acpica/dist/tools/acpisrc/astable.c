
/******************************************************************************
 *
 * Module Name: astable - Tables used for source conversion
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


#include "acpisrc.h"
#include "acapps.h"


/******************************************************************************
 *
 * Standard/Common translation tables
 *
 ******************************************************************************/


ACPI_STRING_TABLE           StandardDataTypes[] = {

    /* Declarations first */

    {"UINT32      ",     "unsigned int",     REPLACE_SUBSTRINGS},
    {"UINT16        ",   "unsigned short",   REPLACE_SUBSTRINGS},
    {"UINT8        ",    "unsigned char",    REPLACE_SUBSTRINGS},
    {"BOOLEAN      ",    "unsigned char",    REPLACE_SUBSTRINGS},

    /* Now do embedded typecasts */

    {"UINT32",           "unsigned int",     REPLACE_SUBSTRINGS},
    {"UINT16",           "unsigned short",   REPLACE_SUBSTRINGS},
    {"UINT8",            "unsigned char",    REPLACE_SUBSTRINGS},
    {"BOOLEAN",          "unsigned char",    REPLACE_SUBSTRINGS},

    {"INT32  ",          "int    ",          REPLACE_SUBSTRINGS},
    {"INT32",            "int",              REPLACE_SUBSTRINGS},
    {"INT16",            "short",            REPLACE_SUBSTRINGS},
    {"INT8",             "char",             REPLACE_SUBSTRINGS},

    /* Put back anything we broke (such as anything with _INT32_ in it) */

    {"_int_",            "_INT32_",          REPLACE_SUBSTRINGS},
    {"_unsigned int_",   "_UINT32_",         REPLACE_SUBSTRINGS},
    {NULL,               NULL,               0}
};


/******************************************************************************
 *
 * Linux-specific translation tables
 *
 ******************************************************************************/

char                        LinuxHeader[] =
"/*\n"
" * Copyright (C) 2000 - 2010, Intel Corp.\n"
" * All rights reserved.\n"
" *\n"
" * Redistribution and use in source and binary forms, with or without\n"
" * modification, are permitted provided that the following conditions\n"
" * are met:\n"
" * 1. Redistributions of source code must retain the above copyright\n"
" *    notice, this list of conditions, and the following disclaimer,\n"
" *    without modification.\n"
" * 2. Redistributions in binary form must reproduce at minimum a disclaimer\n"
" *    substantially similar to the \"NO WARRANTY\" disclaimer below\n"
" *    (\"Disclaimer\") and any redistribution must be conditioned upon\n"
" *    including a substantially similar Disclaimer requirement for further\n"
" *    binary redistribution.\n"
" * 3. Neither the names of the above-listed copyright holders nor the names\n"
" *    of any contributors may be used to endorse or promote products derived\n"
" *    from this software without specific prior written permission.\n"
" *\n"
" * Alternatively, this software may be distributed under the terms of the\n"
" * GNU General Public License (\"GPL\") version 2 as published by the Free\n"
" * Software Foundation.\n"
" *\n"
" * NO WARRANTY\n"
" * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
" * \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
" * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR\n"
" * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
" * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
" * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n"
" * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n"
" * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,\n"
" * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING\n"
" * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n"
" * POSSIBILITY OF SUCH DAMAGES.\n"
" */\n";

ACPI_STRING_TABLE           LinuxDataTypes[] = {

/*
 * Extra space is added after the type so there is room to add "struct", "union",
 * etc. when the existing struct typedefs are eliminated.
 */

    /* Declarations first - ACPI types and standard C types */

    {"INT64       ",             "s64         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"UINT64      ",             "u64         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"UINT32      ",             "u32         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"INT32       ",             "s32         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"UINT16      ",             "u16         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"INT16       ",             "s16         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"UINT8       ",             "u8          ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"BOOLEAN     ",             "u8          ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"char        ",             "char        ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"void        ",             "void        ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"char *      ",             "char *      ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"void *      ",             "void *      ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"int         ",             "int         ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"FILE        ",             "FILE        ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},
    {"size_t      ",             "size_t      ",     REPLACE_WHOLE_WORD | EXTRA_INDENT_C},

    /* Now do embedded typecasts */

    {"UINT64",                   "u64",                      REPLACE_WHOLE_WORD},
    {"UINT32",                   "u32",                      REPLACE_WHOLE_WORD},
    {"UINT16",                   "u16",                      REPLACE_WHOLE_WORD},
    {"UINT8",                    "u8",                       REPLACE_WHOLE_WORD},
    {"BOOLEAN",                  "u8",                       REPLACE_WHOLE_WORD},

    {"INT64  ",                  "s64    ",                  REPLACE_WHOLE_WORD},
    {"INT64",                    "s64",                      REPLACE_WHOLE_WORD},
    {"INT32  ",                  "s32    ",                  REPLACE_WHOLE_WORD},
    {"INT32",                    "s32",                      REPLACE_WHOLE_WORD},
    {"INT16  ",                  "s16    ",                  REPLACE_WHOLE_WORD},
    {"INT8   ",                  "s8     ",                  REPLACE_WHOLE_WORD},
    {"INT16",                    "s16",                      REPLACE_WHOLE_WORD},
    {"INT8",                     "s8",                       REPLACE_WHOLE_WORD},

    {NULL,                       NULL,                       0},
};

ACPI_TYPED_IDENTIFIER_TABLE           AcpiIdentifiers[] = {

    {"ACPI_ADR_SPACE_HANDLER",              SRC_TYPE_SIMPLE},
    {"ACPI_ADR_SPACE_SETUP",                SRC_TYPE_SIMPLE},
    {"ACPI_ADR_SPACE_TYPE",                 SRC_TYPE_SIMPLE},
    {"ACPI_AML_OPERANDS",                   SRC_TYPE_UNION},
    {"ACPI_BIT_REGISTER_INFO",              SRC_TYPE_STRUCT},
    {"ACPI_BUFFER",                         SRC_TYPE_STRUCT},
    {"ACPI_BUS_ATTRIBUTE",                  SRC_TYPE_STRUCT},
    {"ACPI_CACHE_T",                        SRC_TYPE_SIMPLE},
    {"ACPI_COMMON_FACS",                    SRC_TYPE_STRUCT},
    {"ACPI_COMMON_STATE",                   SRC_TYPE_STRUCT},
    {"ACPI_COMMON_DESCRIPTOR",              SRC_TYPE_STRUCT},
    {"ACPI_COMPATIBLE_ID",                  SRC_TYPE_STRUCT},
    {"ACPI_CONTROL_STATE",                  SRC_TYPE_STRUCT},
    {"ACPI_CONVERSION_TABLE",               SRC_TYPE_STRUCT},
    {"ACPI_CPU_FLAGS",                      SRC_TYPE_SIMPLE},
    {"ACPI_CREATE_FIELD_INFO",              SRC_TYPE_STRUCT},
    {"ACPI_DB_METHOD_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_DEBUG_MEM_BLOCK",                SRC_TYPE_STRUCT},
    {"ACPI_DEBUG_MEM_HEADER",               SRC_TYPE_STRUCT},
    {"ACPI_DEBUG_PRINT_INFO",               SRC_TYPE_STRUCT},
    {"ACPI_DESCRIPTOR",                     SRC_TYPE_UNION},
    {"ACPI_DEVICE_ID",                      SRC_TYPE_STRUCT},
    {"ACPI_DEVICE_ID_LIST",                 SRC_TYPE_STRUCT},
    {"ACPI_DEVICE_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_DEVICE_WALK_INFO",               SRC_TYPE_STRUCT},
    {"ACPI_DMTABLE_DATA",                   SRC_TYPE_STRUCT},
    {"ACPI_DMTABLE_INFO",                   SRC_TYPE_STRUCT},
    {"ACPI_DMTABLE_HANDLER",                SRC_TYPE_SIMPLE},
    {"ACPI_EVALUATE_INFO",                  SRC_TYPE_STRUCT},
    {"ACPI_EVENT_HANDLER",                  SRC_TYPE_SIMPLE},
    {"ACPI_EVENT_STATUS",                   SRC_TYPE_SIMPLE},
    {"ACPI_EVENT_TYPE",                     SRC_TYPE_SIMPLE},
    {"ACPI_EXCEPTION_HANDLER",              SRC_TYPE_SIMPLE},
    {"ACPI_EXDUMP_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_EXECUTE_TYPE",                   SRC_TYPE_SIMPLE},
    {"ACPI_EXECUTE_WALK",                   SRC_TYPE_STRUCT},
    {"ACPI_EXTERNAL_LIST",                  SRC_TYPE_STRUCT},
    {"ACPI_FADT_INFO",                      SRC_TYPE_STRUCT},
    {"ACPI_FADT_PM_INFO",                   SRC_TYPE_STRUCT},
    {"ACPI_FIELD_INFO",                     SRC_TYPE_STRUCT},
    {"ACPI_FIND_CONTEXT",                   SRC_TYPE_STRUCT},
    {"ACPI_FIXED_EVENT_HANDLER",            SRC_TYPE_STRUCT},
    {"ACPI_FIXED_EVENT_INFO",               SRC_TYPE_STRUCT},
    {"ACPI_GENERIC_ADDRESS",                SRC_TYPE_STRUCT},
    {"ACPI_GENERIC_STATE",                  SRC_TYPE_UNION},
    {"ACPI_GET_DEVICES_INFO",               SRC_TYPE_STRUCT},
    {"ACPI_GPE_BLOCK_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_GPE_CALLBACK",                   SRC_TYPE_SIMPLE},
    {"ACPI_GPE_DEVICE_INFO",                SRC_TYPE_STRUCT},
    {"ACPI_GPE_EVENT_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_GPE_HANDLER",                    SRC_TYPE_SIMPLE},
    {"ACPI_GPE_INDEX_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_GPE_REGISTER_INFO",              SRC_TYPE_STRUCT},
    {"ACPI_GPE_WALK_INFO",                  SRC_TYPE_STRUCT},
    {"ACPI_GPE_XRUPT_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_HANDLE",                         SRC_TYPE_SIMPLE},
    {"ACPI_HANDLER_INFO",                   SRC_TYPE_STRUCT},
    {"ACPI_INIT_HANDLER",                   SRC_TYPE_SIMPLE},
    {"ACPI_IDENTIFIER_TABLE",               SRC_TYPE_STRUCT},
    {"ACPI_INIT_WALK_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_INTEGER",                        SRC_TYPE_SIMPLE},
    {"ACPI_INTEGER_OVERLAY",                SRC_TYPE_STRUCT},
    {"ACPI_INTEGRITY_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_INTERFACE_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_INTERPRETER_MODE",               SRC_TYPE_SIMPLE},
    {"ACPI_IO_ADDRESS",                     SRC_TYPE_SIMPLE},
    {"ACPI_IO_ATTRIBUTE",                   SRC_TYPE_STRUCT},
    {"ACPI_MEM_SPACE_CONTEXT",              SRC_TYPE_STRUCT},
    {"ACPI_MEMORY_ATTRIBUTE",               SRC_TYPE_STRUCT},
    {"ACPI_MEMORY_LIST",                    SRC_TYPE_STRUCT},
    {"ACPI_MUTEX",                          SRC_TYPE_SIMPLE},
    {"ACPI_MUTEX_HANDLE",                   SRC_TYPE_SIMPLE},
    {"ACPI_MUTEX_INFO",                     SRC_TYPE_STRUCT},
    {"ACPI_NAME",                           SRC_TYPE_SIMPLE},
    {"ACPI_NAME_INFO",                      SRC_TYPE_STRUCT},
    {"ACPI_NAME_UNION",                     SRC_TYPE_UNION},
    {"ACPI_NAMESPACE_NODE",                 SRC_TYPE_STRUCT},
    {"ACPI_NAMESTRING_INFO",                SRC_TYPE_STRUCT},
    {"ACPI_NATIVE_INT",                     SRC_TYPE_SIMPLE},
    {"ACPI_NATIVE_UINT",                    SRC_TYPE_SIMPLE},
    {"ACPI_NOTIFY_HANDLER",                 SRC_TYPE_SIMPLE},
    {"ACPI_NOTIFY_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_NS_SEARCH_DATA",                 SRC_TYPE_STRUCT},
    {"ACPI_OBJ_INFO_HEADER",                SRC_TYPE_STRUCT},
    {"ACPI_OBJECT",                         SRC_TYPE_UNION},
    {"ACPI_OBJECT_ADDR_HANDLER",            SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_BANK_FIELD",              SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_BUFFER",                  SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_BUFFER_FIELD",            SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_CACHE_LIST",              SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_COMMON",                  SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_DATA",                    SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_DEVICE",                  SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_EVENT",                   SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_EXTRA",                   SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_FIELD_COMMON",            SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_HANDLER",                 SRC_TYPE_SIMPLE},
    {"ACPI_OBJECT_INDEX_FIELD",             SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_INTEGER",                 SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_LIST",                    SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_METHOD",                  SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_MUTEX",                   SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_NOTIFY_COMMON",           SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_NOTIFY_HANDLER",          SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_PACKAGE",                 SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_POWER_RESOURCE",          SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_PROCESSOR",               SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_REFERENCE",               SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_REGION",                  SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_REGION_FIELD",            SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_STRING",                  SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_THERMAL_ZONE",            SRC_TYPE_STRUCT},
    {"ACPI_OBJECT_TYPE",                    SRC_TYPE_SIMPLE},
    {"ACPI_OBJECT_TYPE8",                   SRC_TYPE_SIMPLE},
    {"ACPI_OP_WALK_INFO",                   SRC_TYPE_STRUCT},
    {"ACPI_OPCODE_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_OPERAND_OBJECT",                 SRC_TYPE_UNION},
    {"ACPI_OSD_HANDLER",                    SRC_TYPE_SIMPLE},
    {"ACPI_OSD_EXEC_CALLBACK",              SRC_TYPE_SIMPLE},
    {"ACPI_OWNER_ID",                       SRC_TYPE_SIMPLE},
    {"ACPI_PACKAGE_INFO",                   SRC_TYPE_STRUCT},
    {"ACPI_PACKAGE_INFO2",                  SRC_TYPE_STRUCT},
    {"ACPI_PACKAGE_INFO3",                  SRC_TYPE_STRUCT},
    {"ACPI_PARSE_DOWNWARDS",                SRC_TYPE_SIMPLE},
    {"ACPI_PARSE_OBJ_ASL",                  SRC_TYPE_STRUCT},
    {"ACPI_PARSE_OBJ_COMMON",               SRC_TYPE_STRUCT},
    {"ACPI_PARSE_OBJ_NAMED",                SRC_TYPE_STRUCT},
    {"ACPI_PARSE_OBJECT",                   SRC_TYPE_UNION},
    {"ACPI_PARSE_STATE",                    SRC_TYPE_STRUCT},
    {"ACPI_PARSE_UPWARDS",                  SRC_TYPE_SIMPLE},
    {"ACPI_PARSE_VALUE",                    SRC_TYPE_UNION},
    {"ACPI_PCI_ID",                         SRC_TYPE_STRUCT},
    {"ACPI_PCI_ROUTING_TABLE",              SRC_TYPE_STRUCT},
    {"ACPI_PHYSICAL_ADDRESS",               SRC_TYPE_SIMPLE},
    {"ACPI_PKG_CALLBACK",                   SRC_TYPE_SIMPLE},
    {"ACPI_PKG_INFO",                       SRC_TYPE_STRUCT},
    {"ACPI_PKG_STATE",                      SRC_TYPE_STRUCT},
    {"ACPI_POINTER",                        SRC_TYPE_STRUCT},
    {"ACPI_POINTERS",                       SRC_TYPE_UNION},
    {"ACPI_PORT_INFO",                      SRC_TYPE_STRUCT},
    {"ACPI_PREDEFINED_DATA",                SRC_TYPE_STRUCT},
    {"ACPI_PREDEFINED_INFO",                SRC_TYPE_UNION},
    {"ACPI_PREDEFINED_NAMES",               SRC_TYPE_STRUCT},
    {"ACPI_PSCOPE_STATE",                   SRC_TYPE_STRUCT},
    {"ACPI_REPAIR_FUNCTION",                SRC_TYPE_SIMPLE},
    {"ACPI_REPAIR_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE",                       SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_ADDRESS",               SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_ADDRESS16",             SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_ADDRESS32",             SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_ADDRESS64",             SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_EXTENDED_ADDRESS64",    SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_ATTRIBUTE",             SRC_TYPE_UNION},
    {"ACPI_RESOURCE_DATA",                  SRC_TYPE_UNION},
    {"ACPI_RESOURCE_DMA",                   SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_END_TAG",               SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_EXTENDED_IRQ",          SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_FIXED_IO",              SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_FIXED_MEMORY32",        SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_GENERIC_REGISTER",      SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_INFO",                  SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_IO",                    SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_IRQ",                   SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_MEMORY24",              SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_MEMORY32",              SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_SOURCE",                SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_START_DEPENDENT",       SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_TAG",                   SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_TYPE",                  SRC_TYPE_SIMPLE},
    {"ACPI_RESOURCE_VENDOR",                SRC_TYPE_STRUCT},
    {"ACPI_RESOURCE_VENDOR_TYPED",          SRC_TYPE_STRUCT},
    {"ACPI_RESULT_VALUES",                  SRC_TYPE_STRUCT},
    {"ACPI_ROUND_UP_TO_32_BIT",             SRC_TYPE_SIMPLE},
    {"ACPI_RSCONVERT_INFO",                 SRC_TYPE_STRUCT},
    {"ACPI_RSDUMP_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_RW_LOCK",                        SRC_TYPE_STRUCT},
    {"ACPI_SCOPE_STATE",                    SRC_TYPE_STRUCT},
    {"ACPI_SEMAPHORE",                      SRC_TYPE_SIMPLE},
    {"ACPI_SIGNAL_FATAL_INFO",              SRC_TYPE_STRUCT},
    {"ACPI_SIZE",                           SRC_TYPE_SIMPLE},
    {"ACPI_SPINLOCK",                       SRC_TYPE_SIMPLE},
    {"ACPI_STATISTICS",                     SRC_TYPE_STRUCT},
    {"ACPI_STATUS",                         SRC_TYPE_SIMPLE},
    {"ACPI_STRING",                         SRC_TYPE_SIMPLE},
    {"ACPI_STRING_TABLE",                   SRC_TYPE_STRUCT},
    {"ACPI_SUBTABLE_HEADER",                SRC_TYPE_STRUCT},
    {"ACPI_SYSTEM_INFO",                    SRC_TYPE_STRUCT},
    {"ACPI_TABLE_DESC",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_HANDLER",                  SRC_TYPE_SIMPLE},
    {"ACPI_TABLE_HEADER",                   SRC_TYPE_STRUCT},
    {"ACPI_TABLE_INFO",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_LIST",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_SUPPORT",                  SRC_TYPE_STRUCT},
    {"ACPI_TABLE_TYPE",                     SRC_TYPE_SIMPLE},
    {"ACPI_THREAD_ID",                      SRC_TYPE_SIMPLE},
    {"ACPI_THREAD_STATE",                   SRC_TYPE_STRUCT},
    {"ACPI_TYPED_IDENTIFIER_TABLE",         SRC_TYPE_STRUCT},
    {"ACPI_UINTPTR_T",                      SRC_TYPE_SIMPLE},
    {"ACPI_UPDATE_STATE",                   SRC_TYPE_STRUCT},
    {"ACPI_UUID",                           SRC_TYPE_STRUCT},
    {"ACPI_VENDOR_UUID",                    SRC_TYPE_STRUCT},
    {"ACPI_VENDOR_WALK_INFO",               SRC_TYPE_STRUCT},
    {"ACPI_WALK_AML_CALLBACK",              SRC_TYPE_SIMPLE},
    {"ACPI_WALK_CALLBACK",                  SRC_TYPE_SIMPLE},
    {"ACPI_WALK_RESOURCE_CALLBACK",         SRC_TYPE_SIMPLE},
    {"ACPI_WALK_INFO",                      SRC_TYPE_STRUCT},
    {"ACPI_WALK_STATE",                     SRC_TYPE_STRUCT},
    {"ACPI_WHEA_HEADER",                    SRC_TYPE_STRUCT},

    {"ACPI_RS_LENGTH",                      SRC_TYPE_SIMPLE},
    {"ACPI_RSDESC_SIZE",                    SRC_TYPE_SIMPLE},

    {"AML_RESOURCE",                        SRC_TYPE_UNION},
    {"AML_RESOURCE_ADDRESS",                SRC_TYPE_STRUCT},
    {"AML_RESOURCE_ADDRESS16",              SRC_TYPE_STRUCT},
    {"AML_RESOURCE_ADDRESS32",              SRC_TYPE_STRUCT},
    {"AML_RESOURCE_ADDRESS64",              SRC_TYPE_STRUCT},
    {"AML_RESOURCE_DMA",                    SRC_TYPE_STRUCT},
    {"AML_RESOURCE_END_DEPENDENT",          SRC_TYPE_STRUCT},
    {"AML_RESOURCE_END_TAG",                SRC_TYPE_STRUCT},
    {"AML_RESOURCE_EXTENDED_ADDRESS64",     SRC_TYPE_STRUCT},
    {"AML_RESOURCE_EXTENDED_IRQ",           SRC_TYPE_STRUCT},
    {"AML_RESOURCE_FIXED_IO",               SRC_TYPE_STRUCT},
    {"AML_RESOURCE_FIXED_MEMORY32",         SRC_TYPE_STRUCT},
    {"AML_RESOURCE_GENERIC_REGISTER",       SRC_TYPE_STRUCT},
    {"AML_RESOURCE_IO",                     SRC_TYPE_STRUCT},
    {"AML_RESOURCE_IRQ",                    SRC_TYPE_STRUCT},
    {"AML_RESOURCE_IRQ_NOFLAGS",            SRC_TYPE_STRUCT},
    {"AML_RESOURCE_LARGE_HEADER",           SRC_TYPE_STRUCT},
    {"AML_RESOURCE_MEMORY24",               SRC_TYPE_STRUCT},
    {"AML_RESOURCE_MEMORY32",               SRC_TYPE_STRUCT},
    {"AML_RESOURCE_SMALL_HEADER",           SRC_TYPE_STRUCT},
    {"AML_RESOURCE_START_DEPENDENT",        SRC_TYPE_STRUCT},
    {"AML_RESOURCE_START_DEPENDENT_NOPRIO", SRC_TYPE_STRUCT},
    {"AML_RESOURCE_VENDOR_LARGE",           SRC_TYPE_STRUCT},
    {"AML_RESOURCE_VENDOR_SMALL",           SRC_TYPE_STRUCT},

    {"APIC_HEADER",                         SRC_TYPE_STRUCT},
    {"ARGUMENT_INFO",                       SRC_TYPE_STRUCT},
    {"AE_DEBUG_REGIONS",                    SRC_TYPE_STRUCT},
    {"AE_REGION",                           SRC_TYPE_STRUCT},
    {"ASL_ANALYSIS_WALK_INFO",              SRC_TYPE_STRUCT},
    {"ASL_ERROR_MSG",                       SRC_TYPE_STRUCT},
    {"ASL_EVENT_INFO",                      SRC_TYPE_STRUCT},
    {"ASL_FILE_INFO",                       SRC_TYPE_STRUCT},
    {"ASL_FILE_STATUS",                     SRC_TYPE_STRUCT},
    {"ASL_LISTING_NODE",                    SRC_TYPE_STRUCT},
    {"ASL_MAPPING_ENTRY",                   SRC_TYPE_STRUCT},
    {"ASL_METHOD_INFO",                     SRC_TYPE_STRUCT},
    {"ASL_RESERVED_INFO",                   SRC_TYPE_STRUCT},
    {"ASL_RESOURCE_NODE",                   SRC_TYPE_STRUCT},
    {"ASL_WALK_CALLBACK",                   SRC_TYPE_SIMPLE},
    {"COMMAND_INFO",                        SRC_TYPE_STRUCT},
    {"UINT64_OVERLAY",                      SRC_TYPE_UNION},
    {"UINT64_STRUCT",                       SRC_TYPE_STRUCT},

    /*
     * Acpi table definition names.
     */
    {"ACPI_TABLE_ASF",                      SRC_TYPE_STRUCT},
    {"ACPI_TABLE_BERT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_BOOT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_CPEP",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_DBGP",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_DMAR",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_ECDT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_EINJ",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_ERST",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_FACS",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_FADT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_HEST",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_HPET",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_IBFT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_IVRS",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_MADT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_MCFG",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_MCHI",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_MSCT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_RSDP",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_RSDT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_SBST",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_SLIT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_SPCR",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_SPMI",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_SRAT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_TCPA",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_UEFI",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_WAET",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_WDAT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_WDRT",                     SRC_TYPE_STRUCT},
    {"ACPI_TABLE_XSDT",                     SRC_TYPE_STRUCT},

    {"ACPI_ASF_ADDRESS",                    SRC_TYPE_STRUCT},
    {"ACPI_ASF_ALERT",                      SRC_TYPE_STRUCT},
    {"ACPI_ASF_ALERT_DATA",                 SRC_TYPE_STRUCT},
    {"ACPI_ASF_CONTROL_DATA",               SRC_TYPE_STRUCT},
    {"ACPI_ASF_HEADER",                     SRC_TYPE_STRUCT},
    {"ACPI_ASF_INFO",                       SRC_TYPE_STRUCT},
    {"ACPI_ASF_REMOTE",                     SRC_TYPE_STRUCT},
    {"ACPI_ASF_RMCP",                       SRC_TYPE_STRUCT},
    {"ACPI_BERT_REGION",                    SRC_TYPE_STRUCT},
    {"ACPI_CPEP_POLLING",                   SRC_TYPE_STRUCT},
    {"ACPI_DMAR_HEADER",                    SRC_TYPE_STRUCT},
    {"ACPI_DMAR_DEVICE_SCOPE",              SRC_TYPE_STRUCT},
    {"ACPI_DMAR_ATSR",                      SRC_TYPE_STRUCT},
    {"ACPI_DMAR_RHSA",                      SRC_TYPE_STRUCT},
    {"ACPI_DMAR_HARDWARE_UNIT",             SRC_TYPE_STRUCT},
    {"ACPI_DMAR_RESERVED_MEMORY",           SRC_TYPE_STRUCT},
    {"ACPI_EINJ_ENTRY",                     SRC_TYPE_STRUCT},
    {"ACPI_EINJ_TRIGGER",                   SRC_TYPE_STRUCT},
    {"ACPI_ERST_ENTRY",                     SRC_TYPE_STRUCT},
    {"ACPI_ERST_INFO",                      SRC_TYPE_STRUCT},
    {"ACPI_HEST_AER_COMMON",                SRC_TYPE_STRUCT},
    {"ACPI_HEST_HEADER",                    SRC_TYPE_STRUCT},
    {"ACPI_HEST_NOTIFY",                    SRC_TYPE_STRUCT},
    {"ACPI_HEST_IA_ERROR_BANK",             SRC_TYPE_STRUCT},
    {"ACPI_HEST_IA_MACHINE_CHECK",          SRC_TYPE_STRUCT},
    {"ACPI_HEST_IA_CORRECTED",              SRC_TYPE_STRUCT},
    {"ACPI_HEST_IA_NMI",                    SRC_TYPE_STRUCT},
    {"ACPI_HEST_AER_ROOT",                  SRC_TYPE_STRUCT},
    {"ACPI_HEST_AER",                       SRC_TYPE_STRUCT},
    {"ACPI_HEST_AER_BRIDGE",                SRC_TYPE_STRUCT},
    {"ACPI_HEST_GENERIC",                   SRC_TYPE_STRUCT},
    {"ACPI_HEST_GENERIC_STATUS",            SRC_TYPE_STRUCT},
    {"ACPI_HEST_GENERIC_DATA",              SRC_TYPE_STRUCT},
    {"ACPI_IBFT_HEADER",                    SRC_TYPE_STRUCT},
    {"ACPI_IBFT_CONTROL",                   SRC_TYPE_STRUCT},
    {"ACPI_IBFT_INITIATOR",                 SRC_TYPE_STRUCT},
    {"ACPI_IBFT_NIC",                       SRC_TYPE_STRUCT},
    {"ACPI_IBFT_TARGET",                    SRC_TYPE_STRUCT},
    {"ACPI_IVRS_HEADER",                    SRC_TYPE_STRUCT},
    {"ACPI_IVRS_HARDWARE",                  SRC_TYPE_STRUCT},
    {"ACPI_IVRS_DE_HEADER",                 SRC_TYPE_STRUCT},
    {"ACPI_IVRS_DEVICE4",                   SRC_TYPE_STRUCT},
    {"ACPI_IVRS_DEVICE8A",                  SRC_TYPE_STRUCT},
    {"ACPI_IVRS_DEVICE8B",                  SRC_TYPE_STRUCT},
    {"ACPI_IVRS_DEVICE8C",                  SRC_TYPE_STRUCT},
    {"ACPI_IVRS_MEMORY",                    SRC_TYPE_STRUCT},
    {"ACPI_MADT_ADDRESS_OVERRIDE",          SRC_TYPE_STRUCT},
    {"ACPI_MADT_HEADER",                    SRC_TYPE_STRUCT},
    {"ACPI_MADT_IO_APIC",                   SRC_TYPE_STRUCT},
    {"ACPI_MADT_IO_SAPIC",                  SRC_TYPE_STRUCT},
    {"ACPI_MADT_LOCAL_APIC",                SRC_TYPE_STRUCT},
    {"ACPI_MADT_LOCAL_APIC_NMI",            SRC_TYPE_STRUCT},
    {"ACPI_MADT_LOCAL_APIC_OVERRIDE",       SRC_TYPE_STRUCT},
    {"ACPI_MADT_LOCAL_SAPIC",               SRC_TYPE_STRUCT},
    {"ACPI_MADT_LOCAL_X2APIC",              SRC_TYPE_STRUCT},
    {"ACPI_MADT_LOCAL_X2APIC_NMI",          SRC_TYPE_STRUCT},
    {"ACPI_MADT_INTERRUPT_OVERRIDE",        SRC_TYPE_STRUCT},
    {"ACPI_MADT_INTERRUPT_SOURCE",          SRC_TYPE_STRUCT},
    {"ACPI_MADT_NMI_SOURCE",                SRC_TYPE_STRUCT},
    {"ACPI_MADT_PROCESSOR_APIC",            SRC_TYPE_STRUCT},
    {"ACPI_MCFG_ALLOCATION",                SRC_TYPE_STRUCT},
    {"ACPI_MSCT_PROXIMITY",                 SRC_TYPE_STRUCT},
    {"ACPI_SRAT_CPU_AFFINITY",              SRC_TYPE_STRUCT},
    {"ACPI_SRAT_HEADER",                    SRC_TYPE_STRUCT},
    {"ACPI_SRAT_MEM_AFFINITY",              SRC_TYPE_STRUCT},
    {"ACPI_SRAT_X2APIC_CPU_AFFINITY",       SRC_TYPE_STRUCT},
    {"ACPI_WDAT_ENTRY",                     SRC_TYPE_STRUCT},


    /* Legacy names, should be removed  */

    {"EC_BOOT_RESOURCES",                   SRC_TYPE_STRUCT},
    {"HPET_TABLE",                          SRC_TYPE_STRUCT},
    {"MADT_ADDRESS_OVERRIDE",               SRC_TYPE_STRUCT},
    {"MADT_INTERRUPT_OVERRIDE",             SRC_TYPE_STRUCT},
    {"MADT_INTERRUPT_SOURCE",               SRC_TYPE_STRUCT},
    {"MADT_IO_APIC",                        SRC_TYPE_STRUCT},
    {"MADT_IO_SAPIC",                       SRC_TYPE_STRUCT},
    {"MADT_LOCAL_APIC_NMI",                 SRC_TYPE_STRUCT},
    {"MADT_LOCAL_SAPIC",                    SRC_TYPE_STRUCT},
    {"MADT_NMI_SOURCE",                     SRC_TYPE_STRUCT},
    {"MADT_PROCESSOR_APIC",                 SRC_TYPE_STRUCT},
    {"MEMORY_AFFINITY",                     SRC_TYPE_STRUCT},
    {"MULTIPLE_APIC_TABLE",                 SRC_TYPE_STRUCT},
    {"SMART_BATTERY_TABLE",                 SRC_TYPE_STRUCT},
    {"STATIC_RESOURCE_ALLOC",               SRC_TYPE_STRUCT},
    {"SYSTEM_RESOURCE_AFFINITY",            SRC_TYPE_STRUCT},
    {"SYSTEM_LOCALITY_INFO",                SRC_TYPE_STRUCT},
    {"FACS_DESCRIPTOR",                     SRC_TYPE_STRUCT},
    {"FADT_DESCRIPTOR_REV1",                SRC_TYPE_STRUCT},
    {"FADT_DESCRIPTOR",                     SRC_TYPE_STRUCT},
    {"FADT_DESCRIPTOR_REV2_MINUS",          SRC_TYPE_STRUCT},
    {"RSDP_DESCRIPTOR",                     SRC_TYPE_STRUCT},
    {"RSDT_DESCRIPTOR",                     SRC_TYPE_STRUCT},
    {"XSDT_DESCRIPTOR",                     SRC_TYPE_STRUCT},
    {NULL, 0}
};


ACPI_IDENTIFIER_TABLE       LinuxAddStruct[] = {
    {"acpi_namespace_node"},
    {"acpi_parse_object"},
    {"acpi_table_desc"},
    {"acpi_walk_state"},
    {NULL}
};


ACPI_IDENTIFIER_TABLE       LinuxEliminateMacros[] = {

    {"ACPI_GET_ADDRESS"},
    {"ACPI_VALID_ADDRESS"},
    {NULL}
};


ACPI_IDENTIFIER_TABLE       LinuxEliminateLines_C[] = {

    {"#define __"},
    {NULL}
};


ACPI_IDENTIFIER_TABLE       LinuxEliminateLines_H[] = {

    {NULL}
};


ACPI_IDENTIFIER_TABLE       LinuxConditionalIdentifiers[] = {

/*    {"ACPI_USE_STANDARD_HEADERS"}, */
    {"WIN32"},
    {"_MSC_VER"},
    {NULL}
};

ACPI_CONVERSION_TABLE       LinuxConversionTable = {

    LinuxHeader,
    FLG_NO_CARRIAGE_RETURNS | FLG_LOWERCASE_DIRNAMES,

    AcpiIdentifiers,

    /* C source files */

    LinuxDataTypes,
    LinuxEliminateLines_C,
    NULL,
    LinuxEliminateMacros,
    AcpiIdentifiers,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_CHECK_BRACES | CVT_TRIM_LINES | CVT_BRACES_ON_SAME_LINE |
     CVT_MIXED_CASE_TO_UNDERSCORES | CVT_LOWER_CASE_IDENTIFIERS |
     CVT_REMOVE_DEBUG_MACROS | CVT_TRIM_WHITESPACE |
     CVT_REMOVE_EMPTY_BLOCKS | CVT_SPACES_TO_TABS8),

    /* C header files */

    LinuxDataTypes,
    LinuxEliminateLines_H,
    LinuxConditionalIdentifiers,
    NULL,
    AcpiIdentifiers,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_TRIM_LINES | CVT_MIXED_CASE_TO_UNDERSCORES |
     CVT_LOWER_CASE_IDENTIFIERS | CVT_TRIM_WHITESPACE |
     CVT_REMOVE_EMPTY_BLOCKS| CVT_REDUCE_TYPEDEFS | CVT_SPACES_TO_TABS8),
};


/******************************************************************************
 *
 * Code cleanup translation tables
 *
 ******************************************************************************/


ACPI_CONVERSION_TABLE       CleanupConversionTable = {

    NULL,
    FLG_DEFAULT_FLAGS,
    NULL,
    /* C source files */

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_CHECK_BRACES | CVT_TRIM_LINES | CVT_TRIM_WHITESPACE),

    /* C header files */

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_TRIM_LINES | CVT_TRIM_WHITESPACE),
};


ACPI_CONVERSION_TABLE       StatsConversionTable = {

    NULL,
    FLG_NO_FILE_OUTPUT,
    NULL,

    /* C source files */

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_COUNT_SHORTMULTILINE_COMMENTS),

    /* C header files */

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_COUNT_SHORTMULTILINE_COMMENTS),
};


/******************************************************************************
 *
 * Customizable translation tables
 *
 ******************************************************************************/

ACPI_STRING_TABLE           CustomReplacements[] = {


    {"ACPI_INTEGER_MAX", "ACPI_UINT64_MAX",   REPLACE_WHOLE_WORD},

#if 0
    {"(ACPI_INTEGER)", "(UINT64)",   REPLACE_WHOLE_WORD},
    {"ACPI_INTEGER        ", "UINT64              ",   REPLACE_WHOLE_WORD},
    {"ACPI_INTEGER", "UINT64",   REPLACE_WHOLE_WORD},
    {"(c) 1999 - 2009",      "(c) 1999 - 2010",         REPLACE_WHOLE_WORD}, /* Main ACPICA source */
    {"(c) 2006 - 2009",      "(c) 2006 - 2010",         REPLACE_WHOLE_WORD}, /* Test suites */
    {"#include \"acpi.h\"",   "#include \"acpi.h\"\n#include \"accommon.h\"",  REPLACE_SUBSTRINGS},
    {"AcpiTbSumTable", "AcpiTbSumTable",  REPLACE_WHOLE_WORD},
    {"ACPI_SIG_BOOT", "ACPI_SIG_BOOT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_DBGP", "ACPI_SIG_DBGP",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_DSDT", "ACPI_SIG_DSDT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_ECDT", "ACPI_SIG_ECDT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_FACS", "ACPI_SIG_FACS",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_FADT", "ACPI_SIG_FADT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_HPET", "ACPI_SIG_HPET",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_MADT", "ACPI_SIG_MADT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_MCFG", "ACPI_SIG_MCFG",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_PSDT", "ACPI_SIG_PSDT",   REPLACE_WHOLE_WORD},
    {"ACPI_NAME_RSDP", "ACPI_NAME_RSDP",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_RSDP", "ACPI_SIG_RSDP",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_RSDT", "ACPI_SIG_RSDT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SBST", "ACPI_SIG_SBST",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SLIT", "ACPI_SIG_SLIT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SPCR", "ACPI_SIG_SPCR",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SPIC", "ACPI_SIG_SPIC",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SPMI", "ACPI_SIG_SPMI",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SRAT", "ACPI_SIG_SRAT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_SSDT", "ACPI_SIG_SSDT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_TCPA", "ACPI_SIG_TCPA",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_WDRT", "ACPI_SIG_WDRT",   REPLACE_WHOLE_WORD},
    {"ACPI_SIG_XSDT", "ACPI_SIG_XSDT",   REPLACE_WHOLE_WORD},

    {"ACPI_ALLOCATE_ZEROED",    "ACPI_ALLOCATE_ZEROED",   REPLACE_WHOLE_WORD},
    {"ACPI_ALLOCATE",           "ACPI_ALLOCATE",          REPLACE_WHOLE_WORD},
    {"ACPI_FREE",               "ACPI_FREE",              REPLACE_WHOLE_WORD},

    "ACPI_NATIVE_UINT",     "ACPI_NATIVE_UINT",         REPLACE_WHOLE_WORD,
    "ACPI_NATIVE_UINT *",   "ACPI_NATIVE_UINT *",       REPLACE_WHOLE_WORD,
    "ACPI_NATIVE_UINT",     "ACPI_NATIVE_UINT",         REPLACE_WHOLE_WORD,
    "ACPI_NATIVE_INT",      "ACPI_NATIVE_INT",          REPLACE_WHOLE_WORD,
    "ACPI_NATIVE_INT *",    "ACPI_NATIVE_INT *",        REPLACE_WHOLE_WORD,
    "ACPI_NATIVE_INT",      "ACPI_NATIVE_INT",          REPLACE_WHOLE_WORD,
#endif

    {NULL,                    NULL, 0}
};


ACPI_CONVERSION_TABLE       CustomConversionTable = {

    NULL,
    FLG_DEFAULT_FLAGS,
    NULL,

    /* C source files */

    CustomReplacements,
    LinuxEliminateLines_H,
    NULL,
    NULL,
    NULL,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_TRIM_LINES | CVT_TRIM_WHITESPACE),

    /* C header files */

    CustomReplacements,
    LinuxEliminateLines_H,
    NULL,
    NULL,
    NULL,
    (CVT_COUNT_TABS | CVT_COUNT_NON_ANSI_COMMENTS | CVT_COUNT_LINES |
     CVT_TRIM_LINES | CVT_TRIM_WHITESPACE),
};

