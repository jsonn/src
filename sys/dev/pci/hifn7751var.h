/*	$NetBSD: hifn7751var.h,v 1.1.2.2 2000/11/20 11:42:20 bouyer Exp $	*/
/*	$OpenBSD: hifn7751var.h,v 1.18 2000/06/02 22:36:45 deraadt Exp $	*/

/*
 * Invertex AEON / Hi/fn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000 Network Security Technologies, Inc.
 *			http://www.netsec.net
 *
 * Please send any comments, feedback, bug-fixes, or feature requests to
 * software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
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
 *
 */

#ifndef __DEV_PCI_HIFN7751VAR_H__
#define __DEV_PCI_HIFN7751VAR_H__

/*
 *  Length values for cryptography
 */
#define HIFN_DES_KEY_LENGTH		8
#define HIFN_3DES_KEY_LENGTH		24
#define HIFN_MAX_CRYPT_KEY_LENGTH	HIFN_3DES_KEY_LENGTH
#define HIFN_IV_LENGTH			8

/*
 *  Length values for authentication
 */
#define HIFN_MAC_KEY_LENGTH		64
#define HIFN_MD5_LENGTH			16
#define HIFN_SHA1_LENGTH		20
#define HIFN_MAC_TRUNC_LENGTH		12

#define MAX_SCATTER 64

/*
 *  hifn_command_t
 *
 *  This is the control structure used to pass commands to hifn_encrypt().
 *
 *  flags
 *  -----
 *  Flags is the bitwise "or" values for command configuration.  A single
 *  encrypt direction needs to be set:
 *
 *	HIFN_ENCODE or HIFN_DECODE
 *
 *  To use cryptography, a single crypto algorithm must be included:
 *
 *	HIFN_CRYPT_3DES or HIFN_CRYPT_DES
 *
 *  To use authentication is used, a single MAC algorithm must be included:
 *
 *	HIFN_MAC_MD5 or HIFN_MAC_SHA1
 *
 *  By default MD5 uses a 16 byte hash and SHA-1 uses a 20 byte hash.
 *  If the value below is set, hash values are truncated or assumed
 *  truncated to 12 bytes:
 *
 *	HIFN_MAC_TRUNC
 *
 *  Keys for encryption and authentication can be sent as part of a command,
 *  or the last key value used with a particular session can be retrieved
 *  and used again if either of these flags are not specified.
 *
 *	HIFN_CRYPT_NEW_KEY, HIFN_MAC_NEW_KEY
 *
 *  result_flags
 *  ------------
 *  result_flags is a bitwise "or" of result values.  The result_flags
 *  values should not be considered valid until:
 *
 *	callback routine NULL:  hifn_crypto() returns
 *	callback routine set:   callback routine called
 *
 *  Right now there is only one result flag:  HIFN_MAC_BAD
 *  It's bit is set on decode operations using authentication when a
 *  hash result does not match the input hash value.
 *  The HIFN_MAC_OK(r) macro can be used to help inspect this flag.
 *
 *  session_num
 *  -----------
 *  A number between 0 and 2048 (for DRAM models) or a number between 
 *  0 and 768 (for SRAM models).  Those who don't want to use session
 *  numbers should leave value at zero and send a new crypt key and/or
 *  new MAC key on every command.  If you use session numbers and
 *  don't send a key with a command, the last key sent for that same
 *  session number will be used.
 *
 *  Warning:  Using session numbers and multiboard at the same time
 *            is currently broken.
 *
 *  mbuf
 *  ----
 *  Either fill in the mbuf pointer and npa=0 or
 *	 fill packp[] and packl[] and set npa to > 0
 * 
 *  mac_header_skip
 *  ---------------
 *  The number of bytes of the source_buf that are skipped over before
 *  authentication begins.  This must be a number between 0 and 2^16-1
 *  and can be used by IPSec implementers to skip over IP headers.
 *  *** Value ignored if authentication not used ***
 *
 *  crypt_header_skip
 *  -----------------
 *  The number of bytes of the source_buf that are skipped over before
 *  the cryptographic operation begins.  This must be a number between 0
 *  and 2^16-1.  For IPSec, this number will always be 8 bytes larger
 *  than the auth_header_skip (to skip over the ESP header).
 *  *** Value ignored if cryptography not used ***
 *
 *  source_length
 *  -------------
 *  Length of input data including all skipped headers.  On decode
 *  operations using authentication, the length must also include the
 *  the appended MAC hash (12, 16, or 20 bytes depending on algorithm
 *  and truncation settings).
 *
 *  If encryption is used, the encryption payload must be a non-zero
 *  multiple of 8.  On encode operations, the encryption payload size
 *  is (source_length - crypt_header_skip - (MAC hash size)).  On
 *  decode operations, the encryption payload is
 *  (source_length - crypt_header_skip).
 *
 *  dest_length
 *  -----------
 *  Length of the dest buffer.  It must be at least as large as the
 *  source buffer when authentication is not used.  When authentication
 *  is used on an encode operation, it must be at least as long as the
 *  source length plus an extra 12, 16, or 20 bytes to hold the MAC
 *  value (length of mac value varies with algorithm used).  When
 *  authentication is used on decode operations, it must be at least
 *  as long as the source buffer minus 12, 16, or 20 bytes for the MAC
 *  value which is not included in the dest data.  Unlike source_length,
 *  the dest_length does not have to be exact, values larger than required
 *  are fine.
 *
 *  private_data
 *  ------------
 *  An unsigned long quantity (i.e. large enough to hold a pointer), that
 *  can be used by the callback routine if desired.
 */
struct hifn_softc;

typedef struct hifn_command {
	volatile u_int result_flags;

	u_short	session_num;
	u_int16_t base_masks, cry_masks, mac_masks;

	u_char	iv[HIFN_IV_LENGTH], *ck, mac[HIFN_MAC_KEY_LENGTH];

	struct mbuf *src_m;
	long	src_packp[MAX_SCATTER];
	int	src_packl[MAX_SCATTER];
	int	src_npa;
	int	src_l;

	struct mbuf *dst_m;
	long	dst_packp[MAX_SCATTER];
	int	dst_packl[MAX_SCATTER];
	int	dst_npa;
	int	dst_l;

	u_short mac_header_skip, mac_process_len;
	u_short crypt_header_skip, crypt_process_len;

	u_long private_data;
	struct hifn_softc *softc;
} hifn_command_t;

/*
 *  Return values for hifn_crypto()
 */
#define HIFN_CRYPTO_SUCCESS	0
#define HIFN_CRYPTO_BAD_INPUT	(-1)
#define HIFN_CRYPTO_RINGS_FULL	(-2)

/*
 *  Defines for the "result_flags" parameter of hifn_command_t.
 */
#define HIFN_MAC_BAD		1
#define HIFN_MAC_OK(r)		(!((r) & HIFN_MAC_BAD))

#ifdef _KERNEL

/**************************************************************************
 *
 *  Function:  hifn_crypto
 *
 *  Purpose:   Called by external drivers to begin an encryption on the
 *             HIFN board.
 *
 *  Blocking/Non-blocking Issues
 *  ============================
 *  The driver cannot block in hifn_crypto (no calls to tsleep) currently.
 *  hifn_crypto() returns HIFN_CRYPTO_RINGS_FULL if there is not enough
 *  room in any of the rings for the request to proceed.
 *
 *  Return Values
 *  =============
 *  0 for success, negative values on error
 *
 *  Defines for negative error codes are:
 *  
 *    HIFN_CRYPTO_BAD_INPUT  :  The passed in command had invalid settings.
 *    HIFN_CRYPTO_RINGS_FULL :  All DMA rings were full and non-blocking
 *                              behaviour was requested.
 *
 *************************************************************************/

/*
 * Convert back and forth from 'sid' to 'card' and 'session'
 */
#define HIFN_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define HIFN_SESSION(sid)	((sid) & 0x000007ff)
#define HIFN_SID(crd,ses)	(((crd) << 28) | ((ses) & 0x7ff))

#endif /* _KERNEL */

#endif /* __DEV_PCI_HIFN7751VAR_H__ */
