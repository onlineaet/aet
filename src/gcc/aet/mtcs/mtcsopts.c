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
 * base on opts.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a optstion.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "expmed.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "stor-layout.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "cfgexpand.h"
#include "opts.h"
#include "predict.h"

#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "diagnostic.h"
#include "opts.h"
#include "flags.h"
#include "langhooks.h"
#include "dbgcnt.h"
#include "debug.h"
#include "output.h"
#include "plugin.h"
#include "toplev.h"
#include "context.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "file-prefix-map.h" /* add_*_prefix_map()  */

#include "opts.h"
#include "tm.h"
#include "flags.h"
#include "diagnostic.h"
#include "opts-diagnostic.h"
#include "insn-attr-common.h"
#include "common/common-target.h"
#include "spellcheck.h"
#include "opt-suggestions.h"
#include "diagnostic-color.h"
#include "diagnostic-format.h"
#include "version.h"
#include "selftest.h"
#include "file-prefix-map.h"

#include "mtcsopts.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"

#define LEFT_COLUMN 27 //原型 #define LEFT_COLUMN 27 opts.cc

/* What to print when a switch has no documentation.  */
static const char undocumented_msg[] = "This option lacks documentation.";
static const char use_diagnosed_msg[] = "Uses of this option are diagnosed.";
/* These are the hash table functions for the hash table of OPTIMIZATION_NODE
   nodes.  */

/* Return the hash code X, an OPTIMIZATION_NODE or TARGET_OPTION code.  */

hashval_t mtcs_cl_option_hasher::hash (tree x)
{
  //TREE_TARGET_OPTION 返回的是struct cl_target_option 强转成MtcsClTargetOption
  const_tree const t = x;
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  if (TREE_CODE (t) == OPTIMIZATION_NODE)
    return cl_optimization_hash (TREE_OPTIMIZATION (t));
  else if (TREE_CODE (t) == TARGET_OPTION_NODE)
    return mtcs_options_cl_target_option_hash(mtcsOptions,(MtcsClTargetOption *)TREE_TARGET_OPTION (t));
    //return cl_target_option_hash (TREE_TARGET_OPTION (t));//MtcsClTargetOption struct cl_target_option
  else
    gcc_unreachable ();
}

/* Return nonzero if the value represented by *X (an OPTIMIZATION or
   TARGET_OPTION tree node) is the same as that given by *Y, which is the
   same.  */

bool mtcs_cl_option_hasher::equal (tree x, tree y)
{
  const_tree const xt = x;
  const_tree const yt = y;
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  if (TREE_CODE (xt) != TREE_CODE (yt))
    return false;

  if (TREE_CODE (xt) == OPTIMIZATION_NODE)
    return cl_optimization_option_eq (TREE_OPTIMIZATION (xt),TREE_OPTIMIZATION (yt));
  else if (TREE_CODE (xt) == TARGET_OPTION_NODE)
    return mtcs_options_cl_target_option_eq(mtcsOptions,(MtcsClTargetOption *)TREE_TARGET_OPTION (xt),
              (MtcsClTargetOption *)TREE_TARGET_OPTION (yt));
    //return cl_target_option_eq (TREE_TARGET_OPTION (xt),TREE_TARGET_OPTION (yt));
  else
    gcc_unreachable ();
}

/* Default the options in OPTS and OPTS_SET based on the optimization
   settings in DECODED_OPTIONS and DECODED_OPTIONS_COUNT.  */
//原型 default_options_optimization opts.h opts.cc
static void defaultOptimization(MtcsOpts*self,struct cl_decoded_option *decoded_options,
                  unsigned int decoded_options_count,location_t loc,unsigned int lang_mask,
                  const struct mtcs_cl_option_handlers *handlers,diagnostic_context *dc);

static void mtcsOptsInit(MtcsOpts *self)
{
   self->save_opt_decoded_options=NULL;
   self->save_decoded_options=NULL;
   self->save_decoded_options_count=0;

   // 下面3行 移到 mtcs_opts_init 中初始化，在这里会出现 gcc_freed问题
   //问题已解决恢复
   self->cl_option_hash_table = hash_table<mtcs_cl_option_hasher>::create_ggc (64);
   self->cl_optimization_node = make_node (OPTIMIZATION_NODE);
   self->cl_target_option_node = make_node (TARGET_OPTION_NODE);

   /* Hold command-line options associated with stack limitation.  */
   //原型 opts.h opt_fstack_limit_symbol_arg opts-global
   self->opt_fstack_limit_symbol_arg=NULL;
   //原型 opts.h opt_fstack_limit_register_no opts-global.cc 缺省值 =-1
   self->opt_fstack_limit_register_no=-1;
   //原型 opts.h flag_stack_protector_set_by_fhardened_p  opts.cc
   self->flag_stack_protector_set_by_fhardened_p=false;
}

/* Parse -fzero-call-used-regs suboptions from ARG, return the FLAGS.  */
//原型 parse_zero_call_used_regs_options opts.cc
static unsigned int parse_zero_call_used_regs_options (const char *arg)
{
  unsigned int flags = 0;

  /* Check to see if the string matches a sub-option name.  */
  for (unsigned int i = 0; zero_call_used_regs_opts[i].name != NULL; ++i)
     if (strcmp (arg, zero_call_used_regs_opts[i].name) == 0){
        flags = zero_call_used_regs_opts[i].flag;
        break;
     }

  if (!flags)
    error ("unrecognized argument to %<-fzero-call-used-regs=%>: %qs", arg);

  return flags;
}


/* Check that alignment value FLAG for -falign-NAME is valid at a given
   location LOC. OPT_STR points to the stored -falign-NAME=argument and
   OPT_FLAG points to the associated -falign-NAME on/off flag.  */
//原型 check_alignment_argument opts.cc
static void check_alignment_argument (location_t loc, const char *flag, const char *name,
              int *opt_flag, const char **opt_str)
{
  auto_vec<unsigned> align_result;
  parse_and_check_align_values (flag, name, align_result, true, loc);
  if (align_result.length() >= 1 && align_result[0] == 0){
      *opt_flag = 1;
      *opt_str = NULL;
  }
}


/* Save Optimization decoded options.  */
//保存优化选项，比如 -O3 -02
void mtcs_opt_save_optimization_decoded_options(MtcsOpts *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    if(self->save_opt_decoded_options!=NULL){
        n_error("mtcs_opt_save_optimization_decoded_options save_opt_decoded_options应该是空的\n");
        return;
    }
    int clOptionsCount=mtcsOptions->clOptionsCount;
    self->save_opt_decoded_options = new vec<cl_decoded_option> ();
    for (unsigned i = 1; i < self->save_decoded_options_count; ++i){
        if (self->save_decoded_options[i].opt_index < clOptionsCount/*!cl_options_count*/
                && mtcsOptions->clOptions/*!cl_options*/[self->save_decoded_options[i].opt_index].flags & CL_OPTIMIZATION){
          n_debug("mtcsopts.c mtcs_opt_save_optimization_decoded_options ---i:%d errors:%d %s\n",
                    i,self->save_decoded_options[i].errors,self->save_decoded_options[i].orig_option_with_args_text);
          self->save_opt_decoded_options->safe_push(self->save_decoded_options[i]);
        }
    }
}

//typedef const char *const_char_p; /* For DEF_VEC_P.  */
//static vec<const_char_p> ignored_options;

/* Buffer the unknown option described by the string OPT.  Currently,
   we only complain about unknown -Wno-* options if they may have
   prevented a diagnostic. Otherwise, we just ignore them.  Note that
   if we do complain, it is only as a warning, not an error; passing
   the compiler an unrecognized -Wno-* option should never change
   whether the compilation succeeds or fails.  */

static void postpone_unknown_option_warning ( MtcsOpts *self,const char *opt)
{
  self->ignored_options.safe_push (opt);
}

/* Handle an unknown option DECODED, returning true if an error should
   be given.  */

static bool unknown_option_callback_cb (const struct cl_decoded_option *decoded,void *userData)
{
  MtcsOpts *self=(MtcsOpts *)userData;
  const char *opt = decoded->arg;
  if (opt[1] == 'W' && opt[2] == 'n' && opt[3] == 'o' && opt[4] == '-'
      && !(decoded->errors & CL_ERR_NEGATIVE)){
      /* We don't generate warnings for unknown -Wno-* options unless
     we issue diagnostics.  */
      postpone_unknown_option_warning (self,opt);
      return false;
  }else
    return true;
}


/* Complain that switch DECODED does not apply to this front end (mask
   LANG_MASK).  */

static void complain_wrong_lang_cb (const struct cl_decoded_option *decoded,unsigned int lang_mask,void *userData)
{
  MtcsOpts *self=(MtcsOpts *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[decoded->opt_index];
  const char *text = decoded->orig_option_with_args_text;
  char *ok_langs = NULL, *bad_lang = NULL;
  unsigned int opt_flags = option->flags;

  if (!mtcsOptionsItem->x_warn_complain_wrong_lang/*!warn_complain_wrong_lang*/)
    return;

  if (!mtcs_options_hooks_complain_wrong_lang_p/*!lang_hooks.complain_wrong_lang_p*/(mtcsOptions,option))
    return;

  opt_flags &= ((1U << cl_lang_count) - 1) | CL_DRIVER;
  if (opt_flags != CL_DRIVER)
    ok_langs = write_langs (opt_flags);
  if (lang_mask != CL_DRIVER)
    bad_lang = write_langs (lang_mask);

  if (opt_flags == CL_DRIVER)
    error ("command-line option %qs is valid for the driver but not for %s",text, bad_lang);
  else if (lang_mask == CL_DRIVER)
    gcc_unreachable ();
  else if (ok_langs[0] != '\0')
    /* Eventually this should become a hard error IMO.  */
    warning (0, "command-line option %qs is valid for %s but not for %s",text, ok_langs, bad_lang);
  else
    /* Happens for -Werror=warning_name.  */
    warning (0, "%<-Werror=%> argument %qs is not valid for %s",text, bad_lang);
  free (ok_langs);
  free (bad_lang);
}

/* Handle a front-end option; arguments and return value as for
   handle_option.  */
static bool langHandleOption_cb(MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
            const struct cl_decoded_option *decoded,
            unsigned int lang_mask ATTRIBUTE_UNUSED, int kind,
            location_t loc,
            const struct mtcs_cl_option_handlers *handlers,
            diagnostic_context *dc,
            void (*) (void *userData),void *userData)
{
  MtcsOpts *self=(MtcsOpts *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  gcc_assert (opts ==mtcsOptions->global_options);
  gcc_assert (opts_set ==mtcsOptions->global_options_set);
  gcc_assert (dc == global_dc);
  gcc_assert (decoded->canonical_option_num_elements <= 2);
  n_debug("mtcsopts.c langHandleOption_cb-----\n");
  return mtcs_options_hooks_handle_option/*!lang_hooks.handle_option*/(mtcsOptions,decoded->opt_index, decoded->arg,decoded->value, kind, loc, handlers);
}

/* Handle target- and language-independent options.  Return zero to
   generate an "unknown option" message.  Only options that need
   extra handling need to be listed here; if you simply want
   DECODED->value assigned to a variable, it happens automatically.  */
//原型 common_handle_option opts.h opts.cc
static bool commonHandleOption_cb(MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
              const struct cl_decoded_option *decoded,
              unsigned int lang_mask, int kind ATTRIBUTE_UNUSED,
              location_t loc,
              const struct mtcs_cl_option_handlers *handlers,
              diagnostic_context *dc,
              void (*target_option_override_hook) (void *userData),void *userData)
{
    MtcsOpts *self=(MtcsOpts *)userData;
    return mtcs_opts_common_handle_option(self,opts,opts_set,decoded,lang_mask,kind,loc,handlers,dc,target_option_override_hook,userData);
}

/* Handle a back-end option; arguments and return value as for
   handle_option.  */
//原型 target_handle_option opts.h opts.cc
static bool targetHandleOption_cb (MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
              const struct cl_decoded_option *decoded,
              unsigned int lang_mask ATTRIBUTE_UNUSED, int kind,
              location_t loc,
              const struct cl_option_handlers *handlers ATTRIBUTE_UNUSED,
              diagnostic_context *dc, void (*) (void *userData),void *userData)
{
  MtcsOpts *self=(MtcsOpts *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  gcc_assert (dc == global_dc);
  gcc_assert (kind == DK_UNSPECIFIED);
  return target_common_handle_option/*!targetm_common.handle_option*/(mtcsMachine->common,opts, opts_set, decoded, loc);
}


/* Set *HANDLERS to the default set of option handlers for use in the
   compilers proper (not the driver).  */
//原型 set_default_handlers opts.h opts-global.cc
static void setDefaultHandlers(MtcsOpts *self,struct mtcs_cl_option_handlers *handlers,
              void (*target_option_override_hook) (void *userData))
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  handlers->userData = self;
  handlers->unknown_option_callback = unknown_option_callback_cb;
  handlers->wrong_lang_callback = complain_wrong_lang_cb;
  handlers->target_option_override_hook = target_option_override_hook;
  handlers->num_handlers = 3;
  handlers->handlers[0].handler = langHandleOption_cb;
  handlers->handlers[0].mask = mtcsOptions->initial_lang_mask;
  handlers->handlers[1].handler = commonHandleOption_cb;
  handlers->handlers[1].mask = CL_COMMON;
  handlers->handlers[2].handler = targetHandleOption_cb;
  handlers->handlers[2].mask = CL_TARGET;
}

static void target_option_override_cb(void *userData)
{
    MtcsOpts *self=(MtcsOpts *)userData;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    target_option_override/*!targetm.target_option.override*/(mtcsMachine->option);
}


/* Return whether OPTION is OK for the language given by
   LANG_MASK.  */
//原型 option_ok_for_language opts-common.cc
static bool option_ok_for_language (const struct cl_option *option,unsigned int lang_mask)
{
  if (!(option->flags & lang_mask))
    return false;
  else if ((option->flags & CL_TARGET)
       && (option->flags & (CL_LANG_ALL | CL_DRIVER))
       && !(option->flags & (lang_mask & ~CL_COMMON & ~CL_TARGET)))
    /* Complain for target flag language mismatches if any languages
       are specified.  */
    return false;
  return true;
}



/* If indicated by the optimization level LEVEL (-Os if SIZE is set,
   -Ofast if FAST is set, -Og if DEBUG is set), apply the option DEFAULT_OPT
   to OPTS and OPTS_SET, diagnostic context DC, location LOC, with language
   mask LANG_MASK and option handlers HANDLERS.  */
//原型 maybe_default_option opts.cc
static void maybe_default_option (MtcsOpts *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
              const struct default_options *default_opt,
              int level, bool size, bool fast, bool debug,
              unsigned int lang_mask,
              const struct mtcs_cl_option_handlers *handlers,
              location_t loc,
              diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    //default_opt->opt_index是主机的opt_code 转成设备的
  int deviceOptCode= mtcs_options_optcode_host_to_device(mtcsOptions,default_opt->opt_index);
  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[deviceOptCode/*!default_opt->opt_index*/];
  bool enabled;

  if (size)
    gcc_assert (level == 2);
  if (fast)
    gcc_assert (level == 3);
  if (debug)
    gcc_assert (level == 1);

  switch (default_opt->levels){
    case OPT_LEVELS_ALL:
      enabled = true;
      break;
    case OPT_LEVELS_0_ONLY:
      enabled = (level == 0);
      break;
    case OPT_LEVELS_1_PLUS:
      enabled = (level >= 1);
      break;
    case OPT_LEVELS_1_PLUS_SPEED_ONLY:
      enabled = (level >= 1 && !size && !debug);
      break;
    case OPT_LEVELS_1_PLUS_NOT_DEBUG:
      enabled = (level >= 1 && !debug);
      break;
    case OPT_LEVELS_2_PLUS:
      enabled = (level >= 2);
      break;
    case OPT_LEVELS_2_PLUS_SPEED_ONLY:
      enabled = (level >= 2 && !size && !debug);
      break;
    case OPT_LEVELS_3_PLUS:
      enabled = (level >= 3);
      break;
    case OPT_LEVELS_3_PLUS_AND_SIZE:
      enabled = (level >= 3 || size);
      break;
    case OPT_LEVELS_SIZE:
      enabled = size;
      break;
    case OPT_LEVELS_FAST:
      enabled = fast;
      break;
    case OPT_LEVELS_NONE:
    default:
      gcc_unreachable ();
  }

  if (enabled)
      mtcs_opts_handle_generated_option/*!handle_generated_option*/(self,opts, opts_set, deviceOptCode/*!default_opt->opt_index*/,
              default_opt->arg,default_opt->value, lang_mask, DK_UNSPECIFIED, loc,handlers, true, dc);
  else if (default_opt->arg == NULL
       && !option->cl_reject_negative
       && !(option->flags & CL_PARAMS))
      mtcs_opts_handle_generated_option/*!handle_generated_option*/(self,opts, opts_set,deviceOptCode/*!default_opt->opt_index*/,
                 default_opt->arg, !default_opt->value,lang_mask, DK_UNSPECIFIED, loc,handlers, true, dc);
}

/* As indicated by the optimization level LEVEL (-Os if SIZE is set,
   -Ofast if FAST is set), apply the options in array DEFAULT_OPTS to
   OPTS and OPTS_SET, diagnostic context DC, location LOC, with
   language mask LANG_MASK and option handlers HANDLERS.  */
//原型 maybe_default_options opts.cc
static void maybe_default_options (MtcsOpts *self, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,const struct default_options *default_opts,
               int level, bool size, bool fast, bool debug, unsigned int lang_mask,
               const struct mtcs_cl_option_handlers *handlers,location_t loc,diagnostic_context *dc)
{
  size_t i;
  n_debug("mtcsopts.c  maybe_default_options 00 总共选项数:%d %d %d opts:%p\n",i,opts->x_flag_unsafe_math_optimizations,opts_set->x_flag_ipa_profile,opts);
  for (i = 0; default_opts[i].levels != OPT_LEVELS_NONE; i++)
     maybe_default_option (self,opts, opts_set, &default_opts[i],level, size, fast, debug,lang_mask, handlers, loc, dc);
  n_debug("mtcsopts.c maybe_default_options 11 总共选项数:%d %d %d\n",i,opts->x_flag_unsafe_math_optimizations,opts_set->x_flag_ipa_profile);
}

/* Default the options in OPTS and OPTS_SET based on the optimization
   settings in DECODED_OPTIONS and DECODED_OPTIONS_COUNT.  */
//原型 default_options_optimization opts.h opts.cc
static void defaultOptimization(MtcsOpts*self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_decoded_option *decoded_options,
                  unsigned int decoded_options_count,location_t loc,unsigned int lang_mask,
                  const struct mtcs_cl_option_handlers *handlers,diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  struct default_options *defaultOptionsTable = mtcs_options_get_default_options_table(mtcsOptions);

  unsigned int i;
  int opt2;
  /* Scan to see what optimization level has been specified.  That will
     determine the default value of many flags.  */
  for (i = 1; i < decoded_options_count; i++){
      struct cl_decoded_option *opt = &decoded_options[i];
      int hostOptCode=mtcs_options_optcode_device_to_host(mtcsOptions,opt->opt_index);
      switch (hostOptCode/*!opt->opt_index*/){
        case OPT_O:
          if (*opt->arg == '\0'){
              opts->x_optimize = 1;
              opts->x_optimize_size = 0;
              opts->x_optimize_fast = 0;
              opts->x_optimize_debug = 0;
          }else{
              const int optimize_val = integral_argument (opt->arg);
              if (optimize_val == -1)
                  error_at (loc, "argument to %<-O%> should be a non-negative integer, %<g%>, %<s%>, %<z%> or %<fast%>");
              else{
                  opts->x_optimize = optimize_val;
                  if ((unsigned int) opts->x_optimize > 255)
                    opts->x_optimize = 255;
                  opts->x_optimize_size = 0;
                  opts->x_optimize_fast = 0;
                  opts->x_optimize_debug = 0;
              }
          }
          break;

        case OPT_Os:
          opts->x_optimize_size = 1;
          /* Optimizing for size forces optimize to be 2.  */
          opts->x_optimize = 2;
          opts->x_optimize_fast = 0;
          opts->x_optimize_debug = 0;
          break;

        case OPT_Oz:
          opts->x_optimize_size = 2;
          /* Optimizing for size forces optimize to be 2.  */
          opts->x_optimize = 2;
          opts->x_optimize_fast = 0;
          opts->x_optimize_debug = 0;
          break;
        case OPT_Ofast:
          /* -Ofast only adds flags to -O3.  */
          opts->x_optimize_size = 0;
          opts->x_optimize = 3;
          opts->x_optimize_fast = 1;
          opts->x_optimize_debug = 0;
          break;
        case OPT_Og:
          /* -Og selects optimization level 1.  */
          opts->x_optimize_size = 0;
          opts->x_optimize = 1;
          opts->x_optimize_fast = 0;
          opts->x_optimize_debug = 1;
          break;
        default:
          /* Ignore other options in this prescan.  */
          break;
        }
  }

  n_debug("mtcsopts.c default_options_optimization 00 :%d\n",opts->x_flag_unsafe_math_optimizations);

  maybe_default_options(self,opts,opts_set,defaultOptionsTable/*!default_options_table*/,opts->x_optimize, opts->x_optimize_size,
             opts->x_optimize_fast, opts->x_optimize_debug,lang_mask, handlers, loc, dc);
  n_debug("mtcsopts.c default_options_optimization 11  :%d \n",opts->x_flag_unsafe_math_optimizations);

  /* -O2 param settings.  */
  opt2 = (opts->x_optimize >= 2);

  //if (openacc_mode)
  SET_OPTION_IF_UNSET (opts, opts_set, flag_ipa_pta, true);
  n_debug("mtcsopts.c default_options_optimization 22  %d\n",opts->x_flag_unsafe_math_optimizations);


  /* Track fields in field-sensitive alias analysis.  */
  if (opt2)
    SET_OPTION_IF_UNSET (opts, opts_set, param_max_fields_for_field_sensitive,100);
  if (opts->x_optimize_size)
    /* We want to crossjump as much as possible.  */
    SET_OPTION_IF_UNSET (opts, opts_set, param_min_crossjump_insns, 1);
  /* Restrict the amount of work combine does at -Og while retaining
     most of its useful transforms.  */
  if (opts->x_optimize_debug)
    SET_OPTION_IF_UNSET (opts, opts_set, param_max_combine_insns, 2);
  /* Allow default optimizations to be specified on a per-machine basis.  */
  maybe_default_options (self,opts, opts_set,
             mtcsMachine->common->empty_optimization_table/*!targetm_common.option_optimization_table*/,
             opts->x_optimize, opts->x_optimize_size,
             opts->x_optimize_fast, opts->x_optimize_debug,
             lang_mask, handlers, loc, dc);

}


/* Handle FILENAME from the command line.  */
//原型 add_input_filename opts-global.cc
static void add_input_filename (const char *filename)
{
//  num_in_fnames++;
//  in_fnames = XRESIZEVEC (const char *, in_fnames, num_in_fnames);
//  in_fnames[num_in_fnames - 1] = filename;
}

/* Handle the vector of command line options (located at LOC), storing
   the results of processing DECODED_OPTIONS and DECODED_OPTIONS_COUNT
   in OPTS and OPTS_SET and using DC for diagnostic state.  LANG_MASK
   contains has a single bit set representing the current language.
   HANDLERS describes what functions to call for the options.  */
//原型 read_cmdline_options opts-global.cc
static void read_cmdline_options (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,
              struct cl_decoded_option *decoded_options,
              unsigned int decoded_options_count,
              location_t loc,
              unsigned int lang_mask,
              const struct mtcs_cl_option_handlers *handlers,
              diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  unsigned int i;
  for (i = 1; i < decoded_options_count; i++){
      int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,decoded_options[i].opt_index);
      if (hostOptcode/*!decoded_options[i].opt_index*/== OPT_SPECIAL_input_file){
          /* Input files should only ever appear on the main command  line.  */
          gcc_assert (opts == mtcsOptions->global_options);
          gcc_assert (opts_set ==  mtcsOptions->global_options_set);
          n_debug("mtcsopts.c read_cmdline_options 00 i:%d %s\n",i,opts->x_main_input_filename);

          if (opts->x_main_input_filename == NULL){
              opts->x_main_input_filename = decoded_options[i].arg;
              opts->x_main_input_baselength= base_of_path (opts->x_main_input_filename,&opts->x_main_input_basename);
          }
          add_input_filename (decoded_options[i].arg);
          continue;
      }
      n_debug("mtcsopts.cread_cmdline_options 00 opt is :%d %s %s %d\n",
            i,decoded_options[i].orig_option_with_args_text,decoded_options[i].warn_message,decoded_options[i].errors);

      mtcs_opts_read_cmdline_option/*!read_cmdline_option*/(self,opts, opts_set,decoded_options + i, loc, lang_mask, handlers,dc);
  }
}

/* Parse command line options and set default flag values.  Do minimal
   options processing.  The decoded options are in *DECODED_OPTIONS
   and *DECODED_OPTIONS_COUNT; settings go in OPTS, OPTS_SET and DC;
   the options are located at LOC.  */
//原型 decode_options opts.h opts-global.cc
void mtcs_opts_decode_options (MtcsOpts *self,location_t loc, diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;
  MtcsOptionsItem *opts_set=mtcsOptions->global_options_set;
  struct cl_decoded_option *decoded_options=self->save_decoded_options;
  nuint decoded_options_count=self->save_decoded_options_count;
  struct mtcs_cl_option_handlers handlers;
  unsigned int lang_mask = mtcsOptions->initial_lang_mask;
  n_debug("mtcsopts.c decode_options 00 :%d count:%d\n",opts->x_flag_unsafe_math_optimizations,decoded_options_count);
  setDefaultHandlers/*!set_default_handlers*/(self,&handlers, target_option_override_cb);
  n_debug("mtcsopts.c decode_options 11 :%d count:%d\n",opts->x_flag_unsafe_math_optimizations,decoded_options_count);
  defaultOptimization/*!default_options_optimization*/(self,opts,opts_set,decoded_options,decoded_options_count,loc,lang_mask,&handlers,dc);
  n_debug("mtcsopts.c decode_options 22 :%d count:%d\n",opts->x_flag_unsafe_math_optimizations,decoded_options_count);
  read_cmdline_options(self,opts, opts_set,decoded_options, decoded_options_count,loc, lang_mask, &handlers, dc);
  n_debug("mtcsopts.c decode_options 33 :%d count:%d\n",opts->x_flag_unsafe_math_optimizations,decoded_options_count);
  mtcs_opts_finish_options(self,opts, opts_set, loc);
  n_debug("mtcsopts.c decode_options 44 :%d count:%d\n",opts->x_flag_unsafe_math_optimizations,decoded_options_count);
}


/* Return the address of the flag variable for option OPT_INDEX in
   options structure OPTS, or NULL if there is no flag variable.  */
//原型 option_flag_var opts.h opts-common.cc
void *mtcs_opts_option_flag_var (MtcsOpts *self,int opt_index, MtcsOptionsItem/*!struct gcc_options*/ *opts)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[opt_index];
  if (option->flag_var_offset == (unsigned short) -1)
    return NULL;
  return (void *)(((char *) opts) + option->flag_var_offset);
}

/* Handle option DECODED for the language indicated by LANG_MASK,
   using the handlers in HANDLERS and setting fields in OPTS and
   OPTS_SET.  KIND is the diagnostic_t if this is a diagnostics
   option, DK_UNSPECIFIED otherwise, and LOC is the location of the
   option for options from the source file, UNKNOWN_LOCATION
   otherwise.  GENERATED_P is true for an option generated as part of
   processing another option or otherwise generated internally, false
   for one explicitly passed by the user.  control_warning_option
   generated options are considered explicitly passed by the user.
   Returns false if the switch was invalid.  DC is the diagnostic
   context for options affecting diagnostics state, or NULL.  */
//原型 handle_option opts-common.cc
static bool handle_option (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,
           const struct cl_decoded_option *decoded,unsigned int lang_mask, int kind, location_t loc,
           const struct mtcs_cl_option_handlers *handlers, bool generated_p, diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  size_t opt_index = decoded->opt_index;
  const char *arg = decoded->arg;
  HOST_WIDE_INT value = decoded->value;
  HOST_WIDE_INT mask = decoded->mask;
  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[opt_index];
  void *flag_var = mtcs_opts_option_flag_var/*!option_flag_var*/(self,opt_index, opts);
  size_t i;

  if (flag_var)
      mtcs_opts_set_option/*!set_option*/(self,opts, (generated_p ? NULL : opts_set),opt_index, value, arg, kind, loc, dc, mask);

  for (i = 0; i < handlers->num_handlers; i++){
     // n_debug("mtcsopts.c handle_option ---00 %d %d %s\n",i,(option->flags & handlers->handlers[i].mask),option->opt_text);
    if (option->flags & handlers->handlers[i].mask){
        if (!handlers->handlers[i].handler (opts, opts_set, decoded,lang_mask, kind,
                loc,handlers, dc,handlers->target_option_override_hook,handlers->userData))
          return false;
    }
  }

  return true;
}

/* Like handle_option, but OPT_INDEX, ARG and VALUE describe the
   option instead of DECODED.  This is used for callbacks when one
   option implies another instead of an option being decoded from the
   command line.  */
//原型 handle_generated_option opts.h opts-common.cc
//opt_index 一定是设备的opt_code 所有函数中声明的 opt_code都默认是device opt_code
bool mtcs_opts_handle_generated_option (MtcsOpts *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
             size_t opt_index, const char *arg, HOST_WIDE_INT value, unsigned int lang_mask, int kind, location_t loc,
             const struct mtcs_cl_option_handlers *handlers,bool generated_p, diagnostic_context *dc)
{
  struct cl_decoded_option decoded;
  mtcs_opts_generate_option/*!generate_option*/ (self,opt_index, arg, value, lang_mask, &decoded);
  return handle_option(self,opts, opts_set, &decoded, lang_mask, kind, loc,
            handlers, generated_p, dc);
}

/* Fill in the canonical option part of *DECODED with an option
   described by OPT_INDEX, ARG and VALUE.  */
//原型 generate_canonical_option opts-common.cc
//opt_index 一定是设备的opt_code 所有函数中声明的 opt_code都默认是device opt_code
static void generate_canonical_option (MtcsOpts *self,size_t opt_index, const char *arg,
               HOST_WIDE_INT value,struct cl_decoded_option *decoded)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[opt_index];
  const char *opt_text = option->opt_text;

  if (value == 0 && !option->cl_reject_negative
      && (opt_text[1] == 'W' || opt_text[1] == 'f'
      || opt_text[1] == 'g' || opt_text[1] == 'm')){
      char *t = XOBNEWVEC (&opts_obstack, char, option->opt_len + 5);
      t[0] = '-';
      t[1] = opt_text[1];
      t[2] = 'n';
      t[3] = 'o';
      t[4] = '-';
      memcpy (t + 5, opt_text + 2, option->opt_len);
      opt_text = t;
  }

  decoded->canonical_option[2] = NULL;
  decoded->canonical_option[3] = NULL;

  if (arg){
      if ((option->flags & CL_SEPARATE)  && !option->cl_separate_alias){
          decoded->canonical_option[0] = opt_text;
          decoded->canonical_option[1] = arg;
          decoded->canonical_option_num_elements = 2;
      }else{
          gcc_assert (option->flags & CL_JOINED);
          decoded->canonical_option[0] = opts_concat (opt_text, arg, NULL);
          decoded->canonical_option[1] = NULL;
          decoded->canonical_option_num_elements = 1;
      }
  }else{
      decoded->canonical_option[0] = opt_text;
      decoded->canonical_option[1] = NULL;
      decoded->canonical_option_num_elements = 1;
  }
}


/* Fill in *DECODED with an option described by OPT_INDEX, ARG and
   VALUE for a front end using LANG_MASK.  This is used when the
   compiler generates options internally.  */
//原型 generate_option opts.h opts-common.cc
//opt_index 一定是设备的opt_code 所有函数中声明的 opt_code都默认是device opt_code
void mtcs_opts_generate_option (MtcsOpts *self,size_t opt_index, const char *arg, HOST_WIDE_INT value,
         unsigned int lang_mask, struct cl_decoded_option *decoded)
{

  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[opt_index];

  decoded->opt_index = opt_index;
  decoded->warn_message = NULL;
  decoded->arg = arg;
  decoded->value = value;
  decoded->mask = 0;
  decoded->errors = (option_ok_for_language (option, lang_mask)? 0 : CL_ERR_WRONG_LANG);

  generate_canonical_option (self,opt_index, arg, value, decoded);
  switch (decoded->canonical_option_num_elements){
    case 1:
      decoded->orig_option_with_args_text = decoded->canonical_option[0];
      break;
    case 2:
      decoded->orig_option_with_args_text = opts_concat (decoded->canonical_option[0], " ",decoded->canonical_option[1], NULL);
      break;
    default:
      gcc_unreachable ();
  }
}

/* Set any field in OPTS, and OPTS_SET if not NULL, for option
   OPT_INDEX according to VALUE and ARG, diagnostic kind KIND,
   location LOC, using diagnostic context DC if not NULL for
   diagnostic classification.  */
//原型 set_option opts.h opts-common.cc
//opt_index 一定是设备的opt_code 所有函数中声明的 opt_code都默认是device opt_code
void mtcs_opts_set_option (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,
        int opt_index, HOST_WIDE_INT value, const char *arg, int kind,
        location_t loc, diagnostic_context *dc, HOST_WIDE_INT mask /* = 0 */)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  const struct cl_option *option = &mtcsOptions->clOptions/*!cl_options*/[opt_index];
  void *flag_var = mtcs_opts_option_flag_var/*!option_flag_var*/(self,opt_index, opts);
  void *set_flag_var = NULL;

  if (!flag_var)
    return;
  //diagnostic_classify_diagnostic 方法，所以opt_index 要从设备转成主机
  int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,opt_index);
  if ((diagnostic_t) kind != DK_UNSPECIFIED && dc != NULL)
    diagnostic_classify_diagnostic (dc, hostOptcode/*!opt_index*/, (diagnostic_t) kind, loc);

  if (opts_set != NULL)
    set_flag_var =  mtcs_opts_option_flag_var/*!option_flag_var*/(self,opt_index, opts_set);

  switch (option->var_type){
    case CLVC_INTEGER:
        if (option->cl_host_wide_int){
            *(HOST_WIDE_INT *) flag_var = value;
            if (set_flag_var)
              *(HOST_WIDE_INT *) set_flag_var = 1;
        }else{
            if (value > INT_MAX)
              error_at (loc, "argument to %qs is bigger than %d",option->opt_text, INT_MAX);
            else{
                *(int *) flag_var = value;
                if (set_flag_var)
                  *(int *) set_flag_var = 1;
            }
        }
        break;

    case CLVC_SIZE:
        if (option->cl_host_wide_int){
            *(HOST_WIDE_INT *) flag_var = value;
            if (set_flag_var)
              *(HOST_WIDE_INT *) set_flag_var = value;
        }else{
            *(int *) flag_var = value;
            if (set_flag_var)
              *(int *) set_flag_var = value;
        }

        break;

    case CLVC_EQUAL:
        if (option->cl_host_wide_int){
            *(HOST_WIDE_INT *) flag_var = (value ? option->var_value: !option->var_value);
            if (set_flag_var)
              *(HOST_WIDE_INT *) set_flag_var = 1;
        }else{
            *(int *) flag_var = (value ? option->var_value: !option->var_value);
            if (set_flag_var)
              *(int *) set_flag_var = 1;
        }
        break;

    case CLVC_BIT_CLEAR:
    case CLVC_BIT_SET:
        if ((value != 0) == (option->var_type == CLVC_BIT_SET)){
            if (option->cl_host_wide_int)
              *(HOST_WIDE_INT *) flag_var |= option->var_value;
            else
              *(int *) flag_var |= option->var_value;
        }else{
            if (option->cl_host_wide_int)
              *(HOST_WIDE_INT *) flag_var &= ~option->var_value;
            else
              *(int *) flag_var &= ~option->var_value;
        }
        if (set_flag_var){
            if (option->cl_host_wide_int)
              *(HOST_WIDE_INT *) set_flag_var |= option->var_value;
            else
              *(int *) set_flag_var |= option->var_value;
        }
        break;

    case CLVC_STRING:
        *(const char **) flag_var = arg;
        if (set_flag_var)
          *(const char **) set_flag_var = "";
        break;

    case CLVC_ENUM:
      {
        const struct cl_enum *e = &cl_enums[option->var_enum];
        if (mask)
          e->set (flag_var, value | (e->get (flag_var) & ~mask));
        else
          e->set (flag_var, value);
        if (set_flag_var)
          e->set (set_flag_var, 1);
      }
      break;

    case CLVC_DEFER:
      {
          vec<cl_deferred_option> *v = (vec<cl_deferred_option> *) *(void **) flag_var;
          cl_deferred_option p = {opt_index, arg, value};
          if (!v)
            v = XCNEW (vec<cl_deferred_option>);
          v->safe_push (p);
          *(void **) flag_var = v;
          if (set_flag_var)
            *(void **) set_flag_var = v;
      }
      break;
  }
}


/* Return whether ENUM_ARG is OK for the language given by
   LANG_MASK.  */

static bool enum_arg_ok_for_language (MtcsOpts *self,const struct cl_enum_arg *enum_arg, unsigned int lang_mask)
{
  return (lang_mask & CL_DRIVER) || !(enum_arg->flags & CL_ENUM_DRIVER_ONLY);
}

/* Look up ARG in ENUM_ARGS for language LANG_MASK, returning the cl_enum_arg
   index and storing the value in *VALUE if found, and returning -1 without
   modifying *VALUE if not found.  */

static int enum_arg_to_value (MtcsOpts *self,const struct cl_enum_arg *enum_args,
           const char *arg, size_t len, HOST_WIDE_INT *value,unsigned int lang_mask)
{
  unsigned int i;
  n_debug("mtcsopts.c enum_arg_to_value --- %s %d %d\n",arg,len,lang_mask);
  for (i = 0; enum_args[i].arg != NULL; i++){
     bool ret= enum_arg_ok_for_language (self,&enum_args[i], lang_mask);
     n_debug("mtcsopts.c enum_arg_to_value --- i:%d %s %d %s %d\n",i,arg,len,enum_args[i].arg,ret);

     if ((len? (strncmp (arg, enum_args[i].arg, len) == 0 && enum_args[i].arg[len] == '\0'): strcmp (arg, enum_args[i].arg) == 0)
        && enum_arg_ok_for_language (self,&enum_args[i], lang_mask)){
        *value = enum_args[i].value;
        return i;
     }
  }
  return -1;
}


/* Perform diagnostics for read_cmdline_option and control_warning_option
   functions.  Returns true if an error has been diagnosed.
   LOC and LANG_MASK arguments like in read_cmdline_option.
   OPTION is the option to report diagnostics for, OPT the name
   of the option as text, ARG the argument of the option (for joined
   options), ERRORS is bitmask of CL_ERR_* values.  */
//原型 cmdline_handle_error opts-common.cc
static bool cmdline_handle_error (MtcsOpts *self,location_t loc, const struct cl_option *option,
              const char *opt, const char *arg, int errors,unsigned int lang_mask)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  if (errors & CL_ERR_DISABLED){
      error_at (loc, "command-line option %qs is not supported by this configuration", opt);
      return true;
  }

  if (errors & CL_ERR_MISSING_ARG){
      if (option->missing_argument_error)
          error_at (loc, option->missing_argument_error, opt);
      else
          error_at (loc, "missing argument to %qs", opt);
      return true;
  }

  if (errors & CL_ERR_UINT_ARG){
      if (option->cl_byte_size)
          error_at (loc, "argument to %qs should be a non-negative integer optionally followed by a size unit",option->opt_text);
      else
          error_at (loc, "argument to %qs should be a non-negative integer",option->opt_text);
      return true;
  }

  if (errors & CL_ERR_INT_RANGE_ARG){
      error_at (loc, "argument to %qs is not between %d and %d",option->opt_text, option->range_min, option->range_max);
      return true;
  }

  if (errors & CL_ERR_ENUM_SET_ARG){
      const struct cl_enum *e = &cl_enums[option->var_enum];
      const char *p = arg;
      unsigned HOST_WIDE_INT used_sets = 0;
      const char *second_opt = NULL;
      size_t second_opt_len = 0;
      errors = 0;
      do{
          const char *q = strchr (p, ',');
          HOST_WIDE_INT this_value = 0;
          if (q && q == p){
              arg = "";
              errors = CL_ERR_ENUM_ARG;
              break;
          }
          int idx = enum_arg_to_value (self,e->values, p, q ? q - p : 0,&this_value, lang_mask);
          if (idx < 0){
              if (q == NULL)
                  q = strchr (p, '\0');
              char *narg = XALLOCAVEC (char, (q - p) + 1);
              memcpy (narg, p, q - p);
              narg[q - p] = '\0';
              arg = narg;
              errors = CL_ERR_ENUM_ARG;
              break;
          }

          if (option->var_value == CLEV_BITSET){
              if (q == NULL)
                  break;
              p = q + 1;
              continue;
          }

          unsigned set = e->values[idx].flags >> CL_ENUM_SET_SHIFT;
          gcc_checking_assert (set >= 1 && set <= HOST_BITS_PER_WIDE_INT);
          if ((used_sets & (HOST_WIDE_INT_1U << (set - 1))) != 0){
              if (q == NULL)
                  q = strchr (p, '\0');
              if (second_opt == NULL){
                  used_sets = HOST_WIDE_INT_1U << (set - 1);
                  second_opt = p;
                  second_opt_len = q - p;
                  p = arg;
                  continue;
              }
              char *args = XALLOCAVEC (char, (q - p) + 1 + second_opt_len + 1);
              memcpy (args, p, q - p);
              args[q - p] = '\0';
              memcpy (args + (q - p) + 1, second_opt, second_opt_len);
              args[(q - p) + 1 + second_opt_len] = '\0';
              error_at (loc, "invalid argument in option %qs", opt);
              if (strcmp (args, args + (q - p) + 1) == 0)
                  inform (loc, "%qs specified multiple times in the same option",args);
              else
                  inform (loc, "%qs is mutually exclusive with %qs and cannot be specified together", args, args + (q - p) + 1);
              return true;
          }
          used_sets |= HOST_WIDE_INT_1U << (set - 1);
          if (q == NULL)
            break;
          p = q + 1;
        } while (1);
  }
  n_debug("mtcsopts.c cmdline_handle_error 00 %s errors:%d %d\n",opt,errors,(errors & CL_ERR_ENUM_ARG));

  if (errors & CL_ERR_ENUM_ARG){
      const struct cl_enum *e = &mtcsOptions->clEnums/*!cl_enums*/[option->var_enum];
      unsigned int i;
      char *s;
      auto_diagnostic_group d;
      n_debug("mtcsopts.c cmdline_handle_error 00 %s\n",opt);
      if (e->unknown_error)
          error_at (loc, e->unknown_error, arg);
      else
          error_at (loc, "unrecognized argument in option %qs", opt);

      auto_vec <const char *> candidates;
      for (i = 0; e->values[i].arg != NULL; i++){
          if (!enum_arg_ok_for_language(self,&e->values[i], lang_mask))
            continue;
          candidates.safe_push (e->values[i].arg);
      }
      const char *hint = candidates_list_and_hint (arg, s, candidates);
      if (hint)
          inform (loc, "valid arguments to %qs are: %s; did you mean %qs?", option->opt_text, s, hint);
      else
          inform (loc, "valid arguments to %qs are: %s", option->opt_text, s);
      XDELETEVEC (s);

      return true;
  }

  return false;
}



/* Handle the switch DECODED (location LOC) for the language indicated
   by LANG_MASK, using the handlers in *HANDLERS and setting fields in
   OPTS and OPTS_SET and using diagnostic context DC (if not NULL) for
   diagnostic options.  */
//原型 read_cmdline_option opts.h opts-common.cc
void mtcs_opts_read_cmdline_option (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,struct cl_decoded_option *decoded,
             location_t loc,unsigned int lang_mask,const struct mtcs_cl_option_handlers *handlers,diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  const struct cl_option *option;
  const char *opt = decoded->orig_option_with_args_text;
  n_debug("mtcsopts.c mtcs_opts_read_cmdline_option 00 opt is :%s %s %d\n",opt,decoded->warn_message,decoded->errors);
  if (decoded->warn_message)
    warning_at (loc, 0, decoded->warn_message, opt);

  int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,decoded->opt_index);
  n_debug("mtcsopts.c mtcs_opts_read_cmdline_option 11 hostOptcode:%d %d %d %d\n",
        hostOptcode,OPT_SPECIAL_unknown,OPT_SPECIAL_ignore,OPT_SPECIAL_warn_removed);

  if (hostOptcode/*!decoded->opt_index*/ == OPT_SPECIAL_unknown){
      if (handlers->unknown_option_callback (decoded,handlers->userData))
          error_at (loc, "unrecognized command-line option %qs", decoded->arg);
      return;
  }
  if (hostOptcode/*!decoded->opt_index*/ == OPT_SPECIAL_ignore)
    return;
  if (hostOptcode/*!decoded->opt_index*/  == OPT_SPECIAL_warn_removed){
      /* Warn only about positive ignored options.  */
      if (decoded->value)
          warning_at (loc, 0, "switch %qs is no longer supported", opt);
      return;
  }
  n_debug("mtcsopts.c mtcs_opts_read_cmdline_option 22 hostOptcode:%d %d %d %d %s decoded->errors:%d\n",
        hostOptcode,OPT_SPECIAL_unknown,OPT_SPECIAL_ignore,OPT_SPECIAL_warn_removed,decoded->orig_option_with_args_text,decoded->errors);

  option = &mtcsOptions->clOptions/*!cl_options*/[decoded->opt_index];
  if (decoded->errors
      && cmdline_handle_error(self,loc, option, opt, decoded->arg, decoded->errors, lang_mask))
    return;

  if (decoded->errors & CL_ERR_WRONG_LANG){
      handlers->wrong_lang_callback (decoded, lang_mask,handlers->userData);
      return;
  }
  n_debug("mtcsopts.c mtcs_opts_read_cmdline_option 33 hostOptcode:%d %d %d %d\n",
        hostOptcode,OPT_SPECIAL_unknown,OPT_SPECIAL_ignore,OPT_SPECIAL_warn_removed);

  gcc_assert (!decoded->errors);
  if (!handle_option(self,opts, opts_set, decoded, lang_mask, DK_UNSPECIFIED,loc, handlers, false, dc))
    error_at (loc, "unrecognized command-line option %qs", opt);
}

/* Return the string name describing a sanitizer argument which has been
   provided on the command line and has set this particular flag.  */
//原型 find_sanitizer_argument opts.cc
static const char *mtcs_find_sanitizer_argument (MtcsOptionsItem *opts, unsigned int flags)
{
  for (int i = 0; sanitizer_opts[i].name != NULL; ++i){
      /* Need to find the sanitizer_opts element which:
     a) Could have set the flags requested.
     b) Has been set on the command line.

     Can have (a) without (b) if the flag requested is e.g.
     SANITIZE_ADDRESS, since both -fsanitize=address and
     -fsanitize=kernel-address set this flag.

     Can have (b) without (a) by requesting more than one sanitizer on the
     command line.  */
      if ((sanitizer_opts[i].flag & opts->x_flag_sanitize)!= sanitizer_opts[i].flag)
          continue;
      if ((sanitizer_opts[i].flag & flags) != flags)
          continue;
      return sanitizer_opts[i].name;
  }
  return NULL;
}

/* Report an error to the user about sanitizer options they have requested
   which have set conflicting flags.

   LEFT and RIGHT indicate sanitizer flags which conflict with each other, this
   function reports an error if both have been set in OPTS->x_flag_sanitize and
   ensures the error identifies the requested command line options that have
   set these flags.  */
//原型 report_conflicting_sanitizer_options opts.cc
static void report_conflicting_sanitizer_options (MtcsOptionsItem *opts, location_t loc,
                      unsigned int left, unsigned int right)
{
  unsigned int left_seen = (opts->x_flag_sanitize & left);
  unsigned int right_seen = (opts->x_flag_sanitize & right);
  if (left_seen && right_seen){
      const char* left_arg = mtcs_find_sanitizer_argument (opts, left_seen);
      const char* right_arg = mtcs_find_sanitizer_argument (opts, right_seen);
      gcc_assert (left_arg && right_arg);
      error_at (loc,"%<-fsanitize=%s%> is incompatible with %<-fsanitize=%s%>",left_arg, right_arg);
  }
}

/* Control IPA optimizations based on different live patching LEVEL.  */
static void control_options_for_live_patching (MtcsOptionsItem *opts,
        MtcsOptionsItem *opts_set,enum live_patching_level level,location_t loc)
{

  gcc_assert (level > LIVE_PATCHING_NONE);
  switch (level){
    case LIVE_PATCHING_INLINE_ONLY_STATIC:
#define LIVE_PATCHING_OPTION "-flive-patching=inline-only-static"
      if (opts_set->x_flag_ipa_cp_clone && opts->x_flag_ipa_cp_clone)
          error_at (loc, "%qs is incompatible with %qs","-fipa-cp-clone", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_cp_clone = 0;

      if (opts_set->x_flag_ipa_sra && opts->x_flag_ipa_sra)
          error_at (loc, "%qs is incompatible with %qs","-fipa-sra", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_sra = 0;

      if (opts_set->x_flag_partial_inlining && opts->x_flag_partial_inlining)
          error_at (loc, "%qs is incompatible with %qs","-fpartial-inlining", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_partial_inlining = 0;

      if (opts_set->x_flag_ipa_cp && opts->x_flag_ipa_cp)
          error_at (loc, "%qs is incompatible with %qs","-fipa-cp", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_cp = 0;

      /* FALLTHROUGH.  */
    case LIVE_PATCHING_INLINE_CLONE:
#undef LIVE_PATCHING_OPTION
#define LIVE_PATCHING_OPTION "-flive-patching=inline-only-static|inline-clone"
      /* live patching should disable whole-program optimization.  */
      if (opts_set->x_flag_whole_program && opts->x_flag_whole_program)
          error_at (loc, "%qs is incompatible with %qs","-fwhole-program", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_whole_program = 0;

      /* visibility change should be excluded by !flag_whole_program
     && !in_lto_p && !flag_ipa_cp_clone && !flag_ipa_sra
     && !flag_partial_inlining.  */

      if (opts_set->x_flag_ipa_pta && opts->x_flag_ipa_pta)
          error_at (loc, "%qs is incompatible with %qs","-fipa-pta", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_pta = 0;

      if (opts_set->x_flag_ipa_reference && opts->x_flag_ipa_reference)
          error_at (loc, "%qs is incompatible with %qs","-fipa-reference", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_reference = 0;

      if (opts_set->x_flag_ipa_ra && opts->x_flag_ipa_ra)
          error_at (loc, "%qs is incompatible with %qs","-fipa-ra", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_ra = 0;

      if (opts_set->x_flag_ipa_icf && opts->x_flag_ipa_icf)
          error_at (loc, "%qs is incompatible with %qs","-fipa-icf", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_icf = 0;

      if (opts_set->x_flag_ipa_icf_functions && opts->x_flag_ipa_icf_functions)
          error_at (loc, "%qs is incompatible with %qs","-fipa-icf-functions", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_icf_functions = 0;

      if (opts_set->x_flag_ipa_icf_variables && opts->x_flag_ipa_icf_variables)
          error_at (loc, "%qs is incompatible with %qs","-fipa-icf-variables", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_icf_variables = 0;

      if (opts_set->x_flag_ipa_bit_cp && opts->x_flag_ipa_bit_cp)
          error_at (loc, "%qs is incompatible with %qs","-fipa-bit-cp", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_bit_cp = 0;

      if (opts_set->x_flag_ipa_vrp && opts->x_flag_ipa_vrp)
          error_at (loc, "%qs is incompatible with %qs","-fipa-vrp", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_vrp = 0;

      if (opts_set->x_flag_ipa_pure_const && opts->x_flag_ipa_pure_const)
          error_at (loc, "%qs is incompatible with %qs","-fipa-pure-const", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_pure_const = 0;

      if (opts_set->x_flag_ipa_modref && opts->x_flag_ipa_modref)
          error_at (loc,"%<-fipa-modref%> is incompatible with %qs",LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_modref = 0;

      /* FIXME: disable unreachable code removal.  */

      /* discovery of functions/variables with no address taken.  */
      if (opts_set->x_flag_ipa_reference_addressable && opts->x_flag_ipa_reference_addressable)
          error_at (loc, "%qs is incompatible with %qs","-fipa-reference-addressable", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_reference_addressable = 0;

      /* ipa stack alignment propagation.  */
      if (opts_set->x_flag_ipa_stack_alignment  && opts->x_flag_ipa_stack_alignment)
          error_at (loc, "%qs is incompatible with %qs","-fipa-stack-alignment", LIVE_PATCHING_OPTION);
      else
          opts->x_flag_ipa_stack_alignment = 0;
      break;
    default:
      gcc_unreachable ();
  }

#undef LIVE_PATCHING_OPTION
}



/* After all options at LOC have been read into OPTS and OPTS_SET,
   finalize settings of those options and diagnose incompatible
   combinations.  */
//原型 finish_options opts.h opts.cc
void mtcs_opts_finish_options (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,location_t loc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  if (opts->x_dump_base_name && ! opts->x_dump_base_name_prefixed){
      const char *sep = opts->x_dump_base_name;
      for (; *sep; sep++)
        if (IS_DIR_SEPARATOR (*sep))
          break;

      if (*sep)
        /* If dump_base_path contains subdirectories, don't prepend
           anything.  */;
      else if (opts->x_dump_dir_name)
        /* We have a DUMP_DIR_NAME, prepend that.  */
        opts->x_dump_base_name = opts_concat (opts->x_dump_dir_name,opts->x_dump_base_name, NULL);

      /* It is definitely prefixed now.  */
      opts->x_dump_base_name_prefixed = true;
  }

  /* Handle related options for unit-at-a-time, toplevel-reorder, and
     section-anchors.  */
  if (!opts->x_flag_unit_at_a_time){
      if (opts->x_flag_section_anchors && opts_set->x_flag_section_anchors)
          error_at (loc, "section anchors must be disabled when unit-at-a-time is disabled");
      opts->x_flag_section_anchors = 0;
      if (opts->x_flag_toplevel_reorder == 1)
          error_at (loc, "toplevel reorder must be disabled when unit-at-a-time is disabled");
      opts->x_flag_toplevel_reorder = 0;
  }

  /* -fself-test depends on the state of the compiler prior to
     compiling anything.  Ideally it should be run on an empty source
     file.  However, in case we get run with actual source, assume
     -fsyntax-only which will inhibit any compiler initialization
     which may confuse the self tests.  */
  if (opts->x_flag_self_test)
    opts->x_flag_syntax_only = 1;

  if (opts->x_flag_tm && opts->x_flag_non_call_exceptions)
    sorry ("transactional memory is not supported with non-call exceptions");

  /* Unless the user has asked for section anchors, we disable toplevel
     reordering at -O0 to disable transformations that might be surprising
     to end users and to get -fno-toplevel-reorder tested.  */
  if (!opts->x_optimize  && opts->x_flag_toplevel_reorder == 2
      && !(opts->x_flag_section_anchors && opts_set->x_flag_section_anchors)) {
      opts->x_flag_toplevel_reorder = 0;
      opts->x_flag_section_anchors = 0;
  }
  if (!opts->x_flag_toplevel_reorder){
      if (opts->x_flag_section_anchors && opts_set->x_flag_section_anchors)
          error_at (loc, "section anchors must be disabled when toplevel reorder is disabled");
      opts->x_flag_section_anchors = 0;
  }

  if (opts->x_flag_hardened){
      if (!opts_set->x_flag_auto_var_init)
          opts->x_flag_auto_var_init = AUTO_INIT_ZERO;
      else if (opts->x_flag_auto_var_init != AUTO_INIT_ZERO)
          warning_at (loc, OPT_Whardened,
            "%<-ftrivial-auto-var-init=zero%> is not enabled by %<-fhardened%> because it was specified on the command line");
  }

  if (!opts->x_flag_opts_finished){
      /* We initialize opts->x_flag_pie to -1 so that targets can set a
     default value.  */
      if (opts->x_flag_pie == -1){
          /* We initialize opts->x_flag_pic to -1 so that we can tell if
             -fpic, -fPIC, -fno-pic or -fno-PIC is used.  */
          if (opts->x_flag_pic == -1)
            opts->x_flag_pie = (opts->x_flag_hardened? /*-fPIE*/ 2 : DEFAULT_FLAG_PIE);
          else
            opts->x_flag_pie = 0;
      }
      /* If -fPIE or -fpie is used, turn on PIC.  */
      if (opts->x_flag_pie)
          opts->x_flag_pic = opts->x_flag_pie;
      else if (opts->x_flag_pic == -1)
          opts->x_flag_pic = 0;
      if (opts->x_flag_pic && !opts->x_flag_pie)
          opts->x_flag_shlib = 1;
      opts->x_flag_opts_finished = true;
  }

  /* We initialize opts->x_flag_stack_protect to -1 so that targets
     can set a default value.  With --enable-default-ssp or -fhardened
     the default is -fstack-protector-strong.  */
  if (opts->x_flag_stack_protect == -1){
      /* This should check FRAME_GROWS_DOWNWARD, but on some targets it's
     defined in such a way that it uses flag_stack_protect which can't
     be used here.  Moreover, some targets like BPF don't support
     -fstack-protector at all but we don't know that here.  So remember
     that flag_stack_protect was set at the behest of -fhardened.  */
      if (opts->x_flag_hardened){
          opts->x_flag_stack_protect = SPCT_FLAG_STRONG;
          self->flag_stack_protector_set_by_fhardened_p = true;
      }else
          opts->x_flag_stack_protect = DEFAULT_FLAG_SSP;
  }else if (opts->x_flag_hardened   && opts->x_flag_stack_protect != SPCT_FLAG_STRONG)
    warning_at (UNKNOWN_LOCATION, OPT_Whardened,
        "%<-fstack-protector-strong%> is not enabled by %<-fhardened%> because it was specified on the command line");

  if (opts->x_optimize == 0){
      /* Inlining does not work if not optimizing,
     so force it not to be done.  */
      opts->x_warn_inline = 0;
      opts->x_flag_no_inline = 1;
  }

  /* At -O0 or -Og, turn __builtin_unreachable into a trap.  */
  if (!opts->x_optimize || opts->x_optimize_debug)
    SET_OPTION_IF_UNSET (opts, opts_set, flag_unreachable_traps, true);

  /* Pipelining of outer loops is only possible when general pipelining
     capabilities are requested.  */
  if (!opts->x_flag_sel_sched_pipelining)
    opts->x_flag_sel_sched_pipelining_outer_loops = 0;

  if (opts->x_flag_conserve_stack){
      SET_OPTION_IF_UNSET (opts, opts_set, param_large_stack_frame, 100);
      SET_OPTION_IF_UNSET (opts, opts_set, param_stack_frame_growth, 40);
  }

  if (opts->x_flag_lto){
#ifdef ENABLE_LTO
      opts->x_flag_generate_lto = 1;

      /* When generating IL, do not operate in whole-program mode.
     Otherwise, symbols will be privatized too early, causing link
     errors later.  */
      opts->x_flag_whole_program = 0;
#else
      error_at (loc, "LTO support has not been enabled in this configuration");
#endif
      if (!opts->x_flag_fat_lto_objects
              && (!HAVE_LTO_PLUGIN  || (opts_set->x_flag_use_linker_plugin  && !opts->x_flag_use_linker_plugin))){
          if (opts_set->x_flag_fat_lto_objects)
            error_at (loc, "%<-fno-fat-lto-objects%> are supported only with linker plugin");
          opts->x_flag_fat_lto_objects = 1;
      }

      /* -gsplit-dwarf isn't compatible with LTO, see PR88389.  */
      if (opts->x_dwarf_split_debug_info){
          inform (loc, "%<-gsplit-dwarf%> is not supported with LTO, disabling");
          opts->x_dwarf_split_debug_info = 0;
      }
  }

  /* We initialize opts->x_flag_split_stack to -1 so that targets can set a
     default value if they choose based on other options.  */
  if (opts->x_flag_split_stack == -1)
    opts->x_flag_split_stack = 0;
  else if (opts->x_flag_split_stack){
      if (!target_common_supports_split_stack/*!targetm_common.supports_split_stack*/(mtcsMachine->common,true, opts)){
          error_at (loc, "%<-fsplit-stack%> is not supported by this compiler configuration");
          opts->x_flag_split_stack = 0;
      }
  }

  /* If stack splitting is turned on, and the user did not explicitly
     request function partitioning, turn off partitioning, as it
     confuses the linker when trying to handle partitioned split-stack
     code that calls a non-split-stack functions.  But if partitioning
     was turned on explicitly just hope for the best.  */
  if (opts->x_flag_split_stack && opts->x_flag_reorder_blocks_and_partition)
    SET_OPTION_IF_UNSET (opts, opts_set, flag_reorder_blocks_and_partition, 0);

  if (opts->x_flag_reorder_blocks_and_partition)
    SET_OPTION_IF_UNSET (opts, opts_set, flag_reorder_functions, 1);

  /* The -gsplit-dwarf option requires -ggnu-pubnames.  */
  if (opts->x_dwarf_split_debug_info)
    opts->x_debug_generate_pub_sections = 2;

  if ((opts->x_flag_sanitize  & (SANITIZE_USER_ADDRESS | SANITIZE_KERNEL_ADDRESS)) == 0){
      if (opts->x_flag_sanitize & SANITIZE_POINTER_COMPARE)
        error_at (loc,
              "%<-fsanitize=pointer-compare%> must be combined with %<-fsanitize=address%> or %<-fsanitize=kernel-address%>");
      if (opts->x_flag_sanitize & SANITIZE_POINTER_SUBTRACT)
        error_at (loc,
              "%<-fsanitize=pointer-subtract%> must be combined with "
              "%<-fsanitize=address%> or %<-fsanitize=kernel-address%>");
  }

  /* Address sanitizers conflict with the thread sanitizer.  */
  report_conflicting_sanitizer_options (opts, loc, SANITIZE_THREAD,SANITIZE_ADDRESS);
  report_conflicting_sanitizer_options (opts, loc, SANITIZE_THREAD,SANITIZE_HWADDRESS);
  /* The leak sanitizer conflicts with the thread sanitizer.  */
  report_conflicting_sanitizer_options (opts, loc, SANITIZE_LEAK, SANITIZE_THREAD);

  /* No combination of HWASAN and ASAN work together.  */
  report_conflicting_sanitizer_options (opts, loc,SANITIZE_HWADDRESS, SANITIZE_ADDRESS);

  /* The userspace and kernel address sanitizers conflict with each other.  */
  report_conflicting_sanitizer_options (opts, loc, SANITIZE_USER_HWADDRESS,SANITIZE_KERNEL_HWADDRESS);
  report_conflicting_sanitizer_options (opts, loc, SANITIZE_USER_ADDRESS,SANITIZE_KERNEL_ADDRESS);

  /* Check error recovery for -fsanitize-recover option.  */
  for (int i = 0; sanitizer_opts[i].name != NULL; ++i)
    if ((opts->x_flag_sanitize_recover & sanitizer_opts[i].flag)
    && !sanitizer_opts[i].can_recover)
      error_at (loc, "%<-fsanitize-recover=%s%> is not supported",
        sanitizer_opts[i].name);

  /* Check -fsanitize-trap option.  */
  for (int i = 0; sanitizer_opts[i].name != NULL; ++i)
      if ((opts->x_flag_sanitize_trap & sanitizer_opts[i].flag)
        && !sanitizer_opts[i].can_trap
        /* Allow -fsanitize-trap=all or -fsanitize-trap=undefined
         to set flag_sanitize_trap & SANITIZE_VPTR bit which will
         effectively disable -fsanitize=vptr, just disallow
         explicit -fsanitize-trap=vptr.  */
        && sanitizer_opts[i].flag != SANITIZE_VPTR)
         error_at (loc, "%<-fsanitize-trap=%s%> is not supported",sanitizer_opts[i].name);

  /* When instrumenting the pointers, we don't want to remove
     the null pointer checks.  */
  if (opts->x_flag_sanitize & (SANITIZE_NULL | SANITIZE_NONNULL_ATTRIBUTE | SANITIZE_RETURNS_NONNULL_ATTRIBUTE))
    opts->x_flag_delete_null_pointer_checks = 0;

  /* Aggressive compiler optimizations may cause false negatives.  */
  if (opts->x_flag_sanitize & ~(SANITIZE_LEAK | SANITIZE_UNREACHABLE))
    opts->x_flag_aggressive_loop_optimizations = 0;

  /* Enable -fsanitize-address-use-after-scope if either address sanitizer is
     enabled.  */
  if (opts->x_flag_sanitize & (SANITIZE_USER_ADDRESS | SANITIZE_USER_HWADDRESS))
    SET_OPTION_IF_UNSET (opts, opts_set, flag_sanitize_address_use_after_scope,true);

  /* Force -fstack-reuse=none in case -fsanitize-address-use-after-scope
     is enabled.  */
  if (opts->x_flag_sanitize_address_use_after_scope){
      if (opts->x_flag_stack_reuse != SR_NONE && opts_set->x_flag_stack_reuse != SR_NONE)
        error_at (loc,
              "%<-fsanitize-address-use-after-scope%> requires %<-fstack-reuse=none%> option");

      opts->x_flag_stack_reuse = SR_NONE;
  }

  if ((opts->x_flag_sanitize & SANITIZE_USER_ADDRESS) && opts->x_flag_tm)
    sorry ("transactional memory is not supported with %<-fsanitize=address%>");

  if ((opts->x_flag_sanitize & SANITIZE_KERNEL_ADDRESS) && opts->x_flag_tm)
    sorry ("transactional memory is not supported with %<-fsanitize=kernel-address%>");

  /* Currently live patching is not support for LTO.  */
  if (opts->x_flag_live_patching == LIVE_PATCHING_INLINE_ONLY_STATIC && opts->x_flag_lto)
    sorry ("live patching (with %qs) is not supported with LTO","inline-only-static");

  /* Currently vtable verification is not supported for LTO */
  if (opts->x_flag_vtable_verify && opts->x_flag_lto)
    sorry ("vtable verification is not supported with LTO");

  /* Control IPA optimizations based on different -flive-patching level.  */
  if (opts->x_flag_live_patching)
    control_options_for_live_patching (opts, opts_set,opts->x_flag_live_patching,loc);

  /* Allow cunroll to grow size accordingly.  */
  if (!opts_set->x_flag_cunroll_grow_size)
    opts->x_flag_cunroll_grow_size = (opts->x_flag_unroll_loops || opts->x_flag_peel_loops || opts->x_optimize >= 3);

  /* With -fcx-limited-range, we do cheap and quick complex arithmetic.  */
  if (opts->x_flag_cx_limited_range)
    opts->x_flag_complex_method = 0;
  else if (opts_set->x_flag_cx_limited_range)
    opts->x_flag_complex_method = opts->x_flag_default_complex_method;

  /* With -fcx-fortran-rules, we do something in-between cheap and C99.  */
  if (opts->x_flag_cx_fortran_rules)
    opts->x_flag_complex_method = 1;
  else if (opts_set->x_flag_cx_fortran_rules)
    opts->x_flag_complex_method = opts->x_flag_default_complex_method;

  /* Use -fvect-cost-model=cheap instead of -fvect-cost-mode=very-cheap
     by default with explicit -ftree-{loop,slp}-vectorize.  */
  if (opts->x_optimize == 2
      && (opts_set->x_flag_tree_loop_vectorize
      || opts_set->x_flag_tree_vectorize))
    SET_OPTION_IF_UNSET (opts, opts_set, flag_vect_cost_model,VECT_COST_MODEL_CHEAP);

  if (opts->x_flag_gtoggle)
    {
      /* Make sure to process -gtoggle only once.  */
      opts->x_flag_gtoggle = false;
      if (opts->x_debug_info_level == DINFO_LEVEL_NONE)
    {
      opts->x_debug_info_level = DINFO_LEVEL_NORMAL;

      if (opts->x_write_symbols == NO_DEBUG)
        opts->x_write_symbols = PREFERRED_DEBUGGING_TYPE;
    }
      else
    opts->x_debug_info_level = DINFO_LEVEL_NONE;
    }

  if (!opts_set->x_debug_nonbind_markers_p)
    opts->x_debug_nonbind_markers_p
      = (opts->x_optimize
     && opts->x_debug_info_level >= DINFO_LEVEL_NORMAL
     && mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(self,opts)
     && !(opts->x_flag_selective_scheduling
          || opts->x_flag_selective_scheduling2));

  /* We know which debug output will be used so we can set flag_var_tracking
     and flag_var_tracking_uninit if the user has not specified them.  */
  if (opts->x_debug_info_level < DINFO_LEVEL_NORMAL
      || !mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(self,opts)
      /* We have not yet initialized debug hooks so match that to check
     whether we're only doing DWARF2_LINENO_DEBUGGING_INFO.  */
#ifndef DWARF2_DEBUGGING_INFO
      || true
#endif
     )
    {
      if ((opts_set->x_flag_var_tracking && opts->x_flag_var_tracking == 1)
      || (opts_set->x_flag_var_tracking_uninit
          && opts->x_flag_var_tracking_uninit == 1))
    {
      if (opts->x_debug_info_level < DINFO_LEVEL_NORMAL)
        warning_at (UNKNOWN_LOCATION, 0,
            "variable tracking requested, but useless unless producing debug info");
      else
        warning_at (UNKNOWN_LOCATION, 0,
            "variable tracking requested, but not supported  by this debug format");
    }
      opts->x_flag_var_tracking = 0;
      opts->x_flag_var_tracking_uninit = 0;
      opts->x_flag_var_tracking_assignments = 0;
    }

  /* One could use EnabledBy, but it would lead to a circular dependency.  */
  if (!opts_set->x_flag_var_tracking_uninit)
    opts->x_flag_var_tracking_uninit = opts->x_flag_var_tracking;

  if (!opts_set->x_flag_var_tracking_assignments)
    opts->x_flag_var_tracking_assignments
      = (opts->x_flag_var_tracking
     && !(opts->x_flag_selective_scheduling
          || opts->x_flag_selective_scheduling2));

  if (opts->x_flag_var_tracking_assignments_toggle)
    opts->x_flag_var_tracking_assignments
      = !opts->x_flag_var_tracking_assignments;

  if (opts->x_flag_var_tracking_assignments && !opts->x_flag_var_tracking)
    opts->x_flag_var_tracking = opts->x_flag_var_tracking_assignments = -1;

  if (opts->x_flag_var_tracking_assignments && (opts->x_flag_selective_scheduling
      || opts->x_flag_selective_scheduling2))
    warning_at (loc, 0,"var-tracking-assignments changes selective scheduling");

  if (opts->x_flag_syntax_only){
      opts->x_write_symbols = NO_DEBUG;
      opts->x_profile_flag = 0;
  }

  if (opts->x_warn_strict_flex_arrays)
    if (opts->x_flag_strict_flex_arrays == 0){
        opts->x_warn_strict_flex_arrays = 0;
        warning_at (UNKNOWN_LOCATION, 0,
                "%<-Wstrict-flex-arrays%> is ignored when %<-fstrict-flex-arrays%> is not present");
    }

  mtcs_opts_diagnose_options/*!diagnose_options*/(self,opts, opts_set, loc);
}

/* The function diagnoses incompatible combinations for provided options
   (OPTS and OPTS_SET) at a given LOCation.  The function is called both
   when command line is parsed (after the target optimization hook) and
   when an optimize/target attribute (or pragma) is used.  */
//原型 diagnose_options opts.h opts.cc
void mtcs_opts_diagnose_options (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,location_t loc)
{

  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  /* The optimization to partition hot and cold basic blocks into separate
     sections of the .o and executable files does not work (currently)
     with exception handling.  This is because there is no support for
     generating unwind info.  If opts->x_flag_exceptions is turned on
     we need to turn off the partitioning optimization.  */

  enum unwind_info_type ui_except=target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,opts);

  if (opts->x_flag_exceptions  && opts->x_flag_reorder_blocks_and_partition
      && (ui_except == UI_SJLJ || ui_except >= UI_TARGET)){
      if (opts_set->x_flag_reorder_blocks_and_partition)
          inform (loc,"%<-freorder-blocks-and-partition%> does not work with exceptions on this architecture");
      opts->x_flag_reorder_blocks_and_partition = 0;
      opts->x_flag_reorder_blocks = 1;
  }

  /* If user requested unwind info, then turn off the partitioning
     optimization.  */

  if (opts->x_flag_unwind_tables
      && !mtcsMachine->common->unwind_tables_default/*!targetm_common.unwind_tables_default*/
      && opts->x_flag_reorder_blocks_and_partition  && (ui_except == UI_SJLJ || ui_except >= UI_TARGET)){
      if (opts_set->x_flag_reorder_blocks_and_partition)
          inform (loc,"%<-freorder-blocks-and-partition%> does not support unwind info on this architecture");
      opts->x_flag_reorder_blocks_and_partition = 0;
      opts->x_flag_reorder_blocks = 1;
  }

  /* If the target requested unwind info, then turn off the partitioning
     optimization with a different message.  Likewise, if the target does not
     support named sections.  */

  if (opts->x_flag_reorder_blocks_and_partition
      && (!mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/
      || (opts->x_flag_unwind_tables
          && mtcsMachine->common->unwind_tables_default/*!targetm_common.unwind_tables_default*/
          && (ui_except == UI_SJLJ || ui_except >= UI_TARGET)))){
      if (opts_set->x_flag_reorder_blocks_and_partition)
          inform (loc,"%<-freorder-blocks-and-partition%> does not work on this architecture");
      opts->x_flag_reorder_blocks_and_partition = 0;
      opts->x_flag_reorder_blocks = 1;
  }
}

/* Return TRUE iff dwarf2 debug info is enabled.  */
//原型 dwarf_debuginfo_p flags.h opts.cc
bool mtcs_opts_dwarf_debuginfo_p (MtcsOpts *self,MtcsOptionsItem *opts)
{
  return (opts->x_write_symbols & DWARF2_DEBUG);
}

/* Process common options that have been deferred until after the
   handlers have been called for all options.  */
//原型 handle_common_deferred_options opts.h opts-global.cc
void mtcs_opts_handle_common_deferred_options (MtcsOpts *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  unsigned int i;
  cl_deferred_option *opt;
  vec<cl_deferred_option> v;

  //原型 options.h中声明
  //void *x_common_deferred_options;
  //#define common_deferred_options global_options.x_common_deferred_options

  if (mtcsOptionsItem->x_common_deferred_options/*!common_deferred_options*/)
    v = *((vec<cl_deferred_option> *) mtcsOptionsItem->x_common_deferred_options/*!common_deferred_options*/);
  else
    v = vNULL;
  n_debug("mtcsopts.c handle_common_deferred_options ---- 00 v:%p common_deferred_options:%p\n",&v,mtcsOptionsItem->x_common_deferred_options);

  if (mtcsOptionsItem->x_flag_dump_all_passed/*!flag_dump_all_passed*/)
    enable_rtl_dump_file ();

  if (mtcsOptionsItem->x_flag_opt_info/*!flag_opt_info*/)
    opt_info_switch_p (NULL);

  self->flag_canon_prefix_map = false;
  FOR_EACH_VEC_ELT (v, i, opt){
      int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,opt->opt_index);
      switch (hostOptcode/*!opt->opt_index*/){
        case OPT_fcall_used_:
          mtcs_reg_fix_register/*!fix_register*/(mtcsReg,opt->arg, 0, 1);
          break;

        case OPT_fcall_saved_:
           mtcs_reg_fix_register/*!fix_register*/(mtcsReg,opt->arg, 0, 0);
          break;

        case OPT_fdbg_cnt_:
          dbg_cnt_process_opt (opt->arg);
          break;

        case OPT_fdebug_prefix_map_:
          add_debug_prefix_map (opt->arg);
          break;

        case OPT_ffile_prefix_map_:
          add_file_prefix_map (opt->arg);
          break;

        case OPT_fprofile_prefix_map_:
          add_profile_prefix_map (opt->arg);
          break;

        case OPT_fcanon_prefix_map:
          self->flag_canon_prefix_map = opt->value;
          break;

        case OPT_fdump_:
          /* Deferred until plugins initialized.  */
          break;

            case OPT_fopt_info_:
          if (!opt_info_switch_p (opt->arg))
            error ("unrecognized command-line option %<-fopt-info-%s%>",
                       opt->arg);
              break;

        case OPT_fenable_:
        case OPT_fdisable_:
          if (opt->opt_index == OPT_fenable_)
            enable_pass (opt->arg);
              else
            disable_pass (opt->arg);
              break;

        case OPT_ffixed_:
          /* Deferred.  */
           mtcs_reg_fix_register/*!fix_register*/(mtcsReg,opt->arg, 1, 1);
          break;

        case OPT_fplugin_:
    #ifdef ENABLE_PLUGIN
          add_new_plugin (opt->arg);
    #else
          error ("plugin support is disabled; configure with %<--enable-plugin%>");
    #endif
          break;

        case OPT_fplugin_arg_:
    #ifdef ENABLE_PLUGIN
          parse_plugin_arg_opt (opt->arg);
    #else
          error ("plugin support is disabled; configure with %<--enable-plugin%>");
    #endif
          break;

        case OPT_frandom_seed:
          /* The real switch is -fno-random-seed.  */
          if (!opt->value)
            set_random_seed (NULL);
          break;

        case OPT_frandom_seed_:
          set_random_seed (opt->arg);
          break;

        case OPT_fstack_limit:
          /* The real switch is -fno-stack-limit.  */
          if (!opt->value)
              mtcsRTL->stack_limit_rtx/*!stack_limit_rtx*/ = NULL_RTX;
          break;

        case OPT_fstack_limit_register_:
          {
            int reg = decode_reg_name (opt->arg);
            if (reg < 0)
              error ("unrecognized register name %qs", opt->arg);
            else
              {
            /* Deactivate previous OPT_fstack_limit_symbol_ options.  */
            self->opt_fstack_limit_symbol_arg = NULL;
            self->opt_fstack_limit_register_no = reg;
              }
          }
          break;

        case OPT_fstack_limit_symbol_:
          /* Deactivate previous OPT_fstack_limit_register_ options.  */
          self->opt_fstack_limit_register_no = -1;
          self->opt_fstack_limit_symbol_arg = opt->arg;
          break;

        case OPT_fasan_shadow_offset_:
          if (!(mtcsOptionsItem->x_flag_sanitize & SANITIZE_KERNEL_ADDRESS))
            error ("%<-fasan-shadow-offset%> should only be used "
               "with %<-fsanitize=kernel-address%>");
          if (!set_asan_shadow_offset (opt->arg))
             error ("unrecognized shadow offset %qs", opt->arg);
          break;

        case OPT_fsanitize_sections_:
          set_sanitized_sections (opt->arg);
          break;

        default:
          gcc_unreachable ();
      }
  }
}

/* Return true if the current target supports -fsection-anchors.  */
//原型 target_supports_section_anchors_p toplev.cc
static bool target_supports_section_anchors_p (MtcsOpts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   if (mtcsTarget/*!targetm.min_anchor_offset*/->min_anchor_offset== 0
   && mtcsTarget/*!targetm.max_anchor_offset*/->max_anchor_offset == 0)
      return false;

   if (mtcsMachine->asmOut->output_anchor/*!targetm.asm_out.output_anchor*/ == NULL)
      return false;

   return true;
}

/* Returns TRUE if generated code should match ABI version N or
   greater is in use.  */

#define LANG_GNU_C false

#define mtcs_abi_version_at_least(N) \
  (mtcsOptionsItem->x_flag_abi_version == 0 || mtcsOptionsItem->x_flag_abi_version >= (N))
/* Process the options that have been parsed.  */
//原型 process_options toplev.cc
void mtcs_opts_process_options(MtcsOpts *self,bool no_backend)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput  *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsConfig  *mtcsConfig=mtcs_target_get_config(mtcsTarget);
  MtcsFunc    *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsTree    *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
  MtcsDwarf2Out *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsOptionsItem *mtcsOptionsItem_Set=mtcsOptions->global_options_set;

  const char *language_string = "GNU GIMPLE";//lang_hooks.name;

  //maximum_field_alignment = initial_max_fld_align * BITS_PER_UNIT;

   char *pname="GCC";//get_process_name();
   n_debug("mtcsopts.c mtcs_opts_process_options 00 pid:%d name:%s have_named_sections:%d language_string:%s lang_GNU_C:%d\n",
          getpid(),pname,mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/,language_string,LANG_GNU_C);

   n_debug("mtcsopts.c mtcs_opts_process_options 11 %d %d\n",
          mtcsOptionsItem->x_flag_schedule_insns,mtcsOptionsItem->x_flag_schedule_insns_after_reload);
   n_debug("mtcsopts.c mtcs_opts_process_options 22 打印 cl_optimization_node 00:%p\n",self->cl_optimization_node);
  aet_print_tree(self->cl_optimization_node);
  /* Some machines may reject certain combinations of options.  */
  location_t saved_location = input_location;
  input_location = UNKNOWN_LOCATION;
  target_option_override/*!targetm.target_option.override*/(mtcsMachine->option);
  input_location = saved_location;
  n_debug("mtcsopts.c mtcs_opts_process_options 33 name:%s have_named_sections:%d language_string:%s lang_GNU_C:%d\n",
          pname,mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/,language_string,LANG_GNU_C);

  if (mtcsOptionsItem->x_flag_diagnostics_generate_patch/*!flag_diagnostics_generate_patch*/)
    global_dc->create_edit_context ();

  /* Avoid any informative notes in the second run of -fcompare-debug.  */
  if (mtcsOptionsItem->x_flag_compare_debug/*!flag_compare_debug*/)
    diagnostic_inhibit_notes (global_dc);

  if (mtcsOptionsItem->x_flag_section_anchors/*!flag_section_anchors*/ && !target_supports_section_anchors_p(self)){
      warning_at (UNKNOWN_LOCATION, OPT_fsection_anchors,"this target does not support %qs","-fsection-anchors");
      mtcsOptionsItem->x_flag_section_anchors/*!flag_section_anchors*/ = 0;
  }

  if (mtcsOptionsItem->x_flag_short_enums == 2)
      mtcsOptionsItem->x_flag_short_enums =mtcsTarget/*!targetm.default_short_enums*/->default_short_enums(mtcsTarget);

  /* Set aux_base_name if not already set.  */
  if (mtcsOptionsItem->x_aux_base_name/*!aux_base_name*/)
    ;
  else if (mtcsOptionsItem->x_dump_base_name/*!dump_base_name*/){
      const char *name = mtcsOptionsItem->x_dump_base_name/*!dump_base_name*/;
      int nlen, len;

      if (mtcsOptionsItem->x_dump_base_ext/*!dump_base_ext*/ && (len = strlen (mtcsOptionsItem->x_dump_base_ext))
        && (nlen = strlen (name)) && nlen > len
        && strcmp (name + nlen - len, mtcsOptionsItem->x_dump_base_ext) == 0){
          char *p = xstrndup (name, nlen - len);
          name = p;
      }
      mtcsOptionsItem->x_aux_base_name/*!aux_base_name*/ = name;
  }else
      mtcsOptionsItem->x_dump_base_name/*!dump_base_name*/ = "gccaux";

  n_debug("mtcsopts.c mtcs_opts_process_options 44 have_named_sections:%d language_string:%s lang_GNU_C:%d\n",
        mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/,language_string,LANG_GNU_C);

  if(!mtcs_config_ifdef(mtcsConfig, MTCS_HAVE_isl)){/*!#ifndef HAVE_isl*/
      if (mtcsOptionsItem->x_flag_graphite
          || mtcsOptionsItem->x_flag_loop_nest_optimize
          || mtcsOptionsItem->x_flag_graphite_identity
          || mtcsOptionsItem->x_flag_loop_parallelize_all)
        sorry ("Graphite loop optimizations cannot be used (isl is not available) "
           "(%<-fgraphite%>, %<-fgraphite-identity%>, %<-floop-nest-optimize%>, %<-floop-parallelize-all%>)");
  }/*!#endif*/

  if(mtcsOptionsItem->x_flag_cf_protection != CF_NONE  && !(mtcsOptionsItem->x_flag_cf_protection & CF_SET)){
      if (mtcsOptionsItem->x_flag_cf_protection == CF_FULL){
          error_at (UNKNOWN_LOCATION,"%<-fcf-protection=full%> is not supported for this target");
          mtcsOptionsItem->x_flag_cf_protection = CF_NONE;
      }
      if (mtcsOptionsItem->x_flag_cf_protection == CF_BRANCH){
          error_at (UNKNOWN_LOCATION,"%<-fcf-protection=branch%> is not supported for this target");
          mtcsOptionsItem->x_flag_cf_protection = CF_NONE;
      }
      if (mtcsOptionsItem->x_flag_cf_protection == CF_RETURN){
          error_at (UNKNOWN_LOCATION, "%<-fcf-protection=return%> is not supported for this target");
          mtcsOptionsItem->x_flag_cf_protection = CF_NONE;
      }
  }
  n_debug("mtcsopts.c mtcs_opts_process_options 55 have_named_sections:%d language_string:%s lang_GNU_C:%d\n",
        mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/,language_string,LANG_GNU_C);
  /* One region RA really helps to decrease the code size.  */
  if (!mtcsOptionsItem_Set->x_flag_ira_region)
      mtcsOptionsItem->x_flag_ira_region =  mtcsOptionsItem->x_optimize_size || ! mtcsOptionsItem->x_optimize ? IRA_REGION_ONE : IRA_REGION_MIXED;

  if (!mtcs_abi_version_at_least/*!abi_version_at_least*/(2)){
      /* -fabi-version=1 support was removed after GCC 4.9.  */
      error_at (UNKNOWN_LOCATION,"%<-fabi-version=1%> is no longer supported");
      mtcsOptionsItem->x_flag_abi_version = 2;
  }

  if (mtcsOptionsItem->x_flag_non_call_exceptions)
      mtcsOptionsItem->x_flag_asynchronous_unwind_tables = 1;
  if (mtcsOptionsItem->x_flag_asynchronous_unwind_tables)
      mtcsOptionsItem->x_flag_unwind_tables = 1;

  if (mtcsOptionsItem->x_flag_value_profile_transformations)
      mtcsOptionsItem->x_flag_profile_values = 1;
  n_debug("mtcsopts.c mtcs_opts_process_options 66 打印 cl_optimization_node 11:%p\n",self->cl_optimization_node);
  aet_print_tree(self->cl_optimization_node);
  n_debug("mtcsopts.c process_options 11cc %d %d\n",
          mtcsOptionsItem->x_flag_schedule_insns,mtcsOptionsItem->x_flag_schedule_insns_after_reload);
  /* Warn about options that are not supported on this machine.  */
  if(!mtcs_config_ifdef(mtcsConfig, MTCS_INSN_SCHEDULING)){/*!#ifndef INSN_SCHEDULING host=1 nvptx=0*/
     if ( mtcsOptionsItem->x_flag_schedule_insns ||  mtcsOptionsItem->x_flag_schedule_insns_after_reload)
        warning_at (UNKNOWN_LOCATION, 0,"instruction scheduling not supported on this target machine");
  }//#endif

  n_debug("mtcsopts.c mtcs_opts_process_options 77 have_named_sections:%d language_string:%s lang_GNU_C:%d\n",
        mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/,language_string,LANG_GNU_C);
  //DELAY_SLOTS insn-attr-common.h host=0 nvptx=0
  if (!mtcs_insn_attr_get_delay_slots/*!DELAY_SLOTS*/(mtcsInsnAttr) &&  mtcsOptionsItem->x_flag_delayed_branch)
    warning_at (UNKNOWN_LOCATION, 0,"this target machine does not have delayed branches");

  mtcs_output_set_user_label_prefix/*!user_label_prefix = USER_LABEL_PREFIX;*/(mtcsOutput,USER_LABEL_PREFIX);
  if (mtcsOptionsItem->x_flag_leading_underscore != -1){
      /* If the default prefix is more complicated than "" or "_",
     issue a warning and ignore this option.  */
      if (mtcsOutput->user_label_prefix[0] == 0 ||
              (mtcsOutput->user_label_prefix[0] == '_' && mtcsOutput->user_label_prefix[1] == 0)){
          /*!user_label_prefix = mtcsOptionsItem->x_flag_leading_underscore ? "_" : "";*/
          mtcs_output_set_user_label_prefix(mtcsOutput,mtcsOptionsItem->x_flag_leading_underscore ? "_" : "");
      }else
          warning_at (UNKNOWN_LOCATION, 0,
                "%<-f%sleading-underscore%> not supported on this target machine", mtcsOptionsItem->x_flag_leading_underscore ? "" : "no-");
  }

  /* If we are in verbose mode, write out the version and maybe all the
     option flags in use.  */
  if (mtcsOptionsItem->x_version_flag){
      /* We already printed the version header in main ().  */
      if (!mtcsOptionsItem->x_quiet_flag){
         n_debug("mtcsopts.c mtcs_opts_process_options 88 have_named_sections:%d language_string:%s lang_GNU_C:%d\n",
               mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/,language_string,LANG_GNU_C);
          fputs ("options passed: ", stderr);
          char *cmdline =mtcs_opts_gen_command_line_string/*!gen_command_line_string*/
                  (self,self->save_decoded_options,self->save_decoded_options_count);
          n_debug("mtcsopts.c mtcs_opts_process_options 99\n");
          fputs (cmdline, stderr);
          free (cmdline);
          fputc ('\n', stderr);
      }
  }
  n_debug("mtcsopts.c mtcs_opts_process_options 100 打印 cl_optimization_node 22:%p\n",self->cl_optimization_node);
  aet_print_tree(self->cl_optimization_node);
  /* CTF is supported for only C at this time.  */
  if (!LANG_GNU_C/*!lang_GNU_C ()*/ && mtcsOptionsItem->x_ctf_debug_info_level > CTFINFO_LEVEL_NONE){
      /* Compiling with -flto results in frontend language of GNU GIMPLE.  It
     is not useful to warn in that case.  */
      if (!startswith (language_string/*!lang_hooks.name*/, "GNU GIMPLE"))
          inform (UNKNOWN_LOCATION,"CTF debug info requested, but not supported for %qs frontend",language_string);
      mtcsOptionsItem->x_ctf_debug_info_level = CTFINFO_LEVEL_NONE;
  }

  if (mtcsOptionsItem->x_flag_dump_final_insns && !mtcsOptionsItem->x_flag_syntax_only && !no_backend){
      FILE *final_output = fopen (mtcsOptionsItem->x_flag_dump_final_insns, "w");
      if (!final_output){
          error_at (UNKNOWN_LOCATION, "could not open final insn dump file %qs: %m",mtcsOptionsItem->x_flag_dump_final_insns);
          mtcsOptionsItem->x_flag_dump_final_insns = NULL;
      }else if (fclose (final_output)){
          error_at (UNKNOWN_LOCATION,"could not close zeroed insn dump file %qs: %m",mtcsOptionsItem->x_flag_dump_final_insns);
          mtcsOptionsItem->x_flag_dump_final_insns = NULL;
      }
  }

  /* A lot of code assumes write_symbols == NO_DEBUG if the debugging
     level is 0.  */
  if (mtcsOptionsItem->x_debug_info_level == DINFO_LEVEL_NONE
      && mtcsOptionsItem->x_ctf_debug_info_level == CTFINFO_LEVEL_NONE)
      mtcsOptionsItem->x_write_symbols = NO_DEBUG;
  //应该与主机一样 如果 编译参数 -g mtcs只能用mtcsdwarf2lineno,没有-g 用mtcsdonothingdebug。
  if (mtcsOptionsItem->x_write_symbols == NO_DEBUG){
     n_debug("mtcsopts.c mtcs_opts_process_options 101 使用 mtcsDoNothingDebug\n");
     MtcsDoNothingDebug *mtcsDoNothingDebug = mtcs_target_get_do_nothing_debug(mtcsTarget);
     mtcs_target_set_current_debug(mtcsTarget,(MtcsDebug *)mtcsDoNothingDebug);
  }else if(mtcs_config_ifdef(mtcsConfig,MTCS_DWARF2_DEBUGGING_INFO)){/*!#ifdef DWARF2_DEBUGGING_INFO*/
    if (mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(self,mtcsOptionsItem))
      n_error("还未实现 DWARF2_DEBUGGING_INFO ");
  }else if(mtcs_config_ifdef(mtcsConfig,MTCS_CTF_DEBUGGING_INFO)){/*!#ifdef CTF_DEBUGGING_INFO*/
    if (ctf_debuginfo_p ())
       n_error("还未实现 CTF_DEBUGGING_INFO ");
  }else if(mtcs_config_ifdef(mtcsConfig,MTCS_BTF_DEBUGGING_INFO)){/*!#ifdef BTF_DEBUGGING_INFO*/
    if (btf_debuginfo_p ())
       n_error("还未实现 BTF_DEBUGGING_INFO ");
  }else if(mtcs_config_ifdef(mtcsConfig,MTCS_VMS_DEBUGGING_INFO)){/*!#ifdef VMS_DEBUGGING_INFO*/
    if (mtcsOptionsItem->x_write_symbols == VMS_DEBUG || mtcsOptionsItem->x_write_symbols == VMS_AND_DWARF2_DEBUG)
       n_error("还未实现 VMS_DEBUGGING_INFO ");
  }else if(mtcs_config_ifdef(mtcsConfig,MTCS_DWARF2_LINENO_DEBUGGING_INFO)){/*!#ifdef DWARF2_LINENO_DEBUGGING_INFO*/
     if (mtcsOptionsItem->x_write_symbols == DWARF2_DEBUG){
        //fprintf(stderr,"使用 mtcsDwarf2Lineno\n");
         MtcsDwarf2Lineno *mtcsDwarf2Lineno = mtcs_target_get_dwarf2_lineno(mtcsTarget);
         mtcs_target_set_current_debug(mtcsTarget,(MtcsDebug*)mtcsDwarf2Lineno);
     }
  }else{
      gcc_assert (debug_set_count (mtcsOptionsItem->x_write_symbols) <= 1);
      error_at (UNKNOWN_LOCATION,"target system does not support the %qs debug format",
        debug_type_names[debug_set_to_format (mtcsOptionsItem->x_write_symbols)]);
  }

  /* The debug hooks are used to implement -fdump-go-spec because it
     gives a simple and stable API for all the information we need to
     dump.  */
  if (mtcsOptionsItem->x_flag_dump_go_spec != NULL)
     //debug_hooks = dump_go_spec_init (mtcsOptionsItem->x_flag_dump_go_spec, debug_hooks);
     n_error("还未实现 DUMP_EBUG");


  if (!mtcsOptionsItem_Set/*!OPTION_SET_P (dwarf2out_as_loc_support)*/->x_dwarf2out_as_loc_support)
      mtcsOptionsItem->x_dwarf2out_as_loc_support =
            mtcs_dwarf2_out_dwarf2out_default_as_loc_support/*!dwarf2out_default_as_loc_support*/(mtcsDwarf2Out);
  if (!mtcsOptionsItem_Set/*!OPTION_SET_P (dwarf2out_as_locview_support)*/->x_dwarf2out_as_locview_support)
      mtcsOptionsItem->x_dwarf2out_as_locview_support = dwarf2out_default_as_locview_support ();

  if (!mtcsOptionsItem_Set/*!OPTION_SET_P (debug_variable_location_views)*/->x_debug_variable_location_views){
      mtcsOptionsItem->x_debug_variable_location_views = (mtcsOptionsItem->x_flag_var_tracking
       && mtcsOptionsItem->x_debug_info_level >= DINFO_LEVEL_NORMAL
       && mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(self,mtcsOptionsItem)
       && !mtcsOptionsItem->x_dwarf_strict
       && mtcsOptionsItem->x_dwarf2out_as_loc_support
       && mtcsOptionsItem->x_dwarf2out_as_locview_support);
  }else if (mtcsOptionsItem->x_debug_variable_location_views == -1 && mtcsOptionsItem->x_dwarf_version != 5){
      warning_at (UNKNOWN_LOCATION, 0,"without %<-gdwarf-5%>, "
          "%<-gvariable-location-views=incompat5%> is equivalent to %<-gvariable-location-views%>");
      mtcsOptionsItem->x_debug_variable_location_views = 1;
  }

  if (mtcsOptionsItem->x_debug_internal_reset_location_views == 2){
      mtcsOptionsItem->x_debug_internal_reset_location_views = (mtcsOptionsItem->x_debug_variable_location_views
            && mtcsTarget/*!targetm.reset_location_view*/->reset_location_view);
  }else if (mtcsOptionsItem->x_debug_internal_reset_location_views
       && !mtcsOptionsItem->x_debug_variable_location_views){
      warning_at (UNKNOWN_LOCATION, 0,"%<-ginternal-reset-location-views%> is forced disabled without %<-gvariable-location-views%>");
      mtcsOptionsItem->x_debug_internal_reset_location_views = 0;
  }

  if (!mtcsOptionsItem_Set/*!OPTION_SET_P (debug_inline_points)*/->x_debug_inline_points)
      mtcsOptionsItem->x_debug_inline_points = mtcsOptionsItem->x_debug_variable_location_views;
  else if (mtcsOptionsItem->x_debug_inline_points && !mtcsOptionsItem->x_debug_nonbind_markers_p){
      warning_at (UNKNOWN_LOCATION, 0,"%<-ginline-points%> is forced disabled without %<-gstatement-frontiers%>");
      mtcsOptionsItem->x_debug_inline_points = 0;
  }

  if (!mtcsOptionsItem_Set/*!OPTION_SET_P (flag_tree_cselim)*/->x_flag_tree_cselim){
      if (HAVE_conditional_move)
          mtcsOptionsItem->x_flag_tree_cselim = 1;
      else
          mtcsOptionsItem->x_flag_tree_cselim = 0;
  }

  /* If auxiliary info generation is desired, open the output file.
     This goes in the same directory as the source file--unlike
     all the other output files.  */
  if (mtcsOptionsItem->x_flag_gen_aux_info){
      self->aux_info_file/*!aux_info_file*/ = fopen (mtcsOptionsItem->x_aux_info_file_name, "w");
      if (self->aux_info_file == 0)
          fatal_error (UNKNOWN_LOCATION,"cannot open %s: %m", mtcsOptionsItem->x_aux_info_file_name);
  }

  if (!mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/){
      if (mtcsOptionsItem->x_flag_function_sections){
          warning_at (UNKNOWN_LOCATION, 0,"%<-ffunction-sections%> not supported for this target");
          mtcsOptionsItem->x_flag_function_sections = 0;
      }
      if (mtcsOptionsItem->x_flag_data_sections){
          warning_at (UNKNOWN_LOCATION, 0,"%<-fdata-sections%> not supported for this target");
          mtcsOptionsItem->x_flag_data_sections = 0;
      }
  }

  if (mtcsOptionsItem->x_flag_prefetch_loop_arrays > 0
          && !mtcsMachine->tmrtx->code_for_prefetch/*!targetm.code_for_prefetch*/){
      warning_at (UNKNOWN_LOCATION, 0,"%<-fprefetch-loop-arrays%> not supported for this target");
      mtcsOptionsItem->x_flag_prefetch_loop_arrays = 0;
  }else if (mtcsOptionsItem->x_flag_prefetch_loop_arrays > 0
          && !target_rtx_have_prefetch/*!targetm.have_prefetch*/(mtcsMachine->tmrtx)){
      warning_at (UNKNOWN_LOCATION, 0, "%<-fprefetch-loop-arrays%> not supported for this target (try %<-march%> switches)");
      mtcsOptionsItem->x_flag_prefetch_loop_arrays = 0;
  }

  /* This combination of options isn't handled for i386 targets and doesn't
     make much sense anyway, so don't allow it.  */
  if (mtcsOptionsItem->x_flag_prefetch_loop_arrays > 0 && mtcsOptionsItem->x_optimize_size){
      warning_at (UNKNOWN_LOCATION, 0,"%<-fprefetch-loop-arrays%> is not supported with %<-Os%>");
      mtcsOptionsItem->x_flag_prefetch_loop_arrays = 0;
  }

  /* The presence of IEEE signaling NaNs, implies all math can trap.  */
  if (mtcsOptionsItem->x_flag_signaling_nans)
      mtcsOptionsItem->x_flag_trapping_math = 1;

  /* We cannot reassociate if we want traps or signed zeros.  */
  if (mtcsOptionsItem->x_flag_associative_math &&
          (mtcsOptionsItem->x_flag_trapping_math || mtcsOptionsItem->x_flag_signed_zeros)){
      warning_at (UNKNOWN_LOCATION, 0,"%<-fassociative-math%> disabled; other options take precedence");
      mtcsOptionsItem->x_flag_associative_math = 0;
  }

  if (mtcsOptionsItem->x_flag_hardened && !mtcs_config_if(mtcsConfig,MTCS_HAVE_FHARDENED_SUPPORT)){
      warning_at (UNKNOWN_LOCATION, 0, "%<-fhardened%> not supported for this target");
      mtcsOptionsItem->x_flag_hardened = 0;
  }

  /* -fstack-clash-protection is not currently supported on targets
     where the stack grows up.  */
  if (mtcsOptionsItem->x_flag_stack_clash_protection && !mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)){
      warning_at (UNKNOWN_LOCATION, 0,
          "%<-fstack-clash-protection%> is not supported on targets "
          "where the stack grows from lower to higher addresses");
      mtcsOptionsItem->x_flag_stack_clash_protection = 0;
  }else if (mtcsOptionsItem->x_flag_hardened){
      if (!mtcsOptionsItem->x_flag_stack_clash_protection
       /* Don't enable -fstack-clash-protection when -fstack-check=
          is used: it would result in confusing errors.  */
       && mtcsOptionsItem->x_flag_stack_check == NO_STACK_CHECK)
          mtcsOptionsItem->x_flag_stack_clash_protection = 1;
      else if (mtcsOptionsItem->x_flag_stack_check != NO_STACK_CHECK)
        warning_at (UNKNOWN_LOCATION, OPT_Whardened,
                "%<-fstack-clash-protection%> is not enabled by "
                "%<-fhardened%> because %<-fstack-check%> was "
                "specified on the command line");
  }

  /* We cannot support -fstack-check= and -fstack-clash-protection at
     the same time.  */
  if (mtcsOptionsItem->x_flag_stack_check != NO_STACK_CHECK
          && mtcsOptionsItem->x_flag_stack_clash_protection){
      warning_at (UNKNOWN_LOCATION, 0,"%<-fstack-check=%> and %<-fstack-clash-protection%> are mutually exclusive; disabling %<-fstack-check=%>");
      mtcsOptionsItem->x_flag_stack_check = NO_STACK_CHECK;
  }

  /* Targets must be able to place spill slots at lower addresses.  If the
     target already uses a soft frame pointer, the transition is trivial.  */
  if (!mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc) && mtcsOptionsItem->x_flag_stack_protect){
      if (!self->flag_stack_protector_set_by_fhardened_p)
          warning_at (UNKNOWN_LOCATION, 0,"%<-fstack-protector%> not supported for this target");
      mtcsOptionsItem->x_flag_stack_protect = 0;
  }
  if (!mtcsOptionsItem->x_flag_stack_protect)
      mtcsOptionsItem->x_warn_stack_protect = 0;

  /* Address Sanitizer needs porting to each target architecture.  */

  if ((mtcsOptionsItem->x_flag_sanitize & SANITIZE_ADDRESS) && !mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc)){
      warning_at (UNKNOWN_LOCATION, 0,
          "%<-fsanitize=address%> and %<-fsanitize=kernel-address%> are not supported for this target");
      mtcsOptionsItem->x_flag_sanitize &= ~SANITIZE_ADDRESS;
  }

  if ((mtcsOptionsItem->x_flag_sanitize & SANITIZE_USER_ADDRESS)
      && ((targetm.asan_shadow_offset == NULL)
      || (targetm.asan_shadow_offset () == 0))){
      warning_at (UNKNOWN_LOCATION, 0, "%<-fsanitize=address%> not supported for this target");
      mtcsOptionsItem->x_flag_sanitize &= ~SANITIZE_ADDRESS;
  }

  if ((mtcsOptionsItem->x_flag_sanitize & SANITIZE_KERNEL_ADDRESS)
      && (targetm.asan_shadow_offset == NULL   && !asan_shadow_offset_set_p ())){
      warning_at (UNKNOWN_LOCATION, 0,
          "%<-fsanitize=kernel-address%> with stack protection "
          "is not supported without %<-fasan-shadow-offset=%> for this target");
      mtcsOptionsItem->x_flag_sanitize &= ~SANITIZE_ADDRESS;
  }

  /* HWAsan requires top byte ignore feature in the backend.  */
  if ((mtcsOptionsItem->x_flag_sanitize & SANITIZE_HWADDRESS) && ! targetm.memtag.can_tag_addresses ()){
      warning_at (UNKNOWN_LOCATION, 0, "%qs is not supported for this target","-fsanitize=hwaddress");
      mtcsOptionsItem->x_flag_sanitize &= ~SANITIZE_HWADDRESS;
  }

  if (mtcsOptionsItem->x_flag_sanitize & SANITIZE_SHADOW_CALL_STACK){
      if (!mtcsTarget->have_shadow_call_stack/*!targetm.have_shadow_call_stack*/)
          sorry ("%<-fsanitize=shadow-call-stack%> not supported in current platform");
      else if (mtcsOptionsItem->x_flag_exceptions)
          error_at (UNKNOWN_LOCATION, "%<-fsanitize=shadow-call-stack%> requires %<-fno-exceptions%>");
  }
  n_debug("mtcsopts.c  mtcs_opts_process_options 102 打印 cl_optimization_node 33:%p\n",self->cl_optimization_node);
  aet_print_tree(self->cl_optimization_node);
  HOST_WIDE_INT patch_area_size, patch_area_start;
  parse_and_check_patch_area (mtcsOptionsItem->x_flag_patchable_function_entry, false,
                  &patch_area_size, &patch_area_start);

 /* Do not use IPA optimizations for register allocation if profiler is active
    or patchable function entries are inserted for run-time instrumentation
    or port does not emit prologue and epilogue as RTL.  */
  if (mtcsOptionsItem->x_profile_flag || patch_area_size
          || !target_rtx_have_prologue/*!targetm.have_prologue*/(mtcsMachine->tmrtx)
          || !target_rtx_have_epilogue/*!targetm.have_epilogue*/(mtcsMachine->tmrtx))
      mtcsOptionsItem->x_flag_ipa_ra = 0;
  n_debug("mtcsopts.c mtcs_opts_process_options 103\n");
  /* Enable -Werror=coverage-mismatch when -Werror and -Wno-error
     have not been set.  */
  if (!mtcsOptionsItem_Set/*!OPTION_SET_P (warnings_are_errors)*/->x_warnings_are_errors){
      if (mtcsOptionsItem->x_warn_coverage_mismatch  && option_unspecified_p (OPT_Wcoverage_mismatch))
          diagnostic_classify_diagnostic (global_dc, OPT_Wcoverage_mismatch, DK_ERROR, UNKNOWN_LOCATION);
      if (mtcsOptionsItem->x_warn_coverage_invalid_linenum && option_unspecified_p (OPT_Wcoverage_invalid_line_number))
          diagnostic_classify_diagnostic (global_dc, OPT_Wcoverage_invalid_line_number,DK_ERROR, UNKNOWN_LOCATION);
  }

  /* Save the current optimization options.  */
  mtcs_optimization_default_node = mtcs_opts_build_optimization_node (self,mtcsOptions->global_options, mtcsOptions->global_options_set);
  mtcs_optimization_current_node = mtcs_optimization_default_node;
  n_debug("mtcsopts.c mtcs_opts_process_options 104 :%p\n",mtcs_optimization_default_node);
  if (mtcsOptionsItem->x_flag_checking >= 2)
    hash_table_sanitize_eq_limit = mtcsOptionsItem->x_param_hash_table_verification_limit;

  mtcs_opts_diagnose_options/*!diagnose_options (&global_options, &global_options_set, UNKNOWN_LOCATION);*/
              (self,mtcsOptionsItem,mtcsOptionsItem_Set, UNKNOWN_LOCATION);

  /* Please don't change global_options after this point, those changes won't
     be reflected in optimization_{default,current}_node.  */
  n_debug("mtcsopts.c mtcs_opts_process_options 105 x_flag_ipa_pta:%d\n",mtcsOptionsItem->x_flag_ipa_pta);
}

/* Return a heap allocated producer with command line options.  */
//原型 gen_command_line_string opts.h opts.cc
char *mtcs_opts_gen_command_line_string (MtcsOpts *self,cl_decoded_option *options, unsigned int options_count)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  auto_vec<const char *> switches;
  char *options_string, *tail;
  const char *p;
  size_t len = 0;

  for (unsigned i = 0; i < options_count; i++){
    int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,options[i].opt_index);
    switch (hostOptcode/*!options[i].opt_index*/){
      case OPT_o:
      case OPT_d:
      case OPT_dumpbase:
      case OPT_dumpbase_ext:
      case OPT_dumpdir:
      case OPT_quiet:
      case OPT_version:
      case OPT_v:
      case OPT_w:
      case OPT_L:
      case OPT_D:
      case OPT_I:
      case OPT_U:
      case OPT_SPECIAL_unknown:
      case OPT_SPECIAL_ignore:
      case OPT_SPECIAL_warn_removed:
      case OPT_SPECIAL_program_name:
      case OPT_SPECIAL_input_file:
      case OPT_grecord_gcc_switches:
      case OPT_frecord_gcc_switches:
      case OPT__output_pch:
      case OPT_fdiagnostics_show_location_:
      case OPT_fdiagnostics_show_option:
      case OPT_fdiagnostics_show_caret:
      case OPT_fdiagnostics_show_labels:
      case OPT_fdiagnostics_show_line_numbers:
      case OPT_fdiagnostics_color_:
      case OPT_fdiagnostics_format_:
      case OPT_fverbose_asm:
      case OPT____:
      case OPT__sysroot_:
      case OPT_nostdinc:
      case OPT_nostdinc__:
      case OPT_fpreprocessed:
      case OPT_fltrans_output_list_:
      case OPT_fresolution_:
      case OPT_fdebug_prefix_map_:
      case OPT_fmacro_prefix_map_:
      case OPT_ffile_prefix_map_:
      case OPT_fprofile_prefix_map_:
      case OPT_fcanon_prefix_map:
      case OPT_fcompare_debug:
      case OPT_fchecking:
      case OPT_fchecking_:
        /* Ignore these.  */
        continue;
      case OPT_flto_:
        {
          const char *lto_canonical = "-flto";
          switches.safe_push (lto_canonical);
          len += strlen (lto_canonical) + 1;
          break;
        }
      default:
        if (mtcsOptions->clOptions/*!cl_options*/[options[i].opt_index].flags & CL_NO_DWARF_RECORD)
          continue;
        gcc_checking_assert (options[i].canonical_option[0][0] == '-');
        switch (options[i].canonical_option[0][1]){
          case 'M':
          case 'i':
          case 'W':
            continue;
          case 'f':
            if (strncmp (options[i].canonical_option[0] + 2,"dump", 4) == 0)
              continue;
            break;
          default:
            break;
        }
        switches.safe_push (options[i].orig_option_with_args_text);
        len += strlen (options[i].orig_option_with_args_text) + 1;
        break;
    }
  }//end for
  options_string = XNEWVEC (char, len + 1);
  tail = options_string;

  unsigned i;
  FOR_EACH_VEC_ELT (switches, i, p){
      len = strlen (p);
      memcpy (tail, p, len);
      tail += len;
      if (i != switches.length () - 1){
          *tail = ' ';
          ++tail;
      }
  }
  *tail = '\0';
  return options_string;
}

//原型 build_optimization_node tree.h tree.cc
tree mtcs_opts_build_optimization_node (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  tree t;
  /* Use the cache of optimization nodes.  */
  n_debug("mtcsopts.c mtcs_opts_build_optimization_node:%p\n",self->cl_optimization_node);
  aet_print_tree(self->cl_optimization_node);
  mtcs_options_cl_optimization_save/*!cl_optimization_save*/(mtcsOptions,TREE_OPTIMIZATION (self->cl_optimization_node),opts, opts_set);
  tree *slot = self->cl_option_hash_table->find_slot (self->cl_optimization_node, INSERT);
  t = *slot;
  if (!t){
      /* Insert this one into the hash table.  */
      t = self->cl_optimization_node;
      *slot = t;
      /* Make a new node for next time round.  */
      self->cl_optimization_node = make_node (OPTIMIZATION_NODE);
  }
  return t;
}


/* Output ITEM, of length ITEM_WIDTH, in the left column,
   followed by word-wrapped HELP in a second column.  */
static void wrap_help (const char *help,const char *item,unsigned int item_width,unsigned int columns)
{
  unsigned int col_width = LEFT_COLUMN;
  unsigned int remaining, room, len;
  remaining = strlen (help);
  do{
      room = columns - 3 - MAX (col_width, item_width);
      if (room > columns)
          room = 0;
      len = remaining;
      if (room < len){
          unsigned int i;
          for (i = 0; help[i]; i++){
              if (i >= room && len != remaining)
                  break;
              if (help[i] == ' ')
                  len = i;
              else if ((help[i] == '-' || help[i] == '/') && help[i + 1] != ' ' && i > 0 && ISALPHA (help[i - 1]))
                  len = i + 1;
          }
      }
      printf ("  %-*.*s %.*s\n", col_width, item_width, item, len, help);
      item_width = 0;
      while (help[len] == ' ')
          len++;
      help += len;
      remaining -= len;
  }while (remaining);
}
/* Data structure used to print list of valid option values.  */
class option_help_tuple
{
public:
  option_help_tuple (int code, vec<const char *> values):
    m_code (code), m_values (values)
  {}

  /* Code of an option.  */
  int m_code;

  /* List of possible values.  */
  vec<const char *> m_values;
};


/* Print help for a specific front-end, etc.  */
//原型 print_filtered_help opts.cc
static void print_filtered_help (MtcsOpts *self,unsigned int include_flags,
             unsigned int exclude_flags,
             unsigned int any_flags,
             unsigned int columns,
             MtcsOptionsItem *opts,
             unsigned int lang_mask)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  unsigned int i;
  const char *help;
  bool found = false;
  bool displayed = false;
  char new_help[256];
  int clOptionsCount=mtcsOptions->clOptionsCount;

  if (!opts->x_help_printed)
    opts->x_help_printed = XCNEWVAR (char, clOptionsCount/*!cl_options_count*/);
  if (!opts->x_help_enum_printed)
    opts->x_help_enum_printed = XCNEWVAR (char, cl_enums_count);
  auto_vec<option_help_tuple> help_tuples;
  for (i = 0; i < clOptionsCount/*!cl_options_count*/; i++){
      const struct cl_option *option =mtcsOptions->clOptions/*!cl_options*/ + i;
      unsigned int len;
      const char *opt;
      const char *tab;
      if (include_flags == 0  || ((option->flags & include_flags) != include_flags)) {
          if ((option->flags & any_flags) == 0)
            continue;
      }
      /* Skip unwanted switches.  */
      if ((option->flags & exclude_flags) != 0)
          continue;
      /* The driver currently prints its own help text.  */
      if ((option->flags & CL_DRIVER) != 0  && (option->flags & (((1U << cl_lang_count) - 1) | CL_COMMON | CL_TARGET)) == 0)
          continue;
      /* If an option contains a language specification,
     exclude it from common unless all languages are present.  */
      if ((include_flags & CL_COMMON)
      && !(option->flags & CL_DRIVER)
      && (option->flags & CL_LANG_ALL)
      && (option->flags & CL_LANG_ALL) != CL_LANG_ALL)
          continue;
      found = true;
      /* Skip switches that have already been printed.  */
      if (opts->x_help_printed[i])
          continue;
      opts->x_help_printed[i] = true;
      help = option->help;
      if (help == NULL){
          if (exclude_flags & CL_UNDOCUMENTED)
            continue;
          help = undocumented_msg;
      }
      /* Get the translation.  */
      //help = _(help);
      if (option->alias_target < N_OPTS  && mtcsOptions->clOptions/*!cl_options*/[option->alias_target].help){
          const struct cl_option *target = mtcsOptions->clOptions/*!cl_options*/ + option->alias_target;
          if (option->help == NULL){
              /* The option is undocumented but is an alias for an option that
             is documented.  If the option has alias arguments, then its
             purpose is to provide certain arguments to the other option, so
             inform the reader of this.  Otherwise, point the reader to the
             other option in preference to the former.  */
              if (option->alias_arg){
                  if (option->neg_alias_arg)
                    snprintf (new_help, sizeof new_help, "Same as %s%s (or, in negated form, %s%s.",target->opt_text, option->alias_arg,
                          target->opt_text, option->neg_alias_arg);
                  else
                    snprintf (new_help, sizeof new_help, "Same as %s%s.",target->opt_text, option->alias_arg);
              }else
                  snprintf (new_help, sizeof new_help, "Same as %s.",target->opt_text);
          }else{
              /* For documented options with aliases, mention the aliased
             option's name for reference.  */
              snprintf (new_help, sizeof new_help,"%s  Same as %s.", help, mtcsOptions->clOptions/*!cl_options*/[option->alias_target].opt_text);
          }
          help = new_help;
      }

      if (option->warn_message){
          /* Mention that the use of the option will trigger a warning.  */
          if (help == new_help)
            snprintf (new_help + strlen (new_help), sizeof new_help - strlen (new_help), "  %s", use_diagnosed_msg);
          else
            snprintf (new_help, sizeof new_help,"%s  %s", help, use_diagnosed_msg);
          help = new_help;
      }
      /* Find the gap between the name of the
     option and its descriptive text.  */
      tab = strchr (help, '\t');
      if (tab){
          len = tab - help;
          opt = help;
          help = tab + 1;
      }else{
          opt = option->opt_text;
          len = strlen (opt);
      }
      /* With the -Q option enabled we change the descriptive text associated
      with an option to be an indication of its current setting.  */
      if (!opts->x_quiet_flag){
          void *flag_var = mtcs_opts_option_flag_var/*!option_flag_var*/(self,i, opts);
          if (len < (LEFT_COLUMN + 2))
            strcpy (new_help, "\t\t");
          else
            strcpy (new_help, "\t");
              /* Set to print whether the option is enabled or disabled,
             or, if it's an alias for another option, the name of
             the aliased option.  */
          bool print_state = false;
          if (flag_var != NULL  && option->var_type != CLVC_DEFER){
              /* If OPTION is only available for a specific subset
             of languages other than this one, mention them.  */
              bool avail_for_lang = true;
              if (unsigned langset = option->flags & CL_LANG_ALL){
                  if (!(langset & lang_mask)){
                      avail_for_lang = false;
                      strcat (new_help, "[available in ");
                      for (unsigned i = 0, n = 0; (1U << i) < CL_LANG_ALL; ++i)
                         if (langset & (1U << i)){
                            if (n++)
                              strcat (new_help, ", ");
                            strcat (new_help, lang_names[i]);
                         }
                      strcat (new_help, "]");
                  }
              }
              if (!avail_for_lang)
                ; /* Print nothing else if the option is not available
                     in the current language.  */
              else if (option->flags & CL_JOINED){
                  if (option->var_type == CLVC_STRING){
                      if (* (const char **) flag_var != NULL)
                          snprintf (new_help + strlen (new_help), sizeof (new_help) - strlen (new_help), "%s", * (const char **) flag_var);
                  }else if (option->var_type == CLVC_ENUM){
                      const struct cl_enum *e = &mtcsOptions->clEnums/*!cl_enums*/[option->var_enum];
                      int value;
                      const char *arg = NULL;
                      value = e->get (flag_var);
                      enum_value_to_arg (e->values, &arg, value, lang_mask);
                      if (arg == NULL)
                          arg = "[default]";
                      snprintf (new_help + strlen (new_help),sizeof (new_help) - strlen (new_help),"%s", arg);
                  }else{
                      if (option->cl_host_wide_int)
                          sprintf (new_help + strlen (new_help),"%llu bytes", (unsigned long long) *(unsigned HOST_WIDE_INT *) flag_var);
                      else
                          sprintf (new_help + strlen (new_help),"%i", * (int *) flag_var);
                  }
              }else
                  print_state = true;
          }else
                /* When there is no argument, print the option state only
                   if the option takes no argument.  */
                print_state = !(option->flags & CL_JOINED);

          if (print_state){
              if (option->alias_target < N_OPTS
                && option->alias_target != OPT_SPECIAL_warn_removed
                && option->alias_target != OPT_SPECIAL_ignore
                && option->alias_target != OPT_SPECIAL_input_file
                && option->alias_target != OPT_SPECIAL_program_name
                && option->alias_target != OPT_SPECIAL_unknown){
                  const struct cl_option *target  = &mtcsOptions->clOptions/*!cl_options*/[option->alias_target];
                  sprintf (new_help + strlen (new_help), "%s%s",target->opt_text,option->alias_arg ? option->alias_arg : "");
              }else if (option->alias_target == OPT_SPECIAL_ignore)
                  strcat (new_help, ("[ignored]"));
              else{
                  /* Print the state for an on/off option.  */
                  int ena = option_enabled (i, lang_mask, opts);
                  if (ena > 0)
                    strcat (new_help, "[enabled]");
                  else if (ena == 0)
                    strcat (new_help, "[disabled]");
              }
          }
         help = new_help;
      }//end  if (!opts->x_quiet_flag){

      if (option->range_max != -1 && tab == NULL){
          char b[128];
          snprintf (b, sizeof (b), "<%d,%d>", option->range_min, option->range_max);
          opt = concat (opt, b, NULL);
          len += strlen (b);
      }
      wrap_help (help, opt, len, columns);
      displayed = true;
      if (option->var_type == CLVC_ENUM  && opts->x_help_enum_printed[option->var_enum] != 2)
          opts->x_help_enum_printed[option->var_enum] = 1;
      else{
          vec<const char *> option_values = targetm_common.get_valid_option_values (i, NULL);
          if (!option_values.is_empty ())
            help_tuples.safe_push (option_help_tuple (i, option_values));
      }
  }//end for

  if (! found){
      unsigned int langs = include_flags & CL_LANG_ALL;
      if (langs == 0)
          printf (" No options with the desired characteristics were found\n");
      else{
          unsigned int i;
          /* PR 31349: Tell the user how to see all of the
             options supported by a specific front end.  */
          for (i = 0; (1U << i) < CL_LANG_ALL; i ++)
            if ((1U << i) & langs)
                printf (" None found.  Use --help=%s to show *all* the options supported by the %s front-end.\n",
                   lang_names[i], lang_names[i]);
      }
  }else if (! displayed)
    printf (" All options with the desired characteristics have already been displayed\n");

  putchar ('\n');
  /* Print details of enumerated option arguments, if those
     enumerations have help text headings provided.  If no help text
     is provided, presume that the possible values are listed in the
     help text for the relevant options.  */
  for (i = 0; i < cl_enums_count; i++){
      unsigned int j, pos;

      if (opts->x_help_enum_printed[i] != 1)
          continue;
      if (mtcsOptions->clEnums/*!cl_enums*/[i].help == NULL)
          continue;
      printf ("  %s\n    ", mtcsOptions->clEnums/*!cl_enums*/[i].help);
      pos = 4;
      for (j = 0; mtcsOptions->clEnums/*!cl_enums*/[i].values[j].arg != NULL; j++){
          unsigned int len = strlen (mtcsOptions->clEnums/*!cl_enums*/[i].values[j].arg);
          if (pos > 4 && pos + 1 + len <= columns){
              printf (" %s", mtcsOptions->clEnums/*!cl_enums*/[i].values[j].arg);
              pos += 1 + len;
          }else{
              if (pos > 4){
                  printf ("\n    ");
                  pos = 4;
              }
              printf ("%s", mtcsOptions->clEnums/*!cl_enums*/[i].values[j].arg);
              pos += len;
          }
      }
      printf ("\n\n");
      opts->x_help_enum_printed[i] = 2;
  }

  for (unsigned i = 0; i < help_tuples.length (); i++){
      const struct cl_option *option = mtcsOptions->clOptions/*!cl_options*/ + help_tuples[i].m_code;
      printf("  Known valid arguments for %s option:\n   ", option->opt_text);
      for (unsigned j = 0; j < help_tuples[i].m_values.length (); j++)
          printf (" %s", help_tuples[i].m_values[j]);
      printf ("\n\n");
  }
}

/* Display help for a specified type of option.
   The options must have ALL of the INCLUDE_FLAGS set
   ANY of the flags in the ANY_FLAGS set
   and NONE of the EXCLUDE_FLAGS set.  The current option state is in
   OPTS; LANG_MASK is used for interpreting enumerated option state.  */
//原型 print_specific_help opts.cc
static void print_specific_help (MtcsOpts *self,unsigned int include_flags,
             unsigned int exclude_flags,
             unsigned int any_flags,
             MtcsOptionsItem *opts,
             unsigned int lang_mask)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  unsigned int all_langs_mask = (1U <<mtcsOptions->clLangCount/*!cl_lang_count*/) - 1;
  const char * description = NULL;
  const char * descrip_extra = "";
  size_t i;
  unsigned int flag;

  /* Sanity check: Make sure that we do not have more
     languages than we have bits available to enumerate them.  */
  gcc_assert ((1U << mtcsOptions->clLangCount/*!cl_lang_count*/) <= CL_MIN_OPTION_CLASS);

  /* If we have not done so already, obtain
     the desired maximum width of the output.  */
  if (opts->x_help_columns == 0){
      opts->x_help_columns = get_terminal_width ();
      if (opts->x_help_columns == INT_MAX)
        /* Use a reasonable default.  */
        opts->x_help_columns = 80;
  }

  /* Decide upon the title for the options that we are going to display.  */
  for (i = 0, flag = 1; flag <= CL_MAX_OPTION_CLASS; flag <<= 1, i ++){
      switch (flag & include_flags){
        case 0:
        case CL_DRIVER:
          break;
        case CL_TARGET:
          description = "The following options are target specific";
          break;
        case CL_WARNING:
          description = "The following options control compiler warning messages";
          break;
        case CL_OPTIMIZATION:
          description = "The following options control optimizations";
          break;
        case CL_COMMON:
          description = "The following options are language-independent";
          break;
        case CL_PARAMS:
          description = "The following options control parameters";
          break;
        default:
          if (i >= mtcsOptions->clLangCount/*!cl_lang_count*/)
            break;
          if (exclude_flags & all_langs_mask)
            description = "The following options are specific to just the language ";
          else
            description = "The following options are supported by the language ";
          descrip_extra = lang_names [i];
          break;
      }
  }

  if (description == NULL){
      if (any_flags == 0){
          if (include_flags & CL_UNDOCUMENTED)
            description = "The following options are not documented";
          else if (include_flags & CL_SEPARATE)
            description = "The following options take separate arguments";
          else if (include_flags & CL_JOINED)
            description = "The following options take joined arguments";
          else {
              internal_error ("unrecognized %<include_flags 0x%x%> passed to %<print_specific_help%>",include_flags);
              return;
          }
      }else{
          if (any_flags & all_langs_mask)
            description = "The following options are language-related";
          else
            description = "The following options are language-independent";
      }
  }

  printf ("%s%s:\n", description, descrip_extra);
  print_filtered_help(self,include_flags, exclude_flags, any_flags, opts->x_help_columns, opts, lang_mask);
}

/* Add comma-separated strings to a char_p vector.  */
//原型 add_comma_separated_to_vector opts.cc
static void add_comma_separated_to_vector (void **pvec, const char *arg)
{
  char *tmp;
  char *r;
  char *w;
  char *token_start;
  vec<const char *> *v = (vec<const char *> *) *pvec;
  vec_check_alloc (v, 1);
  /* We never free this string.  */
  tmp = xstrdup (arg);
  r = tmp;
  w = tmp;
  token_start = tmp;
  while (*r != '\0'){
      if (*r == ','){
          *w++ = '\0';
          ++r;
          v->safe_push (token_start);
          token_start = w;
      }
      if (*r == '\\' && r[1] == ','){
          *w++ = ',';
          r += 2;
      }else
          *w++ = *r++;
  }

  *w = '\0';
  if (*token_start != '\0')
    v->safe_push (token_start);

  *pvec = v;
}

/* Enable (or disable if VALUE is 0) a warning option ARG (language
   mask LANG_MASK, option handlers HANDLERS) as an error for option
   structures OPTS and OPTS_SET, diagnostic context DC (possibly
   NULL), location LOC.  This is used by -Werror=.  */

static void enable_warning_as_error (MtcsOpts *self,const char *arg, int value, unsigned int lang_mask,
        const struct mtcs_cl_option_handlers *handlers,
        MtcsOptionsItem  *opts, MtcsOptionsItem *opts_set,location_t loc, diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  char *new_option;
  int option_index;
  new_option = XNEWVEC (char, strlen (arg) + 2);
  new_option[0] = 'W';
  strcpy (new_option + 1, arg);
  option_index = mtcs_opts_find_opt/*!find_opt*/(self,new_option, lang_mask);
  int host_OPT_SPECIAL_unknown=mtcs_options_optcode_device_to_host(mtcsOptions,option_index);
  if (host_OPT_SPECIAL_unknown/*!option_index*/ == OPT_SPECIAL_unknown){
      option_proposer op;
      const char *hint = op.suggest_option (new_option);
      if (hint)
          error_at (loc, "%<-W%serror=%s%>: no option %<-%s%>; did you mean %<-%s%>?", value ? "" : "no-",arg, new_option, hint);
      else
          error_at (loc, "%<-W%serror=%s%>: no option %<-%s%>",value ? "" : "no-", arg, new_option);
  }else if (!(mtcsOptions/*!cl_options*/->clOptions[option_index].flags & CL_WARNING))
    error_at (loc, "%<-Werror=%s%>: %<-%s%> is not an option that controls warnings", arg, new_option);
  else{
      const diagnostic_t kind = value ? DK_ERROR : DK_WARNING;
      const char *arg = NULL;

      if (mtcsOptions/*!cl_options*/->clOptions[option_index].flags & CL_JOINED)
          arg = new_option + mtcsOptions/*!cl_options*/->clOptions[option_index].opt_len;
      mtcs_opts_control_warning_option/*!control_warning_option*/(self,
              option_index, (int) kind, arg, value,loc, lang_mask,handlers, opts, opts_set, dc);
  }
  free (new_option);
}

/* Handle a debug output -g switch for options OPTS
   (OPTS_SET->x_write_symbols storing whether a debug format was passed
   explicitly), location LOC.  EXTENDED is true or false to support
   extended output (2 is special and means "-ggdb" was given).  */
//原型 set_debug_level opts.cc
static void set_debug_level (MtcsOpts *self,uint32_t dinfo, int extended, const char *arg,
         MtcsOptionsItem  *opts, MtcsOptionsItem *opts_set,location_t loc)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsConfig  *mtcsConfig=mtcs_target_get_config(mtcsTarget);

  if (dinfo == NO_DEBUG){
      if (opts->x_write_symbols == NO_DEBUG){
      opts->x_write_symbols = PREFERRED_DEBUGGING_TYPE;
      if (extended == 2){
          /*!#if defined DWARF2_DEBUGGING_INFO || defined DWARF2_LINENO_DEBUGGING_INFO*/
          if(mtcs_config_ifdef(mtcsConfig,MTCS_DWARF2_DEBUGGING_INFO)
                  || mtcs_config_ifdef(mtcsConfig,MTCS_DWARF2_LINENO_DEBUGGING_INFO)){
              if (opts->x_write_symbols & CTF_DEBUG)
                  opts->x_write_symbols |= DWARF2_DEBUG;
              else
                  opts->x_write_symbols = DWARF2_DEBUG;
         }
      }

      if (opts->x_write_symbols == NO_DEBUG)
        warning_at (loc, 0, "target system does not support debug output");
      } else if ((opts->x_write_symbols & CTF_DEBUG) || (opts->x_write_symbols & BTF_DEBUG)){
          opts->x_write_symbols |= DWARF2_DEBUG;
          opts_set->x_write_symbols |= DWARF2_DEBUG;
      }
  }else{
      /* Make and retain the choice if both CTF and DWARF debug info are to
     be generated.  */
      if (((dinfo == DWARF2_DEBUG) || (dinfo == CTF_DEBUG))
          && ((opts->x_write_symbols == (DWARF2_DEBUG|CTF_DEBUG))
          || (opts->x_write_symbols == DWARF2_DEBUG) || (opts->x_write_symbols == CTF_DEBUG))){
          opts->x_write_symbols |= dinfo;
          opts_set->x_write_symbols |= dinfo;
      }
      /* However, CTF and BTF are not allowed together at this time.  */
      else if (((dinfo == DWARF2_DEBUG) || (dinfo == BTF_DEBUG))
           && ((opts->x_write_symbols == (DWARF2_DEBUG|BTF_DEBUG))
           || (opts->x_write_symbols == DWARF2_DEBUG)
           || (opts->x_write_symbols == BTF_DEBUG))){
          opts->x_write_symbols |= dinfo;
          opts_set->x_write_symbols |= dinfo;
      }else{
          /* Does it conflict with an already selected debug format?  */
          if (opts_set->x_write_symbols != NO_DEBUG  && opts->x_write_symbols != NO_DEBUG
              && dinfo != opts->x_write_symbols){
              gcc_assert (debug_set_count (dinfo) <= 1);
              error_at (loc, "debug format %qs conflicts with prior selection",
                debug_type_names[debug_set_to_format (dinfo)]);
          }
          opts->x_write_symbols = dinfo;
          opts_set->x_write_symbols = dinfo;
      }
  }

  if (dinfo != BTF_DEBUG){
      /* A debug flag without a level defaults to level 2.
     If off or at level 1, set it to level 2, but if already
     at level 3, don't lower it.  */
      if (*arg == '\0'){
          if (dinfo == CTF_DEBUG)
            opts->x_ctf_debug_info_level = CTFINFO_LEVEL_NORMAL;
          else if (opts->x_debug_info_level < DINFO_LEVEL_NORMAL)
            opts->x_debug_info_level = DINFO_LEVEL_NORMAL;
      }else{
          int argval = integral_argument (arg);
          if (argval == -1)
            error_at (loc, "unrecognized debug output level %qs", arg);
          else if (argval > 3)
            error_at (loc, "debug output level %qs is too high", arg);
          else{
              if (dinfo == CTF_DEBUG)
                  opts->x_ctf_debug_info_level= (enum ctf_debug_info_levels) argval;
              else
                  opts->x_debug_info_level = (enum debug_info_levels) argval;
          }
      }
  }else if (*arg != '\0')
    error_at (loc, "unrecognized btf debug output level %qs", arg);
}

/* Enable FDO-related flags.  */
//原型 enable_fdo_optimizations opts.cc
static void enable_fdo_optimizations (MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,int value)
{
  SET_OPTION_IF_UNSET (opts, opts_set, flag_branch_probabilities, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_profile_values, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_unroll_loops, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_peel_loops, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_tracer, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_value_profile_transformations,
               value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_inline_functions, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_ipa_cp, value);
  if (value){
      SET_OPTION_IF_UNSET (opts, opts_set, flag_ipa_cp_clone, 1);
      SET_OPTION_IF_UNSET (opts, opts_set, flag_ipa_bit_cp, 1);
  }
  SET_OPTION_IF_UNSET (opts, opts_set, flag_predictive_commoning, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_split_loops, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_unswitch_loops, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_gcse_after_reload, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_tree_loop_vectorize, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_tree_slp_vectorize, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_version_loops_for_strides, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_vect_cost_model,VECT_COST_MODEL_DYNAMIC);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_tree_loop_distribute_patterns,value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_loop_interchange, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_unroll_jam, value);
  SET_OPTION_IF_UNSET (opts, opts_set, flag_tree_loop_distribution, value);
}


/* Used to set the level of strict aliasing warnings in OPTS,
   when no level is specified (i.e., when -Wstrict-aliasing, and not
   -Wstrict-aliasing=level was given).
   ONOFF is assumed to take value 1 when -Wstrict-aliasing is specified,
   and 0 otherwise.  After calling this function, wstrict_aliasing will be
   set to the default value of -Wstrict_aliasing=level, currently 3.  */
//原型 set_Wstrict_aliasing opts.cc
static void set_Wstrict_aliasing(MtcsOptionsItem *opts, int onoff)
{
  gcc_assert (onoff == 0 || onoff == 1);
  if (onoff != 0)
    opts->x_warn_strict_aliasing = 3;
  else
    opts->x_warn_strict_aliasing = 0;
}

/* Arrange to dump core on error for diagnostic context DC.  (The
   regular error message is still printed first, except in the case of
   abort ().)  */
//原型 setup_core_dumping opts.cc
static void setup_core_dumping (diagnostic_context *dc)
{
#ifdef SIGABRT
  signal (SIGABRT, SIG_DFL);
#endif
#if defined(HAVE_SETRLIMIT)
  {
    struct rlimit rlim;
    if (getrlimit (RLIMIT_CORE, &rlim) != 0)
      fatal_error (input_location, "getting core file size maximum limit: %m");
    rlim.rlim_cur = rlim.rlim_max;
    if (setrlimit (RLIMIT_CORE, &rlim) != 0)
      fatal_error (input_location,
           "setting core file size limit to maximum: %m");
  }
#endif
  diagnostic_abort_on_error (dc);
}

/* Parse a -d<ARG> command line switch for OPTS, location LOC,
   diagnostic context DC.  */
//原型 decode_d_option opts.cc
static void decode_d_option (const char *arg, MtcsOptionsItem *opts,location_t loc, diagnostic_context *dc)
{
  int c;
  while (*arg)
      switch (c = *arg++){
          case 'A':
            opts->x_flag_debug_asm = 1;
            break;
          case 'p':
            opts->x_flag_print_asm_name = 1;
            break;
          case 'P':
            opts->x_flag_dump_rtl_in_asm = 1;
            opts->x_flag_print_asm_name = 1;
            break;
          case 'x':
            opts->x_rtl_dump_and_exit = 1;
            break;
          case 'D': /* These are handled by the preprocessor.  */
          case 'I':
          case 'M':
          case 'N':
          case 'U':
              break;
          case 'H':
            setup_core_dumping (dc);
            break;
          case 'a':
              opts->x_flag_dump_all_passed = true;
              break;

          default:
              warning_at (loc, 0, "unrecognized gcc debugging option: %c", c);
              break;
      }
}

/* When -funsafe-math-optimizations is set the following
   flags are set as well.  */
//原型 set_unsafe_math_optimizations_flags opts.cc
static void set_unsafe_math_optimizations_flags (MtcsOptionsItem *opts, int set)
{
  if (!opts->frontend_set_flag_trapping_math)
    opts->x_flag_trapping_math = !set;
  if (!opts->frontend_set_flag_signed_zeros)
    opts->x_flag_signed_zeros = !set;
  if (!opts->frontend_set_flag_associative_math)
    opts->x_flag_associative_math = set;
  if (!opts->frontend_set_flag_reciprocal_math)
    opts->x_flag_reciprocal_math = set;
}

/* The following routines are useful in setting all the flags that
   -ffast-math and -fno-fast-math imply.  */
//原型 set_fast_math_flags opts.cc
static void set_fast_math_flags (MtcsOptionsItem *opts, int set)
{
  if (!opts->frontend_set_flag_unsafe_math_optimizations){
      opts->x_flag_unsafe_math_optimizations = set;
      set_unsafe_math_optimizations_flags (opts, set);
  }
  if (!opts->frontend_set_flag_finite_math_only)
    opts->x_flag_finite_math_only = set;
  if (!opts->frontend_set_flag_errno_math)
    opts->x_flag_errno_math = !set;
  if (set){
      if (opts->frontend_set_flag_excess_precision == EXCESS_PRECISION_DEFAULT)
          opts->x_flag_excess_precision= set ? EXCESS_PRECISION_FAST : EXCESS_PRECISION_DEFAULT;
      if (!opts->frontend_set_flag_signaling_nans)
          opts->x_flag_signaling_nans = 0;
      if (!opts->frontend_set_flag_rounding_math)
          opts->x_flag_rounding_math = 0;
      if (!opts->frontend_set_flag_cx_limited_range)
          opts->x_flag_cx_limited_range = 1;
  }
}

/* Handle target- and language-independent options.  Return zero to
   generate an "unknown option" message.  Only options that need
   extra handling need to be listed here; if you simply want
   DECODED->value assigned to a variable, it happens automatically.  */
//原型 common_handle_option opts.h opts.cc
bool mtcs_opts_common_handle_option (MtcsOpts *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
              const struct cl_decoded_option *decoded,
              unsigned int lang_mask, int kind ATTRIBUTE_UNUSED,
              location_t loc,const struct mtcs_cl_option_handlers *handlers,
              diagnostic_context *dc,void (*target_option_override_hook) (void *userData),void *userData)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  //cl_decoded_option *decoded 是设备 cl_decoded_option 所以decoded->opt_index;要转为主机的opt_code
  size_t scode = decoded->opt_index;
  const char *arg = decoded->arg;
  HOST_WIDE_INT value = decoded->value;
  int hostOptCode=mtcs_options_optcode_device_to_host(mtcsOptions,scode);
  enum opt_code code = (enum opt_code)hostOptCode/*!scode*/;

  gcc_assert (decoded->canonical_option_num_elements <= 2);

  switch (code){
    case OPT__help:
      {
        unsigned int all_langs_mask = (1U << cl_lang_count) - 1;
        unsigned int undoc_mask;
        unsigned int i;
        if (lang_mask == CL_DRIVER)
          break;
        undoc_mask = ((opts->x_verbose_flag | opts->x_extra_warnings)? 0: CL_UNDOCUMENTED);
        target_option_override_hook(userData);
        /* First display any single language specific options.  */
        for (i = 0; i < cl_lang_count; i++)
           print_specific_help(self,1U << i, (all_langs_mask & (~ (1U << i))) | undoc_mask, 0, opts,lang_mask);
        /* Next display any multi language specific options.  */
        print_specific_help(self,0, undoc_mask, all_langs_mask, opts, lang_mask);
        /* Then display any remaining, non-language options.  */
        for (i = CL_MIN_OPTION_CLASS; i <= CL_MAX_OPTION_CLASS; i <<= 1)
          if (i != CL_DRIVER)
            print_specific_help(self,i, undoc_mask, 0, opts, lang_mask);
        opts->x_exit_after_options = true;
        break;
      }

    case OPT__target_help:
      if (lang_mask == CL_DRIVER)
          break;

      target_option_override_hook (userData);
      print_specific_help(self,CL_TARGET, 0, 0, opts, lang_mask);
      opts->x_exit_after_options = true;
      break;

    case OPT__help_:
      {
        help_option_arguments.safe_push (arg);
        opts->x_exit_after_options = true;
        break;
      }

    case OPT__version:
      if (lang_mask == CL_DRIVER)
          break;
      opts->x_exit_after_options = true;
          break;

    case OPT__completion_:
      break;

    case OPT_fsanitize_:
      opts_set->x_flag_sanitize = true;
      opts->x_flag_sanitize= parse_sanitizer_options (arg, loc, code,opts->x_flag_sanitize, value, true);

      /* Kernel ASan implies normal ASan but does not yet support
       all features.  */
      if (opts->x_flag_sanitize & SANITIZE_KERNEL_ADDRESS){
          SET_OPTION_IF_UNSET (opts, opts_set,param_asan_instrumentation_with_call_threshold,0);
          SET_OPTION_IF_UNSET (opts, opts_set, param_asan_globals, 0);
          SET_OPTION_IF_UNSET (opts, opts_set, param_asan_stack, 0);
          SET_OPTION_IF_UNSET (opts, opts_set, param_asan_protect_allocas, 0);
          SET_OPTION_IF_UNSET (opts, opts_set, param_asan_use_after_return, 0);
      }
      if (opts->x_flag_sanitize & SANITIZE_KERNEL_HWADDRESS){
          SET_OPTION_IF_UNSET (opts, opts_set,param_hwasan_instrument_stack, 0);
          SET_OPTION_IF_UNSET (opts, opts_set,param_hwasan_random_frame_tag, 0);
          SET_OPTION_IF_UNSET (opts, opts_set,param_hwasan_instrument_allocas, 0);
      }
      break;

    case OPT_fsanitize_recover_:
      opts->x_flag_sanitize_recover = parse_sanitizer_options (arg, loc, code, opts->x_flag_sanitize_recover, value, true);
      break;

    case OPT_fsanitize_trap_:
      opts->x_flag_sanitize_trap= parse_sanitizer_options (arg, loc, code,opts->x_flag_sanitize_trap, value, true);
      break;

    case OPT_fasan_shadow_offset_:
      /* Deferred.  */
      break;

    case OPT_fsanitize_address_use_after_scope:
      opts->x_flag_sanitize_address_use_after_scope = value;
      break;

    case OPT_fsanitize_recover:
      if (value)
          opts->x_flag_sanitize_recover  |= (SANITIZE_UNDEFINED | SANITIZE_UNDEFINED_NONDEFAULT)
             & ~(SANITIZE_UNREACHABLE | SANITIZE_RETURN);
      else
          opts->x_flag_sanitize_recover &= ~(SANITIZE_UNDEFINED | SANITIZE_UNDEFINED_NONDEFAULT);
      break;

    case OPT_fsanitize_trap:
      if (value)
          opts->x_flag_sanitize_trap |= (SANITIZE_UNDEFINED | SANITIZE_UNDEFINED_NONDEFAULT);
      else
          opts->x_flag_sanitize_trap &= ~(SANITIZE_UNDEFINED | SANITIZE_UNDEFINED_NONDEFAULT);
      break;

    case OPT_O:
    case OPT_Os:
    case OPT_Ofast:
    case OPT_Og:
    case OPT_Oz:
      /* Currently handled in a prescan.  */
      break;

    case OPT_Wattributes_:
      if (lang_mask == CL_DRIVER)
          break;

      if (value){
          error_at (loc, "arguments ignored for %<-Wattributes=%>; use %<-Wno-attributes=%> instead");
          break;
      }else if (arg[strlen (arg) - 1] == ','){
          error_at (loc, "trailing %<,%> in arguments for %<-Wno-attributes=%>");
          break;
      }

      add_comma_separated_to_vector (&opts->x_flag_ignored_attributes, arg);
      break;

    case OPT_Werror:
      dc->set_warning_as_error_requested (value);
      break;

    case OPT_Werror_:
      if (lang_mask == CL_DRIVER)
          break;
      enable_warning_as_error(self,arg, value, lang_mask, handlers,opts, opts_set, loc, dc);
      break;

    case OPT_Wfatal_errors:
      dc->m_fatal_errors = value;
      break;

    case OPT_Wstack_usage_:
      opts->x_flag_stack_usage_info = value != -1;
      break;

    case OPT_Wstrict_aliasing:
      set_Wstrict_aliasing(opts, value);
      break;

    case OPT_Wstrict_overflow:
      opts->x_warn_strict_overflow = (value ? (int) WARN_STRICT_OVERFLOW_CONDITIONAL : 0);
      break;

    case OPT_Wsystem_headers:
      dc->m_warn_system_headers = value;
      break;

    case OPT_aux_info:
      opts->x_flag_gen_aux_info = 1;
      break;

    case OPT_d:
      decode_d_option (arg, opts, loc, dc);
      break;

    case OPT_fcall_used_:
    case OPT_fcall_saved_:
      /* Deferred.  */
      break;

    case OPT_fdbg_cnt_:
      /* Deferred.  */
      break;

    case OPT_fdebug_prefix_map_:
    case OPT_ffile_prefix_map_:
    case OPT_fprofile_prefix_map_:
      /* Deferred.  */
      break;

    case OPT_fcanon_prefix_map:
      flag_canon_prefix_map = value;
      break;

    case OPT_fcallgraph_info:
      opts->x_flag_callgraph_info = CALLGRAPH_INFO_NAKED;
      break;

    case OPT_fcallgraph_info_:
      {
        char *my_arg, *p;
        my_arg = xstrdup (arg);
        p = strtok (my_arg, ",");
        while (p){
            if (strcmp (p, "su") == 0){
                opts->x_flag_callgraph_info |= CALLGRAPH_INFO_STACK_USAGE;
                opts->x_flag_stack_usage_info = true;
            }else if (strcmp (p, "da") == 0)
              opts->x_flag_callgraph_info |= CALLGRAPH_INFO_DYNAMIC_ALLOC;
            else
              return 0;
            p = strtok (NULL, ",");
        }
        free (my_arg);
      }
      break;

    case OPT_fdiagnostics_show_location_:
      dc->set_prefixing_rule ((diagnostic_prefixing_rule_t) value);
      break;

    case OPT_fdiagnostics_show_caret:
      dc->m_source_printing.enabled = value;
      break;

    case OPT_fdiagnostics_show_labels:
      dc->m_source_printing.show_labels_p = value;
      break;

    case OPT_fdiagnostics_show_line_numbers:
      dc->m_source_printing.show_line_numbers_p = value;
      break;

    case OPT_fdiagnostics_color_:
      diagnostic_color_init (dc, value);
      break;

    case OPT_fdiagnostics_urls_:
      diagnostic_urls_init (dc, value);
      break;

    case OPT_fdiagnostics_format_:
    {
      const char *basename = (opts->x_dump_base_name ? opts->x_dump_base_name: opts->x_main_input_basename);
      diagnostic_output_format_init (*dc, opts->x_main_input_filename,
            basename, (enum diagnostics_output_format)value, opts->x_flag_diagnostics_json_formatting);
      break;
    }

    case OPT_fdiagnostics_text_art_charset_:
      dc->set_text_art_charset ((enum diagnostic_text_art_charset)value);
      break;

    case OPT_fdiagnostics_parseable_fixits:
      dc->set_extra_output_kind (value ? EXTRA_DIAGNOSTIC_OUTPUT_fixits_v1: EXTRA_DIAGNOSTIC_OUTPUT_none);
      break;

    case OPT_fdiagnostics_column_unit_:
      dc->m_column_unit = (enum diagnostics_column_unit)value;
      break;

    case OPT_fdiagnostics_column_origin_:
      dc->m_column_origin = value;
      break;

    case OPT_fdiagnostics_escape_format_:
      dc->set_escape_format ((enum diagnostics_escape_format)value);
      break;

    case OPT_fdiagnostics_show_cwe:
      dc->set_show_cwe (value);
      break;

    case OPT_fdiagnostics_show_rules:
      dc->set_show_rules (value);
      break;

    case OPT_fdiagnostics_path_format_:
      dc->set_path_format ((enum diagnostic_path_format)value);
      break;

    case OPT_fdiagnostics_show_path_depths:
      dc->set_show_path_depths (value);
      break;

    case OPT_fdiagnostics_show_option:
      dc->set_show_option_requested (value);
      break;

    case OPT_fdiagnostics_minimum_margin_width_:
      dc->m_source_printing.min_margin_width = value;
      break;

    case OPT_fdump_:
      /* Deferred.  */
      break;

    case OPT_ffast_math:
      set_fast_math_flags (opts, value);
      break;

    case OPT_funsafe_math_optimizations:
      set_unsafe_math_optimizations_flags (opts, value);
      break;

    case OPT_ffixed_:
      /* Deferred.  */
      break;

    case OPT_finline_limit_:
      SET_OPTION_IF_UNSET (opts, opts_set, param_max_inline_insns_single,
               value / 2);
      SET_OPTION_IF_UNSET (opts, opts_set, param_max_inline_insns_auto,
               value / 2);
      break;

    case OPT_finstrument_functions_exclude_function_list_:
      add_comma_separated_to_vector
    (&opts->x_flag_instrument_functions_exclude_functions, arg);
      break;

    case OPT_finstrument_functions_exclude_file_list_:
      add_comma_separated_to_vector
    (&opts->x_flag_instrument_functions_exclude_files, arg);
      break;

    case OPT_fmessage_length_:
      pp_set_line_maximum_length (dc->get_reference_printer(), value);
      diagnostic_set_caret_max_width (dc, value);
      break;

    case OPT_fopt_info:
    case OPT_fopt_info_:
      /* Deferred.  */
      break;

    case OPT_foffload_options_:
      /* Deferred.  */
      break;

    case OPT_foffload_abi_:
#ifdef ACCEL_COMPILER
      /* Handled in the 'mkoffload's.  */
#else
      error_at (loc, "%<-foffload-abi%> option can be specified only for offload compiler");
#endif
      break;

    case OPT_fpack_struct_:
      if (value <= 0 || (value & (value - 1)) || value > 16)
          error_at (loc,"structure alignment must be a small power of two, not %wu",value);
      else
          opts->x_initial_max_fld_align = value;
      break;

    case OPT_fplugin_:
    case OPT_fplugin_arg_:
      /* Deferred.  */
      break;

    case OPT_fprofile_use_:
      opts->x_profile_data_prefix = xstrdup (arg);
      opts->x_flag_profile_use = true;
      value = true;
      /* No break here - do -fprofile-use processing. */
      /* FALLTHRU */
    case OPT_fprofile_use:
      enable_fdo_optimizations (opts, opts_set, value);
      SET_OPTION_IF_UNSET (opts, opts_set, flag_profile_reorder_functions,value);
       /* Indirect call profiling should do all useful transformations
       speculative devirtualization does.  */
      if (opts->x_flag_value_profile_transformations)
          SET_OPTION_IF_UNSET (opts, opts_set, flag_devirtualize_speculatively,false);
      break;

    case OPT_fauto_profile_:
      opts->x_auto_profile_file = xstrdup (arg);
      opts->x_flag_auto_profile = true;
      value = true;
      /* No break here - do -fauto-profile processing. */
      /* FALLTHRU */
    case OPT_fauto_profile:
      enable_fdo_optimizations (opts, opts_set, value);
      SET_OPTION_IF_UNSET (opts, opts_set, flag_profile_correction, value);
      break;

    case OPT_fprofile_generate_:
      opts->x_profile_data_prefix = xstrdup (arg);
      value = true;
      /* No break here - do -fprofile-generate processing. */
      /* FALLTHRU */
    case OPT_fprofile_generate:
      SET_OPTION_IF_UNSET (opts, opts_set, profile_arc_flag, value);
      SET_OPTION_IF_UNSET (opts, opts_set, flag_profile_values, value);
      SET_OPTION_IF_UNSET (opts, opts_set, flag_inline_functions, value);
      SET_OPTION_IF_UNSET (opts, opts_set, flag_ipa_bit_cp, value);
      break;

    case OPT_fprofile_info_section:
      opts->x_profile_info_section = ".gcov_info";
      break;

    case OPT_fpatchable_function_entry_:
      {
        HOST_WIDE_INT patch_area_size, patch_area_start;
        parse_and_check_patch_area (arg, true, &patch_area_size, &patch_area_start);
      }
      break;

    case OPT_ftree_vectorize:
      /* Automatically sets -ftree-loop-vectorize and
     -ftree-slp-vectorize.  Nothing more to do here.  */
      break;
    case OPT_fzero_call_used_regs_:
      opts->x_flag_zero_call_used_regs
    = parse_zero_call_used_regs_options (arg);
      break;

    case OPT_fshow_column:
      dc->m_show_column = value;
      break;

    case OPT_frandom_seed:
      /* The real switch is -fno-random-seed.  */
      if (value)
    return false;
      /* Deferred.  */
      break;

    case OPT_frandom_seed_:
      /* Deferred.  */
      break;

    case OPT_fsched_verbose_:
#ifdef INSN_SCHEDULING
      /* Handled with Var in common.opt.  */
      break;
#else
      return false;
#endif

    case OPT_fsched_stalled_insns_:
      opts->x_flag_sched_stalled_insns = value;
      if (opts->x_flag_sched_stalled_insns == 0)
          opts->x_flag_sched_stalled_insns = -1;
      break;

    case OPT_fsched_stalled_insns_dep_:
      opts->x_flag_sched_stalled_insns_dep = value;
      break;

    case OPT_fstack_check_:
      if (!strcmp (arg, "no"))
          opts->x_flag_stack_check = NO_STACK_CHECK;
      else if (!strcmp (arg, "generic"))
        /* This is the old stack checking method.  */
        opts->x_flag_stack_check = STACK_CHECK_BUILTIN
                   ? FULL_BUILTIN_STACK_CHECK
                   : GENERIC_STACK_CHECK;
      else if (!strcmp (arg, "specific"))
        /* This is the new stack checking method.  */
        opts->x_flag_stack_check = STACK_CHECK_BUILTIN
                   ? FULL_BUILTIN_STACK_CHECK
                   : STACK_CHECK_STATIC_BUILTIN
                     ? STATIC_BUILTIN_STACK_CHECK
                     : GENERIC_STACK_CHECK;
      else
          warning_at (loc, 0, "unknown stack check parameter %qs", arg);
      break;

    case OPT_fstack_limit:
      /* The real switch is -fno-stack-limit.  */
      if (value)
          return false;
      /* Deferred.  */
      break;

    case OPT_fstack_limit_register_:
    case OPT_fstack_limit_symbol_:
      /* Deferred.  */
      break;

    case OPT_fstack_usage:
      opts->x_flag_stack_usage = value;
      opts->x_flag_stack_usage_info = value != 0;
      break;

    case OPT_g:
      set_debug_level(self,NO_DEBUG, DEFAULT_GDB_EXTENSIONS, arg, opts, opts_set,loc);
      break;

    case OPT_gcodeview:
      break;

    case OPT_gbtf:
      set_debug_level(self,BTF_DEBUG, false, arg, opts, opts_set, loc);
      /* set the debug level to level 2, but if already at level 3,
     don't lower it.  */
      if (opts->x_debug_info_level < DINFO_LEVEL_NORMAL)
          opts->x_debug_info_level = DINFO_LEVEL_NORMAL;
      break;

    case OPT_gctf:
      set_debug_level(self,CTF_DEBUG, false, arg, opts, opts_set, loc);
      /* CTF generation feeds off DWARF dies.  For optimal CTF, switch debug
     info level to 2.  If off or at level 1, set it to level 2, but if
     already at level 3, don't lower it.  */
      if (opts->x_debug_info_level < DINFO_LEVEL_NORMAL
        && opts->x_ctf_debug_info_level > CTFINFO_LEVEL_NONE)
          opts->x_debug_info_level = DINFO_LEVEL_NORMAL;
      break;

    case OPT_gdwarf:
      if (arg && strlen (arg) != 0){
          error_at (loc, "%<-gdwarf%s%> is ambiguous;use %<-gdwarf-%s%> for DWARF version or %<-gdwarf%> %<-g%s%> for debug level", arg, arg, arg);
          break;
      }else
        value = opts->x_dwarf_version;

      /* FALLTHRU */
    case OPT_gdwarf_:
      if (value < 2 || value > 5)
          error_at (loc, "dwarf version %wu is not supported", value);
      else
          opts->x_dwarf_version = value;
      set_debug_level(self,DWARF2_DEBUG, false, "", opts, opts_set, loc);
      break;

    case OPT_ggdb:
      set_debug_level(self,NO_DEBUG, 2, arg, opts, opts_set, loc);
      break;

    case OPT_gvms:
      set_debug_level(self,VMS_DEBUG, false, arg, opts, opts_set, loc);
      break;

    case OPT_gz:
    case OPT_gz_:
      /* Handled completely via specs.  */
      break;

    case OPT_pedantic_errors:
    {
      dc->m_pedantic_errors = 1;
      int device_OPT_Wpedantic=mtcs_options_optcode_host_to_device(mtcsOptions,OPT_Wpedantic);
      mtcs_opts_control_warning_option/*!control_warning_option*/(self,device_OPT_Wpedantic/*!OPT_Wpedantic*/, DK_ERROR, NULL, value,
                  loc, lang_mask,handlers, opts, opts_set,dc);
    }
      break;

    case OPT_flto:
      opts->x_flag_lto = value ? "" : NULL;
      break;

    case OPT_flto_:
      if (strcmp (arg, "none") != 0
        && strcmp (arg, "jobserver") != 0
        && strcmp (arg, "auto") != 0
        && atoi (arg) == 0)
          error_at (loc,"unrecognized argument to %<-flto=%> option: %qs", arg);
      break;

    case OPT_w:
      dc->m_inhibit_warnings = true;
      break;

    case OPT_fmax_errors_:
      dc->set_max_errors (value);
      break;

    case OPT_fuse_ld_bfd:
    case OPT_fuse_ld_gold:
    case OPT_fuse_ld_lld:
    case OPT_fuse_ld_mold:
    case OPT_fuse_linker_plugin:
      /* No-op. Used by the driver and passed to us because it starts with f.*/
      break;

    case OPT_fwrapv:
      if (value)
          opts->x_flag_trapv = 0;
      break;

    case OPT_ftrapv:
      if (value)
          opts->x_flag_wrapv = 0;
      break;

    case OPT_fstrict_overflow:
      opts->x_flag_wrapv = !value;
      opts->x_flag_wrapv_pointer = !value;
      if (!value)
          opts->x_flag_trapv = 0;
      break;

    case OPT_fipa_icf:
      opts->x_flag_ipa_icf_functions = value;
      opts->x_flag_ipa_icf_variables = value;
      break;

    case OPT_falign_loops_:
      check_alignment_argument (loc, arg, "loops", &opts->x_flag_align_loops,&opts->x_str_align_loops);
      break;

    case OPT_falign_jumps_:
      check_alignment_argument (loc, arg, "jumps", &opts->x_flag_align_jumps, &opts->x_str_align_jumps);
      break;

    case OPT_falign_labels_:
      check_alignment_argument (loc, arg, "labels",&opts->x_flag_align_labels,&opts->x_str_align_labels);
      break;

    case OPT_falign_functions_:
      check_alignment_argument (loc, arg, "functions",&opts->x_flag_align_functions,&opts->x_str_align_functions);
      break;

    case OPT_ftabstop_:
      /* It is documented that we silently ignore silly values.  */
      if (value >= 1 && value <= 100)
          dc->m_tabstop = value;
      break;

    case OPT_freport_bug:
      dc->set_report_bug (value);
      break;

    case OPT_fmultiflags:
      gcc_checking_assert (lang_mask == CL_DRIVER);
      break;

    default:
      /* If the flag was handled in a standard way, assume the lack of
     processing here is intentional.  */
      gcc_assert (mtcs_opts_option_flag_var/*!option_flag_var*/(self,scode, opts));
      break;
  }

  mtcs_options_common_handle_option_auto/*!common_handle_option_auto*/(mtcsOptions,opts, opts_set, decoded, lang_mask, kind,loc, handlers, dc);
  return true;
}

/* An option that is undocumented, that takes a joined argument, and
   that doesn't fit any of the classes of uses (language/common,
   driver, target) is assumed to be a prefix used to catch
   e.g. negated options, and stop them from being further shortened to
   a prefix that could use the negated option as an argument.  For
   example, we want -gno-statement-frontiers to be taken as a negation
   of -gstatement-frontiers, but without catching the gno- prefix and
   signaling it's to be used for option remapping, it would end up
   backtracked to g with no-statemnet-frontiers as the debug level.  */

static bool remapping_prefix_p (const struct cl_option *opt)
{
  return opt->flags & CL_UNDOCUMENTED && opt->flags & CL_JOINED
    && !(opt->flags & (CL_DRIVER | CL_TARGET | CL_COMMON | CL_LANG_ALL));
}

/* Perform a binary search to find which option the command-line INPUT
   matches.  Returns its index in the option array, and
   OPT_SPECIAL_unknown on failure.

   This routine is quite subtle.  A normal binary search is not good
   enough because some options can be suffixed with an argument, and
   multiple sub-matches can occur, e.g. input of "-pedantic" matching
   the initial substring of "-pedantic-errors".

   A more complicated example is -gstabs.  It should match "-g" with
   an argument of "stabs".  Suppose, however, that the number and list
   of switches are such that the binary search tests "-gen-decls"
   before having tested "-g".  This doesn't match, and as "-gen-decls"
   is less than "-gstabs", it will become the lower bound of the
   binary search range, and "-g" will never be seen.  To resolve this
   issue, 'optc-gen.awk' makes "-gen-decls" point, via the back_chain member,
   to "-g" so that failed searches that end between "-gen-decls" and
   the lexicographically subsequent switch know to go back and see if
   "-g" causes a match (which it does in this example).

   This search is done in such a way that the longest match for the
   front end in question wins.  If there is no match for the current
   front end, the longest match for a different front end is returned
   (or N_OPTS if none) and the caller emits an error message.  */
//原型 find_opt opts.h opts-common.cc
//返回是设备的opt_code
size_t mtcs_opts_find_opt(MtcsOpts *self,const char *input, unsigned int lang_mask)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  size_t mn, mn_orig, mx, md, opt_len;
  size_t match_wrong_lang;
  int comp;
  mn = 0;
  mx = mtcsOptions->clOptionsCount/*!cl_options_count*/;

  /* Find mn such this lexicographical inequality holds:
     cl_options[mn] <= input < cl_options[mn + 1].  */
  while (mx - mn > 1){
      md = (mn + mx) / 2;
      opt_len = mtcsOptions->clOptions/*!cl_options*/[md].opt_len;
      comp = strncmp (input, mtcsOptions->clOptions/*!cl_options*/[md].opt_text + 1, opt_len);
      if (comp < 0)
          mx = md;
      else
          mn = md;
  }

  mn_orig = mn;
 // fprintf(stderr,"mtcs_opts_find_opt 00 mn :%d clOptionsCount:%d mx:%d\n",mn,mtcsOptions->clOptionsCount,mx);

  /* This is the switch that is the best match but for a different
     front end, or OPT_SPECIAL_unknown if there is no match at all.  */
  int deviceOptcode_OPT_SPECIAL_unknown=mtcs_options_optcode_host_to_device(mtcsOptions,OPT_SPECIAL_unknown);
  match_wrong_lang = deviceOptcode_OPT_SPECIAL_unknown/*!OPT_SPECIAL_unknown*/;
 // fprintf(stderr,"mtcs_opts_find_opt 11 match_wrong_lang :%d hostOPT_SPECIAL_unknown:%d\n",match_wrong_lang,OPT_SPECIAL_unknown);
  /* Backtrace the chain of possible matches, returning the longest
     one, if any, that fits best.  With current GCC switches, this
     loop executes at most twice.  */
  do{
      const struct cl_option *opt = &mtcsOptions->clOptions/*!cl_options*/[mn];

      /* Is the input either an exact match or a prefix that takes a
     joined argument?  */
      if (!strncmp (input, opt->opt_text + 1, opt->opt_len) && (input[opt->opt_len] == '\0' || (opt->flags & CL_JOINED))){
          /* If language is OK, return it.  */
          if (opt->flags & lang_mask)
            return mn;
          if (remapping_prefix_p (opt))
            return deviceOptcode_OPT_SPECIAL_unknown/*!OPT_SPECIAL_unknown*/;
          /* If we haven't remembered a prior match, remember this
             one.  Any prior match is necessarily better.  */
          if (match_wrong_lang == deviceOptcode_OPT_SPECIAL_unknown/*!OPT_SPECIAL_unknown*/)
            match_wrong_lang = mn;
      }

      /* Try the next possibility.  This is cl_options_count if there
     are no more.  */
      mn = opt->back_chain;
  }while (mn != mtcsOptions->clOptionsCount/*!cl_options_count*/);

  if (match_wrong_lang == deviceOptcode_OPT_SPECIAL_unknown/*!OPT_SPECIAL_unknown*/ && input[0] == '-'){
      /* Long options, starting "--", may be abbreviated if the
     abbreviation is unambiguous.  This only applies to options
     not taking a joined argument, and abbreviations of "--option"
     are permitted even if there is a variant "--option=".  */
      size_t mnc = mn_orig + 1;
      size_t cmp_len = strlen (input);
      //fprintf(stderr,"mtcs_opts_find_opt 22 mnc:%d input:%s\n",mnc,input);

      while (mnc <  mtcsOptions->clOptionsCount/*!cl_options_count*/
              && strncmp (input, mtcsOptions->clOptions/*!cl_options*/[mnc].opt_text + 1, cmp_len) == 0){
          /* Option matching this abbreviation.  OK if it is the first
             match and that does not take a joined argument, or the
             second match, taking a joined argument and with only '='
             added to the first match; otherwise considered
             ambiguous.  */
          if (mnc == mn_orig + 1  && !(mtcsOptions->clOptions/*!cl_options*/[mnc].flags & CL_JOINED))
            match_wrong_lang = mnc;
          else if (mnc == mn_orig + 2
               && match_wrong_lang == mn_orig + 1
               && (mtcsOptions->clOptions/*!cl_options*/[mnc].flags & CL_JOINED)
               && (mtcsOptions->clOptions/*!cl_options*/[mnc].opt_len  == mtcsOptions->clOptions/*!cl_options*/[mn_orig + 1].opt_len + 1)
               && strncmp (mtcsOptions->clOptions/*!cl_options*/[mnc].opt_text + 1,
                       mtcsOptions->clOptions/*!cl_options*/[mn_orig + 1].opt_text + 1,
                       mtcsOptions->clOptions/*!cl_options*/[mn_orig + 1].opt_len) == 0)
            ; /* OK, as long as there are no more matches.  */
          else
            return deviceOptcode_OPT_SPECIAL_unknown/*!OPT_SPECIAL_unknown*/;
          mnc++;
      }
  }
  /* Return the best wrong match, or OPT_SPECIAL_unknown if none.  */
  return match_wrong_lang;
}

/* Set a warning option OPT_INDEX (language mask LANG_MASK, option
   handlers HANDLERS) to have diagnostic kind KIND for option
   structures OPTS and OPTS_SET and diagnostic context DC (possibly
   NULL), at location LOC (UNKNOWN_LOCATION for -Werror=).  ARG is the
   argument of the option for joined options, or NULL otherwise.  If IMPLY,
   the warning option in question is implied at this point.  This is
   used by -Werror= and #pragma GCC diagnostic.  */
//原型 control_warning_option opts.h opts-common.cc
void mtcs_opts_control_warning_option (MtcsOpts *self,unsigned int opt_index, int kind, const char *arg,
            bool imply, location_t loc, unsigned int lang_mask,const struct mtcs_cl_option_handlers *handlers,
            MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,diagnostic_context *dc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  const struct cl_option *clOpt=&mtcsOptions->clOptions[opt_index];
  int hostOptcodeForAliasTarget=mtcs_options_optcode_device_to_host(mtcsOptions,clOpt->alias_target);
  if (hostOptcodeForAliasTarget/*!cl_options[opt_index].alias_target*/ != N_OPTS){
      gcc_assert (!clOpt->cl_separate_alias && !clOpt->cl_negative_alias);
      if (clOpt->alias_arg)
          arg = clOpt->alias_arg;
      opt_index = clOpt->alias_target;
  }
  int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,opt_index);
  if (hostOptcode/*!opt_index*/ == OPT_SPECIAL_ignore || hostOptcode/*!opt_index*/ == OPT_SPECIAL_warn_removed)
    return;
  if (dc)
    diagnostic_classify_diagnostic (dc, hostOptcode/*!opt_index*/, (diagnostic_t) kind, loc);
  if (imply){
      /* -Werror=foo implies -Wfoo.  */
      const struct cl_option *option = &mtcsOptions->clOptions[opt_index];/*!cl_options[opt_index];*/
      HOST_WIDE_INT value = 1;

      if (option->var_type == CLVC_INTEGER  || option->var_type == CLVC_ENUM || option->var_type == CLVC_SIZE){
          if (arg && *arg == '\0' && !option->cl_missing_ok)
            arg = NULL;
          if ((option->flags & CL_JOINED) && arg == NULL){
              cmdline_handle_error(self,loc, option, option->opt_text, arg,CL_ERR_MISSING_ARG, lang_mask);
              return;
          }
           /* If the switch takes an integer argument, convert it.  */
          if (arg && (option->cl_uinteger || option->cl_host_wide_int)){
              int error = 0;
              value = *arg ? integral_argument (arg, &error,option->cl_byte_size) : 0;
              if (error){
                  cmdline_handle_error(self,loc, option, option->opt_text, arg,CL_ERR_UINT_ARG, lang_mask);
                  return;
              }
          }
          /* If the switch takes an enumerated argument, convert it.  */
          if (arg && option->var_type == CLVC_ENUM){
              const struct cl_enum *e = &mtcsOptions->clEnums/*!cl_enums*/[option->var_enum];
              if (enum_arg_to_value(self,e->values, arg, 0, &value,lang_mask) >= 0){
                  const char *carg = NULL;
                  if (enum_value_to_arg (e->values, &carg, value, lang_mask))
                    arg = carg;
                  gcc_assert (carg != NULL);
              }else{
                  cmdline_handle_error(self,loc, option, option->opt_text, arg, CL_ERR_ENUM_ARG, lang_mask);
                  return;
              }
          }
      }
      mtcs_opts_handle_generated_option/*!handle_generated_option*/(self,opts, opts_set, opt_index, arg, value, lang_mask,kind, loc, handlers, false, dc);
  }
}


/* Structure describing mappings from options on the command line to
   options to look up with find_opt.  */
struct option_map
{
  /* Prefix of the option on the command line.  */
  const char *opt0;
  /* If two argv elements are considered to be merged into one option,
     prefix for the second element, otherwise NULL.  */
  const char *opt1;
  /* The new prefix to map to.  */
  const char *new_prefix;
  /* Whether at least one character is needed following opt1 or opt0
     for this mapping to be used.  (--optimize= is valid for -O, but
     --warn- is not valid for -W.)  */
  bool another_char_needed;
  /* Whether the original option is a negated form of the option
     resulting from this map.  */
  bool negated;
};
static const struct option_map option_map[] =
  {
    { "-Wno-", NULL, "-W", false, true },
    { "-fno-", NULL, "-f", false, true },
    { "-gno-", NULL, "-g", false, true },
    { "-mno-", NULL, "-m", false, true },
    { "--debug=", NULL, "-g", false, false },
    { "--machine-", NULL, "-m", true, false },
    { "--machine-no-", NULL, "-m", false, true },
    { "--machine=", NULL, "-m", false, false },
    { "--machine=no-", NULL, "-m", false, true },
    { "--machine", "", "-m", false, false },
    { "--machine", "no-", "-m", false, true },
    { "--optimize=", NULL, "-O", false, false },
    { "--std=", NULL, "-std=", false, false },
    { "--std", "", "-std=", false, false },
    { "--warn-", NULL, "-W", true, false },
    { "--warn-no-", NULL, "-W", false, true },
    { "--", NULL, "-f", true, false },
    { "--no-", NULL, "-f", false, true }
  };


/* Decode the switch beginning at ARGV for the language indicated by
   LANG_MASK (including CL_COMMON and CL_TARGET if applicable), into
   the structure *DECODED.  Returns the number of switches
   consumed.  */
//原型 decode_cmdline_option opts-common.cc
static unsigned int decode_cmdline_option (MtcsOpts *self,const char *const *argv, unsigned int lang_mask,
               struct cl_decoded_option *decoded)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  size_t opt_index;
  const char *arg = 0;
  HOST_WIDE_INT value = 1, mask = 0;
  unsigned int result = 1, i, extra_args, separate_args = 0;
  int adjust_len = 0;
  size_t total_len;
  char *p;
  const struct cl_option *option;
  int errors = 0;
  const char *warn_message = NULL;
  bool separate_arg_flag;
  bool joined_arg_flag;
  bool have_separate_arg = false;

  extra_args = 0;

  int device_N_OPTS=mtcs_options_optcode_host_to_device(mtcsOptions,N_OPTS);/*!N_OPTS*/

  const char *opt_value = argv[0] + 1;
  opt_index = mtcs_opts_find_opt/*!find_opt*/(self,opt_value, lang_mask);
  //n_debug("mtcsopts.c decode_cmdline_option 00 opt_value:%s lang_mask:%d opt_index:%d\n",opt_value,lang_mask,opt_index);
  int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,opt_index);
  i = 0;
  while (hostOptcode/*!opt_index*/ == OPT_SPECIAL_unknown  && i < ARRAY_SIZE (option_map)){
      const char *opt0 = option_map[i].opt0;
      const char *opt1 = option_map[i].opt1;
      const char *new_prefix = option_map[i].new_prefix;
      bool another_char_needed = option_map[i].another_char_needed;
      size_t opt0_len = strlen (opt0);
      size_t opt1_len = (opt1 == NULL ? 0 : strlen (opt1));
      size_t optn_len = (opt1 == NULL ? opt0_len : opt1_len);
      size_t new_prefix_len = strlen (new_prefix);
      //n_debug("mtcsopts.c decode_cmdline_option 11 opt_value:%s lang_mask:%d opt_index:%d\n",opt_value,lang_mask,opt_index);

      extra_args = (opt1 == NULL ? 0 : 1);
      value = !option_map[i].negated;

      if (strncmp (argv[0], opt0, opt0_len) == 0
        && (opt1 == NULL || (argv[1] != NULL && strncmp (argv[1], opt1, opt1_len) == 0))
        && (!another_char_needed  || argv[extra_args][optn_len] != 0)){
          size_t arglen = strlen (argv[extra_args]);
          char *dup;

          adjust_len = (int) optn_len - (int) new_prefix_len;
          dup = XNEWVEC (char, arglen + 1 - adjust_len);
          memcpy (dup, new_prefix, new_prefix_len);
          memcpy (dup + new_prefix_len, argv[extra_args] + optn_len,arglen - optn_len + 1);
          opt_index = mtcs_opts_find_opt/*!find_opt*/(self,dup + 1, lang_mask);
          free (dup);
      }
      i++;
  }

  if (hostOptcode/*!opt_index*/== OPT_SPECIAL_unknown){
      arg = argv[0];
      extra_args = 0;
      value = 1;
      goto done;
  }
  option = &mtcsOptions->clOptions/*!cl_options*/[opt_index];
  //n_debug("mtcsopts.c decode_cmdline_option 22 opt_value:%s result:%d opt_index:%d\n",opt_value,result,opt_index);

  /* Reject negative form of switches that don't take negatives as
     unrecognized.  */
  if (!value && option->cl_reject_negative){
      opt_index = mtcs_options_optcode_host_to_device(mtcsOptions,OPT_SPECIAL_unknown)/*!OPT_SPECIAL_unknown*/;
     // n_debug("mtcsopts.c decode_cmdline_option 33 opt_value:%s result:%d opt_index:%d\n",opt_value,result,opt_index);

      errors |= CL_ERR_NEGATIVE;
      arg = argv[0];
      goto done;
  }
  /* Clear the initial value for size options (it will be overwritten
     later based on the Init(value) specification in the opt file.  */
  if (option->var_type == CLVC_SIZE)
    value = 0;
  result = extra_args + 1;
  //n_debug("mtcsopts.c decode_cmdline_option ---33 opt_value:%s result:%d extra_args:%d\n",opt_value,result,extra_args);

  warn_message = option->warn_message;
  /* Check to see if the option is disabled for this configuration.  */
  if (option->cl_disabled)
    errors |= CL_ERR_DISABLED;
  /* Determine whether there may be a separate argument based on
     whether this option is being processed for the driver, and, if
     so, how many such arguments.  */
  separate_arg_flag = ((option->flags & CL_SEPARATE)  && !(option->cl_no_driver_arg && (lang_mask & CL_DRIVER)));
  separate_args = (separate_arg_flag ? option->cl_separate_nargs + 1: 0);
  joined_arg_flag = (option->flags & CL_JOINED) != 0;

  /* Sort out any argument the switch takes.  */
  if (joined_arg_flag){
      /* Have arg point to the original switch.  This is because
     some code, such as disable_builtin_function, expects its
     argument to be persistent until the program exits.  */
      arg = argv[extra_args] + mtcsOptions->clOptions/*!cl_options*/[opt_index].opt_len + 1 + adjust_len;
      if (*arg == '\0' && !option->cl_missing_ok){
          if (separate_arg_flag){
              arg = argv[extra_args + 1];
              result = extra_args + 2;
              if (arg == NULL)
                  result = extra_args + 1;
              else
                  have_separate_arg = true;
          }else
            /* Missing argument.  */
            arg = NULL;
      }
  }else if (separate_arg_flag){
      arg = argv[extra_args + 1];
      for (i = 0; i < separate_args; i++)
          if (argv[extra_args + 1 + i] == NULL){
              errors |= CL_ERR_MISSING_ARG;
              break;
          }
      result = extra_args + 1 + i;
      if (arg != NULL)
          have_separate_arg = true;
  }

  if (arg == NULL && (separate_arg_flag || joined_arg_flag))
    errors |= CL_ERR_MISSING_ARG;

  /* Is this option an alias (or an ignored option, marked as an alias
     of OPT_SPECIAL_ignore)?  */
  if (option->alias_target != device_N_OPTS/*!N_OPTS*/  && (!option->cl_separate_alias || have_separate_arg)){
      size_t new_opt_index = option->alias_target;
      int hostOptcode1=mtcs_options_optcode_device_to_host(mtcsOptions,new_opt_index);
      if (hostOptcode1/*!new_opt_index*/ == OPT_SPECIAL_ignore  || hostOptcode1/*!new_opt_index*/ == OPT_SPECIAL_warn_removed){
          gcc_assert (option->alias_arg == NULL);
          gcc_assert (option->neg_alias_arg == NULL);
          opt_index = new_opt_index;
          arg = NULL;
      }else{
          const struct cl_option *new_option = &mtcsOptions->clOptions/*!cl_options*/[new_opt_index];
          /* The new option must not be an alias itself.  */
          gcc_assert (new_option->alias_target ==device_N_OPTS/*!N_OPTS*/ || new_option->cl_separate_alias);

          if (option->neg_alias_arg){
              gcc_assert (option->alias_arg != NULL);
              gcc_assert (arg == NULL);
              gcc_assert (!option->cl_negative_alias);
              if (value)
                  arg = option->alias_arg;
              else
                  arg = option->neg_alias_arg;
              value = 1;
          }else if (option->alias_arg){
              gcc_assert (value == 1);
              gcc_assert (arg == NULL);
              gcc_assert (!option->cl_negative_alias);
              arg = option->alias_arg;
          }

          if (option->cl_negative_alias)
            value = !value;

          opt_index = new_opt_index;
          option = new_option;

          if (value == 0)
            gcc_assert (!option->cl_reject_negative);

          /* Recompute what arguments are allowed.  */
          separate_arg_flag = ((option->flags & CL_SEPARATE)  && !(option->cl_no_driver_arg  && (lang_mask & CL_DRIVER)));
          joined_arg_flag = (option->flags & CL_JOINED) != 0;
          if (separate_args > 1 || option->cl_separate_nargs)
            gcc_assert (separate_args == (unsigned int) option->cl_separate_nargs + 1);

          if (!(errors & CL_ERR_MISSING_ARG)){
              if (separate_arg_flag || joined_arg_flag){
                  if (option->cl_missing_ok && arg == NULL)
                    arg = "";
                  gcc_assert (arg != NULL);
              }else
                  gcc_assert (arg == NULL);
          }

          /* Recheck for warnings and disabled options.  */
          if (option->warn_message){
              gcc_assert (warn_message == NULL);
              warn_message = option->warn_message;
          }
          if (option->cl_disabled)
            errors |= CL_ERR_DISABLED;
      }
  }
  n_debug("mtcsopts.c decode_cmdline_option 44 opt_value:%s result:%d opt_index:%d errors:%d\n",opt_value,result,opt_index,errors);

  /* Check if this is a switch for a different front end.  */
  if (!option_ok_for_language (option, lang_mask)){
     n_debug("mtcsopts.c decode_cmdline_option 44-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d\n",
           opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG);

    errors |= CL_ERR_WRONG_LANG;
  }
  else if (strcmp (option->opt_text, "-Werror=") == 0  && strchr (opt_value, ',') == NULL){
      /* Verify that -Werror argument is a valid warning
     for a language.  */
      char *werror_arg = xstrdup (opt_value + 6);
      werror_arg[0] = 'W';
      size_t warning_index = mtcs_opts_find_opt/*!find_opt*/(self,werror_arg, lang_mask);
      free (werror_arg);
      int hostOptcode2=mtcs_options_optcode_device_to_host(mtcsOptions,warning_index);
      n_debug("mtcsopts.c decode_cmdline_option 44aa-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d hostOptcode2:%d\n",
            opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG,hostOptcode2);

      if (hostOptcode2/*!warning_index*/ != OPT_SPECIAL_unknown){
          const struct cl_option *warning_option= &mtcsOptions->clOptions/*!cl_options*/[warning_index];
          if (!option_ok_for_language (warning_option, lang_mask))
            errors |= CL_ERR_WRONG_LANG;
          n_debug("mtcsopts.c decode_cmdline_option 44bb-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d hostOptcode2:%d\n",
                opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG,hostOptcode2);
      }
  }

  /* Convert the argument to lowercase if appropriate.  */
  if (arg && option->cl_tolower){
      size_t j;
      size_t len = strlen (arg);
      char *arg_lower = XOBNEWVEC (&opts_obstack, char, len + 1);
      for (j = 0; j < len; j++)
          arg_lower[j] = TOLOWER ((unsigned char) arg[j]);
      arg_lower[len] = 0;
      arg = arg_lower;
  }
  /* If the switch takes an integer argument, convert it.  */
  if (arg && (option->cl_uinteger || option->cl_host_wide_int)){
      int error = 0;
      value = *arg ? integral_argument (arg, &error, option->cl_byte_size) : 0;
      if (error)
          errors |= CL_ERR_UINT_ARG;
      /* Reject value out of a range.  */
      if (option->range_max != -1 && (value < option->range_min || value > option->range_max))
          errors |= CL_ERR_INT_RANGE_ARG;
      n_debug("mtcsopts.c decode_cmdline_option 44cc-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d \n",
            opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG);
  }
  /* If the switch takes an enumerated argument, convert it.  */
  if (arg && (option->var_type == CLVC_ENUM)){
      const struct cl_enum *e = &mtcsOptions->clEnums/*!cl_enums*/[option->var_enum];
      gcc_assert (option->var_value != CLEV_NORMAL || value == 1);
      n_debug("mtcsopts.c decode_cmdline_option 44ddxx-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d %d var_enum:%d\n",
              opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG,option->var_value,option->var_enum);
      if (option->var_value != CLEV_NORMAL){
         n_debug("mtcsopts.c decode_cmdline_option 44dd-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d\n",
               opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG);
          const char *p = arg;
          HOST_WIDE_INT sum_value = 0;
          unsigned HOST_WIDE_INT used_sets = 0;
          do{
              const char *q = strchr (p, ',');
              HOST_WIDE_INT this_value = 0;
              if (q && q == p){
                  errors |= CL_ERR_ENUM_SET_ARG;
                  break;
              }
              int idx = enum_arg_to_value(self,e->values, p, q ? q - p : 0,&this_value, lang_mask);
              if (idx < 0){
                  errors |= CL_ERR_ENUM_SET_ARG;
                  break;
              }
              HOST_WIDE_INT this_mask = 0;
              if (option->var_value == CLEV_SET){
                  unsigned set = e->values[idx].flags >> CL_ENUM_SET_SHIFT;
                  gcc_checking_assert (set >= 1  && set <= HOST_BITS_PER_WIDE_INT);
                  if ((used_sets & (HOST_WIDE_INT_1U << (set - 1))) != 0){
                      errors |= CL_ERR_ENUM_SET_ARG;
                      break;
                  }
                  used_sets |= HOST_WIDE_INT_1U << (set - 1);
                  for (int i = 0; e->values[i].arg != NULL; i++)
                    if (set == (e->values[i].flags >> CL_ENUM_SET_SHIFT))
                      this_mask |= e->values[i].value;
              }else{
                  gcc_assert (option->var_value == CLEV_BITSET  && ((e->values[idx].flags >> CL_ENUM_SET_SHIFT) == 0));
                  this_mask = this_value;
              }
              sum_value |= this_value;
              mask |= this_mask;
              if (q == NULL)
                  break;
              p = q + 1;
          }while (1);
          if (value == 1)
            value = sum_value;
          else
            gcc_checking_assert (value == 0);
      }else if (enum_arg_to_value(self,e->values, arg, 0, &value, lang_mask) >= 0){
         n_debug("mtcsopts.c decode_cmdline_option 44dd--tt opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d\n",
                 opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG);
          const char *carg = NULL;
          if (enum_value_to_arg (e->values, &carg, value, lang_mask))
            arg = carg;
          gcc_assert (carg != NULL);
      }else{
         n_debug("mtcsopts.c decode_cmdline_option 44dd-- opt_value:%s result:%d opt_index:%d errors:%d CL_ERR_WRONG_LANG:%d\n",
               opt_value,result,opt_index,errors,CL_ERR_WRONG_LANG);
         errors |= CL_ERR_ENUM_ARG;
      }
  }

done:
  decoded->opt_index = opt_index;
  decoded->arg = arg;
  decoded->value = value;
  decoded->mask = mask;
  decoded->errors = errors;
  decoded->warn_message = warn_message;
  int hostOptcode3=mtcs_options_optcode_device_to_host(mtcsOptions,opt_index);
  n_debug("mtcsopts.c decode_cmdline_option 55 opt_value:%s result:%d opt_index:%d errors:%d\n",opt_value,result,opt_index,errors);

  if (hostOptcode3/*!opt_index*/ == OPT_SPECIAL_unknown)
    gcc_assert (result == 1);

  gcc_assert (result >= 1 && result <= ARRAY_SIZE (decoded->canonical_option));
  decoded->canonical_option_num_elements = result;
  total_len = 0;
  for (i = 0; i < ARRAY_SIZE (decoded->canonical_option); i++) {
      if (i < result){
          size_t len;
          if (hostOptcode3/*!opt_index*/ == OPT_SPECIAL_unknown)
              decoded->canonical_option[i] = argv[i];
          else
              decoded->canonical_option[i] = NULL;
          len = strlen (argv[i]);
          /* If the argument is an empty string, we will print it as "" in
             orig_option_with_args_text.  */
          total_len += (len != 0 ? len : 2) + 1;
      }else
          decoded->canonical_option[i] = NULL;
  }
  if (hostOptcode3/*!opt_index*/ != OPT_SPECIAL_unknown && hostOptcode3/*!opt_index*/ != OPT_SPECIAL_ignore
      && hostOptcode3/*!opt_index*/ != OPT_SPECIAL_warn_removed){
      generate_canonical_option(self,opt_index, arg, value, decoded);
      if (separate_args > 1){
          for (i = 0; i < separate_args; i++){
              if (argv[extra_args + 1 + i] == NULL)
                  break;
              else
                  decoded->canonical_option[1 + i] = argv[extra_args + 1 + i];
          }
          gcc_assert (result == 1 + i);
          decoded->canonical_option_num_elements = result;
      }
  }
  decoded->orig_option_with_args_text= p = XOBNEWVEC (&opts_obstack, char, total_len);
  for (i = 0; i < result; i++){
      size_t len = strlen (argv[i]);
      /* Print the empty string verbally.  */
      if (len == 0){
          *p++ = '"';
          *p++ = '"';
      }else
          memcpy (p, argv[i], len);
      p += len;
      if (i == result - 1)
          *p++ = 0;
      else
          *p++ = ' ';
  }
  return result;
}

/* Return true if NEXT_OPT_IDX cancels OPT_IDX.  Return false if the
   next one is the same as ORIG_NEXT_OPT_IDX.  */
//原型 cancel_option opts-common.cc
static bool cancel_option (MtcsOpts *self,int opt_idx, int next_opt_idx, int orig_next_opt_idx)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  /* An option can be canceled by the same option or an option with
     Negative.  */
  //fprintf(stderr,"mtcsopts.c cancel_option 00 opt_idx:%d next_opt_idx:%d orig_next_opt_idx:%d\n",opt_idx,next_opt_idx,orig_next_opt_idx);
  if (mtcsOptions->clOptions/*!cl_options*/[next_opt_idx].neg_index == opt_idx)
    return true;

  if (mtcsOptions->clOptions/*!cl_options*/ [next_opt_idx].neg_index != orig_next_opt_idx)
    return cancel_option (self,opt_idx, mtcsOptions->clOptions/*!cl_options*/ [next_opt_idx].neg_index,
              orig_next_opt_idx);

  return false;
}

/* Filter out options canceled by the ones after them, and related
   rearrangement.  */
//原型 prune_options opts-common.cc
static void prune_options (MtcsOpts *self,struct cl_decoded_option **decoded_options,
           unsigned int *decoded_options_count)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  int clOptionsCount=mtcsOptions->clOptionsCount;
  unsigned int old_decoded_options_count = *decoded_options_count;
  struct cl_decoded_option *old_decoded_options = *decoded_options;
  unsigned int new_decoded_options_count;
  struct cl_decoded_option *new_decoded_options = XNEWVEC (struct cl_decoded_option, old_decoded_options_count);
  unsigned int i;
  const struct cl_option *option;
  unsigned int options_to_prepend = 0;
  unsigned int Wcomplain_wrong_lang_idx = 0;
  unsigned int fdiagnostics_color_idx = 0;
  int hostOptcode=0;
  /* Remove arguments which are negated by others after them.  */
  new_decoded_options_count = 0;
  for (i = 0; i < old_decoded_options_count; i++){
      unsigned int j, opt_idx, next_opt_idx;
      if (old_decoded_options[i].errors & ~CL_ERR_WRONG_LANG)
          goto keep;
      opt_idx = old_decoded_options[i].opt_index;
      hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,opt_idx);

      switch (hostOptcode/*!opt_idx*/){
        case OPT_SPECIAL_unknown:
        case OPT_SPECIAL_ignore:
        case OPT_SPECIAL_warn_removed:
        case OPT_SPECIAL_program_name:
        case OPT_SPECIAL_input_file:
          goto keep;

        /* Do not handle the following yet, just remember the last one.  */
        case OPT_Wcomplain_wrong_lang:
          gcc_checking_assert (i != 0);
          if (Wcomplain_wrong_lang_idx == 0)
            ++options_to_prepend;
          Wcomplain_wrong_lang_idx = i;
          continue;
        case OPT_fdiagnostics_color_:
          gcc_checking_assert (i != 0);
          if (fdiagnostics_color_idx == 0)
            ++options_to_prepend;
          fdiagnostics_color_idx = i;
          continue;

        default:
          gcc_assert (opt_idx < clOptionsCount/*!cl_options_count*/);
          option = &mtcsOptions->clOptions/*!cl_options*/[opt_idx];
          if (option->neg_index < 0)
            goto keep;

          /* Skip joined switches.  */
          if ((option->flags & CL_JOINED) && (!option->cl_reject_negative || (unsigned int) option->neg_index != opt_idx))
            goto keep;

          for (j = i + 1; j < old_decoded_options_count; j++){
              if (old_decoded_options[j].errors & ~CL_ERR_WRONG_LANG)
                  continue;
              next_opt_idx = old_decoded_options[j].opt_index;
              if (next_opt_idx >= clOptionsCount/*!cl_options_count*/)
                  continue;
              if (mtcsOptions->clOptions/*!cl_options*/[next_opt_idx].neg_index < 0)
                  continue;
              if ((mtcsOptions->clOptions/*!cl_options*/[next_opt_idx].flags & CL_JOINED)
                  && (!mtcsOptions->clOptions/*!cl_options*/[next_opt_idx].cl_reject_negative
                  || ((unsigned int) mtcsOptions->clOptions/*!cl_options*/[next_opt_idx].neg_index!= next_opt_idx)))
                  continue;
              //fprintf(stderr,"mtcsopts.c prune_options 00 opt_idx:%d, next_opt_idx:%d j:%d\n",opt_idx, next_opt_idx,j);
              if (cancel_option(self,opt_idx, next_opt_idx, next_opt_idx))
                  break;
             // fprintf(stderr,"mtcsopts.c prune_options 11 opt_idx:%d, next_opt_idx:%d j:%d\n",opt_idx, next_opt_idx,j);

          }
          if (j == old_decoded_options_count){
    keep:
              new_decoded_options[new_decoded_options_count]= old_decoded_options[i];
              new_decoded_options_count++;
          }
          break;
      }
 }

  /* For those not yet handled, put (only) the last at a front position after
     'argv[0]', so they can take effect immediately.  */
  if (options_to_prepend){
      const unsigned int argv_0 = 1;
      memmove (new_decoded_options + argv_0 + options_to_prepend,
           new_decoded_options + argv_0,sizeof (struct cl_decoded_option)
           * (new_decoded_options_count - argv_0));
      unsigned int options_prepended = 0;
      if (Wcomplain_wrong_lang_idx != 0){
          new_decoded_options[argv_0 + options_prepended++]= old_decoded_options[Wcomplain_wrong_lang_idx];
          new_decoded_options_count++;
      }
      if (fdiagnostics_color_idx != 0){
          new_decoded_options[argv_0 + options_prepended++]= old_decoded_options[fdiagnostics_color_idx];
          new_decoded_options_count++;
      }
      gcc_checking_assert (options_to_prepend == options_prepended);
  }

  free (old_decoded_options);
  new_decoded_options = XRESIZEVEC (struct cl_decoded_option,
                    new_decoded_options,new_decoded_options_count);
  *decoded_options = new_decoded_options;
  *decoded_options_count = new_decoded_options_count;
}


/* Fill in *DECODED with an option for input file FILE.  */
//原型 generate_option_input_file opts.h opts-common.cc
static void generateOptionInputFile(MtcsOpts *self,const char *file,struct cl_decoded_option *decoded)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  decoded->opt_index =mtcs_options_optcode_host_to_device(mtcsOptions,OPT_SPECIAL_input_file);
  decoded->warn_message = NULL;
  decoded->arg = file;
  decoded->orig_option_with_args_text = file;
  decoded->canonical_option_num_elements = 1;
  decoded->canonical_option[0] = file;
  decoded->canonical_option[1] = NULL;
  decoded->canonical_option[2] = NULL;
  decoded->canonical_option[3] = NULL;
  decoded->value = 1;
  decoded->mask = 0;
  decoded->errors = 0;
}

/* Decode command-line options (ARGC and ARGV being the arguments of
   main) into an array, setting *DECODED_OPTIONS to a pointer to that
   array and *DECODED_OPTIONS_COUNT to the number of entries in the
   array.  The first entry in the array is always one for the program
   name (OPT_SPECIAL_program_name).  LANG_MASK indicates the language
   flags applicable for decoding (including CL_COMMON and CL_TARGET if
   those options should be considered applicable).  Do not produce any
   diagnostics or set state outside of these variables.  */
//原型 decode_cmdline_options_to_array opts.h opts-common.cc
//对象 MtcsOpts中的    struct cl_decoded_option *save_decoded_options; unsigned int save_decoded_options_count;
//在这里赋值。从mtcscompile中调用 mtcs_opts_decode_cmdline_options_to_array
void mtcs_opts_decode_cmdline_options_to_array (MtcsOpts *self,unsigned int argc, const char **argv,
                 unsigned int lang_mask, struct cl_decoded_option **decoded_options, unsigned int *decoded_options_count)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  unsigned int n, i;
  struct cl_decoded_option *opt_array;
  unsigned int num_decoded_options;

  int opt_array_len = argc;
  opt_array = XNEWVEC (struct cl_decoded_option, opt_array_len);
  int deviceOptcode=mtcs_options_optcode_host_to_device(mtcsOptions,OPT_SPECIAL_program_name);
  opt_array[0].opt_index = deviceOptcode/*!OPT_SPECIAL_program_name*/;
  opt_array[0].warn_message = NULL;
  opt_array[0].arg = argv[0];
  opt_array[0].orig_option_with_args_text = argv[0];
  opt_array[0].canonical_option_num_elements = 1;
  opt_array[0].canonical_option[0] = argv[0];
  opt_array[0].canonical_option[1] = NULL;
  opt_array[0].canonical_option[2] = NULL;
  opt_array[0].canonical_option[3] = NULL;
  opt_array[0].value = 1;
  opt_array[0].mask = 0;
  opt_array[0].errors = 0;
  num_decoded_options = 1;
  n_debug("mtcsopts.c decode_cmdline_options_to_array 00 参数个数据:%d\n",argc);

  for (i = 1; i < argc; i += n){
      const char *opt = argv[i];
      n_debug("mtcsopts.c decode_cmdline_options_to_array 11 i:%d %s\n",i,opt);
      /* Interpret "-" or a non-switch as a file name.  */
      if (opt[0] != '-' || opt[1] == '\0'){
          generateOptionInputFile/*!generate_option_input_file*/(self,opt, &opt_array[num_decoded_options]);
          num_decoded_options++;
          n = 1;
          n_debug("mtcsopts.c decode_cmdline_options_to_array 22 opt:%s\n",opt);
          continue;
      }

      /* Interpret "--param" "key=name" as "--param=key=name".  */
      const char *needle = "--param";
      if (i + 1 < argc && strcmp (opt, needle) == 0){
          const char *replacement  = opts_concat (needle, "=", argv[i + 1], NULL);
          argv[++i] = replacement;
          n_debug("mtcsopts.c decode_cmdline_options_to_array 33 %s\n",opt);

      }

      /* Expand -fdiagnostics-plain-output to its constituents.  This needs
     to happen here so that prune_options can handle -fdiagnostics-color
     specially.  */
      if (!strcmp (opt, "-fdiagnostics-plain-output")){
          /* If you have changed the default diagnostics output, and this new
             output is not appropriately "plain" (e.g., the change needs to be
             undone in order for the testsuite to work properly), then please do
             the following:
             1.  Add the necessary option to undo the new behavior to
                 the array below.
             2.  Update the documentation for -fdiagnostics-plain-output
                 in invoke.texi.  */
          const char *const expanded_args[] = {
            "-fno-diagnostics-show-caret",
            "-fno-diagnostics-show-line-numbers",
            "-fdiagnostics-color=never",
            "-fdiagnostics-urls=never",
            "-fdiagnostics-path-format=separate-events",
            "-fdiagnostics-text-art-charset=none"
          };
          const int num_expanded = ARRAY_SIZE (expanded_args);
          opt_array_len += num_expanded - 1;
          opt_array = XRESIZEVEC (struct cl_decoded_option,opt_array, opt_array_len);
          for (int j = 0, nj; j < num_expanded; j += nj){
              nj = decode_cmdline_option(self,expanded_args + j, lang_mask, &opt_array[num_decoded_options]);
              num_decoded_options++;
          }
          n_debug("mtcsopts.c decode_cmdline_options_to_array 44 %s\n",opt);

          n = 1;
          continue;
      }
      const char *const *testargv=argv+i;
      n = decode_cmdline_option(self,argv + i, lang_mask, &opt_array[num_decoded_options]);
      n_debug("mtcsopts.c decode_cmdline_options_to_array 55 i:%d n:%d %s num_decoded_options:%d opt_array_len:%d %d %d %s\n",
            i,n,testargv[0],num_decoded_options,opt_array_len,opt_array[num_decoded_options].errors,opt_array[num_decoded_options].opt_index,
            opt_array[num_decoded_options].orig_option_with_args_text);
      num_decoded_options++;
  }

  *decoded_options = opt_array;
  *decoded_options_count = num_decoded_options;
  prune_options(self,decoded_options, decoded_options_count);
}


MtcsOpts *mtcs_opts_new(MtcsMode *mtcsMode)
{
     MtcsOpts *self = n_slice_alloc0 (sizeof(MtcsOpts));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsOptsInit(self);
     return self;
}

