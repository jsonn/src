/*	$NetBSD: pm_direct.h,v 1.5.20.1 2002/06/20 03:39:33 nathanw Exp $	*/

/*
 * Copyright (C) 1997 Takashi Hamada
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
 *  This product includes software developed by Takashi Hamada.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* From: pm_direct.h 1.0 01/02/97 Takashi Hamada */

/*
 * Public declarations that other routines may need.
 */

/* data structure of the command of the Power Manager */
typedef	struct	{
	short	command;	/* command of the Power Manager 	*/
	short	num_data;	/* number of data			*/
	char	*s_buf;		/* pointer to buffer for sending 	*/
	char	*r_buf;		/* pointer to buffer for receiving	*/
	char	data[32];	/* data buffer (is it too much?)	*/
}	PMData;

int pmgrop __P((PMData *));
void pm_adb_restart __P((void));
void pm_adb_poweroff __P((void));
void pm_read_date_time __P((u_long *));
void pm_set_date_time __P((u_long));

struct pmu_battery_info
{
	unsigned int flags;
	unsigned int cur_charge;
	unsigned int max_charge;
	signed   int draw;
	unsigned int voltage;
};

int pm_battery_info __P((int, struct pmu_battery_info *));

int pm_read_nvram __P((int));
void pm_write_nvram __P((int, int));
int pm_read_brightness __P((void));
void pm_set_brightness __P((int));
void pm_init_brightness __P((void));
void pm_eject_pcmcia __P((int));

/* PMU commands */
#define PMU_POWER_OFF		0x7e	/* Turn Power off */
#define PMU_RESET_CPU		0xd0	/* Reset CPU */

#define PMU_SET_RTC		0x30	/* Set realtime clock */
#define PMU_READ_RTC		0x38	/* Read realtime clock */

#define PMU_WRITE_PRAM		0x32	/* Write PRAM */
#define PMU_READ_PRAM		0x3a	/* Read PRAM */

#define PMU_WRITE_NVRAM		0x33	/* Write NVRAM */
#define PMU_READ_NVRAM		0x3b	/* Read NVRAM */

#define PMU_EJECT_PCMCIA	0x4c	/* Eject PCMCIA slot */

#define PMU_SET_BRIGHTNESS	0x41	/* Set backlight brightness */
#define PMU_READ_BRIGHTNESS	0xd9	/* Read brightness button position */

#define PMU_POWER_EVENTS        0x8f    /* Send power-event commands to PMU */
#define PMU_SYSTEM_READY        0xdf    /* tell PMU we are awake */

#define PMU_SMART_BATTERY_STATE	0x6f	/* Read battery state */

/* Bits in PMU interrupt and interrupt mask bytes */
#define PMU_INT_ADB_AUTO	0x04	/* ADB autopoll, when PMU_INT_ADB */
#define PMU_INT_PCEJECT		0x04	/* PC-card eject buttons */
#define PMU_INT_SNDBRT		0x08	/* sound/brightness up/down buttons */
#define PMU_INT_ADB		0x10	/* ADB autopoll or reply data */
#define PMU_INT_BATTERY		0x20
#define PMU_INT_WAKEUP		0x40
#define PMU_INT_TICK		0x80	/* 1-second tick interrupt */

/* Bits to use with the PMU_POWER_CTRL0 command */
#define PMU_POW0_ON		0x80	/* OR this to power ON the device */
#define PMU_POW0_OFF		0x00	/* leave bit 7 to 0 to power it OFF */

/* Bits to use with the PMU_POWER_CTRL command */
#define PMU_POW_ON		0x80	/* OR this to power ON the device */
#define PMU_POW_OFF		0x00	/* leave bit 7 to 0 to power it OFF */
#define PMU_POW_BACKLIGHT	0x01	/* backlight power */
#define PMU_POW_CHARGER		0x02	/* battery charger power */
#define PMU_POW_IRLED		0x04	/* IR led power (on wallstreet) */
#define PMU_POW_MEDIABAY	0x08	/* media bay power (wallstreet/lombard ?) */

/* PMU PMU_POWER_EVENTS commands */
enum {
	PMU_PWR_GET_POWERUP_EVENTS      = 0x00,
	PMU_PWR_SET_POWERUP_EVENTS      = 0x01,
	PMU_PWR_CLR_POWERUP_EVENTS      = 0x02,
	PMU_PWR_GET_WAKEUP_EVENTS       = 0x03,
	PMU_PWR_SET_WAKEUP_EVENTS       = 0x04,
	PMU_PWR_CLR_WAKEUP_EVENTS       = 0x05,
};

/* PMU Power Information */

#define PMU_PWR_AC_PRESENT	(1 << 0)
#define PMU_PWR_BATT_PRESENT	(1 << 2)

