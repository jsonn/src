/*      $NetBSD: ac97.c,v 1.20.2.3 2002/09/06 08:44:06 jdolecek Exp $ */
/*	$OpenBSD: ac97.c,v 1.8 2000/07/19 09:01:35 csapuntz Exp $	*/

/*
 * Copyright (c) 1999, 2000 Constantine Sapuntzakis
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE
 */

/* Partially inspired by FreeBSD's sys/dev/pcm/ac97.c. It came with
   the following copyright */

/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ac97.c,v 1.20.2.3 2002/09/06 08:44:06 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

static const struct audio_mixer_enum ac97_on_off = { 2,
					       { { { AudioNoff } , 0 },
					         { { AudioNon }  , 1 } }};


static const struct audio_mixer_enum ac97_mic_select = { 2,
					       { { { AudioNmicrophone "0" }, 
						   0 },
					         { { AudioNmicrophone "1" }, 
						   1 } }};

static const struct audio_mixer_enum ac97_mono_select = { 2,
					       { { { AudioNmixerout },
						   0 },
					         { { AudioNmicrophone }, 
						   1 } }};

static const struct audio_mixer_enum ac97_source = { 8,
					       { { { AudioNmicrophone } , 0 },
						 { { AudioNcd }, 1 },
						 { { "video" }, 2 },
						 { { AudioNaux }, 3 },
						 { { AudioNline }, 4 },
						 { { AudioNmixerout }, 5 },
						 { { AudioNmixerout AudioNmono }, 6 },
						 { { "phone" }, 7 }}};

/*
 * Due to different values for each source that uses these structures, 
 * the ac97_query_devinfo function sets delta in mixer_devinfo_t using
 * ac97_source_info.bits.
 */
static const struct audio_mixer_value ac97_volume_stereo = { { AudioNvolume }, 
						       2 };

static const struct audio_mixer_value ac97_volume_mono = { { AudioNvolume }, 
						     1 };

#define WRAP(a)  &a, sizeof(a)

const struct ac97_source_info {
	const char *class;
	const char *device;
	const char *qualifier;
	int  type;

	const void *info;
	int  info_size;

	u_int8_t  reg;
	u_int16_t default_value;
	u_int8_t  bits:3;
	u_int8_t  ofs:4;
	u_int8_t  mute:1;
	u_int8_t  polarity:1;   /* Does 0 == MAX or MIN */

	int  prev;
	int  next;	
	int  mixer_class;
} source_info[] = {
	{ AudioCinputs ,            NULL,           NULL,    AUDIO_MIXER_CLASS,
	},
	{ AudioCoutputs,            NULL,           NULL,    AUDIO_MIXER_CLASS,
	},
	{ AudioCrecord ,            NULL,           NULL,    AUDIO_MIXER_CLASS,
	},
	/* Stereo master volume*/
	{ AudioCoutputs,     AudioNmaster,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo), 
	  AC97_REG_MASTER_VOLUME, 0x8000, 5, 0, 1,
	},
	/* Mono volume */
	{ AudioCoutputs,       AudioNmono,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono),
	  AC97_REG_MASTER_VOLUME_MONO, 0x8000, 6, 0, 1,
	},
	{ AudioCoutputs,       AudioNmono,AudioNsource,   AUDIO_MIXER_ENUM,
	  WRAP(ac97_mono_select),
	  AC97_REG_GP, 0x0000, 1, 9, 0,
	},
	/* Headphone volume */
	{ AudioCoutputs,  AudioNheadphone,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_HEADPHONE_VOLUME, 0x8000, 6, 0, 1, 
	},
	/* Tone */
	{ AudioCoutputs,           "tone",        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_MASTER_TONE, 0x0f0f, 4, 0, 0,
	},
	/* PC Beep Volume */
	{ AudioCinputs,     AudioNspeaker,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_PCBEEP_VOLUME, 0x0000, 4, 1, 1,
	},
	/* Phone */
	{ AudioCinputs,           "phone",        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_PHONE_VOLUME, 0x8008, 5, 0, 1,
	},
	/* Mic Volume */
	{ AudioCinputs,  AudioNmicrophone,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_MIC_VOLUME, 0x8008, 5, 0, 1,
	},
	{ AudioCinputs,  AudioNmicrophone, AudioNpreamp,   AUDIO_MIXER_ENUM,
	  WRAP(ac97_on_off),
	  AC97_REG_MIC_VOLUME, 0x8008, 1, 6, 0,
	},
	{ AudioCinputs,  AudioNmicrophone, AudioNsource,   AUDIO_MIXER_ENUM,
	  WRAP(ac97_mic_select),
	  AC97_REG_GP, 0x0000, 1, 8, 0,
	},
	/* Line in Volume */
	{ AudioCinputs,        AudioNline,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_LINEIN_VOLUME, 0x8808, 5, 0, 1,
	},
	/* CD Volume */
	{ AudioCinputs,          AudioNcd,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_CD_VOLUME, 0x8808, 5, 0, 1,
	},
	/* Video Volume */
	{ AudioCinputs,           "video",        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_VIDEO_VOLUME, 0x8808, 5, 0, 1,
	},
	/* AUX volume */
	{ AudioCinputs,         AudioNaux,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_AUX_VOLUME, 0x8808, 5, 0, 1,
	},
	/* PCM out volume */
	{ AudioCinputs,         AudioNdac,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_PCMOUT_VOLUME, 0x8808, 5, 0, 1,
	},
	/* Record Source - some logic for this is hard coded - see below */
	{ AudioCrecord,      AudioNsource,        NULL,    AUDIO_MIXER_ENUM,
	  WRAP(ac97_source),
	  AC97_REG_RECORD_SELECT, 0x0000, 3, 0, 0,
	},
	/* Record Gain */
	{ AudioCrecord,      AudioNvolume,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_RECORD_GAIN, 0x8000, 4, 0, 1,
	},
	/* Record Gain mic */
	{ AudioCrecord,  AudioNmicrophone,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_RECORD_GAIN_MIC, 0x8000, 4, 0, 1, 1,
	},
	/* */
	{ AudioCoutputs,   AudioNloudness,        NULL,    AUDIO_MIXER_ENUM,
	  WRAP(ac97_on_off),
	  AC97_REG_GP, 0x0000, 1, 12, 0,
	},
	{ AudioCoutputs,    AudioNspatial,        NULL,    AUDIO_MIXER_ENUM,
	  WRAP(ac97_on_off),
	  AC97_REG_GP, 0x0000, 1, 13, 0,
	},
	{ AudioCoutputs,    AudioNspatial,    "center",    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_3D_CONTROL, 0x0000, 4, 8, 0, 1,
	},
	{ AudioCoutputs,    AudioNspatial,     "depth",    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_3D_CONTROL, 0x0000, 4, 0, 0, 1,
	},

	/* Missing features: Simulated Stereo, POP, Loopback mode */
} ;

#define SOURCE_INFO_SIZE (sizeof(source_info)/sizeof(source_info[0]))

/*
 * Check out http://developer.intel.com/pc-supp/platform/ac97/ for
 * information on AC-97
 */

struct ac97_softc {
	struct ac97_codec_if codec_if;

	struct ac97_host_if *host_if;

	struct ac97_source_info source_info[2 * SOURCE_INFO_SIZE];
	int num_source_info;

	enum ac97_host_flags host_flags;

	u_int16_t shadow_reg[128];
};

int ac97_mixer_get_port __P((struct ac97_codec_if *self, mixer_ctrl_t *cp));
int ac97_mixer_set_port __P((struct ac97_codec_if *self, mixer_ctrl_t *));
int ac97_query_devinfo __P((struct ac97_codec_if *self, mixer_devinfo_t *));
int ac97_get_portnum_by_name __P((struct ac97_codec_if *, char *, char *,
				  char *));
void ac97_restore_shadow __P((struct ac97_codec_if *self));

struct ac97_codec_if_vtbl ac97civ = {
	ac97_mixer_get_port, 
	ac97_mixer_set_port,
	ac97_query_devinfo,
	ac97_get_portnum_by_name,
	ac97_restore_shadow,
};

static const struct ac97_codecid {
	u_int32_t id;
	const char *name;
} ac97codecid[] = {
	{ AC97_CODEC_ID('A', 'D', 'S', 64),	"Analog Devices AD1881" },
	{ AC97_CODEC_ID('A', 'D', 'S', 72),	"Analog Devices AD1881A" },
	{ AC97_CODEC_ID('A', 'D', 'S', 96),	"Analog Devices AD1885" },
	{ AC97_CODEC_ID('A', 'K', 'M', 0),	"Asahi Kasei AK4540"	},
	{ AC97_CODEC_ID('A', 'K', 'M', 2),	"Asahi Kasei AK4543"	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0),	"Crystal CS4297"	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 3),	"Crystal CS4297"	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 19),	"Crystal CS4297A"	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 35),	"Crystal CS4298",	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 43),	"Crystal CS4294",	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 49),	"Crystal CS4299",	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 51),	"Crystal CS4298A",	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 52),	"Crystal CS4299",	},
	{ AC97_CODEC_ID('N', 'S', 'C', 49), "National Semiconductor LM4549", },
	{ AC97_CODEC_ID('S', 'I', 'L', 34),	"Silicon Laboratory Si3036", },
	{ AC97_CODEC_ID('S', 'I', 'L', 35),	"Silicon Laboratory Si3038", },
	{ AC97_CODEC_ID('T', 'R', 'A', 2),	"TriTech TR28022",	},
	{ AC97_CODEC_ID('T', 'R', 'A', 3),	"TriTech TR28023",	},
	{ AC97_CODEC_ID('T', 'R', 'A', 6),	"TriTech TR28026",	},
	{ AC97_CODEC_ID('T', 'R', 'A', 8),	"TriTech TR28028",	},
	{ AC97_CODEC_ID('T', 'R', 'A', 35),	"TriTech unknown",	},
	{ AC97_CODEC_ID('W', 'M', 'L', 0),	"Wolfson WM9704",	},
	{ AC97_CODEC_ID('W', 'M', 'L', 3),	"Wolfson WM9707",	},
	{ 0x45838308,				"ESS Technology ES1921", },
	{ 0x83847600,				"SigmaTel STAC9700",	},
	{ 0x83847604,				"SigmaTel STAC9701/3/4/5", },
	{ 0x83847605,				"SigmaTel STAC9704", 	},
	{ 0x83847608,				"SigmaTel STAC9708", 	},
	{ 0x83847609,				"SigmaTel STAC9721/23",	},
	{ 0x83847644,				"SigmaTel STAC9744/45",	},
	{ 0x83847684,				"SigmaTel STAC9783/84",	},
	{ 0,					NULL,			}
};

static const char * const ac97enhancement[] = {
	"no 3D stereo",
	"Analog Devices Phat Stereo",
	"Creative",
	"National Semi 3D",
	"Yamaha Ymersion",
	"BBE 3D",
	"Crystal Semi 3D",
	"Qsound QXpander",
	"Spatializer 3D",
	"SRS 3D",
	"Platform Tech 3D",
	"AKM 3D",
	"Aureal",
	"AZTECH 3D",
	"Binaura 3D",
	"ESS Technology",
	"Harman International VMAx",
	"Nvidea 3D",
	"Philips Incredible Sound",
	"Texas Instruments' 3D",
	"VLSI Technology 3D",
	"TriTech 3D",
	"Realtek 3D",
	"Samsung 3D",
	"Wolfson Microelectronics 3D",
	"Delta Integration 3D",
	"SigmaTel 3D",
	"Unknown 3D",
	"Rockwell 3D",
	"Unknown 3D",
	"Unknown 3D",
	"Unknown 3D",
};

static const char * const ac97feature[] = {
	"mic channel",
	"reserved",
	"tone",
	"simulated stereo",
	"headphone",
	"bass boost",
	"18 bit DAC",
	"20 bit DAC",
	"18 bit ADC",
	"20 bit ADC"
};


int ac97_str_equal __P((const char *, const char *));
void ac97_setup_source_info __P((struct ac97_softc *));
void ac97_read __P((struct ac97_softc *, u_int8_t, u_int16_t *));
void ac97_setup_defaults __P((struct ac97_softc *));
int ac97_write __P((struct ac97_softc *, u_int8_t, u_int16_t));

/* #define AC97_DEBUG 10 */

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ac97debug) printf x
#define DPRINTFN(n,x)	if (ac97debug>(n)) printf x
#ifdef AC97_DEBUG
int	ac97debug = AC97_DEBUG;
#else
int	ac97debug = 0;
#endif
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

void
ac97_read(as, reg, val)
	struct ac97_softc *as;
	u_int8_t reg;
	u_int16_t *val;
{
	int error;

	if (as->host_flags & AC97_HOST_DONT_READ &&
	    (reg != AC97_REG_VENDOR_ID1 && reg != AC97_REG_VENDOR_ID2 &&
	     reg != AC97_REG_RESET)) {
		*val = as->shadow_reg[reg >> 1];
		return;
	}

	if ((error = as->host_if->read(as->host_if->arg, reg, val))) {
		*val = as->shadow_reg[reg >> 1];
	}
}

int
ac97_write(as, reg, val)
	struct ac97_softc *as;
	u_int8_t reg;
	u_int16_t val;
{

	as->shadow_reg[reg >> 1] = val;

	return (as->host_if->write(as->host_if->arg, reg, val));
}

void
ac97_setup_defaults(as)
	struct ac97_softc *as;
{
	int idx;
	const struct ac97_source_info *si;

	memset(as->shadow_reg, 0, sizeof(as->shadow_reg));

	for (idx = 0; idx < SOURCE_INFO_SIZE; idx++) {
		si = &source_info[idx];
		ac97_write(as, si->reg, si->default_value);
	}
}

void
ac97_restore_shadow(self)
	struct ac97_codec_if *self;
{
	struct ac97_softc *as = (struct ac97_softc *) self;
	int idx;
	const struct ac97_source_info *si;

	for (idx = 0; idx < SOURCE_INFO_SIZE; idx++) {
		si = &source_info[idx];
		ac97_write(as, si->reg, as->shadow_reg[si->reg >> 1]);
	}
}

int
ac97_str_equal(a, b)
	const char *a, *b;
{
	return ((a == b) || (a && b && (!strcmp(a, b))));
}

void
ac97_setup_source_info(as)
	struct ac97_softc *as;
{
	int idx, ouridx;
	struct ac97_source_info *si, *si2; 

	for (idx = 0, ouridx = 0; idx < SOURCE_INFO_SIZE; idx++) {
		si = &as->source_info[ouridx];

		memcpy(si, &source_info[idx], sizeof(*si));

		switch (si->type) {
		case AUDIO_MIXER_CLASS:
		        si->mixer_class = ouridx;
			ouridx++;
			break;
		case AUDIO_MIXER_VALUE:
			/* Todo - Test to see if it works */
			ouridx++;

			/* Add an entry for mute, if necessary */
			if (si->mute) {
				si = &as->source_info[ouridx];
				memcpy(si, &source_info[idx], sizeof(*si));
				si->qualifier = AudioNmute;
				si->type = AUDIO_MIXER_ENUM;
				si->info = &ac97_on_off;
				si->info_size = sizeof(ac97_on_off);
				si->bits = 1;
				si->ofs = 15;
				si->mute = 0;
				si->polarity = 0;
				ouridx++;
			}
			break;
		case AUDIO_MIXER_ENUM:
			/* Todo - Test to see if it works */
			ouridx++;
			break;
		default:
			printf ("ac97: shouldn't get here\n");
			break;
		}
	}

	as->num_source_info = ouridx;

	for (idx = 0; idx < as->num_source_info; idx++) {
		int idx2, previdx;

		si = &as->source_info[idx];

		/* Find mixer class */
		for (idx2 = 0; idx2 < as->num_source_info; idx2++) {
			si2 = &as->source_info[idx2];

			if (si2->type == AUDIO_MIXER_CLASS && 
			    ac97_str_equal(si->class,
					   si2->class)) {
				si->mixer_class = idx2;
			}
		}


		/* Setup prev and next pointers */
		if (si->prev != 0)
			continue;

		if (si->qualifier)
			continue;

		si->prev = AUDIO_MIXER_LAST;
		previdx = idx;

		for (idx2 = 0; idx2 < as->num_source_info; idx2++) {
			if (idx2 == idx)
				continue;

			si2 = &as->source_info[idx2];

			if (!si2->prev &&
			    ac97_str_equal(si->class, si2->class) &&
			    ac97_str_equal(si->device, si2->device)) {
				as->source_info[previdx].next = idx2;
				as->source_info[idx2].prev = previdx;
				
				previdx = idx2;
			}
		}

		as->source_info[previdx].next = AUDIO_MIXER_LAST;
	}
}

int 
ac97_attach(host_if)
	struct ac97_host_if *host_if;
{
	struct ac97_softc *as;
	struct device *sc_dev = (struct device *)host_if->arg;
	int error, i, j;
	u_int16_t id1, id2, caps;
	u_int32_t id;
	mixer_ctrl_t ctl;
	
	as = malloc(sizeof(struct ac97_softc), M_DEVBUF, M_WAITOK|M_ZERO);

	if (as == NULL)
		return (ENOMEM);

	as->codec_if.vtbl = &ac97civ;
	as->host_if = host_if;

	if ((error = host_if->attach(host_if->arg, &as->codec_if))) {
		free (as, M_DEVBUF);
		return (error);
	}

	host_if->reset(host_if->arg);

	host_if->write(host_if->arg, AC97_REG_POWER, 0);
	host_if->write(host_if->arg, AC97_REG_RESET, 0);

	if (host_if->flags)
		as->host_flags = host_if->flags(host_if->arg);

	ac97_setup_defaults(as);
	ac97_read(as, AC97_REG_VENDOR_ID1, &id1);
	ac97_read(as, AC97_REG_VENDOR_ID2, &id2);
	ac97_read(as, AC97_REG_RESET, &caps);

	id = (id1 << 16) | id2;

	printf("%s: ", sc_dev->dv_xname);

	for (i = 0; ; i++) {
		if (ac97codecid[i].id == 0) {
			char pnp[4];

			AC97_GET_CODEC_ID(id, pnp);
#define ISASCII(c) ((c) >= ' ' && (c) < 0x7f)
			if (ISASCII(pnp[0]) && ISASCII(pnp[1]) &&
			    ISASCII(pnp[2]))
				printf("%c%c%c%d", pnp[0], pnp[1], pnp[2],
				    pnp[3]);
			else
				printf("unknown (0x%08x)", id);
			break;
		}
		if (ac97codecid[i].id == id) {
			printf("%s", ac97codecid[i].name);
			break;
		}
	}
	printf(" codec; ");
	for (i = j = 0; i < 10; i++) {
		if (caps & (1 << i)) {
			printf("%s%s", j? ", " : "", ac97feature[i]);
			j++;
		}
	}

	printf("%s%s\n", j? ", " : "", ac97enhancement[(caps >> 10) & 0x1f]);

	ac97_setup_source_info(as);

	/* Just enable the DAC and master volumes by default */
	memset(&ctl, 0, sizeof(ctl));

	ctl.type = AUDIO_MIXER_ENUM;
	ctl.un.ord = 0;  /* off */
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCoutputs,
					   AudioNmaster, AudioNmute);
	ac97_mixer_set_port(&as->codec_if, &ctl);
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCinputs,
					   AudioNdac, AudioNmute);

	ac97_mixer_set_port(&as->codec_if, &ctl);
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCrecord,
					   AudioNvolume, AudioNmute);
	ac97_mixer_set_port(&as->codec_if, &ctl);

	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCrecord,
					   AudioNsource, NULL);
	ctl.type = AUDIO_MIXER_ENUM;
	ctl.un.ord = 0;
	ac97_mixer_set_port(&as->codec_if, &ctl);

	return (0);
}


int 
ac97_query_devinfo(codec_if, dip)
	struct ac97_codec_if *codec_if;
	mixer_devinfo_t *dip;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;

	if (dip->index < as->num_source_info) {
		struct ac97_source_info *si = &as->source_info[dip->index];
		const char *name;

		dip->type = si->type;
		dip->mixer_class = si->mixer_class;
		dip->prev = si->prev;
		dip->next = si->next;
		
		if (si->qualifier)
			name = si->qualifier;
		else if (si->device)
			name = si->device;
		else if (si->class)
			name = si->class;
		else
			name = 0;
		
		if (name)
			strcpy(dip->label.name, name);

		memcpy(&dip->un, si->info, si->info_size);

		/* Set the delta for volume sources */
		if (dip->type == AUDIO_MIXER_VALUE)
			dip->un.v.delta = 1 << (8 - si->bits);

		return (0);
	}

	return (ENXIO);
}



int
ac97_mixer_set_port(codec_if, cp)
	struct ac97_codec_if *codec_if;
	mixer_ctrl_t *cp;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;
	struct ac97_source_info *si = &as->source_info[cp->dev];
	u_int16_t mask;
	u_int16_t val, newval;
	int error;

	if (cp->dev < 0 || cp->dev >= as->num_source_info)
		return (EINVAL);

	if (cp->type != si->type)
		return (EINVAL);

	ac97_read(as, si->reg, &val);

	DPRINTFN(5, ("read(%x) = %x\n", si->reg, val));

	mask = (1 << si->bits) - 1;

	switch (cp->type) {
	case AUDIO_MIXER_ENUM:
		if (cp->un.ord > mask || cp->un.ord < 0)
			return (EINVAL);

		newval = (cp->un.ord << si->ofs);
		if (si->reg == AC97_REG_RECORD_SELECT) {
			newval |= (newval << (8 + si->ofs));
			mask |= (mask << 8);
		}
		break;
	case AUDIO_MIXER_VALUE:
	{
		const struct audio_mixer_value *value = si->info;
		u_int16_t  l, r;

		if ((cp->un.value.num_channels <= 0) ||
		    (cp->un.value.num_channels > value->num_channels)) 
			return (EINVAL);

		if (cp->un.value.num_channels == 1) {
			l = r = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else {
			if (!(as->host_flags & AC97_HOST_SWAPPED_CHANNELS)) {
				l = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				r = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			} else {	/* left/right is reversed here */
				r = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				l = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			}

		}

		if (!si->polarity) {
			l = 255 - l;
			r = 255 - r;
		}
		
		l = l >> (8 - si->bits);
		r = r >> (8 - si->bits);

		newval = ((l & mask) << si->ofs);
		if (value->num_channels == 2) {
			newval |= ((r & mask) << (si->ofs + 8));
			mask |= (mask << 8);
		}

		break;
	}
	default:
		return (EINVAL);
	}

	mask = mask << si->ofs;
	error = ac97_write(as, si->reg, (val & ~mask) | newval);
	if (error)
		return (error);

	return (0);
}

int
ac97_get_portnum_by_name(codec_if, class, device, qualifier)
	struct ac97_codec_if *codec_if;
	char *class, *device, *qualifier;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;
	int idx;

	for (idx = 0; idx < as->num_source_info; idx++) {
		struct ac97_source_info *si = &as->source_info[idx];
		if (ac97_str_equal(class, si->class) &&
		    ac97_str_equal(device, si->device) &&
		    ac97_str_equal(qualifier, si->qualifier))
			return (idx);
	}

	return (-1);
}

int
ac97_mixer_get_port(codec_if, cp)
	struct ac97_codec_if *codec_if;
	mixer_ctrl_t *cp;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;
	struct ac97_source_info *si = &as->source_info[cp->dev];
	u_int16_t mask;
	u_int16_t val;

	if (cp->dev < 0 || cp->dev >= as->num_source_info)
		return (EINVAL);

	if (cp->type != si->type)
		return (EINVAL);

	ac97_read(as, si->reg, &val);

	DPRINTFN(5, ("read(%x) = %x\n", si->reg, val));

	mask = (1 << si->bits) - 1;

	switch (cp->type) {
	case AUDIO_MIXER_ENUM:
		cp->un.ord = (val >> si->ofs) & mask;
		DPRINTFN(4, ("AUDIO_MIXER_ENUM: %x %d %x %d\n", val, si->ofs, mask, cp->un.ord));
		break;
	case AUDIO_MIXER_VALUE:
	{
		const struct audio_mixer_value *value = si->info;
		u_int16_t  l, r;

		if ((cp->un.value.num_channels <= 0) ||
		    (cp->un.value.num_channels > value->num_channels)) 
			return (EINVAL);

		if (value->num_channels == 1) {
			l = r = (val >> si->ofs) & mask;
		} else {
			if (!(as->host_flags & AC97_HOST_SWAPPED_CHANNELS)) {
				l = (val >> si->ofs) & mask;
				r = (val >> (si->ofs + 8)) & mask;
			} else {	/* host has reversed channels */
				r = (val >> si->ofs) & mask;
				l = (val >> (si->ofs + 8)) & mask;
			}
		}

		l = (l << (8 - si->bits));
		r = (r << (8 - si->bits));
		if (!si->polarity) {
			l = 255 - l;
			r = 255 - r;
		}

		/* The EAP driver averages l and r for stereo
		   channels that are requested in MONO mode. Does this
		   make sense? */
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = l;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
		}

		break;
	}
	default:
		return (EINVAL);
	}

	return (0);
}

