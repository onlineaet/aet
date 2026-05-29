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
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "classmgr.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "genericparser.h"
#include "blockmgr.h"
#include "classimpl.h"
#include "funcmgr.h"


static void genericParserInit(GenericParser *self)
{
}

/////////////////////////////以下是E被替换为泛型定义 E abc=7 E *ax=(E *)xx-----------------------------------

static int backupToken(GenericParser *self,c_token *backups)
{
   c_parser *parser=self->parser->parser;
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
   c_parser *parser=self->parser->parser;
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
   if(!before){
      for(i=0;i<add;i++){
         aet_utils_copy_token(&addTokens[i],&parser->tokens[count++]);
      }
   }
   for(i=0;i<bc;i++){
      aet_utils_copy_token(&backups[i],&parser->tokens[count++]);
   }

   parser->tokens_avail=tokenCount+offset;
   aet_print_token_in_parser("generic_parser_replace addReplace ------");
}


/**
 * 通过这条语句获取genDefineStr的类型
 * gen_replace_start %s gen_replace_end
 */
static int createDefineToken(GenericParser *self,char *genDefineStr,c_token *replaces)
{
   c_parser *parser=self->parser->parser;
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


#define INDEX_CONNECT_STR "$YZt0@"


/**
 * 把genericof(E)或genericof(obj,E)替换成int或float
 * 当genericof(obj,E)时 parmName就是obj代表的类，否则parmName=self
 * 当BlockFunc是泛型函数时，parmName=self，说明self是泛型类
 * 这时取的泛型定义应是泛型函数定义的，如果genStr不是泛型函数声明的，就从self中取
 */

//////////////////////////////////////////////////////////typeof//////////////////////////////////

static nboolean getCallerAndGenType(GenericParser *self,char **callObj,char **gen)
{
      c_parser *parser=self->parser->parser;
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

/**
 * 测试 typeof(E)
 */
static void testGenericTypeOf(GenericParser *self)
{
     c_parser *parser=self->parser->parser;
     location_t loc=c_parser_peek_token (parser)->location;
     int tokenCount=parser->tokens_avail;
     int addtoken=2;
     if(tokenCount+addtoken>AET_MAX_TOKEN){
         error("token太多了");
         return FALSE;
     }
     int i;
     for(i=tokenCount;i>0;i--)
        aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+addtoken]);

     aet_utils_create_int_token(&parser->tokens[0],loc);
     aet_utils_create_token(&parser->tokens[1],CPP_CLOSE_PAREN,")",1);
     parser->tokens_avail=tokenCount+addtoken;
     aet_print_token_in_parser("在测试泛型块中testGenericOf xxxx----");
}

///////////////////////新版-------------------------
typedef struct _Directive
{
   //新版 E_int_0_5{,F_float_1_8}
   char *declName; //声明的名字 E、F...
   char *defineTypeName; //定义的名字 int、float,AObject,...
   int  pointerCount;    //指针数
   int  size;            //类型大小
}Directive;

static void freeDirectives(GenericParser *self)
{
   int i;
   for(i=0;i<self->directiveCount;i++){
      n_slice_free(Directive,(Directive *)self->directives[i]);
      self->directives[i]=NULL;
   }
   self->directiveCount=NULL;
}

static void createDirectives(GenericParser *self,char *content)
{
   char **items=n_strsplit(content,",",-1);
   int len=n_strv_length(items);
   int i;
   for(i=0;i<len;i++){
      char **elems=n_strsplit(items[i],"_",-1);
      Directive *d=n_slice_new0(Directive);
      d->declName=n_strdup(elems[0]);
      d->defineTypeName=n_strdup(elems[1]);
      d->pointerCount=atoi(elems[2]);
      d->size=atoi(elems[3]);
      self->directives[i]=(void *)d;
      n_debug("genericparser.c directive---- %d %s %s %d %d\n",i,d->declName,d->defineTypeName,d->pointerCount,d->size);
      n_strfreev(elems);
   }
   n_strfreev(items);

   self->directiveCount=len;

}
/**
 * 当编译块函数实现时，会进入每一个条件块中的第一条语名。aet_goto_compile$ 6 model_0$YZt0@b_0 这是每个参数在
 * 该条件块时的泛型定义
 * 处理 aet_goto_compile$ 6 model_0$YZt0@b_0 model是参数名 0是参数对应的泛型定义所在的索引号
 */
void generic_parser_enter(GenericParser *self)
{
     c_parser *parser=self->parser->parser;
     if(c_parser_peek_token (parser)->type!=CPP_STRING){
        n_error("在generic_create_gen_str不可知的错误，这里应该是一个CPP_STRING符号！");
     }
     tree string=c_parser_peek_token (parser)->value;
     const char *tokenString  = TREE_STRING_POINTER (string);
     //去除一头一尾的"号
     char *genDefine=n_strndup(tokenString+1,strlen(tokenString)-1);
     freeDirectives(self);
     createDirectives(self,genDefine);
     aet_parser_set_generic_state(self->parser,true);
     aet_print_token(c_parser_peek_token (parser));
    // printf("value is  :%s\n",genDefine);
     c_parser_consume_token (parser);
     aet_print_token(c_parser_peek_token (parser));
     n_free(genDefine);
}

static char *getTrueGenStr(Directive *d)
{
   int i;
   NString *str=n_string_new("");
   n_string_append_printf(str," %s ",d->defineTypeName);
   for(i=0;i<d->pointerCount;i++)
      n_string_append(str,"*");
   return n_string_free(str,FALSE);
}
/**
 * 把 E替换为真实的泛型，如 int,Abc
 * 把泛型字符串传给编译器
 */
void generic_parser_replace(GenericParser *self,char *genStr)
{
   c_parser *parser=self->parser->parser;
   if(self->directiveCount==0){
      n_error("在replaceGenericOf找不到泛型块下的泛型定义。%s",genStr);
   }
   int i;
   for(i=0;i<self->directiveCount;i++){
      Directive *d=self->directives[i];
      if(strcmp(d->declName,genStr)==0){
         c_token backups[30];
         int backCount=backupToken(self,backups);
         char *trueGenStr=getTrueGenStr(d);
         c_token replaces[10];
         int replaceCount=createDefineToken(self,trueGenStr,replaces);
         addToken(self,NULL,0,TRUE,replaces,replaceCount,backups,backCount);
         n_free(trueGenStr);
         break;
      }
   }
}

/**
 * 解析 (E*) (E) 等强制转化
 */
void  generic_parser_cast_by_token(GenericParser *self,c_token *token)
{
   c_parser *parser=self->parser->parser;
   if(token->type!=CPP_NAME){
      error_at(token->location,"出错了在(E)");
      return;
   }
   tree id=token->value;
   char *genStr=IDENTIFIER_POINTER(id);
   c_parser_consume_token (parser);//consume (
   c_parser_consume_token (parser); //consume E

   if(self->directiveCount==0){
      n_error("在replaceGenericOf找不到泛型块下的泛型定义。%s",genStr);
   }
   int i;
   for(i=0;i<self->directiveCount;i++){
      Directive *d=self->directives[i];
      if(strcmp(d->declName,genStr)==0){
         c_token backups[30];
         int backCount=backupToken(self,backups);
         char *trueGenStr=getTrueGenStr(d);
         c_token replaces[10];
         int replaceCount=createDefineToken(self,trueGenStr,replaces);
         c_token prefixTokens[1];
         aet_utils_create_token(&prefixTokens[0],CPP_OPEN_PAREN,"(",1);
         addToken(self,prefixTokens,1,TRUE,replaces,replaceCount,backups,backCount);
         n_free(trueGenStr);
         break;
      }
   }
}

void generic_parser_parser_typeof(GenericParser *self)
{
   c_parser *parser=self->parser->parser;
   if(!aet_parser_is_generic_state(self->parser) && !aet_parser_is_test_generic_block_state(self->parser))
      return ;
   char *callObj=NULL;
   char *genStr=NULL;
   nboolean ok=getCallerAndGenType(self,&callObj,&genStr);
   if(ok){
      if(self->parser->isAet && aet_parser_is_test_generic_block_state(self->parser)
      && !aet_parser_is_generic_state(self->parser)){
         n_debug("在aet不在isGenericState。测试block状态");
         testGenericTypeOf(self);
      }else{
         n_debug("generic_parser_parser_typeof 11 在isGenericState %s %s\n",callObj,genStr);
         if(self->directiveCount==0){
             n_error("在replaceGenericOf找不到泛型块下的泛型定义。%s",genStr);
          }
          int i;
          for(i=0;i<self->directiveCount;i++){
             Directive *d=self->directives[i];
             if(strcmp(d->declName,genStr)==0){
                c_token backups[30];
                int backCount=backupToken(self,backups);
                char *trueGenStr=getTrueGenStr(d);
                c_token replaces[10];
                int replaceCount=createDefineToken(self,trueGenStr,replaces);
                c_token afterTokens[1];
                aet_utils_create_token(&afterTokens[0],CPP_CLOSE_PAREN,")",1);
                addToken(self,afterTokens,1,FALSE,replaces,replaceCount,backups,backCount);
                n_free(trueGenStr);
                break;
             }
          }
      }
   }
   if(callObj)
      n_free(callObj);
   if(genStr)
      n_free(genStr);
}


GenericParser *generic_parser_get()
{
   static GenericParser *singleton = NULL;
   if (!singleton){
       singleton =n_slice_alloc0 (sizeof(GenericParser));
       genericParserInit(singleton);
       singleton->parser = aet_parser_get();
   }
   return singleton;
}
