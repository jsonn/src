/*	$NetBSD: atapiconf.h,v 1.7.14.1 1999/11/15 00:41:23 fvdl Exp $	*/

/*
 * Copyright (c) 1996 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
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

#include <dev/scsipi/scsipiconf.h>

/* drive states stored in ata_drive_datas */
#define PIOMODE		0
#define PIOMODE_WAIT	1
#define DMAMODE		2
#define DMAMODE_WAIT	3
#define READY		4

struct atapi_mode_header;
struct ataparams;

int	wdc_atapi_get_params __P((struct scsipi_link *, u_int8_t, int,
	    struct ataparams *)); 
void	atapi_print_addr __P((struct scsipi_link *));
int	atapi_interpret_sense __P((struct scsipi_xfer *));
int	atapi_scsipi_cmd __P((struct scsipi_link *, struct scsipi_generic *,
	    int, u_char *, int, int, int, struct buf *, int));
int	atapi_mode_select __P((struct scsipi_link *,
	    struct atapi_mode_header *, int, int, int, int));
int	atapi_mode_sense __P((struct scsipi_link *, int,
	    struct atapi_mode_header *, int, int, int, int));
void	atapi_kill_pending __P((struct scsipi_link *));
