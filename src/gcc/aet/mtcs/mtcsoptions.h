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
 */

#ifndef __GCC_MTCS_OPTIONS__
#define __GCC_MTCS_OPTIONS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsoptionsitem.h"
#include "mtcsmicro.h"

typedef struct _MtcsOptions MtcsOptions;

struct _MtcsOptions
{
    MtcsComponent parent;
    MtcsBackupRestore mtcsBackupRestore;
    MtcsOptionsItem *global_options;
    MtcsOptionsItem *global_options_set;

    //原型 initial_lang_mask opts-global.cc
    unsigned int initial_lang_mask;
    //原型 extern struct obstack opts_obstack; opts.h opts.cc
    struct obstack opts_obstack;
    struct cl_option  *clOptions;//原型 const struct cl_option cl_options[] ; options.cc
    int clOptionsCount;//原型 const unsigned int cl_options_count = N_OPTS; options.cc
    const struct cl_enum *clEnums; //原型 const struct cl_enum cl_enums[] =; options.cc
    //原型 cl_lang_count options.cc;
    int clLangCount;
    //#define TARGET_SOFT_STACK ((target_flags & MASK_SOFT_STACK) != 0) //来自平台options.h
    int (*target_soft_stack)(MtcsOptions *self);
    //原型  init_options_struct opts.h opts.cc
    void (*init_options_struct)(MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);
    //原型  void (*init_options_struct) (struct gcc_options *opts); langhooks.h  #define LANG_HOOKS_INIT_OPTIONS_STRUCT lto_init_options_struct
    void (*hooks_init_options_struct)(MtcsOptions *self);
    //原型  lang_hooks.init_options (save_decoded_options_count, save_decoded_options); #define LANG_HOOKS_INIT_OPTIONS        lhd_init_options
    void (*hooks_init_options)(MtcsOptions *self,unsigned int decoded_options_count,struct cl_decoded_option *decoded_options);
    //原型 lang_hooks.complain_wrong_lang_p (option) #define LANG_HOOKS_COMPLAIN_WRONG_LANG_P lto_complain_wrong_lang_p
    bool (*hooks_complain_wrong_lang_p)(MtcsOptions *self,const struct cl_option *option);
    //原型  lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option
    bool(*hooks_handle_option)(MtcsOptions *self,size_t scode, const char *arg,
               HOST_WIDE_INT value ATTRIBUTE_UNUSED, int kind ATTRIBUTE_UNUSED, location_t loc ATTRIBUTE_UNUSED,
               const struct mtcs_cl_option_handlers *handlers ATTRIBUTE_UNUSED);
    //设备的opt_code转主机的opt_code
    int (*optcode_device_to_host)(MtcsOptions *self,int deviceOptcode);
    //主机的opt_code转设备的opt_code
    int (*optcode_host_to_device)(MtcsOptions *self,int hostOptcode);
    //原型 cl_optimization_save optons.h options-save.cc
    void (*cl_optimization_save)(MtcsOptions *self,struct cl_optimization *ptr, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);
    //原型 cl_optimization_restore options.h options-save.cc
    void (*cl_optimization_restore)(MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr);
    //原型 cl_target_option_hash options.h
    hashval_t (*cl_target_option_hash)(MtcsOptions *self,MtcsClTargetOption const *ptr ATTRIBUTE_UNUSED);
    //原型 bool cl_target_option_eq (struct cl_target_option const *ptr1 ATTRIBUTE_UNUSED, struct cl_target_option const *ptr2 ATTRIBUTE_UNUSED)
    bool (*cl_target_option_eq)(MtcsOptions *self,MtcsClTargetOption const *ptr1 ATTRIBUTE_UNUSED,
            MtcsClTargetOption const *ptr2 ATTRIBUTE_UNUSED);
    //用mtcsoptions中的global_opts和global_opts_set设置主机中的global_opts global_opts_set
    void (*override_host_options)(MtcsOptions *self,struct gcc_options *hostOpts,struct gcc_options *hostOptsSet);
    //原型 common_handle_option_auto options.h options.cc
    bool (*common_handle_option_auto) (MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
            const struct cl_decoded_option *decoded,unsigned int lang_mask,
            int kind,location_t loc, const struct mtcs_cl_option_handlers *handlers, diagnostic_context *dc);
    //创建MtcsOptionsItem 每个平台的mtcsOptonsItem都是MtcsOptionsItem的子类
    MtcsOptionsItem *(*create_item)(MtcsOptions *self);
    void   (*free_item)(MtcsOptions *self,MtcsOptionsItem *item);
    //原型 default_options_table opts.cc
    struct default_options *default_options_table;

    void *backup;

};

void  mtcs_options_init(MtcsOptions *self);
int   mtcs_options_target_soft_stack(MtcsOptions *self);
//原型 extern void init_options_once (void); opts.h opts-global.cc
void  mtcs_options_init_once(MtcsOptions *self);
//原型 extern void init_opts_obstack (void); opts.h opts.cc
void  mtcs_options_init_opts_obstack(MtcsOptions *self);
//原型  init_options_struct opts.h opts.cc
void  mtcs_options_init_options_struct (MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);
//原型  void (*init_options_struct) (struct gcc_options *opts); langhooks.h  #define LANG_HOOKS_INIT_OPTIONS_STRUCT lto_init_options_struct
void mtcs_options_hooks_init_options_struct(MtcsOptions *self);
//把ptx-options.cc中的 cl_options设给mtcsoptions.h中声明的变量clOptions;
void mtcs_options_set_cl_options(MtcsOptions *self,struct cl_option  *clOptions,int count);
//把ptx-opitons.cc中的const struct cl_enum ptx_cl_enums设给clEnum;
void mtcs_options_set_cl_enum(MtcsOptions *self,struct cl_enum *clOptions);
//原型 cl_lang_count options.cc;
void mtcs_options_set_cl_lang_count(MtcsOptions *self,int clLangCount);
//原型  lang_hooks.init_options (save_decoded_options_count, save_decoded_options); #define LANG_HOOKS_INIT_OPTIONS        lhd_init_options
void mtcs_options_hooks_init_options(MtcsOptions *self,unsigned int decoded_options_count,struct cl_decoded_option *decoded_options);
//原型 lang_hooks.complain_wrong_lang_p (option) #define LANG_HOOKS_COMPLAIN_WRONG_LANG_P lto_complain_wrong_lang_p
bool mtcs_options_hooks_complain_wrong_lang_p(MtcsOptions *self,const struct cl_option *option);
//原型  lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option
bool mtcs_options_hooks_handle_option(MtcsOptions *self,size_t scode, const char *arg,
           HOST_WIDE_INT value ATTRIBUTE_UNUSED, int kind ATTRIBUTE_UNUSED, location_t loc ATTRIBUTE_UNUSED,
           const struct mtcs_cl_option_handlers *handlers ATTRIBUTE_UNUSED);

void mtcs_options_set_gcc_options_gcc_options_set(MtcsOptions *self, MtcsOptionsItem *gccOptions,MtcsOptionsItem *gccOptionsSet);
//设备的opt_code转在主机的opt_code
int mtcs_options_optcode_device_to_host(MtcsOptions *self,int deviceOptcode);
//主机的opt_code转设备的opt_code
int mtcs_options_optcode_host_to_device(MtcsOptions *self,int hostOptcode);
//原型 lang_hooks.post_options (&main_input_filename); #define LANG_HOOKS_POST_OPTIONS        lhd_post_options
bool mtcs_options_post_options(MtcsOptions *self,char *fileName);
//原型 cl_optimization_save optons.h   options-save.cc
void mtcs_options_cl_optimization_save(MtcsOptions *self,struct cl_optimization *ptr, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);
//原型 cl_optimization_restore options.h options-save.cc
void mtcs_options_cl_optimization_restore(MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr);
//原型 cl_target_option_hash options.h
hashval_t mtcs_options_cl_target_option_hash(MtcsOptions *self,MtcsClTargetOption const *ptr ATTRIBUTE_UNUSED);
//原型 bool cl_target_option_eq (struct cl_target_option const *ptr1 ATTRIBUTE_UNUSED, struct cl_target_option const *ptr2 ATTRIBUTE_UNUSED)
bool  mtcs_options_cl_target_option_eq(MtcsOptions *self,MtcsClTargetOption const *ptr1 ATTRIBUTE_UNUSED,
        MtcsClTargetOption const *ptr2 ATTRIBUTE_UNUSED);

/**
 * 用mtcsoptions中的global_opts和global_opts_set设置主机中的global_opts global_opts_set
 */
void mtcs_options_override_host_options(MtcsOptions *self);


//原型 common_handle_option_auto options.h options.cc
bool mtcs_options_common_handle_option_auto (MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded,unsigned int lang_mask,
        int kind,location_t loc, const struct mtcs_cl_option_handlers *handlers, diagnostic_context *dc);

MtcsOptionsItem *mtcs_options_create_item(MtcsOptions *self);
void             mtcs_options_free_item(MtcsOptions *self,MtcsOptionsItem *item);

//原型 default_options_table opts.cc
//在opts.cc default_options_table需要两个宏来创建 DELAY_SLOTS INSN_SCHEDULING 由子类调用该方法可以避免这个宏的使用
//在nvptx中 //#if DELAY_SLOTS定义，但值=0 INSN_SCHEDULING宏未定义
void mtcs_options_set_default_options_table(MtcsOptions *self,struct default_options *defaultTable);
struct default_options * mtcs_options_get_default_options_table(MtcsOptions *self);
void mtcs_options_set_options_from_host(MtcsOptions *self);

#endif

