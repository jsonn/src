/*#define DEBUG 1*/
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)fd.c	7.4 (Berkeley) 5/25/91
 *	$Id: fd.c,v 1.19.2.1 1993/07/28 00:11:21 deraadt Exp $
 *
 * Largely rewritten to handle multiple controllers and drives
 * By Julian Elischer, Sun Apr  4 16:34:33 WST 1993
 */
/*
 * $Log: fd.c,v $
 * Revision 1.19.2.1  1993/07/28 00:11:21  deraadt
 * wd.c: fixes 1 drive systems
 * fd.c: improves reliability
 * changes from wolfgang
 *
 * Revision 1.19  1993/07/16  15:44:22  mycroft
 * #include cpufunc,h so inb() and outb() are inlined.
 *
 * Revision 1.18  1993/07/06  06:06:29  deraadt
 * clean up code for timeout/untimeout/wakeup prototypes.
 *
 * Revision 1.17  1993/06/29  19:12:44  deraadt
 * uninitialized variable reported by <jfw@ksr.com>
 *
 * Revision 1.16  1993/06/21  09:39:52  deraadt
 * I don't know what I did that was so critical, but now the floppy driver
 * works on my machine (it did not before). Big voodoo.
 *
 * Revision 1.15  1993/06/20  08:42:05  deraadt
 * if the floppy does not exist, say nothing.
 *
 * Revision 1.14  1993/06/18  06:19:16  cgd
 * new floppy driver, merged from patchkit patch #153
 *
 * Revision 1.1.1.1  1993/06/12  14:58:02  rgrimes
 * Initial import, 0.1 + pk 0.2.4-B1
 *
 * Revision 1.10  93/04/13  16:53:29  root
 * make sure turning off a drive motor doesn't deselect another
 * drive active at the time.
 * Also added a pointer from the fd_data to it's fd_type.
 * 
 * Revision 1.9  93/04/13  15:31:02  root
 * make all seeks go through DOSEEK state so are sure of being done right.
 * 
 * Revision 1.8  93/04/12  21:20:13  root
 * only check if old fd is the one we are working on if there IS
 * an old fd pointer. (in fdstate())
 * 
 * Revision 1.7  93/04/11  17:05:35  root
 * cleanup timeouts etc.
 * also fix bug to select teh correct drive when running > 1 drive
 * at a time.
 * 
 * Revision 1.6  93/04/05  00:48:45  root
 * change a timeout and add version to banner message
 * 
 * Revision 1.5  93/04/04  16:39:08  root
 * first working version.. some floppy controllers don't seem to
 * like 2 int. status inquiries in a row.
 * 
 */

#include "fd.h"
#if NFDC > 0

#include "param.h"
#include "dkbad.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "ioctl.h"
#include "buf.h"
#include "uio.h"
#include "machine/cpufunc.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/fdreg.h"
#include "i386/isa/icu.h"
#include "i386/isa/rtc.h"

#define	FDUNIT(s)	((s>>3)&1)
#define	FDTYPE(s)	((s)&7)

#define b_cylin b_resid
#define FDBLK 512

struct fd_type {
	int	sectrac;		/* sectors per track         */
	int	secsize;		/* size code for sectors     */
	int	datalen;		/* data len when secsize = 0 */
	int	gap;			/* gap len between sectors   */
	int	tracks;			/* total num of tracks       */
	int	size;			/* size of disk in sectors   */
	int	steptrac;		/* steps per cylinder        */
	int	trans;			/* transfer speed code       */
	int	heads;			/* number of heads	     */
};

struct fd_type fd_types[] =
{
 	{ 18,2,0xFF,0x1B,80,2880,1,0,2 }, /* 1.44 meg HD 3.5in floppy    */
	{ 15,2,0xFF,0x1B,80,2400,1,0,2 }, /* 1.2 meg HD floppy           */
	{ 9,2,0xFF,0x23,40,720,2,1,2 },	/* 360k floppy in 1.2meg drive */
	{ 9,2,0xFF,0x2A,40,720,1,1,2 },	/* 360k floppy in DD drive     */
	{ 9,2,0xFF,0x2A,80,1440,1,0 },	/* 720K drive. PROBABLY WRONG	*/
};

#define DRVS_PER_CTLR 2
/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
struct fdc_data
{
	int	fdcu;		/* our unit number */
	int	baseport;
	int	dmachan;
	int	flags;
#define FDC_ATTACHED	0x01
	struct	fd_data *fd;
	int fdu;		/* the active drive	*/
	struct buf head;	/* Head of buf chain      */
	struct buf rhead;	/* Raw head of buf chain  */
	int state;
	int retry;
	int status[7];		/* copy of the registers */
}fdc_data[NFDC];

/***********************************************************************\
* Per drive structure.							*
* N per controller (presently 2) (DRVS_PER_CTLR)			*
\***********************************************************************/
struct fd_data {
	struct	fdc_data *fdc;
	int	fdu;		/* this unit number */
	int	fdsu;		/* this units number on this controller */
	int	type;		/* Drive type (HD, DD     */
	struct	fd_type *ft;	/* pointer to the type descriptor */
	int	flags;
#define	FD_OPEN		0x01	/* it's open		*/
#define	FD_ACTIVE	0x02	/* it's active		*/
#define	FD_MOTOR	0x04	/* motor should be on	*/
#define	FD_MOTOR_WAIT	0x08	/* motor coming up	*/
	int skip;
	int hddrv;
	int track;		/* where we think the head is */
} fd_data[NFD];

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/
typedef int	fdu_t;
typedef int	fdcu_t;
typedef int	fdsu_t;
typedef	struct fd_data *fd_p;
typedef struct fdc_data *fdc_p;

#define DEVIDLE		0
#define FINDWORK	1
#define	DOSEEK		2
#define SEEKCOMPLETE 	3
#define	IOCOMPLETE	4
#define RECALCOMPLETE	5
#define	STARTRECAL	6
#define	RESETCTLR	7
#define	SEEKWAIT	8
#define	RECALWAIT	9
#define	MOTORWAIT	10
#define	IOTIMEDOUT	11

#ifdef	DEBUG
char *fdstates[] =
{
"DEVIDLE",
"FINDWORK",
"DOSEEK",
"SEEKCOMPLETE",
"IOCOMPLETE",
"RECALCOMPLETE",
"STARTRECAL",
"RESETCTLR",
"SEEKWAIT",
"RECALWAIT",
"MOTORWAIT",
"IOTIMEDOUT"
};


int	fd_debug = 1;
#define TRACE0(arg) if(fd_debug) printf(arg)
#define TRACE1(arg1,arg2) if(fd_debug) printf(arg1,arg2)
#else	DEBUG
#define TRACE0(arg)
#define TRACE1(arg1,arg2)
#endif	DEBUG

extern int hz;
/* state needed for current transfer */

/****************************************************************************/
/*                      autoconfiguration stuff                             */
/****************************************************************************/
int fdprobe(), fdattach(), fd_turnoff();

struct	isa_driver fdcdriver = {
	fdprobe, fdattach, "fdc",
};

/*
 * probe for existance of controller
 */
fdprobe(dev)
struct isa_device *dev;
{
	fdc_p fdc = &fdc_data[dev->id_unit];
	int fdcu = dev->id_unit;

	fdc->baseport = dev->id_iobase;

	/* Try a reset, don't change motor on */
	set_motor(fdcu,0,1);
	DELAY(100);
	set_motor(fdcu,0,0);
	/* see if it can handle a command */
	if (out_fdc(fdcu,NE7CMD_SPECIFY) < 0)
	{
		return(0);
	}
	out_fdc(fdcu,0xDF);
	out_fdc(fdcu,2);

	fdc->dmachan = dev->id_drq;
	fdc->fdcu = dev->id_unit;
	fdc->flags |= FDC_ATTACHED;
	fdc->state = DEVIDLE;

	/*
	 * tdr: I've put this here, because with the new way for
	 * attach() being called it cannot go there.
	 */
	/* Set transfer to 500kbps */
	outb(fdc->baseport+fdctl,0); /*XXX*/

	return (IO_FDCSIZE);
}

/*
 * wire controller into system, look for floppy units
 */
fdattach(dev)
struct isa_device *dev;
{
	unsigned fdt=0,st0, cyl;
	fdu_t	fdu = dev->id_unit;
	fdcu_t	fdcu = dev->id_masunit;
	fdc_p	fdc = &fdc_data[dev->id_masunit];
	fd_p	fd = &fd_data[dev->id_unit];
	int	fdsu = dev->id_physid;

	if(dev->id_physid < 0 || dev->id_physid > 1) {
		printf("fdc%d: cannot support physical unit %d\n",
			dev->id_masunit, dev->id_physid);
		return 0;
	}
	if(dev->id_masunit==0)
		fdt = rtcin(RTC_FDISKETTE);
	else
		fdt = 0xff;	/* cmos only knows two floppies */

	if(dev->id_physid == 1)
		fdt <<= 4;

#ifdef notyet
	/* select it */
	fd_turnon1(fdu);
	spinwait(1000);	/* 1 sec */
	out_fdc(fdcu,NE7CMD_RECAL);	/* Recalibrate Function */
	out_fdc(fdcu,fdsu);
	spinwait(1000);	/* 1 sec */

	/* anything responding */
	out_fdc(fdcu,NE7CMD_SENSEI);
	st0 = in_fdc(fdcu);
	cyl = in_fdc(fdcu);
	if (st0 & 0xd0)
		continue;
#endif
	fd->track = -2;
	fd->fdc = fdc;
	fd->fdsu = fdsu;

	switch(fdt & 0xf0) {
	case RTCFDT_NONE:
		/*printf("fd%d at fdc%d targ %d: nonexistant device\n",
			dev->id_unit, dev->id_masunit, dev->id_physid);*/
		return 0;
		break;
	case RTCFDT_12M:
		printf("fd%d at fdc%d targ %d: 1.2MB 80 cyl, 2 head, 15 sec\n",
			dev->id_unit, dev->id_masunit, dev->id_physid);
		fd->type = 1;
		break;
	case RTCFDT_144M:
		printf("fd%d at fdc%d targ %d: 1.44MB 80 cyl, 2 head, 18 sec\n",
			dev->id_unit, dev->id_masunit, dev->id_physid);
		fd->type = 0;
		break;
	case RTCFDT_360K:
		printf("fd%d at fdc%d targ %d: 360KB 40 cyl, 2 head, 9 sec\n",
			dev->id_unit, dev->id_masunit, dev->id_physid);
		fd->type = 3;
		break;
	case RTCFDT_720K:
		printf("fd%d at fdc%d targ %d: 720KB 80 cyl, 2 head, 9 sec\n",
			dev->id_unit, dev->id_masunit, dev->id_physid);
		fd->type = 4;
		break;
	default:
		printf("fd%d at fdc%d targ %d: unknown device type 0x%x\n",
			dev->id_unit, dev->id_masunit, dev->id_physid,
			fdt & 0xf0);
		return 0;
		break;
	}

	fd->ft = &fd_types[fd->type];
	fd_turnoff(fdu);
	return 1;
}

int
fdsize(dev)
dev_t	dev;
{
	return(0);
}

/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
fdstrategy(bp)
	register struct buf *bp;	/* IO operation to perform */
{
	register struct buf *dp,*dp0,*dp1;
	long nblocks,blknum;
 	int	s;
 	fdcu_t	fdcu;
 	fdu_t	fdu;
 	fdc_p	fdc;
 	fd_p	fd;

 	fdu = FDUNIT(minor(bp->b_dev));
	fd = &fd_data[fdu];
	fdc = fd->fdc;
	fdcu = fdc->fdcu;
 	/*type = FDTYPE(minor(bp->b_dev));*/

	if ((fdu >= NFD) || (bp->b_blkno < 0)) {
		printf("fdstrat: fdu = %d, blkno = %d, bcount = %d\n",
			fdu, bp->b_blkno, bp->b_bcount);
		pg("fd:error in fdstrategy");
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	/*
	 * Set up block calculations.
	 */
	blknum = (unsigned long) bp->b_blkno * DEV_BSIZE/FDBLK;
 	nblocks = fd->ft->size;
	if (blknum + (bp->b_bcount / FDBLK) > nblocks) {
		if (blknum == nblocks) {
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = ENOSPC;
			bp->b_flags |= B_ERROR;
		}
		goto bad;
	}
 	bp->b_cylin = blknum / (fd->ft->sectrac * fd->ft->heads);
	dp = &(fdc->head);
	s = splbio();
	disksort(dp, bp);
	untimeout((timeout_t)fd_turnoff, (caddr_t)fdu); /* a good idea */
	fdstart(fdcu);
	splx(s);
	return;

bad:
	biodone(bp);
}

/****************************************************************************/
/*                            motor control stuff                           */
/*		remember to not deselect the drive we're working on         */
/****************************************************************************/
set_motor(fdcu_t fdcu, fdu_t fdu, int reset)
{
	int m0,m1;
	int selunit;
	fd_p fd;
	if(fd = fdc_data[fdcu].fd)/* yes an assign! */
	{
		selunit =  fd->fdsu;
	}
	else
	{
		selunit = 0;
	}
	m0 = fd_data[fdcu * DRVS_PER_CTLR + 0].flags & FD_MOTOR;
	m1 = fd_data[fdcu * DRVS_PER_CTLR + 1].flags & FD_MOTOR;
	outb(fdc_data[fdcu].baseport+fdout,
		selunit
		| (reset ? 0 : (FDO_FRST|FDO_FDMAEN))
		| (m0 ? FDO_MOEN0 : 0)
		| (m1 ? FDO_MOEN1 : 0));
	TRACE1("[0x%x->fdout]",(
		selunit
		| (reset ? 0 : (FDO_FRST|FDO_FDMAEN))
		| (m0 ? FDO_MOEN0 : 0)
		| (m1 ? FDO_MOEN1 : 0)));
}

fd_turnoff(fdu_t fdu)
{
	fd_p fd = fd_data + fdu;
	fd->flags &= ~FD_MOTOR;
	set_motor(fd->fdc->fdcu,fd->fdsu,0);
}

fd_motor_on(fdu_t fdu)
{
	fd_p fd = fd_data + fdu;
	fd->flags &= ~FD_MOTOR_WAIT;
	if((fd->fdc->fd == fd) && (fd->fdc->state == MOTORWAIT))
	{
		fd_pseudointr(fd->fdc->fdcu);
	}
}

fd_turnon(fdu_t fdu) 
{
	fd_p fd = fd_data + fdu;
	if(!(fd->flags & FD_MOTOR))
	{
		fd_turnon1(fdu);
		fd->flags |= FD_MOTOR_WAIT;
		timeout((timeout_t)fd_motor_on, (caddr_t)fdu, hz); /* in 1 sec its ok */
	}
}

fd_turnon1(fdu_t fdu) 
{
	fd_p fd = fd_data + fdu;
	fd->flags |= FD_MOTOR;
	set_motor(fd->fdc->fdcu,fd->fdsu,0);
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
int
in_fdc(fdcu_t fdcu)
{
	int baseport = fdc_data[fdcu].baseport;
	int i, j = 100000;
	while ((i = inb(baseport+fdsts) & (NE7_DIO|NE7_RQM))
		!= (NE7_DIO|NE7_RQM) && j-- > 0)
		if (i == NE7_RQM) return -1;
	if (j <= 0)
		return(-1);
#ifdef	DEBUG
	i = inb(baseport+fddata);
	TRACE1("[fddata->0x%x]",(unsigned char)i);
	return(i);
#else
	return inb(baseport+fddata);
#endif
}

out_fdc(fdcu_t fdcu,int x)
{
	int baseport = fdc_data[fdcu].baseport;
	int i = 100000;

	while ((inb(baseport+fdsts) & NE7_DIO) && i-- > 0);
	while ((inb(baseport+fdsts) & NE7_RQM) == 0 && i-- > 0);
	if (i <= 0) return (-1);
	outb(baseport+fddata,x);
	TRACE1("[0x%x->fddata]",x);
	return (0);
}

static fdopenf;
/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
Fdopen(dev, flags)
	dev_t	dev;
	int	flags;
{
 	fdu_t fdu = FDUNIT(minor(dev));
 	/*int type = FDTYPE(minor(dev));*/
	int s;

	/* check bounds */
	if (fdu >= NFD) return(ENXIO);
	/*if (type >= sizeof(fd_types)/sizeof(fd_types[0]) ) return(ENXIO);*/
	fd_data[fdu].flags |= FD_OPEN;

	return 0;
}

fdclose(dev, flags)
	dev_t dev;
{
 	fdu_t fdu = FDUNIT(minor(dev));
	fd_data[fdu].flags &= ~FD_OPEN;
	return(0);
}


/***************************************************************\
*				fdstart				*
* We have just queued something.. if the controller is not busy	*
* then simulate the case where it has just finished a command	*
* So that it (the interrupt routine) looks on the queue for more*
* work to do and picks up what we just added.			*
* If the controller is already busy, we need do nothing, as it	*
* will pick up our work when the present work completes		*
\***************************************************************/
fdstart(fdcu_t fdcu)
{
	register struct buf *dp,*bp;
	int s;
 	fdu_t fdu;

	s = splbio();
	if(fdc_data[fdcu].state == DEVIDLE)
	{
		fdintr(fdcu);
	}
	splx(s);
}

fd_timeout(fdcu_t fdcu)
{
	fdu_t fdu = fdc_data[fdcu].fdu;
	int st0, st3, cyl;
	struct buf *dp,*bp;

	dp = &fdc_data[fdcu].head;
	bp = dp->b_actf;

	out_fdc(fdcu,NE7CMD_SENSED);
	out_fdc(fdcu,fd_data[fdu].hddrv);
	st3 = in_fdc(fdcu);

	out_fdc(fdcu,NE7CMD_SENSEI);
	st0 = in_fdc(fdcu);
	cyl = in_fdc(fdcu);
	printf("fd%d: Operation timeout ST0 %b cyl %d ST3 %b\n",
			fdu,
			st0,
			NE7_ST0BITS,
			cyl,
			st3,
			NE7_ST3BITS);

	if (bp)
	{
		retrier(fdcu);
		fdc_data[fdcu].status[0] = 0xc0;
		fdc_data[fdcu].state = IOTIMEDOUT;
		if( fdc_data[fdcu].retry < 6)
			fdc_data[fdcu].retry = 6;
	}
	else
	{
		fdc_data[fdcu].fd = (fd_p) 0;
		fdc_data[fdcu].fdu = -1;
		fdc_data[fdcu].state = DEVIDLE;
	}
	fd_pseudointr(fdcu);
}

/* just ensure it has the right spl */
fd_pseudointr(fdcu_t fdcu)
{
	int	s;
	s = splbio();
	fdintr(fdcu);
	splx(s);
}

/***********************************************************************\
*                                 fdintr				*
* keep calling the state machine until it returns a 0			*
* ALWAYS called at SPLBIO 						*
\***********************************************************************/
fdintr(fdcu_t fdcu)
{
	fdc_p fdc = fdc_data + fdcu;
	while(fdstate(fdcu, fdc));
}

/***********************************************************************\
* The controller state machine.						*
* if it returns a non zero value, it should be called again immediatly	*
\***********************************************************************/
int fdstate(fdcu_t fdcu, fdc_p fdc)
{
	int read,head,trac,sec,i,s,sectrac,cyl,st0;
	unsigned long blknum;
	fdu_t fdu = fdc->fdu;
	fd_p fd;
	register struct buf *dp,*bp;

	dp = &(fdc->head);
	bp = dp->b_actf;
	if(!bp) 
	{
		/***********************************************\
		* nothing left for this controller to do	*
		* Force into the IDLE state,			*
		\***********************************************/
		fdc->state = DEVIDLE;
		if(fdc->fd)
		{
			printf("unexpected valid fd pointer (fdu = %d)\n"
						,fdc->fdu);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
		}
		TRACE1("[fdc%d IDLE]",fdcu);
 		return(0);
	}
	fdu = FDUNIT(minor(bp->b_dev));
	fd = fd_data + fdu;
	if (fdc->fd && (fd != fdc->fd))
	{
		printf("confused fd pointers\n");
	}
	read = bp->b_flags & B_READ;
	TRACE1("fd%d",fdu);
	TRACE1("[%s]",fdstates[fdc->state]);
	TRACE1("(0x%x)",fd->flags);
	untimeout((timeout_t)fd_turnoff, (caddr_t)fdu);
	timeout((timeout_t)fd_turnoff, (caddr_t)fdu, 4 * hz);
	switch (fdc->state)
	{
	case DEVIDLE:
	case FINDWORK:	/* we have found new work */
		fdc->retry = 0;
		fd->skip = 0;
		fdc->fd = fd;
		fdc->fdu = fdu;
		/*******************************************************\
		* If the next drive has a motor startup pending, then	*
		* it will start up in it's own good time		*
		\*******************************************************/
		if(fd->flags & FD_MOTOR_WAIT)
		{
			fdc->state = MOTORWAIT;
			return(0); /* come back later */
		}
		/*******************************************************\
		* Maybe if it's not starting, it SHOULD be starting	*
		\*******************************************************/
		if (!(fd->flags & FD_MOTOR))
		{
			fdc->state = MOTORWAIT;
			fd_turnon(fdu);
			return(0);
		}
		else	/* at least make sure we are selected */
		{
			set_motor(fdcu,fd->fdsu,0);
		}
		fdc->state = DOSEEK;
		break;
	case DOSEEK:
		if (bp->b_cylin == fd->track)
		{
			fdc->state = SEEKCOMPLETE;
			break;
		}
		out_fdc(fdcu,NE7CMD_SEEK);	/* Seek function */
		out_fdc(fdcu,fd->fdsu);		/* Drive number */
		out_fdc(fdcu,bp->b_cylin * fd->ft->steptrac);
		fd->track = -2;
		fdc->state = SEEKWAIT;
		return(0);	/* will return later */
	case SEEKWAIT:
		/* allow heads to settle */
		timeout((timeout_t)fd_pseudointr, (caddr_t)fdcu, hz/50);
		fdc->state = SEEKCOMPLETE;
		return(0);	/* will return later */
		break;
		
	case SEEKCOMPLETE : /* SEEK DONE, START DMA */
		/* Make sure seek really happened*/
		if(fd->track == -2)
		{
			int descyl = bp->b_cylin * fd->ft->steptrac;
			out_fdc(fdcu,NE7CMD_SENSEI);
			i = in_fdc(fdcu);
			cyl = in_fdc(fdcu);
			if (cyl != descyl)
			{
				printf("fd%d: Seek to cyl %d failed; am at cyl %d (ST0 = 0x%x)\n", fdu,
				descyl, cyl, i, NE7_ST0BITS);
				return(retrier(fdcu));
			}
		}

		fd->track = bp->b_cylin;
		isa_dmastart(bp->b_flags, bp->b_un.b_addr+fd->skip,
			FDBLK, fdc->dmachan);
		blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/FDBLK
			+ fd->skip/FDBLK;
		sectrac = fd->ft->sectrac;
		sec = blknum %  (sectrac * fd->ft->heads);
		head = sec / sectrac;
		sec = sec % sectrac + 1;
/*XXX*/		fd->hddrv = ((head&1)<<2)+fdu;

		if (read)
		{
			out_fdc(fdcu,NE7CMD_READ);	/* READ */
		}
		else
		{
			out_fdc(fdcu,NE7CMD_WRITE);	/* WRITE */
		}
		out_fdc(fdcu,head << 2 | fdu);	/* head & unit */
		out_fdc(fdcu,fd->track);		/* track */
		out_fdc(fdcu,head);
		out_fdc(fdcu,sec);			/* sector XXX +1? */
		out_fdc(fdcu,fd->ft->secsize);		/* sector size */
		out_fdc(fdcu,sectrac);		/* sectors/track */
		out_fdc(fdcu,fd->ft->gap);		/* gap size */
		out_fdc(fdcu,fd->ft->datalen);		/* data length */
		fdc->state = IOCOMPLETE;
		timeout((timeout_t)fd_timeout, (caddr_t)fdcu, 2 * hz);
		return(0);	/* will return later */
	case IOCOMPLETE: /* IO DONE, post-analyze */
		untimeout((timeout_t)fd_timeout, (caddr_t)fdcu);
		for(i=0;i<7;i++)
		{
			fdc->status[i] = in_fdc(fdcu);
		}
	case IOTIMEDOUT: /*XXX*/
		isa_dmadone(bp->b_flags, bp->b_un.b_addr+fd->skip,
			FDBLK, fdc->dmachan);
		if (fdc->status[0]&0xF8)
		{
			return(retrier(fdcu));
		}
		/* All OK */
		fd->skip += FDBLK;
		if (fd->skip < bp->b_bcount)
		{
			/* set up next transfer */
			blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/FDBLK
				+ fd->skip/FDBLK;
			bp->b_cylin = (blknum / (fd->ft->sectrac * fd->ft->heads));
			fdc->state = DOSEEK;
		}
		else
		{
			/* ALL DONE */
			fd->skip = 0;
			bp->b_resid = 0;
			dp->b_actf = bp->av_forw;
			biodone(bp);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
			fdc->state = FINDWORK;
		}
		return(1);
	case RESETCTLR:
		/* Try a reset, keep motor on */
		set_motor(fdcu,fd->fdsu,1);
		DELAY(100);
		set_motor(fdcu,fd->fdsu,0);
		outb(fdc->baseport+fdctl,fd->ft->trans);
		TRACE1("[0x%x->fdctl]",fd->ft->trans);
		fdc->retry++;
		fdc->state = STARTRECAL;
		break;
	case STARTRECAL:
		out_fdc(fdcu,NE7CMD_SPECIFY); /* specify command */
		out_fdc(fdcu,0xDF);
		out_fdc(fdcu,2);
		out_fdc(fdcu,NE7CMD_RECAL);	/* Recalibrate Function */
		out_fdc(fdcu,fdu);
		fdc->state = RECALWAIT;
		return(0);	/* will return later */
	case RECALWAIT:
		/* allow heads to settle */
		timeout((timeout_t)fd_pseudointr, (caddr_t)fdcu, hz/30);
		fdc->state = RECALCOMPLETE;
		return(0);	/* will return later */
	case RECALCOMPLETE:
		out_fdc(fdcu,NE7CMD_SENSEI);
		st0 = in_fdc(fdcu);
		cyl = in_fdc(fdcu);
		if (cyl != 0)
		{
			printf("fd%d: recal failed ST0 %b cyl %d\n", fdu,
				st0, NE7_ST0BITS, cyl);
			return(retrier(fdcu));
		}
		fd->track = 0;
		/* Seek (probably) necessary */
		fdc->state = DOSEEK;
		return(1);	/* will return immediatly */
	case	MOTORWAIT:
		if(fd->flags & FD_MOTOR_WAIT)
		{
			return(0); /* time's not up yet */
		}
		fdc->state = DOSEEK;
		return(1);	/* will return immediatly */
	default:
		printf("Unexpected FD int->");
		out_fdc(fdcu,NE7CMD_SENSEI);
		st0 = in_fdc(fdcu);
		cyl = in_fdc(fdcu);
		printf("ST0 = %lx, PCN = %lx\n",i,sec);
		out_fdc(fdcu,0x4A); 
		out_fdc(fdcu,fd->fdsu);
		for(i=0;i<7;i++) {
			fdc->status[i] = in_fdc(fdcu);
		}
	printf("intr status :%lx %lx %lx %lx %lx %lx %lx ",
		fdc->status[0],
		fdc->status[1],
		fdc->status[2],
		fdc->status[3],
		fdc->status[4],
		fdc->status[5],
		fdc->status[6] );
		return(0);
	}
	return(1); /* Come back immediatly to new state */
}

retrier(fdcu_t fdcu)
{
	fdc_p fdc = fdc_data + fdcu;
	register struct buf *dp,*bp;

	dp = &(fdc->head);
	bp = dp->b_actf;

	switch(fdc->retry)
	{
	case 0: case 1: case 2:
		fdc->state = SEEKCOMPLETE;
		break;
	case 3: case 4: case 5:
		fdc->state = STARTRECAL;
		break;
	case 6:
		fdc->state = RESETCTLR;
		break;
	case 7:
		break;
	default:
		{
			printf("fd%d: hard error (ST0 %b ",
				 fdc->fdu, fdc->status[0], NE7_ST0BITS);
			printf(" ST1 %b ", fdc->status[1], NE7_ST1BITS);
			printf(" ST2 %b ", fdc->status[2], NE7_ST2BITS);
			printf(" ST3 %b ", fdc->status[3], NE7_ST3BITS);
			printf("cyl %d hd %d sec %d)\n",
				 fdc->status[4], fdc->status[5], fdc->status[6]);
		}
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount - fdc->fd->skip;
		dp->b_actf = bp->av_forw;
		fdc->fd->skip = 0;
		biodone(bp);
		fdc->state = FINDWORK;
		fdc->fd = (fd_p) 0;
		fdc->fdu = -1;
		return(1);
	}
	fdc->retry++;
	return(1);
}

#endif

