/*	$NetBSD: ac97var.h,v 1.4.2.1 2002/01/10 19:54:02 thorpej Exp $	*/
/*	$OpenBSD: ac97.h,v 1.4 2000/07/19 09:01:35 csapuntz Exp $	*/

/*
 * Copyright (c) 1999 Constantine Sapuntzakis
 *
 * Author:        Constantine Sapuntzakis <csapuntz@stanford.edu>
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
 * THIS SOFTWARE IS PROVIDED BY CONSTANTINE SAPUNTZAKIS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct ac97_codec_if;

/* 
 * This is the interface used to attach the AC97 compliant CODEC.
 */
enum ac97_host_flags {
	AC97_HOST_DONT_READ = 0x1,
	AC97_HOST_SWAPPED_CHANNELS = 0x2	/* l/r is reversed */
};

struct ac97_host_if {
	void  *arg;

	int (*attach)(void *arg, struct ac97_codec_if *codecif);
	int (*read)(void *arg, u_int8_t reg, u_int16_t *val);
	int (*write)(void *arg, u_int8_t reg, u_int16_t val);
	void (*reset)(void *arg);

	enum ac97_host_flags (*flags)(void *arg);
};

/*
 * This is the interface exported by the AC97 compliant CODEC
 */
struct ac97_codec_if_vtbl {
	int (*mixer_get_port)(struct ac97_codec_if *addr, mixer_ctrl_t *cp);
	int (*mixer_set_port)(struct ac97_codec_if *addr, mixer_ctrl_t *cp);
	int (*query_devinfo)(struct ac97_codec_if *addr, mixer_devinfo_t *cp);
	int (*get_portnum_by_name)(struct ac97_codec_if *addr,
				      char *class, char *device,
				      char *qualifier);
	/*
	 * The AC97 codec driver records the various port settings.  This
	 * function can be used to restore the port settings, e.g. after
	 * resume from a laptop suspend to disk.
	 */
	void (*restore_ports)(struct ac97_codec_if *addr);
};

struct ac97_codec_if {
	struct ac97_codec_if_vtbl *vtbl;
};

int ac97_attach __P((struct ac97_host_if *));
