/*	$NetBSD: extern.h,v 1.28.28.1 2009/05/13 19:18:05 jym Exp $ */

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)externs.h	8.1 (Berkeley) 5/31/93
 */

#include <sys/types.h>
#include <stdbool.h>

#include "machdep.h"

	/* program mode */
extern int mode;
#define MODE_PLAYER	1
#define MODE_DRIVER	2
#define MODE_LOGGER	3

	/* command line flags */
extern int randomize;			/* -x, give first available ship */
extern int longfmt;			/* -l, print score in long format */
extern int nobells;			/* -b, don't ring bell before Signal */

	/* other initial modes */
extern gid_t gid;
extern gid_t egid;

#define dieroll()		((random()) % 6 + 1)
#define sqr(a)		((a) * (a))
#define min(a,b)	((a) < (b) ? (a) : (b))

#define grappled(a)	((a)->file->ngrap)
#define fouled(a)	((a)->file->nfoul)
#define snagged(a)	(grappled(a) + fouled(a))

#define grappled2(a, b)	((a)->file->grap[(b)->file->index].sn_count)
#define fouled2(a, b)	((a)->file->foul[(b)->file->index].sn_count)
#define snagged2(a, b)	(grappled2(a, b) + fouled2(a, b))

#define Xgrappled2(a, b) ((a)->file->grap[(b)->file->index].sn_turn < turn-1 \
			  ? grappled2(a, b) : 0)
#define Xfouled2(a, b)	((a)->file->foul[(b)->file->index].sn_turn < turn-1 \
			 ? fouled2(a, b) : 0)
#define Xsnagged2(a, b)	(Xgrappled2(a, b) + Xfouled2(a, b))

#define cleangrapple(a, b, c)	Cleansnag(a, b, c, 1)
#define cleanfoul(a, b, c)	Cleansnag(a, b, c, 2)
#define cleansnag(a, b, c)	Cleansnag(a, b, c, 3)

#define sterncolour(sp)	((sp)->file->stern+'0'-((sp)->file->captured ? 10 : 0))
#define sternrow(sp)	((sp)->file->row + dr[(sp)->file->dir])
#define sterncol(sp)	((sp)->file->col + dc[(sp)->file->dir])

#define capship(sp)	((sp)->file->captured?(sp)->file->captured:(sp))

#define readyname(r)	((r) & R_LOADING ? '*' : ((r) & R_INITIAL ? '!' : ' '))

/* loadL and loadR, should match loadname[] */
#define L_EMPTY		0		/* should be 0, don't change */
#define L_GRAPE		1
#define L_CHAIN		2
#define L_ROUND		3
#define L_DOUBLE	4
#define L_EXPLODE	5

/*
 * readyL and readyR, these are bits, except R_EMPTY
 */
#define R_EMPTY		0		/* not loaded and not loading */
#define R_LOADING	1		/* loading */
#define R_DOUBLE	2		/* loading double */
#define R_LOADED	4		/* loaded */
#define R_INITIAL	8		/* loaded initial */

#define HULL		0
#define RIGGING		1

#define NLOG 10
struct logs {
	char l_name[20];
	int l_uid;
	int l_shipnum;
	int l_gamenum;
	int l_netpoints;
};

struct BP {
	struct ship *toship;
	short turnsent;
	short mensent;
};

struct snag {
	short sn_count;
	short sn_turn;
};

#define NSCENE	nscene
#define NSHIP	10
#define NBP	3

#define NNATION	8
#define N_A	0
#define N_B	1
#define N_S	2
#define N_F	3
#define N_J	4
#define N_D	5
#define N_K	6
#define N_O	7

struct File {
	int index;
	char captain[20];		/* 0 */
	short points;			/* 20 */
	unsigned char loadL;		/* 22 */
	unsigned char loadR;		/* 24 */
	unsigned char readyL;		/* 26 */
	unsigned char readyR;		/* 28 */
	struct BP OBP[NBP];		/* 30 */
	struct BP DBP[NBP];		/* 48 */
	char struck;			/* 66 */
	struct ship *captured;		/* 68 */
	short pcrew;			/* 70 */
	char movebuf[10];		/* 72 */
	char drift;			/* 82 */
	short nfoul;
	short ngrap;
	struct snag foul[NSHIP];	/* 84 */
	struct snag grap[NSHIP];	/* 124 */
	char RH;			/* 224 */
	char RG;			/* 226 */
	char RR;			/* 228 */
	char FS;			/* 230 */
	char explode;			/* 232 */
	char sink;			/* 234 */
	unsigned char dir;
	short col;
	short row;
	char loadwith;
	char stern;
};

struct ship {
	const char *shipname;		/* 0 */
	struct shipspecs *specs;	/* 2 */
	unsigned char nationality;	/* 4 */
	short shiprow;			/* 6 */
	short shipcol;			/* 8 */
	char shipdir;			/* 10 */
	struct File *file;		/* 12 */
};

struct scenario {
	char winddir;			/* 0 */
	char windspeed;			/* 2 */
	char windchange;		/* 4 */
	unsigned char vessels;		/* 12 */
	const char *name;		/* 14 */
	struct ship ship[NSHIP];	/* 16 */
};
extern struct scenario scene[];
extern int nscene;

struct shipspecs {
	char bs;
	char fs;
	char ta;
	short guns;
	unsigned char class;
	char hull;
	unsigned char qual;
	char crew1;
	char crew2;
	char crew3;
	char gunL;
	char gunR;
	char carL;
	char carR;
	int rig1;
	int rig2;
	int rig3;
	int rig4;
	short pts;
};
extern struct shipspecs specs[];

extern struct scenario *cc;		/* the current scenario */
extern struct ship *ls;		/* &cc->ship[cc->vessels] */

#define SHIP(s)		(&cc->ship[s])
#define foreachship(sp)	for ((sp) = cc->ship; (sp) < ls; (sp)++)

struct windeffects {
	char A, B, C, D;
};
extern const struct windeffects WET[7][6];

struct Tables {
	char H, G, C, R;
};
extern const struct Tables RigTable[11][6];
extern const struct Tables HullTable[11][6];

extern const char AMMO[9][4];
extern const char HDT[9][10];
extern const char HDTrake[9][10];
extern const char QUAL[9][5];
extern const char MT[9][3];

extern const char *const countryname[];
extern const char *const classname[];
extern const char *const directionname[];
extern const char *const qualname[];
extern const char loadname[];

extern const char rangeofshot[];

extern const char dr[], dc[];

extern int winddir;
extern int windspeed;
extern int turn;
extern int game;
extern int alive;
extern int people;
extern int hasdriver;

/* assorted.c */
void table(struct ship *, struct ship *, int, int, int, int);
void Cleansnag(struct ship *, struct ship *, int, int);

/* dr_1.c */
void unfoul(void);
void boardcomp(void);
void resolve(void);
void compcombat(void);
int next(void);

/* dr_2.c */
void thinkofgrapples(void);
void checkup(void);
void prizecheck(void);
void closeon(struct ship *, struct ship *, char *, size_t, int, int, bool);

/* dr_3.c */
void moveall(void);
void sendbp(struct ship *, struct ship *, int, int);
int is_toughmelee(struct ship *, struct ship *, int, int);
void reload(void);
void checksails(void);

/* dr_4.c */
void ungrap(struct ship *, struct ship *);
void grap(struct ship *, struct ship *);

/* dr_5.c */
void subtract(struct ship *, struct ship *, int, int [3], int);
int mensent(struct ship *, struct ship *, int[3], struct ship **, int *, int);

/* dr_main.c */
int dr_main(void);

/* game.c */
int maxturns(struct ship *, bool *);
int maxmove(struct ship *, int, int);

/* lo_main.c */
int lo_main(void);

/* misc.c */
int range(struct ship *, struct ship *);
struct ship *closestenemy(struct ship *, int, int);
int gunsbear(struct ship *, struct ship *);
int portside(struct ship *, struct ship *, int);
int colours(struct ship *);
void logger(struct ship *);

/* parties.c */
int meleeing(struct ship *, struct ship *);
int boarding(struct ship *, int);
void unboard(struct ship *, struct ship *, int);

/* pl_1.c */
void leave(int) __attribute__((__noreturn__));
void choke(int) __attribute__((__noreturn__));
void child(int);

/* pl_2.c */
void newturn(int);
void play(void) __attribute__((__noreturn__));

/* pl_3.c */
void acceptcombat(void);
void grapungrap(void);
void unfoulplayer(void);

/* pl_4.c */
void changesail(void);
void acceptsignal(void);
void lookout(void);
const char *saywhat(struct ship *, int);
void eyeball(struct ship *);

/* pl_5.c */
void acceptmove(void);
void acceptboard(void);

/* pl_6.c */
void repair(void);
void loadplayer(void);

/* pl_7.c */
void initscreen(void);
void cleanupscreen(void);
void Signal(const char *, struct ship *, ...)
	 __attribute__((__format__(__printf__,1,3)));
void Msg(const char *, ...)
	 __attribute__((__format__(__printf__,1,2)));
void prompt(const char *, struct ship *);
int sgetch(const char *, struct ship *, int);
void sgetstr(const char *, char *, int);
void centerview(void);
void upview(void);
void downview(void);
void leftview(void);
void rightview(void);

/* pl_main.c */
int pl_main(void);

/* sync.c */
void fmtship(char *, size_t, const char *, struct ship *);
void makesignal(struct ship *, const char *, struct ship *, ...)
	 __attribute__((__format__(__printf__,2,4)));
void makemsg(struct ship *, const char *, ...)
	 __attribute__((__format__(__printf__,2,3)));
int sync_exists(int);
int sync_open(void);
void sync_close(int);
int Sync(void);

void send_captain(struct ship *ship, const char *astr);
void send_captured(struct ship *ship, long a);
void send_class(struct ship *ship, long a);
void send_crew(struct ship *ship, long a, long b, long c);
void send_dbp(struct ship *ship, long a, long b, long c, long d);
void send_drift(struct ship *ship, long a);
void send_explode(struct ship *ship, long a);
void send_foul(struct ship *ship, long a);
void send_gunl(struct ship *ship, long a, long b);
void send_gunr(struct ship *ship, long a, long b);
void send_hull(struct ship *ship, long a);
void send_move(struct ship *ship, const char *astr);
void send_obp(struct ship *ship, long a, long b, long c, long d);
void send_pcrew(struct ship *ship, long a);
void send_unfoul(struct ship *ship, long a, long b);
void send_points(struct ship *ship, long a);
void send_qual(struct ship *ship, long a);
void send_ungrap(struct ship *ship, long a, long b);
void send_rigg(struct ship *ship, long a, long b, long c, long d);
void send_col(struct ship *ship, long a);
void send_dir(struct ship *ship, long a);
void send_row(struct ship *ship, long a);
void send_signal(struct ship *ship, const char *astr);
void send_sink(struct ship *ship, long a);
void send_struck(struct ship *ship, long a);
void send_ta(struct ship *ship, long a);
void send_alive(void);
void send_turn(long a);
void send_wind(long a, long b);
void send_fs(struct ship *ship, long a);
void send_grap(struct ship *ship, long a);
void send_rig1(struct ship *ship, long a);
void send_rig2(struct ship *ship, long a);
void send_rig3(struct ship *ship, long a);
void send_rig4(struct ship *ship, long a);
void send_begin(struct ship *ship);
void send_end(struct ship *ship);
void send_ddead(void);
