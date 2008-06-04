/*	$NetBSD: top.c,v 1.26.10.1 2008/06/04 02:05:59 yamt Exp $	*/

const char copyright[] = "Copyright (c) 1984 through 1996, William LeFebvre";

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  See the file "Changes" for information on version-to-version changes.
 */

/*
 *  This file contains "main" and other high-level routines.
 */

/*
 * The following preprocessor variables, when defined, are used to
 * distinguish between different Unix implementations:
 *
 *	SIGHOLD  - use SVR4 sighold function when defined
 *	SIGRELSE - use SVR4 sigrelse function when defined
 */

#include <sys/cdefs.h>

#ifndef lint
__RCSID("$NetBSD: top.c,v 1.26.10.1 2008/06/04 02:05:59 yamt Exp $");
#endif

#include "os.h"
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/time.h>
#include <poll.h>

/* includes specific to top */
#include "display.h"		/* interface to display package */
#include "screen.h"		/* interface to screen package */
#include "top.h"
#include "top.local.h"
#include "boolean.h"
#include "commands.h"
#include "machine.h"
#include "username.h"
#include "utils.h"

/* Size of the stdio buffer given to stdout */
#define Buffersize	2048

/* The buffer that stdio will use */
char stdoutbuf[Buffersize];

/* build Signal masks */
#define Smask(s)	(1 << ((s) - 1))

/* for getopt: */
extern int  optind;
extern char *optarg;

/* imported from screen.c */
extern int overstrike;

/* values which need to be accessed by signal handlers */
static int max_topn;		/* maximum displayable processes */

/* miscellaneous things */
char *myname = "top";
jmp_buf jmp_int;

/* pointers to display routines */
void (*d_loadave) __P((int, double *)) = i_loadave;
void (*d_procstates) __P((int, int *, int)) = i_procstates;
void (*d_cpustates) __P((int *)) = i_cpustates;
void (*d_memory) __P((int *)) = i_memory;
void (*d_swap) __P((int *)) = i_swap;
void (*d_message) __P((void)) = i_message;
void (*d_header) __P((char *)) = i_header;
void (*d_process) __P((int, char *)) = i_process;

int main __P((int, char *[]));

int
main(argc, argv)

int  argc;
char *argv[];

{
    register int i;
    register int active_procs;
    register int change;

    struct system_info system_info;
    struct statics statics;
    caddr_t processes;

    static char tempbuf1[50];
    static char tempbuf2[50];
    int old_sigmask;		/* only used for BSD-style signals */
    int topn = Default_TOPN;
    double delay = Default_DELAY, t;
    int displays = 0;		/* indicates unspecified */
    time_t curr_time;
    char *(*get_userid) __P((int)) = username;
    char *uname_field = "USERNAME";
    char *header_text;
    char *env_top;
    char **preset_argv;
    int  preset_argc = 0;
    char **av = NULL;
    int  ac = 0;
    char dostates = No;
    char do_unames = Yes;
    char interactive = Maybe;
    char warnings = 0;
#if Default_TOPN == Infinity
    char topn_specified = No;
#endif
    char ch;
    char *iptr;
    char no_command = 1;
    struct process_select ps;
#ifdef ORDER
    char *order_name = NULL;
    int order_index = 0;
#endif
    struct pollfd set[1];

#ifdef ORDER
    static char command_chars[] = "\f qh?en#sdkrSiIuotp";
#else
    static char command_chars[] = "\f qh?en#sdkrSiIutp";
#endif
/* these defines enumerate the "strchr"s of the commands in command_chars */
#define CMD_redraw	0
#define CMD_update	1
#define CMD_quit	2
#define CMD_help1	3
#define CMD_help2	4
#define CMD_OSLIMIT	4    /* terminals with OS can only handle commands */
#define CMD_errors	5    /* less than or equal to CMD_OSLIMIT	   */
#define CMD_number1	6
#define CMD_number2	7
#define CMD_delay	8
#define CMD_displays	9
#define CMD_kill	10
#define CMD_renice	11
#define CMD_system	12
#define CMD_idletog     13
#define CMD_idletog2    14
#define CMD_user	15
#ifdef ORDER
#define CMD_order       16
#endif
#define	CMD_threads	17
#define	CMD_pid		18

    /* set the buffer for stdout */
#ifdef DEBUG
    extern FILE *debug;
    debug = fopen("debug.run", "w");
    setbuffer(stdout, NULL, 0);
#else
    setbuffer(stdout, stdoutbuf, Buffersize);
#endif

    /* get our name */
    if (argc > 0)
    {
	if ((myname = strrchr(argv[0], '/')) == 0)
	{
	    myname = argv[0];
	}
	else
	{
	    myname++;
	}
    }

    /* initialize some selection options */
    ps.idle    = Yes;
    ps.system  = Yes;
    ps.uid     = -1;
    ps.command = NULL;
    ps.threads = No;
    ps.pid     = -1;

    /* get preset options from the environment */
    if ((env_top = getenv("TOP")) != NULL)
    {
	av = preset_argv = argparse(env_top, &preset_argc);
	ac = preset_argc;

	/* set the dummy argument to an explanatory message, in case
	   getopt encounters a bad argument */
	preset_argv[0] = "while processing environment";
    }

    /* process options */
    do {
	/* if we're done doing the presets, then process the real arguments */
	if (preset_argc == 0)
	{
	    ac = argc;
	    av = argv;

	    /* this should keep getopt happy... */
	    optind = 1;
	}

	while ((i = getopt(ac, av, "SIbinquvs:td:U:o:")) != -1)
	{
	    switch(i)
	    {
	      case 'v':			/* show version number */
		fprintf(stderr, "%s: version %s\n",
			myname, version_string());
		exit(1);
		break;

	      case 'u':			/* toggle uid/username display */
		do_unames = !do_unames;
		break;

	      case 'U':			/* display only username's processes */
		if ((ps.uid = userid(optarg)) == -1)
		{
		    fprintf(stderr, "%s: unknown user\n", optarg);
		    exit(1);
		}
		break;

	      case 'S':			/* show system processes */
		ps.system = !ps.system;
		break;

	      case 'I':                   /* show idle processes */
		ps.idle = !ps.idle;
		break;

	      case 'i':			/* go interactive regardless */
		interactive = Yes;
		break;

	      case 'n':			/* batch, or non-interactive */
	      case 'b':
		interactive = No;
		break;

	      case 'd':			/* number of displays to show */
		if ((i = atoiwi(optarg)) == Invalid || i == 0)
		{
		    fprintf(stderr,
			"%s: warning: display count should be positive -- option ignored\n",
			myname);
		    warnings++;
		}
		else
		{
		    displays = i;
		}
		break;

	      case 's':
		delay = atof(optarg);
		if (delay < 0.1 || (delay < 0.5 && getuid() != 0))
		{
		    fprintf(stderr,
			"%s: warning: seconds delay should be positive -- using default\n",
			myname);
		    delay = Default_DELAY;
		    warnings++;
		}
		break;
              
              case 't':
                ps.threads = !ps.threads;
                break;

	      case 'q':		/* be quick about it */
		/* only allow this if user is really root */
		if (getuid() == 0)
		{
		    /* be very un-nice! */
		    (void) nice(-20);
		}
		else
		{
		    fprintf(stderr,
			"%s: warning: `-q' option can only be used by root\n",
			myname);
		    warnings++;
		}
		break;

	      case 'o':		/* select sort order */
#ifdef ORDER
		order_name = optarg;
#else
		fprintf(stderr,
			"%s: this platform does not support arbitrary ordering.  Sorry.\n",
			myname);
		warnings++;
#endif
		break;

	      default:
		fprintf(stderr, "\
Top version %s\n\
Usage: %s [-bIinqSuv] [-d count] [-o field] [-s time] [-U username] [number]\n",
			version_string(), myname);
		exit(1);
	    }
	}

	/* get count of top processes to display (if any) */
	if (optind < ac)
	{
	    if ((topn = atoiwi(av[optind])) == Invalid)
	    {
		fprintf(stderr,
			"%s: warning: process display count should be non-negative -- using default\n",
			myname);
		warnings++;
	    }
#if Default_TOPN == Infinity
            else
	    {
		topn_specified = Yes;
	    }
#endif
	}

	/* tricky:  remember old value of preset_argc & set preset_argc = 0 */
	i = preset_argc;
	preset_argc = 0;

    /* repeat only if we really did the preset arguments */
    } while (i != 0);

    /* set constants for username/uid display correctly */
    if (!do_unames)
    {
	uname_field = "   UID  ";
	get_userid = itoa7;
    }

    /* initialize the kernel memory interface */
    if (machine_init(&statics) == -1)
    {
	exit(1);
    }

#ifdef ORDER
    /* determine sorting order index, if necessary */
    if (order_name != NULL)
    {
	if ((order_index = string_index(order_name, statics.order_names)) == -1)
	{
	    char **pp;

	    fprintf(stderr, "%s: '%s' is not a recognized sorting order.\n",
		    myname, order_name);
	    fprintf(stderr, "\tTry one of these:");
	    pp = statics.order_names;
	    while (*pp != NULL)
	    {
		fprintf(stderr, " %s", *pp++);
	    }
	    fputc('\n', stderr);
	    exit(1);
	}
    }
#endif

#ifdef no_initialization_needed
    /* initialize the hashing stuff */
    if (do_unames)
    {
	init_hash();
    }
#endif

    /* initialize termcap */
    init_termcap(interactive);

    /* get the string to use for the process area header */
    header_text = format_header(uname_field, &ps);

    /* initialize display interface */
    if ((max_topn = display_init(&statics)) == -1)
    {
	fprintf(stderr, "%s: can't allocate sufficient memory\n", myname);
	exit(4);
    }
    
    /* print warning if user requested more processes than we can display */
    if (topn > max_topn)
    {
	fprintf(stderr,
		"%s: warning: this terminal can only display %d processes.\n",
		myname, max_topn);
	warnings++;
    }

    /* adjust for topn == Infinity */
    if (topn == Infinity)
    {
	/*
	 *  For smart terminals, infinity really means everything that can
	 *  be displayed, or Largest.
	 *  On dumb terminals, infinity means every process in the system!
	 *  We only really want to do that if it was explicitly specified.
	 *  This is always the case when "Default_TOPN != Infinity".  But if
	 *  topn wasn't explicitly specified and we are on a dumb terminal
	 *  and the default is Infinity, then (and only then) we use
	 *  "Nominal_TOPN" instead.
	 */
#if Default_TOPN == Infinity
	topn = smart_terminal ? Largest :
		    (topn_specified ? Largest : Nominal_TOPN);
#else
	topn = Largest;
#endif
    }

    /* set header display accordingly */
    display_header(topn > 0);

    /* determine interactive state */
    if (interactive == Maybe)
    {
	interactive = smart_terminal;
    }

    /* if # of displays not specified, fill it in */
    if (displays == 0)
    {
	displays = smart_terminal ? Infinity : 1;
    }

    /* hold interrupt signals while setting up the screen and the handlers */
#ifdef SIGHOLD
    sighold(SIGINT);
    sighold(SIGQUIT);
    sighold(SIGTSTP);
#else
    old_sigmask = sigblock(Smask(SIGINT) | Smask(SIGQUIT) | Smask(SIGTSTP));
#endif
    init_screen();
    (void) signal(SIGINT, leave);
    (void) signal(SIGQUIT, leave);
    (void) signal(SIGTSTP, tstop);
#ifdef SIGWINCH
    (void) signal(SIGWINCH, winch);
#endif
#ifdef SIGRELSE
    sigrelse(SIGINT);
    sigrelse(SIGQUIT);
    sigrelse(SIGTSTP);
#else
    (void) sigsetmask(old_sigmask);
#endif
    if (warnings)
    {
	fputs("....", stderr);
	fflush(stderr);			/* why must I do this? */
	sleep((unsigned)(3 * warnings));
	fputc('\n', stderr);
    }

    /* setup the jump buffer for stops */
    if (setjmp(jmp_int) != 0)
    {
	/* control ends up here after an interrupt */
	reset_display();
    }

    /*
     *  main loop -- repeat while display count is positive or while it
     *		indicates infinity (by being -1)
     */

    while ((displays == -1) || (displays-- > 0))
    {
	/* get the current stats */
	get_system_info(&system_info);

	/* get the current set of processes */
	processes =
		get_process_info(&system_info,
				 &ps,
#ifdef ORDER
				 proc_compares[order_index]);
#else
				 proc_compare);
#endif

	/* display the load averages */
	(*d_loadave)(system_info.last_pid,
		     system_info.load_avg);

	/* display the current time */
	/* this method of getting the time SHOULD be fairly portable */
	time(&curr_time);
	i_timeofday(&curr_time, &system_info.uptime);

	/* display process state breakdown */
	(*d_procstates)(system_info.p_total,
			system_info.procstates,
			ps.threads == Yes);

	/* display the CPU state percentage breakdown */
	if (dostates)	/* but not the first time */
	{
	    (*d_cpustates)(system_info.cpustates);
	}
	else
	{
	    /* we'll do it next time */
	    z_cpustates();
	    dostates = Yes;
	}

	/* display memory stats */
	(*d_memory)(system_info.memory);

	/* display swap stats */
	(*d_swap)(system_info.swap); 

	/* handle message area */
	(*d_message)();

	/* update the header area */
	(*d_header)(header_text);
    
	if (topn > 0)
	{
	    /* determine number of processes to actually display */
	    /* this number will be the smallest of:  active processes,
	       number user requested, number current screen accommodates */
	    active_procs = system_info.P_ACTIVE;
	    if (active_procs > topn)
	    {
		active_procs = topn;
	    }
	    if (active_procs > max_topn)
	    {
		active_procs = max_topn;
	    }

	    /* now show the top "n" processes. */
	    for (i = 0; i < active_procs; i++)
	    {
		(*d_process)(i, format_next_process(processes, get_userid));
	    }
	}
	else
	{
	    i = 0;
	}

	/* do end-screen processing */
	u_endscreen(i);

	/* now, flush the output buffer */
	if (fflush(stdout) != 0)
	{
	    new_message(MT_standout, " Write error on stdout");
	    putchar('\r');
	    quit(1);
	    /*NOTREACHED*/
	}

	/* only do the rest if we have more displays to show */
	if (displays)
	{
	    /* switch out for new display on smart terminals */
	    if (smart_terminal)
	    {
		if (overstrike)
		{
		    reset_display();
		}
		else
		{
		    d_loadave = u_loadave;
		    d_procstates = u_procstates;
		    d_cpustates = u_cpustates;
		    d_memory = u_memory;
		    d_swap = u_swap;
		    d_message = u_message;
		    d_header = u_header;
		    d_process = u_process;
		}
	    }
    
	    no_command = Yes;
	    if (!interactive)
	    {
		/* set up alarm */
		(void) signal(SIGALRM, onalrm);
		(void) alarm((unsigned)delay);
    
		/* wait for the rest of it .... */
		pause();
	    }
	    else while (no_command)
	    {
		/* assume valid command unless told otherwise */
		no_command = No;

		/* set up arguments for poll with timeout */
		set[0].fd = 0;
		set[0].events = POLLIN;

		/* wait for either input or the end of the delay period */
		if (poll(set, 1, (int)(delay * 1000)) > 0)
		{
		    int newval;
		    char *errmsg;
    
		    /* something to read -- clear the message area first */
		    clear_message();

		    /* now read it and convert to command strchr */
		    /* (use "change" as a temporary to hold strchr) */
		    if (read(0, &ch, 1) != 1)
		    {
			/* read error: either 0 or -1 */
			new_message(MT_standout, " Read error on stdin");
			putchar('\r');
			quit(1);
			/*NOTREACHED*/
		    }
		    if ((iptr = strchr(command_chars, ch)) == NULL)
		    {
			/* illegal command */
			new_message(MT_standout, " Command not understood");
			putchar('\r');
			no_command = Yes;
		    }
		    else
		    {
			change = iptr - command_chars;
			if (overstrike && change > CMD_OSLIMIT)
			{
			    /* error */
			    new_message(MT_standout,
			    " Command cannot be handled by this terminal");
			    putchar('\r');
			    no_command = Yes;
			}
			else switch(change)
			{
			    case CMD_redraw:	/* redraw screen */
				reset_display();
				break;
    
			    case CMD_update:	/* merely update display */
				/* is the load average high? */
				if (system_info.load_avg[0] > LoadMax)
				{
				    /* yes, go home for visual feedback */
				    go_home();
				    fflush(stdout);
				}
				break;
	    
			    case CMD_quit:	/* quit */
				quit(0);
				/*NOTREACHED*/
				break;
	    
			    case CMD_help1:	/* help */
			    case CMD_help2:
				reset_display();
				clear();
				show_help(&statics);
				standout("Hit any key to continue: ");
				fflush(stdout);
				(void) read(0, &ch, 1);
				break;
	
			    case CMD_errors:	/* show errors */
				if (error_count() == 0)
				{
				    new_message(MT_standout,
					" Currently no errors to report.");
				    putchar('\r');
				    no_command = Yes;
				}
				else
				{
				    reset_display();
				    clear();
				    show_errors();
				    standout("Hit any key to continue: ");
				    fflush(stdout);
				    (void) read(0, &ch, 1);
				}
				break;
	
			    case CMD_number1:	/* new number */
			    case CMD_number2:
				new_message(MT_standout,
				    "Number of processes to show: ");
				newval = readline(tempbuf1, 8, Yes);
				if (newval > -1)
				{
				    if (newval > max_topn)
				    {
					new_message(MT_standout | MT_delayed,
					  " This terminal can only display %d processes.",
					  max_topn);
					putchar('\r');
				    }

				    if (newval == 0)
				    {
					/* inhibit the header */
					display_header(No);
				    }
				    else if (newval > topn && topn == 0)
				    {
					/* redraw the header */
					display_header(Yes);
					d_header = i_header;
				    }
				    topn = newval;
				}
				break;
	    
			    case CMD_delay:	/* new seconds delay */
				new_message(MT_standout, "Seconds to delay: ");
				if ((t = readline(tempbuf1, 8, Yes)) > -1)
				{
				    delay = t;
				    if (t < 0.1 || (t < 0.5 && getuid() != 0))
				    {
					delay = 0.5;
				    }
				}
				clear_message();
				break;
	
			    case CMD_displays:	/* change display count */
				new_message(MT_standout,
					"Displays to show (currently %s): ",
					displays == -1 ? "infinite" :
							 itoa(displays));
				if ((i = readline(tempbuf1, 10, Yes)) > 0)
				{
				    displays = i;
				}
				else if (i == 0)
				{
				    quit(0);
				}
				clear_message();
				break;
    
			    case CMD_kill:	/* kill program */
				new_message(0, "kill ");
				if (readline(tempbuf2, sizeof(tempbuf2), No) > 0)
				{
				    if ((errmsg = kill_procs(tempbuf2)) != NULL)
				    {
					new_message(MT_standout, "%s", errmsg);
					putchar('\r');
					no_command = Yes;
				    }
				}
				else
				{
				    clear_message();
				}
				break;
	    
			    case CMD_renice:	/* renice program */
				new_message(0, "renice ");
				if (readline(tempbuf2, sizeof(tempbuf2), No) > 0)
				{
				    if ((errmsg = renice_procs(tempbuf2)) != NULL)
				    {
					new_message(MT_standout, "%s", errmsg);
					putchar('\r');
					no_command = Yes;
				    }
				}
				else
				{
				    clear_message();
				}
				break;

			    case CMD_pid:	/* one process */
				new_message(0, "select pid ");
				if (readline(tempbuf2, sizeof(tempbuf2), No) > 0)
				{
				    ps.pid = atoi(tempbuf2);
				}
				else
				{
				    ps.pid = -1;
				    clear_message();
				}
				break;

			    case CMD_system:
				ps.system = !ps.system;
				new_message(MT_standout | MT_delayed,
				    " %sisplaying system processes.",
				    ps.system ? "D" : "Not d");
				break;

			    case CMD_threads:
				ps.threads = !ps.threads;
				new_message(MT_standout | MT_delayed,
				    " Displaying %s.",
				    ps.threads ? "threads" : "processes");
                                header_text = format_header(uname_field, &ps);
                                reset_display();
				break;

			    case CMD_idletog:
			    case CMD_idletog2:
				ps.idle = !ps.idle;
				new_message(MT_standout | MT_delayed,
				    " %sisplaying idle processes.",
				    ps.idle ? "D" : "Not d");
				putchar('\r');
				break;

			    case CMD_user:
				new_message(MT_standout,
				    "Username to show: ");
				if (readline(tempbuf2, sizeof(tempbuf2), No) > 0)
				{
				    if (tempbuf2[0] == '+' &&
					tempbuf2[1] == '\0')
				    {
					ps.uid = -1;
				    }
				    else if ((i = userid(tempbuf2)) == -1)
				    {
					new_message(MT_standout,
					    " %s: unknown user", tempbuf2);
					no_command = Yes;
				    }
				    else
				    {
					ps.uid = i;
				    }
				    putchar('\r');
				}
				else
				{
				    clear_message();
				}
				break;
	    
#ifdef ORDER
			    case CMD_order:
				new_message(MT_standout,
				    "Order to sort: ");
				if (readline(tempbuf2, sizeof(tempbuf2), No) > 0)
				{
				  if ((i = string_index(tempbuf2, statics.order_names)) == -1)
					{
					  new_message(MT_standout,
					      " %s: unrecognized sorting order", tempbuf2);
					  no_command = Yes;
				    }
				    else
				    {
					order_index = i;
				    }
				    putchar('\r');
				}
				else
				{
				    clear_message();
				}
				break;
#endif
	    
			    default:
				new_message(MT_standout, " BAD CASE IN SWITCH!");
				putchar('\r');
			}
		    }

		    /* flush out stuff that may have been written */
		    fflush(stdout);
		}
	    }
	}
    }

#ifdef DEBUG
    fclose(debug);
#endif
    quit(0);
    /*NOTREACHED*/
    return 0;
}

/*
 *  reset_display() - reset all the display routine pointers so that entire
 *	screen will get redrawn.
 */

void
reset_display()

{
    d_loadave    = i_loadave;
    d_procstates = i_procstates;
    d_cpustates  = i_cpustates;
    d_memory     = i_memory;
    d_swap       = i_swap;
    d_message	 = i_message;
    d_header	 = i_header;
    d_process	 = i_process;
}

/*
 *  signal handlers
 */

sigret_t leave(n)	/* exit under normal conditions -- INT handler */

int n;

{
    end_screen();
    exit(0);
}

sigret_t tstop(i)	/* SIGTSTP handler */

int i;

{
    /* move to the lower left */
    end_screen();
    fflush(stdout);

    /* default the signal handler action */
    (void) signal(SIGTSTP, SIG_DFL);

    /* unblock the signal and send ourselves one */
#ifdef SIGRELSE
    sigrelse(SIGTSTP);
#else
    (void) sigsetmask(sigblock(0) & ~(1 << (SIGTSTP - 1)));
#endif
    (void) kill(0, SIGTSTP);

    /* reset the signal handler */
    (void) signal(SIGTSTP, tstop);

    /* reinit screen */
    reinit_screen();

    /* jump to appropriate place */
    longjmp(jmp_int, 1);

    /*NOTREACHED*/
}

#ifdef SIGWINCH
sigret_t winch(i)		/* SIGWINCH handler */

int i;

{
    /* reascertain the screen dimensions */
    get_screensize();

    /* tell display to resize */
    max_topn = display_resize();

    /* reset the signal handler */
    (void) signal(SIGWINCH, winch);

    /* jump to appropriate place */
    longjmp(jmp_int, 1);
}
#endif

void quit(status)		/* exit under duress */

int status;

{
    end_screen();
    exit(status);
    /*NOTREACHED*/
}

sigret_t onalrm(n)	/* SIGALRM handler */

int n;

{
    /* this is only used in batch mode to break out of the pause() */
    /* return; */
}

