/*-
 * Copyright (C) 1993, 1994	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: adbsys.h,v 1.1.2.2 1994/07/30 04:21:59 lkestel Exp $
 *
 */


#include <sys/time.h>	/* timeval stuff */
#include <sys/ioctl.h>	/* ioctls */


/* Handy visual constants */
#define ADB_MAX_HANDLERS	256
#define ADB_MAX_DEVS	16


/* Different ADB system types */
enum adb_system_e {
	MacIIADB,
	MacIIsiADB,
	MacPBADB};
extern enum adb_system_e adb_system_type;


/* an ADB event */
typedef struct adb_event_s {
	int addr;			/* device address */
	int hand_id;			/* handler id */
	int def_addr;			/* default address */
	int byte_count;			/* number of bytes */
	unsigned char bytes[8];		/* bytes from register 0 */
	struct timeval timestamp;	/* time event was acquired */
	union {
		struct adb_keydata_s{
			int key;	/* ADB key code */
		} k;
		struct adb_mousedata_s{
			int dx;		/* mouse delta x */
			int dy;		/* mouse delta y */
			int buttons;	/* buttons (down << (buttonnum)) */
		} m;
	} u;				/* courtesy interpretation */
} adb_event_t;


/* a device on the ADB */
typedef struct adb_dev_s{
	int		default_addr;	/* startup address */
	int		handler_id;	/* handler ID */
} adb_dev_t;

extern adb_dev_t adb_devs[ADB_MAX_DEVS];


	/* Interesting default addresses */
#define ADBADDR_MAP	2
#define ADBADDR_REL	3
#define ADBADDR_ABS	4
#define ADBADDR_KBD	ADBADDR_MAP
#define ADBADDR_MS	ADBADDR_REL
#define ADBADDR_TABLET	ADBADDR_ABS


	/* Interesting handler IDs */
#define ADB_STDKBD	1
#define ADB_EXTKBD	2
#define ADBMS_100DPI	1
#define ADBMS_200DPI	2


/* Need to export for mac68k/machdep.c:setmachdep() */
extern long adb_intr_II(void);
extern long adb_intr_SI(void);
extern long adb_intr_PB(void);


	/* Get device info from ADB system */
typedef struct adb_devinfo_s{
	adb_dev_t	dev[ADB_MAX_DEVS];
} adb_devinfo_t;
#define ADBIOC_GETDEVS	_IOR('A', 128, adb_devinfo_t)


	/* Set keyboard lights */
typedef struct adb_setlights_s{
	int addr;
	unsigned int lights;
} adb_setlights_t;
#define ADBIOC_SETLIGHTS	_IOWR('A', 129, adb_setlights_t)


	/* Event auto-repeat */
typedef struct adb_rptinfo_s{
	int delay_ticks;	/* ticks before repeat */
	int interval_ticks;	/* ticks between repeats */
} adb_rptinfo_t;
#define ADBIOC_GETREPEAT	_IOR('A', 130, adb_rptinfo_t)
#define ADBIOC_SETREPEAT	_IOW('A', 131, adb_rptinfo_t)
