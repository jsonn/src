/*	$NetBSD: ac97var.h,v 1.7.6.6 2004/11/14 08:15:43 skrll Exp $	*/
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

#ifndef _DEV_IC_AC97VAR_H_
#define	_DEV_IC_AC97VAR_H_

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

	int (*attach)(void *, struct ac97_codec_if *);
	int (*read)(void *, u_int8_t, u_int16_t *);
	int (*write)(void *, u_int8_t, u_int16_t);
	int (*reset)(void *);

	enum ac97_host_flags (*flags)(void *);
};

/*
 * This is the interface exported by the AC97 compliant CODEC
 */
struct ac97_codec_if_vtbl {
	int (*mixer_get_port)(struct ac97_codec_if *, mixer_ctrl_t *);
	int (*mixer_set_port)(struct ac97_codec_if *, mixer_ctrl_t *);
	int (*query_devinfo)(struct ac97_codec_if *, mixer_devinfo_t *);
	int (*get_portnum_by_name)(struct ac97_codec_if *, const char *,
	    const char *, const char *);
	/*
	 * The AC97 codec driver records the various port settings.  This
	 * function can be used to restore the port settings, e.g. after
	 * resume from a laptop suspend to disk.
	 */
	void (*restore_ports)(struct ac97_codec_if *);

	u_int16_t (*get_extcaps)(struct ac97_codec_if *);
	int (*set_rate)(struct ac97_codec_if *, int, u_long *);
	void (*set_clock)(struct ac97_codec_if *, unsigned int);
	void (*detach)(struct ac97_codec_if *);
};

struct ac97_codec_if {
	struct ac97_codec_if_vtbl *vtbl;
};

int ac97_attach(struct ac97_host_if *);

#define AC97_IS_FIXED_RATE(codec)	\
	!((codec)->vtbl->get_extcaps(codec) & AC97_EXT_AUDIO_VRA)
#define AC97_IS_4CH(codec)		\
	((codec)->vtbl->get_extcaps(codec) & AC97_EXT_AUDIO_SDAC)
#define AC97_IS_6CH(codec)		\
	(((codec)->vtbl->get_extcaps(codec) \
	& (AC97_EXT_AUDIO_SDAC | AC97_EXT_AUDIO_CDAC | AC97_EXT_AUDIO_LDAC)) \
	== (AC97_EXT_AUDIO_SDAC | AC97_EXT_AUDIO_CDAC | AC97_EXT_AUDIO_LDAC))

#endif /* _DEV_IC_AC97VAR_H_ */
