/*	$NetBSD: adb_direct.c,v 1.6.4.1 1997/11/11 01:35:01 mellon Exp $	*/

/*  From: adb_direct.c 2.02 4/18/97 jpw */

/*
 * Copyright (C) 1996, 1997 John P. Wittkoski
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
 *  This product includes software developed by John P. Wittkoski.
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

/* This code is rather messy, but I don't have time right now
 * to clean it up as much as I would like.
 * But it works, so I'm happy. :-) jpw */
 
/* TO DO:
 *  - We could reduce the time spent in the adb_intr_* routines
 *    by having them save the incoming and outgoing data directly 
 *    in the adbInbound and adbOutbound queues, as it would reduce
 *    the number of times we need to copy the data around. It
 *    would also make the code more readable and easier to follow.
 *  - (Related to above) Use the header part of adbCommand to 
 *    reduce the number of copies we have to do of the data.
 *  - (Related to above) Actually implement the adbOutbound queue.
 *    This is fairly easy once you switch all the intr routines
 *    over to using adbCommand structs directly.
 *  - There is a bug in the state machine of adb_intr_cuda
 *    code that causes hangs, especially on 030 machines, probably
 *    because of some timing issues. Because I have been unable to 
 *    determine the exact cause of this bug, I used the timeout function 
 *    to check for and recover from this condition. If anyone finds 
 *    the actual cause of this bug, the calls to timeout and the 
 *    adb_cuda_tickle routine can be removed.
 */

#ifdef __NetBSD__
#include "opt_mrg_adb.h"

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/systm.h>

#include <machine/viareg.h>
#include <machine/param.h>
#include <machine/cpu.h>
#include <machine/adbsys.h>			/* required for adbvar.h */

#include <mac68k/mac68k/macrom.h>
#include <mac68k/dev/adb_direct.h>
#include <mac68k/dev/adbvar.h>
#define printf_intr printf
#else
#include "via.h"				/* for macos based testing */
#endif

/* more verbose for testing */
/*#define DEBUG*/

/* some misc. leftovers */
#define vPB		0x0000
#define vPB3		0x08
#define vPB4		0x10
#define vPB5		0x20
#define vSR_INT		0x04
#define vSR_OUT		0x10

/* types of adb hardware that we (will eventually) support */
#define ADB_HW_UNKNOWN		0x01	/* don't know */
#define ADB_HW_II		0x02	/* Mac II series */
#define ADB_HW_IISI		0x03	/* Mac IIsi series */
#define ADB_HW_PB		0x04	/* PowerBook series */
#define ADB_HW_CUDA		0x05	/* Machines with a Cuda chip */

/* the type of ADB action that we are currently preforming */
#define ADB_ACTION_NOTREADY	0x01	/* has not been initialized yet */
#define ADB_ACTION_IDLE		0x02	/* the bus is currently idle */
#define ADB_ACTION_OUT		0x03	/* sending out a command */
#define ADB_ACTION_IN		0x04	/* receiving data */

/*
 * These describe the state of the ADB bus itself, although they
 * don't necessarily correspond directly to ADB states.
 * Note: these are not really used in the IIsi code.
 */
#define ADB_BUS_UNKNOWN		0x01	/* we don't know yet - all models */
#define ADB_BUS_IDLE		0x02	/* bus is idle - all models */
#define ADB_BUS_CMD		0x03	/* starting a command - II models */
#define ADB_BUS_ODD		0x04	/* the "odd" state - II models */
#define ADB_BUS_EVEN		0x05	/* the "even" state - II models */
#define ADB_BUS_ACTIVE		0x06	/* active state - IIsi models */
#define ADB_BUS_ACK		0x07	/* currently ACKing - IIsi models */

/*
 * Shortcuts for setting or testing the VIA bit states.
 * Not all shortcuts are used for every type of ADB hardware.
 */
#define ADB_SET_STATE_IDLE_II()		via_reg(VIA1, vBufB) |= (vPB4 | vPB5)
#define ADB_SET_STATE_IDLE_IISI()	via_reg(VIA1, vBufB) &= ~(vPB4 | vPB5)
#define ADB_SET_STATE_IDLE_CUDA()	via_reg(VIA1, vBufB) |= (vPB4 | vPB5)
#define ADB_SET_STATE_CMD()		via_reg(VIA1, vBufB) &= ~(vPB4 | vPB5)
#define ADB_SET_STATE_EVEN()		via_reg(VIA1, vBufB) = ((via_reg(VIA1, \
						vBufB) | vPB4) & ~vPB5)
#define ADB_SET_STATE_ODD()		via_reg(VIA1, vBufB) = ((via_reg(VIA1, \
						vBufB) | vPB5) & ~vPB4 )
#define ADB_SET_STATE_ACTIVE() 		via_reg(VIA1, vBufB) |= vPB5
#define ADB_SET_STATE_INACTIVE()	via_reg(VIA1, vBufB) &= ~vPB5
#define ADB_SET_STATE_TIP()		via_reg(VIA1, vBufB) &= ~vPB5
#define ADB_CLR_STATE_TIP() 		via_reg(VIA1, vBufB) |= vPB5
#define ADB_SET_STATE_ACKON()		via_reg(VIA1, vBufB) |= vPB4
#define ADB_SET_STATE_ACKOFF()		via_reg(VIA1, vBufB) &= ~vPB4
#define ADB_TOGGLE_STATE_ACK_CUDA()	via_reg(VIA1, vBufB) ^= vPB4
#define ADB_SET_STATE_ACKON_CUDA()	via_reg(VIA1, vBufB) &= ~vPB4
#define ADB_SET_STATE_ACKOFF_CUDA()	via_reg(VIA1, vBufB) |= vPB4
#define ADB_SET_SR_INPUT()		via_reg(VIA1, vACR) &= ~vSR_OUT
#define ADB_SET_SR_OUTPUT()		via_reg(VIA1, vACR) |= vSR_OUT
#define ADB_SR()			via_reg(VIA1, vSR)
#define ADB_VIA_INTR_ENABLE()		via_reg(VIA1, vIER) = 0x84
#define ADB_VIA_INTR_DISABLE()		via_reg(VIA1, vIER) = 0x04
#define ADB_VIA_CLR_INTR()		via_reg(VIA1, vIFR) = 0x04
#define ADB_INTR_IS_OFF			(vPB3 == (via_reg(VIA1, vBufB) & vPB3))
#define ADB_INTR_IS_ON			(0 == (via_reg(VIA1, vBufB) & vPB3))
#define ADB_SR_INTR_IS_OFF		(0 == (via_reg(VIA1, vIFR) & vSR_INT))
#define ADB_SR_INTR_IS_ON		(vSR_INT == (via_reg(VIA1, \
						vIFR) & vSR_INT))

/*
 * This is the delay that is required (in uS) between certain
 * ADB transactions. The actual timing delay for for each uS is
 * calculated at boot time to account for differences in machine speed.
 */
#define ADB_DELAY	150

/*
 * Maximum ADB message length; includes space for data, result, and
 * device code - plus a little for safety.
 */
#define ADB_MAX_MSG_LENGTH	16
#define ADB_MAX_HDR_LENGTH	8

#define ADB_QUEUE		32
#define ADB_TICKLE_TICKS	4

/*
 * A structure for storing information about each ADB device.
 */
struct ADBDevEntry {
        void    (*ServiceRtPtr) __P((void));
        void    *DataAreaAddr;
        char    devType;
        char    origAddr;
        char    currentAddr;
};

/*
 * Used to hold ADB commands that are waiting to be sent out.
 */
struct adbCmdHoldEntry {
	u_char	outBuf[ADB_MAX_MSG_LENGTH];	/* our message */
	u_char	*saveBuf;	/* buffer to know where to save result */
	u_char	*compRout;	/* completion routine pointer */
	u_char	*data;		/* completion routine data pointer */
};

/*
 * Eventually used for two separate queues, the queue between 
 * the upper and lower halves, and the outgoing packet queue.
 * TO DO: adbCommand can replace all of adbCmdHoldEntry eventually
 */
struct adbCommand {
	u_char	header[ADB_MAX_HDR_LENGTH];	/* not used yet */
	u_char	data[ADB_MAX_MSG_LENGTH];	/* packet data only */
	u_char	*saveBuf;	/* where to save result */
	u_char	*compRout;	/* completion routine pointer */
	u_char	*compData;	/* completion routine data pointer */
	u_int	cmd;		/* the original command for this data */
	u_int	unsol;		/* 1 if packet was unsolicited */
	u_int	ack_only;	/* 1 for no special processing */
};

/*
 * A few variables that we need and their initial values.
 */
int	adbHardware = ADB_HW_UNKNOWN;
int	adbActionState = ADB_ACTION_NOTREADY;
int	adbBusState = ADB_BUS_UNKNOWN;
int	adbWaiting = 0;		/* waiting for return data from the device */
int	adbWriteDelay = 0;	/* working on (or waiting to do) a write */
int	adbOutQueueHasData = 0;	/* something in the queue waiting to go out */
int	adbNextEnd = 0;		/* the next incoming bute is the last (II) */
int	adbSoftPower = 0;	/* machine supports soft power */

int	adbWaitingCmd = 0;	/* ADB command we are waiting for */
u_char	*adbBuffer = (long) 0;	/* pointer to user data area */
void	*adbCompRout = (long) 0;	/* pointer to the completion routine */
void	*adbCompData = (long) 0;	/* pointer to the completion routine data */
long	adbFakeInts = 0;	/* keeps track of fake ADB interrupts for
				 * timeouts (II) */
int	adbStarting = 1;	/* doing ADBReInit so do polling differently */
int	adbSendTalk = 0;	/* the intr routine is sending the talk, not
				 * the user (II) */
int	adbPolling = 0;		/* we are polling for service request */
int	adbPollCmd = 0;		/* the last poll command we sent */

u_char	adbInputBuffer[ADB_MAX_MSG_LENGTH];	/* data input buffer */
u_char	adbOutputBuffer[ADB_MAX_MSG_LENGTH];	/* data output buffer */
struct	adbCmdHoldEntry adbOutQueue;		/* our 1 entry output queue */

int	adbSentChars = 0;	/* how many characters we have sent */
int	adbLastDevice = 0;	/* last ADB dev we heard from (II ONLY) */
int	adbLastDevIndex = 0;	/* last ADB dev loc in dev table (II ONLY) */
int	adbLastCommand = 0;	/* the last ADB command we sent (II) */

struct	ADBDevEntry ADBDevTable[16];	/* our ADB device table */
int	ADBNumDevices;		/* num. of ADB devices found with ADBReInit */

struct	adbCommand adbInbound[ADB_QUEUE];	/* incoming queue */
int	adbInCount=0;			/* how many packets in in queue */
int	adbInHead=0;			/* head of in queue */
int	adbInTail=0;			/* tail of in queue */
struct	adbCommand adbOutbound[ADB_QUEUE];	/* outgoing queue - not used yet */
int	adbOutCount=0;			/* how many packets in out queue */
int	adbOutHead=0;			/* head of out queue */
int	adbOutTail=0;			/* tail of out queue */

int	tickle_count=0;			/* how many tickles seen for this packet? */
int	tickle_serial=0;		/* the last packet tickled */
int	adb_cuda_serial=0;		/* the current packet */

extern struct mac68k_machine_S mac68k_machine;

int	zshard __P((int));

void	pm_setup_adb __P((void));
void	pm_check_adb_devices __P((int));
void	pm_intr __P((void));
int	pm_adb_op __P((u_char *, void *, void *, int));
void	pm_init_adb_device __P((void));

/*
 * The following are private routines.
 */
void	print_single __P((u_char *));
void	adb_intr __P((void));
void	adb_intr_II __P((void));
void	adb_intr_IIsi __P((void));
void	adb_intr_cuda __P((void));
void	adb_soft_intr __P((void));
int	send_adb_II __P((u_char *, u_char *, void *, void *, int));
int	send_adb_IIsi __P((u_char *, u_char *, void *, void *, int));
int	send_adb_cuda __P((u_char *, u_char *, void *, void *, int));
void	adb_intr_cuda_test __P((void));
void	adb_cuda_tickle __P((void));
void	adb_pass_up __P((struct adbCommand *));
void	adb_op_comprout __P((void));
void	adb_reinit __P((void));
int	count_adbs __P((void));
int	get_ind_adb_info __P((ADBDataBlock *, int));
int	get_adb_info __P((ADBDataBlock *, int));
int	set_adb_info __P((ADBSetInfoBlock *, int));
void	adb_setup_hw_type __P((void));
int	adb_op __P((Ptr, Ptr, Ptr, short));
int	adb_op_sync __P((Ptr, Ptr, Ptr, short));
void	adb_read_II __P((u_char *));
void	adb_hw_setup __P((void));
void	adb_hw_setup_IIsi __P((u_char *));
void    adb_comp_exec __P((void));
int	adb_cmd_result __P((u_char *));
int	adb_cmd_extra __P((u_char *));
int	adb_guess_next_device __P((void));
int	adb_prog_switch_enable __P((void));
int	adb_prog_switch_disable __P((void));
/* we should create this and it will be the public version */
int	send_adb __P((u_char *, void *, void *));

/*
 * print_single
 * Diagnostic display routine. Displays the hex values of the
 * specified elements of the u_char. The length of the "string"
 * is in [0].
 */
void
print_single(thestring)
	u_char *thestring;
{
	int x;

	if ((int) (thestring[0]) == 0) {
		printf_intr("nothing returned\n");
		return;
	}
	if (thestring == 0) {
		printf_intr("no data - null pointer\n");
		return;
	}
	if (thestring[0] > 20) {
		printf_intr("ADB: ACK > 20 no way!\n");
		thestring[0] = 20;
	}
	printf_intr("(length=0x%x):", thestring[0]);
	for (x = 0; x < thestring[0]; x++)
		printf_intr("  0x%02x", thestring[x + 1]);
	printf_intr("\n");
}

void
adb_cuda_tickle(void)
{
	volatile int s;

	if (adbActionState==ADB_ACTION_IN) {
		if (tickle_serial==adb_cuda_serial) {
			if (++tickle_count>0) {
				s=splhigh();
				adbActionState = ADB_ACTION_IDLE;
				adbInputBuffer[0] = 0;
				ADB_SET_STATE_IDLE_CUDA();
				splx(s);
			}
		} else {
			tickle_serial=adb_cuda_serial;
			tickle_count=0;
		}
	} else {
		tickle_serial=adb_cuda_serial;
		tickle_count=0;
	}

	timeout((void *)adb_cuda_tickle, 0, ADB_TICKLE_TICKS);
}

/*
 * called when when an adb interrupt happens
 *
 * Cuda version of adb_intr
 * TO DO: do we want to add some zshard calls in here?
 */
void
adb_intr_cuda(void)
{
	volatile int i, ending;
	volatile unsigned int s;
	struct adbCommand packet;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */
	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

switch_start:
	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		/* This is an unexpected packet, so grab the first (dummy)
		 * byte, set up the proper vars, and tell the chip we are
		 * starting to receive the packet by setting the TIP bit. */
		adbInputBuffer[1] = ADB_SR();
		adb_cuda_serial++;
		if (ADB_INTR_IS_OFF)	/* must have been a fake start */
			break;

		ADB_SET_SR_INPUT();
		ADB_SET_STATE_TIP();

		adbInputBuffer[0] = 1;
		adbActionState = ADB_ACTION_IN;
#ifdef DEBUG
		printf_intr("idle 0x%02x ", adbInputBuffer[1]);
#endif
		break;

	case ADB_ACTION_IN:
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();
		/* intr off means this is the last byte (end of frame) */
		if (ADB_INTR_IS_OFF)
			ending = 1;
		else
			ending = 0;

		if (1 == ending) {	/* end of message? */
#ifdef DEBUG
			printf_intr("in end 0x%02x ",
			    adbInputBuffer[adbInputBuffer[0]]);
			print_single(adbInputBuffer);
#endif

			/* Are we waiting AND does this packet match what we
			 * are waiting for AND is it coming from either the
			 * ADB or RTC/PRAM sub-device? This section _should_
			 * recognize all ADB and RTC/PRAM type commands, but
			 * there may be more... NOTE: commands are always at
			 * [4], even for RTC/PRAM commands. */
			/* set up data for adb_pass_up */
			for (i=0; i<=adbInputBuffer[0]; i++)
				packet.data[i]=adbInputBuffer[i];
				
			if ((adbWaiting == 1) &&
			    (adbInputBuffer[4] == adbWaitingCmd) &&
			    ((adbInputBuffer[2] == 0x00) ||
			    (adbInputBuffer[2] == 0x01))) {

				packet.saveBuf=adbBuffer;
				packet.compRout=adbCompRout;
				packet.compData=adbCompData;
				packet.unsol=0;
				packet.ack_only=0;
				adb_pass_up(&packet);

				adbWaitingCmd = 0;	/* reset "waiting" vars */
				adbWaiting = 0;
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
			} else {
				packet.unsol=1;
				packet.ack_only=0;
				adb_pass_up(&packet);
			}


			/* reset vars and signal the end of this frame */
			adbActionState = ADB_ACTION_IDLE;
			adbInputBuffer[0] = 0;
			ADB_SET_STATE_IDLE_CUDA();
			/*ADB_SET_SR_INPUT();*/

			/*
			 * If there is something waiting to be sent out,
			 * the set everything up and send the first byte.
			 */
			if (adbWriteDelay == 1) {
				delay(ADB_DELAY);	/* required */
				adbSentChars = 0;
				adbActionState = ADB_ACTION_OUT;
				/*
				 * If the interrupt is on, we were too slow
				 * and the chip has already started to send
				 * something to us, so back out of the write
				 * and start a read cycle.
				 */
				if (ADB_INTR_IS_ON) {
					ADB_SET_SR_INPUT();
					ADB_SET_STATE_IDLE_CUDA();
					adbSentChars = 0;
					adbActionState = ADB_ACTION_IDLE;
					adbInputBuffer[0] = 0;
					break;
				}
				/*
				 * If we got here, it's ok to start sending
				 * so load the first byte and tell the chip
				 * we want to send.
				 */
				ADB_SET_STATE_TIP();
				ADB_SET_SR_OUTPUT();
				ADB_SR() = adbOutputBuffer[adbSentChars + 1];
			}
		} else {
			ADB_TOGGLE_STATE_ACK_CUDA();
#ifdef DEBUG
			printf_intr("in 0x%02x ",
			    adbInputBuffer[adbInputBuffer[0]]);
#endif
		}
		break;

	case ADB_ACTION_OUT:
		i = ADB_SR();	/* reset SR-intr in IFR */
#ifdef DEBUG
		printf_intr("intr out 0x%02x ", i);
#endif

		adbSentChars++;
		if (ADB_INTR_IS_ON) {	/* ADB intr low during write */
#ifdef DEBUG
			printf_intr("intr was on ");
#endif
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
			ADB_SET_STATE_IDLE_CUDA();
			adbSentChars = 0;	/* must start all over */
			adbActionState = ADB_ACTION_IDLE;	/* new state */
			adbInputBuffer[0] = 0;
			adbWriteDelay = 1;	/* must retry when done with
						 * read */
			delay(ADB_DELAY);
			goto switch_start;	/* process next state right
						 * now */
			break;
		}
		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			if (0 == adb_cmd_result(adbOutputBuffer)) {	/* do we expect data
									 * back? */
				adbWaiting = 1;	/* signal waiting for return */
				adbWaitingCmd = adbOutputBuffer[2];	/* save waiting command */
			} else {/* no talk, so done */
                                /* set up stuff for adb_pass_up */
                                for (i=0; i<=adbInputBuffer[0]; i++)
                                        packet.data[i]=adbInputBuffer[i];
                                packet.saveBuf=adbBuffer;
                                packet.compRout=adbCompRout;
                                packet.compData=adbCompData;
                                packet.cmd=adbWaitingCmd;
                                packet.unsol=0;
                                packet.ack_only=1;
                                adb_pass_up(&packet);

                                /* reset "waiting" vars, just in case */
                                adbWaitingCmd = 0;
                                adbBuffer = (long) 0;
                                adbCompRout = (long) 0;
                                adbCompData = (long) 0;
			}

			adbWriteDelay = 0;	/* done writing */
			adbActionState = ADB_ACTION_IDLE;	/* signal bus is idle */
			ADB_SET_SR_INPUT();
			ADB_SET_STATE_IDLE_CUDA();
#ifdef DEBUG
			printf_intr("write done ");
#endif
		} else {
			ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* send next byte */
			ADB_TOGGLE_STATE_ACK_CUDA();	/* signal byte ready to
							 * shift */
#ifdef DEBUG
			printf_intr("toggle ");
#endif
		}
		break;

	case ADB_ACTION_NOTREADY:
		printf_intr("adb: not yet initialized\n");
		break;

	default:
		printf_intr("intr: unknown ADB state\n");
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return;
}				/* end adb_intr_cuda */


int
send_adb_cuda(u_char * in, u_char * buffer, void *compRout, void *data, int
	command)
{
	int i, s, len;

#ifdef DEBUG
	printf_intr("SEND\n");
#endif

	if (adbActionState == ADB_ACTION_NOTREADY)
		return 1;

	s = splhigh();		/* don't interrupt while we are messing with
				 * the ADB */

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* ADB available? */
	    (ADB_INTR_IS_OFF)) {	/* and no incoming interrupt? */
	} else
		if (adbWriteDelay == 0)	/* it's busy, but is anything waiting? */
			adbWriteDelay = 1;	/* if no, then we'll "queue"
						 * it up */
		else {
			splx(s);
			return 1;	/* really busy! */
		}

#ifdef DEBUG
	printf_intr("QUEUE\n");
#endif
	if ((long) in == (long) 0) {	/* need to convert? */
		/* don't need to use adb_cmd_extra here because this section
		 * will be called */
		/* ONLY when it is an ADB command (no RTC or PRAM) */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutputBuffer[0] = 2 + len;	/* dev. type + command + addl.
						 * data */
		adbOutputBuffer[1] = 0x00;	/* mark as an ADB command */
		adbOutputBuffer[2] = (u_char) command;	/* load command */

		for (i = 1; i <= len; i++)	/* copy additional output
						 * data, if any */
			adbOutputBuffer[2 + i] = buffer[i];
	} else
		for (i = 0; i <= (adbOutputBuffer[0] + 1); i++)
			adbOutputBuffer[i] = in[i];

	adbSentChars = 0;	/* nothing sent yet */
	adbBuffer = buffer;	/* save buffer to know where to save result */
	adbCompRout = compRout;	/* save completion routine pointer */
	adbCompData = data;	/* save completion routine data pointer */
	adbWaitingCmd = adbOutputBuffer[2];	/* save wait command */

	if (adbWriteDelay != 1) {	/* start command now? */
#ifdef DEBUG
		printf_intr("out start NOW");
#endif
		delay(ADB_DELAY);
		adbActionState = ADB_ACTION_OUT;	/* set next state */
		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
		ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* load byte for output */
		ADB_SET_STATE_ACKOFF_CUDA();
		ADB_SET_STATE_TIP();	/* tell ADB that we want to send */
	}
	adbWriteDelay = 1;	/* something in the write "queue" */

	splx(s);

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked ? */
		/* poll until byte done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON) {	/* wait for "interrupt" */
				adb_intr_cuda();	/* go process
							 * "interrupt" */
				adb_soft_intr();
				}

	return 0;
}				/* send_adb_cuda */


/* TO DO: add one or two zshard calls in here */
void
adb_intr_II(void)
{
	struct adbCommand packet;
	int i, intr_on = 0;
	int send = 0, do_srq = 0;
	unsigned int s;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */

	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

/*if (ADB_INTR_IS_ON)*/
/*	printf_intr("INTR ON ");*/
	if (ADB_INTR_IS_ON)
		intr_on = 1;	/* save for later */

	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		if (!intr_on) {
			/* printf_intr("FAKE DROPPED \n"); */
			/* printf_intr(" XX "); */
			i = ADB_SR();
			break;
		}
		adbNextEnd = 0;
		/* printf_intr("idle "); */
		adbInputBuffer[0] = 1;
		adbInputBuffer[1] = ADB_SR();	/* get first byte */
		/* printf_intr("0x%02x ", adbInputBuffer[1]); */
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		adbActionState = ADB_ACTION_IN;	/* set next state */
		ADB_SET_STATE_EVEN();	/* set bus state to even */
		adbBusState = ADB_BUS_EVEN;
		break;

	case ADB_ACTION_IN:
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();	/* get byte */
		/* printf_intr("in 0x%02x ",
		 * adbInputBuffer[adbInputBuffer[0]]); */
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */

		/*
		 * Check for an unsolicited Service Request (SRQ).
		 * An empty SRQ packet NEVER ends, so we must manually
		 * check for the following condition.
		 */
		if (adbInputBuffer[0] == 4 && adbInputBuffer[2] == 0xff &&
		    adbInputBuffer[3] == 0xff && adbInputBuffer[4] == 0xff &&
		    intr_on && !adbNextEnd)
			do_srq = 1;

		if (adbNextEnd == 1) {	/* process last byte of packet */
			adbNextEnd = 0;
			/* printf_intr("done: "); */

			/* If the following conditions are true (4 byte
			 * message, last 3 bytes are 0xff) then we basically
			 * got a "no response" from the ADB chip, so change
			 * the message to an empty one. We also clear intr_on
			 * to stop the SRQ send later on because these packets
			 * normally have the SRQ bit set even when there is
			 * NOT a pending SRQ. */
			if (adbInputBuffer[0] == 4 && adbInputBuffer[2] == 0xff &&
			    adbInputBuffer[3] == 0xff && adbInputBuffer[4] == 0xff) {
				/* printf_intr("NO RESP "); */
				intr_on = 0;
				adbInputBuffer[0] = 0;
			}
			adbLastDevice = (adbInputBuffer[1] & 0xf0) >> 4;

                        /* set up data for adb_pass_up */
                        for (i=0; i<=adbInputBuffer[0]; i++)
                                packet.data[i]=adbInputBuffer[i];

			if ((!adbWaiting || adbPolling)
			    && (adbInputBuffer[0] != 0)) {
				packet.unsol=1;
				packet.ack_only=0;
				adb_pass_up(&packet);
			} else
				if (!adbPolling) {	/* someone asked for it */
                                	packet.saveBuf=adbBuffer;
                                	packet.compRout=adbCompRout;
                                	packet.compData=adbCompData;
                                	packet.unsol=0;
                                	packet.ack_only=0;
                                	adb_pass_up(&packet);
				}
			adbWaiting = 0;
			adbPolling = 0;
			adbInputBuffer[0] = 0;
			adbBuffer = (long) 0;
			adbCompRout = (long) 0;
			adbCompData = (long) 0;
			/*
			 * Since we are done, check whether there is any data
			 * waiting to do out. If so, start the sending the data.
			 */
			if (adbOutQueueHasData == 1) {
				/* printf_intr("XXX: DOING OUT QUEUE\n"); */
				/* copy over data */
				for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)
					adbOutputBuffer[i] = adbOutQueue.outBuf[i];
				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				adbPolling = 0;
				send = 1;
				/* if intr_on is true, then it's a SRQ so poll
				 * other devices. */
			} else
				if (intr_on) {
					/* printf_intr("starting POLL "); */
					do_srq = 1;
					adbPolling = 1;
				} else
					if ((adbInputBuffer[1] & 0x0f) != 0x0c) {
						/* printf_intr("xC HACK "); */
						adbPolling = 1;
						send = 1;
						adbOutputBuffer[0] = 1;
						adbOutputBuffer[1] = (adbInputBuffer[1] & 0xf0) | 0x0c;
					} else {
						/* printf_intr("ending "); */
						adbBusState = ADB_BUS_IDLE;
						adbActionState = ADB_ACTION_IDLE;
						ADB_SET_STATE_IDLE_II();
						break;
					}
		}
		/*
		 * If do_srq is true then something above determined that
		 * the message has ended and some device is sending a
		 * service request. So we need to determine the next device
		 * and send a poll to it. (If the device we send to isn't the
		 * one that sent the SRQ, that ok as it will be caught
		 * the next time though.)
		 */
		if (do_srq) {
			/* printf_intr("SRQ! "); */
			adbPolling = 1;
			adb_guess_next_device();
			adbOutputBuffer[0] = 1;
			adbOutputBuffer[1] = ((adbLastDevice & 0x0f) << 4) | 0x0c;
			send = 1;
		}
		/*
		 * If send is true then something above determined that
		 * the message has ended and we need to start sending out
		 * a new message immediately. This could be because there
		 * is data waiting to go out or because an SRQ was seen.
		 */
		if (send) {
			adbNextEnd = 0;
			adbSentChars = 0;	/* nothing sent yet */
			adbActionState = ADB_ACTION_OUT;	/* set next state */
			ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
			ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
			adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
			ADB_SET_STATE_CMD();	/* tell ADB that we want to
						 * send */
			break;
		}
		/* We only get this far if the message hasn't ended yet. */
		if (!intr_on)	/* if adb intr. on then the */
			adbNextEnd = 1;	/* NEXT byte is the last */

		switch (adbBusState) {	/* set to next state */
		case ADB_BUS_EVEN:
			ADB_SET_STATE_ODD();	/* set state to odd */
			adbBusState = ADB_BUS_ODD;
			break;

		case ADB_BUS_ODD:
			ADB_SET_STATE_EVEN();	/* set state to even */
			adbBusState = ADB_BUS_EVEN;
			break;
		default:
			printf_intr("strange state!!!\n");	/* huh? */
			break;
		}
		break;

	case ADB_ACTION_OUT:
		adbNextEnd = 0;
		if (!adbPolling)
			adbWaiting = 1;	/* not unsolicited */
		i = ADB_SR();	/* clear interrupt */
		adbSentChars++;
		/*
		 * If the outgoing data was a TALK, we must
		 * switch to input mode to get the result.
		 */
		if ((adbOutputBuffer[1] & 0x0c) == 0x0c) {
			adbInputBuffer[0] = 1;
			adbInputBuffer[1] = i;
			adbActionState = ADB_ACTION_IN;
			ADB_SET_SR_INPUT();
			adbBusState = ADB_BUS_EVEN;
			ADB_SET_STATE_EVEN();
			/* printf_intr("talk out 0x%02x ", i); */
			break;
		}
		/* If it's not a TALK, check whether all data has been sent.
		 * If so, call the completion routine and clean up. If not,
		 * advance to the next state. */
		/* printf_intr("non-talk out 0x%0x ", i); */
		ADB_SET_SR_OUTPUT();
		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			/* printf_intr("done \n"); */
                        /* set up stuff for adb_pass_up */
                        for (i=0; i<=adbInputBuffer[0]; i++)
                                packet.data[i]=adbInputBuffer[i];
                        packet.saveBuf=adbBuffer;
                        packet.compRout=adbCompRout;
                        packet.compData=adbCompData;
                        packet.cmd=adbWaitingCmd;
                        packet.unsol=0;
                        packet.ack_only=1;
                        adb_pass_up(&packet);

                        /* reset "waiting" vars, just in case */
                        adbWaitingCmd = 0;
                        adbBuffer = (long) 0;
                        adbCompRout = (long) 0;
                        adbCompData = (long) 0;

			if (adbOutQueueHasData == 1) {
				/* copy over data */
				for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)
					adbOutputBuffer[i] = adbOutQueue.outBuf[i];
				adbBuffer = adbOutQueue.saveBuf;	/* user data area */
				adbCompRout = adbOutQueue.compRout;	/* completion routine */
				adbCompData = adbOutQueue.data;	/* comp. rout. data */
				adbOutQueueHasData = 0;	/* currently processing
							 * "queue" entry */
				adbPolling = 0;
			} else {
				adbOutputBuffer[0] = 1;
				adbOutputBuffer[1] = (adbOutputBuffer[1] & 0xf0) | 0x0c;
				adbPolling = 1;	/* non-user poll */
			}
			adbNextEnd = 0;
			adbSentChars = 0;	/* nothing sent yet */
			adbActionState = ADB_ACTION_OUT;	/* set next state */
			ADB_SET_SR_OUTPUT();	/* set shift register for OUT */
			ADB_SR() = adbOutputBuffer[1];	/* load byte for output */
			adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */
			ADB_SET_STATE_CMD();	/* tell ADB that we want to
						 * send */
			break;
		}
		ADB_SR() = adbOutputBuffer[adbSentChars + 1];
		switch (adbBusState) {	/* advance to next state */
		case ADB_BUS_EVEN:
			ADB_SET_STATE_ODD();	/* set state to odd */
			adbBusState = ADB_BUS_ODD;
			break;

		case ADB_BUS_CMD:
		case ADB_BUS_ODD:
			ADB_SET_STATE_EVEN();	/* set state to even */
			adbBusState = ADB_BUS_EVEN;
			break;

		default:
			printf_intr("strange state!!! (0x%x)\n", adbBusState);
			break;
		}
		break;

	default:
		printf_intr("adb: unknown ADB state (during intr)\n");
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return;

}


/*
 * send_adb version for II series machines
 */
int
send_adb_II(u_char * in, u_char * buffer, void *compRout, void *data, int command)
{
	int i, s, len;

	if (adbActionState == ADB_ACTION_NOTREADY)	/* return if ADB not
							 * available */
		return 1;

	s = splhigh();		/* don't interrupt while we are messing with
				 * the ADB */

	if (0 != adbOutQueueHasData) {	/* right now, "has data" means "full" */
		splx(s);	/* sorry, try again later */
		return 1;
	}
	if ((long) in == (long) 0) {	/* need to convert? */
		/*
		 * Don't need to use adb_cmd_extra here because this section
		 * will be called ONLY when it is an ADB command (no RTC or
		 * PRAM), especially on II series!
		 */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutQueue.outBuf[0] = 1 + len;	/* command + addl. data */
		adbOutQueue.outBuf[1] = (u_char) command;	/* load command */

		for (i = 1; i <= len; i++)	/* copy additional output
						 * data, if any */
			adbOutQueue.outBuf[1 + i] = buffer[i];
	} else
		/* if data ready, just copy over */
		for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)
			adbOutQueue.outBuf[i] = in[i];

	adbOutQueue.saveBuf = buffer;	/* save buffer to know where to save
					 * result */
	adbOutQueue.compRout = compRout;	/* save completion routine
						 * pointer */
	adbOutQueue.data = data;/* save completion routine data pointer */

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* is ADB available? */
	    (ADB_INTR_IS_OFF) &&/* and no incoming interrupts? */
	    (adbPolling == 0)) {/* and we are not currently polling */
		/* then start command now */
		for (i = 0; i <= (adbOutQueue.outBuf[0] + 1); i++)	/* copy over data */
			adbOutputBuffer[i] = adbOutQueue.outBuf[i];

		adbBuffer = adbOutQueue.saveBuf;	/* pointer to user data
							 * area */
		adbCompRout = adbOutQueue.compRout;	/* pointer to the
							 * completion routine */
		adbCompData = adbOutQueue.data;	/* pointer to the completion
						 * routine data */

		adbSentChars = 0;	/* nothing sent yet */
		adbActionState = ADB_ACTION_OUT;	/* set next state */
		adbBusState = ADB_BUS_CMD;	/* set bus to cmd state */

		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* load byte for output */
		ADB_SET_STATE_CMD();	/* tell ADB that we want to send */
		adbOutQueueHasData = 0;	/* currently processing "queue" entry */
	} else
		adbOutQueueHasData = 1;	/* something in the write "queue" */

	splx(s);

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked ? */
		/* poll until message done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1) || (adbPolling == 1))
			if (ADB_SR_INTR_IS_ON) {	/* wait for "interrupt" */
				adb_intr_II();	/* go process "interrupt" */
				adb_soft_intr();
			}

	return 0;
}


/*
 * This routine is called from the II series interrupt routine
 * to determine what the "next" device is that should be polled.
 */
int
adb_guess_next_device(void)
{
	int last, i, dummy;

	if (adbStarting) {
		/* start polling EVERY device, since we can't be sure there is
		 * anything in the device table yet */
		if (adbLastDevice < 1 || adbLastDevice > 15)
			adbLastDevice = 1;
		if (++adbLastDevice > 15)	/* point to next one */
			adbLastDevice = 1;
	} else {
		/* find the next device using the device table */
		if (adbLastDevice < 1 || adbLastDevice > 15)	/* let's be parinoid */
			adbLastDevice = 2;
		last = 1;	/* default index location */

		for (i = 1; i < 16; i++)	/* find index entry */
			if (ADBDevTable[i].currentAddr == adbLastDevice) {	/* look for device */
				last = i;	/* found it */
				break;
			}
		dummy = last;	/* index to start at */
		for (;;) {	/* find next device in index */
			if (++dummy > 15)	/* wrap around if needed */
				dummy = 1;
			if (dummy == last) {	/* didn't find any other
						 * device! This can happen if
						 * there are no devices on the
						 * bus */
				dummy = 2;
				break;
			}
			/* found the next device */
			if (ADBDevTable[dummy].devType != 0)
				break;
		}
		adbLastDevice = ADBDevTable[dummy].currentAddr;
	}
	return adbLastDevice;
}


/*
 * Called when when an adb interrupt happens.
 * This routine simply transfers control over to the appropriate
 * code for the machine we are running on.
 */
void
adb_intr(void)
{
	switch (adbHardware) {
	case ADB_HW_II:
		adb_intr_II();
		break;

	case ADB_HW_IISI:
		adb_intr_IIsi();
		break;

	case ADB_HW_PB:
		break;

	case ADB_HW_CUDA:
		adb_intr_cuda();
		break;

	case ADB_HW_UNKNOWN:
		break;
	}
}


/*
 * called when when an adb interrupt happens
 *
 * IIsi version of adb_intr
 *
 */
void
adb_intr_IIsi(void)
{
	struct adbCommand packet;
	int i, ending;
	unsigned int s;

	s = splhigh();		/* can't be too careful - might be called */
	/* from a routine, NOT an interrupt */

	ADB_VIA_CLR_INTR();	/* clear interrupt */

	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */

switch_start:
	switch (adbActionState) {
	case ADB_ACTION_IDLE:
		delay(ADB_DELAY);	/* short delay is required before the
					 * first byte */

		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		ADB_SET_STATE_ACTIVE();	/* signal start of data frame */
		adbInputBuffer[1] = ADB_SR();	/* get byte */
		adbInputBuffer[0] = 1;
		adbActionState = ADB_ACTION_IN;	/* set next state */

		ADB_SET_STATE_ACKON();	/* start ACK to ADB chip */
		delay(ADB_DELAY);	/* delay */
		ADB_SET_STATE_ACKOFF();	/* end ACK to ADB chip */
		zshard(0);	/* grab any serial interrupts */
		break;

	case ADB_ACTION_IN:
		ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
		adbInputBuffer[++adbInputBuffer[0]] = ADB_SR();	/* get byte */
		if (ADB_INTR_IS_OFF)	/* check for end of frame */
			ending = 1;
		else
			ending = 0;

		ADB_SET_STATE_ACKON();	/* start ACK to ADB chip */
		delay(ADB_DELAY);	/* delay */
		ADB_SET_STATE_ACKOFF();	/* end ACK to ADB chip */
		zshard(0);	/* grab any serial interrupts */

		if (1 == ending) {	/* end of message? */
			ADB_SET_STATE_INACTIVE();	/* signal end of frame */
			/* this section _should_ handle all ADB and RTC/PRAM
			 * type commands, */
			/* but there may be more... */
			/* note: commands are always at [4], even for rtc/pram
			 * commands */
			/* set up data for adb_pass_up */
			for (i=0; i<=adbInputBuffer[0]; i++)
				packet.data[i]=adbInputBuffer[i];
				
			if ((adbWaiting == 1) &&	/* are we waiting AND */
			    (adbInputBuffer[4] == adbWaitingCmd) &&	/* the cmd we sent AND */
			    ((adbInputBuffer[2] == 0x00) ||	/* it's from the ADB
								 * device OR */
				(adbInputBuffer[2] == 0x01))) {	/* it's from the
								 * PRAM/RTC device */

				packet.saveBuf=adbBuffer;
				packet.compRout=adbCompRout;
				packet.compData=adbCompData;
				packet.unsol=0;
				packet.ack_only=0;
				adb_pass_up(&packet);

				adbWaitingCmd = 0;	/* reset "waiting" vars */
				adbWaiting = 0;
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
			} else {
				packet.unsol=1;
				packet.ack_only=0;
				adb_pass_up(&packet);
			}

			adbActionState = ADB_ACTION_IDLE;
			adbInputBuffer[0] = 0;	/* reset length */

			if (adbWriteDelay == 1) {	/* were we waiting to
							 * write? */
				adbSentChars = 0;	/* nothing sent yet */
				adbActionState = ADB_ACTION_OUT;	/* set next state */

				delay(ADB_DELAY);	/* delay */
				zshard(0);	/* grab any serial interrupts */

				if (ADB_INTR_IS_ON) {	/* ADB intr low during
							 * write */
					ADB_SET_STATE_IDLE_IISI();	/* reset */
					ADB_SET_SR_INPUT();	/* make sure SR is set
								 * to IN */
					adbSentChars = 0;	/* must start all over */
					adbActionState = ADB_ACTION_IDLE;	/* new state */
					adbInputBuffer[0] = 0;
					/* may be able to take this out later */
					delay(ADB_DELAY);	/* delay */
					break;
				}
				ADB_SET_STATE_ACTIVE();	/* tell ADB that we want
							 * to send */
				ADB_SET_STATE_ACKOFF();	/* make sure */
				ADB_SET_SR_OUTPUT();	/* set shift register
							 * for OUT */
				ADB_SR() = adbOutputBuffer[adbSentChars + 1];
				ADB_SET_STATE_ACKON();	/* tell ADB byte ready
							 * to shift */
			}
		}
		break;

	case ADB_ACTION_OUT:
		i = ADB_SR();	/* reset SR-intr in IFR */
		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		ADB_SET_STATE_ACKOFF();	/* finish ACK */
		adbSentChars++;
		if (ADB_INTR_IS_ON) {	/* ADB intr low during write */
			ADB_SET_STATE_IDLE_IISI();	/* reset */
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
			adbSentChars = 0;	/* must start all over */
			adbActionState = ADB_ACTION_IDLE;	/* new state */
			adbInputBuffer[0] = 0;
			adbWriteDelay = 1;	/* must retry when done with
						 * read */
			delay(ADB_DELAY);	/* delay */
			zshard(0);		/* grab any serial interrupts */
			goto switch_start;	/* process next state right
						 * now */
			break;
		}
		delay(ADB_DELAY);	/* required delay */
		zshard(0);	/* grab any serial interrupts */

		if (adbOutputBuffer[0] == adbSentChars) {	/* check for done */
			if (0 == adb_cmd_result(adbOutputBuffer)) {	/* do we expect data
									 * back? */
				adbWaiting = 1;	/* signal waiting for return */
				adbWaitingCmd = adbOutputBuffer[2];	/* save waiting command */
			} else {/* no talk, so done */
				/* set up stuff for adb_pass_up */
				for (i=0; i<=adbInputBuffer[0]; i++)
					packet.data[i]=adbInputBuffer[i];
				packet.saveBuf=adbBuffer;
				packet.compRout=adbCompRout;
				packet.compData=adbCompData;
				packet.cmd=adbWaitingCmd;
				packet.unsol=0;
				packet.ack_only=1;
				adb_pass_up(&packet);

				/* reset "waiting" vars, just in case */
				adbWaitingCmd = 0;
				adbBuffer = (long) 0;
				adbCompRout = (long) 0;
				adbCompData = (long) 0;
			}

			adbWriteDelay = 0;	/* done writing */
			adbActionState = ADB_ACTION_IDLE;	/* signal bus is idle */
			ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
			ADB_SET_STATE_INACTIVE();	/* end of frame */
		} else {
			ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* send next byte */
			ADB_SET_STATE_ACKON();	/* signal byte ready to shift */
		}
		break;

	case ADB_ACTION_NOTREADY:
		printf_intr("adb: not yet initialized\n");
		break;

	default:
		printf_intr("intr: unknown ADB state\n");
	}

	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */

	splx(s);		/* restore */

	return;
}				/* end adb_intr_IIsi */


/*****************************************************************************
 * if the device is currently busy, and there is no data waiting to go out, then
 * the data is "queued" in the outgoing buffer. If we are already waiting, then
 * we return.
 * in: if (in==0) then the command string is built from command and buffer
 *     if (in!=0) then in is used as the command string
 * buffer: additional data to be sent (used only if in==0)
 *         this is also where return data is stored
 * compRout: the completion routine that is called when then return value
 *	     is received (if a return value is expected)
 * data: a data pointer that can be used by the completion routine
 * command: an ADB command to be sent (used only if in==0)
 *
 */
int
send_adb_IIsi(u_char * in, u_char * buffer, void *compRout, void *data, int
	command)
{
	int i, s, len;

	if (adbActionState == ADB_ACTION_NOTREADY)
		return 1;

	s = splhigh();		/* don't interrupt while we are messing with
				 * the ADB */

	if ((adbActionState == ADB_ACTION_IDLE) &&	/* ADB available? */
	    (ADB_INTR_IS_OFF)) {/* and no incoming interrupt? */

	} else
		if (adbWriteDelay == 0)	/* it's busy, but is anything waiting? */
			adbWriteDelay = 1;	/* if no, then we'll "queue"
						 * it up */
		else {
			splx(s);
			return 1;	/* really busy! */
		}

	if ((long) in == (long) 0) {	/* need to convert? */
		/* don't need to use adb_cmd_extra here because this section
		 * will be called */
		/* ONLY when it is an ADB command (no RTC or PRAM) */
		if ((command & 0x0c) == 0x08)	/* copy addl data ONLY if
						 * doing a listen! */
			len = buffer[0];	/* length of additional data */
		else
			len = 0;/* no additional data */

		adbOutputBuffer[0] = 2 + len;	/* dev. type + command + addl.
						 * data */
		adbOutputBuffer[1] = 0x00;	/* mark as an ADB command */
		adbOutputBuffer[2] = (u_char) command;	/* load command */

		for (i = 1; i <= len; i++)	/* copy additional output
						 * data, if any */
			adbOutputBuffer[2 + i] = buffer[i];
	} else
		for (i = 0; i <= (adbOutputBuffer[0] + 1); i++)
			adbOutputBuffer[i] = in[i];

	adbSentChars = 0;	/* nothing sent yet */
	adbBuffer = buffer;	/* save buffer to know where to save result */
	adbCompRout = compRout;	/* save completion routine pointer */
	adbCompData = data;	/* save completion routine data pointer */
	adbWaitingCmd = adbOutputBuffer[2];	/* save wait command */

	if (adbWriteDelay != 1) {	/* start command now? */
		adbActionState = ADB_ACTION_OUT;	/* set next state */

		ADB_SET_STATE_ACTIVE();	/* tell ADB that we want to send */
		ADB_SET_STATE_ACKOFF();	/* make sure */

		ADB_SET_SR_OUTPUT();	/* set shift register for OUT */

		ADB_SR() = adbOutputBuffer[adbSentChars + 1];	/* load byte for output */

		ADB_SET_STATE_ACKON();	/* tell ADB byte ready to shift */
	}
	adbWriteDelay = 1;	/* something in the write "queue" */

	splx(s);

	if (0x0100 <= (s & 0x0700))	/* were VIA1 interrupts blocked ? */
		/* poll until byte done */
		while ((adbActionState != ADB_ACTION_IDLE) || (ADB_INTR_IS_ON)
		    || (adbWaiting == 1))
			if (ADB_SR_INTR_IS_ON) {	/* wait for "interrupt" */
				adb_intr_IIsi();	/* go process
							 * "interrupt" */
				adb_soft_intr();
			}

	 return 0;
}				/* send_adb_IIsi */


/* 
 * adb_pass_up is called by the interrupt-time routines.
 * It takes the raw packet data that was received from the
 * device and puts it into the queue that the upper half
 * processes. It then signals for a soft ADB interrupt which
 * will eventually call the upper half routine (adb_soft_intr).
 *
 * If in->unsol is 0, then this is either the notification
 * that the packet was sent (on a LISTEN, for example), or the 
 * response from the device (on a TALK). The completion routine
 * is called only if the user specified one.
 *
 * If in->unsol is 1, then this packet was unsolicited and
 * so we look up the device in the ADB device table to determine
 * what it's default service routine is.
 *
 * If in->ack_only is 1, then we really only need to call
 * the completion routine, so don't do any other stuff.
 *
 * Note that in->data contains the packet header AND data,
 * while adbInbound[]->data contains ONLY data.
 *
 * Note: Called only at interrupt time. Assumes this.
 *
 */
void
adb_pass_up(struct adbCommand *in)
{
	int i, start=0, len=0, cmd=0;
	ADBDataBlock block;

	/* temp for testing */
	/*u_char *buffer = 0;*/
	/*u_char *compdata = 0;*/
	/*u_char *comprout = 0;*/

	if (adbInCount>=ADB_QUEUE) {
		printf_intr("adb: ring buffer overflow\n");
		return;
	}

	if (in->ack_only) {
		len=in->data[0];
		cmd=in->cmd;
		start=0;
	} else {
		switch (adbHardware) {
		case ADB_HW_II:
			cmd = in->data[1];
			if (in->data[0] < 2)
				len=0;
			else
				len=in->data[0]-1;
			start=1;
			break;

		case ADB_HW_IISI:
		case ADB_HW_CUDA:
			/* If it's unsolicited, accept only ADB data for now */
			if (in->unsol)
				if (0 != in->data[2])
					return;
			cmd = in->data[4];
			if (in->data[0] < 5)
				len=0;
			else
				len=in->data[0]-4;
			start=4;
			break;

		case ADB_HW_PB:
			return;		/* how does PM handle "unsolicited" messages? */

		case ADB_HW_UNKNOWN:
			return;
		}

		/* Make sure there is a valid device entry for this device */
		if (in->unsol) {
			/* ignore unsolicited data during adbreinit */
			if (adbStarting)
				return;
			/* get device's comp. routine and data area */
			if (-1 == get_adb_info(&block, ((cmd & 0xf0) >> 4)))
				return;
		}
	}

	/*
 	 * If this is an unsolicited packet, we need to fill in
 	 * some info so adb_soft_intr can process this packet
 	 * properly. If it's not unsolicited, then use what
 	 * the caller sent us.
 	 */
	if (in->unsol) {
		adbInbound[adbInTail].compRout=(void *)block.dbServiceRtPtr;
		adbInbound[adbInTail].compData=(void *)block.dbDataAreaAddr;
		adbInbound[adbInTail].saveBuf=(void *)adbInbound[adbInTail].data;
	} else {
		adbInbound[adbInTail].compRout=(void *)in->compRout;
		adbInbound[adbInTail].compData=(void *)in->compData;
		adbInbound[adbInTail].saveBuf=(void *)in->saveBuf;
	}

#if DEBUG
	if (in->data[1] == 2) 
		printf_intr("adb: caught error\n");
#endif

	/* copy the packet data over */
	/* TO DO: If the *_intr routines fed their incoming data
	 * directly into an adbCommand struct, which is passed to 
	 * this routine, then we could eliminate this copy.
	 */
	for (i = 1; i <= len; i++)
		adbInbound[adbInTail].data[i]=in->data[start+i];

	adbInbound[adbInTail].data[0]=len;
	adbInbound[adbInTail].cmd=cmd;

	adbInCount++;
	if (++adbInTail >= ADB_QUEUE)
		adbInTail=0;

	setsoftadb();

	return;
}


/*
 * Called to process the packets after they have been
 * placed in the incoming queue.
 *
 */
void
adb_soft_intr(void)
{
	int s, i;
	int cmd=0;
	u_char *buffer=0;
	u_char *comprout=0;
	u_char *compdata=0;

#if 0
	s=splhigh();
	printf_intr("sr: %x\n", (s & 0x0700));
	splx(s);
#endif

/*delay(2*ADB_DELAY);*/

	while (adbInCount) {
/*printf_intr("%x %x %x ", adbInCount, adbInHead, adbInTail);*/
		/* get the data we need from the queue */
		buffer=adbInbound[adbInHead].saveBuf;
		comprout=adbInbound[adbInHead].compRout;
		compdata=adbInbound[adbInHead].compData;
		cmd=adbInbound[adbInHead].cmd;
	
		/* copy over data to data area if it's valid */
		/* note that for unsol packets we don't want to copy the
	 	* data anywhere, so buffer was already set to 0.
	 	* For ack_only buffer was set to 0, so don't copy. */
		if (buffer)
			for (i = 0; i <= adbInbound[adbInHead].data[0]; i++) 
				*(buffer+i)=adbInbound[adbInHead].data[i];
			
/*printf_intr("%lx %lx %lx %x ", buffer, comprout, compdata, cmd);*/
/*printf_intr("buf: ");*/
/*print_single(adbInbound[adbInHead].data);*/

		/* call default completion routine if it's valid */
		if (comprout) {
#ifdef __NetBSD__
			asm("
		    	movml #0xffff, sp@-		| save all registers
		    	movl %0, a2 		| compdata
		    	movl %1, a1 		| comprout
		    	movl %2, a0 		| buffer
		    	movl %3, d0 		| cmd
		    	jbsr a1@ 			| go call the routine
		    	movml sp@+, #0xffff		| restore all registers"
		    	:
		    	: "g"(compdata),
		      	"g"(comprout), 
		      	"g"(buffer), 
		      	"g"(cmd)
		    	: "d0", "a0", "a1", "a2");
#else					/* for macos based testing */
			asm
			{
				movem.l a0/a1/a2/d0, -(a7)
				move.l compdata, a2
				move.l comprout, a1
				move.l buffer, a0
				move.w cmd, d0
				jsr(a1)
				movem.l(a7)+, d0/a2/a1/a0
			}
#endif
		}

		s=splhigh();
        	adbInCount--;
        	if (++adbInHead >= ADB_QUEUE)
                	adbInHead=0;
		splx(s);

	}
	return;
}


/*
 * This is my version of the ADBOp routine. It mainly just calls the hardware-specific
 * routine.
 *
 *   data 	: pointer to data area to be used by compRout
 *   compRout	: completion routine
 *   buffer	: for LISTEN: points to data to send - MAX 8 data bytes,
 *		  byte 0 = # of bytes
 *		: for TALK: points to place to save return data
 *   command	: the adb command to send
 *   result     : 0 = success
 *              : -1 = could not complete
 */
int
adb_op(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int result;

	switch (adbHardware) {
	case ADB_HW_II:
		result = send_adb_II((u_char *) 0,
		    (u_char *) buffer, (void *) compRout,
		    (void *) data, (int) command);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_IISI:
		result = send_adb_IIsi((u_char *) 0,
		    (u_char *) buffer, (void *) compRout,
		    (void *) data, (int) command);
		/*
		 * I wish I knew why this delay is needed. It usually needs to
		 * be here when several commands are sent in close succession,
		 * especially early in device probes when doing collision
		 * detection. It must be some race condition. Sigh. - jpw
		 */
		delay(100);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_PB:
		result = pm_adb_op((u_char *)buffer, (void *)compRout,
		    (void *)data, (int)command);

		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_CUDA:
		result = send_adb_cuda((u_char *) 0,
		    (u_char *) buffer, (void *) compRout,
		    (void *) data, (int) command);
		if (result == 0)
			return 0;
		else
			return -1;
		break;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}


/*
 * adb_hw_setup
 * This routine sets up the possible machine specific hardware
 * config (mainly VIA settings) for the various models.
 */
void
adb_hw_setup(void)
{
	volatile int i;
	u_char send_string[ADB_MAX_MSG_LENGTH];

	switch (adbHardware) {
	case ADB_HW_II:
		via_reg(VIA1, vDirB) |= 0x30;	/* register B bits 4 and 5:
						 * outputs */
		via_reg(VIA1, vDirB) &= 0xf7;	/* register B bit 3: input */
		via_reg(VIA1, vACR) &= ~vSR_OUT;	/* make sure SR is set
							 * to IN (II, IIsi) */
		adbActionState = ADB_ACTION_IDLE;	/* used by all types of
							 * hardware (II, IIsi) */
		adbBusState = ADB_BUS_IDLE;	/* this var. used in II-series
						 * code only */
		via_reg(VIA1, vIER) = 0x84;	/* make sure VIA interrupts
						 * are on (II, IIsi) */
		ADB_SET_STATE_IDLE_II();	/* set ADB bus state to idle */

		ADB_VIA_CLR_INTR();	/* clear interrupt */
		break;

	case ADB_HW_IISI:
		via_reg(VIA1, vDirB) |= 0x30;	/* register B bits 4 and 5:
						 * outputs */
		via_reg(VIA1, vDirB) &= 0xf7;	/* register B bit 3: input */
		via_reg(VIA1, vACR) &= ~vSR_OUT;	/* make sure SR is set
							 * to IN (II, IIsi) */
		adbActionState = ADB_ACTION_IDLE;	/* used by all types of
							 * hardware (II, IIsi) */
		adbBusState = ADB_BUS_IDLE;	/* this var. used in II-series
						 * code only */
		via_reg(VIA1, vIER) = 0x84;	/* make sure VIA interrupts
						 * are on (II, IIsi) */
		ADB_SET_STATE_IDLE_IISI();	/* set ADB bus state to idle */

		/* get those pesky clock ticks we missed while booting */
		for (i = 0; i < 30; i++) {
			delay(ADB_DELAY);
			adb_hw_setup_IIsi(send_string);
			printf_intr("adb: cleanup: ");
			print_single(send_string);
			delay(ADB_DELAY);
			if (ADB_INTR_IS_OFF)
				break;
		}
		break;

	case ADB_HW_PB:
		/*
		 * XXX -  really PM_VIA_CLR_INTR - should we put it in
		 * pm_direct.h?
		 */
		via_reg(VIA1, vIFR) = 0x90;	/* clear interrupt */
		break;

	case ADB_HW_CUDA:
		via_reg(VIA1, vDirB) |= 0x30;	/* register B bits 4 and 5:
						 * outputs */
		via_reg(VIA1, vDirB) &= 0xf7;	/* register B bit 3: input */
		via_reg(VIA1, vACR) &= ~vSR_OUT;	/* make sure SR is set
							 * to IN */
		via_reg(VIA1, vACR) = (via_reg(VIA1, vACR) | 0x0c) & ~0x10;
		adbActionState = ADB_ACTION_IDLE;	/* used by all types of
							 * hardware */
		adbBusState = ADB_BUS_IDLE;	/* this var. used in II-series
						 * code only */
		via_reg(VIA1, vIER) = 0x84;	/* make sure VIA interrupts
						 * are on */
		ADB_SET_STATE_IDLE_CUDA();	/* set ADB bus state to idle */

		/* sort of a device reset */
		i = ADB_SR();	/* clear interrupt */
		ADB_VIA_INTR_DISABLE();	/* no interrupts while clearing */
		ADB_SET_STATE_IDLE_CUDA();	/* reset state to idle */
		delay(ADB_DELAY);
		ADB_SET_STATE_TIP();	/* signal start of frame */
		delay(ADB_DELAY);
		ADB_TOGGLE_STATE_ACK_CUDA();
		delay(ADB_DELAY);
		ADB_CLR_STATE_TIP();
		delay(ADB_DELAY);
		ADB_SET_STATE_IDLE_CUDA();	/* back to idle state */
		i = ADB_SR();	/* clear interrupt */
		ADB_VIA_INTR_ENABLE();	/* ints ok now */
		break;

	case ADB_HW_UNKNOWN:
	default:
		via_reg(VIA1, vIER) = 0x04;	/* turn interrupts off - TO
						 * DO: turn PB ints off? */
		return;
		break;
	}
}


/*
 * adb_hw_setup_IIsi
 * This is sort of a "read" routine that forces the adb hardware through a read cycle
 * if there is something waiting. This helps "clean up" any commands that may have gotten
 * stuck or stopped during the boot process.
 *
 */
void
adb_hw_setup_IIsi(u_char * buffer)
{
	int i;
	int dummy;
	int s;
	long my_time;
	int endofframe;

	delay(ADB_DELAY);

	i = 1;			/* skip over [0] */
	s = splhigh();		/* block ALL interrupts while we are working */
	ADB_SET_SR_INPUT();	/* make sure SR is set to IN */
	ADB_VIA_INTR_DISABLE();	/* disable ADB interrupt on IIs. */
	/* this is required, especially on faster machines */
	delay(ADB_DELAY);

	if (ADB_INTR_IS_ON) {
		ADB_SET_STATE_ACTIVE();	/* signal start of data frame */

		endofframe = 0;
		while (0 == endofframe) {
			/* poll for ADB interrupt and watch for timeout */
			/* if time out, keep going in hopes of not hanging the
			 * ADB chip - I think */
			my_time = ADB_DELAY * 5;
			while ((ADB_SR_INTR_IS_OFF) && (my_time-- > 0))
				dummy = via_reg(VIA1, vBufB);

			buffer[i++] = ADB_SR();	/* reset interrupt flag by
						 * reading vSR */
			/* perhaps put in a check here that ignores all data
			 * after the first ADB_MAX_MSG_LENGTH bytes ??? */
			if (ADB_INTR_IS_OFF)	/* check for end of frame */
				endofframe = 1;

			ADB_SET_STATE_ACKON();	/* send ACK to ADB chip */
			delay(ADB_DELAY);	/* delay */
			ADB_SET_STATE_ACKOFF();	/* send ACK to ADB chip */
		}
		ADB_SET_STATE_INACTIVE();	/* signal end of frame and
						 * delay */

		/* probably don't need to delay this long */
		delay(ADB_DELAY);
	}
	buffer[0] = --i;	/* [0] is length of message */
	ADB_VIA_INTR_ENABLE();	/* enable ADB interrupt on IIs. */
	splx(s);		/* restore interrupts */

	return;
}				/* adb_hw_setup_IIsi */



/*
 * adb_reinit sets up the adb stuff
 *
 */
void
adb_reinit(void)
{
	u_char send_string[ADB_MAX_MSG_LENGTH];
	int s = 0;
	volatile int i, x;
	int command;
	int result;
	int saveptr;		/* point to next free relocation address */
	int device;
	int nonewtimes;		/* times thru loop w/o any new devices */
	ADBDataBlock data;	/* temp. holder for getting device info */

	(void)(&s);		/* work around lame GCC bug */

	/* Make sure we are not interrupted while building the table. */
	if (adbHardware != ADB_HW_PB)	/* ints must be on for PB? */
		s = splhigh();

	ADBNumDevices = 0;	/* no devices yet */

	/* Let intr routines know we are running reinit */
	adbStarting = 1;

	/* Initialize the ADB table.  For now, we'll always use the same table
	 * that is defined at the beginning of this file - no mallocs. */
	for (i = 0; i < 16; i++)
		ADBDevTable[i].devType = 0;

	adb_setup_hw_type();	/* setup hardware type */

	adb_hw_setup();		/* init the VIA bits and hard reset ADB */

	/* send an ADB reset first */
	adb_op_sync((Ptr) 0, (Ptr) 0, (Ptr) 0, (short) 0x00);

	/* Probe for ADB devices. Probe devices 1-15 quickly to determine
	 * which device addresses are in use and which are free. For each
	 * address that is in use, move the device at that address to a higher
	 * free address. Continue doing this at that address until no device
	 * responds at that address. Then move the last device that was moved
	 * back to the original address. Do this for the remaining addresses
	 * that we determined were in use.
	 * 
	 * When finished, do this entire process over again with the updated list
	 * of in use addresses. Do this until no new devices have been found
	 * in 20 passes though the in use address list. (This probably seems
	 * long and complicated, but it's the best way to detect multiple
	 * devices at the same address - sometimes it takes a couple of tries
	 * before the collision is detected.) */

	/* initial scan through the devices */
	for (i = 1; i < 16; i++) {
		command = (int) (0x0f | ((int) (i & 0x000f) << 4));	/* talk R3 */
		result = adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);
		if (0x00 != send_string[0]) {	/* anything come back ?? */
			ADBDevTable[++ADBNumDevices].devType = (u_char) send_string[2];
			ADBDevTable[ADBNumDevices].origAddr = i;
			ADBDevTable[ADBNumDevices].currentAddr = i;
			ADBDevTable[ADBNumDevices].DataAreaAddr = (long) 0;
			ADBDevTable[ADBNumDevices].ServiceRtPtr = (void *) 0;
			pm_check_adb_devices(i);	/* tell pm driver device
							 * is here */
		}
	}

	/* find highest unused address */
	for (saveptr = 15; saveptr > 0; saveptr--)
		if (-1 == get_adb_info(&data, saveptr))
			break;

	if (saveptr == 0)	/* no free addresses??? */
		saveptr = 15;

	/* printf_intr("first free is: 0x%02x\n", saveptr); */
	/* printf_intr("devices: %i\n", ADBNumDevices); */

	nonewtimes = 0;		/* no loops w/o new devices */
	while (nonewtimes++ < 11) {
		for (i = 1; i <= ADBNumDevices; i++) {
			device = ADBDevTable[i].currentAddr;
			/* printf_intr("moving device 0x%02x to 0x%02x (index
			 * 0x%02x)  ", device, saveptr, i); */

			/* send TALK R3 to address */
			command = (int) (0x0f | ((int) (device & 0x000f) << 4));
			adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);

			/* move device to higher address */
			command = (int) (0x0b | ((int) (device & 0x000f) << 4));
			send_string[0] = 2;
			send_string[1] = (u_char) (saveptr | 0x60);
			send_string[2] = 0xfe;
			adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);

			/* send TALK R3 - anything at old address? */
			command = (int) (0x0f | ((int) (device & 0x000f) << 4));
			result = adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);
			if (send_string[0] != 0) {
				/* new device found */
				/* update data for previously moved device */
				ADBDevTable[i].currentAddr = saveptr;
				/* printf_intr("old device at index %i\n",i); */
				/* add new device in table */
				/* printf_intr("new device found\n"); */
				ADBDevTable[++ADBNumDevices].devType = (u_char) send_string[2];
				ADBDevTable[ADBNumDevices].origAddr = device;
				ADBDevTable[ADBNumDevices].currentAddr = device;
				/* These will be set correctly in adbsys.c */
				/* Until then, unsol. data will be ignored. */
				ADBDevTable[ADBNumDevices].DataAreaAddr = (long) 0;
				ADBDevTable[ADBNumDevices].ServiceRtPtr = (void *) 0;
				/* find next unused address */
				for (x = saveptr; x > 0; x--)
					if (-1 == get_adb_info(&data, x)) {
						saveptr = x;
						break;
					}
				/* printf_intr("new free is 0x%02x\n",
				 * saveptr); */
				nonewtimes = 0;
				/* tell pm driver device is here */
				pm_check_adb_devices(device);
			} else {
				/* printf_intr("moving back...\n"); */
				/* move old device back */
				command = (int) (0x0b | ((int) (saveptr & 0x000f) << 4));
				send_string[0] = 2;
				send_string[1] = (u_char) (device | 0x60);
				send_string[2] = 0xfe;
				adb_op_sync((Ptr) send_string, (Ptr) 0, (Ptr) 0, (short) command);
			}
		}
	}

#ifdef DEBUG
	for (i = 1; i <= ADBNumDevices; i++) {
		x = get_ind_adb_info(&data, i);
		if (x != -1)
			printf_intr("index 0x%x, addr 0x%x, type 0x%x\n", i, x, data.devType);

	}
#endif

	adb_prog_switch_enable();	/* enable the programmer's switch, if
					 * we have one */

	if (0 == ADBNumDevices)	/* tell user if no devices found */
		printf_intr("adb: no devices found\n");

	adbStarting = 0;	/* not starting anymore */
	printf_intr("adb: ADBReInit complete\n");

	if (adbHardware==ADB_HW_CUDA)
		timeout((void *)adb_cuda_tickle, 0, ADB_TICKLE_TICKS);

	if (adbHardware != ADB_HW_PB)	/* ints must be on for PB? */
		splx(s);
	return;
}


/*
 * adb_comp_exec
 * This is a general routine that calls the completion routine if there is one.
 * NOTE: This routine is now only used by pm_direct.c
 *       All the code in this file (adb_direct.c) uses 
 *       the adb_pass_up routine now.
 */
void
adb_comp_exec(void)
{
        if ((long) 0 != adbCompRout)    /* don't call if empty return location */
#ifdef __NetBSD__
                asm("
                    movml #0xffff, sp@-         | save all registers
                    movl %0, a2                 | adbCompData
                    movl %1, a1                 | adbCompRout
                    movl %2, a0                 | adbBuffer
                    movl %3, d0                 | adbWaitingCmd
                    jbsr a1@                    | go call the routine
                    movml sp@+, #0xffff         | restore all registers"
                    :
                    :"g"(adbCompData), "g"(adbCompRout),
                     "g"(adbBuffer), "g"(adbWaitingCmd)
                    :"d0", "a0", "a1", "a2");
#else                                   /* for macos based testing */
                asm {
                        movem.l a0/a1/a2/d0, -(a7)
                        move.l adbCompData, a2
                        move.l adbCompRout, a1
                        move.l adbBuffer, a0
                        move.w adbWaitingCmd, d0
                        jsr(a1)
                        movem.l(a7) +, d0/a2/a1/a0
                }
#endif
}


/* adb_cmd_result
 * This routine lets the caller know whether the specified adb command string should
 * expect a returned result, such as a TALK command.
 * returns: 0 if a result should be expected
 *          1 if a result should NOT be expected
 */
int
adb_cmd_result(u_char * in)
{
	switch (adbHardware) {
		case ADB_HW_II:
		/* was it an ADB talk command? */
		if ((in[1] & 0x0c) == 0x0c)
			return 0;
		else
			return 1;
		break;

	case ADB_HW_IISI:
	case ADB_HW_CUDA:
		/* was is an ADB talk command? */
		if ((in[1] == 0x00) && ((in[2] & 0x0c) == 0x0c))
			return 0;
		/* was is an RTC/PRAM read date/time? */
		else
			if ((in[1] == 0x01) && (in[2] == 0x03))
				return 0;
			else
				return 1;
		break;

	case ADB_HW_PB:
		return 1;
		break;

	case ADB_HW_UNKNOWN:
	default:
		return 1;
	}
}


/* adb_cmd_extra
 * This routine lets the caller know whether the specified adb command string may have
 * extra data appended to the end of it, such as a LISTEN command.
 * returns: 0 if extra data is allowed
 *          1 if extra data is NOT allowed
 */
int
adb_cmd_extra(u_char * in)
{
	switch (adbHardware) {
		case ADB_HW_II:
		if ((in[1] & 0x0c) == 0x08)	/* was it a listen command? */
			return 0;
		else
			return 1;
		break;

	case ADB_HW_IISI:
	case ADB_HW_CUDA:
		/* TO DO: support needs to be added to recognize RTC and PRAM
		 * commands */
		if ((in[2] & 0x0c) == 0x08)	/* was it a listen command? */
			return 0;
		else		/* add others later */
			return 1;
		break;

	case ADB_HW_PB:
		return 1;
		break;

	case ADB_HW_UNKNOWN:
	default:
		return 1;
	}
}


/* adb_op_sync
 * This routine does exactly what the adb_op routine does, except that after the
 * adb_op is called, it waits until the return value is present before returning.
 * NOTE: The user specified compRout is ignored, since this routine specifies
 * it's own to adb_op, which is why you really called this in the first place
 * anyway.
 */
int
adb_op_sync(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int result;
	volatile int flag = 0;

	result = adb_op(buffer, (void *) adb_op_comprout,
	    (void *) &flag, command);	/* send command */
	if (result == 0) {	/* send ok? */
		while (0 == flag);	/* wait for compl. routine */
		return 0;
	} else
		return result;
}


/* adb_op_comprout
 * This function is used by the adb_op_sync routine so it knows when the function is
 * done.
 */
void 
adb_op_comprout(void)
{
#ifdef __NetBSD__
	asm("movw	#1,a2@			| update flag value");
#else				/* for macos based testing */
	asm {
		move.w #1,(a2) }		/* update flag value */
#endif
}

void 
adb_setup_hw_type(void)
{
	long response;

	response = mac68k_machine.machineid;

	/*
	 * Determine what type of ADB hardware we are running on.
	 */
	switch (response) {
	case 6:		/* II */
	case 7:		/* IIx */
	case 8:		/* IIcx */
	case 9:		/* SE/30 */
	case 11:	/* IIci */
	case 22:	/* Quadra 700 */
	case 30:	/* Centris 650 */
	case 35:	/* Quadra 800 */
	case 36:	/* Quadra 650 */
	case 52:	/* Centris 610 */
	case 53:	/* Quadra 610 */
		adbHardware = ADB_HW_II;
		printf_intr("adb: using II series hardware support\n");
		break;
	case 18:	/* IIsi */
	case 20:	/* Quadra 900 - not sure if IIsi or not */
	case 23:	/* Classic II */
	case 26:	/* Quadra 950 - not sure if IIsi or not */
	case 27:	/* LC III, Performa 450 */
	case 37:	/* LC II, Performa 400/405/430 */
	case 44:	/* IIvi */
	case 45:	/* Performa 600 */
	case 48:	/* IIvx */
	case 49:	/* Color Classic - not sure if IIsi or not */
	case 62:	/* Performa 460/465/467 */
	case 83:	/* Color Classic II - not sure if IIsi or not */
		adbHardware = ADB_HW_IISI;
		printf_intr("adb: using IIsi series hardware support\n");
		break;
	case 21:	/* PowerBook 170 */
	case 25:	/* PowerBook 140 */
	case 54:	/* PowerBook 145 */
	case 34:	/* PowerBook 160 */
	case 84:	/* PowerBook 165 */
	case 50:	/* PowerBook 165c */
	case 33:	/* PowerBook 180 */
	case 71:	/* PowerBook 180c */
	case 115:	/* PowerBook 150 */
		adbHardware = ADB_HW_PB;
		pm_setup_adb();
		printf_intr("adb: using PowerBook 100-series hardware support\n");
		break;
	case 29:	/* PowerBook Duo 210 */
	case 32:	/* PowerBook Duo 230 */
	case 38:	/* PowerBook Duo 250 */
	case 72:	/* PowerBook 500 series */
	case 77:	/* PowerBook Duo 270 */
	case 102:	/* PowerBook Duo 280 */
	case 103:	/* PowerBook Duo 280c */
		adbHardware = ADB_HW_PB;
		pm_setup_adb();
		printf_intr("adb: using PowerBook Duo-series and PowerBook 500-series hardware support\n");
		break;
	case 56:	/* LC 520 */
	case 60:	/* Centris 660AV */
	case 78:	/* Quadra 840AV */
	case 80:	/* LC 550, Performa 550 */
	case 89:	/* LC 475, Performa 475/476 */
	case 92:	/* LC 575, Performa 575/577/578 */
	case 94:	/* Quadra 605 */
	case 98:	/* LC 630, Performa 630, Quadra 630 */
		adbHardware = ADB_HW_CUDA;
		printf_intr("adb: using Cuda series hardware support\n");
		break;
	default:
		adbHardware = ADB_HW_UNKNOWN;
		printf_intr("adb: hardware type unknown for this machine\n");
		printf_intr("adb: ADB support is disabled\n");
		break;
	}

	/*
	 * Determine whether this machine has ADB based soft power.
	 */
	switch (response) {
	case 18:	/* IIsi */
	case 20:	/* Quadra 900 - not sure if IIsi or not */
	case 26:	/* Quadra 950 - not sure if IIsi or not */
	case 44:	/* IIvi */
	case 45:	/* Performa 600 */
	case 48:	/* IIvx */
	case 49:	/* Color Classic - not sure if IIsi or not */
	case 83:	/* Color Classic II - not sure if IIsi or not */
	case 56:	/* LC 520 */
	case 78:	/* Quadra 840AV */
	case 80:	/* LC 550, Performa 550 */
	case 92:	/* LC 575, Performa 575/577/578 */
	case 98:	/* LC 630, Performa 630, Quadra 630 */
		adbSoftPower=1;
		break;
	}
}
	
int 
count_adbs(void)
{
	int i;
	int found;

	found = 0;

	for (i = 1; i < 16; i++)
		if (0 != ADBDevTable[i].devType)
			found++;

	return found;
}

int 
get_ind_adb_info(ADBDataBlock * info, int index)
{
	if ((index < 1) || (index > 15))	/* check range 1-15 */
		return (-1);

	/* printf_intr("index 0x%x devType is: 0x%x\n", index,
	    ADBDevTable[index].devType); */
	if (0 == ADBDevTable[index].devType)	/* make sure it's a valid entry */
		return (-1);

	info->devType = ADBDevTable[index].devType;
	info->origADBAddr = ADBDevTable[index].origAddr;
	info->dbServiceRtPtr = (Ptr) ADBDevTable[index].ServiceRtPtr;
	info->dbDataAreaAddr = (Ptr) ADBDevTable[index].DataAreaAddr;

	return (ADBDevTable[index].currentAddr);
}

int 
get_adb_info(ADBDataBlock * info, int adbAddr)
{
	int i;

	if ((adbAddr < 1) || (adbAddr > 15))	/* check range 1-15 */
		return (-1);

	for (i = 1; i < 15; i++)
		if (ADBDevTable[i].currentAddr == adbAddr) {
			info->devType = ADBDevTable[i].devType;
			info->origADBAddr = ADBDevTable[i].origAddr;
			info->dbServiceRtPtr = (Ptr)ADBDevTable[i].ServiceRtPtr;
			info->dbDataAreaAddr = ADBDevTable[i].DataAreaAddr;
			return 0;	/* found */
		}

	return (-1);		/* not found */
}

int 
set_adb_info(ADBSetInfoBlock * info, int adbAddr)
{
	int i;

	if ((adbAddr < 1) || (adbAddr > 15))	/* check range 1-15 */
		return (-1);

	for (i = 1; i < 15; i++)
		if (ADBDevTable[i].currentAddr == adbAddr) {
			ADBDevTable[i].ServiceRtPtr =
			    (void *)(info->siServiceRtPtr);
			ADBDevTable[i].DataAreaAddr = info->siDataAreaAddr;
			return 0;	/* found */
		}

	return (-1);		/* not found */

}

#ifndef MRG_ADB
long
mrg_adbintr(void)
{
	adb_intr();
	return 1;	/* mimic mrg_adbintr in macrom.h just in case */
}

long
mrg_pmintr(void)
{
	pm_intr();
	return 1;	/* mimic mrg_pmintr in macrom.h just in case */
}
#endif

/* caller should really use machine-independant version: getPramTime */
/* this version does pseudo-adb access only */
int 
adb_read_date_time(unsigned long *time)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_II:
		return -1;

	case ADB_HW_IISI:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x03;	/* read date/time */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) output, (void *) adb_op_comprout,
		    (int *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for result */
			;

		*time = (long) (*(long *) (output + 1));
		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x03;	/* read date/time */
		result = send_adb_cuda((u_char *) output,
		    (u_char *) output, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for result */
			;

		*time = (long) (*(long *) (output + 1));
		return 0;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

/* caller should really use machine-independant version: setPramTime */
/* this version does pseudo-adb access only */
int 
adb_set_date_time(unsigned long time)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_II:
		return -1;

	case ADB_HW_IISI:
		output[0] = 0x06;	/* 6 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x09;	/* set date/time */
		output[3] = (u_char) (time >> 24);
		output[4] = (u_char) (time >> 16);
		output[5] = (u_char) (time >> 8);
		output[6] = (u_char) (time);
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x06;	/* 6 byte message */
		output[1] = 0x01;	/* to pram/rtc device */
		output[2] = 0x09;	/* set date/time */
		output[3] = (u_char) (time >> 24);
		output[4] = (u_char) (time >> 16);
		output[5] = (u_char) (time >> 8);
		output[6] = (u_char) (time);
		result = send_adb_cuda((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}


int 
adb_poweroff(void)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;

	if (!adbSoftPower)
		return -1;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x0a;	/* set date/time */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) 0, (void *) 0, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		for (;;);		/* wait for power off */

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_CUDA:
		output[0] = 0x02;	/* 2 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x0a;	/* set date/time */
		result = send_adb_cuda((u_char *) output,
		    (u_char *) 0, (void *) 0, (void *) 0, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		for (;;);		/* wait for power off */

		return 0;

	case ADB_HW_II:			/* II models don't do ADB soft power */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

int 
adb_prog_switch_enable(void)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x03;	/* 3 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x1c;	/* prog. switch control */
		output[3] = 0x01;	/* enable */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_II:		/* II models don't do prog. switch */
	case ADB_HW_CUDA:	/* cuda doesn't do prog. switch TO DO: verify this */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

int 
adb_prog_switch_disable(void)
{
	u_char output[ADB_MAX_MSG_LENGTH];
	int result;
	volatile int flag = 0;

	switch (adbHardware) {
	case ADB_HW_IISI:
		output[0] = 0x03;	/* 3 byte message */
		output[1] = 0x01;	/* to pram/rtc/soft-power device */
		output[2] = 0x1c;	/* prog. switch control */
		output[3] = 0x01;	/* disable */
		result = send_adb_IIsi((u_char *) output,
		    (u_char *) 0, (void *) adb_op_comprout,
		    (void *) &flag, (int) 0);
		if (result != 0)	/* exit if not sent */
			return -1;

		while (0 == flag)	/* wait for send to finish */
			;

		return 0;

	case ADB_HW_PB:
		return -1;

	case ADB_HW_II:		/* II models don't do prog. switch */
	case ADB_HW_CUDA:	/* cuda doesn't do prog. switch */
	case ADB_HW_UNKNOWN:
	default:
		return -1;
	}
}

#ifndef MRG_ADB

int 
CountADBs(void)
{
	return (count_adbs());
}

void 
ADBReInit(void)
{
	adb_reinit();
}

int 
GetIndADB(ADBDataBlock * info, int index)
{
	return (get_ind_adb_info(info, index));
}

int 
GetADBInfo(ADBDataBlock * info, int adbAddr)
{
	return (get_adb_info(info, adbAddr));
}

int 
SetADBInfo(ADBSetInfoBlock * info, int adbAddr)
{
	return (set_adb_info(info, adbAddr));
}

int 
ADBOp(Ptr buffer, Ptr compRout, Ptr data, short commandNum)
{
	return (adb_op(buffer, compRout, data, commandNum));
}

#endif

