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
#include "plugin.h"

#include "gcc-plugin.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"

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
#include "makefileparm.h"
#include "parserhelp.h"
#include "aetlib.h"
#include "aetprintgimple.h"

typedef struct _ConstDeclData
{
   location_t loc;//关键靠位置和id确定源代码中的token是这是const_decl
   char *id;
   char *number;
   tree func;
}ConstDeclData;

static void genericParserInit(GenericParser *self)
{
   self->funcWithBlockArray=n_ptr_array_new();
   self->localFwgbArray=n_ptr_array_new();

   self->directiveCount = 0;
   self->funcWithGBFileName = NULL;

   self->constDeclArray=n_ptr_array_new();
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

static void addToken(GenericParser *self,c_token *addTokens,int add,
      nboolean before,c_token *replaces,int rc,c_token *backups,int bc)
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

/**
 * E_aet_io_AFile_1_8字符串解析出
 * E、aet_io_AFile、1、8
 */
static void createDirectives(GenericParser *self,char *content)
{
   char **items=n_strsplit(content,",",-1);
   int len=n_strv_length(items);
   int i;
   for(i=0;i<len;i++){
      char **elems=n_strsplit(items[i],"_",-1);
      int elemsCount = n_strv_length(elems);
      Directive *d=n_slice_new0(Directive);
      d->declName=n_strdup(elems[0]);
      char *typeNameStr=xmalloc(255);
      memset(typeNameStr,0,255);
      int j;
      for(j=1;j<=elemsCount-3;j++){
         //还原回类型名，如aet_io_AFile;
         strcat(typeNameStr,elems[j]);
         if(j<elemsCount-3)
            strcat(typeNameStr,"_");
      }
      d->defineTypeName=typeNameStr;//n_strdup(elems[1]);
      d->pointerCount=atoi(elems[elemsCount-2]);
      d->size=atoi(elems[elemsCount-1]);
      self->directives[i]=(void *)d;
      n_debug("genericparser.c directive  %d %s %s %d %d\n",i,d->declName,d->defineTypeName,d->pointerCount,d->size);
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


///////////////---以下是解析泛型块中泛型变量的赋值-------------------------
/**
 * 只转化类中域是指针的情况
 * 例如
 * class$ Abc{
 *   E *abc;或 E **abc;
 * };
 * 如果 E 不是指针类型，则转成
 * int *abc;
 * pointer是从 E *abc中提取的，不是泛型定义的。比如 E=int **
 * 如果 E的真实类型 不是指针，就按下面的方法处理
 * typeName 真实类型如int float 类，结构体等
 * genericPointer 真实类型的指针数
 */
static tree createCast(char *typeName,int genericPointer,tree component,int fieldPointer,tree *genDefine)
{
   tree castType = lookup_name(get_identifier(typeName));
   if(TREE_CODE(castType)==TYPE_DECL)
      castType = TREE_TYPE(castType);
   if(genDefine)
      *genDefine=castType;
   tree origType = castType;
   gcc_assert(castType);
   if(genericPointer==0){
      //E = int 变量 = E queue 转成*((int*)queue)
      if(fieldPointer==0){
         castType = build_pointer_type (castType);
         tree casted = fold_build1 (NOP_EXPR, castType, component);
         //E queue变成 *((int*)queue)
         printf("进这里了--xxxx---\n");
         tree deref = casted;//fold_build1 (INDIRECT_REF, origType,casted);
         return deref;
      }else{
         //E = int 变量 =E *value 转成 (int *)value;变量 =E **value 转成 (int **)value
         int i;
         for(i=0;i<fieldPointer;i++)
            castType = build_pointer_type (castType);
      }
      tree casted = fold_build1 (NOP_EXPR, castType, component);
      return casted;
   }else{
      int i;
      if(fieldPointer==0){
         //E =int * 变量 =E queue  转成 E *queue = (int *)value;
         //for(i=0;i<genericPointer;i++)
            //castType = build_pointer_type (castType);
         return component;
      }else{
         //E =int * 变量 =E *queue  转成 E **queue = (int *)value;
         for(i=0;i<genericPointer+fieldPointer;i++)
             castType = build_pointer_type (castType);
      }
      tree casted = fold_build1 (NOP_EXPR, castType, component);
      printf("convert----ttt-\n");
      aet_print_tree(casted);
      return casted;
   }
}


/**
 * 获取 E对应的具体类型的索引号
 */
static int getDirective(GenericParser *self,tree componentRef,int *fieldPointer)
{
   int pointer=0;
   char *str=generic_util_get_type_str(componentRef,&pointer);
   if(!str)
      return -1;
   //str是 aet_generic_E
   int i;
   for(i=0;i<self->directiveCount;i++){
      Directive *directive=(Directive *)self->directives[i];
      if(endswith(str,directive->declName)){
         //printf("getDirective %s declPointer:%d definePointer:%d\n",
               //str,directive->pointerCount,pointer);
         if(fieldPointer)
            *fieldPointer=pointer;
         return i;
      }
   }
   return -1;
}


///////////-----结束解析泛型块中泛型变量赋值-------------

///////---------------解析泛型类中速泛型块的函数，克隆真实泛型类型------------
//在impl$结束时，generic_parser_save_fwgb 从classfunc取出带泛型块的函数源代码，
//通过aet_utils_add_token_with_force 生成需要token,并保存到FuncWithGbData中。
//在编译文件结束时，调用 generic_parser_register_fwg 注册 PLUGIN_ALL_IPA_PASSES_START回调
//检查是否需要保留带泛型块函数，最后保存该编译单元的带泛型块函数到文件
//
/* 返回 true 表示该函数可以安全迁移到其他编译单元使用 */
static bool can_safely_migrate_function (ClassFunc *classFunc,tree fndecl)
{
   if (!fndecl || TREE_CODE (fndecl) != FUNCTION_DECL)
      return false;

   /* 1. 函数本身最好不是 static（也可以强制改名后迁移，这里先禁止） */
  // if (TREE_STATIC (fndecl) && !TREE_PUBLIC (fndecl))
     // return false;
   cgraph_node *node = cgraph_node::get (fndecl);
   if (!node || !node->has_gimple_body_p ())
      return false;

   node->get_body ();
   struct function *fn = DECL_STRUCT_FUNCTION (fndecl);
   if (!fn)
      return false;

   /* 用于记录是否发现不可迁移的引用 */
   bool safe = true;

   /* 检查一个 tree 是否是本单元私有的实体 */
   auto is_local_private = [] (tree t,tree fndecl) -> bool {
     // printf("tiss ----\n");
     // aet_print_tree_skip_debug(t);
      if (!t || (!DECL_P (t) && TREE_CODE(t)!=COMPONENT_REF))
         return false;

      /* static 变量或 static 函数（非 public） */
     // printf("can_safely_migrate_function is_local_private: %s static:%d public:%d\n",
               //get_tree_code_name(TREE_CODE(t)),TREE_STATIC (t),TREE_PUBLIC (t));
      if (TREE_STATIC (t) && !TREE_PUBLIC (t)){
         //判断是不是类或父类或接口中声明的方法
         if(TREE_CODE(t)==FUNCTION_DECL){
            if(func_mgr_can_use_outside(func_mgr_get(),t))
               return false;

         }else if(TREE_CODE(t)==COMPONENT_REF){
            if(func_mgr_can_use_outside(func_mgr_get(),TREE_OPERAND(t,1)))
                return false;
         }
         return true;
      }else if(TREE_CODE(t)==COMPONENT_REF){
         if(func_mgr_can_use_outside(func_mgr_get(),TREE_OPERAND(t,1)))
             return false;
      }else if(TREE_CODE(t)==VAR_DECL){
         tree type= TREE_TYPE(t);
         //代码 static ZeroOptInfo zeroOptimiation ZeroOptInfo定义在.c中，这种情況fwgb不能迁移
         if(TREE_CODE(type)==ENUMERAL_TYPE){
            tree typeName=TYPE_NAME(type);
            if(typeName && TREE_CODE(typeName)==TYPE_DECL){
               expanded_location xloc = expand_location (DECL_SOURCE_LOCATION (typeName));
               if (xloc.file){
                  const char *filename = lbasename (xloc.file);
                  printf("返回的变量是有名枚举，定义所在文件 %s\n",filename);
                  return !endswith(filename,".h");
               }
            }
         }
      }

      /* 函数内 static 局部变量 */
      if (TREE_CODE (t) == VAR_DECL
      && DECL_CONTEXT (t)
      && TREE_CODE (DECL_CONTEXT (t)) == FUNCTION_DECL
      && TREE_STATIC (t))
         return true;

      return false;
   };

   /* 检查类型是否为本单元私有（简化版：只检查主要情况） */
   auto is_local_type = [] (tree type) -> bool {
      if (!type)
         return false;
      type = TYPE_MAIN_VARIANT (type);

      /* 匿名 struct/union 或本地定义的类型较难精确判断，
      这里做保守处理：如果类型的名称上下文是当前函数，则认为私有 */
      if (TYPE_NAME (type)
      && TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
      && DECL_CONTEXT (TYPE_NAME (type))
      && TREE_CODE (DECL_CONTEXT (TYPE_NAME (type))) == FUNCTION_DECL)
         return true;

      return false;
   };
   //printf("can_safely_migrate_function 11:%s\n",IDENTIFIER_POINTER(DECL_NAME(fndecl)));
   /* 遍历所有 GIMPLE 语句 */
   basic_block bb;
   FOR_EACH_BB_FN (bb, fn){
      for (gimple_stmt_iterator gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         //aet_print_gimple_skip_debug(stmt);
           /* 检查语句中操作数 */
         for (unsigned i = 0; i < gimple_num_ops (stmt); ++i){
            tree op = gimple_op (stmt, i);
            if (!op)
               continue;
             /* 处理 SSA_NAME */
            if (TREE_CODE (op) == SSA_NAME)
               op = SSA_NAME_VAR (op);

            if (!op)
               continue;

            /* 检查变量/函数引用 */
            if ((DECL_P (op) || TREE_CODE(op)==COMPONENT_REF) && is_local_private (op,fndecl)){
               safe = false;
               break;
            }

            /* 检查类型 */
            if (is_local_type (TREE_TYPE (op))){
               safe = false;
               break;
            }
         }
         if (!safe)
            break;

         /* 额外检查 call */
         if (is_gimple_call (stmt)){
            tree callee = gimple_call_fndecl (stmt);
            if (callee && is_local_private (callee,fndecl)){
               safe = false;
               break;
            }
         }

         /* 检查是否取标签地址（computed goto） */
         if (gimple_code (stmt) == GIMPLE_ASSIGN || gimple_code (stmt) == GIMPLE_CALL){
            /* 简单检测 ADDR_EXPR of LABEL_DECL */
            for (unsigned i = 0; i < gimple_num_ops (stmt); ++i){
               tree op = gimple_op (stmt, i);
               if (op && TREE_CODE (op) == ADDR_EXPR){
                  tree base = TREE_OPERAND (op, 0);
                  if (base && TREE_CODE (base) == LABEL_DECL){
                     safe = false;
                     break;
                  }
               }
            }
         }
         if (!safe)
            break;
      }

      if (!safe)
         break;

      /* 检查 PHI 节点 */
      for (gphi_iterator gpi = gsi_start_phis (bb); !gsi_end_p (gpi); gsi_next (&gpi)){
         gphi *phi = gpi.phi ();
         for (unsigned i = 0; i < gimple_phi_num_args (phi); ++i){
            tree arg = gimple_phi_arg_def (phi, i);
            if (TREE_CODE (arg) == SSA_NAME)
               arg = SSA_NAME_VAR (arg);
            if (arg && DECL_P (arg) && is_local_private (arg,fndecl)){
               safe = false;
               break;
            }
         }
         if (!safe)
            break;
      }
      if (!safe)
         break;
   }
   return safe;
}

/**
 * 取开始到结束位置的源代码
 */
static char *aet_get_source_text(location_t start_loc, location_t end_loc)
{
    expanded_location start;
    expanded_location end;

    start = expand_location(start_loc);
    end   = expand_location(end_loc);

    if (!start.file || !end.file)
        return NULL;
    FILE *fp = fopen(start.file, "r");
    if (!fp)
        return NULL;
    size_t cap = 4096;
    size_t len = 0;
    char *result = XNEWVEC(char, cap);
    char line[4096];
    int line_no = 1;
    while (fgets(line, sizeof(line), fp)){
        if (line_no >= start.line &&  line_no <= end.line){
            char *begin = line;
            char *finish = line + strlen(line);
            /*
             * 第一行
             */
            if (line_no == start.line){
                begin = line + start.column - 1;
            }
            /*
             * 最后一行
             *
             * end.column 是结束字符的位置，
             * 所以需要 +1 包含该字符
             */
            if (line_no == end.line){
                finish = line + end.column;
            }

            size_t copy_len = finish - begin;
            if (len + copy_len + 1 >= cap){
                cap = (cap + copy_len + 4096) * 2;
                result = XRESIZEVEC(char, result,cap);
            }
            memcpy(result + len, begin,copy_len);
            len += copy_len;
        }
        if (line_no >= end.line)
            break;
        line_no++;
    }

    fclose(fp);
    result[len] = '\0';
    return result;
}

#define FUNC_WITH_GB_START "func_with_gb_start:"
#define FUNC_WITH_GB_END "func_with_gb_end"


static void freeFuncData(FuncWithGbData *data)
{
   free(data->file);
   free(data->className);
   free(data->code);
   free(data->mangleFunName);
   n_slice_free(FuncWithGbData,data);
}

static void funcDataToString(NString *codes,FuncWithGbData *item)
{
   n_string_append(codes,FUNC_WITH_GB_START);
   n_string_append(codes,"\n");
   n_string_append_printf(codes,"file=%s\n",item->file);
   n_string_append_printf(codes,"class=%s\n",item->className);
   n_string_append_printf(codes,"blocks=%d\n",item->blockCount);
   n_string_append_printf(codes,"firstNumber=%d\n",item->firstNumber);
   n_string_append_printf(codes,"mangleFunName=%s\n",item->mangleFunName);
   n_string_append_printf(codes,"code=%s\n",item->code);
   n_string_append(codes,FUNC_WITH_GB_END);
   n_string_append(codes,"\n");
}

/**
 * 注册插件回调函数
 * 移除不能外部调用的带泛型块的函数，并保存函数内容到文件
 * 处理编译单元元进入ipa前。
 * 有可能源代码中的函数经过gimple后被删除了，funcWithBlockArray中的函数在 FOR_EACH_FUNCTION_WITH_GIMPLE_BODY
 * 中没有，移走
 */
static void genericBlock_cb (void *event_data, void *data ATTRIBUTE_UNUSED)
{
   GenericParser *self = (GenericParser *)data;
   cgraph_node *node;
   FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node){
      tree fndecl = node->decl;
      int i;
      for(i=0;i<self->funcWithBlockArray->len;i++){
         FuncWithGbData *funcData=n_ptr_array_index(self->funcWithBlockArray,i);
         ClassFunc *func=funcData->func;
         //printf("genericBlock_cb 11 找到了速泛型块的函数 xx %s %p\n",func->orgiName,func->fromImplDefine);
         if(func->fromImplDefine == fndecl){
            bool can = can_safely_migrate_function(func,fndecl);
            n_debug("genericBlock_cb 11 找到了带泛型块的函数 11 %s 是否可外部使用:%d\n",func->orgiName,can);
            if(!can){
               n_ptr_array_remove(self->funcWithBlockArray,funcData);
               freeFuncData(funcData);
               i--;
            }
         }
      }
   }

   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   char newName[255];
   sprintf(newName,"%s.func_with_gb.o",objfile);
   //如果没有泛型块，移走原来的块代码文件
   if(self->funcWithBlockArray->len==0){
      remove(newName);//移走带泛型块的数据文件
      return;
   }
   NString *codes=n_string_new("");
   int i;
   for(i=0;i<self->funcWithBlockArray->len;i++){
      FuncWithGbData *item=n_ptr_array_index(self->funcWithBlockArray,i);
      funcDataToString(codes,item);
   }
   FILE *fp=fopen(newName,"w");
   int rx=fwrite(codes->str,1,codes->len,fp);
   fclose(fp);
   n_string_free(codes,TRUE);
   self->funcWithGBFileName=n_strdup(newName);
}

void generic_parser_register_fwg(GenericParser *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("genericparser.c generic_parser_register_fwg 是第二次编译返回。\n",in_fnames[0]);
      return;
   }

   n_debug("generic_parser_register_fwg 有泛型块的函数吗？ %d\n",self->funcWithBlockArray->len);
   if(self->funcWithBlockArray->len>0){
      static char *pluginName="clone_func_with_gb";
      flag_plugin_added = true;
      register_callback (pluginName, PLUGIN_ALL_IPA_PASSES_START,genericBlock_cb, (void*)self);
   }

}


static void printFuncData(FuncWithGbData *data)
{
   printf("file=%s\n",data->file);
   printf("class=%s\n",data->className);
   printf("blockCount=%d\n",data->blockCount);
   printf("firstNumber=%d\n",data->firstNumber);
   printf("mangleFunName=%s\n",data->mangleFunName);
   printf("codes=%s\n",data->code);
}

static char *createItem(char *content,char *tag,nboolean last)
{
   char *start=strstr(content,tag);
   char *n=start+strlen(tag);
   if(last)
      return n;
   char *end=strstr(n,"\n");
   int len=strlen(n);
   int remain=strlen(end);
   char *ret=xmalloc(len-remain+1);
   memcpy(ret,n,len-remain);
   ret[len-remain]='\0';
   return ret;
}

static FuncWithGbData *createFuncData(char *content)
{
   FuncWithGbData *data=n_slice_new0(FuncWithGbData);
   data->file=createItem(content,"file=",FALSE);
   data->className=createItem(content,"class=",FALSE);
   char *blockCount=createItem(content,"blocks=",FALSE);
   data->blockCount=atoi(blockCount);
   free(blockCount);
   char *firstNumber=createItem(content,"firstNumber=",FALSE);
   data->firstNumber=atoi(firstNumber);
   free(firstNumber);
   data->mangleFunName=createItem(content,"mangleFunName=",FALSE);
   char *code=createItem(content,"code=",TRUE);
   data->code=n_strdup(code);
   return data;
}

static void readData(char *content,NPtrArray *array)
{
   while(strstr(content,FUNC_WITH_GB_START)){
      char *c=content;
      char *start=strstr(c,FUNC_WITH_GB_START);
      char *n=start+strlen(FUNC_WITH_GB_START)+1;//加1跳过 CLASS_BLOCK_START 后的\n号
      char *end=strstr(n,FUNC_WITH_GB_END);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      FuncWithGbData *data=createFuncData(ret);
      n_ptr_array_add(array,data);
      free(ret);
      content = end+strlen(FUNC_WITH_GB_END);
   }
}

/**
 * 处于编译 ttemp_func_track_45.c 中
 * 编译单元中的泛型块函数的源代码，来自的源文件，类名，块数，序号
 */
static void readFuncWithGbCodes(GenericParser *self,char *localFileList)
{
   if(!localFileList || strlen(localFileList)==0)
      return;
   nchar **items=n_strsplit(localFileList,"\n",-1);
   int length= n_strv_length(items);
   int i;
   for(i=0;i<length;i++){
      char *fn=items[i];
      FILE *fp=fopen(fn,"r");
      if(fp){
         char buffer[1024*150];
         int rev=fread(buffer,1,1024*150,fp);
         if(rev>0){
            buffer[rev]='\0';
            readData(buffer,self->funcWithBlockArray);
         }
         fclose(fp);
      }
   }
   n_strfreev(items);
}

/* Expect the current token to be a #pragma.  Consume it and remember
   that we've begun parsing a pragma.  */
static void c_parser_consume_pragma (c_parser *parser)
{
   gcc_assert (!parser->in_pragma);
   gcc_assert (parser->tokens_avail >= 1);
   gcc_assert (parser->tokens[0].type == CPP_PRAGMA);
   if (parser->tokens != &parser->tokens_buf[0])
      parser->tokens++;
   else if (parser->tokens_avail >= 2){
      parser->tokens[0] = parser->tokens[1];
      if (parser->tokens_avail >= 3)
         parser->tokens[1] = parser->tokens[2];
   }
   parser->tokens_avail--;
   parser->in_pragma = true;
}

static void c_parser_skip_to_pragma_eol (c_parser *parser, bool error_if_not_eol = true)
{
  gcc_assert (parser->in_pragma);
  parser->in_pragma = false;

  if (error_if_not_eol && c_parser_peek_token (parser)->type != CPP_PRAGMA_EOL)
    c_parser_error (parser, "expected end of line");

  cpp_ttype token_type;
  do
    {
      c_token *token = c_parser_peek_token (parser);
      token_type = token->type;
      if (token_type == CPP_EOF)
   break;
      c_parser_consume_token (parser);
    }
  while (token_type != CPP_PRAGMA_EOL);

  parser->error = false;
}


/**
 * 如果const_decl的位置与fwgb中的token名字位置一样，把token替换为整形常数
 */
static char *replaceTokenByContDecl(GenericParser *self,c_token *token)
{
   if(token->type==CPP_NAME && token->id_kind == C_ID_ID){
      int i;
      for(i=0;i<self->constDeclArray->len;i++){
         ConstDeclData *item = n_ptr_array_index(self->constDeclArray,i);
         if(!strcmp(item->id,IDENTIFIER_POINTER(token->value))){
            expanded_location dloc;
            dloc = expand_location(token->location);
            expanded_location sloc;
            sloc = expand_location(item->loc);
            if(dloc.line==sloc.line && dloc.column==sloc.column){
               return item->number;
            }
         }
      }
   }
   return NULL;
}

/**
 * 原型来自aetparser.h
 */
static void c_parser_skip_to_end_of_block_or_statement (GenericParser *self,NString *codes)
{
   c_parser *parser = self->parser->parser;
   unsigned nesting_depth = 0;
   bool save_error = parser->error;
   enum cpp_ttype previewType=0;
   nboolean first = FALSE;
   while (true){
      c_token *token;
      /* Peek at the next token.  */
      token = c_parser_peek_token (parser);
      switch (token->type){
         case CPP_EOF:
            return;

         case CPP_PRAGMA_EOL:
            if (parser->in_pragma)
               return;
            break;

         case CPP_SEMICOLON:
            /* If the next token is a ';', we have reached the
            end of the statement.  */
            if (!nesting_depth){
               /* Consume the ';'.  */
               c_parser_consume_token (parser);
               goto finished;
            }
            break;

         case CPP_CLOSE_BRACE:
            /* If the next token is a non-nested '}', then we have
            reached the end of the current block.  */
            if (nesting_depth == 0 || --nesting_depth == 0){
               c_parser_consume_token (parser);
               //printf("完成最后的----\n");
               n_string_append(codes,"}\n");
               goto finished;
            }
            break;

         case CPP_OPEN_BRACE:
            /* If it the next token is a '{', then we are entering a new
            block.  Consume the entire block.  */
            ++nesting_depth;
            break;

         case CPP_PRAGMA:
            /* If we see a pragma, consume the whole thing at once.  We
            have some safeguards against consuming pragmas willy-nilly.
            Normally, we'd expect to be here with parser->error set,
            which disables these safeguards.  But it's possible to get
            here for secondary error recovery, after parser->error has
            been cleared.  */
            c_parser_consume_pragma (parser);
            c_parser_skip_to_pragma_eol (parser);
            parser->error = save_error;
            continue;
         default:
            break;
      }
      char *source=aet_utils_convert_token_to_string(token);
      if(token->type==CPP_NAME){
         if(strlen(source)==1 && source[0]>='A' && source[0]<='Z'){
            n_string_append(codes,"aet_generic_");
         }else{
            char *buf =replaceTokenByContDecl(self,token);
            if(buf)
               source = buf;
         }
      }
      n_string_append(codes,source);
      if(previewType==CPP_CLOSE_PAREN && token->type==CPP_SEMICOLON){
         n_string_append(codes,"\n");
      }else{
         n_string_append(codes," ");
      }
      previewType=token->type;
      c_parser_consume_token (parser);
   }
finished:
   parser->error = false;
}


static void restore(c_parser *parser,c_token *backups,int count)
{
   if(count==0)
      return;
   int tokenCount=parser->tokens_avail;
   if(tokenCount+count>AET_MAX_TOKEN){
      error("token太多了");
      return;
   }
   int i;
   for(i=0;i<count;i++){
      aet_utils_copy_token(&backups[i],&parser->tokens[i+tokenCount]);
   }
   parser->tokens_avail=tokenCount+count;
   //aet_print_token_in_parser("generic_call restore ------");
}

/**
 * 在 aet_generic_E push (....
 * 取push (之前的字符串，判断有没有函数返回类型
 * 如果源代码的函数返回有 E 只能取函数名，如果是类型可以取到类型
 * int push 返回就是 int push
 * E   push 返回只有  push
 */
static nboolean haveFunctionReturn(char *origName,char *src)
{
   char ret[256];
   sprintf(ret,"%s (",origName);
   if(strstr(src,ret)){
      char *s=strstr(src,ret);
      int before=strlen(src)-strlen(s);
      if(before==0)
         return FALSE;
      char str[before+1];
      memcpy(str,src,before);
      str[before]='\0';
      return TRUE;
   }
   return FALSE;
}

/**
 * 保存编译单元的带泛型块函数
 */
void   generic_parser_save_fwgb(GenericParser *self,ClassName *className)
{
   if(self->parser->isAet || makefile_parm_is_second_compile(makefile_parm_get()))
      return;
   NPtrArray    *array = func_mgr_get_funcs(func_mgr_get(),className);
   if(!array)
      return;
   GenericInfo *info = block_mgr_get_info (block_mgr_get(),className);

   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *func=n_ptr_array_index(array,i);
      if(class_func_have_generic_block(func) && !class_func_is_func_generic(func)){
           location_t startloc = DECL_SOURCE_LOCATION(DECL_RESULT(func->fromImplDefine));
           aet_print_location(startloc);
           location_t endloc = func->endLoc;
           aet_print_location(endloc);
           //从文件读出的数据有'\0',改到NString追加换行符，长度是字符长度，所以不会有'\0'符号存在
           char *sourcecode=aet_get_source_text(startloc,endloc);
           NString *re=n_string_new(sourcecode);
           n_string_append(re,"\n");
           c_token backups[30];
           int backCount=backupToken(self,backups);
           parser_help_set_forbidden(true);
           /* 1. 进入一个“虚拟文件”或生成内容的 map */
           const char *fake_name = "<injected>";   /* 或你自己的名字 */
           int line = LOCATION_LINE (startloc);
           linemap_add (line_table, LC_ENTER, /*sysp=*/0, fake_name, line);
           /* 可选：标记为系统头，减少警告 */
           /* cpp_make_system_header (parse_in, 1, 0); */
           /* 2. 压入 buffer */
           cpp_buffer *buf = cpp_push_buffer (parse_in,(const unsigned char *)re->str,re->len, true);
           NString *funcdefineSourceCode=n_string_new("");
           c_parser_skip_to_end_of_block_or_statement(self,funcdefineSourceCode);
           /* 4. 读完后弹出 */
           _cpp_pop_buffer (parse_in);
           linemap_add (line_table, LC_LEAVE, 0, NULL, 0);

           if(!haveFunctionReturn(func->orgiName,funcdefineSourceCode->str)){
               char *returnStr=class_util_get_return_type_name(func->fromImplDefine);
               n_string_prepend(funcdefineSourceCode," ");
               n_string_prepend(funcdefineSourceCode,returnStr);
           }
           restore(self->parser->parser,backups,backCount);
           parser_help_set_forbidden(FALSE);
           free(sourcecode);
           n_string_free(re,TRUE);
           FuncWithGbData *data=n_slice_new(FuncWithGbData);
           data->file = n_strdup(in_fnames[0]);
           data->className=n_strdup(className->sysName);
           data->blockCount =generic_info_get_block_count_by_belong(info,func->mangleFunName);
           GenericBlock *block = generic_info_get_first_block_by_belong(info,func->mangleFunName);
           data->firstNumber = block->index;
           data->code = n_string_free(funcdefineSourceCode,FALSE);
           data->mangleFunName=n_strdup(func->mangleFunName);
           n_debug("genericparser.c 取fwgb函数的源代码 func:%s src:%s\n",data->mangleFunName,data->code);
           data->func = func;
           n_ptr_array_add(self->funcWithBlockArray,data);
      }
   }
}

/**
 * 编译temp_func_track_45.c时进入这里,所以funcWithBlockArray长度是零
 * GCC_AET_FUNC_WITH_GB_LIST_PATH是一个文件名，内容是各个编译单元有带泛型块
 * 函数的文件名列表。在这里把分散的函数集中到一起了。
 */
void generic_parser_ready(GenericParser *self)
{
   char *fileName = getenv("GCC_AET_FUNC_WITH_GB_LIST_PATH");
   if(fileName==NULL ||strlen(fileName)==0){
      return;
   }
   gcc_assert(self->funcWithBlockArray->len==0);
   FILE *fp=fopen(fileName,"r");
   if(fp){
      char fileList[50*1024];
      int rev=fread(fileList,1,50*1024,fp);
      fclose(fp);
      fileList[rev]='\0';
      readFuncWithGbCodes(self,fileList);
   }
   //把本地的FuncWithGbData转存到localFwgbArray,因为库的FuncWithGbData也要到funcWithBlockArray
   int i;
   for(i=0;i<self->funcWithBlockArray->len;i++){
      FuncWithGbData *d=n_ptr_array_index(self->funcWithBlockArray,i);
      n_ptr_array_add(self->localFwgbArray,d);
   }
   n_debug("generic_parser_ready 所有带泛型块的函数所在文件的列表:%d\n",self->funcWithBlockArray->len);
   NPtrArray *funcDataFromlibFunc = aet_lib_get_func_with_gb(aet_lib_get());
   if(funcDataFromlibFunc && funcDataFromlibFunc->len>0){
      for(i=0;i<funcDataFromlibFunc->len;i++){
         FuncWithGbData *d=n_ptr_array_index(funcDataFromlibFunc,i);
         n_ptr_array_add(self->funcWithBlockArray,d);
      }
   }
   /*
   int i;
   for(i=0;i<self->funcWithBlockArray->len;i++){
      FuncWithGbData *item=n_ptr_array_index(self->funcWithBlockArray,i);
      printf("带泛型块的函数如下 i:%d\n",i);
      printFuncData(item);
   }
   */
}

#define FUNC_WITH_GB_CONTENT_START "func_with_gb_content_start:"
#define FUNC_WITH_GB_CONTENT_END    "func_with_gb_content_end"

/**
 * 编译temp_func_track_45.c时进入这里,获取整个项目的带泛型块函数的源代码
 */
char *generic_parser_get_fwg_source(GenericParser *self)
{
   if(self->localFwgbArray->len==0)
      return NULL;
   int i;
   NString *codes=n_string_new(FUNC_WITH_GB_CONTENT_START);
   n_string_append(codes,"\n");
   for(i=0;i<self->localFwgbArray->len;i++){
      FuncWithGbData *item=n_ptr_array_index(self->localFwgbArray,i);
      funcDataToString(codes,item);
   }
   n_string_append(codes,FUNC_WITH_GB_CONTENT_END);
   n_string_append(codes,"\n");
   return n_string_free(codes,FALSE);
}

/**
 * 由aetlib库调用,从库中生成FuncWithGbData
 */
NPtrArray  *generic_parser_create_fwg(char *content)
{
   if(strstr(content,FUNC_WITH_GB_CONTENT_START)){
      char *c=content;
      char *start=strstr(c,FUNC_WITH_GB_CONTENT_START);
      char *n=start+strlen(FUNC_WITH_GB_CONTENT_START)+1;//加1跳过 CLASS_BLOCK_START 后的\n号
      char *end=strstr(n,FUNC_WITH_GB_CONTENT_END);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      NPtrArray *array=n_ptr_array_new();
      readData(ret,array);
      free(ret);
      return array;
   }
   return NULL;
}

/**
 * 返回FuncWithGbData
 * sysName：带泛型块函数的类
 * data:FuncWithGbData指针数组
 */
int generic_parser_get_func(GenericParser *self,char *sysName,FuncWithGbData **data)
{
   int i;
   int count=0;
   //n_debug("generic_parser_get_func --- %d sysName:%s\n",self->funcWithBlockArray->len,sysName);
   for(i=0;i<self->funcWithBlockArray->len;i++){
      FuncWithGbData *item=n_ptr_array_index(self->funcWithBlockArray,i);
      if(strcmp(sysName,item->className)==0){
         data[count++]=item;
      }
   }
   return count;
}

/**
 * 根据泛型声明返回真实的类型,调用该方法应处于编译泛型块函数期。
 */
char *generic_parser_get_true_type(GenericParser *self,char *genStr,int *pointerCount)
{
   c_parser *parser=self->parser->parser;
   if(self->directiveCount==0){
      n_error("无泛型块函数的的泛型定义。%s",genStr);
   }
   int i;
   for(i=0;i<self->directiveCount;i++){
      Directive *d=self->directives[i];
      if(strcmp(d->declName,genStr)==0){
        *pointerCount = d->pointerCount;
        return d->defineTypeName;
      }
   }
   return NULL;
}

//获取index泛型的定义大小
int   generic_parser_get_true_type_size(GenericParser *self,int index)
{
   c_parser *parser=self->parser->parser;
   if(self->directiveCount==0){
      n_error("无泛型块函数的的泛型定义。");
   }
   if(index<0 || index>=self->directiveCount){
      n_error("无泛型块函数的的泛型定义。");
   }
   Directive *d=self->directives[index];
   return d->size;
}

/**
 * 是否是正在编译泛型块函数
 */
static nboolean atCompileGenericBlock(GenericParser *self)
{
   return (current_function_decl
      && aet_parser_is_generic_state(self->parser)
      && generic_util_is_block_func_name(IDENTIFIER_POINTER(DECL_NAME(current_function_decl))));
}

/**
 * 外部变量是 E queue，真实的类型 E = 值
 * 需要转化的类型如果是指针直接返回
 * queue = value value是值
 * queue 转成了 *((int*)queue)
 */
static tree convert_E_E_COMPONENT_REF(tree component,tree trueType,tree rhsType)
{
   if(rhsType && POINTER_TYPE_P(rhsType)){
      n_warning("应该报错 E = int 右值类型是指针。\n");
      return  component;
   }
   printf("第一种情况 E =int E queue queue=...\n");
   //queue = val;
   //-->变成 *queue = val;
   tree pointerType = build_pointer_type (trueType);
   //E queue 转成  (int*)queue
   tree casted = fold_build1 (NOP_EXPR,pointerType,component);
   //E queue变成 *((int*)queue)
   tree deref = fold_build1 (INDIRECT_REF, trueType,casted);
   return deref;
}

static tree convert_E_EP_COMPONENT_REF(tree component,tree trueType,tree rhsType)
{
   return component;
}

/**
 * 外部变量 E *,E 定义为值
 * queue = 指针，直接返回
 * queue = value value是值
 * queue 转成了 *((int*)queue)
 */
static tree convert_EP_E_COMPONENT_REF(tree component,tree trueType,tree rhsType)
{
   if(rhsType && POINTER_TYPE_P(rhsType)){
      return  component;
   }
   printf("第二种情况 E =int E *queue queue=...\n");
   //queue = val;
   //-->变成 *queue = val;
   tree pointerType = build_pointer_type (trueType);
   //E queue 转成  (int*)queue
   tree casted = fold_build1 (NOP_EXPR,pointerType,component);
   //E queue变成 *((int*)queue)
   tree deref = fold_build1 (INDIRECT_REF, trueType,casted);
   return deref;
}

/**
 * 如果rhsType不是指针由系统判断
 * E *queue
 * queue = value value是指针
 * 这种情况留系统判断
 */
static tree convert_EP_EP_COMPONENT_REF(tree compref,tree trueType,tree rhsType)
{
   printf("convert_EP_EP_COMPONENT_REF 转化左值到类型 11-- queue[0] lhs:%d\n",
         (rhsType && POINTER_TYPE_P(rhsType)));
   // 1. 构造 int **
   tree trueTypePointer = build_pointer_type (trueType);
   trueTypePointer = build_pointer_type (trueTypePointer);
   //2. void **queue -->(int **)queue;
   tree casted = fold_convert (trueTypePointer, compref);
   //3.再做间接引用，得到 int * (int**)queue --> *((int**)queue)
   //也可用 tree result = build1 (INDIRECT_REF, build_pointer_type (trueType), casted);
   tree result   = build_fold_indirect_ref (casted);    // *((int **)queue)
   //4.转lhs类型
   //if(lhsType && POINTER_TYPE_P(lhsType))
   //  result = fold_convert(lhsType, result);
   aet_print_tree(result);
   return result;
}

static tree castComponentRef(char *trueTypeName,int truePointer,
      tree component,int fieldPointer,tree rhs,tree rhsType)
{
   tree trueType = lookup_name(get_identifier(trueTypeName));
   if(TREE_CODE(trueType)==TYPE_DECL)
      trueType = TREE_TYPE(trueType);
   if(fieldPointer==0){//外部变量声明为 E queue
      if(truePointer == 0 )//真实类型是值
        return convert_E_E_COMPONENT_REF(component,trueType,rhsType);
      else
        return convert_E_EP_COMPONENT_REF(component,trueType,rhsType);
   }else if(fieldPointer==1){//外部变量声明为 E *queue
      if(truePointer == 0 )//真实类型是值
         return convert_EP_E_COMPONENT_REF(component,trueType,rhsType);
      else{
         //如果rhs是(void **)((char *)queue+...) //直接返回
         if(gneric_util_have_generic_type(rhs)){
            return component;
         }
         return convert_EP_EP_COMPONENT_REF(component,trueType,rhsType);
      }
   }
   return component;
}

static tree convert_E_E_INDIRECT_REF(tree indirect,tree trueType,tree rhsType)
{
   if(rhsType && POINTER_TYPE_P(rhsType)){
      n_error("void *类型数组不能被指针赋值  E = int 但右值类型是指针。\n");
      return  indirect;
   }
   //只能有一种形式 queue[0]
   tree compref = TREE_OPERAND(indirect,0);
   if(TREE_CODE(compref)==POINTER_PLUS_EXPR){
      printf("第一种情 E =int E queue queue[2] 或 queue[expr]=...\n");
      tree base = TREE_OPERAND (TREE_OPERAND (indirect, 0), 0);  // 原始 void*
      tree idx  = TREE_OPERAND (TREE_OPERAND (indirect, 0), 1);  // 原始下标
      // 先转成 int*
      tree trueTypePointer = fold_convert (build_pointer_type (trueType), base);
      // 再用指针 + 下标的方式生成（自动处理 * sizeof）
      tree result = build_array_ref (EXPR_LOCATION (indirect), trueTypePointer, idx);
      return result;
   }else if(TREE_CODE(compref)==COMPONENT_REF){
      printf("第一种情 E =int E queue queue[0]=...\n");
      tree queue_ptr = TREE_OPERAND (indirect, 0);   // 原来的 component_ref，类型是 void*
      // 1. 构造 int *
      tree trueTypePointer = build_pointer_type (trueType);
      // 2. 做强制类型转换
      tree casted = fold_convert (trueTypePointer, queue_ptr);
      // 或者更明确：
      // tree casted = build1 (CONVERT_EXPR, int_ptr_type, queue_ptr);
      // 或 NOP_EXPR（如果只是改类型、不改值）
      // 3. 再做间接引用，得到 int
      tree result = build1 (INDIRECT_REF, trueType, casted);
      // 也可以写成：
      // tree result = build_fold_indirect_ref (casted);
      aet_print_tree(result);
      return result;
   }else{
      aet_print_tree_skip_debug(indirect);
      n_error("在编译泛型块函数中的左值类型还未支持。");
   }
   return indirect;
}

static tree convert_EP_E_INDIRECT_REF(tree indirect,tree trueType,tree rhsType)
{
   if(rhsType && POINTER_TYPE_P(rhsType)){
      n_error("void ** 类型数组不能被指针赋值  E = int 但右值类型是指针。\n");
      return  indirect;
   }
   tree compref = TREE_OPERAND(indirect,0);
   if(TREE_CODE(compref)==POINTER_PLUS_EXPR){
      tree orig = indirect;

      tree plus   = TREE_OPERAND (orig, 0);          // POINTER_PLUS_EXPR
      tree base   = TREE_OPERAND (plus, 0);          // queue
      tree offset = TREE_OPERAND (plus, 1);          // 这里是 NOP_EXPR

      // 剥掉 NOP_EXPR / CONVERT_EXPR 等无意义的类型转换
      tree real_offset = offset;
      while (CONVERT_EXPR_P (real_offset) || TREE_CODE (real_offset) == NON_LVALUE_EXPR)
         real_offset = TREE_OPERAND (real_offset, 0);

      // 现在 real_offset 应该是 MULT_EXPR
      tree index;
      if (TREE_CODE (real_offset) == MULT_EXPR
      && TREE_CODE (TREE_OPERAND (real_offset, 1)) == INTEGER_CST
      && tree_to_uhwi (TREE_OPERAND (real_offset, 1)) == 8){
         index = TREE_OPERAND (real_offset, 0);     // 原始下标 expr
      } else if (TREE_CODE (real_offset) == INTEGER_CST){
         // 常量偏移情况
         index = size_int (tree_to_uhwi (real_offset) / 8);
      }else{
         // 兜底
         index = fold_build2 (EXACT_DIV_EXPR, sizetype,
                  fold_convert (sizetype, real_offset), size_int (8));
      }

      // 构造新的表达式
      tree int_ptr_type = build_pointer_type (integer_type_node);
      tree new_base     = fold_convert (int_ptr_type, base);
      tree result       = build_array_ref (EXPR_LOCATION (orig), new_base, index);
      return result;
   }else if(TREE_CODE(compref)==COMPONENT_REF){
      printf("第二种情 E =int E *queue queue[0]=...\n");
      tree queue_ptr = TREE_OPERAND (indirect, 0);   // 原来的 component_ref，类型是 void*
      // 1. 构造 int *
      tree trueTypePointer = build_pointer_type (trueType);
      // 2. 做强制类型转换
      tree casted = fold_convert (trueTypePointer, queue_ptr);
      // 或者更明确：
      // tree casted = build1 (CONVERT_EXPR, int_ptr_type, queue_ptr);
      // 或 NOP_EXPR（如果只是改类型、不改值）
      // 3. 再做间接引用，得到 int
      tree result = build1 (INDIRECT_REF, trueType, casted);
      // 也可以写成：
      // tree result = build_fold_indirect_ref (casted);
      aet_print_tree(result);
      return result;
   }else{
      aet_print_tree_skip_debug(indirect);
      n_error("在编译泛型块函数中的左值类型还未支持。");
   }
   return  indirect;
}

static tree castIndirectRef(char *trueTypeName,int truePointer,tree indirectref,int fieldPointer,tree rhsType)
{
   tree trueType = lookup_name(get_identifier(trueTypeName));
   if(TREE_CODE(trueType)==TYPE_DECL)
      trueType = TREE_TYPE(trueType);
   if(fieldPointer==0){//外部变量声明为 E queue
      if(truePointer == 0 )//真实类型是值
        return convert_E_E_INDIRECT_REF(indirectref,trueType,rhsType);
      else{
         return convert_E_E_INDIRECT_REF(indirectref,trueType,rhsType);
      }
   }else if(fieldPointer == 1){
      if(truePointer == 0 )//真实类型是值
         return convert_EP_E_INDIRECT_REF(indirectref,trueType,rhsType);
   }
   return indirectref;
}

static tree convertLhs(GenericParser *self,tree lhs,tree rhs,tree rhsType)
{
   if(TREE_CODE(lhs)!=COMPONENT_REF && TREE_CODE(lhs)!=INDIRECT_REF)
      return lhs;
   tree compref = lhs;
   if(TREE_CODE(lhs)==INDIRECT_REF)
      compref = TREE_OPERAND(lhs,0);
   if(TREE_CODE(compref)==POINTER_PLUS_EXPR)
      compref = TREE_OPERAND(compref,0);
   int fieldPointerLhs = 0;
   int p1 = getDirective(self,compref,&fieldPointerLhs);
   n_debug("convertLhs--- 泛型索引值:%d fieldPointerLhs:%d\n",p1,fieldPointerLhs);
   if(p1<0)
      return lhs;
   Directive *directive=(Directive *)self->directives[p1];
   if(TREE_CODE(lhs)==COMPONENT_REF)
     lhs = castComponentRef(directive->defineTypeName,directive->pointerCount,lhs,fieldPointerLhs,rhs,rhsType);
   else
     lhs = castIndirectRef(directive->defineTypeName,directive->pointerCount,lhs,fieldPointerLhs,rhsType);
   return lhs;
}

//修改泛型块函数中的左右值
void generic_parser_modify(GenericParser *self,tree *mlhs,tree *mrhs)
{
   if(!atCompileGenericBlock(self))
      return;
   tree lhs = *mlhs;
   tree rhs = *mrhs;
   tree newlhs = lhs;
   tree newrhs = rhs;
   n_debug("generic_parser_modify --- 00\n");
   aet_print_tree(lhs);
   newlhs = convertLhs(self,lhs,rhs,TREE_TYPE(rhs));
   if(lhs==newlhs){
      //如果左值没改变，可以判断右值
      newrhs = convertLhs(self,rhs,lhs,TREE_TYPE(lhs));
   }
   *mlhs = newlhs;
   *mrhs = newrhs;
}

void generic_parser_parm(GenericParser *self,vec<tree, va_gc> *params, vec<tree, va_gc> *origtypes)
{
   if(!atCompileGenericBlock(self) || !params || !origtypes)
        return;
   if(origtypes->length()!=params->length())
      return;
   int ix;
   tree arg;
   for (ix = 0; params->iterate (ix, &arg); ++ix){
      tree old = arg;
      tree type=(*origtypes)[ix] ;

      n_debug("参数----- ix:%d\n",ix);
      aet_print_tree(arg);
      aet_print_tree(type);
      //arg = convertLhs(self,arg,NULL_TREE,type);

      if(old!=arg){
         n_debug("参数----被改了- ix:%d\n",ix);
         (*params)[ix]=arg;
      }
   }
}

//在生成二元表达式前，转化变量
void generic_parser_binary_op(GenericParser *self,enum tree_code code,tree *blhs,tree *brhs)
{
    if(!atCompileGenericBlock(self))
        return;
     if(code!=LT_EXPR && code!=LE_EXPR && code!=GT_EXPR
           && code!=GE_EXPR && code!=EQ_EXPR && code!=NE_EXPR)
        return;
     tree lhs = *blhs;
     tree rhs = *brhs;
     tree newlhs = lhs;
     tree newrhs = rhs;
     n_debug("generic_parser_binary_op --- 00\n");
     aet_print_tree(lhs);
     newlhs = convertLhs(self,lhs,rhs,TREE_TYPE(rhs));
     if(lhs==newlhs){
        //如果左值没改变，可以判断右值
        newrhs = convertLhs(self,rhs,lhs,TREE_TYPE(lhs));
     }
     *blhs = newlhs;
     *brhs = newrhs;
}

//********************返回值的转化 非数组转化复用lhs -------------------------
static tree castComponentRefRhs(char *trueTypeName,int truePointer,tree component,int fieldPointer,tree lhsType)
{
   tree trueType = lookup_name(get_identifier(trueTypeName));
   if(TREE_CODE(trueType)==TYPE_DECL)
      trueType = TREE_TYPE(trueType);
   if(fieldPointer==0){//外部变量声明为 E queue
      if(truePointer == 0 )//真实类型是值
        return convert_E_E_COMPONENT_REF(component,trueType,lhsType);
      else
        return convert_E_EP_COMPONENT_REF(component,trueType,lhsType);
   }else if(fieldPointer==1){//外部变量声明为 E *queue
      if(truePointer == 0 )//真实类型是值
         return convert_EP_E_COMPONENT_REF(component,trueType,lhsType);
      else
         return convert_EP_EP_COMPONENT_REF(component,trueType,lhsType);
   }
   return component;
}


/**
 * 作为右值 E queue E = value
 * COMPONENT_REF queue->(int*)queue 如果返回不是指针，解引用 *((int*)queue)
 */
static tree convert_E_E_INDIRECT_REF_RHS(tree indirect,tree trueType,tree lhsType)
{
   tree compref = TREE_OPERAND(indirect,0);
   if(TREE_CODE(compref)==COMPONENT_REF){
      printf("convert_E_E_INDIRECT_REF_RHS 转化右值到类型 00 queue[0] lhs:%d\n",
            (lhsType && POINTER_TYPE_P(lhsType)));
      //相当于直接返回queue,就为queue本身就是void *
      tree trueTypePointer = build_pointer_type(trueType);
      tree casted = fold_convert(trueTypePointer, compref);
      tree result = NULL_TREE;
      if(lhsType && POINTER_TYPE_P(lhsType))
         result = fold_convert(lhsType, casted);
      else
         //trueType 是值相当于把 (int*)queue -->*((int*)queue)
         result = build1 (INDIRECT_REF, trueType, casted);
      return result;
   }else if(TREE_CODE(compref)==POINTER_PLUS_EXPR){
      printf("convert_E_E_INDIRECT_REF_RHS 转化右值到类型 11 queue[index] lhs:%d\n",
            (lhsType && POINTER_TYPE_P(lhsType)));
      /*
      NOP_EXPR / CONVERT          type: void*          ← 最终返回类型
      └── POINTER_PLUS_EXPR    type: int*
      op0: NOP/CONVERT   type: int*            ← (int*)self->queue
      └── COMPONENT_REF  self->queue
      op1: MULT_EXPR     (index * 4)
      注意：没有 最外层的 INDIRECT_REF。
      */
      tree queue   = TREE_OPERAND (compref, 0);          // queue
      tree idx = TREE_OPERAND (compref, 1);          // 这里是 NOP_EXPR
      // 剥掉 NOP_EXPR / CONVERT_EXPR 等无意义的类型转换
      tree index = idx;
      while (CONVERT_EXPR_P (index) || TREE_CODE (index) == NON_LVALUE_EXPR)
         index = TREE_OPERAND (index, 0);

      tree trueTypePointer = build_pointer_type(trueType);
      // 1. 转成 int*
      tree base = fold_convert(trueTypePointer, queue);

      //成为指针 queue[index]-->(int*)queue+index
      if(lhsType && POINTER_TYPE_P(lhsType)){
         // 2. index * sizeof(int)   ← 这里必须有 MULT
         tree size   = TYPE_SIZE_UNIT(trueType);          // 4
         tree offset = fold_build2(MULT_EXPR, sizetype,fold_convert(sizetype, index),size);
         // 3. (int*)self->queue + index
         tree result = fold_build2(POINTER_PLUS_EXPR, trueTypePointer, base, offset);
         // 4. 转成最终返回的 void*
         result = fold_convert(lhsType, result);
         aet_print_tree(result);
         return result;
      }else{
         tree result = build_array_ref (EXPR_LOCATION (indirect), base, index);
         return result;
      }
   }else{
      aet_print_tree_skip_debug(indirect);
      n_error("在编译泛型块函数中的左值类型还未支持。");
   }

   return indirect;
}

/**
 * E queue return queue[0] 或 return queue[index]
 * queue[0]-->(int **)queue+0
 * queue[index]-->(int **)queue+index*sizeof(pointer)
 * build_array_ref会根据类型大小乘与index
 * build_array_ref (EXPR_LOCATION (orig), new_base, index);
 * 所以得到的是指针。
 */
static tree convert_E_EP_INDIRECT_REF_RHS(tree indirect,tree trueType,tree lhsType)
{
   tree compref = TREE_OPERAND(indirect,0);
   if(TREE_CODE(compref)==COMPONENT_REF){
      printf("convert_E_EP_INDIRECT_REF_RHS 转化右值到类型 00 queue[0] lhs:%d\n",
               (lhsType && POINTER_TYPE_P(lhsType)));
      // 1. 构造 int**
      tree trueTypePointer = build_pointer_type (trueType);          // int*
      trueTypePointer      = build_pointer_type (trueTypePointer);       // int**
      // 2. 把 compref 转成 int**
      tree casted          = fold_convert (trueTypePointer, compref);
      // 3. 解引用，得到 int*
      tree result          = build1 (INDIRECT_REF, trueTypePointer, casted);
      //4.转lhs类型
      if(lhsType && POINTER_TYPE_P(lhsType))
         result = fold_convert(lhsType, result);
      return result;
   }else if(TREE_CODE(compref)==POINTER_PLUS_EXPR){
      printf("convert_E_EP_INDIRECT_REF_RHS 转化右值到类型 11 queue[index] lhs:%d\n",
               (lhsType && POINTER_TYPE_P(lhsType)));
      tree orig = indirect;
      tree plus = TREE_OPERAND (orig, 0);        // POINTER_PLUS_EXPR
      tree base = TREE_OPERAND (plus, 0);        // self->queue
      tree offset = TREE_OPERAND (plus, 1);      // 字节偏移

      // 剥掉可能的 NOP
      while (CONVERT_EXPR_P (offset) || TREE_CODE (offset) == NON_LVALUE_EXPR)
         offset = TREE_OPERAND (offset, 0);

      // 取原始下标（如果是 MULT * sizeof）
      tree index =offset;
      // 重新构造
      tree trueTypePointer = build_pointer_type (trueType);
      trueTypePointer  = build_pointer_type (trueTypePointer);

      tree new_base = fold_convert (trueTypePointer, base);
      tree result   = build_array_ref (EXPR_LOCATION (orig), new_base, index);
      //4.转lhs类型
      if(lhsType && POINTER_TYPE_P(lhsType))
         result = fold_convert(lhsType, result);
      return result;
   }else{
      aet_print_tree_skip_debug(indirect);
      n_error("在编译泛型块函数中的左值类型还未支持。");
   }
   return indirect;
}

/**
 * E *queue E = int
 */
static tree convert_EP_E_INDIRECT_REF_RHS(tree indirect,tree trueType,tree lhsType)
{
   tree compref = TREE_OPERAND(indirect,0);
   if(TREE_CODE(compref)==POINTER_PLUS_EXPR){
      printf("convert_EP_E_INDIRECT_REF_RHS 转化右值到类型 00 queue[index] lhs:%d\n",
               (lhsType && POINTER_TYPE_P(lhsType)));
      tree base = TREE_OPERAND (compref, 0); // queue
      tree offset = TREE_OPERAND (compref, 1);

      // 剥掉 NOP/CONVERT
      tree real_offset = offset;
      while (CONVERT_EXPR_P (real_offset) || TREE_CODE (real_offset) == NON_LVALUE_EXPR)
        real_offset = TREE_OPERAND (real_offset, 0);

      // 提取真正的逻辑下标
      tree index;
      if (TREE_CODE (real_offset) == MULT_EXPR)
        index = TREE_OPERAND (real_offset, 0);
      else if (TREE_CODE (real_offset) == INTEGER_CST)
        index = size_int (tree_to_uhwi (real_offset) / 8);
      else
        index = fold_build2 (EXACT_DIV_EXPR, sizetype,
                             fold_convert (sizetype, real_offset), size_int (8));

      // 转成目标指针类型
      tree trueTypePointer = build_pointer_type (trueType);
      tree new_base = fold_convert (trueTypePointer, base);

      // build_array_ref 已经返回 trueType 类型的左值（即 queue[index]）
      tree result = build_array_ref (EXPR_LOCATION (compref), new_base, index);

      // 根据需要的类型决定是否取地址或转换
      if (lhsType && POINTER_TYPE_P (lhsType)){
          // 需要指针（地址），取元素的地址
          // 或者更干净：直接构造 POINTER_PLUS，不走 array_ref
           tree new_offset = fold_build2 (MULT_EXPR, sizetype,
                                          fold_convert (sizetype, index),TYPE_SIZE_UNIT (trueType));
           result = fold_build2 (POINTER_PLUS_EXPR, trueTypePointer, new_base, new_offset);
        }else{
          // 需要值，直接用 build_array_ref 的结果即可，不要再 INDIRECT_REF
          // result 已经是 trueType
          if (lhsType)
            result = fold_convert (lhsType, result);
        }
      return result;
   }else if(TREE_CODE(compref)==COMPONENT_REF){
      printf("convert_EP_E_INDIRECT_REF_RHS 转化右值到类型 11 queue[0] lhs:%d\n",
               (lhsType && POINTER_TYPE_P(lhsType)));
      // 1. 构造 int *
      tree trueTypePointer = build_pointer_type (trueType);
      // 2. 做强制类型转换 void **queue-->(int*)queue;
      tree casted = fold_convert (trueTypePointer, compref);
      tree result = NULL_TREE;
      //4.转lhs类型
      if(lhsType && POINTER_TYPE_P(lhsType))
         result = fold_convert(lhsType, casted);
      else
      // 3. 再做间接引用，得到 int (int*)queue --> *((int*)queue)
       result = build1 (INDIRECT_REF, trueType, casted);
      // 也可以写成：
      // tree result = build_fold_indirect_ref (casted);
      aet_print_tree(result);
      return result;
   }else{
      aet_print_tree_skip_debug(indirect);
      n_error("在编译泛型块函数中的左值类型还未支持。");
   }
   return indirect;
}

/**
 * E *queue E = int *
 */
static tree convert_EP_EP_INDIRECT_REF_RHS(tree indirect,tree trueType,tree lhsType)
{
   tree compref = TREE_OPERAND(indirect,0);
   if(TREE_CODE(compref)==POINTER_PLUS_EXPR){
      printf("convert_EP_EP_INDIRECT_REF_RHS 转化右值到类型 00 queue[index] lhs:%d\n",
               (lhsType && POINTER_TYPE_P(lhsType)));
      tree base = TREE_OPERAND (compref, 0); // queue
      tree offset = TREE_OPERAND (compref, 1);

      // 剥掉 NOP/CONVERT
      tree real_offset = offset;
      while (CONVERT_EXPR_P (real_offset) || TREE_CODE (real_offset) == NON_LVALUE_EXPR)
        real_offset = TREE_OPERAND (real_offset, 0);

      // 提取真正的逻辑下标
      tree index;
      if (TREE_CODE (real_offset) == MULT_EXPR)
        index = TREE_OPERAND (real_offset, 0);
      else if (TREE_CODE (real_offset) == INTEGER_CST)
        index = size_int (tree_to_uhwi (real_offset) / 8);
      else
        index = fold_build2 (EXACT_DIV_EXPR, sizetype,
                             fold_convert (sizetype, real_offset), size_int (8));

      // 转成目标指针类型
      tree trueTypePointer = build_pointer_type (trueType);
      trueTypePointer = build_pointer_type (trueTypePointer);
      tree new_base = fold_convert (trueTypePointer, base);
      aet_print_tree(index);

      // build_array_ref 已经返回 trueType 类型的左值（即 queue[index]）
      tree result = build_array_ref (EXPR_LOCATION (compref), new_base, index);

      // 根据需要的类型决定是否取地址或转换
      if (lhsType && POINTER_TYPE_P (lhsType)){
         result = fold_convert (lhsType, result);
       }else{
          // 需要值，直接用 build_array_ref 的结果即可，不要再 INDIRECT_REF
          // result 已经是 trueType
          if (lhsType)
            result = fold_convert (lhsType, result);
       }
      return result;
   }else if(TREE_CODE(compref)==COMPONENT_REF){
      printf("convert_EP_EP_INDIRECT_REF_RHS 转化右值到类型 11-- queue[0] lhs:%d\n",
               (lhsType && POINTER_TYPE_P(lhsType)));
      // 1. 构造 int **
      tree trueTypePointer = build_pointer_type (trueType);
      trueTypePointer = build_pointer_type (trueTypePointer);
      //2. void **queue -->(int **)queue;
      tree casted = fold_convert (trueTypePointer, compref);
      //3.再做间接引用，得到 int * (int**)queue --> *((int**)queue)
      //也可用 tree result = build1 (INDIRECT_REF, build_pointer_type (trueType), casted);
      tree result   = build_fold_indirect_ref (casted);    // *((int **)queue)
      //4.转lhs类型
      if(lhsType && POINTER_TYPE_P(lhsType))
         result = fold_convert(lhsType, result);
      aet_print_tree(result);
      return result;
   }else{
      aet_print_tree_skip_debug(indirect);
      n_error("在编译泛型块函数中的左值类型还未支持。");
   }
   return indirect;
}

static tree castIndirectRefRhs(char *trueTypeName,int truePointer,tree indirectref,int fieldPointer,tree lhsType)
{
   tree trueType = lookup_name(get_identifier(trueTypeName));
   if(TREE_CODE(trueType)==TYPE_DECL)
      trueType = TREE_TYPE(trueType);
   if(fieldPointer==0){//外部变量声明为 E queue
      if(truePointer == 0 )//真实类型是值
        return convert_E_E_INDIRECT_REF_RHS(indirectref,trueType,lhsType);
      else{
         return convert_E_EP_INDIRECT_REF_RHS(indirectref,trueType,lhsType);
      }
   }else if(fieldPointer == 1){
      if(truePointer == 0 )//真实类型是值
         return convert_EP_E_INDIRECT_REF_RHS(indirectref,trueType,lhsType);
      else
         return convert_EP_EP_INDIRECT_REF_RHS(indirectref,trueType,lhsType);

   }
   return indirectref;
}

/**
 * 转化右值匹配，左值
 */
static tree convertRhs(GenericParser *self,tree rhs,tree lhsType)
{
   if(TREE_CODE(rhs)!=COMPONENT_REF && TREE_CODE(rhs)!=INDIRECT_REF)
      return rhs;
   tree compref = rhs;
   if(TREE_CODE(rhs)==INDIRECT_REF)
      compref = TREE_OPERAND(rhs,0);
   if(TREE_CODE(compref)==POINTER_PLUS_EXPR)
      compref = TREE_OPERAND(compref,0);
   int fieldPointerLhs = 0;
   int p1 = getDirective(self,compref,&fieldPointerLhs);
   n_debug("convertLhs--- 泛型索引值:%d fieldPointerLhs:%d\n",p1,fieldPointerLhs);
   if(p1<0)
      return rhs;
   Directive *directive=(Directive *)self->directives[p1];
   if(TREE_CODE(rhs)==COMPONENT_REF)
      rhs = castComponentRefRhs(directive->defineTypeName,directive->pointerCount,rhs,fieldPointerLhs,lhsType);
   else
      rhs = castIndirectRefRhs(directive->defineTypeName,directive->pointerCount,rhs,fieldPointerLhs,lhsType);
   return rhs;
}


//判断返回表达式是否需要转化
void generic_parser_return (GenericParser *self,tree *expr)
{
   if(!atCompileGenericBlock(self) || !expr)
       return;
   tree rhs = *expr;
   tree type = TREE_TYPE(TREE_TYPE(current_function_decl));
   tree newrhs = convertRhs(self,rhs,type);
   *expr = newrhs;
}

/**
 * 在c_parser_initializer调用该方法
 * decl相当于左值，但不能去做转化
 * init做转化来匹配，因为在泛型块函数中 decl是一个确定的类型，不可有泛型声明存在。
 */
tree generic_parser_initializer(GenericParser *self,tree decl,tree init)
{
   if(!atCompileGenericBlock(self) || !decl || !init)
         return init;
   init = convertRhs(self,init,TREE_TYPE(decl));
   return init;
}


// 核心逻辑：遍历 AST 节点的隐式回调函数
static tree walk_ast_cb(tree *tp, int *walk_subtrees, void *data) {
    tree t = *tp;
    if (!t) return NULL_TREE;

    // 关键点 1：寻找代码中的常量节点 (如被替换后的 0, 1, 2)
    if (TREE_CODE(t) == INTEGER_CST) {
        // 获取该常量的类型节点
        tree type = TREE_TYPE(t);
        aet_print_tree(t);

        // 关键点 2：判断这个常量的底层类型是否为枚举 (ENUMERAL_TYPE)
        if (type && TREE_CODE(type) == ENUMERAL_TYPE) {
            // 获取该枚举类型的类型声明节点 (TYPE_DECL)
            tree type_decl = TYPE_NAME(type);
            printf("是不是enumeral----\n");

            if (type_decl && TREE_CODE(type_decl) == TYPE_DECL) {
                // 关键点 3：获取该枚举定义在源码中的物理位置 (Location)
                location_t loc = DECL_SOURCE_LOCATION(type_decl);
                const char *file = LOCATION_FILE(loc);

                // 关键点 4：获取当前正在编译的主输入文件名 (.c 文件)
                const char *main_file = main_input_filename;

                // 如果枚举定义的文件名和当前编译的 .c 文件名一致，说明是本地枚举
                if (file && main_file && strcmp(file, main_file) == 0) {
                    // 打印迁移提示信息
                    const char *enum_name = IDENTIFIER_POINTER(DECL_NAME(type_decl));
                    if (!enum_name) enum_name = "anonymous enum"; // 处理匿名枚举

                    inform(EXPR_LOCATION(t),
                           "发现不可直接迁移的本地枚举依赖: 常量值 %ld 来自定义于 [%s:%d] 的 '%s'",
                           (long)TREE_INT_CST_LOW(t), file, LOCATION_LINE(loc), enum_name);
                }
            }
        }
    }

    return NULL_TREE;
}

// 每一个函数被解析完、准备转为 GENERIC 树时触发的回调
static void on_pre_genericize_cb(void *event_data, void *data) {
    tree fndecl = (tree)event_data;
    GenericParser *self = (GenericParser *)data;
    if(!self->isAddBlock)
       return;
    self->isAddBlock = FALSE;
    printf("on_pre_genericize_cb ----\n");
    if (TREE_CODE(fndecl) == FUNCTION_DECL) {
        // 获取当前函数的函数体
        tree body = DECL_SAVED_TREE(fndecl);
        printf("on_pre_genericize_cb -xxx--- body:%p\n",body);

        if (body) {
            // 使用 GCC 内置的 walk_tree 深度优先遍历函数体 AST
            walk_tree(&body, walk_ast_cb, NULL, NULL);
        }
    }
}

/**
 * 记录fwgb函数中的const_decl
 */
void generic_parser_record_const_decl(GenericParser *self,location_t loc,tree id,tree ref)
{
   c_parser *parser=self->parser->parser;
   if(!self->parser->isAet || TREE_CODE(ref)!=INTEGER_CST)
      return;
   tree decl = lookup_name (id);
   if(TREE_CODE(decl)==CONST_DECL){
      ConstDeclData *data=n_slice_new0(ConstDeclData);
      data->loc = loc;
      data->id = IDENTIFIER_POINTER(id);
      data ->func = current_function_decl;
      data->number=xmalloc(128);
      signop sgn = TYPE_UNSIGNED (TREE_TYPE (ref)) ? UNSIGNED : SIGNED;
      wide_int w = wi::to_wide (ref);
      print_dec (w, data->number, sgn);
      n_debug("genericparser.c 加入 const_decl id:%s val:%s\n",data->id,data->number);
      n_ptr_array_add(self->constDeclArray,data);
   }
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

