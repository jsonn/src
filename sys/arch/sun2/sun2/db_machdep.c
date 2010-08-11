/*	$NetBSD: db_machdep.c,v 1.10.20.2 2010/08/11 22:52:49 yamt Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: db_machdep.c,v 1.10.20.2 2010/08/11 22:52:49 yamt Exp $");

#include <sys/param.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <machine/promlib.h>
#include <machine/pte.h>

#include <sun2/sun2/machdep.h>
#include <sun2/sun2/control.h>

#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>

static void db_mach_abort  (db_expr_t, bool, db_expr_t, const char *);
static void db_mach_halt   (db_expr_t, bool, db_expr_t, const char *);
static void db_mach_reboot (db_expr_t, bool, db_expr_t, const char *);
static void db_mach_pagemap(db_expr_t, bool, db_expr_t, const char *);

const struct db_command db_machine_command_table[] = {
	{ DDB_ADD_CMD("abort",	db_mach_abort,	0,
	  "Calls prom_abort()", NULL, NULL) },
	{ DDB_ADD_CMD("halt",	db_mach_halt,	0,
	  "Calls prom_halt()", NULL, NULL) },
	{ DDB_ADD_CMD("pgmap",	db_mach_pagemap, CS_SET_DOT,
	  "Prints the PTE and segmap values", "virtual-address", NULL) },
	{ DDB_ADD_CMD("reboot",	db_mach_reboot,	0,
	  "Calls prom_boot()", NULL, NULL) },
	{ DDB_ADD_CMD( NULL,NULL,0,NULL,NULL,NULL) }
};

/*
 * Machine-specific ddb commands for the sun2:
 *    abort:	Drop into monitor via abort (allows continue)
 *    halt: 	Exit to monitor as in halt(8)
 *    reboot:	Reboot the machine as in reboot(8)
 *    pgmap:	Given addr, Print addr, segmap, pagemap, pte
 */

static void 
db_mach_abort(db_expr_t addr, bool have_addr, db_expr_t count,
    const char *modif)
{
	prom_abort();
}

static void 
db_mach_halt(db_expr_t addr, bool have_addr, db_expr_t count,
    const char *modif)
{
	prom_halt();
}

static void 
db_mach_reboot(db_expr_t addr, bool have_addr, db_expr_t count,
    const char *modif)
{
	prom_boot("");
}


static void pte_print(int);

static void 
db_mach_pagemap(db_expr_t addr, bool have_addr, db_expr_t count,
    const char *modif)
{
	u_long va = m68k_trunc_page((u_long)addr);
	int pte;
	int sme;

	sme = get_segmap(va);
	if (sme == 0xFF) pte = 0;
	else pte = get_pte(va);
	db_printf("0x%08lx [%02x] 0x%08x", va, sme, pte);

	pte_print(pte);
	db_next = va + PAGE_SIZE;
}

static void 
pte_print(int pte)
{
	int t;
	static const char *pgt_names[] = {
		"MEM", "OBIO", "VME0", "VME8",
	};

	if (pte & PG_VALID) {
		db_printf(" V");
		if (pte & PG_WRITE)
			db_printf(" W");
		if (pte & PG_SYSTEM)
			db_printf(" S");
		if (pte & PG_NC)
			db_printf(" NC");
		if (pte & PG_REF)
			db_printf(" Ref");
		if (pte & PG_MOD)
			db_printf(" Mod");

		t = (pte >> PG_TYPE_SHIFT) & 3;
		db_printf(" %s", pgt_names[t]);
		db_printf(" PA=0x%x\n", PG_PA(pte));
	}
	else db_printf(" INVALID\n");
}
