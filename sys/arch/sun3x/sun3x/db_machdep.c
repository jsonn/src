/*	$NetBSD: db_machdep.c,v 1.3.4.1 1997/03/12 14:22:12 is Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-dependent functions used by ddb
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <machine/db_machdep.h>
#include <machine/machdep.h>
#include <machine/pte.h>

#include <ddb/db_command.h>
#include <ddb/db_output.h>

static void db_mach_abort   __P((db_expr_t, int, db_expr_t, char *));
static void db_mach_halt    __P((db_expr_t, int, db_expr_t, char *));
static void db_mach_reboot  __P((db_expr_t, int, db_expr_t, char *));
static void db_mach_pagemap __P((db_expr_t, int, db_expr_t, char *));

struct db_command db_machine_cmds[] = {
	{ "abort",	db_mach_abort,	0,	0 },
	{ "halt",	db_mach_halt,	0,	0 },
	{ "reboot",	db_mach_reboot,	0,	0 },
	{ "pgmap",	db_mach_pagemap, 	CS_SET_DOT, 0 },
	{ (char *)0, }
};

/*
 * This is called before ddb_init() to install the
 * machine-specific command table. (see machdep.c)
 */
void
db_machine_init()
{
	db_machine_commands_install(db_machine_cmds);
}

/*
 * Machine-specific ddb commands for the sun3x:
 *    abort:	Drop into monitor via abort (allows continue)
 *    halt: 	Exit to monitor as in halt(8)
 *    reboot:	Reboot the machine as in reboot(8)
 *    pgmap:	Given addr, Print addr, segmap, pagemap, pte
 */

static void
db_mach_abort(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	sunmon_abort();
}

static void
db_mach_halt(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	sunmon_halt();
}

static void
db_mach_reboot(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	sunmon_reboot("");
}


static void pte_print __P((int));

static void
db_mach_pagemap(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	u_long va, pte;

	va = trunc_page((u_long)addr);
	pte = get_pte(va);
	db_printf("0x%08x 0x%08x", va, pte);
	pte_print(pte);
	db_next = va + NBPG;
}

static void
pte_print(pte)
	int pte;
{

	if (pte & MMU_SHORT_PTE_DT) {
		if (pte & MMU_SHORT_PTE_CI)
			db_printf(" CI");
		if (pte & MMU_SHORT_PTE_M)
			db_printf(" Mod");
		if (pte & MMU_SHORT_PTE_USED)
			db_printf(" Ref");
		if (pte & MMU_SHORT_PTE_WP)
			db_printf(" WP");
		db_printf(" DT%d\n", pte & MMU_SHORT_PTE_DT);
	}
	else db_printf(" INVALID\n");
}
