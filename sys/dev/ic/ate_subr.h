/*	$NetBSD: ate_subr.h,v 1.2.6.1 2002/10/10 18:38:49 jdolecek Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
 * EEPROM allocation of AT1700/RE2000.
 */
#define FE_ATI_EEP_ADDR		0x08	/* Station address.  (8-13)	*/
#define FE_ATI_EEP_MEDIA	0x18	/* Media type.			*/
#define FE_ATI_EEP_MAGIC	0x19	/* XXX Magic.			*/
#define FE_ATI_EEP_MODEL	0x1e	/* Hardware type.		*/
#define FE_ATI_EEP_REVISION	0x1f	/* Hardware revision.		*/

#define FE_ATI_MODEL_AT1700T	0x00
#define FE_ATI_MODEL_AT1700BT	0x01
#define FE_ATI_MODEL_AT1700FT	0x02
#define FE_ATI_MODEL_AT1700AT	0x03

void ate_read_eeprom __P((bus_space_tag_t, bus_space_handle_t, u_char *));
