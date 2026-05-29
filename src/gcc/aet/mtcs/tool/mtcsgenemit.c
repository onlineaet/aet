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
 * base on genemit.cc
 */

#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "read-md.h"
#include "gensupport.h"
#include "mtcsgen.h"
#include "aet/nlib.h"


/* Data structure for recording the patterns of insns that have CLOBBERs.
   We use this to output a function that adds these CLOBBERs to a
   previously-allocated PARALLEL expression.  */
struct clobber_pat
{
  struct clobber_ent *insns;
  rtx pattern;
  int first_clobber;
  struct clobber_pat *next;
  int has_hard_reg;
} *clobber_list;

/* Records one insn that uses the clobber list.  */
struct clobber_ent
{
  int code_number;      /* Counts only insns.  */
  struct clobber_ent *next;
};

static NPtrArray *genArray=NULL;

static void output_peephole2_scratches (rtx, NString*);

/* True for <X>_optab if that optab isn't allowed to fail.  */
static bool nofail_optabs[NUM_OPTABS];

static void print_code (RTX_CODE code, NString *file)
{
  const char *p1;
  for (p1 = GET_RTX_NAME (code); *p1; p1++)
    n_string_append_printf (file, "%c", TOUPPER (*p1));
}

static void gen_rtx_scratch (rtx x, enum rtx_code subroutine_type, NString *file)
{
  char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  if (subroutine_type == DEFINE_PEEPHOLE2){
     n_string_append_printf (file, "operand%d", XINT (x, 0));
  }else{
     n_string_append_printf (file, "gen_rtx_SCRATCH ((machine_mode)%s_%smode)", platNameUpper,
              mtcs_gen_get_mode_name/*!GET_MODE_NAME*/ (mtcs_gen_get(),GET_MODE (x)));
  }
}

/* Print a C expression to construct an RTX just like X,
   substituting any operand references appearing within.
*/
static void gen_exp (rtx x, enum rtx_code subroutine_type, char *used, md_rtx_info *info,NString *file)
{
  RTX_CODE code;
  int i;
  int len;
  const char *fmt;
  const char *sep = "";
  char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  if (x == 0){
     n_string_append (file, "NULL_RTX");
      return;
  }
  //fprintf(stderr,"mtcsgenemit.c gen_exp 00yyy mode:%d %s\n",GET_MODE (x),XSTR(x, 0));
  code = GET_CODE (x);
  switch (code){
    case MATCH_OPERAND:
    case MATCH_DUP:
      if (used){
          if (used[XINT (x, 0)]){
             n_string_append_printf (file, "copy_rtx (operand%d)", XINT (x, 0));
              return;
          }
          used[XINT (x, 0)] = 1;
     }
      n_string_append_printf (file, "operand%d", XINT (x, 0));
      return;

    case MATCH_OP_DUP:
       n_string_append (file, "gen_rtx_fmt_");
      for (i = 0; i < XVECLEN (x, 1); i++)
         n_string_append (file, "e");
      n_string_append_printf (file, " (GET_CODE (operand%d), ", XINT (x, 0));
      if (GET_MODE (x) == VOIDmode)
         n_string_append_printf (file, "GET_MODE (operand%d)", XINT (x, 0));
      else{
          //fprintf(stderr,"mtcsgenemit.c gen_exp 00 mode:%d\n",GET_MODE (x));
         n_string_append_printf (file, "(machine_mode)%s_%smode",
               platNameUpper,mtcs_gen_get_mode_name/*!GET_MODE_NAME*/ (mtcs_gen_get(),GET_MODE (x)));
      }
      for (i = 0; i < XVECLEN (x, 1); i++){
         n_string_append(file, ",\n\t\t");
          gen_exp (XVECEXP (x, 1, i), subroutine_type, used, info, file);
     }
      n_string_append (file, ")");
      return;

    case MATCH_OPERATOR:
       n_string_append (file, "gen_rtx_fmt_");
      for (i = 0; i < XVECLEN (x, 2); i++)
         n_string_append (file, "e");
      n_string_append_printf (file, " (GET_CODE (operand%d)", XINT (x, 0));
      //fprintf(stderr,"mtcsgenemit.c gen_exp 11 mode:%d\n",GET_MODE (x));
      n_string_append_printf (file, ", (machine_mode)%s_%smode",
            platNameUpper,mtcs_gen_get_mode_name/*!GET_MODE_NAME*/ (mtcs_gen_get(),GET_MODE (x)));
      for (i = 0; i < XVECLEN (x, 2); i++){
         n_string_append (file, ",\n\t\t");
          //fprintf(stderr,"mtcsgenemit.c gen_exp 77xx mode:%d\n",GET_MODE (XVECEXP (x, 2, i)));

          gen_exp (XVECEXP (x, 2, i), subroutine_type, used, info, file);
     }
      n_string_append (file, ")");
      return;

    case MATCH_PARALLEL:
    case MATCH_PAR_DUP:
       n_string_append_printf (file, "operand%d", XINT (x, 0));
      return;

    case MATCH_SCRATCH:
      gen_rtx_scratch (x, subroutine_type, file);
      return;

    case PC:
       n_string_append (file, "pc_rtx");
      return;
    case RETURN:
       n_string_append (file, "ret_rtx");
      return;
    case SIMPLE_RETURN:
       n_string_append (file, "simple_return_rtx");
      return;
    case CLOBBER:
      if (REG_P (XEXP (x, 0))){
          //fprintf(stderr,"mtcsgenemit.c gen_exp 22 mode:%d\n",GET_MODE (XEXP (x, 0)));

         n_string_append_printf (file, "gen_hard_reg_clobber ((machine_mode)%s_%smode, %i)",
                  platNameUpper, mtcs_gen_get_mode_name/*!GET_MODE_NAME*/ (mtcs_gen_get(),GET_MODE (XEXP (x, 0))),
              REGNO (XEXP (x, 0)));
          return;
     }
      break;

    case CONST_INT:
      if (INTVAL (x) == 0)
         n_string_append (file, "const0_rtx");
      else if (INTVAL (x) == 1)
         n_string_append (file, "const1_rtx");
      else if (INTVAL (x) == -1)
         n_string_append (file, "constm1_rtx");
      else if (-MAX_SAVED_CONST_INT <= INTVAL (x)  && INTVAL (x) <= MAX_SAVED_CONST_INT)
         n_string_append_printf (file, "const_int_rtx[MAX_SAVED_CONST_INT + (%d)]",(int) INTVAL (x));
      else if (INTVAL (x) == STORE_FLAG_VALUE)
         n_string_append (file, "const_true_rtx");
      else{
         //n_string_append (file, "GEN_INT (");
         n_string_append (file, "mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsTarget->mtcsRTL,");
         n_string_append_printf (file, HOST_WIDE_INT_PRINT_DEC_C, INTVAL (x));
         n_string_append (file, ")");
     }
      return;

    case CONST_DOUBLE:
      /* Handle `const_double_zero' rtx.  */
      if (CONST_DOUBLE_REAL_VALUE (x)->cl == rvc_zero){
       //  fprint(stderr,"mtcsgenemit.c gen_exp 33 mode:%d\n",GET_MODE (x));

         n_string_append_printf (file, "CONST_DOUBLE_ATOF (\"0\", (machine_mode)%s_%smode)",
                  platNameUpper,mtcs_gen_get_mode_name/*!GET_MODE_NAME*/(mtcs_gen_get(),GET_MODE (x)));
          return;
     }
      /* Fall through.  */
    case CONST_FIXED:
    case CONST_WIDE_INT:
      /* These shouldn't be written in MD files.  Instead, the appropriate
    routines in varasm.cc should be called.  */
      gcc_unreachable ();

    default:
      break;
  }

  n_string_append (file, "gen_rtx_");
  print_code (code, file);
  n_string_append (file, " (");
  if (!always_void_p (code)){
     n_string_append_printf (file, "(machine_mode)%s_%smode",
           platNameUpper,mtcs_gen_get_mode_name/*!GET_MODE_NAME*/(mtcs_gen_get(),GET_MODE (x)));
      sep = ",\n\t";
  }

  fmt = GET_RTX_FORMAT (code);
  len = GET_RTX_LENGTH (code);
  for (i = 0; i < len; i++){
      if (fmt[i] == '0')
          break;
      n_string_append (file,sep);
      switch (fmt[i]){
        case 'e':
        case 'u':
          gen_exp (XEXP (x, i), subroutine_type, used, info, file);
          break;

        case 'i':
           n_string_append_printf (file, "%u", XINT (x, i));
          break;

        case 'r':
           n_string_append_printf (file, "%u", REGNO (x));
          break;

        case 'p':
          /* We don't have a way of parsing polynomial offsets yet,
             and hopefully never will.  */
           n_string_append_printf (file, "%d", SUBREG_BYTE (x).to_constant ());
          break;

        case 's':
           n_string_append_printf (file, "\"%s\"", XSTR (x, i));
          break;

        case 'E':
          {
            int j;
            n_string_append_printf (file, "gen_rtvec (%d", XVECLEN (x, i));
            for (j = 0; j < XVECLEN (x, i); j++){
               n_string_append (file, ",\n\t\t");
                //fprintf(stderr,"mtcsgenemit.c gen_exp 99xx mode:%d\n",GET_MODE (XVECEXP (x, i, j)));

                gen_exp (XVECEXP (x, i, j), subroutine_type, used, info, file);
            }
            n_string_append (file, ")");
            break;
          }

        default:
          gcc_unreachable ();
      }
      sep = ",\n\t";
  }
  n_string_append (file, ")");
}


//zclei 原型 get_emit_function gensupport.h gensupport.cc
static const char * getEmitFunction (rtx x)
{
  switch (classify_insn (x))
    {
    case INSN:
      return "mtcs_emit_emit_insn";

    case CALL_INSN:
      return "mtcs_emit_emit_call_insn";

    case JUMP_INSN:
      return "mtcs_emit_emit_jump_insn";

    case UNKNOWN:
      return NULL;

    default:
      gcc_unreachable ();
    }
}

/* Output code to emit the instruction patterns in VEC, with each element
   becoming a separate instruction.  USED is as for gen_exp.  */
static void gen_emit_seq (rtvec vec, char *used, md_rtx_info *info, NString *file)
{
  for (int i = 0, len = GET_NUM_ELEM (vec); i < len; ++i){
      bool last_p = (i == len - 1);
      rtx next = RTVEC_ELT (vec, i);
      if (const char *name = getEmitFunction/*!get_emit_function*/ (next)){
          n_string_append_printf (file, "  %s (mtcsTarget->mtcsEmit,", name);
          //fprintf(stderr,"mtcsgenemit.c gen_emit_seq 00 调用 gen_exp\n");
          gen_exp (next, DEFINE_EXPAND, used, info, file);
          n_string_append (file, ");\n");
          if (!last_p && needs_barrier_p (next))
              //fprintf (file, "  emit_barrier ();");
             n_string_append (file, "  mtcs_emit_emit_barrier (mtcsTarget->mtcsEmit);");

     }else{
          //fprintf (file, "  emit (");
        n_string_append (file, "  mtcs_emit_emit (mtcsTarget->mtcsEmit,");
         // fprintf(stderr,"mtcsgenemit.c gen_emit_seq 11 调用 gen_exp\n");
          gen_exp (next, DEFINE_EXPAND, used, info, file);
          n_string_append_printf (file, ", %s);\n", last_p ? "false" : "true");
     }
  }
}


/* Emit the given C code to the output file.  The code is allowed to
   fail if CAN_FAIL_P.  NAME describes what we're generating,
   for use in error messages.
*/
static void emit_c_code (const char *code, bool can_fail_p, const char *name, NString *file)
{
  if (can_fail_p)
     n_string_append (file, "#define FAIL return (mtcs_emit_end_sequence (mtcsTarget->mtcsEmit), _val)\n");
  else
     n_string_append_printf (file, "#define FAIL _Pragma (\"GCC error \\\"%s cannot FAIL\\\"\")"
       " (void)0\n", name);
  n_string_append (file, "#define DONE return (_val = mtcs_rtl_data_get_insns (mtcsTarget->mtcsFunc->mtcsRtlData), "
     "mtcs_emit_end_sequence (mtcsTarget->mtcsEmit), _val)\n");
  //rtx_reader_ptr->print_md_ptr_loc (code, file);
  const struct md_reader::ptr_loc *loc = rtx_reader_ptr->get_md_ptr_loc ((const void *)code);
  if (loc != 0)
     n_string_append_printf (file, "#line %d \"%s\"\n", loc->loc.lineno, loc->loc.filename);
  n_string_append_printf (file, "%s\n",code);
  n_string_append (file, "#undef DONE\n");
  n_string_append (file, "#undef FAIL\n");
}

/*
 *  Generate the `gen_...' function for a DEFINE_INSN.
 */
static void gen_insn (md_rtx_info *info, NString *file)
{
  struct pattern_stats stats;
  int i;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  /* See if the pattern for this insn ends with a group of CLOBBERs of (hard)
     registers or MATCH_SCRATCHes.  If so, store away the information for
     later.  */
  rtx insn = info->def;
  if (XVEC (insn, 1)){
      int has_hard_reg = 0;
      for (i = XVECLEN (insn, 1) - 1; i > 0; i--){
          if (GET_CODE (XVECEXP (insn, 1, i)) != CLOBBER)
            break;
          if (REG_P (XEXP (XVECEXP (insn, 1, i), 0)))
            has_hard_reg = 1;
          else if (GET_CODE (XEXP (XVECEXP (insn, 1, i), 0)) != MATCH_SCRATCH)
            break;
     }
      if (i != XVECLEN (insn, 1) - 1){
          struct clobber_pat *p;
          struct clobber_ent *link = XNEW (struct clobber_ent);
          int j;
          link->code_number = info->index;
          /* See if any previous CLOBBER_LIST entry is the same as this
             one.  */
          for (p = clobber_list; p; p = p->next){
              if (p->first_clobber != i + 1 || XVECLEN (p->pattern, 1) != XVECLEN (insn, 1))
                  continue;
              for (j = i + 1; j < XVECLEN (insn, 1); j++){
                  rtx old_rtx = XEXP (XVECEXP (p->pattern, 1, j), 0);
                  rtx new_rtx = XEXP (XVECEXP (insn, 1, j), 0);
                      /* OLD and NEW_INSN are the same if both are to be a SCRATCH
                     of the same mode,
                     or if both are registers of the same mode and number.  */
                  if (! (GET_CODE (old_rtx) == GET_CODE (new_rtx) && GET_MODE (old_rtx) == GET_MODE (new_rtx)
                     && ((GET_CODE (old_rtx) == MATCH_SCRATCH  && GET_CODE (new_rtx) == MATCH_SCRATCH)
                         || (REG_P (old_rtx) && REG_P (new_rtx) && REGNO (old_rtx) == REGNO (new_rtx)))))
                    break;
              }
              if (j == XVECLEN (insn, 1))
                  break;
          }

          if (p == 0){
              p = XNEW (struct clobber_pat);
              p->insns = 0;
              p->pattern = insn;
              p->first_clobber = i + 1;
              p->next = clobber_list;
              p->has_hard_reg = has_hard_reg;
              clobber_list = p;
          }
          link->next = p->insns;
          p->insns = link;
      }
  }
  /* Don't mention instructions whose names are the null string
     or begin with '*'.  They are in the machine description just
     to be recognized.  */
  if (XSTR (insn, 0)[0] == 0 || XSTR (insn, 0)[0] == '*')
    return;

  n_string_append_printf(file, "/* %s:%d */\n", info->loc.filename, info->loc.lineno);
  /* Find out how many operands this function has.  */
  get_pattern_stats (&stats, XVEC (insn, 1));
  if (stats.max_dup_opno > stats.max_opno)
    fatal_at (info->loc, "match_dup operand number has no match_operand");
  n_ptr_array_add(genArray,XSTR (insn, 0));//movbi、load_arg_regqi...
  /* Output the function name and argument declarations.  */
  n_string_append_printf (file, "rtx\n%s_gen_%s (", platName,XSTR (insn, 0));//zclei
  if (stats.num_generator_args)
    for (i = 0; i < stats.num_generator_args; i++)
      if (i)
         n_string_append_printf (file, ",\n\trtx operand%d ATTRIBUTE_UNUSED", i);
      else
         n_string_append_printf (file, "rtx operand%d ATTRIBUTE_UNUSED", i);
      else
         n_string_append (file, "void");
  n_string_append (file, ")\n");
  n_string_append (file, "{\n");
  /* Output code to construct and return the rtl for the instruction body.  */

  rtx pattern = add_implicit_parallel (XVEC (insn, 1));
  /* ??? This is the traditional behavior, but seems suspect.  */
  char *used = (XVECLEN (insn, 1) == 1 ? NULL: XCNEWVEC (char, stats.num_generator_args));
  n_string_append (file, "  return ");
  //fprintf(stderr,"mtcsgenemit.c gen_insn 00 调用 gen_exp %s\n",XSTR (insn, 0));
  gen_exp (pattern, DEFINE_INSN, used, info, file);
  n_string_append (file, ";\n}\n\n");
  XDELETEVEC (used);
}

/* Generate the `gen_...' function for a DEFINE_EXPAND.
 * */
static void gen_expand (md_rtx_info *info, NString *file)
{
  struct pattern_stats stats;
  int i;
  char *used;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  rtx expand = info->def;
  if (strlen (XSTR (expand, 0)) == 0)
    fatal_at (info->loc, "define_expand lacks a name");
  if (XVEC (expand, 1) == 0)
    fatal_at (info->loc, "define_expand for %s lacks a pattern",
         XSTR (expand, 0));

  /* Find out how many operands this function has.  */
  get_pattern_stats (&stats, XVEC (expand, 1));
  if (stats.min_scratch_opno != -1
      && stats.min_scratch_opno <= MAX (stats.max_opno, stats.max_dup_opno))
    fatal_at (info->loc, "define_expand for %s needs to have match_scratch "
          "numbers above all other operands", XSTR (expand, 0));
 //fprintf(stderr,"mtcsgenemit.c ----gen_expand 00---- %s\n",XSTR (expand, 0));
 //fprintf(stderr,"mtcsgenemit.c ----gen_expand 11 ---- %s\n",XSTR (expand, 3));

  n_ptr_array_add(genArray,XSTR (expand, 0));//movbi、load_arg_regqi...

  /* Output the function name and argument declarations.  */
  n_string_append_printf (file, "rtx\n%s_gen_%s (",platName,XSTR (expand, 0));//zclei
  if (stats.num_generator_args)
    for (i = 0; i < stats.num_generator_args; i++)
      if (i)
         n_string_append_printf (file, ",\n\trtx operand%d", i);
      else
         n_string_append_printf (file, "rtx operand%d", i);
  else
     n_string_append (file, "void");
  n_string_append (file, ")\n");
  n_string_append (file, "{\n");

  /* If we don't have any C code to write, only one insn is being written,
     and no MATCH_DUPs are present, we can just return the desired insn
     like we do for a DEFINE_INSN.  This saves memory.  */
  if ((XSTR (expand, 3) == 0 || *XSTR (expand, 3) == '\0')
      && stats.max_opno >= stats.max_dup_opno
      && XVECLEN (expand, 1) == 1)
    {
     n_string_append (file, "  return ");
      gen_exp (XVECEXP (expand, 1, 0), DEFINE_EXPAND, NULL, info, file);
      n_string_append (file, ";\n}\n\n");
      return;
    }

  /* For each operand referred to only with MATCH_DUPs,
     make a local variable.  */
  for (i = stats.num_generator_args; i <= stats.max_dup_opno; i++)
     n_string_append_printf (file, "  rtx operand%d;\n", i);
  n_string_append (file, "  rtx_insn *_val = 0;\n");
  n_string_append (file, "  mtcs_emit_start_sequence(mtcsTarget->mtcsEmit);\n");

  /* The fourth operand of DEFINE_EXPAND is some code to be executed
     before the actual construction.
     This code expects to refer to `operands'
     just as the output-code in a DEFINE_INSN does,
     but here `operands' is an automatic array.
     So copy the operand values there before executing it.  */
  if (XSTR (expand, 3) && *XSTR (expand, 3)){
     n_string_append (file, "  {\n");
      if (stats.num_operand_vars > 0)
         n_string_append_printf (file, "    rtx operands[%d];\n", stats.num_operand_vars);
      /* Output code to copy the arguments into `operands'.  */
      for (i = 0; i < stats.num_generator_args; i++)
         n_string_append_printf (file, "    operands[%d] = operand%d;\n", i, i);
      /* Output the special code to be executed before the sequence
    is generated.  */
      optab_pattern p;
      bool can_fail_p = true;
      if (mtcs_gen_find_optab/*!find_optab*/ (mtcs_gen_get(),((void*)&p), XSTR (expand, 0))){
          gcc_assert (p.op < NUM_OPTABS);
          if (nofail_optabs[p.op])
            can_fail_p = false;
     }
      emit_c_code (XSTR (expand, 3), can_fail_p, XSTR (expand, 0), file);
      /* Output code to copy the arguments back out of `operands'
    (unless we aren't going to use them at all).  */
      if (XVEC (expand, 1) != 0){
          for (i = 0; i <= MAX (stats.max_opno, stats.max_dup_opno); i++){
             n_string_append_printf (file, "    operand%d = operands[%d];\n", i, i);
             n_string_append_printf (file, "    (void) operand%d;\n", i);
          }
     }
      n_string_append (file, "  }\n");
  }
  used = XCNEWVEC (char, stats.num_operand_vars);
  gen_emit_seq (XVEC (expand, 1), used, info, file);
  XDELETEVEC (used);
  /* Call `get_insns' to extract the list of all the
     insns emitted within this gen_... function.  */
  n_string_append (file, "  _val = mtcs_rtl_data_get_insns (mtcsTarget->mtcsFunc->mtcsRtlData);\n");
  n_string_append (file, "  mtcs_emit_end_sequence (mtcsTarget->mtcsEmit);\n");
  n_string_append (file, "  return _val;\n}\n\n");
}

/* Like gen_expand, but generates insns resulting from splitting SPLIT.
 * */
static void gen_split (md_rtx_info *info, NString *file)
{
  struct pattern_stats stats;
  int i;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  rtx split = info->def;
  const char *const name =
    ((GET_CODE (split) == DEFINE_PEEPHOLE2) ? "peephole2" : "split");
  const char *unused;
  char *used;

  if (XVEC (split, 0) == 0)
    fatal_at (info->loc, "%s lacks a pattern", GET_RTX_NAME (GET_CODE (split)));
  else if (XVEC (split, 2) == 0)
    fatal_at (info->loc, "%s lacks a replacement pattern", GET_RTX_NAME (GET_CODE (split)));

  /* Find out how many operands this function has.  */
  get_pattern_stats (&stats, XVEC (split, 2));
  unused = (stats.num_operand_vars == 0 ? " ATTRIBUTE_UNUSED" : "");
  used = XCNEWVEC (char, stats.num_operand_vars);

  /* Output the prototype, function name and argument declarations.  */
  if (GET_CODE (split) == DEFINE_PEEPHOLE2){
     n_string_append_printf (file, "extern rtx_insn *%s_gen_%s_%d (rtx_insn *, rtx *);\n",platName,name, info->index);
     n_string_append_printf (file, "rtx_insn *\n%s_gen_%s_%d (rtx_insn *curr_insn ATTRIBUTE_UNUSED,"
         " rtx *operands%s)\n",platName,name, info->index, unused);
  }else{
     n_string_append_printf (file, "extern rtx_insn *%s_gen_split_%d (rtx_insn *, rtx *);\n",platName,info->index);//zclei
     n_string_append_printf (file, "rtx_insn *\n%s_gen_split_%d "
         "(rtx_insn *curr_insn ATTRIBUTE_UNUSED, rtx *operands%s)\n", platName,info->index, unused);//zclei
  }
  n_string_append (file, "{\n");

  /* Declare all local variables.  */
  for (i = 0; i < stats.num_operand_vars; i++)
     n_string_append_printf (file, "  rtx operand%d;\n", i);
  n_string_append (file, "  rtx_insn *_val = NULL;\n");

  if (GET_CODE (split) == DEFINE_PEEPHOLE2)
    output_peephole2_scratches (split, file);

  const char *fn = info->loc.filename;
  for (const char *p = fn; *p; p++)
    if (*p == '/')
      fn = p + 1;

  n_string_append (file, "  if (dump_file)\n");
  n_string_append_printf (file, "    fprintf (dump_file, \"Splitting with gen_%s_%d (%s:%d)\\n\");\n",
     name, info->index, fn, info->loc.lineno);

  n_string_append (file, "  mtcs_emit_start_sequence(mtcsTarget->mtcsEmit);\n");
  /* The fourth operand of DEFINE_SPLIT is some code to be executed
     before the actual construction.  */
  if (XSTR (split, 3))
    emit_c_code (XSTR (split, 3), true, name, file);
  /* Output code to copy the arguments back out of `operands'  */
  for (i = 0; i < stats.num_operand_vars; i++){
     n_string_append_printf (file, "  operand%d = operands[%d];\n", i, i);
     n_string_append_printf (file, "  (void) operand%d;\n", i);
  }
  gen_emit_seq (XVEC (split, 2), used, info, file);
  /* Call `get_insns' to make a list of all the
     insns emitted within this gen_... function.  */
  n_string_append (file, "  _val = mtcs_rtl_data_get_insns (mtcsTarget->mtcsFunc->mtcsRtlData);\n");
  n_string_append (file, "  mtcs_emit_end_sequence (mtcsTarget->mtcsEmit);\n");
  n_string_append (file, "  return _val;\n}\n\n");
  free (used);
}

/* Write a function, `add_clobbers', that is given a PARALLEL of sufficient
   size for the insn and an INSN_CODE, and inserts the required CLOBBERs at
   the end of the vector.
   */
static void output_add_clobbers (md_rtx_info *info, NString *file)
{
  struct clobber_pat *clobber;
  struct clobber_ent *ent;
  int i;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  n_string_append_printf (file, "\n\nvoid\n%s_add_clobbers (rtx pattern ATTRIBUTE_UNUSED, int insn_code_number)\n",platName);
  n_string_append (file, "{\n");
  n_string_append (file, "  switch (insn_code_number)\n");
  n_string_append (file, "    {\n");
  for (clobber = clobber_list; clobber; clobber = clobber->next){
      for (ent = clobber->insns; ent; ent = ent->next)
         n_string_append_printf (file, "    case %d:\n", ent->code_number);
      for (i = clobber->first_clobber; i < XVECLEN (clobber->pattern, 1); i++){
         n_string_append_printf (file, "      XVECEXP (pattern, 0, %d) = ", i);
          fprintf(stderr,"mtcsgenemit.c output_add_clobbers 调用 gen_exp\n");
          gen_exp (XVECEXP (clobber->pattern, 1, i),GET_CODE (clobber->pattern), NULL, info, file);
          n_string_append (file, ";\n");
     }
      n_string_append (file, "      break;\n\n");
  }
  n_string_append (file, "    default:\n");
  n_string_append (file, "      gcc_unreachable ();\n");
  n_string_append (file, "    }\n");
  n_string_append (file, "}\n");
}

/* Write a function, `added_clobbers_hard_reg_p' that is given an insn_code
   number that will have clobbers added (as indicated by `recog') and returns
   1 if those include a clobber of a hard reg or 0 if all of them just clobber
   SCRATCH.  */
static void output_added_clobbers_hard_reg_p (NString *file)
{
  struct clobber_pat *clobber;
  struct clobber_ent *ent;
  int clobber_p;
  bool used;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  n_string_append_printf (file, "\n\nbool\n%s_added_clobbers_hard_reg_p (int insn_code_number)\n",platName);//zclei
  n_string_append (file, "{\n");
  n_string_append (file, "  switch (insn_code_number)\n");
  n_string_append (file, "    {\n");
  for (clobber_p = 0; clobber_p <= 1; clobber_p++){
      used = false;
      for (clobber = clobber_list; clobber; clobber = clobber->next)
        if (clobber->has_hard_reg == clobber_p)
          for (ent = clobber->insns; ent; ent = ent->next){
             n_string_append_printf (file, "    case %d:\n", ent->code_number);
              used = true;
          }

      if (used)
         n_string_append_printf (file, "      return %s;\n\n", clobber_p ? "true" : "false");
  }
  n_string_append (file, "    default:\n");
  n_string_append (file, "      gcc_unreachable ();\n");
  n_string_append (file, "    }\n");
  n_string_append (file, "}\n");
}

/* Generate code to invoke find_free_register () as needed for the
   scratch registers used by the peephole2 pattern in SPLIT.
   */
static void output_peephole2_scratches (rtx split, NString *file)
{
  int i;
  int insn_nr = 0;
  bool first = true;
  char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  for (i = 0; i < XVECLEN (split, 0); i++) {
      rtx elt = XVECEXP (split, 0, i);
      if (GET_CODE (elt) == MATCH_SCRATCH){
          int last_insn_nr = insn_nr;
          int cur_insn_nr = insn_nr;
          int j;
          for (j = i + 1; j < XVECLEN (split, 0); j++)
            if (GET_CODE (XVECEXP (split, 0, j)) == MATCH_DUP){
                if (XINT (XVECEXP (split, 0, j), 0) == XINT (elt, 0))
                  last_insn_nr = cur_insn_nr;
            } else if (GET_CODE (XVECEXP (split, 0, j)) != MATCH_SCRATCH)
              cur_insn_nr++;

          if (first){
             n_string_append (file, "  HardRegSet _regs_allocated;\n");
             n_string_append (file, "  mtcs_reg_clear_hard_reg_set (&_regs_allocated);\n");
              first = false;
          }

          n_string_append_printf (file, "  if ((operands[%d] = peep2_find_free_register (%d, %d, \"%s\", (machine_mode)%s_%smode, &_regs_allocated)) == NULL_RTX)\n\
        return NULL;\n",
              XINT (elt, 0),
              insn_nr, last_insn_nr,
              XSTR (elt, 1),
              platNameUpper,platNameUpper,mtcs_gen_get_mode_name/*!GET_MODE_NAME*/ (mtcs_gen_get(),GET_MODE (elt)));

     }else if (GET_CODE (elt) != MATCH_DUP)
         insn_nr++;
  }
}

/* Print "arg<N>" parameter declarations for each argument N of ONAME.  */
static void print_overload_arguments (overloaded_name *oname, NString *file)
{
  for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
     n_string_append_printf (file, "%s%s arg%d", i == 0 ? "" : ", ", oname->arg_types[i], i);
}

/* Print code to test whether INSTANCE should be chosen, given that
   argument N of the overload is available as "arg<N>".  */
static void print_overload_test (overloaded_instance *instance, NString *file)
{
  for (unsigned int i = 0; i < instance->arg_values.length (); ++i){
    char deviceModeName[30];
    int ret=mtcs_gen_convert_host_mode_name(mtcs_gen_get(),instance->arg_values[i],deviceModeName);

    //if(ret!=0)
    n_string_append_printf (file, "%sarg%d == %s", i == 0 ? "  if (" : "\n      && ", i, instance->arg_values[i]);
    //else
      // fprintf (file, "%sarg%d == %s", i == 0 ? "  if (" : "\n      && ", i, deviceModeName);//zclei
  }
  n_string_append (file, ")\n");
}
/* Emit a maybe_code_for_* function for ONAME.  */
static void handle_overloaded_code_for (overloaded_name *oname, NString *file)
{
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  const char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  /* Print the function prototype.  */
  n_string_append_printf (file, "\ninsn_code\n%s_maybe_code_for_%s (",platName, oname->name);//zclei
  print_overload_arguments (oname, file);
  n_string_append (file, ")\n{\n");
  /* Use a sequence of "if" statements for each instance.  */
  for (overloaded_instance *instance = oname->first_instance; instance; instance = instance->next){
      print_overload_test (instance, file);
      n_string_append_printf (file, "    return (enum insn_code)%s_CODE_FOR_%s;\n", platNameUpper,instance->name);//PTX zclei
  }
  /* Return null if no match was found.  */
  n_string_append (file, "  return CODE_FOR_nothing;\n}\n");
}

/* Emit a maybe_gen_* function for ONAME.  */
static void handle_overloaded_gen (overloaded_name *oname, NString *file)
{
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  unsigned HOST_WIDE_INT seen = 0;
  /* All patterns must have the same number of operands.  */
  for (overloaded_instance *instance = oname->first_instance->next; instance; instance = instance->next){
      pattern_stats stats;
      get_pattern_stats (&stats, XVEC (instance->insn, 1));
      unsigned HOST_WIDE_INT mask= HOST_WIDE_INT_1U << stats.num_generator_args;
      if (seen & mask)
          continue;
      seen |= mask;
      /* Print the function prototype.  */
      n_string_append_printf (file, "\nrtx\n%s_maybe_gen_%s (", platName,oname->name);//zclei
      print_overload_arguments (oname, file);
      for (int i = 0; i < stats.num_generator_args; ++i)
         n_string_append_printf (file, ", rtx x%d", i);
      n_string_append (file, ")\n{\n");
      n_string_append (file, "MtcsOutput *mtcsOutput=mtcsTarget->mtcsOutput;\n");//zclei
      /* Use maybe_code_for_*, instead of duplicating the selection
    logic here.  */
      n_string_append_printf (file, "  insn_code code = %s_maybe_code_for_%s (", platName,oname->name);//zclei
      for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
         n_string_append_printf (file, "%sarg%d", i == 0 ? "" : ", ", i);
      n_string_append_printf (file, ");\n"
         "  if (code != CODE_FOR_nothing)\n"
         "    {\n"
         "      gcc_assert (mtcsOutput->insn_data[code].n_generator_args == %d);\n"
         "      return MTCS_GEN_FCN (code) (", stats.num_generator_args);//zclei GEN_FCN改成MTCS_GEN_FCN
      for (int i = 0; i < stats.num_generator_args; ++i)
         n_string_append_printf (file, "%sx%d", i == 0 ? "" : ", ", i);
      n_string_append (file, ");\n"
         "    }\n"
         "  else\n"
         "    return NULL_RTX;\n"
         "}\n");
  }
}

void print_header (NString *file/*!FILE *file*/)
{
  n_string_append (file, "/* Generated automatically by the program `genemit'\n\
    from the machine description file `md'.  */\n\n");

  n_string_append (file, "#define IN_TARGET_CODE 1\n");
  n_string_append (file, "#include \"config.h\"\n");
  n_string_append (file, "#include \"system.h\"\n");
  n_string_append (file, "#include \"coretypes.h\"\n");
  n_string_append (file, "#include \"backend.h\"\n");
  n_string_append (file, "#include \"predict.h\"\n");
  n_string_append (file, "#include \"tree.h\"\n");
  n_string_append (file, "#include \"rtl.h\"\n");
  n_string_append (file, "#include \"alias.h\"\n");
  n_string_append (file, "#include \"varasm.h\"\n");
  n_string_append (file, "#include \"stor-layout.h\"\n");
  n_string_append (file, "#include \"calls.h\"\n");
  n_string_append (file, "#include \"memmodel.h\"\n");
  n_string_append (file, "#include \"tm_p.h\"\n");
  n_string_append (file, "#include \"flags.h\"\n");
  n_string_append (file, "#include \"insn-config.h\"\n");
  n_string_append (file, "#include \"expmed.h\"\n");
  n_string_append (file, "#include \"dojump.h\"\n");
  n_string_append (file, "#include \"explow.h\"\n");
  n_string_append (file, "#include \"emit-rtl.h\"\n");
  n_string_append (file, "#include \"stmt.h\"\n");
  n_string_append (file, "#include \"expr.h\"\n");
  n_string_append (file, "#include \"insn-codes.h\"\n");
  n_string_append (file, "#include \"optabs.h\"\n");
  n_string_append (file, "#include \"dfp.h\"\n");
  n_string_append (file, "#include \"output.h\"\n");
  n_string_append (file, "#include \"recog.h\"\n");
  n_string_append (file, "#include \"df.h\"\n");
  n_string_append (file, "#include \"resource.h\"\n");
  n_string_append (file, "#include \"reload.h\"\n");
  n_string_append (file, "#include \"diagnostic-core.h\"\n");
  n_string_append (file, "#include \"regs.h\"\n");
  n_string_append (file, "#include \"ggc.h\"\n");
  n_string_append (file, "#include \"target.h\"\n\n");
  if(mtcs_gen_is_ptx(mtcs_gen_get())){ //zclei
     n_string_append (file, "#include \"../ptx-common.h\"\n");
     n_string_append (file, "#include \"ptx-insn-modes.h\"\n");
     n_string_append (file, "#include \"ptx-insn-opinit.h\"\n");
     n_string_append (file, "#include \"ptx-insn-codes.h\"\n");
     n_string_append (file, "#include \"ptx-insn-flags.h\"\n");
     n_string_append (file, "#include \"ptx-optionsitem.h\"\n");
     n_string_append (file, "#include \"../mtcsptxemit.h\"\n");
     n_string_append (file, "#include \"../mtcsptxoutput.h\"\n");
     n_string_append (file, "#include \"../mtcsptxcodes.h\"\n");
     n_string_append (file, "#include \"../mtcsptxfunc.h\"\n");
     n_string_append (file, "#include \"../mtcsptxmode.h\"\n");
     n_string_append (file, "#include \"../mtcsptxrecog.h\"\n");
     n_string_append (file, "#include \"../mtcsptxbuiltins.h\"\n");
  }
  n_string_append (file, "#include \"../../mtcstarget.h\"\n");
  n_string_append (file, "#include \"../../mtcsoptions.h\"\n");
  n_string_append (file, "#include \"../../mtcsoutput.h\"\n");
}

static void gen_insn_gen (md_rtx_info *info)
{
   rtx insn = info->def;
   const char *name = XSTR (insn, 0);
   const char *p;
   const char *lt, *gt;
   int len;
   int truth = maybe_eval_c_test (XSTR (insn, 2));

   lt = strchr (name, '<');
   if (lt && strchr (lt + 1, '>')){
      error_at (info->loc, "unresolved iterator in %s", name);
      return;
   }

   gt = strchr (name, '>');
   if (lt || gt){
      error_at (info->loc, "unmatched angle brackets, likely "
      "an error in iterator syntax in %s", name);
      return;
   }
   /* Don't mention instructions whose names are the null string
   or begin with '*'.  They are in the machine description just
   to be recognized.  */
   if (name[0] == 0 || name[0] == '*')
      return;
   if (truth == 0)
     /* Emit nothing.  */;
   else
      fprintf(stderr,"emit gen is :%s\n",name);
}


static void getGenFunc()
{

   //insn_elision = 0;
   /* Read the machine description.  */
   md_rtx_info info;
   fprintf(stderr,"emit getGenFunc start\n");

   while (read_md_rtx (&info))
      switch (GET_CODE (info.def)){
         case DEFINE_INSN:
         case DEFINE_EXPAND:
            gen_insn_gen (&info);
            break;

         default:
            break;
      }
}

static void replaceEmitAndGen(NString *src)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());

   char *patterns[6][2]={
          {" emit_insn ("," mtcs_emit_emit_insn/*!emit_insn*/(mtcsTarget->mtcsEmit,"},
          {" emit_move_insn ("," mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsTarget->mtcsExpr,"},
          {" emit_jump_insn ("," mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsTarget->mtcsEmit,"},
          {" gen_reg_rtx ("," mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsTarget->mtcsEmit,"},
          {" gen_rtx_REG ("," mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsTarget->mtcsRTL,"},
          {" force_reg ("," mtcs_explow_force_reg/*!force_reg*/(mtcsTarget->mtcsExplow,"},
    };
   int i;
   for(i=0;i<6;i++){
      n_string_replace(src,patterns[i][0],patterns[i][1],-1);
   }
   for(i=0;i<genArray->len;i++){
      char find[255];
      char replace[255];
      //模式 1
      sprintf(find," gen_%s (",n_ptr_array_index(genArray,i));
      sprintf(replace," %s_gen_%s(",platName,n_ptr_array_index(genArray,i));
      n_string_replace(src,find,replace,-1);
      //模式 2
      sprintf(find,",gen_%s (",n_ptr_array_index(genArray,i));
      sprintf(replace,",%s_gen_%s(",platName,n_ptr_array_index(genArray,i));
      n_string_replace(src,find,replace,-1);
      //模式 3
      sprintf(find,"(gen_%s (",n_ptr_array_index(genArray,i));
      sprintf(replace,"(%s_gen_%s(",platName,n_ptr_array_index(genArray,i));
      n_string_replace(src,find,replace,-1);
      //模式 4
      sprintf(find,",gen_%s\n",n_ptr_array_index(genArray,i));
      sprintf(replace,",%s_gen_%s\n",platName,n_ptr_array_index(genArray,i));
      n_string_replace(src,find,replace,-1);
   }

   //替换 fake_exceptions
   if(mtcs_gen_is_ptx(mtcs_gen_get()))
      n_string_replace(src,"fake_exceptions","((PtxOptionsItem *)mtcsTarget->mtcsOptions->global_options)->x_fake_exceptions",1);
}

/**
 * 替换 cfun->machine    -->  ((struct ptx_machine_function *)cfun->machine)
 * cfun->machine在md中合用。生成在 xxx-insn-emit
 */
static void replace_cfun_machine(NString *src)
{
   if(mtcs_gen_is_ptx(mtcs_gen_get())){
      static char *patterns[4][2]={
         {"cfun->machine","((struct ptx_machine_function *)cfun->machine)"},
         {"SHUFFLE_BFLY","PTX_SHUFFLE_BFLY"},
         {"SHUFFLE_IDX","PTX_SHUFFLE_IDX"},
         {"SOFTSTACK_PREV_REGNUM","PTX_SOFTSTACK_PREV_REGNUM"},
      };
      int i;
      for(i=0;i<4;i++)
         n_string_replace(src,patterns[i][0],patterns[i][1],-1);
   }
}
/**
 * nvptx-protols.h 中声明的方法，用在 xxx-insn-emit.c中方法见 char *patterns[8][2]
 * 其中 nvptx_mem_local_p insn-emit.c insn-output.c都引用。
 * extern void nvptx_expand_oacc_fork (unsigned);
extern void nvptx_expand_oacc_join (unsigned);
extern void nvptx_expand_call (rtx, rtx);
extern rtx nvptx_gen_shuffle (rtx, rtx, rtx, nvptx_shuffle_kind);
extern rtx nvptx_expand_compare (rtx);
extern const char *nvptx_ptx_type_from_mode (machine_mode, bool);//ptx-insn-emit.c中没引用
extern const char *nvptx_output_mov_insn (rtx, rtx);//insn-output.cc中引用 其它没有
extern const char *nvptx_output_call_insn (rtx_insn *, rtx, rtx);
extern const char *nvptx_output_fake_ptx_alloca (void);
extern const char *nvptx_output_return (void);
extern const char *nvptx_output_set_softstack (unsigned);
extern const char *nvptx_output_simt_enter (rtx, rtx, rtx);
extern const char *nvptx_output_simt_exit (rtx);
extern const char *nvptx_output_red_partition (rtx, rtx);
extern const char *nvptx_output_atomic_insn (const char *, rtx *, int, int);
extern bool nvptx_mem_local_p (rtx); //output emit 两个引用
extern bool nvptx_mem_maybe_shared_p (const_rtx); //emit引用
 */
static void replace_protols(NString *src)
{
   if(mtcs_gen_is_ptx(mtcs_gen_get())){
      char *patterns[8][2]={
         {"nvptx_expand_oacc_fork (","mtcs_ptx_emit_expand_oacc_fork/*!nvptx_expand_oacc_fork*/((MtcsPtxEmit *)mtcsTarget->mtcsEmit,"},
         {"nvptx_expand_oacc_join (","mtcs_ptx_emit_expand_oacc_join/*!nvptx_expand_oacc_join*/((MtcsPtxEmit *)mtcsTarget->mtcsEmit,"},
         {"nvptx_expand_call (","mtcs_ptx_emit_expand_call/*!nvptx_expand_call*/((MtcsPtxEmit *)mtcsTarget->mtcsEmit,"},
         {"nvptx_gen_shuffle (","mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/((MtcsPtxEmit *)mtcsTarget->mtcsEmit,"},
         {"nvptx_expand_compare (","mtcs_ptx_emit_expand_compare/*!nvptx_expand_compare*/((MtcsPtxEmit *)mtcsTarget->mtcsEmit,"},
         {"nvptx_ptx_type_from_mode (","mtcs_mode_get_type/*!nvptx_ptx_type_from_mode*/((MtcsMode *)mtcsTarget->mtcsMode,"},
         {"nvptx_mem_local_p (","mtcs_ptx_output_mem_local_p/*!nvptx_mem_local_p*/((MtcsPtxOutput *)mtcsTarget->mtcsOutput,"},
         {"nvptx_mem_maybe_shared_p (","mtcs_ptx_emit_mem_maybe_shared_p/*!nvptx_mem_maybe_shared_p*/((MtcsPtxEmit *)mtcsTarget->mtcsEmit,"},
      };
      int i;
      for(i=0;i<8;i++)
         n_string_replace(src,patterns[i][0],patterns[i][1],-1);
   }
}
/**
 * 替换convert_modes
 */
static void replace_methods(NString *src)
{
   if(mtcs_gen_is_ptx(mtcs_gen_get())){
      char *patterns[5][2]={
         {"convert_modes (","mtcs_expr_convert_modes/*!convert_modes*/(mtcsTarget->mtcsExpr,"},
         {"convert_memory_address (","mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsTarget->mtcsExplow,"},
         {"init_one_libfunc (","mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsTarget->mtcsLibfuncs,"},
         {"emit_library_call_value (","mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsTarget->mtcsCalls,"},
         {"real_convert (","mtcs_real_real_convert/*!real_convert*/(mtcsTarget->mtcsReal,"},
      };
      int i;
      for(i=0;i<5;i++)
         n_string_replace(src,patterns[i][0],patterns[i][1],-1);
   }
}

int main (int argc, const char **argv)
{
  progname = "mtcsgenemit";
  if (!init_rtx_reader_args_cb (argc, argv, mtcs_gen_handle_arg))
    return (FATAL_EXIT_CODE);
#define DEF_INTERNAL_OPTAB_FN(NAME, FLAGS, OPTAB, TYPE) \
  nofail_optabs[OPTAB##_optab] = true;
#include "internal-fn.def"
 // mtcs_gen_set_platform(mtcs_gen_get(),"ptx");
  mtcs_gen_append_preds(mtcs_gen_get());
  genArray=n_ptr_array_new();
  fprintf(stderr,"mtcsgenemit.c -s-----start %s %s\n",argv[1],argv[2]);
  /* Assign sequential codes to all entries in the machine description
     in parallel with the tables in insn-output.cc.  */
  int npatterns = count_patterns ();
  NString *file = n_string_sized_new(1024*1024);
  print_header (file);
  n_string_append(file,mtcs_gen_create_target_code(mtcs_gen_get(),"emit"));//zclei
  /* Read the machine description.  */
  fprintf(stderr,"emit start 00\n");
  md_rtx_info info;
  while (read_md_rtx (&info)){
      switch (GET_CODE (info.def)){
        case DEFINE_INSN:
          gen_insn (&info, file);
          break;
        case DEFINE_EXPAND:
           n_string_append_printf (file, "/* %s:%d */\n", info.loc.filename, info.loc.lineno);
          gen_expand (&info, file);
          break;
        case DEFINE_SPLIT:
           n_string_append_printf (file, "/* %s:%d */\n", info.loc.filename, info.loc.lineno);
          gen_split (&info, file);
          break;
        case DEFINE_PEEPHOLE2:
           n_string_append_printf (file, "/* %s:%d */\n", info.loc.filename, info.loc.lineno);
          gen_split (&info, file);
          break;
        default:
          break;
      }
  }
  /* Write out the routines to add CLOBBERs to a pattern and say whether they
     clobber a hard reg.  */
  output_add_clobbers (&info, file);
  output_added_clobbers_hard_reg_p (file);
  //加入 6个 平台相关的函数 nvptx_alloca set_softstack nvptx_stacksave nvptx_stackrestore omp_simt_enter omp_simt_exit
  for (overloaded_name *oname = rtx_reader_ptr->get_overloads (); oname; oname = oname->next){
      handle_overloaded_code_for (oname, file);
      handle_overloaded_gen (oname, file);
      n_ptr_array_add(genArray,oname->name);
  }
  replaceEmitAndGen(file);
  replace_cfun_machine(file);
  replace_protols(file);
  replace_methods(file);//替换 convert_modes等。
  mtcs_gen_replace_mode_for_emit(mtcs_gen_get(),file);
  printf("%s",file->str);
  return SUCCESS_EXIT_CODE;
}
