/*	$NetBSD: ncr53c9xreg.h,v 1.1.2.2 1997/03/12 21:22:50 is Exp $	*/

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
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

/*
 * Register addresses, relative to some base address
 */

#define	NCR_TCL		0x00		/* RW - Transfer Count Low	*/
#define	NCR_TCM		0x01		/* RW - Transfer Count Mid	*/
#define	NCR_TCH		0x0e		/* RW - Transfer Count High	*/
					/*	NOT on 53C90		*/

#define	NCR_FIFO	0x02		/* RW - FIFO data		*/

#define	NCR_CMD		0x03		/* RW - Command (2 deep)	*/
#define  NCRCMD_DMA	0x80		/*	DMA Bit			*/
#define  NCRCMD_NOP	0x00		/*	No Operation		*/
#define  NCRCMD_FLUSH	0x01		/*	Flush FIFO		*/
#define  NCRCMD_RSTCHIP	0x02		/*	Reset Chip		*/
#define  NCRCMD_RSTSCSI	0x03		/*	Reset SCSI Bus		*/
#define  NCRCMD_RESEL	0x40		/*	Reselect Sequence	*/
#define  NCRCMD_SELNATN	0x41		/*	Select without ATN	*/
#define  NCRCMD_SELATN	0x42		/*	Select with ATN		*/
#define  NCRCMD_SELATNS	0x43		/*	Select with ATN & Stop	*/
#define  NCRCMD_ENSEL	0x44		/*	Enable (Re)Selection	*/
#define  NCRCMD_DISSEL	0x45		/*	Disable (Re)Selection	*/
#define  NCRCMD_SELATN3	0x46		/*	Select with ATN3	*/
#define  NCRCMD_RESEL3	0x47		/*	Reselect3 Sequence	*/
#define  NCRCMD_SNDMSG	0x20		/*	Send Message		*/
#define  NCRCMD_SNDSTAT	0x21		/*	Send Status		*/
#define  NCRCMD_SNDDATA	0x22		/*	Send Data		*/
#define  NCRCMD_DISCSEQ	0x23		/*	Disconnect Sequence	*/
#define  NCRCMD_TERMSEQ	0x24		/*	Terminate Sequence	*/
#define  NCRCMD_TCCS	0x25		/*	Target Command Comp Seq	*/
#define  NCRCMD_DISC	0x27		/*	Disconnect		*/
#define  NCRCMD_RECMSG	0x28		/*	Receive Message		*/
#define  NCRCMD_RECCMD	0x29		/*	Receive Command 	*/
#define  NCRCMD_RECDATA	0x2a		/*	Receive Data		*/
#define  NCRCMD_RECCSEQ	0x2b		/*	Receive Command Sequence*/
#define  NCRCMD_ABORT	0x04		/*	Target Abort DMA	*/
#define  NCRCMD_TRANS	0x10		/*	Transfer Information	*/
#define  NCRCMD_ICCS	0x11		/*	Initiator Cmd Comp Seq 	*/
#define  NCRCMD_MSGOK	0x12		/*	Message Accepted	*/
#define  NCRCMD_TRPAD	0x18		/*	Transfer Pad		*/
#define  NCRCMD_SETATN	0x1a		/*	Set ATN			*/
#define  NCRCMD_RSTATN	0x1b		/*	Reset ATN		*/

#define	NCR_STAT	0x04		/* RO - Status			*/
#define  NCRSTAT_INT	0x80		/*	Interrupt		*/
#define  NCRSTAT_GE	0x40		/*	Gross Error		*/
#define  NCRSTAT_PE	0x20		/*	Parity Error		*/
#define  NCRSTAT_TC	0x10		/*	Terminal Count		*/
#define  NCRSTAT_VGC	0x08		/*	Valid Group Code	*/
#define  NCRSTAT_PHASE	0x07		/*	Phase bits		*/

#define	NCR_SELID	0x04		/* WO - Select/Reselect Bus ID	*/

#define	NCR_INTR	0x05		/* RO - Interrupt		*/
#define  NCRINTR_SBR	0x80		/*	SCSI Bus Reset		*/
#define  NCRINTR_ILL	0x40		/*	Illegal Command		*/
#define  NCRINTR_DIS	0x20		/*	Disconnect		*/
#define  NCRINTR_BS	0x10		/*	Bus Service		*/
#define  NCRINTR_FC	0x08		/*	Function Complete	*/
#define  NCRINTR_RESEL	0x04		/*	Reselected		*/
#define  NCRINTR_SELATN	0x02		/*	Select with ATN		*/
#define  NCRINTR_SEL	0x01		/*	Selected		*/

#define	NCR_TIMEOUT	0x05		/* WO - Select/Reselect Timeout */

#define	NCR_STEP	0x06		/* RO - Sequence Step		*/
#define  NCRSTEP_MASK	0x07		/*	the last 3 bits		*/
#define  NCRSTEP_DONE	0x04		/*	command went out	*/

#define	NCR_SYNCTP	0x06		/* WO - Synch Transfer Period	*/
					/*	Default 5 (53C9X)	*/

#define	NCR_FFLAG	0x07		/* RO - FIFO Flags		*/
#define  NCRFIFO_SS	0xe0		/*	Sequence Step (Dup)	*/
#define  NCRFIFO_FF	0x1f		/*	Bytes in FIFO		*/

#define	NCR_SYNCOFF	0x07		/* WO - Synch Offset		*/
					/*	0 = ASYNC		*/
					/*	1 - 15 = SYNC bytes	*/

#define	NCR_CFG1	0x08		/* RW - Configuration #1	*/
#define  NCRCFG1_SLOW	0x80		/*	Slow Cable Mode		*/
#define  NCRCFG1_SRR	0x40		/*	SCSI Reset Rep Int Dis	*/
#define  NCRCFG1_PTEST	0x20		/*	Parity Test Mod		*/
#define  NCRCFG1_PARENB	0x10		/*	Enable Parity Check	*/
#define  NCRCFG1_CTEST	0x08		/*	Enable Chip Test	*/
#define  NCRCFG1_BUSID	0x07		/*	Bus ID			*/

#define	NCR_CCF		0x09		/* WO -	Clock Conversion Factor	*/
					/*	0 = 35.01 - 40Mhz	*/
					/*	NEVER SET TO 1		*/
					/*	2 = 10Mhz		*/
					/*	3 = 10.01 - 15Mhz	*/
					/*	4 = 15.01 - 20Mhz	*/
					/*	5 = 20.01 - 25Mhz	*/
					/*	6 = 25.01 - 30Mhz	*/
					/*	7 = 30.01 - 35Mhz	*/

#define	NCR_TEST	0x0a		/* WO - Test (Chip Test Only)	*/

#define	NCR_CFG2	0x0b		/* RW - Configuration #2	*/
#define	 NCRCFG2_RSVD	0xa0		/*	reserved		*/
#define  NCRCFG2_FE	0x40		/* 	Features Enable		*/
#define  NCRCFG2_DREQ	0x10		/* 	DREQ High Impedance	*/
#define  NCRCFG2_SCSI2	0x08		/* 	SCSI-2 Enable		*/
#define  NCRCFG2_BPA	0x04		/* 	Target Bad Parity Abort	*/
#define  NCRCFG2_RPE	0x02		/* 	Register Parity Error	*/
#define  NCRCFG2_DPE	0x01		/* 	DMA Parity Error	*/

/* Config #3 only on 53C9X */
#define	NCR_CFG3	0x0c		/* RW - Configuration #3	*/
#define	 NCRCFG3_RSVD	0xe0		/*	reserved		*/
#define  NCRCFG3_IDM	0x10		/*	ID Message Res Check	*/
#define  NCRCFG3_QTE	0x08		/*	Queue Tag Enable	*/
#define  NCRCFG3_CDB	0x04		/*	CDB 10-bytes OK		*/
#define  NCRCFG3_FSCSI	0x02		/*	Fast SCSI		*/
#define  NCRCFG3_FCLK	0x01		/*	Fast Clock (>25Mhz)	*/
