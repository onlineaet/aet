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
#include "toplev.h"

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
#include "aetutils.h"
#include "classmgr.h"
#include "supercall.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"
#include "classimpl.h"

static void superCallInit(SuperCall *self)
{
	self->nest=0;
	self->hashTable=n_hash_table_new(n_str_hash,n_str_equal);
}


/**
 * 取调用的函数名
 */
static char *getSuperMethod(SuperCall *self,tree exprValue,char **callClass,char **orgiName)
{
	if(TREE_CODE(exprValue)!=COMPONENT_REF)
		return NULL;
	tree indirectRef=TREE_OPERAND(exprValue,0);
	if(TREE_CODE(indirectRef)!=INDIRECT_REF && TREE_CODE(indirectRef)!=COMPONENT_REF)
		return NULL;
    tree record=TREE_TYPE(indirectRef);
	if(TREE_CODE(record)!=RECORD_TYPE)
		return NULL;
	tree typeName=TYPE_NAME(record);
	if(TREE_CODE(typeName)!=TYPE_DECL)
		return NULL;
	tree refId=DECL_NAME(typeName);
    char *sysClassName=IDENTIFIER_POINTER(refId);
    if(sysClassName==NULL)
		return NULL;
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
    if(info==NULL)
		return NULL;
    ClassName *className=&info->className;
	tree field=TREE_OPERAND(exprValue,1);
	if(TREE_CODE(field)!=FIELD_DECL)
		return NULL;
	tree fieldName=DECL_NAME (field);
	char *funcName=IDENTIFIER_POINTER(fieldName);
	int len=IDENTIFIER_LENGTH(fieldName);
	if(funcName==NULL || len<2 || funcName[0]!='_' || funcName[1]!='Z')
		return NULL;
	tree type=TREE_TYPE(field);
	if(TREE_CODE(type)!=POINTER_TYPE)
		return NULL;
	tree funtype=TREE_TYPE(type);
	if(TREE_CODE(funtype)!=FUNCTION_TYPE)
		return NULL;
	char funClass[255];
	char orgiFunName[255];
	len= func_mgr_get_orgi_func_and_class_name(func_mgr_get(),funcName,funClass,orgiFunName);
	if(len<=0)
		return NULL;
	if(strcmp(funClass,className->userName))
		return NULL;
	*callClass=n_strdup(funClass);
	*orgiName=n_strdup(orgiFunName);
	return funcName;
}

typedef struct _SuperCallInfo{
    char *atSysName;//在那个类里调用的super
    ClassFunc *primitive;//原始的的ClassFunc，但不一定是最终调用的
    tree typedecl;
    int distance;//atSysName到AObject的距离。作为访问superCalls的索引
    int index;
    tree convert_expr;
}SuperCallInfo;

static tree createTypeDecl(char *atSysName,ClassFunc*primitive)
{
	char fname[255];
	sprintf(fname,"_%s_%s",atSysName,primitive->rawMangleName);
	tree fntype=TREE_TYPE(primitive->fieldDecl);
	fntype=TREE_TYPE(fntype);
	tree pointer=build_pointer_type(fntype);
    tree typeNameId=aet_utils_create_ident(fname);
	tree decl = build_decl (input_location, TYPE_DECL, typeNameId, pointer);
    DECL_ARTIFICIAL (decl) = 1;
    DECL_CONTEXT(decl)=NULL_TREE;
    DECL_EXTERNAL(decl)=0;
    TREE_STATIC(decl)=0;
    TREE_PUBLIC(decl)=0;
    set_underlying_type (decl);
    record_locally_defined_typedef (decl);
	c_c_decl_bind_file_scope(decl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
	finish_decl (decl, input_location, NULL_TREE,NULL_TREE, NULL_TREE);
	return decl;
}

static SuperCallInfo *createSuperCallInfo(char *atSysName,char *mangle,int index)
{
	SuperCallInfo *info =n_slice_alloc0 (sizeof(SuperCallInfo));
	ClassFunc    *func=func_mgr_get_entity_by_mangle(func_mgr_get(),mangle);
	info->atSysName=n_strdup(atSysName);
	info->primitive=func;
	info->typedecl=createTypeDecl(atSysName,func);
	info->distance=class_mgr_get_distance(class_mgr_get(),atSysName);
	info->index=index;
	return info;
}

static SuperCallInfo *getEntity(NPtrArray *array,char *mangle)
{
   int i;
   for(i=0;i<array->len;i++){
	   SuperCallInfo *item=(SuperCallInfo *)n_ptr_array_index(array,i);
	   if(strcmp(item->primitive->mangleFunName,mangle)==0){
		   n_debug("getEntity 已经存在的mangle %s %s",item->primitive->mangleFunName,mangle);
		   return item;
	   }
   }
   return NULL;
}

/**
 *	  ( (setData) (((AObject *)self)->superCalls[0]) [0])  ();
 *
 */
static struct c_expr createExpr(SuperCall *self,SuperCallInfo *info)
{
	  struct c_expr expr;
	  c_parser *parser=self->parser;
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+23>AET_MAX_TOKEN){
			error("token太多了");
			return  expr;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+23]);
	  }
	  char *funcNameTypeDecl=IDENTIFIER_POINTER(DECL_NAME(info->typedecl));
	  ClassName *aObjectClassName=class_mgr_get_class_name_by_user(class_mgr_get(), AET_ROOT_OBJECT);

	  aet_utils_create_token(&parser->tokens[0],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[2],CPP_NAME,funcNameTypeDecl,strlen(funcNameTypeDecl));
	  parser->tokens[2].id_kind=C_ID_TYPENAME;//关键

	  aet_utils_create_token(&parser->tokens[3],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[4],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[5],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[6],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[7],CPP_NAME,aObjectClassName->sysName,strlen(aObjectClassName->sysName));
	  parser->tokens[7].id_kind=C_ID_TYPENAME;//关键
	  aet_utils_create_token(&parser->tokens[8],CPP_MULT,"*",1);
	  aet_utils_create_token(&parser->tokens[9],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[10],CPP_NAME,"self",4);
	  aet_utils_create_token(&parser->tokens[11],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[12],CPP_DEREF,"->",2);
	  aet_utils_create_token(&parser->tokens[13],CPP_NAME,SUPER_FUNC_ADDR_VAR_NAME,strlen(SUPER_FUNC_ADDR_VAR_NAME));
	  aet_utils_create_token(&parser->tokens[14],CPP_OPEN_SQUARE,"[",1);
	  aet_utils_create_number_token(&parser->tokens[15],info->distance);
	  aet_utils_create_token(&parser->tokens[16],CPP_CLOSE_SQUARE,"]",1);
	  aet_utils_create_token(&parser->tokens[17],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[18],CPP_OPEN_SQUARE,"[",1);
	  aet_utils_create_number_token(&parser->tokens[19],info->index);
	  aet_utils_create_token(&parser->tokens[20],CPP_CLOSE_SQUARE,"]",1);
	  aet_utils_create_token(&parser->tokens[21],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[22],CPP_SEMICOLON,";",1);
	  parser->tokens_avail=tokenCount+23;
	  aet_print_token_in_parser("createSuperCallExpr  %s %s",info->atSysName,funcNameTypeDecl);
	  expr = c_c_parser_expr_no_commas (parser, NULL);
	  return expr;

}



/**
 * 生成函数的typedecl,如:typedef void (*setData)(AObject *self,int value);
 * 然后这样调用
 * (setData)( (((AObject *)self)->superCalls[index])[pos] )
 */
tree super_call_replace_super_call(SuperCall *self,tree exprValue)
{
	c_parser *parser=self->parser;
	if(!parser->isAet)
		return NULL_TREE;
	ClassName *currentClassName=class_impl_get()->className;
	char *callClass=NULL;
	char *orgiName=NULL;
	char *mangle=getSuperMethod(self,exprValue,&callClass,&orgiName);
	if(mangle==NULL){
		aet_print_tree_skip_debug(exprValue);
		n_error("在super_call_add_super_call不应该发生这样的错误。\n");
		return NULL_TREE;
	}
	char *sysName=currentClassName->sysName;
	SuperCallInfo *item=NULL;
	if(!n_hash_table_contains(self->hashTable,sysName)){
		item=createSuperCallInfo(sysName,mangle,0);
		NPtrArray *array=n_ptr_array_sized_new(2);
		n_ptr_array_add(array,item);
		n_hash_table_insert (self->hashTable, n_strdup(sysName),array);
	 }else{
		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
		item=getEntity(array,mangle);
		if(item==NULL){
			item=createSuperCallInfo(sysName,mangle,array->len);
			n_ptr_array_add(array,item);
		}else{
		  return item->convert_expr;
		}
	 }
	struct c_expr expr=createExpr(self,item);
	if(aet_utils_valid_tree(expr.value)){
		item->convert_expr=expr.value;
        return expr.value;
	}
	return NULL_TREE;
}

nboolean   super_call_is_super_state(SuperCall *self)
{
   return self->nest>0;
}

void   super_call_minus(SuperCall *self)
{
	self->nest--;
}


/**
 * super-> 变成 ((Parent*)self)->
 */
static nboolean superDerefTo(SuperCall *self,ClassName *className)
{
	c_parser *parser=self->parser;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(info==NULL){
        c_parser_error (parser, "找不到类。");
		return FALSE;
	}
	char *parent=info->parentName.userName;
	if(parent==NULL){
        c_parser_error (parser, "找不到父类！");
        return FALSE;
	}
	ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&info->parentName);
	if(parentInfo==NULL){
        c_parser_error (parser, "找不到父类。");
		return FALSE;
	}

	  location_t  loc = c_parser_peek_token (parser)->location;
	  c_token *deref = c_parser_peek_token (parser);//->
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+7>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  int i;
	  parser->tokens_avail=tokenCount+7;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+7]);
	  }
	  aet_utils_create_token(&parser->tokens[6],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[5],CPP_NAME,(char *)"self",4);
	  aet_utils_create_token(&parser->tokens[4],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[3],CPP_MULT,"*",1);
	  aet_utils_create_token(&parser->tokens[2],CPP_NAME,info->parentName.sysName,strlen(info->parentName.sysName));
	  parser->tokens[2].id_kind=C_ID_TYPENAME;//关键
	  aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[0],CPP_OPEN_PAREN,"(",1);
	  aet_print_token_in_parser("super call superDerefTo className ---- %s nest:%d",className->sysName,self->nest);
	  return TRUE;
}


void       super_call_parser(SuperCall *self,ClassName *className,nboolean isConstructor)
{
	  c_parser *parser=self->parser;
	  if(!parser->isAet){
		  c_parser_error (parser, "super关键字只能用在类的实现中。");
		  return;
	  }
	  c_parser_consume_token (parser);
	  if (c_parser_next_token_is (parser, CPP_DEREF)){
		  nboolean result=superDerefTo(self,className);
		  if(result)
			  self->nest++;
	  }else if(c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
		   //如果出现super(但不是第一条句时错的
		  if(isConstructor)
			  c_parser_error (parser, "构造函数中super$()语句,只能是第一条语句。");
		  else
		      c_parser_error (parser, "类函数中不能有super$()语句。");
	  }else{
		  c_parser_error (parser, "super$后只能是->或(符号。");
	  }
}



static tree createCastType(ClassName *className)
{
	tree recordId=aet_utils_create_ident(className->sysName);
	tree record=lookup_name(recordId);
	if(!record || record==NULL_TREE || record==error_mark_node){
		printf("没有找到 %s\n",className);
		error("没有找到 class");
	}
	//printf("createCastType 00  %s\n",get_tree_code_name(TREE_CODE(record)));
	tree recordType=TREE_TYPE(record);
	tree pointer=build_pointer_type(recordType);
	return pointer;
}

/**
 * 把 super$生成表达式 ((AObject*)self)
 */
static tree superAtPostfixExpression(SuperCall *self,ClassName *className,location_t  loc)
{
	c_parser *parser=self->parser;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(info==NULL){
        c_parser_error (parser, "找不到类。");
		return error_mark_node;
	}
	char *parent=info->parentName.userName;
	if(parent==NULL){
        c_parser_error (parser, "找不到父类！");
        return error_mark_node;
	}
	ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&info->parentName);
	if(parentInfo==NULL){
        c_parser_error (parser, "找不到父类。");
		return error_mark_node;
	}
	tree parmOrVar=lookup_name(aet_utils_create_ident("self"));
	tree type=createCastType(&parentInfo->className);
    tree castParent = build1 (NOP_EXPR, type, parmOrVar);
	protected_set_expr_location (castParent, loc);
	return castParent;
}

/**
 * 建立((AObject *)self)表达式
 */
tree  super_call_parser_at_postfix_expression(SuperCall *self,ClassName *className)
{
	  c_parser *parser=self->parser;
	  if(!parser->isAet){
		  c_parser_error (parser, "super关键字只能用在类的实现中。");
		  return error_mark_node;
	  }
	  location_t  loc = c_parser_peek_token (parser)->location;
	  c_parser_consume_token (parser);//consume super$
	  if (c_parser_next_token_is (parser, CPP_DEREF)){
		  tree result=superAtPostfixExpression(self,className,loc);
		  if(result!=error_mark_node)
			  self->nest++;
		  return result;
	  }else if(c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
		   //如果出现super(但不是第一条句时错的
		  c_parser_error (parser, "类函数中不能有super$()语句。");
	  }else{
		  c_parser_error (parser, "super后只能是->或(符号。");
	  }
	  return error_mark_node;
}



static char **getParents(char *sysName)
{
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	if(info==NULL)
		return NULL;
	int distance=class_mgr_get_distance(class_mgr_get(),sysName);
	n_debug("getParents --- distnce:%d sysName:%s\n",distance,sysName);
	char **res=n_new(char *,distance+2);
	int count=0;
	while(info->parentName.sysName!=NULL){
		res[count++]=n_strdup(info->parentName.sysName);
		info=class_mgr_get_class_info(class_mgr_get(),info->parentName.sysName);
	}
	res[count]=NULL;
	return res;
}

/**
 * parents从低到高 最高是AObject
 */
static void createCode(char *fromSysName,char **parents,SuperCallInfo *info,int superCallCount,NString *codes)
{
	ClassName *aObjectClassName=class_mgr_get_class_name_by_user(class_mgr_get(), AET_ROOT_OBJECT);
    char *funcName=info->primitive->rawMangleName;
	nuint len=n_strv_length(parents);
	int i;
	for(i=0;i<len;i++){
	   if(i==0){
          n_string_append_printf(codes,"   if(%s((%s *)self,\"%s\",%d,%d,%d,\"%s\",\"%s\")>0){\n",
        		  SUPER_FILL_ADDRESS_FUNC,aObjectClassName->sysName,fromSysName,info->distance,superCallCount,info->index, parents[i],funcName);
	   }else{
	      n_string_append_printf(codes,"   }else if(%s((%s *)self,\"%s\",%d,%d,%d,\"%s\",\"%s\")>0){\n",
	        		  SUPER_FILL_ADDRESS_FUNC,aObjectClassName->sysName,fromSysName,info->distance,superCallCount,info->index, parents[i],funcName);
	   }
       n_string_append(codes,"      ;\n");
       if(i==len-1){
    		n_string_append(codes,"   }\n");
       }
    }
}


char *super_call_create_codes(SuperCall *self,ClassName *className)
{
    char *sysName=className->sysName;
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
    if(array==NULL || array->len==0)
        return NULL;
    NString *codes=n_string_new("");
    int superCallCount=array->len;
    char **parents=getParents(sysName);
    int i;
    for(i=0;i<array->len;i++){
        SuperCallInfo *item=n_ptr_array_index(array,i);
        createCode(sysName,parents,item,superCallCount,codes);
    }
    char *result=NULL;
    if(codes->len>0)
        result=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return result;
}



SuperCall *super_call_new()
{
	SuperCall *self = n_slice_alloc0 (sizeof(SuperCall));
	superCallInit(self);
	return self;
}
