/* $NetBSD: wsemul_vt100var.h,v 1.4.10.1 2000/11/20 11:43:37 bouyer Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define	VT100_EMUL_NARGS	10	/* max # of args to a command */

struct wsemul_vt100_emuldata {
	const struct wsdisplay_emulops *emulops;
	void *emulcookie;
	int scrcapabilities;
	u_int nrows, ncols, crow, ccol;
	long defattr;			/* default attribute */

	long kernattr;			/* attribute for kernel output */
	void *cbcookie;
#ifdef DIAGNOSTIC
	int console;
#endif

	u_int state;			/* processing state */
	int flags;
#define VTFL_LASTCHAR	0x001	/* printed last char on line (below cursor) */
#define VTFL_INSERTMODE	0x002
#define VTFL_APPLKEYPAD	0x004
#define VTFL_APPLCURSOR	0x008
#define VTFL_DECOM	0x010	/* origin mode */
#define VTFL_DECAWM	0x020	/* auto wrap */
#define VTFL_CURSORON	0x040
#define VTFL_NATCHARSET	0x080	/* national replacement charset mode */
	long curattr, bkgdattr;		/* currently used attribute */
	int attrflags, fgcol, bgcol;	/* properties of curattr */
	u_int scrreg_startrow;
	u_int scrreg_nrows;
	char *tabs;
	char *dblwid;
	int dw;

	int chartab0, chartab1;
	u_int *chartab_G[4];
	u_int *isolatin1tab, *decgraphtab, *dectechtab;
	u_int *nrctab;
	int sschartab; /* single shift */

	int nargs;
	u_int args[VT100_EMUL_NARGS]; /* numeric command args (CSI/DCS) */

	char modif1;	/* {>?} in VT100_EMUL_STATE_CSI */
	char modif2;	/* {!"$&} in VT100_EMUL_STATE_CSI */

	int designating;	/* substate in VT100_EMUL_STATE_SCS* */

	int dcstype;		/* substate in VT100_EMUL_STATE_STRING */
	char *dcsarg;
	int dcspos;
#define DCS_MAXLEN 256 /* ??? */
#define DCSTYPE_TABRESTORE 1 /* DCS2$t */

	u_int savedcursor_row, savedcursor_col;
	long savedattr, savedbkgdattr;
	int savedattrflags, savedfgcol, savedbgcol;
	int savedchartab0, savedchartab1;
	u_int *savedchartab_G[4];
};

/* some useful utility macros */
#define	ARG(n)			(edp->args[(n)])
#define	DEF1_ARG(n)		(ARG(n) ? ARG(n) : 1)
#define	DEFx_ARG(n, x)		(ARG(n) ? ARG(n) : (x))
/* the following two can be negative if we are outside the scrolling region */
#define ROWS_ABOVE	((int)edp->crow - (int)edp->scrreg_startrow)
#define ROWS_BELOW	((int)(edp->scrreg_startrow + edp->scrreg_nrows) \
					- (int)edp->crow - 1)
#define CHECK_DW do { \
	if (edp->dblwid && edp->dblwid[edp->crow]) { \
		edp->dw = 1; \
		if (edp->ccol > (edp->ncols >> 1) - 1) \
			edp->ccol = (edp->ncols >> 1) - 1; \
	} else \
		edp->dw = 0; \
} while (0)
#define NCOLS		(edp->ncols >> edp->dw)
#define	COLS_LEFT	(NCOLS - edp->ccol - 1)
#define COPYCOLS(f, t, n) (*edp->emulops->copycols)(edp->emulcookie, \
	edp->crow, (f) << edp->dw, (t) << edp->dw, (n) << edp->dw)
#define ERASECOLS(f, n, a) (*edp->emulops->erasecols)(edp->emulcookie, \
	edp->crow, (f) << edp->dw, (n) << edp->dw, a)

/*
 * response to primary DA request
 * operating level: 61 = VT100, 62 = VT200, 63 = VT300
 * extensions: 1 = 132 cols, 2 = printer port, 6 = selective erase,
 *	7 = soft charset, 8 = UDKs, 9 = NRC sets
 * VT100 = "033[?1;2c"
 */
#define WSEMUL_VT_ID1 "\033[?62;6c"
/*
 * response to secondary DA request
 * ident code: 24 = VT320
 * firmware version
 * hardware options: 0 = no options
 */
#define WSEMUL_VT_ID2 "\033[>24;20;0c"

void wsemul_vt100_reset __P((struct wsemul_vt100_emuldata *));
void wsemul_vt100_scrollup __P((struct wsemul_vt100_emuldata *, int));
void wsemul_vt100_scrolldown __P((struct wsemul_vt100_emuldata *, int));
void wsemul_vt100_ed __P((struct wsemul_vt100_emuldata *, int));
void wsemul_vt100_el __P((struct wsemul_vt100_emuldata *, int));
void wsemul_vt100_handle_csi __P((struct wsemul_vt100_emuldata *, u_char));
void wsemul_vt100_handle_dcs __P((struct wsemul_vt100_emuldata *));

int wsemul_vt100_translate __P((void *cookie, keysym_t, char **));

void vt100_initchartables __P((struct wsemul_vt100_emuldata *));
void vt100_setnrc __P((struct wsemul_vt100_emuldata *, int));
