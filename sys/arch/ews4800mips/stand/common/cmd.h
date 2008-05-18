/*	$NetBSD: cmd.h,v 1.1.78.1 2008/05/18 12:31:55 yamt Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#define	CMDBUF_SIZE	0x100
#define	CMDARG_MAX	8

struct cmd_batch_tab {
	int (*func)(int, char **, int);
	int argc;
	char *arg[CMDARG_MAX - 1];
};
extern struct cmd_batch_tab cmd_batch_tab[];

void cmd_exec(const char *);

/* registered functions */
int cmd_help(int, char **, int);
int cmd_info(int, char **, int);
int cmd_reboot(int, char **, int);
int cmd_boot(int, char **, int);
int cmd_boot_ux(int, char **, int);
int cmd_mem(int, char **, int);
int cmd_batch(int, char **, int);

int cmd_load_binary(int, char **, int);
int cmd_jump(int, char **, int);
int cmd_disklabel(int, char **, int);
int cmd_ls(int, char **, int);
int cmd_log_save(int, char **, int);

/* test routines */
int cmd_test(int, char **, int);
int cmd_tlb(int, char **, int);
int cmd_cop0(int, char **, int);
int cmd_kbd_scancode(int, char **, int);
int cmd_ether_test(int, char **, int);
int cmd_ga_test(int, char **, int);
