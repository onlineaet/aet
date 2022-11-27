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
#include "attribs.h"
#include "toplev.h"
#include "varasm.h"
#include "c-family/c-common.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "aet-c-parser-header.h"
#include "classimpl.h"
#include "classmgr.h"
#include "implicitlycall.h"
#include "genericcheck.h"
#include "funcmgr.h"
#include "aet-typeck.h"
#include "genericcall.h"
#include "selectfield.h"

static int number=0;//创建的函数声明编号


typedef struct _ImplicitlyData {
    char *sysName;//所在的类
    location_t loc;
    char *origFuncName;//原始名字
    char *newFuncName; //新的名字  _implicitly_xxxx_yyy
    tree funcDecl;//函数声明 _implicitly_xxxx_yyy
    int number;   //序号
    vec<tree, va_gc> *exprList;
    vec<tree, va_gc> *origtypes;
    vec<location_t>   arg_loc;
    tree selfTree;//self参数
    tree call;//调用implicitly函数的函数。
    nboolean isStatic;//作为静态函数调用吗
}ImplicitlyData;


/**
 * 函数调用分两种情况
 * 在implimpl的没有加self->访问
 * 在implimpl加self->或其它对象的 如abc->
 */
static void implicitlyCallInit(ImplicitlyCall *self)
{
    self->implicitlyArray=n_ptr_array_new();
    self->funcHelp=func_help_new();
}

static tree createTempDeclare (location_t loc, tree functionid)
{
    tree decl = NULL_TREE;
    tree asmspec_tree;
    decl = build_decl (loc, FUNCTION_DECL, functionid, default_function_type);
    DECL_EXTERNAL (decl) = 1;
    TREE_PUBLIC (decl) = 1;
    C_DECL_IMPLICIT (decl) = 1;
    asmspec_tree = maybe_apply_renaming_pragma (decl, /*asmname=*/NULL);
    if (asmspec_tree)
       set_user_assembler_name (decl, TREE_STRING_POINTER (asmspec_tree));
    decl = pushdecl (decl);
    rest_of_decl_compilation (decl, 0, 0);
    gen_aux_info_record (decl, 0, 1, 0);
    decl_attributes (&decl, NULL_TREE, 0);
    return decl;
}

/**
 * 为什么不调用gcc的build_external_ref,因为会产生警告信息-Wimplicit-function-declaration
 */
static tree buildExternalRef (location_t loc, tree id)
{
    tree ref;
    ref = createTempDeclare (loc, id);
    if (TREE_TYPE (ref) == error_mark_node)
       return error_mark_node;
    if (ref != current_function_decl){
       TREE_USED (ref) = 1;
    }

    if (TREE_CODE (ref) == FUNCTION_DECL && !in_alignof){
       if (!in_sizeof && !in_typeof){
          C_DECL_USED (ref) = 1;
       }else if (DECL_INITIAL (ref) == NULL_TREE && DECL_EXTERNAL (ref) && !TREE_PUBLIC (ref)){
          aet_record_maybe_used_decl (ref);
       }
    }

    if (TREE_CODE (ref) == CONST_DECL){
        used_types_insert (TREE_TYPE (ref));
        if (warn_cxx_compat  && TREE_CODE (TREE_TYPE (ref)) == ENUMERAL_TYPE  && C_TYPE_DEFINED_IN_STRUCT (TREE_TYPE (ref))){
           inform (DECL_SOURCE_LOCATION (ref), "enum constant defined here");
        }
        ref = DECL_INITIAL (ref);
        TREE_CONSTANT (ref) = 1;
    }else if (current_function_decl != NULL_TREE && !DECL_FILE_SCOPE_P (current_function_decl)
       && (VAR_OR_FUNCTION_DECL_P (ref) || TREE_CODE (ref) == PARM_DECL)){
       tree context = decl_function_context (ref);
       if (context != NULL_TREE && context != current_function_decl)
          DECL_NONLOCAL (ref) = 1;
    }else if (current_function_decl != NULL_TREE
       && DECL_DECLARED_INLINE_P (current_function_decl)
       && DECL_EXTERNAL (current_function_decl)
       && VAR_OR_FUNCTION_DECL_P (ref)
       && (!VAR_P (ref) || TREE_STATIC (ref))
       && ! TREE_PUBLIC (ref)
       && DECL_CONTEXT (ref) != current_function_decl){
        record_inline_static (loc, current_function_decl, ref, csi_internal);
    }
     return ref;
}

static tree  createImplicitlyId(location_t loc,tree id,int *order)
{
    char newName[255];
    *order=number;
    sprintf(newName,"_implicitly_%s_%d",IDENTIFIER_POINTER(id),number);
    tree newId=aet_utils_create_ident(newName);
    tree decl=buildExternalRef(loc,newId);
    number++;
    return decl;
}

static void add(ImplicitlyCall *self,char *sysName,location_t loc,tree oldId,tree decl,int order,tree selfTree,nboolean isStatic)
{
    ImplicitlyData *item=(ImplicitlyData *)n_slice_new(ImplicitlyData);
    item->sysName=n_strdup(sysName);
    item->loc=loc;
    item->origFuncName=n_strdup(IDENTIFIER_POINTER(oldId));
    item->newFuncName=n_strdup(IDENTIFIER_POINTER(DECL_NAME(decl)));
    item->funcDecl=decl;
    item->number=order;
    item->selfTree=selfTree;
    item->exprList=NULL;
    item->origtypes=NULL;
    item->arg_loc=vNULL;
    item->call=current_function_decl;
    item->isStatic=isStatic;
    n_ptr_array_add(self->implicitlyArray,item);
}

/**
 * 是不是内建的函数
 */
static nboolean  isBuiltInFunc(tree functionId)
{
      tree decl = c_c_decl_get_decl_in_symbol_binding(functionId);
      if(aet_utils_valid_tree(decl)){
         char *funcName=IDENTIFIER_POINTER(functionId);
         char *treeTypeStr=get_tree_code_name(TREE_CODE(decl));
         if (TREE_CODE (decl) != FUNCTION_DECL){
            n_debug("isBuiltInFunc 00 不是函数声明返回 decl id:%s %s ",funcName,treeTypeStr);
            return TRUE ;
         }

         if (!fndecl_built_in_p (decl) && DECL_IS_BUILTIN (decl)){
             n_debug("isBuiltInFunc 11 !fndecl_built_in_p (decl) && DECL_IS_BUILTIN (decl) decl id:%s %s ",funcName,treeTypeStr);
         }else{
             n_debug("isBuiltInFunc 22 newtype= default_function_type id:%s %s ",funcName,treeTypeStr);

            /* Implicit declaration of a function already declared
               (somehow) in a different scope, or as a built-in.
               If this is the first time this has happened, warn;
               then recycle the old declaration but with the new type.  */
            if (!C_DECL_IMPLICIT (decl)){
               n_warning("isBuiltInFunc 33 !C_DECL_IMPLICIT (decl) id:%s %s ",funcName,treeTypeStr);
               //C_DECL_IMPLICIT (decl) = 1;
            }
            if (fndecl_built_in_p (decl)){
                n_debug("isBuiltInFunc 44 fndecl_built_in_p (decl) id:%s %s ",funcName,treeTypeStr);
            }else{
                n_debug("isBuiltInFunc 55 fndecl_built_in_p (decl) id:%s %s ",funcName,treeTypeStr);
            }
         }
         return TRUE;
      }
      return FALSE;
}



static ImplicitlyData *find(ImplicitlyCall *self,char *sysName,char *funcName)
{
    int i;
    for(i=0;i<self->implicitlyArray->len;i++){
        ImplicitlyData *item=n_ptr_array_index(self->implicitlyArray,i);
        if(!strcmp(item->sysName,sysName) && !strcmp(item->newFuncName,funcName)){
            return item;
        }
    }
    return NULL;
}

/**
 * 把泛型形参对应的实参转化为:实参--中间形参:如<int>---void*
 */
static void convertParmForGenerics(ImplicitlyCall *self,ClassName *className,CandidateFunc *selectedDecl,vec<tree, va_gc> *exprlist,
        location_t expr_loc,GenericModel *generics)
{
        tree decl=NULL_TREE;
        ClassFunc *item=selectedDecl->classFunc;
        n_debug("convertParmForGenerics  %s  field?:%d\n",className->sysName,aet_utils_valid_tree(item->fieldDecl));
        decl=item->fromImplDefine;
        if(decl==NULL_TREE){
            return;
        }
        int count=generic_call_replace_parm(generic_call_get(),expr_loc,decl, exprlist,className,generics);
       // printf("泛型转化成功的个数:%d\n",count);
}

/**
 * 明确是静态的，就执行这里。
 */
static tree selectedFromStatic(ImplicitlyCall *self,ImplicitlyData *data)
{
       tree func=data->funcDecl;
       tree id=DECL_NAME (func);
       tree last=NULL_TREE;
       char *origFunName=data->origFuncName;
       char *sysName=data->sysName;
       vec<tree, va_gc> *exprlist=data->exprList;
       vec<tree, va_gc> *origtypes=data->origtypes;
       location_t expr_loc =data->loc;
       vec<location_t> arg_loc=data->arg_loc;
       CandidateFunc *item=NULL;
       n_debug("selecImplicitly 00 name:%s className:%s origFunName:%s",IDENTIFIER_POINTER(id),sysName,origFunName);
       ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
       if(className==NULL){
           return NULL_TREE;
       }
       n_debug("selecImplicitly 找静态函数 找之前把self移走。");
       exprlist->ordered_remove(0);//把self参数移走
       origtypes->ordered_remove(0);//把self参数移走
       item=select_field_get_implicitly_static_func(select_field_get(),className,origFunName,exprlist,origtypes,arg_loc,expr_loc,NULL);
       if(item!=NULL)
           last=item->classFunc->fromImplDefine;
       else{
          ;
       }
       return last;
}

/**
 * 根据实参选择类方法。
 */
static CandidateFunc *selectFunc(ImplicitlyCall *self,ClassName *className,char *origName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,GenericModel *generics,GenericModel * funcGenericDefine)
{
    CandidateFunc *last=select_field_get_implicitly_func(select_field_get(),className,origName,exprlist,origtypes,arg_loc,expr_loc,generics,NULL);
    if(last==NULL)
        return NULL;
    if(class_func_is_func_generic(last->classFunc) || class_func_is_query_generic(last->classFunc)){
        generic_call_add_fpgi_parm(generic_call_get(),last->classFunc,className,exprlist,funcGenericDefine);
    }
    return last;
}

static tree selecImplicitly(ImplicitlyCall *self,ImplicitlyData *data)
{
    tree func=data->funcDecl;
    tree id=DECL_NAME (func);
    tree last=NULL_TREE;
    char *origFunName=data->origFuncName;
    char *sysName=data->sysName;
    vec<tree, va_gc> *exprlist=data->exprList;
    vec<tree, va_gc> *origtypes=data->origtypes;
    location_t expr_loc =data->loc;
    vec<location_t> arg_loc=data->arg_loc;
    n_debug("selecImplicitly 00 name:%s className:%s origFunName:%s\n",IDENTIFIER_POINTER(id),sysName,origFunName);
    ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
    if(className==NULL){
        return NULL_TREE;
    }
    CandidateFunc *item=NULL;
    GenericModel *generics=NULL;//在这里的调用都是在类实现中，不会有具体的泛型找到，所以是空的.
    GenericModel *funcGenericDefine=NULL;//如果funcGenericDefine是有效的说明这是一个泛型函数
    item=selectFunc(self,className,origFunName,exprlist,origtypes,arg_loc,expr_loc,generics,funcGenericDefine);
    //如果是field要加入指针，否则访问不到
    if(item!=NULL){
        n_debug("selecImplicitly is 找到了 00 class:%s mangleFunName:%s\n",className->sysName,item->classFunc->mangleFunName);
       convertParmForGenerics(self,className,item,exprlist,expr_loc,generics);//转化实参到void*
       last=item->classFunc->fromImplDefine;
       generic_check_parm(sysName,item->classFunc,exprlist,last);
    }else{
        n_debug("selecImplicitly 找静态函数 找之前把self移走。");
        exprlist->ordered_remove(0);//把self参数移走
        origtypes->ordered_remove(0);//把self参数移走
        item=select_field_get_implicitly_static_func(select_field_get(),className,origFunName,exprlist,origtypes,arg_loc,expr_loc,NULL);
        if(item!=NULL)
            last=item->classFunc->fromImplDefine;
    }
    if(item!=NULL){
        select_field_free_candidate(item);
    }
    return last;
}

typedef struct _WalkData{
    ImplicitlyData *item;
    tree replace;
}WalkData;

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
       if(TREE_CODE(func)==ADDR_EXPR){
           tree funcDecl=TREE_OPERAND (func, 0);
           if(TREE_CODE(funcDecl)==FUNCTION_DECL){
              char *funcName=IDENTIFIER_POINTER(DECL_NAME(funcDecl));
              ImplicitlyData *item=dp->item;
              if(!strcmp(funcName,item->newFuncName) && funcDecl==item->funcDecl){
                 tree newCallExpr = c_build_function_call_vec (item->loc, vNULL, dp->replace, item->exprList, NULL);
                 n_debug("implicitly 替换调用的函数这是一个调用----00 %s %s",funcName,item->newFuncName);
                 *tp=newCallExpr;
              }
           }

       }
       aet_print_tree(t);
   }
   return NULL_TREE;
}

static void link(ImplicitlyCall *self ,ImplicitlyData *item)
{
    //连到静态的
    n_debug("start link %s\n",item->origFuncName);
    tree ret=NULL_TREE;
    if(item->isStatic)
         ret=selectedFromStatic(self,item);
    else
         ret=selecImplicitly(self,item);
    if(aet_utils_valid_tree(ret)){
          aet_print_tree(ret);
          n_debug("start link  找到了函数定义 %s\n",item->origFuncName);
          WalkData data;
          data.item = item;
          data.replace=ret;
          walk_tree (&DECL_SAVED_TREE ( item->call), link_cb, &data, NULL);
    }else{
         //error_at(item->loc,"在类%qs中，没有找到函数%qs的实现。",item->sysName,item->origFuncName);
         //恢复原名调用，由link器查找。
         aet_print_tree(ret);
         n_debug("start link  没有找到函数定义 恢复原函数调用。%s\n",item->origFuncName);
         tree id=aet_utils_create_ident(item->origFuncName);
         tree type=NULL_TREE;
         tree ret = build_external_ref (item->loc, id,TRUE,&type);
         WalkData data;
         data.item = item;
         data.replace=ret;
         //item中的参数在selectedFromStatic或selecImplicitly，如果到这里说明已以移走了。
         walk_tree (&DECL_SAVED_TREE ( item->call), link_cb, &data, NULL);
    }
}


/**
 * 链接到定义的函数中
 * 在impl$结尾处调用
 */
void  implicitly_call_link(ImplicitlyCall *self)
{
    c_parser *parser=self->parser;
    ClassImpl *impl=class_impl_get();
    char *sysName=impl->className->sysName;
    int i;
    for(i=0;i<self->implicitlyArray->len;i++){
          ImplicitlyData *item=n_ptr_array_index(self->implicitlyArray,i);
          if(!strcmp(item->sysName,sysName)){
              link(self,item);
          }
    }
}

/**
 * 把参数加到ImplicitlyData
 */
void implicitly_call_add_parm(ImplicitlyCall *self,tree func,vec<tree, va_gc> *exprlist,vec<tree, va_gc> *origtypes,vec<location_t> arg_loc)
{
    c_parser *parser=self->parser;
    if(!parser->isAet)
        return;
    char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
    ClassImpl *impl=class_impl_get();
    char *sysName=impl->className->sysName;
    ImplicitlyData *item=find(self,sysName,funcName);
    if(item==NULL)
        return;
    if(item->funcDecl!=func){
        aet_print_tree_skip_debug(func);
        n_error("隐式函数:%s,函数声明不匹配。报错此错误。\n",item->newFuncName);
        return;
    }

    vec<tree, va_gc> *destParmVec;
    vec<tree, va_gc> *destOrigTypesVec;
    destParmVec = make_tree_vector ();
    destOrigTypesVec = make_tree_vector ();
    nboolean valid=aet_utils_valid_tree(item->selfTree);
    //printf("self is null %d\n",valid);
    //aet_print_tree( item->selfTree);

    vec_safe_push (destParmVec, item->selfTree);
    vec_safe_push (destOrigTypesVec, valid?TREE_TYPE(item->selfTree):NULL_TREE);
    item->arg_loc.safe_push (item->loc);
    int ix;
    tree arg;
    if(exprlist){
        for (ix = 0; exprlist->iterate (ix, &arg); ++ix){
           vec_safe_push (destParmVec, arg);
        }
    }
    if(origtypes){
        for (ix = 0; origtypes->iterate (ix, &arg); ++ix){
           // printf("implicitly_call_add_parm 加参数原始类型到  %d\n",ix);
           vec_safe_push (destOrigTypesVec, arg);
        }
    }
    item->exprList=destParmVec;
    item->origtypes=destOrigTypesVec;
    location_t parmLoc;
    for (ix = 0; arg_loc.iterate (ix, &parmLoc); ++ix){
        //printf("implicitly_call_add_parm 加参数位置到  %lu\n",parmLoc);
        item->arg_loc.safe_push (parmLoc);
    }
}

/**
 * 当在classimpl.c中的class_impl_process_expression方法，找不到函数时，进入这里
 * 不在aet中，或不是直接调用都返回
 */
tree  implicitly_call_call(ImplicitlyCall *self,struct c_expr expr,location_t loc,tree id)
{
    c_parser *parser=self->parser;
    if(!parser->isAet)
        return NULL_TREE;
    if(aet_utils_valid_tree(expr.value) && TREE_CODE(expr.value)<MAX_TREE_CODES ){
        printf("只有在impl$中直接调用的方法，才能处理。\n");
        aet_print_tree(expr.value);
        return NULL_TREE;
    }
    nboolean builtIn=isBuiltInFunc(id);
    if(builtIn){
        n_debug("implicitly_call_call 00 是内建的。 %s",IDENTIFIER_POINTER(id));
    }else{
        ClassImpl *impl=class_impl_get();
        int order=0;
        tree funcDecl=createImplicitlyId(loc,id,&order);
        tree selfTree=lookup_name(aet_utils_create_ident("self"));
        add(self,impl->className->sysName,loc,id,funcDecl,order,selfTree,FALSE);
        n_warning("implicitly_call_call 11 隐式的函数的。 old:%s new:%s",IDENTIFIER_POINTER(id),IDENTIFIER_POINTER(DECL_NAME(funcDecl)));
        return funcDecl;
    }
    return NULL_TREE;
}

/**
 * 如果调用的不是本类的方法，不处理。
 * 是与静态函数调用作为约束的。
 */
tree  implicitly_call_call_from_static(ImplicitlyCall *self,struct c_expr expr,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean onlyStatic)
{
    c_parser *parser=self->parser;
    tree func=expr.value;
    if(!parser->isAet)
        return NULL_TREE;
    printf("有静态声明的函数，但参数不正确，归于隐藏的调用implicitly\n");
    aet_print_tree(func);
    char currentClassName[256];
    char funcName[256];
    int len=aet_utils_get_orgi_func_and_class_name(IDENTIFIER_POINTER(DECL_NAME(func)),currentClassName,funcName);
    if(len==0)
        return NULL_TREE;
    tree id=aet_utils_create_ident(funcName);
    nboolean builtIn=isBuiltInFunc(id);
    if(builtIn){
        n_debug("implicitly_call_call_from_static 00 是内建的。 %s",IDENTIFIER_POINTER(id));
    }else{
        printf("有静态声明的函数，但参数不正确，归于隐藏的调用implicitly funcName:%s\n",funcName);
        aet_print_tree(func);
        ClassImpl *impl=class_impl_get();
        if(strcmp(impl->className->sysName,currentClassName)){
            n_warning("implicitly_call_call_from_static 11 不是本类的方法。 old:%s 所在类:%s 类方法：%s",
                    IDENTIFIER_POINTER(id),impl->className->sysName,currentClassName);
            return NULL_TREE;
        }
        int order=0;
        tree funcDecl=createImplicitlyId(expr_loc,id,&order);
        tree selfTree=lookup_name(aet_utils_create_ident("self"));
        add(self,impl->className->sysName,expr_loc,id,funcDecl,order,selfTree,onlyStatic);
        n_warning("implicitly_call_call_from_static 22 隐式的函数的。 old:%s new:%s 所在类:%s 类方法：%s",
                IDENTIFIER_POINTER(id),IDENTIFIER_POINTER(DECL_NAME(funcDecl)),impl->className->sysName,currentClassName);
        implicitly_call_add_parm(self,funcDecl,exprlist,origtypes,arg_loc);
        return funcDecl;
    }
    return NULL_TREE;
}


void  implicitly_call_set_parser(ImplicitlyCall *self,c_parser *parser)
{
    self->parser=parser;
}


ImplicitlyCall *implicitly_call_new()
{
    ImplicitlyCall *self = n_slice_alloc0 (sizeof(ImplicitlyCall));
    implicitlyCallInit(self);
	return self;
}


