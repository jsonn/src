/* NetBSD/arm32 version.
   Copyright (C) 1993, 1994, 1997, 1998 Free Software Foundation, Inc.
   Contributed by Mark Brinicombe (amb@physig.ph.kcl.ac.uk)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Run-time Target Specification.  */
#define TARGET_VERSION fputs (" (ARM/NetBSD)", stderr);

/* This is used in ASM_FILE_START.  */
#define ARM_OS_NAME "NetBSD"

/* Unsigned chars produces much better code than signed.  */
#define DEFAULT_SIGNED_CHAR  0

/* Since we always use GAS as our assembler we support stabs.  */
#define DBX_DEBUGGING_INFO 1

/*#undef ASM_DECLARE_FUNCTION_NAME*/

/* ARM6 family default cpu.  */
#define SUBTARGET_CPU_DEFAULT TARGET_CPU_arm6

/* Default is to use APCS-32 mode.  */
#define TARGET_DEFAULT (ARM_FLAG_APCS_32 | ARM_FLAG_SOFT_FLOAT)

#include "arm/aout.h"

/* This gets redefined in config/netbsd.h.  */
#undef TARGET_MEM_FUNCTIONS

#include <netbsd.h>

/* NetBSD uses gas so we want to use .ident */
#undef ASM_OUTPUT_IDENT
#define ASM_OUTPUT_IDENT(STREAM,STRING) \
  fprintf (STREAM,"\t.ident \"%s\"\n",STRING)

/* On the ARM `@' introduces a comment, so we must use something else
   for .type directives.  Most NetBSD platforms use %, but we use #
   because of some legacy assemblers.  */
#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT "#%s"

/* Until they use ELF or something that handles dwarf2 unwinds
   and initialization stuff better.  */
#undef DWARF2_UNWIND_INFO

/* Some defines for CPP.  arm32 is the NetBSD port name, so we always (only)
   define __arm32__ and __NetBSD__, and add __KPRINTF_ATTRIBUTE__ since
   this compiler is hacked with the NetBSD kprintf attribute mods. */
#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-D__arm32__ -D__arm__ -D__NetBSD__ -D__KPRINTF_ATTRIBUTE__ \
-Asystem(unix) -Asystem(NetBSD) -Acpu(arm) -Amachine(arm)"

/* Because TARGET_DEFAULT sets ARM_FLAG_APCS_32 */
#undef CPP_APCS_PC_DEFAULT_SPEC
#define CPP_APCS_PC_DEFAULT_SPEC "-D__APCS_32__"

/* Because TARGET_DEFAULT sets ARM_FLAG_SOFT_FLOAT */
#undef CPP_FLOAT_DEFAULT_SPEC
#define CPP_FLOAT_DEFAULT_SPEC "-D__SOFTFP__"

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_UNSIGNED
#define WCHAR_UNSIGNED 0

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef MAX_WCHAR_TYPE_SIZE
#define MAX_WCHAR_TYPE_SIZE 16

#define HANDLE_SYSV_PRAGMA

/* We don't have any limit on the length as out debugger is GDB.  */
#undef DBX_CONTIN_LENGTH

/* NetBSD does its profiling differently to the Acorn compiler. We
   don't need a word following the mcount call; and to skip it
   requires either an assembly stub or use of fomit-frame-pointer when
   compiling the profiling functions.  Since we break Acorn CC
   compatibility below a little more won't hurt.  */

#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(STREAM,LABELNO)  				    \
{									    \
  fprintf(STREAM, "\tmov\t%sip, %slr\n", REGISTER_PREFIX, REGISTER_PREFIX); \
  fprintf(STREAM, "\tbl\tmcount\n");					    \
}

/* NetBSD uses the old PCC style aggregate returning conventions. */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 1

/* Although not normally relevant (since by default, all aggregates
   are returned in memory) compiling some parts of libc requires
   non-APCS style struct returns.  */
#undef RETURN_IN_MEMORY

/* VERY BIG NOTE : Change of structure alignment for RiscBSD.
   There are consequences you should be aware of...

   Normally GCC/arm uses a structure alignment of 32 for compatibility
   with armcc.  This means that structures are padded to a word
   boundary.  However this causes problems with bugged NetBSD kernel
   code (possibly userland code as well - I have not checked every
   binary).  The nature of this bugged code is to rely on sizeof()
   returning the correct size of various structures rounded to the
   nearest byte (SCSI and ether code are two examples, the vm system
   is another).  This code breaks when the structure alignment is 32
   as sizeof() will report a word=rounded size.  By changing the
   structure alignment to 8. GCC will conform to what is expected by
   NetBSD.

   This has several side effects that should be considered.
   1. Structures will only be aligned to the size of the largest member.
      i.e. structures containing only bytes will be byte aligned.
           structures containing shorts will be half word alinged.
           structures containing ints will be word aligned.

      This means structures should be padded to a word boundary if
      alignment of 32 is required for byte structures etc.
      
   2. A potential performance penalty may exist if strings are no longer
      word aligned.  GCC will not be able to use word load/stores to copy
      short strings.
      
   This modification is not encouraged but with the present state of the
   NetBSD source tree it is currently the only solution that meets the
   requirements.  */
#undef STRUCTURE_SIZE_BOUNDARY
#define STRUCTURE_SIZE_BOUNDARY 8
