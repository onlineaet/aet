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

#ifndef __GCC_MTCS_OPTS__
#define __GCC_MTCS_OPTS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "mtcsoptionsitem.h"


struct mtcs_cl_option_hasher : ggc_cache_ptr_hash<tree_node>
{
  static hashval_t hash (tree t);
  static bool equal (tree x, tree y);
};


typedef struct _MtcsOpts MtcsOpts;

struct _MtcsOpts
{
    MtcsComponent parent;
    //原型 toplev.h 与选项有关
    struct cl_decoded_option *save_decoded_options;
    unsigned int save_decoded_options_count;
    vec<cl_decoded_option> *save_opt_decoded_options;
    //原型 opts-global.cc
    vec<const char *> ignored_options;
    //原型 flag_canon_prefix_map file-prefix-map.h opts.cc
    bool flag_canon_prefix_map;

    FILE *aux_info_file;
    //原型 static GTY (()) tree cl_optimization_node;    tree.cc
    tree cl_optimization_node;
    //原型  static GTY (()) tree cl_target_option_node;   tree.cc
    tree cl_target_option_node;
    //原型 cl_option_hash_table tree.cc
    hash_table<mtcs_cl_option_hasher> *cl_option_hash_table;
    /* Hold command-line options associated with stack limitation.  */
    //原型 opts.h opt_fstack_limit_symbol_arg opts-global
    const char *opt_fstack_limit_symbol_arg;
    //原型 opts.h opt_fstack_limit_register_no opts-global.cc 缺省值 =-1
    int opt_fstack_limit_register_no;
    //原型 opts.h flag_stack_protector_set_by_fhardened_p  opts.cc
    bool flag_stack_protector_set_by_fhardened_p;


};

struct mtcs_cl_option_handlers ;

/* Structure describing a single option-handling callback.  */
struct mtcs_cl_option_handler_func
{
  /* The function called to handle the option.  */
  bool (*handler) (MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
           const struct cl_decoded_option *decoded,
           unsigned int lang_mask, int kind, location_t loc,
           const struct mtcs_cl_option_handlers *handlers,diagnostic_context *dc,
           void (*target_option_override_hook) (void *userData),void *userData);
  /* The mask that must have some bit in common with the flags for the
     option for this particular handler to be used.  */
  unsigned int mask;
};

/* Structure describing the callbacks used in handling options.  */
struct mtcs_cl_option_handlers
{
  /* Callback for an unknown option to determine whether to give an
     error for it, and possibly store information to diagnose the
     option at a later point.  Return true if an error should be
     given, false otherwise.  */
  bool (*unknown_option_callback) (const struct cl_decoded_option *decoded,void *userData);
  /* Callback to handle, and possibly diagnose, an option for another
     language.  */
  void (*wrong_lang_callback) (const struct cl_decoded_option *decoded,unsigned int lang_mask,void *userData);
  /* Target option override hook.  */
  void (*target_option_override_hook) (void *userData);
  /* The number of individual handlers.  */
  size_t num_handlers;
  void *userData;
  /* The handlers themselves.  */
  struct mtcs_cl_option_handler_func handlers[3];


};


MtcsOpts *mtcs_opts_new(MtcsMode *mtcsMode);
/* Save Optimization decoded options.  */
void mtcs_opt_save_optimization_decoded_options(MtcsOpts *self);
//原型 decode_options opts.h opts-global.cc
void mtcs_opts_decode_options (MtcsOpts *self,location_t loc, diagnostic_context *dc);
//原型 generate_option opts.h opts-common.cc
//opt_index 一定是设备的opt_code 所有函数中声明的 opt_code都默认是device opt_code
void mtcs_opts_generate_option (MtcsOpts *self,size_t opt_index, const char *arg, HOST_WIDE_INT value,
         unsigned int lang_mask, struct cl_decoded_option *decoded);
//opt_index 一定是设备的opt_code 所有函数中声明的 opt_code都默认是device opt_code
bool mtcs_opts_handle_generated_option (MtcsOpts *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
             size_t opt_index, const char *arg, HOST_WIDE_INT value, unsigned int lang_mask, int kind, location_t loc,
             const struct mtcs_cl_option_handlers *handlers,bool generated_p, diagnostic_context *dc);
//原型 option_flag_var opts.h opts-common.cc
void *mtcs_opts_option_flag_var (MtcsOpts *self,int opt_index, MtcsOptionsItem/*!struct gcc_options*/ *opts);

//原型 set_option opts.h opts-common.cc
void mtcs_opts_set_option (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,
        int opt_index, HOST_WIDE_INT value, const char *arg, int kind,
        location_t loc, diagnostic_context *dc, HOST_WIDE_INT mask=0 /* = 0 */);
//原型 read_cmdline_option opts.h opts-common.cc
void mtcs_opts_read_cmdline_option (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,struct cl_decoded_option *decoded,
             location_t loc,unsigned int lang_mask,const struct mtcs_cl_option_handlers *handlers,diagnostic_context *dc);

//原型 finish_options opts.h opts.cc
void mtcs_opts_finish_options (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,location_t loc);
//原型 diagnose_options opts.h opts.cc
void mtcs_opts_diagnose_options (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,location_t loc);
//原型 dwarf_debuginfo_p flags.h opts.cc
bool mtcs_opts_dwarf_debuginfo_p (MtcsOpts *self,MtcsOptionsItem *opts);
//原型 handle_common_deferred_options opts.h opts-global.cc
void mtcs_opts_handle_common_deferred_options (MtcsOpts *self);
//原型 gen_command_line_string opts.h opts.cc
char *mtcs_opts_gen_command_line_string (MtcsOpts *self,cl_decoded_option *options, unsigned int options_count);
//原型 process_options toplev.cc
void mtcs_opts_process_options(MtcsOpts *self,bool no_backend);
//原型 build_optimization_node tree.h tree.cc
tree mtcs_opts_build_optimization_node (MtcsOpts *self,MtcsOptionsItem *opts, MtcsOptionsItem *opts_set);

//原型 common_handle_option opts.h opts.cc
bool mtcs_opts_common_handle_option (MtcsOpts *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
              const struct cl_decoded_option *decoded,
              unsigned int lang_mask, int kind ATTRIBUTE_UNUSED,
              location_t loc,const struct mtcs_cl_option_handlers *handlers,
              diagnostic_context *dc,void (*target_option_override_hook) (void *userData),void *userData);

//原型 find_opt opts.h opts-common.cc
size_t mtcs_opts_find_opt(MtcsOpts *self,const char *input, unsigned int lang_mask);

//原型 control_warning_option opts.h opts-common.cc
void mtcs_opts_control_warning_option (MtcsOpts *self,unsigned int opt_index, int kind, const char *arg,
            bool imply, location_t loc, unsigned int lang_mask,const struct mtcs_cl_option_handlers *handlers,
            MtcsOptionsItem*opts, MtcsOptionsItem *opts_set,diagnostic_context *dc);

//原型 decode_cmdline_options_to_array opts.h opts-common.cc
void mtcs_opts_decode_cmdline_options_to_array (MtcsOpts *self,unsigned int argc, const char **argv,
                 unsigned int lang_mask, struct cl_decoded_option **decoded_options, unsigned int *decoded_options_count);

//原型 decode_cmdline_options_to_array opts.h opts-common.cc
void mtcs_opts_decode_cmdline_options_to_array (MtcsOpts *self,unsigned int argc, const char **argv,
                 unsigned int lang_mask, struct cl_decoded_option **decoded_options, unsigned int *decoded_options_count);


#endif
