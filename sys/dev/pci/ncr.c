/*	$NetBSD: ncr.c,v 1.87.2.6 2001/01/15 09:27:43 bouyer Exp $	*/

/**************************************************************************
**
**  Id: ncr.c,v 1.110 1997/09/10 20:46:11 se Exp
**
**  Device driver for the   NCR 53C810   PCI-SCSI-Controller.
**
**  FreeBSD / NetBSD
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	Wolfgang Stanglmeier	<wolf@cologne.de>
**	Stefan Esser		<se@mi.Uni-Koeln.de>
**
**  Ported to NetBSD by
**	Charles M. Hannum	<mycroft@gnu.ai.mit.edu>
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***************************************************************************
*/

#define NCR_DATE "pl24 96/12/14"

#define NCR_VERSION	(2)
#define	MAX_UNITS	(16)

#define NCR_GETCC_WITHMSG

#if defined (__FreeBSD__) && defined(KERNEL)
#include "opt_ncr.h"
#endif /* defined (__FreeBSD__) && defined(KERNEL) */

#ifdef FAILSAFE
#ifndef SCSI_NCR_DFLT_TAGS
#define	SCSI_NCR_DFLT_TAGS (0)
#endif /* SCSI_NCR_DFLT_TAGS */
#define	NCR_CDROM_ASYNC
#endif /* FAILSAFE */

/*==========================================================
**
**	Configuration and Debugging
**
**	May be overwritten in <arch/conf/xxxx>
**
**==========================================================
*/

/*
**    SCSI address of this device.
**    The boot routines should have set it.
**    If not, use this.
*/

#ifndef SCSI_NCR_MYADDR
#define SCSI_NCR_MYADDR      (7)
#endif /* SCSI_NCR_MYADDR */

/*
**    The default synchronous period factor
**    (0=asynchronous)
**    If maximum synchronous frequency is defined, use it instead.
*/

#ifndef	SCSI_NCR_MAX_SYNC

#ifndef SCSI_NCR_DFLT_SYNC
#define SCSI_NCR_DFLT_SYNC   (12)
#endif /* SCSI_NCR_DFLT_SYNC */

#else

#if	SCSI_NCR_MAX_SYNC == 0
#define	SCSI_NCR_DFLT_SYNC 0
#else
#define	SCSI_NCR_DFLT_SYNC (250000 / SCSI_NCR_MAX_SYNC)
#endif

#endif

/*
**    The minimal asynchronous pre-scaler period (ns)
**    Shall be 40.
*/

#ifndef SCSI_NCR_MIN_ASYNC
#define SCSI_NCR_MIN_ASYNC   (40)
#endif /* SCSI_NCR_MIN_ASYNC */

/*
**    The maximal bus with (in log2 byte)
**    (0=8 bit, 1=16 bit)
*/

#ifndef SCSI_NCR_MAX_WIDE
#define SCSI_NCR_MAX_WIDE   (1)
#endif /* SCSI_NCR_MAX_WIDE */

/*
**    The maximum number of tags per logic unit.
**    Used only for disk devices that support tags.
*/

#ifndef SCSI_NCR_DFLT_TAGS
#define SCSI_NCR_DFLT_TAGS    (4)
#endif /* SCSI_NCR_DFLT_TAGS */

/*==========================================================
**
**      Configuration and Debugging
**
**==========================================================
*/

/*
**    Number of targets supported by the driver.
**    n permits target numbers 0..n-1.
**    Default is 7, meaning targets #0..#6.
**    #7 .. is myself.
*/

#define MAX_TARGET  (16)

/*
**    Number of logic units supported by the driver.
**    n enables logic unit numbers 0..n-1.
**    The common SCSI devices require only
**    one lun, so take 1 as the default.
*/

#ifndef	MAX_LUN
#define MAX_LUN     (8)
#endif	/* MAX_LUN */

/*
**    The maximum number of jobs scheduled for starting.
**    There should be one slot per target, and one slot
**    for each tag of each target in use.
**    The calculation below is actually quite silly ...
*/

#define MAX_START   (256)

/*
**    The maximum number of segments a transfer is split into.
*/

#define MAX_SCATTER (33)

/*
**    The maximum transfer length (should be >= 64k).
**    MUST NOT be greater than (MAX_SCATTER-1) * PAGE_SIZE.
*/

#ifdef __NetBSD__
#define MAX_SIZE  ((MAX_SCATTER-1) * (long) NBPG)
#else
#define MAX_SIZE  ((MAX_SCATTER-1) * (long) PAGE_SIZE)
#endif

/*
**	other
*/

#define NCR_SNOOP_TIMEOUT (1000000)

/*==========================================================
**
**      Include files
**
**==========================================================
*/

#ifdef __NetBSD__
#ifdef _KERNEL
#define KERNEL
#endif
#endif

#include <sys/param.h>
#include <sys/time.h>

#ifdef KERNEL
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#ifdef __NetBSD__
#include <sys/reboot.h>
#endif
#ifndef __NetBSD__
#include <sys/sysctl.h>
#include <machine/clock.h>
#endif
#ifdef __NetBSD__
#include <uvm/uvm_extern.h>
#else
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#endif
#endif /* KERNEL */


#ifndef __NetBSD__
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/ncrreg.h>
#else
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <dev/pci/ncrreg.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#ifndef __alpha__
#undef DELAY
#define DELAY(x)	delay(x)
#endif
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#endif /* __NetBSD__ */

#include <dev/scsipi/scsiconf.h>

#if defined(__NetBSD__) && defined(__alpha__)
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap(va)
#endif

#if defined(__NetBSD__) && defined(__mips__)
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#undef vtophys
extern paddr_t kvtophys __P((vaddr_t)); /* XXX */
#define	vtophys(va)	kvtophys(va)
#endif

/*==========================================================
**
**	Debugging tags
**
**==========================================================
*/

#define DEBUG_ALLOC    (0x0001)
#define DEBUG_PHASE    (0x0002)
#define DEBUG_POLL     (0x0004)
#define DEBUG_QUEUE    (0x0008)
#define DEBUG_RESULT   (0x0010)
#define DEBUG_SCATTER  (0x0020)
#define DEBUG_SCRIPT   (0x0040)
#define DEBUG_TINY     (0x0080)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
#define DEBUG_TAGS     (0x0400)
#define DEBUG_FREEZE   (0x0800)
#define DEBUG_RESTART  (0x1000)

/*
**    Enable/Disable debug messages.
**    Can be changed at runtime too.
*/

#ifdef SCSI_NCR_DEBUG
	#define DEBUG_FLAGS ncr_debug
#else /* SCSI_NCR_DEBUG */
	#define SCSI_NCR_DEBUG	0
	#define DEBUG_FLAGS	0
#endif /* SCSI_NCR_DEBUG */



/*==========================================================
**
**	assert ()
**
**==========================================================
**
**	modified copy from 386bsd:/usr/include/sys/assert.h
**
**----------------------------------------------------------
*/

#undef assert
#define	assert(expression) { \
	if (!(expression)) { \
		(void)printf(\
			"assertion \"%s\" failed: file \"%s\", line %d\n", \
			#expression, \
			__FILE__, __LINE__); \
	} \
}

/*==========================================================
**
**	Access to the controller chip.
**
**==========================================================
*/

#ifdef __NetBSD__

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_stream_4		bus_space_read_4
#define	bus_space_write_stream_4	bus_space_write_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#define	INB(r) \
    INB_OFF(offsetof(struct ncr_reg, r))
#define	INB_OFF(o) \
    bus_space_read_1 (np->sc_st, np->sc_sh, (o))
#define	INW(r) \
    bus_space_read_2 (np->sc_st, np->sc_sh, offsetof(struct ncr_reg, r))
#define	INL(r) \
    INL_OFF(offsetof(struct ncr_reg, r))
#define	INL_OFF(o) \
    bus_space_read_4 (np->sc_st, np->sc_sh, (o))

#define	OUTB(r, val) \
    bus_space_write_1 (np->sc_st, np->sc_sh, offsetof(struct ncr_reg, r), (val))
#define	OUTW(r, val) \
    bus_space_write_2 (np->sc_st, np->sc_sh, offsetof(struct ncr_reg, r), (val))
#define	OUTL(r, val) \
    OUTL_OFF(offsetof(struct ncr_reg, r), (val))
#define	OUTL_OFF(o, val) \
    bus_space_write_4 (np->sc_st, np->sc_sh, (o), (val))

#define	READSCRIPT_OFF(base, off) \
    le32toh(base ? *((INT32 *)((char *)base + (off))) :			\
		  bus_space_read_stream_4(np->ram_tag, np->ram_handle, off))

#define	WRITESCRIPT_OFF(base, off, val) \
    do {								\
    	if (base)							\
    		*((INT32 *)((char *)base + (off))) = (htole32(val));	\
    	else								\
    		bus_space_write_stream_4(np->ram_tag, np->ram_handle,	\
		    off, htole32(val)); \
    } while (0)

#define	READSCRIPT(r) \
    READSCRIPT_OFF(np->script, offsetof(struct script, r))

#define	WRITESCRIPT(r, val) \
    WRITESCRIPT_OFF(np->script, offsetof(struct script, r), val)

#else /* !__NetBSD__ */

#ifdef NCR_IOMAPPED
 
#define	INB(r) inb (np->port + offsetof(struct ncr_reg, r))
#define	INB_OFF(o) inb (np->port + (o))
#define	INW(r) inw (np->port + offsetof(struct ncr_reg, r))
#define	INL(r) inl (np->port + offsetof(struct ncr_reg, r))
#define	INL_OFF(o) inl (np->port + (o))

#define	OUTB(r, val) outb (np->port+offsetof(struct ncr_reg,r),(val))
#define	OUTW(r, val) outw (np->port+offsetof(struct ncr_reg,r),(val))
#define	OUTL(r, val) outl (np->port+offsetof(struct ncr_reg,r),(val))
#define	OUTL_OFF(o, val) outl (np->port+(o),(val))

#else

#define	INB(r) (np->reg->r)
#define	INB_OFF(o) (*((volatile INT8 *)((char *)np->reg + (o))))
#define	INW(r) (np->reg->r)
#define	INL(r) (np->reg->r)
#define	INL_OFF(o) (*((volatile INT32 *)((char *)np->reg + (o))))

#define	OUTB(r, val) np->reg->r = (val)
#define	OUTW(r, val) np->reg->r = (val)
#define	OUTL(r, val) np->reg->r = (val)
#define	OUTL_OFF(o, val) *((volatile INT32 *)((char *)np->reg + (o))) = val

#endif

#define	READSCRIPT_OFF(base, off) (*((INT32 *)((char *)base + (off))))
#define	READSCRIPT(r) (np->script->r)
#define	WRITESCRIPT(r, val) np->script->r = (val)

#endif /* __NetBSD__ */

/*
**	Set bit field ON, OFF 
*/

#define OUTONB(r, m)	OUTB(r, INB(r) | (m))
#define OUTOFFB(r, m)	OUTB(r, INB(r) & ~(m))
#define OUTONW(r, m)	OUTW(r, INW(r) | (m))
#define OUTOFFW(r, m)	OUTW(r, INW(r) & ~(m))
#define OUTONL(r, m)	OUTL(r, INL(r) | (m))
#define OUTOFFL(r, m)	OUTL(r, INL(r) & ~(m))

/*==========================================================
**
**	Command control block states.
**
**==========================================================
*/

#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	/* sync/wide data transfer*/
#define HS_DISCONNECT	(3)	/* Disconnected by target */

#define HS_COMPLETE	(4)
#define HS_SEL_TIMEOUT	(5)	/* Selection timeout      */
#define HS_RESET	(6)	/* SCSI reset	     */
#define HS_ABORTED	(7)	/* Transfer aborted       */
#define HS_TIMEOUT	(8)	/* Software timeout       */
#define HS_FAIL		(9)	/* SCSI or PCI bus errors */
#define HS_UNEXPECTED	(10)	/* Unexpected disconnect  */

#define HS_DONEMASK	(0xfc)

/*==========================================================
**
**	Software Interrupt Codes
**
**==========================================================
*/

#define	SIR_SENSE_RESTART	(1)
#define	SIR_SENSE_FAILED	(2)
#define	SIR_STALL_RESTART	(3)
#define	SIR_STALL_QUEUE		(4)
#define	SIR_NEGO_SYNC		(5)
#define	SIR_NEGO_WIDE		(6)
#define	SIR_NEGO_FAILED		(7)
#define	SIR_NEGO_PROTO		(8)
#define	SIR_REJECT_RECEIVED	(9)
#define	SIR_REJECT_SENT		(10)
#define	SIR_IGN_RESIDUE		(11)
#define	SIR_MISSING_SAVE	(12)
#define	SIR_MAX			(12)

/*==========================================================
**
**	Extended error codes.
**	xerr_status field of struct ccb.
**
**==========================================================
*/

#define	XE_OK		(0)
#define	XE_EXTRA_DATA	(1)	/* unexpected data phase */
#define	XE_BAD_PHASE	(2)	/* illegal phase (4/5)   */

/*==========================================================
**
**	Negotiation status.
**	nego_status field	of struct ccb.
**
**==========================================================
*/

#define NS_SYNC		(1)
#define NS_WIDE		(2)

/*==========================================================
**
**	"Special features" of targets.
**	quirks field		of struct tcb.
**	actualquirks field	of struct ccb.
**
**==========================================================
*/

#define	QUIRK_AUTOSAVE	(0x01)
#define	QUIRK_NOMSG	(0x02)
#define QUIRK_NOSYNC	(0x10)
#define QUIRK_NOWIDE16	(0x20)
#define QUIRK_NOTAGS	(0x40)
#define	QUIRK_UPDATE	(0x80)

/*==========================================================
**
**	Capability bits in Inquire response byte 7.
**
**==========================================================
*/

#define	INQ7_QUEUE	(0x02)
#define	INQ7_SYNC	(0x10)
#define	INQ7_WIDE16	(0x20)

/*==========================================================
**
**	Misc.
**
**==========================================================
*/

#define CCB_MAGIC	(0xf2691ad2)
#define	MAX_TAGS	(16)		/* hard limit */

/*==========================================================
**
**	OS dependencies.
**
**	Note that various types are defined in ncrreg.h.
**
**==========================================================
*/

#define PRINT_ADDR(xp) scsi_print_addr(xp->sc_link)

/*==========================================================
**
**	Declaration of structs.
**
**==========================================================
*/

struct tcb;
struct lcb;
struct ccb;
struct ncb;
struct script;

typedef struct ncb * ncb_p;
typedef struct tcb * tcb_p;
typedef struct lcb * lcb_p;
typedef struct ccb * ccb_p;

struct link {
	ncrcmd	l_cmd;
	ncrcmd	l_paddr;
};

struct	usrcmd {
	u_long	target;
	u_long	lun;
	u_long	data;
	u_long	cmd;
};

#define UC_SETSYNC      10
#define UC_SETTAGS	11
#define UC_SETDEBUG	12
#define UC_SETORDER	13
#define UC_SETWIDE	14
#define UC_SETFLAG	15

#define	UF_TRACE	(0x01)

/*---------------------------------------
**
**	Timestamps for profiling
**
**---------------------------------------
*/

struct tstamp {
	struct timeval	start;
	struct timeval	end;
	struct timeval	select;
	struct timeval	command;
	struct timeval	data;
	struct timeval	status;
	struct timeval	disconnect;
	struct timeval	reselect;
};

/*
**	profiling data (per device)
*/

struct profile {
	u_long	num_trans;
	u_long	num_bytes;
	u_long	num_disc;
	u_long	num_break;
	u_long	num_int;
	u_long	num_fly;
	u_long	ms_setup;
	u_long	ms_data;
	u_long	ms_disc;
	u_long	ms_post;
};

/*==========================================================
**
**	Declaration of structs:		target control block
**
**==========================================================
*/

struct tcb {
	/*
	**	during reselection the ncr jumps to this point
	**	with SFBR set to the encoded target number
	**	with bit 7 set.
	**	if it's not this target, jump to the next.
	**
	**	JUMP  IF (SFBR != #target#)
	**	@(next tcb)
	*/

	struct link   jump_tcb;

	/*
	**	load the actual values for the sxfer and the scntl3
	**	register (sync/wide mode).
	**
	**	SCR_COPY (1);
	**	@(sval field of this tcb)
	**	@(sxfer register)
	**	SCR_COPY (1);
	**	@(wval field of this tcb)
	**	@(scntl3 register)
	*/

	ncrcmd	getscr[6];

	/*
	**	if next message is "identify"
	**	then load the message to SFBR,
	**	else load 0 to SFBR.
	**
	**	CALL
	**	<RESEL_LUN>
	*/

	struct link   call_lun;

	/*
	**	now look for the right lun.
	**
	**	JUMP
	**	@(first ccb of this lun)
	*/

	struct link   jump_lcb;

	/*
	**	pointer to interrupted getcc ccb
	*/

	ccb_p   hold_cp;

	/*
	**	pointer to ccb used for negotiating.
	**	Avoid to start a nego for all queued commands 
	**	when tagged command queuing is enabled.
	*/

	ccb_p   nego_cp;

	/*
	**	statistical data
	*/

	u_long	transfers;
	u_long	bytes;

	/*
	**	user settable limits for sync transfer
	**	and tagged commands.
	*/

	u_char	usrsync;
	u_char	usrtags;
	u_char	usrwide;
	u_char	usrflag;

	/*
	**	negotiation of wide and synch transfer.
	**	device quirks.
	*/

/*0*/	u_char	minsync;
/*1*/	u_char	sval;
/*2*/	u_short	period;
/*0*/	u_char	maxoffs;

/*1*/	u_char	quirks;

/*2*/	u_char	widedone;
/*3*/	u_char	wval;
	/*
	**	inquire data
	*/
#define MAX_INQUIRE 36
	u_char	inqdata[MAX_INQUIRE];

	/*
	**	the lcb's of this tcb
	*/

	lcb_p   lp[MAX_LUN];
};

/*==========================================================
**
**	Declaration of structs:		lun control block
**
**==========================================================
*/

struct lcb {
	/*
	**	during reselection the ncr jumps to this point
	**	with SFBR set to the "Identify" message.
	**	if it's not this lun, jump to the next.
	**
	**	JUMP  IF (SFBR != #lun#)
	**	@(next lcb of this target)
	*/

	struct link	jump_lcb;

	/*
	**	if next message is "simple tag",
	**	then load the tag to SFBR,
	**	else load 0 to SFBR.
	**
	**	CALL
	**	<RESEL_TAG>
	*/

	struct link	call_tag;

	/*
	**	now look for the right ccb.
	**
	**	JUMP
	**	@(first ccb of this lun)
	*/

	struct link	jump_ccb;

	/*
	**	start of the ccb chain
	*/

	ccb_p	next_ccb;

	/*
	**	Control of tagged queueing
	*/

	u_char		reqccbs;
	u_char		actccbs;
	u_char		reqlink;
	u_char		actlink;
	u_char		usetags;
	u_char		lasttag;
};

/*==========================================================
**
**      Declaration of structs:     COMMAND control block
**
**==========================================================
**
**	This substructure is copied from the ccb to a
**	global address after selection (or reselection)
**	and copied back before disconnect.
**
**	These fields are accessible to the script processor.
**
**----------------------------------------------------------
*/

struct head {
	/*
	**	Execution of a ccb starts at this point.
	**	It's a jump to the "SELECT" label
	**	of the script.
	**
	**	After successful selection the script
	**	processor overwrites it with a jump to
	**	the IDLE label of the script.
	*/

	struct link	launch;

	/*
	**	Saved data pointer.
	**	Points to the position in the script
	**	responsible for the actual transfer
	**	of data.
	**	It's written after reception of a
	**	"SAVE_DATA_POINTER" message.
	**	The goalpointer points after
	**	the last transfer command.
	*/

	u_int32_t	savep;
	u_int32_t	lastp;
	u_int32_t	goalp;

	/*
	**	The virtual address of the ccb
	**	containing this header.
	*/

	ccb_p	cp;

	/*
	**	space for some timestamps to gather
	**	profiling data about devices and this driver.
	*/

	struct tstamp	stamp;

	/*
	**	status fields.
	*/

	u_char		status[8];
};

/*
**	The status bytes are used by the host and the script processor.
**
**	The first four byte are copied to the scratchb register
**	(declared as scr0..scr3 in ncrreg.h) just after the select/reselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_REG are used.
**
**	The last four bytes are used inside the script by "COPY" commands.
**	Because source and destination must have the same alignment
**	in a longword, the fields HAVE to be at the choosen offsets.
**		xerr_st	(4)	0	(0x34)	scratcha
**		sync_st	(5)	1	(0x05)	sxfer
**		wide_st	(7)	3	(0x03)	scntl3
*/

/*
**	First four bytes (script)
*/
#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  PS_REG	scr3

/*
**	First four bytes (host)
*/
#define  actualquirks  phys.header.status[0]
#define  host_status   phys.header.status[1]
#define  scsi_status   phys.header.status[2]
#define  parity_status phys.header.status[3]

/*
**	Last four bytes (script)
*/
#define  xerr_st       header.status[4]	/* MUST be ==0 mod 4 */
#define  sync_st       header.status[5]	/* MUST be ==1 mod 4 */
#define  nego_st       header.status[6]
#define  wide_st       header.status[7]	/* MUST be ==3 mod 4 */

/*
**	Last four bytes (host)
*/
#define  xerr_status   phys.xerr_st
#define  sync_status   phys.sync_st
#define  nego_status   phys.nego_st
#define  wide_status   phys.wide_st

/*==========================================================
**
**      Declaration of structs:     Data structure block
**
**==========================================================
**
**	During execution of a ccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the ccb.
**	This substructure contains the header with
**	the script-processor-changable data and
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/

struct dsb {

	/*
	**	Header.
	**	Has to be the first entry,
	**	because it's jumped to by the
	**	script processor
	*/

	struct head	header;

	/*
	**	Table data for Script
	*/

	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove smsg2 ;
	struct scr_tblmove cmd   ;
	struct scr_tblmove scmd  ;
	struct scr_tblmove sense ;
	struct scr_tblmove data [MAX_SCATTER];
};

/*==========================================================
**
**      Declaration of structs:     Command control block.
**
**==========================================================
**
**	During execution of a ccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the ccb.
**	This substructure contains the header with
**	the script-processor-changable data and then
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/


struct ccb {
	/*
	**	This filler ensures that the global header is 
	**	cache line size aligned.
	*/
	ncrcmd	filler[4];

	/*
	**	during reselection the ncr jumps to this point.
	**	If a "SIMPLE_TAG" message was received,
	**	then SFBR is set to the tag.
	**	else SFBR is set to 0
	**	If looking for another tag, jump to the next ccb.
	**
	**	JUMP  IF (SFBR != #TAG#)
	**	@(next ccb of this lun)
	*/

	struct link		jump_ccb;

	/*
	**	After execution of this call, the return address
	**	(in  the TEMP register) points to the following
	**	data structure block.
	**	So copy it to the DSA register, and start
	**	processing of this data structure.
	**
	**	CALL
	**	<RESEL_TMP>
	*/

	struct link		call_tmp;

	/*
	**	This is the data structure which is
	**	to be executed by the script processor.
	*/

	struct dsb		phys;

	/*
	**	If a data transfer phase is terminated too early
	**	(after reception of a message (i.e. DISCONNECT)),
	**	we have to prepare a mini script to transfer
	**	the rest of the data.
	*/

	ncrcmd			patch[8];

	/*
	**	A copy of the SCSI command; this is where the script
	**	reads it from.
	*/
	struct scsi_generic	scsi_cmd;

	/*
	**	The incoming sense data comes to here, and is copied
	**	back into the scsipi_xfer.
	*/
	struct scsipi_sense_data sense_data;

	/*
	**	The general SCSI driver provides a
	**	pointer to a control block.
	*/

	struct scsipi_xfer	*xfer;
#ifdef __NetBSD__
	bus_dmamap_t		xfer_dmamap;
#endif

	/*
	**	We prepare a message to be sent after selection,
	**	and a second one to be sent after getcc selection.
	**      Contents are IDENTIFY and SIMPLE_TAG.
	**	While negotiating sync or wide transfer,
	**	a SDTM or WDTM message is appended.
	*/

	u_char			scsi_smsg [8];
	u_char			scsi_smsg2[8];

	/*
	**	Lock this ccb.
	**	Flag is used while looking for a free ccb.
	*/

	u_long		magic;

	/*
	**	Physical address of this instance of ccb
	*/

	u_long		p_ccb;

	/*
	**	Completion time out for this job.
	**	It's set to time of start + allowed number of seconds.
	*/

	u_long		tlimit;

	/*
	**	All ccbs of one hostadapter are chained.
	*/

	ccb_p		link_ccb;

	/*
	**	All ccbs of one target/lun are chained.
	*/

	ccb_p		next_ccb;

	/*
	**	Sense command
	*/

	u_char		sensecmd[6];

	/*
	**	Tag for this transfer.
	**	It's patched into jump_ccb.
	**	If it's not zero, a SIMPLE_TAG
	**	message is included in smsg.
	*/

	u_char			tag;
};

#define CCB_PHYS(cp,lbl)	(cp->p_ccb + offsetof(struct ccb, lbl))

/*==========================================================
**
**      Declaration of structs:     NCR device descriptor
**
**==========================================================
*/

/*
 * These variables are softc variables that the script must access, and
 * thus must be in DMA-safe memory.  The comments above them associate them
 * with sections of the rest of the ncb.
 */
struct ncr_softc_dma {
	/*
	 * The global header.  Accessible to both the host and
	 * the script processor.  We assume it is cache line size
	 * aligned.
	 */
	struct head	header;

	/*
	 * During reselection, the NCR jumps to this point.  The
	 * SFBR register is loaded with the encoded target ID.
	 *
	 * Jump to the first target.
	 *
	 * JUMP
	 * @(next tcb)
	 */
	struct link	jump_tcb;

	/*
	 * Profiling data.
	 */
	u_long		disc_phys;

	/*
	 * Timeout handler.
	 */
	u_long		heartbeat;

	/*
	 * Start queue.
	 */
	u_int32_t	squeue[MAX_START];

	/*
	 * Message buffers.  Should be longword aligned, because they're
	 * written with a COPY script command.
	 */
	u_char		msgout[8];
	u_char		msgin[8];
	u_int32_t	lastmsg;

	/*
	 * Buffer for STATUS_IN phase.
	 */
	u_char		scratch;
};

struct ncb {
#ifdef __NetBSD__
	struct device sc_dev;
	pci_chipset_tag_t sc_pc;
	void *sc_ih;
	struct callout sc_timo_ch;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_ncb_dmamap;
	int sc_iomapped;
#else /* !__NetBSD__ */
	int	unit;
#endif /* __NetBSD__ */

	struct ncr_softc_dma *ncb_dma;

	/*-----------------------------------------------
	**	Configuration ..
	**-----------------------------------------------
	**
	**	virtual and physical addresses
	**	of the 53c810 chip.
	*/
#ifndef __NetBSD__
	vaddr_t     vaddr;
	paddr_t     paddr;

	vaddr_t     vaddr2;
	paddr_t     paddr2;

#else
	bus_addr_t	paddr;

	bus_space_tag_t ram_tag;
	bus_space_handle_t ram_handle;
	bus_addr_t	paddr2;
	int		scriptmapped;
#endif

#ifndef __NetBSD__
	/*
	**	pointer to the chip's registers.
	*/
	volatile
	struct ncr_reg* reg;
#endif

	/*
	**	Scripts instance virtual address.
	*/
	struct script	*script;
	struct scripth	*scripth;

#ifdef __NetBSD__
	/*
	**	DMA maps for the script instances.
	*/
	bus_dmamap_t	script_dmamap;
	bus_dmamap_t	scripth_dmamap;

	/*
	**	Scripts instance physical address.
	*/
	bus_addr_t	p_script;
	bus_addr_t	p_scripth;
#else
	/*
	**	Scripts instance physical address.
	*/
	u_long		p_script;
	u_long		p_scripth;
#endif /* __NetBSD__ */

	/*
	**	The SCSI address of the host adapter.
	*/
	u_char		myaddr;

	/*
	**	timing parameters
	*/
	u_char		minsync;	/* Minimum sync period factor	*/
	u_char		maxsync;	/* Maximum sync period factor	*/
	u_char		maxoffs;	/* Max scsi offset		*/
	u_char		clock_divn;	/* Number of clock divisors	*/
	u_long		clock_khz;	/* SCSI clock frequency in KHz	*/
	u_long		features;	/* Chip features map		*/
	u_char		multiplier;	/* Clock multiplier (1,2,4)	*/

	u_char		maxburst;	/* log base 2 of dwords burst	*/

	/*
	**	BIOS supplied PCI bus options
	*/
	u_char		rv_scntl3;
	u_char		rv_dcntl;
	u_char		rv_dmode;
	u_char		rv_ctest3;
	u_char		rv_ctest4;
	u_char		rv_ctest5;
	u_char		rv_gpcntl;
	u_char		rv_stest2;

	/*-----------------------------------------------
	**	Link to the generic SCSI driver
	**-----------------------------------------------
	*/
#ifdef __NetBSD__
	struct scsipi_link	sc_link;
	struct scsipi_adapter	sc_adapter;
#else
	struct scsi_link	sc_link;
#endif

	/*-----------------------------------------------
	**	Job control
	**-----------------------------------------------
	**
	**	Commands from user
	*/
	struct usrcmd	user;
	u_char		order;

	/*
	**	Target data
	*/
	struct tcb	target[MAX_TARGET];

	/*
	**	Start queue.
	*/
	u_short		squeueput;
	u_short		actccbs;

	/*
	**	Timeout handler
	*/
	u_short		ticks;
	u_short		latetime;
	u_long		lasttime;

	/*-----------------------------------------------
	**	Debug and profiling
	**-----------------------------------------------
	**
	**	register dump
	*/
	struct ncr_reg	regdump;
	struct timeval	regtime;

	/*
	**	Profiling data
	*/
	struct profile	profile;
	u_long		disc_ref;

	/*
	**	The global control block.
	**	It's used only during the configuration phase.
	**	A target control block will be created
	**	after the first successful transfer.
	**	It is allocated separately in order to insure 
	**	cache line size alignment.
	*/
	struct ccb      *ccb;

	/*
	**	controller chip dependent maximal transfer width.
	*/
	u_char		maxwide;

	/*
	**	option for M_IDENTIFY message: enables disconnecting
	*/
	u_char		disc;

#if defined(NCR_IOMAPPED) && !defined(__NetBSD__)
	/*
	**	address of the ncr control registers in io space
	*/
	u_short		port;
#endif
};

#define NCB_SCRIPT_PHYS(np,lbl)	(np->p_script + offsetof (struct script, lbl))
#define NCB_SCRIPTH_PHYS(np,lbl) (np->p_scripth + offsetof (struct scripth,lbl))

/*==========================================================
**
**
**      Script for NCR-Processor.
**
**	Use ncr_script_fill() to create the variable parts.
**	Use ncr_script_copy_and_bind() to make a copy and
**	bind to physical addresses.
**
**
**==========================================================
**
**	We have to know the offsets of all labels before
**	we reach them (for forward jumps).
**	Therefore we declare a struct here.
**	If you make changes inside the script,
**	DONT FORGET TO CHANGE THE LENGTHS HERE!
**
**----------------------------------------------------------
*/

/*
**	Script fragments which are loaded into the on-board RAM 
**	of 825A, 875 and 895 chips.
*/
struct script {
	ncrcmd	start		[  7];
	ncrcmd	start0		[  2];
	ncrcmd	start1		[  3];
	ncrcmd  startpos	[  1];
	ncrcmd  trysel		[  8];
	ncrcmd	skip		[  8];
	ncrcmd	skip2		[  3];
	ncrcmd  idle		[  2];
	ncrcmd	select		[ 22];
	ncrcmd	prepare		[  4];
	ncrcmd	loadpos		[ 14];
	ncrcmd	prepare2	[ 24];
	ncrcmd	setmsg		[  5];
	ncrcmd  clrack		[  2];
	ncrcmd  dispatch	[ 33];
	ncrcmd	no_data		[ 17];
	ncrcmd  checkatn	[ 10];
	ncrcmd  command		[ 15];
	ncrcmd  status		[ 27];
	ncrcmd  msg_in		[ 26];
	ncrcmd  msg_bad		[  6];
	ncrcmd  complete	[ 13];
	ncrcmd	cleanup		[ 12];
	ncrcmd	cleanup0	[ 9];
	ncrcmd	signal		[ 12];
	ncrcmd  save_dp		[  5];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 12];
	ncrcmd  disconnect0	[  5];
	ncrcmd  disconnect1	[ 23];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd  badgetcc	[  6];
	ncrcmd	reselect	[  8];
	ncrcmd	reselect1	[  8];
	ncrcmd	reselect2	[  8];
	ncrcmd	resel_tmp	[  5];
	ncrcmd  resel_lun	[ 18];
	ncrcmd	resel_tag	[ 24];
	ncrcmd  data_in		[MAX_SCATTER * 4 + 7];
	ncrcmd  data_out	[MAX_SCATTER * 4 + 7];
};

/*
**	Script fragments which stay in main memory for all chips.
*/
struct scripth {
	ncrcmd  tryloop		[MAX_START*5+2];
	ncrcmd  msg_parity	[  6];
	ncrcmd	msg_reject	[  8];
	ncrcmd	msg_ign_residue	[ 32];
	ncrcmd  msg_extended	[ 18];
	ncrcmd  msg_ext_2	[ 18];
	ncrcmd	msg_wdtr	[ 27];
	ncrcmd  msg_ext_3	[ 18];
	ncrcmd	msg_sdtr	[ 27];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  getcc		[  4];
	ncrcmd  getcc1		[  5];
#ifdef NCR_GETCC_WITHMSG
	ncrcmd	getcc2		[ 33];
#else
	ncrcmd	getcc2		[ 14];
#endif
	ncrcmd	getcc3		[ 10];
	ncrcmd	aborttag	[  4];
	ncrcmd	abort		[ 22];
	ncrcmd	snooptest	[  9];
	ncrcmd	snoopend	[  2];
};

#ifdef NCR_TEKRAM_EEPROM
struct tekram_eeprom_dev {
  u_char	devmode;
#define	TKR_PARCHK	0x01
#define	TKR_TRYSYNC	0x02
#define	TKR_ENDISC	0x04
#define	TKR_STARTUNIT	0x08
#define	TKR_USETAGS	0x10
#define	TKR_TRYWIDE	0x20
  u_char	syncparam;	/* max. sync transfer rate (table ?) */
  u_char	filler1;
  u_char	filler2;
};

struct tekram_eeprom {
  struct tekram_eeprom_dev 
		dev[16];
  u_char	adaptid;
  u_char	adaptmode;
#define	TKR_ADPT_GT2DRV	0x01
#define	TKR_ADPT_GT1GB	0x02
#define	TKR_ADPT_RSTBUS	0x04
#define	TKR_ADPT_ACTNEG	0x08
#define	TKR_ADPT_NOSEEK	0x10
#define	TKR_ADPT_MORLUN	0x20
  u_char	delay;		/* unit ? (table ???) */
  u_char	tags;		/* use 4 times as many ... */
  u_char	filler[60];
};
#endif

/*==========================================================
**
**
**      Function headers.
**
**
**==========================================================
*/

#ifdef KERNEL
static	int	ncr_ccb_dma_init(ncb_p np, ccb_p cp);
static	void	ncr_alloc_ccb	(ncb_p np, u_long target, u_long lun);
static	void	ncr_complete	(ncb_p np, ccb_p cp);
static	int	ncr_delta	(struct timeval * from, struct timeval * to);
static	void	ncr_exception	(ncb_p np);
static	void	ncr_free_ccb	(ncb_p np, ccb_p cp, int flags);
static	void	ncr_selectclock	(ncb_p np, u_char scntl3);
static	void	ncr_getclock	(ncb_p np, u_char multiplier);
static	ccb_p	ncr_get_ccb	(ncb_p np, u_long flags, u_long t,u_long l);
static	void	ncr_init	(ncb_p np, char * msg, u_long code);
#ifdef __NetBSD__
static	int	ncr_intr	(void *vnp);
#else
static	void	ncr_intr	(void *vnp);
static  U_INT32 ncr_info	(int unit);
#endif	/* !__NetBSD__ */	
static	void	ncr_int_ma	(ncb_p np, u_char dstat);
static	void	ncr_int_sir	(ncb_p np);
static  void    ncr_int_sto     (ncb_p np);
#ifdef __NetBSD__
static	u_long	ncr_lookup	(char* id);
static	void	ncr_minphys	(struct buf *bp);
#else
static	void	ncr_min_phys	(struct buf *bp);
#endif
static	void	ncr_negotiate	(struct ncb* np, struct tcb* tp);
static	void	ncr_opennings	(ncb_p np, lcb_p lp, struct scsipi_xfer * xp);
static	void	ncb_profile	(ncb_p np, ccb_p cp);
static	void	ncr_script_copy_and_bind
				(ncb_p np, ncrcmd *src, ncrcmd *dst, int len);
static  void    ncr_script_fill (struct script * scr, struct scripth *scrh);
static	int	ncr_scatter	(ncb_p np, ccb_p, vaddr_t vaddr,
				 vsize_t datalen);
static	void	ncr_setmaxtags	(tcb_p tp, u_long usrtags);
static	void	ncr_getsync	(ncb_p np, u_char sfac, u_char *fakp,
				 u_char *scntl3p);
static	void	ncr_setsync	(ncb_p np, ccb_p cp,u_char scntl3,u_char sxfer);
static	void	ncr_settags     (tcb_p tp, lcb_p lp, u_long usrtags);
static	void	ncr_setwide	(ncb_p np, ccb_p cp, u_char wide, u_char ack);
static	int	ncr_show_msg	(u_char * msg);
static	int	ncr_snooptest	(ncb_p np);
static	INT32	ncr_start       (struct scsipi_xfer *xp);
static	void	ncr_timeout	(void *arg);
static	void	ncr_usercmd	(ncb_p np);
static  void    ncr_wakeup      (ncb_p np, u_long code);

#ifdef __NetBSD__
static	int	ncr_probe	(struct device *, struct cfdata *, void *);
static	void	ncr_attach	(struct device *, struct device *, void *);
#else /* !__NetBSD__ */
static  char*	ncr_probe       (pcici_t tag, pcidi_t type);
static	void	ncr_attach	(pcici_t tag, int unit);
#endif /* __NetBSD__ */

#ifdef NCR_TEKRAM_EEPROM
static	int	read_tekram_eeprom
				(ncb_p np, struct tekram_eeprom *buffer);
#endif

#endif /* KERNEL */

/*==========================================================
**
**
**      Global static data.
**
**
**==========================================================
*/


#if 0
static char ident[] =
	"\n$NetBSD: ncr.c,v 1.87.2.6 2001/01/15 09:27:43 bouyer Exp $\n";
#endif

static const u_long	ncr_version = NCR_VERSION	* 11
	+ (u_long) sizeof (struct ncb)	*  7
	+ (u_long) sizeof (struct ccb)	*  5
	+ (u_long) sizeof (struct lcb)	*  3
	+ (u_long) sizeof (struct tcb)	*  2;

#ifdef KERNEL

#ifndef __NetBSD__
static const int nncr=MAX_UNITS;	/* XXX to be replaced by SYSCTL */
ncb_p         ncrp [MAX_UNITS];		/* XXX to be replaced by SYSCTL */
#endif /* !__NetBSD__ */

static int ncr_debug = SCSI_NCR_DEBUG;
#ifndef __NetBSD__
SYSCTL_INT(_debug, OID_AUTO, ncr_debug, CTLFLAG_RW, &ncr_debug, 0, "");
#endif /* !__NetBSD__ */

static int ncr_cache; /* to be aligned _NOT_ static */

/*==========================================================
**
**
**      Global static data:	auto configure
**
**
**==========================================================
*/

#define	NCR_810_ID	(0x00011000ul)
#define	NCR_815_ID	(0x00041000ul)
#define	NCR_820_ID	(0x00021000ul)
#define	NCR_825_ID	(0x00031000ul)
#define	NCR_860_ID	(0x00061000ul)
#define	NCR_875_ID	(0x000f1000ul)
#define	NCR_875_ID2	(0x008f1000ul)
#define	NCR_885_ID	(0x000d1000ul)
#define	NCR_895_ID	(0x000c1000ul)
#define	NCR_896_ID	(0x000b1000ul)

#ifdef __NetBSD__

struct	cfattach ncr_ca = {
	sizeof(struct ncb), ncr_probe, ncr_attach
};

#else /* !__NetBSD__ */

static u_long ncr_count;

static struct	pci_device ncr_device = {
	"ncr",
	ncr_probe,
	ncr_attach,
	&ncr_count,
	NULL
};

DATA_SET (pcidevice_set, ncr_device);

#endif /* !__NetBSD__ */

#ifndef __NetBSD__
static struct scsipi_adapter ncr_switch =
{
	ncr_start,
	ncr_min_phys,
	0,
	0,
	ncr_info,
	"ncr",
	NULL,			/* scsipi_ioctl */
};
#endif /* !__NetBSD__ */

static struct scsipi_device ncr_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
#ifndef __NetBSD__
	"ncr",
#endif /* !__NetBSD__ */
};

#ifdef __NetBSD__

#define	ncr_name(np)	(np->sc_dev.dv_xname)

#else /* !__NetBSD__ */

static char *ncr_name (ncb_p np)
{
	static char name[10];
	sprintf(name, "ncr%d", np->unit);
	return (name);
}
#endif

/*==========================================================
**
**
**      Scripts for NCR-Processor.
**
**      Use ncr_script_bind for binding to physical addresses.
**
**
**==========================================================
**
**	NADDR generates a reference to a field of the controller data.
**	PADDR generates a reference to another part of the script.
**	RADDR generates a reference to a script processor register.
**	FADDR generates a reference to a script processor register
**		with offset.
**
**----------------------------------------------------------
*/

#define	RELOC_SOFTC	0x40000000
#define	RELOC_LABEL	0x50000000
#define	RELOC_REGISTER	0x60000000
#define	RELOC_KVAR	0x70000000
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncr_softc_dma, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct scripth, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#define	KVAR(which)	(RELOC_KVAR | (which))

#define KVAR_TIME_TV_SEC		(0)
#define KVAR_TIME			(1)
#define KVAR_NCR_CACHE			(2)

#define	SCRIPT_KVAR_FIRST		(0)
#define	SCRIPT_KVAR_LAST		(3)

/*
 * Kernel variables referenced in the scripts.
 * THESE MUST ALL BE ALIGNED TO A 4-BYTE BOUNDARY.
 */
#ifdef __NetBSD__
static unsigned long script_kvars[] = {
	(unsigned long)&mono_time.tv_sec,
	(unsigned long)&mono_time,
	(unsigned long)&ncr_cache,
};
#else
static void *script_kvars[] =
	{ &time.tv_sec, &time, &ncr_cache };
#endif

static	struct script script0 = {
/*--------------------------< START >-----------------------*/ {
	/*
	**	Claim to be still alive ...
	*/
	SCR_COPY (sizeof (((struct ncr_softc_dma *)0)->heartbeat)),
		KVAR (KVAR_TIME_TV_SEC),
		NADDR (heartbeat),
	/*
	**      Make data structure address invalid.
	**      clear SIGP.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_FROM_REG (ctest2),
		0,
}/*-------------------------< START0 >----------------------*/,{
	/*
	**	Hook for interrupted GetConditionCode.
	**	Will be patched to ... IFTRUE by
	**	the interrupt handler.
	*/
	SCR_INT ^ IFFALSE (0),
		SIR_SENSE_RESTART,

}/*-------------------------< START1 >----------------------*/,{
	/*
	**	Hook for stalled start queue.
	**	Will be patched to IFTRUE by the interrupt handler.
	*/
	SCR_INT ^ IFFALSE (0),
		SIR_STALL_RESTART,
	/*
	**	Then jump to a certain point in tryloop.
	**	Due to the lack of indirect addressing the code
	**	is self modifying here.
	*/
	SCR_JUMP,
}/*-------------------------< STARTPOS >--------------------*/,{
		PADDRH(tryloop),

}/*-------------------------< TRYSEL >----------------------*/,{
	/*
	**	Now:
	**	DSA: Address of a Data Structure
	**	or   Address of the IDLE-Label.
	**
	**	TEMP:	Address of a script, which tries to
	**		start the NEXT entry.
	**
	**	Save the TEMP register into the SCRATCHA register.
	**	Then copy the DSA to TEMP and RETURN.
	**	This is kind of an indirect jump.
	**	(The script processor has NO stack, so the
	**	CALL is actually a jump and link, and the
	**	RETURN is an indirect jump.)
	**
	**	If the slot was empty, DSA contains the address
	**	of the IDLE part of this script. The processor
	**	jumps to IDLE and waits for a reselect.
	**	It will wake up and try the same slot again
	**	after the SIGP bit becomes set by the host.
	**
	**	If the slot was not empty, DSA contains
	**	the address of the phys-part of a ccb.
	**	The processor jumps to this address.
	**	phys starts with head,
	**	head starts with launch,
	**	so actually the processor jumps to
	**	the lauch part.
	**	If the entry is scheduled for execution,
	**	then launch contains a jump to SELECT.
	**	If it's not scheduled, it contains a jump to IDLE.
	*/
	SCR_COPY (4),
		RADDR (temp),
		RADDR (scratcha),
	SCR_COPY (4),
		RADDR (dsa),
		RADDR (temp),
	SCR_RETURN,
		0

}/*-------------------------< SKIP >------------------------*/,{
	/*
	**	This entry has been canceled.
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (scratcha),
		PADDR (startpos),
	/*
	**	patch the launch field.
	**	should look like an idle process.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (skip2),
	SCR_COPY (8),
		PADDR (idle),
}/*-------------------------< SKIP2 >-----------------------*/,{
		0,
	SCR_JUMP,
		PADDR(start),
}/*-------------------------< IDLE >------------------------*/,{
	/*
	**	Nothing to do?
	**	Wait for reselect.
	*/
	SCR_JUMP,
		PADDR(reselect),

}/*-------------------------< SELECT >----------------------*/,{
	/*
	**	DSA	contains the address of a scheduled
	**		data structure.
	**
	**	SCRATCHA contains the address of the script,
	**		which starts the next entry.
	**
	**	Set Initiator mode.
	**
	**	(Target mode is left as an exercise for the reader)
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, 0xff),
		0,

	/*
	**      And try to select this target.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (reselect),

	/*
	**	Now there are 4 possibilities:
	**
	**	(1) The ncr looses arbitration.
	**	This is ok, because it will try again,
	**	when the bus becomes idle.
	**	(But beware of the timeout function!)
	**
	**	(2) The ncr is reselected.
	**	Then the script processor takes the jump
	**	to the RESELECT label.
	**
	**	(3) The ncr completes the selection.
	**	Then it will execute the next statement.
	**
	**	(4) There is a selection timeout.
	**	Then the ncr should interrupt the host and stop.
	**	Unfortunately, it seems to continue execution
	**	of the script. But it will fail with an
	**	IID-interrupt on the next WHEN.
	*/

	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_IN)),
		0,

	/*
	**	Save target id to ctest0 register
	*/

	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
	/*
	**	Send the IDENTIFY and SIMPLE_TAG messages
	**	(and the M_X_SYNC_REQ message)
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
#ifdef undef /* XXX better fail than try to deal with this ... */
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		-16,
#endif
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	/*
	**	Selection complete.
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (scratcha),
		PADDR (startpos),
}/*-------------------------< PREPARE >----------------------*/,{
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos),
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/

}/*-------------------------< LOADPOS >---------------------*/,{
		0,
		NADDR (header),
	/*
	**      Mark this ccb as not scheduled.
	*/
	SCR_COPY (8),
		PADDR (idle),
		NADDR (header.launch),
	/*
	**      Set a time stamp for this selection
	*/
	SCR_COPY (sizeof (struct timeval)),
		KVAR (KVAR_TIME),
		NADDR (header.stamp.select),
	/*
	**      load the savep (saved pointer) into
	**      the TEMP register (actual pointer)
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	/*
	**      Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),

}/*-------------------------< PREPARE2 >---------------------*/,{
	/*
	**      Load the synchronous mode register
	*/
	SCR_COPY (1),
		NADDR (sync_st),
		RADDR (sxfer),
	/*
	**      Load the wide mode and timing register
	*/
	SCR_COPY (1),
		NADDR (wide_st),
		RADDR (scntl3),
	/*
	**	Initialize the msgout buffer with a NOOP message.
	*/
	SCR_LOAD_REG (scratcha, M_NOOP),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgin),
	/*
	**	Message in phase ?
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	Extended or reject message ?
	*/
	SCR_FROM_REG (sbdl),
		0,
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (DATA (M_REJECT)),
		PADDRH (msg_reject),
	/*
	**	normal processing
	*/
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< SETMSG >----------------------*/,{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
}/*-------------------------< CLRACK >----------------------*/,{
	/*
	**	Terminate possible pending message phase.
	*/
	SCR_CLR (SCR_ACK),
		0,

}/*-----------------------< DISPATCH >----------------------*/,{
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	/*
	**	remove bogus output signals
	*/
	SCR_REG_REG (socl, SCR_AND, CACK|CATN),
		0,
	SCR_RETURN ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		0,
	SCR_RETURN ^ IFTRUE (IF (SCR_DATA_IN)),
		0,
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDR (msg_out),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
	/*
	**      Discard one illegal phase byte, if required.
	*/
	SCR_LOAD_REG (scratcha, XE_BAD_PHASE),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		NADDR (scratch),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< NO_DATA >--------------------*/,{
	/*
	**	The target wants to tranfer too much data
	**	or in the wrong direction.
	**      Remember that in extended error.
	*/
	SCR_LOAD_REG (scratcha, XE_EXTRA_DATA),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	/*
	**      Discard one data byte, if required.
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	/*
	**      .. and repeat as required.
	*/
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*-------------------------< CHECKATN >--------------------*/,{
	/*
	**	If AAP (bit 1 of scntl0 register) is set
	**	and a parity error is detected,
	**	the script processor asserts ATN.
	**
	**	The target should switch to a MSG_OUT phase
	**	to get the message.
	*/
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFFALSE (MASK (CATN, CATN)),
		PADDR (dispatch),
	/*
	**	count it
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 1),
		0,
	/*
	**	Prepare a M_ID_ERROR message
	**	(initiator detected error).
	**	The target should retry the transfer.
	*/
	SCR_LOAD_REG (scratcha, M_ID_ERROR),
		0,
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< COMMAND >--------------------*/,{
	/*
	**	If this is not a GETCC transfer ...
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFTRUE (DATA (S_CHECK_COND)),
		28,
	/*
	**	... set a timestamp ...
	*/
	SCR_COPY (sizeof (struct timeval)),
		KVAR (KVAR_TIME),
		NADDR (header.stamp.command),
	/*
	**	... and send the command
	*/
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
	SCR_JUMP,
		PADDR (dispatch),
	/*
	**	Send the GETCC command
	*/
/*>>>*/	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, scmd),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< STATUS >--------------------*/,{
	/*
	**	set the timestamp.
	*/
	SCR_COPY (sizeof (struct timeval)),
		KVAR (KVAR_TIME),
		NADDR (header.stamp.status),
	/*
	**	If this is a GETCC transfer,
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (S_CHECK_COND)),
		40,
	/*
	**	get the status
	*/
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	/*
	**	Save status to scsi_status.
	**	Mark as complete.
	**	And wait for disconnect.
	*/
	SCR_TO_REG (SS_REG),
		0,
	SCR_REG_REG (SS_REG, SCR_OR, S_SENSE),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (checkatn),
	/*
	**	If it was no GETCC transfer,
	**	save the status to scsi_status.
	*/
/*>>>*/	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	SCR_TO_REG (SS_REG),
		0,
	/*
	**	if it was no check condition ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (S_CHECK_COND)),
		PADDR (checkatn),
	/*
	**	... mark as complete.
	*/
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (checkatn),

}/*-------------------------< MSG_IN >--------------------*/,{
	/*
	**	Get the first byte of the message
	**	and save it to SCRATCHA.
	**
	**	The script processor doesn't negate the
	**	ACK signal after this transfer.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	**	Parity was ok, handle this message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (M_SAVE_DP)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_RESTORE_DP)),
		PADDR (restore_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDRH (msg_extended),
	SCR_JUMP ^ IFTRUE (DATA (M_NOOP)),
		PADDR (clrack),
	SCR_JUMP ^ IFTRUE (DATA (M_REJECT)),
		PADDRH (msg_reject),
	SCR_JUMP ^ IFTRUE (DATA (M_IGN_RESIDUE)),
		PADDRH (msg_ign_residue),
	/*
	**	Rest of the messages left as
	**	an exercise ...
	**
	**	Unimplemented messages:
	**	fall through to MSG_BAD.
	*/
}/*-------------------------< MSG_BAD >------------------*/,{
	/*
	**	unimplemented message - reject it.
	*/
	SCR_INT,
		SIR_REJECT_SENT,
	SCR_LOAD_REG (scratcha, M_REJECT),
		0,
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
	**	If it's not the get condition code,
	**	copy TEMP register to LASTP in header.
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFTRUE (MASK (S_SENSE, S_SENSE)),
		12,
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.lastp),
/*>>>*/	/*
	**	When we terminate the cycle by clearing ACK,
	**	the target may disconnect immediately.
	**
	**	We don't want to be told of an
	**	"unexpected disconnect",
	**	so we disable this feature.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	/*
	**	Terminate cycle ...
	*/
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	... and wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
}/*-------------------------< CLEANUP >-------------------*/,{
	/*
	**      dsa:    Pointer to ccb
	**	      or xxxxxxFF (no ccb)
	**
	**      HS_REG:   Host-Status (<>0!)
	*/
	SCR_FROM_REG (dsa),
		0,
	SCR_JUMP ^ IFTRUE (DATA (0xff)),
		PADDR (signal),
	/*
	**      dsa is valid.
	**	save the status registers
	*/
	SCR_COPY (4),
		RADDR (scr0),
		NADDR (header.status),
	/*
	**	and copy back the header to the ccb.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (cleanup0),
	SCR_COPY (sizeof (struct head)),
		NADDR (header),
}/*-------------------------< CLEANUP0 >--------------------*/,{
		0,

	/*
	**	If command resulted in "check condition"
	**	status and is not yet completed,
	**	try to get the condition code.
	*/
	SCR_FROM_REG (HS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (0, HS_DONEMASK)),
		16,
	SCR_FROM_REG (SS_REG),
		0,
	SCR_JUMP ^ IFTRUE (DATA (S_CHECK_COND)),
		PADDRH(getcc2),
}/*-------------------------< SIGNAL >----------------------*/,{
	/*
	**	if status = queue full,
	**	reinsert in startqueue and stall queue.
	*/
/*>>>*/	SCR_FROM_REG (SS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (S_QUEUE_FULL)),
		SIR_STALL_QUEUE,
	/*
	**	And make the DSA register invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff), /* invalid */
		0,
	/*
	**	if job completed ...
	*/
	SCR_FROM_REG (HS_REG),
		0,
	/*
	**	... signal completion to the host
	*/
	SCR_INT_FLY ^ IFFALSE (MASK (0, HS_DONEMASK)),
		0,
	/*
	**	Auf zu neuen Schandtaten!
	*/
	SCR_JUMP,
		PADDR(start),

}/*-------------------------< SAVE_DP >------------------*/,{
	/*
	**	SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	SCR_JUMP,
		PADDR (clrack),
}/*-------------------------< RESTORE_DP >---------------*/,{
	/*
	**	RESTORE_DP message:
	**	Copy SAVEP in header to TEMP register.
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< DISCONNECT >---------------*/,{
	/*
	**	If QUIRK_AUTOSAVE is set,
	**	do an "save pointer" operation.
	*/
	SCR_FROM_REG (QU_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (QUIRK_AUTOSAVE, QUIRK_AUTOSAVE)),
		12,
	/*
	**	like SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
/*>>>*/	/*
	**	Check if temp==savep or temp==goalp:
	**	if not, log a missing save pointer message.
	**	In fact, it's a comparison mod 256.
	**
	**	Hmmm, I hadn't thought that I would be urged to
	**	write this kind of ugly self modifying code.
	**
	**	It's unbelievable, but the ncr53c8xx isn't able
	**	to subtract one register from another.
	*/
	SCR_FROM_REG (temp),
		0,
	/*
	**	You are not expected to understand this ..
	**
	**	CAUTION: only little endian architectures supported! XXX
	*/
	SCR_COPY_F (1),
		NADDR (header.savep),
		PADDR (disconnect0),
}/*-------------------------< DISCONNECT0 >--------------*/,{
/*<<<*/	SCR_JUMPR ^ IFTRUE (DATA (1)),
		20,
	/*
	**	neither this
	*/
	SCR_COPY_F (1),
		NADDR (header.goalp),
		PADDR (disconnect1),
}/*-------------------------< DISCONNECT1 >--------------*/,{
	SCR_INT ^ IFFALSE (DATA (1)),
		SIR_MISSING_SAVE,
/*>>>*/

	/*
	**	DISCONNECTing  ...
	**
	**	disable the "unexpected disconnect" feature,
	**	and remove the ACK signal.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	Wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
	/*
	**	Profiling:
	**	Set a time stamp,
	**	and count the disconnects.
	*/
	SCR_COPY (sizeof (struct timeval)),
		KVAR (KVAR_TIME),
		NADDR (header.stamp.disconnect),
	SCR_COPY (4),
		NADDR (disc_phys),
		RADDR (temp),
	SCR_REG_REG (temp, SCR_ADD, 0x01),
		0,
	SCR_COPY (4),
		RADDR (temp),
		NADDR (disc_phys),
	/*
	**	Status is: DISCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_JUMP,
		PADDR (cleanup),

}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	**	The target requests a message.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	/*
	**	If it was no ABORT message ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_ABORT)),
		PADDRH (msg_out_abort),
	/*
	**	... wait for the next phase
	**	if it's a message out, send it again, ...
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR (msg_out),
}/*-------------------------< MSG_OUT_DONE >--------------*/,{
	/*
	**	... else clear the message ...
	*/
	SCR_LOAD_REG (scratcha, M_NOOP),
		0,
	SCR_COPY (4),
		RADDR (scratcha),
		NADDR (msgout),
	/*
	**	... and process the next phase
	*/
	SCR_JUMP,
		PADDR (dispatch),

}/*------------------------< BADGETCC >---------------------*/,{
	/*
	**	If SIGP was set, clear it and try again.
	*/
	SCR_FROM_REG (ctest2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CSIGP,CSIGP)),
		PADDRH (getcc2),
	SCR_INT,
		SIR_SENSE_FAILED,
}/*-------------------------< RESELECT >--------------------*/,{
	/*
	**	This NOP will be patched with LED OFF
	**	SCR_REG_REG (gpreg, SCR_OR, 0x01)
	*/
	SCR_NO_OP,
		0,

	/*
	**	make the DSA invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_CLR (SCR_TRG),
		0,
	/*
	**	Sleep waiting for a reselection.
	**	If SIGP is set, special treatment.
	**
	**	Zu allem bereit ..
	*/
	SCR_WAIT_RESEL,
		PADDR(reselect2),
}/*-------------------------< RESELECT1 >--------------------*/,{
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**	... zu nichts zu gebrauchen ?
	**
	**      load the target id into the SFBR
	**	and jump to the control block.
	**
	**	Look at the declarations of
	**	- struct ncb
	**	- struct tcb
	**	- struct lcb
	**	- struct ccb
	**	to understand what's going on.
	*/
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (ctest0),
		0,
	SCR_JUMP,
		NADDR (jump_tcb),
}/*-------------------------< RESELECT2 >-------------------*/,{
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**	If it's not connected :(
	**	-> interrupted by SIGP bit.
	**	Jump to start.
	*/
	SCR_FROM_REG (ctest2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CSIGP,CSIGP)),
		PADDR (start),
	SCR_JUMP,
		PADDR (reselect),

}/*-------------------------< RESEL_TMP >-------------------*/,{
	/*
	**	The return address in TEMP
	**	is in fact the data structure address,
	**	so copy it to the DSA register.
	*/
	SCR_COPY (4),
		RADDR (temp),
		RADDR (dsa),
	SCR_JUMP,
		PADDR (prepare),

}/*-------------------------< RESEL_LUN >-------------------*/,{
	/*
	**	come back to this point
	**	to get an IDENTIFY message
	**	Wait for a msg_in phase.
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		48,
	/*
	**	message phase
	**	It's not a sony, it's a trick:
	**	read the data without acknowledging it.
	*/
	SCR_FROM_REG (sbdl),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (M_IDENTIFY, 0x98)),
		32,
	/*
	**	It WAS an Identify message.
	**	get it and ack it!
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	Mask out the lun.
	*/
	SCR_REG_REG (sfbr, SCR_AND, 0x07),
		0,
	SCR_RETURN,
		0,
	/*
	**	No message phase or no IDENTIFY message:
	**	return 0.
	*/
/*>>>*/	SCR_LOAD_SFBR (0),
		0,
	SCR_RETURN,
		0,

}/*-------------------------< RESEL_TAG >-------------------*/,{
	/*
	**	come back to this point
	**	to get a SIMPLE_TAG message
	**	Wait for a MSG_IN phase.
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		64,
	/*
	**	message phase
	**	It's a trick - read the data
	**	without acknowledging it.
	*/
	SCR_FROM_REG (sbdl),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (M_SIMPLE_TAG)),
		48,
	/*
	**	It WAS a SIMPLE_TAG message.
	**	get it and ack it!
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	Wait for the second byte (the tag)
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		24,
	/*
	**	Get it and ack it!
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_CLR (SCR_ACK|SCR_CARRY),
		0,
	SCR_RETURN,
		0,
	/*
	**	No message phase or no SIMPLE_TAG message
	**	or no second byte: return 0.
	*/
/*>>>*/	SCR_LOAD_SFBR (0),
		0,
	SCR_SET (SCR_CARRY),
		0,
	SCR_RETURN,
		0,

}/*-------------------------< DATA_IN >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTER parameter,
**	it is filled in at runtime.
**
**	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
**		PADDR (no_data),
**	SCR_COPY (sizeof (struct timeval)),
**		KVAR (KVAR_TIME),
**		NADDR (header.stamp.data),
**	SCR_MOVE_TBL ^ SCR_DATA_IN,
**		offsetof (struct dsb, data[ 0]),
**
**  ##===========< i=1; i<MAX_SCATTER >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
**  ||		PADDR (checkatn),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**	SCR_CALL,
**		PADDR (checkatn),
**	SCR_JUMP,
**		PADDR (no_data),
*/
0
}/*-------------------------< DATA_OUT >-------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTER parameter,
**	it is filled in at runtime.
**
**	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**		PADDR (no_data),
**	SCR_COPY (sizeof (struct timeval)),
**		KVAR (KVAR_TIME),
**		NADDR (header.stamp.data),
**	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**		offsetof (struct dsb, data[ 0]),
**
**  ##===========< i=1; i<MAX_SCATTER >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**	SCR_CALL,
**		PADDR (dispatch),
**	SCR_JUMP,
**		PADDR (no_data),
**
**---------------------------------------------------------
*/
#ifdef __NetBSD__
0
#else
(u_long)&ident
#endif

}/*--------------------------------------------------------*/
};


static	struct scripth scripth0 = {
/*-------------------------< TRYLOOP >---------------------*/{
/*
**	Load an entry of the start queue into dsa
**	and try to start it by jumping to TRYSEL.
**
**	Because the size depends on the
**	#define MAX_START parameter, it is filled
**	in at runtime.
**
**-----------------------------------------------------------
**
**  ##===========< I=0; i<MAX_START >===========
**  ||	SCR_COPY (4),
**  ||		NADDR (squeue[i]),
**  ||		RADDR (dsa),
**  ||	SCR_CALL,
**  ||		PADDR (trysel),
**  ##==========================================
**
**	SCR_JUMP,
**		PADDRH(tryloop),
**
**-----------------------------------------------------------
*/
0
}/*-------------------------< MSG_PARITY >---------------*/,{
	/*
	**	count it
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 0x01),
		0,
	/*
	**	send a "message parity error" message.
	*/
	SCR_LOAD_REG (scratcha, M_PARITY),
		0,
	SCR_JUMP,
		PADDR (setmsg),
}/*-------------------------< MSG_REJECT >---------------*/,{
	/*
	**	If a negotiation was in progress,
	**	negotiation failed.
	*/
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	/*
	**	else make host log this message
	*/
	SCR_INT ^ IFFALSE (DATA (HS_NEGOTIATE)),
		SIR_REJECT_RECEIVED,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_IGN_RESIDUE >----------*/,{
	/*
	**	Terminate cycle
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get residue size.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	**	Size is 0 .. ignore message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDR (clrack),
	/*
	**	Size is not 1 .. have to interrupt.
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40,
	/*
	**	Check for residue byte in swide register
	*/
	SCR_FROM_REG (scntl2),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	/*
	**	There IS data in the swide register.
	**	Discard it.
	*/
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDR (clrack),
	/*
	**	Load again the size to the sfbr register.
	*/
/*>>>*/	SCR_FROM_REG (scratcha),
		0,
/*>>>*/	SCR_INT,
		SIR_IGN_RESIDUE,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_EXTENDED >-------------*/,{
	/*
	**	Terminate cycle
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get length.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	*/
	SCR_JUMP ^ IFTRUE (DATA (3)),
		PADDRH (msg_ext_3),
	SCR_JUMP ^ IFFALSE (DATA (2)),
		PADDR (msg_bad),
}/*-------------------------< MSG_EXT_2 >----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get extended message code.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMP ^ IFTRUE (DATA (M_X_WIDE_REQ)),
		PADDRH (msg_wdtr),
	/*
	**	unknown extended message
	*/
	SCR_JUMP,
		PADDR (msg_bad)
}/*-------------------------< MSG_WDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get data bus width
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_WIDE,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,

	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_NEGO_PROTO,
	/*
	**	Send the M_X_WIDE_REQ
	*/
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< MSG_EXT_3 >----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get extended message code.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMP ^ IFTRUE (DATA (M_X_SYNC_REQ)),
		PADDRH (msg_sdtr),
	/*
	**	unknown extended message
	*/
	SCR_JUMP,
		PADDR (msg_bad)

}/*-------------------------< MSG_SDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get period and offset
	*/
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_SYNC,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,

	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_NEGO_PROTO,
	/*
	**	Send the M_X_SYNC_REQ
	*/
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< MSG_OUT_ABORT >-------------*/,{
	/*
	**	After ABORT message,
	**
	**	expect an immediate disconnect, ...
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	/*
	**	... and set the status to "ABORTED"
	*/
	SCR_LOAD_REG (HS_REG, HS_ABORTED),
		0,
	SCR_JUMP,
		PADDR (cleanup),

}/*-------------------------< GETCC >-----------------------*/,{
	/*
	**	The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can modify it.
	**
	**	We patch the address part of a COPY command
	**	with the address of the dsa register ...
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDRH (getcc1),
	/*
	**	... then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
}/*-------------------------< GETCC1 >----------------------*/,{
		0,
		NADDR (header),
	/*
	**	Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
}/*-------------------------< GETCC2 >----------------------*/,{
	/*
	**	Get the condition code from a target.
	**
	**	DSA points to a data structure.
	**	Set TEMP to the script location
	**	that receives the condition code.
	**
	**	Because there is no script command
	**	to load a longword into a register,
	**	we use a CALL command.
	*/
/*<<<*/	SCR_CALLR,
		24,
	/*
	**	Get the condition code.
	*/
	SCR_MOVE_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	/*
	**	No data phase may follow!
	*/
	SCR_CALL,
		PADDR (checkatn),
	SCR_JUMP,
		PADDR (no_data),
/*>>>*/

	/*
	**	The CALL jumps to this point.
	**	Prepare for a RESTORE_POINTER message.
	**	Save the TEMP register into the saved pointer.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	/*
	**	Load scratcha, because in case of a selection timeout,
	**	the host will expect a new value for startpos in
	**	the scratcha register.
	*/
	SCR_COPY (4),
		PADDR (startpos),
		RADDR (scratcha),
#ifdef NCR_GETCC_WITHMSG
	/*
	**	If QUIRK_NOMSG is set, select without ATN.
	**	and don't send a message.
	*/
	SCR_FROM_REG (QU_REG),
		0,
	SCR_JUMP ^ IFTRUE (MASK (QUIRK_NOMSG, QUIRK_NOMSG)),
		PADDRH(getcc3),
	/*
	**	Then try to connect to the target.
	**	If we are reselected, special treatment
	**	of the current job is required before
	**	accepting the reselection.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR(badgetcc),
	/*
	**	save target id.
	*/
	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
	/*
	**	Send the IDENTIFY message.
	**	In case of short transfer, remove ATN.
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg2),
	SCR_CLR (SCR_ATN),
		0,
	/*
	**	save the first byte of the message.
	*/
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (prepare2),

#endif
}/*-------------------------< GETCC3 >----------------------*/,{
	/*
	**	Try to connect to the target.
	**	If we are reselected, special treatment
	**	of the current job is required before
	**	accepting the reselection.
	**
	**	Silly target won't accept a message.
	**	Select without ATN.
	*/
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR(badgetcc),
	/*
	**	save target id.
	*/
	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
	/*
	**	Force error if selection timeout
	*/
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_IN)),
		0,
	/*
	**	don't negotiate.
	*/
	SCR_JUMP,
		PADDR (prepare2),
}/*-------------------------< ABORTTAG >-------------------*/,{
	/*
	**      Abort a bad reselection.
	**	Set the message to ABORT vs. ABORT_TAG
	*/
	SCR_LOAD_REG (scratcha, M_ABORT_TAG),
		0,
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
}/*-------------------------< ABORT >----------------------*/,{
	SCR_LOAD_REG (scratcha, M_ABORT),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	and send it.
	**	we expect an immediate disconnect
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< SNOOPTEST >-------------------*/,{
	/*
	**	Read the variable.
	*/
	SCR_COPY (4),
		KVAR (KVAR_NCR_CACHE),
		RADDR (scratcha),
	/*
	**	Write the variable.
	*/
	SCR_COPY (4),
		RADDR (temp),
		KVAR (KVAR_NCR_CACHE),
	/*
	**	Read back the variable.
	*/
	SCR_COPY (4),
		KVAR (KVAR_NCR_CACHE),
		RADDR (temp),
}/*-------------------------< SNOOPEND >-------------------*/,{
	/*
	**	And stop.
	*/
	SCR_INT,
		99,
}/*--------------------------------------------------------*/
};


/*==========================================================
**
**
**	Fill in #define dependent parts of the script
**
**
**==========================================================
*/

void ncr_script_fill (struct script * scr, struct scripth * scrh)
{
	int	i;
	ncrcmd	*p;

	p = scrh->tryloop;
	for (i=0; i<MAX_START; i++) {
		*p++ =SCR_COPY (4);
		*p++ =NADDR (squeue[i]);
		*p++ =RADDR (dsa);
		*p++ =SCR_CALL;
		*p++ =PADDR (trysel);
	};
	*p++ =SCR_JUMP;
	*p++ =PADDRH(tryloop);

	assert ((u_long)p == (u_long)&scrh->tryloop + sizeof (scrh->tryloop));

	p = scr->data_in;

	*p++ =SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN));
	*p++ =PADDR (no_data);
	*p++ =SCR_COPY (sizeof (struct timeval));
	*p++ =(ncrcmd) KVAR (KVAR_TIME);
	*p++ =NADDR (header.stamp.data);
	*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
	*p++ =offsetof (struct dsb, data[ 0]);

	for (i=1; i<MAX_SCATTER; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (checkatn);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	};

	*p++ =SCR_CALL;
	*p++ =PADDR (checkatn);
	*p++ =SCR_JUMP;
	*p++ =PADDR (no_data);

	assert ((u_long)p == (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scr->data_out;

	*p++ =SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_OUT));
	*p++ =PADDR (no_data);
	*p++ =SCR_COPY (sizeof (struct timeval));
	*p++ =(ncrcmd) KVAR (KVAR_TIME);
	*p++ =NADDR (header.stamp.data);
	*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
	*p++ =offsetof (struct dsb, data[ 0]);

	for (i=1; i<MAX_SCATTER; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	};

	*p++ =SCR_CALL;
	*p++ =PADDR (dispatch);
	*p++ =SCR_JUMP;
	*p++ =PADDR (no_data);

	assert ((u_long)p == (u_long)&scr->data_out + sizeof (scr->data_out));
}

/*==========================================================
**
**
**	Copy and rebind a script.
**
**
**==========================================================
*/

static void ncr_script_copy_and_bind (ncb_p np, ncrcmd *src, ncrcmd *dst, int len)
{
	ncrcmd  opcode, new, old, tmp1, tmp2;
	ncrcmd	*start, *end;
	int relocs, offset;

	start = src;
	end = src + len/4;
	offset = 0;

	while (src < end) {

		opcode = *src++;
		WRITESCRIPT_OFF(dst, offset, opcode);
		offset += 4;

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0) {
			printf ("%s: ERROR0 IN SCRIPT at %ld.\n",
				ncr_name(np), (long)(src-start-1));
			DELAY (1000000);
		};

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printf ("%p:  <%x>\n",
				(src-1), (unsigned)opcode);

		/*
		**	We don't have to decode ALL commands
		*/
		switch (opcode >> 28) {

		case 0xc:
			/*
			**	COPY has TWO arguments.
			*/
			relocs = 2;
			tmp1 = src[0];
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
			tmp2 = src[1];
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
			if ((tmp1 ^ tmp2) & 3) {
				printf ("%s: ERROR1 IN SCRIPT at %ld.\n",
					ncr_name(np), (long)(src-start-1));
				DELAY (1000000);
			}
			/*
			**	If PREFETCH feature not enabled, remove 
			**	the NO FLUSH bit if present.
			*/
			if ((opcode & SCR_NO_FLUSH) && !(np->features&FE_PFEN))
				WRITESCRIPT_OFF(dst, offset - 4,
				    (opcode & ~SCR_NO_FLUSH));
			break;

		case 0x0:
			/*
			**	MOVE (absolute address)
			*/
			relocs = 1;
			break;

		case 0x8:
			/*
			**	JUMP / CALL
			**	dont't relocate if relative :-)
			*/
			if (opcode & 0x00800000)
				relocs = 0;
			else
				relocs = 1;
			break;

		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			relocs = 1;
			break;

		default:
			relocs = 0;
			break;
		};

		if (relocs) {
			while (relocs--) {
				old = *src++;

				switch (old & RELOC_MASK) {
				case RELOC_REGISTER:
					new = (old & ~RELOC_MASK) + np->paddr;
					break;
				case RELOC_LABEL:
					new = (old & ~RELOC_MASK) + np->p_script;
					break;
				case RELOC_LABELH:
					new = (old & ~RELOC_MASK) + np->p_scripth;
					break;
				case RELOC_SOFTC:
					new = (old & ~RELOC_MASK) +
					  np->sc_ncb_dmamap->dm_segs[0].ds_addr;
					break;
				case RELOC_KVAR:
					if (((old & ~RELOC_MASK) <
					     SCRIPT_KVAR_FIRST) ||
					    ((old & ~RELOC_MASK) >
					     SCRIPT_KVAR_LAST))
						panic("ncr KVAR out of range");
					new = vtophys((vaddr_t)script_kvars[old &
					    ~RELOC_MASK]);
					break;
				case 0:
					/* Don't relocate a 0 address. */
					if (old == 0) {
						new = old;
						break;
					}
					/* fall through */
				default:
					panic("ncr_script_copy_and_bind: weird relocation %x @ %ld\n", old, (long)(src - start));
					break;
				}

				WRITESCRIPT_OFF(dst, offset, new);
				offset += 4;
			}
		} else {
			WRITESCRIPT_OFF(dst, offset, *src++);
			offset += 4;
		}

	};
}

/*==========================================================
**
**
**      Auto configuration.
**
**
**==========================================================
*/

/*----------------------------------------------------------
**
**	Reduce the transfer length to the max value
**	we can transfer safely.
**
**      Reading a block greater then MAX_SIZE from the
**	raw (character) device exercises a memory leak
**	in the vm subsystem. This is common to ALL devices.
**	We have submitted a description of this bug to
**	<FreeBSD-bugs@freefall.cdrom.com>.
**	It should be fixed in the current release.
**
**----------------------------------------------------------
*/

#ifndef __NetBSD__
void ncr_min_phys (struct  buf *bp)
{
	if ((unsigned long)bp->b_bcount > MAX_SIZE) bp->b_bcount = MAX_SIZE;
}
#else
void ncr_minphys (struct buf *bp)
{
	if(bp->b_bcount > MAX_SIZE)
		bp->b_bcount = MAX_SIZE;
	minphys(bp);
}
#endif

/*----------------------------------------------------------
**
**	Maximal number of outstanding requests per target.
**
**----------------------------------------------------------
*/

#ifndef __NetBSD__
U_INT32 ncr_info (int unit)
{
	return (1);   /* may be changed later */
}
#endif

/*----------------------------------------------------------
**
**	NCR chip devices table and chip look up function.
**	Features bit are defined in ncrreg.h. Is it the 
**	right place?
**
**----------------------------------------------------------
*/
typedef struct {
	unsigned long	device_id;
	unsigned short	minrevid;
	char	       *name;
	unsigned char	maxburst;
	unsigned char	maxoffs;
	unsigned char	clock_divn;
	unsigned int	features;
} ncr_chip;

static ncr_chip ncr_chip_table[] = {
 {NCR_810_ID, 0x00,	"ncr 53c810 fast10 scsi",		4,  8, 4,
 FE_ERL}
 ,
 {NCR_810_ID, 0x10,	"ncr 53c810a fast10 scsi",		4,  8, 4,
 FE_ERL|FE_LDSTR|FE_PFEN|FE_BOF}
 ,
 {NCR_815_ID, 0x00,	"ncr 53c815 fast10 scsi", 		4,  8, 4,
 FE_ERL|FE_BOF}
 ,
 {NCR_820_ID, 0x00,	"ncr 53c820 fast10 wide scsi", 		4,  8, 4,
 FE_WIDE|FE_ERL}
 ,
 {NCR_825_ID, 0x00,	"ncr 53c825 fast10 wide scsi",		4,  8, 4,
 FE_WIDE|FE_ERL|FE_BOF}
 ,
 {NCR_825_ID, 0x10,	"ncr 53c825a fast10 wide scsi",		7,  8, 4,
 FE_WIDE|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_860_ID, 0x00,	"ncr 53c860 fast20 scsi",		4,  8, 5,
 FE_ULTRA|FE_CLK80|FE_CACHE_SET|FE_LDSTR|FE_PFEN}
 ,
 {NCR_875_ID, 0x00,	"ncr 53c875 fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_CLK80|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_875_ID, 0x02,	"ncr 53c875 fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_875_ID2, 0x00,	"ncr 53c875j fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_885_ID, 0x00,	"ncr 53c885 fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_895_ID, 0x00,	"ncr 53c895 fast40 wide scsi",		7, 31, 7,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_896_ID, 0x00,	"ncr 53c896 fast40 wide scsi",		7, 31, 7,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
};

static int ncr_chip_lookup(u_long device_id, u_char revision_id)
{
	int i, found;
	
	found = -1;
	for (i = 0; i < sizeof(ncr_chip_table)/sizeof(ncr_chip_table[0]); i++) {
		if (device_id	== ncr_chip_table[i].device_id &&
		    ncr_chip_table[i].minrevid <= revision_id) {
			if (found < 0 || 
			    ncr_chip_table[found].minrevid 
			      < ncr_chip_table[i].minrevid) {
				found = i;
			}
		}
	}
	return found;
}

/*----------------------------------------------------------
**
**	Probe the hostadapter.
**
**----------------------------------------------------------
*/

#ifdef __NetBSD__

int
ncr_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	u_char rev = PCI_REVISION(pa->pa_class);
#if 0
	struct cfdata *cf = match;

	if (!pci_targmatch(cf, pa))
		return 0;
#endif
	if (ncr_chip_lookup(pa->pa_id, rev) < 0)
		return 0;

	return 1;
}

#else /* !__NetBSD__ */


static	char* ncr_probe (pcici_t tag, pcidi_t type)
{
	u_char rev = PCI_REVISION(pa->pa_class);
	int i;

	i = ncr_chip_lookup(type, rev);
	if (i >= 0)
		return ncr_chip_table[i].name;

	return (NULL);
}

#endif /* !__NetBSD__ */


/*==========================================================
**
**	NCR chip clock divisor table.
**	Divisors are multiplied by 10,000,000 in order to make 
**	calculations more simple.
**
**==========================================================
*/

#define _5M 5000000
static u_long div_10M[] =
	{2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};

/*===============================================================
**
**	NCR chips allow burst lengths of 2, 4, 8, 16, 32, 64, 128 
**	transfers. 32,64,128 are only supported by 875 and 895 chips.
**	We use log base 2 (burst length) as internal code, with 
**	value 0 meaning "burst disabled".
**
**===============================================================
*/

/*
 *	Burst length from burst code.
 */
#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

/*
 *	Burst code from io register bits.
 */
#define burst_code(dmode, ctest4, ctest5) \
	(ctest4) & 0x80? 0 : (((dmode) & 0xc0) >> 6) + ((ctest5) & 0x04) + 1

/*
 *	Set initial io register bits from burst code.
 */
static void ncr_init_burst(ncb_p np, u_char bc)
{
	np->rv_ctest4	&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		np->rv_ctest4	|= 0x80;
	}
	else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

/*==========================================================
**
**
**      Auto configuration:  attach and init a host adapter.
**
**
**==========================================================
*/

#ifdef __NetBSD__
void
ncr_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
#else
static void ncr_attach (pcici_t config_id, int unit)
#endif
{
#ifdef __NetBSD__
	struct pci_attach_args * const pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	int retval;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	ncb_p np = (void *)self;
	u_long	period;
	int	i;
	u_char rev = PCI_REVISION(pa->pa_class);
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_addr_t ioaddr, memaddr;
	int ioh_valid, memh_valid;
	bus_dma_segment_t seg;
	int rseg, error;

	callout_init(&np->sc_timo_ch);

	i = ncr_chip_lookup(pa->pa_id, rev);
	printf(": %s\n", ncr_chip_table[i].name);

	np->sc_pc = pc;
	np->sc_dmat = pa->pa_dmat;

	np->ccb = (ccb_p) malloc (sizeof (struct ccb), M_DEVBUF, M_WAITOK);
	if (!np->ccb) return;
	bzero (np->ccb, sizeof (*np->ccb));

	if (ncr_ccb_dma_init(np, np->ccb) != 0)
		return;

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	ioh_valid = (pci_mapreg_map(pa, 0x10,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, &ioaddr, NULL) == 0);
	memh_valid = (pci_mapreg_map(pa, 0x14,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, &memaddr, NULL) == 0);

#if defined(NCR_IOMAPPED)
	if (ioh_valid) {
		np->sc_st = iot;
		np->sc_sh = ioh;
		np->paddr = ioaddr;
		np->sc_iomapped = 1;
	} else if (memh_valid) {
		np->sc_st = memt;
		np->sc_sh = memh;
		np->paddr = memaddr;
		np->sc_iomapped = 0;
	}
#else /* defined(NCR_IOMAPPED) */
	if (memh_valid) {
		np->sc_st = memt;
		np->sc_sh = memh;
		np->paddr = memaddr;
		np->sc_iomapped = 0;
	} else if (ioh_valid) {
		np->sc_st = iot;
		np->sc_sh = ioh;
		np->paddr = ioaddr;
		np->sc_iomapped = 1;
	}
#endif /* defined(NCR_IOMAPPED) */
	else {
		printf("%s: unable to map device registers\n", self->dv_xname);
		return;
	}

	/*
	 * Allocate DMA-safe memory for the script-accessible parts of
	 * the ncb, and map it.
	 */
	if ((error = bus_dmamem_alloc(np->sc_dmat,
	    sizeof(struct ncr_softc_dma), NBPG, 0, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate ncb_dma, error = %d\n",
		    self->dv_xname, error);
		return;
	}

	if ((error = bus_dmamem_map(np->sc_dmat, &seg, rseg,
	    sizeof(struct ncr_softc_dma), (caddr_t *)&np->ncb_dma,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map ncb_dma, error = %d\n",
		    self->dv_xname, error);
		return;
	}

	memset(np->ncb_dma, 0, sizeof(struct ncr_softc_dma));

	if ((error = bus_dmamap_create(np->sc_dmat,
	    sizeof(struct ncr_softc_dma), 1,
	    sizeof(struct ncr_softc_dma), 0, BUS_DMA_NOWAIT,
	    &np->sc_ncb_dmamap)) != 0) {
		printf("%s: unable to create ncb_dma DMA map, error = %d\n",
		    self->dv_xname, error);
		return;
	}

	if ((error = bus_dmamap_load(np->sc_dmat, np->sc_ncb_dmamap,
	    np->ncb_dma, sizeof(struct ncr_softc_dma), NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load ncb_dma DMA map, error = %d\n",
		    self->dv_xname, error);
		return;
	}

	/*
	**	Set up the controller chip's interrupt.
	*/
	retval = pci_intr_map(pa, &intrhandle);
	if (retval) {
		printf("%s: couldn't map interrupt\n", self->dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, intrhandle);
	np->sc_ih = pci_intr_establish(pc, intrhandle, IPL_BIO,
	    ncr_intr, np);
	if (np->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt", self->dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", self->dv_xname, intrstr);

#else /* !__NetBSD__ */
	ncb_p np = (struct ncb*) 0;
#if ! (__FreeBSD__ >= 2)
	extern unsigned bio_imask;
#endif

#if (__FreeBSD__ >= 2)
	struct scsibus_data *scbus;
#endif
	u_char	rev = 0;
	u_long	period;
	int	i;

	/*
	**	allocate and initialize structures.
	*/

	if (!np) {
		np = (ncb_p) malloc (sizeof (struct ncb), M_DEVBUF, M_WAITOK);
		if (!np) return;
		ncrp[unit]=np;
	}
	bzero (np, sizeof (*np));

	np->ccb = (ccb_p) malloc (sizeof (struct ccb), M_DEVBUF, M_WAITOK);
	if (!np->ccb) return;
	bzero (np->ccb, sizeof (*np->ccb));

	np->unit = unit;

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	if (!pci_map_mem (config_id, 0x14, &np->vaddr, &np->paddr))
		return;

	/*
	**	Make the controller's registers available.
	**	Now the INB INW INL OUTB OUTW OUTL macros
	**	can be used safely.
	*/

	np->reg = (struct ncr_reg*) np->vaddr;

#ifdef NCR_IOMAPPED
	/*
	**	Try to map the controller chip into iospace.
	*/

	if (!pci_map_port (config_id, 0x10, &np->port))
		return;
#endif

#endif /* !__NetBSD__ */

	/*
	**	Save some controller register default values
	*/

	np->rv_scntl3	= INB(nc_scntl3) & 0x77;
	np->rv_dmode	= INB(nc_dmode)  & 0xce;
	np->rv_dcntl	= INB(nc_dcntl)  & 0xa9;
	np->rv_ctest3	= INB(nc_ctest3) & 0x01;
	np->rv_ctest4	= INB(nc_ctest4) & 0x88;
	np->rv_ctest5	= INB(nc_ctest5) & 0x24;
	np->rv_gpcntl	= INB(nc_gpcntl);
	np->rv_stest2	= INB(nc_stest2) & 0x20;

	if (bootverbose >= 2) {
		printf ("\tBIOS values:  SCNTL3:%02x DMODE:%02x  DCNTL:%02x\n",
			np->rv_scntl3, np->rv_dmode, np->rv_dcntl);
		printf ("\t              CTEST3:%02x CTEST4:%02x CTEST5:%02x\n",
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	np->rv_dcntl  |= NOCOM;

	/*
	**	Do chip dependent initialization.
	*/

#ifndef __NetBSD__
	rev = pci_conf_read (config_id, PCI_CLASS_REG) & 0xff;
#endif /* !__NetBSD__ */

	/*
	**	Get chip features from chips table.
	*/
#ifndef __NetBSD__
	i = ncr_chip_lookup(pci_conf_read(config_id, PCI_ID_REG), rev);
#endif /* !__NetBSD__ */

	if (i >= 0) {
		np->maxburst	= ncr_chip_table[i].maxburst;
		np->maxoffs	= ncr_chip_table[i].maxoffs;
		np->clock_divn	= ncr_chip_table[i].clock_divn;
		np->features	= ncr_chip_table[i].features;
	} else {	/* Should'nt happen if probe() is ok */
		np->maxburst	= 4;
		np->maxoffs	= 8;
		np->clock_divn	= 4;
		np->features	= FE_ERL;
	}

	np->maxwide	= np->features & FE_WIDE ? 1 : 0;
	np->clock_khz	= np->features & FE_CLK80 ? 80000 : 40000;
	if	(np->features & FE_QUAD)	np->multiplier = 4;
	else if	(np->features & FE_DBLR)	np->multiplier = 2;
	else					np->multiplier = 1;

	/*
	**	Get the frequency of the chip's clock.
	**	Find the right value for scntl3.
	*/
	if (np->features & (FE_ULTRA|FE_ULTRA2))
		ncr_getclock(np, np->multiplier);

#ifdef NCR_TEKRAM_EEPROM
	if (bootverbose) {
		printf ("%s: Tekram EEPROM read %s\n",
			ncr_name(np),
			read_tekram_eeprom (np, NULL) ?
			"succeeded" : "failed");
	}
#endif /* NCR_TEKRAM_EEPROM */

	/*
	 *	If scntl3 != 0, we assume BIOS is present.
	 */
	if (np->rv_scntl3)
		np->features |= FE_BIOS;

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 */
	i = np->clock_divn - 1;
	while (i >= 0) {
		--i;
		if (10ul * SCSI_NCR_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;

	/*
	 * Minimum synchronous period factor supported by the chip.
	 * Btw, 'period' is in tenths of nanoseconds.
	 */

	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	/*
	 * Check against chip SCSI standard support (SCSI-2,ULTRA,ULTRA2).
	 */

	if	(np->minsync < 25 && !(np->features & (FE_ULTRA|FE_ULTRA2)))
		np->minsync = 25;
	else if	(np->minsync < 12 && !(np->features & FE_ULTRA2))
		np->minsync = 12;

	/*
	 * Maximum synchronous period factor supported by the chip.
	 */

	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	/*
	 * Now, some features available with Symbios compatible boards.
	 * LED support through GPIO0 and DIFF support.
	 */

#ifdef	SCSI_NCR_SYMBIOS_COMPAT
	if (!(np->rv_gpcntl & 0x01))
		np->features |= FE_LED0;
#if 0	/* Not safe enough without NVRAM support or user settable option */
	if (!(INB(nc_gpreg) & 0x08))
		np->features |= FE_DIFF;
#endif
#endif	/* SCSI_NCR_SYMBIOS_COMPAT */

	/*
	 * Prepare initial IO registers settings.
	 * Trust BIOS only if we believe we have one and if we want to.
	 */
#ifdef	SCSI_NCR_TRUST_BIOS
	if (!(np->features & FE_BIOS))
#else
	if (1)
#endif
	/* if */ {
		np->rv_dmode = 0;
		np->rv_dcntl = NOCOM;
		np->rv_ctest3 = 0;
		np->rv_ctest4 = MPEE;
		np->rv_ctest5 = 0;
		np->rv_stest2 = 0;

		if (np->features & FE_ERL)
			np->rv_dmode 	|= ERL;	  /* Enable Read Line */
		if (np->features & FE_BOF)
			np->rv_dmode 	|= BOF;	  /* Burst Opcode Fetch */
		if (np->features & FE_ERMP)
			np->rv_dmode	|= ERMP;  /* Enable Read Multiple */
		if (np->features & FE_CLSE)
			np->rv_dcntl	|= CLSE;  /* Cache Line Size Enable */
		if (np->features & FE_WRIE)
			np->rv_ctest3	|= WRIE;  /* Write and Invalidate */
		if (np->features & FE_PFEN)
			np->rv_dcntl	|= PFEN;  /* Prefetch Enable */
		if (np->features & FE_DFS)
			np->rv_ctest5	|= DFS;	  /* Dma Fifo Size */
		if (np->features & FE_DIFF)	
			np->rv_stest2	|= 0x20;  /* Differential mode */
		ncr_init_burst(np, np->maxburst); /* Max dwords burst length */
	} else {
		np->maxburst =
			burst_code(np->rv_dmode, np->rv_ctest4, np->rv_ctest5);
	}

#ifndef NCR_IOMAPPED
	/*
	**	Get on-chip SRAM address, if supported
	*/
	if ((np->features & FE_RAM) && sizeof(struct script) <= 4096) {
#ifdef __NetBSD__
		if (pci_mapreg_map(pa, 0x18,
		    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
		    &memt, &memh, &memaddr, NULL) == 0) {
			np->ram_tag = memt;
			np->ram_handle = memh;
			np->paddr2 = memaddr;
			np->scriptmapped = 1;
		} else {
			np->scriptmapped = 0;
		}
#else	/* !__NetBSD__ */
		(void)(!pci_map_mem (config_id,0x18, &np->vaddr2, &np->paddr2));
#endif	/* __NetBSD__ */
	}
#endif	/* !NCR_IOMAPPED */

	/*
	**	Allocate structure for script relocation.
	*/
#ifdef __FreeBSD__
	if (np->vaddr2 != NULL) {
		np->script = np->vaddr2;
		np->script = (struct script *) np->vaddr2;
	} else if (sizeof (struct script) > PAGE_SIZE)
#else
	if (ISSCRIPTRAMMAPPED(np))
#endif
	/* if */ {
#ifdef __FreeBSD__
		np->script  = (struct script*) vm_page_alloc_contig 
			(round_page(sizeof (struct script)), 
			 0x100000, 0xffffffff, PAGE_SIZE);
#else
		np->script = NULL;
		np->p_script = np->paddr2;
#endif /* __FreeBSD__ */
		/*
		 * A NULL value means that the script is in the chip's
		 * on-board RAM and has no virtual address.
		 */
	} else {
#ifdef __NetBSD__
		if ((error = bus_dmamem_alloc(np->sc_dmat,
		    sizeof(struct script), NBPG, 0, &seg, 1, &rseg,
		    BUS_DMA_NOWAIT)) != 0) {
			printf("%s: unable to allocate script, error = %d\n",
			    self->dv_xname, error);
			return;
		}

		if ((error = bus_dmamem_map(np->sc_dmat, &seg, rseg,
		    sizeof(struct script), (caddr_t *)&np->script,
		    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
			printf("%s: unable to map script, error = %d\n",
			    self->dv_xname, error);
			return;
		}

		if ((error = bus_dmamap_create(np->sc_dmat,
		    sizeof(struct script), 1,
		    sizeof(struct script), 0, BUS_DMA_NOWAIT,
		    &np->script_dmamap)) != 0) {
			printf("%s: unable to create script DMA map, "
			    "error = %d\n", self->dv_xname, error);
			return;
		}

		if ((error = bus_dmamap_load(np->sc_dmat,
		    np->script_dmamap, np->script, sizeof(struct script),
		    NULL, BUS_DMA_NOWAIT)) != 0) {
			printf("%s: unable to load script DMA map, "
			    "error = %d\n", self->dv_xname, error);
			return;
		}
		memset(np->script, 0, sizeof(struct script));
#else
		np->script  = (struct script *)
			malloc (sizeof (struct script), M_DEVBUF, M_WAITOK);
#endif /* __NetBSD__ */
	}

#ifdef __FreeBSD__
	if (sizeof (struct scripth) > PAGE_SIZE) {
		np->scripth = (struct scripth*) vm_page_alloc_contig 
			(round_page(sizeof (struct scripth)), 
			 0x100000, 0xffffffff, PAGE_SIZE);
	} else {
		np->scripth = (struct scripth *)
			malloc (sizeof (struct scripth), M_DEVBUF, M_WAITOK);
	}
#else
	if ((error = bus_dmamem_alloc(np->sc_dmat,
	    sizeof(struct scripth), NBPG, 0, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate scripth, error = %d\n",
		    self->dv_xname, error);
		return;
	}

	if ((error = bus_dmamem_map(np->sc_dmat, &seg, rseg,
	    sizeof(struct scripth), (caddr_t *)&np->scripth,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map scripth, error = %d\n",
		    self->dv_xname, error);
		return;
	}

	if ((error = bus_dmamap_create(np->sc_dmat,
	    sizeof(struct scripth), 1,
	    sizeof(struct scripth), 0, BUS_DMA_NOWAIT,
	    &np->scripth_dmamap)) != 0) {
		printf("%s: unable to create scripth DMA map, "
		    "error = %d\n", self->dv_xname, error);
		return;
	}

	if ((error = bus_dmamap_load(np->sc_dmat,
	    np->scripth_dmamap, np->scripth, sizeof(struct scripth),
	    NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load scripth DMA map, "
		    "error = %d\n", self->dv_xname, error);
		return;
	}
	memset(np->scripth, 0, sizeof(struct scripth));
#endif /* __FreeBSD__ */

#ifdef SCSI_NCR_PCI_CONFIG_FIXUP
	/*
	**	If cache line size is enabled, check PCI config space and 
	**	try to fix it up if necessary.
	*/
#ifdef PCIR_CACHELNSZ	/* To be sure that new PCI stuff is present */
	{
		u_char cachelnsz = pci_cfgread(config_id, PCIR_CACHELNSZ, 1);
		u_short command  = pci_cfgread(config_id, PCIR_COMMAND, 2);

		if (!cachelnsz) {
			cachelnsz = 8;
			printf("%s: setting PCI cache line size register to %d.\n",
				ncr_name(np), (int)cachelnsz);
			pci_cfgwrite(config_id, PCIR_CACHELNSZ, cachelnsz, 1);
		}

		if (!(command & (1<<4))) {
			command |= (1<<4);
			printf("%s: setting PCI command write and invalidate.\n",
				ncr_name(np));
			pci_cfgwrite(config_id, PCIR_COMMAND, command, 2);
		}
	}
#endif /* PCIR_CACHELNSZ */

#endif /* SCSI_NCR_PCI_CONFIG_FIXUP */

	/*
	**	Bells and whistles   ;-)
	*/
	if (bootverbose)
		printf("%s: minsync=%d, maxsync=%d, maxoffs=%d, %d dwords burst, %s dma fifo\n",
		ncr_name(np), np->minsync, np->maxsync, np->maxoffs,
		burst_length(np->maxburst),
		(np->rv_ctest5 & DFS) ? "large" : "normal");

	/*
	**	Print some complementary information that can be helpfull.
	*/
	if (bootverbose)
		printf("%s: %s, %s IRQ driver%s\n",
			ncr_name(np),
			np->rv_stest2 & 0x20 ? "differential" : "single-ended",
			np->rv_dcntl & IRQM ? "totem pole" : "open drain",
			ISSCRIPTRAMMAPPED(np) ? ", using on-chip SRAM" : "");
			
	/*
	**	Patch scripts to physical addresses
	*/
	ncr_script_fill (&script0, &scripth0);

#ifdef __NetBSD__
	if (np->script)
		np->p_script = np->script_dmamap->dm_segs[0].ds_addr;

	np->p_scripth = np->scripth_dmamap->dm_segs[0].ds_addr;
#else
	if (np->script)
		np->p_script = vtophys(np->script);

	np->p_scripth	= vtophys(np->scripth);
#endif /* __NetBSD__ */

	ncr_script_copy_and_bind (np, (ncrcmd *) &script0,
			(ncrcmd *) np->script, sizeof(struct script));

	ncr_script_copy_and_bind (np, (ncrcmd *) &scripth0,
		(ncrcmd *) np->scripth, sizeof(struct scripth));

	np->ccb->p_ccb	= vtophys((vaddr_t)np->ccb);

	/*
	**    Patch the script for LED support.
	*/

	if (np->features & FE_LED0) {
		WRITESCRIPT(reselect[0],  SCR_REG_REG(gpreg, SCR_OR,  0x01));
		WRITESCRIPT(reselect1[0], SCR_REG_REG(gpreg, SCR_AND, 0xfe));
		WRITESCRIPT(reselect2[0], SCR_REG_REG(gpreg, SCR_AND, 0xfe));
	}

	/*
	**	init data structure
	*/

	np->ncb_dma->jump_tcb.l_cmd	= htole32(SCR_JUMP);
	np->ncb_dma->jump_tcb.l_paddr	= htole32(NCB_SCRIPTH_PHYS (np, abort));

	/*
	**  Get SCSI addr of host adapter (set by bios?).
	*/

	np->myaddr = INB(nc_scid) & 0x07;
	if (!np->myaddr) np->myaddr = SCSI_NCR_MYADDR;

#ifdef NCR_DUMP_REG
	/*
	**	Log the initial register contents
	*/
	{
		int reg;
#ifdef __NetBSD__
		u_long config_id = pa->pa_tag;
#endif /* __NetBSD__ */
		for (reg=0; reg<256; reg+=4) {
			if (reg%16==0) printf ("reg[%2x]", reg);
			printf (" %08x", (int)pci_conf_read (config_id, reg));
			if (reg%16==12) printf ("\n");
		}
	}
#endif /* NCR_DUMP_REG */

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat,  0   );


	/*
	**	Now check the cache handling of the pci chipset.
	*/

	if (ncr_snooptest (np)) {
		printf ("CACHE INCORRECTLY CONFIGURED.\n");
		return;
	};

#ifndef __NetBSD__
	/*
	**	Install the interrupt handler.
	*/

	if (!pci_map_int (config_id, ncr_intr, np, &bio_imask))
		printf ("\tinterruptless mode: reduced performance.\n");
#endif /* !__NetBSD__ */

	/*
	**	After SCSI devices have been opened, we cannot
	**	reset the bus safely, so we do it here.
	**	Interrupt handler does the real work.
	*/

	OUTB (nc_scntl1, CRST);
	DELAY (1000);

	/*
	**	Process the reset exception,
	**	if interrupts are not enabled yet.
	**	Then enable disconnects.
	*/
	ncr_exception (np);
	np->disc = 1;

	/*
	**	Now let the generic SCSI driver
	**	look for the SCSI devices on the bus ..
	*/

#ifdef __NetBSD__
	np->sc_adapter.scsipi_cmd = ncr_start;
	np->sc_adapter.scsipi_minphys = ncr_minphys;

	np->sc_link.adapter_softc = np;
	np->sc_link.scsipi_scsi.adapter_target = np->myaddr;
	np->sc_link.openings = 1;
	np->sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	np->sc_link.scsipi_scsi.max_target   = np->maxwide ? 15 : 7;
	np->sc_link.scsipi_scsi.max_lun = MAX_LUN-1;
	np->sc_link.type = BUS_SCSI;
	np->sc_link.adapter      = &np->sc_adapter;
#else /* !__NetBSD__ */
	np->sc_link.adapter_unit = unit;
	np->sc_link.adapter_softc = np;
	np->sc_link.adapter_targ = np->myaddr;
	np->sc_link.fordriver	 = 0;
	np->sc_link.adapter      = &ncr_switch;
#endif /* !__NetBSD__ */
	np->sc_link.device       = &ncr_dev;
	np->sc_link.flags	 = 0;

#ifdef __NetBSD__
	config_found(self, &np->sc_link, scsiprint);
#else /* !__NetBSD__ */
#if (__FreeBSD__ >= 2)
	scbus = scsi_alloc_bus();
	if(!scbus)
		return;
	scbus->adapter_link = &np->sc_link;

	if(np->maxwide)
		scbus->maxtarg = 15;

	if (bootverbose) {
		unsigned t_from = 0;
		unsigned t_to   = scbus->maxtarg;
		unsigned myaddr = np->myaddr;

		char *txt_and = "";
		printf ("%s scanning for targets ", ncr_name (np));
		if (t_from < myaddr) {
			printf ("%d..%d ", t_from, myaddr -1);
			txt_and = "and ";
		}
		if (myaddr < t_to)
			printf ("%s%d..%d ", txt_and, myaddr +1, t_to);
		printf ("(V%d " NCR_DATE ")\n", NCR_VERSION);
	}
		
	scsi_attachdevs (scbus);
	scbus = NULL;   /* Upper-level SCSI code owns this now */
#else
	scsi_attachdevs (&np->sc_link);
#endif /* !__FreeBSD__ >= 2 */
#endif /* !__NetBSD__ */

	/*
	**	start the timeout daemon
	*/
	ncr_timeout (np);
	np->lasttime=0;

	/*
	**  use SIMPLE TAG messages by default
	*/

	np->order = M_SIMPLE_TAG;

	/*
	**  Done.
	*/

	return;
}

int
ncr_ccb_dma_init(ncb_p np, ccb_p cp)
{
	int error;

	if ((error = bus_dmamap_create(np->sc_dmat, MAX_SIZE, MAX_SCATTER,
	    MAX_SIZE, 0, BUS_DMA_NOWAIT, &cp->xfer_dmamap)) != 0) {
		printf("%s: unable to create CCB xfer DMA map, error = %d\n",
		    ncr_name(np), error);
		return (error);
	}

	return (0);
}

/*==========================================================
**
**
**	Process pending device interrupts.
**
**
**==========================================================
*/

#ifdef __NetBSD__
static int
#else /* !__NetBSD__ */
static void
#endif /* __NetBSD__ */
ncr_intr(vnp)
	void *vnp;
{
#ifdef __NetBSD__
	int n = 0;
#endif
	ncb_p np = vnp;
	int oldspl = splbio();

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("[");

	if (INB(nc_istat) & (INTF|SIP|DIP)) {
		/*
		**	Repeat until no outstanding ints
		*/
		do {
			ncr_exception (np);
		} while (INB(nc_istat) & (INTF|SIP|DIP));

#ifdef __NetBSD__
		n=1;
#endif
		np->ticks = 100;
	};

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("]\n");

	splx (oldspl);
#ifdef __NetBSD__
	return (n);
#endif
}

/*==========================================================
**
**
**	Start execution of a SCSI command.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/

static void ncr_scsipi_request(struct scsipi_channel *chan,
    scsipi_adapter_req_t req, void *arg)
{

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		ncr_start((struct scsipi_xfer *)arg);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		/* XXX XXX XXX */
		return;
	}
}

static void ncr_start(struct scsipi_xfer *xp)
{
	ncb_p np  = (ncb_p) xp->sc_link->adapter_softc;

	struct scsi_generic * cmd = (struct scsi_generic *)xp->cmd;
	ccb_p cp;
	lcb_p lp;
	tcb_p tp = &np->target[xp->sc_link->scsipi_scsi.target];

	int	i, oldspl, segments, flags = xp->xs_control, pollmode;
	u_char	qidx, nego, idmsg, *msgptr;
	u_long  msglen, msglen2;

	/*---------------------------------------------
	**
	**   Reset SCSI bus
	**
	**	Interrupt handler does the real work.
	**
	**---------------------------------------------
	*/

	if (flags & XS_CTL_RESET) {
		OUTB (nc_scntl1, CRST);
		DELAY (1000);
		return(COMPLETE);
	};

	/*---------------------------------------------
	**
	**      Some shortcuts ...
	**
	**---------------------------------------------
	*/

	if ((xp->sc_link->scsipi_scsi.target == np->myaddr	  ) ||
		(xp->sc_link->scsipi_scsi.target >= MAX_TARGET) ||
		(xp->sc_link->scsipi_scsi.lun    >= MAX_LUN   ) ||
		(flags    & XS_CTL_DATA_UIO)) {
		xp->error = XS_DRIVER_STUFFUP;
		return(COMPLETE);
	};

	/*---------------------------------------------
	**
	**      Diskaccess to partial blocks?
	**
	**---------------------------------------------
	*/

	if ((xp->datalen & 0x1ff) && !(tp->inqdata[0] & 0x1f)) {
		switch (cmd->opcode) {
		case 0x28:  /* READ_BIG  (10) */
		case 0xa8:  /* READ_HUGE (12) */
		case 0x2a:  /* WRITE_BIG (10) */
		case 0xaa:  /* WRITE_HUGE(12) */
			PRINT_ADDR(xp);
			printf ("access to partial disk block refused.\n");
			xp->error = XS_DRIVER_STUFFUP;
			return(COMPLETE);
		};
	};

	if ((unsigned)xp->datalen > 128*1024*1024) {
		PRINT_ADDR(xp);
		printf ("trying to transfer %8x bytes, mem addr = %p\n", 
			xp->datalen, xp->data);
		{
			int i;
			PRINT_ADDR(xp);
			printf ("command: %2x (", cmd->opcode);
			for (i = 0; i<11; i++)
				printf (" %2x", cmd->bytes[i]);
			printf (")\n");
		}
	}

	if (DEBUG_FLAGS & DEBUG_TINY) {
		PRINT_ADDR(xp);
		printf ("CMD=%x F=%x A=%p L=%x ", 
			cmd->opcode, (unsigned)xp->xs_control, xp->data,
			(unsigned)xp->datalen);
	}

	/*--------------------------------------------
	**
	**   Sanity checks ...
	**	copied from Elischer's Adaptec driver.
	**
	**--------------------------------------------
	*/

	flags = xp->xs_control;

	if (xp->bp)
		flags |= (XS_CTL_NOSLEEP); /* just to be sure */

	/*---------------------------------------------------
	**
	**	Assign a ccb / bind xp
	**
	**----------------------------------------------------
	*/

	oldspl = splbio();

	if (!(cp=ncr_get_ccb (np, flags, xp->sc_link->scsipi_scsi.target,
		xp->sc_link->scsipi_scsi.lun))) {
		printf ("%s: no ccb.\n", ncr_name (np));
		xp->error = XS_DRIVER_STUFFUP;
		splx(oldspl);
		return(TRY_AGAIN_LATER);
	};
	cp->xfer = xp;

	/*---------------------------------------------------
	**
	**	timestamp
	**
	**----------------------------------------------------
	*/

	bzero (&cp->phys.header.stamp, sizeof (struct tstamp));
#ifdef __NetBSD__
	cp->phys.header.stamp.start = mono_time;
#else
	gettime(&cp->phys.header.stamp.start);
#endif

	/*----------------------------------------------------
	**
	**	Get device quirks from a speciality table.
	**
	**	@GENSCSI@
	**	This should be a part of the device table
	**	in "scsi_conf.c".
	**
	**----------------------------------------------------
	*/

	if (tp->quirks & QUIRK_UPDATE) {
#ifdef __NetBSD__
		tp->quirks = ncr_lookup ((char*) &tp->inqdata[0]);
#else
		int q = xp->sc_link->quirks;
		tp->quirks = QUIRK_NOMSG;
		if (q & SD_Q_NO_TAGS)
			tp->quirks |= QUIRK_NOTAGS;
		if (q & SD_Q_NO_SYNC)
			tp->quirks |= QUIRK_NOSYNC;
		if (q & SD_Q_NO_WIDE)
			tp->quirks |= QUIRK_NOWIDE16;
#endif
		if (bootverbose && (tp->quirks & ~QUIRK_NOMSG)) {
			PRINT_ADDR(xp);
			printf ("NCR quirks=0x%x\n", tp->quirks);
		};
		/*
		**	set number of tags
		*/
		ncr_setmaxtags (tp, tp->usrtags);
	};

	/*---------------------------------------------------
	**
	**	negotiation required?
	**
	**----------------------------------------------------
	*/

	nego = 0;

	if (!tp->nego_cp && tp->inqdata[7]) {
		/*
		**	negotiate wide transfers ?
		*/

		if (!tp->widedone) {
			if (tp->inqdata[7] & INQ7_WIDE16) {
				nego = NS_WIDE;
			} else
				tp->widedone=1;
		};

		/*
		**	negotiate synchronous transfers?
		*/

		if (!nego && !tp->period) {
			if (SCSI_NCR_DFLT_SYNC 
#ifdef NCR_CDROM_ASYNC
			    && ((tp->inqdata[0] & 0x1f) != 5)
#endif /* NCR_CDROM_ASYNC */
			    && (tp->inqdata[7] & INQ7_SYNC)) {
				nego = NS_SYNC;
			} else {
				tp->period  =0xffff;
				tp->sval = 0xe0;
				PRINT_ADDR(xp);
				printf ("asynchronous.\n");
			};
		};

		/*
		**	remember nego is pending for the target.
		**	Avoid to start a nego for all queued commands 
		**	when tagged command queuing is enabled.
		*/

		if (nego)
			tp->nego_cp = cp;
	};

	/*---------------------------------------------------
	**
	**	choose a new tag ...
	**
	**----------------------------------------------------
	*/

	if ((lp = tp->lp[xp->sc_link->scsipi_scsi.lun]) && (lp->usetags)) {
		/*
		**	assign a tag to this ccb!
		*/
		while (!cp->tag) {
			ccb_p cp2 = lp->next_ccb;
			lp->lasttag = lp->lasttag % 255 + 1;
			while (cp2 && cp2->tag != lp->lasttag)
				cp2 = cp2->next_ccb;
			if (cp2) continue;
			cp->tag=lp->lasttag;
			if (DEBUG_FLAGS & DEBUG_TAGS) {
				PRINT_ADDR(xp);
				printf ("using tag #%d.\n", cp->tag);
			};
		};
	} else {
		cp->tag=0;
	};

	/*----------------------------------------------------
	**
	**	Build the identify / tag / sdtr message
	**
	**----------------------------------------------------
	*/

	idmsg = M_IDENTIFY | xp->sc_link->scsipi_scsi.lun;
	if ((cp!=np->ccb) && (np->disc))
		idmsg |= 0x40;

	msgptr = cp->scsi_smsg;
	msglen = 0;
	msgptr[msglen++] = idmsg;

	if (cp->tag) {
	    char tag;

	    tag = np->order;
	    if (tag == 0) {
		/*
		**	Ordered write ops, unordered read ops.
		*/
		switch (cmd->opcode) {
		case 0x08:  /* READ_SMALL (6) */
		case 0x28:  /* READ_BIG  (10) */
		case 0xa8:  /* READ_HUGE (12) */
		    tag = M_SIMPLE_TAG;
		    break;
		default:
		    tag = M_ORDERED_TAG;
		}
	    }
	    msgptr[msglen++] = tag;
	    msgptr[msglen++] = cp -> tag;
	}

	switch (nego) {
	case NS_SYNC:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 3;
		msgptr[msglen++] = M_X_SYNC_REQ;
		msgptr[msglen++] = tp->minsync;
		msgptr[msglen++] = tp->maxoffs;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgout: ");
			ncr_show_msg (&cp->scsi_smsg [msglen-5]);
			printf (".\n");
		};
		break;
	case NS_WIDE:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 2;
		msgptr[msglen++] = M_X_WIDE_REQ;
		msgptr[msglen++] = tp->usrwide;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("wide msgout: ");
			ncr_show_msg (&cp->scsi_smsg [msglen-4]);
			printf (".\n");
		};
		break;
	};

	/*----------------------------------------------------
	**
	**	Build the identify message for getcc.
	**
	**----------------------------------------------------
	*/

	cp -> scsi_smsg2 [0] = idmsg;
	msglen2 = 1;

	/*----------------------------------------------------
	**
	**	Build the data descriptors
	**
	**----------------------------------------------------
	*/

	segments = ncr_scatter (np, cp, (vaddr_t) xp->data,
					(vsize_t) xp->datalen);

	if (segments < 0) {
		xp->error = XS_DRIVER_STUFFUP;
		ncr_free_ccb(np, cp, flags);
		splx(oldspl);
		return(COMPLETE);
	};

	/*----------------------------------------------------
	**
	**	Set the SAVED_POINTER.
	**
	**----------------------------------------------------
	*/

	if (flags & XS_CTL_DATA_IN) {
		bus_addr_t sp = NCB_SCRIPT_PHYS (np, data_in);
		cp->phys.header.savep = htole32(sp);
		cp->phys.header.goalp = htole32(sp + 20 + segments * 16);
	} else if (flags & XS_CTL_DATA_OUT) {
		bus_addr_t sp = NCB_SCRIPT_PHYS (np, data_out);
		cp->phys.header.savep = htole32(sp);
		cp->phys.header.goalp = htole32(sp + 20 + segments * 16);
	} else {
		cp->phys.header.savep = htole32(NCB_SCRIPT_PHYS (np, no_data));
		cp->phys.header.goalp = cp->phys.header.savep;
	};
	cp->phys.header.lastp = cp->phys.header.savep;

	/*
	**	Copy the SCSI command into the ccb.
	*/
	memcpy(&cp->scsi_cmd, cmd, sizeof(cp->scsi_cmd));

	/*
	**	Make sure the sense buffer is empty.
	*/
	memset(&cp->sense_data, 0, sizeof(cp->sense_data));

	/*----------------------------------------------------
	**
	**	fill in ccb
	**
	**----------------------------------------------------
	**
	**
	**	physical -> virtual backlink
	**	Generic SCSI command
	*/
	cp->phys.header.cp		= cp;
	/*
	**	Startqueue
	*/
	cp->phys.header.launch.l_paddr	= htole32(NCB_SCRIPT_PHYS (np, select));
	cp->phys.header.launch.l_cmd	= htole32(SCR_JUMP);
	/*
	**	select
	*/
	cp->phys.select.sel_id		= xp->sc_link->scsipi_scsi.target;
	cp->phys.select.sel_scntl3	= tp->wval;
	cp->phys.select.sel_sxfer	= tp->sval;
	/*
	**	message
	*/
	cp->phys.smsg.addr		= htole32(CCB_PHYS (cp, scsi_smsg));
	cp->phys.smsg.size		= htole32(msglen);

	cp->phys.smsg2.addr		= htole32(CCB_PHYS (cp, scsi_smsg2));
	cp->phys.smsg2.size		= htole32(msglen2);
	/*
	**	command
	*/
	cp->phys.cmd.addr		= htole32(CCB_PHYS (cp, scsi_cmd));
	cp->phys.cmd.size		= htole32(xp->cmdlen);
	/*
	**	sense command
	*/
	cp->phys.scmd.addr		= htole32(CCB_PHYS (cp, sensecmd));
	cp->phys.scmd.size		= htole32(6);
	/*
	**	patch requested size into sense command
	*/
	cp->sensecmd[0]			= 0x03;
	cp->sensecmd[1]			= xp->sc_link->scsipi_scsi.lun << 5;
	cp->sensecmd[4]			= sizeof(struct scsipi_sense_data);
	/*
	**	sense data
	*/
	cp->phys.sense.addr		= htole32(CCB_PHYS (cp, sense_data));
	cp->phys.sense.size		= htole32(sizeof(struct scsipi_sense_data));
	/*
	**	status
	*/
	cp->actualquirks		= tp->quirks;
	cp->host_status			= nego ? HS_NEGOTIATE : HS_BUSY;
	cp->scsi_status			= S_ILLEGAL;
	cp->parity_status		= 0;

	cp->xerr_status			= XE_OK;
	cp->sync_status			= tp->sval;
	cp->nego_status			= nego;
	cp->wide_status			= tp->wval;

	/*----------------------------------------------------
	**
	**	Critical region: start this job.
	**
	**----------------------------------------------------
	*/

	/*
	**	reselect pattern and activate this job.
	*/

	cp->jump_ccb.l_cmd	= htole32(SCR_JUMP ^ IFFALSE (DATA (cp->tag)));
#ifdef __NetBSD__
	cp->tlimit		= mono_time.tv_sec + xp->timeout / 1000 + 2;
#else
	cp->tlimit		= time.tv_sec + xp->timeout / 1000 + 2;
#endif
	cp->magic		= CCB_MAGIC;

	/*
	**	insert into start queue.
	*/

	qidx = np->squeueput + 1;
	if (qidx >= MAX_START) qidx=0;
	np->ncb_dma->squeue [qidx	  ] = htole32(NCB_SCRIPT_PHYS (np, idle));
	np->ncb_dma->squeue [np->squeueput] = htole32(CCB_PHYS (cp, phys));
	np->squeueput = qidx;

	if(DEBUG_FLAGS & DEBUG_QUEUE)
		printf ("%s: queuepos=%d tryoffset=%d.\n", ncr_name (np),
		np->squeueput,
		(unsigned)(READSCRIPT(startpos[0])- 
			   (NCB_SCRIPTH_PHYS (np, tryloop))));

	/*
	**	Script processor may be waiting for reselect.
	**	Wake it up.
	*/
	OUTB (nc_istat, SIGP);

	/*
	**	and reenable interrupts
	*/
#ifdef __NetBSD__
	pollmode = flags & XS_CTL_POLL;
#else
	pollmode = flags & SCSI_NOMASK;
#endif
	splx (oldspl);

	/*
	**	If interrupts are enabled, return now.
	**	Command is successfully queued.
	*/

	if (!pollmode) {
		if(DEBUG_FLAGS & DEBUG_TINY) printf ("Q");
		return(SUCCESSFULLY_QUEUED);
	};

	/*----------------------------------------------------
	**
	**	Interrupts not yet enabled - have to poll.
	**
	**----------------------------------------------------
	*/

	if (DEBUG_FLAGS & DEBUG_POLL) printf("P");

	for (i=xp->timeout; i && !(xp->xs_status & XS_STS_DONE);i--) {
		if ((DEBUG_FLAGS & DEBUG_POLL) && (cp->host_status))
			printf ("%c", (cp->host_status & 0xf) + '0');
		DELAY (1000);
		ncr_exception (np);
	};

	/*
	**	Abort if command not done.
	*/
	if (!(xp->xs_status & XS_STS_DONE)) {
		printf ("%s: aborting job ...\n", ncr_name (np));
		OUTB (nc_istat, CABRT);
		DELAY (100000);
		OUTB (nc_istat, SIGP);
		ncr_exception (np);
	};

	if (!(xp->xs_status & XS_STS_DONE)) {
		printf ("%s: abortion failed at %x.\n",
			ncr_name (np), (unsigned) INL(nc_dsp));
		ncr_init (np, "timeout", HS_TIMEOUT);
	};

	if (!(xp->xs_status & XS_STS_DONE)) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	if (DEBUG_FLAGS & DEBUG_RESULT) {
		printf ("%s: result: %x %x.\n",
			ncr_name (np), cp->host_status, cp->scsi_status);
	};
	switch (xp->error) {
	case  0     : return (COMPLETE);
	case XS_BUSY: return (TRY_AGAIN_LATER);
	};
	return (COMPLETE);
}

/*==========================================================
**
**
**	Complete execution of a SCSI command.
**	Signal completion to the generic SCSI driver.
**
**
**==========================================================
*/

void ncr_complete (ncb_p np, ccb_p cp)
{
	struct scsipi_xfer * xp;
	tcb_p tp;
	lcb_p lp;

	/*
	**	Sanity check
	*/

	if (!cp || (cp->magic!=CCB_MAGIC) || !cp->xfer) return;
	cp->magic = 1;
	cp->tlimit= 0;

	/*
	**	No Reselect anymore.
	*/
	cp->jump_ccb.l_cmd = htole32(SCR_JUMP);

	/*
	**	No starting.
	*/
	cp->phys.header.launch.l_paddr= htole32(NCB_SCRIPT_PHYS (np, idle));

	/*
	**	timestamp
	*/
	ncb_profile (np, cp);

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("CCB=%lx STAT=%x/%x\n", (unsigned long)cp & 0xfff,
			cp->host_status,cp->scsi_status);

	xp = cp->xfer;
	cp->xfer = NULL;

#ifdef __NetBSD__
	/*
	**	Unload the DMA maps.
	*/
	if (xp->datalen != 0)
		bus_dmamap_unload(np->sc_dmat, cp->xfer_dmamap);
#endif

	/*
	**	Copyback the sense data.
	**	XXX Only do this if we know there's sense data?
	*/
	memcpy(&xp->sense, &cp->sense_data, sizeof(xp->sense));

	tp = &np->target[xp->sc_link->scsipi_scsi.target];
	lp = tp->lp[xp->sc_link->scsipi_scsi.lun];

	/*
	**	We donnot queue more than 1 ccb per target 
	**	with negotiation at any time. If this ccb was 
	**	used for negotiation, clear this info in the tcb.
	*/

	if (cp == tp->nego_cp)
		tp->nego_cp = 0;

	/*
	**	Check for parity errors.
	*/

	if (cp->parity_status) {
		PRINT_ADDR(xp);
		printf ("%d parity error(s), fallback.\n", cp->parity_status);
		/*
		**	fallback to asynch transfer.
		*/
		tp->usrsync=255;
		tp->period =  0;
	};

	/*
	**	Check for extended errors.
	*/

	if (cp->xerr_status != XE_OK) {
		PRINT_ADDR(xp);
		switch (cp->xerr_status) {
		case XE_EXTRA_DATA:
			printf ("extraneous data discarded.\n");
			break;
		case XE_BAD_PHASE:
			printf ("illegal scsi phase (4/5).\n");
			break;
		default:
			printf ("extended error %d.\n", cp->xerr_status);
			break;
		};
		if (cp->host_status==HS_COMPLETE)
			cp->host_status = HS_FAIL;
	};

	/*
	**	Check the status.
	*/
#ifdef __NetBSD__
	if (xp->error != XS_NOERROR) { 
                                
                /*              
                **      Don't override the error value.
                */
	} else                        
#endif /* __NetBSD__ */
	if (   (cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_GOOD)) {

		/*
		**	All went well.
		*/

		xp->resid = 0;

		/*
		** if (cp->phys.header.lastp != cp->phys.header.goalp)...
		**
		**	@RESID@
		**	Could dig out the correct value for resid,
		**	but it would be quite complicated.
		**
		**	The ah1542.c driver sets it to 0 too ...
		*/

		/*
		**	Try to assign a ccb to this nexus
		*/
		ncr_alloc_ccb (np, xp->sc_link->scsipi_scsi.target,
			xp->sc_link->scsipi_scsi.lun);

		/*
		**	On inquire cmd (0x12) save some data.
		*/
		if (xp->cmd->opcode == 0x12 && xp->sc_link->scsipi_scsi.lun == 0) {
			bcopy (	xp->data,
				&tp->inqdata,
				sizeof (tp->inqdata));
			/*
			**	prepare negotiation of synch and wide.
			*/
			ncr_negotiate (np, tp);

			/*
			**	force quirks update before next command start
			*/
			tp->quirks |= QUIRK_UPDATE;
		};

		/*
		**	Announce changes to the generic driver
		*/
		if (lp) {
			if (lp->reqlink != lp->actlink)
				ncr_opennings (np, lp, xp);
		};

		tp->bytes     += xp->datalen;
		tp->transfers ++;
#ifndef __NetBSD__
	} else if (xp->xs_control & SCSI_ERR_OK) {

		/*
		**   Not correct, but errors expected.
		*/
		xp->resid = 0;
#endif /* !__NetBSD__ */
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == (S_SENSE|S_GOOD))) {

		/*
		**   Check condition code
		*/
		xp->error = XS_SENSE;

		if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
			u_char * p = (u_char*) & xp->sense.scsi_sense;
			int i;
			printf ("\n%s: sense data:", ncr_name (np));
			for (i=0; i<14; i++) printf (" %x", *p++);
			printf (".\n");
		};

	} else if ((cp->host_status == HS_COMPLETE)
		   && ((cp->scsi_status == S_BUSY)
		       || (cp->scsi_status == S_CONFLICT))) {

		/*
		**   Target is busy, or reservation conflict
		*/
		xp->error = XS_BUSY;

	} else if (cp->host_status == HS_SEL_TIMEOUT) {

		/*
		**   Device failed selection
		*/
		xp->error = XS_SELTIMEOUT;

	} else if(cp->host_status == HS_TIMEOUT) {

		/*
		**   No response
		*/
		xp->error = XS_TIMEOUT;

	} else {

		/*
		**  Other protocol messes
		*/
		PRINT_ADDR(xp);
		printf ("COMMAND FAILED (%x %x) @%p.\n",
			cp->host_status, cp->scsi_status, cp);

		xp->error = XS_TIMEOUT;
	}

	/*
	**	trace output
	*/

	if (tp->usrflag & UF_TRACE) {
		u_char * p;
		int i;
		PRINT_ADDR(xp);
		printf (" CMD:");
		p = (u_char*) &xp->cmd->opcode;
		for (i=0; i<xp->cmdlen; i++) printf (" %x", *p++);

		if (cp->host_status==HS_COMPLETE) {
			switch (cp->scsi_status) {
			case S_GOOD:
				printf ("  GOOD");
				break;
			case S_CHECK_COND:
				printf ("  SENSE:");
				p = (u_char*) &xp->sense.scsi_sense;
				for (i=0; i<sizeof(struct scsipi_sense_data);
				    i++)
					printf (" %x", *p++);
				break;
			default:
				printf ("  STAT: %x\n", cp->scsi_status);
				break;
			};
		} else printf ("  HOSTERROR: %x", cp->host_status);
		printf ("\n");
	};

	/*
	**	Free this ccb
	*/
	ncr_free_ccb (np, cp, xp->xs_control);

	/*
	**	signal completion to generic driver.
	*/
	scsipi_done (xp);
}

/*==========================================================
**
**
**	Signal all (or one) control block done.
**
**
**==========================================================
*/

void ncr_wakeup (ncb_p np, u_long code)
{
	/*
	**	Starting at the default ccb and following
	**	the links, complete all jobs with a
	**	host_status greater than "disconnect".
	**
	**	If the "code" parameter is not zero,
	**	complete all jobs that are not IDLE.
	*/

	ccb_p cp = np->ccb;
	while (cp) {
		switch (cp->host_status) {

		case HS_IDLE:
			break;

		case HS_DISCONNECT:
			if(DEBUG_FLAGS & DEBUG_TINY) printf ("D");
			/* fall through */

		case HS_BUSY:
		case HS_NEGOTIATE:
			if (!code) break;
			cp->host_status = code;

			/* fall through */

		default:
			ncr_complete (np, cp);
			break;
		};
		cp = cp -> link_ccb;
	};
}

/*==========================================================
**
**
**	Start NCR chip.
**
**
**==========================================================
*/

void ncr_init (ncb_p np, char * msg, u_long code)
{
	int	i;
	u_long	usrsync;
	u_char	usrwide;

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat, 0);

	/*
	**	Message.
	*/

	if (msg) printf ("%s: restart (%s).\n", ncr_name (np), msg);

	/*
	**	Clear Start Queue
	*/

	for (i=0;i<MAX_START;i++)
		np -> ncb_dma -> squeue [i] = htole32(NCB_SCRIPT_PHYS (np, idle));

	/*
	**	Start at first entry.
	*/

	np->squeueput = 0;
	WRITESCRIPT(startpos[0], NCB_SCRIPTH_PHYS (np, tryloop));
	WRITESCRIPT(start0  [0], SCR_INT ^ IFFALSE (0));

	/*
	**	Wakeup all pending jobs.
	*/

	ncr_wakeup (np, code);

	/*
	**	Init chip.
	*/

	OUTB (nc_istat,  0x00   );      /*  Remove Reset, abort ...	     */
	OUTB (nc_scntl0, 0xca   );      /*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00	);	/*  odd parity, and remove CRST!!    */
	ncr_selectclock(np, np->rv_scntl3); /* Select SCSI clock             */
	OUTB (nc_scid  , RRE|np->myaddr);/*  host adapter SCSI address       */
	OUTW (nc_respid, 1ul<<np->myaddr);/*  id to respond to		     */
	OUTB (nc_istat , SIGP	);	/*  Signal Process		     */
	OUTB (nc_dmode , np->rv_dmode);	/* XXX modify burstlen ??? */
	OUTB (nc_dcntl , np->rv_dcntl);
	OUTB (nc_ctest3, np->rv_ctest3);
	OUTB (nc_ctest5, np->rv_ctest5);
	OUTB (nc_ctest4, np->rv_ctest4);/*  enable master parity checking    */
	OUTB (nc_stest2, np->rv_stest2|EXT); /* Extended Sreq/Sack filtering */
	OUTB (nc_stest3, TE     );	/*  TolerANT enable		     */
	OUTB (nc_stime0, 0x0b	);	/*  HTH = disabled, STO = 0.1 sec.   */

	if (bootverbose) {
		printf ("\tACTUAL values:SCNTL3:%02x DMODE:%02x  DCNTL:%02x\n",
			np->rv_scntl3, np->rv_dmode, np->rv_dcntl);
		printf ("\t              CTEST3:%02x CTEST4:%02x CTEST5:%02x\n",
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	/*
	**    Enable GPIO0 pin for writing if LED support.
	*/

	if (np->features & FE_LED0) {
		OUTOFFB (nc_gpcntl, 0x01);
	}

	/*
	**	Reinitialize usrsync.
	**	Have to renegotiate synch mode.
	*/

	usrsync = 255;
	if (SCSI_NCR_DFLT_SYNC) {
		usrsync = SCSI_NCR_DFLT_SYNC;
		if (usrsync > np->maxsync)
			usrsync = np->maxsync;
		if (usrsync < np->minsync)
			usrsync = np->minsync;
	};

	/*
	**	Reinitialize usrwide.
	**	Have to renegotiate wide mode.
	*/

	usrwide = (SCSI_NCR_MAX_WIDE);
	if (usrwide > np->maxwide) usrwide=np->maxwide;

	/*
	**	Disable disconnects.
	*/

	np->disc = 0;

	/*
	**	Fill in target structure.
	*/

	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->sval    = 0;
		tp->wval    = np->rv_scntl3;

		tp->usrsync = usrsync;
		tp->usrwide = usrwide;

		ncr_negotiate (np, tp);
	}

	/*
	**      enable ints
	*/

	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST);
	OUTB (nc_dien , MDPE|BF|ABRT|SSI|SIR|IID);

	/*
	**    Start script processor.
	*/

	OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, start));
}

/*==========================================================
**
**	Prepare the negotiation values for wide and
**	synchronous transfers.
**
**==========================================================
*/

static void ncr_negotiate (struct ncb* np, struct tcb* tp)
{
	/*
	**	minsync unit is 4ns !
	*/

	u_long minsync = tp->usrsync;

	/*
	**	if not scsi 2
	**	don't believe FAST!
	*/

	if ((minsync < 50) && (tp->inqdata[2] & 0x0f) < 2)
		minsync=50;

	/*
	**	our limit ..
	*/

	if (minsync < np->minsync)
		minsync = np->minsync;

	/*
	**	divider limit
	*/

	if (minsync > np->maxsync)
		minsync = 255;

	tp->minsync = minsync;
	tp->maxoffs = (minsync<255 ? np->maxoffs : 0);

	/*
	**	period=0: has to negotiate sync transfer
	*/

	tp->period=0;

	/*
	**	widedone=0: has to negotiate wide transfer
	*/
	tp->widedone=0;
}

/*==========================================================
**
**	Get clock factor and sync divisor for a given 
**	synchronous factor period.
**	Returns the clock factor (in sxfer) and scntl3 
**	synchronous divisor field.
**
**==========================================================
*/

static void ncr_getsync(ncb_p np, u_char sfac, u_char *fakp, u_char *scntl3p)
{
	u_long	clk = np->clock_khz;	/* SCSI clock frequency in kHz	*/
	int	div = np->clock_divn;	/* Number of divisors supported	*/
	u_long	fak;			/* Sync factor in sxfer		*/
	u_long	per;			/* Period in tenths of ns	*/
	u_long	kpc;			/* (per * clk)			*/

	/*
	**	Compute the synchronous period in tenths of nano-seconds
	*/
	if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;

	/*
	**	Look for the greatest clock divisor that allows an 
	**	input speed faster than the period.
	*/
	kpc = per * clk;
	while (--div >= 0)
		if (kpc >= (div_10M[div] * 4)) break;

	/*
	**	Calculate the lowest clock factor that allows an output 
	**	speed not faster than the period.
	*/
	fak = (kpc - 1) / div_10M[div] + 1;

#if 0	/* You can #if 1 if you think this optimization is usefull */

	per = (fak * div_10M[div]) / clk;

	/*
	**	Why not to try the immediate lower divisor and to choose 
	**	the one that allows the fastest output speed ?
	**	We dont want input speed too much greater than output speed.
	*/
	if (div >= 1 && fak < 6) {
		u_long fak2, per2;
		fak2 = (kpc - 1) / div_10M[div-1] + 1;
		per2 = (fak2 * div_10M[div-1]) / clk;
		if (per2 < per && fak2 <= 6) {
			fak = fak2;
			per = per2;
			--div;
		}
	}
#endif

	if (fak < 4) fak = 4;	/* Should never happen, too bad ... */

	/*
	**	Compute and return sync parameters for the ncr
	*/
	*fakp		= fak - 4;
	*scntl3p	= ((div+1) << 4) + (sfac < 25 ? 0x80 : 0);
}

/*==========================================================
**
**	Switch sync mode for current job and it's target
**
**==========================================================
*/

static void ncr_setsync (ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer)
{
	struct scsipi_xfer *xp;
	tcb_p tp;
	int div;
	u_char target = INB (nc_ctest0) & 0x0f;

	assert (cp);
	if (!cp) return;

	xp = cp->xfer;
	assert (xp);
	if (!xp) return;
	assert (target == (xp->sc_link->scsipi_scsi.target & 0x0f));

	tp = &np->target[target];

	if (!scntl3 || !(sxfer & 0x1f))
		scntl3 = np->rv_scntl3;
	scntl3 = (scntl3 & 0xf0) | (tp->wval & EWS) | (np->rv_scntl3 & 0x07);

	/*
	**	Deduce the value of controller sync period from scntl3.
	**	period is in tenths of nano-seconds.
	*/

	div = ((scntl3 >> 4) & 0x7);
	if ((sxfer & 0x1f) && div)
		tp->period = (((sxfer>>5)+4)*div_10M[div-1])/np->clock_khz;
	else
		tp->period = 0xffff;

	/*
	**	 Stop there if sync parameters are unchanged
	*/

	if (tp->sval == sxfer && tp->wval == scntl3) return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	/*
	**	Bells and whistles   ;-)
	*/
	PRINT_ADDR(xp);
	if (sxfer & 0x1f) {
		unsigned f10 = 100000 << (tp->widedone ? tp->widedone -1 : 0);
		unsigned mb10 = (f10 + tp->period/2) / tp->period;
		/*
		**  Disable extended Sreq/Sack filtering
		*/
		if (tp->period <= 2000) OUTOFFB (nc_stest2, EXT);
		printf ("%d.%d MB/s (%d ns, offset %d)\n",
			mb10 / 10, mb10 % 10, tp->period / 10, sxfer & 0x1f);
	} else  printf ("asynchronous.\n");

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_sxfer, sxfer);
	np->ncb_dma->sync_st = sxfer;
	OUTB (nc_scntl3, scntl3);
	np->ncb_dma->wide_st = scntl3;

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->xfer) continue;
		if (cp->xfer->sc_link->scsipi_scsi.target != target) continue;
		cp->sync_status = sxfer;
		cp->wide_status = scntl3;
	};
}

/*==========================================================
**
**	Switch wide mode for current job and it's target
**	SCSI specs say: a SCSI device that accepts a WDTR 
**	message shall reset the synchronous agreement to 
**	asynchronous mode.
**
**==========================================================
*/

static void ncr_setwide (ncb_p np, ccb_p cp, u_char wide, u_char ack)
{
	struct scsipi_xfer *xp;
	u_short target = INB (nc_ctest0) & 0x0f;
	tcb_p tp;
	u_char	scntl3;
	u_char	sxfer;

	assert (cp);
	if (!cp) return;

	xp = cp->xfer;
	assert (xp);
	if (!xp) return;
	assert (target == (xp->sc_link->scsipi_scsi.target & 0x0f));

	tp = &np->target[target];
	tp->widedone  =  wide+1;
	scntl3 = (tp->wval & (~EWS)) | (wide ? EWS : 0);

	sxfer = ack ? 0 : tp->sval;

	/*
	**	 Stop there if sync/wide parameters are unchanged
	*/
	if (tp->sval == sxfer && tp->wval == scntl3) return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	/*
	**	Bells and whistles   ;-)
	*/
	PRINT_ADDR(xp);
	if (scntl3 & EWS)
		printf ("WIDE SCSI (16 bit) enabled\n");
	else
		printf ("WIDE SCSI disabled\n");

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_sxfer, sxfer);
	np->ncb_dma->sync_st = sxfer;
	OUTB (nc_scntl3, scntl3);
	np->ncb_dma->wide_st = scntl3;

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->xfer) continue;
		if (cp->xfer->sc_link->scsipi_scsi.target != target) continue;
		cp->sync_status = sxfer;
		cp->wide_status = scntl3;
	};
}

/*==========================================================
**
**	Switch tagged mode for a target.
**
**==========================================================
*/

static void ncr_setmaxtags (tcb_p tp, u_long usrtags)
{
	int l;
	for (l=0; l<MAX_LUN; l++) {
		lcb_p lp;
		if (!tp) break;
		lp=tp->lp[l];
		if (!lp) continue;
		ncr_settags (tp, lp, usrtags);
	};
}

static void ncr_settags (tcb_p tp, lcb_p lp, u_long usrtags)
{
	u_char reqtags, tmp;

	if ((!tp) || (!lp)) return;

	/*
	**	only devices capable of tagges commands
	**	only disk devices
	**	only if enabled by user ..
	*/
	if ((tp->inqdata[0] & 0x1f) != 0x00
	    || (tp->inqdata[7] & INQ7_QUEUE) == 0
	    || (tp->quirks & QUIRK_NOTAGS) != 0) {
	    usrtags=0;
	}
	if (usrtags) {
		reqtags = usrtags;
		if (lp->actlink <= 1)
			lp->usetags=reqtags;
	} else {
		reqtags = 1;
		if (lp->actlink <= 1)
			lp->usetags=0;
	};

	/*
	**	don't announce more than available.
	*/
	tmp = lp->actccbs;
	if (tmp > reqtags) tmp = reqtags;
	lp->reqlink = tmp;

	/*
	**	don't discard if announced.
	*/
	tmp = lp->actlink;
	if (tmp < reqtags) tmp = reqtags;
	lp->reqccbs = tmp;
	if (lp->reqlink < lp->reqccbs)
		lp->reqlink = lp->reqccbs;
}

/*----------------------------------------------------
**
**	handle user commands
**
**----------------------------------------------------
*/

static void ncr_usercmd (ncb_p np)
{
	u_char t;
	tcb_p tp;

	switch (np->user.cmd) {

	case 0: return;

	case UC_SETSYNC:
		for (t=0; t<MAX_TARGET; t++) {
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			tp->usrsync = np->user.data;
			ncr_negotiate (np, tp);
		};
		break;

	case UC_SETTAGS:
		if (np->user.data > MAX_TAGS)
			break;
		for (t=0; t<MAX_TARGET; t++) {
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			tp->usrtags = np->user.data;
			ncr_setmaxtags (tp, tp->usrtags);
		};
		break;

	case UC_SETDEBUG:
		ncr_debug = np->user.data;
		break;

	case UC_SETORDER:
		np->order = np->user.data;
		break;

	case UC_SETWIDE:
		for (t=0; t<MAX_TARGET; t++) {
			u_long size;
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			size = np->user.data;
			if (size > np->maxwide) size=np->maxwide;
			tp->usrwide = size;
			ncr_negotiate (np, tp);
		};
		break;

	case UC_SETFLAG:
		for (t=0; t<MAX_TARGET; t++) {
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			tp->usrflag = np->user.data;
		};
		break;
	}
	np->user.cmd=0;
}




/*==========================================================
**
**
**	ncr timeout handler.
**
**
**==========================================================
**
**	Misused to keep the driver running when
**	interrupts are not configured correctly.
**
**----------------------------------------------------------
*/

static void ncr_timeout (void *arg)
{
	ncb_p	np = arg;
#ifdef __NetBSD__
	u_long	thistime = mono_time.tv_sec;
#else
	u_long	thistime = time.tv_sec;
#endif
	u_long	step  = np->ticks;
	u_long	count = 0;
	long signed   t;
	ccb_p cp;

	if (np->lasttime != thistime) {
		/*
		**	block ncr interrupts
		*/
		int oldspl = splbio();
		np->lasttime = thistime;

		ncr_usercmd (np);

		/*----------------------------------------------------
		**
		**	handle ncr chip timeouts
		**
		**	Assumption:
		**	We have a chance to arbitrate for the
		**	SCSI bus at least every 10 seconds.
		**
		**----------------------------------------------------
		*/

		t = thistime - np->ncb_dma->heartbeat;

		if (t<2) np->latetime=0; else np->latetime++;

		if (np->latetime>2) {
			/*
			**      If there are no requests, the script
			**      processor will sleep on SEL_WAIT_RESEL.
			**      But we have to check whether it died.
			**      Let's try to wake it up.
			*/
			OUTB (nc_istat, SIGP);
		};

		/*----------------------------------------------------
		**
		**	handle ccb timeouts
		**
		**----------------------------------------------------
		*/

		for (cp=np->ccb; cp; cp=cp->link_ccb) {
			/*
			**	look for timed out ccbs.
			*/
			if (!cp->host_status) continue;
			count++;
			if (cp->tlimit > thistime) continue;

			/*
			**	Disable reselect.
			**      Remove it from startqueue.
			*/
			cp->jump_ccb.l_cmd = htole32(SCR_JUMP);
			if (cp->phys.header.launch.l_paddr ==
				htole32(NCB_SCRIPT_PHYS (np, select))) {
				printf ("%s: timeout ccb=%p (skip)\n",
					ncr_name (np), cp);
				cp->phys.header.launch.l_paddr
				= htole32(NCB_SCRIPT_PHYS (np, skip));
			};

			switch (cp->host_status) {

			case HS_BUSY:
			case HS_NEGOTIATE:
				/*
				** still in start queue ?
				*/
				if (cp->phys.header.launch.l_paddr ==
					htole32(NCB_SCRIPT_PHYS (np, skip)))
					continue;

				/* fall through */
			case HS_DISCONNECT:
				cp->host_status=HS_TIMEOUT;
			};
			cp->tag = 0;

			/*
			**	wakeup this ccb.
			*/
			ncr_complete (np, cp);
		};
		splx (oldspl);
	}

	callout_reset (&np->sc_timo_ch, step ? step : 1, ncr_timeout, np);

	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		/*
		**	Process pending interrupts.
		*/

		int	oldspl	= splbio ();
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("{");
		ncr_exception (np);
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("}");
		splx (oldspl);
	};
}

/*==========================================================
**
**	log message for real hard errors
**
**	"ncr0 targ 0?: ERROR (ds:si) (so-si-sd) (sxfer/scntl3) @ name (dsp:dbc)."
**	"	      reg: r0 r1 r2 r3 r4 r5 r6 ..... rf."
**
**	exception register:
**		ds:	dstat
**		si:	sist
**
**	SCSI bus lines:
**		so:	control lines as driven by NCR.
**		si:	control lines as seen by NCR.
**		sd:	scsi data lines as seen by NCR.
**
**	wide/fastmode:
**		sxfer:	(see the manual)
**		scntl3:	(see the manual)
**
**	current script command:
**		dsp:	script adress (relative to start of script).
**		dbc:	first word of script command.
**
**	First 16 register of the chip:
**		r0..rf
**
**==========================================================
*/

static void ncr_log_hard_error(ncb_p np, u_short sist, u_char dstat)
{
	u_int32_t dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL (nc_dsp);

	if (np->p_script < dsp && 
	    dsp <= np->p_script + sizeof(struct script)) {
		script_ofs	= dsp - np->p_script;
		script_size	= sizeof(struct script);
		script_base	= (u_char *) np->script;
		script_name	= "script";
	}
	else if (np->p_scripth < dsp && 
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		script_ofs	= dsp - np->p_scripth;
		script_size	= sizeof(struct scripth);
		script_base	= (u_char *) np->scripth;
		script_name	= "scripth";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= 0;
		script_name	= "mem";
	}

	printf ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%s %x:%08x).\n",
		ncr_name (np), (unsigned)INB (nc_ctest0)&0x0f, dstat, sist,
		(unsigned)INB (nc_socl), (unsigned)INB (nc_sbcl), (unsigned)INB (nc_sbdl),
		(unsigned)INB (nc_sxfer),(unsigned)INB (nc_scntl3), script_name, script_ofs,
		(unsigned)INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printf ("%s: script cmd = %08x\n", ncr_name(np),
		    (int)READSCRIPT_OFF(script_base, script_ofs));
	}

        printf ("%s: regdump:", ncr_name(np));
        for (i=0; i<16;i++)
            printf (" %02x", (unsigned)INB_OFF(i));
        printf (".\n");
}

/*==========================================================
**
**
**	ncr chip exception handler.
**
**
**==========================================================
*/

void ncr_exception (ncb_p np)
{
	U_INT8	istat, dstat;
	U_INT16	sist;

	/*
	**	interrupt on the fly ?
	*/
	while ((istat = INB (nc_istat)) & INTF) {
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("F ");
		OUTB (nc_istat, INTF);
		np->profile.num_fly++;
		ncr_wakeup (np, 0);
	};
	if (!(istat & (SIP|DIP))) {
		return;
	}

	/*
	**	Steinbach's Guideline for Systems Programming:
	**	Never test for an error condition you don't know how to handle.
	*/

	sist  = (istat & SIP) ? INW (nc_sist)  : 0;
	dstat = (istat & DIP) ? INB (nc_dstat) : 0;
	np->profile.num_int++;

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("<%d|%x:%x|%x:%x>",
			INB(nc_scr0),
			dstat,sist,
			(unsigned)INL(nc_dsp),
			(unsigned)INL(nc_dbc));
	if ((dstat==DFE) && (sist==PAR)) return;

/*==========================================================
**
**	First the normal cases.
**
**==========================================================
*/
	/*-------------------------------------------
	**	SCSI reset
	**-------------------------------------------
	*/

	if (sist & RST) {
		ncr_init (np, bootverbose ? "scsi reset" : NULL, HS_RESET);
		return;
	};

	/*-------------------------------------------
	**	selection timeout
	**
	**	IID excluded from dstat mask!
	**	(chip bug)
	**-------------------------------------------
	*/

	if ((sist  & STO) &&
		!(sist  & (GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR))) {
		ncr_int_sto (np);
		return;
	};

	/*-------------------------------------------
	**      Phase mismatch.
	**-------------------------------------------
	*/

	if ((sist  & MA) &&
		!(sist  & (STO|GEN|HTH|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		ncr_int_ma (np, dstat);
		return;
	};

	/*----------------------------------------
	**	move command with length 0
	**----------------------------------------
	*/

	if ((dstat & IID) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR)) &&
		((INL(nc_dbc) & 0xf8000000) == SCR_MOVE_TBL)) {
		/*
		**      Target wants more data than available.
		**	The "no_data" script will do it.
		*/
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, no_data));
		return;
	};

	/*-------------------------------------------
	**	Programmed interrupt
	**-------------------------------------------
	*/

	if ((dstat & SIR) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|IID)) &&
		(INB(nc_dsps) <= SIR_MAX)) {
		ncr_int_sir (np);
		return;
	};

	/*========================================
	**	log message for real hard errors
	**========================================
	*/

	ncr_log_hard_error(np, sist, dstat);

	/*========================================
	**	do the register dump
	**========================================
	*/

#ifdef __NetBSD__
	if (mono_time.tv_sec - np->regtime.tv_sec>10) 
#else
	if (time.tv_sec - np->regtime.tv_sec>10)
#endif
	/* if */ {
		int i;
#ifdef __NetBSD__
		np->regtime = mono_time;
#else
		gettime(&np->regtime);
#endif
		for (i=0; i<sizeof(np->regdump); i++)
			((char*)&np->regdump)[i] = INB_OFF(i);
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	};


	/*----------------------------------------
	**	clean up the dma fifo
	**----------------------------------------
	*/

	if ( (INB(nc_sstat0) & (ILF|ORF|OLF)   ) ||
	     (INB(nc_sstat1) & (FF3210)	) ||
	     (INB(nc_sstat2) & (ILF1|ORF1|OLF1)) ||	/* wide .. */
	     !(dstat & DFE)) {
		printf ("%s: have to clear fifos.\n", ncr_name (np));
		OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);
						/* clear dma fifo  */
	}

	/*----------------------------------------
	**	handshake timeout
	**----------------------------------------
	*/

	if (sist & HTH) {
		printf ("%s: handshake timeout\n", ncr_name(np));
		OUTB (nc_scntl1, CRST);
		DELAY (1000);
		OUTB (nc_scntl1, 0x00);
		OUTB (nc_scr0, HS_FAIL);
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, cleanup));
		return;
	}

	/*----------------------------------------
	**	unexpected disconnect
	**----------------------------------------
	*/

	if ((sist  & UDC) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		OUTB (nc_scr0, HS_UNEXPECTED);
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, cleanup));
		return;
	};

	/*----------------------------------------
	**	cannot disconnect
	**----------------------------------------
	*/

	if ((dstat & IID) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR)) &&
		((INL(nc_dbc) & 0xf8000000) == SCR_WAIT_DISC)) {
		/*
		**      Unexpected data cycle while waiting for disconnect.
		*/
		if (INB(nc_sstat2) & LDSC) {
			/*
			**	It's an early reconnect.
			**	Let's continue ...
			*/
			OUTB (nc_dcntl, np->rv_dcntl | STD);
			/*
			**	info message
			*/
			printf ("%s: INFO: LDSC while IID.\n",
				ncr_name (np));
			return;
		};
		printf ("%s: target %d doesn't release the bus.\n",
			ncr_name (np), INB (nc_ctest0)&0x0f);
		/*
		**	return without restarting the NCR.
		**	timeout will do the real work.
		*/
		return;
	};

	/*----------------------------------------
	**	single step
	**----------------------------------------
	*/

	if ((dstat & SSI) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		OUTB (nc_dcntl, np->rv_dcntl | STD);
		return;
	};

/*
**	@RECOVER@ HTH, SGE, ABRT.
**
**	We should try to recover from these interrupts.
**	They may occur if there are problems with synch transfers, or 
**	if targets are switched on or off while the driver is running.
*/

	if (sist & SGE) {
		/* clear scsi offsets */
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);
	}

	/*
	**	Freeze controller to be able to read the messages.
	*/

	if (DEBUG_FLAGS & DEBUG_FREEZE) {
		int i;
		unsigned char val;
		for (i=0; i<0x60; i++) {
			switch (i%16) {

			case 0:
				printf ("%s: reg[%d0]: ",
					ncr_name(np),i/16);
				break;
			case 4:
			case 8:
			case 12:
				printf (" ");
				break;
			};
			val = INB_OFF(i);
			printf (" %x%x", val/16, val%16);
			if (i%16==15) printf (".\n");
		};

		callout_stop (&np->sc_timo_ch);

		printf ("%s: halted!\n", ncr_name(np));
		/*
		**	don't restart controller ...
		*/
		OUTB (nc_istat,  SRST);
		return;
	};

#ifdef NCR_FREEZE
	/*
	**	Freeze system to be able to read the messages.
	*/
	printf ("ncr: fatal error: system halted - press reset to reboot ...");
	(void) splhigh();
	for (;;);
#endif

	/*
	**	sorry, have to kill ALL jobs ...
	*/

	ncr_init (np, "fatal error", HS_FAIL);
}

/*==========================================================
**
**	ncr chip exception handler for selection timeout
**
**==========================================================
**
**	There seems to be a bug in the 53c810.
**	Although a STO-Interrupt is pending,
**	it continues executing script commands.
**	But it will fail and interrupt (IID) on
**	the next instruction where it's looking
**	for a valid phase.
**
**----------------------------------------------------------
*/

void ncr_int_sto (ncb_p np)
{
	u_long dsa, scratcha, diff;
	ccb_p cp;
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("T");

	/*
	**	look for ccb and set the status.
	*/

	dsa = INL (nc_dsa);
	cp = np->ccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_ccb;

	if (cp) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	/*
	**	repair start queue
	*/

	scratcha = INL (nc_scratcha);
	diff = scratcha - NCB_SCRIPTH_PHYS (np, tryloop);

/*	assert ((diff <= MAX_START * 20) && !(diff % 20));*/

	if ((diff <= MAX_START * 20) && !(diff % 20)) {
		WRITESCRIPT(startpos[0], scratcha);
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, start));
		return;
	};
	ncr_init (np, "selection timeout", HS_FAIL);
}

/*==========================================================
**
**
**	ncr chip exception handler for phase errors.
**
**
**==========================================================
**
**	We have to construct a new transfer descriptor,
**	to transfer the rest of the current block.
**
**----------------------------------------------------------
*/

static void ncr_int_ma (ncb_p np, u_char dstat)
{
	u_int32_t	dbc;
	u_int32_t	rest;
	u_int32_t	dsa;
	u_int32_t	dsp;
	u_int32_t	nxtdsp;
	void	*vdsp_base;
	size_t	vdsp_off;
	u_int32_t	oadr, olen;
	u_int32_t	*tblp;
	ncrcmd	*newcmd;
	U_INT32	cmd, sbcl, delta, ss0, ss2, ctest5;
	ccb_p	cp;

	dsp = INL (nc_dsp);
	dsa = INL (nc_dsa);
	dbc = INL (nc_dbc);
	ss0 = INB (nc_sstat0);
	ss2 = INB (nc_sstat2);
	sbcl= INB (nc_sbcl);

	cmd = dbc >> 24;
	rest= dbc & 0xffffff;

	ctest5 = (np->rv_ctest5 & DFS) ? INB (nc_ctest5) : 0;
	if (ctest5 & DFS)
		delta=(((ctest5<<8) | (INB (nc_dfifo) & 0xff)) - rest) & 0x3ff;
	else
		delta=(INB (nc_dfifo) - rest) & 0x7f;


	/*
	**	The data in the dma fifo has not been transfered to
	**	the target -> add the amount to the rest
	**	and clear the data.
	**	Check the sstat2 register in case of wide transfer.
	*/

	if (!(dstat & DFE)) rest += delta;
	if (ss0 & OLF) rest++;
	if (ss0 & ORF) rest++;
	if (INB(nc_scntl3) & EWS) {
		if (ss2 & OLF1) rest++;
		if (ss2 & ORF1) rest++;
	};
	OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);		/* clear scsi fifo */

	/*
	**	locate matching cp
	*/
	dsa = INL (nc_dsa);
	cp = np->ccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_ccb;

	if (!cp) {
	    printf ("%s: SCSI phase error fixup: CCB already dequeued (%p)\n", 
		    ncr_name (np), np->ncb_dma->header.cp);
	    return;
	}
	if (cp != np->ncb_dma->header.cp) {
	    printf ("%s: SCSI phase error fixup: CCB address mismatch (0x%08lx != 0x%08lx) np->ccb = %p\n", 
		    ncr_name (np), (u_long) cp, (u_long) np->ncb_dma->header.cp, np->ccb);
/*	    return;*/
	}

	/*
	**	find the interrupted script command,
	**	and the address at which to continue.
	*/

	if (dsp == vtophys((vaddr_t)&cp->patch[2])) {
		vdsp_base = cp;
		vdsp_off = offsetof(struct ccb, patch[0]);
		nxtdsp = READSCRIPT_OFF(vdsp_base, vdsp_off + 3*4);
	} else if (dsp == vtophys((vaddr_t)&cp->patch[6])) {
		vdsp_base = cp;
		vdsp_off = offsetof(struct ccb, patch[4]);
		nxtdsp = READSCRIPT_OFF(vdsp_base, vdsp_off + 3*4);
	} else if (dsp > np->p_script &&
		   dsp <= np->p_script + sizeof(struct script)) {
		vdsp_base = np->script;
		vdsp_off = dsp - np->p_script - 8;
		nxtdsp = dsp;
	} else {
		vdsp_base = np->scripth;
		vdsp_off = dsp - np->p_scripth - 8;
		nxtdsp = dsp;
	};

	/*
	**	log the information
	*/
	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE)) {
		printf ("P%x%x ",cmd&7, sbcl&7);
		printf ("RL=%d D=%d SS0=%x ",
			(unsigned) rest, (unsigned) delta, ss0);
	};
	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("\nCP=%p CP2=%p DSP=%x NXT=%x VDSP_BASE=%p VDSP_OFF=0x%x CMD=%x ",
			cp, np->ncb_dma->header.cp, (unsigned)dsp,
			(unsigned)nxtdsp, vdsp_base, (unsigned)vdsp_off, cmd);
	};

	/*
	**	get old startaddress and old length.
	*/

	oadr = READSCRIPT_OFF(vdsp_base, vdsp_off + 1*4);

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u_int32_t *) ((char*) &cp->phys + oadr);
		olen = le32toh(tblp[0]);
		oadr = le32toh(tblp[1]);
	} else {
		tblp = (u_int32_t *) 0;
		olen = READSCRIPT_OFF(vdsp_base, vdsp_off) & 0xffffff;
	};

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (READSCRIPT_OFF(vdsp_base, vdsp_off) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	};

	/*
	**	if old phase not dataphase, leave here.
	*/

	if (cmd != (READSCRIPT_OFF(vdsp_base, vdsp_off) >> 24)) {
		PRINT_ADDR(cp->xfer);
		printf ("internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			(unsigned)cmd,
			(unsigned)READSCRIPT_OFF(vdsp_base, vdsp_off) >> 24);
		
		return;
	}
	if (cmd & 0x06) {
		PRINT_ADDR(cp->xfer);
		printf ("phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, sbcl&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);

		OUTB (nc_dcntl, np->rv_dcntl | STD);
		return;
	};

	/*
	**	choose the correct patch area.
	**	if savep points to one, choose the other.
	*/

	newcmd = cp->patch;
	if (cp->phys.header.savep == htole32(vtophys((vaddr_t)newcmd)))
		newcmd += 4;

	/*
	**	fillin the commands
	*/

	newcmd[0] = htole32(((cmd & 0x0f) << 24) | rest);
	newcmd[1] = htole32(oadr + olen - rest);
	newcmd[2] = htole32(SCR_JUMP);
	newcmd[3] = htole32(nxtdsp);

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		PRINT_ADDR(cp->xfer);
		printf ("newcmd[%ld] %x %x %x %x.\n",
			(long)(newcmd - cp->patch),
			(unsigned)le32toh(newcmd[0]),
			(unsigned)le32toh(newcmd[1]),
			(unsigned)le32toh(newcmd[2]),
			(unsigned)le32toh(newcmd[3]));
	}
	/*
	**	fake the return address (to the patch).
	**	and restart script processor at dispatcher.
	*/
	np->profile.num_break++;
	OUTL (nc_temp, vtophys((vaddr_t)newcmd));
	if ((cmd & 7) == 0)
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, dispatch));
	else
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, checkatn));
}

/*==========================================================
**
**
**      ncr chip exception handler for programmed interrupts.
**
**
**==========================================================
*/

static int ncr_show_msg (u_char * msg)
{
	u_char i;
	printf ("%x",*msg);
	if (*msg==M_EXTENDED) {
		for (i=1;i<8;i++) {
			if (i-1>msg[1]) break;
			printf ("-%x",msg[i]);
		};
		return (i+1);
	} else if ((*msg & 0xf0) == 0x20) {
		printf ("-%x",msg[1]);
		return (2);
	};
	return (1);
}

void ncr_int_sir (ncb_p np)
{
	u_char scntl3;
	u_char chg, ofs, per, fak, wide;
	u_char num = INB (nc_dsps);
	ccb_p	cp=0;
	u_long	dsa;
	u_char	target = INB (nc_ctest0) & 0x0f;
	tcb_p	tp     = &np->target[target];
	int     i;
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("I#%d", num);

	switch (num) {
	case SIR_SENSE_RESTART:
	case SIR_STALL_RESTART:
		break;

	default:
		/*
		**	lookup the ccb
		*/
		dsa = INL (nc_dsa);
		cp = np->ccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_ccb;

		assert (cp);
		if (!cp)
			goto out;
		assert (cp == np->ncb_dma->header.cp);
		if (cp != np->ncb_dma->header.cp)
			goto out;
	}

	switch (num) {

/*--------------------------------------------------------------------
**
**	Processing of interrupted getcc selects
**
**--------------------------------------------------------------------
*/

	case SIR_SENSE_RESTART:
		/*------------------------------------------
		**	Script processor is idle.
		**	Look for interrupted "check cond"
		**------------------------------------------
		*/

		if (DEBUG_FLAGS & DEBUG_RESTART)
			printf ("%s: int#%d",ncr_name (np),num);
		cp = (ccb_p) 0;
		for (i=0; i<MAX_TARGET; i++) {
			if (DEBUG_FLAGS & DEBUG_RESTART) printf (" t%d", i);
			tp = &np->target[i];
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("+");
			cp = tp->hold_cp;
			if (!cp) continue;
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("+");
			if ((cp->host_status==HS_BUSY) &&
				(cp->scsi_status==S_CHECK_COND))
				break;
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("- (remove)");
			tp->hold_cp = cp = (ccb_p) 0;
		};

		if (cp) {
			if (DEBUG_FLAGS & DEBUG_RESTART)
				printf ("+ restart job ..\n");
			OUTL (nc_dsa, CCB_PHYS (cp, phys));
			OUTL (nc_dsp, NCB_SCRIPTH_PHYS (np, getcc));
			return;
		};

		/*
		**	no job, resume normal processing
		*/
		if (DEBUG_FLAGS & DEBUG_RESTART) printf (" -- remove trap\n");
		WRITESCRIPT(start0[0], SCR_INT ^ IFFALSE (0));
		break;

	case SIR_SENSE_FAILED:
		/*-------------------------------------------
		**	While trying to select for
		**	getting the condition code,
		**	a target reselected us.
		**-------------------------------------------
		*/
		if (DEBUG_FLAGS & DEBUG_RESTART) {
			PRINT_ADDR(cp->xfer);
			printf ("in getcc reselect by t%d.\n",
				INB(nc_ssid) & 0x0f);
		}

		/*
		**	Mark this job
		*/
		cp->host_status = HS_BUSY;
		cp->scsi_status = S_CHECK_COND;
		np->target[cp->xfer->sc_link->scsipi_scsi.target].hold_cp = cp;

		/*
		**	And patch code to restart it.
		*/
		WRITESCRIPT(start0[0], SCR_INT);
		break;

/*-----------------------------------------------------------------------------
**
**	Was Sie schon immer ueber transfermode negotiation wissen wollten ...
**
**	We try to negotiate sync and wide transfer only after
**	a successfull inquire command. We look at byte 7 of the
**	inquire data to determine the capabilities if the target.
**
**	When we try to negotiate, we append the negotiation message
**	to the identify and (maybe) simple tag message.
**	The host status field is set to HS_NEGOTIATE to mark this
**	situation.
**
**	If the target doesn't answer this message immidiately
**	(as required by the standard), the SIR_NEGO_FAIL interrupt
**	will be raised eventually.
**	The handler removes the HS_NEGOTIATE status, and sets the
**	negotiated value to the default (async / nowide).
**
**	If we receive a matching answer immediately, we check it
**	for validity, and set the values.
**
**	If we receive a Reject message immediately, we assume the
**	negotiation has failed, and fall back to standard values.
**
**	If we receive a negotiation message while not in HS_NEGOTIATE
**	state, it's a target initiated negotiation. We prepare a
**	(hopefully) valid answer, set our parameters, and send back 
**	this answer to the target.
**
**	If the target doesn't fetch the answer (no message out phase),
**	we assume the negotiation has failed, and fall back to default
**	settings.
**
**	When we set the values, we adjust them in all ccbs belonging 
**	to this target, in the controller's register, and in the "phys"
**	field of the controller's struct ncb.
**
**	Possible cases:		   hs  sir   msg_in value  send   goto
**	We try try to negotiate:
**	-> target doesnt't msgin   NEG FAIL  noop   defa.  -      dispatch
**	-> target rejected our msg NEG FAIL  reject defa.  -      dispatch
**	-> target answered  (ok)   NEG SYNC  sdtr   set    -      clrack
**	-> target answered (!ok)   NEG SYNC  sdtr   defa.  REJ--->msg_bad
**	-> target answered  (ok)   NEG WIDE  wdtr   set    -      clrack
**	-> target answered (!ok)   NEG WIDE  wdtr   defa.  REJ--->msg_bad
**	-> any other msgin	   NEG FAIL  noop   defa.  -      dispatch
**
**	Target tries to negotiate:
**	-> incoming message	   --- SYNC  sdtr   set    SDTR   -
**	-> incoming message	   --- WIDE  wdtr   set    WDTR   -
**      We sent our answer:
**	-> target doesn't msgout   --- PROTO ?      defa.  -      dispatch
**
**-----------------------------------------------------------------------------
*/

	case SIR_NEGO_FAILED:
		/*-------------------------------------------------------
		**
		**	Negotiation failed.
		**	Target doesn't send an answer message,
		**	or target rejected our message.
		**
		**      Remove negotiation request.
		**
		**-------------------------------------------------------
		*/
		OUTB (HS_PRT, HS_BUSY);

		/* fall through */

	case SIR_NEGO_PROTO:
		/*-------------------------------------------------------
		**
		**	Negotiation failed.
		**	Target doesn't fetch the answer message.
		**
		**-------------------------------------------------------
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("negotiation failed sir=%x status=%x.\n",
				num, cp->nego_status);
		};

		/*
		**	any error in negotiation:
		**	fall back to default mode.
		*/
		switch (cp->nego_status) {

		case NS_SYNC:
			ncr_setsync (np, cp, 0, 0xe0);
			break;

		case NS_WIDE:
			ncr_setwide (np, cp, 0, 0);
			break;

		};
		np->ncb_dma->msgin [0] = M_NOOP;
		np->ncb_dma->msgout[0] = M_NOOP;
		cp->nego_status = 0;
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, dispatch));
		break;

	case SIR_NEGO_SYNC:
		/*
		**	Synchronous request message received.
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgin: ");
			(void) ncr_show_msg (np->ncb_dma->msgin);
			printf (".\n");
		};

		/*
		**	get requested values.
		*/

		chg = 0;
		per = np->ncb_dma->msgin[3];
		ofs = np->ncb_dma->msgin[4];
		if (ofs==0) per=255;

		/*
		**      if target sends SDTR message,
		**	      it CAN transfer synch.
		*/

		if (ofs)
			tp->inqdata[7] |= INQ7_SYNC;

		/*
		**	check values against driver limits.
		*/

		if (per < np->minsync)
			{chg = 1; per = np->minsync;}
		if (per < tp->minsync)
			{chg = 1; per = tp->minsync;}
		if (ofs > tp->maxoffs)
			{chg = 1; ofs = tp->maxoffs;}

		/*
		**	Check against controller limits.
		*/

		fak	= 7;
		scntl3	= 0;
		if (ofs != 0) {
			ncr_getsync(np, per, &fak, &scntl3);
			if (fak > 7) {
				chg = 1;
				ofs = 0;
			}
		}
		if (ofs == 0) {
			fak	= 7;
			per	= 0;
			scntl3	= 0;
			tp->minsync = 0;
		}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync: per=%d scntl3=0x%x ofs=%d fak=%d chg=%d.\n",
				per, scntl3, ofs, fak, chg);
		}

		if (INB (HS_PRT) == HS_NEGOTIATE) {
			OUTB (HS_PRT, HS_BUSY);
			switch (cp->nego_status) {

			case NS_SYNC:
				/*
				**      This was an answer message
				*/
				if (chg) {
					/*
					**	Answer wasn't acceptable.
					*/
					ncr_setsync (np, cp, 0, 0xe0);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setsync (np,cp,scntl3,(fak<<5)|ofs);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_WIDE:
				ncr_setwide (np, cp, 0, 0);
				break;
			};
		};

		/*
		**	It was a request. Set value and
		**      prepare an answer message
		*/

		ncr_setsync (np, cp, scntl3, (fak<<5)|ofs);

		np->ncb_dma->msgout[0] = M_EXTENDED;
		np->ncb_dma->msgout[1] = 3;
		np->ncb_dma->msgout[2] = M_X_SYNC_REQ;
		np->ncb_dma->msgout[3] = per;
		np->ncb_dma->msgout[4] = ofs;

		cp->nego_status = NS_SYNC;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgout: ");
			(void) ncr_show_msg (np->ncb_dma->msgout);
			printf (".\n");
		}

		if (!ofs) {
			OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
			return;
		}
		np->ncb_dma->msgin [0] = M_NOOP;

		break;

	case SIR_NEGO_WIDE:
		/*
		**	Wide request message received.
		*/
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("wide msgin: ");
			(void) ncr_show_msg (np->ncb_dma->msgin);
			printf (".\n");
		};

		/*
		**	get requested values.
		*/

		chg  = 0;
		wide = np->ncb_dma->msgin[3];

		/*
		**      if target sends WDTR message,
		**	      it CAN transfer wide.
		*/

		if (wide)
			tp->inqdata[7] |= INQ7_WIDE16;

		/*
		**	check values against driver limits.
		*/

		if (wide > tp->usrwide)
			{chg = 1; wide = tp->usrwide;}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("wide: wide=%d chg=%d.\n", wide, chg);
		}

		if (INB (HS_PRT) == HS_NEGOTIATE) {
			OUTB (HS_PRT, HS_BUSY);
			switch (cp->nego_status) {

			case NS_WIDE:
				/*
				**      This was an answer message
				*/
				if (chg) {
					/*
					**	Answer wasn't acceptable.
					*/
					ncr_setwide (np, cp, 0, 1);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setwide (np, cp, wide, 1);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_SYNC:
				ncr_setsync (np, cp, 0, 0xe0);
				break;
			};
		};

		/*
		**	It was a request, set value and
		**      prepare an answer message
		*/

		ncr_setwide (np, cp, wide, 1);

		np->ncb_dma->msgout[0] = M_EXTENDED;
		np->ncb_dma->msgout[1] = 2;
		np->ncb_dma->msgout[2] = M_X_WIDE_REQ;
		np->ncb_dma->msgout[3] = wide;

		np->ncb_dma->msgin [0] = M_NOOP;

		cp->nego_status = NS_WIDE;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("wide msgout: ");
			(void) ncr_show_msg (np->ncb_dma->msgout);
			printf (".\n");
		}
		break;

/*--------------------------------------------------------------------
**
**	Processing of special messages
**
**--------------------------------------------------------------------
*/

	case SIR_REJECT_RECEIVED:
		/*-----------------------------------------------
		**
		**	We received a M_REJECT message.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_REJECT received (%x:%x).\n",
			(unsigned)np->ncb_dma->lastmsg, np->ncb_dma->msgout[0]);
		break;

	case SIR_REJECT_SENT:
		/*-----------------------------------------------
		**
		**	We received an unknown message
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_REJECT sent for ");
		(void) ncr_show_msg (np->ncb_dma->msgin);
		printf (".\n");
		break;

/*--------------------------------------------------------------------
**
**	Processing of special messages
**
**--------------------------------------------------------------------
*/

	case SIR_IGN_RESIDUE:
		/*-----------------------------------------------
		**
		**	We received an IGNORE RESIDUE message,
		**	which couldn't be handled by the script.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_IGN_RESIDUE received, but not yet implemented.\n");
		break;

	case SIR_MISSING_SAVE:
		/*-----------------------------------------------
		**
		**	We received an DISCONNECT message,
		**	but the datapointer wasn't saved before.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_DISCONNECT received, but datapointer not saved:\n"
			"\tdata=%x save=%x goal=%x.\n",
			(unsigned) INL (nc_temp),
			le32toh((unsigned) np->ncb_dma->header.savep),
			le32toh((unsigned) np->ncb_dma->header.goalp));
		break;

/*--------------------------------------------------------------------
**
**	Processing of a "S_QUEUE_FULL" status.
**
**	The current command has been rejected,
**	because there are too many in the command queue.
**	We have started too many commands for that target.
**
**	If possible, reinsert at head of queue.
**	Stall queue until there are no disconnected jobs
**	(ncr is REALLY idle). Then restart processing.
**
**	We should restart the current job after the controller
**	has become idle. But this is not yet implemented.
**
**--------------------------------------------------------------------
*/
	case SIR_STALL_QUEUE:
		/*-----------------------------------------------
		**
		**	Stall the start queue.
		**
		**-----------------------------------------------
		*/
		PRINT_ADDR(cp->xfer);
		printf ("queue full.\n");

		WRITESCRIPT(start1[0], SCR_INT);

		/*
		**	Try to disable tagged transfers.
		*/
		ncr_setmaxtags (&np->target[target], 0);

		/*
		** @QUEUE@
		**
		**	Should update the launch field of the
		**	current job to be able to restart it.
		**	Then prepend it to the start queue.
		*/

		/* fall through */

	case SIR_STALL_RESTART:
		/*-----------------------------------------------
		**
		**	Enable selecting again,
		**	if NO disconnected jobs.
		**
		**-----------------------------------------------
		*/
		/*
		**	Look for a disconnected job.
		*/
		cp = np->ccb;
		while (cp && cp->host_status != HS_DISCONNECT)
			cp = cp->link_ccb;

		/*
		**	if there is one, ...
		*/
		if (cp) {
			/*
			**	wait for reselection
			*/
			OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, reselect));
			return;
		};

		/*
		**	else remove the interrupt.
		*/

		printf ("%s: queue empty.\n", ncr_name (np));
		WRITESCRIPT(start1[0], SCR_INT ^ IFFALSE (0));
		break;
	};

out:
	OUTB (nc_dcntl, np->rv_dcntl | STD);
}

/*==========================================================
**
**
**	Aquire a control block
**
**
**==========================================================
*/

static	ccb_p ncr_get_ccb
	(ncb_p np, u_long flags, u_long target, u_long lun)
{
	lcb_p lp;
	ccb_p cp = (ccb_p) 0;
	int oldspl;

	oldspl = splbio();
	/*
	**	Lun structure available ?
	*/

	lp = np->target[target].lp[lun];
	if (lp) {
		cp = lp->next_ccb;

		/*
		**	Look for free CCB
		*/

		while (cp && cp->magic) {
			cp = cp->next_ccb;
		}
	}

	/*
	**	if nothing available, take the default.
	*/

	if (!cp) cp = np->ccb;

	/*
	**	Wait until available.
	*/

	while (cp->magic) {
		if (flags & XS_CTL_NOSLEEP) break;
		if (tsleep ((caddr_t)cp, PRIBIO|PCATCH, "ncr", 0))
			break;
	};

	if (cp->magic) {
		splx(oldspl);
		return ((ccb_p) 0);
	}

	cp->magic = 1;
	splx(oldspl);
	return (cp);
}

/*==========================================================
**
**
**	Release one control block
**
**
**==========================================================
*/

void ncr_free_ccb (ncb_p np, ccb_p cp, int flags)
{
	/*
	**    sanity
	*/

	assert (cp != NULL);

	cp -> host_status = HS_IDLE;
	cp -> magic = 0;
	if (cp == np->ccb)
		wakeup ((caddr_t) cp);
}

/*==========================================================
**
**
**      Allocation of resources for Targets/Luns/Tags.
**
**
**==========================================================
*/

static	void ncr_alloc_ccb (ncb_p np, u_long target, u_long lun)
{
	tcb_p tp;
	lcb_p lp;
	ccb_p cp;

	assert (np != NULL);

	if (target>=MAX_TARGET) return;
	if (lun   >=MAX_LUN   ) return;

	tp=&np->target[target];

	if (!tp->jump_tcb.l_cmd) {

		/*
		**	initialize it.
		*/
		tp->jump_tcb.l_cmd   = htole32(SCR_JUMP^IFFALSE (DATA (0x80 + target)));
		tp->jump_tcb.l_paddr = np->ncb_dma->jump_tcb.l_paddr;

		tp->getscr[0] =
			(np->features & FE_PFEN)? htole32(SCR_COPY(1)) : htole32(SCR_COPY_F(1));
		tp->getscr[1] = htole32(vtophys((vaddr_t)&tp->sval));
		tp->getscr[2] = htole32(np->paddr + offsetof (struct ncr_reg, nc_sxfer));
		tp->getscr[3] = tp->getscr[0];
		tp->getscr[4] = htole32(vtophys((vaddr_t)&tp->wval));
		tp->getscr[5] = htole32(np->paddr + offsetof (struct ncr_reg, nc_scntl3));

		assert (( (offsetof(struct ncr_reg, nc_sxfer) ^
			offsetof(struct tcb    , sval    )) &3) == 0);
		assert (( (offsetof(struct ncr_reg, nc_scntl3) ^
			offsetof(struct tcb    , wval    )) &3) == 0);

		tp->call_lun.l_cmd   = htole32(SCR_CALL);
		tp->call_lun.l_paddr = htole32(NCB_SCRIPT_PHYS (np, resel_lun));

		tp->jump_lcb.l_cmd   = htole32(SCR_JUMP);
		tp->jump_lcb.l_paddr = htole32(NCB_SCRIPTH_PHYS (np, abort));
		np->ncb_dma->jump_tcb.l_paddr = htole32(vtophys((vaddr_t)&tp->jump_tcb));

		tp->usrtags = SCSI_NCR_DFLT_TAGS;
		ncr_setmaxtags (tp, tp->usrtags);
	}

	/*
	**	Logic unit control block
	*/
	lp = tp->lp[lun];
	if (!lp) {
		/*
		**	Allocate a lcb
		*/
		lp = (lcb_p) malloc (sizeof (struct lcb), M_DEVBUF, M_NOWAIT);
		if (!lp) return;

		/*
		**	Initialize it
		*/
		bzero (lp, sizeof (*lp));
		lp->jump_lcb.l_cmd   = htole32(SCR_JUMP ^ IFFALSE (DATA (lun)));
		lp->jump_lcb.l_paddr = tp->jump_lcb.l_paddr;

		lp->call_tag.l_cmd   = htole32(SCR_CALL);
		lp->call_tag.l_paddr = htole32(NCB_SCRIPT_PHYS (np, resel_tag));

		lp->jump_ccb.l_cmd   = htole32(SCR_JUMP);
		lp->jump_ccb.l_paddr = htole32(NCB_SCRIPTH_PHYS (np, aborttag));

		lp->actlink = 1;

		/*
		**   Chain into LUN list
		*/
		tp->jump_lcb.l_paddr = htole32(vtophys((vaddr_t)&lp->jump_lcb));
		tp->lp[lun] = lp;

	}

	/*
	**	Limit possible number of ccbs.
	**
	**	If tagged command queueing is enabled,
	**	can use more than one ccb.
	*/

	if (np->actccbs >= MAX_START-2) return;
	if (lp->actccbs && (lp->actccbs >= lp->reqccbs))
		return;

	/*
	**	Allocate a ccb
	*/
	cp = (ccb_p) malloc (sizeof (struct ccb), M_DEVBUF, M_NOWAIT);

	if (!cp)
		return;

	if (DEBUG_FLAGS & DEBUG_ALLOC) {
		printf ("new ccb @%p.\n", cp);
	}

	/*
	**	Count it
	*/
	lp->actccbs++;
	np->actccbs++;

	/*
	**	Initialize it
	*/
	bzero (cp, sizeof (*cp));

	/*
	**	Fill in physical addresses
	*/

	cp->p_ccb = vtophys((vaddr_t)cp);

#ifdef __NetBSD__
	if (ncr_ccb_dma_init(np, cp) != 0)
		return;
#endif

	/*
	**	Chain into reselect list
	*/
	cp->jump_ccb.l_cmd   = htole32(SCR_JUMP);
	cp->jump_ccb.l_paddr = lp->jump_ccb.l_paddr;
	lp->jump_ccb.l_paddr = htole32(CCB_PHYS (cp, jump_ccb));
	cp->call_tmp.l_cmd   = htole32(SCR_CALL);
	cp->call_tmp.l_paddr = htole32(NCB_SCRIPT_PHYS (np, resel_tmp));

	/*
	**	Chain into wakeup list
	*/
	cp->link_ccb      = np->ccb->link_ccb;
	np->ccb->link_ccb  = cp;

	/*
	**	Chain into CCB list
	*/
	cp->next_ccb	= lp->next_ccb;
	lp->next_ccb	= cp;
}

/*==========================================================
**
**
**	Announce the number of ccbs/tags to the scsi driver.
**
**
**==========================================================
*/

static void ncr_opennings (ncb_p np, lcb_p lp, struct scsipi_xfer * xp)
{
	/*
	**	want to reduce the number ...
	*/
	if (lp->actlink > lp->reqlink) {

		/*
		**	Try to  reduce the count.
		**	We assume to run at splbio ..
		*/
		u_char diff = lp->actlink - lp->reqlink;

		if (!diff) return;

#ifdef __NetBSD__
		if (diff > (xp->sc_link->openings - xp->sc_link->active))
			diff = (xp->sc_link->openings - xp->sc_link->active);

		xp->sc_link->openings	-= diff;
#else /* !__NetBSD__ */
		if (diff > xp->sc_link->opennings)
			diff = xp->sc_link->opennings;

		xp->sc_link->opennings	-= diff;
#endif /* __NetBSD__ */
		lp->actlink		-= diff;
		if (DEBUG_FLAGS & DEBUG_TAGS)
			printf ("%s: actlink: diff=%d, new=%d, req=%d\n",
				ncr_name(np), diff, lp->actlink, lp->reqlink);
		return;
	};

	/*
	**	want to increase the number ?
	*/
	if (lp->reqlink > lp->actlink) {
		u_char diff = lp->reqlink - lp->actlink;

#ifdef __NetBSD__
		xp->sc_link->openings	+= diff;
#else /* !__NetBSD__ */
		xp->sc_link->opennings	+= diff;
#endif /* __NetBSD__ */
		lp->actlink		+= diff;
		wakeup ((caddr_t) xp->sc_link);
		if (DEBUG_FLAGS & DEBUG_TAGS)
			printf ("%s: actlink: diff=%d, new=%d, req=%d\n",
				ncr_name(np), diff, lp->actlink, lp->reqlink);
	};
}

/*==========================================================
**
**
**	Build Scatter Gather Block
**
**
**==========================================================
**
**	The transfer area may be scattered among
**	several non adjacent physical pages.
**
**	We may use MAX_SCATTER blocks.
**
**----------------------------------------------------------
*/

static	int	ncr_scatter
	(ncb_p np, ccb_p cp, vaddr_t vaddr, vsize_t datalen)
{
	struct dsb *phys = &cp->phys;
#ifdef __NetBSD__
	int error, segment;

	bzero (&phys->data, sizeof (phys->data));
	if (!datalen) return (0);

	if ((error = bus_dmamap_load(np->sc_dmat, cp->xfer_dmamap,
	    (void *)vaddr, datalen, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load xfer DMA map, error = %d\n",
		    ncr_name(np), error);
		return (-1);
	}

	for (segment = 0; segment < cp->xfer_dmamap->dm_nsegs; segment++) {
		phys->data[segment].addr =
		    htole32(cp->xfer_dmamap->dm_segs[segment].ds_addr);
		phys->data[segment].size =
		    htole32(cp->xfer_dmamap->dm_segs[segment].ds_len);
	}
	return (segment);
#else
	u_long	paddr, pnext;
	u_short	segment  = 0;
	u_long	segsize, segaddr;
	u_long	size, csize    = 0;
	u_long	chunk = MAX_SIZE;
	int	free;

	bzero (&phys->data, sizeof (phys->data));
	if (!datalen) return (0);

	paddr = vtophys (vaddr);

	/*
	**	insert extra break points at a distance of chunk.
	**	We try to reduce the number of interrupts caused
	**	by unexpected phase changes due to disconnects.
	**	A typical harddisk may disconnect before ANY block.
	**	If we wanted to avoid unexpected phase changes at all
	**	we had to use a break point every 512 bytes.
	**	Of course the number of scatter/gather blocks is
	**	limited.
	*/

	free = MAX_SCATTER - 1;

	if (vaddr & PAGE_MASK) free -= datalen / PAGE_SIZE;

	if (free>1)
		while ((chunk * free >= 2 * datalen) && (chunk>=1024))
			chunk /= 2;

	if(DEBUG_FLAGS & DEBUG_SCATTER)
		printf("ncr?:\tscattering virtual=0x%x size=%d chunk=%d.\n",
			(unsigned) vaddr, (unsigned) datalen, (unsigned) chunk);

	/*
	**   Build data descriptors.
	*/
	while (datalen && (segment < MAX_SCATTER)) {

		/*
		**	this segment is empty
		*/
		segsize = 0;
		segaddr = paddr;
		pnext   = paddr;

		if (!csize) csize = chunk;

		while ((datalen) && (paddr == pnext) && (csize)) {

			/*
			**	continue this segment
			*/
			pnext = (paddr & (~PAGE_MASK)) + PAGE_SIZE;

			/*
			**	Compute max size
			*/

			size = pnext - paddr;		/* page size */
			if (size > datalen) size = datalen;  /* data size */
			if (size > csize  ) size = csize  ;  /* chunksize */

			segsize += size;
			vaddr   += size;
			csize   -= size;
			datalen -= size;
			paddr    = vtophys (vaddr);
		};

		if(DEBUG_FLAGS & DEBUG_SCATTER)
			printf ("\tseg #%d  addr=%x  size=%d  (rest=%d).\n",
			segment,
			(unsigned) segaddr,
			(unsigned) segsize,
			(unsigned) datalen);

		phys->data[segment].addr = SCR_BO(segaddr);
		phys->data[segment].size = SCR_BO(segsize);
		segment++;
	}

	if (datalen) {
		printf("ncr?: scatter/gather failed (residue=%d).\n",
			(unsigned) datalen);
		return (-1);
	};

	return (segment);
#endif /* __NetBSD__ */
}

/*==========================================================
**
**
**	Test the pci bus snoop logic :-(
**
**	Has to be called with interrupts disabled.
**
**
**==========================================================
*/

#if !defined(NCR_IOMAPPED) || defined(__NetBSD__)
static int ncr_regtest (struct ncb* np)
{
	register volatile u_int32_t data;
	/*
	**	ncr registers may NOT be cached.
	**	write 0xffffffff to a read only register area,
	**	and try to read it back.
	*/
	data = 0xffffffff;
	OUTL_OFF(offsetof(struct ncr_reg, nc_dstat), data);
	data = INL_OFF(offsetof(struct ncr_reg, nc_dstat));
#if 1
	if (data == 0xffffffff)
#else
	if ((data & 0xe2f0fffd) != 0x02000080)
#endif
	/* if */ {
		printf ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return (0x10);
	};
	return (0);
}
#endif

static int ncr_snooptest (struct ncb* np)
{
	u_int32_t ncr_rd, ncr_wr, ncr_bk, host_rd, host_wr, pc;
	int	i, err=0;
#if !defined(NCR_IOMAPPED) || defined(__NetBSD__)
#ifdef __NetBSD__
	if (!np->sc_iomapped)
#endif
	{
		err |= ncr_regtest (np);
		if (err) return (err);
	}
#endif
	/*
	**	init
	*/
	pc  = NCB_SCRIPTH_PHYS (np, snooptest);
	host_wr = 1;
	ncr_wr  = 2;
	/*
	**	Set memory and register.
	*/
	ncr_cache = htole32(host_wr);
	OUTL (nc_temp, ncr_wr);
	/*
	**	Start script (exchange values)
	*/
	OUTL (nc_dsp, pc);
	/*
	**	Wait 'til done (with timeout)
	*/
	for (i=0; i<NCR_SNOOP_TIMEOUT; i++)
		if (INB(nc_istat) & (INTF|SIP|DIP))
			break;
	/*
	**	Save termination position.
	*/
	pc = INL (nc_dsp);
	/*
	**	Read memory and register.
	*/
	host_rd = le32toh(ncr_cache);
	ncr_rd  = INL (nc_scratcha);
	ncr_bk  = INL (nc_temp);
	/*
	**	Reset ncr chip
	*/
	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat,  0   );
	/*
	**	check for timeout
	*/
	if (i>=NCR_SNOOP_TIMEOUT) {
		printf ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	};
	/*
	**	Check termination position.
	*/
	if (pc != NCB_SCRIPTH_PHYS (np, snoopend)+8) {
		printf ("CACHE TEST FAILED: script execution failed.\n");
		printf ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) NCB_SCRIPTH_PHYS (np, snooptest), (u_long) pc,
			(u_long) NCB_SCRIPTH_PHYS (np, snoopend) +8);
		return (0x40);
	};
	/*
	**	Show results.
	*/
	if (host_wr != ncr_rd) {
		printf ("CACHE TEST FAILED: host wrote %d, ncr read %d.\n",
			(int) host_wr, (int) ncr_rd);
		err |= 1;
	};
	if (host_rd != ncr_wr) {
		printf ("CACHE TEST FAILED: ncr wrote %d, host read %d.\n",
			(int) ncr_wr, (int) host_rd);
		err |= 2;
	};
	if (ncr_bk != ncr_wr) {
		printf ("CACHE TEST FAILED: ncr wrote %d, read back %d.\n",
			(int) ncr_wr, (int) ncr_bk);
		err |= 4;
	};
	return (err);
}

/*==========================================================
**
**
**	Profiling the drivers and targets performance.
**
**
**==========================================================
*/

/*
**	Compute the difference in milliseconds.
**/

static	int ncr_delta (struct timeval * from, struct timeval * to)
{
	if (!from->tv_sec) return (-1);
	if (!to  ->tv_sec) return (-2);
	return ( (to->tv_sec  - from->tv_sec  -       2)*1000+
		+(to->tv_usec - from->tv_usec + 2000000)/1000);
}

#define PROFILE  cp->phys.header.stamp
static	void ncb_profile (ncb_p np, ccb_p cp)
{
	int co, da, st, en, di, se, post,work,disc;
	u_long diff;

#ifdef __NetBSD__
	PROFILE.end = mono_time;
#else
	gettime(&PROFILE.end);
#endif

	st = ncr_delta (&PROFILE.start,&PROFILE.status);
	if (st<0) return;	/* status  not reached  */

	da = ncr_delta (&PROFILE.start,&PROFILE.data);
	if (da<0) return;	/* No data transfer phase */

	co = ncr_delta (&PROFILE.start,&PROFILE.command);
	if (co<0) return;	/* command not executed */

	en = ncr_delta (&PROFILE.start,&PROFILE.end),
	di = ncr_delta (&PROFILE.start,&PROFILE.disconnect),
	se = ncr_delta (&PROFILE.start,&PROFILE.select);
	post = en - st;

	/*
	**	@PROFILE@  Disconnect time invalid if multiple disconnects
	*/

	if (di>=0) disc = se-di; else  disc = 0;

	work = (st - co) - disc;

	diff = (np->ncb_dma->disc_phys - np->disc_ref) & 0xff;
	np->disc_ref += diff;

	np->profile.num_trans	+= 1;
	if (cp->xfer)
	np->profile.num_bytes	+= cp->xfer->datalen;
	np->profile.num_disc	+= diff;
	np->profile.ms_setup	+= co;
	np->profile.ms_data	+= work;
	np->profile.ms_disc	+= disc;
	np->profile.ms_post	+= post;
}
#undef PROFILE

#ifdef __NetBSD__
/*==========================================================
**
**
**	Device lookup.
**
**	@GENSCSI@ should be integrated to scsiconf.c
**
**
**==========================================================
*/

struct table_entry {
	char *	manufacturer;
	char *	model;
	char *	version;
	u_long	info;
};

static struct table_entry device_tab[] =
{
	/* XXX maybe doesn't need QUIRK_NOMSG? */
	{"HP      ",	"C372",		"",	QUIRK_NOTAGS|QUIRK_NOMSG},

	/* XXX maybe doesn't need QUIRK_NOMSG? */
	{"QUANTUM",	"ATLAS IV",	"",	QUIRK_NOTAGS|QUIRK_NOMSG},

	/*
	 * XXX not clear what the value of NCR_GETCC_WITHMSG is if
	 * XXX QUIRK_NOMSG is always turned on, but I am just an
	 * XXX egg.  --cgd
	 */
	/* catch all: must be the last entry. */
	{"",		"", 		"",	QUIRK_NOMSG},
};

static u_long ncr_lookup(char * id)
{
	struct table_entry * p = device_tab;
	char *d, *r, c;

	for (;;p++) {

		d = id+8;
		r = p->manufacturer;
		while ((c=*r++)) if (c!=*d++) break;
		if (c) continue;

		d = id+16;
		r = p->model;
		while ((c=*r++)) if (c!=*d++) break;
		if (c) continue;

		d = id+32;
		r = p->version;
		while ((c=*r++)) if (c!=*d++) break;
		if (c) continue;

		return (p->info);
	}
}
#endif /* __NetBSD__ */

/*==========================================================
**
**	Determine the ncr's clock frequency.
**	This is essential for the negotiation
**	of the synchronous transfer rate.
**
**==========================================================
**
**	Note: we have to return the correct value.
**	THERE IS NO SAVE DEFAULT VALUE.
**
**	Most NCR/SYMBIOS boards are delivered with a 40 Mhz clock.
**	53C860 and 53C875 rev. 1 support fast20 transfers but 
**	do not have a clock doubler and so are provided with a 
**	80 MHz clock. All other fast20 boards incorporate a doubler 
**	and so should be delivered with a 40 MHz clock.
**	The future fast40 chips (895/895) use a 40 Mhz base clock 
**	and provide a clock quadrupler (160 Mhz). The code below 
**	tries to deal as cleverly as possible with all this stuff.
**
**----------------------------------------------------------
*/

/*
 *	Select NCR SCSI clock frequency
 */
static void ncr_selectclock(ncb_p np, u_char scntl3)
{
	if (np->multiplier < 2) {
		OUTB(nc_scntl3,	scntl3);
		return;
	}

	if (bootverbose >= 2)
		printf ("%s: enabling clock multiplier\n", ncr_name(np));

	OUTB(nc_stest1, DBLEN);	   /* Enable clock multiplier		  */
	if (np->multiplier > 2) {  /* Poll bit 5 of stest4 for quadrupler */
		int i = 20;
		while (!(INB(nc_stest4) & LCKFRQ) && --i > 0)
			DELAY(20);
		if (!i)
			printf("%s: the chip cannot lock the frequency\n", ncr_name(np));
	} else			/* Wait 20 micro-seconds for doubler	*/
		DELAY(20);
	OUTB(nc_stest3, HSC);		/* Halt the scsi clock		*/
	OUTB(nc_scntl3,	scntl3);
	OUTB(nc_stest1, (DBLEN|DBLSEL));/* Select clock multiplier	*/
	OUTB(nc_stest3, 0x00);		/* Restart scsi clock 		*/
}

/*
 *	calculate NCR SCSI clock frequency (in KHz)
 */
static unsigned
ncrgetfreq (ncb_p np, int gen)
{
	int ms = 0;
	/*
	 * Measure GEN timer delay in order 
	 * to calculate SCSI clock frequency
	 *
	 * This code will never execute too
	 * many loop iterations (if DELAY is 
	 * reasonably correct). It could get
	 * too low a delay (too high a freq.)
	 * if the CPU is slow executing the 
	 * loop for some reason (an NMI, for
	 * example). For this reason we will
	 * if multiple measurements are to be 
	 * performed trust the higher delay 
	 * (lower frequency returned).
	 */
	OUTB (nc_stest1, 0);	/* make sure clock doubler is OFF	    */
	OUTW (nc_sien , 0);	/* mask all scsi interrupts		    */
	(void) INW (nc_sist);	/* clear pending scsi interrupt		    */
	OUTB (nc_dien , 0);	/* mask all dma interrupts		    */
	(void) INW (nc_sist);	/* another one, just to be sure :)	    */
	OUTB (nc_scntl3, 4);	/* set pre-scaler to divide by 3	    */
	OUTB (nc_stime1, 0);	/* disable general purpose timer	    */
	OUTB (nc_stime1, gen);	/* set to nominal delay of (1<<gen) * 125us */
	while (!(INW(nc_sist) & GEN) && ms++ < 1000)
		DELAY(1000);	/* count ms				    */
	OUTB (nc_stime1, 0);	/* disable general purpose timer	    */
	OUTB (nc_scntl3, 0);
	/*
	 * Set prescaler to divide by whatever "0" means.
	 * "0" ought to choose divide by 2, but appears
	 * to set divide by 3.5 mode in my 53c810 ...
	 */
	OUTB (nc_scntl3, 0);

	if (bootverbose >= 2)
	  	printf ("\tDelay (GEN=%d): %u msec\n", gen, ms);
	/*
	 * adjust for prescaler, and convert into KHz 
	 */
	return ms ? ((1 << gen) * 4440) / ms : 0;
}

static void ncr_getclock (ncb_p np, u_char multiplier)
{
	unsigned char scntl3;
	unsigned char stest1;
	scntl3 = INB(nc_scntl3);
	stest1 = INB(nc_stest1);
	  
	np->multiplier = 1;
	/* always false, except for 875 with clock doubler selected */
	if ((stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		np->multiplier	= multiplier;
		np->clock_khz	= 40000 * multiplier;
	} else {
		if ((scntl3 & 7) == 0) {
			unsigned f1, f2;
			/* throw away first result */
			(void) ncrgetfreq (np, 11);
			f1 = ncrgetfreq (np, 11);
			f2 = ncrgetfreq (np, 11);

			if (bootverbose >= 2)
			  printf ("\tNCR clock is %uKHz, %uKHz\n", f1, f2);
			if (f1 > f2) f1 = f2;	/* trust lower result	*/
			if (f1 > 45000) {
				scntl3 = 5;	/* >45Mhz: assume 80MHz	*/
			} else {
				scntl3 = 3;	/* <45Mhz: assume 40MHz	*/
			}
		}
		else if ((scntl3 & 7) == 5)
			np->clock_khz = 80000;	/* Probably a 875 rev. 1 ? */
	}
}

/*=========================================================================*/

#ifdef NCR_TEKRAM_EEPROM

static void
tekram_write_bit (ncb_p np, int bit)
{
	u_char val = 0x10 + ((bit & 1) << 1);

	DELAY(10);
	OUTB (nc_gpreg, val);
	DELAY(10);
	OUTB (nc_gpreg, val | 0x04);
	DELAY(10);
	OUTB (nc_gpreg, val);
	DELAY(10);
}

static int
tekram_read_bit (ncb_p np)
{
	OUTB (nc_gpreg, 0x10);
	DELAY(10);
	OUTB (nc_gpreg, 0x14);
	DELAY(10);
	return INB (nc_gpreg) & 1;
}

static u_short
read_tekram_eeprom_reg (ncb_p np, int reg)
{
	int bit;
	u_short result = 0;
	int cmd = 0x80 | reg;

	OUTB (nc_gpreg, 0x10);

	tekram_write_bit (np, 1);
	for (bit = 7; bit >= 0; bit--)
	{
		tekram_write_bit (np, cmd >> bit);
	}

	for (bit = 0; bit < 16; bit++)
	{
		result <<= 1;
		result |= tekram_read_bit (np);
	}

	OUTB (nc_gpreg, 0x00);
	return result;
}

static int 
read_tekram_eeprom(ncb_p np, struct tekram_eeprom *buffer)
{
	u_short *p = (u_short *) buffer;
	u_short sum = 0;
	int i;

	if (INB (nc_gpcntl) != 0x09)
	{
		return 0;
        }
	for (i = 0; i < 64; i++)
	{
		u_short val;
if((i&0x0f) == 0) printf ("%02x:", i*2);
		val = read_tekram_eeprom_reg (np, i);
		if (p)
			*p++ = val;
		sum += val;
if((i&0x01) == 0x00) printf (" ");
		printf ("%02x%02x", val & 0xff, (val >> 8) & 0xff);
if((i&0x0f) == 0x0f) printf ("\n");
	}
printf ("Sum = %04x\n", sum);
	return sum == 0x1234;
}
#endif /* NCR_TEKRAM_EEPROM */

/*=========================================================================*/
#endif /* KERNEL */
