/*	$NetBSD: am7990var.h,v 1.9.2.1 1997/02/21 19:28:32 is Exp $	*/

/*
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef DDB
#define	integrate
#define hide
#else
#define	integrate	static __inline
#define hide		static
#endif

/*
 * Ethernet software status per device.
 *
 * Each interface is referenced by a network interface structure,
 * ethercom.ec_if, which the routing code uses to locate the interface. 
 * This structure contains the output queue for the interface, its address, ...
 *
 * NOTE: this structure MUST be the first element in machine-dependent
 * le_softc structures!  This is designed SPECIFICALLY to make it possible
 * to simply cast a "void *" to "struct le_softc *" or to
 * "struct am7990_softc *".  Among other things, this saves a lot of hair
 * in the interrupt handlers.
 */
struct am7990_softc {
	struct	device sc_dev;		/* base device glue */
	struct	ethercom sc_ethercom;	/* Ethernet common part */

	/*
	 * Memory functions:
	 *
	 *	copy to/from descriptor
	 *	copy to/from buffer
	 *	zero bytes in buffer
	 */
	void	(*sc_copytodesc)
		    __P((struct am7990_softc *, void *, int, int));
	void	(*sc_copyfromdesc)
		    __P((struct am7990_softc *, void *, int, int));
	void	(*sc_copytobuf)
		    __P((struct am7990_softc *, void *, int, int));
	void	(*sc_copyfrombuf)
		    __P((struct am7990_softc *, void *, int, int));
	void	(*sc_zerobuf)
		    __P((struct am7990_softc *, int, int));

	/*
	 * Machine-dependent functions:
	 *
	 *	read/write CSR
	 *	hardware init hook - may be NULL
	 *	no carrier hook - may be NULL
	 */
	u_int16_t (*sc_rdcsr)
		    __P((struct am7990_softc *, u_int16_t));
	void	(*sc_wrcsr)
		    __P((struct am7990_softc *, u_int16_t, u_int16_t));
	void	(*sc_hwinit) __P((struct am7990_softc *));
	void	(*sc_nocarrier) __P((struct am7990_softc *));

	void	*sc_sh;		/* shutdownhook cookie */

	u_int16_t sc_conf3;	/* CSR3 value */
	u_int16_t sc_pad1;	/* be nice to m68k ports */

	void	*sc_mem;	/* base address of RAM -- CPU's view */
	u_long	sc_addr;	/* base address of RAM -- LANCE's view */

	u_long	sc_memsize;	/* size of RAM */

	int	sc_nrbuf;	/* number of receive buffers */
	int	sc_ntbuf;	/* number of transmit buffers */
	int	sc_last_rd;
	int	sc_first_td, sc_last_td, sc_no_td;

	int	sc_initaddr;
	int	sc_rmdaddr;
	int	sc_tmdaddr;
	int	sc_rbufaddr;
	int	sc_tbufaddr;

#ifdef LEDEBUG
	int	sc_debug;
#endif
	u_int8_t sc_enaddr[6];
	u_int8_t sc_pad[2];
};

/* Export this to machine-dependent drivers. */
extern struct cfdriver le_cd;

void am7990_config __P((struct am7990_softc *));
void am7990_init __P((struct am7990_softc *));
int am7990_ioctl __P((struct ifnet *, u_long, caddr_t));
void am7990_meminit __P((struct am7990_softc *));
void am7990_reset __P((struct am7990_softc *));
void am7990_setladrf __P((struct ethercom *, u_int16_t *));
void am7990_start __P((struct ifnet *));
void am7990_stop __P((struct am7990_softc *));
void am7990_watchdog __P((struct ifnet *));
int am7990_intr __P((void *));

/*
 * The following functions are only useful on certain cpu/bus
 * combinations.  They should be written in assembly language for
 * maximum efficiency, but machine-independent versions are provided
 * for drivers that have not yet been optimized.
 */
void am7990_copytobuf_contig __P((struct am7990_softc *, void *, int, int));
void am7990_copyfrombuf_contig __P((struct am7990_softc *, void *, int, int));
void am7990_zerobuf_contig __P((struct am7990_softc *, int, int));

#if 0	/* Example only - see am7990.c */
void am7990_copytobuf_gap2 __P((struct am7990_softc *, void *, int, int));
void am7990_copyfrombuf_gap2 __P((struct am7990_softc *, void *, int, int));
void am7990_zerobuf_gap2 __P((struct am7990_softc *, int, int));

void am7990_copytobuf_gap16 __P((struct am7990_softc *, void *, int, int));
void am7990_copyfrombuf_gap16 __P((struct am7990_softc *, void *, int, int));
void am7990_zerobuf_gap16 __P((struct am7990_softc *, int, int));
#endif /* Example only */
