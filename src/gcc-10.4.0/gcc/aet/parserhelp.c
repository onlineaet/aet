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
#include "asan.h"
#include "opts.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aetutils.h"
#include "classmgr.h"
#include "parserhelp.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "classparser.h"
#include "parserstatic.h"
#include "classutil.h"
#include "aet-c-parser-header.h"
#include "classaccess.h"
#include "enumparser.h"


#define VOID_POINTER 1
#define BASE_POINTER 2
#define LAST_VOID_PARM 3

#define UNKNOWN_POINTER 0

static nboolean forbidden=FALSE;//禁止改变token

static inline tree canonicalize_for_substitution (tree node)
{
  /* For a TYPE_DECL, use the type instead.  */
  if (TREE_CODE (node) == TYPE_DECL)
    node = TREE_TYPE (node);
  if (TYPE_P (node)  && TYPE_CANONICAL (node) != node   && TYPE_MAIN_VARIANT (node) != node){
      tree orig = node;
      if (TREE_CODE (node) == FUNCTION_TYPE)
	      node = build_qualified_type (TYPE_MAIN_VARIANT (node),TYPE_QUALS (node));
  }
  return node;
}

static int getQualCount(const tree type)
{
  int num_qualifiers = 0;
  int quals = TYPE_QUALS (type);
  if (quals & TYPE_QUAL_RESTRICT)
    {
      ++num_qualifiers;
    }
  if (quals & TYPE_QUAL_VOLATILE)
    {
      ++num_qualifiers;
    }
  if (quals & TYPE_QUAL_CONST)
    {
      ++num_qualifiers;
    }

  return num_qualifiers;
}

static nboolean compareQual(tree parm1,tree parm2)
{
	tree c1=canonicalize_for_substitution(parm1);
	tree c2=canonicalize_for_substitution(parm2);
	int q1 = TYPE_QUALS (c1);
	int q2 = TYPE_QUALS (c2);
	nboolean q1r=(q1 & TYPE_QUAL_RESTRICT);
	nboolean q2r=(q2 & TYPE_QUAL_RESTRICT);
	nboolean q1v=(q1 & TYPE_QUAL_VOLATILE);
	nboolean q2v=(q2 & TYPE_QUAL_VOLATILE);
	nboolean q1c=(q1 & TYPE_QUAL_CONST);
	nboolean q2c=(q2 & TYPE_QUAL_CONST);
	nboolean re=(q1r==q2r &&  q1v==q2v &&  q1c==q2c);
	return re;
}

static int getParmCount(tree funcType)
{
	int count=0;
	for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
	    count++;
	}
	return count;
}

static tree getParm(tree funcType,int index)
{
	int count=0;
	for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
	    tree parm = TREE_VALUE (al);
		if(count==index)
			return parm;
	    count++;
	}
	return NULL_TREE;
}


//两咱类型
//void *
static int getType(tree type)
{
  /* This gets set to nonzero if TYPE turns out to be a (possibly
     CV-qualified) builtin type.  */
   int is_builtin_type = 0;
   if (type == error_mark_node)
      return UNKNOWN_POINTER;
   type = canonicalize_for_substitution (type);
   if (getQualCount (type) > 0){
	  //printf("进这里 00 code:%s\n",get_tree_code_name(TREE_CODE(type)));
	  tree t = TYPE_MAIN_VARIANT (type);
	  //printf("进这里 11 code:%s code:%s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(t)));
	  type = TYPE_MAIN_VARIANT (t);
   }else{
      type = TYPE_MAIN_VARIANT (type);
   }
   if(TREE_CODE (type)==VOID_TYPE){
	   return LAST_VOID_PARM;
   }

   if(TREE_CODE (type)==POINTER_TYPE){
	 tree target = TREE_TYPE (type);
	 if(TREE_CODE (target)==VOID_TYPE){
		 return VOID_POINTER;
	 }else{
		 switch (TREE_CODE (target)){
		   case VOID_TYPE:
		   case BOOLEAN_TYPE:
		   case INTEGER_TYPE:  /* Includes wchar_t.  */
		   case REAL_TYPE:
		   case FIXED_POINT_TYPE:
		   case COMPLEX_TYPE:
		   case UNION_TYPE:
		   case RECORD_TYPE:
		   case ENUMERAL_TYPE:
			  return BASE_POINTER;
		}
	 }
   }
   return UNKNOWN_POINTER;
}

/**
 * 比较两个函数的参数是否相同
 * 见AHashTable
 */
nboolean parser_help_compare(tree funcType1,tree funcType2)
{
	int count1=getParmCount(funcType1);
	int count2=getParmCount(funcType2);
	if(count1!=count2)
		return FALSE;
	if(count1==0)
		return TRUE;
	int i;
	for(i=0;i<count1;i++){
		tree parm1=getParm(funcType1,i);
		tree parm2=getParm(funcType2,i);
		if(parm1==NULL_TREE || parm2==NULL_TREE )
			return FALSE;
		nboolean qual=compareQual(parm1,parm2);
		if(!qual)
			return FALSE;
		 int t1=getType(parm1);
		 int t2=getType(parm2);
		 if(t1==UNKNOWN_POINTER || t2==UNKNOWN_POINTER)
			 return FALSE;
	}
	return TRUE;
}

/**
 * 为类声明和接口声明加一个变量作用魔数
 * 并作为第一个参数。
 * private$ int _aet_magic$_123;
 */
void     parser_help_add_magic()
{
      ClassParser *classParser=class_parser_get();
      c_parser *parser=classParser->parser;
      c_token *token = c_parser_peek_token (parser);//
      int tokenCount=parser->tokens_avail;
      if(tokenCount+4>AET_MAX_TOKEN){
            error("token太多了");
            return FALSE;
      }
      int i;
      for(i=tokenCount;i>0;i--){
          aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+4]);
      }
      aet_utils_create_private$_token(&parser->tokens[0],input_location);
      aet_utils_create_int_token(&parser->tokens[1],input_location);
      aet_utils_create_token(&parser->tokens[2],CPP_NAME,AET_MAGIC_NAME,strlen(AET_MAGIC_NAME));
      aet_utils_create_token(&parser->tokens[3],CPP_SEMICOLON,";",1);
      parser->tokens_avail=tokenCount+4;
      aet_print_token_in_parser("parser_help_add_magic ---- ",classParser->currentClassName->sysName);
}

void parser_help_set_forbidden(nboolean is)
{
    forbidden=is;
}


/**
 * 把c_token 中的value设为类的sysName名
 * id_kind 设为 C_ID_TYPENAME; //描述类型的标识符
 * 由c-parser.c调用
 * 当参数是类名，找到当前类名，取系统名替换原来的类名。
 * 如who=AObject 替换成who=test_AObject
 * 如果当前编译文件中有两个相同的类型会引起冲突。如何解决，根据上下文决定选那一个，上下文？？
 * 比如:
 *  include "ARandom.h"
 *  include "debug/repeat/ARandom.h"
 *  debug/repeat/ARandom.h中的ARandom会被改成上一次加入的debug_ARandom
 */
nboolean parser_help_set_class_or_enum_type(c_token *who)
{
    if(forbidden){
        //printf("parser_help_set_class_or_enum_type 禁止的.\n",IDENTIFIER_POINTER(who->value));
        return FALSE;
    }
    tree value=who->value;
    char *id=IDENTIFIER_POINTER(value);
    ClassName *className=class_mgr_get_class_name_by_user(class_mgr_get(),id);
    nboolean result=FALSE;
    if(className!=NULL){
       if(strcmp(className->sysName,className->userName)==0){
           //if(strstr(id,"ARandom") || strstr(id,"ATxy") || strstr(id,"DataType"))
            // printf("parser_help_set_class_or_enum_type 找到的class 包名是空的，不应该出现这种情况:%s %s\n",className->sysName,className->userName);
         result= FALSE;
       }else{
          tree newValue=aet_utils_create_ident(className->sysName);
          who->value=newValue;
          //if(strstr(id,"ARandom") || strstr(id,"ATxy") || strstr(id,"DataType"))
            // printf("parser_help_set_class_or_enum_type 找到的class 改成className->sysName:%s id:%s\n",className->sysName,id);
          result= TRUE;
       }
    }else{
         //检查是不是类中声明的枚举。如果是，也需要c-parser.c设为ID_TYPENAME
        // if(strstr(id,"ARandom") || strstr(id,"ATxy") || strstr(id,"DataType"))
           // printf("parser_help_set_class_or_enum_type 找不到className 进枚举中找。%s\n",id);
         result= enum_parser_set_enum_type(enum_parser_get(),who);
    }
    if(result){
       tree  decl = lookup_name (who->value);
       if(decl){
            if (TREE_CODE (decl) == TYPE_DECL){
               // if(strstr(id,"ARandom") || strstr(id,"ATxy"))
               // printf("parser_help_set_class_or_enum_type 22 通过lookup_name 这是一个类型名  decl:%p value:%s\n",decl,IDENTIFIER_POINTER(who->value));
                who->id_kind = C_ID_TYPENAME; //描述类型的标识符
                return TRUE;
            }else if(TREE_CODE (decl) == FUNCTION_DECL){
                char *name=IDENTIFIER_POINTER(DECL_NAME(decl));
                if(!strcmp(name,className->sysName)){
                   // if(strstr(id,"ARandom") || strstr(id,"ATxy"))
                    //printf("parser_help_set_class_or_enum_type 33 通过lookup_name 这是一个构造函数名  decl:%p value:%s\n",decl,IDENTIFIER_POINTER(who->value));
                    who->id_kind = C_ID_TYPENAME; //描述类型的标识符
                    return TRUE;
                }
            }
       }
    }
    return FALSE;
}

/**
 * 右值，只分析到有类名就替换。
 */
static nboolean packageDotClass(char *firstId,int start)
{
       ClassParser *classParser=class_parser_get();
       c_parser *parser=classParser->parser;
       parser_help_set_forbidden(TRUE);
       int count=start;//当前c_parser_peek_token (parser)->type=CPP_DOT
       NString *str=n_string_new(firstId);
       n_string_append(str,".");
       nboolean result=FALSE;
       while(TRUE){
           c_token *token1=c_parser_peek_nth_token (parser,count++);
           if(token1->type==CPP_NAME){
               n_string_append(str,IDENTIFIER_POINTER(token1->value));
               NString *temp=n_string_new(str->str);
               n_string_replace(temp,".","_",-1);
               ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),temp->str);
               if(info!=NULL){
                   //printf("找到了-----%s\n",temp->str);
                   n_string_free(temp,TRUE);
                   break;
               }
               n_string_free(temp,TRUE);
               c_token *token2=c_parser_peek_nth_token (parser,count++);
               if(token2->type==CPP_DOT){
                   n_string_append(str,".");
                   continue;
               }else{
                  // printf("count+1已不是dot了。count:%d\n",count);
                  // aet_print_token(token2);
                   break;
               }
           }else{
               break;
           }
       }
      // printf("parser_help_parser_right_package_dot_class 00 end:%d %s\n",count,str->str);
       if(!n_string_ends_with(str,".")){
           n_string_replace(str,".","_",-1);
           ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),str->str);
          // printf("parser_help_parser_right_package_dot_class 11 end:%d %s info:%p\n",count,str->str,info);
           if(info!=NULL){
               int i;
               for(i=0;i<count-2;i++){
                  c_parser_consume_token (parser);
               }
               c_token *token=c_parser_peek_token (parser);
               //printf("保留的最后一个token\n");
              // aet_print_token(token);
               tree newValue=aet_utils_create_ident(str->str);
               token->value=newValue;
               token->id_kind = C_ID_TYPENAME; //描述类型的标识符
               result=TRUE;
           }
       }
       n_string_free(str,TRUE);
       parser_help_set_forbidden(FALSE);
       return result;
}

/**
 * 解析 com.ai.NLayer
 */
nboolean parser_help_parser_left_package_dot_class()
{
      ClassParser *classParser=class_parser_get();
      c_parser *parser=classParser->parser;
      c_token *token=c_parser_peek_token (parser);
      return packageDotClass(IDENTIFIER_POINTER(token->value),3);
}


nboolean parser_help_parser_right_package_dot_class(char *firstId)
{
      return packageDotClass(firstId,2);
}

