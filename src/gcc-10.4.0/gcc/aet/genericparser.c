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
#include "toplev.h"
#include "opts.h"

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
#include "genericcommon.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "genericparser.h"
#include "blockmgr.h"
#include "classimpl.h"
#include "funcmgr.h"
#include "genericcommon.h"
#include "blockfunc.h"


static void genericParserInit(GenericParser *self)
{
}

/////////////////////////////以下是E被替换为泛型定义 E abc=7 E *ax=(E *)xx-----------------------------------


static BlockFunc *getBlockFunc(GenericParser *self)
{
	    tree currentFunc=current_function_decl;
		char *funcName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
		NPtrArray *blockFuncArray=self->help->blockFuncsArray;
		//printf("getBlockFunc 00 %s\n",funcName);
		int i;
	    for(i=0;i<blockFuncArray->len;i++){
	        BlockFunc *item=n_ptr_array_index(blockFuncArray,i);
			//printf("getBlockFunc 11 %s %s\n",item->funcDefineDecl,item->funcDefineName);
		   if(strcmp(item->funcDefineName,funcName)==0){
			   return item;
		   }
	   }
	   return NULL;
}

static int backupToken(GenericParser *self,c_token *backups)
{
	  c_parser *parser=self->parser;
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
	     aet_utils_copy_token(token,&backups[i]);
	  }
	  for(i=0;i<tokenCount;i++){
		 c_parser_consume_token (parser);
	  }
	  return tokenCount;
}

static void addToken(GenericParser *self,c_token *addTokens,int add,nboolean before,c_token *replaces,int rc,c_token *backups,int bc)
{
	  c_parser *parser=self->parser;
	  int tokenCount=parser->tokens_avail;
	  int offset=rc+bc+add;
	  if(tokenCount+offset>AET_MAX_TOKEN){
			error("token太多了");
			return;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
		 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+offset]);
	  }
	  int count=0;
	  if(before){
	     for(i=0;i<add;i++){
		   aet_utils_copy_token(&addTokens[i],&parser->tokens[count++]);
	     }
	  }
	  for(i=0;i<rc;i++){
		  aet_utils_copy_token(&replaces[i],&parser->tokens[count++]);
	  }
	  for(i=0;i<bc;i++){
	      aet_utils_copy_token(&backups[i],&parser->tokens[count++]);
	  }
	  if(!before){
		 for(i=0;i<add;i++){
		   aet_utils_copy_token(&addTokens[i],&parser->tokens[count++]);
		 }
	   }
	  parser->tokens_avail=tokenCount+offset;
	  aet_print_token_in_parser("generic_parser_replace addReplace ------");
}

/**
 * 转GenericUnit转成 int,char *等字符串。
 */

static char *convertToStr(GenericUnit *define)
{
	NString *str=n_string_new("");
	n_string_append_printf(str," %s ",define->name);
	int i;
	for(i=0;i<define->pointerCount;i++){
		n_string_append(str,"*");
	}
	char *re=n_strdup(str->str);
	n_string_free(str,TRUE);
	return re;
}


/**
 * 通过这条语句获取genDefineStr的类型
 * gen_replace_start %s gen_replace_end
 */
static int createDefineToken(GenericParser *self,char *genDefineStr,c_token *replaces)
{
   c_parser *parser=self->parser;
   char replaceStr[255];
   sprintf(replaceStr,"gen_replace_start %s gen_replace_end \n",genDefineStr);
   aet_utils_add_token(parse_in,replaceStr,strlen(replaceStr));
   int i;
   int replaceCount=0;
   for(i=0;i<100;i++){
	 c_token *token=c_parser_peek_token (parser);
	 aet_print_token(token);
	 if(token->type==CPP_NAME){
		tree id=token->value;
		char *str=IDENTIFIER_POINTER(id);
		if(strcmp(str,"gen_replace_start")==0){
		   //printf("开始了------\n");
		   c_parser_consume_token (parser);
		   continue;
		}
		if(strcmp(str,"gen_replace_end")==0){
			//printf("结束了------\n");
			c_parser_consume_token (parser);
			break;
		}
	 }
	 aet_utils_copy_token(token,&replaces[replaceCount++]);
	 c_parser_consume_token (parser);
   }
   //_cpp_pop_buffer (parse_in);
	//printf("结束了------xxxxxxccccccxxxx \n");
   _cpp_pop_buffer(parse_in);
	//printf("结束了------wwwwwwwwwww \n");
	cpp_buffer *buffer = parse_in->buffer;
	//printf("结束了------111 %p %p\n",buffer,buffer->prev);
	buffer->prev=NULL;
	return replaceCount;
}

/**
 * 是不是泛型函数中的泛型声明
 */
static int getFuncGenericDeclIndex(BlockFunc *impl,char *genStr)
{
	if(!impl->isFuncGeneric)
		return -1;
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),impl->sysClassName);
	ClassFunc *func=func_mgr_get_entity(func_mgr_get(),className,impl->belongFunc);
	tree fieldDecl=func->fieldDecl;
	GenericModel *gendecl=c_aet_get_func_generics_model(fieldDecl);
	int index= generic_model_get_index(gendecl,genStr);
	return index;
}

static int getGenericDeclFromClass(BlockFunc *impl,char *genStr)
{
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),impl->sysClassName);
	if(info->genericModel){
		int index= generic_model_get_index(info->genericModel,genStr);
		return index;
	}
	return -1;
}

#define INDEX_CONNECT_STR "$YZt0@"


/**
 * 把genericof(E)或genericof(obj,E)替换成int或float
 * 当genericof(obj,E)时 parmName就是obj代表的类，否则parmName=self
 * 当BlockFunc是泛型函数时，parmName=self，说明self是泛型类
 * 这时取的泛型定义应是泛型函数定义的，如果genStr不是泛型函数声明的，就从self中取
 */

static void replaceGenericOf(GenericParser *self,char *object,char *genStr,c_token *tokens,int addTokens,nboolean before)
{
	   c_parser *parser=self->parser;
	   BlockFunc *impl=getBlockFunc(self);
	   char *parmName=n_strdup(object);
	   if(impl==NULL){
		   n_error("在replaceGenericOf找不到泛型块函数，BlockFunc是空的。%s %s %s",self->currentDefineStr,parmName,genStr);
	   }
	   if(self->currentDefineStr==NULL){
		   n_error("在replaceGenericOf找不到泛型块下的泛型定义。  %s %s %s",self->currentDefineStr,parmName,genStr);
	   }
	   if(impl->isFuncGeneric && !strcmp(parmName,"self")){
		  int index=getFuncGenericDeclIndex(impl,genStr);
		  n_debug("是泛型函数，在genericof$(E) 查一下是self的E还是泛型函数的E %d\n",index);
		  if(index<0){
             //在self里找
			 index= getGenericDeclFromClass(impl,genStr);
			 if(index<0){
				n_error("在泛型函数%s和类%s中都找不到泛型声明%s！",impl->belongFunc,impl->sysClassName,genStr);
			 }
		  }else{
		     n_free(parmName);
		     parmName=n_strdup(AET_FUNC_GENERIC_PARM_NAME);
		  }
	   }
	   //printf("replaceGenericOf 22 %p parmName:%s\n",impl,parmName);

       GenericUnit *info=NULL;
	   nchar **items=n_strsplit(self->currentDefineStr,INDEX_CONNECT_STR,-1);
	   int length= n_strv_length(items);
	   int i;
	   for(i=0;i<length;i++){
		  NString *ns=n_string_new(items[i]);
		  int pos=n_string_last_indexof(ns,"_");
		  NString *parmStr=n_string_substring_from(ns,0,pos);
		  NString *indexStr=n_string_substring(ns,pos+1);
		  if(strcmp(parmStr->str,parmName)==0){
             int pos=atoi(indexStr->str);
             info=block_func_get_define(impl,parmName,genStr,pos);
   		     n_string_free(ns,TRUE);
   		     n_string_free(parmStr,TRUE);
   		     n_string_free(indexStr,TRUE);
             break;
		  }
		  n_string_free(ns,TRUE);
		  n_string_free(parmStr,TRUE);
		  n_string_free(indexStr,TRUE);
	   }

	   if(info==NULL){
	     if(!strcmp(parmName,"self"))
			n_error("在类%s中的第%d个泛型块，没有变量：%s typeof(%s) FILE:%s",impl->sysClassName,impl->index,parmName,genStr,in_fnames[0]);
	     else
		   n_error("在类%s中的第%d个泛型块，没有变量：%s typeof(%s,%s) FILE:%s",impl->sysClassName,impl->index,parmName,parmName,genStr,in_fnames[0]);
		 return;
	   }
	   n_free(parmName);
	   n_strfreev(items);
	   c_token backups[30];
	   int backCount=backupToken(self,backups);
	   char *trueGenStr=convertToStr(info);
	   c_token replaces[10];
	   int replaceCount=createDefineToken(self,trueGenStr,replaces);
       addToken(self,tokens,addTokens,before,replaces,replaceCount,backups,backCount);
}



/**
 * 解析 (E*) (E) 等强制转化
 */
void  generic_parser_cast_by_token(GenericParser *self,c_token *token)
{
	c_parser *parser=self->parser;
	if(token->type!=CPP_NAME){
		error_at(token->location,"出错了在(E)");
		return;
	}
	tree id=token->value;
	char *genStr=IDENTIFIER_POINTER(id);
	//printf("generic_parser_cast_by_token %s\n",genStr);
    c_parser_consume_token (parser);//consume (
    c_parser_consume_token (parser); //consume E
   	c_token prefixTokens[1];
	aet_utils_create_token(&prefixTokens[0],CPP_OPEN_PAREN,"(",1);
	replaceGenericOf(self,"self",genStr,prefixTokens,1,TRUE);
}

/**
 * 把 E替换为真实的泛型，如 int,Abc
 * 把泛型字符串传给编译器
 */
void generic_parser_replace(GenericParser *self,char *genStr)
{
 	 replaceGenericOf(self,"self",genStr,NULL,0,TRUE);
}

/**
 * 当编译块函数实现时，会进入每一个条件块中的第一条语名。aet_goto_compile$ 6 model_0$YZt0@b_0 这是每个参数在
 * 该条件块时的泛型定义
 * 处理 aet_goto_compile$ 6 model_0$YZt0@b_0 model是参数名 0是参数对应的泛型定义所在的索引号
 */
void generic_parser_produce_cond_define(GenericParser *self)
{
	  c_parser *parser=self->parser;
	  if(c_parser_peek_token (parser)->type!=CPP_STRING){
		  n_error("在generic_create_gen_str不可知的错误，这里应该是一个CPP_STRING符号！");
	  }
	  tree string=c_parser_peek_token (parser)->value;
	  const char *tokenString  = TREE_STRING_POINTER (string);
	  //去除一头一尾的"号
	  char *genDefine=n_strndup(tokenString+1,strlen(tokenString)-1);
	  if(self->currentDefineStr){
		  n_free(self->currentDefineStr);
		  self->currentDefineStr=NULL;
	  }
	  self->currentDefineStr=n_strdup(genDefine);
	  aet_print_token(c_parser_peek_token (parser));
	 // printf("value is  :%s\n",genDefine);
	  c_parser_consume_token (parser);
	  aet_print_token(c_parser_peek_token (parser));
	  n_free(genDefine);
}

//////////////////////////////////////////////////////////typeof//////////////////////////////////

static nboolean getCallerAndGenType(GenericParser *self,char **callObj,char **gen)
{
      c_parser *parser=self->parser;
	  tree currentFunc=current_function_decl;
      char *funcName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
	  //printf("getCallerAndGenType 00 %s\n",funcName);
	  aet_print_token(c_parser_peek_token (parser));
	  c_token *first=c_parser_peek_token (parser);
	  c_token *second=c_parser_peek_2nd_token (parser);
	  if(first->type==CPP_NAME){
		  tree id1=first->value;
		  nboolean re=generic_util_valid_all(id1);
		  if(re){
			  //是 E
			  c_token *second=c_parser_peek_2nd_token (parser);
			  if(second->type==CPP_CLOSE_PAREN){
				  //是 E)
				  *callObj=n_strdup("self");
				  *gen=n_strdup(IDENTIFIER_POINTER(id1));
				  c_parser_consume_token (parser); // E
				  c_parser_consume_token (parser); // )
				  return TRUE;
			  }else{
				 return FALSE;
			  }
		  }else{
			  c_token *second=c_parser_peek_2nd_token (parser);
			  if(second->type==CPP_COMMA){
			 			  //是 xxx,
				 c_token *three=c_parser_peek_nth_token (parser,3);
				 if(three->type==CPP_NAME){
				    tree id1=three->value;
				    nboolean re=generic_util_valid_all(id1);
				    if(re){
					  //是 xxx,E
					   c_token *four=c_parser_peek_nth_token (parser,4);
					   if(four->type==CPP_CLOSE_PAREN){
						  //是 xxx,E)
						  *callObj=n_strdup(IDENTIFIER_POINTER(first->value));
						  *gen=n_strdup(IDENTIFIER_POINTER(id1));
						   c_parser_consume_token (parser); // obj
						   c_parser_consume_token (parser); // ,
						   c_parser_consume_token (parser); // E
						   c_parser_consume_token (parser); // )
						   return TRUE;
					   }else{
						 return FALSE;
					   }
				    }else{
					  return FALSE;
				    }
				 }else{
				   return FALSE;
				 }//end cpp_name
			  }else{
			 	return FALSE;
			  }//end CPP_COMMA
		  }//end re
	  }else{

	  }
	  return FALSE;
}


static void testGenericOf(GenericParser *self)
{
	  c_parser *parser=self->parser;
	  location_t loc=c_parser_peek_token (parser)->location;
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+3>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
		  aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+3]);
	  }
	  aet_utils_create_token(&parser->tokens[0],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_int_token(&parser->tokens[1],loc);
	  aet_utils_create_token(&parser->tokens[2],CPP_CLOSE_PAREN,")",1);
	  parser->tokens_avail=tokenCount+3;
	  aet_print_token_in_parser("在测试泛型块中testGenericOf xxxx----");
}

void generic_parser_parser_typeof(GenericParser *self)
{
	 c_parser *parser=self->parser;
	 if(!parser->isGenericState && !parser->isTestGenericBlockState)
		 return ;
	  aet_print_token(c_parser_peek_token (parser));
	  aet_print_token(c_parser_peek_2nd_token (parser));
	  char *callObj=NULL;
	  char *genStr=NULL;
	  nboolean ok=getCallerAndGenType(self,&callObj,&genStr);
	  n_debug("generic_of_parser_typeof 22 ok:%d callObj:%s,genStr:%s\n",ok,callObj,genStr);
	  if(ok){
		 if(parser->isAet && parser->isTestGenericBlockState && !parser->isGenericState){
			 n_debug("在aet不在isGenericState\n");
			 testGenericOf(self);
		 }else{
			 n_debug("在isGenericState %s %s\n",callObj,genStr);
			 c_token afterTokens[1];
			 aet_utils_create_token(&afterTokens[0],CPP_CLOSE_PAREN,")",1);
			 replaceGenericOf(self,callObj,genStr,afterTokens,1,FALSE);
		 }
	  }
	  if(callObj)
		n_free(callObj);
	  if(genStr)
		n_free(genStr);
}

void generic_parser_set_parser(GenericParser *self,c_parser *parser)
{
   self->parser=parser;
}

void  generic_parser_set_help(GenericParser *self,SetBlockHelp *help)
{
	   self->help=help;
}


GenericParser *generic_parser_new()
{
	GenericParser *self =n_slice_alloc0 (sizeof(GenericParser));
    genericParserInit(self);
	return self;
}

