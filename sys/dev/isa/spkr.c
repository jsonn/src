/*	$NetBSD: spkr.c,v 1.1.14.2 2001/03/12 13:30:39 bouyer Exp $	*/

/*
 * spkr.c -- device driver for console speaker on 80386
 *
 * v1.1 by Eric S. Raymond (esr@snark.thyrsus.com) Feb 1990
 *      modified for 386bsd by Andrew A. Chernov <ache@astral.msk.su>
 *      386bsd only clean version, all SYSV stuff removed
 *      use hz value from param.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>

#include <dev/isa/pcppivar.h>

#include <dev/isa/spkrio.h>

cdev_decl(spkr);

int spkrprobe __P((struct device *, struct cfdata *, void *));
void spkrattach __P((struct device *, struct device *, void *));

struct spkr_softc {
	struct device sc_dev;
};

struct cfattach spkr_ca = {
	sizeof(struct spkr_softc), spkrprobe, spkrattach
};

static pcppi_tag_t ppicookie;

#define SPKRPRI (PZERO - 1)

static void tone __P((u_int, u_int));
static void rest __P((int));
static void playinit __P((void));
static void playtone __P((int, int, int));
static void playstring __P((char *, int));

static
void tone(hz, ticks)
/* emit tone of frequency hz for given number of ticks */
    u_int hz, ticks;
{
	pcppi_bell(ppicookie, hz, ticks, PCPPI_BELL_SLEEP);
}

static void
rest(ticks)
/* rest for given number of ticks */
    int	ticks;
{
    /*
     * Set timeout to endrest function, then give up the timeslice.
     * This is so other processes can execute while the rest is being
     * waited out.
     */
#ifdef SPKRDEBUG
    printf("rest: %d\n", ticks);
#endif /* SPKRDEBUG */
    if (ticks > 0)
	    tsleep(rest, SPKRPRI | PCATCH, "rest", ticks);
}

/**************** PLAY STRING INTERPRETER BEGINS HERE **********************
 *
 * Play string interpretation is modelled on IBM BASIC 2.0's PLAY statement;
 * M[LNS] are missing and the ~ synonym and octave-tracking facility is added.
 * Requires tone(), rest(), and endtone(). String play is not interruptible
 * except possibly at physical block boundaries.
 */

typedef int	bool;
#define TRUE	1
#define FALSE	0

#define toupper(c)	((c) - ' ' * (((c) >= 'a') && ((c) <= 'z')))
#define isdigit(c)	(((c) >= '0') && ((c) <= '9'))
#define dtoi(c)		((c) - '0')

static int octave;	/* currently selected octave */
static int whole;	/* whole-note time at current tempo, in ticks */
static int value;	/* whole divisor for note time, quarter note = 1 */
static int fill;	/* controls spacing of notes */
static bool octtrack;	/* octave-tracking on? */
static bool octprefix;	/* override current octave-tracking state? */

/*
 * Magic number avoidance...
 */
#define SECS_PER_MIN	60	/* seconds per minute */
#define WHOLE_NOTE	4	/* quarter notes per whole note */
#define MIN_VALUE	64	/* the most we can divide a note by */
#define DFLT_VALUE	4	/* default value (quarter-note) */
#define FILLTIME	8	/* for articulation, break note in parts */
#define STACCATO	6	/* 6/8 = 3/4 of note is filled */
#define NORMAL		7	/* 7/8ths of note interval is filled */
#define LEGATO		8	/* all of note interval is filled */
#define DFLT_OCTAVE	4	/* default octave */
#define MIN_TEMPO	32	/* minimum tempo */
#define DFLT_TEMPO	120	/* default tempo */
#define MAX_TEMPO	255	/* max tempo */
#define NUM_MULT	3	/* numerator of dot multiplier */
#define DENOM_MULT	2	/* denominator of dot multiplier */

/* letter to half-tone:  A   B  C  D  E  F  G */
static const int notetab[8] = {9, 11, 0, 2, 4, 5, 7};

/*
 * This is the American Standard A440 Equal-Tempered scale with frequencies
 * rounded to nearest integer. Thank Goddess for the good ol' CRC Handbook...
 * our octave 0 is standard octave 2.
 */
#define OCTAVE_NOTES	12	/* semitones per octave */
static const int pitchtab[] =
{
/*        C     C#    D     D#    E     F     F#    G     G#    A     A#    B*/
/* 0 */   65,   69,   73,   78,   82,   87,   93,   98,  103,  110,  117,  123,
/* 1 */  131,  139,  147,  156,  165,  175,  185,  196,  208,  220,  233,  247,
/* 2 */  262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
/* 3 */  523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,
/* 4 */ 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1975,
/* 5 */ 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951,
/* 6 */ 4186, 4435, 4698, 4978, 5274, 5588, 5920, 6272, 6644, 7040, 7459, 7902,
};
#define NOCTAVES (sizeof(pitchtab) / sizeof(pitchtab[0]) / OCTAVE_NOTES)

static void
playinit()
{
    octave = DFLT_OCTAVE;
    whole = (hz * SECS_PER_MIN * WHOLE_NOTE) / DFLT_TEMPO;
    fill = NORMAL;
    value = DFLT_VALUE;
    octtrack = FALSE;
    octprefix = TRUE;	/* act as though there was an initial O(n) */
}

static void
playtone(pitch, value, sustain)
/* play tone of proper duration for current rhythm signature */
    int	pitch, value, sustain;
{
    int	sound, silence, snum = 1, sdenom = 1;

    /* this weirdness avoids floating-point arithmetic */
    for (; sustain; sustain--)
    {
	snum *= NUM_MULT;
	sdenom *= DENOM_MULT;
    }

    if (pitch == -1)
	rest(whole * snum / (value * sdenom));
    else
    {
	sound = (whole * snum) / (value * sdenom)
		- (whole * (FILLTIME - fill)) / (value * FILLTIME);
	silence = whole * (FILLTIME-fill) * snum / (FILLTIME * value * sdenom);

#ifdef SPKRDEBUG
	printf("playtone: pitch %d for %d ticks, rest for %d ticks\n",
	    pitch, sound, silence);
#endif /* SPKRDEBUG */

	tone(pitchtab[pitch], sound);
	if (fill != LEGATO)
	    rest(silence);
    }
}

static void
playstring(cp, slen)
/* interpret and play an item from a notation string */
    char	*cp;
    int		slen;
{
    int		pitch, lastpitch = OCTAVE_NOTES * DFLT_OCTAVE;

#define GETNUM(cp, v)	for(v=0; slen > 0 && isdigit(cp[1]); ) \
				{v = v * 10 + (*++cp - '0'); slen--;}
    for (; slen--; cp++)
    {
	int		sustain, timeval, tempo;
	char	c = toupper(*cp);

#ifdef SPKRDEBUG
	printf("playstring: %c (%x)\n", c, c);
#endif /* SPKRDEBUG */

	switch (c)
	{
	case 'A':  case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':

	    /* compute pitch */
	    pitch = notetab[c - 'A'] + octave * OCTAVE_NOTES;

	    /* this may be followed by an accidental sign */
	    if (slen > 0 && (cp[1] == '#' || cp[1] == '+'))
	    {
		++pitch;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && cp[1] == '-')
	    {
		--pitch;
		++cp;
		slen--;
	    }

	    /*
	     * If octave-tracking mode is on, and there has been no octave-
	     * setting prefix, find the version of the current letter note
	     * closest to the last regardless of octave.
	     */
	    if (octtrack && !octprefix)
	    {
		if (abs(pitch-lastpitch) > abs(pitch+OCTAVE_NOTES-lastpitch))
		{
		    ++octave;
		    pitch += OCTAVE_NOTES;
		}

		if (abs(pitch-lastpitch) > abs((pitch-OCTAVE_NOTES)-lastpitch))
		{
		    --octave;
		    pitch -= OCTAVE_NOTES;
		}
	    }
	    octprefix = FALSE;
	    lastpitch = pitch;

	    /* ...which may in turn be followed by an override time value */
	    GETNUM(cp, timeval);
	    if (timeval <= 0 || timeval > MIN_VALUE)
		timeval = value;

	    /* ...and/or sustain dots */
	    for (sustain = 0; slen > 0 && cp[1] == '.'; cp++)
	    {
		slen--;
		sustain++;
	    }

	    /* time to emit the actual tone */
	    playtone(pitch, timeval, sustain);
	    break;

	case 'O':
	    if (slen > 0 && (cp[1] == 'N' || cp[1] == 'n'))
	    {
		octprefix = octtrack = FALSE;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && (cp[1] == 'L' || cp[1] == 'l'))
	    {
		octtrack = TRUE;
		++cp;
		slen--;
	    }
	    else
	    {
		GETNUM(cp, octave);
		if (octave >= NOCTAVES)
		    octave = DFLT_OCTAVE;
		octprefix = TRUE;
	    }
	    break;

	case '>':
	    if (octave < NOCTAVES - 1)
		octave++;
	    octprefix = TRUE;
	    break;

	case '<':
	    if (octave > 0)
		octave--;
	    octprefix = TRUE;
	    break;

	case 'N':
	    GETNUM(cp, pitch);
	    for (sustain = 0; slen > 0 && cp[1] == '.'; cp++)
	    {
		slen--;
		sustain++;
	    }
	    playtone(pitch - 1, value, sustain);
	    break;

	case 'L':
	    GETNUM(cp, value);
	    if (value <= 0 || value > MIN_VALUE)
		value = DFLT_VALUE;
	    break;

	case 'P':
	case '~':
	    /* this may be followed by an override time value */
	    GETNUM(cp, timeval);
	    if (timeval <= 0 || timeval > MIN_VALUE)
		timeval = value;
	    for (sustain = 0; slen > 0 && cp[1] == '.'; cp++)
	    {
		slen--;
		sustain++;
	    }
	    playtone(-1, timeval, sustain);
	    break;

	case 'T':
	    GETNUM(cp, tempo);
	    if (tempo < MIN_TEMPO || tempo > MAX_TEMPO)
		tempo = DFLT_TEMPO;
	    whole = (hz * SECS_PER_MIN * WHOLE_NOTE) / tempo;
	    break;

	case 'M':
	    if (slen > 0 && (cp[1] == 'N' || cp[1] == 'n'))
	    {
		fill = NORMAL;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && (cp[1] == 'L' || cp[1] == 'l'))
	    {
		fill = LEGATO;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && (cp[1] == 'S' || cp[1] == 's'))
	    {
		fill = STACCATO;
		++cp;
		slen--;
	    }
	    break;
	}
    }
}

/******************* UNIX DRIVER HOOKS BEGIN HERE **************************
 *
 * This section implements driver hooks to run playstring() and the tone(),
 * endtone(), and rest() functions defined above.
 */

static int spkr_active;	/* exclusion flag */
static void *spkr_inbuf;

static int spkr_attached = 0;

int
spkrprobe (parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (!spkr_attached);
}

void
spkrattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf("\n");
	ppicookie = ((struct pcppi_attach_args *)aux)->pa_cookie;
	spkr_attached = 1;
}

int
spkropen(dev, flags, mode, p)
    dev_t dev;
    int	flags;
    int mode;
    struct proc *p;
{
#ifdef SPKRDEBUG
    printf("spkropen: entering with dev = %x\n", dev);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0 || !spkr_attached)
	return(ENXIO);
    else if (spkr_active)
	return(EBUSY);
    else
    {
	playinit();
	spkr_inbuf = malloc(DEV_BSIZE, M_DEVBUF, M_WAITOK);
	spkr_active = 1;
    }
    return(0);
}

int
spkrwrite(dev, uio, flags)
    dev_t dev;
    struct uio *uio;
    int flags;
{
    int n;
    int error;
#ifdef SPKRDEBUG
    printf("spkrwrite: entering with dev = %x, count = %d\n",
		dev, uio->uio_resid);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0)
	return(ENXIO);
    else
    {
	n = min(DEV_BSIZE, uio->uio_resid);
	error = uiomove(spkr_inbuf, n, uio);
	if (!error)
		playstring((char *)spkr_inbuf, n);
	return(error);
    }
}

int spkrclose(dev, flags, mode, p)
    dev_t	dev;
    int flags;
    int mode;
    struct proc *p;
{
#ifdef SPKRDEBUG
    printf("spkrclose: entering with dev = %x\n", dev);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0)
	return(ENXIO);
    else
    {
	tone(0, 0);
	free(spkr_inbuf, M_DEVBUF);
	spkr_active = 0;
    }
    return(0);
}

int spkrioctl(dev, cmd, data, flag, p)
    dev_t dev;
    u_long cmd;
    caddr_t data;
    int	flag;
    struct proc *p;
{
#ifdef SPKRDEBUG
    printf("spkrioctl: entering with dev = %x, cmd = %lx\n", dev, cmd);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0)
	return(ENXIO);
    else if (cmd == SPKRTONE)
    {
	tone_t	*tp = (tone_t *)data;

	if (tp->frequency == 0)
	    rest(tp->duration);
	else
	    tone(tp->frequency, tp->duration);
    }
    else if (cmd == SPKRTUNE)
    {
	tone_t  *tp = (tone_t *)(*(caddr_t *)data);
	tone_t ttp;
	int error;

	for (; ; tp++) {
	    error = copyin(tp, &ttp, sizeof(tone_t));
	    if (error)
		    return(error);
	    if (ttp.duration == 0)
		    break;
	    if (ttp.frequency == 0)
		rest(ttp.duration);
	    else
		tone(ttp.frequency, ttp.duration);
	}
    }
    else
	return(EINVAL);
    return(0);
}

/* spkr.c ends here */
