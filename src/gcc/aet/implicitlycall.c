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
#include "attribs.h"
#include "toplev.h"
#include "varasm.h"
#include "c-family/c-common.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "aet-c-parser-header.h"
#include "tree-iterator.h"
#include "tree-pass.h"
#include "cfg.h"
#include "function.h"
#include "value-range.h"

#include "basic-block.h"
#include "pass_manager.h"
#include "context.h"
#include "gimple.h"
#include "gimplify.h"
#include "gimple-ssa.h"
#include "gimple-iterator.h"
#include "tree-iterator.h"
#include "tree-into-ssa.h"
#include "tree-ssa-alias.h"
#include "tree-ssanames.h"

#include "aetutils.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "classimpl.h"
#include "classmgr.h"
#include "implicitlycall.h"
#include "funcmgr.h"
#include "genericcall.h"
#include "selectfield.h"
#include "genericutil.h"
#include "varmgr.h"
#include "mtcsparser.h"
#include "classparser.h"
#include "aetprintgimple.h"

static int number=0;//创建的函数声明编号

enum impl_conv {
  ic_argpass,
  ic_assign,
  ic_init,
  ic_init_const,
  ic_return,
  ic_unknow
};

typedef struct _ImplicitlyData {
    char *sysName;//所在的类
    location_t loc;
    char *origFuncName;//原始名字
    char *newFuncName; //新的名字  _implicitly_xxxx_yyy
    tree  funcDecl;//函数声明 _implicitly_xxxx_yyy
    int number;   //序号
    vec<tree, va_gc> *exprList;
    vec<tree, va_gc> *origtypes;
    vec<location_t>   arg_loc;
    tree selfTree;//self参数
    tree caller;//调用implicitly的函数。
    tree callee;//被调用的隐藏函数
    nboolean isStatic;//作为静态函数调用吗
    struct {
        enum impl_conv implConv;
        tree lhsOrType;
        tree rhsOrigType;
        int parmNum;//参数位置
    }convertParm;
    nboolean isKernel;//是一个核函数 这个参数是在link_cb中赋值的
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
   if (ref != current_function_decl)
      TREE_USED (ref) = 1;

   if (TREE_CODE (ref) == FUNCTION_DECL && !in_alignof){
      if (!in_sizeof && !in_typeof){
         C_DECL_USED (ref) = 1;
      }else if (DECL_INITIAL (ref) == NULL_TREE && DECL_EXTERNAL (ref) && !TREE_PUBLIC (ref)){
         aet_record_maybe_used_decl (ref);
      }
   }

   if (TREE_CODE (ref) == CONST_DECL){
      used_types_insert (TREE_TYPE (ref));
      if (warn_cxx_compat  && TREE_CODE (TREE_TYPE (ref)) == ENUMERAL_TYPE
            && C_TYPE_DEFINED_IN_STRUCT (TREE_TYPE (ref))){
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
   item->caller=current_function_decl;
   item->callee=NULL_TREE;
   item->isStatic=isStatic;
   item->convertParm.implConv=ic_unknow;
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
         return FALSE ;
      }
      nboolean re= fndecl_built_in_p (decl);
      n_debug("isBuiltInFunc 11 fndecl_built_in_p (decl) id:%s %s OK:%d ",funcName,treeTypeStr,re);
      if(re)
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

static ImplicitlyData *findByFunctionDecl(ImplicitlyCall *self,tree fndecl)
{
   int i;
   for(i=0;i<self->implicitlyArray->len;i++){
      ImplicitlyData *item=n_ptr_array_index(self->implicitlyArray,i);
      if(item->funcDecl==fndecl)
         return item;
   }
   return NULL;
}

static nboolean createParamList(ImplicitlyCall *self,ImplicitlyData *item)
{
   if(item->exprList){
      error_at(item->loc,"exprlist参数列表只能创建一次。");
      return FALSE;
   }
   item->exprList = make_tree_vector ();
   item->origtypes = make_tree_vector ();
   nboolean valid=aet_utils_valid_tree(item->selfTree);
   vec_safe_push (item->exprList, item->selfTree);
   vec_safe_push ( item->origtypes, valid?TREE_TYPE(item->selfTree):NULL_TREE);
   tree call = item->callee;
   if(!aet_utils_valid_tree(call))
      return FALSE;
   int nargs=call_expr_nargs (call);
   int i;
   for(i=0;i<nargs;i++){
      tree arg=CALL_EXPR_ARG (call, i);
      n_debug("createParamList 00 %d %d %d",item->exprList->length(),item->origtypes->length(),item->arg_loc.length());
      aet_print_tree(arg);
      vec_safe_push (item->exprList, arg);
      vec_safe_push (item->origtypes, TREE_TYPE(arg));
      //if(DECL_P(arg))
       // item->arg_loc.safe_push (DECL_SOURCE_LOCATION(arg));
   //   else
      //  item->arg_loc.safe_push (item->loc);
   }
   return TRUE;
}

/**
 * 明确是静态的，就执行这里。
 */
static tree selectedFromStatic(ImplicitlyCall *self,ImplicitlyData *data,SelectFunc *selectFunc)
{
   tree func=data->funcDecl;
   tree id=DECL_NAME (func);
   tree last=NULL_TREE;
   char *origFunName=data->origFuncName;
   char *sysName=data->sysName;
   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
   if(className==NULL){
      return NULL_TREE;
   }
   n_debug("selectedFromStatic 00 name:%s className:%s origFunName:%s",IDENTIFIER_POINTER(id),sysName,origFunName);
   if(!createParamList(self,data))
      return NULL_TREE;
   vec<tree, va_gc> *exprlist=data->exprList;
   vec<tree, va_gc> *origtypes=data->origtypes;
   location_t expr_loc =data->loc;
   vec<location_t> arg_loc=data->arg_loc;
   CandidateFunc *item=NULL;
   n_debug("selectedFromStatic 11 找静态函数 找之前把self移走。exprlist:%p origtypes:%p",exprlist,origtypes);
   exprlist->ordered_remove(0);//把self参数移走
   origtypes->ordered_remove(0);//把self参数类型移走
   if(!data->arg_loc.is_empty())
      data->arg_loc.ordered_remove(0);//把self位置移走
   item=select_field_get_implicitly_static_func(select_field_get(),className,origFunName,exprlist,origtypes, data->arg_loc,expr_loc,NULL);
   if(item!=NULL){
      last=item->classFunc->fromImplDefine;
      selectFunc->classFunc=item->classFunc;
      selectFunc->sysName=class_mgr_get_class_name_by_sys(class_mgr_get(),item->sysName)->sysName;
      selectFunc->sucessed=1;
      select_field_free_candidate(item);
   }else{
      ;
   }
   return last;
}

/**
 * 根据实参选择类方法。
 */
static CandidateFunc *selectFunc(ImplicitlyCall *self,ClassName *className,char *origName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,GenericModel *generics,
        GenericModel * funcGenericDefine)
{
   CandidateFunc *last=select_field_get_implicitly_func(select_field_get(),className,origName,exprlist,origtypes,arg_loc,expr_loc,generics,NULL);
   if(last==NULL)
      return NULL;
   if(class_func_is_func_generic(last->classFunc) || class_func_have_query_param(last->classFunc)){
      nboolean ok=generic_call_check/*!generic_call_add_fpgi_parm*/(generic_call_get(),
            last->classFunc,className,exprlist,funcGenericDefine,expr_loc);
   }
   return last;
}

static tree selecImplicitly(ImplicitlyCall *self,ImplicitlyData *data,SelectFunc *selRet)
{
   tree func=data->funcDecl;
   tree id=DECL_NAME (func);
   tree last=NULL_TREE;
   char *origFunName=data->origFuncName;
   char *sysName=data->sysName;
   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
   if(className==NULL)
      return NULL_TREE;
   if(!createParamList(self,data))
      return NULL_TREE;
   vec<tree, va_gc> *exprlist=data->exprList;
   vec<tree, va_gc> *origtypes=data->origtypes;
   location_t expr_loc =data->loc;
   n_debug("selecImplicitly 00 name:%s className:%s origFunName:%s 参数个数----%p %p %d %d\n",
         IDENTIFIER_POINTER(id),sysName,origFunName,exprlist,origtypes,exprlist->length(),origtypes->length());

   CandidateFunc *item=NULL;
   GenericModel *generics=NULL;//在这里的调用都是在类实现中，不会有具体的泛型找到，所以是空的.
   GenericModel *funcGenericDefine=NULL;//如果funcGenericDefine是有效的说明这是一个泛型函数
   item=selectFunc(self,className,origFunName,exprlist,origtypes,data->arg_loc,expr_loc,generics,funcGenericDefine);
   //如果是field要加入指针，否则访问不到
   if(item!=NULL){
      n_debug("selecImplicitly 11  找到了  class:%s mangleFunName:%s\n",className->sysName,item->classFunc->mangleFunName);
      //convertParmForGenerics(self,className,item,exprlist,expr_loc,generics);//转化实参到void*
      last=item->classFunc->fromImplDefine;
      //convertParmForGenerics  generic_check_parm功能在select_field_call_back调用
      //设置后再select_field_build_function_call_vec再调用setGenericQuery
      //检查data->funcDecl是不是integer，如果不是说明调用该函数的返回值来类型来自左边
      if(TREE_CODE(TREE_TYPE(func))==INTEGER_TYPE){
         ;
      }else{
         tree type=TREE_TYPE(TREE_TYPE(item->classFunc->fromImplDefine));
      }
   }else{
      n_debug("selecImplicitly 22 找静态函数 找之前把self移走。exprlist:%p origtypes:%p %d\n",exprlist,origtypes,exprlist->length());
      exprlist->ordered_remove(0);//把self参数移走
      origtypes->ordered_remove(0);//把self参数类型移走
      if(!data->arg_loc.is_empty())
         data->arg_loc.ordered_remove(0);//把self位置移走

      item=select_field_get_implicitly_static_func(select_field_get(),className,origFunName,exprlist,origtypes,data->arg_loc,expr_loc,NULL);
      if(item!=NULL)
         last=item->classFunc->fromImplDefine;
   }
   if(item!=NULL){
      selRet->classFunc=item->classFunc;
      selRet->sysName=class_mgr_get_class_name_by_sys(class_mgr_get(),item->sysName)->sysName;
      selRet->sucessed=1;
      select_field_free_candidate(item);
   }
   return last;
}

/**
 * 参照c-typeck.cc中的store_init_value
 */
static tree convertForInit(ImplicitlyData *item,tree init)
{
    location_t init_loc=item->loc;
    tree decl=item->convertParm.lhsOrType;
    tree origtype=item->convertParm.rhsOrigType;
    tree value, type;
    bool npc = false;
    bool int_const_expr = false;
    bool arith_const_expr = false;

    /* If variable's type was invalidly declared, just ignore it.  */
    type = TREE_TYPE (decl);
    if (TREE_CODE (type) == ERROR_MARK)
      return init;
    npc = null_pointer_constant_p (init);
    int_const_expr = (TREE_CODE (init) == INTEGER_CST && !TREE_OVERFLOW (init)  && INTEGRAL_TYPE_P (TREE_TYPE (init)));
    arith_const_expr = true;
    bool constexpr_p = (VAR_P (decl)  && C_DECL_DECLARED_CONSTEXPR (decl));
    value = aet_digest_init (init_loc, decl,type, init, origtype, npc, int_const_expr,
    arith_const_expr, true, TREE_STATIC (decl) || constexpr_p, constexpr_p);
    return value;
}

/**
 * 参照build_modify_expr
 */
static tree convertForModify (ImplicitlyData *item,tree rhs)
{
    location_t location=item->loc;
    tree lhs=item->convertParm.lhsOrType;
    tree lhs_origtype=TREE_TYPE(lhs);
    location_t rhs_loc=item->loc;
    tree rhs_origtype=item->convertParm.rhsOrigType;
    tree newrhs;
    tree lhstype = TREE_TYPE (lhs);
    bool npc;

    if (TREE_CODE (lhs) == ERROR_MARK || TREE_CODE (rhs) == ERROR_MARK)
        return error_mark_node;
    // Ensure an error for assigning a non-lvalue array to an array in C90.
    if (TREE_CODE (lhstype) == ARRAY_TYPE){
        error_at (location, "assignment to expression with array type");
        return error_mark_node;
    }
    newrhs = rhs;
    npc = null_pointer_constant_p (newrhs);
    newrhs = aet_convert_for_assignment (location, rhs_loc, lhstype, newrhs,
            rhs_origtype, ic_assign, npc,NULL_TREE, NULL_TREE, 0,0);
    return newrhs;
}

static tree convertForReturn (ImplicitlyData *item,tree retval)
{
    location_t loc=item->loc;
    //item->lhsOrType已经通过 TREE_TYPE (TREE_TYPE (current_function_decl))获取到了函数返回值类型
    tree valtype = item->convertParm.lhsOrType;
    printf("函数返回值-----\n");
    aet_print_tree(item->convertParm.lhsOrType);

    aet_print_tree(valtype);
    tree origtype=item->convertParm.rhsOrigType;
    tree ret_stmt;
    bool no_warning = false;
    bool npc = false;

  /* Use the expansion point to handle cases such as returning NULL
     in a function returning void.  */
   location_t xloc = expansion_point_location_if_in_system_header (loc);
   if (retval){
      tree semantic_type = NULL_TREE;
      npc = null_pointer_constant_p (retval);
      if (TREE_CODE (retval) == EXCESS_PRECISION_EXPR){
         semantic_type = TREE_TYPE (retval);
         retval = TREE_OPERAND (retval, 0);
      }
      retval = c_fully_fold (retval, false, NULL);
      if (semantic_type && valtype != NULL_TREE  && TREE_CODE (valtype) != VOID_TYPE)
          retval = build1 (EXCESS_PRECISION_EXPR, semantic_type, retval);
   }

    if (valtype == NULL_TREE || VOID_TYPE_P (valtype))
    {
      current_function_returns_null = 1;
      bool warned_here;
      if (TREE_CODE (TREE_TYPE (retval)) != VOID_TYPE)
          warned_here = permerror_opt(xloc, OPT_Wreturn_mismatch,"%<return%> with a value, in function returning void");
      else
        warned_here = pedwarn(xloc, OPT_Wpedantic, "ISO C forbids %<return%> with expression, in function returning void");
      if (warned_here)
           inform (loc,"declared here");
      return retval;
    }else {
      tree t = aet_convert_for_assignment (loc, UNKNOWN_LOCATION, valtype,
                       retval, origtype, ic_return,npc, NULL_TREE, NULL_TREE, 0,0);
      return t;
    }
}


/**
 * 参照c-tyhpeck.cc中的convert_arguments
 */
static tree convert_arguments (ImplicitlyData *item, tree fundecl,tree parmType,tree impli)
{
  bool error_args = false;
  const bool type_generic = fundecl
    && lookup_attribute ("type generic", TYPE_ATTRIBUTES (TREE_TYPE (fundecl)));
  bool type_generic_remove_excess_precision = false;
  bool type_generic_overflow_p = false;
  bool type_generic_bit_query = false;
  tree function=item->convertParm.lhsOrType;
  tree origtype=item->convertParm.rhsOrigType;
  location_t loc=item->loc;
  tree val=impli;
  tree type=parmType;
  unsigned int parmnum=item->convertParm.parmNum;
  /* Change pointer to function to the function itself for
     diagnostics.  */
  if (TREE_CODE (function) == ADDR_EXPR && TREE_CODE (TREE_OPERAND (function, 0)) == FUNCTION_DECL)
    function = TREE_OPERAND (function, 0);


  /* For a call to a built-in function declared without a prototype,
     set to the built-in function's argument list.  */
  tree builtin_typetail = NULL_TREE;

  /* For type-generic built-in functions, determine whether excess
     precision should be removed (classification) or not
     (comparison).  */
  if (fundecl   && fndecl_built_in_p (fundecl, BUILT_IN_NORMAL)){
      built_in_function code = DECL_FUNCTION_CODE (fundecl);
      if (C_DECL_BUILTIN_PROTOTYPE (fundecl)){
          /* For a call to a built-in function declared without a prototype
             use the types of the parameters of the internal built-in to
             match those of the arguments to.  */
          if (tree bdecl = builtin_decl_explicit (code)){
            tree builtin_typelist = TYPE_ARG_TYPES (TREE_TYPE (bdecl));
            int cc=0;
            for (tree al = builtin_typelist; al; al = TREE_CHAIN (al)){
                 tree type=TREE_VALUE(al);
                 if(cc==parmnum){
                     builtin_typetail=al;
                     break;
                 }
             }
          }
      }

      /* For type-generic built-in functions, determine whether excess
     precision should be removed (classification) or not
     (comparison).  */
      if (type_generic)
        switch (code)
          {
          case BUILT_IN_ISFINITE:
          case BUILT_IN_ISINF:
          case BUILT_IN_ISINF_SIGN:
          case BUILT_IN_ISNAN:
          case BUILT_IN_ISNORMAL:
          case BUILT_IN_ISSIGNALING:
          case BUILT_IN_FPCLASSIFY:
            type_generic_remove_excess_precision = true;
            break;

          case BUILT_IN_ADD_OVERFLOW_P:
          case BUILT_IN_SUB_OVERFLOW_P:
          case BUILT_IN_MUL_OVERFLOW_P:
            /* The last argument of these type-generic builtins
               should not be promoted.  */
            type_generic_overflow_p = true;
            break;

          case BUILT_IN_CLZG:
          case BUILT_IN_CTZG:
          case BUILT_IN_CLRSBG:
          case BUILT_IN_FFSG:
          case BUILT_IN_PARITYG:
          case BUILT_IN_POPCOUNTG:
            /* The first argument of these type-generic builtins
               should not be promoted.  */
            type_generic_bit_query = true;
            break;

          default:
            break;
          }
    }

  /* Scan the given expressions (VALUES) and types (TYPELIST), producing
     individual converted arguments.  */


      tree builtin_type = (builtin_typetail? TREE_VALUE (builtin_typetail) : NULL_TREE);
      /* The original type of the argument being passed to the function.  */
      tree valtype = TREE_TYPE (val);
      /* The called function (or function selector in Objective C).  */
      tree rname = function;
      int argnum=parmnum+1;
      const char *invalid_func_diag;
      /* Set for EXCESS_PRECISION_EXPR arguments.  */
      bool excess_precision = false;
      /* The value of the argument after conversion to the type
     of the function parameter it is passed to.  */
      tree parmval;
      /* Some __atomic_* builtins have additional hidden argument at
     position 0.  */
      location_t ploc=loc;

      if (type == void_type_node){
          error_at (loc, "too many arguments to function %qE", function);
          return val;
      }

      if (builtin_type == void_type_node){
         warning_at (loc, OPT_Wbuiltin_declaration_mismatch,"too many arguments to built-in function %qE expecting %d", function, parmnum);
         builtin_typetail = NULL_TREE;
      }
      /* Determine if VAL is a null pointer constant before folding it.  */
      bool npc = null_pointer_constant_p (val);

      /* If there is excess precision and a prototype, convert once to
     the required type rather than converting via the semantic
     type.  Likewise without a prototype a float value represented
     as long double should be converted once to double.  But for
     type-generic classification functions excess precision must
     be removed here.  */
      if (TREE_CODE (val) == EXCESS_PRECISION_EXPR  && (type || !type_generic || !type_generic_remove_excess_precision)){
          val = TREE_OPERAND (val, 0);
          excess_precision = true;
      }
      val = c_fully_fold (val, false, NULL);
      STRIP_TYPE_NOPS (val);
      val = require_complete_type (ploc, val);

      /* Some floating-point arguments must be promoted to double when
     no type is specified by a prototype.  This applies to
     arguments of type float, and to architecture-specific types
     (ARM __fp16), but not to _FloatN or _FloatNx types.  */
      bool promote_float_arg = false;
      if (type == NULL_TREE  && TREE_CODE (valtype) == REAL_TYPE && (TYPE_PRECISION (valtype) <= TYPE_PRECISION (double_type_node))
          && TYPE_MAIN_VARIANT (valtype) != double_type_node  && TYPE_MAIN_VARIANT (valtype) != long_double_type_node
          && !DECIMAL_FLOAT_MODE_P (TYPE_MODE (valtype))){
         /* Promote this argument, unless it has a _FloatN or
         _FloatNx type.  */
          promote_float_arg = true;
          for (int i = 0; i < NUM_FLOATN_NX_TYPES; i++)
              if (TYPE_MAIN_VARIANT (valtype) == FLOATN_NX_TYPE_NODE (i)){
                promote_float_arg = false;
                break;
              }
          /* Don't promote __bf16 either.  */
          if (TYPE_MAIN_VARIANT (valtype) == bfloat16_type_node)
            promote_float_arg = false;
      }

      if (type != NULL_TREE){
          parmval = aet_convert_argument (ploc, function, fundecl, type, origtype,val, valtype, npc, rname, parmnum, argnum,
                          excess_precision, 0);
      }else if (promote_float_arg){
          if (type_generic)
              parmval = val;
          else{
              /* Convert `float' to `double'.  */
              if (warn_double_promotion && !c_inhibit_evaluation_warnings)
                   warning_at (ploc, OPT_Wdouble_promotion,"implicit conversion from %qT to %qT when passing argument to function",
                               valtype, double_type_node);
              n_debug("implicitlycall.c  convert_arguments 11 %p %s %d\n",
                       val,get_tree_code_name(TREE_CODE(TREE_TYPE(val))),TYPE_MODE(TREE_TYPE(val)));
              parmval = convert (double_type_node, val);
              n_debug("implicitlycall.c  convert_arguments 22 %p %s %d\n",
                      parmval,get_tree_code_name(TREE_CODE(TREE_TYPE(parmval))),TYPE_MODE(TREE_TYPE(parmval)));
         }
      }else if ((excess_precision && !type_generic)
           || (type_generic_overflow_p && parmnum == 2)
           || (type_generic_bit_query && parmnum == 0)){
            /* A "double" argument with excess precision being passed
               without a prototype or in variable arguments.
               The last argument of __builtin_*_overflow_p should not be
               promoted, similarly the first argument of
               __builtin_{clz,ctz,clrsb,ffs,parity,popcount}g.  */
          parmval = convert (valtype, val);
    /*  else if ((invalid_func_diag = targetm.calls.invalid_arg_for_unprototyped_fn (typelist, fundecl, val))){
          error (invalid_func_diag);
          return -1;
          */
      } else if (TREE_CODE (val) == ADDR_EXPR && reject_gcc_builtin (val)){
          return -1;
      }else{
        /* Convert `short' and `char' to full-size `int'.  */
        parmval = default_conversion (val);
      }

      if (!type && builtin_type && TREE_CODE (builtin_type) != VOID_TYPE){
      /* For a call to a built-in function declared without a prototype,
         perform the conversions from the argument to the expected type
         but issue warnings rather than errors for any mismatches.
         Ignore the converted argument and use the PARMVAL obtained
         above by applying default conversions instead.  */
          parmval=aet_convert_argument (ploc, function, fundecl, builtin_type, origtype,
                    val, valtype, npc, rname, parmnum, argnum,
                    excess_precision,
                    OPT_Wbuiltin_declaration_mismatch);
      }
      return parmval;
}

/**
 * 重要，获取第n个参数的函数声明中的类型
 * 像printf这样的可变参数函数
 * int printf(const char *format, ...)
 * 第一个参数类型确定，第二个...未知，所以返回的类型是NULL_TREE
 * 在
 */
static tree getFndeclArgType(tree fndecl,int n)
{
   tree funcType=TREE_TYPE(fndecl);
   int i=0;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      if(i==n)
         return type;
      printf("getFndeclArgType -----i:%d type:%s %d\n",i,get_tree_code_name(TREE_CODE(type)),TYPE_MODE(type));
      aet_print_tree(type);
      i++;
   }
   return NULL_TREE;
}

/**
 * 在函数 convert_arguments 中如果type是空的，实参是float
 * 会通过 promote_float_arg 提升 float 到 double,
 * 解决了 bug 030的问题。
 */
static tree convertForFuncCall (ImplicitlyData *item,tree impli)
{
   tree funcCall=item->convertParm.lhsOrType;
   if(TREE_CODE(funcCall)!=CALL_EXPR)//隐藏函数的调用可能还有CONVERT_EXPR
      return impli;
   tree fn=CALL_EXPR_FN (funcCall);
   if(TREE_CODE(fn)!=ADDR_EXPR)
      return impli;

   tree fndecl=TREE_OPERAND (fn, 0);
   if(TREE_CODE(fndecl)!=FUNCTION_DECL)
      return impli;
   int i = 0;
   tree arg;
   call_expr_arg_iterator iter;
   FOR_EACH_CALL_EXPR_ARG (arg, iter, funcCall){
      if(i==item->convertParm.parmNum){
         tree  type=getFndeclArgType(fndecl,i);
         impli=convert_arguments (item, fndecl,type,impli);
         type=TREE_TYPE(impli);
         n_debug("convertForFuncCall 00 进入这里转化----parmNum:%d %s %d\n",i,get_tree_code_name(TREE_CODE(type)),TYPE_MODE(type));
         break;
      }
      i++;
   }
   return impli;
}

typedef struct _WalkData{
    ImplicitlyData *item;
    tree replace;
    SelectFunc *selectFunc;
    ImplicitlyCall *implicitlyCall;
}WalkData;

static void test_print_exprlist(vec<tree, va_gc> *exprlist)
{
   int ix;
   tree arg;
   int count=0;
   for (ix = 0; exprlist->iterate (ix, &arg); ++ix){
      printf("print_exprlist -- %d\n",count++);
      aet_print_tree(arg);
   }
}

static void test_add_list(tree replaceKernel)
{
   if(!replaceKernel)
      return;
   tree fndecl = replaceKernel;
   tree save=DECL_SAVED_TREE (fndecl);
   char *id=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   if(!strstr(id,"activate"))
      return;
   n_debug("test_add_list 00xxxx\n");
   aet_print_tree(replaceKernel);
   tree fnbody=save;
   aet_print_tree(save);
//   tree stmtList = save;
//      tree_stmt_iterator tsi;
//      tree fnbody ;
//      for (tsi = tsi_start(stmtList); !tsi_end_p(tsi);  tsi_next(&tsi)){
//         fnbody = tsi_stmt(tsi);
//         break;
//
//      }
//
//
//   n_debug("test_add_list 00\n");
//   aet_print_tree(fnbody);

   if(TREE_CODE(fnbody)!=BIND_EXPR)
      return;
   tree body = BIND_EXPR_BODY(fnbody);
   if(TREE_CODE(body)!=CALL_EXPR)
      return;
   tree fn= CALL_EXPR_FN (body);
   tree callee=TREE_OPERAND(fn,0);
   char *idx=IDENTIFIER_POINTER(DECL_NAME(callee));
   if(!strstr(idx,"_implicitly_forwardMTCS_0"))
      return;
   tree stmt_list = alloc_stmt_list();
   append_to_statement_list(body, &stmt_list);
   BIND_EXPR_BODY(fnbody)=stmt_list;
   n_debug("test_add_list ok\n");
   aet_print_tree(fnbody);

}


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
                  tree newCallExpr;
                  if(dp->selectFunc->sucessed==0){
                      n_warning("找不到类函数，还是用外部函数实现call funcName：%s",funcName);
                      //test_print_exprlist(item->exprList);
                       newCallExpr = c_build_function_call_vec (item->loc, item->arg_loc, dp->replace, item->exprList, item->origtypes);
                  }else{
                       newCallExpr= select_field_build_function_call_vec(select_field_get(),item->loc, item->arg_loc,
                                       dp->replace, item->exprList, item->origtypes,dp->selectFunc);
                  }
                  if(newCallExpr==error_mark_node){
                     *walk_subtrees = 0;
                     return NULL_TREE;
                  }
                 n_debug("implicitly 替换调用的函数这是一个调用----00 %s %s %d",funcName,item->newFuncName,item->convertParm.implConv);
                 aet_print_tree(newCallExpr);
                 if(item->convertParm.implConv==ic_init || item->convertParm.implConv==ic_init_const){
                     n_debug("implicitly 替换调用的函数这是一个调用----11 %s %s",funcName,item->newFuncName);
                     aet_print_tree(item->convertParm.lhsOrType);
                     *tp=convertForInit(item,newCallExpr);
                      n_debug("implicitly 替换调用的函数这是一个调用----22 %s %s",funcName,item->newFuncName);
                      aet_print_tree(item->convertParm.lhsOrType);
                 }else if(item->convertParm.implConv==ic_assign){
                     *tp=convertForModify(item,newCallExpr);
                 }else if(item->convertParm.implConv==ic_return){
                     *tp=convertForReturn(item,newCallExpr);
                 }else if(item->convertParm.implConv==ic_argpass){
                     *tp=convertForFuncCall(item,newCallExpr);
                 }else{
                      *tp=newCallExpr;
                 }
                 *walk_subtrees = 0;
              }
           }
       }
   }
   return NULL_TREE;
}

static tree kernel_replace_cb (tree *tp, int *walk_subtrees, void *data)
{
   WalkData *dp = (WalkData *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR){
      walk_tree (&BIND_EXPR_BODY (t), kernel_replace_cb, data, NULL);
   }else if(TREE_CODE(t)==CALL_EXPR){
       tree func=CALL_EXPR_FN(t);
       if(TREE_CODE(func)==ADDR_EXPR){
           tree funcDecl=TREE_OPERAND (func, 0);
           if(TREE_CODE(funcDecl)==FUNCTION_DECL){
              char *funcName=IDENTIFIER_POINTER(DECL_NAME(funcDecl));
              ImplicitlyData *item=dp->item;
              if(!strcmp(funcName,item->newFuncName) && funcDecl==item->funcDecl){
                 *tp=dp->replace;
                 *walk_subtrees = 0;
              }
           }
       }
   }
   return NULL_TREE;
}

static void link(ImplicitlyCall *self ,ImplicitlyData *item)
{
   //连到静态的
   n_debug("implicitlycall.c link 00 %s\n",item->origFuncName);
   SelectFunc selectFunc;
   selectFunc.classFunc=NULL;
   selectFunc.sysName=NULL;
   selectFunc.sucessed=0;
   selectFunc.genericQuery=0;
   tree ret=NULL_TREE;
   if(item->isStatic)
      ret=selectedFromStatic(self,item,&selectFunc);
   else
      ret=selecImplicitly(self,item,&selectFunc);
   if(aet_utils_valid_tree(ret)){
      n_debug("implicitlycall.c link 11 找到了函数定义 %s 是不是核函数:%d\n",item->origFuncName,class_func_is_kernel(selectFunc.classFunc));
      //替换隐藏核数不能在walk_tree中进行，见bug 074
      if(class_func_is_kernel(selectFunc.classFunc)){
         tree replace = mtcs_lanuch_replace_call_implicitly(mtcs_parser_get()->mtcsLanuch,
         item->funcDecl,item->loc,item->exprList,item->caller, selectFunc.classFunc,item->callee);
         WalkData data;
         data.item = item;
         data.replace=replace;
         data.selectFunc=&selectFunc;
         data.implicitlyCall = self;
         walk_tree (&DECL_SAVED_TREE (item->caller), kernel_replace_cb, &data, NULL);
      }else{
         WalkData data;
         data.item = item;
         data.replace=ret;
         data.selectFunc=&selectFunc;
         data.implicitlyCall = self;
         walk_tree (&DECL_SAVED_TREE ( item->caller), link_cb, &data, NULL);
      }
   }else{
      //error_at(item->loc,"在类%qs中，没有找到函数%qs的实现。",item->sysName,item->origFuncName);
      //恢复原名调用，由link器查找。
      nboolean isMtcs = func_mgr_is_mtcs_func(func_mgr_get(),item->caller);
      n_debug("start link  没有找到函数定义 恢复原函数调用。%s 所在函数是不是 Mtcs:%d\n",item->origFuncName,isMtcs);
      aet_print_tree(ret);
      tree id=aet_utils_create_ident(item->origFuncName);
      tree type=NULL_TREE;
      tree ret = build_external_ref (item->loc, id,TRUE,&type);
      WalkData data;
      data.item = item;
      data.replace=ret;
      data.selectFunc=&selectFunc;
      data.implicitlyCall = self;
      //item中的参数在selectedFromStatic或selecImplicitly，如果到这里说明已以移走了。
      walk_tree (&DECL_SAVED_TREE ( item->caller), link_cb, &data, NULL);
   }
}

static void freeImplicitlyData(ImplicitlyData *item)
{
   if(item->sysName)
      free(item->sysName);
   if(item->origFuncName)
      free(item->origFuncName);
   if(item->newFuncName)
      free(item->newFuncName);
   if(item->exprList)
      release_tree_vector(item->exprList);
   if(item->origtypes)
      release_tree_vector(item->origtypes);
   item->arg_loc.release();
   n_slice_free(ImplicitlyData,item);
}

/**
 * 链接到定义的函数中
 * 在impl$结尾处调用
 */
void  implicitly_call_link(ImplicitlyCall *self)
{
   int i;
   for(i=0;i<self->implicitlyArray->len;i++){
      ImplicitlyData *item=n_ptr_array_index(self->implicitlyArray,i);
      //test_add_list(item->caller);
      n_debug("implicitlycall.c implicitly_call_link 00 补全类%s中调用的隐藏函数\n",item->sysName);
      link(self,item);
      n_debug("implicitlycall.c implicitly_call_link 11 补全类%s中调用的隐藏函数\n",item->sysName);
      n_ptr_array_remove(self->implicitlyArray,item);
      freeImplicitlyData(item);
      i--;
   }
}

/**
 * 是不是当前类中的标记的隐藏函数
 */
nboolean implicitly_call_have_func(ImplicitlyCall *self,tree func)
{
   c_parser *parser=self->parser->parser;
   ClassParser *classParser =  class_parser_get();
   ClassName *className=NULL;
   //bug 050
   if(!self->parser->isAet && classParser->state!=CLASS_STATE_FIELD)
      return FALSE;
   char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
   ClassImpl *classImpl=class_impl_get();
   className=classImpl->className;
   if(className==NULL)
      className=classParser->currentClassName;
   ImplicitlyData *item=find(self,className->sysName,funcName);
   return (item!=NULL);
}

/**
 * 当在classimpl.c中的class_impl_process_expression方法，找不到函数时，进入这里
 * 不在aet中，或不是直接调用都返回
 */
tree  implicitly_call_call(ImplicitlyCall *self,location_t loc,tree id)
{
   ClassParser *classParser =  class_parser_get();
   c_parser *parser=self->parser->parser;
   if(!self->parser->isAet && classParser->state!=CLASS_STATE_FIELD)
      return NULL_TREE;
   //    printf("dsfsdfxxx %p\n",expr->value);
   //    if(aet_utils_valid_tree(expr->value) && TREE_CODE(expr->value)<MAX_TREE_CODES ){
   //        printf("只有在impl$中直接调用的方法，才能处理。\n");
   //        aet_print_tree(expr->value);
   //        return NULL_TREE;
   //    }
   nboolean builtIn=isBuiltInFunc(id);
   if(builtIn){
      n_debug("implicitly_call_call 00 是内建的。 %s",IDENTIFIER_POINTER(id));
   }else{
      ClassImpl *classImpl=class_impl_get();
      ClassName *className=classImpl->className;
      if(className==NULL && classParser->state==CLASS_STATE_FIELD)
         className=classParser->currentClassName;
      int order=0;
      tree funcDecl=createImplicitlyId(loc,id,&order);
      tree selfTree=lookup_name(aet_utils_create_ident("self"));
      add(self,className->sysName,loc,id,funcDecl,order,selfTree,FALSE);
      n_debug("implicitly_call_call 11 隐式的函数的。 old:%s new:%s\n",IDENTIFIER_POINTER(id),IDENTIFIER_POINTER(DECL_NAME(funcDecl)));
      return funcDecl;
   }
   return NULL_TREE;
}

nboolean  implicitly_call_is_builtin(ImplicitlyCall *self,tree funcdel)
{
   nboolean builtIn=isBuiltInFunc(DECL_NAME(funcdel));
   return builtIn;
}

/**
 * 如果调用的不是本类的方法，不处理。
 * 是与静态函数调用作为约束的。
 */
tree  implicitly_call_call_from_static(ImplicitlyCall *self,struct c_expr *expr,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean onlyStatic)
{
    c_parser *parser=self->parser->parser;
    tree func=expr->value;
    if(!self->parser->isAet)
        return NULL_TREE;
    n_debug("有静态声明的函数，但参数不正确，归于隐藏的调用implicitly\n");
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
        n_debug("有静态声明的函数，但参数不正确，归于隐藏的调用implicitly funcName:%s\n",funcName);
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
        printf("implicitly_call_call_from_static 22 隐式的函数的。 old:%s new:%s 所在类:%s 类方法：%s",
                IDENTIFIER_POINTER(id),IDENTIFIER_POINTER(DECL_NAME(funcDecl)),impl->className->sysName,currentClassName);
        return funcDecl;
    }
    return NULL_TREE;
}

/**
 * 判断是不是一个implicitly调用
 * lhsOrType 是一个函数调用或左值类型
 * rhs是一个右值或一个参数
 */
static nboolean set_impl_conv(ImplicitlyCall *self,tree lhsOrType,tree rhs,
        tree rhsOrigType,char *sysName,enum impl_conv errType,int parmNum)
{
   if(!aet_utils_valid_tree(rhs) || sysName==NULL)
      return FALSE;
   tree callExpr=NULL_TREE;
   if(TREE_CODE(rhs)==CONVERT_EXPR){
      tree ff=TREE_OPERAND (rhs, 0);
      if(TREE_CODE(ff)!=NOP_EXPR)
         return FALSE;
      callExpr=TREE_OPERAND (ff, 0);
      if(TREE_CODE(callExpr)!=CALL_EXPR)//隐藏函数的调用可能还有CONVERT_EXPR
         return FALSE;
   }else if(TREE_CODE(rhs)==CALL_EXPR){
      callExpr=rhs;
   }else{
      if(EXPR_P(rhs)){
         callExpr=TREE_OPERAND (rhs, 0);
         if(TREE_CODE(callExpr)!=CALL_EXPR)
            return FALSE;
      }else{
         n_info("在implicitly_call_set_impl_conv有未处理的类型！\n");
         return FALSE;
      }
   }
   tree fn=CALL_EXPR_FN (callExpr);
   if(TREE_CODE(fn)!=ADDR_EXPR)
      return FALSE;
   tree fndecl=TREE_OPERAND (fn, 0);
   if(TREE_CODE(fndecl)!=FUNCTION_DECL)
      return FALSE;
   char *funName=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   int i;
   for(i=0;i<self->implicitlyArray->len;i++){
      ImplicitlyData *item=n_ptr_array_index(self->implicitlyArray,i);
      if(!strcmp(item->sysName,sysName) && !strcmp(item->newFuncName,funName)){
         item->convertParm.lhsOrType=lhsOrType;
         item->convertParm.implConv=errType;
         item->convertParm.rhsOrigType=rhsOrigType;
         item->convertParm.parmNum=parmNum;
         return TRUE;
      }
   }
   return FALSE;
}

/**
 * 初始化变量和modify表达式，在link时需要转化隐藏函数的返回值类型与lhs的类型相同.
 */
nboolean implicitly_call_set_init_or_modify_impl_conv(ImplicitlyCall *self,tree lhs,
      tree rhs,tree rhsOrigType,char *sysName,nboolean isModify)
{
   return set_impl_conv(self,lhs,rhs,rhsOrigType,sysName,isModify?ic_assign:ic_init,0);
}

nboolean implicitly_call_set_return_impl_conv(ImplicitlyCall *self,tree lhsOrValue,tree rhs,tree rhsOrigType,char *sysName)
{
   return set_impl_conv(self,lhsOrValue,rhs,rhsOrigType,sysName,ic_return,0);
}

/**
 * funcCall 被调用的函数
 * arg 被调用函数的参数
 * pos 参数位置
 */
nboolean implicitly_call_set_funcall_impl_conv(ImplicitlyCall *self,tree funcCall,tree arg,tree argOrigType,int pos,char *sysName)
{
   return set_impl_conv(self,funcCall,arg,argOrigType,sysName,ic_argpass,pos);
}

nboolean  implicitly_call_is_func_decl(ImplicitlyCall *self,tree fundecl)
{
   if(TREE_CODE(fundecl)!=FUNCTION_DECL)
      return FALSE;
   tree id=DECL_NAME(fundecl);
   char *funcName=IDENTIFIER_POINTER(id);
   c_parser *parser=self->parser->parser;
   if(!self->parser->isAet)
      return FALSE;
   ClassImpl *impl=class_impl_get();
   ImplicitlyData *item=find(self,impl->className->sysName,funcName);
   return item!=NULL;
}

/**
 * 跳过内建函数
 */
tree  implicitly_call_call_skip_builtin(ImplicitlyCall *self,location_t loc,tree id)
{
   c_parser *parser=self->parser->parser;
   if(!self->parser->isAet)
      return NULL_TREE;
   ClassImpl *impl=class_impl_get();
   int order=0;
   tree funcDecl=createImplicitlyId(loc,id,&order);
   tree selfTree=lookup_name(aet_utils_create_ident("self"));
   add(self,impl->className->sysName,loc,id,funcDecl,order,selfTree,FALSE);
   n_debug("implicitly_call_call_skip_builtin 11 隐式的函数的。 old:%s new:%s\n",IDENTIFIER_POINTER(id),IDENTIFIER_POINTER(DECL_NAME(funcDecl)));
   return funcDecl;
}

/**
 * 从callee中找出参数并构造vec
 */
void   implicitly_call_set_call(ImplicitlyCall *self,tree callee,vec<location_t> arg_loc)
{
   if(!callee || TREE_CODE(callee)!=CALL_EXPR)
      return;
   tree fndecl= TREE_OPERAND(CALL_EXPR_FN (callee),0);
   ImplicitlyData *item = findByFunctionDecl(self,fndecl);
   if(item==NULL){
      error_at (EXPR_LOCATION (callee),"找不到隐藏函数调用记录！");
   }
   if(self->parser->isAet && mtcs_parser_is_compiling(mtcs_parser_get())){
      n_debug("implicitlycall 在 mtcs函数内\n");
      aet_print_tree(current_function_decl);
   }
   item->callee=callee;
   int i;
   item->arg_loc.safe_push (item->loc);//item->loc是被调函数的位置，相当于加self
   for(i=0;i<arg_loc.length();i++){
      location_t loc=arg_loc[i];
      item->arg_loc.safe_push (loc);
   }
}

ImplicitlyCall *implicitly_call_new()
{
   ImplicitlyCall *self = n_slice_alloc0 (sizeof(ImplicitlyCall));
   implicitlyCallInit(self);
   self->parser=aet_parser_get();
   return self;
}


