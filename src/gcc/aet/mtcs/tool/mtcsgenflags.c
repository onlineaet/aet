/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the onlineaet@163.com
 * base on genflags.cc
 */
#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "obstack.h"
#include "errors.h"
#include "read-md.h"
#include "gensupport.h"
#include "mtcsgen.h"
#include "../../nlib.h"

/* Obstack to remember insns with.  */
static struct obstack obstack;

/* Max size of names encountered.  */
static int max_id_len;

/* Max operand encountered in a scan over some insn.  */
static int max_opno;

static void max_operand_1 (rtx);
static int num_operands (rtx);
static void gen_proto (rtx);

/* Count the number of match_operand's found.  */

static void
max_operand_1 (rtx x)
{
  RTX_CODE code;
  int i;
  int len;
  const char *fmt;

  if (x == 0)
    return;

  code = GET_CODE (x);

  if (code == MATCH_OPERAND || code == MATCH_OPERATOR
      || code == MATCH_PARALLEL)
    max_opno = MAX (max_opno, XINT (x, 0));

  fmt = GET_RTX_FORMAT (code);
  len = GET_RTX_LENGTH (code);
  for (i = 0; i < len; i++)
    {
      if (fmt[i] == 'e' || fmt[i] == 'u')
	max_operand_1 (XEXP (x, i));
      else if (fmt[i] == 'E')
	{
	  int j;
	  for (j = 0; j < XVECLEN (x, i); j++)
	    max_operand_1 (XVECEXP (x, i, j));
	}
    }
}

static int
num_operands (rtx insn)
{
  int len = XVECLEN (insn, 1);
  int i;

  max_opno = -1;

  for (i = 0; i < len; i++)
    max_operand_1 (XVECEXP (insn, 1, i));

  return max_opno + 1;
}

/* Print out prototype information for a generator function.  If the
   insn pattern has been elided, print out a dummy generator that
   does nothing.  */

static void
gen_proto (rtx insn)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  int num = num_operands (insn);
  int i;
  const char *name = XSTR (insn, 0);
  int truth = maybe_eval_c_test (XSTR (insn, 2));

  if (truth != 0)
    printf ("extern rtx        %s_gen_%-*s (", platName,max_id_len, name);
  else
    printf ("static inline rtx %s_gen_%-*s (", platName,max_id_len, name);

  if (num == 0)
    fputs ("void", stdout);
  else
    {
      for (i = 1; i < num; i++)
	fputs ("rtx, ", stdout);

      fputs ("rtx", stdout);
    }

  puts (");");

  /* Some back ends want to take the address of generator functions,
     so we cannot simply use #define for these dummy definitions.  */
  if (truth == 0)
    {
      printf ("static inline rtx\n%s_gen_%s", platName,name);
      if (num > 0)
	{
	  putchar ('(');
	  for (i = 0; i < num-1; i++)
	    printf ("rtx ARG_UNUSED (%c), ", 'a' + i);
	  printf ("rtx ARG_UNUSED (%c))\n", 'a' + i);
	}
      else
	puts ("(void)");
      puts ("{\n  return 0;\n}");
    }
}

static void
gen_insn (md_rtx_info *info)
{
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  rtx insn = info->def;
  const char *name = XSTR (insn, 0);
  const char *p;
  const char *lt, *gt;
  int len;
  int truth = maybe_eval_c_test (XSTR (insn, 2));

  lt = strchr (name, '<');
  if (lt && strchr (lt + 1, '>'))
    {
      error_at (info->loc, "unresolved iterator in %s", name);
      return;
    }

  gt = strchr (name, '>');
  if (lt || gt)
    {
      error_at (info->loc, "unmatched angle brackets, likely "
		"an error in iterator syntax in %s", name);
      return;
    }

  /* Don't mention instructions whose names are the null string
     or begin with '*'.  They are in the machine description just
     to be recognized.  */
  if (name[0] == 0 || name[0] == '*')
    return;

  len = strlen (name);

  if (len > max_id_len)
    max_id_len = len;

  if (truth == 0)
    /* Emit nothing.  */;
  else if (truth == 1)
    printf ("#define %s_HAVE_%s 1\n", upperPlatName,name);
  else
    {
      /* Write the macro definition, putting \'s at the end of each line,
	 if more than one.  */
      printf ("#define %s_HAVE_%s (", upperPlatName,name);
      for (p = XSTR (insn, 2); *p; p++)
	{
	  if (IS_VSPACE (*p))
	    fputs (" \\\n", stdout);
	  else
	    putchar (*p);
	}
      fputs (")\n", stdout);
    }

  obstack_grow (&obstack, &insn, sizeof (rtx));
}


int main (int argc, const char **argv)
{
  rtx dummy;
  rtx *insns;
  rtx *insn_ptr;

  progname = "mtcsgenflags";
  obstack_init (&obstack);

  /* We need to see all the possibilities.  Elided insns may have
     direct calls to their generators in C code.  */
  insn_elision = 0;

  //原来是init_rtx_reader_args改为init_rtx_reader_args_cb，通过mtcs_gen_handle_arg可以处理mtcs的特殊参数
  if (!init_rtx_reader_args_cb(argc, argv,mtcs_gen_handle_arg))
    return (FATAL_EXIT_CODE);

  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

  puts ("/* Generated automatically by the program `mtcsgenflags'");
  puts ("   from the machine description file `md'.  */\n");
  printf("#ifndef %s_INSN_FLAGS_H\n",upperPlatName);
  printf ("#define %s_INSN_FLAGS_H\n\n",upperPlatName);

  /* Read the machine description.  */
  md_rtx_info info;
  while (read_md_rtx (&info))
    switch (GET_CODE (info.def))
      {
      case DEFINE_INSN:
      case DEFINE_EXPAND:
	gen_insn (&info);
	break;

      default:
	break;
      }

  /* Print out the prototypes now.  */
  dummy = (rtx) 0;
  obstack_grow (&obstack, &dummy, sizeof (rtx));
  insns = XOBFINISH (&obstack, rtx *);

  for (insn_ptr = insns; *insn_ptr; insn_ptr++)
    gen_proto (*insn_ptr);

  printf ("\n#endif /* %s_INSN_FLAGS_H */\n",upperPlatName);
  if(mtcs_gen_replace_mode_for_flags_file(mtcs_gen_get(),stdout)!=0){
     return FATAL_EXIT_CODE;
  }
  //if (have_error || ferror (stdout) || fflush (stdout) || fclose (stdout))
   // return FATAL_EXIT_CODE;

  return SUCCESS_EXIT_CODE;
}

