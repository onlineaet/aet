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

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"
#include "opts.h"


#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "newheap.h"
#include "genericutil.h"
#include "genericfile.h"
#include "blockmgr.h"
#include "genericinfo.h"
#include "genericcommon.h"
#include "genericexpand.h"

static void newHeapInit(NewHeap *self)
{
	   ((NewStrategy*)self)->nest=0;
}

static void setVarProperties(NewHeap *self,NewObjectType varType)
{
	c_parser *parser=((NewStrategy *)self)->parser;
    new_strategy_set_var_type((NewStrategy*)self,varType);
}

static char * getRemainToken(NewHeap *self)
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
		 n_debug("getRemainToken backupToken -- %d %d\n",i,tokenCount);
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


static void createInitCodes(NewHeap *self,tree var,ClassName *className,NString *codes,nboolean modify)
{
	char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
	ClassInit *classInit=((NewStrategy *)self)->classInit;
	if(modify){
	   n_string_append(codes,"=");
	}else{
	   n_string_append_printf(codes,"%s=",varName);
	}
	GenericModel *genericDefine= c_aet_get_generics_model(var);//可能是<E,int>或<Abc *,float>如果含有A,B,E等泛型声明不能定义块
	n_debug("createInitCodes-- 泛型 genericDefine %p",genericDefine);
	char *ctorStr=NULL;
	tree ctor=c_aet_get_ctor_codes(var);
	if(aet_utils_valid_tree(ctor)){
		ctorStr=IDENTIFIER_POINTER(ctor);
	}
	char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_HEAP);
	new_strategy_new_object((NewStrategy *)self,tempVarName,genericDefine,className,ctorStr,codes,TRUE);
}

static void setInitGlobalVar(NewHeap *self,tree var,ClassName *className)
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
	  n_string_append_printf(codes,"static __attribute__((constructor)) void %s_ctor()\n",ctorName);
	  n_string_append(codes,"{\n");
      createInitCodes(self,var,className,codes,FALSE);
	  n_string_append(codes,"}\n");
      //不能加因为_cpp_pop_buffer 会把最后一个}省略了 n_string_append(codes,"}\n");//结否constructor函数
	}
	location_t ctorLoc= c_aet_get_ctor_location(var);
	if(ctorLoc==0){
	     error_at(input_location,"newheap.c中的setInitGlobalVar 不应该出现的错误，构造函数的位置是零，报告此错误。");
	     return ;
	}
	aet_utils_add_token_with_location(parse_in,codes->str,codes->len,ctorLoc);
	n_debug("setInitGlobalVar 源代码:\n%s\n",codes->str);
	n_string_free(codes,TRUE);
}

/**
 * 严禁在调用aet_utils_add_token前 c_parser_peek_token
 * 否则出错不容易查找。可能破坏了cpp_reader buffer中的数据引起
 */
static void setInitLocalVar(NewHeap *self,tree var,ClassName *className,nboolean modify)
{
	char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
    NString *codes=n_string_new("");
    createInitCodes(self,var,className,codes,modify);
	if(modify){
	  char *remain= getRemainToken(self);
	  if(remain!=NULL){
		   n_string_append_printf(codes,"%s\n",remain);
		   printf("remain setInitLocalVar is :%s\n",remain);
		   n_free(remain);
	  }
	}
	location_t ctorLoc= c_aet_get_ctor_location(var);
    if(ctorLoc==0){
        error_at(input_location,"setInitLocalVar 不应该出现的错误，构造函数的位置是零，报告此错误。");
        return ;
    }

    aet_utils_add_token_with_location(parse_in,codes->str,codes->len,ctorLoc);
	n_debug("new_heap setInitLocalVar 源代码:varName:%s file:%s\n%s\n",varName,in_fnames[0],codes->str);
	n_string_free(codes,TRUE);
}

/**
 * 调用构造函数完成了
 */
void  new_heap_finish(NewHeap *self,CreateClassMethod method,tree func)
{
   c_parser *parser=((NewStrategy *)self)->parser;
   ClassName *className=new_strategy_get_class_name((NewStrategy*)self);
   char *varName=new_strategy_get_var_name((NewStrategy*)self);
   NewObjectType varType=new_strategy_get_var_type((NewStrategy*)self);
   if(((NewStrategy*)self)->nest==0){
	 n_warning("new_heap_finish 没有初始化变量 返回");
	 return ;
   }
   n_debug("new_heap_finish xxx %s %s varType:%d\n",className->sysName,varName,varType);
   new_strategy_recude_new((NewStrategy*)self);
}

static nboolean newHeapObject(NewHeap *self,tree var,nboolean modify)
{
	c_parser *parser=((NewStrategy *)self)->parser;
	if(!VAR_P(var))
		return FALSE;
	int external= DECL_EXTERNAL(var);
	int isPublic= TREE_PUBLIC(var);
	int isStatic=TREE_STATIC(var);
	char *declName=IDENTIFIER_POINTER(DECL_NAME(var));
	tree scpe =DECL_CONTEXT(var);
	nboolean re=isUnpointerClass(var);
	n_debug("new_heap_create_object 00 var is name:%s isExternal:%d isPublic:%d isStatic:%d isUnpointerClass:%d",declName,external,isPublic,isStatic,re);
	if(re)
		return FALSE;
	int isFileStatic=0;
	if(!isPublic && isStatic && (scpe==NULL || scpe==NULL_TREE || scpe==error_mark_node)){
		//检查
		isFileStatic=1;
	}
	tree ctorSysClassNameTree=c_aet_get_ctor_sys_class_name(var);
	if(!aet_utils_valid_tree(ctorSysClassNameTree)){
		error_at(input_location,"newHeapObject 不应该出现的错误，报告。");
		return FALSE;
	}
	char *varSysClassName=class_util_get_class_name_by_pointer(TREE_TYPE(var));
	char *ctorSysClassName=IDENTIFIER_POINTER(ctorSysClassNameTree);
	if(strcmp(varSysClassName,ctorSysClassName)){
		n_warning("变量的类型与new$的类型不一样。%s %s",varSysClassName,ctorSysClassName);
	}
	ClassName *className=class_mgr_clone_class_name(class_mgr_get(),ctorSysClassName);
	new_strategy_add_new((NewStrategy*)self,className,declName);
	c_aet_set_create_method(var,CREATE_OBJECT_METHOD_HEAP);
	n_debug("new_heap_create_object 11 全局变量 var is name:%s className:%s isExternal:%d isPublic:%d isStatic:%d isFileStatic:%d",
			declName,className->sysName,external,isPublic,isStatic,isFileStatic);
	if(!modify && (isPublic || isFileStatic)){
		setInitGlobalVar(self,var,className);
	}else{
		setInitLocalVar(self,var,className,modify);
	}
	setVarProperties(self,(isPublic || isFileStatic)?NEW_OBJECT_GLOBAL:(isStatic?NEW_OBJECT_LOCAL_STATIC:NEW_OBJECT_LOCAL));
	return TRUE;
}

/**
 * 为参数是无名new对象，创建一个函数内的变量。
 */
static tree createNamelessVar(char *sysName,char *varName)
{
    tree id=aet_utils_create_ident(sysName);
    tree decl=lookup_name(id);
    if(!aet_utils_valid_tree(decl)){
        n_error("错误，创建实参无名对象%s，找不到类声明。",sysName);
        return NULL_TREE;
    }
    aet_print_tree(decl);
    tree pointerType=build_pointer_type(TREE_TYPE(decl));
    tree varNameId=aet_utils_create_ident(varName);
    location_t  loc = input_location;
    decl = build_decl (loc, VAR_DECL, varNameId, pointerType);
    TREE_READONLY (decl) = 0;
    DECL_ARTIFICIAL (decl) = 0;
    DECL_INITIAL (decl) = NULL_TREE;
    TREE_USED (decl) = 1;
    DECL_CONTEXT(decl)=current_function_decl;
    DECL_EXTERNAL(decl)=0;
    TREE_STATIC(decl)=0;
    TREE_PUBLIC(decl)=0;
    return decl;
}

/**
 * 不要加分号
 * 函数中作为参数加分号要出错
 * getName(new$ Abc())
 * 无名创建对象 当作用函数参数时 setData(new$ Abc());，需要另外处理.
 * 在当前函数范围创建临时变量。该变量初始化在参数体中。代码如下
 * void xxxx(){
 *  setData(new$ Abc());转化成
 * }
 *
 * void xxxx(){
 *   Abc *tempAbc __attribute__((cleanup(a_object_cleanup_local_object_from_static_or_stack)));
 *   setData(({tempAbc=debug_AObject.newObject(sizeof(debug_AObject));....});转化成
 * }
 *
 */
void new_heap_create_object_no_decl(NewHeap *self,ClassName *className,GenericModel *genericDefine,char *ctorStr,nboolean isParserParmsState)
{
	 char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_NO_DECL_HEAP);
	 ClassInit *classInit=((NewStrategy *)self)->classInit;
	 if(aet_utils_valid_tree(current_function_decl) && isParserParmsState){
	     printf("在函数内调用new$ Object() isParserParmsState:%d\n",isParserParmsState);
	     c_parser *parser=((NewStrategy *)self)->parser;
	     tree decl=createNamelessVar(className->sysName,tempVarName);
	     tree  attr = NULL_TREE;
	     tree cleanfunc=lookup_name(get_identifier(AET_CLEANUP_NAMELESS_OBJECT_METHOD));
	     attr=tree_cons (get_identifier ("cleanup"), cleanfunc, attr);
	     DECL_ATTRIBUTES(decl)=attr;
	     location_t  loc = DECL_SOURCE_LOCATION(decl);
         tree type=TREE_TYPE(decl);
         c_bind(loc,decl,FALSE);
	     finish_decl (decl, loc, NULL_TREE,type, NULL_TREE);
	 }
	 NString *codes=n_string_new("");
	 new_strategy_new_object((NewStrategy *)self,tempVarName,genericDefine,className,ctorStr,codes,FALSE);
	 char *remain= getRemainToken(self);
	 if(remain!=NULL){
	   n_string_append_printf(codes,"%s\n",remain);
	   printf("remain is :%s\n",remain);
	   n_free(remain);
	 }
	 new_strategy_add_new((NewStrategy*)self,className,tempVarName);
	 new_strategy_set_var_type((NewStrategy*)self,NEW_OBJECT_LOCAL);
     if(aet_utils_valid_tree(current_function_decl) && isParserParmsState){
         //替换debug_AObject *_notv5_13debug_AObject1;为空:
         char find[512];
         sprintf(find,"%s *%s;",className->sysName,tempVarName);
         n_string_replace(codes,find,"",1);
     }
	 aet_utils_add_token(parse_in,codes->str,codes->len);
	 n_debug("new_heap new_heap_create_object_no_decl 源代码:\n%s\n",codes->str);
	 n_string_free(codes,TRUE);
}

nboolean new_heap_create_object(NewHeap *self,tree var)
{
	return newHeapObject(self,var,FALSE);
}

char *new_heap_create_object_for_static(NewHeap *self,tree var)
{
		c_parser *parser=((NewStrategy *)self)->parser;
		if(!VAR_P(var))
			return NULL;
		char *declName=IDENTIFIER_POINTER(DECL_NAME(var));
		nboolean re=isUnpointerClass(var);
		n_debug("new_heap_create_object_for_static 00 var is name:%s isUnpointerClass:%d",declName,re);
		aet_print_token(c_parser_peek_token (parser));
		if(re)
			return NULL;
		tree ctorSysClassNameTree=c_aet_get_ctor_sys_class_name(var);
		if(!aet_utils_valid_tree(ctorSysClassNameTree)){
			error_at(input_location,"newHeapObject 不应该出现的错误，报告。");
			return FALSE;
		}
		char *varSysClassName=class_util_get_class_name_by_pointer(TREE_TYPE(var));
		char *ctorSysClassName=IDENTIFIER_POINTER(ctorSysClassNameTree);
		if(strcmp(varSysClassName,ctorSysClassName)){
			n_warning("变量的类型与new$的类型不一样。%s %s",varSysClassName,ctorSysClassName);
		}
		ClassName *className=class_mgr_clone_class_name(class_mgr_get(),ctorSysClassName);
		//new_strategy_add_new((NewStrategy*)self,className,declName);//不能加，否则会乱，因为实现只能在一个文件。但其它文件可以引用所在的.h
		n_debug("new_heap_create_object_for_static 11 全局变量 var is name:%s className:%s ",declName,className->sysName);
		char *varName=declName;
		struct timeval tv;
	    gettimeofday (&tv, NULL);
		long local_tick = (unsigned) tv.tv_sec * 1000 + tv.tv_usec / 1000;
		long randNumber=local_tick+rand ();
		char ctorName[255];
		sprintf(ctorName,"%s_%s_%ld",className->sysName,varName,randNumber);
		NString *codes=n_string_new("");
	    n_string_append_printf(codes,"static __attribute__((constructor)) void %s_ctor()\n",ctorName);
	    n_string_append(codes,"{\n");
		ClassInit *classInit=((NewStrategy *)self)->classInit;
		n_string_append_printf(codes,"%s=",varName);
		GenericModel *genericDefine= c_aet_get_generics_model(var);//可能是<E,int>或<Abc *,float>如果含有A,B,E等泛型声明不能定义块
		n_debug("createInitCodes-- 泛型 genericDefine %p",genericDefine);
		char *ctorStr=NULL;
		tree ctor=c_aet_get_ctor_codes(var);
		if(aet_utils_valid_tree(ctor)){
			ctorStr=IDENTIFIER_POINTER(ctor);
		}
		char *tempVarName=n_strdup_printf("_temp_%s_%d",className->sysName,CREATE_OBJECT_METHOD_HEAP);
		new_strategy_new_object((NewStrategy *)self,tempVarName,genericDefine,className,ctorStr,codes,TRUE);
	    n_string_append(codes,"}\n");
	      //不能加因为_cpp_pop_buffer 会把最后一个}省略了 n_string_append(codes,"}\n");//结否constructor函数
		char *result=n_strdup(codes->str);
		n_string_free(codes,TRUE);
		return result;
}

nboolean new_heap_modify_object(NewHeap *self,tree var)
{
	return newHeapObject(self,var,TRUE);
}


NewHeap *new_heap_new()
{
	NewHeap *self = n_slice_alloc0 (sizeof(NewHeap));
	newHeapInit(self);
	return self;
}



