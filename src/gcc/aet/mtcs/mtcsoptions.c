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
 * base on options.cc
 */
#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "aet/aetprinttree.h"
#include "mtcsoptions.h"

typedef struct _MtcsOptionsBackup
{
       //备份的内容
   struct gcc_options *global;
   struct gcc_options *globalSet;

}MtcsOptionsBackup;

static   void backup_cb(MtcsBackupRestore *iface);
static   void restore_cb(MtcsBackupRestore *iface);

void        mtcs_options_init(MtcsOptions *self)
{
 //  self->x_flag_lto=NULL;//原型  #define flag_lto global_options.x_flag_lto
   //self->x_flag_wpa=NULL;//原型 #define flag_wpa global_options.x_flag_wpa
  // self->x_warn_suggest_final_methods=0; //原型 #define warn_suggest_final_methods global_options.x_warn_suggest_final_methods
   //self->x_warn_suggest_final_types=0; //原型  #define warn_suggest_final_types global_options.x_warn_suggest_final_types
  // self->x_flag_devirtualize=0; //原型  #define flag_devirtualize global_options.x_flag_devirtualize
 //  self->x_flag_devirtualize_speculatively=0; //原型 #define flag_devirtualize_speculatively global_options.x_flag_devirtualize_speculatively
 //  self->x_flag_ipa_cp=0; //原型  #define flag_ipa_cp global_options.x_flag_ipa_cp
  // self->x_flag_ipa_cp_clone=0;//原型 #define flag_ipa_cp_clone global_options.x_flag_ipa_cp_clone
 //  self->x_flag_ipa_vrp=0; //原型  #define flag_ipa_vrp global_options.x_flag_ipa_vrp
 //  self->x_flag_profile_partial_training=0;  //原型 #define flag_profile_partial_training global_options.x_flag_profile_partial_training
  // self->x_flag_ipa_bit_cp=0; //原型  #define flag_ipa_bit_cp global_options.x_flag_ipa_bit_cp
  // self->x_flag_ipa_sra=0; //原型  #define flag_ipa_sra global_options.x_flag_ipa_sra

   self->mtcsBackupRestore.backup=backup_cb;
   self->mtcsBackupRestore.restore=restore_cb;
   self->mtcsBackupRestore.impl=(npointer)self;
   self->backup=(void*) n_slice_alloc0 (sizeof(MtcsOptionsBackup));
}

static   void backup_cb(MtcsBackupRestore *iface)
{
   MtcsOptions *self=(MtcsOptions *)iface->impl;
   MtcsOptionsBackup *backup=(MtcsOptionsBackup *)self->backup;

   if(!backup->global)
      backup->global=xmalloc(sizeof(struct gcc_options));
   if(!backup->globalSet)
      backup->globalSet=xmalloc(sizeof(struct gcc_options));
   memcpy(backup->global,&global_options,sizeof(struct gcc_options));
   memcpy(backup->globalSet,&global_options_set,sizeof(struct gcc_options));
};

static   void restore_cb(MtcsBackupRestore *iface)
{
   MtcsOptions *self=(MtcsOptions *)iface->impl;
   MtcsOptionsBackup *backup=(MtcsOptionsBackup *)self->backup;
   memcpy(&global_options,backup->global,sizeof(struct gcc_options));
   memcpy(&global_options_set,backup->globalSet,sizeof(struct gcc_options));
}

int   mtcs_options_target_soft_stack(MtcsOptions *self)
{
    return self->target_soft_stack(self);
}

//原型 extern void init_options_once (void); opts.h opts-global.cc
void  mtcs_options_init_once(MtcsOptions *self)
{
    /* Perform language-specific options initialization.  */
    //lang_hooks.option_lang_mask () 原型 extern unsigned int lto_option_lang_mask (void); lto.h lto.cc #define LANG_HOOKS_OPTION_LANG_MASK lto_option_lang_mask

    self->initial_lang_mask =CL_LTO;//lang_hooks.option_lang_mask ();

   // lang_hooks.initialize_diagnostics (global_dc);
    /* ??? Ideally, we should do this earlier and the FEs will override
       it if desired (none do it so far).  However, the way the FEs
       construct their pretty-printers means that all previous settings
       are overriden.  */
    //diagnostic_color_init (global_dc);
    //diagnostic_urls_init (global_dc);
}

//原型 extern void init_opts_obstack (void); opts.h opts.cc
void mtcs_options_init_opts_obstack(MtcsOptions *self)
{
    gcc_obstack_init (&self->opts_obstack);
}

//原型  init_options_struct opts.h opts.cc
void mtcs_options_init_options_struct (MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set)
{
    self->init_options_struct(self,opts,opts_set);
}
//原型  void (*init_options_struct) (struct gcc_options *opts); langhooks.h  #define LANG_HOOKS_INIT_OPTIONS_STRUCT lto_init_options_struct
void mtcs_options_hooks_init_options_struct(MtcsOptions *self)
{
    self->hooks_init_options_struct(self);
}

//原型  lang_hooks.init_options (save_decoded_options_count, save_decoded_options); #define LANG_HOOKS_INIT_OPTIONS        lhd_init_options
void mtcs_options_hooks_init_options(MtcsOptions *self,unsigned int decoded_options_count,struct cl_decoded_option *decoded_options)
{
    self->hooks_init_options(self,decoded_options_count,decoded_options);
}

//原型 lang_hooks.complain_wrong_lang_p (option) #define LANG_HOOKS_COMPLAIN_WRONG_LANG_P lto_complain_wrong_lang_p
bool mtcs_options_hooks_complain_wrong_lang_p(MtcsOptions *self,const struct cl_option *option)
{
    return self->hooks_complain_wrong_lang_p(self,option);
}

//原型  lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option
bool mtcs_options_hooks_handle_option(MtcsOptions *self,size_t scode, const char *arg,
           HOST_WIDE_INT value ATTRIBUTE_UNUSED, int kind ATTRIBUTE_UNUSED, location_t loc ATTRIBUTE_UNUSED,
           const struct mtcs_cl_option_handlers *handlers ATTRIBUTE_UNUSED)
{
    return self->hooks_handle_option(self,scode,arg,value,kind,loc,handlers);
}

//设备的opt_code转在主机的opt_code
int mtcs_options_optcode_device_to_host(MtcsOptions *self,int deviceOptcode)
{
    int re= self->optcode_device_to_host(self,deviceOptcode);
    if(re==-1)
        re=deviceOptcode;
    return re;
}

//主机的opt_code转设备的opt_code
int mtcs_options_optcode_host_to_device(MtcsOptions *self,int hostOptcode)
{
    int re= self->optcode_host_to_device(self,hostOptcode);
    if(re==-1)
       re=hostOptcode;
    return re;
}

void mtcs_options_set_cl_options(MtcsOptions *self,struct cl_option *clOptions,int count)
{
   self->clOptions=clOptions;
   self->clOptionsCount=count;
}
//原型 lang_hooks.post_options (&main_input_filename); #define LANG_HOOKS_POST_OPTIONS        lhd_post_options
bool mtcs_options_post_options(MtcsOptions *self,char *fileName)
{
  /* Excess precision other than "fast" requires front-end
     support.  */
  self->global_options->x_flag_excess_precision/*!flag_excess_precision*/= EXCESS_PRECISION_FAST;
  return false;
}

//原型 cl_optimization_save optons.h options-save.cc
void mtcs_options_cl_optimization_save(MtcsOptions *self,struct cl_optimization *ptr, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set)
{
    self->cl_optimization_save(self,ptr,opts,opts_set);
}

//原型 cl_optimization_restore options.h options-save.cc
void mtcs_options_cl_optimization_restore(MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr)
{
    self->cl_optimization_restore(self,opts,opts_set,ptr);
}


//原型 cl_target_option_hash options.h
hashval_t mtcs_options_cl_target_option_hash(MtcsOptions *self,MtcsClTargetOption const *ptr ATTRIBUTE_UNUSED)
{
    return self->cl_target_option_hash(self,ptr);
}

//原型 bool cl_target_option_eq (struct cl_target_option const *ptr1 ATTRIBUTE_UNUSED, struct cl_target_option const *ptr2 ATTRIBUTE_UNUSED)
bool  mtcs_options_cl_target_option_eq(MtcsOptions *self,MtcsClTargetOption const *ptr1 ATTRIBUTE_UNUSED,
        MtcsClTargetOption const *ptr2 ATTRIBUTE_UNUSED)
{
    return self->cl_target_option_eq(self,ptr1,ptr2);
}
//从主机中取出目标需要的选项,并赋值
void mtcs_options_set_options_from_host(MtcsOptions *self)
{
   MtcsOptionsItem *deviceOpts=self->global_options;
   MtcsOptionsItem *deviceOptsSet=self->global_options_set;

   deviceOpts->x_flag_unsafe_math_optimizations = global_options.x_flag_unsafe_math_optimizations;
   deviceOptsSet->x_flag_unsafe_math_optimizations = global_options_set.x_flag_unsafe_math_optimizations;
   deviceOpts->x_flag_finite_math_only = global_options.x_flag_finite_math_only;
   deviceOptsSet->x_flag_finite_math_only = global_options_set.x_flag_finite_math_only;
   deviceOpts->x_flag_trapping_math = global_options.x_flag_trapping_math;
   deviceOptsSet->x_flag_trapping_math = global_options_set.x_flag_trapping_math;
   deviceOpts->x_flag_errno_math = global_options.x_flag_errno_math;
   deviceOptsSet->x_flag_errno_math = global_options_set.x_flag_errno_math;
}

/**
 * 用mtcsoptions中的global_opts和global_opts_set设置主机中的global_opts global_opts_set
 */
void mtcs_options_override_host_options(MtcsOptions *self)
{
    self->override_host_options(self,&global_options,&global_options_set);
}

//原型 common_handle_option_auto options.h options.cc
bool mtcs_options_common_handle_option_auto (MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded,unsigned int lang_mask,
        int kind,location_t loc, const struct mtcs_cl_option_handlers *handlers, diagnostic_context *dc)
{
    return self->common_handle_option_auto(self,opts,opts_set,decoded,lang_mask,kind,loc,handlers,dc);
}

//把ptx-opitons.cc中的const struct cl_enum ptx_cl_enums设给clEnum;
void mtcs_options_set_cl_enum(MtcsOptions *self,struct cl_enum *clEnum)
{
    self->clEnums=clEnum;
}

//原型 cl_lang_count options.cc;
void mtcs_options_set_cl_lang_count(MtcsOptions *self,int clLangCount)
{
    self->clLangCount=clLangCount;
}

void mtcs_options_set_gcc_options_gcc_options_set(MtcsOptions *self, MtcsOptionsItem *gccOptions,MtcsOptionsItem *gccOptionsSet)
{
   self->global_options=gccOptions;
   self->global_options_set=gccOptionsSet;
}

MtcsOptionsItem *mtcs_options_create_item(MtcsOptions *self)
{
    return self->create_item(self);
}

void  mtcs_options_free_item(MtcsOptions *self,MtcsOptionsItem *item)
{
    self->free_item(self,item);
}

//在opts.cc default_options_table需要两个宏来创建 DELAY_SLOTS INSN_SCHEDULING 由子类调用该方法可以避免这个宏的使用
//在nvptx中 //#if DELAY_SLOTS定义，但值=0 INSN_SCHEDULING宏未定义
void mtcs_options_set_default_options_table(MtcsOptions *self,struct default_options *defaultTable)
{
   self->default_options_table=defaultTable;
}

struct default_options * mtcs_options_get_default_options_table(MtcsOptions *self)
{
   return self->default_options_table;
}
