/*	$NetBSD: hpckbdkeymap.h,v 1.6.2.2 2000/11/20 20:45:54 bouyer Exp $ */

/*
 * Copyright (c) 1999, 2000, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */
#define UNK	255	/* unknown */
#define IGN	254	/* ignore */
#define SPL	253	/* special key */
#define KC(n)	KS_KEYCODE(n)
#define CMDMAP(map)	{ map, (sizeof(map)/sizeof(keysym_t)) }
#define NULLCMDMAP	{ NULL, 0 }

#define KEY_SPECIAL_OFF		0
#define KEY_SPECIAL_LIGHT	1

const u_int8_t default_keymap[] = {
/*      0    1    2    3    4    5    6    7 */       
/* 0 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 1 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 2 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 3 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 4 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 5 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 6 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 7 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 8 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 9 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*10 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*11 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*12 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*13 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*14 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK
};

const int default_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1,
	[KEY_SPECIAL_LIGHT]	= -1
};

const u_int8_t tc5165_mobilon_keymap[] = {
/*      0    1    2    3    4    5    6    7 */       
/* 0 */	37 , 45 , 44 , UNK, 9  , 51 , 23 , UNK,
/* 1 */	UNK, 56 , UNK, UNK, UNK, UNK, UNK, UNK,
/* 2 */	UNK, UNK, 29 , UNK, UNK, UNK, UNK, UNK,
/* 3 */	24 , 203, UNK, 38 , 10 , 27 , 13 , UNK,
/* 4 */	40 , UNK, UNK, 39 , 26 , 53 , 11 , 12 ,
/* 5 */	UNK, UNK, UNK, 53 , 25 , UNK, UNK, SPL, /* Light */
/* 6 */	208, UNK, UNK, UNK, 52 , UNK, 43 , 14 ,
/* 7 */	205, 200, UNK, UNK, SPL, UNK, UNK, 28 , /* Off key */
/* 8 */	UNK, 41 , 59 , 15 , 2  , UNK, UNK, UNK,
/* 9 */	63 , 64 , 1  , UNK, 65 , 16 , 17 , UNK,
/*10 */	60 , UNK, 61 , 62 , 3  , UNK, UNK, UNK,
/*11 */	UNK, UNK, UNK, 42 , 58 , UNK, UNK, UNK,
/*12 */	47 , 33 , 46 , 5  , 4  , 18 , 19 , UNK,
/*13 */	34 , 35 , 20 , 48 , 6  , 7  , 21 , 49 ,
/*14 */	22 , 31 , 32 , 36 , 8  , 30 , 50 , 57 ,
/*15 */	UNK, IGN, UNK, UNK, UNK, UNK, UNK, UNK /* Windows key */
};

const int tc5165_mobilon_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 60,
	[KEY_SPECIAL_LIGHT]	= 47
};

const u_int8_t tc5165_telios_jp_keymap[] = {
/*      0    1    2    3    4    5    6    7 */       
/* 0 */	58,  15,  IGN, 1,   IGN, IGN, IGN, IGN,
/* 1 */	IGN, IGN, IGN, IGN, 54,  42,  IGN, IGN,
/* 2 */	31,  18,  4,   IGN, IGN, 32,  45,  59,
/* 3 */	33,  19,  5,   61,  IGN, 46,  123, 60,
/* 4 */	35,  21,  8,   64,  IGN, 48,  49,  63,
/* 5 */	17,  16,  3,   IGN, 2,   30,  44,  41,
/* 6 */	IGN, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 7 */	IGN, IGN, IGN, IGN, IGN, IGN, 56,  IGN,
/* 8 */	34,  20,  7,   IGN, 6,   47,  57,  62,
/* 9 */	IGN, IGN, IGN, IGN, IGN, IGN, 29,  IGN,
/*10 */	27,  125, 13,  75,  80,  40,  115, 68,
/*11 */	39,  26,  25,  IGN, 12,  52,  53,  67,
/*12 */	37,  24,  11,  IGN, 10,  38,  51,  66,
/*13 */	23,  22,  9,   IGN, IGN, 36,  50,  65,
/*14 */	28,  43,  14,  72,  77,  IGN, IGN, 211,
/*15 */	IGN, IGN, IGN, IGN, IGN, IGN, 221, IGN
};

const u_int8_t tc5165_compaq_c_jp_keymap[] = {
/*      0    1    2    3    4    5    6    7 */       
/* 0 */	38,  50,  49,  48,  47,  46,  45,  44, 
/* 1 */	56,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 2 */	13,  IGN, 112, 121, 123, 41,  28,  57, 
/* 3 */	77,  75,  80,  72,  39,  53,  52,  51,
/* 4 */	24,  25,  40,  IGN, 43,  26,  115, 58,
/* 5 */	54,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 6 */	IGN, IGN, IGN, SPL, IGN, IGN, IGN, IGN, /* Light */
/* 7 */	IGN, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 8 */	42,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 9 */	29,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*10 */	221, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*11 */	221, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*12 */	14,  27,  12,  11,  10,  15,  1,   125,
/*13 */	9,   8,   7,   6,   5,   4,   3,   2,
/*14 */	23,  22,  21,  20,  19,  18,  17,  16,
/*15 */	37,  36,  35,  34,  33,  32,  31,  30
};

const int tc5165_compaq_c_jp_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1, /* don't have off button */
	[KEY_SPECIAL_LIGHT]	= 51
};

const u_int8_t m38813c_keymap[] = {
/*      0    1    2    3    4    5    6    7 */       
/* 0 */	0,   1,   2,   3,   4,   5,   6,   7,
/* 1 */	8,   9,   10,  11,  12,  13,  14,  15,
/* 2 */	16,  17,  18,  19,  20,  21,  22,  23,
/* 3 */	24,  25,  26,  27,  28,  29,  30,  31,
/* 4 */	32,  33,  34,  35,  36,  37,  38,  39,
/* 5 */	40,  41,  42,  43,  44,  45,  46,  47,
/* 6 */	48,  49,  50,  51,  52,  53,  54,  55,
/* 7 */	56,  57,  58,  59,  60,  61,  62,  63,
/* 8 */	64,  65,  66,  67,  68,  69,  70,  71,
/* 9 */	72,  73,  74,  75,  76,  77,  78,  79,
/*10 */	80,  81,  82,  83,  84,  85,  86,  87,
/*11 */	88,  89,  90,  91,  92,  93,  94,  95,
/*12 */	96,  97,  98,  99,  100, 101, 102, 103,
/*13 */	104, 105, 106, 107, 108, 109, 110, 111,
/*14 */	112, 113, 114, 115, 116, 117, 118, 119,
/*15 */	120, 121, 122, 123, 124, 125, 126, 127
};

/* NEC MobileGearII MCR series (Japan) */
static u_int8_t mcr_jp_keytrans[] = {
/*00	right	ent	p	.	y	b	z	space	*/
/*08	down	/	o	,	t	v	a	nfer	*/
/*10	left	\	i	m	r	c	w	menu	*/
/*18	^	-	u	-	e	x	q	1	*/
/*20	pgdn	h/z	0	l	:	g	tab	f1	*/
/*28	xfer	;	9	n	5	f	2	k	*/
/*30	up	[	8	j	4	d	6	-	*/
/*38	-	@	7	h	3	]	s	-	*/
/*40	caps	-	-	-	bs	fnc	f8	f3	*/
/*48	-	alt	-	-	|	k/h	f7	f4	*/
/*50	-	-	ctrl	-	f10	pgup	f6	f2	*/
/*58	-	-	-	shift	del	f9	f5	esc	*/
/*----------------------------------------------------------------------*/
/*00*/	 77,	 28,	 25,	 52,	 21,	 48,	 44,	 57,
/*08*/	 80,	 53,	 24,	 51,	 20,	 47,	 30,	123,
/*10*/	 75,	115,	 23,	 50,	 19,	 46,	 17,	221,
/*18*/	 13,	IGN,	 22,	IGN,	 18,	 45,	 16,	  2,
/*20*/	 81,	 41,	 11,	 38,	 40,	 34,	 15,	 59,
/*28*/	121,	 39,	 10,	 49,	  6,	 33,	  3,	 37,
/*30*/	 72,	 27,	  9,	 36,	  5,	 32,	  7,	IGN,
/*38*/	 12,	 26,	  8,	 35,	  4,	 43,	 31,	IGN,
/*40*/	 58,	IGN,	IGN,	IGN,	 14,	184,	 66,	 61,
/*48*/	IGN,	 56,	IGN,	IGN,	125,	112,	 65,	 62,
/*50*/	IGN,	IGN,	 29,	IGN,	 68,	 73,	 64,	 60,
/*58*/	IGN,	IGN,	IGN,	 42,	 14,	 67,	 63,	  1,
};

static const keysym_t mcr_jp_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(184), KS_Cmd,		KS_Alt_R,	KS_Multi_key,
	KC(73),  KS_Cmd_BrightnessUp,	KS_KP_Prior,	KS_KP_9,
	KC(81),  KS_Cmd_BrightnessDown,	KS_KP_Next,	KS_KP_3,
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

/* IBM WorkPad z50 */
static u_int8_t z50_keytrans[] = {
/*00	f1	f3	f5	f7	f9	-	-	f11	*/
/*08	f2	f4	f6	f8	f10	-	-	f12	*/
/*10	'	[	-	0	p	;	up	/	*/
/*18	-	-	-	9	o	l	.	-	*/
/*20	left	]	=	8	i	k	,	-	*/
/*28	h	y	6	7	u	j	m	n	*/
/*30	-	bs	num	del	-	\	ent	sp	*/
/*38	g	t	5	4	r	f	v	b	*/
/*40	-	-	-	3	e	d	c	right	*/
/*48	-	-	-	2	w	s	x	down	*/
/*50	esc	tab	~	1	q	a	z	-	*/
/*58	menu	Ls	Lc	Rc	La	Ra	Rs	-	*/
/*----------------------------------------------------------------------*/
/*00*/	 59,	 61,	 63,	 65,	 67,	IGN,	IGN,	 87,
/*08*/	 60,	 62,	 64,	 66,	 68,	IGN,	IGN,	 88,
/*10*/	 40,	 26,	 12,	 11,	 25,	 39,	 72,	 53,
/*18*/	IGN,	IGN,	IGN,	 10,	 24,	 38,	 52,	IGN,
/*20*/	 75,	 27,	 13,	  9,	 23,	 37,	 51,	IGN,
/*28*/	 35,	 21,	  7,	  8,	 22,	 36,	 50,	 49,
/*30*/	IGN,	 14,	 69,	 14,	IGN,	 43,	 28,	 57,
/*38*/	 34,	 20,	  6,	  5,	 19,	 33,	 47,	 48,
/*40*/	IGN,	IGN,	IGN,	  4,	 18,	 32,	 46,	 77,
/*48*/	IGN,	IGN,	IGN,	  3,	 17,	 31,	 45,	 80,
/*50*/	  1,	 15,	 41,	  2,	 16,	 30,	 44,	IGN,
/*58*/	221,	 42,	 29,	 29,	 56,	 56,	 54,	IGN,
};

/* Sharp Tripad PV6000 and VADEM CLIO */
static u_int8_t tripad_keytrans[] = {
/*00	lsh	tab	`	q	esc	1	WIN	-	*/
/*08	ctrl	z	x	a	s	w	e	2	*/
/*10	lalt	sp	c	v	d	f	r	3	*/
/*18	b	n	g	h	t	y	4	5	*/
/*20	m	,	j	k	u	i	6	7	*/
/*28	Fn	caps	l	o	p	8	9	0	*/
/*30	[	]	la	.	/	;	-	=	*/
/*38	rsh	ra	ua	da	'	ent	\	del	*/
/*40	-	-	-	-	-	-	-	-	*/
/*48	-	-	-	-	-	-	-	-	*/
/*50	-	-	-	-	-	-	-	-	*/
/*58	-	-	-	-	-	-	-	-	*/
/*----------------------------------------------------------------------*/
/*00*/	 42,	 15,	 41,	 16,	  1,	  2,	104,	221,
/*08*/	 29,	 44,	 45,	 30,	 31,	 17,	 18,	  3,
/*10*/	 56,	 57,	 46,	 47,	 32,	 33,	 19,	  4,
/*18*/	 48,	 49,	 34,	 35,	 20,	 21,	  5,	  6,
/*20*/	 50,	 51,	 36,	 37,	 22,	 23,	  7,	  8,
/*28*/	105,	 58,	 38,	 24,	 25,	  9,	 10,	 11,
/*30*/	 26,	 27,	 75,	 52,	 53,	 39,	 12,	 13,
/*38*/	 54,	 77,	 72,	 80,	 40,	 28,	 43,	 14,
/*40*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*48*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*50*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*58*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
};

/* NEC Mobile Gear MCCS series */
static u_int8_t mccs_keytrans[] = {
/*00	caps	cr	rar	p	.	y	b	z	*/
/*08	alt	[	dar	o	,	t	v	a	*/
/*10	zen	@	lar	i	m	r	c	w	*/
/*18	lctrl	;	uar	u	n	e	x	q	*/
/*20	lshft	bs	\	0	l	6	g	tab	*/
/*28	nconv	|	/	9	k	5	f	2	*/
/*30	conv	=	]	8	j	4	d	1	*/
/*38	hira	-	'	7	h	3	s	esc	*/
/*40	-	sp	-	-	-	-	-	-	*/
/*48	-	-	-	-	-	-	-	-	*/
/*50	-	-	-	-	-	-	-	-	*/
/*58	-	-	-	-	-	-	-	-	*/
/*----------------------------------------------------------------------*/
/*00*/	 58,	 28,	 77,	 25,	 52,	 21,	 48,	 44,
/*08*/	 56,	 27,	 80,	 24,	 51,	 20,	 47,	 30,
/*10*/	 41,	 26,	 75,	 23,	 50,	 19,	 46,	 17,
/*18*/	 29,	 39,	 72,	 22,	 49,	 18,	 45,	 16,
/*20*/	 42,	 14,	115,	 11,	 38,	  7,	 34,	 15,
/*28*/	123,	125,	 53,	 10,	 37,	  6,	 33,	  3,
/*30*/	121,	 13,	 43,	  9,	 36,	  5,	 32,	  2,
/*38*/	112,	 12,	 40,	  8,	 35,	  4,	 31,	  1,
/*40*/	IGN,	 57,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*48*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*50*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*58*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
};

static const keysym_t mccs_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

static u_int8_t mobilepro_keytrans[] = {
/*00	space	]	\	/	-	-	enter	l	*/
/*08	-	[	'	;	-	-	.	o	*/
/*10	-	-	-	Windows	v	c	x	z	*/
/*18	-	=	\-	`	f	d	s	a	*/
/*20	8	7	6	5	r	e	w	q	*/
/*28	,	m	n	b	-	-	0	9	*/
/*30	k	j	h	g	4	3	2	1	*/
/*38	i	u	y	t	-	caps	del	esc	*/
/*40	alt_R	-	-	-	BS	p	TAB	Fn	*/
/*48	-	alt_L	-	-	f12	f11	f10	f9	*/
/*50	-	-	ctrl	-	f8	f7	f6	f5	*/
/*58	-	-	-	shift	f4	f3	f2	f1	*/
/*----------------------------------------------------------------------*/
/*00*/	 57,	 27,	 43,	 53,	 75,	 80,	 28,	 38,
/*08*/	IGN,	 26,	 40,	 39,	 77,	 72,	 52,	 24,
/*10*/	IGN,	IGN,	IGN,	221,	 47,	 46,	 45,	 44,
/*18*/	IGN,	 13,	 12,	 41,	 33,	 32,	 31,	 30,
/*20*/	  9,	  8,	  7,	  6,	 19,	 18,	 17,	 16,
/*28*/	 51,	 50,	 49,	 48,	IGN,	IGN,	 11,	 10,
/*30*/	 37,	 36,	 35,	 34,	  5,	  4,	  3,	  2,
/*38*/	 23,	 22,	 21,	 20,	IGN,	 58,	 14,	  1,
/*40*/	184,	IGN,	IGN,	IGN,	 14,	 25,	 15,	IGN,
/*48*/	IGN,	 56,	IGN,	IGN,	 88,	 87,	 68,	 67,
/*50*/	IGN,	IGN,	 29,	IGN,	 66,	 65,	 64,	 63,
/*58*/	IGN,	IGN,	IGN,	 42,	 62,	 61,	 60,	 59,
};

/* NEC MobilePro 750c by "Castor Fu" <castor@geocast.com> */
static u_int8_t mobilepro750c_keytrans[] = {
/*00	right	\	p	.	y	b	z	space	*/
/*08	down	/	o	,	t	v	a	-	*/
/*10	left	enter	i	m	r	c	w	Win	*/
/*18	num	]	u	n	e	x	q	caps	*/
/*20	pgdn	-	0	l	:	g	tab	esc	*/
/*28	-	;	9	k	5	f	2	`	*/
/*30	up	[	8	j	4	d	1	'	*/
/*38	-	@	7	h	3	s	del	-	*/
/*40	shift	-	-	-	bs	f12	f8	f4	*/
/*48	-	alt	-	-	|	f11	f7	f3	*/
/*50	-	-	ctrl	-	f10	f10	f6	f2	*/
/*58	-	-	-	shift	del	f9	f5	f1	*/
/*----------------------------------------------------------------------*/
/*00*/	77,	43,	25,	52,	21,	48,	44,	57,
/*08*/	80,	53,	24,	51,	20,	47,	30,	IGN,
/*10*/	75,	28,	23,	50,	19,	46,	17,	221,
/*18*/	69,	27,	22,	49,	18,	45,	16,	58,
/*20*/	81,	IGN,	11,	38,	7,	34,	15,	1,
/*28*/	IGN,	39,	10,	37,	6,	33,	3,	41,
/*30*/	72,	26,	9,	36,	5,	32,	2,	40,
/*38*/	12,	26,	8,	35,	4,	31,	83,	IGN,
/*40*/	42,	IGN,	IGN,	IGN,	14,	88,	66,	62,
/*48*/	IGN,	56,	IGN,	IGN,	125,	87,	65,	61,
/*50*/	IGN,	IGN,	29,	IGN,	68,	68,	64,	60,
/*58*/	IGN,	IGN,	IGN,	42,	13,	67,	63,	59,
};

/* FUJITSU INTERTOP CX300 */
static u_int8_t intertop_keytrans[] = {
/*00	space   a2      1       tab     enter   caps    left    zenkaku	*/
/*08	hiraga  a1      2       q       -       a       fnc     esc	*/
/*10	ins     w       3       s       del     ]       down    x	*/
/*18	z       e       4       d       a10     \       right   c	*/
/*20	backsla r       ;       f       a9      @       ^       v	*/
/*28	/       t       5       g       a8      p       -       b	*/
/*30	.       y       6       h       a7      l       0       n	*/
/*38	-       u       7       j       a5      o       bs      m	*/
/*40	-       a3      8       a4      -       i       k       ,	*/
/*48	num     :       9       [       a6      -       up      -	*/
/*50	-       -       -       -       shift_L -       -       shift_R	*/
/*58	ctrl    win     muhenka henkan  alt     -       -       -	*/
/*----------------------------------------------------------------------*/
/*00*/	57,	60,	2,	15,	28,	58,	77,	41,
/*08*/	112,	59,	3,	16,	IGN,	30,	56,	1,
/*10*/	210,	17,	4,	31,	83,	43,	80,	45,
/*18*/	44,	18,	5,	32,	68,	125,	75,	46,
/*20*/	115,	19,	39,	33,	67,	26,	13,	47,
/*28*/	53,	20,	6,	34,	66,	25,	12,	48,
/*30*/	52,	21,	7,	35,	65,	38,	11,	49,
/*38*/	IGN,	22,	8,	36,	63,	24,	14,	50,
/*40*/	IGN,	61,	9,	62,	IGN,	23,	37,	51,
/*48*/	69,	40,	10,	27,	64,	IGN,	72,	IGN,
/*50*/	IGN,	IGN,	IGN,	IGN,	42,	IGN,	IGN,	54,
/*58*/	29,	221,	123,	121,	184,	IGN,	IGN,	IGN,
};

/* DoCoMo sigmarion (Japan) */
static u_int8_t sigmarion_jp_keytrans[] = {
/*00	right	ent	p	.	y	b	z	space	*/
/*08	down	/	o	,	t	v	a	nfer	*/
/*10	left	\	i	m	r	c	w	menu	*/
/*18	|	-	u	-	e	x	q	1	*/
/*20	pgdn	h/z	0	l	:	g	tab	f1	*/
/*28	xfer	;	9	n	5	f	2	k	*/
/*30	up	[	8	j	4	d	6	-	*/
/*38	-	@	7	h	3	]	s	-	*/
/*40	caps	-	-	-	bs	fnc	f8	f3	*/
/*48	-	alt	-	-	^	k/h	f7	f4	*/
/*50	-	-	ctrl	-	f10	pgup	f6	f2	*/
/*58	-	-	-	shift	del	f9	f5	esc	*/
/*----------------------------------------------------------------------*/
/*00*/	 77,	 28,	 25,	 52,	 21,	 48,	 44,	 57,
/*08*/	 80,	 53,	 24,	 51,	 20,	 47,	 30,	123,
/*10*/	 75,	115,	 23,	 50,	 19,	 46,	 17,	221,
/*18*/	125,	IGN,	 22,	IGN,	 18,	 45,	 16,	  2,
/*20*/	 81,	 41,	 11,	 38,	 40,	 34,	 15,	IGN,
/*28*/	121,	 39,	 10,	 49,	  6,	 33,	  3,	 37,
/*30*/	 72,	 27,	  9,	 36,	  5,	 32,	  7,	IGN,
/*38*/	 12,	 26,	  8,	 35,	  4,	 43,	 31,	IGN,
/*40*/	 58,	IGN,	IGN,	IGN,	 14,	184,	 66,	IGN,
/*48*/	IGN,	 56,	IGN,	IGN,	 13,	112,	 65,	IGN,
/*50*/	IGN,	IGN,	 29,	IGN,	 68,	 73,	 64,	IGN,
/*58*/	IGN,	IGN,	IGN,	 42,	 14,	 67,	IGN,	  1,
};

static const keysym_t sigmarion_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(184), KS_Cmd,		KS_Alt_R,	KS_Multi_key,
	KC(64),  KS_Cmd_Screen1,	KS_f6,		KS_f1,
	KC(65),  KS_Cmd_Screen2,	KS_f7,		KS_f2,
	KC(66),  KS_Cmd_Screen3,	KS_f8,		KS_f3,
	KC(67),  KS_Cmd_Screen4,	KS_f9,		KS_f4,
	KC(68),  KS_Cmd_Screen5,	KS_f10,		KS_f5,
	KC(27),  KS_Cmd_BrightnessUp,	KS_bracketleft,	KS_braceleft,
	KC(43),  KS_Cmd_BrightnessDown,	KS_bracketright,KS_braceright,
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

const struct hpckbd_keymap_table {
	platid_t	*ht_platform;
	const u_int8_t	*ht_keymap;
	const int	*ht_special;
	struct {
		const keysym_t	*map;
		int size;
	} ht_cmdmap;
	kbd_t		ht_layout;
} hpckbd_keymap_table[] = {
	{	&platid_mask_MACH_COMPAQ_C,
		tc5165_compaq_c_jp_keymap, 
		tc5165_compaq_c_jp_special_keymap,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_VICTOR_INTERLINK,
		m38813c_keymap, 
		default_special_keymap,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_SHARP_TELIOS,
		tc5165_telios_jp_keymap, 
		default_special_keymap,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_SHARP_MOBILON,
		tc5165_mobilon_keymap, 
		tc5165_mobilon_special_keymap,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_500A,
		mobilepro750c_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_520A,
		mobilepro_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_530A,
		mobilepro_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_700A,
		mobilepro_keytrans, 
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_730A,
		mobilepro_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_MPRO700,
		mobilepro_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_SIGMARION,
		sigmarion_jp_keytrans,
		NULL,
		CMDMAP(sigmarion_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_NEC_MCR,
		mcr_jp_keytrans,
		NULL,
		CMDMAP(mcr_jp_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_IBM_WORKPAD_Z50,
		z50_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_SHARP_TRIPAD,
		tripad_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_VADEM_CLIO_C,
		tripad_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCCS,
		mccs_keytrans,
		NULL,
		CMDMAP(mccs_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_FUJITSU_INTERTOP,
		intertop_keytrans,
		NULL,
		NULLCMDMAP,
		KB_JP },
	{ NULL } /* end mark */
};
