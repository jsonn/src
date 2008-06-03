/*	$NetBSD: bill.c,v 1.7.18.1 2008/06/03 20:47:00 skrll Exp $	 */

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)bill.c	5.2 (Berkeley) 5/28/91";
#else
__RCSID("$NetBSD: bill.c,v 1.7.18.1 2008/06/03 20:47:00 skrll Exp $");
#endif
#endif /* not lint */

#include <sys/file.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include "header.h"
#include "extern.h"

/* bill.c		 Larn is copyrighted 1986 by Noah Morgan. */

char *mail[] = {
	"From: the LRS (Larn Revenue Service)\n",
	"~s undeclared income\n",
	"\n   We have heard you survived the caverns of Larn.  Let me be the",
	"\nfirst to congratulate you on your success.  It was quite a feat.",
	"\nIt was also very profitable for you...",
	"\n\n   The Dungeon Master has informed us that you brought",
	"1",
	"\ncounty of Larn is in dire need of funds, we have spared no time",
	"2",
	"\nof this notice, and is due within 5 days.  Failure to pay will",
	"\nmean penalties.  Once again, congratulations, We look forward",
	"\nto your future successful expeditions.\n",
	NULL,
	"From: His Majesty King Wilfred of Larndom\n",
	"~s a noble deed\n",
	"\n   I have heard of your magnificent feat, and I, King Wilfred,",
	"\nforthwith declare today to be a national holiday.  Furthermore,",
	"\nhence three days, ye be invited to the castle to receive the",
	"\nhonour of Knight of the realm.  Upon thy name shall it be written...",
	"\n\nBravery and courage be yours.",
	"\n\nMay you live in happiness forevermore...\n",
	NULL,
	"From: Count Endelford\n",
	"~s You Bastard!\n",
	"\n   I have heard (from sources) of your journey.  Congratulations!",
	"\nYou Bastard!  With several attempts I have yet to endure the",
	" caves,\nand you, a nobody, makes the journey!  From this time",
	" onward, bewarned\nupon our meeting you shall pay the price!\n",
	NULL,
	"From: Mainair, Duke of Larnty\n",
	"~s High Praise\n",
	"\n   With certainty, a hero I declare to be amongst us!  A nod of",
	"\nfavour I send to thee.  Me thinks Count Endelford this day of",
	"\nright breath'eth fire as of dragon of whom ye are slayer.  I",
	"\nyearn to behold his anger and jealously.  Should ye choose to",
	"\nunleash some of thy wealth upon those who be unfortunate, I,",
	"\nDuke Mainair, shall equal thy gift also.\n",
	NULL,
	"From: St. Mary's Children's Home\n",
	"~s these poor children\n",
	"\n   News of your great conquests has spread to all of Larndom.",
	"\nMight I have a moment of a great adventurers's time?  We here at",
	"\nSt. Mary's Children's Home are very poor, and many children are",
	"\nstarving.  Disease is widespread and very often fatal without",
	"\ngood food.  Could you possibly find it in your heart to help us",
	"\nin our plight?  Whatever you could give will help much.",
	"\n(your gift is tax deductible)\n",
	NULL,
	"From: The National Cancer Society of Larn\n",
	"~s hope\n",
	"\nCongratulations on your successful expedition.  We are sure much",
	"\ncourage and determination were needed on your quest.  There are",
	"\nmany though, that could never hope to undertake such a journey",
	"\ndue to an enfeebling disease -- cancer.  We at the National",
	"\nCancer Society of Larn wish to appeal to your philanthropy in",
	"\norder to save many good people -- possibly even yourself a few",
	"\nyears from now.  Much work needs to be done in researching this",
	"\ndreaded disease, and you can help today.  Could you please see it",
	"\nin your heart to give generously?  Your continued good health",
	"\ncan be your everlasting reward.\n",
	NULL
};

/*
 * function to mail the letters to the player if a winner
 */

void
mailbill()
{
	int    i;
	char   fname[32];
	char   buf[128];
	char **cp;
	int    fd;

	wait(0);
	if (fork() == 0) {
		resetscroll();
		cp = mail;
		snprintf(fname, sizeof(fname), "%slarnmail.XXXXXX", _PATH_TMP);
		for (i = 0; i < 6; i++) {
			if ((fd = mkstemp(fname)) == -1)
				exit(0);
			while (*cp != NULL) {
				if (*cp[0] == '1') {
					snprintf(buf, sizeof(buf),
		"\n%ld gold pieces back with you from your journey.  As the",
						(long) c[GOLD]);
					write(fd, buf, strlen(buf));
				} else if (*cp[0] == '2') {
					snprintf(buf, sizeof(buf),
		"\nin preparing your tax bill.  You owe %ld gold pieces as",
					    (long) c[GOLD] * TAXRATE);
					write(fd, buf, strlen(buf));
				} else
					write(fd, *cp, strlen(*cp));
				cp++;
			}
			cp++;

			close(fd);
			snprintf(buf, sizeof(buf),
			    "mail -I %s < %s > /dev/null", loginname, fname);
			system(buf);
			unlink(fname);
		}
	}
	exit(0);
}
