/*	$NetBSD: clock.c,v 1.7.56.1 2008/02/18 21:04:50 mjf Exp $ */

#include <sys/types.h>
#include <machine/prom.h>

#include <lib/libsa/stand.h>
#include "libsa.h"

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x)      (int)((((unsigned int)(x)) >> 4) * 10 +\
				(((unsigned int)(x)) & 0xf))
#define TOBCD(x)        (int)((((unsigned int)(x)) / 10 * 16) +\
				(((unsigned int)(x)) % 10))

#define SECDAY          (24 * 60 * 60)
#define SECYR           (SECDAY * 365)
#define LEAPYEAR(y)     (((y) & 3) == 0)
#define YEAR0		68

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

u_long
chiptotime(int sec, int min, int hour, int day, int mon, int year)
{
	int days, yr;

	sec = FROMBCD(sec);
	min = FROMBCD(min);
	hour = FROMBCD(hour);
	day = FROMBCD(day);
	mon = FROMBCD(mon);
	year = FROMBCD(year) + YEAR0;
	if (year < 70)
		year = 70;

	/* simple sanity checks */
	if (year < 70 || mon < 1 || mon > 12 || day < 1 || day > 31)
		return (0);
	days = 0;
	for (yr = 70; yr < year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[mon - 1] + day - 1;
	if (LEAPYEAR(yr) && mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	return days * SECDAY + hour * 3600 + min * 60 + sec;
}

time_t
getsecs(void)
{
	struct mvmeprom_time m;

	mvmeprom_rtc_rd(&m);
	return chiptotime(m.sec_BCD, m.min_BCD, m.hour_BCD, m.day_BCD,
	    m.month_BCD, m.year_BCD);
}
