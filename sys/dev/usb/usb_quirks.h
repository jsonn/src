/*	$NetBSD: usb_quirks.h,v 1.8.2.2 2000/12/13 15:50:16 bouyer Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb_quirks.h,v 1.9 1999/11/12 23:31:03 n_hibma Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct usbd_quirks {
	u_int32_t uq_flags;	/* Device problems: */
#define UQ_NO_SET_PROTO	0x0001	/* cannot handle SET PROTOCOL. */
#define UQ_SWAP_UNICODE	0x0002	/* has some Unicode strings swapped. */
#define UQ_MS_REVZ	0x0004	/* mouse has Z-axis reversed */
#define UQ_NO_STRINGS	0x0008	/* string descriptors are broken. */
#define UQ_BAD_ADC	0x0010	/* bad audio spec version number. */
#define UQ_BUS_POWERED	0x0020	/* device is bus powered, despite claim */
#define UQ_BAD_AUDIO	0x0040	/* device claims audio class, but isn't */
#define UQ_SPUR_BUT_UP	0x0080	/* spurious mouse button up events */
#define UQ_NO_XU	0x0100	/* audio device has broken extension unit */
#define UQ_POWER_CLAIM	0x0200	/* hub lies about power status */
};

extern struct usbd_quirks usbd_no_quirk;

struct usbd_quirks *usbd_find_quirk(usb_device_descriptor_t *);
