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
#include "plugin.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aet-c-parser-header.h"
#include "mtcsbuiltintree.h"
#include "aetprinttree.h"
#include "mtcsinfo.h"

static tree  createDimFunc();
static char *getFuncName(char *src);


//初始化内置变量
static void mtcsBuilinTreeInit(MtcsBuiltinTree *self)
{
   self->parser = aet_parser_get();
   self->builtinVars[0].name="threadIdx";
   self->builtinVars[0].elements=3;
   self->builtinVars[0].region=THREAD_IDX;

   self->builtinVars[1].name="unitIdx";
   self->builtinVars[1].elements=3;
   self->builtinVars[1].region=UNIT_IDX;

   self->builtinVars[2].name="blockIdx"; //等同于 unitIdx
   self->builtinVars[2].elements=3;
   self->builtinVars[2].region=UNIT_IDX;

   self->builtinVars[3].name="unitDim";
   self->builtinVars[3].elements=3;
   self->builtinVars[3].region=UNIT_DIM;

   self->builtinVars[4].name="blockDim";//等同于 unitDim
   self->builtinVars[4].elements=3;
   self->builtinVars[4].region=UNIT_DIM;

   self->builtinVars[5].name="matDim";
   self->builtinVars[5].elements=3;
   self->builtinVars[5].region=MAT_DIM;

   self->builtinVars[6].name="gridDim"; //等同于 matDim
   self->builtinVars[6].elements=3;
   self->builtinVars[6].region=MAT_DIM;

   self->createBuiltinFuncs = FALSE;
   self->createInternalFuncs=FALSE;
}

typedef struct _InternalFuncData
{
   int code;
   char *func;
}InternalFuncData;

/**
 * 在函数内声明共享变量 __shared__ int xxxx;
 *  解析时创建内建函数 __internal_mtcs_shared_var 的调用该调用生成的汇编是：
 *   .shared .align 4 .u32 _ZZ9helloCUDAPfE6texvar_0
 *
 */
static InternalFuncData internalCollects[]={
      {INTERNEL_CODE_DIM,  "static uint __internal_mtcs_level_dim (int region,int dim);"},
      {INTERNAL_CODE_SHARED_VAR,"static void __internal_mtcs_shared_var (int id,int regserver);"},
};

//创建 __internal_mtcs_level_dim
static tree createDimFunc()
{
   InternalFuncData data= internalCollects[0];
   tree param_type_list = NULL;
   //第一个参数 类型是 int
   tree firstParam=integer_type_node;
   param_type_list = tree_cons (NULL_TREE, firstParam, param_type_list);
   //加入第二个参数 void objectFuncGenParmInfo
   tree secondParam=integer_type_node;
   param_type_list = tree_cons (NULL_TREE, secondParam, param_type_list);

   param_type_list = nreverse (param_type_list);
   tree rtntype=unsigned_type_node;
   tree fntype = build_function_type (rtntype, param_type_list);
   char *funName = getFuncName(data.func);
   tree fndecl = build_decl (UNKNOWN_LOCATION, FUNCTION_DECL, get_identifier (funName), fntype);

   TREE_STATIC (fndecl) = 1;
   DECL_ARTIFICIAL (fndecl) = 1;
   TREE_PUBLIC (fndecl) = 0;
   DECL_EXTERNAL (fndecl) = 0;
   TREE_READONLY (fndecl) = 1;

    //如果用下面的属性，报错 undefined reference to `__internal_mtcs_level_dim' bug 060
//   DECL_CONTEXT (fndecl) = NULL_TREE;   // global
//   TREE_PUBLIC (fndecl) = 1;
//   TREE_STATIC (fndecl) = 1;
//   DECL_EXTERNAL (fndecl) = 1;
//   TREE_USED (fndecl) = 1;
//   DECL_ARTIFICIAL (fndecl) = 1;
//   DECL_IGNORED_P (fndecl) = 1;

   tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
   fn.built_in_class = NOT_BUILT_IN;
   fn.function_code = data.code;
   return fndecl;
}

//为 int shared var 插入一条调用语句。该调用生成汇编代码 .shared .align 4 .u32 var.0;
//id 每个id唯一的对应声明共享变量
tree mtcs_builtin_tree_create_shared_fndecl(MtcsBuiltinTree *self)
{
   InternalFuncData data= internalCollects[INTERNAL_CODE_SHARED_VAR-MTCS_INTERNAL_FN_CODE_START];
   tree param_type_list = NULL;
   //第一个参数 类型是 int
   tree firstParam=integer_type_node;
   param_type_list = tree_cons (NULL_TREE, firstParam, param_type_list);
   //加入第二个参数 void objectFuncGenParmInfo
   tree secondParam=integer_type_node;
   param_type_list = tree_cons (NULL_TREE, secondParam, param_type_list);

   param_type_list = nreverse (param_type_list);
   tree rtntype=void_type_node;
   tree fntype = build_function_type (rtntype, param_type_list);
   char *funName = getFuncName(data.func);
   tree fndecl = build_decl (UNKNOWN_LOCATION, FUNCTION_DECL, get_identifier (funName), fntype);
   //加 TREE_STATIC (fndecl) = 1; DECL_ARTIFICIAL (fndecl) = 1; TREE_READONLY (fndecl) = 1;都会令函数被优化掉。
   TREE_PUBLIC (fndecl) = 1;
   DECL_EXTERNAL (fndecl) = 1;
   DECL_ASSEMBLER_NAME (fndecl);//重要，生成 mgnl名字。

   tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
   fn.built_in_class = NOT_BUILT_IN;
   fn.function_code = data.code;
   return fndecl;
}

//为以下内置变量创建对应的函数
//threadIdx
//threadIdx.x
//threadIdx.y;
//threadIdx.z;
//blockDim;
//blockDim.x;
//blockDim.y;
//blockDim.z;
//blockIdx;
//blockIdx.x;
//blockIdx.y;
//blockIdx.z
//gridDim;
//gridDim.x;
//gridDim.y;
//gridDim.z;
//创建内置函数 threadIdx.x 等同于 uint a= __builtin_mtcs_level_dim(region,0);
//region = threadIdx dim = x=0 或 y=1 或 z=2;
static tree createDimCall(MtcsBuiltinTree *self,location_t loc,int region,int dim)
{
   vec<tree, va_gc> *arg_vec;
   vec_alloc (arg_vec, 2);
   tree tr = build_int_cst (integer_type_node, region);
   arg_vec->quick_push (tr);
   tree te = build_int_cst (integer_type_node, dim);
   arg_vec->quick_push (te);
  // build_call_expr_loc_vec 等同于 build1 (ADDR_EXPR,build_call_vec
  // tree fn = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (fndecl)),fndecl);
  // tree call = build_call_vec (rtntype, fn, arg_vec);
   if(!self->createInternalFuncs){
      self->createInternalFuncs=TRUE;
      self->internalFunc[0]=createDimFunc();
   }
   tree call = build_call_expr_loc_vec (loc,self->internalFunc[0],arg_vec);
   return call;
}

//tree call = createCall(loc,self->builtinVars[i].region,pos);

//解析到 threadIdx 变成调用 uint3 a={__builtin_mtcs_level_dim(region,0),__builtin_mtcs_level_dim(region,1),__builtin_mtcs_level_dim(region,2)}
//region 代表 threadIdx,0 =x, 1 = y, 2=z
static tree getUint3Constructor(MtcsBuiltinTree *self,location_t loc,int region)
{
   tree ruint3=lookup_name (get_identifier ("uint3"));
   // ruint3 是类型声明 type_decl 它的类型才是record
   tree type=TREE_TYPE(ruint3);
   tree field;
   vec<constructor_elt, va_gc> *v;
   vec_alloc (v, 3);
   int i=0;
   for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
      tree  value = createDimCall(self,loc, region,i++);
      constructor_elt elt = {field, value};
      v->quick_push (elt);
   }
   tree init= build_constructor (type, v);
   return init;
}

/**
 * 在生成后缀表达式时，生成内置变量的声明
 * 声明在函数开头
 */
tree mtcs_builtin_tree_parser(MtcsBuiltinTree *self,location_t loc,tree id)
{
   c_parser *parser = self->parser->parser;
   if(!id)
      return NULL_TREE;
   if (c_parser_next_token_is (parser, CPP_OPEN_PAREN)) //是函数调用吗?
      return NULL_TREE;
   const char *name=IDENTIFIER_POINTER(id);
   int i;
   for(i=0;i<MTCS_BUILTIN_VAR_COUNT;i++){
      if(self->builtinVars[i].name!=NULL && strcmp(name,self->builtinVars[i].name)==0){
         if (c_parser_next_token_is (parser, CPP_DOT)){
            c_token *secondToken =  c_parser_peek_2nd_token(parser);
            if(secondToken->type==CPP_NAME){
               tree dim = secondToken->value;
               char *str=IDENTIFIER_POINTER(dim);
               if(!strcmp(str,"x") || !strcmp(str,"y") || !strcmp(str,"z")){
                  int pos=0;
                  if(!strcmp(str,"y"))
                     pos=1;
                  if(!strcmp(str,"z"))
                     pos=2;
                  c_parser_consume_token (parser); //consume dot
                  c_parser_consume_token (parser); //consume  x or y or z
                  //解析 threadIdx.x;
                  tree call = createDimCall(self,loc,self->builtinVars[i].region,pos);
                  n_debug("mtcsbuiltintree.c 解析 name:%s pos:%s 内置变量转成函数调用了。region:%d pos(x,y,z):%d\n",
                        name,str,self->builtinVars[i].region,pos);
                  aet_print_tree(call);
                  return call;
               }
            }
         }
         return getUint3Constructor(self,loc,self->builtinVars[i].region);
      }
   }
   return NULL_TREE;
}

typedef struct _BuiltinFnData
{
   int fclass;//类别
   int code;
   char *func;
}BuiltinFnData;
//uint3 是在 mtcstypes的初始化方法创建的，比创建内置函数要早
static char *builtins[]={
      {"extern  void  __syncthreads ();"},
      {"extern  float __atomic_fetch_add_fs (float *p, float value,int flag);"},
      {"extern  int __shfl_xor_sync (int memberMask, int val,  int laneMask);"},
      {"extern  float __shfl_xor_sync_fs (int memberMask, float val,int laneMask);"},
 };

static char *getFuncName(char *src)
{
   NString *n=n_string_new(src);
   int pos = n_string_last_indexof(n,"(");
   NString *nn=n_string_substring_from(n,0,pos);
   n_string_trim(nn);
   pos = n_string_last_indexof(nn," ");
   NString *nnn=n_string_substring(nn,pos+1);
   n_string_free(n,TRUE);
   n_string_free(nn,TRUE);
   return n_string_free(nnn,FALSE);
}

/**
 * 生成内建函数声明
 */
char *mtcs_builtin_tree_create_builtins_decl(MtcsBuiltinTree *self)
{
   if(!self->createBuiltinFuncs){
      self->createBuiltinFuncs=TRUE;
      NString *buf=n_string_new("");
      int count=    ARRAY_SIZE (builtins);
      int i;
      for(i=0;i<count;i++){
         char *funName=builtins[i];
         n_string_append(buf,funName);
         n_string_append(buf,"\n");
      }
      return n_string_free(buf,FALSE);
   }
   return NULL;
}

/**
 * 为内建函数设置fn code
 */
void mtcs_builtin_tree_set_builtins_code(MtcsBuiltinTree *self)
{
   if(!self->createBuiltinFuncs)
      return;
   int count=    ARRAY_SIZE (builtins);
   int i;
   for(i=0;i<count;i++){
      char *funName=getFuncName(builtins[i]);
      tree decl = lookup_name (get_identifier (funName));
      if(!decl){
         n_error("没有内建函数:%s\n",funName);
      }
      free(funName);
      tree_function_decl &fndecl = FUNCTION_DECL_CHECK (decl)->function_decl;
      fndecl.built_in_class = NOT_BUILT_IN;
      fndecl.function_code = (MTCS_BUILTIN_FN_CODE_START+i);
   }
}


MtcsBuiltinTree *mtcs_builtin_tree_new()
{
   MtcsBuiltinTree *self =n_slice_alloc0 (sizeof(MtcsBuiltinTree));
   mtcsBuilinTreeInit(self);
   return self;
}

