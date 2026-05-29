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
#ifndef __GCC_MTCS_GEN__
#define __GCC_MTCS_GEN__


#include "aet/nlib.h"
typedef struct _MtcsGen MtcsGen;
struct _MtcsGen{
   char *platformName;//ptx gcn spirv等等
   char *platformNameUppercase;//PTX GCN,SPIRV等等
   char *platformNameObject;//ptx Gcn Spirv等等
   char *saveRootPath;//生成文件的保存路径，由平台名+gen组成完整路径

   int maxMachineMode;
   char **modeNames;
   unsigned char *modeClass;
   unsigned short *modeInner;
   poly_uint16 *modeNunits;
   bool isCFile;//输出c文件
   char *cFile;
   char *hFile;
};

//原型 init_predicate_table (void) gensupport.cc
MtcsGen *mtcs_gen_get();
void mtcs_gen_append_preds(MtcsGen *self);
/* Return true if NAME is the name of an optab, describing it in P if so.  */
//optabs gensupport.h gensupport.cc
bool        mtcs_gen_find_optab (MtcsGen *self,void /*!optab_pattern*/ *op, const char *name);
const char *mtcs_gen_get_platform_name(MtcsGen *self);
const char *mtcs_gen_get_platform_upper_name(MtcsGen *self);
const char *mtcs_gen_get_platform_object_name(MtcsGen *self);
const char *mtcs_gen_get_save_root_path(MtcsGen *self);

int         mtcs_gen_hostmode_to_devicemode(MtcsGen *self,int hostMode);
bool        mtcs_gen_is_ptx(MtcsGen *self);
bool        mtcs_gen_is_gcn(MtcsGen *self);
bool        mtcs_gen_is_spirv(MtcsGen *self);
char       *mtcs_gen_create_target_code(MtcsGen *self,char *object);
char       *mtcs_gen_get_mode_name(MtcsGen *self,int mode);
bool        mtcs_gen_is_vector_p(MtcsGen *self,int mode);
int         mtcs_gen_get_inner(MtcsGen *self,int mode);
poly_uint16 mtcs_gen_get_nunits (MtcsGen *self,int  mode);
//原型 GET_MODE_CLASS
unsigned char  mtcs_gen_get_class(MtcsGen *self,int mode);
/**
 * 从 E_SImode 转成 PTX_SImode
 */
int         mtcs_gen_convert_host_mode_name(MtcsGen *self,char *hostModeName,char *deviceModeName);
bool        mtcs_gen_handle_arg (const char *arg);        //需要重设machine_mode
bool        mtcs_gen_handle_arg_no_mode (const char *arg);//不重设machine_mode
char       *mtcs_gen_get_c_file(MtcsGen *self);//输出c文件
char       *mtcs_gen_get_h_file(MtcsGen *self);//输出h文件

//在文件写完后 替换 ptx-insn-emit.c 中的mode
int mtcs_gen_replace_mode_for_emit(MtcsGen *self,NString *fd);
int mtcs_gen_replace_mode_for_flags_file(MtcsGen *self,FILE *fd);
/**
 * 替换xxx-insn-recog.c中的mode TARGET_SOFT_STACK
 */
int mtcs_gen_replace_recog(MtcsGen *self,NString *recogContent);
//代替 rtx_reader_ptr->print_c_condition (f, exp);
void mtcs_gen_print_c_condition(MtcsGen *self,NString *strs,char *exp);
nboolean mtcs_gen_is_reserve_preds(MtcsGen *self,char *predsName);
void mtcs_gen_replace_plat_preds(MtcsGen *self,NString *src);
void mtcs_gen_replace_common_preds(MtcsGen *self,NString *src);
unsigned int mtcs_gen_get_max_machine_mode(MtcsGen *self);


#endif
