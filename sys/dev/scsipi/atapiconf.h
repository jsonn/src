/*	$NetBSD: atapiconf.h,v 1.10.6.1 2001/06/21 20:05:42 nathanw Exp $	*/

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

struct atapibus_softc {
	struct device sc_dev;
	struct scsipi_channel *sc_channel;	/* our scsipi_channel */
	struct ata_drive_datas *sc_drvs;	/* array supplied by adapter */
};

extern const struct scsipi_periphsw atapi_probe_periphsw;


/*
 * We need some more callbacks than in scsipi_adapter.
 * So define a new atapi_adapter, we'll cast channel->chan_adapter to
 * atapi_adapter when we need the extra callback (only in ATAPI code)
 */
struct atapi_adapter {
	struct scsipi_adapter _generic;
	void (*atapi_probe_device) __P((struct atapibus_softc *, int));
};

void 	*atapi_probe_device __P((struct atapibus_softc *, int,
	    struct scsipi_periph *, struct scsipibus_attach_args *));
int	atapiprint __P((void *, const char *));
void	atapi_print_addr __P((struct scsipi_periph *));
int	atapi_interpret_sense __P((struct scsipi_xfer *));
int	atapi_scsipi_cmd __P((struct scsipi_periph *, struct scsipi_generic *,
	    int, void *, size_t, int, int, struct buf *, int));
void	atapi_kill_pending __P((struct scsipi_periph *));
