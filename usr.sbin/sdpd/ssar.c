/*	$NetBSD: ssar.c,v 1.1.10.1 2008/01/09 02:02:28 matt Exp $	*/

/*
 * ssar.c
 *
 * Copyright (c) 2004 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ssar.c,v 1.1.10.1 2008/01/09 02:02:28 matt Exp $
 * $FreeBSD: src/usr.sbin/bluetooth/sdpd/ssar.c,v 1.2 2005/01/05 18:37:37 emax Exp $
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: ssar.c,v 1.1.10.1 2008/01/09 02:02:28 matt Exp $");

#include <sys/queue.h>
#include <bluetooth.h>
#include <sdp.h>
#include <string.h>
#include "profile.h"
#include "provider.h"
#include "server.h"
#include "uuid-private.h"

/*
 * Prepare SDP Service Search Attribute Response
 */

int32_t
server_prepare_service_search_attribute_response(server_p srv, int32_t fd)
{
	uint8_t const	*req = srv->req + sizeof(sdp_pdu_t);
	uint8_t const	*req_end = req + ((sdp_pdu_p)(srv->req))->len;
	uint8_t		*rsp = srv->fdidx[fd].rsp;
	uint8_t const	*rsp_end = rsp + L2CAP_MTU_MAXIMUM;

	uint8_t const	*aidptr = NULL;

	provider_t	*provider = NULL;
	int32_t		 type, rsp_limit, ucount, aidlen, cslen, cs;
	uint128_t	 ulist[12];

	/*
	 * Minimal Service Search Attribute Request request
	 *
	 * seq8 len8		- 2 bytes
	 *	uuid16 value16  - 3 bytes ServiceSearchPattern
	 * value16		- 2 bytes MaximumAttributeByteCount
	 * seq8 len8		- 2 bytes
	 *	uint16 value16	- 3 bytes AttributeIDList
	 * value8		- 1 byte  ContinuationState
	 */

	/* Get ServiceSearchPattern */
	ucount = server_get_service_search_pattern(&req, req_end, ulist);
	if (ucount < 1 || ucount > 12)
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	/* Get MaximumAttributeByteCount */
	if (req + 2 > req_end)
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	SDP_GET16(rsp_limit, req);
	if (rsp_limit <= 0)
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	/* Get size of AttributeIDList */
	if (req + 1 > req_end)
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	aidlen = 0;
	SDP_GET8(type, req);
	switch (type) {
	case SDP_DATA_SEQ8:
		if (req + 1 > req_end)
			return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

		SDP_GET8(aidlen, req);
		break;

	case SDP_DATA_SEQ16:
		if (req + 2 > req_end)
			return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

		SDP_GET16(aidlen, req);
		break;

	case SDP_DATA_SEQ32:
		if (req + 4 > req_end)
			return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

		SDP_GET32(aidlen, req);
		break;
	}
	if (aidlen <= 0)
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	aidptr = req;
	req += aidlen;

	/* Get ContinuationState */
	if (req + 1 > req_end)
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	SDP_GET8(cslen, req);
	if (cslen == 2 && req + 2 == req_end)
		SDP_GET16(cs, req);
	else if (cslen == 0 && req == req_end)
		cs = 0;
	else
		return (SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);

	/* Process the request. First, check continuation state */
	if (srv->fdidx[fd].rsp_cs != cs)
		return (SDP_ERROR_CODE_INVALID_CONTINUATION_STATE);
	if (srv->fdidx[fd].rsp_size > 0)
		return (0);

	/*
	 * Service Search Attribute Response format
	 *
	 * value16		- 2 bytes  AttributeListByteCount (not incl.)
	 * seq8 len16		- 3 bytes
	 *	attr list	- 3+ bytes AttributeLists
	 *	[ attr list ]
	 */

	rsp += 3;	/* leave space for sequence header */

	for (provider = provider_get_first();
	     provider != NULL;
	     provider = provider_get_next(provider)) {
		if (!provider_match_bdaddr(provider, &srv->req_sa.bt_bdaddr))
			continue;

		if (!provider_match_uuid(provider, ulist, ucount))
			continue;

		cs = server_prepare_attr_list(provider,
			aidptr, aidptr + aidlen, rsp, rsp_end);
		if (cs < 0)
			return (SDP_ERROR_CODE_INSUFFICIENT_RESOURCES);

		rsp += cs;
	}

	/* Set reply size (not counting PDU header and continuation state) */
	srv->fdidx[fd].rsp_limit = srv->fdidx[fd].omtu - sizeof(sdp_pdu_t) - 2;
	if (srv->fdidx[fd].rsp_limit > rsp_limit)
		srv->fdidx[fd].rsp_limit = rsp_limit;

	srv->fdidx[fd].rsp_size = rsp - srv->fdidx[fd].rsp;
	srv->fdidx[fd].rsp_cs = 0;

	/* Fix AttributeLists sequence header */
	rsp = srv->fdidx[fd].rsp;
	SDP_PUT8(SDP_DATA_SEQ16, rsp);
	SDP_PUT16(srv->fdidx[fd].rsp_size - 3, rsp);

	return (0);
}
