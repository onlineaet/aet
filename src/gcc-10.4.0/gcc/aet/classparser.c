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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "classparser.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "newobject.h"
#include "genericcall.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericexpand.h"
#include "genericquery.h"
#include "parserhelp.h"
#include "middlefile.h"
#include "ifacefile.h"
#include "enumparser.h"

/**
 * 主要实现功能：
 * Class Abc{
 *   int getName();
 *   int getName(int a)
 * }；
 * 1.把Class Abc{分两二步替换为 typedef struct _Abc Abc 然后 再Class _Abc，
 *    struct 是一个类型声明说明符class_parser_parser_class_specifier 然后再调declspecs_add_type
 * 2.把Abc替换为_Abc
 * 4.把 int getName()变成 int (*getName)();主要是把struct c_parser中的tokens_buf改为从原来的2改为5
 *   并添加 ( * )三个token
 * 5.在 c_parser_class_declaration 生成每个函数指针的FIELD_DECL树，然后函数名被AetMangle改名。
 * 6.class_mgr_add把tree RECORD_TYPE 加入到ClassMgr中 里面用的是_Abc名称 其它用的是Abc，注意!!!
 */

int enter_aet=0;

static void classParserInit(ClassParser *self)
{
	self->classCtor=class_ctor_new();
	self->classInterface=class_interface_new();
	self->classInit=class_init_new();
	self->classFinalize=class_finalize_new();
	self->classFinal=class_final_new();
	self->classPackage=class_package_new();
	self->classPermission=class_permission_new();
	self->classBuild=class_build_new();//AClass中的变量赋值代码生成器
	self->funcCheck=func_check_new();
	self->state=CLASS_STATE_STOP;
	self->currentClassName=NULL;
	n_log_set_param_from_env();
	n_log_append_domain(in_fnames[0]);
}

/**
 * 把 getName( 重整为  getName(Abc *self
 */
static void addSelfToField(c_parser *parser,ClassName *className,int openParenPos,nboolean isFuncGeneric)
{
	  c_token *who=c_parser_peek_nth_token (parser,openParenPos+1);
	  c_token *who1=c_parser_peek_nth_token (parser,openParenPos+2);
	  int tokenCount=parser->tokens_avail;
	  int i;
	  int addGenToken=0;
	  if(isFuncGeneric){
		  addGenToken=3;
	  }
      nboolean dhaoAfter=FALSE;//逗号加后面还是前面
	  if (who->keyword == RID_VOID  && who1->type == CPP_CLOSE_PAREN){
		  //printf("class_parser 进这里了---000 who->keyword == RID_VOID  && who1->type == CPP_CLOSE_PAREN %d openParenPos:%d\n",tokenCount,openParenPos);
		  //需要加2个token 因为 void被替换了
		  //getName(void) getName 1 ( 2 void 3 )4
		  parser->tokens_avail=tokenCount+2+addGenToken;
		  for(i=tokenCount;i>openParenPos+1;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2+addGenToken]);
		  }
	  }else{//()->(Abc *self)
		  //需要加2个token 因为 void被替换了
		  int offset=(who->type == CPP_CLOSE_PAREN)?3+addGenToken:4+addGenToken;
		  //printf("class_parser 进这里了---111 who->type == CPP_CLOSE_PAREN %d openParenPos:%d offset:%d\n",tokenCount,openParenPos,offset);
		  parser->tokens_avail=tokenCount+offset;
		  for(i=tokenCount;i>openParenPos;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+offset]);
		  }
		  if(offset==4+addGenToken){
	  		  aet_utils_create_token(&parser->tokens[openParenPos+3],CPP_COMMA,",",1);
	  		  dhaoAfter=TRUE;
		  }
	  }
	  aet_utils_create_token(&parser->tokens[openParenPos+2],CPP_NAME,"self",4);
	  aet_utils_create_token(&parser->tokens[openParenPos+1],CPP_MULT,"*",1);
	  aet_utils_create_token(&parser->tokens[openParenPos],CPP_NAME,className->sysName,strlen(className->sysName));
	  parser->tokens[openParenPos].id_kind=C_ID_TYPENAME;//关键
	  if(addGenToken>0){
		  char *varName=AET_FUNC_GENERIC_PARM_NAME;
		  if(dhaoAfter){
	  		 aet_utils_create_token(&parser->tokens[openParenPos+6],CPP_COMMA,",",1);
	  	     aet_utils_create_token(&parser->tokens[openParenPos+5],CPP_NAME,varName,strlen(varName));
		     aet_utils_create_token(&parser->tokens[openParenPos+4],CPP_NAME,AET_FUNC_GENERIC_PARM_STRUCT_NAME,strlen(AET_FUNC_GENERIC_PARM_STRUCT_NAME));
		     parser->tokens[openParenPos+4].id_kind=C_ID_TYPENAME;//关键
		  }else{
			 aet_utils_create_token(&parser->tokens[openParenPos+5],CPP_NAME,varName,strlen(varName));
			 aet_utils_create_token(&parser->tokens[openParenPos+4],CPP_NAME,AET_FUNC_GENERIC_PARM_STRUCT_NAME,strlen(AET_FUNC_GENERIC_PARM_STRUCT_NAME));
			 parser->tokens[openParenPos+4].id_kind=C_ID_TYPENAME;//关键
		     aet_utils_create_token(&parser->tokens[openParenPos+3],CPP_COMMA,",",1);
		  }
	  }
	  aet_print_token_in_parser("class_parser rearrangeMode className ---- sysName:%s userName:%s addGenToken:%d",className->sysName,className->userName,addGenToken);
}

static void rearrangeMode(c_parser *parser,ClassName *className,int openParenPos,nboolean isFuncGeneric)
{
     //第一步 把 getName(变成 (*getName(
	  c_token *who=c_parser_peek_nth_token (parser,openParenPos+1);
	  c_token *who1=c_parser_peek_nth_token (parser,openParenPos+2);
	  int i;
	  int tokenCount=parser->tokens_avail;
	  for(i=tokenCount;i>openParenPos-2;i--){
		 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
	  }
	  aet_utils_create_token(&parser->tokens[openParenPos-2],CPP_OPEN_PAREN,"(",1);
	  aet_utils_create_token(&parser->tokens[openParenPos-1],CPP_MULT,"*",1);
	  parser->tokens_avail=tokenCount+2;
	  //aet_print_token_in_parser("className ---- 000 %s tokenCount:%d openParenPos:%d",className->sysName,tokenCount,openParenPos);
	  //第二步 把 (*getName(变成 (*getName)(
	  openParenPos+=2;
	  tokenCount=parser->tokens_avail;
	  for(i=tokenCount;i>openParenPos-1;i--){
		 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
	  }
	  aet_utils_create_token(&parser->tokens[openParenPos-1],CPP_CLOSE_PAREN,")",1);
	  parser->tokens_avail=tokenCount+1;
	  ///aet_print_token_in_parser("className ---- 111 %s tokenCount:%d openParenPos:%d\n",className->sysName,tokenCount,openParenPos);
	  openParenPos+=1;
	  addSelfToField(parser,className,openParenPos,isFuncGeneric);
}


static void checkConstructor(c_parser *parser,c_token *token,ClassName *className,nboolean need)
{
	 if(!need)
		 return;
	 tree ident = token->value;
	 const char *funName=IDENTIFIER_POINTER (ident);
	 if(strcmp(funName,className->userName)==0){
         c_parser_error (parser, "除构造方法外，其它方法不能使用类名命名。");
	 }
	 if((strcmp(funName,"unref")==0 || strcmp(funName,"ref")==0) && strcmp(className->userName,AET_ROOT_OBJECT)){
         c_parser_error (parser, "ref或unref函数名是系统保留的，不能覆盖。");
	 }
}

static nboolean isFieldFunc(ClassParser *self,tree field)
{
	//检查是不是field_decl
	if(TREE_CODE(field)!=FIELD_DECL)
		return FALSE;
	char *id=IDENTIFIER_POINTER(DECL_NAME(field));
	int len=IDENTIFIER_LENGTH(DECL_NAME(field));
	if(id==NULL || len<2 || id[0]!='_' || id[1]!='Z')
		return FALSE;
	tree type=TREE_TYPE(field);
	if(TREE_CODE(type)!=POINTER_TYPE)
		return FALSE;
	tree funtype=TREE_TYPE(type);
	if(TREE_CODE(funtype)!=FUNCTION_TYPE)
		return FALSE;
    return TRUE;
}

/**
 * 检查泛型在函数中或变量中是否合法
 * gen就变量声明的泛型
 * 如果decl是函数，就是函数返回值的泛型
 * funcGen是泛型函数
 */
static void checkAndSetGeneric(ClassParser *self,tree decl,GenericModel *gen,GenericModel *funcGen,nboolean isFunc,struct c_declarator *declarator)
{
	  if(gen){
		 n_debug("checkAndSetGeneric 00，field是一个变量声明或函数声明的返回值 %p %p\n",decl,gen);
	     c_aet_set_generics_model(decl,gen);
	  }
	  if(isFunc){
	     if(funcGen){
	    	 int gs= generic_model_get_count( funcGen);
	    	 int undefine=generic_model_get_undefine_count(funcGen);
	    	 if(gs!=undefine){
	    		 error_at(DECL_SOURCE_LOCATION(decl),"声明泛型函数的泛型不能有具体的类型。");
	    		 return;
	    	 }
	    	 n_debug("checkAndSetGeneric 11 field_decl是一个泛型函数 %s:decl:%p funcGeneric:%p gen：%p\n",IDENTIFIER_POINTER(DECL_NAME(decl)),decl,funcGen,gen);
		     c_aet_set_func_generics_model(decl,funcGen);
	     }
	     generic_impl_check_func_at_field(generic_impl_get(),decl,declarator);
	  }else{
		   nboolean  ok= generic_impl_check_var(generic_impl_get(),decl,gen);
		   n_debug("checkAndSetGeneric 22 检查带泛型的类变量 ok:%d\n",ok);
	  }
}


/**
 * setData(Abc<?> *abc) ---->setData(Efg *self,FuncGenParmInfo tempFgpi1234,Abc<?> *abc);
 * 专门分析结构体内的内容
 * struct中是不允许static关键字的，c_parser_declspecs第三个参数
 * bool scspec_ok原来是false 为了支持static 现在改为true
 */
static tree  c_parser_class_declaration (ClassParser *self,ClassInfo *classInfo,tree structTree,ClassType classType,int *isStatic)
{
    c_parser *parser=self->parser;
    struct c_declspecs *specs;
    tree prefix_attrs;
    tree all_prefix_attrs;
    tree decls;
    location_t decl_loc;
    ClassName *className=&classInfo->className;
    nboolean needCheckConstructors=TRUE;
    nboolean needCheckFinalize=TRUE;
    tree fieldGeneric=NULL_TREE;//函数返回值或变量声明是否是泛型
    tree funcGeneric=NULL_TREE; //是不是一个函数泛型
    GenericModel *fieldGenericModel=NULL;//函数返回值或变量声明是否是泛型
    GenericModel *funcGenericModel=NULL; //是不是一个函数泛型
    nboolean isIface=class_info_is_interface(classInfo);//是不是接口声明
    if (c_parser_next_token_is_keyword (parser, RID_EXTENSION)){
       int ext;
       tree decl;
       ext = c_disable_extension_diagnostics ();
       c_parser_consume_token (parser);
       decl = c_parser_class_declaration (self,classInfo,structTree,classType,isStatic);
       c_restore_extension_diagnostics (ext);
       n_debug("新----struct内的声明 ---00 RID_EXTENSION 返回 ");
       return decl;
    }
    if (c_parser_next_token_is_keyword (parser, RID_STATIC_ASSERT)){
       c_c_parser_static_assert_declaration_no_semi (parser);
       n_debug("新----struct内的声明 ---11 RID_STATIC_ASSERT 返回 ");
       return NULL_TREE;
    }

    FieldDecorate dr=class_permission_try_parser(self->classPermission,classType);
    if(dr.isStatic){
        n_debug("新----struct内的声明 是一个静态声明明符 11 编完class$后再处理 sysClassName:%s permission:%d", className->sysName,dr.permission);
        *isStatic=1;
         parser_static_collect_token(parser_static_get(),dr.permission,dr.isFinal);
         return NULL_TREE;
    }
	n_debug("class_permission_set_decorate 11 %s fileName:%s state:%d permission:%d",self->currentClassName->sysName,self->fileName,self->state,dr.permission);
    class_permission_set_decorate(self->classPermission,&dr);
	if(c_parser_next_token_is (parser, CPP_NAME)){
	    aet_print_token(c_parser_peek_token (parser));
	    needCheckConstructors=!(class_ctor_parser_constructor(self->classCtor,className));
	    c_token *token=c_parser_peek_token (parser);
	    n_debug("新----struct内的声明 是一个泛型吗------00 %s\n",IDENTIFIER_POINTER(token->value));
		if(generic_util_valid_id(token->value)){
			generic_impl_replace_token(generic_impl_get(),token);
		}
	}else if(c_parser_next_token_is (parser, CPP_COMPL)){
		class_finalize_parser(self->classFinalize,className);
	}
	//检查是不是泛型方法
	//格式如下：public$ <T> void getName(); <T>在函数返回值之前。
    if(c_parser_next_token_is (parser, CPP_LESS)){
	    funcGenericModel=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
	    n_debug("新----struct内的声明 可以是一个泛型函数\n");
		if(c_parser_next_token_is (parser, CPP_NAME)){
		    needCheckConstructors=!(class_ctor_parser_constructor(self->classCtor,className));
		    c_token *token=c_parser_peek_token (parser);
		    n_debug("新----struct内的声明 是一个泛型吗------11 %s\n",IDENTIFIER_POINTER(token->value));
			if(generic_util_valid_id(token->value)){
				generic_impl_replace_token(generic_impl_get(),token);
			}
		}else if(c_parser_next_token_is (parser, CPP_COMPL)){
			class_finalize_parser(self->classFinalize,className);
		}
    }

    specs = build_null_declspecs ();
    decl_loc = c_parser_peek_token (parser)->location;
    n_debug("新----struct内的声明 00 建立空的声明说明符 sysClassName:%s", className->sysName);
    aet_print_token(c_parser_peek_token (parser));
    c_parser_declspecs (parser, specs, false, true, true,true, false, true, true, cla_nonabstract_decl);
    if (parser->error)
       return NULL_TREE;
    if (!specs->declspecs_seen_p){
       c_parser_error (parser, "expected specifier-qualifier-list");
       return NULL_TREE;
    }
    finish_declspecs (specs);
    n_debug("新----struct内的声明 11 到这里specs已有声明说明符了比如 int 完成finish_declspecs %p",specs);
    if(TREE_CODE(specs->type)==ENUMERAL_TYPE){
         if(specs->typespec_kind==ctsk_tagdef){
             n_debug("为类中的枚举加入类型声明 %s\n",className->sysName);
             enum_parser_create_decl(enum_parser_get(),decl_loc,className,specs,dr.permission);
        }
    }
    /**
     * 检查函数返回类型的泛型
     * 如 Abc<E> *getAbc();
     * 或是变量声明的泛型如：
     * Abc<E> *abc;
     */
    if(c_parser_next_token_is (parser, CPP_LESS)){
       n_error("不会再进这里了，先保留c_parser_class_declaration");
	   fieldGenericModel=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
    }else{
        fieldGenericModel=generic_impl_pop_generic_from_declspecs(generic_impl_get(),specs);
    }

    /*如果next是* 然后下一个是 CPP_NAME 再下一个是"("说明这是一个返回指针的函数
     *如果next是CPP_NAME 再下一个是"("说明是一个函数
     * 把int getName 变成 int (*getName
    */
    if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_NAME
		  && !c_token_starts_typename(c_parser_peek_2nd_token(parser)) && c_parser_peek_nth_token(parser,3)->type==CPP_OPEN_PAREN){
    	n_debug("新----struct内的声明 --11--把函数声明'int * getName('重整为'int *(*getName)('  此种格式 ");
	  /*匹配 * getName(*/
	  checkConstructor(parser,c_parser_peek_2nd_token(parser),className,needCheckConstructors);
	  rearrangeMode(parser,className,3,funcGenericModel!=NULL);
    }else  if(c_parser_next_token_is (parser, CPP_NAME) && c_parser_peek_2nd_token(parser)->type==CPP_OPEN_PAREN){
    	n_debug("新----struct内的声明 -22-- 把函数声明'int  getName('重整为'int (*getName)('  此种格式  ");
	  /*匹配 getName(*/
	  checkConstructor(parser,c_parser_peek_token(parser),className,needCheckConstructors);
	  rearrangeMode(parser,className,2,funcGenericModel!=NULL);
    }else if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_MULT &&
		  c_parser_peek_nth_token(parser,3)->type==CPP_NAME
		  && !c_token_starts_typename(c_parser_peek_nth_token(parser,3)) && c_parser_peek_nth_token(parser,4)->type==CPP_OPEN_PAREN){
    	n_debug("新----struct内的声明 --33--把函数声明'int ** getName('重整为'int *(*getName)('  此种格式  ");
	  checkConstructor(parser,c_parser_peek_nth_token(parser,3),className,needCheckConstructors);
	  /*匹配 ** getName(*/
	  rearrangeMode(parser,className,4,funcGenericModel!=NULL);
    }else if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_MULT
		  && c_parser_peek_nth_token(parser,3)->type==CPP_MULT
		  && c_parser_peek_nth_token(parser,4)->type==CPP_NAME
		  && !c_token_starts_typename(c_parser_peek_nth_token(parser,4)) && c_parser_peek_nth_token(parser,5)->type==CPP_OPEN_PAREN){
    	n_debug("新----struct内的声明 --33--把函数声明'int ** getName('重整为'int *(*getName)('  此种格式 ");
	  checkConstructor(parser,c_parser_peek_nth_token(parser,3),className,needCheckConstructors);
	  /*匹配 ** getName(*/
	  rearrangeMode(parser,className,5,funcGenericModel!=NULL);
    }

   if (c_parser_next_token_is (parser, CPP_SEMICOLON) || c_parser_next_token_is (parser, CPP_CLOSE_BRACE)){
	   n_debug("新----struct内的声明 22 如果是CPP_SEMICOLON或CPP_CLOSE_BRACE 返回 ");
      tree ret;
      if (specs->typespec_kind == ctsk_none){
	     pedwarn (decl_loc, OPT_Wpedantic,"ISO C forbids member declarations with no members");
	     shadow_tag_warned (specs, pedantic);
	     ret = NULL_TREE;
	  }else{
	  /* Support for unnamed structs or unions as members of
	     structs or unions (which is [a] useful and [b] supports
	     MS P-SDK).  */
	     tree attrs = NULL;
	     n_debug("新----struct内的声明 33 Support for unnamed structs ");
	     if(!RECORD_OR_UNION_TYPE_P (specs->type) && TREE_CODE(specs->type)==ENUMERAL_TYPE){
	    	// printf("在类%s中定义了枚举%s，但没有声明变量。\n",className->sysName,IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(specs->type))));
	    	// printf("在类%s中定义了枚举%s，但没有声明变量。\n",className->sysName,IDENTIFIER_POINTER(TYPE_NAME(specs->type)));

	    	 return NULL_TREE;
	     }
	     ret = grokfield (c_parser_peek_token (parser)->location,build_id_declarator (NULL_TREE), specs,NULL_TREE, &attrs);
	     if (ret)
	       decl_attributes (&ret, attrs, 0);
	  }
      return ret;
   }
   n_debug("新----struct内的声明 44 不是CPP_SEMICOLON或CPP_CLOSE_BRACE 继续");

  /* Provide better error recovery.  Note that a type name here is valid,
     and will be treated as a field name.  */
   if (specs->typespec_kind == ctsk_tagdef
      && TREE_CODE (specs->type) != ENUMERAL_TYPE
      && c_parser_next_token_starts_declspecs (parser)
      && !c_parser_next_token_is (parser, CPP_NAME)){
      c_parser_error (parser, "expected %<;%>, identifier or %<(%>");
      parser->error = false;
      n_debug("新----struct内的声明 55 返回 NULL_TREE ");
      return NULL_TREE;
   }
   pending_xref_error ();
   prefix_attrs = specs->attrs;
   all_prefix_attrs = prefix_attrs;
   specs->attrs = NULL_TREE;
   decls = NULL_TREE;
   int testcount=0;
   while (true){
      /* Declaring one or more declarators or un-named bit-fields.  */
      struct c_declarator *declarator;
      bool dummy = false;
      if (c_parser_next_token_is (parser, CPP_COLON)){
	    declarator = build_id_declarator (NULL_TREE);
      }else{
    	  n_debug("新----struct内的声明 66 一定进这里 这时peek token 声明符getName declarator 的id_loc可能会变很大。不正常！！，count:%d\n", testcount);
  	      //在这里token 是getName了
	      declarator = c_parser_declarator(parser,specs->typespec_kind != ctsk_none,C_DTR_NORMAL, &dummy);
      }
      if (declarator == NULL){
    	  n_debug("新----struct内的声明 77 出错了 break count:%d", testcount);
	      c_c_parser_skip_to_end_of_block_or_statement (parser);
	      break;
	  }
      if (c_parser_next_token_is (parser, CPP_COLON)
	  || c_parser_next_token_is (parser, CPP_COMMA)
	  || c_parser_next_token_is (parser, CPP_SEMICOLON)
	  || c_parser_next_token_is (parser, CPP_CLOSE_BRACE)
	  || c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE)){
	     tree postfix_attrs = NULL_TREE;
	     tree width = NULL_TREE;
	     tree d;
	     if (c_parser_next_token_is (parser, CPP_COLON)){
	         c_parser_consume_token (parser);
	         width = c_c_parser_expr_no_commas (parser, NULL).value;
	     }
	     if(c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE))
	        postfix_attrs = c_c_parser_gnu_attributes (parser);
	     nboolean isQueryGenFunc=generic_query_is_function_declarator(generic_query_get(),className,declarator);
    	 if(!funcGenericModel && isQueryGenFunc){
    		 generic_query_add_parm_to_declarator(generic_query_get(),declarator);
	     }
	    // printf("新----struct内的声明 88 并在这里改名 grokfield className:%s isQueryFunction:%d\n", className->sysName,isQueryGenFunc);
	     nboolean change=func_mgr_change_class_func_decl(func_mgr_get(),declarator,className,structTree);
	     d = grokfield (c_parser_peek_token (parser)->location,declarator, specs, width, &all_prefix_attrs);
	     decl_attributes (&d, chainon (postfix_attrs,all_prefix_attrs), 0);
	     checkAndSetGeneric(self,d, fieldGenericModel,funcGenericModel,change,declarator);
	     DECL_CHAIN (d) = decls;
	     decls = d;
	     if(change)
	    	 func_mgr_set_class_func_decl(func_mgr_get(),decls,className);
	     else{//说明是变量
	         char *varName=IDENTIFIER_POINTER(DECL_NAME(decls));
	    	 if(isIface && strcmp(varName,IFACE_COMMON_STRUCT_VAR_NAME) && strcmp(varName,AET_MAGIC_NAME)){
                error_at(input_location,"接口中不能声明或定义变量。%qD",decls);
                return NULL_TREE;
	    	 }
	    	 var_mgr_add(var_mgr_get(),className,decls);
	     }
	    if(c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE))
	       all_prefix_attrs = chainon (c_c_parser_gnu_attributes (parser),prefix_attrs);
	    else
	       all_prefix_attrs = prefix_attrs;
	    if (c_parser_next_token_is (parser, CPP_COMMA))
	      c_parser_consume_token (parser);
	    else if (c_parser_next_token_is (parser, CPP_SEMICOLON) || c_parser_next_token_is (parser, CPP_CLOSE_BRACE)){
	       /* Semicolon consumed in caller.  */
	    	n_debug("新----struct内的声明 99 下一个 token 是CPP_SEMICOLON or CPP_CLOSE_BRACE 退出循环 count:%d", testcount);
	       break;
	    } else{
	      c_parser_error (parser, "expected %<,%>, %<;%> or %<}%>");
	      break;
	    }
	}else{
	  c_parser_error (parser,"expected %<:%>, %<,%>, %<;%>, %<}%> or %<__attribute__%>");
	  n_debug("新----struct内的声明 100 出错了 count:%d", testcount);
	  break;
	}
  }//end while
   n_debug("新----struct内的声明 101 over count:%d ", testcount);
  return decls;
}

static void parserExentdsAndImplements(ClassParser *self,ClassInfo *classInfo,char **parent,char **impls,int *implCount)
{
	  c_parser *parser=self->parser;
	  location_t start_loc = c_parser_peek_token (parser)->location;
	  int extends=0;
	  int implments=0;
      if(!c_parser_next_token_is (parser, CPP_KEYWORD))
    	  return;
      while(!c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
          switch (c_parser_peek_token (parser)->keyword){
    	     case RID_AET_EXTENDS:
    	     {
    	    	 if(extends>0){
    	        	 error_at(start_loc,"关键字Extends只能有一个。");
    	        	 return;
    	    	 }
    	    	 if(implments==1){
    	        	 error_at(start_loc,"关键字Implments应在Extends后。");
    	    	 }
    	         c_parser_consume_token (parser);
    	         if(!c_parser_next_token_is (parser, CPP_NAME)){
    	        	 error_at(start_loc,"关键字Extends后是类名。");
    	         }
    	         extends=1;
    	         tree id=c_parser_peek_token(parser)->value;
    	         c_parser_consume_token (parser);
    	         *parent=n_strdup(IDENTIFIER_POINTER(id));
    	         //分析是不是还有泛型
    	         if(c_parser_next_token_is (parser, CPP_LESS)){
    	        	GenericModel *model=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
    	            class_info_set_parent_generic_model(classInfo,model);
    	         }
    	     }
    	    	 break;
    	     case RID_AET_IMPLEMENTS:
    	     {
    	         c_parser_consume_token (parser);
    	         if(implments>0){
    	              error_at(start_loc,"关键字IMPLEMENTS只能有一个。");
    	              return;
    	         }
    	         implments=1;
    	         if(!c_parser_next_token_is (parser, CPP_NAME)){
    	             error_at(start_loc,"关键字Implements后是接口名。");
    	         }
    	         *implCount=class_interface_parser(self->classInterface,classInfo,impls);
    	     }
    	         return;
    	     default:
    	    	 error_at(start_loc,"Class Xxx Extends Yyy格式");
    	    	 return;
          }
      }
}

static nboolean checkTypeExists(char *id)
{
	tree checkTypeId=aet_utils_create_ident(id);
	tree re=lookup_name(checkTypeId);
	if(!re || re==error_mark_node){
		return FALSE;
	}
	return TRUE;
}

/**
 * 加parent
 * 加interface
 * Class Abc Extends XXX Implements YYY,ZZZ{
 * 变为
 * { XXX parentXXX1234;YYY ifaceYYY1234;ZZZ ifaceZZZ1234;
 */
static void addParentAndIfaceToToken(ClassParser *self,ClassName *parent,ClassName **ifaces,int count)
{
	  c_parser *parser=self->parser;
	  c_token *openBrace=c_parser_peek_token(parser); //"{"
	  int tokenCount=parser->tokens_avail;
	  c_token *move1=NULL;
	  c_token *move2=NULL;

	  if(tokenCount==2){//要移到第11位去
		  move1=&parser->tokens[1];
	  }
	  if(tokenCount==3){//要移到第11位去
		  move1=&parser->tokens[1];
		  move2=&parser->tokens[2];
	  }
	  int offset=1;
	  nboolean exists=checkTypeExists(parent->sysName);
	  if(!exists){
		  n_warning("不存在的类型 %s",parent->userName);
	      c_parser_error (parser, "不存在的类型。检查是否包含了类型所在的头文件。");
	      return;
	  }
	  aet_utils_create_token(&parser->tokens[offset],CPP_NAME,parent->sysName,(int)strlen(parent->sysName));
	  parser->tokens[offset].id_kind=C_ID_TYPENAME;//关键
	  offset++;
	  char *name=class_util_create_parent_class_var_name(parent->userName);
	  aet_utils_create_token(&parser->tokens[offset],CPP_NAME,name,(int)strlen(name));
	  n_free(name);
	  offset++;
	  aet_utils_create_token(&parser->tokens[offset],CPP_SEMICOLON,";",1);
	  offset++;
	  int i;
	  for(i=0;i<count;i++){
		  nboolean exists=checkTypeExists(ifaces[i]->sysName);
		  if(!exists){
			  n_warning("不存在的类型 %s",ifaces[i]->sysName);
			  c_parser_error (parser, "不存在的类型。检查是否包含了类型所在的头文件。");
			  return;
		  }
		  aet_utils_create_token(&parser->tokens[offset],CPP_NAME,ifaces[i]->sysName,(int)strlen(ifaces[i]->sysName));
		  parser->tokens[offset].id_kind=C_ID_TYPENAME;//关键
		  //检查是不存在该类型
		  offset++;
		  char name[255];
		  aet_utils_create_in_class_iface_var(ifaces[i]->userName,name);
		  aet_utils_create_token(&parser->tokens[offset],CPP_NAME,name,(int)strlen(name));
		  offset++;
		  aet_utils_create_token(&parser->tokens[offset],CPP_SEMICOLON,";",1);
		  offset++;
	  }
	  parser->tokens_avail=tokenCount+offset-1;
	  if(move1){
 	    aet_utils_copy_token(move1,&parser->tokens[offset]);
 	    offset++;
	  }
	  if(move2){
	  	 aet_utils_copy_token(move2,&parser->tokens[offset]);
	  	 offset++;
	  }
	  aet_print_token_in_parser("add parent and interface---- parent %s",parent->sysName);
}

static void addExentdsAndImplements(ClassParser *self,ClassInfo *classInfo)
{
	 c_parser *parser=self->parser;
	 location_t start_loc = c_parser_peek_token (parser)->location;
	 nboolean isIface=class_info_is_interface(classInfo);
     char *userClassName=classInfo->className.userName;
     char *sysClassName=classInfo->className.sysName;
     char *parent=NULL;
     char *interfaces[20];
     int ifaceCount=0;
     parserExentdsAndImplements(self,classInfo,&parent,interfaces,&ifaceCount);
     if(!strcmp(userClassName,AET_ROOT_OBJECT)){
    	 if(parent!=NULL || ifaceCount>0){
 	    	error_at(start_loc,"根类%qs不能继承或实现接口",userClassName);
    	 }
    	 return;
     }
     if(isIface){
    	if((parent!=NULL || ifaceCount>0))
	    	error_at(start_loc,"接口%qs不能继承或实现接口",userClassName);
     }else{
    	 if(parent==NULL){
    		 parent=n_strdup(AET_ROOT_OBJECT);
    	 }
    	 if(strcmp(userClassName,parent)==0){
 	    	error_at(start_loc,"类名与父类重名%qs",parent);
 	    	return;
    	 }
    	 ClassName *parentClass=class_mgr_get_class_name_by_user(class_mgr_get(),parent);
    	 if(parentClass==NULL){
        	 parentClass=class_mgr_get_class_name_by_sys(class_mgr_get(),parent);
        	 if(parentClass==NULL){
  	    	   error_at(start_loc,"没有引入父类所在的头文件%qs",parent);
  	    	   return;
        	 }
        	 //检查是不是final$
        	 ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parentClass);
        	 if(class_info_is_final(parentInfo)){
    	        error_at(start_loc,"类%qs不能继承父类%qs，因为父类被final$修饰。",userClassName,parentClass->userName);
    	    	return;
        	 }
    	 }
    	 ClassName *ifaces[20];
    	 int i;
    	 for(i=0;i<ifaceCount;i++){
        	 ClassName *ifaceClass=class_mgr_get_class_name_by_sys(class_mgr_get(),interfaces[i]);
         	 if(ifaceClass==NULL){
          	    error_at(start_loc,"没有引入接口所在的头文件%qs",interfaces[i]);
          	    return;
             }
         	ifaces[i]=ifaceClass;
    	 }
    	 addParentAndIfaceToToken(self,parentClass,ifaces,ifaceCount);
    	 class_info_set_parent(classInfo,parentClass);
    	 class_info_set_ifaces(classInfo,ifaces,ifaceCount);
     }
     if(parent)
    	 n_free(parent);
     int i;
     for(i=0;i<ifaceCount;i++)
    	 n_free(interfaces[i]);
}

static void testAddNewField(tree record)
{
	tree field;
	tree fieldList=TYPE_FIELDS(record);
	for (field = fieldList; field; field = DECL_CHAIN (field)){
	  tree name=DECL_NAME(field);
	  n_debug("class 的域成员:%s\n",IDENTIFIER_POINTER(name));
	}
}

static nboolean setFuncAbstractQual(ClassParser *self,tree decls,ClassName *className,ClassType classType,location_t loc)
{
	nboolean is=isFieldFunc(self,decls);
	if(!is){
	   if(self->state==CLASS_STATE_ABSTRACT){
		 error_at(loc,"在类:%qs，只有函数才能有abstract$修饰。",className->userName);
		 return FALSE;
	   }
	   return TRUE;
	}
	if(self->state==CLASS_STATE_ABSTRACT){
	   if(classType!=CLASS_TYPE_ABSTRACT_CLASS){
		  error_at(loc,"只有抽象类才能有抽象方法。当前类:%qs的类型是:%qs",
				  className->userName,classType==CLASS_TYPE_INTERFACE?"接口":"非抽象类");
		  return FALSE;
	   }
	   char *id=IDENTIFIER_POINTER(DECL_NAME(decls));
	   ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,id);
	   if(entity==NULL){
		   error_at(loc,"在类:%qs，找不到mangle函数名。",className->userName);
		   return FALSE;
	   }
	   if(entity->isCtor){
		   error_at(loc,"类%qs的构造函数不能有abstract$修饰。",className->userName);
		   return FALSE;
	   }
	   entity->isAbstract=TRUE;
    }
	return TRUE;
}

/**
 * 当类声明结束，跳转到静态变量或函数以及类初始化函数声明;
 */
static void gotoStaticAndInit(ClassParser *self,char *sysName)
{
	  c_parser *parser=self->parser;
	  c_token *semicolon = c_parser_peek_token (parser);//
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+3>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  int i;
	  for(i=tokenCount;i>1;i--){
		  aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+3]);
	  }
	  aet_utils_create_aet_goto_token(&parser->tokens[1],input_location);
	  aet_utils_create_number_token(&parser->tokens[2],GOTO_STATIC_VAR_FUNC);
	  aet_utils_create_string_token(&parser->tokens[3],sysName,strlen(sysName));
	  parser->tokens_avail=tokenCount+3;
	  aet_print_token_in_parser("gotoStaticAndInit ---- %s",sysName);
}


/**
 *解析类或接口声明，即 class Abc{
 */
struct c_typespec class_parser_parser_class_specifier (ClassParser *self)
{
  c_parser *parser=self->parser;
  enter_aet=1;//进入了aet
  parser_static_ready(parser_static_get());
  struct c_typespec ret;
  bool have_std_attrs;
  tree std_attrs = NULL_TREE;
  tree attrs;
  tree ident = NULL_TREE;
  location_t struct_loc;
  location_t ident_loc = UNKNOWN_LOCATION;
  enum tree_code code;
  code = RECORD_TYPE;
  ClassType classType=CLASS_TYPE_NORMAL;
  ClassParserState oldState=self->state;
  ClassInfo *classInfo=NULL;
  if (c_parser_next_token_is_keyword (parser, RID_AET_CLASS) && oldState!=CLASS_STATE_ABSTRACT){
	  classType=CLASS_TYPE_NORMAL;
  }else if(c_parser_next_token_is_keyword (parser, RID_AET_CLASS) && oldState==CLASS_STATE_ABSTRACT){
	  classType=CLASS_TYPE_ABSTRACT_CLASS;
  }else if(c_parser_next_token_is_keyword (parser, RID_AET_INTERFACE)){
	  if(oldState==CLASS_STATE_ABSTRACT){
		 error_at(c_parser_peek_token (parser)->location,"abstract$关键字只能修饰class$，不能修饰interface$。");
		 return ret;
	  }
	  classType=CLASS_TYPE_INTERFACE;
  }else{
      c_parser_error (parser, "类型不正确。只能是Class、AbstractClass、Interface");
      return ret;
  }
  self->state=CLASS_STATE_START;
  int keyword=c_parser_peek_token (parser)->keyword;
  struct_loc = c_parser_peek_token (parser)->location;
  c_parser_consume_token (parser);//消耗class 或Interface
  have_std_attrs = c_c_parser_nth_token_starts_std_attributes (parser, 1); //原设计 返回是0
  if (have_std_attrs)
    std_attrs = c_c_parser_std_attribute_specifier_sequence (parser);
  attrs = c_c_parser_gnu_attributes (parser);//设计 返回是NULL_TREE
  n_debug("新 ---分析struct 00  have_std_attrs:%d---attrs==NULL_TREE:%d ",have_std_attrs,attrs==NULL_TREE);
  /* Set the location in case we create a decl now.  */
  c_c_parser_set_source_position_from_token (c_parser_peek_token (parser));

  if (c_parser_next_token_is (parser, CPP_NAME)){
      ident = c_parser_peek_token (parser)->value;
      char sysClassName[255];//没有下划线的类名
      memcpy(sysClassName,IDENTIFIER_POINTER(ident)+1,IDENTIFIER_LENGTH(ident)-1);//去除下划线
      sysClassName[IDENTIFIER_LENGTH(ident)-1]='\0';
      ident_loc = c_parser_peek_token (parser)->location;
      struct_loc = ident_loc;
      c_parser_consume_token (parser);
      FieldDecorate dr= class_permission_get_decorate_by_class(self->classPermission,sysClassName,classType);
      n_debug("class_permission_stop 00 sysClassName:%s classType:%d isFinal:%d %d\n",sysClassName,classType,dr.isFinal,dr.permission);
	  class_mgr_set_type(class_mgr_get(),ident_loc,sysClassName,classType,dr.permission,dr.isFinal);
	  class_permission_stop(self->classPermission);
	  n_debug("新 ---分析struct 11 设类的类型和访问权限 sysClassName:%s classType:%d permission:%d", sysClassName,classType,dr.permission);
      //分析Extends Implements
       classInfo=class_mgr_get_class_info(class_mgr_get(),sysClassName);
       if(classInfo==NULL){
    	  c_parser_error (parser, "类型不正确。classInfo是空的");
    	  return ret;
       }
       self->currentClassName=&classInfo->className;
       if(c_parser_next_token_is (parser, CPP_LESS)){
      	  n_info("这是一个带泛型的 class %s classType:%d",classInfo->className.sysName,classInfo->type);
      	  GenericModel *model=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
      	  class_info_set_generic_model(classInfo,model);
       }
       addExentdsAndImplements(self,classInfo);
   }
   if(classInfo==NULL){
	  c_parser_error (parser, "类型不正确。classInfo是空的");
	  return ret;
   }
   if (c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
	  n_debug("新 --- 分析struct 22 下一个token是CPP_OPEN_BRACE have_std_attrs:%d--- ", have_std_attrs);
      /* Parse a struct or union definition.  Start the scope of the
	 tag before parsing components.  */
      class c_struct_parse_info *struct_info;
      tree type = start_struct (struct_loc, code, ident, &struct_info);
      tree postfix_attrs;
      /* We chain the components in reverse order, then put them in
	 forward order at the end.  Each struct-declaration may
	 declare multiple components (comma-separated), so we must use
	 chainon to join them, although when parsing each
	 struct-declaration we can use TREE_CHAIN directly.

	 The theory behind all this is that there will be more
	 semicolon separated fields than comma separated fields, and
	 so we'll be minimizing the number of node traversals required
	 by chainon.  */
     tree contents;
     timevar_push (TV_PARSE_STRUCT);
     contents = NULL_TREE;
     c_parser_consume_token (parser);//consume {
     aet_print_token(c_parser_peek_token (parser));
     //加一个魔数变量作为类或接口的第一个变量
     if(class_info_is_root(classInfo))
       parser_help_add_magic();
	 //如果是接口加入一个变量和两个方法
     if(classType==CLASS_TYPE_INTERFACE){
         n_debug("是接口声明:%s\n",classInfo->className.sysName);
        class_interface_add_var_ref_unref_method(self->classInterface);
     }

      /* Parse the struct-declarations and semicolons.  Problems with
	 semicolons are diagnosed here; empty structures are diagnosed
	 elsewhere.  */
      self->state=CLASS_STATE_FIELD;//CLASS的状态是进入field
      while (true){
	      tree decls;
	      /* Parse any stray semicolon.  */
	      if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
		      n_debug("新 ---分析struct 33 下一个token是CPP_SEMICOLON  have_std_attrs:%d--- ",have_std_attrs);
	          location_t semicolon_loc= c_parser_peek_token (parser)->location;
	          gcc_rich_location richloc (semicolon_loc);
	          richloc.add_fixit_remove ();
	          pedwarn (&richloc, OPT_Wpedantic,"extra semicolon in struct or union specified");
	          c_parser_consume_token (parser);
	          continue;
	      }
	      /* Stop if at the end of the struct or union contents.  */
	      if (c_parser_next_token_is (parser, CPP_CLOSE_BRACE)){
			  n_debug("新 ---分析struct 44 下一个token是CPP_CLOSE_BRACE break ");
			  c_parser_consume_token (parser);
			  break;
	      }

	  /* Parse some comma-separated declarations, but not the
	     trailing semicolon if any.  */
		 n_debug("新 ---分析struct 55 应该在这里mangle 如果结构体内没内容不会进这里，下一个token是什么? --- global_bindings_p() %d ",global_bindings_p());
		 location_t loc=c_parser_peek_token (parser)->location;
		 int isStatic=0;
	     decls = c_parser_class_declaration(self,classInfo,type,classType,&isStatic);
	     if(decls==NULL_TREE){//如果是一个枚举，并且没有声明变量，就会进这里
	   	     class_permission_stop(self->classPermission);
	    	 continue;
	     }
	     n_debug("新 ---分析struct 55xx");
	     if(isStatic==0){
	        contents = chainon (decls, contents);
	        nboolean result=setFuncAbstractQual(self,decls,&classInfo->className,classType,loc);
	  	   // printf("class_permission_stop 11 \n",(&classInfo->className)->sysName);
	        class_permission_set_field_decorate(self->classPermission,decls,&classInfo->className);
	     	class_permission_stop(self->classPermission);
	        self->state=CLASS_STATE_FIELD;
	        if(!result)
	    	   break;
	     }else{
		    n_info("新 ---分析struct ---55 在类:的静态成员\n");
		    continue;
	     }

	  /* If no semicolon follows, either we have a parse error or
	     are at the end of the struct or union and should
	     pedwarn.  */
	     if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
		      n_debug("新 ---分析struct 66 下一个token是CPP_SEMICOLON code:%d RECORD_TYPE:%d have_std_attrs:%d--- ",
		 		  		 	 			  code,RECORD_TYPE,have_std_attrs);
	         c_parser_consume_token (parser);
	     }else{
	        if (c_parser_next_token_is (parser, CPP_CLOSE_BRACE))
		       pedwarn (c_parser_peek_token (parser)->location, 0, "no semicolon at end of struct or union");
	        else if (parser->error || !c_parser_next_token_starts_declspecs (parser)){
		        c_parser_error (parser, "expected %<;%>");
		        c_parser_skip_until_found (parser, CPP_CLOSE_BRACE, NULL);
		        break;
		    }

	      /* If we come here, we have already emitted an error
		 for an expected `;', identifier or `(', and we also
	         recovered already.  Go on with the next field. */
	     }
	  }//end while
      if(!class_info_is_interface(classInfo)){
    	  n_debug("新 ---分析struct xx 不是接口，检查有没有缺省的构造函数 className:%s\n",classInfo->className.sysName);
    	  if(!class_ctor_have_default_field(self->classCtor,&classInfo->className)){
        	  n_info("新 ---分析struct 在class中加field函数 00 不是接口，没有构造函数 加constructor className:%s",classInfo->className.sysName);
        	  tree decls= class_ctor_create_default_decl(self->classCtor,&classInfo->className,type);
     	      contents = chainon (decls, contents);
    	  }
    	  if(!class_finalize_have_field(self->classFinalize,&classInfo->className)){
    		  n_info("新 ---分析struct 在class中加field函数 11--- 不是接口，没有释放函数 加finalize className:%s",classInfo->className.sysName);
			  tree decls= class_finalize_create_finalize_decl(self->classFinalize,&classInfo->className,type);
			  contents = chainon (decls, contents);
          }
    	  n_info("新 ---分析struct 在class中加field函数 22--- 加unref className:%s",classInfo->className.sysName);
    	  tree decls= class_finalize_create_unref_decl(self->classFinalize,&classInfo->className,type);
          contents = chainon (decls, contents);
      }

	  int genericCount=class_info_get_generic_count(classInfo);
	  if((genericCount>0 || func_mgr_have_generic_func(func_mgr_get(),&classInfo->className))&& !class_info_is_interface(classInfo)){
		   n_info("加一个数组到类中，专门存放泛型的sizeof");
		   tree  genericArray=generic_impl_create_generic_info_array_field(generic_impl_get(),&classInfo->className, genericCount);
	       contents = chainon (genericArray, contents);
	       var_mgr_add(var_mgr_get(),&classInfo->className,genericArray);

		   tree blockFuncs=generic_impl_create_generic_block_array_field(generic_impl_get());//void *blockFuncs[30];
		   aet_print_tree(blockFuncs);
	       contents = chainon (blockFuncs, contents);
	       var_mgr_add(var_mgr_get(),&classInfo->className,blockFuncs);
	  }

	  if(class_mgr_about_generic(class_mgr_get(),&classInfo->className)){
		  int isStatic=0;
		  generic_impl_add_about_generic_field_to_class(generic_impl_get());
	      tree decls = c_parser_class_declaration(self,classInfo,type,classType,&isStatic);
	      contents = chainon (decls, contents);
		  class_permission_stop(self->classPermission);

	      generic_impl_add_about_generic_block_info_list(generic_impl_get());
	      decls = c_parser_class_declaration(self,classInfo,type,classType,&isStatic);
	      contents = chainon (decls, contents);
		  class_permission_stop(self->classPermission);
	  }

      postfix_attrs = c_c_parser_gnu_attributes (parser);
      ret.spec = finish_struct(struct_loc, type, nreverse (contents),chainon (std_attrs,chainon (attrs, postfix_attrs)),struct_info);
      n_debug("新 ---分析struct 77 完成finish_struct %s %p",IDENTIFIER_POINTER(TYPE_NAME (ret.spec)),ret.spec);
      class_mgr_set_record(class_mgr_get(),&classInfo->className,ret.spec);
      testAddNewField(ret.spec);
      ret.kind = ctsk_tagdef;
      ret.expr = NULL_TREE;
      ret.expr_const_operands = true;
      timevar_pop (TV_PARSE_STRUCT);

      if (!c_parser_next_token_is (parser, CPP_SEMICOLON)){
          c_parser_error (parser, "类声明以分号结束");
      }else{
    	  /**
    	   * 加入初始化函数声明 Abc *Abc_AET_INIT_GLOBAL_METHOD_STRING_Abc(Abc *self);
    	   * c_parser_peek_token (parser)必须是;号。否则结构体声明不能结束
    	   */
      	  gotoStaticAndInit(self,classInfo->className.sysName);
      }
      class_build_replace_getclass_rtn(self->classBuild,self->currentClassName);
      self->state=CLASS_STATE_STOP;
      self->currentClassName=NULL;
      return ret;
    } else if (!ident) {
      c_parser_error (parser, "expected %<{%>");
      ret.spec = error_mark_node;
      ret.kind = ctsk_tagref;
      ret.expr = NULL_TREE;
      ret.expr_const_operands = true;
      self->state=CLASS_STATE_STOP;
      self->currentClassName=NULL;
      return ret;
    }//end c_parser_next_token_is (parser, CPP_OPEN_BRACE)
  /* Attributes may only appear when the members are defined or in
     certain forward declarations.  */
   if (have_std_attrs && c_parser_next_token_is_not (parser, CPP_SEMICOLON))
      c_parser_error (parser, "expected %<;%>");
  /* ??? Existing practice is that GNU attributes are ignored after
     the struct or union keyword when not defining the members.  */
   n_debug("新 ---分析struct over %s--- ",IDENTIFIER_POINTER (ident));
   ret = parser_xref_tag (ident_loc, code, ident, have_std_attrs, std_attrs);
   self->state=CLASS_STATE_STOP;
   self->currentClassName=NULL;
   return ret;
}

static char *createClassName(ClassParser *self,char *orgiName,char **package)
{
     cpp_buffer *buffer= parse_in->buffer;
     struct _cpp_file *file=buffer->file;
     const char *fileName=_cpp_get_file_name (file);
     cpp_dir *dir=cpp_get_dir (file);
     char *classPrefix=class_package_get_class_prefix(self->classPackage,fileName,dir->name);
     char *className=NULL;
     if(classPrefix!=NULL){
    	 className=n_strdup_printf("%s_%s",classPrefix,orgiName);
         *package=n_strdup(classPrefix);
     }else
    	 className=n_strdup(orgiName);
     return className;
}

/**
 * 把class$ Abc 或 interface$ Abc 或 abstract$ class$ Abc
 * 替换为 typedef sruct _Abc Abc; Class _Abc
 * Abc是类型名是一个IDENTIFIER tree 全局只有一个
 * 所以lookup_name 用的是第一个从c-lex.c 中创建的ident
 * 参见c-parser.c 中的c_lex_one_token decl = lookup_name (token->value);
 * 新加的token要加位置，否则start_decl pushdecl set_underlying_type 不会复制一个新的record
 * 当用abc->时会
 */
void class_parser_replace_class_to_typedef(ClassParser *self)
{
	  c_parser *parser=self->parser;
	  c_token *classSpec = c_parser_peek_token (parser);//"Class"
	  classSpec->id_kind=C_ID_CLASSNAME;/*这是一个指示看	c-parser.c中的case RID_AET_CLASS:*/
	  c_token *classNameToken=c_parser_peek_2nd_token(parser); //Abc
	  location_t orgLoc=classSpec->location;
	  if(classNameToken->type==CPP_KEYWORD){
		  error_at(orgLoc,"class$或interface$后不能是关键字。");
		  return;
	  }
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+5>AET_MAX_TOKEN){
		 error("token太多了");
		 return ;
	  }
	  /* 加一个下划线 把className改为“_”+className*/
	  char *package=NULL;
	  char *userClassName=n_strdup(IDENTIFIER_POINTER(classNameToken->value));
	  char *sysClassName=createClassName(self,userClassName,&package);
	  char buff[255];
	  sprintf(buff,"_%s",sysClassName);
	  int i;
	  for(i=tokenCount-2;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+5]);
	  }
	  aet_utils_create_token(&parser->tokens[6],CPP_NAME,buff,strlen(buff));
	  aet_utils_copy_token(classSpec,&parser->tokens[5]);
	  aet_utils_create_token(&parser->tokens[4],CPP_SEMICOLON,(char *)";",1);
	  aet_utils_copy_token(classNameToken,&parser->tokens[3]);
	  //把classNameToken拷贝后再改value 如果直接aet_utils_create_token(&parser->tokens[3],CPP_NAME,sysClassName,strlen(sysClassName));
	  //缺少token的其它信息导致找到class
	  tree sysValue=aet_utils_create_ident(sysClassName);
	  parser->tokens[3].value=sysValue;
	  aet_utils_create_token(&parser->tokens[2],CPP_NAME,buff,strlen(buff));
	  aet_utils_create_struct_token(&parser->tokens[1],orgLoc);
	  aet_utils_create_typedef_token(&parser->tokens[0],orgLoc);
	  parser->tokens_avail=tokenCount+5;
	  aet_print_token_in_parser("class_parser_replace ---- %s  %s package:%s sysClassName:%s",userClassName,buff,package,sysClassName);
	  class_mgr_add(class_mgr_get(),sysClassName,userClassName,package);
	  n_free(userClassName);
	  n_free(sysClassName);
	  if(package)
		  n_free(package);
}

void class_parser_abstract_keyword(ClassParser *self)
{
  c_parser *parser=self->parser;
  location_t  loc = c_parser_peek_token (parser)->location;
  if(parser->isAet){
	 error_at(loc,"在类实现中不能出现关键字abstract$。");
	 return;
  }
  if(self->state!=CLASS_STATE_STOP && self->state!=CLASS_STATE_FIELD){
	error_at(loc,"abstract$关键字只能在class$之前或函数声明之前");
	return;
  }
  self->state=CLASS_STATE_ABSTRACT;
}



/**
 * 分析final$
 */
void   class_parser_final(ClassParser *self,struct c_declspecs *specs)
{
  // printf("分析final$ ----\n");
   c_parser *parser=self->parser;
   class_final_parser (self->classFinal,self->state,specs);
}

/**
 * 解析关键字public$ final$ static 等
 * 然后设状态为运行,等分析完class或interface后，再通过调用stop把运行状态变为FALSE
 */
void   class_parser_decorate(ClassParser *self){
	FieldDecorate  dr= class_permission_try_parser(self->classPermission,-1);
	n_debug("class_permission_set_decorate 00----%s fileName:%s state:%d permission:%d\n",
			self->currentClassName?self->currentClassName->sysName:"null",self->fileName,self->state,dr.permission);
	class_permission_set_decorate(self->classPermission,&dr);
}


nboolean  class_parser_is_parsering(ClassParser *self)
{
	return self->state!=CLASS_STATE_STOP;
}

/**
 * 解析关键字 RID_AET_GOTO
 */
nboolean  class_parser_goto(ClassParser *self,nboolean start_attr_ok,int *action)
{
  c_parser *parser=self->parser;
  c_token *tok = c_parser_peek_2nd_token (parser);
  if(tok->type!=CPP_NUMBER){
	  error_at(input_location,"在class_parser_goto发生不可知的错误！");
	  n_error("报告此错误！");
	  return FALSE;
  }
  tree value= tok->value;
  wide_int result=wi::to_wide(value);
  AetGotoTag pos=result.to_shwi();
  if(pos==GOTO_STATIC_VAR_FUNC){
	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
	  c_parser_consume_token (parser);//consume   GOTO_STATIC_VAR_FUNC
	  c_token *sysNameToken = c_parser_peek_token (parser);
	  tree string=sysNameToken->value;
	  const char *tokenString  = TREE_STRING_POINTER (string);
	  //去除一头一尾的"号
	  char *sysName=tokenString;//n_strndup(tokenString+1,strlen(tokenString)-1);
	  c_parser_consume_token (parser);//consume   "test_AObject"
	  ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	  n_debug("class_parser_goto GOTO_STATIC_VAR_FUNC 编译类声明中的静态部份:%s %p",sysName,info);
	  parser_static_compile(parser_static_get(),sysName);
	  class_init_create_init_decl(self->classInit,&info->className);
	  return FALSE;
  }else if(pos==GOTO_STATIC_CONTINUE_COMPILE){
	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
	  c_parser_consume_token (parser);//consume   GOTO_STATIC_CONTINUE_COMPILE
	  n_debug("class_parser_goto GOTO_STATIC_CONTINUE_COMPILE 继续编译静态部份");
	  parser_static_continue_compile(parser_static_get());
	  if(action!=NULL)
	    *action=GOTO_STATIC_CONTINUE_COMPILE;
	  return  FALSE;
  }else if(pos==GOTO_CHECK_FUNC_DEFINE){
	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
	  c_parser_consume_token (parser);//consume   GOTO_CHECK_FUNC_DEFINE
	  middle_file_import_lib(middle_file_get());//为第二次编译和类方法检查生成库连结
	  func_check_check_all_define(self->funcCheck);
	  iface_file_compile_ready(iface_file_get());
	  middle_file_create_global_var(middle_file_get());
	  return FALSE;
  }else if(pos==GOTO_IFACE_COMPILE){
	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
	  c_parser_consume_token (parser);//consume   GOTO_IFACE_COMPILE
	  c_token *sysNameToken = c_parser_peek_token (parser);
	  tree string=sysNameToken->value;
	  const char *tokenString  = TREE_STRING_POINTER (string);
	  char *sysNames=tokenString;
	  c_parser_consume_token (parser);//consume   "test_Iface1,test_Iface2"
	  iface_file_compile(iface_file_get(),sysNames);
	  return FALSE;
  }else{
	  return block_mgr_parser_goto(block_mgr_get(),start_attr_ok,pos);
  }
  return FALSE;
}

void  class_parser_is_static_continue_compile_and_goto(ClassParser *self)
{
   c_parser *parser=self->parser;
   if (c_parser_next_token_is_keyword (parser, RID_AET_GOTO)){
	   c_token *tok = c_parser_peek_2nd_token (parser);
	   if(tok->type!=CPP_NUMBER){
		   return ;
	   }
	   tree value= tok->value;
	   wide_int result=wi::to_wide(value);
	   AetGotoTag pos=result.to_shwi();
	   if(pos==GOTO_STATIC_CONTINUE_COMPILE){
		   class_parser_goto(self,FALSE,NULL);
	   }
   }
}

/**
 * 分析AClass *getClass() 这时，还没有AClass 把AClass 替换成 void *
 */
nboolean   class_parser_exception(ClassParser *self,tree value)
{
   //printf("分析class_parser_exception 00 %s\n",IDENTIFIER_POINTER(value));
   c_parser *parser=self->parser;
   if(strcmp(IDENTIFIER_POINTER(value),"AClass")!=0){
	   return FALSE;
   }
   if(self->state!=CLASS_STATE_FIELD || self->currentClassName==NULL){
	   printf("分析class_parser_exception 00eeee %s %p\n",IDENTIFIER_POINTER(value),self->currentClassName);
	   return FALSE;
   }
   //printf("分析class_parser_exception 11 %s\n",self->currentClassName->userName);
   if(strcmp(self->currentClassName->userName,"AObject")!=0){
	   return FALSE;
   }
   c_token *t=c_parser_peek_token (parser);
   aet_print_token(t);
   if(t->type!=CPP_MULT)
	   return FALSE;
   //printf("分析class_parser_exception 22 %s\n",self->currentClassName->userName);

   t=c_parser_peek_2nd_token (parser);
   aet_print_token(t);
   if(t->type!=CPP_NAME)
	   return FALSE;
   char *name=IDENTIFIER_POINTER(t->value);
   if(strcmp(name,"getClass"))
       return FALSE;
   //printf("分析class_parser_exception 33 %s\n",self->currentClassName->userName);
   int tokenCount=parser->tokens_avail;
   location_t  loc = t->location;
   int i;
   for(i=tokenCount;i>0;i--){
		 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
   }
   aet_utils_create_void_token(&parser->tokens[0],loc);
   parser->tokens_avail=tokenCount+1;
   return TRUE;
}

void  class_parser_parser_enum_dot(ClassParser *self,struct c_typespec *ret)
{
	tree value=enum_parser_parser_dot(enum_parser_get(),ret);
	if(value!=NULL_TREE){
		ret->spec=value;
	}
}

/**
 * 解析在文件或类中的枚举定义
 */
struct c_typespec class_parser_enum(ClassParser *self,location_t loc)
{
	//printf("class_parser_enum---- \n");
	c_parser *parser=self->parser;
	struct c_typespec ret;
	ClassName *current=NULL;
	if(class_parser_is_parsering(self)){
		//printf("在classparser状态----\n");
		current=self->currentClassName;
	}
	c_token *t=c_parser_peek_2nd_token(parser);
	if(t->type!=CPP_NAME)
		return ret;
	t=c_parser_peek_nth_token(parser,3);
	if(t->type!=CPP_OPEN_BRACE)
		return ret;
	ret=enum_parser_parser(enum_parser_get(),loc,current);
	return ret;
}

/**
 * 完成在文件中的枚举定义
 * 在c_parser_class_declaration方法中完成的是类中的枚举定义。
 */
void class_parser_over_enum(ClassParser *self,struct c_declspecs *specs)
{
    if(TREE_CODE(specs->type)==ENUMERAL_TYPE){
         if(specs->typespec_kind==ctsk_tagdef){
        	 if(!aet_utils_valid_tree(specs->expr) || TREE_CODE(specs->expr)!=IDENTIFIER_NODE)
        		 return;
        	 char *tag=IDENTIFIER_POINTER(specs->expr);
        	 if(strcmp(tag,"enum$"))
        		 return;
        	 specs->expr=NULL_TREE;
            // printf("处理enumberaa--class_parser_over_enum-11 \n");
             ClassPermissionType permission=CLASS_PERMISSION_PUBLIC;
             location_t loc=0;
			 if(self->classPermission->running){
				 permission=self->classPermission->currentDecorate.permission;
				 loc=self->classPermission->currentDecorate.loc;
	             class_permission_stop(self->classPermission);
             }
			 if(permission!=CLASS_PERMISSION_PUBLIC){
				 if(loc==0){
					 EnumData *item=enum_parser_get_by_enum_name(enum_parser_get(),"",IDENTIFIER_POINTER(TYPE_NAME(specs->type)));
					 if(item==NULL){
						n_error("找不到枚举，不应该出错的错误。");
					 }
					 loc=item->loc;
				 }
				 error_at(loc,"文件中定义的枚举只能用public$修饰。");
				 return;
			 }
             enum_parser_create_decl(enum_parser_get(),input_location,NULL,specs,permission);
        }
    }
}


void  class_parser_set_parser(ClassParser *self,c_parser *parser)
{
	 self->parser=parser;
	 self->classCtor->parser=parser;
	 self->classInterface->parser=parser;
	 self->classInit->parser=parser;
	 self->classFinalize->parser=parser;
	 self->classFinal->parser=parser;
	 self->classPackage->parser=parser;
	 self->classPermission->parser=parser;
	 self->classBuild->parser=parser;
	 self->funcCheck->parser=parser;
	 enum_parser_set_parser(enum_parser_get(),parser);
	 parser->isAet=FALSE;
	 parser->isGenericState=FALSE;
	 parser->isTestGenericBlockState=FALSE;
	 parser_static_set_parser(parser_static_get(),parser,self->classPermission);
	 var_mgr_set_parser(var_mgr_get(),parser);
	 generic_impl_set_parser(generic_impl_get(),parser);
	 block_mgr_set_parser(block_mgr_get(),parser);
	 generic_expand_set_parser(generic_expand_get(),parser);
	 new_object_set_parser(new_object_get(),parser);
}


ClassParser *class_parser_get()
{
	static ClassParser *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(ClassParser));
		 classParserInit(singleton);
	}
	return singleton;
}


