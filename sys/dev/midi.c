/*	$NetBSD: midi.c,v 1.34.2.1 2003/07/02 15:26:00 darrenr Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
__KERNEL_RCSID(0, "$NetBSD: midi.c,v 1.34.2.1 2003/07/02 15:26:00 darrenr Exp $");

#include "midi.h"
#include "sequencer.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>
#include <sys/device.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>

#if NMIDI > 0

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (mididebug) printf x
#define DPRINTFN(n,x)	if (mididebug >= (n)) printf x
int	mididebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

int midi_wait;

void	midi_in(void *, int);
void	midi_out(void *);
int	midi_start_output(struct midi_softc *, int);
int	midi_sleep_timo(int *, char *, int);
int	midi_sleep(int *, char *);
void	midi_wakeup(int *);
void	midi_initbuf(struct midi_buffer *);
void	midi_timeout(void *);

int	midiprobe(struct device *, struct cfdata *, void *);
void	midiattach(struct device *, struct device *, void *);
int	mididetach(struct device *, int);
int	midiactivate(struct device *, enum devact);

dev_type_open(midiopen);
dev_type_close(midiclose);
dev_type_read(midiread);
dev_type_write(midiwrite);
dev_type_ioctl(midiioctl);
dev_type_poll(midipoll);
dev_type_kqfilter(midikqfilter);

const struct cdevsw midi_cdevsw = {
	midiopen, midiclose, midiread, midiwrite, midiioctl,
	nostop, notty, midipoll, nommap, midikqfilter,
};

CFATTACH_DECL(midi, sizeof(struct midi_softc),
    midiprobe, midiattach, mididetach, midiactivate);

#ifdef MIDI_SAVE
#define MIDI_SAVE_SIZE 100000
int midicnt;
struct {
	int cnt;
	u_char buf[MIDI_SAVE_SIZE];
} midisave;
#define MIDI_GETSAVE		_IOWR('m', 100, int)

#endif

extern struct cfdriver midi_cd;

int
midiprobe(struct device *parent, struct cfdata *match, void *aux)
{
	struct audio_attach_args *sa = aux;

	DPRINTFN(6,("midiprobe: type=%d sa=%p hw=%p\n", 
		 sa->type, sa, sa->hwif));
	return (sa->type == AUDIODEV_TYPE_MIDI);
}

void
midiattach(struct device *parent, struct device *self, void *aux)
{
	struct midi_softc *sc = (void *)self;
	struct audio_attach_args *sa = aux;
	struct midi_hw_if *hwp = sa->hwif;
	void *hdlp = sa->hdl;

	DPRINTFN(6, ("MIDI attach\n"));

#ifdef DIAGNOSTIC
	if (hwp == 0 ||
	    hwp->open == 0 ||
	    hwp->close == 0 ||
	    hwp->output == 0 ||
	    hwp->getinfo == 0) {
		printf("midi: missing method\n");
		return;
	}
#endif

	callout_init(&sc->sc_callout);

	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	sc->dying = 0;
	midi_attach(sc, parent);
}

int
midiactivate(struct device *self, enum devact act)
{
	struct midi_softc *sc = (struct midi_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		sc->dying = 1;
		break;
	}
	return (0);
}

int
mididetach(struct device *self, int flags)
{
	struct midi_softc *sc = (struct midi_softc *)self;
	int maj, mn;

	DPRINTF(("midi_detach: sc=%p flags=%d\n", sc, flags));

	sc->dying = 1;

	wakeup(&sc->wchan);
	wakeup(&sc->rchan);

	/* locate the major number */
	maj = cdevsw_lookup_major(&midi_cdevsw);

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

void
midi_attach(struct midi_softc *sc, struct device *parent)
{
	struct midi_info mi;

	sc->isopen = 0;

	midi_wait = MIDI_WAIT * hz / 1000000;
	if (midi_wait == 0)
		midi_wait = 1;

	sc->sc_dev = parent;
	sc->hw_if->getinfo(sc->hw_hdl, &mi);
	sc->props = mi.props;
	printf(": %s\n", mi.name);
}

int
midi_unit_count(void)
{
	return midi_cd.cd_ndevs;
}

void
midi_initbuf(struct midi_buffer *mb)
{
	mb->used = 0;
	mb->usedhigh = MIDI_BUFSIZE;
	mb->end = mb->start + mb->usedhigh;
	mb->inp = mb->outp = mb->start;
}

int
midi_sleep_timo(int *chan, char *label, int timo)
{
	int st;

	if (!label)
		label = "midi";

	DPRINTFN(5, ("midi_sleep_timo: %p %s %d\n", chan, label, timo));
	*chan = 1;
	st = tsleep(chan, PWAIT | PCATCH, label, timo);
	*chan = 0;
#ifdef MIDI_DEBUG
	if (st != 0)
		printf("midi_sleep: %d\n", st);
#endif
	return st;
}

int
midi_sleep(int *chan, char *label)
{
	return midi_sleep_timo(chan, label, 0);
}

void
midi_wakeup(int *chan)
{
	if (*chan) {
		DPRINTFN(5, ("midi_wakeup: %p\n", chan));
		wakeup(chan);
		*chan = 0;
	}
}

static int midi_lengths[] = { 2,2,2,2,1,1,2,0 };
/* Number of bytes in a MIDI command */
#define MIDI_LENGTH(d) (midi_lengths[((d) >> 4) & 7])

void
midi_in(void *addr, int data)
{
	struct midi_softc *sc = addr;
	struct midi_buffer *mb = &sc->inbuf;
	int i;

	if (!sc->isopen)
		return;
	if (data == MIDI_ACK)
		return;

	DPRINTFN(3, ("midi_in: sc=%p data=0x%02x state=%d pos=%d\n", 
		     sc, data, sc->in_state, sc->in_pos));

	if (!(sc->flags & FREAD))
		return;		/* discard data if not reading */

	switch(sc->in_state) {
	case MIDI_IN_START:
		if (MIDI_IS_STATUS(data)) {
			switch(data) {
			case 0xf0: /* Sysex */
				sc->in_state = MIDI_IN_SYSEX;
				break;
			case 0xf1: /* MTC quarter frame */
			case 0xf3: /* Song select */
				sc->in_state = MIDI_IN_DATA;
				sc->in_msg[0] = data;
				sc->in_pos = 1;
				sc->in_left = 1;
				break;
			case 0xf2: /* Song position pointer */
				sc->in_state = MIDI_IN_DATA;
				sc->in_msg[0] = data;
				sc->in_pos = 1;
				sc->in_left = 2;
				break;
			default:
				if (MIDI_IS_COMMON(data)) {
					sc->in_msg[0] = data;
					sc->in_pos = 1;
					goto deliver;
				} else {
					sc->in_state = MIDI_IN_DATA;
					sc->in_msg[0] = sc->in_status = data;
					sc->in_pos = 1;
					sc->in_left = MIDI_LENGTH(data);
				}
				break;
			}
		} else {
			if (MIDI_IS_STATUS(sc->in_status)) {
				sc->in_state = MIDI_IN_DATA;
				sc->in_msg[0] = sc->in_status;
				sc->in_msg[1] = data;
				sc->in_pos = 2;
				sc->in_left = MIDI_LENGTH(sc->in_status) - 1;
			}
		}
		return;
	case MIDI_IN_DATA:
		sc->in_msg[sc->in_pos++] = data;
		if (--sc->in_left <= 0)
			break;	/* deliver data */
		return;
	case MIDI_IN_SYSEX:
		if (data == MIDI_SYSEX_END)
			sc->in_state = MIDI_IN_START;
		return;
	}
deliver:
	sc->in_state = MIDI_IN_START;
#if NSEQUENCER > 0
	if (sc->seqopen) {
		extern void midiseq_in(struct midi_dev *,u_char *,int);
		midiseq_in(sc->seq_md, sc->in_msg, sc->in_pos);
		return;
	}
#endif

	if (mb->used + sc->in_pos > mb->usedhigh) {
		DPRINTF(("midi_in: buffer full, discard data=0x%02x\n", 
			 sc->in_msg[0]));
		return;
	}
	for (i = 0; i < sc->in_pos; i++) {
		*mb->inp++ = sc->in_msg[i];
		if (mb->inp >= mb->end)
			mb->inp = mb->start;
		mb->used++;
	}
	midi_wakeup(&sc->rchan);
	selnotify(&sc->rsel, 0);
	if (sc->async)
		psignal(sc->async, SIGIO);
}

void
midi_out(void *addr)
{
	struct midi_softc *sc = addr;

	if (!sc->isopen)
		return;
	DPRINTFN(3, ("midi_out: %p\n", sc));
	midi_start_output(sc, 1);
}

int
midiopen(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct midi_softc *sc;
	struct midi_hw_if *hw;
	int error;

	sc = device_lookup(&midi_cd, MIDIUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (sc->dying)
		return (EIO);

	DPRINTF(("midiopen %p\n", sc));

	hw = sc->hw_if;
	if (!hw)
		return ENXIO;
	if (sc->isopen)
		return EBUSY;
	sc->in_state = MIDI_IN_START;
	sc->in_status = 0;
	error = hw->open(sc->hw_hdl, flags, midi_in, midi_out, sc);
	if (error)
		return error;
	sc->isopen++;
	midi_initbuf(&sc->outbuf);
	midi_initbuf(&sc->inbuf);
	sc->flags = flags;
	sc->rchan = 0;
	sc->wchan = 0;
	sc->pbus = 0;
	sc->async = 0;

#ifdef MIDI_SAVE
	if (midicnt != 0) {
		midisave.cnt = midicnt;
		midicnt = 0;
	}
#endif

	return 0;
}

int
midiclose(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_hw_if *hw = sc->hw_if;
	int s, error;

	DPRINTF(("midiclose %p\n", sc));

	midi_start_output(sc, 0);
	error = 0;
	s = splaudio();
	while (sc->outbuf.used > 0 && !error) {
		DPRINTFN(2,("midiclose sleep used=%d\n", sc->outbuf.used));
		error = midi_sleep_timo(&sc->wchan, "mid_dr", 30*hz);
	}
	splx(s);
	sc->isopen = 0;
	hw->close(sc->hw_hdl);
#if NSEQUENCER > 0
	sc->seqopen = 0;
	sc->seq_md = 0;
#endif
	return 0;
}

int
midiread(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_buffer *mb = &sc->inbuf;
	int error;
	u_char *outp;
	int used, cc, n, resid;
	int s;

	DPRINTF(("midiread: %p, count=%lu\n", sc, 
		 (unsigned long)uio->uio_resid));

	if (sc->dying)
		return EIO;

	error = 0;
	resid = uio->uio_resid;
	while (uio->uio_resid == resid && !error) {
		s = splaudio();
		while (mb->used <= 0) {
			if (ioflag & IO_NDELAY) {
				splx(s);
				return EWOULDBLOCK;
			}
			error = midi_sleep(&sc->rchan, "mid rd");
			if (error) {
				splx(s);
				return error;
			}
		}
		used = mb->used;
		outp = mb->outp;
		splx(s);
		if (sc->dying)
			return EIO;
		cc = used;	/* maximum to read */
		n = mb->end - outp;
		if (n < cc)
			cc = n;	/* don't read beyond end of buffer */
		if (uio->uio_resid < cc)
			cc = uio->uio_resid; /* and no more than we want */
		DPRINTFN(3, ("midiread: uiomove cc=%d\n", cc));
		error = uiomove(outp, cc, uio);
		if (error)
			break;
		used -= cc;
		outp += cc;
		if (outp >= mb->end)
			outp = mb->start;
		s = splaudio();
		mb->outp = outp;
		mb->used = used;
		splx(s);
	}
	return error;
}

void
midi_timeout(void *arg)
{
	struct midi_softc *sc = arg;

	DPRINTFN(3,("midi_timeout: %p\n", sc));
	midi_start_output(sc, 1);
}

int
midi_start_output(struct midi_softc *sc, int intr)
{
	struct midi_buffer *mb = &sc->outbuf;
	u_char out;
	int error;
	int s;
	int i;

	error = 0;

	if (sc->dying)
		return EIO;

	if (sc->pbus && !intr) {
		DPRINTFN(4, ("midi_start_output: busy\n"));
		return 0;
	}
	sc->pbus = (mb->used > 0)?1:0;
	for (i = 0; i < MIDI_MAX_WRITE && mb->used > 0 &&
		   (!error || error==EINPROGRESS); i++) {
		s = splaudio();
		out = *mb->outp;
		mb->outp++;
		if (mb->outp >= mb->end)
			mb->outp = mb->start;
		mb->used--;
		splx(s);
#ifdef MIDI_SAVE
		midisave.buf[midicnt] = out;
		midicnt = (midicnt + 1) % MIDI_SAVE_SIZE;
#endif
		DPRINTFN(4, ("midi_start_output: %p i=%d, data=0x%02x\n", 
			     sc, i, out));
		error = sc->hw_if->output(sc->hw_hdl, out);
		if ((sc->props & MIDI_PROP_OUT_INTR) && error!=EINPROGRESS)
			/* If ointr is enabled, midi_start_output()
			 * normally writes only one byte,
			 * except hw_if->output() returns EINPROGRESS.
			 */
			break;
	}
	midi_wakeup(&sc->wchan);
	selnotify(&sc->wsel, 0);
	if (sc->async)
		psignal(sc->async, SIGIO);
	if (!(sc->props & MIDI_PROP_OUT_INTR) || error==EINPROGRESS) {
		if (mb->used > 0)
			callout_reset(&sc->sc_callout, midi_wait,
				      midi_timeout, sc);
		else
			sc->pbus = 0;
	}
	if ((sc->props & MIDI_PROP_OUT_INTR) && error==EINPROGRESS)
		error = 0;

	return error;
}

int
midiwrite(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_buffer *mb = &sc->outbuf;
	int error;
	u_char *inp;
	int used, cc, n;
	int s;

	DPRINTFN(2, ("midiwrite: %p, unit=%d, count=%lu\n", sc, unit, 
		     (unsigned long)uio->uio_resid));

	if (sc->dying)
		return EIO;

	error = 0;
	while (uio->uio_resid > 0 && !error) {
		s = splaudio();
		if (mb->used >= mb->usedhigh) {
			DPRINTFN(3,("midi_write: sleep used=%d hiwat=%d\n", 
				 mb->used, mb->usedhigh));
			if (ioflag & IO_NDELAY) {
				splx(s);
				return EWOULDBLOCK;
			}
			error = midi_sleep(&sc->wchan, "mid wr");
			if (error) {
				splx(s);
				return error;
			}
		}			
		used = mb->used;
		inp = mb->inp;
		splx(s);
		if (sc->dying)
			return EIO;
		cc = mb->usedhigh - used; 	/* maximum to write */
		n = mb->end - inp;
		if (n < cc)
			cc = n;		/* don't write beyond end of buffer */
		if (uio->uio_resid < cc)
			cc = uio->uio_resid; 	/* and no more than we have */
		error = uiomove(inp, cc, uio);
#ifdef MIDI_DEBUG
		if (error)
		        printf("midi_write:(1) uiomove failed %d; "
			       "cc=%d inp=%p\n",
			       error, cc, inp);
#endif
		if (error)
			break;
		inp = mb->inp + cc;
		if (inp >= mb->end)
			inp = mb->start;
		s = splaudio();
		mb->inp = inp;
		mb->used += cc;
		splx(s);
		error = midi_start_output(sc, 0);
	}
	return error;
}

/*
 * This write routine is only called from sequencer code and expects
 * a write that is smaller than the MIDI buffer.
 */
int
midi_writebytes(int unit, u_char *buf, int cc)
{
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_buffer *mb = &sc->outbuf;
	int n, s;

	DPRINTFN(2, ("midi_writebytes: %p, unit=%d, cc=%d\n", sc, unit, cc));
	DPRINTFN(3, ("midi_writebytes: %x %x %x\n",buf[0],buf[1],buf[2]));

	if (sc->dying)
		return EIO;

	s = splaudio();
	if (mb->used + cc >= mb->usedhigh) {
		splx(s);
		return (EWOULDBLOCK);
	}
	n = mb->end - mb->inp;
	if (cc < n)
		n = cc;
	mb->used += cc;
	memcpy(mb->inp, buf, n);
	mb->inp += n;
	if (mb->inp >= mb->end) {
		mb->inp = mb->start;
		cc -= n;
		if (cc > 0) {
			memcpy(mb->inp, buf + n, cc);
			mb->inp += cc;
		}
	}
	splx(s);
	return (midi_start_output(sc, 0));
}

int
midiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct lwp *l)
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_hw_if *hw = sc->hw_if;
	int error;

	DPRINTF(("midiioctl: %p cmd=0x%08lx\n", sc, cmd));

	if (sc->dying)
		return EIO;

	error = 0;
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->async)
				return EBUSY;
			sc->async = l->l_proc;
			DPRINTF(("midi_ioctl: FIOASYNC %p\n", l->l_proc));
		} else
			sc->async = 0;
		break;

#if 0
	case MIDI_PRETIME:
		/* XXX OSS
		 * This should set up a read timeout, but that's
		 * why we have poll(), so there's nothing yet. */
		error = EINVAL;
		break;
#endif

#ifdef MIDI_SAVE
	case MIDI_GETSAVE:
		error = copyout(&midisave, *(void **)addr, sizeof midisave);
  		break;
#endif

	default:
		if (hw->ioctl)
			error = hw->ioctl(sc->hw_hdl, cmd, addr, flag, l);
		else
			error = EINVAL;
		break;
	}
	return error;
}

int
midipoll(dev_t dev, int events, struct lwp *l)
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	int revents = 0;
	int s;

	DPRINTF(("midipoll: %p events=0x%x\n", sc, events));

	if (sc->dying)
		return EIO;

	s = splaudio();

	if (events & (POLLIN | POLLRDNORM))
		if (sc->inbuf.used > 0)
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (sc->outbuf.used < sc->outbuf.usedhigh)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(l, &sc->rsel);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(l, &sc->wsel);
	}

	splx(s);
	return revents;
}

static void
filt_midirdetach(struct knote *kn)
{
	struct midi_softc *sc = kn->kn_hook;
	int s;

	s = splaudio();
	SLIST_REMOVE(&sc->rsel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_midiread(struct knote *kn, long hint)
{
	struct midi_softc *sc = kn->kn_hook;

	/* XXXLUKEM (thorpej): please make sure this is correct. */

	kn->kn_data = sc->inbuf.used;
	return (kn->kn_data > 0);
}

static const struct filterops midiread_filtops =
	{ 1, NULL, filt_midirdetach, filt_midiread };

static void
filt_midiwdetach(struct knote *kn)
{
	struct midi_softc *sc = kn->kn_hook;
	int s;

	s = splaudio();
	SLIST_REMOVE(&sc->wsel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_midiwrite(struct knote *kn, long hint)
{
	struct midi_softc *sc = kn->kn_hook;

	/* XXXLUKEM (thorpej): please make sure this is correct. */

	kn->kn_data = sc->outbuf.usedhigh - sc->outbuf.used;
	return (kn->kn_data > 0);
}

static const struct filterops midiwrite_filtops =
	{ 1, NULL, filt_midiwdetach, filt_midiwrite };

int
midikqfilter(dev_t dev, struct knote *kn)
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->rsel.sel_klist;
		kn->kn_fop = &midiread_filtops;
		break;

	case EVFILT_WRITE:
		klist = &sc->wsel.sel_klist;
		kn->kn_fop = &midiwrite_filtops;
		break;

	default:
		return (1);
	}

	kn->kn_hook = sc;

	s = splaudio();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

void
midi_getinfo(dev_t dev, struct midi_info *mi)
{
	struct midi_softc *sc;

	sc = device_lookup(&midi_cd, MIDIUNIT(dev));
	if (sc == NULL)
		return;
	if (sc->dying)
		return;

	sc->hw_if->getinfo(sc->hw_hdl, mi);
}

#endif /* NMIDI > 0 */

#if NMIDI > 0 || NMIDIBUS > 0

int	audioprint(void *, const char *);

struct device *
midi_attach_mi(struct midi_hw_if *mhwp, void *hdlp, struct device *dev)
{
	struct audio_attach_args arg;

#ifdef DIAGNOSTIC
	if (mhwp == NULL) {
		aprint_error("midi_attach_mi: NULL\n");
		return (0);
	}
#endif
	arg.type = AUDIODEV_TYPE_MIDI;
	arg.hwif = mhwp;
	arg.hdl = hdlp;
	return (config_found(dev, &arg, audioprint));
}

#endif /* NMIDI > 0 || NMIDIBUS > 0 */
