/*	$NetBSD: i2c_bitbang.c,v 1.1.4.3 2004/09/18 14:45:47 skrll Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Common module for bit-bang'ing an I2C bus.
 */

#include <sys/param.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>

#define	SET(x)		ops->ibo_set_bits(v, (x))
#define	DIR(x)		ops->ibo_set_dir(v, (x))
#define	READ		ops->ibo_read_bits(v)

#define	SDA		ops->ibo_bits[I2C_BIT_SDA]	/* i2c signal */
#define	SCL		ops->ibo_bits[I2C_BIT_SCL]	/* i2c signal */
#define	OUTPUT		ops->ibo_bits[I2C_BIT_OUTPUT]	/* SDA is output */
#define	INPUT		ops->ibo_bits[I2C_BIT_INPUT]	/* SDA is input */

/*ARGSUSED*/
int
i2c_bitbang_send_start(void *v, int flags, i2c_bitbang_ops_t ops)
{

	DIR(OUTPUT);

	SET(SDA | SCL);
	delay(5);		/* bus free time (4.7 uS) */
	SET(      SCL);
	delay(4);		/* start hold time (4.0 uS) */
	SET(        0);
	delay(5);		/* clock low time (4.7 uS) */

	return (0);
}

/*ARGSUSED*/
int
i2c_bitbang_send_stop(void *v, int flags, i2c_bitbang_ops_t ops)
{

	DIR(OUTPUT);

	SET(      SCL);
	delay(4);		/* stop setup time (4.0 uS) */
	SET(SDA | SCL);

	return (0);
}

int
i2c_bitbang_initiate_xfer(void *v, i2c_addr_t addr, int flags,
    i2c_bitbang_ops_t ops)
{
	int i2caddr;

	/* XXX Only support 7-bit addressing for now. */
	if ((addr & 0x78) == 0x78)
		return (EINVAL);

	i2caddr = (addr << 1) | ((flags & I2C_F_READ) ? 1 : 0);

	(void) i2c_bitbang_send_start(v, flags, ops);
	return (i2c_bitbang_write_byte(v, i2caddr, flags & ~I2C_F_STOP, ops));
}

int
i2c_bitbang_read_byte(void *v, uint8_t *valp, int flags,
    i2c_bitbang_ops_t ops)
{
	int i;
	uint8_t val = 0;
	uint32_t bit;

	DIR(INPUT);
	SET(SDA      );

	for (i = 0; i < 8; i++) {
		val <<= 1;
		SET(SDA | SCL);
		delay(4);	/* clock high time (4.0 uS) */
		if (READ & SDA)
			val |= 1;
		SET(SDA      );
		delay(5);	/* clock low time (4.7 uS) */
	}

	bit = (flags & I2C_F_LAST) ? SDA : 0;
	DIR(OUTPUT);
	SET(bit      );
	delay(1);	/* data setup time (250 nS) */
	SET(bit | SCL);
	delay(4);	/* clock high time (4.0 uS) */
	SET(bit      );
	delay(5);	/* clock low time (4.7 uS) */

	DIR(INPUT);
	SET(SDA      );
	delay(5);

	if ((flags & (I2C_F_STOP | I2C_F_LAST)) == (I2C_F_STOP | I2C_F_LAST))
		(void) i2c_bitbang_send_stop(v, flags, ops);

	*valp = val;
	return (0);
}

int
i2c_bitbang_write_byte(void *v, uint8_t val, int flags,
    i2c_bitbang_ops_t ops)
{
	uint32_t bit;
	uint8_t mask;
	int error;

	DIR(OUTPUT);

	for (mask = 0x80; mask != 0; mask >>= 1) {
		bit = (val & mask) ? SDA : 0;
		SET(bit      );
		delay(1);	/* data setup time (250 nS) */
		SET(bit | SCL);
		delay(4);	/* clock high time (4.0 uS) */
		SET(bit      );
		delay(5);	/* clock low time (4.7 uS) */
	}

	DIR(INPUT);

	SET(SDA      );
	delay(5);
	SET(SDA | SCL);
	delay(4);
	error = (READ & SDA) ? EIO : 0;
	SET(SDA      );
	delay(5);

	if (flags & I2C_F_STOP)
		(void) i2c_bitbang_send_stop(v, flags, ops);

	return (error);
}
