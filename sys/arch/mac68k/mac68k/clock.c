/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 *
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *   from: @(#)clock.c   7.6 (Berkeley) 5/7/91
 *	$Id: clock.c,v 1.5.2.1 1994/07/24 01:23:28 cgd Exp $
 */

#if !defined(STANDALONE)
#include "param.h"
#include "kernel.h"

#include "machine/psl.h"
#include "machine/cpu.h"

#if defined(GPROF) && defined(PROFTIMER)
#include "sys/gprof.h"
#endif

#else /* STANDALONE */
#include "stand.h"
#endif /* STANDALONE */

#include "clockreg.h"
#include "via.h"

static int month_days[12] = {
   31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define DIFF19041970 2082844800

/*
 * Mac II machine-dependent clock routines.
 */


/*
   Start the real-time clock; i.e. set timer latches and boot timer.

   We use VIA1 timer 1.

 */
void startrtclock(void)
{
/* BARF MF startrt clock is called twice in init_main, configure,
   the reason why is doced in configure */

   /* be certain clock interrupts are off */
   via_reg(VIA1, vIER) = V1IF_T1;

   /* set timer latch */
   via_reg(VIA1, vACR) |= ACR_T1LATCH;

   /* set VIA timer 1 latch to 60 Hz (100 Hz) */
   via_reg(VIA1, vT1L) = CLK_INTL;
   via_reg(VIA1, vT1LH) = CLK_INTH;

   /* set VIA timer 1 counter started for 60(100) Hz */
   via_reg(VIA1, vT1C) = CLK_INTL;
   via_reg(VIA1, vT1CH) = CLK_INTH;

}

void
enablertclock(void)
{
   /* clear then enable clock interrupt. */
   via_reg(VIA1, vIFR) |= V1IF_T1;
   via_reg(VIA1, vIER) = 0x80 | (V1IF_ADBRDY | V1IF_T1);
}

void
cpu_initclocks(void)
{
	enablertclock();
}

void
setstatclockrate(int rateinhz)
{
}

void
disablertclock(void)
{
   /* disable clock interrupt */
   via_reg(VIA1, vIER) = V1IF_T1;
}


/*
 * Returns number of usec since last clock tick/interrupt.
 *
 * Check high byte twice to prevent missing a roll-over.
 * (race condition?)
 */
u_long clkread()
{
   register int high, high2, low;

   high = via_reg(VIA1, vT1CH);
   low = via_reg(VIA1, vT1C);

   high2 = via_reg(VIA1, vT1CH);
   if(high != high2)
      high = high2;

   /* return count left in timer / 1.27 */
   /* return((CLK_INTERVAL - (high << 8) - low) / CLK_SPEED); */
   return((CLK_INTERVAL - (high << 8) - low) * 10000 / 12700);
}


#ifdef PROFTIMER
/*
 * Here, we have implemented code that causes VIA2's timer to count
 * the profiling clock.  Following the HP300's lead, this reduces
 * the impact on other tasks, since locore turns off the profiling clock
 * on context switches.  If need be, the profiling clock's resolution can
 * be cranked higher than the real-time clock's resolution, to prevent
 * aliasing and allow higher accuracy.
 */
int  profint   = PRF_INTERVAL;   /* Clock ticks between interrupts */
int  profinthigh;
int  profintlow;
int  profscale = 0;   	/* Scale factor from sys clock to prof clock */
char profon    = 0;   	/* Is profiling clock on? */

/* profon values - do not change, locore.s assumes these values */
#define PRF_NONE   0x00
#define   PRF_USER   0x01
#define   PRF_KERNEL   0x80

void initprofclock(void)
{
   /* profile interval must be even divisor of system clock interval */
   if(profint > CLK_INTERVAL)
      profint = CLK_INTERVAL;
   else if(CLK_INTERVAL % profint != 0)
      /* try to intelligently fix clock interval */
      profint = CLK_INTERVAL / (CLK_INTERVAL / profint);

   profscale = CLK_INTERVAL / profint;

   profinthigh = profint >> 8;
   profintlow = profint & 0xff;
}

void startprofclock(void)
{
   via_reg(VIA2, vT1L) = (profint - 1) & 0xff;
   via_reg(VIA2, vT1LH) = (profint - 1) >> 8;
   via_reg(VIA2, vACR) |= ACR_T1LATCH;
   via_reg(VIA2, vT1C) = (profint - 1) & 0xff;
   via_reg(VIA2, vT1CH) = (profint - 1) >> 8;
}

void stopprofclock(void)
{
   via_reg(VIA2, vT1L) = 0;
   via_reg(VIA2, vT1LH) = 0;
   via_reg(VIA2, vT1C) = 0;
   via_reg(VIA2, vT1CH) = 0;
}

#ifdef GPROF
/*
 * BARF: we should check this:
 *
 * profclock() is expanded in line in lev6intr() unless profiling kernel.
 * Assumes it is called with clock interrupts blocked.
 */
void profclock(clockframe *pclk)
{
   /*
    * Came from user mode.
    * If this process is being profiled record the tick.
    */
   if (USERMODE(pclk->ps)) {
      if (p->p_stats.p_prof.pr_scale)
	 addupc_task(&curproc, pclk->pc, 1);
   }
   /*
    * Came from kernel (supervisor) mode.
    * If we are profiling the kernel, record the tick.
    */
   else if (profiling < 2) {
      register int s = pclk->pc - s_lowpc;

      if (s < s_textsize)
         kcount[s / (HISTFRACTION * sizeof (*kcount))]++;
   }
   /*
    * Kernel profiling was on but has been disabled.
    * Mark as no longer profiling kernel and if all profiling done,
    * disable the clock.
    */
   if (profiling && (profon & PRF_KERNEL)) {
      profon &= ~PRF_KERNEL;
      if (profon == PRF_NONE)
         stopprofclock();
   }
}
#endif
#endif

#if defined(BARF_ON_YOUR_SHOES)
static int got_timezone = 0;
#endif

/*
 * convert a Mac PRAM time value to GMT, using /etc/TIMEZONE.
 */
u_long ugmt_2_pramt(u_long t)
{
   /* don't know how to open a file properly. */
   /* assume compiled timezone is correct. */

   return(t = t + DIFF19041970 - tz.tz_minuteswest );
}

/*
 * convert GMT to Mac PRAM time, using global timezone
 */
u_long pramt_2_ugmt(u_long t)
{
   return(t = t - DIFF19041970 + tz.tz_minuteswest);
}

/*
 * Set global GMT time register, using a file system time base for comparison
 * and sanity checking.
 */
void inittodr(time_t base)
{
   u_long timbuf;
   u_long pramtime;

   pramtime = pram_readtime();
   timbuf = pramt_2_ugmt(pramtime);

   if (base < 5*SECYR) {
      printf("WARNING: file system time earlier than 1975\n");
      printf(" -- CHECK AND RESET THE DATE!\n");
      base = 21 * SECYR;	/* 1991 is our sane date */
   }
   
   if (base > 40*SECYR) {
      printf("WARNING: file system time later than 2010\n");
      printf(" -- CHECK AND RESET THE DATE!\n");
      base = 21 * SECYR;	/* 1991 is our sane date */
   }
   
   if(timbuf < base){
      printf("WARNING: Battery clock has earlier time than UNIX fs.\n");
      if(((u_long)base) < (40 * SECYR))	/* the year 2010.  Let's hope MacBSD */
					/* doesn't run that long! */
         timbuf = base;
   }

   time.tv_sec = timbuf;
   time.tv_usec = 0;	/* clear usec; Mac's PRAM clock stores only seconds */
}

/*
 * Set battery backed clock to a new time, presumably after someone has
 * changed system time.
 */
void resettodr(void)
{
   if(!pram_settime(ugmt_2_pramt(time.tv_sec)))
      printf("WARNING: cannot set battery-backed clock.\n");
}

void
delay(n)
int n;
{
	n *= 1000;
	while (n--);
}
