/* Native-dependent definitions for Alpha running NetBSD, for GDB.
   Copyright (C) 1986, 1987, 1989, 1992, 1995, 1996
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Need to define this before including the common nm-nbsd.h. */
#define	SVR4_SHARED_LIBS 1
#define TARGET_ELF64 1

/* Get generic NetBSD native definitions. */
#include <nm-nbsd.h>
#include <machine/alpha_cpu.h>

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   argument regs.  A0_REGNUM points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

/* The alpha does not step over a breakpoint, the manpage is lying again.  */

#define CANNOT_STEP_BREAKPOINT /* ??? */

/* NetBSD/alpha has shared libraries.  */

#define GDB_TARGET_HAS_SHARED_LIBS
