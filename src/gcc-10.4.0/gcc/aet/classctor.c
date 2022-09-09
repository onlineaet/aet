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
#include "classctor.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classctor.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"
#include "genericcall.h"
#include "accesscontrols.h"



static void classCtorCtor(ClassCtor *self)
{
    self->funcHelp=func_help_new();
    int i;
    for(i=0;i<30;i++){
    	self->superInfos[i].sysCreate=FALSE;
    	self->superInfos[i].funcName=NULL;
    	self->superInfos[i].sysName=NULL;
    }
    self->superInfoCount=0;
    self->superOfSelfParseing=FALSE;
    self->superOfSelfCount=0;
}

/**
 * 在构造函数内如遇return 在return后加self;
 */
static void addSelfToReturn(ClassCtor *self)
{
	  c_parser *parser=self->parser;
	  c_token *token=c_parser_peek_token(parser); //}
	  if (token->type!=CPP_SEMICOLON){
		  return ;
	  }
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+1>AET_MAX_TOKEN){
			error("token太多了");
			return;
	  }
	  location_t  decl_loc = token->location;
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
	  }
	  aet_utils_create_token(&parser->tokens[0],CPP_NAME,"self",4);
	  parser->tokens_avail=tokenCount+1;
	  aet_print_token_in_parser("addSelfToReturn ------");
}

void  class_ctor_parser_return(ClassCtor *self,nboolean isConstructor)
{
   c_parser *parser=self->parser;
   if(parser->isAet && isConstructor){
	 if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
					//加一个self;
	    addSelfToReturn(self);
	 }
   }
}

/**
 * 加 return self;
 */
nboolean class_ctor_add_return_stmt(ClassCtor *self)
{
	  c_parser *parser=self->parser;
	  c_token *token=c_parser_peek_token(parser); //}
	  if (token->type!=CPP_CLOSE_BRACE){
		  return FALSE;
	  }
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
	  aet_utils_create_token(&parser->tokens[2],CPP_SEMICOLON,";",1);
	  aet_utils_create_token(&parser->tokens[1],CPP_NAME,"self",4);
	  aet_utils_create_return_token(&parser->tokens[0],decl_loc);
	  parser->tokens_avail=tokenCount+3;
	  aet_print_token_in_parser("class_ctor_add_return_stmt ----");
	  return TRUE;
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
	 n_debug("创建c_parm 99 00 %s, %s, %d\n",__FILE__, __FUNCTION__, __LINE__);
     id_declarator = id_declarator->declarator;
  }

  location_t end_loc = parser->last_token_location;

  location_t caret_loc = (id_declarator->u.id.id ? id_declarator->id_loc : start_loc);
  location_t param_loc = make_location (caret_loc, start_loc, end_loc);
  n_debug("创建c_parm 99  11 %s %s, %d\n",__FILE__, __FUNCTION__, __LINE__);
  tree expr;
  struct c_parm *parm= build_c_parm (specs, NULL_TREE,pointer, param_loc);
  push_parm_decl (parm, &expr);
  struct c_arg_info * args= get_parm_info (false, expr);
  pop_scope ();
  return args;

}


typedef struct _CandidateFun
{
	ClassFunc *mangle;
	WarnAndError *warnErr;
}CandidateFun;

static void freeCandidate_cb(CandidateFun *item)
{
	n_free(item->warnErr);
	n_free(item);
}


static nint warnCompare_cb(nconstpointer  cand1,nconstpointer  cand2)
{
	CandidateFun *p1 = (CandidateFun *)cand1;
	CandidateFun *p2 = (CandidateFun *)cand2;
    int a=p1->warnErr->warnCount;
    int b=p2->warnErr->warnCount;
    return (a > b ? +1 : a == b ? 0 : -1);
}

static void printWarnInfo(NList *okList)
{
	int len=n_list_length(okList);
	int i;
	for(i=0;i<len;i++){
	   	  CandidateFun *cand=(CandidateFun *)n_list_nth_data(okList,i);
	  	  WarnAndError *warnErr=cand->warnErr;
	  	  int j;
	  	  for(j=0;j<warnErr->warnCount;j++){
             n_debug("warning is :%s %d\n",cand->mangle->mangleFunName,warnErr->warns[j]);
	  	  }
	}

}

static CandidateFun *filterGoodFunc(NList *okList)
{
	  if(n_list_length(okList)==0){
		  printf("没有匹配的函数!!! %s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
		  return NULL;
	  }
   	  okList=n_list_sort(okList,warnCompare_cb);
   	  CandidateFun *cand=(CandidateFun *)n_list_nth_data(okList,0);
   	printWarnInfo(okList);
	  n_debug("找到了声明的函数 成功匹配参数，只有一个 decl code:%s name:%s",
	  		 get_tree_code_name(TREE_CODE(cand->mangle->fieldDecl)),IDENTIFIER_POINTER(DECL_NAME(cand->mangle->fieldDecl)));
	  return cand;
}

static tree createTempFunction(tree field)
{

   tree funid=DECL_NAME(field);
   tree type=TREE_TYPE(field);
   tree functionType=TREE_TYPE(type);
   tree decl = build_decl (0, FUNCTION_DECL, funid, default_function_type);
   TREE_TYPE(decl)=functionType;
   return decl;
}

static ClassFunc *candidateFunc(ClassCtor *self,vec<tree, va_gc> *exprlist,vec<tree, va_gc> *origtypes,
		vec<location_t> arg_loc,location_t expr_loc,NPtrArray *funcs)
{
    unsigned int i;
	NList *okList=NULL;
	for(i=0;i<funcs->len;i++){
	   ClassFunc *mangle=(ClassFunc *)n_ptr_array_index(funcs,i);
	   tree field = mangle->fieldDecl;
	   aet_warn_and_error_reset();
	   if(!aet_utils_valid_tree(field)){
		   n_warning("找到了声明的构造函数 但出错了 name:%s\n",mangle->mangleFunName);
		   continue;
	   }
	   tree decl=createTempFunction(field);
	   n_debug("找到了声明的构造函数 开始匹配参数 decl code:%s name:%s",get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)));
	   tree value=decl;
	   mark_exp_read (value);
	   value= aet_check_funcs_param (expr_loc, arg_loc, value,exprlist, origtypes);
	   if(value==error_mark_node){
		   n_debug("找到了声明的构造函数 不能匹配参数 decl code:%s name:%s", get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)));
	   }else{
		   n_debug("找到了声明的构造函数 有错误吗? decl code:%s name:%s 错误数:%d warn:%d",
				get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),
				argsFuncsInfo.errorCount,argsFuncsInfo.warnCount);
		  if(argsFuncsInfo.errorCount==0){
			CandidateFun *candidate=n_slice_new(CandidateFun);
			candidate->mangle=mangle;
			candidate->warnErr=aet_warn_and_error_copy();
			okList=n_list_append(okList,candidate);
		 }
	  }
	}
	CandidateFun *cand=filterGoodFunc(okList);
	ClassFunc *classFunc=NULL;
	if(cand!=NULL){
		classFunc=cand->mangle;
	}
	n_list_free_full(okList,freeCandidate_cb);
  	return classFunc;
}

/**
 * TREE_CODE(datum)!=VAR_DECL 是因为 是引用了变量的地址也即变量转地址 (&varName)
 */
static tree rebuildComponentRef(ClassCtor *self,tree componentref ,tree field)
{
	tree datum=TREE_OPERAND(componentref,0);
	if(TREE_CODE(datum)!=INDIRECT_REF && TREE_CODE(datum)!=VAR_DECL && TREE_CODE(datum)!=COMPONENT_REF && TREE_CODE(datum)!=ARRAY_REF){
		n_warning("在classctor中 rebuildComponentRef所需要的类型不符,不是 INDIRECT_REF VAR_DECL COMPONENT_REF ！！！%s",get_tree_code_name(TREE_CODE(datum)));
		return error_mark_node;
	}
    tree type = TREE_TYPE (datum);
    tree ref;
    bool datum_lvalue = lvalue_p (datum);
    tree subdatum =field;
    //printf("rebuildComponentRef 22 ---%p\n",subdatum);
    //printf("rebuildComponentRef 33 ---%s\n",get_tree_code_name(TREE_CODE(subdatum)));
	//printf("rebuildComponentRef 44 field:%s subdatum:%s %s %s %d\n",
	//		   get_tree_code_name(TREE_CODE(field)), get_tree_code_name(TREE_CODE(TREE_TYPE (subdatum))),__FILE__,__FUNCTION__,__LINE__);
    int quals;
    tree subtype;
    bool use_datum_quals;
	use_datum_quals = (datum_lvalue || TREE_CODE (TREE_TYPE (subdatum)) != ARRAY_TYPE);
	quals = TYPE_QUALS (strip_array_types (TREE_TYPE (subdatum)));
	if (use_datum_quals)
	  quals |= TYPE_QUALS (TREE_TYPE (datum));
    subtype = c_build_qualified_type (TREE_TYPE (subdatum), quals);
    TREE_TYPE(componentref)=subtype;
    TREE_OPERAND(componentref,1)=subdatum;
    return componentref;
}


/**
 * 获取参数的个数
 */
static int getParamsNumber(tree parm_types)
{
     tree first_parm_type;
     int count=0;
     for (first_parm_type = parm_types; parm_types;parm_types = TREE_CHAIN (parm_types)){
         tree parm = TREE_VALUE (parm_types);
		 //printf("getParamsNumber  %s args:%d\n",get_tree_code_name(TREE_CODE(parm)),count);
         if (parm !=void_type_node)
	         count++;
     }
     return count;
}

/**
 * 把 Abc( 重整为 Abc* Abc(
 */
static void rearrangeConstructor(c_parser *parser,ClassName *className)
{
	  c_token *funName=c_parser_peek_token(parser); //Abc
	  c_token *openParen=c_parser_peek_2nd_token (parser);//"(";
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+2>AET_MAX_TOKEN){
			 error("token太多了");
			 return;
	  }

	  int i;
	  for(i=tokenCount;i>0;i--){
			aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
	  }
	  aet_utils_create_token(&parser->tokens[3],CPP_OPEN_PAREN,"(",1);
	  aet_utils_copy_token(funName,&parser->tokens[2]);
	  tree value=aet_utils_create_ident(className->userName);
	  parser->tokens[2].value=value;
	  parser->tokens[2].id_kind=C_ID_ID;//关键
	  aet_utils_create_token(&parser->tokens[1],CPP_MULT,"*",1);
	  aet_utils_create_token(&parser->tokens[0],CPP_NAME,className->sysName,(int)strlen(className->sysName));
	  parser->tokens[0].id_kind=C_ID_TYPENAME;//关键
	  parser->tokens_avail=tokenCount+2;
	  aet_print_token_in_parser("class ctor rearrangeConstructor className ---- %s %s",className->sysName,className->userName);
}


/**
 * 检查是不是型如 Abc(的构造函数
 * 如果是代码用户写成 Abc() 通过c-parser后，变成了 com_ai_Abc 整有包名的。
 */
nboolean class_ctor_parser_constructor(ClassCtor *self,ClassName *className)
{
    if(className==NULL)
    	return FALSE;
    c_parser *parser=self->parser;
	if(c_parser_next_token_is (parser, CPP_NAME)){
		tree ident = c_parser_peek_token (parser)->value;
		const char *str1=IDENTIFIER_POINTER (ident);
		if(strcmp(str1,className->sysName)==0 && c_parser_peek_2nd_token(parser)->type==CPP_OPEN_PAREN){
			n_info("class_ctor_parser_constructor 这是一个构函数 name:%s current_function_decl:%p", str1,current_function_decl);
			if(current_function_decl){
			    location_t loc = c_parser_peek_token (parser)->location;
				error_at(loc,"不能在类中调用构造函数%qs。",className->userName);
				return FALSE;
			}
			if(class_mgr_is_interface(class_mgr_get(),className)){
			    location_t loc = c_parser_peek_token (parser)->location;
			    error_at(loc,"接口不能有有构造函数:%qs",className->userName);
			    return FALSE;
		    }
			rearrangeConstructor(parser,className);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * 获取参数的个数
 */
static nboolean isDefualtCtorField(tree fieldDecl)
{
   tree pointerType=TREE_TYPE(fieldDecl);
   tree funtype=TREE_TYPE(pointerType);
   tree args=TYPE_ARG_TYPES (funtype);
   int count=getParamsNumber(args);
   return count==1;
}

/**
 * 在class中声明的缺省的构造函数是否存在
 */
static nboolean  haveFieldOrDefine(ClassCtor *self,ClassName *className,nboolean field)
{
	NPtrArray *array=func_mgr_get_funcs(func_mgr_get(),className);
	if(array==NULL)
		return FALSE;
    int i;
    for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->orgiName,className->userName)==0 && aet_utils_valid_tree(field?item->fieldDecl:item->fromImplDefine)){
			tree decl=field?item->fieldDecl:item->fromImplDefine;
			if(field){
				nboolean re=isDefualtCtorField(decl);
				//printf("haveFieldOrDefine 00 classname:%s isDefault:%d\n",className->sysName,re);
				if(re)
					return TRUE;
			}else{
			   tree type=TREE_TYPE(decl);//函数类型的树
			   tree argTypes= TYPE_ARG_TYPES (type);
			   int count=getParamsNumber(argTypes);
			   //printf("haveFieldOrDefine 11 classname:%s count:%d\n",className->sysName,count);
			   if(count==1 || (class_func_have_generic_block(item) && count==2))
				   return TRUE;
			}
	   }
    }
    //n_warning("在方法haveFieldOrDefine中没有找到缺省的构造函数！");
    return FALSE;
}

nboolean  class_ctor_have_default_field(ClassCtor *self,ClassName *className)
{
	return haveFieldOrDefine(self,className,TRUE);
}

/**
 * 创建缺省的构造函数
 */
tree   class_ctor_create_default_decl(ClassCtor *self,ClassName *className,tree structType)
{
	c_parser *parser=self->parser;
	struct c_declspecs *specs;
	specs = build_null_declspecs ();
	struct c_typespec t;
	t.kind = ctsk_typedef;
	tree value=aet_utils_create_ident(className->sysName);
	t.spec = lookup_name (value);
	t.expr = NULL_TREE;
	t.expr_const_operands = true;
	c_token *token = c_parser_peek_token (parser);//随便找一个位置
	declspecs_add_type (token->location, specs, t);
	finish_declspecs (specs);
	tree prefix_attrs = specs->attrs;
	tree all_prefix_attrs = prefix_attrs;
	specs->attrs = NULL_TREE;
	struct c_declspecs *quals_attrs = build_null_declspecs ();
	char *funcName=className->userName;
	value=aet_utils_create_ident(funcName);
	struct c_declarator *funcsDeclarator= build_id_declarator (value);
	funcsDeclarator->id_loc = c_parser_peek_token (parser)->location;
	struct c_declarator *pointer= make_pointer_declarator (quals_attrs, funcsDeclarator);
	struct c_arg_info *args=createParam(parser,className->sysName);
	struct c_declarator *fundecl = build_function_declarator (args, pointer);
	struct c_declarator *pointerLast= make_pointer_declarator (quals_attrs, fundecl);
	nboolean change=func_mgr_change_class_func_decl(func_mgr_get(),pointerLast,className,structType);
	tree decls = NULL_TREE;
	tree postfix_attrs = NULL_TREE;
	tree d = grokfield (c_parser_peek_token (parser)->location,pointerLast, specs, NULL_TREE, &all_prefix_attrs);
	decl_attributes (&d, chainon (postfix_attrs,all_prefix_attrs), 0);
	DECL_CHAIN (d) = decls;
	decls = d;
	if(change){
	   func_mgr_set_class_func_decl(func_mgr_get(),d,className);
       char *id=IDENTIFIER_POINTER(DECL_NAME(d));
       ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,id);
       if(entity==NULL){
           n_error("在类%s中找不到函数:%s 不应该出现这样的错误！！！\n",className->sysName,id);
           return NULL_TREE;
       }
       entity->permission=CLASS_PERMISSION_PUBLIC;
	}
	return decls;
}

/**
 * 加缺省的构造函数定义
 */
void class_ctor_build_default_define(ClassCtor *self,ClassName *className)
{
	  c_parser *parser=self->parser;
	  if(class_mgr_is_interface(class_mgr_get(),className)){
          return;
      }
	  c_token *constructor = c_parser_peek_token (parser);
 	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+5>AET_MAX_TOKEN){
			error("token太多了");
			return;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
	    aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+5]);
	  }
	  aet_utils_create_token(&parser->tokens[4],CPP_CLOSE_BRACE,"}",1);
	  aet_utils_create_token(&parser->tokens[3],CPP_OPEN_BRACE,"{",1);
	  aet_utils_create_token(&parser->tokens[2],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[0],CPP_NAME,className->sysName,(int)strlen(className->sysName));
	  parser->tokens[0].id_kind=C_ID_ID;//关键
	  parser->tokens_avail=tokenCount+5;
	  aet_print_token_in_parser("class_ctor_build_default_define className Abc(){}---- %s",className->sysName);
}

nboolean   class_ctor_have_default_define(ClassCtor *self,ClassName *className)
{
	return haveFieldOrDefine(self,className,FALSE);
}


nboolean  class_ctor_is(ClassCtor *self,struct c_declarator *declarator,ClassName *className)
{
	int i;
	struct c_declarator *funcdel=NULL;
	struct c_declarator *temp=declarator;
	for(i=0;i<100;i++){
		if(temp!=NULL){
		   enum c_declarator_kind kind=temp->kind;
		   if(kind==cdk_function){
			   n_debug("isConstruct 在class中找到一个函数声明 第%d个 %d %s\n",i,kind,aet_c_declarator_kind_str[kind]);
			   funcdel=temp;
               break;
		   }
		   temp=temp->declarator;
		}else{
			break;
		}
	}
	if(!funcdel)
		return FALSE;
	struct c_declarator *funid=funcdel->declarator;
	if(funid==NULL)
		return FALSE;
	enum c_declarator_kind kind=funid->kind;
	if(kind!=cdk_id)
		return FALSE;
  	tree funName=funid->u.id.id;
  	char *orgiName=IDENTIFIER_POINTER(funName);
    return strcmp(orgiName,className->userName)==0;
}

static void rearrangeSuper(ClassCtor *self,ClassName *className)
{
	  c_parser *parser=self->parser;
	  location_t  loc = c_parser_peek_token (parser)->location;
	  c_token *openbrace = c_parser_peek_token (parser);//{
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+4>AET_MAX_TOKEN){
			error("token太多了");
			return;
	  }
	  int i;
	  parser->tokens_avail=tokenCount+4;
	  for(i=tokenCount;i>1;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+4]);
	  }
	  aet_utils_create_token(&parser->tokens[4],CPP_SEMICOLON,";",1);
	  aet_utils_create_token(&parser->tokens[3],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[2],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_super_token(&parser->tokens[1],loc);
	  aet_print_token_in_parser("class actor rearrangeSuper className ---- %s",className->sysName);
}

/**
 * 如果构造函数的第一行不是 super则加一个缺省的
 * 返回TRUE 用户加的
 * 返回FALSE 系统加缺省的
 */
void class_ctor_add_super_keyword(ClassCtor *self,ClassName *className)
{
	  c_parser *parser=self->parser;
	  if(strcmp(className->userName,AET_ROOT_OBJECT)==0)
		  return;
      if (!c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
    	  return;
      }
	  c_token *token=c_parser_peek_2nd_token(parser);
	  nboolean isSuper=token->type == CPP_KEYWORD && token->keyword==RID_AET_SUPER;
	  n_debug("class_ctor_add_super_keyword %s %d",className->sysName,isSuper);
	  nboolean openParen=c_parser_peek_nth_token (parser,3)->type ==CPP_OPEN_PAREN;
	  int index=self->superInfoCount;
	  self->superInfos[index].sysName=n_strdup(className->sysName);
	  if(current_function_decl){
		  char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
		  self->superInfos[index].funcName=n_strdup(funcName);
	  }
	  if(isSuper && openParen){
		  self->superInfos[index].sysCreate=FALSE;
	  }else{
	     //加super();
		  self->superInfos[index].sysCreate=TRUE;
	      rearrangeSuper(self,className);
	  }
	  self->superInfoCount++;
}

/**
 * 判断调用self()时，该语句所在的构造函数中的super是系统加的还是用户加的
 */
nboolean class_ctor_self_is_first(ClassCtor *self,ClassName *className,int *error)
{
   int i;
   char *nowFuncName=NULL;
   if(current_function_decl){
	   nowFuncName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
   }
   for(i=0;i<self->superInfoCount;i++){
	   char *sysName=self->superInfos[i].sysName;
	   char *funcName=self->superInfos[i].funcName;
	   //printf("class_ctor_self_is_first --- %s %s %s %s\n",sysName,funcName,nowFuncName,className->sysName);
	   if(sysName!=NULL && !strcmp(sysName,className->sysName)){
	      // printf("class_ctor_self_is_first -11-- %s %s %s %s\n",sysName,funcName,nowFuncName,className->sysName);

		   if(funcName!=NULL && nowFuncName!=NULL && !strcmp(funcName,nowFuncName)){
		       // printf("class_ctor_self_is_first -22-- %s %s %s %s\n",sysName,funcName,nowFuncName,className->sysName);
			   return self->superInfos[i].sysCreate;
		   }
	   }
   }
   *error=1;
   return FALSE;
}

/**
 * 把super$() 变成XXXX * _check_super_return_null_var_1234=((XXX *)self)->XXX(
 * #define SUPER_CONSTRUCTOR_TEMP_VAR "_check_super_return_null_var_1234";//调用super$()时变成新语句 XXX *_check_super_return_null_var_1234=....
 *
 */
static void superToConstructor(ClassCtor *self,ClassName *className)
{
    c_parser *parser=self->parser;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(info==NULL){
        c_parser_error (parser, "找不到类:。");
        return;
    }
    char *parent=info->parentName.userName;
    if(parent==NULL){
        c_parser_error (parser, "找不到父类！");
        return;
    }
    ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&info->parentName);
    if(parentInfo==NULL){
        c_parser_error (parser, "找不到父类:。");
        return;
    }

      location_t  loc = c_parser_peek_token (parser)->location;
      c_token *openParen = c_parser_peek_token (parser);//->
      int tokenCount=parser->tokens_avail;
      if(tokenCount+11>AET_MAX_TOKEN){
            error("token太多了");
            return;
      }
      int i;
      parser->tokens_avail=tokenCount+11;
      for(i=tokenCount;i>0;i--){
             aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+11]);
      }
      aet_utils_create_if_token(&parser->tokens[0],input_location);
      aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
      aet_utils_create_token(&parser->tokens[2],CPP_OPEN_PAREN,"(",1);
      aet_utils_create_token(&parser->tokens[3],CPP_OPEN_PAREN,"(",1);
      aet_utils_create_token(&parser->tokens[4],CPP_NAME,info->parentName.sysName,strlen(info->parentName.sysName));
      parser->tokens[4].id_kind=C_ID_TYPENAME;//关键
      aet_utils_create_token(&parser->tokens[5],CPP_MULT,"*",1);
      aet_utils_create_token(&parser->tokens[6],CPP_CLOSE_PAREN,")",1);
      aet_utils_create_token(&parser->tokens[7],CPP_NAME,"self",4);
      aet_utils_create_token(&parser->tokens[8],CPP_CLOSE_PAREN,")",1);
      aet_utils_create_token(&parser->tokens[9],CPP_DEREF,"->",2);
      aet_utils_create_token(&parser->tokens[10],CPP_NAME,parent,strlen(parent));
      parser->tokens[10].id_kind=C_ID_ID;//关键
      self->superOfSelfParseing=TRUE;
      aet_print_token_in_parser("class_ctor superToConstructor className ---- %s superConstructorCount:%d",className->sysName,self->superOfSelfParseing);
}

/**
 * 完成super$()的调用 之前把super$()改在 if(....self->XXX();
 * 现在编译到分号; 把;号改成 ==NULL)return NULL;
 * if(xxx==NULL)
 *  return NULL;
 */
void  class_ctor_end_super_or_self_ctor_call(ClassCtor *self,tree expr)
{
    // printf("class_ctor_end_super_or_self_ctor_call--000 superOfSelfParseing:%d\n",self->superOfSelfParseing);
     c_parser *parser=self->parser;
     if(self->superOfSelfParseing)
           return;
     int i;
     nboolean find=FALSE;
     int pos=0;
     for(i=0;i<self->superOfSelfCount;i++){
         if(self->superOfSelf[i]==expr){
             find=TRUE;
             pos=i;
             break;
         }
     }
     if(!find)
         return;
     //移走找到的
     for(i=pos+1;i<self->superOfSelfCount;i++){
         self->superOfSelf[i-1]=self->superOfSelf[i];
     }
     self->superOfSelfCount--;
     location_t  loc = c_parser_peek_token (parser)->location;
     if(!c_parser_next_token_is (parser, CPP_SEMICOLON)){
        error_at(loc,"super$（）后，应接分号；");
        return;
     }

     c_parser_consume_token (parser);//consume 分号;
     int tokenCount=parser->tokens_avail;
     if(tokenCount+18>AET_MAX_TOKEN){
           error("token太多了");
           return;
     }
     parser->tokens_avail=tokenCount+18;
     for(i=tokenCount;i>0;i--){
            aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+18]);
     }
     /*==((void*)0)) return ((void*)0);*/
     aet_utils_create_token(&parser->tokens[0],CPP_EQ_EQ,"==",2);
     aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
     aet_utils_create_token(&parser->tokens[2],CPP_OPEN_PAREN,"(",1);
     aet_utils_create_void_token(&parser->tokens[3],loc);
     aet_utils_create_token(&parser->tokens[4],CPP_MULT,"*",1);
     aet_utils_create_token(&parser->tokens[5],CPP_CLOSE_PAREN,")",1);
     aet_utils_create_number_token(&parser->tokens[6],0);
     aet_utils_create_token(&parser->tokens[7],CPP_CLOSE_PAREN,")",1);
     aet_utils_create_token(&parser->tokens[8],CPP_CLOSE_PAREN,")",1);
     aet_utils_create_return_token(&parser->tokens[9],loc);
     aet_utils_create_token(&parser->tokens[10],CPP_OPEN_PAREN,"(",1);
     aet_utils_create_token(&parser->tokens[11],CPP_OPEN_PAREN,"(",1);
     aet_utils_create_void_token(&parser->tokens[12],loc);
     aet_utils_create_token(&parser->tokens[13],CPP_MULT,"*",1);
     aet_utils_create_token(&parser->tokens[14],CPP_CLOSE_PAREN,")",1);
     aet_utils_create_number_token(&parser->tokens[15],0);
     aet_utils_create_token(&parser->tokens[16],CPP_CLOSE_PAREN,")",1);
     aet_utils_create_token(&parser->tokens[17],CPP_SEMICOLON,";",1);
     aet_print_token_in_parser("class_ctor class_ctor_end_super_ctor_call className ----");
}


/**
 * 进这里说明是在构造函数内，并且是函数内的第一条语句
 */
void       class_ctor_parser_super(ClassCtor *self,ClassName *className)
{
   c_parser *parser=self->parser;
   location_t loc = c_parser_peek_token(parser)->location;
   c_parser_consume_token (parser);
   if(!parser->isAet){
	  error_at (loc, "super关键字只能用在类的实现中。%qs",className->sysName);
	  return;
   }
   if (c_parser_next_token_is (parser, CPP_DEREF)){
	 error_at(loc,"在构造函数内调用父类的构造函数形式只能是:super()或super(parm) %qs",className->sysName);
	 return;
   }else if(c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
		 //如果出现super(但不是第一条句时错的
	  superToConstructor(self,className);
   }else{
	 error_at(loc,"在构造函数内调用父类的构造函数形式只能是:super()或super(parm) %qs",className->sysName);
	 return;
   }
}


static char *getLowClassName(tree field)
{
   tree pointerType=TREE_TYPE(field);
   tree type=TREE_TYPE(pointerType);
   tree next=TYPE_NAME(type);
   tree id=DECL_NAME(next);
   return IDENTIFIER_POINTER(id);
}

static ClassFunc *getCandidate(ClassCtor *self,ClassName *className,vec<tree, va_gc> *exprlist,
		vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc)
{
    NPtrArray *funcs=func_mgr_get_constructors(func_mgr_get(),className);
    if(funcs==NULL || funcs->len==0){
        n_info("没有mangle的构造函数名 00:%s",className);
        if(funcs)
           	n_ptr_array_unref(funcs);
       	return NULL;
    }
   	ClassFunc *classFunc= candidateFunc(self,exprlist,origtypes,arg_loc,expr_loc,funcs);
   	n_ptr_array_set_free_func(funcs,NULL);
   	n_ptr_array_unref(funcs);
   	return classFunc;
}

/**
 * 根据参数选择最终的构造函数
 */
tree  class_ctor_select(ClassCtor *self,tree func,vec<tree, va_gc> *exprlist,
		vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc )
{
	c_parser *parser=self->parser;
	tree field=TREE_OPERAND(func,1);
	tree id=DECL_NAME (field);
	char *funName=IDENTIFIER_POINTER(id);
	char *lowClassName=getLowClassName(field);
	BINFO_FLAG_4(func)=0;
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),lowClassName);
	if(className==NULL)
		className=class_mgr_get_class_name_by_user(class_mgr_get(),funName);
	StaticFuncParm *staticParm=parser_static_get_func_parms(parser_static_get(),exprlist);
	if(staticParm!=NULL){
       n_warning("class_ctor_select 有不确定的静态函数参数");
       parser_static_select_static_parm_for_ctor(parser_static_get(),className,staticParm);
       if(staticParm->ok){
    	   n_info("class_ctor_select 可以替换vec中的参数列表了");
           parser_static_replace(parser_static_get(),staticParm,exprlist);
       }else{
    	   //报错
    	   parser_static_report_error(parser_static_get(),staticParm,exprlist);
    	   return;
       }
       //free staticParm;
	   parser_static_free(parser_static_get(),staticParm);
	   staticParm=NULL;
	}
	ClassFunc *classFunc=getCandidate(self,className ,exprlist, origtypes,arg_loc,expr_loc);
	if(classFunc==NULL || !aet_utils_valid_tree(classFunc->fieldDecl)){
		n_warning("class_ctor_select 33 error %s %s %s\n",get_tree_code_name(TREE_CODE(field)),className->sysName,funName);
		error_at(expr_loc,"传递的参数与构造函数%qs不匹配。请检查类%qs中是否声明了构造函数。",funName,className->userName);
		return error_mark_node;
	}
	GenericModel *funcGenericDefine=NULL;
	if(class_func_is_func_generic(classFunc) || class_func_is_query_generic(classFunc)){
		n_debug("class_ctor_select 进这里了------因为%s是一个泛型类或带问号的构造函数。\n",id);
		generic_call_add_fpgi_parm(generic_call_get(),classFunc,className,exprlist,funcGenericDefine);
	}
	tree last=classFunc->fieldDecl;
	location_t accessLoc=aet_utils_get_location(expr_loc);//如果在附加代码中,该方法返回原始的位置
	access_controls_access_method(access_controls_get(),accessLoc,classFunc);
	func=rebuildComponentRef(self,func,last);
	return func;
}

/**
 * 来自在构造函数中第一条语句调用self()
 */
tree  class_ctor_select_from_self(ClassCtor *self,tree func,vec<tree, va_gc> *exprlist,
		vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc )
{
	c_parser *parser=self->parser;
	char *funName=IDENTIFIER_POINTER(DECL_NAME(func));
	char *lowClassName=funName;
	BINFO_FLAG_0(func)=0;
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),lowClassName);
	StaticFuncParm *staticParm=parser_static_get_func_parms(parser_static_get(),exprlist);
	if(staticParm!=NULL){
       n_warning("有不确定的静态函数参数\n");
       parser_static_select_static_parm_for_ctor(parser_static_get(),className,staticParm);
       if(staticParm->ok){
    	   n_info("可以替换vec中的参数列表了\n");
           parser_static_replace(parser_static_get(),staticParm,exprlist);
       }else{
    	   //报错
    	   parser_static_report_error(parser_static_get(),staticParm,exprlist);
    	   return;
       }
       //free staticParm;
	   parser_static_free(parser_static_get(),staticParm);
	   staticParm=NULL;
	}
	ClassFunc *classFunc=getCandidate(self,className ,exprlist, origtypes,arg_loc,expr_loc);
	if(classFunc==NULL || !aet_utils_valid_tree(classFunc->fieldDecl)){
		n_warning("class_ctor_select 33 error %s\n",funName);
		return error_mark_node;
	}
	GenericModel *funcGenericDefine=NULL;
	if(class_func_is_func_generic(classFunc) || class_func_is_query_generic(classFunc)){
		printf("class_ctor_select_from_self 进这里了------\n");
		generic_call_add_fpgi_parm(generic_call_get(),classFunc,className,exprlist,funcGenericDefine);
	}
	tree last=classFunc->fieldDecl;
	tree selfVarOrParm = (*exprlist)[0];
	func=func_help_create_itself_deref(self->funcHelp,selfVarOrParm,last,expr_loc);
	return func;
}

/**
 * 如果构造函数第一条语句是self();
 * 改为if(XXX(self,
 */
void class_ctor_process_self_call(ClassCtor *self,ClassName *className)
{
     c_parser *parser=self->parser;
     location_t  loc = c_parser_peek_token (parser)->location;
     c_parser_consume_token (parser);//self;
     int tokenCount=parser->tokens_avail;
     if(tokenCount+5>AET_MAX_TOKEN){
           error("token太多了");
           return;
     }
     int i;
     parser->tokens_avail=tokenCount+5;
     for(i=tokenCount;i>0;i--){
            aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+5]);
     }
     char *constructorFuncName=className->userName;
     /*if(self->XXX*/
     aet_utils_create_if_token(&parser->tokens[0],input_location);
     aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
     aet_utils_create_token(&parser->tokens[2],CPP_NAME,"self",4);
     aet_utils_create_token(&parser->tokens[3],CPP_DEREF,"->",2);
     aet_utils_create_token(&parser->tokens[4],CPP_NAME,constructorFuncName,strlen(constructorFuncName));
     aet_print_token_in_parser("class_ctor_process_self_call ----%s",className->sysName);
     self->superOfSelfParseing=TRUE;
}

/**
 * self(new Abc()) 以下功能就是区别出 self生成的构造函数与 new Abc生成的构造函数区别
 */
void class_ctor_set_tag_for_self_and_super_call(ClassCtor *self,tree ref)
{
    if(self->superOfSelfParseing){
        self->superOfSelfParseing=FALSE;
        self->superOfSelf[self->superOfSelfCount++]=ref;
    }
}

ClassCtor *class_ctor_new()
{
	ClassCtor *self = n_slice_alloc0 (sizeof(ClassCtor));
	classCtorCtor(self);
	return self;
}


