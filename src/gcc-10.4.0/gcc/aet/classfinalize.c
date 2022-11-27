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
#include "classfinalize.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classfinalize.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"

static void classFinalizeFinalize(ClassFinalize *self)
{
}


static struct c_arg_info *createParam (c_parser *parser, char *className)
{
  struct c_declspecs *specs;
  push_scope ();

  specs = build_null_declspecs ();
  struct c_typespec t;
  t.kind = ctsk_typedef;
  tree value=aet_utils_create_ident(className);
  t.spec = lookup_name (value);
  t.expr = NULL_TREE;
  t.expr_const_operands = true;
  c_token *token = c_parser_peek_token (parser);//随便找一个位置
  declspecs_add_type (token->location, specs, t);
  finish_declspecs (specs);
  specs->attrs = NULL_TREE;
  location_t start_loc = c_parser_peek_token (parser)->location;

  struct c_declspecs *quals_attrs = build_null_declspecs ();
  value=aet_utils_create_ident("self");
  struct c_declarator *paramDeclarator= build_id_declarator (value);
  paramDeclarator->id_loc = c_parser_peek_token (parser)->location;
  struct c_declarator *pointer= make_pointer_declarator (quals_attrs, paramDeclarator);

  c_declarator *id_declarator = pointer;
  while (id_declarator && id_declarator->kind != cdk_id){
     id_declarator = id_declarator->declarator;
  }
  location_t end_loc = parser->last_token_location;
  location_t caret_loc = (id_declarator->u.id.id ? id_declarator->id_loc : start_loc);
  location_t param_loc = make_location (caret_loc, start_loc, end_loc);
  tree expr;
  struct c_parm *parm= build_c_parm (specs, NULL_TREE,pointer, param_loc);
  push_parm_decl (parm, &expr);
  struct c_arg_info * args= get_parm_info (false, expr);
  pop_scope ();
  return args;
}

/**
 * 把 ~Abc( 重整为 void ~Abc(
 */
static void rearrangeFinalize(c_parser *parser,ClassName *className)
{
   int tokenCount=parser->tokens_avail;
   location_t  loc = c_parser_peek_token(parser)->location;
   if(tokenCount+2>AET_MAX_TOKEN){
		 error("token太多了");
		 return;
   }
   int i;
   for(i=tokenCount;i>0;i--){
		aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
   }
   aet_utils_create_void_token(&parser->tokens[0],loc);
   char finalizeName[255];
   aet_utils_create_finalize_name(className->userName,finalizeName);
   aet_utils_create_token(&parser->tokens[1],CPP_NAME,finalizeName,strlen(finalizeName));
   parser->tokens_avail=tokenCount+2;
   aet_print_token_in_parser("class_finalize rearrangeFinalize className ---- %s finalize:%s",className->userName,finalizeName);
}


static nboolean  haveFieldOrDefine(ClassFinalize *self,ClassName *className,nboolean field)
{
	NPtrArray *array=func_mgr_get_funcs(func_mgr_get(),className);
	if(array==NULL)
		return FALSE;
	char temp[255];
	aet_utils_create_finalize_name(className->userName,temp);
    int i;
    for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->orgiName,temp)==0 && aet_utils_valid_tree(field?item->fieldDecl:item->fromImplDefine)){
			return TRUE;
	   }
    }
    return FALSE;
}

static tree createUnrefOrFinalizeDecl(ClassFinalize *self,ClassName *className,tree structType,nboolean isUnref)
{
	c_parser *parser=self->parser;
    struct c_declspecs *specs;
    specs = build_null_declspecs ();
    tree value=aet_utils_create_ident("void");
    struct c_typespec t;
    t.kind = ctsk_resword;
    t.spec = value;
    t.expr = NULL_TREE;
  	t.expr_const_operands = true;
    c_token *token = c_parser_peek_token (parser);//随便找一个位置
    declspecs_add_type (token->location, specs, t);
    finish_declspecs (specs);
    tree prefix_attrs = specs->attrs;
    tree all_prefix_attrs = prefix_attrs;
    specs->attrs = NULL_TREE;
    struct c_declspecs *quals_attrs = build_null_declspecs ();
    char funcName[255];
    if(isUnref)
       aet_utils_create_unref_name(className->userName,funcName);
    else
       aet_utils_create_finalize_name(className->userName,funcName);
    value=aet_utils_create_ident(funcName);
    struct c_declarator *funcsDeclarator= build_id_declarator (value);
    funcsDeclarator->id_loc = c_parser_peek_token (parser)->location;
  	struct c_declarator *pointer=make_pointer_declarator (quals_attrs, funcsDeclarator);
  	struct c_arg_info *args=createParam(parser,className->sysName);
  	struct c_declarator *fundecl = build_function_declarator (args, pointer);
  	struct c_declarator *pointerLast=fundecl;//make_pointer_declarator (quals_attrs, fundecl);
  	nboolean change=func_mgr_change_class_func_decl(func_mgr_get(),pointerLast,className,structType);
    tree decls = NULL_TREE;
    tree postfix_attrs = NULL_TREE;
  	//printf("class_finalize_create_finalize_decl 11\n");
  	tree d = grokfield (c_parser_peek_token (parser)->location,pointerLast, specs, NULL_TREE, &all_prefix_attrs);
    decl_attributes (&d, chainon (postfix_attrs,all_prefix_attrs), 0);
  	DECL_CHAIN (d) = decls;
  	decls = d;
  	if(change)
  		func_mgr_set_class_func_decl(func_mgr_get(),d,className);
  	return decls;
}


nboolean class_finalize_parser(ClassFinalize *self,ClassName *className)
{
    if(className==NULL)
    	return FALSE;
    c_parser *parser=self->parser;
	if(class_mgr_is_interface(class_mgr_get(),className)){
		location_t  loc = c_parser_peek_token(parser)->location;
	    error_at(loc,"接口不能有释放函数:%qs",className->userName);
	    return FALSE;
    }

	if(c_parser_peek_2nd_token(parser)->type==CPP_NAME){
	   tree ident = c_parser_peek_2nd_token(parser)->value;
	   const char *str1=IDENTIFIER_POINTER (ident);
	   if(strcmp(str1,className->sysName)){
		   n_warning("eee vvxxx %s %s\n",str1,className->userName);
		  return FALSE;
	   }
	   //printf("这里一个释放函数---- %s %s\n",className->userName,className->sysName);
	   c_parser_consume_token (parser); //消耗掉~
	   if(c_parser_peek_2nd_token(parser)->type==CPP_OPEN_PAREN){
		  c_parser_consume_token (parser); //消耗掉className
		  c_token *who=c_parser_peek_2nd_token (parser);//应该是 void 或)
		  if (who->keyword == RID_VOID  || who->type == CPP_CLOSE_PAREN){
			  rearrangeFinalize(parser,className);
			  return TRUE;
		  }else{
			  location_t  loc = c_parser_peek_token(parser)->location;
			  error_at(loc,"类%qs的释放函数不能带参数。",className->userName);
			  return FALSE;
		  }

         //检查有没有参数
	   }else{
		  location_t  loc = c_parser_peek_token(parser)->location;
		  error_at(loc,"这不是类%qs的释放函数。",className->userName);
		  return FALSE;
	   }
	}
    return FALSE;
}

nboolean  class_finalize_have_field(ClassFinalize *self,ClassName *className)
{
	return haveFieldOrDefine(self,className,TRUE);
}

nboolean   class_finalize_have_define(ClassFinalize *self,ClassName *className)
{
	return haveFieldOrDefine(self,className,FALSE);
}


tree class_finalize_create_finalize_decl(ClassFinalize *self,ClassName *className,tree structType)
{
	return createUnrefOrFinalizeDecl(self,className,structType,FALSE);
}

tree class_finalize_create_unref_decl(ClassFinalize *self,ClassName *className,tree structType)
{
	return createUnrefOrFinalizeDecl(self,className,structType,TRUE);
}

static void fillReleaseCodes(ClassFinalize *self,ClassName *className,NString *codes)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info!=NULL){
	   char funcName[255];
	   aet_utils_create_finalize_name(className->userName,funcName);
	   n_string_append_printf(codes,"((%s *)self)->%s();\n",className->sysName,funcName);
	   fillReleaseCodes(self,&info->parentName,codes);
   }
}

nboolean  class_finalize_build_unref_define(ClassFinalize *self,ClassName *className)
{
   NString *codes=n_string_new("");
   char funcName[255];
   aet_utils_create_unref_name(className->userName,funcName);
   n_string_append_printf(codes,"void %s()\n",funcName);
   n_string_append(codes,"{\n");
   fillReleaseCodes(self,className,codes);
   n_string_append(codes,"}\n");
   n_string_append(codes,";\n");//只有加一个分号才不会出错.
   n_debug("重要的方法源代码:\n%s\n",codes->str);
   aet_utils_add_token(parse_in,codes->str,strlen(codes->str));
   return TRUE;
}

void  class_finalize_build_finalize_define(ClassFinalize *self,ClassName *className)
{
   c_parser *parser=self->parser;
   int tokenCount=parser->tokens_avail;
   location_t  loc = c_parser_peek_token(parser)->location;
   if(tokenCount+6>AET_MAX_TOKEN){
	 error("token太多了");
	 return ;
   }
   int i;
   for(i=tokenCount;i>0;i--){
	  aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+6]);
   }
   char finalizeName[255];
   aet_utils_create_finalize_name(className->userName,finalizeName);
   aet_utils_create_void_token(&parser->tokens[0],loc);
   aet_utils_create_token(&parser->tokens[1],CPP_NAME,finalizeName,strlen(finalizeName));
   aet_utils_create_token(&parser->tokens[2],CPP_OPEN_PAREN,"(",1);
   aet_utils_create_token(&parser->tokens[3],CPP_CLOSE_PAREN,")",1);
   aet_utils_create_token(&parser->tokens[4],CPP_OPEN_BRACE,"{",1);
   aet_utils_create_token(&parser->tokens[5],CPP_CLOSE_BRACE,"}",1);
   parser->tokens_avail=tokenCount+6;
   aet_print_token_in_parser("class_finalize_build_finalize_define className ---- %s finalize:%s",className->sysName,finalizeName);
}


ClassFinalize *class_finalize_new()
{
	ClassFinalize *self = n_slice_alloc0 (sizeof(ClassFinalize));
	classFinalizeFinalize(self);
	return self;
}


