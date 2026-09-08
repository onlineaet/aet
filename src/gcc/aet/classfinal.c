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
#include "attribs.h"
#include "tree-core.h"

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classfinal.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "classfinal.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "varmgr.h"
#include "genericutil.h"
#include "classutil.h"
#include "classparser.h"
#include "classimpl.h"

/**
 * 对于变量采用 确定赋值一次（definite assignment）+ 初始化后不可重新赋值”的语义
 * 只能在构造函数中初始化，如果在构造函数中有控制流，需要在分支判断，如果分支中没有，也要报错。
 */
static void classFinalFinal(ClassFinal *self)
{
}

void class_final_parser(ClassFinal *self,ClassParserState state,struct c_declspecs *specs)
{
   c_parser *parser=self->parser->parser;
   enum rid keyword=c_parser_peek_token (parser)->keyword;
   location_t  loc = c_parser_peek_token (parser)->location;
   c_parser_consume_token (parser); //
   if(state!=CLASS_STATE_STOP && state!=CLASS_STATE_FIELD){
      if(state!=CLASS_STATE_FIELD)
         error_at(loc,"访问final$关键字只能出现在类的第一个字符！");
      else
         error_at(loc,"访问final$关键字只能出现在方法或变量的第一个字符！");
      return;
      }
   if(state==CLASS_STATE_FIELD){
      if(specs!=NULL && (specs->typespec_word!=cts_none
            || specs->storage_class!=csc_none || specs->typespec_kind != ctsk_none)){
         error_at(loc,"访问final$关键字只能出现在方法或变量的第一个字符!!！");
         return;
      }
   }
}

/**
 * 加入final$变量到arrays
 * lhs是component_ref
 * c_parser_expr_no_commas中调用 生成 modify_expr前
 */
void class_final_check_modify(ClassFinal *self,location_t loc,tree lhs,tree rhs)
{
   if(TREE_CODE(lhs)!=COMPONENT_REF)
      return;
   tree ref=TREE_OPERAND(lhs,0);
   tree field=TREE_OPERAND(lhs,1);
   char *sysName = class_util_get_class_name(TREE_TYPE(ref));
   if(!sysName)
      return;
   const char *name = IDENTIFIER_POINTER(DECL_NAME(field));
   //是变量还是函数
   ClassFunc  *func = func_mgr_get_entity_by_sys_name(func_mgr_get(),sysName,name);
   if(func)
      return ;
   func = func_mgr_get_static_method(func_mgr_get(),sysName,name);
   if(func)
      return;
   VarEntity *entity = var_mgr_get_var(var_mgr_get(),sysName,name);
   if(!entity || !entity->isFinal)
      return;
   if(!current_function_decl || !self->parser->isAet)
      error_at(loc,"final$ 变量%qs只能在构造函数内初始化。",name);
   ClassFunc    *ctorfunc = func_mgr_get_entity_by_sys_name(func_mgr_get(),
         sysName,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
   if(!ctorfunc || !ctorfunc->isCtor)
      error_at(loc,"final$ 变量%qs只能在构造函数内初始化。00",name);

}

typedef struct _WalkData{
   NPtrArray *ctors;
   VarEntity  *var;
   location_t loc;
   int count;//在构造函数中赋值次数
   ClassFunc *selfCtor;//在构造函数中调用self(...);
}WalkData;

static ClassFunc *getCtors(NPtrArray *ctors,tree field)
{
   int i;
   for(i=0;i<ctors->len;i++){
      ClassFunc *item = n_ptr_array_index(ctors,i);
      if(item->fieldDecl==field)
         return item;
   }
   return NULL;
}

static tree findModifyExpr_cb (tree *tp, int *walk_subtrees, void *data)
{
   WalkData *dp = (WalkData *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   //else if (TREE_CODE (t) == BIND_EXPR){
    //  walk_tree (&BIND_EXPR_BODY (t), findModifyExpr_cb, data, NULL);
   else if(TREE_CODE(t)==MODIFY_EXPR){
      tree lhs = TREE_OPERAND (t, 0);

      if(TREE_CODE(lhs)==COMPONENT_REF && TREE_OPERAND (lhs, 1)==dp->var->decl){
         //找到对final$的赋值
         dp->loc = EXPR_LOCATION(t);
         dp->count++;
      }
   }else if(TREE_CODE(t)==CALL_EXPR){
      //printf("这是一个函数调用 检查是否调用其它的构造函数\n");
      tree compref=CALL_EXPR_FN(t);
      if(TREE_CODE(compref)==COMPONENT_REF){
         tree field = TREE_OPERAND (compref, 1);
         //找一下
         //printf("这是一个函数调用 检查是否调用其它的构造函数 11\n");
         ClassFunc *func = getCtors(dp->ctors,field);
         if(func){
            gcc_assert(func);
            dp->selfCtor = func;
         }
      }
   }
   return NULL_TREE;
}

typedef struct _CheckData
{
   location_t loc;
   ClassFunc *ctor;
   ClassFunc *selfCtor;
}CheckData;

static void check(NPtrArray *ctors,
      VarEntity *var,ClassFunc *ctor,ClassInfo *info,NPtrArray *ok,NPtrArray *fail)
{
   WalkData data={ctors,var,0,0,NULL};
   walk_tree (&DECL_SAVED_TREE(ctor->fromImplDefine), findModifyExpr_cb, &data, NULL);
   if(data.count>1){
      error_at(data.loc,"final$变量%qs在构造函数中被多次赋值。",var->orgiName);
   }else if(data.count==0){
      if(!data.selfCtor){
         int count = class_func_get_param_count(ctor);
         if(count==1){ //缺省的构造函数
            ClassParser *p=class_parser_get();
            nboolean decl = class_ctor_is_artificial(class_parser_get()->classCtor,&info->className);
            nboolean define = class_ctor_is_artificial(class_impl_get()->classCtor,&info->className);
            printf("check 22 参数：%d 缺省构造函数是不是人工的:decl:%d define:%d \n",count,decl,define);
            if(!define && !class_func_is_private(ctor)){
               //声明和定义都是人工加的，报错位置放什么地方
               error_at(DECL_SOURCE_LOCATION(ctor->fromImplDefine),
               "final$ 变量%qs在缺省的构造函数中没有初始化。\n",var->orgiName);
            }else{
               warning_at(info->implLoc,0,"人工缺省的构造函数并不能初始化 final$ 变量%qs。",var->orgiName);
            }
         }else{
            //是不是缺省的构造函数，缺省的构造函数中有一个参数
            printf("check 33  %p %p %s\n",ctor->fromImplDefine,ctor->fieldDecl,ctor->mangleFunName);
            location_t loc = ctor->fromImplDefine?DECL_SOURCE_LOCATION(ctor->fromImplDefine):
                  DECL_SOURCE_LOCATION(ctor->fieldDecl);
            error_at(loc,"final$ 变量%qs在构造函数中没有初始化。\n",var->orgiName);
         }
      }else{
        //没有初始化，但调用了self
         CheckData *d = n_slice_new(CheckData);
         d->ctor = ctor;
         d->selfCtor = data.selfCtor;
         n_ptr_array_add(fail,d);
      }
   }else{
      //成功了
      CheckData *d = n_slice_new(CheckData);
      d->loc = data.loc;
      d->ctor = ctor;
      d->selfCtor = data.selfCtor;
      n_ptr_array_add(ok,d);
   }
}

static void freeCheckData_cb(CheckData *data)
{
   n_slice_free(CheckData,data);
}

static void lastCheck(VarEntity *var,NPtrArray *ok,NPtrArray *fail)
{
   int i,j;
   for(i=0;i<fail->len;i++){
      CheckData *item = n_ptr_array_index(fail,i);
      ClassFunc *ctor = item->ctor;
      nboolean find = FALSE;
      for(j=0;j<ok->len;j++){
         CheckData *okItem = n_ptr_array_index(ok,j);
         if(item->selfCtor == okItem->ctor){
            find = TRUE;
            break;
         }
      }
      if(!find){
         location_t loc = ctor->fromImplDefine?DECL_SOURCE_LOCATION(ctor->fromImplDefine):
                  DECL_SOURCE_LOCATION(ctor->fieldDecl);
         error_at(loc,"final$ 变量%qs在构造函数中没有初始化。\n",var->orgiName);
         return;
      }
   }

   //成功初始化变量的构造函数如查调用了self(...)检查self是不是也初始化了变量，如果是，报重复初始化的错误
   for(i=0;i<fail->len;i++){
      CheckData *item = n_ptr_array_index(ok,i);
      if(item->selfCtor){
         for(j=0;j<ok->len;j++){
            CheckData *other = n_ptr_array_index(ok,j);
            if(i!=j && other->ctor!=item->selfCtor){
               error_at(item->loc,"final$变量%qs在其它构造函数也被赋值。",var->orgiName);
               return;
            }
         }
      }
   }
}

/**
 * 类实现结束后检查final$ 变量是否在构造函数初始化
 */
void class_final_check_var(ClassFinal *self,ClassName *className)
{
   if(className==NULL)
      return;
   NPtrArray *vars = var_mgr_get_vars(var_mgr_get(),className->sysName);
   NPtrArray *ctors = func_mgr_get_constructors(func_mgr_get(),className);
   ClassInfo *info = class_mgr_get_class_info_by_class_name(class_mgr_get(),className);

   NPtrArray *ok=n_ptr_array_new_with_free_func(freeCheckData_cb);
   NPtrArray *fail=n_ptr_array_new_with_free_func(freeCheckData_cb);

   int varCount = vars->len;
   int i,j;
   for(i=0;i<varCount;i++){
      VarEntity *item = n_ptr_array_index(vars,i);
      if(!(item->isFinal && !item->isStatic))
         continue;

      if(ctors->len == 0){
         error_at(DECL_SOURCE_LOCATION(item->decl),"类%qs中没有构造函数可以初始化final$变量%qs。",
               className->userName,item->orgiName);
         n_ptr_array_unref(ctors);
         return;
      }
      for(j=0;j<ctors->len;j++){
         ClassFunc *ctor = n_ptr_array_index(ctors,j);
         check(ctors,item,ctor,info,ok,fail);
      }
      lastCheck(item,ok,fail);
      n_ptr_array_remove_range(ok,0,ok->len);
      n_ptr_array_remove_range(fail,0,fail->len);

   }
   n_ptr_array_unref(ctors);
   n_ptr_array_unref(ok);
   n_ptr_array_unref(fail);
}

ClassFinal *class_final_get()
{
   static ClassFinal *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(ClassFinal));
      singleton->parser = aet_parser_get();
      classFinalFinal(singleton);
   }
   return singleton;
}
