/*	$NetBSD: scsiconf.h,v 1.38.2.3 1997/10/14 10:25:12 thorpej Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 * Originally written by Julian Elischer (julian@tfs.com)
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
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#ifndef	SCSI_SCSICONF_H
#define SCSI_SCSICONF_H 1

#include <dev/scsipi/scsipiconf.h>

/*
 * Other definitions used by autoconfiguration.
 */
#define	SCSI_CHANNEL_ONLY_ONE	-1	/* only one channel on controller */

int	scsiprint __P((void *, const char *));

/*
 * One of these is allocated and filled in for each scsi bus.
 * it holds pointers to allow the scsi bus to get to the driver
 * That is running each LUN on the bus
 * it also has a template entry which is the prototype struct
 * supplied by the adapter driver, this is used to initialise
 * the others, before they have the rest of the fields filled in
 */
struct scsibus_softc {
	struct device sc_dev;
	struct scsipi_link *adapter_link; /* prototype supplied by adapter */
	struct scsipi_link ***sc_link;		/* dynamically allocated */
	int	sc_maxtarget;
	u_int8_t moreluns;
};

#define SCSI_OP_TARGET	0x0001
#define	SCSI_OP_RESET	0x0002
#define	SCSI_OP_BDINFO	0x0003

int	scsi_change_def __P((struct scsipi_link *, int));
int	scsi_interpret_sense __P((struct scsipi_xfer *));
void	scsi_print_addr __P((struct scsipi_link *));
#ifdef SCSIVERBOSE
void	scsi_print_sense __P((struct scsipi_xfer *, int));
#endif
int	scsi_probe_busses __P((int, int, int));
int	scsi_scsipi_cmd __P((struct scsipi_link *, struct scsipi_generic *,
	    int cmdlen, u_char *data_addr, int datalen, int retries,
	    int timeout, struct buf *bp, int flags));

#endif /* SCSI_SCSICONF_H */
