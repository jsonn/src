/* Native-dependent definitions for ARM running NetBSD, for GDB.
   Copyright 1986, 1987, 1989, 1992, 1994, 1999 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef NM_NBSD_H
#define NM_NBSD_H

#ifdef __ELF__
#define SVR4_SHARED_LIBS
#endif

/* Get generic NetBSD native definitions. */
#include <nm-nbsd.h>

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = arm_register_u_addr ((blockend),(regno));

extern int
arm_register_u_addr PARAMS ((int, int));

/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)

/* We'd like the functions for handling 26-bit modes, please. */
#define ARM_26BIT_R15

#endif /* NM_NBSD_H */
