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
#include "c-family/c-pragma.h"
#include "c-family/c-common.h"
#include "tree-iterator.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "aet-c-parser-header.h"
#include "newstack.h"
#include "classutil.h"
#include "genericutil.h"
#include "genericfile.h"
#include "blockmgr.h"
#include "genericinfo.h"
#include "genericcommon.h"

static void newStackInit(NewStack *self)
{
	   ((NewStrategy*)self)->nest=0;
}

/**
 * 是不是非指针类型的对象变量。
 */
static nboolean isUnpointerClass(tree var)
{
    tree type=TREE_TYPE(var);
    if(TREE_CODE(type)!=RECORD_TYPE)
    	return FALSE;
	tree next=TYPE_NAME(type);
	if(TREE_CODE(next)!=TYPE_DECL)
		return FALSE;
	tree decl=DECL_NAME(next);
	char *sysClassName=IDENTIFIER_POINTER(decl);
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
	if(className==NULL)
		return FALSE;
	return TRUE;
}

static void setClearup(NewStack *self,tree decl)
{
	  tree attr = lookup_attribute ("cleanup", DECL_ATTRIBUTES (decl));
	  if (attr){
		  printf("用户自设的cleanup %s\n",IDENTIFIER_POINTER(DECL_NAME(decl)));
		  return;
	  }
	  tree cleanup_id = aet_utils_create_ident(AET_CLEANUP_OBJECT_METHOD);
	  tree cleanup_decl = lookup_name (cleanup_id);
	  tree cleanup;
	  vec<tree, va_gc> *v;

	  /* Build "cleanup(&decl)" for the destructor.  */
	  cleanup = build_unary_op (input_location, ADDR_EXPR, decl, false);
	  vec_alloc (v, 1);
	  v->quick_push (cleanup);
	  cleanup = c_build_function_call_vec (DECL_SOURCE_LOCATION (decl), vNULL, cleanup_decl, v, NULL);
	  vec_free (v);

	  /* Don't warn about decl unused; the cleanup uses it.  */
	  TREE_USED (decl) = 1;
	  TREE_USED (cleanup_decl) = 1;
	  DECL_READ_P (decl) = 1;
	  push_cleanup (decl, cleanup, false);
}

static void createInitCodes(NewStack *self,tree var,ClassName *className,NString *codes)
{
	new_strategy_new_object_from_stack((NewStrategy *)self,var,className,codes);
}

static void setInitGlobalVar(NewStack *self,tree var,ClassName *className)
{
	char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
	struct timeval tv;
    gettimeofday (&tv, NULL);
	long local_tick = (unsigned) tv.tv_sec * 1000 + tv.tv_usec / 1000;
	long randNumber=local_tick+rand ();
	char ctorName[255];
	sprintf(ctorName,"%s_%s_%ld",className->sysName,varName,randNumber);
	NString *codes=n_string_new("");
	if(!current_function_decl){
	  n_string_append_printf(codes,"static __attribute__((destructor)) void %s_dctor()\n",ctorName);
	  n_string_append(codes,"{\n");
	  n_string_append_printf(codes,"(&%s)->unref();\n",varName);
	  n_string_append(codes,"}\n");
	  n_string_append_printf(codes,"static __attribute__((constructor)) void %s_ctor()\n",ctorName);
	  n_string_append(codes,"{\n");
      createInitCodes(self,var,className,codes);
      //不能加因为_cpp_pop_buffer 会把最后一个}省略了 n_string_append(codes,"}\n");//结否constructor函数
	}else{
	    n_string_append(codes,"{\n");
	    createInitCodes(self,var,className,codes);
	}
    n_string_append(codes,"}\n");
    location_t ctorLoc= c_aet_get_ctor_location(var);
    if(ctorLoc==0){
           error_at(input_location,"newstatck.c中的setInitGlobalVar 不应该出现的错误，构造函数的位置是零，报告此错误。");
           return ;
    }
    aet_utils_add_token_with_location(parse_in,codes->str,codes->len,ctorLoc);
	n_debug("setInitGlobalVar 原代码:\n%s\n",codes->str);
	n_string_free(codes,TRUE);
}

static void setInitLocalStaticVar(NewStack *self,tree var,ClassName *className)
{
	char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
    NString *codes=n_string_new("");
    n_string_append(codes,"if(");
    n_string_append_printf(codes,"((%s *)(&%s))",AET_ROOT_OBJECT,varName);
    n_string_append(codes,"->refCount<=0){\n");
    createInitCodes(self,var,className,codes);
    n_string_append(codes,"}\n");
	aet_utils_add_token(parse_in,codes->str,codes->len);
	n_debug("setInitLocalStaticVar 原代码:\n%s\n",codes->str);
	n_string_free(codes,TRUE);
}



static void setInitLocalVar(NewStack *self,tree var,ClassName *className)
{
	char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
    NString *codes=n_string_new("");
    n_string_append(codes,"{\n");
    createInitCodes(self,var,className,codes);
    n_string_append(codes,"}\n");
    location_t ctorLoc= c_aet_get_ctor_location(var);
    if(ctorLoc==0){
        error_at(input_location,"newstatck.c中的setInitLocalVar 不应该出现的错误，构造函数的位置是零，报告此错误。");
        return ;
    }
    aet_utils_add_token_with_location(parse_in,codes->str,codes->len,ctorLoc);
	n_debug("newstack setInitLocalVar 原代码:\n%s\n",codes->str);
	n_string_free(codes,TRUE);
}

static void setVarProperties(NewStack *self,NewObjectType varType)
{
	c_parser *parser=((NewStrategy *)self)->parser;
    new_strategy_set_var_type((NewStrategy*)self,varType);
}

static nboolean initObject(NewStack *self,tree var)
{
	c_parser *parser=((NewStrategy *)self)->parser;
	if(!VAR_P(var))
		return FALSE;
	int external= DECL_EXTERNAL(var);
	int isPublic= TREE_PUBLIC(var);
	int isStatic=TREE_STATIC(var);
	char *declName=IDENTIFIER_POINTER(DECL_NAME(var));
	tree scpe =DECL_CONTEXT(var);
	aet_print_tree(scpe);
	nboolean re=isUnpointerClass(var);
	//printf("new_stack_check 00 var is name:%s isExternal:%d isPublic:%d isStatic:%d isUnpointerClass:%d\n",declName,external,isPublic,isStatic,re);
	//aet_print_token(c_parser_peek_token (parser));
	if(!re)
		return FALSE;
	int isFileStatic=0;
	if(!isPublic && isStatic && (scpe==NULL || scpe==NULL_TREE || scpe==error_mark_node)){
		//检查
		isFileStatic=1;
	}
	char *sysClassName=class_util_get_class_name_by_record(TREE_TYPE(var));
	ClassName *className=class_mgr_clone_class_name(class_mgr_get(),sysClassName);
	new_strategy_add_new((NewStrategy*)self,className,declName);
	c_aet_set_create_method(var,CREATE_OBJECT_METHOD_STACK);//因该不需要了
	if(isPublic || isFileStatic){
		setInitGlobalVar(self,var,className);
		n_debug("new_stack_check 11 全局变量 var is name:%s className:%s isExternal:%d isPublic:%d isStatic:%d isFileStatic:%d\n",
				declName,className->sysName,external,isPublic,isStatic,isFileStatic);
		setVarProperties(self,NEW_OBJECT_GLOBAL);
		return TRUE;
	}else{
		if(isStatic){
			setInitLocalStaticVar(self,var,className);
			n_debug("new_stack_check 22 局部静态变量 var is name:%s className:%s isExternal:%d isPublic:%d isStatic:%d\n",
						declName,className->sysName,external,isPublic,isStatic);
			setVarProperties(self,NEW_OBJECT_LOCAL_STATIC);
			return TRUE;
		}else{
			setClearup(self,var);
			setInitLocalVar(self,var,className);
			n_debug("new_stack_check 33 局部变量 var is name:%s className:%s isExternal:%d isPublic:%d isStatic:%d\n",
						declName,className->sysName,external,isPublic,isStatic);
			setVarProperties(self,NEW_OBJECT_LOCAL);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * 调用构造函数完成了
 */
void  new_stack_finish(NewStack *self)
{
   c_parser *parser=((NewStrategy *)self)->parser;
   ClassName *className=new_strategy_get_class_name((NewStrategy*)self);
   char *varName=new_strategy_get_var_name((NewStrategy*)self);
   NewObjectType varType=new_strategy_get_var_type((NewStrategy*)self);
   if(((NewStrategy*)self)->nest==0){
     n_warning("new_stack_finish 没有初始化变量 返回");
	 return ;
   }
  // printf("new_stack_finish 00 %s %s varType:%d\n",className->sysName,varName,varType);
   new_strategy_recude_new((NewStrategy*)self);
}

/**
 * 当finish_decl后调用
 * Abc abc=new$ Abc();
 */
nboolean new_stack_init_object(NewStack *self,tree var)
{
	return initObject(self,var);
}

/**
 * Abc abc;
 * abc=new$ Abc();
 */
nboolean new_stack_modify_object(NewStack *self,tree var)
{
	return initObject(self,var);
}

static char * getRemainToken(NewStack *self)
{
	  NString *codes=n_string_new("");
	  c_parser *parser=((NewStrategy *)self)->parser;
	  int tokenCount=parser->tokens_avail;
	  int i;
	  for(i=0;i<tokenCount;i++){
		 c_token *token;
		 if(i==0){
			token=c_parser_peek_token (parser);
		 }else if(i==1){
			token=c_parser_peek_2nd_token (parser);
		 }else{
			token=c_parser_peek_nth_token (parser,i);
		 }
		 char *re=aet_utils_convert_token_to_string(token);
		 if(re==NULL){
			 n_error("token不能转成字符!!!");
			 return NULL;
		 }
		 n_string_append(codes,re);
	  }
	  for(i=0;i<tokenCount;i++){
		 c_parser_consume_token (parser);
	  }
	  char *result=NULL;
	  if(codes->len>0)
		  result=n_strdup(codes->str);
	  n_string_free(codes,TRUE);
	  return result;
}

void new_stack_create_object_no_name(NewStack *self,ClassName *className,GenericModel *genericDefine,char *ctorStr)
{
	     char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_STACK);
		 ClassInit *classInit=((NewStrategy *)self)->classInit;
		 NString *codes=n_string_new("");
		 n_string_append(codes,"({");
		 char *varName="catObject";
		 n_string_append_printf(codes,"__attribute__((cleanup(%s))) %s %s;\n",AET_CLEANUP_OBJECT_METHOD,className->sysName,varName);
		 new_strategy_new_object_from_stack_no_name((NewStrategy *)self,varName,genericDefine,
		 		                  ctorStr,className,codes);//完成如 return new Abc();返回的是对象而不是指针。
		 n_string_append_printf(codes,"%s;\n",varName);
		 n_string_append(codes,"})");

		 char *remain= getRemainToken(self);
		 if(remain!=NULL){
		   n_string_append_printf(codes,"%s\n",remain);
		   printf("remain is :%s\n",remain);
		   n_free(remain);
		 }
		 new_strategy_add_new((NewStrategy*)self,className,varName);
		 new_strategy_set_var_type((NewStrategy*)self,NEW_OBJECT_LOCAL);
		 aet_utils_add_token(parse_in,codes->str,codes->len);
		 printf("new_stack_create_object_no_name 源代码:\n%s\n",codes->str);
		 n_string_free(codes,TRUE);
}

NewStack *new_stack_new()
{
	NewStack *self = n_slice_alloc0 (sizeof(NewStack));
	newStackInit(self);
	return self;
}

