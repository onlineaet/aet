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
 * base on genattr.cc
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
#include "../../nlib.h"



static vec<rtx> const_attrs, reservations;
static NString *attrStr;
static NString *commonStr;

static void write_upcase (const char *str)
{
  for (; *str; str++)
     n_string_append_c(commonStr,TOUPPER (*str));
}


static void gen_attr (md_rtx_info *info)
{
   char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

   const char *p;
   rtx attr = info->def;
   int is_const = GET_CODE (XEXP (attr, 2)) == CONST;

   if (is_const)
      const_attrs.safe_push (attr);

   n_string_append_printf(attrStr,"#define HAVE_ATTR_%s 1\n", XSTR (attr, 0));

   /* If numeric attribute, don't need to write an enum.  */
   if (GET_CODE (attr) == DEFINE_ENUM_ATTR)
      n_string_append_printf(attrStr,"extern enum %s_%s %s_get_attr_%s (MtcsInsnAttr *self,%s);\n\n",
            platName,XSTR (attr, 1), platName,XSTR (attr, 0), (is_const ? "void" : "rtx_insn *"));
   else{
      p = XSTR (attr, 1);
      if (*p == '\0')
         n_string_append_printf(attrStr,"extern int %s_get_attr_%s (MtcsInsnAttr *self,%s);\n",
               platName,XSTR (attr, 0),(is_const ? "void" : "rtx_insn *"));
      else{
         n_string_append_printf(attrStr,"extern enum %s_attr_%s %s_get_attr_%s (MtcsInsnAttr *self,%s);\n\n",
               platName,XSTR (attr, 0), platName,XSTR (attr, 0),(is_const ? "void" : "rtx_insn *"));
         const char *tag;
         n_string_append_printf(commonStr,"enum %s_attr_%s {", platName,XSTR (attr, 0));
         int first=0;
         while ((tag = scan_comma_elt (&p)) != 0){
            if(first==0){
               n_string_append_printf(commonStr,"%s_",platUpperName);
               first=1;
            }
            write_upcase (XSTR (attr, 0));
            n_string_append_c(commonStr,'_');
            while (tag != p)
               n_string_append_c(commonStr,TOUPPER (*tag++));
            if (*p == ','){
               n_string_append(commonStr,", ");
               first=0;
            }
         }
         n_string_append(commonStr,"};\n");
      }
   }

   /* If `length' attribute, write additional function definitions and define
   variables used by `insn_current_length'.  */
   if (! strcmp (XSTR (attr, 0), "length")){
      n_string_append(attrStr,"\
      extern void shorten_branches (rtx_insn *);\n\
      extern int insn_default_length (rtx_insn *);\n\
      extern int insn_min_length (rtx_insn *);\n\
      extern int insn_variable_length_p (rtx_insn *);\n\
      extern int insn_current_length (rtx_insn *);\n\n\
      #include \"insn-addr.h\"\n");
   }
}

/* Check that attribute NAME is used in define_insn_reservation condition
   EXP.  Return true if it is.  */
static bool check_tune_attr (const char *name, rtx exp)
{
   switch (GET_CODE (exp)){
      case AND:
      if (check_tune_attr (name, XEXP (exp, 0)))
         return true;
      return check_tune_attr (name, XEXP (exp, 1));

      case IOR:
         return (check_tune_attr (name, XEXP (exp, 0))  && check_tune_attr (name, XEXP (exp, 1)));

      case EQ_ATTR:
         return strcmp (XSTR (exp, 0), name) == 0;

      default:
         return false;
   }
}

/* Try to find a const attribute (usually cpu or tune) that is used
   in all define_insn_reservation conditions.  */
static bool find_tune_attr (rtx exp)
{
   unsigned int i;
   rtx attr;

   switch (GET_CODE (exp)){
      case AND:
      case IOR:
         if (find_tune_attr (XEXP (exp, 0)))
            return true;
         return find_tune_attr (XEXP (exp, 1));

      case EQ_ATTR:
         if (strcmp (XSTR (exp, 0), "alternative") == 0)
            return false;

         FOR_EACH_VEC_ELT (const_attrs, i, attr)
         if (strcmp (XSTR (attr, 0), XSTR (exp, 0)) == 0){
            unsigned int j;
            rtx resv;

            FOR_EACH_VEC_ELT (reservations, j, resv)
               if (! check_tune_attr (XSTR (attr, 0), XEXP (resv, 2)))
               return false;
            return true;
         }
         return false;

      default:
         return false;
   }
}

/**
 * 原型 genattr.cc genattr-common.cc
 */
int main (int argc, const char **argv)
{
   bool have_annul_true = false;
   bool have_annul_false = false;
   int num_insn_reservations = 0;
   int i;
   progname = "mtcsgenattr";
   if (!init_rtx_reader_args_cb (argc, argv, mtcs_gen_handle_arg))
      return (FATAL_EXIT_CODE);


   char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
   attrStr=n_string_new("");
   commonStr=n_string_new("");

   n_string_append(commonStr,"/* Generated automatically by the program `mtcsgenattr'");
   n_string_append(commonStr,"   from the machine description file `md'.  */\n");
   n_string_append_printf(commonStr,"#ifndef __GCC_%s_INSN_ATTR_H__\n",platUpperName);
   n_string_append_printf(commonStr,"#define __GCC_%s_INSN_ATTR_H__\n\n",platUpperName);
   n_string_append_printf(commonStr,"#include \"../../mtcsinsnattr.h\"\n\n",platUpperName);

   //不再通过genattr-common.cc生成 insn-attr-common.h,直接在这里生成
   //puts ("#include \"insn-attr-common.h\"\n");

   /* Read the machine description.  */

   md_rtx_info info;
   bool have_delay = false;
   bool have_sched = false;

   while (read_md_rtx (&info)){
      rtx def = info.def;
      switch (GET_CODE (def)){
         case DEFINE_ATTR:
         case DEFINE_ENUM_ATTR:
            gen_attr (&info);
            break;

         case DEFINE_DELAY:
            have_delay = true;
            for (i = 0; i < XVECLEN (def, 1); i += 3){
               if (XVECEXP (def, 1, i + 1))
                  have_annul_true = true;

               if (XVECEXP (def, 1, i + 2))
                  have_annul_false = true;
            }
            break;

         case DEFINE_INSN_RESERVATION:
            num_insn_reservations++;
            reservations.safe_push (def);
            if (!have_sched){
               n_string_append(commonStr,"#define INSN_SCHEDULING\n");
               have_sched = true;
            }
            break;

         default:
            break;
      }
   }

   n_string_append_printf(attrStr,"extern int %s_num_delay_slots (MtcsInsnAttr *self,rtx_insn *);\n",platName);
   n_string_append_printf(attrStr,"extern int %s_eligible_for_delay (MtcsInsnAttr *self,rtx_insn *, int, rtx_insn *, int);\n\n",platName);
   n_string_append_printf(attrStr,"extern int %s_const_num_delay_slots (MtcsInsnAttr *self,rtx_insn *);\n\n",platName);
   n_string_append_printf(attrStr,"#define %s_ANNUL_IFTRUE_SLOTS %d\n", platUpperName,have_annul_true);
   n_string_append_printf(attrStr,"extern int %s_eligible_for_annul_true (rtx_insn *, int, rtx_insn *, int);\n",platName);
   n_string_append_printf(attrStr,"#define %s_ANNUL_IFFALSE_SLOTS %d\n", platUpperName,have_annul_false);
   n_string_append_printf(attrStr,"extern int %s_eligible_for_annul_false (rtx_insn *, int, rtx_insn *, int);\n",platName);

   if (num_insn_reservations > 0){
      bool has_tune_attr = find_tune_attr (XEXP (reservations[0], 2));
      /* Output interface for pipeline hazards recognition based on
      DFA (deterministic finite state automata.  */
      n_string_append(attrStr,"\n/* DFA based pipeline interface.  */");
      n_string_append(attrStr,"\n#ifndef AUTOMATON_ALTS\n");
      n_string_append(attrStr,"#define AUTOMATON_ALTS 0\n");
      n_string_append(attrStr,"#endif\n\n");
      n_string_append(attrStr,"\n#ifndef AUTOMATON_STATE_ALTS\n");
      n_string_append(attrStr,"#define AUTOMATON_STATE_ALTS 0\n");
      n_string_append(attrStr,"#endif\n\n");
      n_string_append(attrStr,"#ifndef CPU_UNITS_QUERY\n");
      n_string_append(attrStr,"#define CPU_UNITS_QUERY 0\n");
      n_string_append(attrStr,"#endif\n\n");
      /* Interface itself: */
      if (has_tune_attr){
         n_string_append(attrStr,"/* Initialize fn pointers for internal_dfa_insn_code\n");
         n_string_append(attrStr,"   and insn_default_latency.  */\n");
         n_string_append(attrStr,"extern void init_sched_attrs (void);\n\n");
         n_string_append(attrStr,"/* Internal insn code number used by automata.  */\n");
         n_string_append(attrStr,"extern int (*internal_dfa_insn_code) (rtx_insn *);\n\n");
         n_string_append(attrStr,"/* Insn latency time defined in define_insn_reservation. */\n");
         n_string_append(attrStr,"extern int (*insn_default_latency) (rtx_insn *);\n\n");
      }else{
         n_string_append(attrStr,"#define init_sched_attrs() do { } while (0)\n\n");
         n_string_append(attrStr,"/* Internal insn code number used by automata.  */\n");
         n_string_append(attrStr,"extern int internal_dfa_insn_code (rtx_insn *);\n\n");
         n_string_append(attrStr,"/* Insn latency time defined in define_insn_reservation. */\n");
         n_string_append(attrStr,"extern int insn_default_latency (rtx_insn *);\n\n");
      }
      n_string_append(attrStr,"/* Return nonzero if there is a bypass for given insn\n");
      n_string_append(attrStr,"   which is a data producer.  */\n");
      n_string_append(attrStr,"extern int bypass_p (rtx_insn *);\n\n");
      n_string_append(attrStr,"/* Insn latency time on data consumed by the 2nd insn.\n");
      n_string_append(attrStr,"   Use the function if bypass_p returns nonzero for\n");
      n_string_append(attrStr,"   the 1st insn. */\n");
      n_string_append(attrStr,"extern int insn_latency (rtx_insn *, rtx_insn *);\n\n");
      n_string_append(attrStr,"/* Maximal insn latency time possible of all bypasses for this insn.\n");
      n_string_append(attrStr,"   Use the function if bypass_p returns nonzero for\n");
      n_string_append(attrStr,"   the 1st insn. */\n");
      n_string_append(attrStr,"extern int maximal_insn_latency (rtx_insn *);\n\n");
      n_string_append(attrStr,"\n#if AUTOMATON_ALTS\n");
      n_string_append(attrStr,"/* The following function returns number of alternative\n");
      n_string_append(attrStr,"   reservations of given insn.  It may be used for better\n");
      n_string_append(attrStr,"   insns scheduling heuristics. */\n");
      n_string_append(attrStr,"extern int insn_alts (rtx);\n\n");
      n_string_append(attrStr,"#endif\n\n");
      n_string_append(attrStr,"/* Maximal possible number of insns waiting results being\n");
      n_string_append(attrStr,"   produced by insns whose execution is not finished. */\n");
      n_string_append(attrStr,"extern const int max_insn_queue_index;\n\n");
      n_string_append(attrStr,"/* Pointer to data describing current state of DFA.  */\n");
      n_string_append(attrStr,"typedef void *state_t;\n\n");
      n_string_append(attrStr,"/* Size of the data in bytes.  */\n");
      n_string_append(attrStr,"extern int state_size (void);\n\n");
      n_string_append(attrStr,"/* Initiate given DFA state, i.e. Set up the state\n");
      n_string_append(attrStr,"   as all functional units were not reserved.  */\n");
      n_string_append(attrStr,"extern void state_reset (state_t);\n");
      n_string_append(attrStr,"/* The following function returns negative value if given\n");
      n_string_append(attrStr,"   insn can be issued in processor state described by given\n");
      n_string_append(attrStr,"   DFA state.  In this case, the DFA state is changed to\n");
      n_string_append(attrStr,"   reflect the current and future reservations by given\n");
      n_string_append(attrStr,"   insn.  Otherwise the function returns minimal time\n");
      n_string_append(attrStr,"   delay to issue the insn.  This delay may be zero\n");
      n_string_append(attrStr,"   for superscalar or VLIW processors.  If the second\n");
      n_string_append(attrStr,"   parameter is NULL the function changes given DFA state\n");
      n_string_append(attrStr,"   as new processor cycle started.  */\n");
      n_string_append(attrStr,"extern int state_transition (state_t, rtx);\n");
      n_string_append(attrStr,"\n#if AUTOMATON_STATE_ALTS\n");
      n_string_append(attrStr,"/* The following function returns number of possible\n");
      n_string_append(attrStr,"   alternative reservations of given insn in given\n");
      n_string_append(attrStr,"   DFA state.  It may be used for better insns scheduling\n");
      n_string_append(attrStr,"   heuristics.  By default the function is defined if\n");
      n_string_append(attrStr,"   macro AUTOMATON_STATE_ALTS is defined because its\n");
      n_string_append(attrStr,"   implementation may require much memory.  */\n");
      n_string_append(attrStr,"extern int state_alts (state_t, rtx);\n");
      n_string_append(attrStr,"#endif\n\n");
      n_string_append(attrStr,"extern int min_issue_delay (state_t, rtx_insn *);\n");
      n_string_append(attrStr,"/* The following function returns nonzero if no one insn\n");
      n_string_append(attrStr,"   can be issued in current DFA state. */\n");
      n_string_append(attrStr,"extern int state_dead_lock_p (state_t);\n");
      n_string_append(attrStr,"/* The function returns minimal delay of issue of the 2nd\n");
      n_string_append(attrStr,"   insn after issuing the 1st insn in given DFA state.\n");
      n_string_append(attrStr,"   The 1st insn should be issued in given state (i.e.\n");
      n_string_append(attrStr,"    state_transition should return negative value for\n");
      n_string_append(attrStr,"    the insn and the state).  Data dependencies between\n");
      n_string_append(attrStr,"    the insns are ignored by the function.  */\n");
      n_string_append(attrStr,"extern int "
      "min_insn_conflict_delay (state_t, rtx_insn *, rtx_insn *);\n");
      n_string_append(attrStr,"/* The following function outputs reservations for given\n");
      n_string_append(attrStr,"   insn as they are described in the corresponding\n");
      n_string_append(attrStr,"   define_insn_reservation.  */\n");
      n_string_append(attrStr,"extern void print_reservation (FILE *, rtx_insn *);\n");
      n_string_append(attrStr,"\n#if CPU_UNITS_QUERY\n");
      n_string_append(attrStr,"/* The following function returns code of functional unit\n");
      n_string_append(attrStr,"   with given name (see define_cpu_unit). */\n");
      n_string_append(attrStr,"extern int get_cpu_unit_code (const char *);\n");
      n_string_append(attrStr,"/* The following function returns nonzero if functional\n");
      n_string_append(attrStr,"   unit with given code is currently reserved in given\n");
      n_string_append(attrStr,"   DFA state.  */\n");
      n_string_append(attrStr,"extern int cpu_unit_reservation_p (state_t, int);\n");
      n_string_append(attrStr,"#endif\n\n");
      n_string_append(attrStr,"/* The following function returns true if insn\n");
      n_string_append(attrStr,"   has a dfa reservation.  */\n");
      n_string_append(attrStr,"extern bool insn_has_dfa_reservation_p (rtx_insn *);\n\n");
      n_string_append(attrStr,"/* Clean insn code cache.  It should be called if there\n");
      n_string_append(attrStr,"   is a chance that condition value in a\n");
      n_string_append(attrStr,"   define_insn_reservation will be changed after\n");
      n_string_append(attrStr,"   last call of dfa_start.  */\n");
      n_string_append(attrStr,"extern void dfa_clean_insn_cache (void);\n\n");
      n_string_append(attrStr,"extern void dfa_clear_single_insn_cache (rtx_insn *);\n\n");
      n_string_append(attrStr,"/* Initiate and finish work with DFA.  They should be\n");
      n_string_append(attrStr,"   called as the first and the last interface\n");
      n_string_append(attrStr,"   functions.  */\n");
      n_string_append(attrStr,"extern void dfa_start (void);\n");
      n_string_append(attrStr,"extern void dfa_finish (void);\n");
   }else{
      /* Otherwise we do no scheduling, but we need these typedefs
      in order to avoid uglifying other code with more ifdefs.  */
      n_string_append(attrStr,"typedef void *state_t;\n\n");
   }

   /* Special-purpose attributes should be tested with if, not #ifdef.  */
   const char * const special_attrs[] = { "length", "enabled",
   "preferred_for_size",
   "preferred_for_speed", 0 };
   for (const char * const *p = special_attrs; *p; p++){
      n_string_append_printf(attrStr,"#ifndef %s_HAVE_ATTR_%s\n"
      "#define %s_HAVE_ATTR_%s 0\n"
      "#endif\n",platUpperName, *p,platUpperName, *p);
   }
   /* We make an exception here to provide stub definitions for
   insn_*_length* / get_attr_enabled functions.  */
   n_string_append(attrStr,"#if !HAVE_ATTR_length\n"
   "extern int hook_int_rtx_insn_unreachable (rtx_insn *);\n"
   "#define insn_default_length hook_int_rtx_insn_unreachable\n"
   "#define insn_min_length hook_int_rtx_insn_unreachable\n"
   "#define insn_variable_length_p hook_int_rtx_insn_unreachable\n"
   "#define insn_current_length hook_int_rtx_insn_unreachable\n"
   "#include \"insn-addr.h\"\n"
   "#endif\n"
   "extern int hook_int_rtx_1 (rtx);\n"
   "#if !HAVE_ATTR_enabled\n"
   "#define get_attr_enabled hook_int_rtx_1\n"
   "#endif\n"
   "#if !HAVE_ATTR_preferred_for_size\n"
   "#define get_attr_preferred_for_size hook_int_rtx_1\n"
   "#endif\n"
   "#if !HAVE_ATTR_preferred_for_speed\n"
   "#define get_attr_preferred_for_speed hook_int_rtx_1\n"
   "#endif\n");

   /* Output flag masks for use by reorg.

   Flags are used to hold branch direction for use by eligible_for_...  */
   n_string_append(attrStr,"\n#define ATTR_FLAG_forward\t0x1\n");
   n_string_append(attrStr,"#define ATTR_FLAG_backward\t0x2\n");

   n_string_append_printf(attrStr,"\n#endif /* __GCC_%s_INSN_ATTR_H__ */",platUpperName);
   //commonStr最后一句
   n_string_append_printf(commonStr,"#define %s_DELAY_SLOTS %d\n",platUpperName,have_delay);

   printf("%s\n%s\n",commonStr->str,attrStr->str);

   if (ferror (stdout) || fflush (stdout) || fclose (stdout))
      return FATAL_EXIT_CODE;

   return SUCCESS_EXIT_CODE;
}
