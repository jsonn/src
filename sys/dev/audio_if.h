/*	$NetBSD: audio_if.h,v 1.53.2.1 2003/07/02 15:25:59 darrenr Exp $	*/

/*
 * Copyright (c) 1994 Havard Eidnes.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SYS_DEV_AUDIO_IF_H_
#define _SYS_DEV_AUDIO_IF_H_

/* check we have an audio(4) configured into kernel */
#if defined(_KERNEL_OPT)
#include "audio.h"

#if (NAUDIO == 0) && (NMIDI == 0) && (NMIDIBUS == 0)
#error "No 'audio* at audiobus?' or 'midi* at midibus?' or similar configured"
#endif

#endif /* _KERNEL_OPT */

/*
 * Generic interface to hardware driver.
 */

struct audio_softc;

struct audio_params {
	u_long	sample_rate;			/* sample rate */
	u_int	encoding;			/* e.g. mu-law, linear, etc */
	u_int	precision;			/* bits/sample */
	u_int	channels;			/* mono(1), stereo(2) */
	/* Software en/decode functions, set if SW coding required by HW */
	void	(*sw_code)(void *, u_char *, int);
	int	factor;				/* coding space change */
	int	factor_denom;			/* denominator of factor */
	/*
	 * The following four members represent what format is used in a
	 * hardware.  If hw_sample_rate != sample_rate || hw_channels !=
	 * channels, the audio framework converts data.  Encoding and
	 * precision are converted in sw_code().
	 * set_params() should set correct values to them if no conversion is
	 * needed.
	 */
	u_long	hw_sample_rate;
	u_int	hw_encoding;
	u_int	hw_precision;
	u_int	hw_channels;
};

/* The default audio mode: 8 kHz mono mu-law */
extern const struct audio_params audio_default;

struct malloc_type;
struct audio_hw_if {
	int	(*open)(void *, int);	/* open hardware */
	void	(*close)(void *);	/* close hardware */
	int	(*drain)(void *);	/* Optional: drain buffers */

	/* Encoding. */
	/* XXX should we have separate in/out? */
	int	(*query_encoding)(void *, struct audio_encoding *);

	/* Set the audio encoding parameters (record and play).
	 * Return 0 on success, or an error code if the
	 * requested parameters are impossible.
	 * The values in the params struct may be changed (e.g. rounding
	 * to the nearest sample rate.)
	 */
	int	(*set_params)(void *, int, int, struct audio_params *,
		    struct audio_params *);

	/* Hardware may have some say in the blocksize to choose */
	int	(*round_blocksize)(void *, int);

	/*
	 * Changing settings may require taking device out of "data mode",
	 * which can be quite expensive.  Also, audiosetinfo() may
	 * change several settings in quick succession.  To avoid
	 * having to take the device in/out of "data mode", we provide
	 * this function which indicates completion of settings
	 * adjustment.
	 */
	int	(*commit_settings)(void *);

	/* Start input/output routines. These usually control DMA. */
	int	(*init_output)(void *, void *, int);
	int	(*init_input)(void *, void *, int);
	int	(*start_output)(void *, void *, int,
				    void (*)(void *), void *);
	int	(*start_input)(void *, void *, int,
				   void (*)(void *), void *);
	int	(*halt_output)(void *);
	int	(*halt_input)(void *);

	int	(*speaker_ctl)(void *, int);
#define SPKR_ON		1
#define SPKR_OFF	0

	int	(*getdev)(void *, struct audio_device *);
	int	(*setfd)(void *, int);

	/* Mixer (in/out ports) */
	int	(*set_port)(void *, mixer_ctrl_t *);
	int	(*get_port)(void *, mixer_ctrl_t *);

	int	(*query_devinfo)(void *, mixer_devinfo_t *);

	/* Allocate/free memory for the ring buffer. Usually malloc/free. */
	void	*(*allocm)(void *, int, size_t, struct malloc_type *, int);
	void	(*freem)(void *, void *, struct malloc_type *);
	size_t	(*round_buffersize)(void *, int, size_t);
	paddr_t	(*mappage)(void *, void *, off_t, int);

	int	(*get_props)(void *); /* device properties */

	int	(*trigger_output)(void *, void *, void *, int,
		    void (*)(void *), void *, struct audio_params *);
	int	(*trigger_input)(void *, void *, void *, int,
		    void (*)(void *), void *, struct audio_params *);
	int	(*dev_ioctl)(void *, u_long, caddr_t, int, struct lwp *);
};

struct audio_attach_args {
	int	type;
	void	*hwif;		/* either audio_hw_if * or midi_hw_if * */
	void	*hdl;
};
#define	AUDIODEV_TYPE_AUDIO	0
#define	AUDIODEV_TYPE_MIDI	1
#define AUDIODEV_TYPE_OPL	2
#define AUDIODEV_TYPE_MPU	3
#define AUDIODEV_TYPE_AUX	4

/* Attach the MI driver(s) to the MD driver. */
struct device *audio_attach_mi(struct audio_hw_if *, void *, struct device *);
int	audioprint(void *, const char *);

/* Device identity flags */
#define SOUND_DEVICE		0
#define AUDIO_DEVICE		0x80
#define AUDIOCTL_DEVICE		0xc0
#define MIXER_DEVICE		0x10

#define AUDIOUNIT(x)		(minor(x)&0x0f)
#define AUDIODEV(x)		(minor(x)&0xf0)

#define ISDEVSOUND(x)		(AUDIODEV((x)) == SOUND_DEVICE)
#define ISDEVAUDIO(x)		(AUDIODEV((x)) == AUDIO_DEVICE)
#define ISDEVAUDIOCTL(x)	(AUDIODEV((x)) == AUDIOCTL_DEVICE)
#define ISDEVMIXER(x)		(AUDIODEV((x)) == MIXER_DEVICE)

#if !defined(__i386__) && !defined(__arm32__) && !defined(IPL_AUDIO)
#define splaudio splbio		/* XXX */
#define IPL_AUDIO IPL_BIO	/* XXX */
#endif

/*
 * USB Audio specification defines 12 channels:
 *	L R C LFE Ls Rs Lc Rc S Sl Sr T
 */
#define AUDIO_MAX_CHANNELS	12

#endif /* _SYS_DEV_AUDIO_IF_H_ */

