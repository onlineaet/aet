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
 * base on gencodes.cc
 */

#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "gensupport.h"
#include "mtcsgen.h"
#include "../../nlib.h"



static void
gen_insn (md_rtx_info *info,char *upperPlatName)
{
  const char *name = XSTR (info->def, 0);
  int truth = maybe_eval_c_test (XSTR (info->def, 2));

  /* Don't mention instructions whose names are the null string
     or begin with '*'.  They are in the machine description just
     to be recognized.  */
  if (name[0] != 0 && name[0] != '*')
    {
      if (truth == 0)
	printf (",\n   %s_CODE_FOR_%s = CODE_FOR_nothing", upperPlatName,name);
      else
	printf (",\n  %s_CODE_FOR_%s = %d",upperPlatName, name, info->index);
    }
}

int main (int argc, const char **argv)
{
   progname = "mtcsgencodes";

   /* We need to see all the possibilities.  Elided insns may have
   direct references to CODE_FOR_xxx in C code.  */
   insn_elision = 0;

   if (!init_rtx_reader_args_cb (argc, argv, mtcs_gen_handle_arg))
      return (FATAL_EXIT_CODE);
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   const char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
   printf ("\
   /* Generated automatically by the program `mtcsgencodes'\n\
   from the machine description file `md'.  */\n\
   \n\
   #ifndef %s_INSN_CODES_H\n\
   #define %s_INSN_CODES_H\n\
   \n\
   enum %s_insn_code {\n\
   %s_CODE_FOR_nothing = 0",platNameUpper,platNameUpper,platName,platNameUpper);

   /* Read the machine description.  */

   md_rtx_info info;
   while (read_md_rtx (&info))
      switch (GET_CODE (info.def)){
         case DEFINE_INSN:
         case DEFINE_EXPAND:
            gen_insn (&info,platNameUpper);
            break;

         default:
            break;
      }

   printf ("\n};\n\
   \n\
   const unsigned int %s_NUM_INSN_CODES = %d;\n\
   #endif /* %s_INSN_CODES_H */\n", platNameUpper,get_num_insn_codes (),platNameUpper);

   if (ferror (stdout) || fflush (stdout) || fclose (stdout))
      return FATAL_EXIT_CODE;

   return SUCCESS_EXIT_CODE;
}
