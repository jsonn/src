/*      $Id: internals.h,v 1.4.2.2 1999/12/27 18:30:03 wrstuden Exp $ */

/*-
 * Copyright (c) 1998-1999 Brett Lymn (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <menu.h>

#ifndef INTERNALS_H
#define INTERNALS_H

#define MATCH_FORWARD 1
#define MATCH_REVERSE 2
#define MATCH_NEXT_FORWARD 3
#define MATCH_NEXT_REVERSE 4

/* stole this from curses.h */
#define max(a,b)        ((a) > (b) ? a : b)

/* function prototypes */

void _menui_draw_item __P((MENU *, int));
int _menui_draw_menu __P((MENU *));
int _menui_goto_item __P((MENU *, ITEM *, int));
int _menui_match_pattern __P((MENU *, char, int, int *));
int _menui_match_items __P((MENU *, int, int *));
void _menui_max_item_size __P((MENU *));
int _menui_stitch_items __P((MENU *));

#endif
