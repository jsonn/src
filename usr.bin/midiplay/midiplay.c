/*	$NetBSD: midiplay.c,v 1.22.12.1 2006/05/26 22:52:34 chap Exp $	*/

/*
 * Copyright (c) 1998, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org).
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

#ifndef lint
__RCSID("$NetBSD: midiplay.c,v 1.22.12.1 2006/05/26 22:52:34 chap Exp $");
#endif


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/midiio.h>

#define DEVMUSIC "/dev/music"

struct track {
	u_char *start, *end;
	u_long curtime;
	u_char status;
};

#define MIDI_META 0xff

#define META_SEQNO	0x00
#define META_TEXT	0x01
#define META_COPYRIGHT	0x02
#define META_TRACK	0x03
#define META_INSTRUMENT	0x04
#define META_LYRIC	0x05
#define META_MARKER	0x06
#define META_CUE	0x07
#define META_CHPREFIX	0x20
#define META_EOT	0x2f
#define META_SET_TEMPO	0x51
#define META_KEY	0x59
#define META_SMPTE	0x54
#define META_TIMESIGN	0x58

char *metanames[] = { 
	"", "Text", "Copyright", "Track", "Instrument", 
	"Lyric", "Marker", "Cue",
};

static int midi_lengths[] = { 2,2,2,2,1,1,2,0 };
/* Number of bytes in a MIDI command */
#define MIDI_LENGTH(d) (midi_lengths[((d) >> 4) & 7])

void usage(void);
void send_event(seq_event_t *);
void dometa(u_int, u_char *, u_int);
void midireset(void);
void send_sysex(u_char *, u_int);
u_long getvar(struct track *);
void playfile(FILE *, char *);
void playdata(u_char *, u_int, char *);
int main(int argc, char **argv);

/*
 * This plays at an apparent tempo of 120 bpm when the BASETEMPO is 150 bpm,
 * because the quavers are 5 divisions (4 on 1 off) rather than 4 total.
 */
#define P(c) 1,0x90,c,0x7f,4,0x80,c,0
#define PL(c) 1,0x90,c,0x7f,8,0x80,c,0
#define C 0x3c
#define D 0x3e
#define E 0x40
#define F 0x41

u_char sample[] = { 
	'M','T','h','d',  0,0,0,6,  0,1,  0,1,  0,8,
	'M','T','r','k',  0,0,0,4+13*8,
	P(C), P(C), P(C), P(E), P(D), P(D), P(D), 
	P(F), P(E), P(E), P(D), P(D), PL(C),
	0, 0xff, 0x2f, 0
};
#undef P
#undef PL
#undef C
#undef D
#undef E
#undef F

#define MARK_HEADER "MThd"
#define MARK_TRACK "MTrk"
#define MARK_LEN 4

#define	RMID_SIG "RIFF"
#define	RMID_MIDI_ID "RMID"
#define	RMID_DATA_ID "data"

#define SIZE_LEN 4
#define HEADER_LEN 6

#define GET8(p) ((p)[0])
#define GET16(p) (((p)[0] << 8) | (p)[1])
#define GET24(p) (((p)[0] << 16) | ((p)[1] << 8) | (p)[2])
#define GET32(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])
#define GET32_LE(p) (((p)[3] << 24) | ((p)[2] << 16) | ((p)[1] << 8) | (p)[0])

void
usage(void)
{
	printf("usage: %s [-d unit] [-f file] [-l] [-m] [-p pgm] [-q] "
	       "[-t tempo] [-v] [-x] [file ...]\n",
		getprogname());
	exit(1);
}

int showmeta = 0;
int verbose = 0;
#define BASETEMPO 400000		/* 150 bpm */
u_int tempo = BASETEMPO;		/* microsec / quarter note */
u_int ttempo = 100;
int unit = 0;
int play = 1;
int fd = -1;
int sameprogram = 0;

void
send_event(seq_event_t *ev)
{
	/*
	printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
	       ev->arr[0], ev->arr[1], ev->arr[2], ev->arr[3], 
	       ev->arr[4], ev->arr[5], ev->arr[6], ev->arr[7]);
	*/
	if (play)
		write(fd, ev, sizeof *ev);
}

u_long
getvar(struct track *tp)
{
	u_long r, c;

	r = 0;
	do {
		c = *tp->start++;
		r = (r << 7) | (c & 0x7f);
	} while ((c & 0x80) && tp->start < tp->end);
	return (r);
}

void
dometa(u_int meta, u_char *p, u_int len)
{
	switch (meta) {
	case META_TEXT:
	case META_COPYRIGHT:
	case META_TRACK:
	case META_INSTRUMENT:
	case META_LYRIC:
	case META_MARKER:
	case META_CUE:
		if (showmeta) {
			printf("%s: ", metanames[meta]);
			fwrite(p, len, 1, stdout);
			printf("\n");
		}
		break;
	case META_SET_TEMPO:
		tempo = GET24(p);
		if (showmeta)
			printf("Tempo: %d us / quarter note\n", tempo);
		break;
	case META_TIMESIGN:
		if (showmeta) {
			int n = p[1];
			int d = 1;
			while (n-- > 0)
				d *= 2;
			printf("Time signature: %d/%d %d,%d\n",
			       p[0], d, p[2], p[3]);
		}
		break;
	case META_KEY:
		if (showmeta)
			printf("Key: %d %s\n", (char)p[0],
			       p[1] ? "minor" : "major");
		break;
	default:
		break;
	}
}

void
midireset(void)
{
	/* General MIDI reset sequence */
	send_event(&SEQ_MK_SYSEX(unit,[0]=0x7e, 0x7f, 0x09, 0x01, 0xf7));
}

#define SYSEX_CHUNK 6
void
send_sysex(u_char *p, u_int l)
{
	seq_event_t event;

	while ( l >= SYSEX_CHUNK ) {
		send_event(&SEQ_MK_SYSEX(unit,[0]=
		    p[0],p[1],p[2],p[3],p[4],p[5]));
		p += SYSEX_CHUNK;
		l -= SYSEX_CHUNK;
	}
	if ( l > 0 ) {
		event = SEQ_MK_SYSEX(unit);
		memcpy(event.sysex.buffer, p, l);
		send_event(&event);
	}
}

void
playfile(FILE *f, char *name)
{
	u_char *buf, *nbuf;
	u_int tot, n, size, nread;

	/* 
	 * We need to read the whole file into memory for easy processing.
	 * Using mmap() would be nice, but some file systems do not support
	 * it, nor does reading from e.g. a pipe.  The latter also precludes
	 * finding out the file size without reading it.
	 */
	size = 1000;
	buf = malloc(size);
	if (buf == 0)
		errx(1, "malloc() failed");
	nread = size;
	tot = 0;
	for (;;) {
		n = fread(buf + tot, 1, nread, f);
		tot += n;
		if (n < nread)
			break;
		/* There must be more to read. */
		nread = size;
		nbuf = realloc(buf, size * 2);
		if (nbuf == NULL)
			errx(1, "realloc() failed");
		buf = nbuf;
		size *= 2;
	}
	playdata(buf, tot, name);
	free(buf);
}

void
playdata(u_char *buf, u_int tot, char *name)
{
	int format, ntrks, divfmt, ticks, t, besttrk = 0;
	u_int len, mlen, status, chan;
	u_char *p, *end, byte, meta, *msg;
	struct track *tracks;
	u_long bestcur, now;
	struct track *tp;

	end = buf + tot;
	if (verbose)
		printf("Playing %s (%d bytes) ... \n", name, tot);

	if (tot < MARK_LEN + 4) {
		warnx("Not a MIDI file, too short");
		return;
	}

	if (memcmp(buf, RMID_SIG, MARK_LEN) == 0) {
		u_char *eod;
		/* Detected a RMID file, let's just check if it's
		 * a MIDI file */
		if (GET32_LE(buf + MARK_LEN) != tot - 8) {
			warnx("Not a RMID file, bad header");
			return;
		}

		buf += MARK_LEN + 4;
		if (memcmp(buf, RMID_MIDI_ID, MARK_LEN) != 0) {
			warnx("Not a RMID file, bad ID");
			return;
		}

		/* Now look for the 'data' chunk, which contains
		 * MIDI data */
		buf += MARK_LEN;

		/* Test against end-8 since we must have at least 8 bytes
		 * left to read */
		while(buf < end-8 && memcmp(buf, RMID_DATA_ID, MARK_LEN))
			buf += GET32_LE(buf+4) + 8; /* MARK_LEN + 4 */

		if (buf >= end-8) {
			warnx("Not a valid RMID file, no data chunk");
			return;
		}

		buf += MARK_LEN; /* "data" */
		eod = buf + 4 + GET32_LE(buf);
		if (eod >= end) {
			warnx("Not a valid RMID file, bad data chunk size");
			return;
		}

		end = eod;
		buf += 4;
	}

	if (memcmp(buf, MARK_HEADER, MARK_LEN) != 0) {
		warnx("Not a MIDI file, missing header");
		return;
	}

	if (GET32(buf + MARK_LEN) != HEADER_LEN) {
		warnx("Not a MIDI file, bad header");
		return;
	}
	format = GET16(buf + MARK_LEN + SIZE_LEN);
	ntrks = GET16(buf + MARK_LEN + SIZE_LEN + 2);
	divfmt = GET8(buf + MARK_LEN + SIZE_LEN + 4);
	ticks = GET8(buf + MARK_LEN + SIZE_LEN + 5);
	p = buf + MARK_LEN + SIZE_LEN + HEADER_LEN;
	if ((divfmt & 0x80) == 0)
		ticks |= divfmt << 8;
	else
		errx(1, "Absolute time codes not implemented yet");
	if (verbose > 1)
		printf("format=%d ntrks=%d divfmt=%x ticks=%d\n",
		       format, ntrks, divfmt, ticks);
	if (format != 0 && format != 1) {
		warnx("Cannot play MIDI file of type %d", format);
		return;
	}
	if (ntrks == 0)
		return;
	tracks = malloc(ntrks * sizeof(struct track));
	if (tracks == NULL)
		errx(1, "malloc() tracks failed");
	for (t = 0; t < ntrks; ) {
		if (p >= end - MARK_LEN - SIZE_LEN) {
			warnx("Cannot find track %d", t);
			goto ret;
		}
		len = GET32(p + MARK_LEN);
		if (len > 1000000) { /* a safe guard */
			warnx("Crazy track length");
			goto ret;
		}
		if (memcmp(p, MARK_TRACK, MARK_LEN) == 0) {
			tracks[t].start = p + MARK_LEN + SIZE_LEN;
			tracks[t].end = tracks[t].start + len;
			tracks[t].curtime = getvar(&tracks[t]);
			t++;
		}
		p += MARK_LEN + SIZE_LEN + len;
	}

	/* 
	 * Play MIDI events by selecting the track with the lowest
	 * curtime.  Execute the event, update the curtime and repeat.
	 */
	if (sameprogram) {
		for(t = 0; t < 16; t++) {
			send_event(&SEQ_MK_CHN(PGM_CHANGE, .device=unit,
			    .channel=t, .program=sameprogram-1));
		}
	}
	/*
	 * The ticks variable is the number of ticks that make up a beat
	 * (beat: 24 MIDI clocks always, a quarter note by usual convention)
	 * and is used as a reference value for the delays between
	 * the MIDI events.
	 */
	now = 0;
	for (;;) {
		/* Locate lowest curtime */
		bestcur = ~0;
		for (t = 0; t < ntrks; t++) {
			if (tracks[t].curtime < bestcur) {
				bestcur = tracks[t].curtime;
				besttrk = t;
			}
		}
		if (bestcur == ~0)
			break;
		if (verbose > 1) {
			printf("DELAY %4ld TRACK %2d ", bestcur-now, besttrk);
			fflush(stdout);
		}
		if (now < bestcur) {
			u_int32_t delta = bestcur - now;
			delta = (int)((double)delta * tempo / (1000.0*ticks));
			send_event(&SEQ_MK_TIMING(WAIT_REL,.divisions=delta));
		}
		now = bestcur;
		tp = &tracks[besttrk];
		byte = *tp->start++;
		if (byte == MIDI_META) {
			meta = *tp->start++;
			mlen = getvar(tp);
			if (verbose > 1)
				printf("META %02x (%d)\n", meta, mlen);
			dometa(meta, tp->start, mlen);
			tp->start += mlen;
		} else {
			if (MIDI_IS_STATUS(byte))
				tp->status = byte;
			else
				tp->start--;
			mlen = MIDI_LENGTH(tp->status);
			msg = tp->start;
			if (verbose > 1) {
			    if (mlen == 1)
				printf("MIDI %02x (%d) %02x\n",
				       tp->status, mlen, msg[0]);
			    else   
				printf("MIDI %02x (%d) %02x %02x\n",
				       tp->status, mlen, msg[0], msg[1]);
			}
			status = MIDI_GET_STATUS(tp->status);
			chan = MIDI_GET_CHAN(tp->status);
			switch (status) {
			case MIDI_NOTEOFF:
				send_event(&SEQ_MK_CHN(NOTEOFF, .device=unit,
				.channel=chan, .key=msg[0], .velocity=msg[1]));
				break;
			case MIDI_NOTEON:
				send_event(&SEQ_MK_CHN(NOTEON, .device=unit,
				.channel=chan, .key=msg[0], .velocity=msg[1]));
				break;
			case MIDI_KEY_PRESSURE:
				send_event(&SEQ_MK_CHN(KEY_PRESSURE,
				.device=unit, .channel=chan,
				.key=msg[0], .pressure=msg[1]));
				break;
			case MIDI_CTL_CHANGE:
				send_event(&SEQ_MK_CHN(CTL_CHANGE,
				.device=unit, .channel=chan,
				.controller=msg[0], .value=msg[1]));
				break;
			case MIDI_PGM_CHANGE:
				if (!sameprogram)
					send_event(&SEQ_MK_CHN(PGM_CHANGE,
					.device=unit, .channel=chan,
					.program=msg[0]));
				break;
			case MIDI_CHN_PRESSURE:
				send_event(&SEQ_MK_CHN(CHN_PRESSURE,
				.device=unit, .channel=chan, .pressure=msg[0]));
				break;
			case MIDI_PITCH_BEND:
				send_event(&SEQ_MK_CHN(PITCH_BEND,
				.device=unit, .channel=chan,
				.value=(msg[0] & 0x7f) | ((msg[1] & 0x7f)<<7)));
				break;
			case MIDI_SYSTEM_PREFIX:
				mlen = getvar(tp);
				if (tp->status == MIDI_SYSEX_START) {
					send_sysex(tp->start, mlen);
					break;
				}
				/* Sorry, can't do this yet; FALLTHROUGH */
			default:
				if (verbose)
					printf("MIDI event 0x%02x ignored\n",
					       tp->status);
			}
			tp->start += mlen;
		}
		if (tp->start >= tp->end)
			tp->curtime = ~0;
		else
			tp->curtime += getvar(tp);
	}
	if (ioctl(fd, SEQUENCER_SYNC, 0) < 0)
		err(1, "SEQUENCER_SYNC");

 ret:
	free(tracks);
}

int
main(int argc, char **argv)
{
	int ch;
	int listdevs = 0;
	int example = 0;
	int nmidi;
	int t;
	const char *file = DEVMUSIC;
	const char *sunit;
	struct synth_info info;
	FILE *f;

	if ((sunit = getenv("MIDIUNIT")))
		unit = atoi(sunit);

	while ((ch = getopt(argc, argv, "?d:f:lmp:qt:vx")) != -1) {
		switch(ch) {
		case 'd':
			unit = atoi(optarg);
			break;
		case 'f':
			file = optarg;
			break;
		case 'l':
			listdevs++;
			break;
		case 'm':
			showmeta++;
			break;
		case 'p':
			sameprogram = atoi(optarg);
			break;
		case 'q':
			play = 0;
			break;
		case 't':
			ttempo = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			example++;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
    
	if (!play)
		goto output;

	fd = open(file, O_WRONLY);
	if (fd < 0)
		err(1, "%s", file);
	if (ioctl(fd, SEQUENCER_NRMIDIS, &nmidi) < 0)
		err(1, "ioctl(SEQUENCER_NRMIDIS) failed, ");
	if (nmidi == 0)
		errx(1, "Sorry, no MIDI devices available");
	if (listdevs) {
		for (info.device = 0; info.device < nmidi; info.device++) {
			if (ioctl(fd, SEQUENCER_INFO, &info) < 0)
				err(1, "ioctl(SEQUENCER_INFO) failed, ");
			printf("%d: %s\n", info.device, info.name);
		}
		exit(0);
	}

	/*
	 * The sequencer has two "knobs": the TIMEBASE and the TEMPO.
	 * The delay specified in TMR_WAIT_REL is specified in
	 * sequencer time units.  The length of a unit is
	 * 60*1000000 / (TIMEBASE * TEMPO).
	 * Set it to 1ms/unit (adjusted by user tempo changes).
	 */
	t = 500 * ttempo / 100;
	if (ioctl(fd, SEQUENCER_TMR_TIMEBASE, &t) < 0)
		err(1, "SEQUENCER_TMR_TIMEBASE");
	t = 120;
	if (ioctl(fd, SEQUENCER_TMR_TEMPO, &t) < 0)
		err(1, "SEQUENCER_TMR_TEMPO");
	if (ioctl(fd, SEQUENCER_TMR_START, 0) < 0)
		err(1, "SEQUENCER_TMR_START");

	midireset();

 output:
	if (example)
		while (example--)
			playdata(sample, sizeof sample, "<Gubben Noa>");
	else if (argc == 0)
		playfile(stdin, "<stdin>");
	else
		while (argc--) {
			f = fopen(*argv, "r");
			if (f == NULL)
				err(1, "%s", *argv);
			else {
				playfile(f, *argv);
				fclose(f);
			}
			argv++;
		}

	exit(0);
}
