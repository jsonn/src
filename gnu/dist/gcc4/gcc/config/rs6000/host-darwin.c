/* Darwin/powerpc host-specific hook definitions.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include <signal.h>
#include <sys/ucontext.h>
#include "hosthooks.h"
#include "hosthooks-def.h"
#include "toplev.h"
#include "diagnostic.h"
#include "config/host-darwin.h"

static void segv_crash_handler (int);
static void segv_handler (int, siginfo_t *, void *);
static void darwin_rs6000_extra_signals (void);

#undef HOST_HOOKS_EXTRA_SIGNALS
#define HOST_HOOKS_EXTRA_SIGNALS darwin_rs6000_extra_signals

/* On Darwin/powerpc, hitting the stack limit turns into a SIGSEGV.
   This code detects the difference between hitting the stack limit and
   a true wild pointer dereference by looking at the instruction that
   faulted; only a few kinds of instruction are used to access below
   the previous bottom of the stack.  */

static void
segv_crash_handler (int sig ATTRIBUTE_UNUSED)
{
  internal_error ("Segmentation Fault (code)");
}

static void
segv_handler (int sig ATTRIBUTE_UNUSED,
	      siginfo_t *sip ATTRIBUTE_UNUSED,
	      void *scp)
{
  ucontext_t *uc = (ucontext_t *)scp;
  sigset_t sigset;
  unsigned faulting_insn;

  /* The fault might have happened when trying to run some instruction, in
     which case the next line will segfault _again_.  Handle this case.  */
  signal (SIGSEGV, segv_crash_handler);
  sigemptyset (&sigset);
  sigaddset (&sigset, SIGSEGV);
  sigprocmask (SIG_UNBLOCK, &sigset, NULL);

#if __DARWIN_UNIX03
  faulting_insn = *(unsigned *)uc->uc_mcontext->__ss.__srr0;
#else
  faulting_insn = *(unsigned *)uc->uc_mcontext->ss.srr0;
#endif

  /* Note that this only has to work for GCC, so we don't have to deal
     with all the possible cases (GCC has no AltiVec code, for
     instance).  It's complicated because Darwin allows stores to
     below the stack pointer, and the prologue code takes advantage of
     this.  */

  if ((faulting_insn & 0xFFFF8000) == 0x94218000  /* stwu %r1, -xxx(%r1) */
      || (faulting_insn & 0xFC1F03FF) == 0x7C01016E /* stwux xxx, %r1, xxx */
      || (faulting_insn & 0xFC1F8000) == 0x90018000 /* stw xxx, -yyy(%r1) */
      || (faulting_insn & 0xFC1F8000) == 0xD8018000 /* stfd xxx, -yyy(%r1) */
      || (faulting_insn & 0xFC1F8000) == 0xBC018000 /* stmw xxx, -yyy(%r1) */)
    {
      char *shell_name;
      
      fnotice (stderr, "Out of stack space.\n");
      shell_name = getenv ("SHELL");
      if (shell_name != NULL)
	shell_name = strrchr (shell_name, '/');
      if (shell_name != NULL)
	{
	  static const char * shell_commands[][2] = {
	    { "sh", "ulimit -S -s unlimited" },
	    { "bash", "ulimit -S -s unlimited" },
	    { "tcsh", "limit stacksize unlimited" },
	    { "csh", "limit stacksize unlimited" },
	    /* zsh doesn't have "unlimited", this will work under the
	       default configuration.  */
	    { "zsh", "limit stacksize 32m" }
	  };
	  size_t i;
	  
	  for (i = 0; i < ARRAY_SIZE (shell_commands); i++)
	    if (strcmp (shell_commands[i][0], shell_name + 1) == 0)
	      {
		fnotice (stderr, 
			 "Try running '%s' in the shell to raise its limit.\n",
			 shell_commands[i][1]);
	      }
	}
      
      if (global_dc->abort_on_error)
	fancy_abort (__FILE__, __LINE__, __FUNCTION__);

      exit (FATAL_EXIT_CODE);
    }

#if __DARWIN_UNIX03
  fprintf (stderr, "[address=%08lx pc=%08x]\n",
	  uc->uc_mcontext->__es.__dar, uc->uc_mcontext->__ss.__srr0);
#else
  fprintf (stderr, "[address=%08lx pc=%08x]\n", 
	   uc->uc_mcontext->es.dar, uc->uc_mcontext->ss.srr0);
#endif
  internal_error ("Segmentation Fault");
  exit (FATAL_EXIT_CODE);
}

static void
darwin_rs6000_extra_signals (void)
{
  struct sigaction sact;
  stack_t sigstk;

  sigstk.ss_sp = xmalloc (SIGSTKSZ);
  sigstk.ss_size = SIGSTKSZ;
  sigstk.ss_flags = 0;
  if (sigaltstack (&sigstk, NULL) < 0)
    fatal_error ("While setting up signal stack: %m");

  sigemptyset(&sact.sa_mask);
  sact.sa_flags = SA_ONSTACK | SA_SIGINFO;
  sact.sa_sigaction = segv_handler;
  if (sigaction (SIGSEGV, &sact, 0) < 0) 
    fatal_error ("While setting up signal handler: %m");
}


const struct host_hooks host_hooks = HOST_HOOKS_INITIALIZER;
