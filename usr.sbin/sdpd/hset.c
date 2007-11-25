/*	$NetBSD: hset.c,v 1.1.4.1 2007/11/25 09:01:15 xtraeme Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: hset.c,v 1.1.4.1 2007/11/25 09:01:15 xtraeme Exp $");

#include <sys/queue.h>
#include <bluetooth.h>
#include <sdp.h>
#include <stdio.h>
#include <string.h>

#include <netbt/rfcomm.h>

#include "profile.h"
#include "provider.h"

static int32_t
hset_profile_create_service_class_id_list(
		uint8_t *buf, uint8_t const * const eob,
		uint8_t const *data, uint32_t datalen)
{
	static uint16_t	service_classes[] = {
		SDP_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY,
		SDP_SERVICE_CLASS_GENERIC_AUDIO,
	};

	return (common_profile_create_service_class_id_list(
			buf, eob,
			(uint8_t const *) service_classes,
			sizeof(service_classes)));
}

static int32_t
hset_profile_create_protocol_descriptor_list(
		uint8_t *buf, uint8_t const * const eob,
		uint8_t const *data, uint32_t datalen)
{
	provider_t const	*provider = (provider_t const *) data;
	sdp_hset_profile_t	*hset = (sdp_hset_profile_t *) provider->data;

	return (rfcomm_profile_create_protocol_descriptor_list(
			buf, eob,
			(uint8_t const *) &hset->server_channel, 1));
}

static int32_t
hset_profile_create_bluetooth_profile_descriptor_list(
		uint8_t *buf, uint8_t const * const eob,
		uint8_t const *data, uint32_t datalen)
{
	static uint16_t	profile_descriptor_list[] = {
		SDP_SERVICE_CLASS_HEADSET,
		0x0100
	};

	return (common_profile_create_bluetooth_profile_descriptor_list(
			buf, eob,
			(uint8_t const *) profile_descriptor_list,
			sizeof(profile_descriptor_list)));
}

static int32_t
hset_profile_create_service_name(
		uint8_t *buf, uint8_t const * const eob,
		uint8_t const *data, uint32_t datalen)
{
	static char	service_name[] = "Voice Gateway";

	return (common_profile_create_string8(
			buf, eob,
			(uint8_t const *) service_name, strlen(service_name)));
}

static int32_t
hset_profile_data_valid(uint8_t const *data, uint32_t datalen)
{
	sdp_hset_profile_t const *hset = (sdp_hset_profile_t const *) data;

	if (hset->server_channel < RFCOMM_CHANNEL_MIN
	    || hset->server_channel > RFCOMM_CHANNEL_MAX)
		return 0;

	return 1;
}

static attr_t	hset_profile_attrs[] = {
	{ SDP_ATTR_SERVICE_RECORD_HANDLE,
	  common_profile_create_service_record_handle },
	{ SDP_ATTR_SERVICE_CLASS_ID_LIST,
	  hset_profile_create_service_class_id_list },
	{ SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST,
	  hset_profile_create_protocol_descriptor_list },
	{ SDP_ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST,
	  hset_profile_create_bluetooth_profile_descriptor_list },
	{ SDP_ATTR_PRIMARY_LANGUAGE_BASE_ID + SDP_ATTR_SERVICE_NAME_OFFSET,
	  hset_profile_create_service_name },
	{ 0, NULL } /* end entry */
};

static uint16_t hset_profile_uuids[] = {
	SDP_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY,
	SDP_SERVICE_CLASS_GENERIC_AUDIO,
	SDP_UUID_PROTOCOL_L2CAP,
	SDP_UUID_PROTOCOL_RFCOMM,
	SDP_SERVICE_CLASS_HEADSET,
};

profile_t	hset_profile_descriptor = {
	hset_profile_uuids,
	sizeof(hset_profile_uuids),
	sizeof(sdp_hset_profile_t),
	hset_profile_data_valid,
	(attr_t const * const) &hset_profile_attrs
};
