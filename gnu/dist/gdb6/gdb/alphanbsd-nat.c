/* Native-dependent code for NetBSD/alpha.

   Copyright (C) 2007 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "regcache.h"
#include "target.h"

#include "alpha-tdep.h"
#include "alphabsd-nat.h"

#include "nbsd-nat.h"

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_alphanbsd_nat (void);

void
_initialize_alphanbsd_nat (void)
{
  struct target_ops *t;

  /* Add some extra features to the common *BSD/alpha target.  */
  t = alphabsd_target ();
  t->to_pid_to_exec_file = nbsd_pid_to_exec_file;
  add_target (t);
}
