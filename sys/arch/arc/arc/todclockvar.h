/* $NetBSD: todclockvar.h,v 1.1.10.2 2001/06/13 15:00:28 soda Exp $ */
/* NetBSD: clockvar.h,v 1.4 1997/06/22 08:02:18 jonathan Exp  */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Definitions for cpu-independent todclock handling for the arc.
 */

/*
 * todclocktime structure:
 *
 * structure passed to TOY clocks when setting them.  broken out this
 * way, so that the time_t -> field conversion can be shared.
 */
struct todclocktime {
	int	year;			/* year - 1900 */
	int	mon;			/* month (1 - 12) */
	int	day;			/* day (1 - 31) */
	int	hour;			/* hour (0 - 23) */
	int	min;			/* minute (0 - 59) */
	int	sec;			/* second (0 - 59) */
	int	dow;			/* day of week (0 - 6; 0 = Sunday) */
};

/*
 * todclockfns structure:
 *
 * function switch used by chip-independent todclock code, to access
 * chip-dependent routines.
 */
struct todclockfns {
	void (*tcf_get) __P((struct device *, time_t, struct todclocktime *));
	void (*tcf_set) __P((struct device *, struct todclocktime *));
};

void todclockattach __P((struct device *, const struct todclockfns *, int));
