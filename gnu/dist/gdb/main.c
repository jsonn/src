/* Top level stuff for GDB, the GNU debugger.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include <setjmp.h>
#include "top.h"
#include "target.h"
#include "inferior.h"
#include "call-cmds.h"
#include "gdbcore.h"

#include "getopt.h"

#include <sys/types.h>
#include "gdb_stat.h"
#include <ctype.h>

#include "gdb_string.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef	NO_SYS_FILE
#include <sys/file.h>
#endif

/* Temporary variable for SET_TOP_LEVEL.  */

static int top_level_val;

/* Do a setjmp on error_return and quit_return.  catch_errors is
   generally a cleaner way to do this, but main() would look pretty
   ugly if it had to use catch_errors each time.  */

#define SET_TOP_LEVEL() \
  (((top_level_val = setjmp (error_return)) \
    ? (PTR) 0 : (PTR) memcpy (quit_return, error_return, sizeof (jmp_buf))) \
   , top_level_val)

/* If nonzero, display time usage both at startup and for each command.  */

int display_time;

/* If nonzero, display space usage both at startup and for each command.  */

int display_space;

extern void gdb_init PARAMS ((void));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int count;
  static int quiet = 0;
  static int batch = 0;

  /* Pointers to various arguments from command line.  */
  char *symarg = NULL;
  char *execarg = NULL;
  char *corearg = NULL;
  char *cdarg = NULL;
  char *ttyarg = NULL;

  /* These are static so that we can take their address in an initializer.  */
  static int print_help;
  static int print_version;

  /* Pointers to all arguments of --command option.  */
  char **cmdarg;
  /* Allocated size of cmdarg.  */
  int cmdsize;
  /* Number of elements of cmdarg used.  */
  int ncmd;

  /* Indices of all arguments of --directory option.  */
  char **dirarg;
  /* Allocated size.  */
  int dirsize;
  /* Number of elements used.  */
  int ndir;
  
  struct stat homebuf, cwdbuf;
  char *homedir, *homeinit;

  register int i;

  long time_at_startup = get_run_time ();

  START_PROGRESS (argv[0], 0);

#ifdef MPW
  /* Do all Mac-specific setup. */
  mac_init ();
#endif /* MPW */

  /* This needs to happen before the first use of malloc.  */
  init_malloc ((PTR) NULL);

#if defined (ALIGN_STACK_ON_STARTUP)
  i = (int) &count & 0x3;
  if (i != 0)
    alloca (4 - i);
#endif

  /* If error() is called from initialization code, just exit */
  if (SET_TOP_LEVEL ()) {
    exit(1);
  }

  cmdsize = 1;
  cmdarg = (char **) xmalloc (cmdsize * sizeof (*cmdarg));
  ncmd = 0;
  dirsize = 1;
  dirarg = (char **) xmalloc (dirsize * sizeof (*dirarg));
  ndir = 0;

  quit_flag = 0;
  line = (char *) xmalloc (linesize);
  line[0] = '\0';		/* Terminate saved (now empty) cmd line */
  instream = stdin;

  getcwd (gdb_dirbuf, sizeof (gdb_dirbuf));
  current_directory = gdb_dirbuf;

  /* Parse arguments and options.  */
#ifndef WINGDB
  {
    int c;
    /* When var field is 0, use flag field to record the equivalent
       short option (or arbitrary numbers starting at 10 for those
       with no equivalent).  */
    static struct option long_options[] =
      {
	{"readnow", no_argument, &readnow_symbol_files, 1},
	{"r", no_argument, &readnow_symbol_files, 1},
	{"mapped", no_argument, &mapped_symbol_files, 1},
	{"m", no_argument, &mapped_symbol_files, 1},
	{"quiet", no_argument, &quiet, 1},
	{"q", no_argument, &quiet, 1},
	{"silent", no_argument, &quiet, 1},
	{"nx", no_argument, &inhibit_gdbinit, 1},
	{"n", no_argument, &inhibit_gdbinit, 1},
	{"batch", no_argument, &batch, 1},
	{"epoch", no_argument, &epoch_interface, 1},

	/* This is a synonym for "--annotate=1".  --annotate is now preferred,
	   but keep this here for a long time because people will be running
	   emacses which use --fullname.  */
	{"fullname", no_argument, 0, 'f'},
	{"f", no_argument, 0, 'f'},

	{"annotate", required_argument, 0, 12},
	{"help", no_argument, &print_help, 1},
	{"se", required_argument, 0, 10},
	{"symbols", required_argument, 0, 's'},
	{"s", required_argument, 0, 's'},
	{"exec", required_argument, 0, 'e'},
	{"e", required_argument, 0, 'e'},
	{"core", required_argument, 0, 'c'},
	{"c", required_argument, 0, 'c'},
	{"command", required_argument, 0, 'x'},
	{"version", no_argument, &print_version, 1},
	{"x", required_argument, 0, 'x'},
	{"directory", required_argument, 0, 'd'},
	{"cd", required_argument, 0, 11},
	{"tty", required_argument, 0, 't'},
	{"baud", required_argument, 0, 'b'},
	{"b", required_argument, 0, 'b'},
	{"nw", no_argument, &use_windows, 0},
	{"nowindows", no_argument, &use_windows, 0},
	{"w", no_argument, &use_windows, 1},
	{"windows", no_argument, &use_windows, 1},
	{"statistics", no_argument, 0, 13},
	{"write", no_argument, &write_files, 1},
/* Allow machine descriptions to add more options... */
#ifdef ADDITIONAL_OPTIONS
	ADDITIONAL_OPTIONS
#endif
	{0, no_argument, 0, 0}
      };

    while (1)
      {
	int option_index;

	c = getopt_long_only (argc, argv, "",
			      long_options, &option_index);
	if (c == EOF)
	  break;

	/* Long option that takes an argument.  */
	if (c == 0 && long_options[option_index].flag == 0)
	  c = long_options[option_index].val;

	switch (c)
	  {
	  case 0:
	    /* Long option that just sets a flag.  */
	    break;
	  case 10:
	    symarg = optarg;
	    execarg = optarg;
	    break;
	  case 11:
	    cdarg = optarg;
	    break;
	  case 12:
	    /* FIXME: what if the syntax is wrong (e.g. not digits)?  */
	    annotation_level = atoi (optarg);
	    break;
	  case 13:
	    /* Enable the display of both time and space usage.  */
	    display_time = 1;
	    display_space = 1;
	    break;
	  case 'f':
	    annotation_level = 1;
/* We have probably been invoked from emacs.  Disable window interface.  */
	    use_windows = 0;
	    break;
	  case 's':
	    symarg = optarg;
	    break;
	  case 'e':
	    execarg = optarg;
	    break;
	  case 'c':
	    corearg = optarg;
	    break;
	  case 'x':
	    cmdarg[ncmd++] = optarg;
	    if (ncmd >= cmdsize)
	      {
		cmdsize *= 2;
		cmdarg = (char **) xrealloc ((char *)cmdarg,
					     cmdsize * sizeof (*cmdarg));
	      }
	    break;
	  case 'd':
	    dirarg[ndir++] = optarg;
	    if (ndir >= dirsize)
	      {
		dirsize *= 2;
		dirarg = (char **) xrealloc ((char *)dirarg,
					     dirsize * sizeof (*dirarg));
	      }
	    break;
	  case 't':
	    ttyarg = optarg;
	    break;
	  case 'q':
	    quiet = 1;
	    break;
	  case 'b':
	    {
	      int i;
	      char *p;

	      i = strtol (optarg, &p, 0);
	      if (i == 0 && p == optarg)

		/* Don't use *_filtered or warning() (which relies on
                   current_target) until after initialize_all_files(). */

		fprintf_unfiltered
		  (gdb_stderr,
		   "warning: could not set baud rate to `%s'.\n", optarg);
	      else
		baud_rate = i;
	    }
	    break;

#ifdef ADDITIONAL_OPTION_CASES
	  ADDITIONAL_OPTION_CASES
#endif
	  case '?':
	    fprintf_unfiltered (gdb_stderr,
		     "Use `%s --help' for a complete list of options.\n",
		     argv[0]);
	    exit (1);
	  }
      }

    /* If --help or --version, disable window interface.  */
    if (print_help || print_version)
      use_windows = 0;

    /* OK, that's all the options.  The other arguments are filenames.  */
    count = 0;
    for (; optind < argc; optind++)
      switch (++count)
	{
	case 1:
	  symarg = argv[optind];
	  execarg = argv[optind];
	  break;
	case 2:
	  corearg = argv[optind];
	  break;
	case 3:
	  fprintf_unfiltered (gdb_stderr,
		   "Excess command line arguments ignored. (%s%s)\n",
		   argv[optind], (optind == argc - 1) ? "" : " ...");
	  break;
	}
    if (batch)
      quiet = 1;
  }

#endif
  gdb_init ();

  /* Do these (and anything which might call wrap_here or *_filtered)
     after initialize_all_files.  */
  if (print_version)
    {
      print_gdb_version (gdb_stdout);
      wrap_here ("");
      printf_filtered ("\n");
      exit (0);
    }

  if (print_help)
    {
      /* --version is intentionally not documented here, because we
	 are printing the version here, and the help is long enough
	 already.  */

      print_gdb_version (gdb_stdout);
      /* Make sure the output gets printed.  */
      wrap_here ("");
      printf_filtered ("\n");

      /* But don't use *_filtered here.  We don't want to prompt for continue
	 no matter how small the screen or how much we're going to print.  */
      fputs_unfiltered ("\
This is the GNU debugger.  Usage:\n\
    gdb [options] [executable-file [core-file or process-id]]\n\
Options:\n\
  --help             Print this message.\n\
  --quiet            Do not print version number on startup.\n\
  --fullname         Output information used by emacs-GDB interface.\n\
  --epoch            Output information used by epoch emacs-GDB interface.\n\
", gdb_stdout);
      fputs_unfiltered ("\
  --batch            Exit after processing options.\n\
  --nx               Do not read .gdbinit file.\n\
  --tty=TTY          Use TTY for input/output by the program being debugged.\n\
  --cd=DIR           Change current directory to DIR.\n\
  --directory=DIR    Search for source files in DIR.\n\
", gdb_stdout);
      fputs_unfiltered ("\
  --command=FILE     Execute GDB commands from FILE.\n\
  --symbols=SYMFILE  Read symbols from SYMFILE.\n\
  --exec=EXECFILE    Use EXECFILE as the executable.\n\
  --se=FILE          Use FILE as symbol file and executable file.\n\
", gdb_stdout);
      fputs_unfiltered ("\
  --core=COREFILE    Analyze the core dump COREFILE.\n\
  -b BAUDRATE        Set serial port baud rate used for remote debugging.\n\
  --mapped           Use mapped symbol files if supported on this system.\n\
  --readnow          Fully read symbol files on first access.\n\
  --nw		     Do not use a window interface.\n\
  --write            Set writing into executable and core files.\n\
", gdb_stdout);
#ifdef ADDITIONAL_OPTION_HELP
      fputs_unfiltered (ADDITIONAL_OPTION_HELP, gdb_stdout);
#endif
      fputs_unfiltered ("\n\
For more information, type \"help\" from within GDB, or consult the\n\
GDB manual (available as on-line info or a printed manual).\n", gdb_stdout);
      exit (0);
    }

  if (!quiet)
    {
      /* Print all the junk at the top, with trailing "..." if we are about
	 to read a symbol file (possibly slowly).  */
      print_gnu_advertisement ();
      print_gdb_version (gdb_stdout);
      if (symarg)
	printf_filtered ("..");
      wrap_here("");
      gdb_flush (gdb_stdout);		/* Force to screen during slow operations */
    }

  error_pre_print = "\n\n";
  quit_pre_print = error_pre_print;

  /* We may get more than one warning, don't double space all of them... */
  warning_pre_print = "\nwarning: ";

  /* Read and execute $HOME/.gdbinit file, if it exists.  This is done
     *before* all the command line arguments are processed; it sets
     global parameters, which are independent of what file you are
     debugging or what directory you are in.  */
  homedir = getenv ("HOME");
  if (homedir)
    {
      homeinit = (char *) alloca (strlen (getenv ("HOME")) +
				  strlen (gdbinit) + 10);
      strcpy (homeinit, getenv ("HOME"));
      strcat (homeinit, "/");
      strcat (homeinit, gdbinit);

      if (!inhibit_gdbinit && access (homeinit, R_OK) == 0)
	{
	  if (!SET_TOP_LEVEL ())
	    source_command (homeinit, 0);
	}
      do_cleanups (ALL_CLEANUPS);

      /* Do stats; no need to do them elsewhere since we'll only
	 need them if homedir is set.  Make sure that they are
	 zero in case one of them fails (this guarantees that they
	 won't match if either exists).  */
      
      memset (&homebuf, 0, sizeof (struct stat));
      memset (&cwdbuf, 0, sizeof (struct stat));
      
      stat (homeinit, &homebuf);
      stat (gdbinit, &cwdbuf); /* We'll only need this if
				       homedir was set.  */
    }

  /* Now perform all the actions indicated by the arguments.  */
  if (cdarg != NULL)
    {
      if (!SET_TOP_LEVEL ())
	{
	  cd_command (cdarg, 0);
	}
    }
  do_cleanups (ALL_CLEANUPS);

  for (i = 0; i < ndir; i++)
    if (!SET_TOP_LEVEL ())
      directory_command (dirarg[i], 0);
  free ((PTR)dirarg);
  do_cleanups (ALL_CLEANUPS);

  if (execarg != NULL
      && symarg != NULL
      && STREQ (execarg, symarg))
    {
      /* The exec file and the symbol-file are the same.  If we can't open
	 it, better only print one error message.  */
      if (!SET_TOP_LEVEL ())
	{
	  exec_file_command (execarg, !batch);
	  symbol_file_command (symarg, 0);
	}
    }
  else
    {
      if (execarg != NULL)
	if (!SET_TOP_LEVEL ())
	  exec_file_command (execarg, !batch);
      if (symarg != NULL)
	if (!SET_TOP_LEVEL ())
	  symbol_file_command (symarg, 0);
    }
  do_cleanups (ALL_CLEANUPS);

  /* After the symbol file has been read, print a newline to get us
     beyond the copyright line...  But errors should still set off
     the error message with a (single) blank line.  */
  if (!quiet)
    printf_filtered ("\n");
  error_pre_print = "\n";
  quit_pre_print = error_pre_print;
  warning_pre_print = "\nwarning: ";

  if (corearg != NULL)
    if (!SET_TOP_LEVEL ())
      core_file_command (corearg, !batch);
    else if (isdigit (corearg[0]) && !SET_TOP_LEVEL ())
      attach_command (corearg, !batch);
  do_cleanups (ALL_CLEANUPS);

  if (ttyarg != NULL)
    if (!SET_TOP_LEVEL ())
      tty_command (ttyarg, !batch);
  do_cleanups (ALL_CLEANUPS);

#ifdef ADDITIONAL_OPTION_HANDLER
  ADDITIONAL_OPTION_HANDLER;
#endif

  /* Error messages should no longer be distinguished with extra output. */
  error_pre_print = NULL;
  quit_pre_print = NULL;
  warning_pre_print = "warning: ";

  /* Read the .gdbinit file in the current directory, *if* it isn't
     the same as the $HOME/.gdbinit file (it should exist, also).  */
  
  if (!homedir
      || memcmp ((char *) &homebuf, (char *) &cwdbuf, sizeof (struct stat)))
    if (!inhibit_gdbinit && access (gdbinit, R_OK) == 0)
      {
	if (!SET_TOP_LEVEL ())
	  source_command (gdbinit, 0);
      }
  do_cleanups (ALL_CLEANUPS);

  for (i = 0; i < ncmd; i++)
    {
      if (!SET_TOP_LEVEL ())
	{
	  if (cmdarg[i][0] == '-' && cmdarg[i][1] == '\0')
	    read_command_file (stdin);
	  else
	    source_command (cmdarg[i], !batch);
	  do_cleanups (ALL_CLEANUPS);
	}
    }
  free ((PTR)cmdarg);

  /* Read in the old history after all the command files have been read. */
  init_history();

  if (batch)
    {
      /* We have hit the end of the batch file.  */
      exit (0);
    }

  /* Do any host- or target-specific hacks.  This is used for i960 targets
     to force the user to set a nindy target and spec its parameters.  */

#ifdef BEFORE_MAIN_LOOP_HOOK
  BEFORE_MAIN_LOOP_HOOK;
#endif

  END_PROGRESS (argv[0]);

  /* Show time and/or space usage.  */

  if (display_time)
    {
      long init_time = get_run_time () - time_at_startup;

      printf_unfiltered ("Startup time: %ld.%06ld\n",
			 init_time / 1000000, init_time % 1000000);
    }

  if (display_space)
    {
#ifdef HAVE_SBRK
      extern char **environ;
      char *lim = (char *) sbrk (0);

      printf_unfiltered ("Startup size: data size %ld\n",
			 (long) (lim - (char *) &environ));
#endif
    }

  /* The default command loop. 
     The WIN32 Gui calls this main to set up gdb's state, and 
     has its own command loop. */
#if !defined (WINGDB)
  while (1)
    {
      if (!SET_TOP_LEVEL ())
	{
	  do_cleanups (ALL_CLEANUPS);		/* Do complete cleanup */
	  /* GUIs generally have their own command loop, mainloop, or whatever.
	     This is a good place to gain control because many error
	     conditions will end up here via longjmp(). */
	  if (command_loop_hook)
	    command_loop_hook ();
	  else
	    command_loop ();
          quit_command ((char *)0, instream == stdin);
	}
    }

  /* No exit -- exit is through quit_command.  */
#endif

}

void
init_proc ()
{
}

void
proc_remove_foreign (pid)
     int pid;
{
}

void
fputs_unfiltered (linebuffer, stream)
     const char *linebuffer;
     FILE *stream;
{
  if (fputs_unfiltered_hook)
    {
      /* FIXME: I think we should only be doing this for stdout or stderr.
	 Either that or we should be passing stream to the hook so it can
	 deal with it.  If that is cleaned up, this function can go back
	 into utils.c and the fputs_unfiltered_hook can replace the current
	 ability to avoid this function by not linking with main.c.  */
      fputs_unfiltered_hook (linebuffer, stream);
      return;
    }

  fputs (linebuffer, stream);
}
