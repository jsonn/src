/*	$NetBSD: scsi_base.c,v 1.47.2.5 1997/10/14 10:25:01 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1997 Charles M. Hannum.  All rights reserved.
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

/*
 * Originally written by Julian Elischer (julian@dialix.oz.au)
 */

#include "opt_scsiverbose.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsipi_base.h>

#ifdef SCSIVERBOSE
static	void asc2ascii __P((unsigned char, unsigned char, char *));
char	*scsi_decode_sense __P((void *, int));
#endif

/*
 * Do a scsi operation, asking a device to run as SCSI-II if it can.
 */
int
scsi_change_def(sc_link, flags)
	struct scsipi_link *sc_link;
	int flags;
{
	struct scsi_changedef scsipi_cmd;

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = SCSI_CHANGE_DEFINITION;
	scsipi_cmd.how = SC_SCSI_2;

	return ((*sc_link->scsipi_cmd)(sc_link,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, 100000, NULL, flags));
}

/*
 * ask the scsi driver to perform a command for us.
 * tell it where to read/write the data, and how
 * long the data is supposed to be. If we have  a buf
 * to associate with the transfer, we need that too.
 */
int
scsi_scsipi_cmd(sc_link, scsipi_cmd, cmdlen, data_addr, datalen,
	retries, timeout, bp, flags)
	struct scsipi_link *sc_link;
	struct scsipi_generic *scsipi_cmd;
	int cmdlen;
	u_char *data_addr;
	int datalen;
	int retries;
	int timeout;
	struct buf *bp;
	int flags;
{
	struct scsipi_xfer *xs;
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_scsipi_cmd\n"));

#ifdef DIAGNOSTIC
	if (bp != 0 && (flags & SCSI_NOSLEEP) == 0)
		panic("scsi_scsipi_cmd: buffer without nosleep");
#endif

	if ((xs = scsipi_make_xs(sc_link, scsipi_cmd, cmdlen, data_addr,
	    datalen, retries, timeout, bp, flags)) == NULL)
		return (ENOMEM);

	if ((error = scsipi_execute_xs(xs)) == EJUSTRETURN)
		return (0);

	/*
	 * we have finished with the xfer stuct, free it and
	 * check if anyone else needs to be started up.
	 */
	scsipi_free_xs(xs, flags);
	return (error);
}

/*
 * Look at the returned sense and act on the error, determining
 * the unix error number to pass back.  (0 = report no error)
 *
 * THIS IS THE DEFAULT ERROR HANDLER FOR SCSI DEVICES
 */
int
scsi_interpret_sense(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_sense_data *sense;
	struct scsipi_link *sc_link = xs->sc_link;
	u_int8_t key;
	u_int32_t info;
	int error;
#ifndef	SCSIVERBOSE
	static char *error_mes[] = {
		"soft error (corrected)",
		"not ready", "medium error",
		"non-media hardware failure", "illegal request",
		"unit attention", "readonly device",
		"no data found", "vendor unique",
		"copy aborted", "command aborted",
		"search returned equal", "volume overflow",
		"verify miscompare", "unknown error key"
	};
#endif

	sense = &xs->sense.scsi_sense;
#ifdef	SCSIDEBUG
	if ((sc_link->flags & SDEV_DB1) != 0) {
		int count;
		printf("code 0x%x valid 0x%x ",
			sense->error_code & SSD_ERRCODE,
			sense->error_code & SSD_ERRCODE_VALID ? 1 : 0);
		printf("seg 0x%x key 0x%x ili 0x%x eom 0x%x fmark 0x%x\n",
			sense->segment,
			sense->flags & SSD_KEY,
			sense->flags & SSD_ILI ? 1 : 0,
			sense->flags & SSD_EOM ? 1 : 0,
			sense->flags & SSD_FILEMARK ? 1 : 0);
		printf("info: 0x%x 0x%x 0x%x 0x%x followed by %d extra bytes\n",
			sense->info[0],
			sense->info[1],
			sense->info[2],
			sense->info[3],
			sense->extra_len);
		printf("extra: ");
		for (count = 0; count < ADD_BYTES_LIM(sense); count++)
			printf("0x%x ", sense->cmd_spec_info[count]);
		printf("\n");
	}
#endif	/* SCSIDEBUG */
	/*
	 * If the device has it's own error handler, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal error processing.
	 */
	if (sc_link->device->err_handler) {
		SC_DEBUG(sc_link, SDEV_DB2,
		    ("calling private err_handler()\n"));
		error = (*sc_link->device->err_handler)(xs);
		if (error != -1)
			return (error);		/* error >= 0  better ? */
	}
	/* otherwise use the default */
	switch (sense->error_code & SSD_ERRCODE) {
		/*
		 * If it's code 70, use the extended stuff and
		 * interpret the key
		 */
	case 0x71:		/* delayed error */
		sc_link->sc_print_addr(sc_link);
		key = sense->flags & SSD_KEY;
		printf(" DEFERRED ERROR, key = 0x%x\n", key);
		/* FALLTHROUGH */
	case 0x70:
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0)
			info = _4btol(sense->info);
		else
			info = 0;
		key = sense->flags & SSD_KEY;

		switch (key) {
		case 0x0:	/* NO SENSE */
		case 0x1:	/* RECOVERED ERROR */
			if (xs->resid == xs->datalen)
				xs->resid = 0;	/* not short read */
		case 0xc:	/* EQUAL */
			error = 0;
			break;
		case 0x2:	/* NOT READY */
			if ((sc_link->flags & SDEV_REMOVABLE) != 0)
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if ((xs->flags & SCSI_IGNORE_NOT_READY) != 0)
				return (0);
			if ((xs->flags & SCSI_SILENT) != 0)
				return (EIO);
			error = EIO;
			break;
		case 0x5:	/* ILLEGAL REQUEST */
			if ((xs->flags & SCSI_IGNORE_ILLEGAL_REQUEST) != 0)
				return (0);
			if ((xs->flags & SCSI_SILENT) != 0)
				return (EIO);
			error = EINVAL;
			break;
		case 0x6:	/* UNIT ATTENTION */
			if ((sc_link->flags & SDEV_REMOVABLE) != 0)
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if ((xs->flags & SCSI_IGNORE_MEDIA_CHANGE) != 0 ||
				/* XXX Should reupload any transient state. */
				(sc_link->flags & SDEV_REMOVABLE) == 0)
				return (ERESTART);
			if ((xs->flags & SCSI_SILENT) != 0)
				return (EIO);
			error = EIO;
			break;
		case 0x7:	/* DATA PROTECT */
			error = EACCES;
			break;
		case 0x8:	/* BLANK CHECK */
			error = 0;
			break;
		case 0xb:	/* COMMAND ABORTED */
			error = ERESTART;
			break;
		case 0xd:	/* VOLUME OVERFLOW */
			error = ENOSPC;
			break;
		default:
			error = EIO;
			break;
		}

#ifdef SCSIVERBOSE
		if ((xs->flags & SCSI_SILENT) == 0)
			scsi_print_sense(xs, 0);
#else
		if (key) {
			sc_link->sc_print_addr(sc_link);
			printf("%s", error_mes[key - 1]);
			if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
				switch (key) {
				case 0x2:	/* NOT READY */
				case 0x5:	/* ILLEGAL REQUEST */
				case 0x6:	/* UNIT ATTENTION */
				case 0x7:	/* DATA PROTECT */
					break;
				case 0x8:	/* BLANK CHECK */
					printf(", requested size: %d (decimal)",
					    info);
					break;
				case 0xb:
					if (xs->retries)
						printf(", retrying");
					printf(", cmd 0x%x, info 0x%x",
					    xs->cmd->opcode, info);
					break;
				default:
					printf(", info = %d (decimal)", info);
				}
			}
			if (sense->extra_len != 0) {
				int n;
				printf(", data =");
				for (n = 0; n < sense->extra_len; n++)
					printf(" %02x",
					    sense->cmd_spec_info[n]);
			}
			printf("\n");
		}
#endif
		return (error);

	/*
	 * Not code 70, just report it
	 */
	default:
		sc_link->sc_print_addr(sc_link);
		printf("error code %d", sense->error_code & SSD_ERRCODE);
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			struct scsipi_sense_data_unextended *usense =
			    (struct scsipi_sense_data_unextended *)sense;
			printf(" at block no. %d (decimal)",
			    _3btol(usense->block));
		}
		printf("\n");
		return (EIO);
	}
}

/*
 * Utility routines often used in SCSI stuff
 */


/*
 * Print out the scsi_link structure's address info.
 */
void
scsi_print_addr(sc_link)
	struct scsipi_link *sc_link;
{

	printf("%s(%s:%d:%d): ",
	    sc_link->device_softc ?
	    ((struct device *)sc_link->device_softc)->dv_xname : "probe",
	    ((struct device *)sc_link->adapter_softc)->dv_xname,
	    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun);
}

#ifdef SCSIVERBOSE
static const char *sense_keys[16] = {
	"No Additional Sense",
	"Soft Error",
	"Not Ready",
	"Media Error",
	"Hardware Error",
	"Illegal Request",
	"Unit Attention",
	"Write Protected",
	"Blank Check",
	"Vendor Unique",
	"Copy Aborted",
	"Aborted Command",
	"Equal Error",
	"Volume Overflow",
	"Miscompare Error",
	"Reserved"
};
static const struct {
	unsigned char asc;
	unsigned char ascq;
	char *description;
} adesc[] = {
{ 0x00, 0x00, "No Additional Sense Information" },
{ 0x00, 0x01, "Filemark Detected" },
{ 0x00, 0x02, "End-Of-Partition/Medium Detected" },
{ 0x00, 0x03, "Setmark Detected" },
{ 0x00, 0x04, "Beginning-Of-Partition/Medium Detected" },
{ 0x00, 0x05, "End-Of-Data Detected" },
{ 0x00, 0x06, "I/O Process Terminated" },
{ 0x00, 0x11, "Audio Play Operation In Progress" },
{ 0x00, 0x12, "Audio Play Operation Paused" },
{ 0x00, 0x13, "Audio Play Operation Successfully Completed" },
{ 0x00, 0x14, "Audio Play Operation Stopped Due to Error" },
{ 0x00, 0x15, "No Current Audio Status To Return" },
{ 0x01, 0x00, "No Index/Sector Signal" },
{ 0x02, 0x00, "No Seek Complete" },
{ 0x03, 0x00, "Peripheral Device Write Fault" },
{ 0x03, 0x01, "No Write Current" },
{ 0x03, 0x02, "Excessive Write Errors" },
{ 0x04, 0x00, "Logical Unit Not Ready, Cause Not Reportable" },
{ 0x04, 0x01, "Logical Unit Is in Process Of Becoming Ready" },
{ 0x04, 0x02, "Logical Unit Not Ready, Initialization Command Required" },
{ 0x04, 0x03, "Logical Unit Not Ready, Manual Intervention Required" },
{ 0x04, 0x04, "Logical Unit Not Ready, Format In Progress" },
{ 0x05, 0x00, "Logical Unit Does Not Respond To Selection" },
{ 0x06, 0x00, "No Reference Position Found" },
{ 0x07, 0x00, "Multiple Peripheral Devices Selected" },
{ 0x08, 0x00, "Logical Unit Communication Failure" },
{ 0x08, 0x01, "Logical Unit Communication Timeout" },
{ 0x08, 0x02, "Logical Unit Communication Parity Error" },
{ 0x09, 0x00, "Track Following Error" },
{ 0x09, 0x01, "Tracking Servo Failure" },
{ 0x09, 0x02, "Focus Servo Failure" },
{ 0x09, 0x03, "Spindle Servo Failure" },
{ 0x0A, 0x00, "Error Log Overflow" },
{ 0x0C, 0x00, "Write Error" },
{ 0x0C, 0x01, "Write Error Recovered with Auto Reallocation" },
{ 0x0C, 0x02, "Write Error - Auto Reallocate Failed" },
{ 0x10, 0x00, "ID CRC Or ECC Error" },
{ 0x11, 0x00, "Unrecovered Read Error" },
{ 0x11, 0x01, "Read Retried Exhausted" },
{ 0x11, 0x02, "Error Too Long To Correct" },
{ 0x11, 0x03, "Multiple Read Errors" },
{ 0x11, 0x04, "Unrecovered Read Error - Auto Reallocate Failed" },
{ 0x11, 0x05, "L-EC Uncorrectable Error" },
{ 0x11, 0x06, "CIRC Unrecovered Error" },
{ 0x11, 0x07, "Data Resynchronization Error" },
{ 0x11, 0x08, "Incomplete Block Found" },
{ 0x11, 0x09, "No Gap Found" },
{ 0x11, 0x0A, "Miscorrected Error" },
{ 0x11, 0x0B, "Uncorrected Read Error - Recommend Reassignment" },
{ 0x11, 0x0C, "Uncorrected Read Error - Recommend Rewrite the Data" },
{ 0x12, 0x00, "Address Mark Not Found for ID Field" },
{ 0x13, 0x00, "Address Mark Not Found for Data Field" },
{ 0x14, 0x00, "Recorded Entity Not Found" },
{ 0x14, 0x01, "Record Not Found" },
{ 0x14, 0x02, "Filemark or Setmark Not Found" },
{ 0x14, 0x03, "End-Of-Data Not Found" },
{ 0x14, 0x04, "Block Sequence Error" },
{ 0x15, 0x00, "Random Positioning Error" },
{ 0x15, 0x01, "Mechanical Positioning Error" },
{ 0x15, 0x02, "Positioning Error Detected By Read of Medium" },
{ 0x16, 0x00, "Data Synchronization Mark Error" },
{ 0x17, 0x00, "Recovered Data With No Error Correction Applied" },
{ 0x17, 0x01, "Recovered Data With Retries" },
{ 0x17, 0x02, "Recovered Data With Positive Head Offset" },
{ 0x17, 0x03, "Recovered Data With Negative Head Offset" },
{ 0x17, 0x04, "Recovered Data With Retries and/or CIRC Applied" },
{ 0x17, 0x05, "Recovered Data Using Previous Sector ID" },
{ 0x17, 0x06, "Recovered Data Without ECC - Data Auto-Reallocated" },
{ 0x17, 0x07, "Recovered Data Without ECC - Recommend Reassignment" },
{ 0x17, 0x08, "Recovered Data Without ECC - Recommend Rewrite" },
{ 0x18, 0x00, "Recovered Data With Error Correction Applied" },
{ 0x18, 0x01, "Recovered Data With Error Correction & Retries Applied" },
{ 0x18, 0x02, "Recovered Data - Data Auto-Reallocated" },
{ 0x18, 0x03, "Recovered Data With CIRC" },
{ 0x18, 0x04, "Recovered Data With LEC" },
{ 0x18, 0x05, "Recovered Data - Recommend Reassignment" },
{ 0x18, 0x06, "Recovered Data - Recommend Rewrite" },
{ 0x19, 0x00, "Defect List Error" },
{ 0x19, 0x01, "Defect List Not Available" },
{ 0x19, 0x02, "Defect List Error in Primary List" },
{ 0x19, 0x03, "Defect List Error in Grown List" },
{ 0x1A, 0x00, "Parameter List Length Error" },
{ 0x1B, 0x00, "Synchronous Data Transfer Error" },
{ 0x1C, 0x00, "Defect List Not Found" },
{ 0x1C, 0x01, "Primary Defect List Not Found" },
{ 0x1C, 0x02, "Grown Defect List Not Found" },
{ 0x1D, 0x00, "Miscompare During Verify Operation" },
{ 0x1E, 0x00, "Recovered ID with ECC" },
{ 0x20, 0x00, "Invalid Command Operation Code" },
{ 0x21, 0x00, "Logical Block Address Out of Range" },
{ 0x21, 0x01, "Invalid Element Address" },
{ 0x22, 0x00, "Illegal Function (Should 20 00, 24 00, or 26 00)" },
{ 0x24, 0x00, "Illegal Field in CDB" },
{ 0x25, 0x00, "Logical Unit Not Supported" },
{ 0x26, 0x00, "Invalid Field In Parameter List" },
{ 0x26, 0x01, "Parameter Not Supported" },
{ 0x26, 0x02, "Parameter Value Invalid" },
{ 0x26, 0x03, "Threshold Parameters Not Supported" },
{ 0x27, 0x00, "Write Protected" },
{ 0x28, 0x00, "Not Ready To Ready Transition (Medium May Have Changed)" },
{ 0x28, 0x01, "Import Or Export Element Accessed" },
{ 0x29, 0x00, "Power On, Reset, or Bus Device Reset Occurred" },
{ 0x2A, 0x00, "Parameters Changed" },
{ 0x2A, 0x01, "Mode Parameters Changed" },
{ 0x2A, 0x02, "Log Parameters Changed" },
{ 0x2B, 0x00, "Copy Cannot Execute Since Host Cannot Disconnect" },
{ 0x2C, 0x00, "Command Sequence Error" },
{ 0x2C, 0x01, "Too Many Windows Specified" },
{ 0x2C, 0x02, "Invalid Combination of Windows Specified" },
{ 0x2D, 0x00, "Overwrite Error On Update In Place" },
{ 0x2F, 0x00, "Commands Cleared By Another Initiator" },
{ 0x30, 0x00, "Incompatible Medium Installed" },
{ 0x30, 0x01, "Cannot Read Medium - Unknown Format" },
{ 0x30, 0x02, "Cannot Read Medium - Incompatible Format" },
{ 0x30, 0x03, "Cleaning Cartridge Installed" },
{ 0x31, 0x00, "Medium Format Corrupted" },
{ 0x31, 0x01, "Format Command Failed" },
{ 0x32, 0x00, "No Defect Spare Location Available" },
{ 0x32, 0x01, "Defect List Update Failure" },
{ 0x33, 0x00, "Tape Length Error" },
{ 0x36, 0x00, "Ribbon, Ink, or Toner Failure" },
{ 0x37, 0x00, "Rounded Parameter" },
{ 0x39, 0x00, "Saving Parameters Not Supported" },
{ 0x3A, 0x00, "Medium Not Present" },
{ 0x3B, 0x00, "Positioning Error" },
{ 0x3B, 0x01, "Tape Position Error At Beginning-of-Medium" },
{ 0x3B, 0x02, "Tape Position Error At End-of-Medium" },
{ 0x3B, 0x03, "Tape or Electronic Vertical Forms Unit Not Ready" },
{ 0x3B, 0x04, "Slew Failure" },
{ 0x3B, 0x05, "Paper Jam" },
{ 0x3B, 0x06, "Failed To Sense Top-Of-Form" },
{ 0x3B, 0x07, "Failed To Sense Bottom-Of-Form" },
{ 0x3B, 0x08, "Reposition Error" },
{ 0x3B, 0x09, "Read Past End Of Medium" },
{ 0x3B, 0x0A, "Read Past Begining Of Medium" },
{ 0x3B, 0x0B, "Position Past End Of Medium" },
{ 0x3B, 0x0C, "Position Past Beginning Of Medium" },
{ 0x3B, 0x0D, "Medium Destination Element Full" },
{ 0x3B, 0x0E, "Medium Source Element Empty" },
{ 0x3D, 0x00, "Invalid Bits In IDENTFY Message" },
{ 0x3E, 0x00, "Logical Unit Has Not Self-Configured Yet" },
{ 0x3F, 0x00, "Target Operating Conditions Have Changed" },
{ 0x3F, 0x01, "Microcode Has Changed" },
{ 0x3F, 0x02, "Changed Operating Definition" },
{ 0x3F, 0x03, "INQUIRY Data Has Changed" },
{ 0x40, 0x00, "RAM FAILURE (Should Use 40 NN)" },
{ 0x41, 0x00, "Data Path FAILURE (Should Use 40 NN)" },
{ 0x42, 0x00, "Power-On or Self-Test FAILURE (Should Use 40 NN)" },
{ 0x43, 0x00, "Message Error" },
{ 0x44, 0x00, "Internal Target Failure" },
{ 0x45, 0x00, "Select Or Reselect Failure" },
{ 0x46, 0x00, "Unsuccessful Soft Reset" },
{ 0x47, 0x00, "SCSI Parity Error" },
{ 0x48, 0x00, "INITIATOR DETECTED ERROR Message Received" },
{ 0x49, 0x00, "Invalid Message Error" },
{ 0x4A, 0x00, "Command Phase Error" },
{ 0x4B, 0x00, "Data Phase Error" },
{ 0x4C, 0x00, "Logical Unit Failed Self-Configuration" },
{ 0x4E, 0x00, "Overlapped Commands Attempted" },
{ 0x50, 0x00, "Write Append Error" },
{ 0x50, 0x01, "Write Append Position Error" },
{ 0x50, 0x01, "Write Append Position Error" },
{ 0x50, 0x02, "Position Error Related To Timing" },
{ 0x51, 0x00, "Erase Failure" },
{ 0x52, 0x00, "Cartridge Fault" },
{ 0x53, 0x00, "Media Load or Eject Failed" },
{ 0x53, 0x01, "Unload Tape Failure" },
{ 0x53, 0x02, "Medium Removal Prevented" },
{ 0x54, 0x00, "SCSI To Host System Interface Failure" },
{ 0x55, 0x00, "System Resource Failure" },
{ 0x57, 0x00, "Unable To Recover Table-Of-Contents" },
{ 0x58, 0x00, "Generation Does Not Exist" },
{ 0x59, 0x00, "Updated Block Read" },
{ 0x5A, 0x00, "Operator Request or State Change Input (Unspecified)" },
{ 0x5A, 0x01, "Operator Medium Removal Requested" },
{ 0x5A, 0x02, "Operator Selected Write Protect" },
{ 0x5A, 0x03, "Operator Selected Write Permit" },
{ 0x5B, 0x00, "Log Exception" },
{ 0x5B, 0x01, "Threshold Condition Met" },
{ 0x5B, 0x02, "Log Counter At Maximum" },
{ 0x5B, 0x03, "Log List Codes Exhausted" },
{ 0x5C, 0x00, "RPL Status Change" },
{ 0x5C, 0x01, "Spindles Synchronized" },
{ 0x5C, 0x02, "Spindles Not Synchronized" },
{ 0x60, 0x00, "Lamp Failure" },
{ 0x61, 0x00, "Video Acquisition Error" },
{ 0x61, 0x01, "Unable To Acquire Video" },
{ 0x61, 0x02, "Out Of Focus" },
{ 0x62, 0x00, "Scan Head Positioning Error" },
{ 0x63, 0x00, "End Of User Area Encountered On This Track" },
{ 0x64, 0x00, "Illegal Mode For This Track" },
{ 0x00, 0x00, NULL }
};

static inline void
asc2ascii(asc, ascq, result)
	unsigned char asc, ascq;
	char *result;
{
	register int i = 0;

	while (adesc[i].description != NULL) {
		if (adesc[i].asc == asc && adesc[i].ascq == ascq)
			break;
		i++;
	}
	if (adesc[i].description == NULL) {
		if (asc == 0x40 && ascq != 0)
			(void)sprintf(result,
			    "Diagnostic Failure on Component 0x%02x",
			    ascq & 0xff);
		else
			(void)sprintf(result, "ASC 0x%02x ASCQ 0x%02x",
			    asc & 0xff, ascq & 0xff);
	} else
		(void)strcpy(result, adesc[i].description);
}

void
scsi_print_sense(xs, verbosity)
	struct scsipi_xfer *xs;
	int verbosity;
{
	int32_t info;
	register int i, j, k;
	char *sbs, *s;

	xs->sc_link->sc_print_addr(xs->sc_link);
	s = (char *) &xs->sense.scsi_sense;
	printf(" Check Condition on opcode 0x%x\n", xs->cmd->opcode);

	/*
	 * Basics- print out SENSE KEY
	 */
	printf("    SENSE KEY:  %s", scsi_decode_sense(s, 0));

	/*
	 * Print out, unqualified but aligned, FMK, EOM and ILI status.
	 */
	if (s[2] & 0xe0) {
		char pad;
		printf("\n              ");
		pad = ' ';
		if (s[2] & SSD_FILEMARK) {
			printf("%c Filemark Detected", pad);
			pad = ',';
		}
		if (s[2] & SSD_EOM) {
			printf("%c EOM Detected", pad);
			pad = ',';
		}
		if (s[2] & SSD_ILI)
			printf("%c Incorrect Length Indicator Set", pad);
	}

	/*
	 * Now we should figure out, based upon device type, how
	 * to format the information field. Unfortunately, that's
	 * not convenient here, so we'll print it as a signed
	 * 32 bit integer.
	 */
	info = _4btol(&s[3]);
	if (info)
		printf("\n   INFO FIELD:  %d", info);

	/*
	 * Now we check additional length to see whether there is
	 * more information to extract.
	 */

	/* enough for command specific information? */
	if (s[7] < 4) {
		printf("\n");
		return;
	}
	info = _4btol(&s[8]);
	if (info)
		printf("\n COMMAND INFO:  %d (0x%x)", info, info);

	/*
	 * Decode ASC && ASCQ info, plus FRU, plus the rest...
	 */

	sbs = scsi_decode_sense(s, 1);
	if (sbs)
		printf("\n     ASC/ASCQ:  %s", sbs);
	if (s[14] != 0)
		printf("\n     FRU CODE:  0x%x\n", s[14] & 0xff);
	sbs = scsi_decode_sense(s, 3);
	if (sbs)
		printf("         SKSV:  %s", sbs);
	printf("\n");
	if (verbosity == 0) {
		printf("\n");
		return;
	}

	/*
	 * Now figure whether we should print any additional informtion.
	 *
	 * Where should we start from? If we had SKSV data,
	 * start from offset 18, else from offset 15.
	 *
	 * From that point until the end of the buffer, check for any
	 * nonzero data. If we have some, go back and print the lot,
	 * otherwise we're done.
	 */
	if (sbs)
		i = 18;
	else
		i = 15;
	for (j = i; j < sizeof (xs->sense); j++)
		if (s[j])
			break;
	if (j == sizeof (xs->sense))
		return;

	printf("\n Additional Sense Information (byte %d out...):\n", i);
	if (i == 15) {
		printf("\n\t%2d:", i);
		k = 7;
	} else {
		printf("\n\t%2d:", i);
		k = 2;
		j -= 2;
	}
	while (j > 0) {
		if (i >= sizeof (xs->sense))
			break;
		if (k == 8) {
			k = 0;
			printf("\n\t%2d:", i);
		}
		printf(" 0x%02x", s[i] & 0xff);
		k++;
		j--;
		i++;
	}
	printf("\n\n");
}

char *
scsi_decode_sense(sinfo, flag)
	void *sinfo;
	int flag;
{
	unsigned char *snsbuf;
	unsigned char skey;
	static char rqsbuf[132];

	skey = 0;

	snsbuf = (unsigned char *) sinfo;
	if (flag == 0 || flag == 2 || flag == 3)
		skey = snsbuf[2] & 0xf;
	if (flag == 0) {			/* Sense Key Only */
		(void) strcpy(rqsbuf, sense_keys[skey]);
		return (rqsbuf);
	} else if (flag == 1) {			/* ASC/ASCQ Only */
		asc2ascii(snsbuf[12], snsbuf[13], rqsbuf);
		return (rqsbuf);
	} else  if (flag == 2) {		/* Sense Key && ASC/ASCQ */
		auto char localbuf[64];
		asc2ascii(snsbuf[12], snsbuf[13], localbuf);
		(void) sprintf(rqsbuf, "%s, %s", sense_keys[skey], localbuf);
		return (rqsbuf);
	} else if (flag == 3 && snsbuf[7] >= 9 && (snsbuf[15] & 0x80)) {
		/*
		 * SKSV Data
		 */
		switch (skey) {
		case 0x5:			/* Illegal Request */
			if (snsbuf[15] & 0x8)
				(void)sprintf(rqsbuf,
				    "Error in %s, Offset %d, bit %d",
				    (snsbuf[15] & 0x40)? "CDB" : "Parameters",
				    (snsbuf[16] & 0xff) << 8 |
				    (snsbuf[17] & 0xff), snsbuf[15] & 0x7);
			else
				(void)sprintf(rqsbuf,
				    "Error in %s, Offset %d",
				    (snsbuf[15] & 0x40)? "CDB" : "Parameters",
				    (snsbuf[16] & 0xff) << 8 |
				    (snsbuf[17] & 0xff));
			return (rqsbuf);
		case 0x1:
		case 0x3:
		case 0x4:
			(void)sprintf(rqsbuf, "Actual Retry Count: %d",
			    (snsbuf[16] & 0xff) << 8 | (snsbuf[17] & 0xff));
			return (rqsbuf);
		case 0x2:
			(void)sprintf(rqsbuf, "Progress Indicator: %d",
			    (snsbuf[16] & 0xff) << 8 | (snsbuf[17] & 0xff));
			return (rqsbuf);
		default:
			break;
		}
	}
	return (NULL);
}
#endif
