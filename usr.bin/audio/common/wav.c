/*	$NetBSD: wav.c,v 1.3.2.2 2002/01/29 23:10:14 he Exp $	*/

/*
 * Copyright (c) 2002 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * WAV support for the audio tools; thanks go to the sox utility for
 * clearing up issues with WAV files.
 */

#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libaudio.h"

struct {
	int	wenc;
	const char *wname;
} wavencs[] = {
	{ WAVE_FORMAT_UNKNOWN, 	"Microsoft Official Unknown" },
	{ WAVE_FORMAT_PCM,	"Microsoft PCM" },
	{ WAVE_FORMAT_ADPCM,	"Microsoft ADPCM" },
	{ WAVE_FORMAT_ALAW,	"Microsoft A-law" },
	{ WAVE_FORMAT_MULAW,	"Microsoft U-law" },
	{ WAVE_FORMAT_OKI_ADPCM,"OKI ADPCM" },
	{ WAVE_FORMAT_DIGISTD,	"Digistd format" },
	{ WAVE_FORMAT_DIGIFIX,	"Digifix format" },
	{ -1, 			"?Unknown?" },
};

const char *
wav_enc_from_val(int encoding)
{
	int	i;

	for (i = 0; wavencs[i].wenc != -1; i++)
		if (wavencs[i].wenc == encoding)
			break;
	return (wavencs[i].wname);
}

/*
 * sample header is:
 *
 *   RIFF\^@^C^@WAVEfmt ^P^@^@^@^A^@^B^@D<AC>^@^@^P<B1>^B^@^D^@^P^@data^@^@^C^@^@^@^@^@^@^@^@^@^@
 *
 */
/*
 * WAV format helpers
 */
/*
 * find a .wav header, etc. returns header length on success
 */
ssize_t
audio_wav_parse_hdr(hdr, sz, enc, prec, sample, channels, datasize)
	void	*hdr;
	size_t	sz;
	int	*enc;
	int	*prec;
	int	*sample;
	int	*channels;
	size_t *datasize;
{
	char	*where = hdr, *owhere;
	wav_audioheaderpart part;
	wav_audioheaderfmt fmt;
	char	*end = (((char *)hdr) + sz);
	int	newenc, newprec;
	static const char
	    strfmt[4] = "fmt ",
	    strRIFF[4] = "RIFF",
	    strWAVE[4] = "WAVE",
	    strdata[4] = "data";
		
	if (sz < 32)
		return (AUDIO_ENOENT);

	if (strncmp(where, strRIFF, sizeof strRIFF))
		return (AUDIO_ENOENT);
	where += 8;
	if (strncmp(where, strWAVE, sizeof strWAVE))
		return (AUDIO_ENOENT);
	where += 4;

	do {
		memcpy(&part, where, sizeof part);
		owhere = where;
		where += getle32(part.len) + 8;
	} while (where < end && strncmp(part.name, strfmt, sizeof strfmt));

	/* too short ? */
	if (where + sizeof fmt > end)
		return (AUDIO_ESHORTHDR);

	memcpy(&fmt, (owhere + 8), sizeof fmt);

#if 0
printf("fmt header is:\n\t%d\ttag\n\t%d\tchannels\n\t%d\tsample rate\n\t%d\tavg_bps\n\t%d\talignment\n\t%d\tbits per sample\n", getle16(fmt.tag), getle16(fmt.channels), getle32(fmt.sample_rate), getle32(fmt.avg_bps), getle16(fmt.alignment), getle16(fmt.bits_per_sample));
#endif

	switch (getle16(fmt.tag)) {
	case WAVE_FORMAT_UNKNOWN:
	case WAVE_FORMAT_ADPCM:
	case WAVE_FORMAT_OKI_ADPCM:
	case WAVE_FORMAT_DIGISTD:
	case WAVE_FORMAT_DIGIFIX:
	case IBM_FORMAT_MULAW:
	case IBM_FORMAT_ALAW:
	case IBM_FORMAT_ADPCM:
	default:
		return (AUDIO_EWAVUNSUPP);

	case WAVE_FORMAT_PCM:
		switch (getle16(fmt.bits_per_sample)) {
		case 8:
			newprec = 8;
			break;
		case 16:
			newprec = 16;
			break;
		case 24:
			newprec = 24;
			break;
		case 32:
			newprec = 32;
			break;
		default:
			return (AUDIO_EWAVBADPCM);
		}
		if (newprec == 8)
			newenc = AUDIO_ENCODING_ULINEAR_LE;
		else
			newenc = AUDIO_ENCODING_SLINEAR_LE;
		break;
	case WAVE_FORMAT_ALAW:
		newenc = AUDIO_ENCODING_ALAW;
		newprec = 8;
		break;
	case WAVE_FORMAT_MULAW:
		newenc = AUDIO_ENCODING_ULAW;
		newprec = 8;
		break;
	}

	do {
		memcpy(&part, where, sizeof part);
#if 0
printf("part `%c%c%c%c' len = %d\n", part.name[0], part.name[1], part.name[2], part.name[3], getle32(part.len));
#endif
		owhere = where;
		where += (getle32(part.len) + 8);
	} while (where < end && strncmp(part.name, strdata, sizeof strdata));

	if ((where - getle32(part.len)) <= end) {
		if (channels)
			*channels = getle16(fmt.channels);
		if (sample)
			*sample = getle32(fmt.sample_rate);
		if (enc)
			*enc = newenc;
		if (prec)
			*prec = newprec;
		if (datasize)
			*datasize = (size_t)getle32(part.len);
		return (owhere - (char *)hdr + 8);
	}
	return (AUDIO_EWAVNODATA);
}
