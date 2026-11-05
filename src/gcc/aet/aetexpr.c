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
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "aetinfo.h"
#include "aetexpr.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "classmgr.h"
#include "classimpl.h"
#include "classbuild.h"

static void aetExprInit(AetExpr *self)
{
   self->useVarOf = FALSE;
   self->state = 0;
}

static tree createStringActualParm(location_t loc,char *rightSysName)
{
   size_t length=strlen(rightSysName);
   tree strCst = build_string (length, (const char *) rightSysName);
   tree type = build_array_type (char_type_node,build_index_type (size_int (length)));
   TREE_TYPE(strCst)=type;
   struct c_expr expr;
   expr.value=strCst;
   expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
   set_c_expr_source_range (&expr, loc, loc);
   return expr.value;

}

static  vec<tree, va_gc> *createParm(location_t loc,tree firstParm,char *rightSysName)
{
   tree secondParm=createStringActualParm(loc,rightSysName);
   vec<tree, va_gc> *parmVec;
   parmVec = make_tree_vector ();
   vec_safe_push(parmVec,firstParm);
   vec_safe_push(parmVec,secondParm);
   return parmVec;
}

static tree create_varof_object_or_interface_decl (void)
{
   location_t loc = UNKNOWN_LOCATION;
   tree name = get_identifier ("varof_object_or_interface");
   tree object_type = build_pointer_type (void_type_node);
   tree name_type = build_pointer_type (char_type_node);
   tree argtypes = tree_cons (NULL_TREE, object_type,
                   tree_cons (NULL_TREE, name_type,
                   tree_cons (NULL_TREE, void_type_node, NULL_TREE)));
   tree fntype = build_function_type (integer_type_node, argtypes);
   tree fndecl = build_decl (loc, FUNCTION_DECL, name, fntype);

   /* static */
   TREE_PUBLIC (fndecl) = 0;
   TREE_STATIC (fndecl) = 1;

   /* inline */
   //DECL_DECLARED_INLINE_P (fndecl) = 1;

   /* 仅声明，没有函数定义 */
   DECL_EXTERNAL (fndecl) = 1;

   /* 登记到文件作用域 */
   c_c_decl_bind_file_scope(fndecl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   finish_decl (fndecl, loc, NULL_TREE,NULL_TREE, NULL_TREE);
   return fndecl;
}

/**
 * varof_object_or_interface
 * 关键字 varof$的用法
 * if(var varof$ type)
 * var转成 void*类型 作为 varof_object_or_interface的第一个参数 object
 * type 取类型名作为第二个参数 name
 * name= type 的名字
 * object = var
 * AET类或接口的第一个参数是魔数
 */
static tree buildExpr(AetExpr *self,location_t loc,tree leftValue,char *rightSysName)
{
   tree valueType=TREE_TYPE(leftValue);
   tree type=build_pointer_type(void_type_node);
   tree firstParm;
   if(TREE_CODE(valueType)!=POINTER_TYPE){
      tree pointer = build_unary_op (loc, ADDR_EXPR, leftValue, FALSE);//转指针
      firstParm = build1 (NOP_EXPR, type, pointer);//转成(void*)value
   }else{
      firstParm = build1 (NOP_EXPR, type, leftValue);//转成(void*)value
   }
   tree funcName=aet_utils_create_ident(AET_VAR_OF_FUNC_NAME);
   tree decl=lookup_name(funcName);
   if(!decl)
      decl = create_varof_object_or_interface_decl();
   vec<tree, va_gc> *parms=createParm(loc,firstParm,rightSysName);
   tree newCallExpr = c_build_function_call_vec (loc, vNULL, decl,parms, NULL);
   release_tree_vector (parms);
   self->useVarOf = TRUE;
   return newCallExpr;
}
/**
 * 加入新关键词 varof$ 判断变量是否是某个类或接口的实例。
 * varof$是一个二元运算符。左边是变量 右边是类或接口。返回TRUE或FALSE
 * 可以在编译时确定的报错。其它在运行时确定。为了实现RTTI（Run-Time Type Identification)
 * 在类或接口的第一个元素加入魔数magic。
 * xxx varof$ YYY 表达式
 * AObject varof$ Any ok 运行时确定
 * void * varof$  Any ok 运行时确定
 * AObject varof$ 接口 ok 运行时确定
 */
struct c_expr aet_expr_varof_parser(AetExpr *self,struct c_expr lhs)
{
   c_parser *parser=self->parser->parser;
   struct c_expr expr;
   location_t startLoc=lhs.get_start();
   tree left=lhs.value;
   c_parser_consume_token (parser);//consume varof$
   c_token *token = c_parser_peek_token (parser);
   location_t loc=c_parser_peek_token (parser)->location;
   if(token->id_kind != C_ID_TYPENAME){
      error_at(loc,"varof$关键字后不是一个AET类或接口。");
      return lhs;
   }
   tree typeName=token->value;
   if(TREE_CODE(typeName)!=IDENTIFIER_NODE){
      error_at(loc,"varof$关键字后不是一个AET类或接口。");
      return lhs;
   }
   char *rightSysName=IDENTIFIER_POINTER(typeName);
   nboolean  isClass= class_mgr_is_class(class_mgr_get(),rightSysName);
   if(!isClass){
      error_at(loc,"varof$关键字后不是一个AET类或接口。");
      return lhs;
   }
   ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),rightSysName);
   c_parser_consume_token (parser);//consume AObject
   tree leftType=TREE_TYPE(left);
   char  *leftSysName=class_util_get_class_name(leftType);
   ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),leftSysName);
   if(leftSysName!=NULL){
      ClassRelationship  ship= class_mgr_relationship(class_mgr_get(), leftSysName,rightSysName);
      //printf("说明变量和比较的类型都是AET类或接口:左:%s 右:%s ship:%d\n",leftSysName,rightSysName,ship);
      if(!strcmp(leftSysName,rightSysName) || ship==CLASS_RELATIONSHIP_CHILD || ship==CLASS_RELATIONSHIP_IMPL){
         lhs.value=build_int_cst(integer_type_node,1);
      }else{
         if(class_info_is_interface(rightInfo)){
            n_debug("左边%s只能在运行时才能知道是否实现了右边%s接口。\n",leftSysName,rightSysName);
            lhs.value=buildExpr(self,startLoc,left,rightSysName);
         }else if(!strcmp(AET_ROOT_OBJECT,leftInfo->className.userName)){
            n_debug("左边%s是一个根类。只能在运行时才能知道是否是右边%s类型。\n",leftSysName,rightSysName);
            lhs.value=buildExpr(self,startLoc,left,rightSysName);
         }else if(class_info_is_interface(leftInfo) && !class_info_is_interface(rightInfo)){
            n_debug("左边%s是一个接口。只能在运行时才能知道接口是从右边%s类型转化而来的。\n",leftSysName,rightSysName);
            lhs.value=buildExpr(self,startLoc,left,rightSysName);
         }else if(!class_info_is_interface(leftInfo) && !class_info_is_interface(rightInfo) && ship==CLASS_RELATIONSHIP_PARENT){
            n_debug("左边%s是右边%s的父类。只能在运行时才能知道左边类型的变量是不是右边的类型。\n",leftSysName,rightSysName);
            lhs.value=buildExpr(self,startLoc,left,rightSysName);
         }else{
            error_at(startLoc,"%qs不是%qs类型。",leftSysName,IDENTIFIER_POINTER(typeName));
            return lhs;
         }
      }
   }else{
      //  printf("lefttype is --- %s\n",get_tree_code_name(TREE_CODE(leftType)));
      if(TREE_CODE(leftType)==POINTER_TYPE){
         n_debug("左边是指针类型，只能在运行时确定是否可以转化为右边%s类型.\n",rightSysName);
         lhs.value=buildExpr(self,startLoc,left,rightSysName);
      }else{
         error_at(startLoc,"不是%qs类型。",rightSysName);
      }
   }
   return lhs;
}

static const char *codes = R"%%%(
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

static inline int is_aet_class (AClass *cl, const char *name)
{
    const char *package = cl->getPackage();
    if (package){
        size_t len = strlen(package);
        return strncmp(package, name, len) == 0
            && name[len] == '_'
            && strcmp(name + len + 1, cl->getName()) == 0;
    }
    return strcmp(cl->getName(), name) == 0;
}

static inline int object_varof(AClass *cl,char *name)
{
   if(cl==NULL){
      return 0;
   }
   if(is_aet_class(cl,name)){
      return 1;
   }
   AClass *parentClass=cl->getParent();
   if(parentClass!=NULL){
      if(is_aet_class(parentClass,name)){
         return 1;
      }
   }
   int ifaceCount=0;
   AClass **interfaceClasses=cl->getInterfaces(&ifaceCount);
   int i;
   for(i=0;i<ifaceCount;i++){
      if(is_aet_class(interfaceClasses[i],name)){
         return 1;
      }
   }
   return object_varof(parentClass,name);
}

#define AET_IFACE_CLASS_OFFSET   ((sizeof(int) + _Alignof(void *) - 1) & ~(_Alignof(void *) - 1))

static int  varof_object_or_interface (void *object,char *name)
{
   char *magic=(char*)object;
   int magicNum =0;
   memcpy(&magicNum,magic,sizeof(int));
   if(magicNum!=1725348960 && magicNum!=1725348961){
      return 0;
   }
   AObject *dest=NULL;
   if(magicNum==1725348960){
      dest=(AObject*)object;
   }else if(magicNum==1725348961){
      memcpy(&dest,magic + AET_IFACE_CLASS_OFFSET,sizeof(void *));
   }
   if(dest==NULL){
      return 0;
   }
   return object_varof(dest->getClass(),name);
}
#pragma GCC diagnostic pop
)%%%";

void aet_expr_compile(AetExpr *self)
{
   c_parser *parser=self->parser->parser;
   if(self->useVarOf && self->state==1 && !self->parser->isAet){
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_STATIC_VAR_FUNC
      aet_utils_add_token(parse_in,codes,strlen(codes));
      self->state = 2;
   }else{
      n_error("报告此错误！");
   }
}

const char *aet_expr_get_compile_codes(AetExpr *self)
{
   if(self->useVarOf && self->state==0){
      self->state=2;
      return codes;
   }
   return NULL;
}

void  aet_expr_compile_ready(AetExpr *self)
{
   c_parser *parser=self->parser->parser;

   if(self->useVarOf && self->state==0 && !self->parser->isAet){
      c_parser *parser=self->parser->parser;
      int tokenCount=parser->tokens_avail;
      for(int i=tokenCount;i>0;i--)
         aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);

      aet_utils_create_aet_goto_token(&parser->tokens[0],input_location);
      aet_utils_create_number_token(&parser->tokens[1],GOTO_COMPILE_VAROF);
      parser->tokens_avail=tokenCount+2;
      self->state = 1;
   }
}


AetExpr *aet_expr_new()
{
   AetExpr *self = n_slice_alloc0 (sizeof(AetExpr));
   aetExprInit(self);
   self->parser = aet_parser_get();
   return self;
}

