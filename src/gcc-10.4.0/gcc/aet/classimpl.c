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
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "tree-iterator.h"
#include "opts.h"

#include "c-aet.h"
#include "aetutils.h"
#include "classmgr.h"
#include "classimpl.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "parserstatic.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "aet-c-parser-header.h"
#include "newobject.h"
#include "funcmgr.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericfile.h"
#include "genericquery.h"
#include "classaccess.h"
#include "genericcall.h"
#include "parserhelp.h"
#include "classinit.h"
#include "makefileparm.h"
#include "ifacefile.h"
#include "objectreturn.h"
#include "enumparser.h"
#include "namelesscall.h"
#include "accesscontrols.h"
#include "selectfield.h"

/**
 * class的实现样式如下：
 * RealizeClass Abc{
 *
 *
 * };
 * 思路：把“RealizeClass Abc{”当成是一个假的 struct。 c_scope，c语言只有三种scope
 * 1.全局的extern_scope 2.文件 file_scope 3.语句块 所以" RealizeClass Abc{"
 * 内的变量、函数都认为在一个文件里"RealizeClass Abc{"可以指示那些函数可以mangle,或加上一些内容
 * int a;要变成static int a,说明只能在.c文件使用否则报 multiple definition of `a';错误。
 *  RealizeClass Abc{
 *    int a;-> int a;
 *    int add();  -->static int _Z3Abc3addEz(Abc *self);
 *    int getName(){  ---> static int _Z3Abc7getNameEz(Abc *self)
 *      return 0;
 *    }
 *    Abc(){   --> Abc* _Z3Abc3Abc_AbcE(Abc *self){return self}
 *    }
 * };
 * c 从 c_parse_file
 *         c_parser_translation_unit 里实现循环接行c_parser_external_declaration
 *              c_parser_external_declaration CPP_KEYWORD(RID_EXTENSION RID_ASM) CPP_SEMICOLON CPP_PRAGMA 其它进入c_parser_declaration_or_fndef处理
 *                           c_parser_declaration_or_fndef
 */

/**
 * implimpl Abc{
 *    int getName(int a){
 *       return 10;
 *    }
 *    int getName(float a){
 *      return getName(10)
 *    }
 * };
 * getName的调用见 class_impl_change_expression class_impl_parser_func_expression
 * c_parser_postfix_expression 该方法调用class_impl_change_expression class_impl_parser_func_expression
 *在implimp中的函数都加上了Abc *self 所以getName前不用加self->getName 因为 getName(Abc *self 如果不在impl内必须有调用对象
 * 主要问题是如何选择函数，根据参数来选
 */

static void classImplInit(ClassImpl *self)
{
	self->className=NULL;
	self->nest=0;//进函数定义c_parser_compound_statement_nostart 如果有 {..,,} 会嵌套调用 保证当遇到}时是最外层的
	self->varCall=var_call_new();
	self->classCtor=class_ctor_new();
	self->superCall=super_call_new();
	self->funcCall=func_call_new();
	self->classInterface=class_interface_new();
	self->classRef=class_ref_new(self->classInterface);
	self->classFinalize=class_finalize_new();
	self->classDot=class_dot_new(self->classInterface);
	self->classBuild=class_build_new();
	self->funcCheck=func_check_new();
	self->classInit=class_init_new();
	self->classCast=class_cast_new();
	self->superDefine=super_define_new();
	self->implicitlyCall=implicitly_call_new();
	self->cmpRefOpt=cmp_ref_opt_new();
    self->aetExpr=aet_expr_new();
    self->objectMacro.count=0;
}

static nboolean checkDefaultConstructorDefine(ClassImpl *self)
{
	 ClassName *className=self->className;
     if(!class_mgr_is_interface(class_mgr_get(),className)){
   	    n_debug("checkDefaultConstructorDefine，检查有没有缺省的构造函数定义 className:%s",className->sysName);
   	    if(!class_ctor_have_default_define(self->classCtor,className)){
       	    n_info("checkDefaultConstructorDefine 不是接口，没有构造函数定义 加一个 className:%s",className->sysName);
       	    return FALSE;
   	    }
     }
     return TRUE;
}

static nboolean checkFinalizeDefine(ClassImpl *self)
{
	 ClassName *className=self->className;
     if(!class_mgr_is_interface(class_mgr_get(),className)){
   	    n_debug("checkFinalizeDefine，检查有释放函数定义 className:%s ",className->sysName);
   	    if(!class_finalize_have_define(self->classFinalize,className)){
   	    	n_debug("checkFinalizeDefine 不是接口，没有释放函数定义 加一个 className:%s ",className->sysName);
       	    return FALSE;
   	    }
     }
     return TRUE;
}


static void class_impl_external_declaration (ClassImpl *self)
{
  c_parser *parser=self->parser;
  int ext;
  switch (c_parser_peek_token (parser)->type){
    case CPP_KEYWORD:
      switch (c_parser_peek_token (parser)->keyword){
	     case RID_EXTENSION:
	        ext = c_disable_extension_diagnostics ();
	        c_parser_consume_token (parser);
	        class_impl_external_declaration (self);
	        c_restore_extension_diagnostics (ext);
	        break;
	     case RID_ASM:
	       c_c_parser_asm_definition (parser);
	       break;
	 	case RID_AET_REALIZE_CLASS:
	 	{
	 		// printf("RID_AET_REALIZE_CLASS 说明符语法开始00：keyword:%d %s %s %d\n",c_parser_peek_token (parser)->keyword,__FILE__,__FUNCTION__,__LINE__);
	         location_t here = c_parser_peek_token (parser)->location;
	         error_at (here, "unknown type name RID_AET_REALIZE_CLASS");
	 	}
	 		break;
	    default:
		  n_debug("跳出处理关键词--- %s, %s, %d", __FILE__, __FUNCTION__, __LINE__);
		  aet_print_token(c_parser_peek_token (parser));
	      goto decl_or_fndef;
	   }
        break;
    case CPP_SEMICOLON:
      {
    	 n_debug("处理分号 --- %s, %s, %d", __FILE__, __FUNCTION__, __LINE__);
         if(self->readyEnd==1)
    	    self->readyEnd=2;
         c_parser_consume_token (parser);
         if(!class_mgr_is_interface(class_mgr_get(),self->className)){
             n_debug("class-impl 在class中加函数定义 00 加unref --- className:%s readyEnd:%d %s, %s, %d",
             		self->className->sysName,self->readyEnd,__FILE__, __FUNCTION__, __LINE__);
            nboolean result=class_finalize_build_unref_define(self->classFinalize,self->className);
            if(result)
	          goto decl_or_fndef;
         }
      }
      break;
    case CPP_PRAGMA:
      mark_valid_location_for_stdc_pragma (true);
      c_c_parser_pragma (parser, 0, NULL);//pragma_external=0
      mark_valid_location_for_stdc_pragma (false);
      break;
    case CPP_CLOSE_BRACE:
    {
    	n_debug("处理CPP_CLOSE_BRACE 并检查有没有构造函数和释放函数---self->readyEnd:%d %s", self->readyEnd,self->className->sysName);
        if(checkDefaultConstructorDefine(self) && checkFinalizeDefine(self)){
            n_info("处理CPP_CLOSE_BRACE 00 并检查有没有构造函数和释放函数---self->readyEnd:%d %s\n", self->readyEnd,self->className->sysName);
           self->readyEnd=1;
           c_parser_consume_token (parser);
           if(c_parser_peek_token (parser)->type!=CPP_SEMICOLON)
             c_parser_error (parser, "expected 缺少冒号  ");
        }else if(!checkFinalizeDefine(self)){
        	n_info("class-impl 在class中加函数定义 00 加finalize --- className:%s readyEnd:%d",self->className->sysName,self->readyEnd);
            class_finalize_build_finalize_define(self->classFinalize,self->className);
   	        goto decl_or_fndef;
        }else{
        	n_info("class-impl 在class中加函数定义 11 加constructor --- className:%s readyEnd:%d",self->className->sysName, self->readyEnd);
            class_ctor_build_default_define(self->classCtor,self->className);
  	       goto decl_or_fndef;
        }
    }
      break;
    case CPP_PLUS:
    case CPP_MINUS:
      /* Else fall through, and yield a syntax error trying to parse
	 as a declaration or function definition.  */
      /* FALLTHRU */
    default:

        decl_or_fndef:
      /* A declaration or a function definition (or, in Objective-C,
	 an @interface or @protocol with prefix attributes).  We can
	 only tell which after parsing the declaration specifiers, if
	 any, and the first declarator.  */
		 // printf("处理关键词 decl_or_fndef:00 --- %s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);
          c_c_parser_declaration_or_fndef (parser, true, true, true, false, true,NULL, vNULL);
		  //printf("处理关键词 decl_or_fndef:11 --- %s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);
         break;
    }
}

static void class_impl_translation_unit (ClassImpl *self)
{
	  c_parser *parser=self->parser;
      int testcount=0;
      do{
	    //printf("classimpl 外部声明  00 --- count:%d %s, %s, %d\n", testcount,__FILE__, __FUNCTION__, __LINE__);
	    class_impl_external_declaration (self);
	    if(self->readyEnd==2){
	    	break;
	    }
	    //printf("class 外部声明  11 --- count:%d %s, %s, %d\n", testcount,__FILE__, __FUNCTION__, __LINE__);
	    testcount++;
	  }while (c_parser_next_token_is_not (parser, CPP_EOF));
	  //printf("外部声明结束 --- count:%d end:%d %s, %s, %d\n", testcount++,self->readyEnd,__FILE__, __FUNCTION__, __LINE__);
}

static void updateClassName(ClassImpl *self,char *sysClassName)
{

    if(self->className!=NULL){
    	class_name_free(self->className);
    	self->className=NULL;
    }
	if(sysClassName!=NULL)
      self->className=class_mgr_clone_class_name(class_mgr_get(),sysClassName);
}

static void freeIfaceCode_cb(IFaceSourceCode *item)
{
	if(item->define!=NULL){
		n_free(item->define);
	}
	if(item->modify!=NULL){
		n_free(item->modify);
	}
	n_slice_free(IFaceSourceCode,item);
}

static char *strCat(char *one,char *two)
{
	NString *codes=n_string_new("");
	if(one!=NULL){
	   n_string_append(codes,one);
	   n_string_append(codes,"\n");
	}
	if(two!=NULL){
	   n_string_append(codes,two);
	}
	char *result=NULL;
	if(codes->len>0)
		result=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	return result;
}

/**
 * 解析助记符
 * impl$ XXX extends$ Parent
 * extends$ Parent仅仅起到助记作用，应为在.h文件中已经声明了。
 */
static void  getInterface(ClassImpl *self,NPtrArray *ifaceArray)
{
      c_parser *parser=self->parser;
      int i;
      for(i=0;i<20;i++){
          if (c_parser_next_token_is (parser, CPP_NAME)){
              tree id=c_parser_peek_token(parser)->value;
              n_ptr_array_add(ifaceArray,id);
              c_parser_consume_token (parser);
          }else if(c_parser_next_token_is (parser, CPP_COMMA)){
              c_parser_consume_token (parser);
              if (c_parser_next_token_is (parser, CPP_NAME)){
                  tree id=c_parser_peek_token(parser)->value;
                  c_parser_consume_token (parser);
                  n_ptr_array_add(ifaceArray,id);
              }
          }else if (c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
              break;
          }else{
              c_parser_consume_token (parser);
          }
      }
}

/**
 * 解析助记符
 * impl$ XXX extends$ Parent
 * extends$ Parent仅仅起到助记作用，应为在.h文件中已经声明了。
 */
static void parserExentdsAndImplements(ClassImpl *self,char *sysName)
{
      c_parser *parser=self->parser;
      NPtrArray *parentArray=n_ptr_array_new();
      NPtrArray *ifaceArray=n_ptr_array_new();
      location_t loc=c_parser_peek_token (parser)->location;
      while(!c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
          switch (c_parser_peek_token (parser)->keyword){
             case RID_AET_EXTENDS:
             {
                 c_parser_consume_token (parser);
                 if(c_parser_next_token_is (parser, CPP_NAME)){
                     tree id=c_parser_peek_token(parser)->value;
                     c_parser_consume_token (parser);
                     n_ptr_array_add(parentArray,id);
                 }
             }
                 break;
             case RID_AET_IMPLEMENTS:
             {
                 c_parser_consume_token (parser);
                 if(c_parser_next_token_is (parser, CPP_NAME)){
                     getInterface(self,ifaceArray);
                 }
             }
                 break;
             default:
                 c_parser_consume_token (parser);
                 break;
          }
          if(!c_parser_next_token_is (parser, CPP_OPEN_BRACE))
              c_parser_consume_token (parser);
      }
      ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
      nboolean re= class_info_decl_equal_impl(info,parentArray,ifaceArray);
      if(!re){
          warning_at(loc,0,"类实现的助记符与类声明不符。");
      }
}


void class_impl_parser(ClassImpl *self)
{
    c_parser *parser=self->parser;
    tree ident = NULL_TREE;
    location_t impl_loc;
    ClassName *tempClassName=NULL;
    self->readyEnd=0;
    parser_help_set_forbidden(TRUE);//禁止下一个C_TOKEN被parser_help_set_class_or_enum_type改名了。
    impl_loc = c_parser_peek_token (parser)->location;
    c_parser_consume_token (parser);//consume impl$
    c_c_parser_set_source_position_from_token (c_parser_peek_token (parser));
    if (c_parser_next_token_is (parser, CPP_NAME)){
        ident = c_parser_peek_token (parser)->value;//类名Abc
        if(c_parser_peek_token (parser)->id_kind!=C_ID_TYPENAME){
          ClassName *className=class_mgr_get_class_name_by_user(class_mgr_get(),IDENTIFIER_POINTER (ident));
          if(className==NULL){
              error_at(impl_loc, "找不到类名:%qs,是否引入了类的头文件?",IDENTIFIER_POINTER (ident));
          }else{
              ident= aet_utils_create_ident(className->sysName);
          }
        }
        impl_loc = c_parser_peek_token (parser)->location;
        c_parser_consume_token (parser); //consume XXX 类名
        parserExentdsAndImplements(self,IDENTIFIER_POINTER (ident));
        c_parser_consume_token (parser);//consume {
    }else{
        error_at(impl_loc, "关键字impl$后应是类名!");
    }

    parser_help_set_forbidden(FALSE);//禁止下一个C_TOKEN被parser_help_set_class_or_enum_type改名了。
    access_controls_add_impl(access_controls_get(),n_strdup(IDENTIFIER_POINTER (ident)));
    n_debug("开始编译 impl{中的代码 class:%s %s\n",IDENTIFIER_POINTER (ident),in_fnames[0]);
    updateClassName(self,IDENTIFIER_POINTER (ident));
    c_c_parser_set_enter_aet(parser,true);
    class_impl_translation_unit(self);
    implicitly_call_link(self->implicitlyCall);
    cmp_ref_opt_opt(self->cmpRefOpt);
    c_c_parser_set_enter_aet(parser,false);
    NString *buf=n_string_new("");
    class_build_create_codes(self->classBuild,self->className,buf);
    NPtrArray *codes=class_interface_add_define(self->classInterface,self->className);
    char *superDefine=super_define_add(self->superDefine,self->className);
    char *fillSuperAddress=super_call_create_codes(self->superCall,self->className);
    char *superCodes=strCat(superDefine,fillSuperAddress);
    class_init_create_init_define(self->classInit,self->className,codes,superCodes,buf);
    var_mgr_define_class_static_var(var_mgr_get(),self->className,buf);
    if(superCodes!=NULL)
      n_free(superCodes);
    if(superDefine!=NULL)
      n_free(superDefine);
    if(fillSuperAddress!=NULL)
      n_free(fillSuperAddress);
    //创建类的类型
    n_debug("重要的初始化方法源代码 ----addInitDefine---- :\n%s\n",buf->str);
    aet_print_token(c_parser_peek_token (parser));
    aet_utils_add_token_with_force(parse_in,buf->str,buf->len,input_location,FALSE);
    n_string_free(buf,TRUE);
    tempClassName=class_name_clone(self->className);
    updateClassName(self,NULL);
    if(codes!=NULL){
      n_ptr_array_set_free_func(codes,freeIfaceCode_cb);
      n_ptr_array_unref(codes);
    }
    n_debug("impl 结束 加入泛型的5种全局变量并存储到文件中 %s %s",IDENTIFIER_POINTER (ident),in_fnames[0]);
    func_check_check_define(self->funcCheck,tempClassName);
    generic_file_save(generic_file_get(),tempClassName);
    access_controls_save_access_code(access_controls_get(),tempClassName);//保存访问控制码到当前文件中的全局变量中。
    class_name_free(tempClassName);
}

/**
 * 把 getName( 重整为  getName(Abc *self
 */
static void rearrangeMode(ClassImpl *self,int openParenPos,nboolean isFuncGeneric)
{
	  c_parser *parser=self->parser;
	  ClassName *className=self->className;
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
		  n_debug("class_impl rearrangeMode 进这里了---000 who->keyword == RID_VOID  && who1->type == CPP_CLOSE_PAREN %d openParenPos:%d\n",tokenCount,openParenPos);
		  //需要加2个token 因为 void被替换了
		  //getName(void) getName 1 ( 2 void 3 )4
		  parser->tokens_avail=tokenCount+2+addGenToken;
		  for(i=tokenCount;i>openParenPos+1;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2+addGenToken]);
		  }
	  }else{//()->(Abc *self)
		  //需要加2个token 因为 void被替换了
		  int offset=(who->type == CPP_CLOSE_PAREN)?3+addGenToken:4+addGenToken;
		  n_debug("class_impl rearrangeMode 进这里了---111 who->type == CPP_CLOSE_PAREN %d openParenPos:%d offset:%d\n",tokenCount,openParenPos,offset);
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
	  aet_utils_create_token(&parser->tokens[openParenPos],CPP_NAME,className->sysName,(int)strlen(className->sysName));
	  parser->tokens[openParenPos].id_kind=C_ID_TYPENAME;//关键
	  if(addGenToken>0){
	      n_debug("rearrangeMode generic add 如果是泛型函数需要一个参数据到函数参数列表中。");
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
	  aet_print_token_in_parser("class_impl rearrangeMode className ---- %s openParenPos:%d",className->sysName,openParenPos);
}


nboolean  class_impl_add_self_to_param(ClassImpl *self,nboolean isFuncGeneric)
{
	  c_parser *parser=self->parser;
	  if(self->className==NULL)
		  error_at(input_location,"加入self到当前函数时出错。因为没有在类实现中。");
	  nboolean ok=FALSE;
	  if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_NAME
			  && !c_token_starts_typename(c_parser_peek_2nd_token(parser)) && c_parser_peek_nth_token(parser,3)->type==CPP_OPEN_PAREN){
		  n_debug("在classimpl实现中，加入self --11--int * getName('重整为'int *getName(Abc *self'  ");
		  /*匹配 * getName(*/
		  ok=TRUE;
		  rearrangeMode(self,3,isFuncGeneric);
	  }else  if(c_parser_next_token_is (parser, CPP_NAME) && c_parser_peek_2nd_token(parser)->type==CPP_OPEN_PAREN){
		  n_debug("在classimpl实现中，加入self --22--int  getName('重整为'int getName(Abc *self'\n");
		  /*匹配 getName(*/
		  ok=TRUE;
		  rearrangeMode(self,2,isFuncGeneric);
	  }else if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_MULT &&
			  c_parser_peek_nth_token(parser,3)->type==CPP_NAME
			  && !c_token_starts_typename(c_parser_peek_nth_token(parser,3)) && c_parser_peek_nth_token(parser,4)->type==CPP_OPEN_PAREN){
		  n_debug("在classimpl实现中，加入self --33--int ** getName('重整为'int ** getName(Abc *self' \n");
		  /*匹配 ** getName(*/
		  ok=TRUE;
		  rearrangeMode(self,4,isFuncGeneric);
	  }else if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_MULT &&
			  c_parser_peek_nth_token(parser,3)->type==CPP_MULT &&
			  c_parser_peek_nth_token(parser,4)->type==CPP_NAME
			  && !c_token_starts_typename(c_parser_peek_nth_token(parser,4)) && c_parser_peek_nth_token(parser,5)->type==CPP_OPEN_PAREN){
		  n_debug("在classimpl实现中，加入self --33--int ** getName('重整为'int ** getName(Abc *self'  ");
		  /*匹配 ** getName(*/
		  ok=TRUE;
		  rearrangeMode(self,5,isFuncGeneric);
	  }
	  return ok;
}

/**
 * 如果specs中已有static 返回
 */
nboolean  class_impl_add_static_to_declspecs(ClassImpl *self,location_t loc,struct c_declspecs *specs)
{
	  if(specs->storage_class ==csc_static){
		  n_debug("已经存在static存储声明说明符了 %s",self->className->sysName);
		  return TRUE;
	  }
	  if(specs->storage_class!=csc_none){
		  n_debug("已经有其它存储声明说明符了 %d %s %s",specs->storage_class,aet_print_get_storage_class_string(specs->storage_class),self->className->sysName);
          return FALSE;
	  }
	  specs->storage_class=csc_static;
	  specs->locations[cdw_storage_class] = loc;
	  return TRUE;
}

/**
 * 返回当前语句数
 */
static int getStmtCount(ClassImpl *self)
{
	tree list=cur_stmt_list;
	int i;
	tree_stmt_iterator it;
    for (i = 0, it = tsi_start (list); !tsi_end_p (it); tsi_next (&it), i++){
	}
	return i;
}

void  class_impl_parser_super(ClassImpl *self)
{
    int aetStmtCount=getStmtCount(self);
	n_debug("class_impl_parser_super----11 %d %d\n",self->isConstructor,aetStmtCount);
	if(self->isConstructor && aetStmtCount<=1)
		class_ctor_parser_super(self->classCtor,self->className);
	else
		super_call_parser(self->superCall,self->className,self->isConstructor);
}

struct c_expr  class_impl_parser_super_at_postfix_expression(ClassImpl *self,struct c_expr expr)
{
	n_debug("class_impl_parser_super_at_postfix_expression super 00 后缀表达式 RID_AET_SUPER");
	location_t loc = c_parser_peek_token (self->parser)->location;
	tree value= super_call_parser_at_postfix_expression(self->superCall,self->className);
	if(value!=error_mark_node){
		n_debug("class_impl_parser_super_at_postfix_expression super 11 后缀表达式 RID_AET_SUPER ");
		expr.value=value;
		location_t close_loc = c_parser_peek_token (self->parser)->location;
		set_c_expr_source_range (&expr, loc, close_loc);
	}
	return expr;
}

void class_impl_nest_op (ClassImpl *self,bool add)
{
    c_parser *parser=self->parser;
	if(parser->isAet){
		if(add)
			self->nest++;
		else
			self->nest--;
	}
}

nboolean  class_impl_start_function(ClassImpl *self,struct c_declspecs *specs,struct c_declarator *declarator,
                    nboolean isFuncGeneric,location_t permissionLoc,nboolean havePermission,ClassPermissionType permission)
{
   ClassName *className=self->className;
   self->isConstructor=class_ctor_is(self->classCtor,declarator,className);
   self->nest=0;
   nboolean isQueryFunction=generic_query_is_function_declarator(generic_query_get(),className,declarator);
   if(isQueryFunction && !isFuncGeneric){
	   generic_query_add_parm_to_declarator(generic_query_get(),declarator);
   }
   nboolean changeNameOk=func_mgr_change_class_impl_func_define(func_mgr_get(),specs,declarator,className);
   n_debug("class_impl_start_function 00： 改函数定义中函数名 className:%s isCtor:%d changeNameOk:%d 是不是问号泛型函数：%d isFuncGeneric:%d\n",
		   className->sysName,self->isConstructor,changeNameOk,isQueryFunction,isFuncGeneric);
   if(changeNameOk){
      struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
      struct c_declarator *funcId=class_util_get_function_id(funcdel);
      tree id= funcId->u.id.id;
      func_mgr_check_permission_decl_between_define(func_mgr_get(),permissionLoc,className,IDENTIFIER_POINTER(id),havePermission,permission);
   }
   return changeNameOk;
}

void   class_impl_end_function(ClassImpl *self,nboolean canChangeFuncName)
{
   ClassName *className=self->className;
   n_debug("class_impl_end_function 如果是构造函数加入ssuper关键字 %s %p canChangeFuncName:%d isCtor:%d %s, %s, %d\n", className->sysName,
		  current_function_decl,canChangeFuncName,self->isConstructor,__FILE__, __FUNCTION__, __LINE__);
   if(canChangeFuncName)
	  func_mgr_set_class_impl_func_define(func_mgr_get(),current_function_decl,className);
   if(self->isConstructor){
	  class_ctor_add_super_keyword(self->classCtor,className);
   }
}

/**
 * fromDot=true 表示调用是Object.xxxx()
 * 否则xxxx()
 */
static tree createTempStaticFunction(char *sysClassName,tree component,nboolean fromDot)
{
	char *fun=IDENTIFIER_POINTER(component);
	tree  name= aet_utils_create_temp_func_name(sysClassName,fun);
    tree decl = build_decl (0, FUNCTION_DECL, name, default_function_type);
    TREE_STATIC(decl)=1;
    DECL_ARTIFICIAL (decl) = fromDot?1:0;
    BINFO_FLAG_3(decl)=1;
    return decl;
}

/**
 * 进这里说明标识符CPP_NAME前没有加self->
 * 如果是isaet，在本类和父类中找变量，如果找到了加self->
 * 然后进c_parser_postfix_expression_after_primary
 * class_ref_is_class_ref如果是class引用,进class_ref_build_deref
 * 判断是变量还是函数，如果是变量调var_call_pass_one_convert
 * 如果在isaet在类或接口中找到与tree id同名的函数,调用func_call_create_temp_func
 * 创建临时的tree并加self到函数参数中
 * setData<int> genericDefine就是<int>
 */
struct c_expr class_impl_process_expression(ClassImpl *self,struct c_expr expr,location_t loc,
                                               GenericModel *genericDefineModel,tree id,nboolean fun,int *action)
{
    c_parser *parser=self->parser;
    ClassName *className=self->className;
	if(fun){
	   FuncAndVarMsg msg=func_call_get_process_express_method(self->funcCall,id,className);
	   n_debug("class_impl_process_expression 00 找函数 %s className:%s re:%d",IDENTIFIER_POINTER(id),className==NULL?"null":className->sysName,msg);
	   if(msg==ID_IS_CONSTRUCTOR){
		   error_at(loc,"构造函数%qs只能通过new或super调用。",IDENTIFIER_POINTER(id));
	   }else if(msg==ISAET_FIND_FUNC){
		   n_debug("class_impl_process_expression 11 找到类函数 -----------className:%s funcName:%s\n",
				   className==NULL?"null":className->sysName,IDENTIFIER_POINTER(id));
		  *action=2;
		   tree  result=func_call_create_temp_func(self->funcCall,className, id);//在这里已向函数加入参数self
		   source_range tok_range = c_parser_peek_token (parser)->get_range ();
		   if(genericDefineModel){
			   c_aet_set_func_generics_model(result,genericDefineModel);
		   }
		   expr.value =result;
		   set_c_expr_source_range (&expr, tok_range);
	   }else if(msg==ISAET_FIND_STATIC_FUNC){
		   n_debug("class_impl_process_expression 22 找到类静态函数 -----------className:%s funcName:%s",
				   className==NULL?"null":className->sysName,IDENTIFIER_POINTER(id));
		  *action=2;
		   if(className==NULL){
			   n_error("class_impl_process_expression 不可出现的错误：报告此错误。");
		   }
		   tree  result =createTempStaticFunction(className->sysName,id,FALSE);
		   source_range tok_range = c_parser_peek_token (parser)->get_range ();
		   expr.value =result;
		   set_c_expr_source_range (&expr, tok_range);
	   }else if(msg==ID_NOT_FIND){
           n_info("class_impl_process_expression 33 找不到，试一下是不是函数指针 %s",IDENTIFIER_POINTER(id));
           char *belongClass=NULL;
           tree var= var_mgr_find_func_pointer(var_mgr_get(), className,id,&belongClass);
           if(var && var!=NULL_TREE && var!=error_mark_node){
               n_info("class_impl_process_expression 44 找到函数指针在类:%s %s",IDENTIFIER_POINTER(id),belongClass);
       		  *action=1;
       		   var_call_add_deref(self->varCall,id,className);
       		   n_free(belongClass);
           }else{
               n_info("class_impl_process_expression 55 找不到，可能定义在调用代码之后 %s",IDENTIFIER_POINTER(id));
               tree value=implicitly_call_call(self->implicitlyCall,expr,loc,id);
               if(aet_utils_valid_tree(value)){
                   n_info("class_impl_process_expression 66 在aet中调用了implicitly函数，生成新的函数名 %s",IDENTIFIER_POINTER(DECL_NAME(value)));
                  *action=3;
                   expr.value=value;
               }
           }
	   }
	}else{
		FuncAndVarMsg msg=var_call_get_process_var_method(self->varCall,loc,id,className);
		n_debug("class_impl_process_expression 77 找变量 %s className:%s FuncAndVarMsg:%d",
				IDENTIFIER_POINTER(id),className==NULL?"null":className->sysName,msg);
		if(msg==ISAET_FIND_VAL){
			*action=1;
			var_call_add_deref(self->varCall,id,className);
		}else if(msg==ISAET_FIND_STATIC_VAL){
		    VarEntity *var=var_mgr_get_static_entity_by_recursion(var_mgr_get(),className,id);
	        if(var!=NULL){
				n_debug("class_impl_process_expression 88 找变量 %s className:%s FuncAndVarMsg:%d",
						IDENTIFIER_POINTER(id),className==NULL?"null":className->sysName,msg);
			   *action=2;
			   source_range tok_range = c_parser_peek_token (parser)->get_range ();
			   expr.value =var_entity_get_tree(var);
			   set_c_expr_source_range (&expr, tok_range);
			}
		}else if(msg==ID_NOT_FIND){
			   nboolean exists=func_mgr_static_func_exits_by_recursion( func_mgr_get(),className,id);
			   printf("变量找不到，是不是静态函数指针赋值。99 exists:%d\n",exists);
			   /*支持在aet中与这些的方式给函数指针var赋值，strHashFunc是类中的静态函数声明。
				* AHashFunc var=strHashFunc;
				*/
			   if(exists){
	              expr.value=parser_static_create_temp_tree(parser_static_get(),loc,className,IDENTIFIER_POINTER(id));
	    		  *action=2;
			   }else{
			       //是不是com.ARandom 模式
			       if(c_parser_peek_token(parser)->type==CPP_DOT){
			             nboolean ok= parser_help_parser_right_package_dot_class(IDENTIFIER_POINTER(id));
			             if(ok){
			                 aet_print_token(c_parser_peek_token (parser));
			                 *action=4;
			             }
			       }
			   }
		}
	}
	return expr;
}

/**
 * 处理 setData<int>()此种情况
 * 返回tree
 */
GenericModel *class_impl_get_func_generic_model(ClassImpl *self,tree id)
{
   c_parser *parser=self->parser;
   ClassName *className=self->className;
   FuncAndVarMsg msg=func_call_get_process_express_method(self->funcCall,id,className);
   n_debug("class_impl_check_func_generic_define 00 找函数 %s className:%s re:%d",IDENTIFIER_POINTER(id),className==NULL?"null":className->sysName,msg);
   if(msg!=ISAET_FIND_FUNC)
	   return NULL;
   n_debug("class_impl_check_func_generic_define 11 创建 -----------className:%s funcName:%s",
			   className==NULL?"null":className->sysName,IDENTIFIER_POINTER(id));
   GenericModel *funcgen= generic_model_new(TRUE,GEN_FROM_CLASS_IMPL);
   if(!generic_impl_check_func_at_call(generic_impl_get(), IDENTIFIER_POINTER(id),funcgen)){
	   return NULL;
   }
   return funcgen;
}

/**
 * 是不是通过newfield调用的构造函数
 * 或 new$ Abc() 或 void open(new$ Abc())
 */
static CreateClassMethod getCreateObjectMethod(tree func)
{
	 tree indirectRef=TREE_OPERAND(func,0);
	 if(TREE_CODE(indirectRef)!=INDIRECT_REF)
		 return CREATE_CLASS_METHOD_UNKNOWN;
	 tree varDecl=TREE_OPERAND(indirectRef,0);
	 if(TREE_CODE(varDecl)!=VAR_DECL)
		return CREATE_CLASS_METHOD_UNKNOWN;
	 tree pointer=TREE_TYPE(varDecl);
	 if(TREE_CODE(pointer)!=POINTER_TYPE)
		 return CREATE_CLASS_METHOD_UNKNOWN;
     char *sysClassName=class_util_get_class_name_by_pointer(pointer);
	 if(sysClassName==NULL)
		 return CREATE_CLASS_METHOD_UNKNOWN;
	 char *varName=IDENTIFIER_POINTER(DECL_NAME(varDecl));
	 CreateClassMethod method=class_util_get_new_object_method(varName);
	 return method;
}

/**
 * 找出最终选择的class或接口方法
 */
struct c_expr class_impl_replace_func_id(ClassImpl *self,struct c_expr expr,vec<tree, va_gc> *exprlist,
		vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc)
{
    tree func=expr.value;
    if(TREE_CODE (func) != FUNCTION_DECL && TREE_CODE (func)!=COMPONENT_REF){
        n_info("class_impl_replace_func_id 00 不是正确的类型 FUNCTION_DECL或COMPONENT_REF code:%s", get_tree_code_name(TREE_CODE (func)));
    	return expr;
    }
    tree id=DECL_NAME (func);
    tree returnType=TREE_TYPE(func);
    if(!aet_utils_valid_tree(id) || !aet_utils_valid_tree(returnType)){
        n_warning("class_impl_replace_func_id 11 func 的id或类型无效。id:%p type:%p", id,returnType);
    	return expr;
    }
    n_debug("class_impl_replace_func_id 33 id:%s returnType:%s ",get_tree_code_name(TREE_CODE (id)),get_tree_code_name(TREE_CODE (returnType)));
    tree last=NULL_TREE;
    FuncPointerError *errors =NULL;
    //对于构造函数的调用一定是指针引用
    if(TREE_CODE (func)==COMPONENT_REF && BINFO_FLAG_4(func)==1){
        nboolean mayBeSelfOrSuperCall=FALSE;
        tree field=TREE_OPERAND(func,1);
        tree id=DECL_NAME (field);
        char *funName=n_strdup(IDENTIFIER_POINTER(id));
        n_debug("class_impl_replace_func_id 44 这是newnew 调用的构造函数--不一定?----funcName:%s 当前函数：%s 参数个数:%d",
    			funName,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)),vec_safe_length (exprlist));
    	CreateClassMethod createMethod=class_util_get_create_method((*exprlist)[0]);
     	last=class_ctor_select(self->classCtor,func,exprlist,origtypes,arg_loc,expr_loc);
    	n_debug("class_impl_replace_func_id -44 %s createMethod:%d",get_tree_code_name(TREE_CODE (last)),createMethod);
        if(createMethod==CREATE_OBJECT_METHOD_STACK || createMethod==CREATE_OBJECT_METHOD_HEAP || createMethod==CREATE_OBJECT_METHOD_FIELD_STACK){
        	new_object_finish(new_object_get(),createMethod,func);
        }else{
           CreateClassMethod method=getCreateObjectMethod(func);
           if(method>0){
		      new_object_finish(new_object_get(),method,func);
           }else{
               n_debug("class_impl_replace_func_id -44 --新建对象,调用构造函数没有走new$ createmethod: %d %s\n",method,funName);
	           mayBeSelfOrSuperCall=TRUE;
           }
        }
        class_cast_parm_convert_from_ctor(self->classCast,last,exprlist);
        if(mayBeSelfOrSuperCall){
            class_ctor_end_super_or_self_ctor_call(self->classCtor,func);
        }
    	n_free(funName);
    }else if(TREE_CODE (func)==FUNCTION_DECL &&  BINFO_FLAG_2(func)==1){
    	char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
    	n_debug("class_impl_replace_func_id 55 这是在implimpl中的函数调用，没有加self-> %s funcName:%s",IDENTIFIER_POINTER(id),funcName);
    	last=func_call_select(self->funcCall,func,exprlist,origtypes,arg_loc,expr_loc,&errors);
    	if(!aet_utils_valid_tree(last)){
    	    error_at(expr_loc,"找不到与实参匹配的方法%qs。",funcName);
    	    return expr;
    	}
    	class_cast_parm_convert_from_deref(self->classCast,last,exprlist);
    }else if(TREE_CODE (func)==FUNCTION_DECL && BINFO_FLAG_3(func)==1 && TREE_STATIC(func)){
    	//这是Class.func调用
    	char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
        nboolean fromDot=DECL_ARTIFICIAL (func);
    	n_debug("class_impl_replace_func_id 66 这是Class.func调用，没有加self-> %s funcName:%s",IDENTIFIER_POINTER(id),funcName);
    	last=func_call_static_select(self->funcCall,func,exprlist,origtypes,arg_loc,expr_loc,&errors);
    	if(last==NULL){
    		//说明没找到方法，可能是参数不对也可能是没有这个函数。
    	    last= implicitly_call_call_from_static(self->implicitlyCall,expr,exprlist,origtypes,arg_loc,expr_loc,fromDot);
    	    if(!aet_utils_valid_tree(last)){
    		   error_at(DECL_SOURCE_LOCATION(func),"找不到静态函数或传给函数的参数不正确。%q+D",func);
    		   return expr;
    	    }
    	}
    }else if(TREE_CODE (func)==COMPONENT_REF && BINFO_FLAG_6(func)==1){
        tree field=TREE_OPERAND(func,1);
        tree id=DECL_NAME (field);
    	char *funcName=IDENTIFIER_POINTER(id);
    	n_debug("class_impl_replace_func_id 77 这是指针引用  funcName:%s",IDENTIFIER_POINTER(id));
    	last=func_call_deref_select(self->funcCall,func,exprlist,origtypes,arg_loc,expr_loc,&errors);
    	class_cast_parm_convert_from_deref(self->classCast,last,exprlist);
    }else if(TREE_CODE (func)==COMPONENT_REF && BINFO_FLAG_5(func)==1){
        tree field=TREE_OPERAND(func,1);
        tree id=DECL_NAME (field);
    	char *funcName=IDENTIFIER_POINTER(id);
    	n_debug("class_impl_replace_func_id 88 这是super指针引用  funcName:%s",IDENTIFIER_POINTER(id));
    	last=func_call_super_select(self->funcCall,func,exprlist,origtypes,arg_loc,expr_loc,&errors);
        class_cast_parm_convert_from_deref(self->classCast,last,exprlist);
        if(aet_utils_valid_tree(last)){
    		last=super_call_replace_super_call(self->superCall,last);
    	}
    }else if(TREE_CODE (func)==FUNCTION_DECL && BINFO_FLAG_0(func)==1){
    	printf("在构造函数中第一条语句调用self()\n");
    	char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
    	n_debug("class_impl_replace_func_id self 这是在implimpl中的self调用，没有加self-> %s funcName:%s",IDENTIFIER_POINTER(id),funcName);
    	last=class_ctor_select_from_self(self->classCtor,func,exprlist,origtypes,arg_loc,expr_loc);
    	class_cast_parm_convert_from_deref(self->classCast,last,exprlist);
    }else{
    	if(TREE_CODE (func)==FUNCTION_DECL){
    	  char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
      	  n_info("class_impl_replace_func_id 99 不处理的函数 由implicilty处理，funcName:%s",funcName);
      	  implicitly_call_add_parm(self->implicitlyCall,func,exprlist,origtypes,arg_loc);
        }
    }
	if(aet_utils_valid_tree(last)){
	   n_debug("class_impl_replace_func_id 100 expr.value 被改变了 id:%s func:%s ",
		 get_tree_code_name(TREE_CODE (id)),get_tree_code_name(TREE_CODE (func)));
	   void *gen=c_aet_get_func_generics_model(last);
	   void *ogriGen=c_aet_get_func_generics_model(func);
	   if(ogriGen && !gen){
		   c_aet_set_func_generics_model(last,ogriGen);
	   }
	   expr.value=last;
	   expr.original_type = TREE_TYPE (last);
	}

	return expr;
}

/**
BINFO_FLAG_0(func);构造函数第一条语句self(),不加了self参数
BINFO_FLAG_1(func);静态方法调用，不加self参数
BINFO_FLAG_2(func);加了self参数 在类中不加self->调用
BINFO_FLAG_3(func);静态方法调用，不用加self
BINFO_FLAG_4(func);构造函数调用，要加self
BINFO_FLAG_5(func);通过objc->method 或 object.method 是super方法 要加self
BINFO_FLAG_6(func);通过objc->method 或 object.method 非super方法 要加self
 */
nboolean class_impl_is_aet_class_component_ref_call(ClassImpl *self,struct c_expr expr)
{
    tree func=expr.value;
    if(TREE_CODE (func) != FUNCTION_DECL && TREE_CODE (func)!=COMPONENT_REF)
    	return FALSE;
    n_debug("class_impl_is_aet_class_call 00 ");
    tree id=DECL_NAME (func);
    tree returnType=TREE_TYPE(func);
    if(!aet_utils_valid_tree(id) || !aet_utils_valid_tree(returnType))
    	return FALSE;

    if(TREE_CODE (func)==COMPONENT_REF && BINFO_FLAG_4(func)==1){
        return TRUE;
    }else if(TREE_CODE (func)==FUNCTION_DECL && BINFO_FLAG_2(func)==1){
        return FALSE;
    }else if(TREE_CODE (func)==FUNCTION_DECL && BINFO_FLAG_3(func)==1 && TREE_STATIC(func)){
        return FALSE;
    }else if(TREE_CODE (func)==COMPONENT_REF && BINFO_FLAG_6(func)==1){
        return TRUE;
    }else if(TREE_CODE (func)==COMPONENT_REF && BINFO_FLAG_5(func)==1){
        return TRUE;
    }else{
       return FALSE;
    }
}

/**
 * 由c-parser.c调用
 * 当参数是类名，找到当前类名，取系统名替换原来的类名。
 * 如who=AObject 替换成who=test_AObject
 * 如果当前编译文件中有两个相同的类型会引起冲突。如何解决，根据上下文决定选那一个，上下文？？
 * Dog 是一个结构体
 * Dog 是一个类
 */
nboolean class_impl_set_class_or_enum_type(ClassImpl *self,c_token *who)
{
   return parser_help_set_class_or_enum_type(who);
}

/**
 * 是不是AObject.
 */
nboolean  class_impl_next_tokens_is_class_and_dot (ClassImpl *self)
{
    c_parser *parser=self->parser;
    c_token *token = c_parser_peek_token (parser);
    if (token->type == CPP_NAME  && token->id_kind == C_ID_TYPENAME && c_parser_peek_2nd_token (parser)->type == CPP_DOT){
       char *sysClassName=IDENTIFIER_POINTER(token->value);
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
       return info!=NULL;
    }
    return FALSE;
}

/**
 * 是不是AObject.Enum
 */
nboolean  class_impl_next_tokens_is_class_dot_enum (ClassImpl *self)
{
    c_parser *parser=self->parser;
    c_token *token = c_parser_peek_token (parser);
    if (token->type == CPP_NAME  && token->id_kind == C_ID_TYPENAME && c_parser_peek_2nd_token (parser)->type == CPP_DOT){
       char *sysClassName=IDENTIFIER_POINTER(token->value);
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
       if(info!=NULL){
    	   token=c_parser_peek_nth_token (parser,3);
           if(token->type==CPP_NAME){
        	   nboolean re=enum_parser_is_class_dot_enum(enum_parser_get(),sysClassName,IDENTIFIER_POINTER(token->value));
        	   return re;
           }
       }
    }
    return FALSE;
}

/**
 * 是不是AObject.Enum.
 */
nboolean  class_impl_next_tokens_is_class_dot_enum_dot (ClassImpl *self)
{
    c_parser *parser=self->parser;
    c_token *token = c_parser_peek_token (parser);
    if (token->type == CPP_NAME  && token->id_kind == C_ID_TYPENAME && c_parser_peek_2nd_token (parser)->type == CPP_DOT){
       char *sysClassName=IDENTIFIER_POINTER(token->value);
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
       if(info!=NULL){
    	   token=c_parser_peek_nth_token (parser,3);
           if(token->type==CPP_NAME){
        	   nboolean re=enum_parser_is_class_dot_enum(enum_parser_get(),sysClassName,IDENTIFIER_POINTER(token->value));
        	   if(re){
            	   token=c_parser_peek_nth_token (parser,4);
            	   if(token->type==CPP_DOT)
            		   return TRUE;
        	   }
           }
       }
    }
    return FALSE;
}

/**
 * 是不是Enum.
 */
nboolean  class_impl_next_tokens_is_enum_and_dot (ClassImpl *self)
{
    c_parser *parser=self->parser;
    c_token *token = c_parser_peek_token (parser);
    if (token->type == CPP_NAME  && token->id_kind == C_ID_TYPENAME && c_parser_peek_2nd_token (parser)->type == CPP_DOT){
       char *typedefName=IDENTIFIER_POINTER(token->value);
       nboolean re=enum_parser_is_enum(enum_parser_get(),typedefName);
       return re;
    }
    return FALSE;
}

/**
 * 解析Class.name表达式或Class.Enum.element
 */
void class_impl_build_class_dot (ClassImpl *self, location_t loc,struct c_expr *expr)
{
	c_parser *parser=self->parser;
	tree classTree = c_parser_peek_token (parser)->value;
	char *sysClassName=IDENTIFIER_POINTER(classTree);
	tree component;
	c_parser_consume_token (parser);//consume AObject
	parser_help_set_forbidden(TRUE);
	if (!c_parser_require (parser, CPP_DOT, "expected %<.%>")){//这里consume CPP_DOT
	   expr->set_error ();
	   return;
	}
	if (c_parser_next_token_is_not (parser, CPP_NAME)){
	   c_parser_error (parser, "expected identifier");
	   expr->set_error ();
	   return;
	}
	c_token *component_tok = c_parser_peek_token (parser);
	component = component_tok->value;
	location_t end_loc = component_tok->get_finish ();
	c_parser_consume_token (parser);//consume CPP_NAME
    parser_help_set_forbidden(FALSE);
	if (c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
       n_debug("是函数调用----Class.func %s %s\n",sysClassName,IDENTIFIER_POINTER(component));
       expr->value =createTempStaticFunction(sysClassName,component,TRUE);
	   set_c_expr_source_range (expr, loc, end_loc);
	}else{
		ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
		if(info==NULL){
		   error_at(loc,"找不到类:%qs",sysClassName);
		   expr->set_error ();
		   return;
		}else{
            VarEntity *varEntity=var_mgr_get_static_entity_by_recursion(var_mgr_get(),&info->className,component);
            tree var=NULL_TREE;
	        if(varEntity==NULL){
			   nboolean exits=func_mgr_static_func_exits_by_recursion(func_mgr_get(),&info->className,component);
			   if(!exits){
				  if(!strcmp(IDENTIFIER_POINTER(component),"class")){
					  n_debug("这是一个获取AClass的方法 类:%s\n",sysClassName);
					  expr->value =class_build_get_func(self->classBuild,&info->className);
					  set_c_expr_source_range (expr, loc, end_loc);
					  return;
				  }else{
					  //查找是不是枚举 Class和CPP_DOT已被consume了
					  char *enumOrigName=IDENTIFIER_POINTER(component);
					  nboolean isClassDotEnum=enum_parser_is_class_dot_enum(enum_parser_get(),sysClassName,enumOrigName);
					  if(isClassDotEnum){
					      parser_help_set_forbidden(TRUE);
						  enum_parser_build_class_dot_enum(enum_parser_get(),loc,sysClassName,enumOrigName,expr);
	                      parser_help_set_forbidden(FALSE);
					  }else{
			             error_at(loc,"在类%qs中找不到变量:%qE",sysClassName,component);
			             expr->set_error ();
					  }
			          return;
				  }
			   }else{
				  n_info("通过%s名，找到了函数,在这里不能判断是不是参数。",IDENTIFIER_POINTER(component));
                  var=parser_static_create_temp_tree(parser_static_get(),loc,&info->className,IDENTIFIER_POINTER(component));
                  n_info("通过%s名，11 找到了函数,在这里不能判断是不是参数。%s",IDENTIFIER_POINTER(component),IDENTIFIER_POINTER(DECL_NAME(var)));
			   }
			}else{
				//找到了变量，获取该该变量是属于那个类的
	             var=var_entity_get_tree(varEntity);
	             access_controls_access_var(access_controls_get(),loc,varEntity->orgiName,varEntity->sysName);
			}
			expr->value =var;
			set_c_expr_source_range (expr, loc, end_loc);
		}
	}
}

void class_impl_build_enum_dot (ClassImpl *self, location_t loc,struct c_expr *expr)
{
    parser_help_set_forbidden(TRUE);
	enum_parser_build_dot(enum_parser_get(),loc,expr);
    parser_help_set_forbidden(FALSE);
}


/**
 * 编译完成后，调用保存泛型及接口静态变量定义需要的xml文件
 */
void   class_impl_compile_over(ClassImpl *self)
{
    n_debug("class_impl_compile_over -- %d\n",enter_aet);
	if(enter_aet){ //说明用到过aet，所以需要检查
	   access_controls_check(access_controls_get());
	   block_mgr_save_define_newobject_and_func_call(block_mgr_get());
	   iface_file_save(iface_file_get());
	   makefile_parm_append_d_file(makefile_parm_get());
	}
}

/**
 * 编译 -O2 会优化掉没有使用的静态变量，
 * 但如果定义如下，就不会
 * static  __attribute__((used))  char tyxy[]="asss";
 *  TREE_STATIC (decl) = 1;
    TREE_READONLY (decl) = 0;
    DECL_ARTIFICIAL (decl) = 0;
	DECL_CONTEXT(decl)=NULL_TREE;
    DECL_EXTERNAL(decl)=0;
	TREE_PUBLIC(decl)=0;
	DECL_READ_P (decl) = 1;
	TREE_USED (decl)=1;
    DECL_PRESERVE_P (decl) = 1;
    特别是DECL_PRESERVE_P=1很重要，参考自c-attribs.c handle_used_attribute
    创建的变量不能是指针。形如:static  __attribute__((used))  char tyxy[]="asss";
 */
static tree createStaticVar(ClassImpl *self, char *varName,char *codes)
{
	c_parser *parser=self->parser;
	location_t  loc = input_location;
	tree id=aet_utils_create_ident(varName);
	size_t length = strlen (codes);
    tree decl, type, init;
    type = build_array_type (char_type_node,build_index_type (size_int (length)));
    decl = build_decl (loc, VAR_DECL, id, type);
    TREE_STATIC (decl) = 1;
    TREE_READONLY (decl) = 0;
    DECL_ARTIFICIAL (decl) = 0;
	DECL_CONTEXT(decl)=NULL_TREE;
    DECL_EXTERNAL(decl)=0;
	TREE_PUBLIC(decl)=0;
	DECL_READ_P (decl) = 1;
	TREE_USED (decl)=1;
    DECL_PRESERVE_P (decl) = 1;

    pushdecl (decl);
    init = build_string (length + 1, codes);
    TREE_TYPE (init) = type;
    DECL_INITIAL (decl) = init;
    TREE_USED (decl) = 1;
    DECL_PRESERVE_P (decl) = 1;
    TREE_STATIC (decl) = 1;
    TREE_READONLY (decl) = 0;
    DECL_ARTIFICIAL (decl) = 0;
	DECL_CONTEXT(decl)=NULL_TREE;
    DECL_EXTERNAL(decl)=0;
	TREE_PUBLIC(decl)=0;
	DECL_READ_P (decl) = 1;
    finish_decl (decl, loc, init, NULL_TREE, NULL_TREE);
    return decl;
}

static tree findOBJECTVarDecl(ClassImpl *self,char *name)
{
	int i;
	for(i=0;i<self->objectMacro.count;i++){
		if(strcmp(self->objectMacro.names[i],name)==0)
			return self->objectMacro.varDecles[i];
	}
	return NULL_TREE;
}

/**
 * 解析__OBJECT__
 */
struct c_expr class_impl_parser_object(ClassImpl *self)
{
	c_parser *parser=self->parser;
    location_t loc = c_parser_peek_token (parser)->location;
    struct c_expr expr;
    expr.original_code = ERROR_MARK;
    expr.original_type = NULL;
	char name[256];
	char *initValue="";
    if(!parser->isAet){
    	//在实现中
    	char *id="__object__default_name";
    	unsigned int hash=aet_utils_create_hash(id,strlen(id));
    	sprintf(name,"%s_%u",id,hash);
    }else{
    	//在实现中
		char *id="__objectClass";
		char *cname=self->className->sysName;
		unsigned int hash=aet_utils_create_hash(cname,strlen(cname));
		sprintf(name,"%s_%s_%u",id,cname,hash);
		initValue=self->className->userName;
    }
    tree varDecl=findOBJECTVarDecl(self,name);
	if(varDecl==NULL_TREE){
		varDecl=createStaticVar(self,name,initValue);
		int index=self->objectMacro.count;
		self->objectMacro.names[index]=n_strdup(name);
		self->objectMacro.varDecles[index]=varDecl;
		self->objectMacro.count++;
	}
	expr.value=varDecl;
    set_c_expr_source_range (&expr, loc, loc);
    c_parser_consume_token (parser);
    return expr;
}

void   class_impl_finish_function(ClassImpl *self,tree fndecl)
{
    object_return_finish_function(object_return_get(),fndecl);
    cmp_ref_opt_add(self->cmpRefOpt,fndecl);
}

/**
 * 如果是对象变量加入到objectreturn中处理，如果是转化由objectreturn处理并返回
 */
tree   class_impl_add_return(ClassImpl *self,location_t loc,tree retExpr)
{
    object_return_add_return(object_return_get(),retExpr);
    tree expr=object_return_convert(object_return_get(),loc,retExpr);
    return expr;
}

void  class_impl_in_finish_decl(ClassImpl *self ,tree decl)
{
	class_cast_in_finish_decl(self->classCast,decl);
}

void class_impl_in_finish_stmt(ClassImpl *self ,tree stmt)
{
    class_cast_in_finish_stmt(self->classCast,stmt);
}

tree  class_impl_cast(ClassImpl *self,struct c_type_name *type_name,tree expr)
{
	return class_cast_cast(self->classCast,type_name,expr);
}

struct c_expr class_impl_nameles_call(ClassImpl *self,struct c_expr expr)
{
    expr.value=nameless_call_parser(nameless_call_get(),expr.value);
    return expr;
}

/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 */
tree class_impl_modify_or_init_func_pointer(ClassImpl *self,tree lhs,tree rhs)
{
   return parser_static_modify_or_init_func_pointer(parser_static_get(),lhs,rhs);
}

/**
 * 是不是Abc.xxxx表达式
 */
nboolean class_impl_is_class_dot_expression(ClassImpl *self)
{
    c_parser *parser=self->parser;
    c_token *token=c_parser_peek_2nd_token(parser);
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),IDENTIFIER_POINTER(token->value));
    if(info==NULL)
        return;
    token=c_parser_peek_nth_token (parser,3);
    if(token->type==CPP_DOT){
        return TRUE;
    }
    return FALSE;
}

/**
 * 检查调用self()是不是在构造函数内的第一条语句。
 */
static nboolean isSelfCall(ClassImpl *self)
{
    c_parser *parser=self->parser;
    ClassName *className=self->className;
    int aetStmtCount=getStmtCount(self);
    int error=0;
    nboolean sysCreate=class_ctor_self_is_first(self->classCtor,className,&error);
    if(error){
        error_at(input_location,"在处理self()函数时出现未知错误！,请报告此错误。");
        n_error("在处理self()函数时出现未知错误！,请报告此错误。");
        return FALSE;
    }
    if(self->isConstructor && aetStmtCount<=1 && sysCreate){
       return TRUE;
    }else{
        if(self->isConstructor){
             c_parser_error (parser,"self()函数只能是构造函数中的第一条语句.");
        }else{
             c_parser_error(parser,"self()函数只能用在构造函数中的第一条语句.");
        }
        return FALSE;
    }
}

/**
 * 如果是self()并且在构造函数内，把self替换成if(self->XXXX(
 */
void class_impl_replace_self_call_at_statement_after_labels(ClassImpl *self)
{
     c_parser *parser=self->parser;
     c_token *token=c_parser_peek_token (parser);
     if(token->type==CPP_NAME){
         tree value=token->value;
         if(value && !strcmp(IDENTIFIER_POINTER(value),"self")){
             c_token *token=c_parser_peek_2nd_token(parser);
             if(token->type==CPP_OPEN_PAREN){
                 nboolean is=isSelfCall(self);
                 //printf("可能是在构造函数中调用self了 %d\n",is);
                 if(is){
                     ClassName *className=self->className;
                     class_ctor_process_self_call(self->classCtor,className);
                 }
             }
         }
     }
}

tree  class_impl_build_deref(ClassImpl *self,location_t loc,location_t component_loc,tree component,tree exprValue)
{
    tree result=class_ref_build_deref(self->classRef,loc,component_loc,component,exprValue);
    if(BINFO_FLAG_4(result)==1){
        n_debug("调用构造函数了---- %s\n",IDENTIFIER_POINTER(component));
        class_ctor_set_tag_for_self_and_super_call(self->classCtor,result);
    }
    return result;
}

/**
 * 解析关键字varof$
 */
struct c_expr   class_impl_varof_parser(ClassImpl *self,struct c_expr lhs){
    return aet_expr_varof_parser(self->aetExpr,lhs);
}

/**
 * 解析com.ai.NLayer 重点是 com.ai.
 * 有三处地方会调用。
 * 1.函数参数声明 2.函数体内变量声明
 */
nboolean class_impl_parser_package_dot_class(ClassImpl *self)
{
      return parser_help_parser_left_package_dot_class();
}

ClassName      *class_impl_get_class_name(ClassImpl *self)
{
    return self->className;
}

void  class_impl_set_parser(ClassImpl *self, c_parser *parser)
{
	  self->parser=parser;
	  self->varCall->parser=parser;
	  self->classCtor->parser=parser;
	  func_call_set_parser(self->funcCall,parser);
	  self->superCall->parser=parser;
	  self->classBuild->parser=parser;
	  self->funcCheck->parser=parser;
	  self->classInit->parser=parser;
	  class_access_set_parser((ClassAccess*)(self->classRef),parser,self->varCall,self->superCall,self->funcCall);
	  class_access_set_parser((ClassAccess*)(self->classDot),parser,self->varCall,self->superCall,self->funcCall);
	  self->classInterface->parser=parser;
	  self->classFinalize->parser=parser;
	  self->classCast->parser=parser;
	  self->superDefine->parser=parser;
	  nameless_call_set_parser(nameless_call_get(),parser);
	  generic_query_set_parser(generic_query_get(),parser);
	  access_controls_set_parser(access_controls_get(),parser);
	  implicitly_call_set_parser(self->implicitlyCall,parser);
      cmp_ref_opt_set_parser(self->cmpRefOpt,parser);
      aet_expr_set_parser(self->aetExpr,parser);
	  printf("编译文件:in_fnames[0]:%s\n",in_fnames[0]);
}

ClassImpl *class_impl_get()
{
	static ClassImpl *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(ClassImpl));
		 classImplInit(singleton);
	}
	return singleton;
}

void class_impl_test_target(tree target)
{
    if(TREE_CODE(target)!=TARGET_EXPR)
        return;
    tree init=TREE_OPERAND (target, 1);
    if(TREE_CODE(init)!=BIND_EXPR)
        return;
    tree body=TREE_OPERAND (init, 1);
    if(TREE_CODE(body)!=STATEMENT_LIST)
        return;
    tree t=body;
    tree_stmt_iterator it;
    int i;
    for (i = 0, it = tsi_start (t); !tsi_end_p (it); tsi_next (&it), i++){
//            char buffer[32];
//            sprintf (buffer, "%u", i);
//            dump_child (buffer, tsi_stmt (it));
    }
    printf("class_impl_test_target --- stmt count:%d\n",i);
    if(i==5){
        for (i = 0, it = tsi_start (t); !tsi_end_p (it); tsi_next (&it), i++){
            if(i==3){
                tree modify=tsi_stmt (it);
                tree decl=TREE_OPERAND (modify, 0);
                aet_print_tree(decl);
                printf("zclei sss createVarDeclStmt --- %d %d %d %d %d TREE_PUBLIC (decl):%d DECL_REGISTER (decl):%d\n",VAR_P (decl),DECL_SEEN_IN_BIND_EXPR_P (decl),TREE_STATIC (decl),DECL_EXTERNAL (decl),
                            decl_function_context (decl) == current_function_decl,TREE_PUBLIC (decl),DECL_REGISTER (decl));

            }
        }
    }
}


