/* $Id: common.h,v 1.1.1.1.2.2 2008/09/18 05:15:42 wrstuden Exp $ */
/*-
 * Copyright (c) 2008 Gregory McGarry <g.mcgarry@ieee.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define IDENT(x) asm(".ident\t\"" x "\"")

#define NULL (void *)0

extern int main(int argc, char *argv[]);
extern void exit(int);
extern int atexit(void (*fcn)(void));

#if PROFILE
extern void monstartup(unsigned long, unsigned long);
static void _mcleanup(void);
extern void monitor(char *, char *, char *, int, int);
#endif

extern void _init(void);
extern void _fini(void);
