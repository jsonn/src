/*
 * Copyright (c) 1992,1993,1994 Hellmuth Michaelis, Brian Dunford-Shore
 *                              and Joerg Wunsch
 *
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Hellmuth Michaelis, Brian Dunford-Shore and Joerg Wunsch.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @(#)pcvt_hdr.h, 3.00, Last Edit-Date: [Tue Mar  1 20:15:21 1994]
 *
 */

/*---------------------------------------------------------------------------
 *
 *	pcvt_hdr.h	VT220 Driver Global Include File
 *	------------------------------------------------
 *	-hm	screensaver update from joerg
 *	-hm	ctrl-alt-delete enclosed in ifdef's
 *	-hm	driver name changed from "pc" -> "vt"
 *	-hm	#define-able compile time options
 *	-jw	include rather primitive X stuff
 *	-hm	NetBSD 0.8 support
 *	-hm	NetBSD-current support
 *	-hm	converting to memory mapped virtual screens
 *	-hm	adding et4000/wd/paradise vga register names
 *	-hm	132 column support for et4000/wd/paradise
 *	-hm	updated WD90C11 register definitions
 *	-hm	patches from joerg for 2.20 alpha 1
 *	-hm	max screen memory definitions
 *	-hm	Video 7 register definitions
 *	-hm	keyboard initialization for ddb activists ...
 *	-hm	include cpufunc.h for netbsd 0.9
 *	-hm	Trident definitions
 *	-hm	cpufunc.h only for netbsd
 *	-hm	ignore keyboard security lock conditional
 *	-hm	vga chiptype definitions moved to pcvt-ioctl.h
 *	-hm	force 24 lines operation
 *	-jw	USL VT compatibility
 *	-hm	int pcstart() -> void pcstart()
 *	-hm	opsys defines from joerg
 *	-hm	made sleeping-while-scrollock-patch from joerg running
 *	-hm	keyboard again ....
 *	-hm	housekeeping and source restructuring
 *	-jw	mouse emulation mode
 *	-jw	all ifdef's converted to if's
 *	-hm	patch from Joerg, PCVT_INHIBIT_NUMLOCK for notebooks
 *	-jw	cleaned up the key prefix recognition
 *	-jw	included PCVT_META_ESC
 *	-hm	kludge to get NetBSD 0.9 compilation right (!?)
 *	-hm	FreeBSD-current patches from Joerg
 *	-hm	PCVT_NETBSD=XXX for version defines
 *	-hm	#include dev/cons.h for NetBSD-current
 *	-hm	intro of PCVT_PCBURST for netbsd
 *	-hm	last second nitty-gritty from Joerg  :-)
 *	-hm	------------ Release 3.00 --------------
 *
 *---------------------------------------------------------------------------*/

#ifdef PCVT_NETBSD
#undef PCVT_NETBSD
#endif
#define	PCVT_NETBSD		10

#include "param.h"
#include "conf.h"
#include "ioctl.h"
#include "proc.h"
#include "user.h"
#include "tty.h"
#include "uio.h"
#include "callout.h"
#include "systm.h"
#include "kernel.h"
#include "syslog.h"
#include "malloc.h"
#include "time.h"
#if PCVT_NETBSD > 9
#include "device.h"
#endif

#if PCVT_NETBSD > 9
#include "i386/isa/isavar.h"
#else
#include "i386/isa/isa_device.h"
#endif
#include "i386/isa/icu.h"
#include "i386/isa/isa.h"

#if PCVT_NETBSD > 9
#include "dev/cons.h"
#else
#include "i386/i386/cons.h"
#endif

#if PCVT_NETBSD <= 9
#include "machine/psl.h"
#include "machine/frame.h"
#endif
#include "machine/stdarg.h"
#if PCVT_NETBSD > 9
#include "i386/isa/pcvt/pcvt_ioctl.h"
#else
#include "machine/pcvt_ioctl.h"
#endif
#include "machine/pc/display.h"

#include "vm/vm_kern.h"

#define	PCVT_REL "3.00"		/* driver attach announcement	*/
				/* this is checked with ispcvt	*/
				/* see also: pcvt_ioctl.h	*/

/*======================================================================*
 *		START OF CONFIGURATION SECTION				*
 *======================================================================*/

/*
 * Note that each of the options below should rather be overriden by the
 * kernel config file instead of this .h file - this allows for different
 * definitions in different kernels compiled at the same machine
 *
 * The convention is as follows:
 *
 * options "PCVT_FOO=1"  - enables the option
 * options "PCVT_FOO"    - is a synonym for the above
 * options "PCVT_FOO=0"  - disables the option
 *
 * omitting an option defaults to what is shown below
 *
 * exceptions from this rule:
 *
 *   options "PCVT_NSCREENS=x"
 *   options "PCVT_SCANSET=x"
 *   options "PCVT_UPDATEFAST=x"
 *   options "PCVT_UPDATESLOW=x"
 *   options "PCVT_SYSBEEPF=x"
 *
 * are always numeric!
 */

/* -------------------------------------------------------------------- */
/* -------------------- OPERATING SYSTEM ------------------------------ */
/* -------------------------------------------------------------------- */
  
/* one of the following options must be set in the kernel config file:	*/

/* options "PCVT_386BSD" 	enable support for 386BSD + pk 0.2.4	*/

/* options "PCVT_NETBSD" 	enable support for NetBSD		*/
/* PCVT_NETBSD = 9 for the NetBSD 0.9 release				*/
/* PCVT_NETBSD > 9 for NetBSD-current (tested with february 5th '94)	*/

/* options "PCVT_FREEBSD"	enable support for FreeBSD		*/
/* PCVT_FREEBSD = 102 for 1.0 release (actually 1.0.2)			*/
/* PCVT_FREEBSD = 103 for -current [as around 31 Jan 94]		*/
/* PCVT_FREEBSD = 110 for 1.1 release (still to come :-)		*/

/* -------------------------------------------------------------------- */
/* ---------------- USER PREFERENCE DRIVER OPTIONS -------------------- */
/* -------------------------------------------------------------------- */

#if !defined PCVT_NSCREENS	/* ---------- DEFAULT: 8 -------------- */
#define	PCVT_NSCREENS 	8	/* this option defines how many virtual	*/
#endif				/* screens you want to have in your	*/
				/* system. each screen allocates memory,*/
				/* so you can't have an unlimited num-	*/
				/* ber...; the value is intented to be	*/
				/* compile-time overridable by a config	*/
				/* options "PCVT_NSCREENS=x" line	*/

#if !defined PCVT_VT220KEYB	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_VT220KEYB 0	/* this compiles a more vt220-like	*/
#elif PCVT_VT220KEYB != 0	/* keyboardlayout as described in the	*/	
# undef PCVT_VT220KEYB		/* file Keyboard.VT220.			*/
# define PCVT_VT220KEYB 1	/* if undefined, a more HP-like         */	
#endif				/* keyboardlayout is compiled		*/
				/* try to find out what YOU like !	*/

#if !defined PCVT_SCREENSAVER	/* ---------- DEFAULT: ON ------------- */
# define PCVT_SCREENSAVER 1	/* enable screen saver feature - this	*/
#elif PCVT_SCREENSAVER != 0	/* just blanks the display screen.	*/
# undef PCVT_SCREENSAVER	/* see PCVT_PRETTYSCRNS below ...	*/
# define PCVT_SCREENSAVER 1
#endif

#if !defined PCVT_PRETTYSCRNS	/* ---------- DEFAULT: ON ------------- */
# define PCVT_PRETTYSCRNS 0	/* for the cost of some microseconds of	*/
#elif PCVT_PRETTYSCRNS != 0	/* cpu time this adds a more "pretty"	*/
#undef PCVT_PRETTYSCRNS		/* version to the screensaver, an "*"	*/
#define PCVT_PRETTYSCRNS 1	/* in random locations of the display.	*/
#endif				/* NOTE: this should not be defined if	*/
				/* you have an energy-saving monitor 	*/
				/* which turns off the display if its	*/
				/* black !!!!!!				*/

#if !defined PCVT_CTRL_ALT_DEL	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_CTRL_ALT_DEL 0	/* this enables the execution of a cpu	*/
#elif PCVT_CTRL_ALT_DEL != 0	/* reset by pressing the CTRL, ALT and	*/
# undef PCVT_CTRL_ALT_DEL	/* DEL keys simultanously. Because this	*/
# define PCVT_CTRL_ALT_DEL 1	/* is a feature of an ancient simple	*/
#endif				/* bootstrap loader, it does not belong */
				/* into modern operating systems and 	*/
				/* was commented out by default ...	*/

#if !defined PCVT_USEKBDSEC	/* ---------- DEFAULT: ON ------------- */
# define PCVT_USEKBDSEC 1	/* do not set the COMMAND_INHOVR bit	*/
#elif PCVT_USEKBDSEC != 0	/* (1 = override security lock inhibit) */
#undef PCVT_USEKBDSEC		/* when initializing the keyboard, so   */
#define PCVT_USEKBDSEC 1	/* that security locking should work    */
#endif				/* now. I guess this has to be done also*/
				/* in the boot code to prevent single   */
				/* user startup ....                    */

#if !defined PCVT_24LINESDEF	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_24LINESDEF 0	/* use 24 lines in VT 25 lines mode and	*/
#elif PCVT_24LINESDEF != 0	/* HP 28 lines mode by default to have	*/
#undef PCVT_24LINESDEF		/* the the better compatibility to the	*/
#define PCVT_24LINESDEF 1	/* real VT220 - you can switch between	*/
#endif				/* the maximum possible screensizes in	*/
				/* those two modes (25 lines) and true	*/
				/* compatibility (24 lines) by using	*/
				/* the scon utility at runtime		*/

#if !defined PCVT_EMU_MOUSE	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_EMU_MOUSE 0	/* emulate a mouse systems mouse via	*/
#elif PCVT_EMU_MOUSE != 0	/* the keypad; this is experimental	*/
# undef PCVT_EMU_MOUSE		/* code intented to be used on note-	*/
# define PCVT_EMU_MOUSE 1	/* books in conjunction with XFree86;	*/
#endif				/* look at the comments in pcvt_kbd.c	*/
				/* if you are interested in testing it.	*/

#if !defined PCVT_META_ESC      /* ---------- DEFAULT: OFF ------------ */
# define PCVT_META_ESC 0        /* if ON, send the sequence "ESC key"	*/
#elif PCVT_META_ESC != 0        /* for a meta-shifted key; if OFF,	*/
# undef PCVT_META_ESC           /* send the normal key code with 0x80	*/
# define PCVT_META_ESC 1        /* added.				*/
#endif

/* -------------------------------------------------------------------- */
/* -------------------- DRIVER DEBUGGING ------------------------------ */
/* -------------------------------------------------------------------- */

#if !defined PCVT_SHOWKEYS	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_SHOWKEYS 0	/* this replaces the system load line	*/
#elif PCVT_SHOWKEYS != 0	/* on the vt 0 in hp mode with a display*/
# undef PCVT_SHOWKEYS		/* of the most recent keyboard scan-	*/
# define PCVT_SHOWKEYS 1	/* and status codes received from the	*/
#endif				/* keyboard controller chip.		*/
				/* this is just for some hardcore	*/
				/* keyboarders ....			*/
	
/* -------------------------------------------------------------------- */
/* -------------------- DRIVER OPTIONS -------------------------------- */
/* -------------------------------------------------------------------- */
/*     it is unlikely that anybody wants to change anything below       */

#if !defined PCVT_PCBURST	/* ---------- DEFAULT: 256 ------------ */
#define PCVT_PCBURST 256	/* NETBSD only: this is the number of	*/
#endif				/* characters handled as a burst in	*/
				/* routine pcstart(), file pcvt_drv.c	*/

#if !defined PCVT_SCANSET	/* ---------- DEFAULT: 1 -------------- */
# define PCVT_SCANSET 1		/* define the keyboard scancode set you	*/
#endif				/* want to use:				*/
				/* 1 - code set 1	(supported)	*/
				/* 2 - code set 2	(supported)	*/
				/* 3 - code set 3	(UNsupported)	*/

#if !defined PCVT_KEYBDID	/* ---------- DEFAULT: ON ------------- */
# define PCVT_KEYBDID 1		/* check type of keyboard connected. at	*/
#elif PCVT_KEYBDID != 0		/* least HP-keyboards send a id other	*/
# undef PCVT_KEYBDID		/* than the industry standard, so it	*/	
# define PCVT_KEYBDID 1		/* CAN lead to problems. if you have	*/
#endif				/* problems with this, TELL ME PLEASE !	*/

#if !defined PCVT_SIGWINCH	/* ---------- DEFAULT: ON ------------- */
# define PCVT_SIGWINCH 1	/* this sends a SIGWINCH signal in case	*/
#elif PCVT_SIGWINCH != 0	/* the window size is changed. to try,	*/
#undef PCVT_SIGWINCH		/* issue "scons -s<size>" while in elvis*/
#define PCVT_SIGWINCH 1		/* and you'll see the effect.		*/
#endif				/* i'm not sure, whether this feature	*/
				/* has to be in the driver or has to    */
				/* move as a ioctl call to scon ....	*/
				
#if !defined PCVT_NULLCHARS	/* ---------- DEFAULT: ON ------------- */
# define PCVT_NULLCHARS 1	/* allow the keyboard to send null 	*/
#elif PCVT_NULLCHARS != 0	/* (0x00) characters to the calling	*/
# undef PCVT_NULLCHARS		/* program. this has the side effect,	*/
# define PCVT_NULLCHARS 1	/* that every undefined key also sends	*/
#endif				/* out nulls. take it as experimental	*/
				/* code, this behaviour will change in	*/
				/* a future release			*/

#if !defined PCVT_BACKUP_FONTS	/* ---------- DEFAULT: ON ------------- */
# define PCVT_BACKUP_FONTS 1	/* fonts are always kept memory-backed;	*/
#elif  PCVT_BACKUP_FONTS != 0	/* otherwise copies are only made if	*/
# undef PCVT_BACKUP_FONTS	/* they are needed.			*/
# define PCVT_BACKUP_FONTS 1
#endif

#if !defined PCVT_FORCE8BIT	/* ---------- DEFAULT: OFF ------------ */
#define PCVT_FORCE8BIT 0	/* this forces the read channel to	*/
#elif PCVT_FORCE8BIT != 0	/* 8 bit on terminal reads. this is 	*/
#undef PCVT_FORCE8BIT		/* a real kludge, have a look at file	*/
#define PCVT_FORCE8BIT 1	/* pcvt_drv.c to see what this does ..	*/
#endif

#ifndef PCVT_UPDATEFAST		/* this is the rate at which the cursor */
#define PCVT_UPDATEFAST	(hz/10)	/* gets updated with it's new position	*/
#endif				/* see: async_update() in pcvt_sup.c	*/

#ifndef PCVT_UPDATESLOW		/* this is the rate at which the cursor	*/
#define PCVT_UPDATESLOW	3	/* position display and the system load	*/
#endif				/* (or the keyboard scancode display)	*/
				/* is updated. the relation is:		*/
				/* PCVT_UPDATEFAST/PCVT_UPDATESLOW	*/

#ifndef PCVT_SYSBEEPF		/* timer chip value to be used for the	*/
#define PCVT_SYSBEEPF 1193182	/* sysbeep frequency value.		*/
#endif				/* this should really go somewhere else,*/
				/* e.g. in isa.h; but it used to be in 	*/
				/* each driver, sometimes even with	*/
				/* different values (:-)		*/

#if !defined PCVT_NEEDPG	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_NEEDPG 0		/* pg moved out to cons.c with pk 0.2.2	*/
#elif PCVT_NEEDPG != 0		/* if you run a system with patchkit	*/
# undef PCVT_NEEDPG		/* 0.2.1 or earlier you must define this*/
# define PCVT_NEEDPG 1
#endif

#if !defined PCVT_SETCOLOR	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_SETCOLOR 0	/* enable making colors settable. this	*/
#elif PCVT_SETCOLOR != 0	/* introduces a new escape sequence	*/
# undef PCVT_SETCOLOR		/* <ESC d> which is (i think) not 	*/
# define PCVT_SETCOLOR 1	/* standardized, so this is an option	*/
#endif
				

#if !defined PCVT_132GENERIC	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_132GENERIC 0	/* if you #define this, you enable	*/
#elif PCVT_132GENERIC != 0	/*	EXPERIMENTAL (!!!!!!!!!!!!)	*/
# undef PCVT_132GENERIC		/* 	USE-AT-YOUR-OWN-RISK, 		*/
# define PCVT_132GENERIC 1	/*	MAY-DAMAGE-YOUR-MONITOR		*/
#endif				/* code to switch generic VGA boards/	*/
				/* chipsets to 132 column mode. Since	*/
				/* i could not verify this option, i	*/
				/* prefer to NOT generally enable this,	*/
				/* if you want to play, look at the 	*/
				/* hints and the code in pcvt_sup.c and	*/
				/* get in contact with Joerg Wunsch, who*/
				/* submitted this code. Be careful !!!	*/

#if !defined PCVT_PALFLICKER	/* ---------- DEFAULT: OFF ------------ */
#define PCVT_PALFLICKER 0	/* this option turns off the screen 	*/
#elif PCVT_PALFLICKER != 0	/* during accesses to the VGA DAC	*/
#undef PCVT_PALFLICKER		/* registers. why: on one fo the tested	*/
#define PCVT_PALFLICKER 1	/* pc's (WD-chipset), accesses to the	*/
#endif				/* vga dac registers caused distortions	*/
				/* on the screen. Ferraro says, one has	*/
				/* to blank the screen. the method used	*/
				/* to accomplish this stopped the noise	*/
				/* but introduced another flicker, so	*/
				/* this is for you to experiment .....	*/
				/* - see also PCVT_WAITRETRACE below --	*/
				
#if !defined PCVT_WAITRETRACE	/* ---------- DEFAULT: OFF ------------ */
#define PCVT_WAITRETRACE 0	/* this option waits for being in a 	*/
#elif PCVT_WAITRETRACE != 0	/* retrace window prior to accessing	*/
#undef PCVT_WAITRETRACE		/* the VGA DAC registers.		*/
#define PCVT_WAITRETRACE 1	/* this is the other method Ferraro	*/
#endif				/* mentioned in his book. this option 	*/
				/* did eleminate the flicker noticably	*/
				/* but not completely. besides that, it	*/
				/* is implemented as a busy-wait loop	*/
				/* which is a no-no-no in environments	*/
				/* like this - VERY BAD PRACTICE !!!!!	*/
				/* the other method implementing it is	*/
				/* using the vertical retrace irq, but	*/
				/* we get short of irq-lines on pc's.	*/
				/* this is for you to experiment .....	*/
				/* -- see also PCVT_PALFLICKER above -- */

#if !defined PCVT_INHIBIT_NUMLOCK /* --------- DEFAULT: OFF ----------- */
#define PCVT_INHIBIT_NUMLOCK 0 	/* A notebook hack: since i am getting	*/
#elif PCVT_INHIBIT_NUMLOCK != 0	/* tired of the numlock LED always	*/
#undef PCVT_INHIBIT_NUMLOCK    	/* being turned on - which causes the	*/
#define PCVT_INHIBIT_NUMLOCK 1 	/* right half of my keyboard being	*/
#endif                         	/* interpreted as a numeric keypad and	*/
				/* thus going unusable - i want to	*/
				/* have a better control over it. If	*/
				/* this option is enabled, only the	*/
				/* numlock key itself and the related	*/
				/* ioctls will modify the numlock	*/
				/* LED. (The ioctl is needed for the	*/
				/* ServerNumLock feature of XFree86.)	*/
				/* The default state is changed to	*/
				/* numlock off, and the escape		*/
				/* sequences to switch between numeric	*/
				/* and application mode keypad are	*/
				/* silently ignored.			*/

#ifdef XSERVER

#if !defined PCVT_USL_VT_COMPAT	/* ---------- DEFAULT: ON ------------- */
# define PCVT_USL_VT_COMPAT 1	/* emulate some compatibility ioctls	*/
#elif PCVT_USL_VT_COMPAT != 0	/* this is not a full USL VT compatibi-	*/
#undef PCVT_USL_VT_COMPAT	/* lity, but just enough to fool XFree86*/
#define PCVT_USL_VT_COMPAT 1	/* release 2 (or higher) in believing	*/
#endif				/* it runs on syscons and thus enabling	*/
				/* the stuff to switch VTs within an X	*/
				/* session				*/

#if !defined PCVT_FAKE_SYSCONS10/* ---------- DEFAULT: OFF ------------ */
# define PCVT_FAKE_SYSCONS10 0	/* fake syscons 1.0; this causes XFree86*/
#elif PCVT_FAKE_SYSCONS10 != 0	/* 2.0 to use the VT_OPENQRY ioctl in	*/
# undef PCVT_FAKE_SYSCONS10	/* order to test for a free vt to use	*/
# define PCVT_FAKE_SYSCONS10 1	/* This is NOT REQUIRED, and not enabled*/
#endif				/* by default due to the possible	*/
				/* confusion for other utilities.	*/
				/* NB: XFree86 (tm) version 2.1 will be	*/
				/* fixed w.r.t. VT_OPENQRY		*/

#endif /* XSERVER */				

/* perform some consistency checks */

#if defined PCVT_386BSD && PCVT_386BSD != 0
#undef PCVT_386BSD
#define PCVT_386BSD 1
#endif

#if defined PCVT_FREEBSD && PCVT_FREEBSD == 1
# undef PCVT_FREEBSD
# define PCVT_FREEBSD 102	/* assume 1.0 release */
#endif

#if defined PCVT_NETBSD && PCVT_NETBSD == 1
#undef PCVT_NETBSD
#define PCVT_NETBSD 9		/* assume 0.9 release for now */
#endif

#if PCVT_386BSD + PCVT_FREEBSD + PCVT_NETBSD == 0
# error "pcvt_hdr.h: You MUST define one of PCVT_{386,NET,FREE}BSD \
in the config file"
#elif (PCVT_386BSD && (PCVT_FREEBSD || PCVT_NETBSD)) || \
      (PCVT_NETBSD && (PCVT_FREEBSD || PCVT_386BSD)) || \
      (PCVT_FREEBSD && (PCVT_386BSD || PCVT_NETBSD))
# error "pcvt_hdr.h: You should only define *one* of PCVT_{386,NET,FREE}BSD \
in the config file"
#endif

#ifdef XSERVER

/* PCVT_NULLCHARS is mandatory for X server */
#if !PCVT_NULLCHARS
#undef PCVT_NULLCHARS
#define PCVT_NULLCHARS 1
#endif

/* PCVT_BACKUP_FONTS is mandatory for PCVT_USL_VT_COMPAT */
#if PCVT_USL_VT_COMPAT && !PCVT_BACKUP_FONTS
#undef PCVT_BACKUP_FONTS
#define PCVT_BACKUP_FONTS 1
#endif

#else /* XSERVER */
  
#if PCVT_USL_VT_COMPAT
#warning "Option PCVT_USL_VT_COMPAT meaningless without XSERVER"
#undef PCVT_USL_VT_COMPAT
#define PCVT_USL_VT_COMPAT 0
#endif

#endif /* XSERVER */

/* #undef PCVT_NEEDPG is mandatory for PCVT_NETBSD and PCVT_FREEBSD */
#if (PCVT_NETBSD || PCVT_FREEBSD) && PCVT_NEEDPG
#undef PCVT_NEEDPG
#define PCVT_NEEDPG 0
#endif

/* PCVT_SCREENSAVER is mandatory for PCVT_PRETTYSCRNS */
#if PCVT_PRETTYSCRNS && !PCVT_SCREENSAVER
#undef PCVT_SCREENSAVER
#define PCVT_SCREENSAVER 1
#endif

/* PCVT_FAKE_SYSCONS10 meaningless without PCVT_USL_VT_COMPAT */
#if PCVT_FAKE_SYSCONS10 && !PCVT_USL_VT_COMPAT
#undef PCVT_FAKE_SYSCONS10
#define PCVT_FAKE_SYSCONS10 0
#endif

#if PCVT_FAKE_SYSCONS10
# warning "options PCVT_FAKE_SYSCONS10 will be removed in the next release"
#endif

/* get the inline inb/outb back again ... */

#if PCVT_NETBSD
#if PCVT_NETBSD == 9
#include "machine/cpufunc.h"	/* NetBSD 0.9 [...and earlier -currents] */
#undef PCVT_USL_VT_COMPAT
#define PCVT_USL_VT_COMPAT 0	/* does not work, workaround ... */
#else
#include "machine/pio.h"	/* recent NetBSD -currents */
#define NEW_AVERUNNABLE		/* averunnable changes for younger currents */
#endif /* PCVT_NETBSD == 9 */
#endif /* PCVT_NETBSD */

#if PCVT_SCANSET !=1 && PCVT_SCANSET !=2
#error "Supported keyboard scancode sets are 1 and 2 only (for now)!!!"
#endif

/*---------------------------------------------------------------------------
	COLORS:

	be careful when designing color combinations, because on
	EGA and VGA displays, bit 3 of the attribute byte is used
	for characterset switching, and is no longer available for
	foreground intensity (bold)!

---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 *	INTERNAL COLOR DEFINITIONS - KERNEL
 *---------------------------------------------------------------------------*/

#define COLOR_KERNEL_FG	FG_LIGHTGREY	/* kernel messages, foreground */
#define COLOR_KERNEL_BG	BG_RED		/* kernel messages, background */

/* monochrome displays - really don't know if it fits ... */

#define MONO_KERNEL_FG	FG_UNDERLINE	/* kernel messages, foreground */
#define MONO_KERNEL_BG	BG_BLACK	/* kernel messages, background */

/*======================================================================*
 *		END OF CONFIGURATION SECTION				*
 *======================================================================*/

/*---------------------------------------------------------------------------*
 *	Keyboard and Keyboard Controller
 *---------------------------------------------------------------------------*/

#define CONTROLLER_CTRL	0x64	/* W - command, R - status	*/
#define CONTROLLER_DATA	0x60	/* R/W - data			*/

/* commands to control the CONTROLLER (8042) on the mainboard */

#define CONTR_READ	0x20	/* read command byte from controller */
#define CONTR_WRITE	0x60	/* write command to controller, see below */
#define CONTR_SELFTEST	0xaa	/* controller selftest, returns 0x55 when ok */
#define CONTR_IFTEST	0xab	/* interface selftest */
#define CONTR_KBDISABL	0xad	/* disable keyboard */
#define CONTR_KBENABL	0xae	/* enable keyboard */

/* command byte for writing to CONTROLLER (8042) via CONTR_WRITE */

#define	 COMMAND_RES7	0x80	/* bit 7, reserved, always write a ZERO ! */
#define	 COMMAND_PCSCAN	0x40	/* bit 6, 1 = convert to pc scan codes */
#define	 COMMAND_RES5	0x20	/* bit 5, perhaps (!) use 9bit frame
				 * instead of 11 */
#define	 COMMAND_DISABL	0x10	/* bit 4, 1 = disable keyboard */
#define	 COMMAND_INHOVR	0x08	/* bit 3, 1 = override security lock inhibit */
#define	 COMMAND_SYSFLG	0x04	/* bit 2, value stored as "system flag" */
#define	 COMMAND_RES2	0x02	/* bit 1, reserved, always write a ZERO ! */
#define	 COMMAND_IRQEN	0x01	/* bit 0, 1 = enable output buffer full
				 * interrupt */

/* status from CONTROLLER (8042) on the mainboard */

#define	STATUS_PARITY	0x80	/* bit 7, 1 = parity error on last byte */
#define STATUS_RXTIMO	0x40	/* bit 6, 1 = receive timeout error occured */
#define STATUS_TXTIMO	0x20	/* bit 5, 1 = transmit timeout error occured */
#define STATUS_ENABLE	0x10	/* bit 4, 1 = keyboard unlocked */
#define STATUS_WHAT	0x08	/* bit 3, 1 = wrote cmd to 0x64, 0 = wrote
				 * data to 0x60 */
#define STATUS_SYSFLG	0x04	/* bit 2, value stored as "system flag" */
#define STATUS_INPBF	0x02	/* bit 1, 1 = input buffer full (to 8042) */
#define STATUS_OUTPBF	0x01	/* bit 0, 1 = output buffer full (from 8042) */

/* commands to the KEYBOARD (via the 8042 controller on mainboard..) */

#define KEYB_C_RESET	0xff	/* reset keyboard to power-on status */
#define	KEYB_C_RESEND	0xfe	/* resend last byte in case of error */
#define KEYB_C_TYPEM	0xf3	/* set keyboard typematic rate/delay */
#define KEYB_C_ID	0xf2	/* return keyboard id */
#define KEYB_C_ECHO	0xee	/* diagnostic, echo 0xee */
#define KEYB_C_LEDS	0xed	/* set/reset numlock,capslock & scroll lock */

/* responses from the KEYBOARD (via the 8042 controller on mainboard..) */

#define	KEYB_R_OVERRUN0	0x00	/* keyboard buffer overflow */
#define KEYB_R_SELFOK	0xaa	/* keyboard selftest ok after KEYB_C_RESET */
#define KEYB_R_EXT0	0xe0	/* keyboard extended scancode prefix 1 */
#define KEYB_R_EXT1	0xe1	/* keyboard extended scancode prefix 2 */
#define KEYB_R_ECHO	0xee	/* keyboard response to KEYB_C_ECHO */
#define KEYB_R_BREAKPFX	0xf0	/* break code prefix for set 2 and 3 */
#define KEYB_R_ACK	0xfa	/* acknowledge after a command has rx'd */
#define KEYB_R_SELFBAD	0xfc	/*keyboard selftest FAILED after KEYB_C_RESET*/
#define KEYB_R_DIAGBAD	0xfd	/* keyboard self diagnostic failure */
#define KEYB_R_RESEND	0xfe	/* keyboard wants command resent or illegal
				 * command rx'd */
#define	KEYB_R_OVERRUN1	0xff	/* keyboard buffer overflow */

#define KEYB_R_MF2ID1	0xab	/* MF II Keyboard id-byte #1 */
#define KEYB_R_MF2ID2	0x41	/* MF II Keyboard id-byte #2 */
#define KEYB_R_MF2ID2HP	0x83	/* MF II Keyboard id-byte #2 from HP keybd's */

/* internal Keyboard Type */

#define KB_UNKNOWN	0	/* unknown keyboard type */
#define KB_AT		1	/* AT (84 keys) Keyboard */
#define KB_MFII		2	/* MF II (101/102 keys) Keyboard */

/*---------------------------------------------------------------------------*
 *	CMOS ram access to get the "Equipment Byte"
 *---------------------------------------------------------------------------*/

#define RTC_EQUIPMENT	0x14	/* equipment byte in cmos ram	*/
#define EQ_EGAVGA	0	/* reserved (= ega/vga)		*/
#define EQ_40COLOR	1	/* display = 40 col color	*/
#define EQ_80COLOR	2	/* display = 80 col color	*/
#define EQ_80MONO	3	/* display = 80 col mono	*/

/*---------------------------------------------------------------------------*
 *	VT220 -> internal color conversion table fields
 *---------------------------------------------------------------------------*/

#define VT_NORMAL	0x00		/* no attributes at all */
#define VT_BOLD		0x01		/* bold attribute */
#define VT_UNDER	0x02		/* underline attribute */
#define VT_BLINK	0x04		/* blink attribute */
#define VT_INVERSE	0x08		/* inverse attribute */

/*---------------------------------------------------------------------------*
 *	VGA GENERAL/EXTERNAL Registers          (3BA or 3DA and 3CA, 3C2, 3CC)
 *---------------------------------------------------------------------------*/

#define GN_MISCOUTR	0x3CC		/* misc output register read */
#define GN_MISCOUTW	0x3C2		/* misc output register write */
#define GN_INPSTAT0	0x3C2		/* input status 0, r/o */
#define GN_INPSTAT1M	0x3BA		/* input status 1, r/o, mono */
#define GN_INPSTAT1C	0x3DA		/* input status 1, r/o, color */
#define GN_FEATR	0x3CA		/* feature control, read */
#define GN_FEATWM	0x3BA		/* feature control, write, mono */
#define GN_FEATWC	0x3DA		/* feature control, write, color */
#define GN_VSUBSYS	0x3C3		/* video subsystem register r/w */
#define GN_DMCNTLM	0x3B8		/* display mode control, r/w, mono */
#define GN_DMCNTLC	0x3D8		/* display mode control, r/w, color */
#define GN_COLORSEL	0x3D9		/* color select register, w/o */
#define GN_HERCOMPAT	0x3BF		/* Hercules compatibility reg, w/o */

/*---------------------------------------------------------------------------*
 *	VGA CRTC Registers			  (3B4 and 3B5 or 3D4 and 3D5)
 *---------------------------------------------------------------------------*/

#define MONO_BASE	0x3B4		/* crtc index register address mono */
#define CGA_BASE	0x3D4		/* crtc index register address color */

#define	CRTC_ADDR	0x00		/* index register */

#define CRTC_HTOTAL	0x00		/* horizontal total */
#define CRTC_HDISPLE	0x01		/* horizontal display end */
#define CRTC_HBLANKS	0x02		/* horizontal blank start */
#define CRTC_HBLANKE	0x03		/* horizontal blank end */
#define CRTC_HSYNCS	0x04		/* horizontal sync start */
#define CRTC_HSYNCE	0x05		/* horizontal sync end */
#define CRTC_VTOTAL	0x06		/* vertical total */
#define CRTC_OVERFLL	0x07		/* overflow low */
#define CRTC_IROWADDR	0x08		/* inital row address */
#define CRTC_MAXROW	0x09		/* maximum row address */
#define CRTC_CURSTART	0x0A		/* cursor start row address */
#define 	CURSOR_ON_BIT 0x20	/* cursor on/off on mda/cga/vga */
#define CRTC_CUREND	0x0B		/* cursor end row address */
#define CRTC_STARTADRH	0x0C		/* linear start address mid */
#define CRTC_STARTADRL	0x0D		/* linear start address low */
#define CRTC_CURSORH	0x0E		/* cursor address mid */
#define CRTC_CURSORL	0x0F		/* cursor address low */
#define CRTC_VSYNCS	0x10		/* vertical sync start */
#define CRTC_VSYNCE	0x11		/* vertical sync end */
#define CRTC_VDE	0x12		/* vertical display end */
#define CRTC_OFFSET	0x13		/* row offset */
#define CRTC_ULOC	0x14		/* underline row address */
#define CRTC_VBSTART	0x15		/* vertical blank start */
#define CRTC_VBEND	0x16		/* vertical blank end */
#define CRTC_MODE	0x17		/* CRTC mode register */
#define CRTC_SPLITL	0x18		/* split screen start low */

/* start of ET4000 extensions */

#define CRTC_RASCAS	0x32		/* ras/cas configuration */
#define CRTC_EXTSTART	0x33		/* extended start address */
#define CRTC_COMPAT6845	0x34		/* 6845 comatibility control */
#define CRTC_OVFLHIGH	0x35		/* overflow high */
#define CRTC_SYSCONF1	0x36		/* video system configuration 1 */
#define CRTC_SYSCONF2	0x36		/* video system configuration 2 */

/* start of WD/Paradise extensions */

#define	CRTC_PR10	0x29		/* r/w unlocking */
#define	CRTC_PR11	0x2A		/* ega switches */
#define	CRTC_PR12	0x2B		/* scratch pad */
#define	CRTC_PR13	0x2C		/* interlace h/2 start */
#define	CRTC_PR14	0x2D		/* interlace h/2 end */
#define	CRTC_PR15	0x2E		/* misc control #1 */
#define	CRTC_PR16	0x2F		/* misc control #2 */
#define	CRTC_PR17	0x30		/* misc control #3 */
					/* 0x31 .. 0x3f reserved */
/* Video 7 */

#define CRTC_V7ID	0x1f		/* identification register */

/* Trident */

#define CRTC_MTEST	0x1e		/* module test register */
#define CRTC_SOFTPROG	0x1f		/* software programming */
#define CRTC_LATCHRDB	0x22		/* latch read back register */
#define CRTC_ATTRSRDB	0x24		/* attribute state read back register*/
#define CRTC_ATTRIRDB	0x26		/* attribute index read back register*/
#define CRTC_HOSTAR	0x27		/* high order start address register */

/*---------------------------------------------------------------------------*
 *	VGA TIMING & SEQUENCER Registers			 (3C4 and 3C5)
 *---------------------------------------------------------------------------*/

#define TS_INDEX	0x3C4		/* index register */
#define TS_DATA		0x3C5		/* data register */

#define TS_SYNCRESET	0x00		/* synchronous reset */
#define TS_MODE		0x01		/* ts mode register */
#define TS_WRPLMASK	0x02		/* write plane mask */
#define TS_FONTSEL	0x03		/* font select register */
#define TS_MEMMODE	0x04		/* memory mode register */

/* ET4000 only */

#define TS_RESERVED	0x05		/* undef, reserved */
#define TS_STATECNTL	0x06		/* state control register */
#define TS_AUXMODE	0x07		/* auxiliary mode control */

/* WD/Paradise only */

#define TS_UNLOCKSEQ	0x06		/* PR20 - unlock sequencer register */
#define TS_DISCFSTAT	0x07		/* PR21 - display config status */
#define TS_MEMFIFOCTL	0x10		/* PR30 - memory i/f & fifo control */
#define TS_SYSIFCNTL	0x11		/* PR31 - system interface control */
#define TS_MISC4	0x12		/* PR32 - misc control #4 */

/* Video 7 */

#define TS_EXTCNTL	0x06		/* extensions control */
#define TS_CLRVDISP	0x30		/* clear vertical display 0x30-0x3f */
#define TS_V7CHIPREV	0x8e		/* chipset revision 0x8e-0x8f */
#define TS_SWBANK	0xe8		/* single/write bank register, rev 5+*/
#define TS_RDBANK	0xe8		/* read bank register, rev 4+ */
#define TS_MISCCNTL	0xe8		/* misc control register, rev 4+ */
#define TS_SWSTROBE	0xea		/* switch strobe */
#define TS_MWRCNTL	0xf3		/* masked write control */
#define TS_MWRMVRAM	0xf4		/* masked write mask VRAM only */
#define TS_BANKSEL	0xf6		/* bank select */
#define TS_SWREADB	0xf7		/* switch readback */
#define TS_PAGESEL	0xf9		/* page select */
#define TS_COMPAT	0xfc		/* compatibility control */
#define TS_16BITCTL	0xff		/* 16 bit interface control */

/* Trident */

#define TS_HWVERS	0x0b		/* hardware version, switch old/new! */
#define TS_CONFPORT1	0x0c		/* config port 1 and 2    - caution! */
#define TS_MODEC2	0x0d		/* old/new mode control 2 - caution! */
#define TS_MODEC1	0x0e		/* old/new mode control 1 - caution! */
#define	TS_PUPM2	0x0f		/* power up mode 2 */

/*---------------------------------------------------------------------------*
 *	VGA GRAPHICS DATA CONTROLLER Registers		    (3CE, 3CF and 3CD)
 *---------------------------------------------------------------------------*/

#define GDC_SEGSEL	0x3CD		/* segment select register */
#define GDC_INDEX	0x3CE		/* index register */
#define GDC_DATA	0x3CF		/* data register */

#define GDC_SETRES	0x00		/* set / reset bits */
#define GDC_ENSETRES	0x01		/* enable set / reset */
#define GDC_COLORCOMP	0x02		/* color compare register */
#define GDC_ROTFUNC	0x03		/* data rotate / function select */
#define GDC_RDPLANESEL	0x04		/* read plane select */
#define GDC_MODE	0x05		/* gdc mode register */
#define GDC_MISC	0x06		/* gdc misc register */
#define GDC_COLORCARE	0x07		/* color care register */
#define GDC_BITMASK	0x08		/* bit mask register */

/* WD/Paradise only */

#define GDC_BANKSWA	0x09		/* PR0A - bank switch a */
#define GDC_BANKSWB	0x0a		/* PR0B - bank switch b */
#define GDC_MEMSIZE	0x0b		/* PR1 memory size */
#define GDC_VIDEOSEL	0x0c		/* PR2 video configuration */
#define GDC_CRTCNTL	0x0d		/* PR3 crt address control */
#define GDC_VIDEOCNTL	0x0e		/* PR4 video control */
#define GDC_PR5GPLOCK	0x0f		/* PR5 gp status and lock */

/* Video 7 */

#define GDC_DATALATCH	0x22		/* gdc data latch */

/*---------------------------------------------------------------------------*
 *	VGA ATTRIBUTE CONTROLLER Registers			 (3C0 and 3C1)
 *---------------------------------------------------------------------------*/

#define ATC_INDEX	0x3C0		/* index register  AND	*/
#define ATC_DATAW	0x3C0		/* data write	   !!!	*/
#define ATC_DATAR	0x3C1		/* data read */

#define ATC_ACCESS	0x20		/* access bit in ATC index register */

#define ATC_PALETTE0	0x00		/* color palette register 0 */
#define ATC_PALETTE1	0x01		/* color palette register 1 */
#define ATC_PALETTE2	0x02		/* color palette register 2 */
#define ATC_PALETTE3	0x03		/* color palette register 3 */
#define ATC_PALETTE4	0x04		/* color palette register 4 */
#define ATC_PALETTE5	0x05		/* color palette register 5 */
#define ATC_PALETTE6	0x06		/* color palette register 6 */
#define ATC_PALETTE7	0x07		/* color palette register 7 */
#define ATC_PALETTE8	0x08		/* color palette register 8 */
#define ATC_PALETTE9	0x09		/* color palette register 9 */
#define ATC_PALETTEA	0x0A		/* color palette register 10 */
#define ATC_PALETTEB	0x0B		/* color palette register 11 */
#define ATC_PALETTEC	0x0C		/* color palette register 12 */
#define ATC_PALETTED	0x0D		/* color palette register 13 */
#define ATC_PALETTEE	0x0E		/* color palette register 14 */
#define ATC_PALETTEF	0x0F		/* color palette register 15 */
#define ATC_MODE	0x10		/* atc mode register */
#define ATC_OVERSCAN	0x11		/* overscan register */
#define ATC_COLPLEN	0x12		/* color plane enable register */
#define ATC_HORPIXPAN	0x13		/* horizontal pixel panning */
#define ATC_COLRESET	0x14		/* color reset */
#define ATC_MISC	0x16		/* misc register (ET3000/ET4000) */

/*---------------------------------------------------------------------------*
 *	VGA palette handling (output DAC palette)
 *---------------------------------------------------------------------------*/
 
#define VGA_DAC		0x3C6		/* vga dac address */
#define VGA_PMSK	0x3F		/* palette mask, 64 distinct values */
#define NVGAPEL 	256		/* number of palette entries */
 
/*---------------------------------------------------------------------------*
 *	function key labels
 *---------------------------------------------------------------------------*/

#define LABEL_LEN	9		/* length of one label */
#define LABEL_MID	8		/* mid-part (row/col)	*/

#define LABEL_ROWH	((4*LABEL_LEN)+1)
#define LABEL_ROWL	((4*LABEL_LEN)+2)
#define LABEL_COLU	((4*LABEL_LEN)+4)
#define LABEL_COLH	((4*LABEL_LEN)+5)
#define LABEL_COLL	((4*LABEL_LEN)+6)

/* tab setting */

#define MAXTAB 132		/* no of possible tab stops */

/* escape detection state machine */

#define STATE_INIT	0	/* normal	*/
#define	STATE_ESC	1	/* got ESC	*/
#define STATE_HASH	2	/* got ESC #	*/
#define STATE_BROPN	3	/* got ESC (	*/
#define STATE_BRCLO	4	/* got ESC )	*/
#define STATE_CSI	5	/* got ESC [	*/
#define STATE_CSIQM	6	/* got ESC [ ?	*/
#define STATE_AMPSND	7	/* got ESC &	*/
#define STATE_STAR	8	/* got ESC *	*/
#define STATE_PLUS	9	/* got ESC +	*/
#define STATE_DCS	10	/* got ESC P	*/
#define STATE_SCA	11	/* got ESC <Ps> " */
#define STATE_STR	12	/* got ESC !	*/
#define STATE_MINUS	13	/* got ESC -	*/
#define STATE_DOT	14	/* got ESC .	*/
#define STATE_SLASH	15	/* got ESC /	*/

/* for storing escape sequence parameters */

#define MAXPARMS 	10	/* maximum no of parms */

/* terminal responses */

#define DA_VT220	"\033[?62;1;2;6;7;8;9c"

/* sub-states for Device Control String processing */

#define DCS_INIT	0	/* got ESC P ... */
#define DCS_AND_UDK	1	/* got ESC P ... | */
#define DCS_UDK_DEF	2	/* got ESC P ... | fnckey / */
#define DCS_UDK_ESC	3	/* got ESC P ... | fnckey / ... ESC */
#define DCS_DLD_DSCS	4	/* got ESC P ... { */
#define DCS_DLD_DEF	5	/* got ESC P ... { dscs */
#define DCS_DLD_ESC	6	/* got ESC P ... { dscs ... / ... ESC */

/* vt220 user defined keys and vt220 downloadable charset */

#define MAXUDKDEF	300	/* max 256 char + 1 '\0' + space.. */
#define	MAXUDKEYS	18	/* plus holes .. */
#define DSCS_LENGTH	3	/* descriptor length */
#define MAXSIXEL	8	/* sixels forever ! */

/* sub-states for HP-terminal emulator */

#define SHP_INIT	0

/* esc & f family */

#define SHP_AND_F	1
#define SHP_AND_Fa	2
#define SHP_AND_Fak	3
#define SHP_AND_Fak1	4
#define SHP_AND_Fakd	5
#define SHP_AND_FakdL	6
#define SHP_AND_FakdLl	7
#define SHP_AND_FakdLls	8

/* esc & j family */

#define SHP_AND_J	9
#define SHP_AND_JL	10

/* esc & every-thing-else */

#define SHP_AND_ETE	11

/* additionals for function key labels */

#define MAX_LABEL	16
#define MAX_STRING	80
#define MAX_STATUS	160

/* MAX values for screen sizes for possible video adaptors */

#define MAXROW_MDACGA	25		/* MDA/CGA can do 25 x 80 max */
#define MAXCOL_MDACGA	80

#define MAXROW_EGA	43		/* EGA can do 43 x 80 max */
#define MAXCOL_EGA	80

#define MAXROW_VGA	50		/* VGA can do 50 x 80 max */
#define MAXCOL_VGA	80
#define MAXCOL_SVGA	132		/* Super VGA can do 50 x 132 max */

/* switch 80/132 columns */

#define SCR_COL80	80		/* in 80 col mode */
#define SCR_COL132	132		/* in 132 col mode */

#define MAXDECSCA	(((MAXCOL_SVGA * MAXROW_VGA) \
			/ (8 * sizeof(unsigned int)) ) + 1 )

/* screen memory start, monochrome */

#ifndef	MONO_BUF
# if PCVT_FREEBSD && (PCVT_FREEBSD > 102)
#  define MONO_BUF	(KERNBASE+0xB0000)
# else
#  define MONO_BUF	0xfe0B0000		 /* NetBSD-current: isa.h */
# endif
#endif

/* screen memory start, color */

#ifndef	CGA_BUF
# if PCVT_FREEBSD && (PCVT_FREEBSD > 102)
#  define CGA_BUF	(KERNBASE+0xB8000)
# else
#  define CGA_BUF	0xfe0B8000		 /* NetBSD-current: isa.h */
# endif
#endif

#define	CHR		2		/* bytes per word in screen mem */

#define NVGAFONTS	8		/* number of vga fonts loadable */

#define MAXKEYNUM	127		/* max no of keys in table */

/* charset tables */

#define	CSL	0x0000		/* ega/vga charset, lower half of 512 */
#define	CSH	0x0800		/* ega/vga charset, upper half of 512 */
#define CSSIZE	96		/* (physical) size of a character set */

/* charset designations */

#define D_G0		0	/* designated as G0 */
#define D_G1		1	/* designated as G1 */
#define D_G2		2	/* designated as G2 */
#define D_G3		3	/* designated as G3 */
#define D_G1_96		4	/* designated as G1 for 96-char charsets */
#define D_G2_96		5	/* designated as G2 for 96-char charsets */
#define D_G3_96		6	/* designated as G3 for 96-char charsets */

/* which fkey-labels */

#define SYS_FKL		0	/* in hp mode, sys-fkls are active */
#define USR_FKL		1	/* in hp mode, user-fkls are active */

/* variables */

#ifdef EXTERN
#define WAS_EXTERN
#else
#define EXTERN extern
#endif

EXTERN	u_char	*more_chars;		/* response buffer via kbd */
EXTERN	int	char_count;		/* response char count */
EXTERN	u_char	color;			/* color or mono display */

EXTERN	u_short	kern_attr;		/* kernel messages char attributes */
EXTERN	u_short	user_attr;		/* character attributes */

#if !PCVT_EMU_MOUSE
#if PCVT_NETBSD
EXTERN struct tty *pc_tty[PCVT_NSCREENS];
#else
EXTERN struct tty pccons[PCVT_NSCREENS];
#endif /* PCVT_NETBSD */
#else /* PCVT_EMU_MOUSE */
#if PCVT_NETBSD
EXTERN struct tty *pc_tty[PCVT_NSCREENS + 1];
#else
EXTERN struct tty pccons[PCVT_NSCREENS + 1];
#endif
#endif /* PCVT_EMU_MOUSE */

struct sixels {
	u_char lower[MAXSIXEL];		/* lower half of char */
	u_char upper[MAXSIXEL];		/* upper half of char */
};

struct udkentry {
	u_char	first[MAXUDKEYS];	/* index to first char */
	u_char	length[MAXUDKEYS];	/* length of this entry */
};

/* VGA palette handling */
struct rgb {
	u_char	r, g, b;		/* red/green/blue, valid 0..VGA_PMSK */
};

typedef struct video_state {
	u_short	*Crtat;			/* video page start addr */
	u_short *Memory;		/* malloc'ed memory start address */
	struct tty *vs_tty;		/* pointer to this screen's tty */
	u_char	maxcol;			/* 80 or 132 cols on screen */
	u_char 	row, col;		/* current cursor position */
	u_short	c_attr;			/* current character attributes */
	u_char	vtsgr;			/* current sgr configuration */
	u_short	cur_offset;		/* current cursor position offset */
	u_char	bell_on;		/* flag, bell enabled */
	u_char	sevenbit;		/* flag, data path 7 bits wide */
	u_char	dis_fnc;		/* flag, display functions enable */
	u_char	transparent;		/* flag, make path temp transparent
					 * for ctrls */
	u_char	scrr_beg;		/* scrolling region, begin */
	u_char	scrr_len;		/* scrolling region, length */
	u_char	scrr_end;		/* scrolling region, end */
	u_char	screen_rows;		/* screen size, length (minus status
					 * lines) */
	u_char	screen_rowsize; 	/* screen size, length */
	u_char	vga_charset;		/* VGA character set value */
	u_char	lastchar;		/* flag, vt100 behaviour of last char
					 * on line */
	u_char	lastrow;		/* save row, vt100 behaviour of last
					 * char on line */
	u_char	*report_chars;		/* ptr, status reports from terminal */
	u_char	report_count;		/* count, -"- */
	u_char	state;			/* escape sequence state machine */
	u_char	m_awm;			/* flag, vt100 mode, auto wrap */
	u_char	m_om;			/* flag, vt100 mode, origin mode */
	u_char	sc_flag;		/* flag, vt100 mode, saved parms
					 * valid */
	u_char	sc_row;			/* saved row */
	u_char	sc_col;			/* saved col */
	u_short sc_cur_offset;		/* saved cursor addr offset */
	u_short	sc_attr;		/* saved attributes */	
	u_char	sc_vtsgr;		/* saved sgr configuration */
	u_char	sc_awm;			/* saved auto wrap mode */
	u_char	sc_om;			/* saved origin mode */
	u_short	*sc_G0;			/* save G0 ptr */
	u_short	*sc_G1;			/* save G1 ptr */
	u_short	*sc_G2;			/* save G2 ptr */
	u_short	*sc_G3;			/* save G3 ptr */
	u_short	*sc_GL;			/* save GL ptr */
	u_short	*sc_GR;			/* save GR ptr */	
	u_char	sc_sel;			/* selective erase state */
	u_char	ufkl[8][17];		/* user fkey-labels */
	u_char	sfkl[8][17];		/* system fkey-labels */
	u_char	labels_on;		/* display fkey labels and status
					 * line on/off */
	u_char	which_fkl;		/* which fkey labels are active */
	char	tab_stops[MAXTAB]; 	/* table of active tab stops */
	u_char	parmi;			/* parameter index */
	u_char	parms[MAXPARMS];	/* parameter array */
	u_char	hp_state;		/* hp escape sequence state machine */
	u_char	attribute;		/* attribute normal, tx only, local */
	u_char	key;			/* fkey label no */
	u_char	l_len;			/* buffer length's */
	u_char	s_len;	
	u_char	m_len;
	u_char	i;			/* help (got short of names ...) */
	u_char	l_buf[MAX_LABEL+1]; 	/* buffers */
	u_char	s_buf[MAX_STRING+1];
	u_char	m_buf[MAX_STATUS+1];
	u_char	openf;			/* we are opened ! */
	u_char	vt_pure_mode;		/* no fkey labels, row/col, status */
	u_char	cursor_start;		/* Start of cursor */
	u_char	cursor_end;		/* End of cursor */
	u_char	cursor_on;		/* cursor switched on */
	u_char	ckm;			/* true = cursor key normal mode */
	u_char	irm;			/* true = insert mode */
	u_char	lnm;			/* Line Feed/New Line Mode */
	u_char	dcs_state;		/* dcs escape sequence state machine */
	u_char	udk_def[MAXUDKDEF]; 	/* new definitions for vt220 FKeys */
	u_char	udk_defi;		/* index for FKey definitions */
	u_char	udk_deflow;		/* low or high nibble in sequence */
	u_char	udk_fnckey;		/* function key to assign to */
	u_char	dld_dscs[DSCS_LENGTH];	/* designate soft character set id */
	u_char	dld_dscsi;		/* index for dscs */
	u_char	dld_sixel_lower;	/* upper/lower sixels of character */
	u_char	dld_sixelli;		/* index for lower sixels */
	u_char	dld_sixelui;		/* index for upper sixels */
	struct sixels sixel;		/* structure for storing char sixels */
	u_char	selchar;		/* true = selective attribute on */
	u_int	decsca[MAXDECSCA];	/* Select Character Attrib bit array */
	u_short *GL;			/* ptr to current GL conversion table*/
	u_short *GR;			/* ptr to current GR conversion table*/
	u_short *G0;			/* ptr to current G0 conversion table*/
	u_short *G1;			/* ptr to current G1 conversion table*/
	u_char force24;			/* force 24 lines in DEC 25 and HP 28
					 * lines */
	u_short *G2;			/* ptr to current G2 conversion table*/
	u_short *G3;			/* ptr to current G3 conversion table*/
	u_char	dld_id[DSCS_LENGTH+1];	/* soft character set id */
	u_char	which[DSCS_LENGTH+1];	/* which set to designate */
	u_char	whichi;			/* index into which ..	*/
	u_char  ss;			/* flag, single shift G2 / G3 -> GL */
	u_short *Gs;			/* ptr to cur. G2/G3 conversion table*/
	u_char	udkbuf[MAXUDKDEF];	/* buffer for user defined keys */
	struct udkentry ukt;		/* index & length for each udk */
	u_char	udkff;			/* index into buffer first free entry*/
	struct rgb palette[NVGAPEL];	/* saved VGA DAC palette */
	u_char	wd132col;		/* we are on a wd vga and in 132 col */
	u_char	scroll_lock; 		/* scroll lock active */
	u_char	caps_lock;		/* caps lock active */
	u_char	shift_lock;		/* shiftlock flag (virtual ..) */
	u_char	num_lock;		/* num lock, true = keypad num mode */
	
#if PCVT_USL_VT_COMPAT			/* SysV compatibility :-( */
	struct vt_mode smode;
	struct proc *proc;
	pid_t pid;
	unsigned vt_status;
#define	VT_WAIT_REL 1			/* wait till process released vt */
#define VT_WAIT_ACK 2			/* wait till process ack vt acquire */
#define VT_GRAFX    4			/* vt runs graphics mode */
#define VT_WAIT_ACT 8			/* a process is sleeping on this vt */
					/*  becoming active */
#endif /* PCVT_USL_VT_COMPAT */

} video_state;

EXTERN video_state vs[PCVT_NSCREENS];	/* parameters for screens */

struct vga_char_state {
	int	loaded;		/* Whether a font is loaded here */
	int	secondloaded;	/* an extension characterset was loaded, */
				/*	the number is found here	 */
	u_char	char_scanlines;	/* Scanlines per character */
	u_char	scr_scanlines;	/* Low byte of scanlines per screen */
	int	screen_size;	/* Screen size in SIZ_YYROWS */
};

EXTERN struct vga_char_state vgacs[NVGAFONTS];	/* Character set states */

#if PCVT_EMU_MOUSE
struct mousestat {
	struct timeval lastmove; /* last time the pointer moved */
	u_char opened;		 /* someone would like to use a mouse */
	u_char minor;		 /* minor device number */
	u_char buttons;		 /* current "buttons" pressed */
	u_char extendedseen;	 /* 0xe0 has been seen, do not use next key */
	u_char breakseen;	 /* key break has been seen for a sticky btn */
};
#endif /* PCVT_EMU_MOUSE */

#ifdef WAS_EXTERN

#if PCVT_NETBSD > 9

int pcprobe ();
void pcattach ();

struct cfdriver vtcd = {
	NULL, "vt", pcprobe, pcattach, DV_TTY, sizeof(struct device)
};

#else

int pcprobe ( struct isa_device *dev );
int pcattach ( struct isa_device *dev );

struct	isa_driver vtdriver = {		/* driver routines */
	pcprobe, pcattach, "vt",
};

#endif /* PCVT_NETBSD > 9 */

u_char fgansitopc[] = {			/* foreground ANSI color -> pc */
	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
};

u_char bgansitopc[] = {			/* background ANSI color -> pc */
	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
};

#if !PCVT_NETBSD
u_short *Crtat	=	(u_short *)MONO_BUF;	/* screen start address */
struct tty *pcconsp =	&pccons[0];		/* ptr to current device */
#else
struct tty *pcconsp;		/* ptr to current device, see pcattach() */
#endif /* PCVT_NETBSD */

#if PCVT_EMU_MOUSE
struct mousestat	mouse = {0};
struct mousedefs	mousedef = {0x3b, 0x3c, 0x3d, 0,     250000};
#endif /* PCVT_EMU_MOUSE */	/*  F1,   F2,   F3,   false, 0.25 sec */

video_state *vsp 		= &vs[0]; /* ptr to current screen parms */

#if PCVT_USL_VT_COMPAT
int	vt_switch_pending	= 0; 		/* if > 0, a vt switch is */
#endif /* PCVT_USL_VT_COMPAT */			/* pending; contains the # */
						/* of the old vt + 1 */

u_int	addr_6845		= MONO_BASE;	/* crtc base addr */
u_char	do_initialization	= 1;		/* we have to init ourselves */
u_char 	shift_down 		= 0;		/* shift key down flag */
u_char	ctrl_down		= 0; 		/* ctrl key down flag */
u_char	meta_down		= 0; 		/* alt key down flag */
u_char	altgr_down		= 0; 		/* altgr key down flag */
u_char	kbrepflag		= 1;		/* key repeat flag */
u_char	totalscreens		= 1;		/* screens available */
u_char	current_video_screen	= 0;		/* displayed screen no */
u_char	adaptor_type 		= UNKNOWN_ADAPTOR;/* adaptor type */
u_char 	vga_type 		= VGA_UNKNOWN;	/* vga chipset */
u_char	can_do_132col		= 0;		/* vga chipset can 132 cols */
u_char	vga_family		= 0;		/* vga manufacturer */
u_char	totalfonts		= 0;		/* fonts available */
u_char	chargen_access		= 0;		/* synchronize access */
u_char	keyboard_type		= KB_UNKNOWN;	/* type of keyboard */
u_char	keyboard_is_initialized = 0;		/* for ddb sanity */
u_char	kbd_polling		= 0;		/* keyboard is being polled */

#if PCVT_SHOWKEYS
u_char	keyboard_show		= 0;		/* normal display */
#endif /* PCVT_SHOWKEYS */

u_char	cursor_pos_valid	= 0;		/* sput left a valid position*/

u_char	critical_scroll		= 0;		/* inside scrolling up */
int	switch_page		= -1;		/* which page to switch to */

#if PCVT_SCREENSAVER
u_char	reset_screen_saver	= 1;		/* reset the saver next time */
u_char	scrnsv_active		= 0;		/* active flag */
#endif /* PCVT_SCREENSAVER */

#ifdef XSERVER
unsigned scrnsv_timeout		= 0;		/* initially off */
#if !PCVT_USL_VT_COMPAT
u_char pcvt_xmode		= 0;		/* display is grafx */
#endif /* PCVT_USL_VT_COMPAT */
u_char pcvt_kbd_raw		= 0;		/* keyboard sends scans */
#endif /* XSERVER */

#if PCVT_BACKUP_FONTS
u_char *saved_charsets[NVGAFONTS] = {0};	/* backup copy of fonts */
#endif /* PCVT_BACKUP_FONTS */

/*---------------------------------------------------------------------------

	VT220 attributes -> internal emulator attributes conversion tables

	be careful when designing color combinations, because on
	EGA and VGA displays, bit 3 of the attribute byte is used
	for characterset switching, and is no longer available for
	foreground intensity (bold)!

---------------------------------------------------------------------------*/

/* color displays */

u_char sgr_tab_color[16] = {
/*00*/  (BG_BLACK     | FG_LIGHTGREY),             /* normal               */
/*01*/  (BG_BLUE      | FG_LIGHTGREY),             /* bold                 */
/*02*/  (BG_BROWN     | FG_LIGHTGREY),             /* underline            */
/*03*/  (BG_MAGENTA   | FG_LIGHTGREY),             /* bold+underline       */
/*04*/  (BG_BLACK     | FG_LIGHTGREY | FG_BLINK),  /* blink                */
/*05*/  (BG_BLUE      | FG_LIGHTGREY | FG_BLINK),  /* bold+blink           */
/*06*/  (BG_BROWN     | FG_LIGHTGREY | FG_BLINK),  /* underline+blink      */
/*07*/  (BG_MAGENTA   | FG_LIGHTGREY | FG_BLINK),  /* bold+underline+blink */
/*08*/  (BG_LIGHTGREY | FG_BLACK),                 /* invers               */
/*09*/  (BG_LIGHTGREY | FG_BLUE),                  /* bold+invers          */
/*10*/  (BG_LIGHTGREY | FG_BROWN),                 /* underline+invers     */
/*11*/  (BG_LIGHTGREY | FG_MAGENTA),               /* bold+underline+invers*/
/*12*/  (BG_LIGHTGREY | FG_BLACK      | FG_BLINK), /* blink+invers         */
/*13*/  (BG_LIGHTGREY | FG_BLUE       | FG_BLINK), /* bold+blink+invers    */
/*14*/  (BG_LIGHTGREY | FG_BROWN      | FG_BLINK), /* underline+blink+invers*/
/*15*/  (BG_LIGHTGREY | FG_MAGENTA    | FG_BLINK)  /*bold+underl+blink+invers*/
};

/* monochrome displays (VGA version, no intensity) */

u_char sgr_tab_mono[16] = {
/*00*/  (BG_BLACK     | FG_LIGHTGREY),            /* normal               */
/*01*/  (BG_BLACK     | FG_UNDERLINE),            /* bold                 */
/*02*/  (BG_BLACK     | FG_UNDERLINE),            /* underline            */
/*03*/  (BG_BLACK     | FG_UNDERLINE),            /* bold+underline       */
/*04*/  (BG_BLACK     | FG_LIGHTGREY | FG_BLINK), /* blink                */
/*05*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK), /* bold+blink           */
/*06*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK), /* underline+blink      */
/*07*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK), /* bold+underline+blink */
/*08*/  (BG_LIGHTGREY | FG_BLACK),                /* invers               */
/*09*/  (BG_LIGHTGREY | FG_BLACK),                /* bold+invers          */
/*10*/  (BG_LIGHTGREY | FG_BLACK),                /* underline+invers     */
/*11*/  (BG_LIGHTGREY | FG_BLACK),                /* bold+underline+invers*/
/*12*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),     /* blink+invers         */
/*13*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),     /* bold+blink+invers    */
/*14*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),     /* underline+blink+invers*/
/*15*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK)      /*bold+underl+blink+invers*/
};

/* monochrome displays (MDA version, with intensity) */

u_char sgr_tab_imono[16] = {
/*00*/  (BG_BLACK     | FG_LIGHTGREY),                /* normal               */
/*01*/  (BG_BLACK     | FG_LIGHTGREY | FG_INTENSE),   /* bold                 */
/*02*/  (BG_BLACK     | FG_UNDERLINE),                /* underline            */
/*03*/  (BG_BLACK     | FG_UNDERLINE | FG_INTENSE),   /* bold+underline       */
/*04*/  (BG_BLACK     | FG_LIGHTGREY | FG_BLINK),     /* blink                */
/*05*/  (BG_BLACK     | FG_LIGHTGREY | FG_INTENSE | FG_BLINK), /* bold+blink  */
/*06*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK),     /* underline+blink      */
/*07*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK | FG_INTENSE), /* bold+underline+blink */
/*08*/  (BG_LIGHTGREY | FG_BLACK),                    /* invers               */
/*09*/  (BG_LIGHTGREY | FG_BLACK | FG_INTENSE),       /* bold+invers          */
/*10*/  (BG_LIGHTGREY | FG_BLACK),                    /* underline+invers     */
/*11*/  (BG_LIGHTGREY | FG_BLACK | FG_INTENSE),       /* bold+underline+invers*/
/*12*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),         /* blink+invers         */
/*13*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK | FG_INTENSE),/* bold+blink+invers*/
/*14*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),         /* underline+blink+invers*/
/*15*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK | FG_INTENSE) /* bold+underl+blink+invers */
};

#else /* WAS_EXTERN */

extern u_char		vga_type;
extern struct tty	*pcconsp;
extern video_state	*vsp;

#if PCVT_EMU_MOUSE
extern struct mousestat mouse;
extern struct mousedefs mousedef;
#endif /* PCVT_EMU_MOUSE */

#if PCVT_USL_VT_COMPAT
extern int		vt_switch_pending;
#endif /* PCVT_USL_VT_COMPAT */

extern u_int		addr_6845;
extern u_short		*Crtat;
extern struct isa_driver vtdriver;
extern u_char		do_initialization;
extern u_char		bgansitopc[];
extern u_char		fgansitopc[];
extern u_char 		shift_down;
extern u_char		ctrl_down;
extern u_char		meta_down;
extern u_char		altgr_down;
extern u_char		kbrepflag;
extern u_char		adaptor_type;
extern u_char		current_video_screen;
extern u_char		totalfonts;
extern u_char		totalscreens;
extern u_char		chargen_access;
extern u_char		keyboard_type;
extern u_char		can_do_132col;
extern u_char		vga_family;
extern u_char		keyboard_is_initialized;
extern u_char		kbd_polling;

#if PCVT_SHOWKEYS
extern u_char		keyboard_show;
#endif /* PCVT_SHOWKEYS */

u_char	cursor_pos_valid;

u_char	critical_scroll;
int	switch_page;

#if PCVT_SCREENSAVER
u_char	reset_screen_saver;
u_char	scrnsv_active;
#endif /* PCVT_SCREENSAVER */

extern u_char		sgr_tab_color[];
extern u_char		sgr_tab_mono[];
extern u_char		sgr_tab_imono[];

#ifdef XSERVER
extern unsigned		scrnsv_timeout;
extern u_char		pcvt_xmode;
extern u_char		pcvt_kbd_raw;
#endif /* XSERVER */

#if PCVT_BACKUP_FONTS
extern u_char		*saved_charsets[NVGAFONTS];
#endif /* PCVT_BACKUP_FONTS */


#endif	/* WAS_EXTERN */


/*
 * FreeBSD > 1.0.2 cleaned up the kernel definitions (with the aim of
 * getting ANSI-clean). Since there has been a mixed usage of types like
 * "dev_t" (actually some short) in prototyped and non-prototyped fasion,
 * each of those types is declared as "int" within function prototypes
 * (which is what the compiler would actually promote it to).
 *
 * The macros below are used to clarify which type a parameter ought to
 * be, regardless of its actual promotion to "int".
 */

#define Dev_t int
#define U_short int
#define U_char int

extern void bcopyb(void *from, void *to, u_int length);
extern void fillw(U_short value, void *addr, u_int length);

int	pcopen ( Dev_t dev, int flag, int mode, struct proc *p );
int	pcclose ( Dev_t dev, int flag, int mode, struct proc *p );
int	pcread ( Dev_t dev, struct uio *uio, int flag );
int	pcwrite ( Dev_t dev, struct uio *uio, int flag );
int	pcioctl ( Dev_t dev, int cmd, caddr_t data, int flag, struct proc *p );
int	pcmmap ( Dev_t dev, int offset, int nprot );
int	pcrint ( void );
int	pcparam ( struct tty *tp, struct termios *t );
int	pccnprobe ( struct consdev *cp );
int	pccninit ( struct consdev *cp );
int	pccnputc ( Dev_t dev, U_char c );
int	pccngetc ( Dev_t dev );

#if PCVT_NETBSD
void	pcstart ( struct tty *tp );
#else
void	pcstart ( struct tty *tp );
void	consinit ( void );
#endif /*  PCVT_NETBSD */

#if PCVT_USL_VT_COMPAT
void	switch_screen ( int n, int dontsave );
int	usl_vt_ioctl (Dev_t dev, int cmd, caddr_t data, int flag,
		      struct proc *);
int	vt_activate ( int newscreen );
int	vgapage ( int n );
void	get_usl_keymap( keymap_t *map );
#else
void	vgapage ( int n );
#endif /* PCVT_USL_VT_COMPAT */

#if PCVT_EMU_MOUSE
int mouse_ioctl ( Dev_t dev, int cmd, caddr_t data );
#endif /*  PCVT_EMU_MOUSE */

#if PCVT_SCREENSAVER
void 	pcvt_scrnsv_reset ( void );
#endif /* PCVT_SCREENSAVER */

#if PCVT_SCREENSAVER && defined(XSERVER)
void 	pcvt_set_scrnsv_tmo ( int );
#endif /* PCVT_SCREENSAVER && defined(XSERVER) */

#ifdef XSERVER
void	vga_move_charset ( unsigned n, unsigned char *b, int save_it);
#endif /* XSERVER */

void	async_update ( void *arg );
void	clr_parms ( struct video_state *svsp );
void	cons_highlight ( void );
void	cons_normal ( void );
void	dprintf ( unsigned flgs, const char *fmt, ... );
int	egavga_test ( void );
void	fkl_off ( struct video_state *svsp );
void	fkl_on ( struct video_state *svsp );
void	init_sfkl ( struct video_state *svsp );
void	init_ufkl ( struct video_state *svsp );
int	kbd_cmd ( int val );
void	kbd_code_init ( void );
void	kbd_code_init1 ( void );
int	kbdioctl ( Dev_t dev, int cmd, caddr_t data, int flag );
void	loadchar ( int fontset, int character, int char_scanlines,
		   u_char *char_table );
#if PCVT_NEEDPG
int	pg ( char *p, int q, int r, int s, int t, int u, int v,
	     int w, int x, int y, int z );
#endif
void	select_vga_charset ( int vga_charset );
void	set_2ndcharset ( void );
void	set_charset ( struct video_state *svsp, int curvgacs );
void	set_emulation_mode ( struct video_state *svsp, int mode );
void	set_screen_size ( struct video_state *svsp, int size );
u_char *sgetc ( int noblock );
void	sixel_vga ( struct sixels *charsixel, u_char *charvga );
void	sput ( u_char *s, U_char attrib, int len, int page );
void	sw_cursor ( int onoff );
void	sw_sfkl ( struct video_state *svsp );
void	sw_ufkl ( struct video_state *svsp );
void	swritefkl ( int num, u_char *string, struct video_state *svsp );
void	toggl_awm ( struct video_state *svsp );
void	toggl_bell ( struct video_state *svsp );
void	toggl_columns ( struct video_state *svsp );
void	toggl_dspf ( struct video_state *svsp );
void	toggl_sevenbit ( struct video_state *svsp );
struct tty *get_pccons ( Dev_t dev );
void	update_led ( void );
void	vga10_vga10 ( u_char *invga, u_char *outvga );
void	vga10_vga14 ( u_char *invga, u_char *outvga );
void	vga10_vga16 ( u_char *invga, u_char *outvga );
void	vga10_vga8 ( u_char *invga, u_char *outvga );
u_char	vga_chipset ( void );
int	vga_col ( struct video_state *svsp, int cols );
void	vga_screen_off ( void );
void	vga_screen_on ( void );
char   *vga_string ( int number );
int	vga_test ( void );
int	vgaioctl ( Dev_t dev, int cmd, caddr_t data, int flag );
void	vgapaletteio ( unsigned idx, struct rgb *val, int writeit );
void	vt_aln ( struct video_state *svsp );
void	vt_clearudk ( struct video_state *svsp );
void	vt_clreol ( struct video_state *svsp );
void	vt_clreos ( struct video_state *svsp );
void	vt_clrtab ( struct video_state *svsp );
int	vt_col ( struct video_state *svsp, int cols );
void	vt_coldmalloc ( void );
void	vt_cub ( struct video_state *svsp );
void	vt_cud ( struct video_state *svsp );
void	vt_cuf ( struct video_state *svsp );
void	vt_curadr ( struct video_state *svsp );
void	vt_cuu ( struct video_state *svsp );
void	vt_da ( struct video_state *svsp );
void	vt_dch ( struct video_state *svsp );
void	vt_dcsentry ( U_char ch, struct video_state *svsp );
void	vt_designate ( struct video_state *svsp);
void	vt_dl ( struct video_state *svsp );
void	vt_dld ( struct video_state *svsp );
void	vt_dsr ( struct video_state *svsp );
void	vt_ech ( struct video_state *svsp );
void	vt_ic ( struct video_state *svsp );
void	vt_il ( struct video_state *svsp );
void	vt_ind ( struct video_state *svsp );
void	vt_initsel ( struct video_state *svsp );
void	vt_keyappl ( struct video_state *svsp );
void	vt_keynum ( struct video_state *svsp );
void	vt_mc ( struct video_state *svsp );
void	vt_nel ( struct video_state *svsp );
void	vt_rc ( struct video_state *svsp );
void	vt_reqtparm ( struct video_state *svsp );
void	vt_reset_ansi ( struct video_state *svsp );
void	vt_reset_dec_priv_qm ( struct video_state *svsp );
void	vt_ri ( struct video_state *svsp );
void	vt_ris ( struct video_state *svsp );
void	vt_sc ( struct video_state *svsp );
void	vt_sca ( struct video_state *svsp );
void	vt_sd ( struct video_state *svsp );
void	vt_sed ( struct video_state *svsp );
void	vt_sel ( struct video_state *svsp );
void	vt_set_ansi ( struct video_state *svsp );
void	vt_set_dec_priv_qm ( struct video_state *svsp );
void	vt_sgr ( struct video_state *svsp );
void	vt_stbm ( struct video_state *svsp );
void	vt_str ( struct video_state *svsp );
void	vt_su ( struct video_state *svsp );
void	vt_tst ( struct video_state *svsp );
void	vt_udk ( struct video_state *svsp );

/*---------------------------------- E O F ----------------------------------*/
