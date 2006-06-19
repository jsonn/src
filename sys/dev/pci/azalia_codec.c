/*	$NetBSD: azalia_codec.c,v 1.8.2.1 2006/06/19 04:01:35 chap Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: azalia_codec.c,v 1.8.2.1 2006/06/19 04:01:35 chap Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/null.h>
#include <sys/systm.h>
#include <dev/pci/azalia.h>

#define XNAME(co)	(((device_t)co->az)->dv_xname)
#ifdef MAX_VOLUME_255
# define MIXER_DELTA(n)	(AUDIO_MAX_GAIN / (n))
#else
# define MIXER_DELTA(n)	(1)
#endif
#define AZ_CLASS_INPUT	0
#define AZ_CLASS_OUTPUT	1
#define AZ_CLASS_RECORD	2
#define ENUM_OFFON	.un.e={2, {{{AudioNoff}, 0}, {{AudioNon}, 1}}}
#define ENUM_IO		.un.e={2, {{{"input"}, 0}, {{"output"}, 1}}}
#define AzaliaNfront	"front"
#define AzaliaNclfe	"clfe"
#define AzaliaNside	"side"


static int	generic_codec_init_dacgroup(codec_t *);
static int	generic_codec_add_dacgroup(codec_t *, int, uint32_t);
static int	generic_codec_find_pin(const codec_t *, int, int, uint32_t);
static int	generic_codec_find_dac(const codec_t *, int, int);

static int	generic_mixer_init(codec_t *);
static int	generic_mixer_fix_indexes(codec_t *);
static int	generic_mixer_default(codec_t *);
static int	generic_mixer_delete(codec_t *);
static int	generic_mixer_ensure_capacity(codec_t *, size_t);
static int	generic_mixer_get(const codec_t *, nid_t, int, mixer_ctrl_t *);
static int	generic_mixer_set(codec_t *, nid_t, int, const mixer_ctrl_t *);
static u_char	generic_mixer_from_device_value
	(const codec_t *, nid_t, int, uint32_t );
static uint32_t	generic_mixer_to_device_value
	(const codec_t *, nid_t, int, u_char);
static uint32_t	generic_mixer_max(const codec_t *, nid_t, int);
static boolean_t generic_mixer_validate_value
	(const codec_t *, nid_t, int, u_char);
static int	generic_set_port(codec_t *, mixer_ctrl_t *);
static int	generic_get_port(codec_t *, mixer_ctrl_t *);

static int	alc260_mixer_init(codec_t *);
static int	alc260_init_dacgroup(codec_t *);
static int	alc260_set_port(codec_t *, mixer_ctrl_t *);
static int	alc880_init_dacgroup(codec_t *);
static int	alc882_init_dacgroup(codec_t *);
static int	alc882_mixer_init(codec_t *);
static int	alc882_set_port(codec_t *, mixer_ctrl_t *);
static int	alc882_get_port(codec_t *, mixer_ctrl_t *);
static int	ad1981hd_init_widget(const codec_t *, widget_t *, nid_t);
static int	stac9221_init_dacgroup(codec_t *);
static int	stac9220_mixer_init(codec_t *);


int
azalia_codec_init_vtbl(codec_t *this)
{
	/**
	 * We can refer this->vid and this->subid.
	 */
	DPRINTF(("%s: vid=%08x subid=%08x\n", __func__, this->vid, this->subid));
	this->name = NULL;
	this->init_dacgroup = generic_codec_init_dacgroup;
	this->mixer_init = generic_mixer_init;
	this->mixer_delete = generic_mixer_delete;
	this->set_port = generic_set_port;
	this->get_port = generic_get_port;
	switch (this->vid) {
	case 0x10ec0260:
		this->name = "Realtek ALC260";
		this->mixer_init = alc260_mixer_init;
		this->init_dacgroup = alc260_init_dacgroup;
		this->set_port = alc260_set_port;
		break;
	case 0x10ec0880:
		this->name = "Realtek ALC880";
		this->init_dacgroup = alc880_init_dacgroup;
		break;
	case 0x10ec0882:
		this->name = "Realtek ALC882";
		this->init_dacgroup = alc882_init_dacgroup;
		this->mixer_init = alc882_mixer_init;
		this->get_port = alc882_get_port;
		this->set_port = alc882_set_port;
		break;
	case 0x11d41981:
		/* http://www.analog.com/en/prod/0,2877,AD1981HD,00.html */
		this->name = "Analog Devices AD1981HD";
		this->init_widget = ad1981hd_init_widget;
		break;
	case 0x11d41983:
		/* http://www.analog.com/en/prod/0,2877,AD1983,00.html */
		this->name = "Analog Devices AD1983";
		break;
	case 0x83847680:
		this->name = "Sigmatel STAC9221";
		this->init_dacgroup = stac9221_init_dacgroup;
		break;
	case 0x83847683:
		this->name = "Sigmatel STAC9221D";
		this->init_dacgroup = stac9221_init_dacgroup;
		break;
	case 0x83847690:
		this->name = "Sigmatel STAC9220";
		this->mixer_init = stac9220_mixer_init;
		break;
	}
	return 0;
}

/* ----------------------------------------------------------------
 * functions for generic codecs
 * ---------------------------------------------------------------- */

static int
generic_codec_init_dacgroup(codec_t *this)
{
	int i, j, assoc, group;

	/*
	 * grouping DACs
	 *   [0] the lowest assoc DACs
	 *   [1] the lowest assoc digital outputs
	 *   [2] the 2nd assoc DACs
	 *      :
	 */
	this->dacs.ngroups = 0;
	for (assoc = 0; assoc < CORB_CD_ASSOCIATION_MAX; assoc++) {
		generic_codec_add_dacgroup(this, assoc, 0);
		generic_codec_add_dacgroup(this, assoc, COP_AWCAP_DIGITAL);
	}

	/* find DACs which do not connect with any pins by default */
	DPRINTF(("%s: find non-connected DACs\n", __func__));
	FOR_EACH_WIDGET(this, i) {
		boolean_t found;

		if (this->w[i].type != COP_AWTYPE_AUDIO_OUTPUT)
			continue;
		found = FALSE;
		for (group = 0; group < this->dacs.ngroups; group++) {
			for (j = 0; j < this->dacs.groups[group].nconv; j++) {
				if (i == this->dacs.groups[group].conv[j]) {
					found = TRUE;
					group = this->dacs.ngroups;
					break;
				}
			}
		}
		if (found)
			continue;
		if (this->dacs.ngroups >= 32)
			break;
		this->dacs.groups[this->dacs.ngroups].nconv = 1;
		this->dacs.groups[this->dacs.ngroups].conv[0] = i;
		this->dacs.ngroups++;
	}
	this->dacs.cur = 0;

	/* enumerate ADCs */
	this->adcs.ngroups = 0;
	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_AUDIO_INPUT)
			continue;
		this->adcs.groups[this->adcs.ngroups].nconv = 1;
		this->adcs.groups[this->adcs.ngroups].conv[0] = i;
		this->adcs.ngroups++;
		if (this->adcs.ngroups >= 32)
			break;
	}
	this->adcs.cur = 0;
	return 0;
}

static int
generic_codec_add_dacgroup(codec_t *this, int assoc, uint32_t digital)
{
	int i, j, n, dac, seq;

	n = 0;
	for (seq = 0 ; seq < CORB_CD_SEQUENCE_MAX; seq++) {
		i = generic_codec_find_pin(this, assoc, seq, digital);
		if (i < 0)
			continue;
		dac = generic_codec_find_dac(this, i, 0);
		if (dac < 0)
			continue;
		/* duplication check */
		for (j = 0; j < n; j++) {
			if (this->dacs.groups[this->dacs.ngroups].conv[j] == dac)
				break;
		}
		if (j < n)	/* this group already has <dac> */
			continue;
		this->dacs.groups[this->dacs.ngroups].conv[n++] = dac;
		DPRINTF(("%s: assoc=%d seq=%d ==> g=%d n=%d\n",
			 __func__, assoc, seq, this->dacs.ngroups, n-1));
	}
	if (n <= 0)		/* no such DACs */
		return 0;
	this->dacs.groups[this->dacs.ngroups].nconv = n;

	/* check if the same combination is already registered */
	for (i = 0; i < this->dacs.ngroups; i++) {
		if (n != this->dacs.groups[i].nconv)
			continue;
		for (j = 0; j < n; j++) {
			if (this->dacs.groups[this->dacs.ngroups].conv[j] !=
			    this->dacs.groups[i].conv[j])
				break;
		}
		if (j >= n) /* matched */
			return 0;
	}
	/* found no equivalent group */
	this->dacs.ngroups++;
	return 0;
}

static int
generic_codec_find_pin(const codec_t *this, int assoc, int seq, uint32_t digital)
{
	int i;

	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if ((this->w[i].d.pin.cap & COP_PINCAP_OUTPUT) == 0)
			continue;
		if ((this->w[i].widgetcap & COP_AWCAP_DIGITAL) != digital)
			continue;
		if (this->w[i].d.pin.association != assoc)
			continue;
		if (this->w[i].d.pin.sequence == seq) {
			return i;
		}
	}
	return -1;
}

static int
generic_codec_find_dac(const codec_t *this, int index, int depth)
{
	const widget_t *w;
	int i, j, ret;

	w = &this->w[index];
	if (w->type == COP_AWTYPE_AUDIO_OUTPUT) {
		DPRINTF(("%s: DAC: nid=0x%x index=%d\n",
		    __func__, w->nid, index));
		return index;
	}
	if (++depth > 50) {
		return -1;
	}
	if (w->selected >= 0) {
		j = w->connections[w->selected];
		if (VALID_WIDGET_NID(j, this)) {
			ret = generic_codec_find_dac(this, j, depth);
			if (ret >= 0) {
				DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
				    __func__, w->nid, index));
				return ret;
			}
		}
	}
	for (i = 0; i < w->nconnections; i++) {
		j = w->connections[i];
		if (!VALID_WIDGET_NID(j, this))
			continue;
		ret = generic_codec_find_dac(this, j, depth);
		if (ret >= 0) {
			DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
			    __func__, w->nid, index));
			return ret;
		}
	}
	return -1;
}

/* ----------------------------------------------------------------
 * Generic mixer functions
 * ---------------------------------------------------------------- */

static int
generic_mixer_init(codec_t *this)
{
	/*
	 * pin		"<color>%2.2x"
	 * audio output	"dac%2.2x"
	 * audio input	"adc%2.2x"
	 * mixer	"mixer%2.2x"
	 * selector	"sel%2.2x"
	 */
	mixer_item_t *m;
	int err, i, j, k;

	this->maxmixers = 10;
	this->nmixers = 0;
	this->mixers = malloc(sizeof(mixer_item_t) * this->maxmixers,
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (this->mixers == NULL) {
		aprint_error("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}

	/* register classes */
	DPRINTF(("%s: register classes\n", __func__));
	m = &this->mixers[AZ_CLASS_INPUT];
	m->devinfo.index = AZ_CLASS_INPUT;
	strlcpy(m->devinfo.label.name, AudioCinputs,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_INPUT;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_OUTPUT];
	m->devinfo.index = AZ_CLASS_OUTPUT;
	strlcpy(m->devinfo.label.name, AudioCoutputs,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_OUTPUT;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_RECORD];
	m->devinfo.index = AZ_CLASS_RECORD;
	strlcpy(m->devinfo.label.name, AudioCrecord,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_RECORD;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	this->nmixers = AZ_CLASS_RECORD + 1;

#define MIXER_REG_PROLOG	\
	mixer_devinfo_t *d; \
	err = generic_mixer_ensure_capacity(this, this->nmixers + 1); \
	if (err) \
		return err; \
	m = &this->mixers[this->nmixers]; \
	d = &m->devinfo; \
	m->nid = i

	FOR_EACH_WIDGET(this, i) {
		const widget_t *w;

		w = &this->w[i];

		/* selector */
		if (w->type != COP_AWTYPE_AUDIO_MIXER && w->nconnections >= 2) {
			MIXER_REG_PROLOG;
			DPRINTF(("%s: selector %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.source", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_RECORD;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_INPUT;
			else
				d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_CONNLIST;
			for (j = 0, k = 0; j < w->nconnections && k < 32; j++) {
				if (!VALID_WIDGET_NID(w->connections[j], this))
					continue;
				DPRINTF(("%s: selector %d=%s\n", __func__, j,
				    this->w[w->connections[j]].name));
				d->un.e.member[k].ord = j;
				strlcpy(d->un.e.member[k].label.name,
				    this->w[w->connections[j]].name,
				    MAX_AUDIO_DEV_LEN);
				k++;
			}
			d->un.e.num_mem = k;
			this->nmixers++;
		}

		/* output mute */
		if (w->widgetcap & COP_AWCAP_OUTAMP &&
		    w->outamp_cap & COP_AMPCAP_MUTE) {
			MIXER_REG_PROLOG;
			DPRINTF(("%s: output mute %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.mute", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_OUTAMP;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* output gain */
		if (w->widgetcap & COP_AWCAP_OUTAMP
		    && COP_AMPCAP_NUMSTEPS(w->outamp_cap)) {
			MIXER_REG_PROLOG;
			DPRINTF(("%s: output gain %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s", w->name);
			d->type = AUDIO_MIXER_VALUE;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_OUTAMP;
			d->un.v.num_channels = WIDGET_CHANNELS(w);
#ifdef MAX_VOLUME_255
			d->un.v.units.name[0] = 0;
#else
			snprintf(d->un.v.units.name, sizeof(d->un.v.units.name),
			    "0.25x%ddB", COP_AMPCAP_STEPSIZE(w->outamp_cap)+1);
#endif
			d->un.v.delta =
			    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->outamp_cap));
			this->nmixers++;
		}

		/* input mute */
		if (w->widgetcap & COP_AWCAP_INAMP &&
		    w->inamp_cap & COP_AMPCAP_MUTE) {
			DPRINTF(("%s: input mute %s\n", __func__, w->name));
			if (w->type != COP_AWTYPE_AUDIO_SELECTOR &&
			    w->type != COP_AWTYPE_AUDIO_MIXER) {
				MIXER_REG_PROLOG;
				snprintf(d->label.name, sizeof(d->label.name),
				    "%s.mute", w->name);
				d->type = AUDIO_MIXER_ENUM;
				if (w->type == COP_AWTYPE_PIN_COMPLEX)
					d->mixer_class = AZ_CLASS_OUTPUT;
				else if (w->type == COP_AWTYPE_AUDIO_INPUT)
					d->mixer_class = AZ_CLASS_RECORD;
				else
					d->mixer_class = AZ_CLASS_INPUT;
				m->target = 0;
				d->un.e.num_mem = 2;
				d->un.e.member[0].ord = 0;
				strlcpy(d->un.e.member[0].label.name,
				    AudioNoff, MAX_AUDIO_DEV_LEN);
				d->un.e.member[1].ord = 1;
				strlcpy(d->un.e.member[1].label.name,
				    AudioNon, MAX_AUDIO_DEV_LEN);
				this->nmixers++;
			} else {
				for (j = 0; j < w->nconnections; j++) {
					MIXER_REG_PROLOG;
					if (!VALID_WIDGET_NID(w->connections[j], this))
						continue;
					DPRINTF(("%s: input mute %s.%s\n", __func__,
					    w->name, this->w[w->connections[j]].name));
					snprintf(d->label.name, sizeof(d->label.name),
					    "%s.%s.mute", w->name,
					    this->w[w->connections[j]].name);
					d->type = AUDIO_MIXER_ENUM;
					if (w->type == COP_AWTYPE_PIN_COMPLEX)
						d->mixer_class = AZ_CLASS_OUTPUT;
					else if (w->type == COP_AWTYPE_AUDIO_INPUT)
						d->mixer_class = AZ_CLASS_RECORD;
					else
						d->mixer_class = AZ_CLASS_INPUT;
					m->target = j;
					d->un.e.num_mem = 2;
					d->un.e.member[0].ord = 0;
					strlcpy(d->un.e.member[0].label.name,
					    AudioNoff, MAX_AUDIO_DEV_LEN);
					d->un.e.member[1].ord = 1;
					strlcpy(d->un.e.member[1].label.name,
					    AudioNon, MAX_AUDIO_DEV_LEN);
					this->nmixers++;
				}
			}
		}

		/* input gain */
		if (w->widgetcap & COP_AWCAP_INAMP
		    && COP_AMPCAP_NUMSTEPS(w->inamp_cap)) {
			DPRINTF(("%s: input gain %s\n", __func__, w->name));
			if (w->type != COP_AWTYPE_AUDIO_SELECTOR &&
			    w->type != COP_AWTYPE_AUDIO_MIXER) {
				MIXER_REG_PROLOG;
				snprintf(d->label.name, sizeof(d->label.name),
				    "%s", w->name);
				d->type = AUDIO_MIXER_VALUE;
				if (w->type == COP_AWTYPE_PIN_COMPLEX)
					d->mixer_class = AZ_CLASS_OUTPUT;
				else if (w->type == COP_AWTYPE_AUDIO_INPUT)
					d->mixer_class = AZ_CLASS_RECORD;
				else
					d->mixer_class = AZ_CLASS_INPUT;
				m->target = 0;
				d->un.v.num_channels = WIDGET_CHANNELS(w);
#ifdef MAX_VOLUME_255
				d->un.v.units.name[0] = 0;
#else
				snprintf(d->un.v.units.name,
				    sizeof(d->un.v.units.name), "0.25x%ddB",
				    COP_AMPCAP_STEPSIZE(w->inamp_cap)+1);
#endif
				d->un.v.delta =
				    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->inamp_cap));
				this->nmixers++;
			} else {
				for (j = 0; j < w->nconnections; j++) {
					MIXER_REG_PROLOG;
					if (!VALID_WIDGET_NID(w->connections[j], this))
						continue;
					DPRINTF(("%s: input gain %s.%s\n", __func__,
					    w->name, this->w[w->connections[j]].name));
					snprintf(d->label.name, sizeof(d->label.name),
					    "%s.%s", w->name,
					    this->w[w->connections[j]].name);
					d->type = AUDIO_MIXER_VALUE;
					if (w->type == COP_AWTYPE_PIN_COMPLEX)
						d->mixer_class = AZ_CLASS_OUTPUT;
					else if (w->type == COP_AWTYPE_AUDIO_INPUT)
						d->mixer_class = AZ_CLASS_RECORD;
					else
						d->mixer_class = AZ_CLASS_INPUT;
					m->target = j;
					d->un.v.num_channels = WIDGET_CHANNELS(w);
#ifdef MAX_VOLUME_255
					d->un.v.units.name[0] = 0;
#else
					snprintf(d->un.v.units.name,
					    sizeof(d->un.v.units.name), "0.25x%ddB",
					    COP_AMPCAP_STEPSIZE(w->inamp_cap)+1);
#endif
					d->un.v.delta =
					    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->inamp_cap));
					this->nmixers++;
				}
			}
		}

		/* pin direction */
		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_OUTPUT &&
		    w->d.pin.cap & COP_PINCAP_INPUT) {
			MIXER_REG_PROLOG;
			DPRINTF(("%s: pin dir %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.dir", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_PINDIR;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNinput,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNoutput,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* pin headphone-boost */
		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_HEADPHONE) {
			MIXER_REG_PROLOG;
			DPRINTF(("%s: hpboost %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.boost", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_PINBOOST;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* volume knob */
		if (w->type == COP_AWTYPE_VOLUME_KNOB &&
		    w->d.volume.cap & COP_VKCAP_DELTA) {
			MIXER_REG_PROLOG;
			DPRINTF(("%s: volume knob %s\n", __func__, w->name));
			strlcpy(d->label.name, w->name, sizeof(d->label.name));
			d->type = AUDIO_MIXER_VALUE;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_VOLUME;
			d->un.v.num_channels = 1;
			d->un.v.units.name[0] = 0;
			d->un.v.delta =
			    MIXER_DELTA(COP_VKCAP_NUMSTEPS(w->d.volume.cap));
			this->nmixers++;
		}
	}

	/* if the codec has multiple DAC groups, create "inputs.usingdac" */
	if (this->dacs.ngroups > 1) {
		MIXER_REG_PROLOG;
		DPRINTF(("%s: create inputs.usingdac\n", __func__));
		strlcpy(d->label.name, "usingdac", sizeof(d->label.name));
		d->type = AUDIO_MIXER_ENUM;
		d->mixer_class = AZ_CLASS_INPUT;
		m->target = MI_TARGET_DAC;
		for (i = 0; i < this->dacs.ngroups && i < 32; i++) {
			d->un.e.member[i].ord = i;
			for (j = 0; j < this->dacs.groups[i].nconv; j++) {
				if (j * 2 >= MAX_AUDIO_DEV_LEN)
					break;
				snprintf(d->un.e.member[i].label.name + j*2,
				    MAX_AUDIO_DEV_LEN - j*2, "%2.2x",
				    this->dacs.groups[i].conv[j]);
			}
		}
		d->un.e.num_mem = i;
		this->nmixers++;
	}

	/* if the codec has multiple ADC groups, create "record.usingadc" */
	if (this->adcs.ngroups > 1) {
		MIXER_REG_PROLOG;
		DPRINTF(("%s: create inputs.usingadc\n", __func__));
		strlcpy(d->label.name, "usingadc", sizeof(d->label.name));
		d->type = AUDIO_MIXER_ENUM;
		d->mixer_class = AZ_CLASS_RECORD;
		m->target = MI_TARGET_ADC;
		for (i = 0; i < this->adcs.ngroups && i < 32; i++) {
			d->un.e.member[i].ord = i;
			for (j = 0; j < this->adcs.groups[i].nconv; j++) {
				if (j * 2 >= MAX_AUDIO_DEV_LEN)
					break;
				snprintf(d->un.e.member[i].label.name + j*2,
				    MAX_AUDIO_DEV_LEN - j*2, "%2.2x",
				    this->adcs.groups[i].conv[j]);
			}
		}
		d->un.e.num_mem = i;
		this->nmixers++;
	}

	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);
	return 0;
}

static int
generic_mixer_ensure_capacity(codec_t *this, size_t newsize)
{
	size_t newmax;
	void *newbuf;

	if (this->maxmixers >= newsize)
		return 0;
	newmax = this->maxmixers + 10;
	if (newmax < newsize)
		newmax = newsize;
	newbuf = realloc(this->mixers, sizeof(mixer_item_t) * newmax, M_DEVBUF,
	    M_ZERO | M_NOWAIT);
	if (newbuf == NULL) {
		aprint_error("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	this->mixers = newbuf;
	/* realloc(9) doesn't clear expanded area even if M_ZERO. */
	memset(&this->mixers[this->maxmixers], 0,
	    sizeof(mixer_item_t) * (newmax - this->maxmixers));
	this->maxmixers = newmax;
	return 0;
}

static int
generic_mixer_fix_indexes(codec_t *this)
{
	int i;
	mixer_devinfo_t *d;

	for (i = 0; i < this->nmixers; i++) {
		d = &this->mixers[i].devinfo;
#ifdef DIAGNOSTIC
		if (d->index != 0 && d->index != i)
			aprint_error("%s: index mismatch %d %d\n", __func__,
			    d->index, i);
#endif
		d->index = i;
		if (d->prev == 0)
			d->prev = AUDIO_MIXER_LAST;
		if (d->next == 0)
			d->next = AUDIO_MIXER_LAST;
	}
	return 0;
}

static int
generic_mixer_default(codec_t *this)
{
	int i;
	mixer_item_t *m;
	/* unmute all */
	DPRINTF(("%s: unmute\n", __func__));
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (!IS_MI_TARGET_INAMP(m->target) &&
		    m->target != MI_TARGET_OUTAMP)
			continue;
		if (m->devinfo.type != AUDIO_MIXER_ENUM)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_ENUM;
		mc.un.ord = 0;
		generic_mixer_set(this, m->nid, m->target, &mc);
	}

	/*
	 * For bidirectional pins,
	 *   Output: lineout, speaker, headphone, spdifout, digitalout, other
	 *   Input: others
	 */
	DPRINTF(("%s: process bidirectional pins\n", __func__));
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (m->target != MI_TARGET_PINDIR)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_ENUM;
		switch (this->w[m->nid].d.pin.device) {
		case CORB_CD_LINEOUT:
		case CORB_CD_SPEAKER:
		case CORB_CD_HEADPHONE:
		case CORB_CD_SPDIFOUT:
		case CORB_CD_DIGITALOUT:
		case CORB_CD_DEVICE_OTHER:
			mc.un.ord = 1;
			break;
		default:
			mc.un.ord = 0;
		}
		generic_mixer_set(this, m->nid, m->target, &mc);
	}

	/* set unextreme volume */
	DPRINTF(("%s: set volume\n", __func__));
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (!IS_MI_TARGET_INAMP(m->target) &&
		    m->target != MI_TARGET_OUTAMP &&
		    m->target != MI_TARGET_VOLUME)
			continue;
		if (m->devinfo.type != AUDIO_MIXER_VALUE)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_VALUE;
		mc.un.value.num_channels = 1;
		mc.un.value.level[0] = AUDIO_MAX_GAIN / 2;
		if (m->target != MI_TARGET_VOLUME &&
		    WIDGET_CHANNELS(&this->w[m->nid]) == 2) {
			mc.un.value.num_channels = 2;
			mc.un.value.level[1] = AUDIO_MAX_GAIN / 2;
		}
		generic_mixer_set(this, m->nid, m->target, &mc);
	}

	return 0;
}

static int
generic_mixer_delete(codec_t *this)
{
	if (this->mixers == NULL)
		return 0;
	free(this->mixers, M_DEVBUF);
	this->mixers = NULL;
	return 0;
}

/**
 * @param mc	mc->type must be set by the caller before the call
 */
static int
generic_mixer_get(const codec_t *this, nid_t nid, int target, mixer_ctrl_t *mc)
{
	uint32_t result;
	nid_t n;
	int err;

	/* inamp mute */
	if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		    MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_GAGM_MUTE ? 1 : 0;
	}

	/* inamp gain */
	else if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_VALUE) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		      MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		mc->un.value.level[0] = generic_mixer_from_device_value(this,
		    nid, target, CORB_GAGM_GAIN(result));
		n = this->w[nid].connections[MI_TARGET_INAMP(target)];
		mc->un.value.num_channels = WIDGET_CHANNELS(&this->w[n]);
		if (mc->un.value.num_channels == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			    CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			    &result);
			if (err)
				return err;
			mc->un.value.level[1] = generic_mixer_from_device_value
			    (this, nid, target, CORB_GAGM_GAIN(result));
		}
	}

	/* outamp mute */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_OUTPUT | CORB_GAGM_LEFT | 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_GAGM_MUTE ? 1 : 0;
	}

	/* outamp gain */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_VALUE) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_OUTPUT | CORB_GAGM_LEFT | 0, &result);
		if (err)
			return err;
		mc->un.value.level[0] = generic_mixer_from_device_value(this,
		    nid, target, CORB_GAGM_GAIN(result));
		mc->un.value.num_channels = WIDGET_CHANNELS(&this->w[nid]);
		if (mc->un.value.num_channels == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE,
			    CORB_GAGM_OUTPUT | CORB_GAGM_RIGHT | 0, &result);
			if (err)
				return err;
			mc->un.value.level[1] = generic_mixer_from_device_value
			    (this, nid, target, CORB_GAGM_GAIN(result));
		}
	}

	/* selection */
	else if (target == MI_TARGET_CONNLIST) {
		err = this->comresp(this, nid,
		    CORB_GET_CONNECTION_SELECT_CONTROL, 0, &result);
		if (err)
			return err;
		result = CORB_CSC_INDEX(result);
		if (!VALID_WIDGET_NID(this->w[nid].connections[result], this))
			mc->un.ord = -1;
		else
			mc->un.ord = result;
	}

	/* pin I/O */
	else if (target == MI_TARGET_PINDIR) {
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_PWC_OUTPUT ? 1 : 0;
	}

	/* pin headphone-boost */
	else if (target == MI_TARGET_PINBOOST) {
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_PWC_HEADPHONE ? 1 : 0;
	}

	/* DAC group selection */
	else if (target == MI_TARGET_DAC) {
		mc->un.ord = this->dacs.cur;
	}

	/* ADC selection */
	else if (target == MI_TARGET_ADC) {
		mc->un.ord = this->adcs.cur;
	}

	/* Volume knob */
	else if (target == MI_TARGET_VOLUME) {
		err = this->comresp(this, nid, CORB_GET_VOLUME_KNOB,
		    0, &result);
		if (err)
			return err;
		mc->un.value.level[0] = generic_mixer_from_device_value(this,
		    nid, target, CORB_VKNOB_VOLUME(result));
		mc->un.value.num_channels = 1;
	}

	else {
		aprint_error("%s: internal error in %s: target=%x\n",
		    XNAME(this), __func__, target);
		return -1;
	}
	return 0;
}

static int
generic_mixer_set(codec_t *this, nid_t nid, int target, const mixer_ctrl_t *mc)
{
	uint32_t result, value;
	int err;

	/* inamp mute */
	if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_ENUM) {
		/* We have to set stereo mute separately to keep each gain value. */
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		    MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		value = CORB_AGM_INPUT | CORB_AGM_LEFT |
		    (target << CORB_AGM_INDEX_SHIFT) |
		    CORB_GAGM_GAIN(result);
		if (mc->un.ord)
			value |= CORB_AGM_MUTE;
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			    CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			    &result);
			if (err)
				return err;
			value = CORB_AGM_INPUT | CORB_AGM_RIGHT |
			    (target << CORB_AGM_INDEX_SHIFT) |
			    CORB_GAGM_GAIN(result);
			if (mc->un.ord)
				value |= CORB_AGM_MUTE;
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* inamp gain */
	else if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_VALUE) {
		if (mc->un.value.num_channels < 1)
			return EINVAL;
		if (!generic_mixer_validate_value(this, nid, target,
		    mc->un.value.level[0]))
			return EINVAL;
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		      MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		value = generic_mixer_to_device_value(this, nid, target,
		    mc->un.value.level[0]);
		value = CORB_AGM_INPUT | CORB_AGM_LEFT |
		    (target << CORB_AGM_INDEX_SHIFT) |
		    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
		    (value & CORB_AGM_GAIN_MASK);
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (mc->un.value.num_channels >= 2 &&
		    WIDGET_CHANNELS(&this->w[nid]) == 2) {
			if (!generic_mixer_validate_value(this, nid, target,
			    mc->un.value.level[1]))
				return EINVAL;
			err = this->comresp(this, nid,
			      CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			      CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			      &result);
			if (err)
				return err;
			value = generic_mixer_to_device_value(this, nid, target,
			    mc->un.value.level[1]);
			value = CORB_AGM_INPUT | CORB_AGM_RIGHT |
			    (target << CORB_AGM_INDEX_SHIFT) |
			    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
			    (value & CORB_AGM_GAIN_MASK);
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* outamp mute */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_OUTPUT | CORB_GAGM_LEFT, &result);
		if (err)
			return err;
		value = CORB_AGM_OUTPUT | CORB_AGM_LEFT | CORB_GAGM_GAIN(result);
		if (mc->un.ord)
			value |= CORB_AGM_MUTE;
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE,
			    CORB_GAGM_OUTPUT | CORB_GAGM_RIGHT, &result);
			if (err)
				return err;
			value = CORB_AGM_OUTPUT | CORB_AGM_RIGHT |
			    CORB_GAGM_GAIN(result);
			if (mc->un.ord)
				value |= CORB_AGM_MUTE;
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* outamp gain */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_VALUE) {
		if (mc->un.value.num_channels < 1)
			return EINVAL;
		if (!generic_mixer_validate_value(this, nid, target,
		    mc->un.value.level[0]))
			return EINVAL;
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_OUTPUT | CORB_GAGM_LEFT, &result);
		if (err)
			return err;
		value = generic_mixer_to_device_value(this, nid, target,
		    mc->un.value.level[0]);
		value = CORB_AGM_OUTPUT | CORB_AGM_LEFT |
		    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
		    (value & CORB_AGM_GAIN_MASK);
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (mc->un.value.num_channels >= 2 &&
		    WIDGET_CHANNELS(&this->w[nid]) == 2) {
			if (!generic_mixer_validate_value(this, nid, target,
			    mc->un.value.level[1]))
				return EINVAL;
			err = this->comresp(this, nid,
			      CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_OUTPUT |
			      CORB_GAGM_RIGHT, &result);
			if (err)
				return err;
			value = generic_mixer_to_device_value(this, nid, target,
			    mc->un.value.level[1]);
			value = CORB_AGM_OUTPUT | CORB_AGM_RIGHT |
			    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
			    (value & CORB_AGM_GAIN_MASK);
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* selection */
	else if (target == MI_TARGET_CONNLIST) {
		if (mc->un.ord < 0 ||
		    mc->un.ord >= this->w[nid].nconnections ||
		    !VALID_WIDGET_NID(this->w[nid].connections[mc->un.ord], this))
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_SET_CONNECTION_SELECT_CONTROL, mc->un.ord, &result);
		if (err)
			return err;
	}

	/* pin I/O */
	else if (target == MI_TARGET_PINDIR) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		if (mc->un.ord == 0) {
			result &= ~CORB_PWC_OUTPUT;
			result |= CORB_PWC_INPUT;
		} else {
			result &= ~CORB_PWC_INPUT;
			result |= CORB_PWC_OUTPUT;
		}
		err = this->comresp(this, nid,
		    CORB_SET_PIN_WIDGET_CONTROL, result, &result);
		if (err)
			return err;
	}

	/* pin headphone-boost */
	else if (target == MI_TARGET_PINBOOST) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		if (mc->un.ord == 0) {
			result &= ~CORB_PWC_HEADPHONE;
		} else {
			result |= CORB_PWC_HEADPHONE;
		}
		err = this->comresp(this, nid,
		    CORB_SET_PIN_WIDGET_CONTROL, result, &result);
		if (err)
			return err;
	}

	/* DAC group selection */
	else if (target == MI_TARGET_DAC) {
		if (this->running)
			return EBUSY;
		if (mc->un.ord >= this->dacs.ngroups)
			return EINVAL;
		return azalia_codec_construct_format(this,
		    mc->un.ord, this->adcs.cur);
	}

	/* ADC selection */
	else if (target == MI_TARGET_ADC) {
		if (this->running)
			return EBUSY;
		if (mc->un.ord >= this->adcs.ngroups)
			return EINVAL;
		return azalia_codec_construct_format(this,
		    this->dacs.cur, mc->un.ord);
	}

	/* Volume knob */
	else if (target == MI_TARGET_VOLUME) {
		if (mc->un.value.num_channels != 1)
			return EINVAL;
		if (!generic_mixer_validate_value(this, nid,
		    target, mc->un.value.level[0]))
			return EINVAL;
		value = generic_mixer_to_device_value(this, nid, target,
		     mc->un.value.level[0]) | CORB_VKNOB_DIRECT;
		err = this->comresp(this, nid, CORB_SET_VOLUME_KNOB,
		   value, &result);
		if (err)
			return err;
	}

	else {
		aprint_error("%s: internal error in %s: target=%x\n",
		    XNAME(this), __func__, target);
		return -1;
	}
	return 0;
}

static u_char
generic_mixer_from_device_value(const codec_t *this, nid_t nid, int target,
    uint32_t dv)
{
#ifdef MAX_VOLUME_255
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	else {
		printf("unknown target: %d\n", target);
		dmax = 255;
	}
	return dv * AUDIO_MAX_GAIN / dmax;
#else
	return dv;
#endif
}

static uint32_t
generic_mixer_to_device_value(const codec_t *this, nid_t nid, int target,
    u_char uv)
{
#ifdef MAX_VOLUME_255
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	else {
		printf("unknown target: %d\n", target);
		dmax = 255;
	}
	return uv * dmax / AUDIO_MAX_GAIN;
#else
	return uv;
#endif
}

static uint32_t
generic_mixer_max(const codec_t *this, nid_t nid, int target)
{
#ifdef MAX_VOLUME_255
	return AUDIO_MAX_GAIN;
#else
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	return dmax;
#endif
}

static boolean_t
generic_mixer_validate_value(const codec_t *this, nid_t nid, int target,
    u_char uv)
{
#ifdef MAX_VOLUME_255
	return TRUE;
#else
	return uv <= generic_mixer_max(this, nid, target);
#endif
}

static int
generic_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;	/* nothing to do */
	return generic_mixer_set(this, m->nid, m->target, mc);
}

static int
generic_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;	/* nothing to do */
	return generic_mixer_get(this, m->nid, m->target, mc);
}


/* ----------------------------------------------------------------
 * Realtek ALC260
 *
 * Fujitsu LOOX T70M/T
 *	Internal Speaker: 0x10
 *	Front Headphone: 0x14
 *	Front mic: 0x12
 * ---------------------------------------------------------------- */

#define ALC260_FUJITSU_ID	0x132610cf
static const mixer_item_t alc260_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, 3}}, 0x08, MI_TARGET_OUTAMP}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmaster".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_PINBOOST},
	{{0, {AudioNmono".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {"mic1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {"mic1"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x12, MI_TARGET_PINDIR},
	{{0, {"mic2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP},
	{{0, {"mic2"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x13, MI_TARGET_PINDIR},
	{{0, {"line1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"line1"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x14, MI_TARGET_PINDIR},
	{{0, {"line2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {"line2"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x15, MI_TARGET_PINDIR},

	{{0, {AudioNdac".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)}, /* and 0x09, 0x0a(mono) */
	{{0, {"mic1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {"mic1"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {"mic2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(1)},
	{{0, {"mic2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(1)},
	{{0, {"line1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(2)},
	{{0, {"line1"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(2)},
	{{0, {"line2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(3)},
	{{0, {"line2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(3)},
	{{0, {AudioNcd".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(5)},

	{{0, {"adc04.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={5, {{{"mic1"}, 0}, {{"mic2"}, 1}, {{"line1"}, 2},
		     {{"line2"}, 3}, {{AudioNcd}, 4}}}},
	 0x04, MI_TARGET_CONNLIST},
	{{0, {"adc04.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc04"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc05.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={6, {{{"mic1"}, 0}, {{"mic2"}, 1}, {{"line1"}, 2},
		     {{"line2"}, 3}, {{AudioNcd}, 4}, {{AudioNmixerout}, 5}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"adc05.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"adc05"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x05, MI_TARGET_INAMP(0)},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{"adc04"}, 0}, {{"adc05"}, 1}, {{"digital"}, 2}}}}, 0, MI_TARGET_ADC},
};

static const mixer_item_t alc260_loox_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, 3}}, 0x08, MI_TARGET_OUTAMP}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmaster".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},

	{{0, {AudioNdac".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmicrophone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AudioNmicrophone}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AudioNcd".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(5)},

	{{0, {"adc04.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{AudioNmicrophone}, 0}, {{AudioNcd}, 4}}}}, 0x04, MI_TARGET_CONNLIST},
	{{0, {"adc04.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc04"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc05.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{AudioNmicrophone}, 0}, {{AudioNcd}, 4}, {{AudioNmixerout}, 5}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"adc05.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"adc05"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x05, MI_TARGET_INAMP(0)},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{"adc04"}, 0}, {{"adc05"}, 1}, {{"digital"}, 2}}}}, 0, MI_TARGET_ADC},
};

static int
alc260_mixer_init(codec_t *this)
{
	const mixer_item_t *mi;
	mixer_ctrl_t mc;

	switch (this->subid) {
	case ALC260_FUJITSU_ID:
		this->nmixers = sizeof(alc260_loox_mixer_items) / sizeof(mixer_item_t);
		mi = alc260_loox_mixer_items;
		break;
	default:
		this->nmixers = sizeof(alc260_mixer_items) / sizeof(mixer_item_t);
		mi = alc260_mixer_items;
	}
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (this->mixers == NULL) {
		aprint_error("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, mi, sizeof(mixer_item_t) * this->nmixers);
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x0f, MI_TARGET_PINDIR, &mc); /* lineout */
	generic_mixer_set(this, 0x10, MI_TARGET_PINDIR, &mc); /* headphones */
	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x12, MI_TARGET_PINDIR, &mc); /* mic1 */
	generic_mixer_set(this, 0x13, MI_TARGET_PINDIR, &mc); /* mic2 */
	generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc); /* line1 */
	generic_mixer_set(this, 0x15, MI_TARGET_PINDIR, &mc); /* line2 */
	mc.un.ord = 0;		/* mute: off */
	generic_mixer_set(this, 0x08, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x08, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x09, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x09, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(1), &mc);
	if (this->subid == ALC260_FUJITSU_ID) {
		mc.un.ord = 1;	/* pindir: output */
		generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc); /* line1 */
		mc.un.ord = 4;	/* connlist: cd */
		generic_mixer_set(this, 0x05, MI_TARGET_CONNLIST, &mc);
	}
	return 0;
}

static int
alc260_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{1, {0x02}},	/* analog 2ch */
		 {1, {0x03}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 3,
		{{1, {0x04}},	/* analog 2ch */
		 {1, {0x05}},	/* analog 2ch */
		 {1, {0x06}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
alc260_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	mixer_ctrl_t mc2;
	int err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->nid == 0x08 && m->target == MI_TARGET_OUTAMP) {
		DPRINTF(("%s: hook for outputs.master\n", __func__));
		err = generic_mixer_set(this, m->nid, m->target, mc);
		if (!err) {
			generic_mixer_set(this, 0x09, m->target, mc);
			mc2 = *mc;
			mc2.un.value.num_channels = 1;
			mc2.un.value.level[0] = (mc2.un.value.level[0]
			    + mc2.un.value.level[1]) / 2;
			generic_mixer_set(this, 0x0a, m->target, &mc2);
		}
		return err;
	} else if (m->nid == 0x08 && m->target == MI_TARGET_INAMP(0)) {
		DPRINTF(("%s: hook for inputs.dac.mute\n", __func__));
		err = generic_mixer_set(this, m->nid, m->target, mc);
		if (!err) {
			generic_mixer_set(this, 0x09, m->target, mc);
			generic_mixer_set(this, 0x0a, m->target, mc);
		}
		return err;
	} else if (m->nid == 0x04 &&
		   m->target == MI_TARGET_CONNLIST &&
		   m->devinfo.un.e.num_mem == 2) {
		if (1 <= mc->un.ord && mc->un.ord <= 3)
			return EINVAL;
	} else if (m->nid == 0x05 &&
		   m->target == MI_TARGET_CONNLIST &&
		   m->devinfo.un.e.num_mem == 3) {
		if (1 <= mc->un.ord && mc->un.ord <= 3)
			return EINVAL;
	}
	return generic_mixer_set(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Realtek ALC880
 * ---------------------------------------------------------------- */

static int
alc880_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}}, /* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC882
 * ---------------------------------------------------------------- */

static const mixer_item_t alc882_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17 */
	{{0, {"mic1."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic1"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic2."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"mic2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {AudioNline"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNline}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNcd"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(5)},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {AzaliaNfront".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(1)},

	{{0, {AudioNsurround}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_PINBOOST},
	{{0, {AudioNsurround".dac.mut"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround".mixer.m"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNclfe}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0e, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_PINBOOST},
	{{0, {AzaliaNclfe".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNside}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_PINBOOST},
	{{0, {AzaliaNside".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNside".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(1)},

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17,0xb */
#define ALC882_MIC1	0x001
#define ALC882_MIC2	0x002
#define ALC882_LINE	0x004
#define ALC882_CD	0x010
#define ALC882_BEEP	0x020
#define ALC882_MIX	0x400
#define ALC882_MASK	0x437
	{{0, {AzaliaNfront"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront"."AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1"}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x24, -1},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround"."AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1"}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x23, -1},
	{{0, {AzaliaNclfe"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe"."AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1"}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x22, -1},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_ADC},
};

static int
alc882_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(alc882_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (this->mixers == NULL) {
		aprint_error("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, alc882_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x1b, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x15, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x16, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x17, MI_TARGET_PINDIR, &mc);
	mc.un.ord = 0;		/* [0] 0x0c */
	generic_mixer_set(this, 0x14, MI_TARGET_CONNLIST, &mc);
	generic_mixer_set(this, 0x1b, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 1;		/* [1] 0x0d */
	generic_mixer_set(this, 0x15, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [2] 0x0e */
	generic_mixer_set(this, 0x16, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [3] 0x0fb */
	generic_mixer_set(this, 0x17, MI_TARGET_CONNLIST, &mc);

	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x18, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x19, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x1a, MI_TARGET_PINDIR, &mc);
	/* XXX: inamp for 18/19/1a */

	mc.un.ord = 0;		/* unmute */
	generic_mixer_set(this, 0x24, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x23, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x22, MI_TARGET_INAMP(2), &mc);
	return 0;
}

static int
alc882_init_dacgroup(codec_t *this)
{
#if 0				/* makes no sense to support for 0x25 node */
	static const convgroupset_t dacs = {
		-1, 3,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}},	/* digital */
		 {1, {0x25}}}};	/* another analog */
#else
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
#endif
	static const convgroupset_t adcs = {
		-1, 2,
		{{3, {0x07, 0x08, 0x09}}, /* analog 6ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
alc882_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	mixer_ctrl_t mc2;
	uint32_t mask, bit;
	int i, err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if ((m->nid == 0x22 || m->nid == 0x23 || m->nid == 0x24)
	    && m->target == -1) {
		DPRINTF(("%s: hook for record.*.source\n", __func__));
		mc2.dev = -1;
		mc2.type = AUDIO_MIXER_ENUM;
		bit = 1;
		mask = mc->un.mask & ALC882_MASK;
		for (i = 0; i < this->w[m->nid].nconnections && i < 32; i++) {
			mc2.un.ord = (mask & bit) ? 0 : 1;
			err = generic_mixer_set(this, m->nid,
			    MI_TARGET_INAMP(i), &mc2);
			if (err)
				return err;
			bit = bit << 1;
		}
		return 0;
	}
	return generic_mixer_set(this, m->nid, m->target, mc);
}

static int
alc882_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	uint32_t mask, bit, result;
	int i, err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if ((m->nid == 0x22 || m->nid == 0x23 || m->nid == 0x24)
	    && m->target == -1) {
		DPRINTF(("%s: hook for record.*.source\n", __func__));
		mask = 0;
		bit = 1;
		for (i = 0; i < this->w[m->nid].nconnections && i < 32; i++) {
			err = this->comresp(this, m->nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
				      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
				      i, &result);
			if (err)
				return err;
			if ((result & CORB_GAGM_MUTE) == 0)
				mask |= bit;
			bit = bit << 1;
		}
		mc->un.mask = mask & ALC882_MASK;
		return 0;
	}
	return generic_mixer_get(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Analog Devices AD1981HD
 * ---------------------------------------------------------------- */

static int
ad1981hd_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	switch (nid) {
	case 0x05:
		strlcpy(w->name, AudioNline "out", sizeof(w->name));
		break;
	case 0x06:
		strlcpy(w->name, "hp", sizeof(w->name));
		break;
	case 0x07:
		strlcpy(w->name, AudioNmono, sizeof(w->name));
		break;
	case 0x08:
		strlcpy(w->name, AudioNmicrophone, sizeof(w->name));
		break;
	case 0x09:
		strlcpy(w->name, AudioNline "in", sizeof(w->name));
		break;
	case 0x0d:
		strlcpy(w->name, "beep", sizeof(w->name));
		break;
	case 0x17:
		strlcpy(w->name, AudioNaux, sizeof(w->name));
		break;
	case 0x18:
		strlcpy(w->name, AudioNmicrophone "2", sizeof(w->name));
		break;
	case 0x19:
		strlcpy(w->name, AudioNcd, sizeof(w->name));
		break;
	case 0x1d:
		strlcpy(w->name, AudioNspeaker, sizeof(w->name));
		break;
	}
	return 0;
}

/* ----------------------------------------------------------------
 * Sigmatel STAC9221 and STAC9221D
 * ---------------------------------------------------------------- */

static int
stac9221_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 3,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x08}},	/* digital */
		 {1, {0x1a}}}};	/* another digital? */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x06, 0x07}}, /* analog 4ch */
		 {1, {0x09}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Sigmatel STAC9220
 * ---------------------------------------------------------------- */

static const mixer_item_t stac9220_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={3, {{{AudioNdac}, 0}, {{"digital-in"}, 1}, {{"selector"}, 2}}}},
	 0x07, MI_TARGET_CONNLIST},
	{{0, {"digital."AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac}, 0}, {{"selector"}, 1}}}},
	 0x09, MI_TARGET_CONNLIST}, /* AudioNdac is not accurate name */
	{{0, {"selector."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0a, MI_TARGET_OUTAMP},
	{{0, {"selector"}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x0a, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_OUTAMP},
	{{0, {"selector."AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={3, {{{"mic1"}, 0}, {{"mic2"}, 1}, {{AudioNcd}, 4}}}},
	 0x0c, MI_TARGET_CONNLIST},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_PINBOOST},
	{{0, {AudioNspeaker".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_PINBOOST},
	{{0, {AudioNmono"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {AudioNmono}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(31)}}, 0x11, MI_TARGET_OUTAMP},
	{{0, {"beep."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"beep"}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(3)}}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}},
	 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}},
	 0, MI_TARGET_ADC},
};

static int
stac9220_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(stac9220_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (this->mixers == NULL) {
		aprint_error("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, stac9220_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x0d, MI_TARGET_PINDIR, &mc); /* headphones */
	generic_mixer_set(this, 0x0e, MI_TARGET_PINDIR, &mc); /* speaker */
	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x0f, MI_TARGET_PINDIR, &mc); /* mic2 */
	generic_mixer_set(this, 0x10, MI_TARGET_PINDIR, &mc); /* mic1 */
	mc.type = AUDIO_MIXER_VALUE;
	mc.un.value.num_channels = 2;
	mc.un.value.level[0] = generic_mixer_max(this, 0x0c, MI_TARGET_OUTAMP);
	mc.un.value.level[1] = mc.un.value.level[0];
	generic_mixer_set(this, 0x0c, MI_TARGET_OUTAMP, &mc);

	return 0;
}
