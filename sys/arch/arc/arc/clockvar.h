/*	$NetBSD: clockvar.h,v 1.3.6.2 2000/11/20 20:00:20 bouyer Exp $	*/
/*	$OpenBSD: clockvar.h,v 1.1 1998/01/29 15:06:19 pefo Exp $	*/
/*	NetBSD: clockvar.h,v 1.1 1995/06/28 02:44:59 cgd Exp 	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * Adopted for r4400: Per Fogelstrom
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
 * Definitions for "cpu-independent" clock handling for the mips arc arch.
 */

/*
 * clocktime structure:
 *
 * structure passed to TOY clocks when setting them.  broken out this
 * way, so that the time_t -> field conversion can be shared.
 */
struct tod_time {
	int	year;			/* year - 1900 */
	int	mon;			/* month (1 - 12) */
	int	day;			/* day (1 - 31) */
	int	hour;			/* hour (0 - 23) */
	int	min;			/* minute (0 - 59) */
	int	sec;			/* second (0 - 59) */
	int	dow;			/* day of week (0 - 6; 0 = Sunday) */
};

/*
 * clockdesc structure:
 *
 * provides clock-specific functions to do necessary operations.
 */
struct clock_softc {
	struct	device sc_dev;

	/*
	 * The functions that all types of clock provide.
	 */
	void	(*sc_attach) __P((struct device *parent, struct device *self,
		    void *aux));
	void	(*sc_init) __P((struct clock_softc *csc));
	void	(*sc_get) __P((struct clock_softc *csc, time_t base,
		    struct tod_time *ct));
	void	(*sc_set) __P((struct clock_softc *csc, struct tod_time *ct));

	/*
	 * Private storage for particular clock types.
	 */
	void	*sc_data;

	/*
	 * Has the time been initialized?
	 */
	int	sc_initted;
};
