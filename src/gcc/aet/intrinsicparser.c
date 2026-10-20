/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#include "config.h"
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "aet-c-parser-header.h"
#include "attribs.h"
#include "plugin.h"

#include "c-aet.h"
#include "classparser.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "aetinfo.h"
#include "varmgr.h"

#include "classutil.h"
#include "intrinsicparser.h"
#include "accesscontrols.h"
#include "classimpl.h"
#include "classparser.h"

/* Implement "#pragma mtcs oepration".  */
static void mtcs_intrinsic_pragma_cb (cpp_reader *);

static void intrinsicParserInit(IntrinsicParser *self)
{
   /*注册新的属性用于修饰class是一个target_intrinsic类*/
   //mii是mtcs_intrinsic_implements的缩写，表示mtcs内在实现
   //public$ final$ __attribute__((mii("ptx"))) class$ CudaMemInstrinsic
   //mtcs_intrinsic_class参数 1.目标(ptx,gcn...) 2.版本(sm_32,sm_75) 3.保留
   const struct attribute_spec *spx=lookup_attribute_spec (get_identifier(MTCS_INTRINSIC_CLASS));
   if(!spx){
      struct attribute_spec *atts =new attribute_spec {xstrdup(MTCS_INTRINSIC_CLASS),
      0, -2, false,  false, false, false, NULL, NULL };
      register_attribute (atts);
   }

   spx=lookup_attribute_spec (get_identifier(MTCS_INTRINSIC_IMPLEMENTS));
   if(!spx){
      struct attribute_spec *atts =new attribute_spec {xstrdup(MTCS_INTRINSIC_IMPLEMENTS),
      0, -2, false,  false, false, false, NULL, NULL };
      register_attribute (atts);
   }

   c_register_pragma ("mtcs", "intrinsic", mtcs_intrinsic_pragma_cb);
   self->mtcsIntrinsic.directives = NULL;
   self->mtcsIntrinsic.enter = FALSE;
}

/**
 * mic的attribute 有2个参数，如果没有就加入default
 */
static void targetIntrinsicFunc_cb(tree fndecl,void *userData)
{
   IntrinsicParser *self = (IntrinsicParser *)userData;
   if(!self->mtcsIntrinsic.enter)
      return ;
   ClassParser *classParser  = class_parser_get();
   //获取target,version ,diretcive
   ClassInfo *info = class_mgr_get_class_info_by_class_name(class_mgr_get(),classParser->currentClassName);
   gcc_assert(info && class_info_is_mic(info));
   tree intrinsicAtt=  class_info_get_mic_attribute(info);

   gcc_assert(intrinsicAtt);
   int device = mtcs_info_get_and_check_func_type(fndecl,DECL_SOURCE_LOCATION(fndecl));
   if(device!=MTCS_FUNC_DEVICE)
      return;
   //把__attribute__((mic())) 与 #pragma mtcs intrinsic
   //合并到 mtcs_intrinsic_class,然后加入到fndecl的属性中
   tree intrinsic = NULL_TREE;
   tree list = NULL_TREE;

   int ix=0;
   for (tree a = TREE_VALUE (intrinsicAtt); a; a = TREE_CHAIN (a)){
      tree d = TREE_VALUE (a);
      list=tree_cons (NULL_TREE, d, list);
      ix++;
   }
   if(ix==0){
      list=tree_cons (NULL_TREE, get_identifier("param1"), list);
      list=tree_cons (NULL_TREE, get_identifier("param2"), list);
   }else if(ix==1){
      list=tree_cons (NULL_TREE, get_identifier("param2"), list);
   }
   char *pragamName=self->mtcsIntrinsic.directives;
   if(!pragamName)
      pragamName = "default";
   list=tree_cons (NULL_TREE, get_identifier(pragamName), list);
   list = nreverse(list);
   intrinsic = build_tree_list (get_identifier(MTCS_INTRINSIC_FUNCTION_DECL),list);

   tree attrs = DECL_ATTRIBUTES (fndecl);
   if(!attrs)
      DECL_ATTRIBUTES (fndecl) = tree_cons (intrinsic,NULL_TREE,DECL_ATTRIBUTES (fndecl));
   else{
      attrs=chainon (attrs, intrinsic);
   }
   DECL_ATTRIBUTES (fndecl) = attrs;
   /*
   tree mtcs=lookup_attribute (MTCS_DEVICE_STR, DECL_ATTRIBUTES (fndecl));
   tree mtcs1=lookup_attribute (MTCS_INTRINSIC_FUNCTION_DECL, DECL_ATTRIBUTES (fndecl));
   printf("mic 的 func attributexxx 22\n");
   aet_print_tree(mtcs1);
   */
}

/**
 * 实现 #pragma mtcs operation
 */
static void mtcs_intrinsic_pragma_cb (cpp_reader *)
{
   tree x;
   location_t loc;
   pragma_lex (&x,&loc);
   const char *name =x?TREE_STRING_POINTER (x):NULL;
   //  if (strcmp (name, "arm_sve.h") == 0)
   //    aarch64_sve::handle_arm_sve_h (false);
   //  else
   //  error ("unknown %<#pragma mtcs operation%> option %qs", name);
   ClassParser *classParser  = class_parser_get();
   if(classParser->state!=CLASS_STATE_FIELD){
      error_at(loc,"#pragma mtcs intrinsic 只能用在类声明中。");
   }
   //检查所在类是不是一个普通的类
   ClassInfo *info= class_mgr_get_class_info_by_class_name(class_mgr_get(),classParser->currentClassName);
   if(!class_info_is_mic(info)){
      error_at(loc,"#pragma mtcs intrinsic 所在的类没有属性 'mic'。");
   }
   IntrinsicParser *self = classParser->intrinsicParser;
   self->mtcsIntrinsic.directives = name;
   self->mtcsIntrinsic.enter = TRUE;
   parser_static_set_func(parser_static_get(),targetIntrinsicFunc_cb,(void *)self);
}

void intrinsic_parser_class_end(IntrinsicParser *self)
{
   self->mtcsIntrinsic.directives = NULL;
   self->mtcsIntrinsic.enter = FALSE;
}


IntrinsicParser  *intrinsic_parser_new()
{
   IntrinsicParser *self =n_slice_alloc0 (sizeof(IntrinsicParser));
   intrinsicParserInit(self);
   return self;
}


