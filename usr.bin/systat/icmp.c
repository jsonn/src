/*	$NetBSD: icmp.c,v 1.3.2.1 2000/06/23 16:39:57 minoura Exp $	*/

/*
 * Copyright (c) 1999 Andrew Doran <ad@NetBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: icmp.c,v 1.3.2.1 2000/06/23 16:39:57 minoura Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>

#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <nlist.h>
#include <kvm.h>

#include "systat.h"
#include "extern.h"

#define LHD(row, str) mvwprintw(wnd, row, 10, str)
#define RHD(row, str) mvwprintw(wnd, row, 45, str);
#define BD(row, str) LHD(row, str); RHD(row, str)
#define SHOW(stat, row, col) \
    mvwprintw(wnd, row, col, "%9llu", (unsigned long long)stats.stat)
#define SHOW2(type, row) SHOW(icps_inhist[type], row, 0); \
    SHOW(icps_outhist[type], row, 35)

static struct icmpstat stats;

static struct nlist namelist[] = {
	{ "_icmpstat" },
	{ "" }
};

WINDOW *
openicmp(void)
{

	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

void
closeicmp(w)
	WINDOW *w;
{

	if (w != NULL) {
		wclear(w);
		wrefresh(w);
		delwin(w);
	}
}

void
labelicmp(void)
{

	wmove(wnd, 0, 0); wclrtoeol(wnd);

	mvwprintw(wnd, 1, 0,  "------------ ICMP input -------------");
	mvwprintw(wnd, 1, 38, "------------- ICMP output -------------");

	mvwprintw(wnd, 8, 0,  "---------- Input histogram ----------");
	mvwprintw(wnd, 8, 38, "---------- Output histogram -----------");
	
	LHD(3, "with bad code");
	LHD(4, "with bad length");
	LHD(5, "with bad checksum");
	LHD(6, "with insufficient data");
	
	RHD(3, "errors generated");
	RHD(4, "suppressed - original too short");
	RHD(5, "suppressed - original was ICMP");
	RHD(6, "responses sent");

	BD(2, "total messages");
	BD(9, "echo response");
	BD(10, "echo request");
	BD(11, "destination unreachable");
	BD(12, "redirect");
	BD(13, "time-to-live exceeded");
	BD(14, "parameter problem");
	LHD(15, "router advertisement");	
	RHD(15, "router solicitation");
}

void
showicmp(void)
{
	u_long tin, tout;
	int i;

	for (i = tin = tout = 0; i <= ICMP_MAXTYPE; i++) {
		tin += stats.icps_inhist[i];
		tout += stats.icps_outhist[i];
	}

	tin += stats.icps_badcode + stats.icps_badlen + stats.icps_checksum + 
	    stats.icps_tooshort;
	mvwprintw(wnd, 2, 0, "%9lu", tin);
	mvwprintw(wnd, 2, 35, "%9lu", tout);

	SHOW(icps_badcode, 3, 0);
	SHOW(icps_badlen, 4, 0);
	SHOW(icps_checksum, 5, 0);
	SHOW(icps_tooshort, 6, 0);
	SHOW(icps_error, 3, 35);
	SHOW(icps_oldshort, 4, 35);
	SHOW(icps_oldicmp, 5, 35);
	SHOW(icps_reflect, 6, 35);

	SHOW2(ICMP_ECHOREPLY, 9);
	SHOW2(ICMP_ECHO, 10);
	SHOW2(ICMP_UNREACH, 11);
	SHOW2(ICMP_REDIRECT, 12);
	SHOW2(ICMP_TIMXCEED, 13);
	SHOW2(ICMP_PARAMPROB, 14);
	SHOW(icps_inhist[ICMP_ROUTERADVERT], 15, 0);
	SHOW(icps_outhist[ICMP_ROUTERSOLICIT], 15, 35);
}

int
initicmp(void)
{

	if (namelist[0].n_type == 0) {
		if (kvm_nlist(kd, namelist)) {
			nlisterr(namelist);
			return(0);
		}
		if (namelist[0].n_type == 0) {
			error("No namelist");
			return(0);
		}
	}
	
	return (1);
}

void
fetchicmp(void)
{

	KREAD((void *)namelist[0].n_value, &stats, sizeof(stats));
}
