/*
 * (Mostly) Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 *	$Id: aha1542.c,v 1.12.2.2 1993/08/13 12:43:54 cgd Exp $
 */

/*
 * HISTORY
 * $Log: aha1542.c,v $
 * Revision 1.12.2.2  1993/08/13 12:43:54  cgd
 * from chmr@edvz.tu-graz.ac.at (Christoph Robitschko):
 * it seems that the bustek needs a little more time to settle down after a
 * reset command than the adaptecs. Up to PK-0.2.3 there was a printf there
 * that did the job quite well.
 *
 * Revision 1.12.2.1  1993/07/29  21:26:50  deraadt
 * reliability fudge for dealing with drives that do scsi resets very slowly
 * (or so it seems)
 *
 * Revision 1.12  1993/07/06  06:06:26  deraadt
 * clean up code for timeout/untimeout/wakeup prototypes.
 *
 * Revision 1.11  1993/06/14  04:16:03  andrew
 * Reduced bus-on time from the default of 11ms -> 9ms, to prevent floppy from
 * becoming data-starved during simultaneous fd & scsi activity.
 *
 * Revision 1.10  1993/06/09  22:36:40  deraadt
 * minor silliness related to two or more controllers
 *
 * Revision 1.9  1993/05/22  08:00:56  cgd
 * add rcsids to everything and clean up headers
 *
 * Revision 1.8  1993/05/04  08:32:40  deraadt
 * support for making dev->id_alive be set, this is for iostat to
 * find disk devices. wee bit of a kludge. sub-device attach()
 * routines must now return 1 for successful attach(), 0 otherwise.
 * Other bsd's do this too..
 *
 * Revision 1.7  1993/04/19  06:02:16  mycroft
 * Fix subtle word-size error.
 *
 * Revision 1.6  1993/04/15  07:57:50  deraadt
 * ioconf changes, see previous cvs's that dumped core
 *
 * Revision 1.4  1993/04/12  08:17:23  deraadt
 * new scsi subsystem.
 * changes also in config/mkioconf.c & sys/scsi/*
 *
 * Revision 1.1  1993/03/21  18:09:54  cgd
 * after 0.2.2 "stable" patches applied
 *
 * Revision 1.6  1992/08/24  21:01:58  jason
 * many changes and bugfixes for osf1
 *
 * Revision 1.5  1992/07/31  01:22:03  julian
 * support improved scsi.h layout
 *
 * Revision 1.4  1992/07/25  03:11:26  julian
 * check each request fro sane flags.
 *
 * Revision 1.3  1992/07/24  00:52:45  julian
 * improved timeout handling.
 * added support for two arguments to the sd_done (or equiv) call so that
 * they can pre-queue several arguments.
 * slightly clean up error handling
 *
 * Revision 1.2  1992/07/17  22:03:54  julian
 * upgraded the timeout code.
 * added support for UIO-based i/o (as used for pmem operations)
 *
 * Revision 1.1  1992/05/27  00:51:12  balsup
 * machkern/cor merge
 */

/*
 * a FEW lines in this driver come from a MACH adaptec-disk driver
 * so the copyright below is included:
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 * 
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "aha.h"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/ioctl.h"
#include "sys/buf.h"
#include "machine/stdarg.h"
#include "sys/proc.h"
#include "sys/user.h"
#include "sys/dkbad.h"
#include "sys/disklabel.h"
#include "i386/isa/isa_device.h"
#include "scsi/scsi_all.h"
#include "scsi/scsiconf.h"


#ifdef	DDB
int	Debugger();
#else	DDB
#define Debugger() panic("should call debugger here (adaptec.c)")
#endif	DDB

extern int delaycount;  /* from clock setup code */

/* I/O Port Interface */
#define	AHA_BASE		aha_base[unit]
#define	AHA_CTRL_STAT_PORT	(AHA_BASE + 0x0)	/* control & status */
#define	AHA_CMD_DATA_PORT	(AHA_BASE + 0x1)	/* cmds and datas */
#define	AHA_INTR_PORT		(AHA_BASE + 0x2)	/* Intr. stat */

/* AHA_CTRL_STAT bits (write) */
#define AHA_HRST		0x80	/* Hardware reset */
#define AHA_SRST		0x40	/* Software reset */
#define AHA_IRST		0x20	/* Interrupt reset */
#define AHA_SCRST		0x10	/* SCSI bus reset */

/* AHA_CTRL_STAT bits (read) */
#define AHA_STST		0x80	/* Self test in Progress */
#define AHA_DIAGF		0x40	/* Diagnostic Failure */
#define AHA_INIT		0x20	/* Mbx Init required */
#define AHA_IDLE		0x10	/* Host Adapter Idle */
#define AHA_CDF			0x08	/* cmd/data out port full */
#define AHA_DF			0x04	/* Data in port full */
#define AHA_INVDCMD		0x01	/* Invalid command */

/* AHA_CMD_DATA bits (write) */
#define	AHA_NOP			0x00	/* No operation */
#define AHA_MBX_INIT		0x01	/* Mbx initialization */
#define AHA_START_SCSI		0x02	/* start scsi command */
#define AHA_START_BIOS		0x03	/* start bios command */
#define AHA_INQUIRE		0x04	/* Adapter Inquiry */
#define AHA_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define AHA_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define AHA_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define AHA_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define AHA_SPEED_SET		0x09	/* set transfer speed */
#define AHA_DEV_GET		0x0a	/* return installed devices */
#define AHA_CONF_GET		0x0b	/* return configuration data */
#define AHA_TARGET_EN		0x0c	/* enable target mode */
#define AHA_SETUP_GET		0x0d	/* return setup data */
#define AHA_WRITE_CH2		0x1a	/* write channel 2 buffer */
#define AHA_READ_CH2		0x1b	/* read channel 2 buffer */
#define AHA_WRITE_FIFO		0x1c	/* write fifo buffer */
#define AHA_READ_FIFO		0x1d	/* read fifo buffer */
#define AHA_ECHO		0x1e	/* Echo command data */

struct aha_cmd_buf {
	 u_char byte[16];
};

/* AHA_INTR_PORT bits (read) */
#define AHA_ANY_INTR		0x80	/* Any interrupt */
#define AHA_SCRD		0x08	/* SCSI reset detected */
#define AHA_HACC		0x04	/* Command complete */
#define AHA_MBOA		0x02	/* MBX out empty */
#define AHA_MBIF		0x01	/* MBX in full */

/* Mail box defs */
#define AHA_MBX_SIZE		16	/* mail box size */

struct aha_mbx {
	struct aha_mbx_out {
		unsigned char cmd;
		unsigned char ccb_addr[3];
	} mbo[AHA_MBX_SIZE];
	struct aha_mbx_in{
		unsigned char stat;
		unsigned char ccb_addr[3];
	} mbi[AHA_MBX_SIZE];
};

/* mbo.cmd values */
#define AHA_MBO_FREE	0x0	/* MBO entry is free */
#define AHA_MBO_START	0x1	/* MBO activate entry */
#define AHA_MBO_ABORT	0x2	/* MBO abort entry */

#define AHA_MBI_FREE	0x0	/* MBI entry is free */
#define AHA_MBI_OK	0x1	/* completed without error */
#define AHA_MBI_ABORT	0x2	/* aborted ccb */
#define AHA_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define AHA_MBI_ERROR	0x4	/* Completed with error */

extern struct aha_mbx aha_mbx[];

/* FOR OLD VERSIONS OF THE !%$@ this may have to be 16 (yuk) */
/* Number of scatter gather segments <= 16, allow 64 K i/o (min) */
#define	AHA_NSEG	17

struct aha_ccb {
	unsigned char	opcode;
	unsigned char	lun:3;
	unsigned char	data_in:1;		/* must be 0 */
	unsigned char	data_out:1;		/* must be 0 */
	unsigned char	target:3;
	unsigned char	scsi_cmd_length;
	unsigned char	req_sense_length;
	unsigned char	data_length[3];
	unsigned char	data_addr[3];
	unsigned char	link_addr[3];
	unsigned char	link_id;
	unsigned char	host_stat;
	unsigned char	target_stat;
	unsigned char	reserved[2];
	struct	scsi_generic	scsi_cmd;
	struct	scsi_sense_data	scsi_sense;
	struct	aha_scat_gath {
		unsigned char seg_len[3];
		unsigned char seg_addr[3];
	} scat_gath[AHA_NSEG];
	struct	aha_ccb	 *next;
	struct	scsi_xfer	 *xfer;		/* the scsi_xfer for this cmd */
	struct	aha_mbx_out	 *mbx;		/* pointer to mail box */
	long int	delta;	/* difference from previous*/
	struct	aha_ccb	 *later,*sooner;
	int	flags;
};

/* flags value? */
#define CCB_FREE        0
#define CCB_ACTIVE      1
#define CCB_ABORTED     2

struct aha_ccb *aha_soonest = (struct  aha_ccb *)0;
struct aha_ccb *aha_latest = (struct  aha_ccb *)0;
long int aha_furtherest = 0;	/* longest time in the timeout queue */

/* opcode fields */
#define AHA_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define AHA_TARGET_CCB		0x01	/* SCSI Target CCB */
#define AHA_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scattter gather*/
#define AHA_RESET_CCB		0x81	/* SCSI Bus reset */


/* aha_ccb.host_stat values */
#define AHA_OK		0x00	/* cmd ok */
#define AHA_LINK_OK	0x0a	/* Link cmd ok */
#define AHA_LINK_IT	0x0b	/* Link cmd ok + int */
#define AHA_SEL_TIMEOUT	0x11	/* Selection time out */
#define AHA_OVER_UNDER	0x12	/* Data over/under run */
#define AHA_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define AHA_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define AHA_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define AHA_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define AHA_BAD_LINK	0x17	/* Not same values of LUN for links */
#define AHA_INV_TARGET	0x18	/* Invalid target direction */
#define AHA_CCB_DUP	0x19	/* Duplicate CCB received */
#define AHA_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define AHA_ABORTED      42

struct aha_setup {
	u_char	sync_neg:1;
	u_char	parity:1;
	u_char	:6;
	u_char	speed;
	u_char	bus_on;
	u_char	bus_off;
	u_char	num_mbx;
	u_char	mbx[3];
	struct {
		u_char	offset:4;
		u_char	period:3;
		u_char	valid:1;
	} sync[8];
	u_char	disc_sts;
};

struct	aha_config {
	u_char	chan;
	u_char	intr;
	u_char	scsi_dev:3;
	u_char	:5;
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80



#define PHYSTOKV(x)	(x | 0xFE000000)
#define KVTOPHYS(x)	vtophys(x)
#define	AHA_DMA_PAGES	AHA_NSEG

#define PAGESIZ 	4096
#define INVALIDATE_CACHE {asm volatile( ".byte	0x0F ;.byte 0x08" ); }

struct scsi_xfer aha_scsi_xfer[NAHA];
struct isa_device *ahainfo[NAHA];
struct aha_mbx aha_mbx[NAHA];
struct aha_ccb *aha_ccb_free[NAHA];
struct aha_ccb aha_ccb[NAHA][AHA_MBX_SIZE];
struct aha_ccb *aha_get_ccb();
u_char aha_scratch_buf[256];
short aha_base[NAHA];		/* base port for each board */
int speed[NAHA];
int aha_int[NAHA];
int aha_dma[NAHA];
int aha_scsi_dev[NAHA];
int aha_initialized[NAHA];

int aha_debug = 0;
static int ahaunit = 0;

#define aha_abortmbx(mbx) \
	(mbx)->cmd = AHA_MBO_ABORT; \
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);
#define aha_startmbx(mbx) \
	(mbx)->cmd = AHA_MBO_START; \
	outb(AHA_CMD_DATA_PORT, AHA_START_SCSI);

#define AHA_CMD_TIMEOUT_FUDGE	200	/* multiplied to get Secs	 */
#define AHA_RESET_TIMEOUT	1000000 /* time to wait for reset	 */
#define AHA_SCSI_TIMEOUT_FUDGE	20	/* divided by for mSecs		 */


int aha_cmd(int, int, int, int, u_char *, ...);
int ahaprobe(struct isa_device *);
int ahaattach(struct isa_device *);
long int aha_adapter_info(int);
int ahaintr(int);
void aha_free_ccb(int, struct aha_ccb *, int);
struct aha_ccb * aha_get_ccb(int, int);
int aha_done(int, struct aha_ccb *);
int aha_init(int);
void ahaminphys(struct buf *);
int aha_scsi_cmd(struct scsi_xfer *);
int aha_set_bus_speed(int);
int aha_bus_speed_check(int, int);
void aha_add_timeout(struct aha_ccb *, int);
void aha_remove_timeout(struct aha_ccb *);
void aha_timeout(int);


struct isa_driver ahadriver = {
	ahaprobe,
	ahaattach,
	"aha"
};

struct scsi_switch aha_switch = {
	"aha",
	aha_scsi_cmd,
	ahaminphys,
	0,
	0,
	aha_adapter_info,
	0, 0, 0
};

/*
 * aha_cmd(unit, icnt, ocnt,wait, retval, ...)
 * Activate Adapter command
 *	icnt:	number of args (outbound bytes written after opcode)
 *	ocnt:	number of expected returned bytes
 *	wait:	number of seconds to wait for response
 *	retval:	buffer where to place returned bytes
 *	...:	opcode AHA_NOP, AHA_MBX_INIT, AHA_START_SCSI & parameters
 *
 * Performs an adapter command through the ports. Not to be confused
 *	with a scsi command, which is read in via the dma
 * One of the adapter commands tells it to read in a scsi command
 */
int
aha_cmd(int unit, int icnt, int ocnt, int wait, u_char *retval, ...)
{
	va_list ap;
	int opc, sts;
	u_char oc;
	register i;

	va_start(ap, retval);
	opc = (u_char)va_arg(ap, int);
	/*printf("command: %08x %02x\n", opc, (u_char)opc);*/

	/*
	 * multiply the wait argument by a big constant
	 * zero defaults to 1
	 */
	if(!wait)
		wait = AHA_CMD_TIMEOUT_FUDGE * delaycount;
	else
		wait *= AHA_CMD_TIMEOUT_FUDGE * delaycount;
	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opc != AHA_MBX_INIT && opc != AHA_START_SCSI) {
		i = AHA_CMD_TIMEOUT_FUDGE * delaycount;	/* 1 sec?*/
		while (--i) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts & AHA_IDLE)
				break;
		}
		if (!i) {
			printf("aha_cmd: aha1542 host not idle(0x%x)\n", sts);
			return(ENXIO);
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the*
	 * queue feeding to us.
	 */
	if (ocnt) {
		while((inb(AHA_CTRL_STAT_PORT)) & AHA_DF)
			inb(AHA_CMD_DATA_PORT);
	}

	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	icnt++;		/* include the command */
	while (icnt--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i=0; i< wait; i++) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (!(sts & AHA_CDF))
				break;
		}
		if (i >=  wait) {
			printf("aha_cmd: aha1542 cmd/data port full\n");
			outb(AHA_CTRL_STAT_PORT, AHA_SRST); 
			return(ENXIO);
		}
		outb(AHA_CMD_DATA_PORT, (u_char)opc);
		if(icnt) {
			opc = (u_char)va_arg(ap, int);
			/*printf("extra: %08x %02x\n", opc, (u_char)opc);*/
		}
	}
	va_end(ap);

	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		sts = inb(AHA_CTRL_STAT_PORT);
		for (i=0; i< wait; i++) {
			sts = inb(AHA_CTRL_STAT_PORT);
			if (sts  & AHA_DF)
				break;
		}
		if (i >=  wait) {
			printf("aha_cmd: aha1542 cmd/data port empty %d\n",ocnt);
			return(ENXIO);
		}
		oc = inb(AHA_CMD_DATA_PORT);
		if (retval)
			 *retval++ = oc;
	}
	/*
	 * Wait for the board to report a finised instruction
	 */
	i=AHA_CMD_TIMEOUT_FUDGE * delaycount;	/* 1 sec? */
	while (--i) {
		sts = inb(AHA_INTR_PORT);
		if (sts & AHA_HACC)
			break;
	}
	if (!i) {
		printf("aha_cmd: aha1542 host not finished(0x%x)\n",sts);
		return(ENXIO);
	}
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	return(0);
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
int
ahaprobe(struct isa_device *dev)
{
 	int	unit = ahaunit;

	dev->id_unit = unit;
	aha_base[unit] = dev->id_iobase;
	if(unit >= NAHA) {
		printf("aha: unit number (%d) too high\n",unit);
		return(0);
	}

	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads aha_int[unit]
	 */
	if (aha_init(unit) != 0)
		return(0);

	/*
	 * If it's there, put in it's interrupt vectors
	 */
	dev->id_irq = (1 << aha_int[unit]);
	dev->id_drq = aha_dma[unit];
	ahaunit++;
	return(8);
}

/*
 * Attach all the sub-devices we can find
 */
int
ahaattach(struct isa_device *dev)
{
	static int firsttime;
	static u_long speedprint;	/* max 32 aha controllers */
	int masunit = dev->id_masunit;
	int r;

	if(!(speedprint & (1<<masunit))) {
		DELAY(1000000);
		speedprint |= (1<<masunit);
		printf("aha%d: bus speed %dns\n", masunit, speed[masunit]);
	}

	r = scsi_attach(masunit, aha_scsi_dev[masunit], &aha_switch,
		&dev->id_physid, &dev->id_unit, dev->id_flags);

	/* only one for all boards */
	if(firsttime==0) {
		firsttime = 1;
		aha_timeout(0);
	}
	return r;
}


/*
 * Return some information to the caller about
 * the adapter and it's capabilities
 */
long int
aha_adapter_info(int unit)
{
	return(2);	/* 2 outstanding requests at a time per device */
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahaintr(int unit)
{
	struct aha_ccb *ccb;
	unsigned char stat;
	register i;

	if(scsi_debug & PRINTROUTINES)
		printf("ahaintr ");
	/*
	 * First acknowlege the interrupt, Then if it's
	 * not telling about a completed operation
	 * just return. 
	 */
	stat = inb(AHA_INTR_PORT);
	outb(AHA_CTRL_STAT_PORT, AHA_IRST);
	if(scsi_debug & TRACEINTERRUPTS)
		printf("int ");
	if (! (stat & AHA_MBIF))
		return(1);
	if(scsi_debug & TRACEINTERRUPTS)
		printf("b ");

	/*
	 * If it IS then process the competed operation
	 */
	for (i = 0; i < AHA_MBX_SIZE; i++) {
		if (aha_mbx[unit].mbi[i].stat != AHA_MBI_FREE) {
			ccb = (struct aha_ccb *)PHYSTOKV(
				(_3btol(aha_mbx[unit].mbi[i].ccb_addr)));
			if((stat =  aha_mbx[unit].mbi[i].stat) != AHA_MBI_OK) {
				switch(stat) {
				case AHA_MBI_ABORT:
					if(aha_debug)
					    printf("abort");
					ccb->host_stat = AHA_ABORTED;
					break;
				case AHA_MBI_UNKNOWN:
					ccb = (struct aha_ccb *)0;
					if(aha_debug)
					     printf("unknown ccb for abort ");
					/* may have missed it */
					/* no such ccb known for abort */
					break;
				case AHA_MBI_ERROR:
					break;
				default:
					panic("Impossible mbxi status");
				}
				if( aha_debug && ccb ) {
					u_char	 *cp;
					cp = (u_char *)(&(ccb->scsi_cmd));
					printf("op=%x %x %x %x %x %x\n", 
						cp[0], cp[1], cp[2],
						cp[3], cp[4], cp[5]);
					printf("stat %x for mbi[%d]\n"
						, aha_mbx[unit].mbi[i].stat, i);
					printf("addr = 0x%x\n", ccb);
				}
			}
			if(ccb) {
				aha_remove_timeout(ccb);
				aha_done(unit,ccb);
			}
			aha_mbx[unit].mbi[i].stat = AHA_MBI_FREE;
		}
	}
	return(1);
}

/*
 * A ccb (and hence a mbx-out is put onto the 
 * free list.
 */
void
aha_free_ccb(int unit, struct aha_ccb *ccb, int flags)
{
	unsigned int opri;

	if(scsi_debug & PRINTROUTINES)
		printf("ccb%d(0x%x)> ",unit,flags);
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();

	ccb->next = aha_ccb_free[unit];
	aha_ccb_free[unit] = ccb;
	ccb->flags = CCB_FREE;
	if(ccb->sooner || ccb->later) {
		printf("yikes, still in timeout queue\n");
		aha_remove_timeout(ccb);
	}
	/*
	 * If there were none, wake abybody waiting for
	 * one to come free, starting with queued entries*
	 */
	if (!ccb->next)
		wakeup( (caddr_t)&aha_ccb_free[unit]);
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
}

/*
 * Get a free ccb (and hence mbox-out entry)
 */
struct aha_ccb *
aha_get_ccb(int unit, int flags)
{
	unsigned opri;
	struct aha_ccb *rc;

	if(scsi_debug & PRINTROUTINES)
		printf("<ccb%d(0x%x) ",unit,flags);
	if (!(flags & SCSI_NOMASK)) 
	  	opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one
	 * to come free
	 */
	while ((!(rc = aha_ccb_free[unit])) && (!(flags & SCSI_NOSLEEP)))
		sleep((caddr_t)&aha_ccb_free[unit], PRIBIO);
	if (rc) {
		aha_ccb_free[unit] = aha_ccb_free[unit]->next;
		rc->flags = CCB_ACTIVE;
	}
	if (!(flags & SCSI_NOMASK)) 
		splx(opri);
	return(rc);
}


/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
int
aha_done(int unit, struct aha_ccb *ccb)
{
	struct	scsi_sense_data *s1,*s2;
	struct	scsi_xfer *xs = ccb->xfer;

	if(scsi_debug & PRINTROUTINES )
		printf("aha_done ");
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if(!(xs->flags & INUSE)) {
		printf("exiting but not in use! ");
		Debugger();
	}
	if ((ccb->host_stat != AHA_OK || ccb->target_stat != SCSI_OK)
	    && (!(xs->flags & SCSI_ERR_OK))) {
		s1 = (struct scsi_sense_data *)(((char *)(&ccb->scsi_cmd))
			+ ccb->scsi_cmd_length);
		s2 = &(xs->sense);

		if(ccb->host_stat) {
			switch(ccb->host_stat) {
			case AHA_ABORTED:
			case AHA_SEL_TIMEOUT:	/* No response */
				xs->error = XS_TIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
				if (aha_debug > 1)
					printf("host_stat%x\n", ccb->host_stat);
			}

		} else {
			switch(ccb->target_stat) {
			case 0x02:
				/* structure copy!!!!!*/
				 *s2=*s1;
				xs->error = XS_SENSE;
				break;
			case 0x08:
				xs->error = XS_BUSY;
				break;
			default:
				if (aha_debug > 1)
					printf("target_stat%x\n", ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	} else
		xs->resid = 0;

	xs->flags |= ITSDONE;
	aha_free_ccb(unit,ccb, xs->flags);
	if(xs->when_done)
		(*(xs->when_done))(xs->done_arg,xs->done_arg2);
}


/*
 * Start the board, ready for normal operation
 */
int
aha_init(int unit)
{
	struct aha_config conf;
	unsigned char ad[3];
	volatile int i,sts;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	outb(AHA_CTRL_STAT_PORT, AHA_HRST|AHA_SRST);

	for (i=0; i < AHA_RESET_TIMEOUT; i++) {
		sts = inb(AHA_CTRL_STAT_PORT) ;
		if(sts == (AHA_IDLE | AHA_INIT))
			break;
	}

	if (i >= AHA_RESET_TIMEOUT) {
		if (aha_debug)
			printf("aha_init: No answer from adaptec board\n");
		return(ENXIO);
	}

	/*
	 * Assume we have a board at this stage
	 * setup dma channel from jumpers and save int level
	 */
	DELAY(1000);
	aha_cmd(unit, 0, sizeof(conf), 0, (u_char *)&conf, AHA_CONF_GET);
	switch(conf.chan) {
	case CHAN0:
		outb(0x0b, 0x0c);
		outb(0x0a, 0x00);
		aha_dma[unit] = 0;
		break;
	case CHAN5:
		outb(0xd6, 0xc1);
		outb(0xd4, 0x01);
		aha_dma[unit] = 5;
		break;
	case CHAN6:
		outb(0xd6, 0xc2);
		outb(0xd4, 0x02);
		aha_dma[unit] = 6;
		break;
	case CHAN7:
		outb(0xd6, 0xc3);
		outb(0xd4, 0x03);
		aha_dma[unit] = 7;
		break;
	default:
		printf("illegal dma jumper setting\n");
		return(EIO);
	}

	switch(conf.intr) {
	case INT9:
		aha_int[unit] = 9;
		break;
	case INT10:
		aha_int[unit] = 10;
		break;
	case INT11:
		aha_int[unit] = 11;
		break;
	case INT12:
		aha_int[unit] = 12;
		break;
	case INT14:
		aha_int[unit] = 14;
		break;
	case INT15:
		aha_int[unit] = 15;
		break;
	default:
		printf("illegal int jumper setting\n");
		return(EIO);
	}
	/* who are we on the scsi bus */
	aha_scsi_dev[unit] = conf.scsi_dev;


	/*
	 * Initialize memory transfer speed
	 */
	speed[unit] = aha_set_bus_speed(unit);
	if(speed[unit] == 0) {
		printf("aha%d found, but unable to talk to it correctly\n");
		return(EIO);
	}

	/*
	 * Initialize bus-on time
	 *
	 * The default is 11ms, which can result in the fd driver becoming
	 * starved for data during simultaneous fd & scsi transfers.  We
	 * set it to 9ms - if this still gives you trouble, set to 6 (ms)
	 * and work your way up.
	 */
	aha_cmd(unit,1, 0, 0, 0, AHA_BUS_ON_TIME_SET, 9);

	/*
	 * Initialize mail box
	 */
	lto3b(KVTOPHYS(&aha_mbx[unit]), ad);

	aha_cmd(unit, 4, 0, 0, (u_char *)0, AHA_MBX_INIT, AHA_MBX_SIZE,
		ad[0], ad[1], ad[2]);


	/*
	 * link the ccb's with the mbox-out entries and
	 * into a free-list
	 */
	for (i=0; i < AHA_MBX_SIZE; i++) {
		aha_ccb[unit][i].next = aha_ccb_free[unit];
		aha_ccb_free[unit] = &aha_ccb[unit][i];
		aha_ccb_free[unit]->flags = CCB_FREE;
		aha_ccb_free[unit]->mbx = &aha_mbx[unit].mbo[i];
		lto3b(KVTOPHYS(aha_ccb_free[unit]), aha_mbx[unit].mbo[i].ccb_addr);
	}

	/*
	 * Note that we are going and return (to probe)
	 */
	aha_initialized[unit]++;
	return(0);
}

/*
 * aha seems to explode with 17 segs (64k may require 17 segs)
 * on old boards so use a max of 16 segs if you have problems
 * here
 */
void
ahaminphys(struct buf *bp)
{
	if(bp->b_bcount > ((AHA_NSEG - 1) * PAGESIZ))
		bp->b_bcount = ((AHA_NSEG - 1) * PAGESIZ);
}

/*
 * start a scsi operation given the command and
 * the data address. Also needs the unit, target
 * and lu
 */
int
aha_scsi_cmd(struct scsi_xfer *xs)
{
	struct	scsi_sense_data *s1,*s2;
	struct aha_ccb *ccb;
	struct aha_scat_gath *sg;
	int	seg;	/* scatter gather seg being worked on */
	int i	= 0;
	int rc	=  0;
	int	thiskv;
	int	thisphys,nextphys;
	int	unit =xs->adapter;
	int	bytes_this_seg,bytes_this_page,datalen,flags;
	struct	iovec	 *iovp;
	int	s;

	if(scsi_debug & PRINTROUTINES)
		printf("aha_scsi_cmd ");
	/*
	 * get a ccb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if(!(flags & INUSE)) {
		printf("not in use!");
		Debugger();
		xs->flags |= INUSE;
	}
	if(flags & ITSDONE) {
		printf("Already done! check device retry code ");
		Debugger();
		xs->flags &= ~ITSDONE;
	}
	if(xs->bp) flags |= (SCSI_NOSLEEP); /* just to be sure */
	if (!(ccb = aha_get_ccb(unit,flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}

	if (ccb->mbx->cmd != AHA_MBO_FREE)
		printf("MBO not free\n");

	/*
	 * Put all the arguments for the xfer in the ccb
	 * (can't use S/G if zero length)
	 */
	ccb->xfer = xs;
	if(flags & SCSI_RESET)
		ccb->opcode = AHA_RESET_CCB;
	else
		ccb->opcode = (xs->datalen ? AHA_INIT_SCAT_GATH_CCB : AHA_INITIATOR_CCB);
	ccb->target = xs->targ;;
	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->lun = xs->lu;
	ccb->scsi_cmd_length = xs->cmdlen;
	ccb->req_sense_length = sizeof(ccb->scsi_sense);

	/* can use S/G only if not zero length */
	if((xs->datalen) && (!(flags & SCSI_RESET))) {
		lto3b(KVTOPHYS(ccb->scat_gath), ccb->data_addr);
		sg = ccb->scat_gath;
		seg = 0;
		if(flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			while ((datalen) && (seg < AHA_NSEG)) {
				lto3b((u_long)iovp->iov_base, (u_char *)&sg->seg_addr);
				lto3b(iovp->iov_len, (u_char *)&sg->seg_len);
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x@0x%x)", iovp->iov_len,
						iovp->iov_base);
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else {
			/*
			 * Set up the scatter gather block
			 */
			if(scsi_debug & SHOWSCATGATH)
				printf("%d @0x%x:- ", xs->datalen, xs->data);
			datalen = xs->datalen;
			thiskv = (int)xs->data;
			thisphys = KVTOPHYS(thiskv);

			while ((datalen) && (seg < AHA_NSEG)) {
				bytes_this_seg	= 0;

				/* put in the base address */
				lto3b(thisphys, (u_char *)&(sg->seg_addr));

				if(scsi_debug & SHOWSCATGATH)
					printf("0x%x",thisphys);

				/* do it at least once */
				nextphys = thisphys;
				while ((datalen) && (thisphys == nextphys)) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
					/* how far to the end of the page? */
					nextphys = (thisphys & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					bytes_this_page	=
						min(nextphys - thisphys, datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					thiskv	= (thiskv & (~(PAGESIZ - 1)))
								+ PAGESIZ;
					if(datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				if(scsi_debug & SHOWSCATGATH)
					printf("(0x%x)",bytes_this_seg);
				lto3b(bytes_this_seg, (u_char *)&(sg->seg_len));
				sg++;
				seg++;
			}
		}
		lto3b(seg * sizeof(struct aha_scat_gath), ccb->data_length);
		if(scsi_debug & SHOWSCATGATH)
			printf("\n");
		if (datalen) {
			/* there's still data, must have run out of segs! */
			printf("aha_scsi_cmd%d: needed more than %d DMA segs, %d\n",
				unit, AHA_NSEG, datalen);
			xs->error = XS_DRIVER_STUFFUP;
			aha_free_ccb(unit, ccb, flags);
			return(HAD_ERROR);
		}
	} else {	/* No data xfer, use non S/G values */
		lto3b(0, ccb->data_addr );
		lto3b(0, ccb->data_length);
	}
	lto3b(0, ccb->link_addr);
	/*
	 * Put the scsi command in the ccb and start it
	 */
	if(!(flags & SCSI_RESET))
		bcopy(xs->cmd, &ccb->scsi_cmd, ccb->scsi_cmd_length);
	if(scsi_debug & SHOWCOMMANDS)
	{
		u_char	 *b = (u_char *)&ccb->scsi_cmd;
		if(!(flags & SCSI_RESET))
		{
			int i = 0;
			printf("aha%d:%d:%d-"
				,unit
				,ccb->target
				,ccb->lun );
				while(i < ccb->scsi_cmd_length )
				{
					if(i) printf(",");
					 printf("%x",b[i++]);
				}
		}
		else
		{
			printf("aha%d:%d:%d-RESET- " 
				,unit 
				,ccb->target
				,ccb->lun
			);
		}
	}
	if (!(flags & SCSI_NOMASK))
	{
		s= splbio(); /* stop instant timeouts */
		aha_add_timeout(ccb,xs->timeout);
		aha_startmbx(ccb->mbx);
		/*
		 * Usually return SUCCESSFULLY QUEUED
		 */
		splx(s);
		if(scsi_debug & TRACEINTERRUPTS)
			printf("sent ");
		return(SUCCESSFULLY_QUEUED);
	}
	aha_startmbx(ccb->mbx);
	if(scsi_debug & TRACEINTERRUPTS)
		printf("cmd_sent, waiting ");
	/*
	 * If we can't use interrupts, poll on completion*
	 */
	{
		int done = 0;
		int count = delaycount * xs->timeout / AHA_SCSI_TIMEOUT_FUDGE;
		while((!done) && count)
		{
			i=0;
			while ( (!done) && i<AHA_MBX_SIZE)
			{
				if ((aha_mbx[unit].mbi[i].stat != AHA_MBI_FREE )
				   && (PHYSTOKV(_3btol(aha_mbx[unit].mbi[i].ccb_addr)
					== (int)ccb)))
				{
					aha_mbx[unit].mbi[i].stat = AHA_MBI_FREE;
					aha_done(unit,ccb);
					done++;
				}
				i++;
			}
			count--;
		}
		if (!count)
		{
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			aha_abortmbx(ccb->mbx);
			count = delaycount * 2000 / AHA_SCSI_TIMEOUT_FUDGE;
			while((!done) && count) {
				i=0;
				while ( (!done) && i<AHA_MBX_SIZE) {
					if ((aha_mbx[unit].mbi[i].stat != AHA_MBI_FREE )
				   	    && (PHYSTOKV(_3btol(aha_mbx[unit].mbi[i].ccb_addr)
						== (int)ccb))) {
						aha_mbx[unit].mbi[i].stat = AHA_MBI_FREE;
						aha_done(unit,ccb);
						done++;
					}
					i++;
				}
				count--;
			}
			if(!count) {
				printf("abort failed in wait\n");
				ccb->mbx->cmd = AHA_MBO_FREE;
			}
			aha_free_ccb(unit,ccb,flags);
			ahaintr(unit);
			xs->error = XS_DRIVER_STUFFUP;
			return(HAD_ERROR);
		}
		ahaintr(unit);
		if(xs->error) return(HAD_ERROR);
		return(COMPLETE);
	} 
	/* return ??? */
}

/*
 * try each speed in turn, when we find one that works, use
 * the NEXT one for a safety margin, unless that doesn't exist
 * or doesn't work. returns the nsec value of the time used
 * or 0 if it could get a working speed ( or the NEXT speed 
 * failed)
 * Go one slower to be safe, unless eisa at 100 ns.. trust it
 */
int
aha_set_bus_speed(int unit)
{
	int retval, retval2;
	int speed;

#ifdef	EISA
	speed = 0; /* start at the fastest */
#else	EISA
	speed = 1; /* 100 ns can crash some ISA busses (!?!) */
#endif	EISA
	while (1) {
		retval = aha_bus_speed_check(unit, speed);
		if(retval == HAD_ERROR) {
			printf("no working bus speed!!!\n");
			return 0;
		}

		if(retval == 0)
			speed++;
		else {
			if(speed != 0)
				speed++;

			/*printf("%d nsec ok, but using ", retval);*/
			retval2 = aha_bus_speed_check(unit, speed);
			if(retval2 == HAD_ERROR) {
				/*printf("marginal ");*/
				retval2 = retval;
			}
			if(retval2) {
				/*printf("%d nsec\n", retval2);*/
				return retval2;
			} else {
				/*printf(".. slower failed, abort.\n", retval);*/
				return 0;
			}
		}
	}
}

/*
 * Set the DMA speed to the Nth speed and try an xfer. If it
 * fails return 0, if it succeeds return the nsec value selected
 * If there is no such speed return HAD_ERROR.
 */
static	struct bus_speed {
	char	arg;
	int	nsecs;
} aha_bus_speeds[] = {
	{0x88, 100},
	{0x99, 150},
	{0xaa, 200},
	{0xbb, 250},
	{0xcc, 300},
	{0xdd, 350},
	{0xee, 400},
	{0xff, 450}
};
static char aha_test_string[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz!@";

int
aha_bus_speed_check(int unit, int speed)
{
	int	numspeeds = sizeof(aha_bus_speeds)/sizeof(struct bus_speed);
	u_char	ad[3];

	/*
	 * Check we have such an entry
	 */
	if(speed >= numspeeds) return(HAD_ERROR);	/* illegal speed */

	/*
	 * Set the dma-speed
	 */
	aha_cmd(unit,1, 0, 0, (u_char *)0, AHA_SPEED_SET,aha_bus_speeds[speed].arg);

	/*
	 * put the test data into the buffer and calculate
	 * it's address. Read it onto the board
	 */
	strcpy((char *)aha_scratch_buf, (char *)aha_test_string);
	lto3b(KVTOPHYS(aha_scratch_buf), ad);

	aha_cmd(unit,3, 0, 0, (u_char *)0, AHA_WRITE_FIFO,
		ad[0], ad[1], ad[2]);

	/*
	 * clear the buffer then copy the contents back from the
	 * board.
	 */
	bzero(aha_scratch_buf,54);	/* 54 bytes transfered by test */

	aha_cmd(unit,3, 0, 0, (u_char *)0, AHA_READ_FIFO,
		ad[0], ad[1], ad[2]);

	/*
	 * Compare the original data and the final data and
	 * return the correct value depending upon the result
	 * if copy fails.. assume too fast
	 */
	if(strcmp(aha_test_string, (char *)aha_scratch_buf))
		return(0);
	return(aha_bus_speeds[speed].nsecs);
}


/*
 *                +----------+    +----------+    +----------+
 * aha_soonest--->|    later |--->|     later|--->|     later|-->0
 *                | [Delta]  |    | [Delta]  |    | [Delta]  |
 *           0<---|sooner    |<---|sooner    |<---|sooner    |<---aha_latest
 *                +----------+    +----------+    +----------+
 *
 *     aha_furtherest = sum(Delta[1..n])
 */
void
aha_add_timeout(struct aha_ccb *ccb, int time)
{
	int	timeprev;
	struct aha_ccb *prev;
	int	s = splbio();

	if(prev = aha_latest) /* yes, an assign */
		timeprev = aha_furtherest;
	else
		timeprev = 0;

	while(prev && (timeprev > time)) {
		timeprev -= prev->delta;
		prev = prev->sooner;
	}
	if(prev) {
		ccb->delta = time - timeprev;
		if( ccb->later = prev->later) {
			ccb->later->sooner = ccb;
			ccb->later->delta -= ccb->delta;
		} else {
			aha_furtherest = time;
			aha_latest = ccb;
		}
		ccb->sooner = prev;
		prev->later = ccb;
	} else {
		if( ccb->later = aha_soonest) {
			ccb->later->sooner = ccb;
			ccb->later->delta -= time;
		} else {
			aha_furtherest = time;
			aha_latest = ccb;
		}
		ccb->delta = time;
		ccb->sooner = (struct aha_ccb *)0;
		aha_soonest = ccb;
	}
	splx(s);
}

void
aha_remove_timeout(struct aha_ccb *ccb)
{
	int	s = splbio();

	if(ccb->sooner)
		ccb->sooner->later = ccb->later;
	else
		aha_soonest = ccb->later;

	if(ccb->later) {
		ccb->later->sooner = ccb->sooner;
		ccb->later->delta += ccb->delta;
	} else {
		aha_latest = ccb->sooner;
		aha_furtherest -= ccb->delta;
	}
	ccb->sooner = ccb->later = (struct aha_ccb *)0;
	splx(s);
}

extern int 	hz;
#define ONETICK 500 /* milliseconds */
#define SLEEPTIME ((hz * 1000) / ONETICK)

void
aha_timeout(int arg)
{
	struct  aha_ccb  *ccb;
	int	unit;
	int	s	= splbio();

	while( ccb = aha_soonest ) {
		if(ccb->delta <= ONETICK) {
			/*
			 * It has timed out, we need to do some work
			 */
			unit = ccb->xfer->adapter;
			printf("aha%d: device %d timed out ", unit,
				ccb->xfer->targ);

			/*
			 * Unlink it from the queue
			 */
			aha_remove_timeout(ccb);

			/*
			 * If The ccb's mbx is not free, then
			 * the board has gone south
			 */
			if(ccb->mbx->cmd != AHA_MBO_FREE) {
				printf("aha%d not taking commands!\n", unit);
				Debugger();
			}
			/*
			 * If it has been through before, then
			 * a previous abort has failed, don't
			 * try abort again
			 */
			if(ccb->flags == CCB_ABORTED) {
				 /* abort timed out */
				printf(" AGAIN\n");
				ccb->xfer->retries = 0;	/* I MEAN IT ! */
				ccb->host_stat = AHA_ABORTED;
				aha_done(unit,ccb);
			} else {
				/* abort the operation that has timed out */
				printf("\n");
				aha_abortmbx(ccb->mbx);
				/* 2 secs for the abort */
				aha_add_timeout(ccb,2000 + ONETICK);
				ccb->flags = CCB_ABORTED;
			}
		} else {
			/*
			 * It has not timed out, adjust and leave
			 */
			ccb->delta -= ONETICK;
			aha_furtherest -= ONETICK;
			break;
		}
	}
	splx(s);
	timeout((timeout_t)aha_timeout, (caddr_t)arg, SLEEPTIME);
}
