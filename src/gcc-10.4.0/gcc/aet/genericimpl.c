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
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"

#include "c-aet.h"
#include "aetutils.h"
#include "genericimpl.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "classimpl.h"
#include "genericexpand.h"
#include "funcmgr.h"

/**
 * 泛型函数需要加一个参数存储类型数据 aet_generic_info
 * 当调用泛型函数时，根据实参生成 aet_generic_info
 */

static void genericImplInit(GenericImpl *self)
{
	self->currentModelOfSpecs.specs=NULL;
	self->currentModelOfSpecs.genericModel=NULL;
	self->castCurrentModelSpecs.expr=NULL_TREE;
	self->castCurrentModelSpecs.genericModel=NULL;

}

static ClassName *getBelongClassName(GenericImpl *self)
{
	 c_parser *parser=self->parser;
	 ClassParser  *classPaser=class_parser_get();
	 ClassImpl  *classImpl=class_impl_get();
	 nboolean pasering=class_parser_is_parsering(classPaser);
	 //printf("generic_impl_get_class_name_by_self_parm--- %d %p\n",parser->isAet,current_function_decl);
	 if(parser->isAet){
		  return classImpl->className;
	 }else if(pasering && classPaser->currentClassName){
        //printf("正在进行 classparsering %s\n",classPaser->currentClassName->userName);
        return classPaser->currentClassName;
	 }
	 return NULL;
}

/**
 * 在泛型fndecl中找参数的类型是泛型并且泛型声明等于id
 */
static int getIndexFromFuncGenc(tree id,tree fndecl)
{
	GenericModel *funcGen=c_aet_get_func_generics_model(fndecl);
	char *funcName=IDENTIFIER_POINTER(DECL_NAME(fndecl));
	int undefine=generic_model_get_undefine_count(funcGen);
	if(undefine==0){
	   error_at(input_location,"泛型函数%qs没有声明%qs",funcName,IDENTIFIER_POINTER(id));
	   return -1;
	}
	int index=generic_model_get_index(funcGen,IDENTIFIER_POINTER(id));
	if(index<0){
	   error_at(input_location,"泛型函数%qs没有声明%qs",funcName,IDENTIFIER_POINTER(id));
	   return -1;
	}
	return index;
}

static void createSizeofToken(GenericImpl *self,int index,nboolean fromFuncGen)
{
	c_parser *parser=self->parser;
    c_parser_consume_token (parser);
	c_parser_consume_token (parser);
	c_parser_consume_token (parser);
	int tokenCount=parser->tokens_avail;
	if(tokenCount+8>AET_MAX_TOKEN){
		error("token太多了");
		return FALSE;
	}
	int i;
	for(i=tokenCount;i>0;i--){
	   aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+8]);
	}
	parser->tokens_avail=tokenCount+8;
	char *refName="self";
	char  *varName=AET_GENERIC_ARRAY; //"_generic_1234_array" 类中的泛型数组名称
	if(fromFuncGen){
		//FuncGenParmInfo  tempFgpi1234
		refName=AET_FUNC_GENERIC_PARM_NAME;
	    varName="pms";
	}
	aet_utils_create_token(&parser->tokens[0],CPP_NAME,refName,strlen(refName));
	if(fromFuncGen)
		aet_utils_create_token(&parser->tokens[1],CPP_DOT,".",1);
	else
        aet_utils_create_token(&parser->tokens[1],CPP_DEREF,"->",2);
	aet_utils_create_token(&parser->tokens[2],CPP_NAME,varName,strlen(varName));
	aet_utils_create_token(&parser->tokens[3],CPP_OPEN_SQUARE,"[",1);
	aet_utils_create_number_token(&parser->tokens[4],index);
	aet_utils_create_token(&parser->tokens[5],CPP_CLOSE_SQUARE,"]",1);
	aet_utils_create_token(&parser->tokens[6],CPP_DOT,".",1);
	aet_utils_create_token(&parser->tokens[7],CPP_NAME,"size",4);
	aet_print_token_in_parser("计算泛型的 sizeof-------");
}

/**
 * sizeof(T)代码只能出现在类定义中
 * 如 impl Abc{};
 * self->genericAObject983478[self->genericAObject983478[0]+1+index]
 */
nboolean  generic_impl_calc_sizeof(GenericImpl *self,ClassName *className)
{
	c_parser *parser=self->parser;
	if(!parser->isAet && !parser->isGenericState)
		return FALSE;
    tree openbrace = c_parser_peek_token (parser)->value;
    tree id=c_parser_peek_2nd_token (parser)->value;
    if(id==NULL)
    	return FALSE;
    location_t loc=c_parser_peek_token (parser)->location;
    tree decl = lookup_name (id);
    //printf("generic_impl_calc_sizeof 11 is %p %p\n",id,decl);
    if(decl)
    	return FALSE ;
    //找不到ID对应的声明，可能是一个泛型E
    ClassInfo *info=NULL;
    if(parser->isAet){
        info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    }else if(parser->isGenericState){
       	tree id=aet_utils_create_ident("self");
       	tree obj=lookup_name(id);
       	tree type=TREE_TYPE(obj);
       	char *sysClassName=class_util_get_class_name(type);
        info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
    }
    if(info==NULL)
    	return FALSE;
    if(c_parser_peek_nth_token (parser,3)->type!=CPP_CLOSE_PAREN)
    	return FALSE;
    nboolean isGenericDecl=class_info_is_generic_decl(info,id);
    if(!isGenericDecl){
	  GenericModel *funcDecl=c_aet_get_func_generics_model(current_function_decl);
	  printf("generic_impl_calc_sizeof ---所在类没有泛型声明，或者不是泛型类:%d %s\n",class_info_is_generic_class(info),IDENTIFIER_POINTER(id));
	  nboolean find=generic_model_exits_ident(funcDecl,IDENTIFIER_POINTER(id));
	  printf("generic_impl_calc_sizeof 22 检查是不在在泛型函数中使用sizeof %s 泛型函数的泛型:%p 中泛型函数中找到 ok:%d\n",IDENTIFIER_POINTER(id),funcDecl,find);
	  if(!find)
		 return FALSE;
	  int index=getIndexFromFuncGenc(id,current_function_decl);
	  createSizeofToken(self,index,TRUE);
    }else{
	   int index=class_info_get_generic_index(info,IDENTIFIER_POINTER(id));
	   if(index<0){
		 n_warning("在类%s中的泛型声明没有包括泛型%s。\n",className->sysName,IDENTIFIER_POINTER(id));
		 return FALSE;
	   }
	   createSizeofToken(self,index,FALSE);
    }
    return TRUE;
}


/**
 * 生成在类中的数组field
 * aet_generic_info genericAObject843094179[genericCount];
 * 数组内容格式如下：
    typedef struct _aet_generic_info{
	  char genericName;
	  char type;
	  char alias[100];
	  int  size;
	  int  pointerCount;
    }aet_generic_info;
 */
tree generic_impl_create_generic_info_array_field(GenericImpl *self,ClassName *className,int genericCount)
{
	 c_parser *parser=self->parser;
	 location_t loc=c_parser_peek_token (parser)->location;
	 char *varName=AET_GENERIC_ARRAY;
	 tree id=aet_utils_create_ident(varName);
	 unsigned HOST_WIDE_INT w;
     w=genericCount;
     tree indextype= build_index_type (build_int_cst (integer_type_node, w));
     aet_print_tree(indextype);
     tree genericInfo=lookup_name(aet_utils_create_ident(AET_GENERIC_INFO_STRUCT_NAME));
     if(!aet_utils_valid_tree(genericInfo)){
    	 error("没找到aet_generic_info结构体，检查是否包含了头文件。");
    	 return NULL_TREE;
     }
	 tree array=build_array_type(TREE_TYPE(genericInfo),indextype);
	 tree decl = build_decl (loc,FIELD_DECL,id, array);
	 finish_decl (decl, loc, NULL_TREE, NULL_TREE, NULL_TREE);
     return decl;
}

/**
 * 在class中生成
 * void *_gen_blocks_array_897[AET_MAX_GENERIC_BLOCKS];
 */
tree generic_impl_create_generic_block_array_field(GenericImpl *self)
{
	 c_parser *parser=self->parser;
	 location_t loc=c_parser_peek_token (parser)->location;
	 char *varName=AET_GENERIC_BLOCK_ARRAY_VAR_NAME;
	 tree id=aet_utils_create_ident(varName);
	 unsigned HOST_WIDE_INT w;
     w=AET_MAX_GENERIC_BLOCKS;
     tree indextype= build_index_type (build_int_cst (integer_type_node, w));
     aet_print_tree(indextype);
     tree type=build_pointer_type(void_type_node);
	 tree array=build_array_type(type,indextype);
	 tree decl = build_decl (loc,FIELD_DECL,id, array);
	 finish_decl (decl, loc, NULL_TREE, NULL_TREE, NULL_TREE);
     return decl;
}

/**
 * 获取对象中_generic_1234_array数组变量中的aet_generic_info
 * id 是'E'或'T' 根据id可以获得数组_generic_1234_array的索引。
 * 当前是不是处理泛型函数中
 */
static tree getGenericInfo(location_t loc,tree objExpr,tree id2)
{
	  char *str=IDENTIFIER_POINTER(id2);
	  char *sysClassName=class_util_get_class_name_from_expr(objExpr);
	  if(!sysClassName){
		  error_at(loc,"调用函数%qs，应在类定义中，并且不能是类静态方法！",AET_GET_GENERIC_INFO_FUNC_NAME);
		  return NULL_TREE;
	  }
	  ClassInfo *selfObjectInfo=class_mgr_get_class_info(class_mgr_get(),sysClassName);
	  //是不是处于泛型函数中
	  GenericModel *funcGen=NULL;
	  if(current_function_decl){
	     funcGen=c_aet_get_func_generics_model(current_function_decl);
	  }
	  nboolean from=FALSE;//来自泛型函数中的参数 FuncGenParmInfo *tempFgpi1234，否则来自类中的aet_generic_info _generic_1234_array数组
	  int index=-1;
	  if(funcGen){
		  index= generic_model_get_index(funcGen,str);
		  if(index<0){
			  index=class_info_get_generic_index(selfObjectInfo,str);
			  from=TRUE;
		  }
		  if(index<0){
			  error_at(loc,"当前函数%qs是泛型函数，但没有找到泛型声明(%qs)，在类%qs也没有找到泛型声明(%qs)！",
					  IDENTIFIER_POINTER(DECL_NAME(current_function_decl)),str,selfObjectInfo->className.userName,str);
			  return NULL_TREE;
		  }
	  }else{
		  index=class_info_get_generic_index(selfObjectInfo,str);
		  from=TRUE;
		  //是不是泛型函数
		  if(index<0){
			  error_at(loc,"类%qs没有泛型声明(%qs)！",selfObjectInfo->className.userName,str);
			  return NULL_TREE;
		  }
	  }
	  tree genericInfo=lookup_name(aet_utils_create_ident(AET_GENERIC_INFO_STRUCT_NAME));
	  if(!aet_utils_valid_tree(genericInfo)){
		 error("没找到aet_generic_info结构体，检查是否包含了头文件。");
		 return NULL_TREE;
	  }
      if(from){
		  char *genericArrayName=AET_GENERIC_ARRAY;
		  tree genericIndex=build_int_cst(integer_type_node,index);
		  tree ident=aet_utils_create_ident(genericArrayName);
		  tree genericArrayRef = build_component_ref (loc,build_indirect_ref (loc,objExpr,RO_ARROW),ident, loc);
		  tree result = build4 (ARRAY_REF, TREE_TYPE (genericInfo), genericArrayRef, genericIndex, NULL_TREE,NULL_TREE);
		  aet_print_tree(result);
		  if(!aet_utils_valid_tree(result)){
			  error_at(loc,"调用函数%qs失败！参数1：%qE 参数2:%qE",AET_GET_GENERIC_INFO_FUNC_NAME,genericIndex,genericArrayRef);
			  return NULL_TREE;
		  }
		  return result;
      }else{
		  tree fgpiParm=lookup_name(aet_utils_create_ident(AET_FUNC_GENERIC_PARM_NAME));
		  if(!aet_utils_valid_tree(fgpiParm)){
			 error("当前泛型函数%qs中没找系统加入的参数:FuncGenParmInfo *tempFgpi1234。",IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
			 return NULL_TREE;
		  }
		  char *genericArrayName="pms";
		  tree genericIndex=build_int_cst(integer_type_node,index);
		  tree ident=aet_utils_create_ident(genericArrayName);
		  tree genericArrayRef = build_component_ref (loc,build_indirect_ref (loc,fgpiParm,RO_ARROW),ident, loc);
		  tree result = build4 (ARRAY_REF, TREE_TYPE (genericInfo), genericArrayRef, genericIndex, NULL_TREE,NULL_TREE);
		  aet_print_tree(result);
		  if(!aet_utils_valid_tree(result)){
			  error_at(loc,"调用函数%qs失败！参数1：%qE 参数2:%qE",AET_GET_GENERIC_INFO_FUNC_NAME,genericIndex,genericArrayRef);
			  return NULL_TREE;
		  }
		  return result;
      }
}


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

static nboolean getCallerAndGenType(GenericImpl *self,location_t loc,tree *callObj,tree *gen)
{
      c_parser *parser=self->parser;
	  struct c_expr expr;
	  if (!c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
		  error_at(loc,"generic_info$用法如:generic_info$(E)");
	  }
	  c_parser_consume_token (parser); // consume (
	  tree id2=NULL_TREE;
	  tree id1=c_parser_peek_token (parser)->value;
	  if(generic_util_valid_all(id1)){
		  c_parser_consume_token (parser); // E
		  if(!c_parser_next_token_is (parser, CPP_CLOSE_PAREN)){
			  error_at(loc,"泛型后应是'）',如果要获取指定对象的泛型，第一个参数是对象变量。用法如:generic_info$(obj,E)");
			  return FALSE;
		  }else{
			  c_parser_consume_token (parser); // ')'
		  }
		  tree selfid=aet_utils_create_ident("self");
		  tree paramSelf=lookup_name(selfid);
		   if(!aet_utils_valid_tree(paramSelf)){
			  error_at(loc,"关键字%qs只能在类定义中使用。！",AET_GET_GENERIC_INFO_FUNC_NAME);
			  return FALSE;
		   }
		   id2=paramSelf;
	  }else{
		 unsigned int literal_zero_mask=0;
		 c_parser_check_literal_zero (parser, &literal_zero_mask, 0);
		 expr = c_c_parser_expr_no_commas (parser, NULL);
		 expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
		 if (!c_parser_next_token_is (parser, CPP_COMMA)){
			  error_at(loc,"关键字generic_info$，第二个参数应是泛型声明符号，如E、K等。");
			  return FALSE;
		 }
		 c_parser_consume_token (parser); // ','
		 if (c_parser_next_token_is (parser, CPP_NAME)){
			  id1=c_parser_peek_token (parser)->value;
			  c_parser_consume_token (parser); // 'E'
			  if(!generic_util_valid_all(id1)){
			      error_at(loc,"generic_info$的第二个参数不是有效的泛型声明,用法如:generic_info$(E)");
				  return FALSE;
			  }
		  }else{
			  error_at(loc,"generic_info$的第二个参数E、T等有效的泛型声明,用法如:generic_info$(E)");
			  return FALSE;
		  }
		  if(c_parser_next_token_is (parser, CPP_CLOSE_PAREN)){
			  c_parser_consume_token (parser); // ')'
		  }else{
			  error_at(loc,"generic_info$没有以')'结束！");
			  return FALSE;
		  }
		  id2=expr.value;
	  }
	  *callObj=id2;
	  *gen=id1;
	  return TRUE;
}

/**
 * 解析在AET类实现中调用 generic_info$ RID_AET_GENERIC_INFO关键字。
 * generic_info$(A) 或者是对象 obj,e
 */
struct c_expr generic_impl_generic_info_expression (GenericImpl *self)
{
	  c_parser *parser=self->parser;
	  struct c_expr expr;
	  struct c_expr result;
	  location_t expr_loc;
	  location_t start;
	  location_t finish = UNKNOWN_LOCATION;
	  start = c_parser_peek_token (parser)->location;
	  location_t loc=c_parser_peek_token (parser);
	  if(!parser->isAet){
		  error_at(start,"关键字generic_info$只能用在类实现中。");
		  struct c_expr ret;
		  ret.set_error ();
		  ret.original_code = ERROR_MARK;
		  ret.original_type = NULL;
		  return ret;
	  }
	  c_parser_consume_token (parser);
	  aet_print_token(c_parser_peek_token (parser));
	  aet_print_token(c_parser_peek_2nd_token (parser));
	  tree callObj=NULL;
	  tree gen=NULL;
	  nboolean ok=getCallerAndGenType(self,loc,&callObj,&gen);
	  if(ok){
		   result.value=getGenericInfo(start,callObj,gen);
	  }
	  if (finish == UNKNOWN_LOCATION)
		finish = start;
	  set_c_expr_source_range (&result, start, finish);
	  return result;
}

/**
 * 在强转中判断是不是泛型
 * 格式如下：(E)
 */

nboolean generic_impl_is_cast_token(GenericImpl *self,c_token *token)
{
	   if(token->type!=CPP_NAME)
		   return FALSE;
	   tree id=token->value;
	   if(!aet_utils_valid_tree(id) || TREE_CODE(id)!=IDENTIFIER_NODE)
		   return FALSE;
	   tree type=lookup_name(id);
	   if(aet_utils_valid_tree(type))
		   return FALSE;
	   char *str=IDENTIFIER_POINTER(id);
	   if(str==NULL || strlen(str)!=1)
		   return FALSE;
	   if(!(str[0]>='A' && str[0]<='Z'))
		   return FALSE;
	   return TRUE;
}

static ClassName *getClassName(tree decl)
{
	tree type=NULL;
	if(TREE_CODE(decl)==RECORD_TYPE){
		type=decl;
	}else{
		type=TREE_TYPE(decl);
		if(TREE_CODE(type)==ARRAY_TYPE){
		    n_debug("这是一个数组 getClassName ----- %s\n",get_tree_code_name(TREE_CODE(type)));
			type=TREE_TYPE(type);
		}else if (TREE_CODE(type)==FUNCTION_TYPE){
			type=TREE_TYPE (type);
		}
	}
	char *sysClassName=class_util_get_class_name(type);
	if(sysClassName==NULL)
		return NULL;
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
	if(className==NULL)
		return NULL;
	return className;
}


static tree getParmsFromDeclarator(struct c_declarator *declarator)
{
	int i;
	struct c_declarator *funcdel=NULL;
	struct c_declarator *temp=declarator;
	for(i=0;i<100;i++){
		if(temp!=NULL){
		   enum c_declarator_kind kind=temp->kind;
		   if(kind==cdk_function){
			   funcdel=temp;
               break;
		   }
		   temp=temp->declarator;
		}else{
			break;
		}
	}
	if(!funcdel)
		return NULL_TREE;
	struct c_declarator *pointer=funcdel->declarator;
	if(pointer==NULL)
		return NULL_TREE;
	enum c_declarator_kind kind=pointer->kind;
	if(kind!=cdk_pointer)
		return NULL_TREE;
	struct c_declarator *funid=pointer->declarator;
	if(funid==NULL)
		return NULL_TREE;
	kind=funid->kind;
	if(kind!=cdk_id)
		return NULL_TREE;
  	struct c_arg_info *args=funcdel->u.arg_info;
  	if(!args)
		return NULL_TREE;
  	return args->parms;
}

/**
 * 如果强转是如下形式
 * (E)getData或(E *)getData
 * E被翻译成了aet_void_E
 */
tree generic_impl_cast(GenericImpl *self,struct c_type_name *type_name,tree expr)
{
	nboolean isGenericType=generic_util_is_generic_pointer(TREE_TYPE(expr));
	if(!isGenericType)
		return expr;
	tree realGen;
    tree type_expr = NULL_TREE;
    bool type_expr_const = true;
    tree ret;
    realGen = groktypename (type_name, &type_expr, &type_expr_const);
   /// printf("generic_impl_cast 00-----\n");
   // aet_print_tree(realGen);
	if(TREE_CODE(realGen)==POINTER_TYPE){
		printf("generic_impl_cast 11 转指针 expr是泛型void_geneirc_E类型 要转的也是指针\n");
//		tree pointer=build_pointer_type(long_unsigned_type_node);
//		tree ret = build1 (NOP_EXPR, pointer,expr);
//		ret = build1 (INDIRECT_REF, long_unsigned_type_node, ret);
//		ret = build1 (CONVERT_EXPR, realGen, ret);
//		return ret;
		return expr;
	}else{
		n_debug("generic_impl_cast 22 转非指针\n");
	   tree realType=realGen;
	   tree pointer=build_pointer_type(realType);
	   tree ret = build1 (NOP_EXPR, pointer,expr);
	   ret = build1 (INDIRECT_REF, realType, ret);
	   return ret;
	}
}

void generic_impl_replace_token(GenericImpl *self,c_token *token)
{
	  c_parser *parser=self->parser;
	  tree id=token->value;
	  char *genericStr=IDENTIFIER_POINTER(id);
	  char temp[255];
	  sprintf(temp,"aet_generic_%s",genericStr);
	  token->value=aet_utils_create_ident(temp);
	  token->id_kind=C_ID_TYPENAME;//关键
}


static void replaceGenericOfUseInt(GenericImpl *self)
{
	  c_parser *parser=self->parser;
	  location_t loc=c_parser_peek_token (parser)->location;
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+2>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
	  }
	  aet_utils_create_token(&parser->tokens[0],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_int_token(&parser->tokens[1],loc);
	  parser->tokens_avail=tokenCount+2;
	  aet_print_token_in_parser("replaceGenericOfUseInt ----");
}

static void replaceGenericOfUseVoid(GenericImpl *self,char *genStr)
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
	  char temp[255];
	  sprintf(temp,"aet_generic_%s",genStr);
	  aet_utils_create_token(&parser->tokens[1],CPP_NAME,temp,strlen(temp));
	  aet_utils_create_token(&parser->tokens[2],CPP_MULT,"*",1);
	  parser->tokens_avail=tokenCount+3;
	  aet_print_token_in_parser("replaceGenericOfUseVoid ----");
}

void generic_impl_cast_by_token(GenericImpl *self,c_token *token)
{
  c_parser *parser=self->parser;
  if(parser->isAet){
	  if(parser->isTestGenericBlockState){
		  c_parser_consume_token (parser); //consume (
		  c_parser_consume_token (parser); //E
		  replaceGenericOfUseInt(self);
	  }else{
		generic_impl_replace_token(self,token);
	  }
  }else if(parser->isGenericState){
	  generic_expand_cast_by_token(generic_expand_get(),token);
  }
}


/**
 * 加一个函数指针变量void *_setGenericBlockFunc_123_varname
 */
void generic_impl_add_about_generic_field_to_class(GenericImpl *self)
{
	  c_parser *parser=self->parser;
	  c_token *token=c_parser_peek_token(parser);
	  location_t loc=token->location;
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+3>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  location_t  decl_loc = token->location;
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+3]);
	  }
	  aet_utils_create_void_token(&parser->tokens[0],loc);
	  aet_utils_create_token(&parser->tokens[1],CPP_MULT,"*",1);
	  aet_utils_create_token(&parser->tokens[2],CPP_NAME,AET_SET_BLOCK_FUNC_VAR_NAME,strlen(AET_SET_BLOCK_FUNC_VAR_NAME));
	  parser->tokens_avail=tokenCount+3;
	  aet_print_token_in_parser("generic_impl_add_about_generic_field_to_class ----");
}
/**
 * FuncGenBlockList   _block_info_list_var_name;(AET_BLOCK_INFO_LIST_VAR_NAME)
 */
void generic_impl_add_about_generic_block_info_list(GenericImpl *self)
{
	  c_parser *parser=self->parser;
	  c_token *token=c_parser_peek_token(parser);
	  location_t loc=token->location;
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+2>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  location_t  decl_loc = token->location;
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
	  }
	  aet_utils_create_token(&parser->tokens[0],CPP_NAME,AET_BLOCK_INFO_LIST_STRUCT_NAME,strlen(AET_BLOCK_INFO_LIST_STRUCT_NAME));
	  parser->tokens[0].id_kind=C_ID_TYPENAME;//关键
	  aet_utils_create_token(&parser->tokens[1],CPP_NAME,AET_BLOCK_INFO_LIST_VAR_NAME,strlen(AET_BLOCK_INFO_LIST_VAR_NAME));
	  parser->tokens_avail=tokenCount+2;
	  aet_print_token_in_parser("generic_impl_add_about_generic_block_info_list ----");
}

/////////////////////////////改为GenericModel

nboolean  generic_impl_check_var(GenericImpl *self,tree decl,GenericModel *varGen)
{
    tree type=TREE_TYPE(decl);
	char *genericStr=generic_util_get_generic_str(type);
	if(genericStr==NULL && !varGen)
		return TRUE;
	location_t loc=DECL_SOURCE_LOCATION(decl);
	char *varName=IDENTIFIER_POINTER(DECL_NAME(decl));
    ClassName *belongClassName=getBelongClassName(self);
    GenericModel *belongGen=NULL;
    GenericModel *funcGen=NULL;
    if(belongClassName){
       ClassInfo *belongInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),belongClassName);
       belongGen= belongInfo->genericModel;
    }
    if(current_function_decl){
    	funcGen=c_aet_get_func_generics_model(current_function_decl);
    }
    if(genericStr!=NULL){
    	nboolean find=generic_model_include_decl_by_str(belongGen,genericStr);
		if(!find){
	    	find=generic_model_include_decl_by_str(funcGen,genericStr);
	    	if(!find){
	    	   if(belongClassName)
			      error_at(loc,"变量%qs声明为泛型%qs，但类%qs中并不存在此泛型声明。",varName,genericStr,belongClassName->userName);
	    	   else
				  error_at(loc,"变量%qs声明为泛型(%qs)，但找不到这样的泛型声明。",varName,genericStr);
			   return FALSE;
	    	}
		}
    }else if(varGen){
		char *sysClassName=class_util_get_class_name(type);
	    //printf("generic_impl_check_var 44 belong:%p %s\n",belongClassName,sysClassName);
		ClassName *varClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
		if(!varClassName){
		   aet_print_tree_skip_debug(type);
		   error_at(loc,"变量%qs不能使用泛型，因为变量类型不是类。",varName);
		   return FALSE;
		}
	    ClassInfo *varClassInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),varClassName);
	    if(!class_info_is_generic_class(varClassInfo)){
		  	error_at(loc,"变量%qs不能声明为泛型变量，因为类%qs不是泛型类。",varName,varClassName->userName);
		  	return FALSE;
	    }
	    int c1=generic_model_get_count(varClassInfo->genericModel);
	    int c2=generic_model_get_count(varGen);
	    if(c1!=c2){
	  	   error_at(loc,"类%qs声明的泛型数量与变量%qs定义的不匹配。",varClassName->userName,varName);
	  	   return FALSE;
	    }
	    if(funcGen){
	    	//在泛型函数中
	    	if(generic_model_get_undefine_count(varGen)!=0){
			   nboolean re=generic_model_include_decl(funcGen,varGen);
			   if(!re){
				  re=generic_model_include_decl(belongGen,varGen);
			   }
			   if(!re){
		 	  	   error_at(loc,"变量%qs定义的泛型中还包含有未定义的泛型单元，在当前上下文中并没有之与匹配的声明。",varName);
	    		   return FALSE;
			   }
	        }else{
	            n_debug("应该检查具体的类型是否是 extends下的类型\n");
	        }
	    }else{
	    	//检果是不是有undefine如果有，就是错了
	    	if(generic_model_get_undefine_count(varGen)!=0){
			   nboolean re=generic_model_include_decl(belongGen,varGen);
			   if(!re){
				  error_at(loc,"变量%qs定义的泛型中还包含有未定义的泛型单元，在当前上下文中并没有之与匹配的声明。",varName);
				  return FALSE;
			   }
	    	}else{
	    	    n_debug("应该检查具体的类型是否是 extends下的类型\n");
	    	}
	    }
    }
    return TRUE;
}


/**
 * Abc<E> *getObject(int var);
 * checkedGen就是<E>
 * retnOrParm是 Abc * 或 var
 * belongClassGenerics 是当前class{中的声明的泛型
 */
static nboolean checkgen(location_t loc,char *funcName,GenericModel *checkedGen,tree retnOrParm,ClassInfo *belongInfo,GenericModel *funcGen)
{
	if(!checkedGen)
		return TRUE;
    ClassName *className=getClassName(retnOrParm);
    if(!className){
       aet_print_tree(retnOrParm);
 	   error_at(loc,"函数返回值或参数中包含有泛型声明或定义，但返回值或参数类型并不是类，只有类才能带泛型参数。");
 	   return FALSE;
    }
    ClassInfo *varClassInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(varClassInfo->genericModel==NULL){
 	   error_at(loc,"类%qs并未声明泛型。",className->userName);
 	   return FALSE;
    }
    int c1=generic_model_get_count(varClassInfo->genericModel);
    int c2=generic_model_get_count(checkedGen);
    if(c1!=c2){
 	   error_at(loc,"类%qs声明的泛型数量与函数返回值或参数据定义的不匹配。",className->userName);
 	   return FALSE;
    }
    //是不是全定义了，如果是，不用检查了
    if(generic_model_get_undefine_count(checkedGen)==0){
    	n_info("函数返回值或参数中包含有泛型声明 已全定义:%s %s %s\n",className->userName,funcName,generic_model_tostring(checkedGen));
    	return TRUE;
    }
    /**
     * 二个地方检查返回值和参数据使用泛型是否正确
     * 1.当前所在类声明或定义。
     * 2.当前函数声明的泛型类型
     */
	nboolean re1=generic_model_include_decl(belongInfo->genericModel,checkedGen);
	nboolean re2=generic_model_include_decl(funcGen,checkedGen);
	//printf("dddd-------------%s %s %d %d\n",generic_model_tostring(funcGen),generic_model_tostring(checkedGen),re1,re2);
	if(re1 || re2)
		return TRUE;
	if(!belongInfo->genericModel && !funcGen){
		 error_at(loc,"类%qs和函数%qs没有声明泛型，当前带泛型的返回值或参数与之不匹配。",belongInfo->className.userName,funcName);
	}else if(!belongInfo->genericModel && funcGen){
		 error_at(loc,"类%qs中的函数%qs声明有泛型，但与当前带泛型的返回值或参数不匹配。",belongInfo->className.userName,funcName);
	}else{
		 error_at(loc,"类%qs中的泛型声明与当前带泛型的返回值或参数不匹配。当前函数:qs",belongInfo->className.userName,funcName);
	}
	 return FALSE;
}

/**
 * 检查在class$ {} 或impl${}中的有关函数,主要检查
 * 1.返回值有泛型
 * 2.函数中有泛型参数
 * 3.泛型函数
 * declGen 函数的返回值的泛型声明
 * funcGen 泛型函数的函数声明
 */
static void checkClassFunc(GenericImpl *self,tree fndecl,GenericModel *declGen,GenericModel *funcGen,tree args)
{
	tree funcType=TREE_TYPE(fndecl);
	if(TREE_CODE(funcType)!=FUNCTION_TYPE)
		funcType=TREE_TYPE(funcType);
	if(TREE_CODE(funcType)!=FUNCTION_TYPE){
		error_at(DECL_SOURCE_LOCATION(fndecl),"函数泛型检查时，声明%qD不是函数",fndecl);
		return;
	}
	char *funcName=IDENTIFIER_POINTER(DECL_NAME(fndecl));
	tree retn=TREE_TYPE(funcType);
	location_t loc=DECL_SOURCE_LOCATION(fndecl);
	n_debug("generic_impl_check_func ----- %s %s declGen:%p funcGen:%p",get_tree_code_name(TREE_CODE(fndecl)),funcName,declGen,funcGen);
	int undefine=generic_model_get_undefine_count(funcGen);
	if(funcGen!=NULL && undefine==0){
		n_warning("泛型函数的声明无效。");
	}
	ClassName *implClassName=getBelongClassName(self);
	if(!implClassName){
		error_at(loc,"函数%qD，只能在类声明或定义中使用泛型。",fndecl);
		return;
	}
	//printf("checkClassFunc --- belongInfo: %s\n",implClassName->sysName);
	ClassInfo *belongInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),implClassName);
	if(!declGen){
		char *str=generic_util_get_generic_str(retn);
		if(str!=NULL){
			n_debug("是泛型aet_void_E,aet_void_T等 %s",str);
			nboolean find=generic_model_include_decl_by_str(belongInfo->genericModel,str);
			if(!find)
			   find=generic_model_include_decl_by_str(funcGen,str);
			if(!find){
				error_at(loc,"函数参数中的泛型%qs在类和函数中没有声明。",str,implClassName->userName);
				return;
			}
		}
	}
	//printf("checkClassFunc --- 检查返回值 belongInfo: %s %p\n",implClassName->sysName,belongInfo->genericModel);
	nboolean result=checkgen(loc,funcName,declGen,retn,belongInfo,funcGen);
	n_debug("generic_impl_check_func -----11 检查完返回值的泛型 declGen:%p belong:%p funcGen:%p ok:%d %s %s",
			declGen,belongInfo->genericModel,funcGen,result,implClassName->sysName,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
	tree parm=NULL_TREE;
	int count=0;
	for (parm = args; parm; parm = DECL_CHAIN (parm)){
		GenericModel *parmGen=c_aet_get_generics_model(parm);
		n_debug("generic_impl_check_func -----22 检查每个参数 count:%d declGen:%p belong:%p funcGen:%p parmGen：%p parm:%p",
				count,declGen,belongInfo->genericModel,funcGen,parmGen,parm);
		count++;
		if(!parmGen){
			char *str=generic_util_get_generic_str(TREE_TYPE(parm));
			if(str!=NULL){
				n_debug("是泛型aet_void_E,aet_void_T等 %s belongInfo->genericModel:%p",str,belongInfo->genericModel);
				nboolean find=FALSE;
				if(belongInfo->genericModel!=NULL)
				 find=generic_model_include_decl_by_str(belongInfo->genericModel,str);
				//printf("find ------str:%s %s %s\n",str,generic_model_tostring(belongInfo->genericModel),generic_model_tostring(funcGen));
				if(!find)
				   find=generic_model_include_decl_by_str(funcGen,str);
				if(!find){
					error_at(loc,"函数参数中的泛型%qs在类和函数中没有声明。",str,implClassName->userName);
					return;
				}
			}
			continue;
		}
		result=checkgen(loc,funcName,parmGen,parm,belongInfo,funcGen);
		if(!result)
		   return;
	}
}


void  generic_impl_check_and_set_func(GenericImpl *self,tree fndecl,GenericModel *genModel,GenericModel *funcGenModel)
{
	c_parser *parser=self->parser;
	if(!parser->isAet)
		return;
	n_debug("generic_impl_check_and_set_func 000 %s func:%p declGen:%p funcGen:%p",IDENTIFIER_POINTER(DECL_NAME(fndecl)),fndecl,genModel,funcGenModel);
    c_aet_set_generics_model(fndecl,genModel);
    c_aet_set_func_generics_model(fndecl,funcGenModel);
    checkClassFunc(self,fndecl,genModel,funcGenModel,DECL_ARGUMENTS (fndecl));
}

GenericModel *generic_impl_pop_generic_from_declspecs(GenericImpl *self,struct c_declspecs *specs)
{
	if(self->currentModelOfSpecs.specs && self->currentModelOfSpecs.specs==specs){
		  GenericModel *ret=self->currentModelOfSpecs.genericModel;
		  self->currentModelOfSpecs.genericModel=NULL;
		  self->currentModelOfSpecs.specs=NULL;
		  return ret;
	}
	return NULL;
}

void   generic_impl_push_generic_from_declspecs(GenericImpl *self,struct c_declspecs *specs)
{
	c_parser *parser=self->parser;
    gcc_assert (c_parser_next_token_is (parser, CPP_LESS));
    GenericModel *model=generic_model_new(TRUE,GEN_FROM_DECL_SPECS);
	self->currentModelOfSpecs.specs=specs;
	self->currentModelOfSpecs.genericModel=model;
}

/**
 * 解析 如 abcd(Abc<E> *abc) 形式
 */
void generic_impl_push_generic_from_c_parm(GenericImpl *self,struct c_declspecs *specs)
{
	  tree type=specs->type;
	  if(TREE_CODE(type)==RECORD_TYPE){
		  char *className=class_util_get_class_name(type);
		  if(className==NULL){
			  error_at(input_location,"泛型只能用在类上。");
			  return;
		  }
	  }
	  c_parser *parser=self->parser;
	  gcc_assert (c_parser_next_token_is (parser, CPP_LESS));
	  GenericModel *model=generic_model_new(TRUE,GEN_FROM_DECL_SPECS);
	  self->currentModelOfSpecs.specs=specs;
	  self->currentModelOfSpecs.genericModel=model;
}

GenericModel *generic_impl_pop_generic_from_c_parm(GenericImpl *self,struct c_parm *parm)
{
	      struct c_declspecs *specs=parm->specs;
		  tree type=specs->type;
		  if(TREE_CODE(type)==RECORD_TYPE){
			  char *className=class_util_get_class_name(type);
			  if(className==NULL)
				  return NULL;
		  }
		  if(self->currentModelOfSpecs.specs && self->currentModelOfSpecs.specs==specs){
			  GenericModel *ret=self->currentModelOfSpecs.genericModel;
			  self->currentModelOfSpecs.genericModel=NULL;
			  self->currentModelOfSpecs.specs=NULL;
			  return ret;
		  }
		  return NULL;
}

/**
 * 当函数定义完后，检查返回值及参数是否有效。
 * 每一步：检查返回值
 */
void  generic_impl_check_func_at_field(GenericImpl *self,tree decl,struct c_declarator *declarator)
{
    GenericModel *declGen=c_aet_get_generics_model(decl);
    GenericModel * funcGen=c_aet_get_func_generics_model(decl);
    tree parms=getParmsFromDeclarator(declarator);
    checkClassFunc(self,decl,declGen,funcGen,parms);
}

/**
 * 检查以下两种泛型函数调用是否正确
 * setData<int>();
 * object->setData<int>()
 */
nboolean  generic_impl_check_func_at_call(GenericImpl *self,char *funcName,GenericModel *funcgen)
{
	   c_parser *parser=self->parser;
	   ClassName *className=class_impl_get()->className;
	   n_debug("generic_impl_check_func_at_call 00 className:%s model:%s",className==NULL?"null":className->sysName,generic_model_tostring(funcgen));
	   int undefine=generic_model_get_undefine_count(funcgen);
	   n_debug("set<int>此种形式:%s\n",generic_model_tostring(funcgen));
	   if(undefine<=0)
		   return funcgen;
	   if(!parser->isAet){
		   char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
		   //printf("当前函数是:---%s\n",funcName);
		   error_at(input_location,"调用泛型函数%qs的泛型参数不能有未定义的泛型。",funcName);
		   return NULL;
	   }
	   //在class中找，如果找不到undefine声明的类型，当前函数是不是泛型，如果声明也说明找到了
	   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	   GenericModel *classModel=info->genericModel;
	   tree currentFunc=current_function_decl;
	   char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
	   ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,currentFuncName);
	   GenericModel *funcModel=class_func_get_func_generic(entity);
	   GenericModel *newModel=generic_model_merge(classModel,funcModel);//合并类与泛型函数中声明的泛型单元。
	   int i;
	   for(i=0;i<funcgen->unitCount;i++){
	   		GenericUnit *unit=funcgen->genUnits[i];
	   		if(!unit->isDefine){
	   			if(!generic_model_exits_unit(newModel,unit)){
	   			   error_at(input_location,"调用泛型函数%qs的泛型%qs参数，在当前类:%qs和函数%qs中都找不到声明。",
	   					funcName,unit->name,className->userName,currentFuncName);
	   			   return FALSE;
	   			}
	   		}
	   	}
	   return TRUE;
}

/**
 * 检查形式
 * E abc=abc->getElement();
 * E getElement(){
 * }
 * 如果abc是具体的类型，报错.
 */
void  generic_impl_check_var_and_parm(GenericImpl *self,tree decl,tree init)
{
	if(!aet_utils_valid_tree(init))
		return;
	nboolean re=generic_util_is_generic_var_or_parm(decl);
	if(!re)
		return;
	char *initType=generic_util_get_type_str(init);
	if(initType==NULL)
		return;
	if(!generic_util_is_generic_ident(initType) && strcmp(initType,"void *")){
		 char *varType=generic_util_get_type_str(decl);
		 char *single=n_strndup(varType+strlen(varType)-1,1);
		 error_at(input_location,"不能从类型%qs转化到类型%qs。",initType,single);
		 n_free(single);
		 return;
	}
}

/**
 * 为检查形式
 * Abc<float> *re=(Abc<float>*)getData();
 * 把右边的<float>保存下来
 */
void generic_impl_ready_check_cast(GenericImpl *self,struct c_type_name *type_name,tree expr)
{
	if(self->currentModelOfSpecs.specs==type_name->specs){
		GenericModel *model=generic_impl_pop_generic_from_declspecs(self,type_name->specs);
		self->castCurrentModelSpecs.genericModel=model;
		self->castCurrentModelSpecs.expr=expr;
	}
}

GenericModel *generic_impl_get_cast_model(GenericImpl *self,tree expr)
{
	if(self->castCurrentModelSpecs.expr==expr){
		return self->castCurrentModelSpecs.genericModel;
	}
	return NULL;
}



void generic_impl_set_parser(GenericImpl *self,c_parser *parser)
{
   self->parser=parser;
}

GenericImpl *generic_impl_get()
{
	static GenericImpl *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(GenericImpl));
		 genericImplInit(singleton);
	}
	return singleton;
}


