/* Macro definitions for i386 running under BSD Unix.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: tm.h,v 1.3.2.1 1995/11/22 03:57:34 perry Exp $
*/

/* Override number of expected traps from sysv. */
#define START_INFERIOR_TRAPS_EXPECTED 2

/* Most definitions from sysv could be used. */
#include "tm-i386v.h"

/* Shared library code */
#include "solib.h"

/* We define our own fetch and store methods. */
#define FETCH_INFERIOR_REGISTERS

/* On 386 bsd, sigtramp is above the user stack and immediately below
   the user area. Using constants here allows for cross debugging.
   These are tested for BSDI but should work on 386BSD.
   XXX - Changed to new kernel VA, not tested */
#define SIGTRAMP_START	0xf7bfdfc0
#define SIGTRAMP_END	0xf7bfe000

/* The following redefines make backtracing through sigtramp work.
   They manufacture a fake sigtramp frame and obtain the saved pc in sigtramp
   from the sigcontext structure which is pushed by the kernel on the
   user stack, along with a pointer to it.  */

/* FRAME_CHAIN takes a frame's nominal address and produces the frame's
   chain-pointer.
   In the case of the i386, the frame's nominal address
   is the address of a 4-byte word containing the calling frame's address.  */
#undef FRAME_CHAIN
#define FRAME_CHAIN(thisframe)  \
  (thisframe->signal_handler_caller \
   ? thisframe->frame \
   : (!inside_entry_file ((thisframe)->pc) \
      ? read_memory_integer ((thisframe)->frame, 4) \
      : 0))

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#undef FRAMELESS_FUNCTION_INVOCATION
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  do { \
    if ((FI)->signal_handler_caller) \
      (FRAMELESS) = 0; \
    else \
      (FRAMELESS) = frameless_look_for_prologue(FI); \
  } while (0)

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */

/* Offset to saved PC in sigcontext, from <sys/signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 44

#undef FRAME_SAVED_PC(FRAME)
#define FRAME_SAVED_PC(FRAME) \
  (((FRAME)->signal_handler_caller \
    ? sigtramp_saved_pc (FRAME) \
    : read_memory_integer ((FRAME)->frame + 4, 4)) \
   )
