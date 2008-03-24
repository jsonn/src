/*	$NetBSD: monop.h,v 1.14.2.1 2008/03/24 07:14:42 keiichi Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
 *	@(#)monop.h	8.1 (Berkeley) 5/31/93
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	bool	char

#define	TRUE	(1)
#define	FALSE	(0)

#define	N_MON	8	/* number of monopolies			*/
#define	N_PROP	22	/* number of normal property squares	*/
#define	N_RR	4	/* number of railroads			*/
#define	N_UTIL	2	/* number of utilities			*/
#define	N_SQRS	40	/* number of squares on board		*/
#define	MAX_PL	9	/* maximum number of players		*/
#define	MAX_PRP	(N_PROP+N_RR+N_UTIL) /* max # ownable property	*/
#define	N_HOUSE	32	/* total number of houses available	*/
#define	N_HOTEL	12	/* total number of hotels available	*/

				/* square type numbers			*/
#define	PRPTY	0	/* normal property			*/
#define	RR	1	/* railroad				*/
#define	UTIL	2	/* water works - electric co		*/
#define	SAFE	3	/* safe spot				*/
#define	CC	4	/* community chest			*/
#define	CHANCE	5	/* chance (surprise!!!)			*/
#define	INC_TAX	6	/* Income tax */
#define	GOTO_J	7	/* Go To Jail! */
#define	LUX_TAX	8	/* Luxury tax */
#define	IN_JAIL	9	/* In jail */

#define	JAIL	40	/* JAIL square number			*/

#define	lucky(str)	printf("%s%s\n",str,lucky_mes[roll(1,num_luck)-1])
#define	printline()	printf("------------------------------\n")
#define	sqnum(sqp)	((short)(sqp - board))

struct sqr_st {			/* structure for square			*/
	const char	*name;		/* place name			*/
	short	owner;			/* owner number			*/
	short	type;			/* place type			*/
	struct prp_st	*desc;		/* description struct		*/
	int	cost;			/* cost				*/
};

typedef struct sqr_st	SQUARE;

struct mon_st {			/* monopoly description structure	*/
	const char	*name;		/* monop. name (color)		*/
	short	owner;			/* owner of monopoly		*/
	short	num_in;			/* # in monopoly		*/
	short	num_own;		/* # owned (-1: not poss. monop)*/
	short	h_cost;			/* price of houses		*/
	const char	*not_m;		/* name if not monopoly		*/
	const char	*mon_n;		/* name if a monopoly		*/
	unsigned char	sqnums[3];	/* Square numbers (used to init)*/
	SQUARE	*sq[3];			/* list of squares in monop	*/
};

typedef struct mon_st	MON;

/*
 * This struct describes a property.  For railroads and utilities, only
 * the "morg" member is used.
 */
struct prp_st {			/* property description structure	*/
	bool	morg;			/* set if mortgaged		*/
	bool	monop;			/* set if monopoly		*/
	short	square;			/* square description		*/
	short	houses;			/* number of houses		*/
	MON	*mon_desc;		/* name of color		*/
	int	rent[6];		/* rents			*/
};

struct own_st {			/* element in list owned things		*/
	SQUARE	*sqr;			/* pointer to square		*/
	struct own_st	*next;		/* next in list			*/
};

typedef struct own_st	OWN;

struct plr_st {			/* player description structure		*/
	char	*name;			/* owner name			*/
	short	num_gojf;		/* # of get-out-of-jail-free's	*/
	short	num_rr;			/* # of railroads owned		*/
	short	num_util;		/* # of water works/elec. co.	*/
	short	loc;			/* location on board		*/
	short	in_jail;		/* count of turns in jail	*/
	int	money;			/* amount of money		*/
	OWN	*own_list;		/* start of property list	*/
};

typedef struct plr_st	PLAY;
typedef struct prp_st	PROP;
typedef struct prp_st	RR_S;
typedef struct prp_st	UTIL_S;

extern bool	trading, spec, fixing, told_em;

extern const char	*const yncoms[], *const comlist[], *name_list[], *const lucky_mes[];

extern int	num_play, player, num_doub, num_luck;

extern void (*const func[])(void);

extern MON	mon[N_MON];

extern PLAY	*play, *cur_p;

extern PROP	prop[N_PROP];

extern RR_S	rr[N_RR];

extern SQUARE	board[N_SQRS + 1];

extern UTIL_S	util[2];


/* cards.c */
void ret_card(PLAY *);

/* execute.c */
void execute(int);
void do_move(void);
void move(int);
void save(void);
void restore(void);
int rest_f(const char *);

/* getinp.c */
int getinp(const char *, const char *const []);

/* houses.c */
void buy_houses(void);
void sell_houses(void);

/* jail.c */
void card(void);
void pay(void);
int move_jail(int, int );
void printturn(void);

/* misc.c */
int getyn(const char *);
void notify(void);
void next_play(void);
int get_int(const char *);
void set_ownlist(int);
void is_monop(MON *, int);
void is_not_monop(MON *);
void list(void);
void list_all(void);
void quit(void);

/* morg.c */
void mortgage(void);
void unmortgage(void);
void force_morg(void);

/* print.c */
void printboard(void);
void where(void);
void printsq(int, bool);
void printhold(int);

/* prop.c */
void buy(int, SQUARE *);
void add_list(int, OWN **, int);
void del_list(int, OWN **, short);
void bid(void);
int prop_worth(PLAY *);

/* rent.c */
void rent(SQUARE *);

/* roll.c */
int roll(int, int);

/* spec.c */
void inc_tax(void);
void goto_jail(void);
void lux_tax(void);
void cc(void);
void chance(void);

/* trade.c */
void trade(void);
void resign(void);
