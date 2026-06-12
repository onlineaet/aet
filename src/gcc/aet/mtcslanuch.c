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
#include "value-range.h"


#include "basic-block.h"
#include "pass_manager.h"
#include "context.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "gimplify.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "gimple-fold.h"
#include "gimple-expr.h"
#include "tree-iterator.h"
#include "tree-into-ssa.h"
#include "tree-ssa-alias.h"
#include "tree-ssanames.h"
#include "tree-pretty-print.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"


#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "aetinfo.h"
#include "mtcslanuch.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "classmgr.h"
#include "classimpl.h"
#include "classbuild.h"
#include "genericimpl.h"
#include "genericutil.h"
#include "mtcs/mtcstool.h"
#include "aetparser.h"
#include "mtcstypes.h"
#include "mtcsinfo.h"


static NPtrArray *createLanuchParams();



static void mtcsLanuchInit(MtcsLanuch *self)
{
   self->funcLanchParamsArray=n_ptr_array_new();
   self->lanuchFuncDecl_1=NULL_TREE;
}

static tree createDim3Constructor(location_t loc,tree x)
{
   tree ruint3=lookup_name (get_identifier ("dim3"));
   tree type=TREE_TYPE(ruint3);
   tree field;
   vec<constructor_elt, va_gc> *v;
   vec_alloc (v, 3);
   int i=0;
   for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
      if(i==0){
         constructor_elt elt = {field, x};
         v->quick_push (elt);
      }else{
         constructor_elt elt = {field, build_int_cst(unsigned_type_node,1)};
         v->quick_push (elt);
      }
      i++;
   }
   tree init= build_constructor (type, v);
   return init;
}
static int gridBlockCount=0;
static tree createDim3Var(location_t loc,tree x)
{
     char  varName[255];
     sprintf(varName,"_gridblock342_%d",gridBlockCount++);
     tree id=aet_utils_create_ident(varName);
     tree recordId=aet_utils_create_ident("dim3");
     tree dim3Record=lookup_name (recordId);
     tree decl=build_decl (loc, VAR_DECL, id, TREE_TYPE(dim3Record));
     tree init=createDim3Constructor(loc,x);
     DECL_CONTEXT(decl)=current_function_decl;
     TREE_READONLY (decl) = 0;
     DECL_ARTIFICIAL (decl) = 1;
     DECL_INITIAL (decl) = init;
     TREE_USED (decl) = 1;
     DECL_EXTERNAL(decl)=0;
     TREE_STATIC(decl)=0;
     TREE_PUBLIC(decl)=0;
     pushdecl (decl);
     finish_decl(decl,loc,NULL_TREE,NULL_TREE,NULL_TREE);
     return decl;
}

/**
 * 参数
 * 1.grid;
 * 2.block;
 * 3.shareMemSize;
 * 4.streamer;
 */
static  vec<tree, va_gc> * createDefaultParamsList(location_t loc)
{
   vec<tree, va_gc> *exprlist;
   exprlist = make_tree_vector ();
   tree dim3grid=createDim3Var(loc,build_int_cst (unsigned_type_node, 1));
   vec_safe_insert(exprlist,0,dim3grid);
   tree dim3block=createDim3Var(loc,build_int_cst (unsigned_type_node, 1));
   vec_safe_insert(exprlist,1,dim3block);
   tree shareMemSize=build_int_cst (integer_type_node, 0);
   vec_safe_insert(exprlist,2,shareMemSize);
   tree streamer=null_pointer_node;
   vec_safe_insert(exprlist,3,streamer);//void *sm
   return exprlist;
}

/**
 * 解析 <<<Dg,Db, Ns, S>>>中的参数
 *
 *共有14种参数组合
 */
void mtcs_lanuch_parser_launch_param(MtcsLanuch *self,vec<tree, va_gc> **lanuchList)
{
   if(!mtcs_types_is_init(mtcs_types_get())){
      mtcs_types_init(mtcs_types_get());
   }
   tree recordId=aet_utils_create_ident("dim3");
   tree dim3Record=lookup_name (recordId);
   if(dim3Record==NULL){
      n_error("报告此错误 createLanuchParams dim3Record 是空的");
      return NULL;
   }

   c_parser *parser=aet_parser_get()->parser;
   unsigned int literal_zero_mask;
   location_t sizeof_arg_loc[6], comp_loc;
   tree sizeof_arg[6];
   vec<tree, va_gc> *exprlist;
   vec<tree, va_gc> *origtypes = NULL;
   vec<location_t> arg_loc = vNULL;
   c_parser_consume_token (parser);//consume <<
   c_parser_consume_token (parser);//consume <
   location_t loc=c_parser_peek_token (parser)->location;
   exprlist = aet_parser_c_parser_expr_list (aet_parser_get(),true, false, &origtypes,
         sizeof_arg_loc, sizeof_arg, &arg_loc, &literal_zero_mask);
   if(c_parser_peek_token (parser)->type==CPP_CLOSE_PAREN){
      c_parser_consume_token (parser);//consume )
   }else{
      error_at(loc,"应该是)符号。");
   }
   int len=exprlist->length();
   if(len==0){
      //没有参数,创建缺省的参数
      exprlist = createDefaultParamsList(loc);
      *lanuchList=exprlist;
      return;
   }
   tree first=(*exprlist)[0];
   n_debug("create lanuch param len:%d %d %d\n",
         len,TREE_CODE(TREE_TYPE(first)),INTEGER_TYPE,get_tree_code_name(TREE_CODE(TREE_TYPE(first))));
   if(TREE_CODE(TREE_TYPE(first))==INTEGER_TYPE){
      if(!TYPE_UNSIGNED (TREE_TYPE(first))){
         first = build1 (NOP_EXPR, unsigned_type_node, first);
      }
      tree np=createDim3Var(arg_loc[0],first);
      (*exprlist)[0]=np;
   }else if(TREE_TYPE(first)!=TREE_TYPE(dim3Record)){
      error_at(arg_loc[0],"启动参数的第1个实参类型不兼容%qT",TREE_TYPE(first));
      return;
   }
   if(len==1){
      tree second=createDim3Var(arg_loc[0],build_int_cst(unsigned_type_node,1));
      vec_safe_insert(exprlist,1,second);//void *sm
      vec_safe_insert(exprlist,2,build_int_cst (integer_type_node, 0));
      vec_safe_insert(exprlist,3,null_pointer_node);//void *sm
      *lanuchList=exprlist;
      return;
   }
   first=(*exprlist)[1];
   if(TREE_CODE(TREE_TYPE(first))==INTEGER_TYPE){
      if(!TYPE_UNSIGNED (TREE_TYPE(first))){
          first = build1 (NOP_EXPR, unsigned_type_node, first);
      }
      tree np=createDim3Var(arg_loc[1],first);
      (*exprlist)[1]=np;
   }else if(TREE_TYPE(first)!=TREE_TYPE(dim3Record)){
      error_at(arg_loc[1],"启动参数的第2个实参类型不兼容%qT",TREE_TYPE(first));
      return;
   }
   if(len==2){
      vec_safe_insert(exprlist,2,build_int_cst (integer_type_node, 0));
      vec_safe_insert(exprlist,3,null_pointer_node);//void *sm
      *lanuchList=exprlist;
      return;
   }
   first=(*exprlist)[2];
   if(TREE_CODE(TREE_TYPE(first))!=INTEGER_TYPE){
      error_at(arg_loc[2],"启动参数的第3个实参类型不兼容%qT",TREE_TYPE(first));
      return;
   }else{
      if(!TYPE_UNSIGNED (TREE_TYPE(first))){
          first = build1 (NOP_EXPR, unsigned_type_node, first);
          (*exprlist)[2]=first;
      }
   }
   if(len==3){
      vec_safe_insert(exprlist,3,null_pointer_node);//void *sm
      *lanuchList=exprlist;
      return;
   }
   first=(*exprlist)[3];
   if(TREE_CODE(TREE_TYPE(first))!=POINTER_TYPE){
      error_at(arg_loc[3],"启动参数的第4个实参类型不兼容%qT",TREE_TYPE(first));
      return;
   }
   if(len>4){
      error_at(arg_loc[4],"启动参数个数不匹配。");
   }
   *lanuchList=exprlist;
}

typedef struct _LanchParamsData
{
   location_t loc;
   tree funcDeclOrRef;
   tree call;//调用类中的某个方法
   vec<tree, va_gc> *lanchParams;
}LanchParamsData;

static char *getCallName(tree funcDeclOrRef)
{
   if(TREE_CODE(funcDeclOrRef)==FUNCTION_DECL)
      return IDENTIFIER_POINTER(DECL_NAME(funcDeclOrRef));
   else if(TREE_CODE(funcDeclOrRef)==COMPONENT_REF){
      tree op0=TREE_OPERAND (funcDeclOrRef, 0);
      tree op1=TREE_OPERAND (funcDeclOrRef, 1);
      return IDENTIFIER_POINTER(DECL_NAME(op1));
   }
   return NULL;
}

static int kernelArgsCount = 0;

/**
 * setData(3.3)
 * 3.3 变成 float constArg_xxx = 3.3;
 * 调用核函数传的实参是常数，需要转为地址
 * finish_decl 相当于decl变成decl_expr然后通过add_stmt加入到当前函数语句列表中。
 */
static tree createTemptVar(location_t loc,tree arg)
{
   char  varName[255];
   sprintf(varName,"non_lvalue_Arg_%d",kernelArgsCount++);
   tree id=aet_utils_create_ident(varName);
   tree decl=build_decl (loc, VAR_DECL, id, TREE_TYPE(arg));
   DECL_CONTEXT(decl)=current_function_decl;
   TREE_READONLY (decl) = 0;
   DECL_ARTIFICIAL (decl) = 1;
   DECL_INITIAL (decl) = arg;
   TREE_USED (decl) = 1;
   DECL_EXTERNAL(decl)=0;
   TREE_STATIC(decl)=0;
   TREE_PUBLIC(decl)=0;
   pushdecl (decl);
   finish_decl(decl,loc,NULL_TREE,NULL_TREE,NULL_TREE);
   return decl;
}

/**
 * 创建参数 void *kernelArgs_x[count]
 * finish_decl 相当于decl变成decl_expr然后通过add_stmt加入到当前函数语句列表中。
 */
static tree createVoidPointerArray(location_t loc,tree call)
{
   int count = 0;
   tree arg;
   call_expr_arg_iterator iter;
   FOR_EACH_CALL_EXPR_ARG (arg, iter, call)
      count++;
   tree dataType=build_pointer_type(void_type_node);
   tree type = build_array_type (dataType,build_index_type (size_int (count)));
   char kernelVarName[255];
   sprintf(kernelVarName,"kernelArgs_%d",kernelArgsCount++);
   tree id=aet_utils_create_ident(kernelVarName);
   tree decl=build_decl (loc, VAR_DECL, id, type);
   DECL_CONTEXT(decl)=current_function_decl;
   TREE_READONLY (decl) = 0;
   DECL_ARTIFICIAL (decl) = 1;
   DECL_INITIAL (decl) = NULL_TREE;
   TREE_USED (decl) = 1;
   DECL_EXTERNAL(decl)=0;
   TREE_STATIC(decl)=0;
   TREE_PUBLIC(decl)=0;
   pushdecl (decl);
   finish_decl(decl,loc,NULL_TREE,NULL_TREE,NULL_TREE);
   count=0;
   n_debug("mtcslanuch.c 创建参数  void *kernels[2] -----:%p\n",decl);
   FOR_EACH_CALL_EXPR_ARG (arg, iter, call){
      tree refIndex0=build_int_cst(integer_type_node,count);
      tree lhs = build4 (ARRAY_REF, TREE_TYPE(TREE_TYPE(decl)), decl, refIndex0, NULL_TREE,NULL_TREE);
      n_debug("mtcslanuch.c 参数的数据类型:%s\n",get_tree_code_name(TREE_CODE(TREE_TYPE(arg))));
      tree argType=TREE_TYPE(arg);
      aet_print_tree(arg);
      if(!POINTER_TYPE_P(argType)){
         n_debug("mtcslanuch.c 不是指针参数---- count:%d lvalue_p (arg)：%d\n",count,lvalue_p (arg));
         if(!lvalue_p (arg)){
            arg=createTemptVar(0,arg);
         }
         arg = build_unary_op (0, ADDR_EXPR, arg, FALSE);//转指针
      }else if(POINTER_TYPE_P(argType)){
         n_debug("mtcslanuch.c  createVoidPointerArray 00\n");
         arg = build1 (ADDR_EXPR, build_pointer_type (argType), arg);
      }else{
         n_debug("mtcslanuch.c  createVoidPointerArray 11\n");
         arg = build_unary_op (0, ADDR_EXPR, arg, FALSE);//转指针

      }
      tree rhs = arg;
      tree modifyVarDecl= build2 (MODIFY_EXPR, TREE_TYPE(TREE_TYPE(decl)), lhs, rhs);
      add_stmt(modifyVarDecl);
      count++;
   }

   /* 创建void **kernelParams;
      等同于
      struct c_expr expr;
      expr.value=NULL_TREE;
      set_c_expr_source_range (&expr, loc, loc);
      expr.value=decl;
      expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
      printf("first----33dfsfdf\n");
      aet_print_tree(expr.value);
      return expr.value;
   */
   tree ppType=build_pointer_type(dataType);
   tree addrExpr= build_unary_op (loc, ADDR_EXPR, decl, false);
   tree kernelParams = build1 (NOP_EXPR, ppType, addrExpr);
   return kernelParams;
}


/**
 * 创建内核函数名实参
  * MtcsSystem.lanuch(char *provider,int deviceNum,char *funcName,....
  * funcName 第三个参数
 * 为什么createKernrlFuncName00 出错?,出错发生在gimple阶段
 */
static tree createKernrlFuncNameActualParam(location_t loc,char *funcName)
{
   size_t length=strlen(funcName);
   tree strCst = build_string (length, (const char *) funcName);
   tree type = build_array_type (char_type_node,build_index_type (size_int (length)));
   TREE_TYPE(strCst)=type;
   struct c_expr expr;
   expr.value=strCst;
   expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
   set_c_expr_source_range (&expr, loc, loc);
   return expr.value;
}

/**
 * 查找启动函数 MtcsSystem.lanuch
 */
static tree getLanuchFunction(MtcsLanuch *self,location_t loc,
      vec<tree, va_gc> *lanuchParamsList,vec<tree, va_gc> *origtypes,vec<location_t> arg_loc)
{
   if(self->lanuchFuncDecl_1)
      return self->lanuchFuncDecl_1;

   ClassImpl *classImpl = class_impl_get();
   ClassName *className=class_mgr_get_class_name_by_user(class_mgr_get(),"MtcsSystem");
   if(!className){
      error_at(loc,"调用核函数需要引入头文件'MtcsSystem.h'");
      return NULL_TREE;
   }

   FuncPointerError *errors =NULL;
   SelectFunc selectFunc;
   memset(&selectFunc,0,sizeof(SelectFunc));
   tree component=aet_utils_create_ident("lanuch");
   char *fun=IDENTIFIER_POINTER(component);
   tree  name= aet_utils_create_temp_func_name(className->sysName,fun);
   tree funcDecl = build_decl (0, FUNCTION_DECL, name, default_function_type);
   TREE_STATIC(funcDecl)=1;
   DECL_ARTIFICIAL (funcDecl) = 1;
   tree last =func_call_static_select(classImpl->funcCall,funcDecl,lanuchParamsList,origtypes,arg_loc,loc,&errors,&selectFunc);
   self->lanuchFuncDecl_1 = last;
   return last;
}


static tree convertCall(location_t loc,tree call)
{
   if(TREE_CODE(call)!=CALL_EXPR){
      error_at(loc,"未支持的类型%qE",call);
      return NULL_TREE;
   }
   tree fn = CALL_EXPR_FN (call);
   if(TREE_CODE(fn)==ADDR_EXPR){
      tree fndecl=TREE_OPERAND(fn,0);
      if(TREE_CODE(fndecl)==FUNCTION_DECL){
         char *funcName = IDENTIFIER_POINTER(DECL_NAME(fndecl));
         tree actualFnName = createKernrlFuncNameActualParam(loc,funcName);
         return actualFnName;
      }else{
         error_at(loc,"ADDR_EXPR 未支持的类型%qE。",call);
      }
   }else if(TREE_CODE(fn)==COMPONENT_REF){
      tree op0=TREE_OPERAND(fn,0);
      tree fielddecl=TREE_OPERAND(fn,1);
      char *funcName = IDENTIFIER_POINTER(DECL_NAME(fielddecl));
      if(TREE_CODE(op0)==INDIRECT_REF){
         op0=TREE_OPERAND(op0,0);
      }
      if(!op0){
         tree actualFnName = createKernrlFuncNameActualParam(loc,funcName);
         return actualFnName;
      }else{
         // 匹配 op0就是tcs
         //TFirst *tcs=new$ TFirst();
         //tcs->setdata(3.2);
         if(VAR_P(op0)){
            tree actualFnName = createKernrlFuncNameActualParam(loc,funcName);
            return actualFnName;
         }
         return fn;
      }
   }else if(TREE_CODE(fn)==CONVERT_EXPR){
      //来自 supercall中的createExpr_new ( ( setData )  _AObject_parent_superFuncAddressArray[1] )
      tree ref=TREE_OPERAND(fn,0);
      if(TREE_CODE(ref)==INDIRECT_REF){
         tree op0=TREE_OPERAND(ref,0);
         if(TREE_CODE(op0)==POINTER_PLUS_EXPR){
            op0=TREE_OPERAND(op0,0);
            if(VAR_P(op0) && DECL_NAME(op0)
                    && super_call_valid_mtcs_parent_super_call_var_name(IDENTIFIER_POINTER(DECL_NAME(op0)))){
                 return ref;
            }
         }
      }
      error_at(loc,"未支持的类型 CONVERT_EXPR 不是super调用。%qE",call);
      return NULL_TREE;
   }else if(TREE_CODE(fn)==NOP_EXPR){
      tree ref=TREE_OPERAND(fn,0);
      if(TREE_CODE(ref)==INDIRECT_REF){
         tree op0=TREE_OPERAND(ref,0);
         if(TREE_CODE(op0)==POINTER_PLUS_EXPR){
            op0=TREE_OPERAND(op0,0);
            if(VAR_P(op0) && DECL_NAME(op0)
                  && super_call_valid_mtcs_parent_super_call_var_name(IDENTIFIER_POINTER(DECL_NAME(op0)))){
               return ref;
            }
         }
      }
      error_at(loc,"fn:NOP_EXPR 有未支持的类型%qE ",call);
   }else{
      error_at(loc,"未支持的类型%qE",call);
      return NULL_TREE;
   }
}

/**
 * 调用函数是不是类非静态函数
 */
static nboolean inAetFunction(MtcsLanuch *self)
{
   AetParser *parser=aet_parser_get();
   if(parser->isAet){
      tree current=current_function_decl;
      ClassFunc    *func=func_mgr_get_func(func_mgr_get(),current);
      if(func){
         //说明在类函数中，并且不是静态函数
        return TRUE;
      }
   }
   return FALSE;
}

/**
 * 替换call为启动函数 MtcsSystem.lanuch
 * 原型:
 * public$  static void lanuch(AObject *inClassObj,int staticFunc,char *funcName,dim3 grid,dim3 block,
 *        auint sharedMemBytes,void *hStream,void **kernelParams,void **extra);
 * loc call的开始位置
 */
tree mtcs_lanuch_replace_call(MtcsLanuch *self,tree earlyFuncDeclOrRef,location_t loc,tree call,ClassFunc *callee)
{
   int i;
   int len=self->funcLanchParamsArray->len;
   nboolean find=FALSE;
   LanchParamsData *item;
   for(i=0;i<len;i++){
      item=n_ptr_array_index(self->funcLanchParamsArray,i);
      if(item->funcDeclOrRef==earlyFuncDeclOrRef /*&& DECL_SOURCE_LOCAITON(item->funcdecl) == DECL_SOURCE_LOCAITON(funcdecl)*/){
         n_debug("mtcs_lanuch_replace_call --- %p funcdecl:%p\n",call,earlyFuncDeclOrRef);
         find=TRUE;
         break;
      }
   }
   vec<tree, va_gc> *lanuchParamsList;
   if(find){
      n_debug("找到了调用核函数的启动参数 ,生成最终启动核数的调用.%s\n",getCallName(earlyFuncDeclOrRef));
      lanuchParamsList = item->lanchParams;
   }else{
      n_info("源代码中没有<<<>>>标签，构建缺省的启动参数.%s\n",getCallName(earlyFuncDeclOrRef));
      lanuchParamsList = createDefaultParamsList(loc);
   }
   ClassInfo *rootInfo=class_mgr_get_root(class_mgr_get());
   tree aobjectType=TREE_TYPE(rootInfo->recordTypeDecl);
   tree aobjectPointerType=build_pointer_type(aobjectType);

   if(inAetFunction(self)){
      tree selftree=lookup_name(get_identifier("self"));
      tree firstParm = build1 (NOP_EXPR, aobjectPointerType, selftree);//转成(AObject*)value
      vec_safe_insert(lanuchParamsList,0,firstParm);
   }else{
      vec_safe_insert(lanuchParamsList,0,null_pointer_node);
   }
   vec_safe_insert(lanuchParamsList,1,
            build_int_cst(integer_type_node,class_func_is_static(callee)?1:0));
   tree actualFnName=convertCall(loc,call);
   vec_safe_insert(lanuchParamsList,2,actualFnName);
   tree kernelArgs = createVoidPointerArray(loc,call);
   vec_safe_insert(lanuchParamsList,7,kernelArgs);
   tree extra=null_pointer_node;
   vec_safe_insert(lanuchParamsList,8,extra);

   vec<location_t> arg_loc =vNULL;
   for(i=0;i<9;i++)
      arg_loc.safe_push (loc);
   vec<tree, va_gc> *origtypes;
   vec_alloc (origtypes, 9);
   //1-9参数类型
   origtypes->quick_push (aobjectPointerType);
   origtypes->quick_push (integer_type_node);
   origtypes->quick_push (build_pointer_type(char_type_node));
   tree recordId=aet_utils_create_ident("dim3");
   tree dim3Record=lookup_name (recordId);
   if(dim3Record==NULL){
      n_error("报告此错误 createLanuchParams dim3Record 是空的");
      return NULL_TREE;
   }
   origtypes->quick_push (TREE_TYPE(dim3Record));
   origtypes->quick_push (TREE_TYPE(dim3Record));
   origtypes->quick_push (unsigned_type_node); //auint sharedMemBytes
   origtypes->quick_push (build_pointer_type(void_type_node)); //void *hStream
   origtypes->quick_push (build_pointer_type(build_pointer_type(void_type_node)));//void **kernelParams
   origtypes->quick_push (build_pointer_type(build_pointer_type(void_type_node))); //void **extra

   //获取的是类MtcsSystem中的lanuch函数 Z21debug_mtcs_MtcsSystem6lanuchEPcDiPcDiDiDiDiDiDiDiPvPPvPPv
   tree funcDecl=getLanuchFunction(self,loc,lanuchParamsList,origtypes,arg_loc);
   //如果 funcDecl是空的，说明出错了 由c-parser.c处理 error_mark_node
   if(funcDecl==NULL_TREE){
      fatal_error(loc,"核函数的启动函数没找到，报告此错误！");
      return error_mark_node;
   }
   tree ret=c_build_function_call_vec (loc, arg_loc, funcDecl, lanuchParamsList, origtypes);
   arg_loc.release();
   vec_free (origtypes);
   return ret;
}

/**
 * earlyFuncDeclOrRef是不是记录在funcLanchParamsArray的MTCS函数调用
 */
nboolean mtcs_lanuch_exists_earlycall(MtcsLanuch *self,tree earlyFuncDeclOrRef)
{
   int i;
   int len=self->funcLanchParamsArray->len;
   for(i=0;i<len;i++){
      LanchParamsData *item=n_ptr_array_index(self->funcLanchParamsArray,i);
      if(item->funcDeclOrRef==earlyFuncDeclOrRef){
         printf("mtcs_lanuch_exists_earlycall ---  funcdecl:%p %s\n",earlyFuncDeclOrRef,getCallName(earlyFuncDeclOrRef));
         return TRUE;
      }
   }
   return FALSE;
}

/*
 * 缓存调用的函数和启动参数，调用该方法是因为用户代码中写入了启动参数，如<<<1,1>>>
*/
void mtcs_lanuch_add_func_and_lanch_params(MtcsLanuch *self,location_t loc,tree funcDeclOrRef, vec<tree, va_gc> *lanchParams)
{
   LanchParamsData *p=n_slice_new(LanchParamsData);
   p->loc = loc;
   p->funcDeclOrRef=funcDeclOrRef;
   p->lanchParams = lanchParams;
   p->call= NULL_TREE;
   n_ptr_array_add( self->funcLanchParamsArray,p);
}

/**********以下是在AET结束，GIMPLE没开始前替换隐藏核函数的方法*****/

typedef struct _FindBindAndList
{
    tree targetstatement;//需查找的目标
    tree bindExpr;//目标所在bind
    tree statementList;//目标所在statementlist
}FindBindAndList;

// 修改回调函数，避免重复处理
static tree print_statement_cb (tree *tp, int *walk_subtrees, void *data)
{
   FindBindAndList *dp = (FindBindAndList *)data;
   tree t = *tp;

   // 如果已经找到目标，不再处理
   if (dp->bindExpr || dp->statementList) {
      *walk_subtrees = 0;
      return NULL_TREE;
   }

   if (TYPE_P (t)) {
      *walk_subtrees = 0;
      return NULL_TREE;
   }

   // 打印当前节点信息（调试用）
   n_debug("Visiting node: %p, code: %s\n",
          (void*)t, get_tree_code_name(TREE_CODE(t)));

   if (TREE_CODE (t) == BIND_EXPR){
      tree vars = TREE_OPERAND (t, 0);
      tree body = TREE_OPERAND (t, 1);

      n_debug("Found BIND_EXPR: %p, body: %p\n", (void*)t, (void*)body);
      n_debug("Target: %p\n", (void*)dp->targetstatement);

      // 情况1: body就是目标语句
      if (body == dp->targetstatement){
         n_debug("Body is target statement %p bindExpr:%p statementList:NULL\n",dp->targetstatement,t);
         dp->bindExpr = t;
         dp->statementList = NULL_TREE;
         *walk_subtrees = 0;  // 不继续遍历
         return NULL_TREE;
      }
      // 情况2: body是STATEMENT_LIST
      else if (TREE_CODE(body) == STATEMENT_LIST){
         tree_stmt_iterator tsi;
         int i=0;
         n_debug("Checking statement_list\n");
         for (tsi = tsi_start(body); !tsi_end_p(tsi); tsi_next(&tsi)){
            tree stmt = tsi_stmt(tsi);
            n_debug("  Statement %d: %p, code: %s\n",
                   i, (void*)stmt, get_tree_code_name(TREE_CODE(stmt)));
            if(stmt == dp->targetstatement){
               n_debug("Found target in statement_list at position %d  target statement %p bindExpr:%p statementList:p\n",
                     i,dp->targetstatement,t,body);
               dp->bindExpr = t;
               dp->statementList = body;
               *walk_subtrees = 0;  // 不继续遍历
               return NULL_TREE;
            }
            i++;
         }

         // 如果在statement_list中没找到，继续遍历
         n_debug("Target not found in top-level statements, continuing traversal...\n");
         *walk_subtrees = 1;  // 继续遍历body
         return NULL_TREE;
      }
      // 情况3: body是其他表达式，继续遍历
      else {
         n_debug("Body is not statement_list, continuing traversal...\n");
         *walk_subtrees = 1;  // 继续遍历body
         return NULL_TREE;
      }
   }
   // 检查目标语句
   else if (t == dp->targetstatement) {
      n_debug("Found target statement directly: %p\n", (void*)t);
      // 这里需要记录父节点信息
      // 但我们不知道父节点是什么
      *walk_subtrees = 0;
      return NULL_TREE;
   }
   // 默认情况下，继续遍历
   *walk_subtrees = 1;
   return NULL_TREE;
}

static void printblock(tree block)
{
   if (block) {
      fprintf(stderr, "\n==== DEBUG BLOCK DETAILS ====\n");
      aet_print_tree(block);

      // 打印该 BLOCK 下的所有变量名，确认链接顺序
      fprintf(stderr, "Block Vars Chain: ");
      for (tree v = BLOCK_VARS(block); v; v = DECL_CHAIN(v)) {
         if (DECL_NAME(v))
            fprintf(stderr, "%s (%p) -> ", IDENTIFIER_POINTER(DECL_NAME(v)), (void*)v);
         else
            fprintf(stderr, "<unnamed> (%p) -> ", (void*)v);
      }
      fprintf(stderr, "NULL\n");
   } else {
      fprintf(stderr, "\n[WARNING]: This BIND_EXPR has NO BLOCK!\n");
   }
   fprintf(stderr, "==== DEBUG BIND_EXPR END ====\n\n");
}

/**
 * 保证变量DECL_EXPR和赋值语句在statement_list的顺序
 */
static void insertStmt(tree stmtList,tree find ,tree newStmt,nboolean before)
{
   tree_stmt_iterator tsi;
   for (tsi = tsi_start(stmtList); !tsi_end_p(tsi);  tsi_next(&tsi)){
      tree stmt = tsi_stmt(tsi);
      if(stmt==find){
         if(before)
            tsi_link_before(&tsi, newStmt, TSI_SAME_STMT);
         else
            tsi_link_after(&tsi, newStmt, TSI_SAME_STMT);
      }
   }
}

static tree createTemptVar_implicitly(location_t loc,tree caller,tree arg)
{
   char  varName[255];
   sprintf(varName,"non_lvalue_Arg_%d",kernelArgsCount++);
   tree id=aet_utils_create_ident(varName);
   tree decl=build_decl (loc, VAR_DECL, id, TREE_TYPE(arg));
   DECL_CONTEXT(decl)=caller;
   TREE_READONLY (decl) = 0;
   DECL_ARTIFICIAL (decl) = 1;
   DECL_INITIAL (decl) = arg;
   TREE_USED (decl) = 1;
   DECL_EXTERNAL(decl)=0;
   TREE_STATIC(decl)=0;
   TREE_PUBLIC(decl)=0;

   return decl;
}

static tree createDim3Var_implicitly(location_t loc,tree x,tree caller,tree impliciCallee)
{
   char  varName[255];
   sprintf(varName,"_gridblock342_%d",gridBlockCount++);
   tree id=aet_utils_create_ident(varName);
   tree recordId=aet_utils_create_ident("dim3");
   tree dim3Record=lookup_name (recordId);
   tree decl=build_decl (loc, VAR_DECL, id, TREE_TYPE(dim3Record));
   TREE_READONLY (decl) = 0;
   DECL_ARTIFICIAL (decl) = 1;
   TREE_USED (decl) = 1;
   DECL_EXTERNAL(decl)=0;
   TREE_STATIC(decl)=0;
   TREE_PUBLIC(decl)=0;
   DECL_CONTEXT(decl) = caller;
   TREE_SIDE_EFFECTS(decl) = 1;
   DECL_SEEN_IN_BIND_EXPR_P(decl) = 1;

   printf("createDim3Var_implicitly 00 调用函数:%p callee:%p\n",caller,impliciCallee);
   aet_print_tree(caller);
   FindBindAndList data={impliciCallee,NULL_TREE,NULL_TREE};
   walk_tree_without_duplicates (&DECL_SAVED_TREE (caller), print_statement_cb, &data);
   if(!data.bindExpr){
      n_error("函数定义没有bindExpr，错误。\n");
      return NULL_TREE;
   }

   if(!data.statementList){
      tree old = BIND_EXPR_BODY(data.bindExpr);
      tree stmtList = alloc_stmt_list();
      tree stmt0 = build_stmt (loc, DECL_EXPR, decl);
      append_to_statement_list (stmt0, &stmtList);
      printf("新建 stmtList ---%p\n",stmtList);
      tree type=TREE_TYPE(dim3Record);
      tree field;
      int i=0;
      for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
         tree ref = build3(COMPONENT_REF, TREE_TYPE(field), decl, field, NULL_TREE);
         tree val = (i == 0) ? fold_convert(TREE_TYPE(field), x) : build_int_cst(TREE_TYPE(field), 1);
         tree modifyVarDecl= build2 (MODIFY_EXPR, TREE_TYPE(field), ref, val);
         append_to_statement_list (modifyVarDecl, &stmtList);
         i++;
      }
      append_to_statement_list (old, &stmtList);
      BIND_EXPR_BODY(data.bindExpr) = stmtList;
   }else{
      printf("用原来的 stmtList ---%p\n",data.statementList);
      tree block=BIND_EXPR_BLOCK(data.bindExpr);
      tree find=impliciCallee;
      tree stmt0 = build_stmt (loc, DECL_EXPR, decl);
      insertStmt(data.statementList,find,stmt0,TRUE);
      find=stmt0;
      tree type=TREE_TYPE(dim3Record);
      tree field;
      int i=0;
      for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
         tree ref = build3(COMPONENT_REF, TREE_TYPE(field), decl, field, NULL_TREE);
         tree val = (i == 0) ? fold_convert(TREE_TYPE(field), x) : build_int_cst(TREE_TYPE(field), 1);
         tree modifyVarDecl= build2 (MODIFY_EXPR, TREE_TYPE(field), ref, val);
         insertStmt(data.statementList,find,modifyVarDecl,FALSE);
         find = modifyVarDecl;
         i++;
      }
   }

   tree old_vars = BIND_EXPR_VARS(data.bindExpr);
   DECL_CHAIN(decl) = old_vars;
   BIND_EXPR_VARS(data.bindExpr) = decl;
   printf("createDim3Var_implicitly 22 调用函数:%p callee:%p\n",caller,impliciCallee);
   aet_print_tree(caller);
   return decl;
}

static tree createDim3Var_implicitly_ok(location_t loc, tree x, tree caller, tree impliciCallee)
{
    static unsigned int gridBlockCount = 0;

    char varName[255];
    sprintf(varName, "_gridblock342_%d", gridBlockCount++);
    tree id = aet_utils_create_ident(varName);
    tree recordId = aet_utils_create_ident("dim3");
    tree dim3Record = lookup_name(recordId);

    if (!dim3Record || TREE_CODE(dim3Record) != TYPE_DECL) {
        n_error("无法找到 dim3 类型！\n");
        return NULL_TREE;
    }

    tree decl = build_decl(loc, VAR_DECL, id, TREE_TYPE(dim3Record));
    DECL_ARTIFICIAL(decl) = 1;
    TREE_USED(decl) = 1;
    DECL_EXTERNAL(decl) = 0;
    TREE_STATIC(decl) = 0;
    TREE_PUBLIC(decl) = 0;
    DECL_CONTEXT(decl) = caller;
    DECL_SOURCE_LOCATION(decl) = loc;
    DECL_SEEN_IN_BIND_EXPR_P(decl) = 1;
    TREE_ADDRESSABLE (decl) = 1;
    TREE_SIDE_EFFECTS(decl) = 1;

    printf("createDim3Var_implicitly 00 加变量前函数定义乌 调用函数:%p callee:%p\n",caller,impliciCallee);
      aet_print_tree(caller);


    layout_decl(decl, 0);

    // 查找 BIND_EXPR
    FindBindAndList data = {impliciCallee, NULL_TREE, NULL_TREE};
    walk_tree_without_duplicates(&DECL_SAVED_TREE(caller), print_statement_cb, &data);

    if (!data.bindExpr) {
        n_error("没有找到 bindExpr！\n");
        return NULL_TREE;
    }

    tree bind = data.bindExpr;

    tree block = BIND_EXPR_BLOCK(bind);
    printf("print block---\n");
    print_generic_stmt(stderr, block, 0);
    if (!block || TREE_CODE(block) != BLOCK) {
        block = make_node(BLOCK);
        BLOCK_SUPERCONTEXT(block) = caller;
        BIND_EXPR_BLOCK(bind) = block;
    }

    // 防重复
    for (tree v = BIND_EXPR_VARS(bind); v; v = DECL_CHAIN(v)) {
        if (DECL_NAME(v) && strcmp(IDENTIFIER_POINTER(DECL_NAME(v)), varName) == 0)
            return v;
    }

    // 插入变量（头部）
    DECL_CHAIN(decl) = BIND_EXPR_VARS(bind);
    BIND_EXPR_VARS(bind) = decl;
    if (block)
        BLOCK_VARS(block) = BIND_EXPR_VARS(bind);   // 同步 BLOCK

    // ====================== 强制确保有 statement_list ======================
    tree body = BIND_EXPR_BODY(bind);

    if (body == NULL_TREE || TREE_CODE(body) != STATEMENT_LIST) {
        tree new_list = alloc_stmt_list();
        TREE_SIDE_EFFECTS(new_list) = 1;
        TREE_SIDE_EFFECTS(data.bindExpr) = 1;

        if (body)
            append_to_statement_list(body, &new_list);
        BIND_EXPR_BODY(bind) = new_list;
        body = new_list;
    }

    // ====================== 插入语句（在 impliciCallee 之前） ======================
    // 1. DECL_EXPR
    tree decl_expr = build_stmt(loc, DECL_EXPR, decl);
    insertStmt(body, impliciCallee, decl_expr, TRUE);

    // 2. 初始化三个字段
    tree last = decl_expr;
    tree dim3_type = TREE_TYPE(dim3Record);
    int i = 0;

    for (tree field = TYPE_FIELDS(dim3_type); field; field = DECL_CHAIN(field)) {
        tree ref = build3(COMPONENT_REF, TREE_TYPE(field), decl, field, NULL_TREE);
        tree val = (i == 0) ? fold_convert(TREE_TYPE(field), x)
                            : build_int_cst(TREE_TYPE(field), 1);

        tree modify_stmt = build2(MODIFY_EXPR, void_type_node, ref, val);
        TREE_SIDE_EFFECTS(modify_stmt) = 1;

        insertStmt(body, last, modify_stmt, FALSE);
        last = modify_stmt;
        i++;
    }

    // 收尾
    if (DECL_INITIAL(caller) == NULL_TREE || DECL_INITIAL(caller) == error_mark_node)
        DECL_INITIAL(caller) = block;

    if (block)
        BLOCK_SUPERCONTEXT(block) = caller;

    printf("=== createDim3Var_implicitly 完成: %s (已插入到现有 statement_list) ===\n", varName);
    print_generic_stmt(stderr, block, 0);

    printf("createDim3Var_implicitly 00 加变量后函数定义乌 调用函数:%p callee:%p\n",caller,impliciCallee);
         aet_print_tree(caller);
    return decl;
}

static vec<tree, va_gc> * createParamList_implicitly(MtcsLanuch *self,location_t loc,
      tree earlyFuncDeclOrRef,tree caller,tree implicitlyCallee)
{
   int i;
   int len=self->funcLanchParamsArray->len;
   nboolean find=FALSE;
   LanchParamsData *item;
   for(i=0;i<len;i++){
      item=n_ptr_array_index(self->funcLanchParamsArray,i);
      if(item->funcDeclOrRef==earlyFuncDeclOrRef /*&& DECL_SOURCE_LOCAITON(item->funcdecl) == DECL_SOURCE_LOCAITON(funcdecl)*/){
         n_debug("createParamList --- funcdecl:%p\n",earlyFuncDeclOrRef);
         find=TRUE;
         break;
      }
   }
   vec<tree, va_gc> *lanuchParamsList;
   if(find){
      n_debug("找到了调用核函数的启动参数 ,生成最终启动核数的调用.%s\n",getCallName(earlyFuncDeclOrRef));
      lanuchParamsList = item->lanchParams;
   }else{
      n_info("源代码中没有<<<>>>标签，构建缺省的启动参数.%s\n",getCallName(earlyFuncDeclOrRef));
      vec<tree, va_gc> *exprlist;
      exprlist = make_tree_vector ();
      tree dim3grid=createDim3Var_implicitly(loc,build_int_cst (unsigned_type_node, 1),caller,implicitlyCallee);
      vec_safe_insert(exprlist,0,dim3grid);
      tree dim3block=createDim3Var_implicitly(loc,build_int_cst (unsigned_type_node, 1),caller,implicitlyCallee);
      vec_safe_insert(exprlist,1,dim3block);
      tree shareMemSize=build_int_cst (integer_type_node, 0);
      vec_safe_insert(exprlist,2,shareMemSize);
      tree streamer=null_pointer_node;
      vec_safe_insert(exprlist,3,streamer);//void *sm
      lanuchParamsList = exprlist;
   }
   return lanuchParamsList;
}

/**
 * 创建核函数的参数
 */
static tree createVoidPointerArray_implicitly(location_t loc,vec<tree, va_gc> *exprlist,tree caller,tree impliciCallee)
{
   char kernelVarName[255];
   sprintf(kernelVarName,"kernelArgs_%d",kernelArgsCount++);
   tree id=aet_utils_create_ident(kernelVarName);
   /* 2. 创建数组类型： void * kernelArgs_[exprlist->length()] */
   tree void_ptr_type = build_pointer_type(void_type_node);
   tree index_type = build_index_type(build_int_cst(integer_type_node, exprlist->length()));
   tree array_type = build_array_type(void_ptr_type, index_type);

   /* 3. 创建新的局部变量 VAR_DECL： void *kernelArgs_[] */
   tree decl = build_decl(loc, VAR_DECL,id, array_type);

   DECL_CONTEXT(decl)     = caller;      // 属于当前函数
   DECL_ARTIFICIAL(decl)  = 1;           // 编译器生成
   DECL_IGNORED_P(decl)   = 1;           // 不生成调试信息（可选）
   TREE_USED(decl)        = 1;
   TREE_STATIC(decl)      = 0;           // 自动局部变量
   TREE_PUBLIC(decl)      = 0;

   TREE_READONLY (decl) = 0;
   DECL_INITIAL (decl) = NULL_TREE;
   TREE_ADDRESSABLE(decl) = 1;
   TREE_SIDE_EFFECTS(decl) = 1;

   FindBindAndList data={impliciCallee,NULL_TREE,NULL_TREE};
   walk_tree_without_duplicates (&DECL_SAVED_TREE (caller), print_statement_cb, &data);
   if(!data.bindExpr){
      n_error("函数定义没有bindExpr，错误。\n");
      return NULL_TREE;
   }

   if(!data.statementList){
      tree stmtList=alloc_stmt_list();
      append_to_statement_list (impliciCallee, &stmtList);
      BIND_EXPR_BODY(data.bindExpr)=stmtList;
      data.statementList = stmtList;
   }

   //加入变量声明的DCL_EXPR语句到stmtlist
   tree find=impliciCallee;
   tree firstStmt = build_stmt (loc, DECL_EXPR, decl);
   insertStmt(data.statementList,find,firstStmt,TRUE);
   find=firstStmt;
   DECL_CHAIN(decl) = BIND_EXPR_VARS(data.bindExpr);
   BIND_EXPR_VARS(data.bindExpr) = decl;

   int i;
   tree arg;
   for (i = 0; exprlist->iterate (i, &arg); ++i){
      tree refIndex0=build_int_cst(integer_type_node,i);
      tree lhs = build4 (ARRAY_REF, TREE_TYPE(TREE_TYPE(decl)), decl, refIndex0, NULL_TREE,NULL_TREE);
      n_debug("mtcslanuch.c 参数的数据类型:%s\n",get_tree_code_name(TREE_CODE(TREE_TYPE(arg))));
      tree argType=TREE_TYPE(arg);
      if(!POINTER_TYPE_P(argType)){
         n_debug("mtcslanuch.c 不是指针参数---- count:%d lvalue_p (arg)：%d\n",i,lvalue_p (arg));
         if(!lvalue_p (arg)){
            arg=createTemptVar_implicitly(loc,caller,arg);
            tree argStmt = build_stmt (loc, DECL_EXPR, arg);
            insertStmt(data.statementList,find,argStmt,FALSE);
            find=argStmt;
            DECL_CHAIN(arg) = BIND_EXPR_VARS(data.bindExpr);
            BIND_EXPR_VARS(data.bindExpr) = arg;
            //要加入到
         }
         arg = build_unary_op (0, ADDR_EXPR, arg, FALSE);//转指针
      }else if(POINTER_TYPE_P(argType)){
         n_debug("mtcslanuch.c  createVoidPointerArray 00\n");
         arg = build1 (ADDR_EXPR, build_pointer_type (argType), arg);
      }else{
         n_debug("mtcslanuch.c  createVoidPointerArray 11\n");
         arg = build_unary_op (0, ADDR_EXPR, arg, FALSE);//转指针
      }
      tree rhs = arg;
      tree modifyVarDecl= build2 (MODIFY_EXPR, TREE_TYPE(TREE_TYPE(decl)), lhs, rhs);
      insertStmt(data.statementList,find,modifyVarDecl,FALSE);
      find=modifyVarDecl;
   }

   tree ppType=build_pointer_type(void_ptr_type);
   tree addrExpr= build_unary_op (loc, ADDR_EXPR, decl, false);
   tree kernelParams = build1 (NOP_EXPR, ppType, addrExpr);
   return kernelParams;
}

/**
 * 替换调用隐藏的核函数
 */
tree mtcs_lanuch_replace_call_implicitly(MtcsLanuch *self,tree earlyFuncDeclOrRef,
      location_t loc,vec<tree, va_gc> *exprlist,tree caller,ClassFunc *callee,tree impliciCallee)
{
   vec<tree, va_gc> *lanuchParamsList=createParamList_implicitly(self,loc,earlyFuncDeclOrRef,caller,impliciCallee);
   ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
   tree id=aet_utils_create_ident(rootClassName->sysName);
   tree aobjectType=lookup_name(id);
   aobjectType=TREE_TYPE(aobjectType);
   tree aobjectPointerType=build_pointer_type(aobjectType);
   ClassFunc  *callerFunc=func_mgr_get_func(func_mgr_get(),caller);
   n_debug("mtcs_lanuch_replace_call_implicitly 调用者:%p %s callee static:%d\n",callerFunc,callerFunc->orgiName,
         class_func_is_static(callee));
   if(callerFunc){
      tree selftree=(*exprlist)[0];
      tree firstParm = build1 (NOP_EXPR, aobjectPointerType, selftree);//转成(AObject*)value
      vec_safe_insert(lanuchParamsList,0,firstParm);
   }else{
      vec_safe_insert(lanuchParamsList,0,null_pointer_node);
   }
   vec_safe_insert(lanuchParamsList,1,
            build_int_cst(integer_type_node,class_func_is_static(callee)?1:0));

   tree actualFnName=createKernrlFuncNameActualParam(loc,callee->mangleFunName);
   vec_safe_insert(lanuchParamsList,2,actualFnName);
   tree kernelArgs = createVoidPointerArray_implicitly(loc,exprlist,caller,impliciCallee);
   vec_safe_insert(lanuchParamsList,7,kernelArgs);
   tree extra=null_pointer_node;
   vec_safe_insert(lanuchParamsList,8,extra);
   int i;
   vec<location_t> arg_loc =vNULL;
   for(i=0;i<9;i++)
      arg_loc.safe_push (loc);
   vec<tree, va_gc> *origtypes;
   vec_alloc (origtypes, 9);
   //1-9参数类型
   origtypes->quick_push (aobjectPointerType);
   origtypes->quick_push (integer_type_node);
   origtypes->quick_push (build_pointer_type(char_type_node));
   tree recordId=aet_utils_create_ident("dim3");
   tree dim3Record=lookup_name (recordId);
   if(dim3Record==NULL){
      n_error("报告此错误 createLanuchParams dim3Record 是空的");
      return NULL_TREE;
   }
   origtypes->quick_push (TREE_TYPE(dim3Record));
   origtypes->quick_push (TREE_TYPE(dim3Record));
   origtypes->quick_push (unsigned_type_node); //auint sharedMemBytes
   origtypes->quick_push (build_pointer_type(void_type_node)); //void *hStream
   origtypes->quick_push (build_pointer_type(build_pointer_type(void_type_node)));//void **kernelParams
   origtypes->quick_push (build_pointer_type(build_pointer_type(void_type_node))); //void **extra

   tree funcDecl=getLanuchFunction(self,loc,lanuchParamsList,origtypes,arg_loc);
   //如果 funcDecl是空的，说明出错了 由c-parser.c处理 error_mark_node
   if(funcDecl==NULL_TREE)
      return error_mark_node;
   tree newCall=c_build_function_call_vec (loc, arg_loc, funcDecl, lanuchParamsList, origtypes);
   TREE_SIDE_EFFECTS(newCall) = 1;
   TREE_TYPE(newCall) = void_type_node;  // 假设返回void
   arg_loc.release();
   vec_free (origtypes);
   return newCall;
}

MtcsLanuch *mtcs_lanuch_new()
{
   MtcsLanuch *self = n_slice_alloc0 (sizeof(MtcsLanuch));
   mtcsLanuchInit(self);
   return self;
}
