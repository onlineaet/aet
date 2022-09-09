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
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "c/c-parser.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"
#include "tree-iterator.h"
#include "sbitmap.h"
#include "c/c-lang.h"
#include "explow.h"
#include "rtl.h"



#include "aet-c-parser-header.h"
#include "classimpl.h"
#include "aetprinttree.h"
#include "funcmgr.h"
#include "aetutils.h"
#include "cmprefopt.h"

static void cmpRefOptInit(CmpRefOpt *self)
{
    self->hashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);
}

static nboolean isSelfCall(tree func,char **funcName)
{
     if(TREE_CODE(func)!=COMPONENT_REF){
        return FALSE;
     }
     tree selfTree=TREE_OPERAND (func, 0);
     if(TREE_CODE(selfTree)!=INDIRECT_REF){
          return FALSE;
     }
     tree selfParm=TREE_OPERAND (selfTree, 0);
     if(TREE_CODE(selfParm)==PARM_DECL){
         char *name=IDENTIFIER_POINTER(DECL_NAME(selfParm));
         if(!strcmp(name,"self")){
             tree fieldDecl=TREE_OPERAND (func, 1);
             char *fieldName=IDENTIFIER_POINTER(DECL_NAME(fieldDecl));
             *funcName=n_strdup(fieldName);
             return TRUE;
         }
     }
     return FALSE;

}

static  vec<tree, va_gc> *createParm(tree callExpr)
{
       vec<tree, va_gc> *parmVec;
       parmVec = make_tree_vector ();
       int i = 0;
       tree arg;
       call_expr_arg_iterator iter;
       FOR_EACH_CALL_EXPR_ARG (arg, iter, callExpr){
           vec_safe_push (parmVec, arg);
           i++;
       }
       return parmVec;
}

typedef struct _WalkData{
    char *sysName;
}WalkData;

/**
 * 把self->xxx() 替换成 yyy()
 */
static tree link_cb (tree *tp, int *walk_subtrees, void *data)
{
   WalkData *dp = (WalkData *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR){
      walk_tree (&BIND_EXPR_BODY (t), link_cb, data, NULL);
   }else if(TREE_CODE(t)==CALL_EXPR){
       tree func=CALL_EXPR_FN(t);
       char *mangleName=NULL;
       nboolean ret= isSelfCall(func,&mangleName);
       if(ret){
           ClassFunc *classFunc=func_mgr_get_entity_by_sys_name(func_mgr_get(),dp->sysName,mangleName);
           if(classFunc && aet_utils_valid_tree(classFunc->fromImplDefine)/* && strstr(mangleName,"getxxx")*/){
               n_debug("cmprefopt.c 可以替换了---vvvv- %s %p",mangleName,classFunc);
               vec<tree, va_gc> *parms=createParm(t);
               tree newCallExpr = c_build_function_call_vec (EXPR_LOCATION(t), vNULL, classFunc->fromImplDefine,parms, NULL);
               aet_print_tree(newCallExpr);
               *tp=newCallExpr;
               vec_free (parms);
           }
       }
    }
    return NULL_TREE;
}


/**
 * 链接到定义的函数中
 * 在impl$结尾处调用
 */
void  cmp_ref_opt_opt(CmpRefOpt *self)
{
    c_parser *parser=self->parser;
    ClassImpl *impl=class_impl_get();
    char *sysName=impl->className->sysName;
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
    int i;
    n_debug("cmp_ref_opt_opt start:%s\n",sysName);
    for(i=0;i<array->len;i++){
          tree func=n_ptr_array_index(array,i);
          WalkData data;
          data.sysName=sysName;
          walk_tree (&DECL_SAVED_TREE(func), link_cb, &data, NULL);
    }
    n_debug("cmp_ref_opt_opt end:%s\n",sysName);

}

void cmp_ref_opt_add(CmpRefOpt *self,tree func)
{
    c_parser *parser=self->parser;
    if(!parser->isAet)
        return NULL_TREE;
    ClassImpl *impl=class_impl_get();
    char *sysName=impl->className->sysName;
    if(!n_hash_table_contains(self->hashTable,sysName)){
        NPtrArray *array=n_ptr_array_new();
        n_ptr_array_add(array,func);
        n_hash_table_insert (self->hashTable, n_strdup(sysName),array);
    }else{
        NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
        n_ptr_array_add(array,func);
    }
}

void cmp_ref_opt_set_parser(CmpRefOpt *self,c_parser *parser)
{
    self->parser=parser;
}


CmpRefOpt *cmp_ref_opt_new()
{
    CmpRefOpt *self =n_slice_alloc0 (sizeof(CmpRefOpt));
    cmpRefOptInit(self);
    return self;
}



