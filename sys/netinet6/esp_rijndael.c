/*	$NetBSD: esp_rijndael.c,v 1.18.22.1 2006/10/22 06:07:35 yamt Exp $	*/
/*	$KAME: esp_rijndael.c,v 1.4 2001/03/02 05:53:05 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esp_rijndael.c,v 1.18.22.1 2006/10/22 06:07:35 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <netinet6/ipsec.h>
#include <netinet6/esp.h>
#include <netinet6/esp_rijndael.h>

#include <crypto/rijndael/rijndael.h>

#include <net/net_osdep.h>

size_t
esp_rijndael_schedlen(const struct esp_algorithm *algo __unused)
{

	return sizeof(rijndael_ctx);
}

int
esp_rijndael_schedule(const struct esp_algorithm *algo __unused,
    struct secasvar *sav)
{
	rijndael_ctx *ctx;

	ctx = (rijndael_ctx *)sav->sched;
	rijndael_set_key(ctx,
	    (u_char *)_KEYBUF(sav->key_enc), _KEYLEN(sav->key_enc) * 8);
	return 0;
}

int
esp_rijndael_blockdecrypt(const struct esp_algorithm *algo __unused,
    struct secasvar *sav, u_int8_t *s, u_int8_t *d)
{
	rijndael_ctx *ctx;

	ctx = (rijndael_ctx *)sav->sched;
	rijndael_decrypt(ctx, s, d);
	return 0;
}

int
esp_rijndael_blockencrypt(const struct esp_algorithm *algo __unused,
    struct secasvar *sav, u_int8_t *s, u_int8_t *d)
{
	rijndael_ctx *ctx;

	ctx = (rijndael_ctx *)sav->sched;
	rijndael_encrypt(ctx, s, d);
	return 0;
}
