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
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "toplev.h"
#include "asan.h"
#include "opts.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "tree-iterator.h"
#include "c/c-parser.h"
#include "fold-const.h"
#include "langhooks.h"
#include "aetutils.h"
#include "classmgr.h"
#include "parserhelp.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "aetprinttree.h"
#include "aet-typeck.h"
#include "aet-c-fold.h"
#include "classutil.h"
#include "genericutil.h"
#include "funcmgr.h"
#include "classimpl.h"


#define GENERIC_BLOCK_TO_FUNC_PREFIX "_inner_generic_func"

static char *genericIdentifier[]={"aet_generic_A","aet_generic_B","aet_generic_C","aet_generic_D","aet_generic_E",
		"aet_generic_F","aet_generic_G","aet_generic_H","aet_generic_I","aet_generic_J",
		"aet_generic_K","aet_generic_L","aet_generic_M","aet_generic_N","aet_generic_O",
		"aet_generic_P","aet_generic_Q","aet_generic_R","aet_generic_S","aet_generic_T",
		"aet_generic_U","aet_generic_V","aet_generic_W","aet_generic_X","aet_generic_Y",
		"aet_generic_Z"};


nboolean generic_util_is_generic_ident(char *name)
{
   if(name==NULL)
	   return FALSE;
   int i;
   for(i=0;i<26;i++){
	   if(strcmp(genericIdentifier[i],name)==0)
		   return TRUE;
   }
   return FALSE;
}
/**
 * 判断type 是不是 aet_generic_A aet_generic_B等
 */
nboolean  generic_util_is_generic_pointer(tree type)
{
    if(TREE_CODE(type)!=POINTER_TYPE)
    	return FALSE;
    tree typeName=TYPE_NAME(type);
    tree mainVar=TYPE_MAIN_VARIANT (type);
    tree ptd=TREE_TYPE (type);
    if(!aet_utils_valid_tree(typeName) || !aet_utils_valid_tree(typeName)|| !aet_utils_valid_tree(typeName))
    	return FALSE;
    if(TREE_CODE(typeName)!=TYPE_DECL)
    	return FALSE;
    if(TREE_CODE(mainVar)!=POINTER_TYPE)
    	return FALSE;
    if(TREE_CODE(ptd)!=VOID_TYPE)
    	return FALSE;
    tree declName=DECL_NAME(typeName);
    if(!aet_utils_valid_tree(declName))
     	return FALSE;
    char *name=IDENTIFIER_POINTER(declName);
    if(!generic_util_is_generic_ident(name))
       return FALSE;
    return TRUE;
}


/**
 * 获取泛型的字符串A-Z。如:E、F
 * type:类声明和定义中函数声明或定义的参数。
 */
char *generic_util_get_generic_str(tree type)
{
	if(!aet_utils_valid_tree(type))
		return NULL;
    if(TREE_CODE(type)!=POINTER_TYPE)
    	return NULL;
    tree typeName=TYPE_NAME(type);
    tree mainVar=TYPE_MAIN_VARIANT (type);
    tree ptd=TREE_TYPE (type);
    if(!aet_utils_valid_tree(typeName) || !aet_utils_valid_tree(mainVar)|| !aet_utils_valid_tree(ptd))
    	return NULL;
    if(TREE_CODE(typeName)!=TYPE_DECL)
    	return NULL;
    if(TREE_CODE(mainVar)!=POINTER_TYPE)
    	return NULL;
    if(TREE_CODE(ptd)!=VOID_TYPE)
    	return NULL;
    tree declName=DECL_NAME(typeName);
    if(!aet_utils_valid_tree(declName))
     	return NULL;
    char *name=IDENTIFIER_POINTER(declName);
    if(!generic_util_is_generic_ident(name))
       return NULL;
    char ax=name[strlen(name)-1];
    char *re=(char*)n_malloc(2);
    re[0]=ax;
    re[1]='\0';
    return re;
}

/**
 * 类型对应的枚举
 */
int generic_util_get_generic_type(tree type)
{
	int gt=-1;
	if(TREE_CODE(type)==POINTER_TYPE)
	  return generic_util_get_generic_type(TREE_TYPE(type));
	if(TREE_CODE(type)==INTEGER_TYPE){
		int precision= TYPE_PRECISION (type);
		int unsig=TYPE_UNSIGNED (type);
		if(precision==CHAR_TYPE_SIZE){
			gt=unsig==1?GENERIC_TYPE_UCHAR:GENERIC_TYPE_CHAR;
		}else if(precision==SHORT_TYPE_SIZE){
			gt=unsig==1?GENERIC_TYPE_USHORT:GENERIC_TYPE_SHORT;
		}else if(precision==INT_TYPE_SIZE){
			gt=unsig==1?GENERIC_TYPE_UINT:GENERIC_TYPE_INT;
		}else if(precision==LONG_TYPE_SIZE){
		    gt=unsig==1?GENERIC_TYPE_ULONG:GENERIC_TYPE_LONG;
		}else if(precision==LONG_LONG_TYPE_SIZE){
		    gt=unsig==1?GENERIC_TYPE_ULONG_LONG:GENERIC_TYPE_LONG_LONG;
	    }
	}else if(TREE_CODE(type)==REAL_TYPE){
		int precision= TYPE_PRECISION (type);
		if(precision==FLOAT_TYPE_SIZE){
			gt=GENERIC_TYPE_FLOAT;
	    }else if(precision==DOUBLE_TYPE_SIZE){
			gt=GENERIC_TYPE_DOUBLE;
	    }else if(precision==LONG_DOUBLE_TYPE_SIZE){
			gt=GENERIC_TYPE_LONG_DOUBLE;
		}else if(precision==DECIMAL32_TYPE_SIZE){
			gt=GENERIC_TYPE_DECIMAL32_FLOAT;
		}else if(precision==DECIMAL64_TYPE_SIZE){
			gt=GENERIC_TYPE_DECIMAL64_FLOAT;
		}else if(precision==DECIMAL128_TYPE_SIZE){
			gt=GENERIC_TYPE_DECIMAL128_FLOAT;
		}
	}else if(TREE_CODE(type)==COMPLEX_TYPE){
		tree t1=TREE_TYPE(type);
		if(TREE_CODE(t1)==INTEGER_TYPE){
			int precision= TYPE_PRECISION (t1);
			int unsig=TYPE_UNSIGNED (t1);
			if(precision==CHAR_TYPE_SIZE){
				gt=unsig==1?GENERIC_TYPE_COMPLEX_UCHAR:GENERIC_TYPE_COMPLEX_CHAR;
			}else if(precision==SHORT_TYPE_SIZE){
				gt=unsig==1?GENERIC_TYPE_COMPLEX_USHORT:GENERIC_TYPE_COMPLEX_SHORT;
			}else if(precision==INT_TYPE_SIZE){
				gt=unsig==1?GENERIC_TYPE_COMPLEX_UINT:GENERIC_TYPE_COMPLEX_INT;
			}else if(precision==LONG_TYPE_SIZE){
				gt=unsig==1?GENERIC_TYPE_COMPLEX_ULONG:GENERIC_TYPE_COMPLEX_LONG;
			}else if(precision==LONG_LONG_TYPE_SIZE){
				gt=unsig==1?GENERIC_TYPE_COMPLEX_ULONG_LONG:GENERIC_TYPE_COMPLEX_LONG_LONG;
			}
		}else if(TREE_CODE(t1)==REAL_TYPE){
			int precision= TYPE_PRECISION (t1);
			if(precision==FLOAT_TYPE_SIZE){
				gt=GENERIC_TYPE_COMPLEX_FLOAT;
			}else if(precision==DOUBLE_TYPE_SIZE){
				gt=GENERIC_TYPE_COMPLEX_DOUBLE;
			}else if(precision==LONG_DOUBLE_TYPE_SIZE){
				gt=GENERIC_TYPE_COMPLEX_LONG_DOUBLE;
			}
		}
	}else if(TREE_CODE(type)==ENUMERAL_TYPE){
		gt=GENERIC_TYPE_ENUMERAL;
	}else if(TREE_CODE(type)==BOOLEAN_TYPE){
			gt=GENERIC_TYPE_BOOLEAN;
	}else if(TREE_CODE(type)==FIXED_POINT_TYPE){
		gt=GENERIC_TYPE_FIXED_POINT;
	}else if(TREE_CODE(type)==RECORD_TYPE){
		char *className=class_util_get_class_name_by_record(type);
		gt=className!=NULL?GENERIC_TYPE_CLASS:GENERIC_TYPE_STRUCT;
	}
	return gt;
}

int generic_util_get_generic_type_name(tree type,char *result)
{
	if(TREE_CODE(type)==POINTER_TYPE)
		type=TREE_TYPE(type);
	if(TREE_CODE(type)==POINTER_TYPE)
		type=TREE_TYPE(type);
	if(TREE_CODE(type)==POINTER_TYPE)
		type=TREE_TYPE(type);
    if(TREE_CODE(type)==RECORD_TYPE){
		tree next=TYPE_NAME(type);
		if(TREE_CODE(next)!=TYPE_DECL){
			return 0;
		}
		char *declClassName=IDENTIFIER_POINTER(DECL_NAME (next));
		sprintf(result,"%s",declClassName);
		return strlen(declClassName);
	}else{
		tree typeName=TYPE_NAME(type);
		if(aet_utils_valid_tree(typeName) && TREE_CODE(typeName)==IDENTIFIER_NODE){
			char *typeNameStr=IDENTIFIER_POINTER(typeName);
			sprintf(result,"%s",typeNameStr);
			return strlen(typeNameStr);
		}else if(aet_utils_valid_tree(typeName) && TREE_CODE(typeName)==TYPE_DECL){
			char *typeNameStr=IDENTIFIER_POINTER(DECL_NAME(typeName));
			sprintf(result,"%s",typeNameStr);
			return strlen(typeNameStr);
		}
	}
	return 0;
}


nboolean  generic_util_valid_by_str(char *str)
{
		if(str==NULL || strlen(str)!=1 )
		  return FALSE;
		char min='A';
		char max='Z';
		char v=str[0];
		return (v>=min && v<=max);
}


nboolean generic_util_valid_all(tree id)
{
	char *str=IDENTIFIER_POINTER(id);
	return generic_util_valid_all_by_str(str);
}

nboolean generic_util_valid_all_by_str(char *str)
{
	if(str==NULL || (strlen(str)!=1 && strlen(str)!=3))
	  return FALSE;
	if(strlen(str)==3 && !strcmp(str,"all"))
	   return TRUE;
	char min='A';
	char max='Z';
	char v=str[0];
	return (v>=min && v<=max);
}

nboolean generic_util_valid_id(tree id)
{
	char *str=IDENTIFIER_POINTER(id);
	return generic_util_valid_by_str(str);
}

/**
 * 通过泛型声明E 获取 aet_generic_E 类型
 */
tree generic_util_get_generic_type_by_str(char *genericStr)
{
	  char temp[255];
	  sprintf(temp,"%s%s",AET_GENERIC_TYPE_NAME_PREFIX,genericStr);
	  tree id=aet_utils_create_ident(temp);
	  tree type=lookup_name(id);
	  if(!type || type==NULL_TREE || type==error_mark_node)
		  return NULL_TREE;
	  return type;
}

tree generic_util_get_fggb_record_type()
{
	 tree aetGenericInfo=aet_utils_create_ident(AET_FUNC_GENERIC_GLOBAL_BLOCK_STRUCT_NAME);
	 tree record=lookup_name(aetGenericInfo);
	 if(!record || record==error_mark_node){
		 return NULL_TREE;
	 }
	 tree recordType=TREE_TYPE(record);
	 return recordType;
}

static nboolean checkSecondParmIsFuncGenParmInfo00(tree decl)
{
   tree  funcType = TREE_TYPE (decl);
   int count=0;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
		tree type=TREE_VALUE(al);
		if(count==1){ //如果第二个参数是FuncGenParmInfo这是一个泛型函数
			if(TREE_CODE(type)==POINTER_TYPE){
				type=TREE_TYPE(type);
				if(TREE_CODE(type)==RECORD_TYPE){
					tree typeName=TYPE_NAME(type);
					if(TREE_CODE(typeName)==TYPE_DECL){
						char *na=IDENTIFIER_POINTER(DECL_NAME(typeName));
						if(na && strcmp(na,AET_FUNC_GENERIC_PARM_STRUCT_NAME)==0){
							return TRUE;
						}
					}
				}
			}
			break;
		}
		count++;
   }
   return FALSE;
}

static nboolean checkSecondParmIsFuncGenParmInfo(tree decl)
{
   tree  funcType = TREE_TYPE (decl);
   int count=0;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
		tree type=TREE_VALUE(al);
		if(count==1){ //如果第二个参数是FuncGenParmInfo这是一个泛型函数
			if(TREE_CODE(type)==RECORD_TYPE){
				tree typeName=TYPE_NAME(type);
				if(TREE_CODE(typeName)==TYPE_DECL){
					char *na=IDENTIFIER_POINTER(DECL_NAME(typeName));
					if(na && strcmp(na,AET_FUNC_GENERIC_PARM_STRUCT_NAME)==0){
						return TRUE;
					}
				}
				break;
			}
		}
		count++;
   }
   return FALSE;
}

/**
 * 是否是一个泛型函数
 * 第一个参数是self
 * 第二个是FuncGenParmInfo info
 */
nboolean generic_util_is_generic_func(tree decl)
{
   if(TREE_CODE(decl)!=FUNCTION_DECL && TREE_CODE(decl)!=FIELD_DECL)
	   return FALSE;
   GenericModel *funcGen=c_aet_get_func_generics_model(decl);
   if(!funcGen){
	   return FALSE;
   }
   nboolean isFuncGenParmInfo=checkSecondParmIsFuncGenParmInfo(decl);
   if(!isFuncGenParmInfo)
	   return FALSE;
   char *mangleFuncName=IDENTIFIER_POINTER(DECL_NAME(decl));
   ClassFunc  *func=func_mgr_get_entity_by_mangle(func_mgr_get(),mangleFuncName);
   if(func==NULL){
	   n_error("generic_util_is_generic_func 不应该发生的未知错误 %s ClassFunc是空的!\n",mangleFuncName);
	   return FALSE;
   }
   nboolean re=class_func_is_func_generic(func);
   if(!re){
	   n_error("generic_util_is_generic_func 不应该发生的未知错误 %s 不是一个泛型函数！\n",mangleFuncName);
	   return FALSE;
   }
   return re;

}

/**
 * 是否通用泛型参数所在的函数
 */
nboolean generic_util_is_query_generic_func(tree decl)
{
	   nboolean isFuncGenParmInfo=checkSecondParmIsFuncGenParmInfo(decl);
	   if(!isFuncGenParmInfo)
		   return FALSE;
	   char *mangleFuncName=IDENTIFIER_POINTER(DECL_NAME(decl));
	   ClassFunc  *func=func_mgr_get_entity_by_mangle(func_mgr_get(),mangleFuncName);
	   if(func==NULL){
		   n_error("generic_util_is_generic_func 不应该发生的未知错误 %s\n",mangleFuncName);
		   return FALSE;
	   }
	   nboolean re=class_func_is_query_generic(func);
	   return re;
}


/**
 * 创建块对应的函数名
 * 用户代码是:genericblock$(a1,a2){
 * }
 * genericblock转成了函数名 abc_
 */

char *generic_util_create_block_func_name(char *sysName,int index)
{
	char blockFuncName[256];
	if(index>=0)
	  sprintf(blockFuncName,"_%s_%s_%d",sysName,GENERIC_BLOCK_TO_FUNC_PREFIX,index);
	else
	  sprintf(blockFuncName,"_%s_%s_",sysName,GENERIC_BLOCK_TO_FUNC_PREFIX);
	return n_strdup(blockFuncName);
}

char *generic_util_create_block_func_type_decl_name(char *sysName,int index)
{
  return n_strdup_printf("_%s_%s_%d_typedecl",sysName,GENERIC_BLOCK_TO_FUNC_PREFIX,index);
}

char *generic_util_create_block_func_define_name(char *sysName,int index)
{
  return n_strdup_printf("_%s_%s_%d_abc123",sysName,GENERIC_BLOCK_TO_FUNC_PREFIX,index);
}

char *generic_util_create_undefine_new_object_var_name(char *sysName)
{
  	    char *varName=n_strdup_printf("%s_create_undefine_var",sysName);
        return varName;
}

char *generic_util_create_define_new_object_var_name(char *sysName)
{
  	    char *varName=n_strdup_printf("%s_create_define_var",sysName);
        return varName;
}

char *generic_util_create_undefine_func_call_var_name(char *sysName)
{
  	    char *varName=n_strdup_printf("_%s_undefine_func_call_var",sysName);
        return varName;
}

char *generic_util_create_define_func_call_var_name(char *sysName)
{
  	    char *varName=n_strdup_printf("_%s_define_func_call_var",sysName);
        return varName;
}


char *generic_util_create_func_generic_call_var_name(char *sysName)
{
	char name[128];
    sprintf(name,"%s_func_generic_call",sysName);
	return n_strdup(name);
}


char *generic_util_create_global_var_for_block_func_name(char *sysName,int index)
{
	char *fname=generic_util_create_block_func_name(sysName,index);
	char varName[256];
    sprintf(varName,"%s_create_block_var",fname);
    n_free(fname);
	return n_strdup(varName);
}


///////////////////以下是生成一个复合语句的参数--------------------------------------

static inline void c_parser_check_literal_zero (c_parser *parser, unsigned *literal_zero_mask,unsigned int idx)
{
  if (idx >= HOST_BITS_PER_INT)
    return;
  c_token *tok = c_parser_peek_token (parser);
  switch (tok->type)
    {
    case CPP_NUMBER:
    case CPP_CHAR:
    case CPP_WCHAR:
    case CPP_CHAR16:
    case CPP_CHAR32:
    case CPP_UTF8CHAR:
      /* If a parameter is literal zero alone, remember it
	 for -Wmemset-transposed-args warning.  */
      if (integer_zerop (tok->value)
	  && !TREE_OVERFLOW (tok->value)
	  && (c_parser_peek_2nd_token (parser)->type == CPP_COMMA
	      || c_parser_peek_2nd_token (parser)->type == CPP_CLOSE_PAREN))
	*literal_zero_mask |= 1U << idx;
    default:
      break;
    }
}

static int backupToken(c_parser *parser,c_token *backups)
{
	  int tokenCount=parser->tokens_avail;
	  int i;
	  c_token *token;
	  for(i=0;i<tokenCount;i++){
	     token=c_parser_peek_token (parser);
	     aet_utils_copy_token(token,&backups[i]);
		  aet_print_token(token);

		 c_parser_consume_token (parser);
	  }
	  return tokenCount;
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
	  aet_print_token_in_parser("generic_call restore ------");
}

static tree createTarget(c_parser *parser,char *codes)
{
	aet_utils_add_token(parse_in,codes,strlen(codes));
	struct c_expr expr;
	int literal_zero_mask=0;
    c_parser_check_literal_zero (parser, &literal_zero_mask, 0);
    n_debug("createTarget 00 --源代码 %s\n",codes);
    expr = c_c_parser_expr_no_commas (parser, NULL);
    expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
    return expr.value;
}

/**
 * 生成一个复合语句的参数如:
 * setData(({int big;big;}));
 * ({int big;big;})后面必须再加上“(\n”
 * codes参数的源代码，最后要加上“(\n”
 * 所以({int big;big;})变成({int big;big;}))\n才能生成参数
 * 最后还要consume CPP_CLOSE_PAREN
 */
tree generic_util_create_target(char *codes)
{
	c_parser *parser=class_impl_get()->parser;
	int count=parser->tokens_avail;
	c_token backups[30];
	int backCount=backupToken(parser,backups);
	tree target=createTarget(parser,codes);
	restore(parser,backups,backCount);
	c_parser_consume_token (parser);
	return target;
}



static char *getId(tree arg,int *pointer)
{
	tree type=TREE_TYPE(arg);
	if(!aet_utils_valid_tree(type))
	    return NULL;
	if(TREE_CODE(type)==POINTER_TYPE){
		*pointer=*pointer+1;
		return getId(type,pointer);
	}else{
        tree typeName=TYPE_NAME(type);
        if(!aet_utils_valid_tree(typeName))
        	return NULL;
        if(TREE_CODE(typeName)==TYPE_DECL){
        	tree name=DECL_NAME(typeName);
        	return IDENTIFIER_POINTER(name);
        }else{
        	return NULL;
        }
	}
	return NULL;
}
/**
 * 是不是A-E泛型，如果是因该转化成aet_void_E,aet_void_F等
 */
static char *getGenericDecl(tree arg)
{
	int pointer=0;
	char *type=getId(arg,&pointer);
	if(pointer>0){
		tree type=TREE_TYPE(arg);
		tree typeName=TYPE_NAME(type);
		if(typeName && TREE_CODE(typeName)==TYPE_DECL){
			tree name=DECL_NAME(typeName);
			if(generic_util_is_generic_ident(IDENTIFIER_POINTER(name)))
	        	return IDENTIFIER_POINTER(name);
		}
	}
	return NULL;
}

/**
 * 类型转成字符串
 */
char *generic_util_get_type_str(tree arg)
{
	char *n=getGenericDecl(arg);
	if(n!=NULL){
	    char *re=n_strdup(n);
	    return re;
	}
    int pointer=0;
    n=getId(arg,&pointer);
    if(n==NULL)
    	return NULL;
    NString *strtype=n_string_new(n);
    if(pointer==1){
    	n_string_append(strtype," *");
    }else if(pointer==2){
    	n_string_append(strtype," **");
    }else if(pointer==3){
    	n_string_append(strtype," ***");
    }
    char *re=n_strdup(strtype->str);
    n_string_free(strtype,TRUE);
    return re;
}

/**
 * 是不是变量
 * T abc或E abc等form
 */
nboolean generic_util_is_generic_var_or_parm(tree decl)
{
    if(!aet_utils_valid_tree(decl))
    	return FALSE;
    if(TREE_CODE(decl)!=VAR_DECL && TREE_CODE(decl)!=PARM_DECL)
    	return FALSE;
    char *str=generic_util_get_type_str(decl);
    if(str==NULL)
    	return FALSE;
    nboolean re= generic_util_is_generic_ident(str);
    n_free(str);
    return re;
}


/**
 * 把参数 int *abcd[][5]从arg中取出来
 */
nboolean generic_util_get_array_type_and_parm_name(tree arg,char **typeStr,char **parmName,char *oldParmName)
{
		tree type=TREE_TYPE(arg);
		if(TREE_CODE(type)!=POINTER_TYPE)
			return FALSE;
		type=TREE_TYPE(type);
		if(TREE_CODE(type)!=ARRAY_TYPE)
		    return FALSE;
	    tree elts=TREE_TYPE (type);
	    tree domn=TYPE_DOMAIN (type);
	    tree typeName=TYPE_NAME(elts);
	    if(!aet_utils_valid_tree(typeName))
	       return FALSE;
	    if(TREE_CODE(typeName)==TYPE_DECL){
			tree name=DECL_NAME(typeName);
			*typeStr=n_strdup( IDENTIFIER_POINTER(name));
	    }else{
	        return FALSE;
	    }
	    tree maxValue=TYPE_MAX_VALUE (domn);
   	    wide_int dimen=wi::to_wide(maxValue);
   	    int ds=	dimen.to_shwi()+1;
	    *parmName=n_strdup_printf("%s[][%d]",oldParmName,ds);
	    return TRUE;
}
