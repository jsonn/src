/*	$NetBSD: macglobals.s,v 1.3.12.1 1997/09/22 06:32:00 thorpej Exp $	*/

/* Copyright 1994 by Bradley A. Grantham, All rights reserved */

/*
 * Mac OS global variable space; storage for global variables used by
 * Mac ROM traps and glue routines (see macrom.c, macrom.h macromasm.s)
 */
	.text
	.space 0x2a00

/*
 * changed from 0xf00 to 0x2a00 as some routine running before ADBReInit
 * chooses to write to 0x1fb8.  With the trap table from 0x0 to 0x3ff,
 * this additional space of 0x2a00 should be sufficient  (WRU)
 */ 
