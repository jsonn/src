/* Target-dependent code for the SPARC for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* ??? Support for calling functions from gdb in sparc64 is unfinished.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "obstack.h"
#include "target.h"
#include "value.h"
#include "bfd.h"
#include "gdb_string.h"

#ifdef	USE_PROC_FS
#include <sys/procfs.h>
#endif

#include "gdbcore.h"

#if defined(TARGET_SPARCLET) || defined(TARGET_SPARCLITE)
#define SPARC_HAS_FPU 0
#else
#define SPARC_HAS_FPU 1
#endif

#ifdef GDB_TARGET_IS_SPARC64
#define FP_REGISTER_BYTES (64 * 4)
#else
#define FP_REGISTER_BYTES (32 * 4)
#endif

/* If not defined, assume 32 bit sparc.  */
#ifndef FP_MAX_REGNUM
#define FP_MAX_REGNUM (FP0_REGNUM + 32)
#endif

#define SPARC_INTREG_SIZE (REGISTER_RAW_SIZE (G0_REGNUM))

/* From infrun.c */
extern int stop_after_trap;

/* We don't store all registers immediately when requested, since they
   get sent over in large chunks anyway.  Instead, we accumulate most
   of the changes and send them over once.  "deferred_stores" keeps
   track of which sets of registers we have locally-changed copies of,
   so we only need send the groups that have changed.  */

int deferred_stores = 0;	/* Cumulates stores we want to do eventually. */


/* Some machines, such as Fujitsu SPARClite 86x, have a bi-endian mode
   where instructions are big-endian and data are little-endian.
   This flag is set when we detect that the target is of this type. */

int bi_endian = 0;


/* Fetch a single instruction.  Even on bi-endian machines
   such as sparc86x, instructions are always big-endian.  */

static unsigned long
fetch_instruction (pc)
     CORE_ADDR pc;
{
  unsigned long retval;
  int i;
  unsigned char buf[4];

  read_memory (pc, buf, sizeof (buf));

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;
  for (i = 0; i < sizeof (buf); ++i)
    retval = (retval << 8) | buf[i];
  return retval;
}


/* Branches with prediction are treated like their non-predicting cousins.  */
/* FIXME: What about floating point branches?  */

/* Macros to extract fields from sparc instructions.  */
#define X_OP(i) (((i) >> 30) & 0x3)
#define X_RD(i) (((i) >> 25) & 0x1f)
#define X_A(i) (((i) >> 29) & 1)
#define X_COND(i) (((i) >> 25) & 0xf)
#define X_OP2(i) (((i) >> 22) & 0x7)
#define X_IMM22(i) ((i) & 0x3fffff)
#define X_OP3(i) (((i) >> 19) & 0x3f)
#define X_RS1(i) (((i) >> 14) & 0x1f)
#define X_I(i) (((i) >> 13) & 1)
#define X_IMM13(i) ((i) & 0x1fff)
/* Sign extension macros.  */
#define X_SIMM13(i) ((X_IMM13 (i) ^ 0x1000) - 0x1000)
#define X_DISP22(i) ((X_IMM22 (i) ^ 0x200000) - 0x200000)
#define X_CC(i) (((i) >> 20) & 3)
#define X_P(i) (((i) >> 19) & 1)
#define X_DISP19(i) ((((i) & 0x7ffff) ^ 0x40000) - 0x40000)
#define X_RCOND(i) (((i) >> 25) & 7)
#define X_DISP16(i) ((((((i) >> 6) && 0xc000) | ((i) & 0x3fff)) ^ 0x8000) - 0x8000)
#define X_FCN(i) (((i) >> 25) & 31)

typedef enum
{
  Error, not_branch, bicc, bicca, ba, baa, ticc, ta,
#ifdef GDB_TARGET_IS_SPARC64
  done_retry
#endif
}
branch_type;

/* Simulate single-step ptrace call for sun4.  Code written by Gary
   Beihl (beihl@mcc.com).  */

/* npc4 and next_pc describe the situation at the time that the
   step-breakpoint was set, not necessary the current value of NPC_REGNUM.  */
static CORE_ADDR next_pc, npc4, target;
static int brknpc4, brktrg;
typedef char binsn_quantum[BREAKPOINT_MAX];
static binsn_quantum break_mem[3];

static branch_type isbranch PARAMS ((long, CORE_ADDR, CORE_ADDR *));

/* single_step() is called just before we want to resume the inferior,
   if we want to single-step it but there is no hardware or kernel single-step
   support (as on all SPARCs).  We find all the possible targets of the
   coming instruction and breakpoint them.

   single_step is also called just after the inferior stops.  If we had
   set up a simulated single-step, we undo our damage.  */

void
sparc_software_single_step (ignore, insert_breakpoints_p)
     enum target_signal ignore;	/* pid, but we don't need it */
     int insert_breakpoints_p;
{
  branch_type br;
  CORE_ADDR pc;
  long pc_instruction;

  if (insert_breakpoints_p)
    {
      /* Always set breakpoint for NPC.  */
      next_pc = read_register (NPC_REGNUM);
      npc4 = next_pc + 4;	/* branch not taken */

      target_insert_breakpoint (next_pc, break_mem[0]);
      /* printf_unfiltered ("set break at %x\n",next_pc); */

      pc = read_register (PC_REGNUM);
      pc_instruction = fetch_instruction (pc);
      br = isbranch (pc_instruction, pc, &target);
      brknpc4 = brktrg = 0;

      if (br == bicca)
	{
	  /* Conditional annulled branch will either end up at
	     npc (if taken) or at npc+4 (if not taken).
	     Trap npc+4.  */
	  brknpc4 = 1;
	  target_insert_breakpoint (npc4, break_mem[1]);
	}
      else if (br == baa && target != next_pc)
	{
	  /* Unconditional annulled branch will always end up at
	     the target.  */
	  brktrg = 1;
	  target_insert_breakpoint (target, break_mem[2]);
	}
#ifdef GDB_TARGET_IS_SPARC64
      else if (br == done_retry)
	{
	  brktrg = 1;
	  target_insert_breakpoint (target, break_mem[2]);
	}
#endif
    }
  else
    {
      /* Remove breakpoints */
      target_remove_breakpoint (next_pc, break_mem[0]);

      if (brknpc4)
	target_remove_breakpoint (npc4, break_mem[1]);

      if (brktrg)
	target_remove_breakpoint (target, break_mem[2]);
    }
}

/* Call this for each newly created frame.  For SPARC, we need to calculate
   the bottom of the frame, and do some extra work if the prologue
   has been generated via the -mflat option to GCC.  In particular,
   we need to know where the previous fp and the pc have been stashed,
   since their exact position within the frame may vary.  */

void
sparc_init_extra_frame_info (fromleaf, fi)
     int fromleaf;
     struct frame_info *fi;
{
  char *name;
  CORE_ADDR prologue_start, prologue_end;
  int insn;

  fi->bottom =
    (fi->next ?
     (fi->frame == fi->next->frame ? fi->next->bottom : fi->next->frame) :
     read_sp ());

  /* If fi->next is NULL, then we already set ->frame by passing read_fp()
     to create_new_frame.  */
  if (fi->next)
    {
      char buf[MAX_REGISTER_RAW_SIZE];

      /* Compute ->frame as if not flat.  If it is flat, we'll change
         it later.  */
      if (fi->next->next != NULL
	  && (fi->next->next->signal_handler_caller
	      || frame_in_dummy (fi->next->next))
	  && frameless_look_for_prologue (fi->next))
	{
	  /* A frameless function interrupted by a signal did not change
	     the frame pointer, fix up frame pointer accordingly.  */
	  fi->frame = FRAME_FP (fi->next);
	  fi->bottom = fi->next->bottom;
	}
      else
	{
	  /* Should we adjust for stack bias here? */
	  get_saved_register (buf, 0, 0, fi, FP_REGNUM, 0);
	  fi->frame = extract_address (buf, REGISTER_RAW_SIZE (FP_REGNUM));
#ifdef GDB_TARGET_IS_SPARC64
	  if (fi->frame & 1)
	    fi->frame += 2047;
	  else
	    fi->frame &= 0x0ffffffffL;
#endif

	}
    }

  /* Decide whether this is a function with a ``flat register window''
     frame.  For such functions, the frame pointer is actually in %i7.  */
  fi->flat = 0;
  fi->in_prologue = 0;
  if (find_pc_partial_function (fi->pc, &name, &prologue_start, &prologue_end))
    {
      /* See if the function starts with an add (which will be of a
         negative number if a flat frame) to the sp.  FIXME: Does not
         handle large frames which will need more than one instruction
         to adjust the sp.  */
      insn = fetch_instruction (prologue_start, 4);
      if (X_OP (insn) == 2 && X_RD (insn) == 14 && X_OP3 (insn) == 0
	  && X_I (insn) && X_SIMM13 (insn) < 0)
	{
	  int offset = X_SIMM13 (insn);

	  /* Then look for a save of %i7 into the frame.  */
	  insn = fetch_instruction (prologue_start + 4);
	  if (X_OP (insn) == 3
	      && X_RD (insn) == 31
	      && X_OP3 (insn) == 4
	      && X_RS1 (insn) == 14)
	    {
	      char buf[MAX_REGISTER_RAW_SIZE];

	      /* We definitely have a flat frame now.  */
	      fi->flat = 1;

	      fi->sp_offset = offset;

	      /* Overwrite the frame's address with the value in %i7.  */
	      get_saved_register (buf, 0, 0, fi, I7_REGNUM, 0);
	      fi->frame = extract_address (buf, REGISTER_RAW_SIZE (I7_REGNUM));
#ifdef GDB_TARGET_IS_SPARC64
	      if (fi->frame & 1)
		fi->frame += 2047;
	      else
		fi->frame &= 0x0ffffffffL;
#endif
	      /* Record where the fp got saved.  */
	      fi->fp_addr = fi->frame + fi->sp_offset + X_SIMM13 (insn);

	      /* Also try to collect where the pc got saved to.  */
	      fi->pc_addr = 0;
	      insn = fetch_instruction (prologue_start + 12);
	      if (X_OP (insn) == 3
		  && X_RD (insn) == 15
		  && X_OP3 (insn) == 4
		  && X_RS1 (insn) == 14)
		fi->pc_addr = fi->frame + fi->sp_offset + X_SIMM13 (insn);
	    }
	}
      else
	{
	  /* Check if the PC is in the function prologue before a SAVE
	     instruction has been executed yet.  If so, set the frame
	     to the current value of the stack pointer and set
	     the in_prologue flag.  */
	  CORE_ADDR addr;
	  struct symtab_and_line sal;

	  sal = find_pc_line (prologue_start, 0);
	  if (sal.line == 0)	/* no line info, use PC */
	    prologue_end = fi->pc;
	  else if (sal.end < prologue_end)
	    prologue_end = sal.end;
	  if (fi->pc < prologue_end)
	    {
	      for (addr = prologue_start; addr < fi->pc; addr += 4)
		{
		  insn = read_memory_integer (addr, 4);
		  if (X_OP (insn) == 2 && X_OP3 (insn) == 0x3c)
		    break;	/* SAVE seen, stop searching */
		}
	      if (addr >= fi->pc)
		{
		  fi->in_prologue = 1;
		  fi->frame = read_register (SP_REGNUM);
		}
	    }
	}
    }
  if (fi->next && fi->frame == 0)
    {
      /* Kludge to cause init_prev_frame_info to destroy the new frame.  */
      fi->frame = fi->next->frame;
      fi->pc = fi->next->pc;
    }
}

CORE_ADDR
sparc_frame_chain (frame)
     struct frame_info *frame;
{
  /* Value that will cause FRAME_CHAIN_VALID to not worry about the chain
     value.  If it realy is zero, we detect it later in
     sparc_init_prev_frame.  */
  return (CORE_ADDR) 1;
}

CORE_ADDR
sparc_extract_struct_value_address (regbuf)
     char regbuf[REGISTER_BYTES];
{
  return extract_address (regbuf + REGISTER_BYTE (O0_REGNUM),
			  REGISTER_RAW_SIZE (O0_REGNUM));
}

/* Find the pc saved in frame FRAME.  */

CORE_ADDR
sparc_frame_saved_pc (frame)
     struct frame_info *frame;
{
  char buf[MAX_REGISTER_RAW_SIZE];
  CORE_ADDR addr;

  if (frame->signal_handler_caller)
    {
      /* This is the signal trampoline frame.
         Get the saved PC from the sigcontext structure.  */

#ifndef SIGCONTEXT_PC_OFFSET
#define SIGCONTEXT_PC_OFFSET 12
#endif

      CORE_ADDR sigcontext_addr;
      char scbuf[TARGET_PTR_BIT / HOST_CHAR_BIT];
      int saved_pc_offset = SIGCONTEXT_PC_OFFSET;
      char *name = NULL;

      /* Solaris2 ucbsigvechandler passes a pointer to a sigcontext
         as the third parameter.  The offset to the saved pc is 12.  */
      find_pc_partial_function (frame->pc, &name,
				(CORE_ADDR *) NULL, (CORE_ADDR *) NULL);
      if (name && STREQ (name, "ucbsigvechandler"))
	saved_pc_offset = 12;

      /* The sigcontext address is contained in register O2.  */
      get_saved_register (buf, (int *) NULL, (CORE_ADDR *) NULL,
			  frame, O0_REGNUM + 2, (enum lval_type *) NULL);
      sigcontext_addr = extract_address (buf, REGISTER_RAW_SIZE (O0_REGNUM + 2));

      /* Don't cause a memory_error when accessing sigcontext in case the
         stack layout has changed or the stack is corrupt.  */
      target_read_memory (sigcontext_addr + saved_pc_offset,
			  scbuf, sizeof (scbuf));
      return extract_address (scbuf, sizeof (scbuf));
    }
  else if (frame->in_prologue ||
	   (frame->next != NULL
	    && (frame->next->signal_handler_caller
		|| frame_in_dummy (frame->next))
	    && frameless_look_for_prologue (frame)))
    {
      /* A frameless function interrupted by a signal did not save
         the PC, it is still in %o7.  */
      get_saved_register (buf, (int *) NULL, (CORE_ADDR *) NULL,
			  frame, O7_REGNUM, (enum lval_type *) NULL);
      return PC_ADJUST (extract_address (buf, SPARC_INTREG_SIZE));
    }
  if (frame->flat)
    addr = frame->pc_addr;
  else
    addr = frame->bottom + FRAME_SAVED_I0 +
      SPARC_INTREG_SIZE * (I7_REGNUM - I0_REGNUM);

  if (addr == 0)
    /* A flat frame leaf function might not save the PC anywhere,
       just leave it in %o7.  */
    return PC_ADJUST (read_register (O7_REGNUM));

  read_memory (addr, buf, SPARC_INTREG_SIZE);
  return PC_ADJUST (extract_address (buf, SPARC_INTREG_SIZE));
}

/* Since an individual frame in the frame cache is defined by two
   arguments (a frame pointer and a stack pointer), we need two
   arguments to get info for an arbitrary stack frame.  This routine
   takes two arguments and makes the cached frames look as if these
   two arguments defined a frame on the cache.  This allows the rest
   of info frame to extract the important arguments without
   difficulty.  */

struct frame_info *
setup_arbitrary_frame (argc, argv)
     int argc;
     CORE_ADDR *argv;
{
  struct frame_info *frame;

  if (argc != 2)
    error ("Sparc frame specifications require two arguments: fp and sp");

  frame = create_new_frame (argv[0], 0);

  if (!frame)
    internal_error ("create_new_frame returned invalid frame");

  frame->bottom = argv[1];
  frame->pc = FRAME_SAVED_PC (frame);
  return frame;
}

/* Given a pc value, skip it forward past the function prologue by
   disassembling instructions that appear to be a prologue.

   If FRAMELESS_P is set, we are only testing to see if the function
   is frameless.  This allows a quicker answer.

   This routine should be more specific in its actions; making sure
   that it uses the same register in the initial prologue section.  */

static CORE_ADDR examine_prologue PARAMS ((CORE_ADDR, int, struct frame_info *,
					   struct frame_saved_regs *));

static CORE_ADDR
examine_prologue (start_pc, frameless_p, fi, saved_regs)
     CORE_ADDR start_pc;
     int frameless_p;
     struct frame_info *fi;
     struct frame_saved_regs *saved_regs;
{
  int insn;
  int dest = -1;
  CORE_ADDR pc = start_pc;
  int is_flat = 0;

  insn = fetch_instruction (pc);

  /* Recognize the `sethi' insn and record its destination.  */
  if (X_OP (insn) == 0 && X_OP2 (insn) == 4)
    {
      dest = X_RD (insn);
      pc += 4;
      insn = fetch_instruction (pc);
    }

  /* Recognize an add immediate value to register to either %g1 or
     the destination register recorded above.  Actually, this might
     well recognize several different arithmetic operations.
     It doesn't check that rs1 == rd because in theory "sub %g0, 5, %g1"
     followed by "save %sp, %g1, %sp" is a valid prologue (Not that
     I imagine any compiler really does that, however).  */
  if (X_OP (insn) == 2
      && X_I (insn)
      && (X_RD (insn) == 1 || X_RD (insn) == dest))
    {
      pc += 4;
      insn = fetch_instruction (pc);
    }

  /* Recognize any SAVE insn.  */
  if (X_OP (insn) == 2 && X_OP3 (insn) == 60)
    {
      pc += 4;
      if (frameless_p)		/* If the save is all we care about, */
	return pc;		/* return before doing more work */
      insn = fetch_instruction (pc);
    }
  /* Recognize add to %sp.  */
  else if (X_OP (insn) == 2 && X_RD (insn) == 14 && X_OP3 (insn) == 0)
    {
      pc += 4;
      if (frameless_p)		/* If the add is all we care about, */
	return pc;		/* return before doing more work */
      is_flat = 1;
      insn = fetch_instruction (pc);
      /* Recognize store of frame pointer (i7).  */
      if (X_OP (insn) == 3
	  && X_RD (insn) == 31
	  && X_OP3 (insn) == 4
	  && X_RS1 (insn) == 14)
	{
	  pc += 4;
	  insn = fetch_instruction (pc);

	  /* Recognize sub %sp, <anything>, %i7.  */
	  if (X_OP (insn) == 2
	      && X_OP3 (insn) == 4
	      && X_RS1 (insn) == 14
	      && X_RD (insn) == 31)
	    {
	      pc += 4;
	      insn = fetch_instruction (pc);
	    }
	  else
	    return pc;
	}
      else
	return pc;
    }
  else
    /* Without a save or add instruction, it's not a prologue.  */
    return start_pc;

  while (1)
    {
      /* Recognize stores into the frame from the input registers.
         This recognizes all non alternate stores of input register,
         into a location offset from the frame pointer.  */
      if ((X_OP (insn) == 3
	   && (X_OP3 (insn) & 0x3c) == 4	/* Store, non-alternate.  */
	   && (X_RD (insn) & 0x18) == 0x18	/* Input register.  */
	   && X_I (insn)	/* Immediate mode.  */
	   && X_RS1 (insn) == 30	/* Off of frame pointer.  */
      /* Into reserved stack space.  */
	   && X_SIMM13 (insn) >= 0x44
	   && X_SIMM13 (insn) < 0x5b))
	;
      else if (is_flat
	       && X_OP (insn) == 3
	       && X_OP3 (insn) == 4
	       && X_RS1 (insn) == 14
	)
	{
	  if (saved_regs && X_I (insn))
	    saved_regs->regs[X_RD (insn)] =
	      fi->frame + fi->sp_offset + X_SIMM13 (insn);
	}
      else
	break;
      pc += 4;
      insn = fetch_instruction (pc);
    }

  return pc;
}

CORE_ADDR
sparc_skip_prologue (start_pc, frameless_p)
     CORE_ADDR start_pc;
     int frameless_p;
{
  return examine_prologue (start_pc, frameless_p, NULL, NULL);
}

/* Check instruction at ADDR to see if it is a branch.
   All non-annulled instructions will go to NPC or will trap.
   Set *TARGET if we find a candidate branch; set to zero if not.

   This isn't static as it's used by remote-sa.sparc.c.  */

static branch_type
isbranch (instruction, addr, target)
     long instruction;
     CORE_ADDR addr, *target;
{
  branch_type val = not_branch;
  long int offset = 0;		/* Must be signed for sign-extend.  */

  *target = 0;

  if (X_OP (instruction) == 0
      && (X_OP2 (instruction) == 2
	  || X_OP2 (instruction) == 6
	  || X_OP2 (instruction) == 1
	  || X_OP2 (instruction) == 3
	  || X_OP2 (instruction) == 5
#ifndef GDB_TARGET_IS_SPARC64
	  || X_OP2 (instruction) == 7
#endif
      ))
    {
      if (X_COND (instruction) == 8)
	val = X_A (instruction) ? baa : ba;
      else
	val = X_A (instruction) ? bicca : bicc;
      switch (X_OP2 (instruction))
	{
	case 2:
	case 6:
#ifndef GDB_TARGET_IS_SPARC64
	case 7:
#endif
	  offset = 4 * X_DISP22 (instruction);
	  break;
	case 1:
	case 5:
	  offset = 4 * X_DISP19 (instruction);
	  break;
	case 3:
	  offset = 4 * X_DISP16 (instruction);
	  break;
	}
      *target = addr + offset;
    }
#ifdef GDB_TARGET_IS_SPARC64
  else if (X_OP (instruction) == 2
	   && X_OP3 (instruction) == 62)
    {
      if (X_FCN (instruction) == 0)
	{
	  /* done */
	  *target = read_register (TNPC_REGNUM);
	  val = done_retry;
	}
      else if (X_FCN (instruction) == 1)
	{
	  /* retry */
	  *target = read_register (TPC_REGNUM);
	  val = done_retry;
	}
    }
#endif

  return val;
}

/* Find register number REGNUM relative to FRAME and put its
   (raw) contents in *RAW_BUFFER.  Set *OPTIMIZED if the variable
   was optimized out (and thus can't be fetched).  If the variable
   was fetched from memory, set *ADDRP to where it was fetched from,
   otherwise it was fetched from a register.

   The argument RAW_BUFFER must point to aligned memory.  */

void
sparc_get_saved_register (raw_buffer, optimized, addrp, frame, regnum, lval)
     char *raw_buffer;
     int *optimized;
     CORE_ADDR *addrp;
     struct frame_info *frame;
     int regnum;
     enum lval_type *lval;
{
  struct frame_info *frame1;
  CORE_ADDR addr;

  if (!target_has_registers)
    error ("No registers.");

  if (optimized)
    *optimized = 0;

  addr = 0;

  /* FIXME This code extracted from infcmd.c; should put elsewhere! */
  if (frame == NULL)
    {
      /* error ("No selected frame."); */
      if (!target_has_registers)
	error ("The program has no registers now.");
      if (selected_frame == NULL)
	error ("No selected frame.");
      /* Try to use selected frame */
      frame = get_prev_frame (selected_frame);
      if (frame == 0)
	error ("Cmd not meaningful in the outermost frame.");
    }


  frame1 = frame->next;

  /* Get saved PC from the frame info if not in innermost frame.  */
  if (regnum == PC_REGNUM && frame1 != NULL)
    {
      if (lval != NULL)
	*lval = not_lval;
      if (raw_buffer != NULL)
	{
	  /* Put it back in target format.  */
	  store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), frame->pc);
	}
      if (addrp != NULL)
	*addrp = 0;
      return;
    }

  while (frame1 != NULL)
    {
      if (frame1->pc >= (frame1->bottom ? frame1->bottom :
			 read_sp ())
	  && frame1->pc <= FRAME_FP (frame1))
	{
	  /* Dummy frame.  All but the window regs are in there somewhere.
	     The window registers are saved on the stack, just like in a
	     normal frame.  */
	  if (regnum >= G1_REGNUM && regnum < G1_REGNUM + 7)
	    addr = frame1->frame + (regnum - G0_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 8 * SPARC_INTREG_SIZE);
	  else if (regnum >= I0_REGNUM && regnum < I0_REGNUM + 8)
	    addr = (frame1->prev->bottom
		    + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_I0);
	  else if (regnum >= L0_REGNUM && regnum < L0_REGNUM + 8)
	    addr = (frame1->prev->bottom
		    + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_L0);
	  else if (regnum >= O0_REGNUM && regnum < O0_REGNUM + 8)
	    addr = frame1->frame + (regnum - O0_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 16 * SPARC_INTREG_SIZE);
#ifdef FP0_REGNUM
	  else if (regnum >= FP0_REGNUM && regnum < FP0_REGNUM + 32)
	    addr = frame1->frame + (regnum - FP0_REGNUM) * 4
	      - (FP_REGISTER_BYTES);
#ifdef GDB_TARGET_IS_SPARC64
	  else if (regnum >= FP0_REGNUM + 32 && regnum < FP_MAX_REGNUM)
	    addr = frame1->frame + 32 * 4 + (regnum - FP0_REGNUM - 32) * 8
	      - (FP_REGISTER_BYTES);
#endif
#endif /* FP0_REGNUM */
	  else if (regnum >= Y_REGNUM && regnum < NUM_REGS)
	    addr = frame1->frame + (regnum - Y_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 24 * SPARC_INTREG_SIZE);
	}
      else if (frame1->flat)
	{

	  if (regnum == RP_REGNUM)
	    addr = frame1->pc_addr;
	  else if (regnum == I7_REGNUM)
	    addr = frame1->fp_addr;
	  else
	    {
	      CORE_ADDR func_start;
	      struct frame_saved_regs regs;
	      memset (&regs, 0, sizeof (regs));

	      find_pc_partial_function (frame1->pc, NULL, &func_start, NULL);
	      examine_prologue (func_start, 0, frame1, &regs);
	      addr = regs.regs[regnum];
	    }
	}
      else
	{
	  /* Normal frame.  Local and In registers are saved on stack.  */
	  if (regnum >= I0_REGNUM && regnum < I0_REGNUM + 8)
	    addr = (frame1->prev->bottom
		    + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_I0);
	  else if (regnum >= L0_REGNUM && regnum < L0_REGNUM + 8)
	    addr = (frame1->prev->bottom
		    + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_L0);
	  else if (regnum >= O0_REGNUM && regnum < O0_REGNUM + 8)
	    {
	      /* Outs become ins.  */
	      get_saved_register (raw_buffer, optimized, addrp, frame1,
				  (regnum - O0_REGNUM + I0_REGNUM), lval);
	      return;
	    }
	}
      if (addr != 0)
	break;
      frame1 = frame1->next;
    }
  if (addr != 0)
    {
      if (lval != NULL)
	*lval = lval_memory;
      if (regnum == SP_REGNUM)
	{
	  if (raw_buffer != NULL)
	    {
	      /* Put it back in target format.  */
	      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), addr);
	    }
	  if (addrp != NULL)
	    *addrp = 0;
	  return;
	}
      if (raw_buffer != NULL)
	read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
    }
  else
    {
      if (lval != NULL)
	*lval = lval_register;
      addr = REGISTER_BYTE (regnum);
      if (raw_buffer != NULL)
	read_register_gen (regnum, raw_buffer);
    }
  if (addrp != NULL)
    *addrp = addr;
}

/* Push an empty stack frame, and record in it the current PC, regs, etc.

   We save the non-windowed registers and the ins.  The locals and outs
   are new; they don't need to be saved. The i's and l's of
   the last frame were already saved on the stack.  */

/* Definitely see tm-sparc.h for more doc of the frame format here.  */

#ifdef GDB_TARGET_IS_SPARC64
#define DUMMY_REG_SAVE_OFFSET (128 + 16)
#else
#define DUMMY_REG_SAVE_OFFSET 0x60
#endif

/* See tm-sparc.h for how this is calculated.  */
#ifdef FP0_REGNUM
#define DUMMY_STACK_REG_BUF_SIZE \
(((8+8+8) * SPARC_INTREG_SIZE) + FP_REGISTER_BYTES)
#else
#define DUMMY_STACK_REG_BUF_SIZE \
(((8+8+8) * SPARC_INTREG_SIZE) )
#endif /* FP0_REGNUM */
#define DUMMY_STACK_SIZE (DUMMY_STACK_REG_BUF_SIZE + DUMMY_REG_SAVE_OFFSET)

void
sparc_push_dummy_frame ()
{
  CORE_ADDR sp, old_sp;
  char register_temp[DUMMY_STACK_SIZE];

  old_sp = sp = read_sp ();

#ifdef GDB_TARGET_IS_SPARC64
  /* PC, NPC, CCR, FSR, FPRS, Y, ASI */
  read_register_bytes (REGISTER_BYTE (PC_REGNUM), &register_temp[0],
		       REGISTER_RAW_SIZE (PC_REGNUM) * 7);
  read_register_bytes (REGISTER_BYTE (PSTATE_REGNUM), &register_temp[8],
		       REGISTER_RAW_SIZE (PSTATE_REGNUM));
  /* FIXME: not sure what needs to be saved here.  */
#else
  /* Y, PS, WIM, TBR, PC, NPC, FPS, CPS regs */
  read_register_bytes (REGISTER_BYTE (Y_REGNUM), &register_temp[0],
		       REGISTER_RAW_SIZE (Y_REGNUM) * 8);
#endif

  read_register_bytes (REGISTER_BYTE (O0_REGNUM),
		       &register_temp[8 * SPARC_INTREG_SIZE],
		       SPARC_INTREG_SIZE * 8);

  read_register_bytes (REGISTER_BYTE (G0_REGNUM),
		       &register_temp[16 * SPARC_INTREG_SIZE],
		       SPARC_INTREG_SIZE * 8);

#ifdef FP0_REGNUM
  read_register_bytes (REGISTER_BYTE (FP0_REGNUM),
		       &register_temp[24 * SPARC_INTREG_SIZE],
		       FP_REGISTER_BYTES);
#endif /* FP0_REGNUM */

  sp -= DUMMY_STACK_SIZE;

  write_sp (sp);

  write_memory (sp + DUMMY_REG_SAVE_OFFSET, &register_temp[0],
		DUMMY_STACK_REG_BUF_SIZE);

  if (strcmp (target_shortname, "sim") != 0)
    {
      write_fp (old_sp);

      /* Set return address register for the call dummy to the current PC.  */
      write_register (I7_REGNUM, read_pc () - 8);
    }
  else
    {
      /* The call dummy will write this value to FP before executing
         the 'save'.  This ensures that register window flushes work
         correctly in the simulator.  */
      write_register (G0_REGNUM + 1, read_register (FP_REGNUM));

      /* The call dummy will write this value to FP after executing
         the 'save'. */
      write_register (G0_REGNUM + 2, old_sp);

      /* The call dummy will write this value to the return address (%i7) after
         executing the 'save'. */
      write_register (G0_REGNUM + 3, read_pc () - 8);

      /* Set the FP that the call dummy will be using after the 'save'.
         This makes backtraces from an inferior function call work properly.  */
      write_register (FP_REGNUM, old_sp);
    }
}

/* sparc_frame_find_saved_regs ().  This function is here only because
   pop_frame uses it.  Note there is an interesting corner case which
   I think few ports of GDB get right--if you are popping a frame
   which does not save some register that *is* saved by a more inner
   frame (such a frame will never be a dummy frame because dummy
   frames save all registers).  Rewriting pop_frame to use
   get_saved_register would solve this problem and also get rid of the
   ugly duplication between sparc_frame_find_saved_regs and
   get_saved_register.

   Stores, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.

   Note that on register window machines, we are currently making the
   assumption that window registers are being saved somewhere in the
   frame in which they are being used.  If they are stored in an
   inferior frame, find_saved_register will break.

   On the Sun 4, the only time all registers are saved is when
   a dummy frame is involved.  Otherwise, the only saved registers
   are the LOCAL and IN registers which are saved as a result
   of the "save/restore" opcodes.  This condition is determined
   by address rather than by value.

   The "pc" is not stored in a frame on the SPARC.  (What is stored
   is a return address minus 8.)  sparc_pop_frame knows how to
   deal with that.  Other routines might or might not.

   See tm-sparc.h (PUSH_DUMMY_FRAME and friends) for CRITICAL information
   about how this works.  */

static void sparc_frame_find_saved_regs PARAMS ((struct frame_info *,
						 struct frame_saved_regs *));

static void
sparc_frame_find_saved_regs (fi, saved_regs_addr)
     struct frame_info *fi;
     struct frame_saved_regs *saved_regs_addr;
{
  register int regnum;
  CORE_ADDR frame_addr = FRAME_FP (fi);

  if (!fi)
    internal_error ("Bad frame info struct in FRAME_FIND_SAVED_REGS");

  memset (saved_regs_addr, 0, sizeof (*saved_regs_addr));

  if (fi->pc >= (fi->bottom ? fi->bottom :
		 read_sp ())
      && fi->pc <= FRAME_FP (fi))
    {
      /* Dummy frame.  All but the window regs are in there somewhere. */
      for (regnum = G1_REGNUM; regnum < G1_REGNUM + 7; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - G0_REGNUM) * SPARC_INTREG_SIZE
	  - DUMMY_STACK_REG_BUF_SIZE + 16 * SPARC_INTREG_SIZE;
      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	  - DUMMY_STACK_REG_BUF_SIZE + 8 * SPARC_INTREG_SIZE;
#ifdef FP0_REGNUM
      for (regnum = FP0_REGNUM; regnum < FP0_REGNUM + 32; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - FP0_REGNUM) * 4
	  - DUMMY_STACK_REG_BUF_SIZE + 24 * SPARC_INTREG_SIZE;
#ifdef GDB_TARGET_IS_SPARC64
      for (regnum = FP0_REGNUM + 32; regnum < FP_MAX_REGNUM; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + 32 * 4 + (regnum - FP0_REGNUM - 32) * 4
	  - DUMMY_STACK_REG_BUF_SIZE + 24 * SPARC_INTREG_SIZE;
#endif
#endif /* FP0_REGNUM */
#ifdef GDB_TARGET_IS_SPARC64
      for (regnum = PC_REGNUM; regnum < PC_REGNUM + 7; regnum++)
	{
	  saved_regs_addr->regs[regnum] =
	    frame_addr + (regnum - PC_REGNUM) * SPARC_INTREG_SIZE
	    - DUMMY_STACK_REG_BUF_SIZE;
	}
      saved_regs_addr->regs[PSTATE_REGNUM] =
	frame_addr + 8 * SPARC_INTREG_SIZE - DUMMY_STACK_REG_BUF_SIZE;
#else
      for (regnum = Y_REGNUM; regnum < NUM_REGS; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - Y_REGNUM) * SPARC_INTREG_SIZE
	  - DUMMY_STACK_REG_BUF_SIZE;
#endif
      frame_addr = fi->bottom ?
	fi->bottom : read_sp ();
    }
  else if (fi->flat)
    {
      CORE_ADDR func_start;
      find_pc_partial_function (fi->pc, NULL, &func_start, NULL);
      examine_prologue (func_start, 0, fi, saved_regs_addr);

      /* Flat register window frame.  */
      saved_regs_addr->regs[RP_REGNUM] = fi->pc_addr;
      saved_regs_addr->regs[I7_REGNUM] = fi->fp_addr;
    }
  else
    {
      /* Normal frame.  Just Local and In registers */
      frame_addr = fi->bottom ?
	fi->bottom : read_sp ();
      for (regnum = L0_REGNUM; regnum < L0_REGNUM + 8; regnum++)
	saved_regs_addr->regs[regnum] =
	  (frame_addr + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
	   + FRAME_SAVED_L0);
      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; regnum++)
	saved_regs_addr->regs[regnum] =
	  (frame_addr + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	   + FRAME_SAVED_I0);
    }
  if (fi->next)
    {
      if (fi->flat)
	{
	  saved_regs_addr->regs[O7_REGNUM] = fi->pc_addr;
	}
      else
	{
	  /* Pull off either the next frame pointer or the stack pointer */
	  CORE_ADDR next_next_frame_addr =
	  (fi->next->bottom ?
	   fi->next->bottom :
	   read_sp ());
	  for (regnum = O0_REGNUM; regnum < O0_REGNUM + 8; regnum++)
	    saved_regs_addr->regs[regnum] =
	      (next_next_frame_addr
	       + (regnum - O0_REGNUM) * SPARC_INTREG_SIZE
	       + FRAME_SAVED_I0);
	}
    }
  /* Otherwise, whatever we would get from ptrace(GETREGS) is accurate */
  /* FIXME -- should this adjust for the sparc64 offset? */
  saved_regs_addr->regs[SP_REGNUM] = FRAME_FP (fi);
}

/* Discard from the stack the innermost frame, restoring all saved registers.

   Note that the values stored in fsr by get_frame_saved_regs are *in
   the context of the called frame*.  What this means is that the i
   regs of fsr must be restored into the o regs of the (calling) frame that
   we pop into.  We don't care about the output regs of the calling frame,
   since unless it's a dummy frame, it won't have any output regs in it.

   We never have to bother with %l (local) regs, since the called routine's
   locals get tossed, and the calling routine's locals are already saved
   on its stack.  */

/* Definitely see tm-sparc.h for more doc of the frame format here.  */

void
sparc_pop_frame ()
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR pc;
  struct frame_saved_regs fsr;
  char raw_buffer[REGISTER_BYTES];
  int regnum;

  sparc_frame_find_saved_regs (frame, &fsr);
#ifdef FP0_REGNUM
  if (fsr.regs[FP0_REGNUM])
    {
      read_memory (fsr.regs[FP0_REGNUM], raw_buffer, FP_REGISTER_BYTES);
      write_register_bytes (REGISTER_BYTE (FP0_REGNUM),
			    raw_buffer, FP_REGISTER_BYTES);
    }
#ifndef GDB_TARGET_IS_SPARC64
  if (fsr.regs[FPS_REGNUM])
    {
      read_memory (fsr.regs[FPS_REGNUM], raw_buffer, 4);
      write_register_bytes (REGISTER_BYTE (FPS_REGNUM), raw_buffer, 4);
    }
  if (fsr.regs[CPS_REGNUM])
    {
      read_memory (fsr.regs[CPS_REGNUM], raw_buffer, 4);
      write_register_bytes (REGISTER_BYTE (CPS_REGNUM), raw_buffer, 4);
    }
#endif
#endif /* FP0_REGNUM */
  if (fsr.regs[G1_REGNUM])
    {
      read_memory (fsr.regs[G1_REGNUM], raw_buffer, 7 * SPARC_INTREG_SIZE);
      write_register_bytes (REGISTER_BYTE (G1_REGNUM), raw_buffer,
			    7 * SPARC_INTREG_SIZE);
    }

  if (frame->flat)
    {
      /* Each register might or might not have been saved, need to test
         individually.  */
      for (regnum = L0_REGNUM; regnum < L0_REGNUM + 8; ++regnum)
	if (fsr.regs[regnum])
	  write_register (regnum, read_memory_integer (fsr.regs[regnum],
						       SPARC_INTREG_SIZE));
      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; ++regnum)
	if (fsr.regs[regnum])
	  write_register (regnum, read_memory_integer (fsr.regs[regnum],
						       SPARC_INTREG_SIZE));

      /* Handle all outs except stack pointer (o0-o5; o7).  */
      for (regnum = O0_REGNUM; regnum < O0_REGNUM + 6; ++regnum)
	if (fsr.regs[regnum])
	  write_register (regnum, read_memory_integer (fsr.regs[regnum],
						       SPARC_INTREG_SIZE));
      if (fsr.regs[O0_REGNUM + 7])
	write_register (O0_REGNUM + 7,
			read_memory_integer (fsr.regs[O0_REGNUM + 7],
					     SPARC_INTREG_SIZE));

      write_sp (frame->frame);
    }
  else if (fsr.regs[I0_REGNUM])
    {
      CORE_ADDR sp;

      char reg_temp[REGISTER_BYTES];

      read_memory (fsr.regs[I0_REGNUM], raw_buffer, 8 * SPARC_INTREG_SIZE);

      /* Get the ins and locals which we are about to restore.  Just
         moving the stack pointer is all that is really needed, except
         store_inferior_registers is then going to write the ins and
         locals from the registers array, so we need to muck with the
         registers array.  */
      sp = fsr.regs[SP_REGNUM];
#ifdef GDB_TARGET_IS_SPARC64
      if (sp & 1)
	sp += 2047;
      else
	sp &= 0x0ffffffffL;
#endif
      read_memory (sp, reg_temp, SPARC_INTREG_SIZE * 16);

      /* Restore the out registers.
         Among other things this writes the new stack pointer.  */
      write_register_bytes (REGISTER_BYTE (O0_REGNUM), raw_buffer,
			    SPARC_INTREG_SIZE * 8);

      write_register_bytes (REGISTER_BYTE (L0_REGNUM), reg_temp,
			    SPARC_INTREG_SIZE * 16);
    }
#ifndef GDB_TARGET_IS_SPARC64
  if (fsr.regs[PS_REGNUM])
    write_register (PS_REGNUM, read_memory_integer (fsr.regs[PS_REGNUM], 4));
#endif
  if (fsr.regs[Y_REGNUM])
    write_register (Y_REGNUM, read_memory_integer (fsr.regs[Y_REGNUM], REGISTER_RAW_SIZE (Y_REGNUM)));
  if (fsr.regs[PC_REGNUM])
    {
      /* Explicitly specified PC (and maybe NPC) -- just restore them. */
      write_register (PC_REGNUM, read_memory_integer (fsr.regs[PC_REGNUM],
					    REGISTER_RAW_SIZE (PC_REGNUM)));
      if (fsr.regs[NPC_REGNUM])
	write_register (NPC_REGNUM,
			read_memory_integer (fsr.regs[NPC_REGNUM],
					   REGISTER_RAW_SIZE (NPC_REGNUM)));
    }
  else if (frame->flat)
    {
      if (frame->pc_addr)
	pc = PC_ADJUST ((CORE_ADDR)
			read_memory_integer (frame->pc_addr,
					     REGISTER_RAW_SIZE (PC_REGNUM)));
      else
	{
	  /* I think this happens only in the innermost frame, if so then
	     it is a complicated way of saying
	     "pc = read_register (O7_REGNUM);".  */
	  char buf[MAX_REGISTER_RAW_SIZE];
	  get_saved_register (buf, 0, 0, frame, O7_REGNUM, 0);
	  pc = PC_ADJUST (extract_address
			  (buf, REGISTER_RAW_SIZE (O7_REGNUM)));
	}

      write_register (PC_REGNUM, pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  else if (fsr.regs[I7_REGNUM])
    {
      /* Return address in %i7 -- adjust it, then restore PC and NPC from it */
      pc = PC_ADJUST ((CORE_ADDR) read_memory_integer (fsr.regs[I7_REGNUM],
						       SPARC_INTREG_SIZE));
      write_register (PC_REGNUM, pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  flush_cached_frames ();
}

/* On the Sun 4 under SunOS, the compile will leave a fake insn which
   encodes the structure size being returned.  If we detect such
   a fake insn, step past it.  */

CORE_ADDR
sparc_pc_adjust (pc)
     CORE_ADDR pc;
{
  unsigned long insn;
  char buf[4];
  int err;

  err = target_read_memory (pc + 8, buf, 4);
  insn = extract_unsigned_integer (buf, 4);
  if ((err == 0) && (insn & 0xffc00000) == 0)
    return pc + 12;
  else
    return pc + 8;
}

/* If pc is in a shared library trampoline, return its target.
   The SunOs 4.x linker rewrites the jump table entries for PIC
   compiled modules in the main executable to bypass the dynamic linker
   with jumps of the form
   sethi %hi(addr),%g1
   jmp %g1+%lo(addr)
   and removes the corresponding jump table relocation entry in the
   dynamic relocations.
   find_solib_trampoline_target relies on the presence of the jump
   table relocation entry, so we have to detect these jump instructions
   by hand.  */

CORE_ADDR
sunos4_skip_trampoline_code (pc)
     CORE_ADDR pc;
{
  unsigned long insn1;
  char buf[4];
  int err;

  err = target_read_memory (pc, buf, 4);
  insn1 = extract_unsigned_integer (buf, 4);
  if (err == 0 && (insn1 & 0xffc00000) == 0x03000000)
    {
      unsigned long insn2;

      err = target_read_memory (pc + 4, buf, 4);
      insn2 = extract_unsigned_integer (buf, 4);
      if (err == 0 && (insn2 & 0xffffe000) == 0x81c06000)
	{
	  CORE_ADDR target_pc = (insn1 & 0x3fffff) << 10;
	  int delta = insn2 & 0x1fff;

	  /* Sign extend the displacement.  */
	  if (delta & 0x1000)
	    delta |= ~0x1fff;
	  return target_pc + delta;
	}
    }
  return find_solib_trampoline_target (pc);
}

#ifdef USE_PROC_FS		/* Target dependent support for /proc */
/* *INDENT-OFF* */
/*  The /proc interface divides the target machine's register set up into
    two different sets, the general register set (gregset) and the floating
    point register set (fpregset).  For each set, there is an ioctl to get
    the current register set and another ioctl to set the current values.

    The actual structure passed through the ioctl interface is, of course,
    naturally machine dependent, and is different for each set of registers.
    For the sparc for example, the general register set is typically defined
    by:

	typedef int gregset_t[38];

	#define	R_G0	0
	...
	#define	R_TBR	37

    and the floating point set by:

	typedef struct prfpregset {
		union { 
			u_long  pr_regs[32]; 
			double  pr_dregs[16];
		} pr_fr;
		void *  pr_filler;
		u_long  pr_fsr;
		u_char  pr_qcnt;
		u_char  pr_q_entrysize;
		u_char  pr_en;
		u_long  pr_q[64];
	} prfpregset_t;

    These routines provide the packing and unpacking of gregset_t and
    fpregset_t formatted data.

 */
/* *INDENT-ON* */



/* Given a pointer to a general register set in /proc format (gregset_t *),
   unpack the register contents and supply them as gdb's idea of the current
   register values. */

void
supply_gregset (gregsetp)
     prgregset_t *gregsetp;
{
  register int regi;
  register prgreg_t *regp = (prgreg_t *) gregsetp;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] =
  {0};

  /* GDB register numbers for Gn, On, Ln, In all match /proc reg numbers.  */
  for (regi = G0_REGNUM; regi <= I7_REGNUM; regi++)
    {
      supply_register (regi, (char *) (regp + regi));
    }

  /* These require a bit more care.  */
  supply_register (PS_REGNUM, (char *) (regp + R_PS));
  supply_register (PC_REGNUM, (char *) (regp + R_PC));
  supply_register (NPC_REGNUM, (char *) (regp + R_nPC));
  supply_register (Y_REGNUM, (char *) (regp + R_Y));

  /* Fill inaccessible registers with zero.  */
  supply_register (WIM_REGNUM, zerobuf);
  supply_register (TBR_REGNUM, zerobuf);
  supply_register (CPS_REGNUM, zerobuf);
}

void
fill_gregset (gregsetp, regno)
     prgregset_t *gregsetp;
     int regno;
{
  int regi;
  register prgreg_t *regp = (prgreg_t *) gregsetp;

  for (regi = 0; regi <= R_I7; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  *(regp + regi) = *(int *) &registers[REGISTER_BYTE (regi)];
	}
    }
  if ((regno == -1) || (regno == PS_REGNUM))
    {
      *(regp + R_PS) = *(int *) &registers[REGISTER_BYTE (PS_REGNUM)];
    }
  if ((regno == -1) || (regno == PC_REGNUM))
    {
      *(regp + R_PC) = *(int *) &registers[REGISTER_BYTE (PC_REGNUM)];
    }
  if ((regno == -1) || (regno == NPC_REGNUM))
    {
      *(regp + R_nPC) = *(int *) &registers[REGISTER_BYTE (NPC_REGNUM)];
    }
  if ((regno == -1) || (regno == Y_REGNUM))
    {
      *(regp + R_Y) = *(int *) &registers[REGISTER_BYTE (Y_REGNUM)];
    }
}

#if defined (FP0_REGNUM)

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), unpack the register contents and supply them as gdb's
   idea of the current floating point register values. */

void
supply_fpregset (fpregsetp)
     prfpregset_t *fpregsetp;
{
  register int regi;
  char *from;

  for (regi = FP0_REGNUM; regi < FP_MAX_REGNUM; regi++)
    {
      from = (char *) &fpregsetp->pr_fr.pr_regs[regi - FP0_REGNUM];
      supply_register (regi, from);
    }
  supply_register (FPS_REGNUM, (char *) &(fpregsetp->pr_fsr));
}

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), update the register specified by REGNO from gdb's idea
   of the current floating point register set.  If REGNO is -1, update
   them all. */
/* ??? This will probably need some changes for sparc64.  */

void
fill_fpregset (fpregsetp, regno)
     prfpregset_t *fpregsetp;
     int regno;
{
  int regi;
  char *to;
  char *from;

  for (regi = FP0_REGNUM; regi < FP_MAX_REGNUM; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &fpregsetp->pr_fr.pr_regs[regi - FP0_REGNUM];
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }
  if ((regno == -1) || (regno == FPS_REGNUM))
    {
      fpregsetp->pr_fsr = *(int *) &registers[REGISTER_BYTE (FPS_REGNUM)];
    }
}

#endif /* defined (FP0_REGNUM) */

#endif /* USE_PROC_FS */


#ifdef GET_LONGJMP_TARGET

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

int
get_longjmp_target (pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
#define LONGJMP_TARGET_SIZE 4
  char buf[LONGJMP_TARGET_SIZE];

  jb_addr = read_register (O0_REGNUM);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  LONGJMP_TARGET_SIZE))
    return 0;

  *pc = extract_address (buf, LONGJMP_TARGET_SIZE);

  return 1;
}
#endif /* GET_LONGJMP_TARGET */

#ifdef STATIC_TRANSFORM_NAME
/* SunPRO (3.0 at least), encodes the static variables.  This is not
   related to C++ mangling, it is done for C too.  */

char *
sunpro_static_transform_name (name)
     char *name;
{
  char *p;
  if (name[0] == '$')
    {
      /* For file-local statics there will be a dollar sign, a bunch
         of junk (the contents of which match a string given in the
         N_OPT), a period and the name.  For function-local statics
         there will be a bunch of junk (which seems to change the
         second character from 'A' to 'B'), a period, the name of the
         function, and the name.  So just skip everything before the
         last period.  */
      p = strrchr (name, '.');
      if (p != NULL)
	name = p + 1;
    }
  return name;
}
#endif /* STATIC_TRANSFORM_NAME */


/* Utilities for printing registers.
   Page numbers refer to the SPARC Architecture Manual.  */

static void dump_ccreg PARAMS ((char *, int));

static void
dump_ccreg (reg, val)
     char *reg;
     int val;
{
  /* page 41 */
  printf_unfiltered ("%s:%s,%s,%s,%s", reg,
		     val & 8 ? "N" : "NN",
		     val & 4 ? "Z" : "NZ",
		     val & 2 ? "O" : "NO",
		     val & 1 ? "C" : "NC"
    );
}

static char *
decode_asi (val)
     int val;
{
  /* page 72 */
  switch (val)
    {
    case 4:
      return "ASI_NUCLEUS";
    case 0x0c:
      return "ASI_NUCLEUS_LITTLE";
    case 0x10:
      return "ASI_AS_IF_USER_PRIMARY";
    case 0x11:
      return "ASI_AS_IF_USER_SECONDARY";
    case 0x18:
      return "ASI_AS_IF_USER_PRIMARY_LITTLE";
    case 0x19:
      return "ASI_AS_IF_USER_SECONDARY_LITTLE";
    case 0x80:
      return "ASI_PRIMARY";
    case 0x81:
      return "ASI_SECONDARY";
    case 0x82:
      return "ASI_PRIMARY_NOFAULT";
    case 0x83:
      return "ASI_SECONDARY_NOFAULT";
    case 0x88:
      return "ASI_PRIMARY_LITTLE";
    case 0x89:
      return "ASI_SECONDARY_LITTLE";
    case 0x8a:
      return "ASI_PRIMARY_NOFAULT_LITTLE";
    case 0x8b:
      return "ASI_SECONDARY_NOFAULT_LITTLE";
    default:
      return NULL;
    }
}

/* PRINT_REGISTER_HOOK routine.
   Pretty print various registers.  */
/* FIXME: Would be nice if this did some fancy things for 32 bit sparc.  */

void
sparc_print_register_hook (regno)
     int regno;
{
  ULONGEST val;

  /* Handle double/quad versions of lower 32 fp regs.  */
  if (regno >= FP0_REGNUM && regno < FP0_REGNUM + 32
      && (regno & 1) == 0)
    {
      char value[16];

      if (!read_relative_register_raw_bytes (regno, value)
	  && !read_relative_register_raw_bytes (regno + 1, value + 4))
	{
	  printf_unfiltered ("\t");
	  print_floating (value, builtin_type_double, gdb_stdout);
	}
#if 0				/* FIXME: gdb doesn't handle long doubles */
      if ((regno & 3) == 0)
	{
	  if (!read_relative_register_raw_bytes (regno + 2, value + 8)
	      && !read_relative_register_raw_bytes (regno + 3, value + 12))
	    {
	      printf_unfiltered ("\t");
	      print_floating (value, builtin_type_long_double, gdb_stdout);
	    }
	}
#endif
      return;
    }

#if 0				/* FIXME: gdb doesn't handle long doubles */
  /* Print upper fp regs as long double if appropriate.  */
  if (regno >= FP0_REGNUM + 32 && regno < FP_MAX_REGNUM
  /* We test for even numbered regs and not a multiple of 4 because
     the upper fp regs are recorded as doubles.  */
      && (regno & 1) == 0)
    {
      char value[16];

      if (!read_relative_register_raw_bytes (regno, value)
	  && !read_relative_register_raw_bytes (regno + 1, value + 8))
	{
	  printf_unfiltered ("\t");
	  print_floating (value, builtin_type_long_double, gdb_stdout);
	}
      return;
    }
#endif

  /* FIXME: Some of these are priviledged registers.
     Not sure how they should be handled.  */

#define BITS(n, mask) ((int) (((val) >> (n)) & (mask)))

  val = read_register (regno);

  /* pages 40 - 60 */
  switch (regno)
    {
#ifdef GDB_TARGET_IS_SPARC64
    case CCR_REGNUM:
      printf_unfiltered ("\t");
      dump_ccreg ("xcc", val >> 4);
      printf_unfiltered (", ");
      dump_ccreg ("icc", val & 15);
      break;
    case FPRS_REGNUM:
      printf ("\tfef:%d, du:%d, dl:%d",
	      BITS (2, 1), BITS (1, 1), BITS (0, 1));
      break;
    case FSR_REGNUM:
      {
	static char *fcc[4] =
	{"=", "<", ">", "?"};
	static char *rd[4] =
	{"N", "0", "+", "-"};
	/* Long, yes, but I'd rather leave it as is and use a wide screen.  */
	printf ("\t0:%s, 1:%s, 2:%s, 3:%s, rd:%s, tem:%d, ns:%d, ver:%d, ftt:%d, qne:%d, aexc:%d, cexc:%d",
		fcc[BITS (10, 3)], fcc[BITS (32, 3)],
		fcc[BITS (34, 3)], fcc[BITS (36, 3)],
		rd[BITS (30, 3)], BITS (23, 31), BITS (22, 1), BITS (17, 7),
		BITS (14, 7), BITS (13, 1), BITS (5, 31), BITS (0, 31));
	break;
      }
    case ASI_REGNUM:
      {
	char *asi = decode_asi (val);
	if (asi != NULL)
	  printf ("\t%s", asi);
	break;
      }
    case VER_REGNUM:
      printf ("\tmanuf:%d, impl:%d, mask:%d, maxtl:%d, maxwin:%d",
	      BITS (48, 0xffff), BITS (32, 0xffff),
	      BITS (24, 0xff), BITS (8, 0xff), BITS (0, 31));
      break;
    case PSTATE_REGNUM:
      {
	static char *mm[4] =
	{"tso", "pso", "rso", "?"};
	printf ("\tcle:%d, tle:%d, mm:%s, red:%d, pef:%d, am:%d, priv:%d, ie:%d, ag:%d",
		BITS (9, 1), BITS (8, 1), mm[BITS (6, 3)], BITS (5, 1),
		BITS (4, 1), BITS (3, 1), BITS (2, 1), BITS (1, 1),
		BITS (0, 1));
	break;
      }
    case TSTATE_REGNUM:
      /* FIXME: print all 4? */
      break;
    case TT_REGNUM:
      /* FIXME: print all 4? */
      break;
    case TPC_REGNUM:
      /* FIXME: print all 4? */
      break;
    case TNPC_REGNUM:
      /* FIXME: print all 4? */
      break;
    case WSTATE_REGNUM:
      printf ("\tother:%d, normal:%d", BITS (3, 7), BITS (0, 7));
      break;
    case CWP_REGNUM:
      printf ("\t%d", BITS (0, 31));
      break;
    case CANSAVE_REGNUM:
      printf ("\t%-2d before spill", BITS (0, 31));
      break;
    case CANRESTORE_REGNUM:
      printf ("\t%-2d before fill", BITS (0, 31));
      break;
    case CLEANWIN_REGNUM:
      printf ("\t%-2d before clean", BITS (0, 31));
      break;
    case OTHERWIN_REGNUM:
      printf ("\t%d", BITS (0, 31));
      break;
#else
    case PS_REGNUM:
      printf ("\ticc:%c%c%c%c, pil:%d, s:%d, ps:%d, et:%d, cwp:%d",
	      BITS (23, 1) ? 'N' : '-', BITS (22, 1) ? 'Z' : '-',
	      BITS (21, 1) ? 'V' : '-', BITS (20, 1) ? 'C' : '-',
	      BITS (8, 15), BITS (7, 1), BITS (6, 1), BITS (5, 1),
	      BITS (0, 31));
      break;
    case FPS_REGNUM:
      {
	static char *fcc[4] =
	{"=", "<", ">", "?"};
	static char *rd[4] =
	{"N", "0", "+", "-"};
	/* Long, yes, but I'd rather leave it as is and use a wide screen.  */
	printf ("\trd:%s, tem:%d, ns:%d, ver:%d, ftt:%d, qne:%d, "
		"fcc:%s, aexc:%d, cexc:%d",
		rd[BITS (30, 3)], BITS (23, 31), BITS (22, 1), BITS (17, 7),
		BITS (14, 7), BITS (13, 1), fcc[BITS (10, 3)], BITS (5, 31),
		BITS (0, 31));
	break;
      }

#endif /* GDB_TARGET_IS_SPARC64 */
    }

#undef BITS
}

int
gdb_print_insn_sparc (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  /* It's necessary to override mach again because print_insn messes it up. */
  info->mach = TARGET_ARCHITECTURE->mach;
  return print_insn_sparc (memaddr, info);
}

/* The SPARC passes the arguments on the stack; arguments smaller
   than an int are promoted to an int.  */

CORE_ADDR
sparc_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  int i;
  int accumulate_size = 0;
  struct sparc_arg
    {
      char *contents;
      int len;
      int offset;
    };
  struct sparc_arg *sparc_args =
  (struct sparc_arg *) alloca (nargs * sizeof (struct sparc_arg));
  struct sparc_arg *m_arg;

  /* Promote arguments if necessary, and calculate their stack offsets
     and sizes. */
  for (i = 0, m_arg = sparc_args; i < nargs; i++, m_arg++)
    {
      value_ptr arg = args[i];
      struct type *arg_type = check_typedef (VALUE_TYPE (arg));
      /* Cast argument to long if necessary as the compiler does it too.  */
      switch (TYPE_CODE (arg_type))
	{
	case TYPE_CODE_INT:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_ENUM:
	  if (TYPE_LENGTH (arg_type) < TYPE_LENGTH (builtin_type_long))
	    {
	      arg_type = builtin_type_long;
	      arg = value_cast (arg_type, arg);
	    }
	  break;
	default:
	  break;
	}
      m_arg->len = TYPE_LENGTH (arg_type);
      m_arg->offset = accumulate_size;
      accumulate_size = (accumulate_size + m_arg->len + 3) & ~3;
      m_arg->contents = VALUE_CONTENTS (arg);
    }

  /* Make room for the arguments on the stack.  */
  accumulate_size += CALL_DUMMY_STACK_ADJUST;
  sp = ((sp - accumulate_size) & ~7) + CALL_DUMMY_STACK_ADJUST;

  /* `Push' arguments on the stack.  */
  for (i = nargs; m_arg--, --i >= 0;)
    write_memory (sp + m_arg->offset, m_arg->contents, m_arg->len);

  return sp;
}


/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

void
sparc_extract_return_value (type, regbuf, valbuf)
     struct type *type;
     char *regbuf;
     char *valbuf;
{
  int typelen = TYPE_LENGTH (type);
  int regsize = REGISTER_RAW_SIZE (O0_REGNUM);

  if (TYPE_CODE (type) == TYPE_CODE_FLT && SPARC_HAS_FPU)
    memcpy (valbuf, &regbuf[REGISTER_BYTE (FP0_REGNUM)], typelen);
  else
    memcpy (valbuf,
	    &regbuf[O0_REGNUM * regsize +
		    (typelen >= regsize
		     || TARGET_BYTE_ORDER == LITTLE_ENDIAN ? 0
		     : regsize - typelen)],
	    typelen);
}


/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  On SPARCs with FPUs,
   float values are returned in %f0 (and %f1).  In all other cases,
   values are returned in register %o0.  */

void
sparc_store_return_value (type, valbuf)
     struct type *type;
     char *valbuf;
{
  int regno;
  char buffer[MAX_REGISTER_RAW_SIZE];

  if (TYPE_CODE (type) == TYPE_CODE_FLT && SPARC_HAS_FPU)
    /* Floating-point values are returned in the register pair */
    /* formed by %f0 and %f1 (doubles are, anyway).  */
    regno = FP0_REGNUM;
  else
    /* Other values are returned in register %o0.  */
    regno = O0_REGNUM;

  /* Add leading zeros to the value. */
  if (TYPE_LENGTH (type) < REGISTER_RAW_SIZE (regno))
    {
      bzero (buffer, REGISTER_RAW_SIZE (regno));
      memcpy (buffer + REGISTER_RAW_SIZE (regno) - TYPE_LENGTH (type), valbuf,
	      TYPE_LENGTH (type));
      write_register_bytes (REGISTER_BYTE (regno), buffer,
			    REGISTER_RAW_SIZE (regno));
    }
  else
    write_register_bytes (REGISTER_BYTE (regno), valbuf, TYPE_LENGTH (type));
}


/* Insert the function address into a call dummy instruction sequence
   stored at DUMMY.

   For structs and unions, if the function was compiled with Sun cc,
   it expects 'unimp' after the call.  But gcc doesn't use that
   (twisted) convention.  So leave a nop there for gcc (FIX_CALL_DUMMY
   can assume it is operating on a pristine CALL_DUMMY, not one that
   has already been customized for a different function).  */

void
sparc_fix_call_dummy (dummy, pc, fun, value_type, using_gcc)
     char *dummy;
     CORE_ADDR pc;
     CORE_ADDR fun;
     struct type *value_type;
     int using_gcc;
{
  int i;
#ifdef GDB_TARGET_IS_SPARC64

  /* 
   * XXXXXXXXX
   * 
   * We'll use jmpl %g1, %o7, so we just overwrite %g1 with the
   * pointer to the function we want to call.
   */
  write_register (G0_REGNUM + 1, fun);
#else

  /* Store the relative adddress of the target function into the
     'call' instruction. */

  store_unsigned_integer (dummy + CALL_DUMMY_CALL_OFFSET, 4,
			  (0x40000000
			   | (((fun - (pc + CALL_DUMMY_CALL_OFFSET)) >> 2)
			      & 0x3fffffff)));

  /* Comply with strange Sun cc calling convention for struct-returning
     functions.  */
  if (!using_gcc
      && (TYPE_CODE (value_type) == TYPE_CODE_STRUCT
	  || TYPE_CODE (value_type) == TYPE_CODE_UNION))
    store_unsigned_integer (dummy + CALL_DUMMY_CALL_OFFSET + 8, 4,
			    TYPE_LENGTH (value_type) & 0x1fff);

#endif
#ifndef GDB_TARGET_IS_SPARC64
  /* If this is not a simulator target, change the first four instructions
     of the call dummy to NOPs.  Those instructions include a 'save'
     instruction and are designed to work around problems with register
     window flushing in the simulator. */
  if (strcmp (target_shortname, "sim") != 0)
    {
      for (i = 0; i < 4; i++)
	store_unsigned_integer (dummy + (i * 4), 4, 0x01000000);
    }
#endif

  /* If this is a bi-endian target, GDB has written the call dummy
     in little-endian order.  We must byte-swap it back to big-endian. */
  if (bi_endian)
    {
      for (i = 0; i < CALL_DUMMY_LENGTH; i += 4)
	{
	  char tmp = dummy[i];
	  dummy[i] = dummy[i + 3];
	  dummy[i + 3] = tmp;
	  tmp = dummy[i + 1];
	  dummy[i + 1] = dummy[i + 2];
	  dummy[i + 2] = tmp;
	}
    }
}


/* Set target byte order based on machine type. */

static int
sparc_target_architecture_hook (ap)
     const bfd_arch_info_type *ap;
{
  int i, j;

  if (ap->mach == bfd_mach_sparc_sparclite_le)
    {
      if (TARGET_BYTE_ORDER_SELECTABLE_P)
	{
	  target_byte_order = LITTLE_ENDIAN;
	  bi_endian = 1;
	}
      else
	{
	  warning ("This GDB does not support little endian sparclite.");
	}
    }
  else
    bi_endian = 0;
  return 1;
}


void
_initialize_sparc_tdep ()
{
  tm_print_insn = gdb_print_insn_sparc;
  tm_print_insn_info.mach = TM_PRINT_INSN_MACH;		/* Selects sparc/sparclite */
  target_architecture_hook = sparc_target_architecture_hook;
}


#ifdef GDB_TARGET_IS_SPARC64

/* Compensate for stack bias. Note that we currently don't handle mixed
   32/64 bit code. */
CORE_ADDR
sparc64_read_sp ()
{
  CORE_ADDR sp = read_register (SP_REGNUM);

  if (sp & 1)
    sp += 2047;
  else
    sp &= 0x0ffffffffL;
  return sp;
}

CORE_ADDR
sparc64_read_fp ()
{
  CORE_ADDR fp = read_register (FP_REGNUM);

  if (fp & 1)
    fp += 2047;
  else
    fp &= 0x0ffffffffL;
  return fp;
}

void
sparc64_write_sp (val)
     CORE_ADDR val;
{
  CORE_ADDR oldsp = read_register (SP_REGNUM);
  if (oldsp & 1)
    write_register (SP_REGNUM, val - 2047);
  else
    write_register (SP_REGNUM, val);
}

void
sparc64_write_fp (val)
     CORE_ADDR val;
{
  CORE_ADDR oldfp = read_register (FP_REGNUM);
  if (oldfp & 1)
    write_register (FP_REGNUM, val - 2047);
  else
    write_register (FP_REGNUM, val);
}

CORE_ADDR
sparc64_frame_address (struct frame_info *fi)
{
	return (fi->frame - 2047);
}

/* The SPARC 64 ABI passes floating-point arguments in FP0-31. They are
   also copied onto the stack in the correct places. */

CORE_ADDR
sp64_push_arguments (nargs, args, sp, struct_return, struct_retaddr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     unsigned char struct_return;
     CORE_ADDR struct_retaddr;
{
  int x;
  int regnum = 0;
  CORE_ADDR tempsp;

  sp = (sp & ~(((unsigned long) TYPE_LENGTH (builtin_type_long)) - 1UL));

  /* Figure out how much space we'll need. */
  for (x = nargs - 1; x >= 0; x--)
    {
      int len = TYPE_LENGTH (check_typedef (VALUE_TYPE (args[x])));
      value_ptr copyarg = args[x];
      int copylen = len;

      /* This code is, of course, no longer correct. */
      if (copylen < TYPE_LENGTH (builtin_type_long))
	{
	  copyarg = value_cast (builtin_type_long, copyarg);
	  copylen = TYPE_LENGTH (builtin_type_long);
	}
      sp -= copylen;
    }

  /* Round down. */
  sp = sp & ~7;
  tempsp = sp;

  /* Now write the arguments onto the stack, while writing FP arguments
     into the FP registers. */
  for (x = 0; x < nargs; x++)
    {
      int len = TYPE_LENGTH (check_typedef (VALUE_TYPE (args[x])));
      value_ptr copyarg = args[x];
      int copylen = len;

      /* This code is, of course, no longer correct. */
      if (copylen < TYPE_LENGTH (builtin_type_long))
	{
	  copyarg = value_cast (builtin_type_long, copyarg);
	  copylen = TYPE_LENGTH (builtin_type_long);
	}
      write_memory (tempsp, VALUE_CONTENTS (copyarg), copylen);
      tempsp += copylen;
      if (TYPE_CODE (VALUE_TYPE (args[x])) == TYPE_CODE_FLT && regnum < 32)
	{
	  /* This gets copied into a FP register. */
	  int nextreg = regnum + 2;
	  char *data = VALUE_CONTENTS (args[x]);
	  /* Floats go into the lower half of a FP register pair; quads
	     use 2 pairs. */

	  if (len == 16)
	    nextreg += 2;
	  else if (len == 4)
	    regnum++;

	  write_register_bytes (REGISTER_BYTE (FP0_REGNUM + regnum),
				data,
				len);
	  regnum = nextreg;
	}
    }
  return sp;
}

/* Values <= 32 bytes are returned in o0-o3 (floating-point values are
   returned in f0-f3). */
void
sparc64_extract_return_value (type, regbuf, valbuf, bitoffset)
     struct type *type;
     char *regbuf;
     char *valbuf;
     int bitoffset;
{
  int typelen = TYPE_LENGTH (type);
  int regsize = REGISTER_RAW_SIZE (O0_REGNUM);

  if (TYPE_CODE (type) == TYPE_CODE_FLT && SPARC_HAS_FPU)
    {
      memcpy (valbuf, &regbuf[REGISTER_BYTE (FP0_REGNUM)], typelen);
      return;
    }

  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
      || (TYPE_LENGTH (type) > 32))
    {
      memcpy (valbuf,
	      &regbuf[O0_REGNUM * regsize +
		      (typelen >= regsize ? 0 : regsize - typelen)],
	      typelen);
      return;
    }
  else
    {
      char *o0 = &regbuf[O0_REGNUM * regsize];
      char *f0 = &regbuf[FP0_REGNUM * regsize];
      int x;

      for (x = 0; x < TYPE_NFIELDS (type); x++)
	{
	  struct field *f = &TYPE_FIELDS (type)[x];
	  /* FIXME: We may need to handle static fields here. */
	  int whichreg = (f->loc.bitpos + bitoffset) / 32;
	  int remainder = ((f->loc.bitpos + bitoffset) % 32) / 8;
	  int where = (f->loc.bitpos + bitoffset) / 8;
	  int size = TYPE_LENGTH (f->type);
	  int typecode = TYPE_CODE (f->type);

	  if (typecode == TYPE_CODE_STRUCT)
	    {
	      sparc64_extract_return_value (f->type,
					    regbuf,
					    valbuf,
					    bitoffset + f->loc.bitpos);
	    }
	  else if (typecode == TYPE_CODE_FLT)
	    {
	      memcpy (valbuf + where, &f0[whichreg * 4] + remainder, size);
	    }
	  else
	    {
	      memcpy (valbuf + where, &o0[whichreg * 4] + remainder, size);
	    }
	}
    }
}


#endif
