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
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c/c-parser.h"
#include "opts.h"

#include "aetutils.h"
#include "classmgr.h"
#include "parserstatic.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "varmgr.h"
#include "aet-c-parser-header.h"
#include "parserhelp.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "genericimpl.h"
#include "genericmodel.h"
#include "newobject.h"
#include "aetmicro.h"
#include "selectfield.h"

static obstack static_obstack;//所有在class中的静态变量和函数都在分析完class后再编译。


typedef struct _StaticTokens
{
    ClassPermissionType permission;
	nboolean isFinal;
    NPtrArray *tokens;
    NString *codes;
}StaticTokens;

static void parserStaticInit(ParserStatic *self)
{
	self->tokenArray=n_ptr_array_new();
	gcc_obstack_init (&static_obstack);
}

/**
 * 全局变量定义
 * 在c文件函数、struct和union外定义在类中声明的静态变量
 * 如:
 * class$ Abc{
 *   static int INFO=5
 * };
 * int AObject.INFO=5;
 */

static void checkAtInterface(ClassName *className,tree decl)
{
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(class_info_is_interface(info) && TREE_CODE(decl)==FUNCTION_DECL){
		error_at(input_location,"接口%qs不能声明静态函数%qD！",className->userName,decl);
	}
}

static void parser_var_or_fun_declaration(ParserStatic *self,struct c_declspecs *specs,
		ClassName *className,tree structTree,ClassPermissionType permi,nboolean isFinal)
{
  c_parser *parser=self->parser;
  tree prefix_attrs;
  tree all_prefix_attrs;
  bool diagnosed_no_specs = false;
  bool have_attrs=false;
  location_t here= c_parser_peek_token (parser)->location;
  specs->storage_class = csc_extern;
  n_debug("静态声明分析 00:把static改成 csc_extern specs->inline_p :%d const:%d file:%s",specs->inline_p ,specs->const_p,in_fnames[0]);
  /* Try to detect an unknown type name when we have "A B" or "A *B".  */
  if (c_parser_peek_token (parser)->type == CPP_NAME   && c_parser_peek_token (parser)->id_kind == C_ID_ID
      && (c_parser_peek_2nd_token (parser)->type == CPP_NAME || c_parser_peek_2nd_token (parser)->type == CPP_MULT)
      && !lookup_name (c_parser_peek_token (parser)->value)){

        tree name = c_parser_peek_token (parser)->value;
        n_warning("静态声明分析 11：一般不到这里 A B\" or \"A *B");
        aet_print_token(c_parser_peek_token (parser));

      /* Issue a warning about NAME being an unknown type name, perhaps
	   with some kind of hint.
	   If the user forgot a "struct" etc, suggest inserting
	   it.  Otherwise, attempt to look for misspellings.  */
       gcc_rich_location richloc (here);
       if (tag_exists_p (RECORD_TYPE, name)){
	      /* This is not C++ with its implicit typedef.  */
	      richloc.add_fixit_insert_before ("struct ");
	      error_at (&richloc,"unknown type name %qE; use %<struct%> keyword to refer to the type",name);
	   }else if (tag_exists_p (UNION_TYPE, name)){
	       richloc.add_fixit_insert_before ("union ");
	       error_at (&richloc,"unknown type name %qE; use %<union%> keyword to refer to the type",name);
	   }else if (tag_exists_p (ENUMERAL_TYPE, name)){
	       richloc.add_fixit_insert_before ("enum ");
	       error_at (&richloc,"unknown type name %qE; use %<enum%> keyword to refer to the type",name);
	   }else{
	      auto_diagnostic_group d;
	      name_hint hint = lookup_name_fuzzy (name, FUZZY_LOOKUP_TYPENAME,here);
	      if (const char *suggestion = hint.suggestion ()){
	          richloc.add_fixit_replace (suggestion);
	          error_at (&richloc,"unknown type name %qE; did you mean %qs?",name, suggestion);
	      }else
	         error_at (here, "unknown type name %qE", name);
	   }

		  /* Parse declspecs normally to get a correct pointer type, but avoid
			 a further "fails to be a type name" error.  Refuse nested functions
			 since it is not how the user likely wants us to recover.  */
       c_parser_peek_token (parser)->type = CPP_KEYWORD;
       c_parser_peek_token (parser)->keyword = RID_VOID;
       c_parser_peek_token (parser)->value = error_mark_node;
  }

  // 此函数把声明说明符的类型信息设置到 specs 上
  // 这个函数执行完,符号“ int ”对应的树节点就设置完了
  bool auto_type_p = specs->typespec_word == cts_auto_type;
  if(auto_type_p){
	   error_at (here, "%<__auto_type%> in empty declaration");
	   return;
  }
  if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
	  n_debug("静态声明分析 22:下一个是分号吗：是的： ");
      if (specs->typespec_kind == ctsk_none  && attribute_fallthrough_p (specs->attrs)){
         pedwarn (here, OPT_Wattributes,"%<fallthrough%> attribute at top level");
	 }else if (!(have_attrs && specs->non_std_attrs_seen_p))
	     shadow_tag (specs);
     else{
	     shadow_tag_warned (specs, 1);
	     pedwarn (here, 0, "empty declaration");
	 }
     c_parser_consume_token (parser);
     return;
  }

  /* Provide better error recovery.  Note that a type name here is usually
     better diagnosed as a redeclaration.  */
  if (specs->typespec_kind == ctsk_tagdef  && c_parser_next_token_starts_declspecs (parser)
      && !c_parser_next_token_is (parser, CPP_NAME)){
      c_parser_error (parser, "expected %<;%>, identifier or %<(%>");
      parser->error = false;
      shadow_tag_warned (specs, 1);
      n_warning("静态声明分析 33：出错了 have_attrs:%d ", have_attrs);
      return;
  }else if (attribute_fallthrough_p (specs->attrs))
    warning_at (here, OPT_Wattributes,"%<fallthrough%> attribute not followed by %<;%>");

  pending_xref_error ();
  prefix_attrs = specs->attrs;
  all_prefix_attrs = prefix_attrs;
  specs->attrs = NULL_TREE;
  int testcount=0;
  while (true){
	  n_debug("静态声明分析 44: 在这里分析声明符： count:%d ", testcount++);
      struct c_declarator *declarator;
      bool dummy = false;
      /* Declaring either one or more declarators (in which case we
	 should diagnose if there were no declaration specifiers) or a
	 function definition (in which case the diagnostic for
	 implicit int suffices).  */
  	  aet_print_token(c_parser_peek_token (parser));
      declarator = c_parser_declarator (parser, specs->typespec_kind != ctsk_none,C_DTR_NORMAL, &dummy);
  	  aet_print_token(c_parser_peek_token (parser));
      if (declarator == NULL){
    	  n_debug("静态声明分析 55： count:%d declarator == NULL ", testcount);
	     c_c_parser_skip_to_end_of_block_or_statement (parser);
	     return;
	  }
      if (c_parser_next_token_is (parser, CPP_EQ)
	      || c_parser_next_token_is (parser, CPP_COMMA)
	      || c_parser_next_token_is (parser, CPP_SEMICOLON)
	      || c_parser_next_token_is_keyword (parser, RID_ASM)
	      || c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE)){
	      tree asm_name = NULL_TREE;
	      tree postfix_attrs = NULL_TREE;
	     if (!diagnosed_no_specs && !specs->declspecs_seen_p){
		       n_debug("静态声明分析 66： count:%d ", testcount);
	           diagnosed_no_specs = true;
	           pedwarn (here, 0, "data definition has no type or storage class");
	     }
    	  n_debug("静态声明分析 77： count:%d declarator:%p ", testcount,declarator);

	      /* Having seen a data definition, there cannot now be a
	       function definition.  */
	     if (c_parser_next_token_is_keyword (parser, RID_ASM)){
            error_at (here,"不能有RID_ASM");
            return;
	     }
	     if (c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE)){
		      n_debug("静态声明分析 88： count:%d ", testcount);
		       error_at (here, "不能有attributes should be specified before the declarator in a function definition");
		       c_c_parser_skip_to_end_of_block_or_statement (parser);
		       return;
	     }
	     if (c_parser_next_token_is (parser, CPP_EQ)){
	         tree d;
	         struct c_expr init;
	         location_t init_loc;
	         c_parser_consume_token (parser);
			 /* The declaration of the variable is in effect while  its initializer is parsed.  */
			 //d = start_decl (declarator, specs, true,chainon (postfix_attrs, all_prefix_attrs));
	         nboolean isStaticVarDecl=var_mgr_change_static_decl(var_mgr_get(),className,specs,declarator);
			 d = start_decl (declarator, specs, false,chainon (postfix_attrs, all_prefix_attrs));
			 if (!d)
				d = error_mark_node;
			 init_loc = c_parser_peek_token (parser)->location;
			 rich_location richloc (line_table, init_loc);
			 start_init (d, asm_name, global_bindings_p (), &richloc);
			  /* A parameter is initialized, which is invalid.  Don't  attempt to instrument the initializer.  */
			 int flag_sanitize_save = flag_sanitize;
			 if (TREE_CODE (d) == PARM_DECL)
				flag_sanitize = 0;
			 n_debug("静态声明分析 初始化做准备。");
			 if (c_parser_next_token_is_keyword (parser, RID_AET_NEW)){
                 //生成new$的原始代码
				 char *codes=new_object_parser_for_static(new_object_get(),d,NULL);
				 tree newObjectCodes=aet_utils_create_ident(codes);
				 init.value=newObjectCodes;
				 n_free(codes);
			 }else{
				 init = c_c_parser_initializer (parser);
				 flag_sanitize = flag_sanitize_save;
				 finish_init ();
			 }
			 aet_print_token(c_parser_peek_token (parser));
			 n_debug("静态声明分析 99：完成初始化 token是等号 count:%d init:%p ",testcount,init.value);
	         if (d != error_mark_node){
				maybe_warn_string_init (init_loc, TREE_TYPE (d), init);
				n_debug("静态声明分析 100 这里是关键，静态变量被声明成extern 但是定义要留到impl$中实现，所以把初始化保存起来。finish_decl value is %p\n",init.value);
				//finish_decl (d, init_loc, init.value,init.original_type, asm_name);
				finish_decl (d, UNKNOWN_LOCATION, NULL_TREE,NULL_TREE, asm_name);
				if(isStaticVarDecl){
					var_mgr_set_static_decl(var_mgr_get(),className,d,&init,permi,isFinal);
				}
		     }
	     }else{//c_parser_next_token_is (parser, CPP_EQ) 声明符右边不是等号
		     n_debug("静态声明分析 101 start_decl 构建完整的 fun 函数声明语句的树结构 specs中有int 声明说明符树 declarator有fun及参数据的树 count:%d ",
		    		 testcount);
	         nboolean  isFuncOk=func_mgr_change_static_func_decl(func_mgr_get(),declarator,className,structTree);
	         nboolean isStaticVarDecl=FALSE;
	         if(!isFuncOk)
	            isStaticVarDecl=var_mgr_change_static_decl(var_mgr_get(),className,specs,declarator);
	         tree d = start_decl (declarator, specs, false,chainon (postfix_attrs,all_prefix_attrs));
	         if (d  && TREE_CODE (d) == FUNCTION_DECL && DECL_ARGUMENTS (d) == NULL_TREE && DECL_INITIAL (d) == NULL_TREE){
		            /* Find the innermost declarator that is neither cdk_id   nor cdk_attrs.  */
		          const struct c_declarator *decl = declarator;
		          const struct c_declarator *last_non_id_attrs = NULL;
		           n_debug("静态声明分析 102： count:%d ", testcount);
		           while (decl)
		              switch (decl->kind){
		                case cdk_array:
		                case cdk_function:
		                case cdk_pointer:
			               last_non_id_attrs = decl;
			               decl = decl->declarator;
			               break;
		                case cdk_attrs:
			               decl = decl->declarator;
			               break;
		                case cdk_id:
			              decl = 0;
			              break;
		               default:
			              gcc_unreachable ();
		             }
		            /* If it exists and is cdk_function, use its parameters.  */
		           if (last_non_id_attrs   && last_non_id_attrs->kind == cdk_function)
		                DECL_ARGUMENTS (d) = last_non_id_attrs->u.arg_info->parms;
		     }
	         if (d){
				 n_debug("静态声明分析 103： finish_decl count:%d permission:%d isFuncOk:%d", testcount,permi,isFuncOk);
				 finish_decl (d, UNKNOWN_LOCATION, NULL_TREE,NULL_TREE, asm_name);
				 checkAtInterface(className,d);
				 if(isFuncOk){
					 func_mgr_set_static_func_decl(func_mgr_get(),d,className);
					 func_mgr_set_static_func_premission(func_mgr_get(),className,d,permi,isFinal);
				 }
				 if(isStaticVarDecl){
					var_mgr_set_static_decl(var_mgr_get(),className,d,NULL,permi,isFinal);
				 }
	         }
	     }// end c_parser_next_token_is (parser, CPP_EQ)

	     if(c_parser_next_token_is (parser, CPP_COMMA)){
			 n_debug("静态声明分析 eee： count:%d declarator:%p ", testcount,declarator);

			 c_parser_consume_token (parser);
			 if (c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE))
				all_prefix_attrs = chainon (c_c_parser_gnu_attributes (parser),prefix_attrs);
			 else
				all_prefix_attrs = prefix_attrs;
			 continue;
	     } else if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
			 n_debug("静态声明分析 104： count:%d ", testcount);
			 //c_parser_consume_token (parser); //不屏掉，会报 "警告：结构或联合后没有分号"
			 return;
	     }else {
			 c_parser_error (parser, "expected %<,%> or %<;%>");
			 c_c_parser_skip_to_end_of_block_or_statement (parser);
			 return;
	     }//end c_parser_next_token_is (parser, CPP_COMMA 分析完所有的EQ COMMA SEMICOLON RID_IN...
      }else if(c_parser_next_token_is (parser, CPP_OPEN_BRACE) ){
		   n_debug("静态声明分析 105： 这是一个内联函数 count:%d ", testcount);
		  specs->inline_p=TRUE;
    	  specs->storage_class = csc_static;
	  }else{
		   n_debug("静态声明分析 106： 出错了 ,出错的符号是 count:%d ", testcount);
	       c_parser_error (parser, "expected %<=%>, %<,%>, %<;%>, %<asm%> or %<__attribute__%>");
	       c_c_parser_skip_to_end_of_block_or_statement (parser);
	       return;
	  }

      nboolean  isFuncOk=func_mgr_change_static_func_decl(func_mgr_get(),declarator,className,structTree);
  	  n_debug("静态声明分析 107： 分析函数定义 count:%d declarator:%p isFuncOk:%d ",
  			  testcount,declarator,isFuncOk);
      if (!start_function (specs, declarator, all_prefix_attrs)){
        	  n_debug("静态声明分析 108：函数定义出错了 count:%d declarator:%p ", testcount,declarator);

	        gcc_assert (!c_parser_next_token_is (parser, CPP_SEMICOLON));
	        if (c_parser_next_token_starts_declspecs (parser)) {
	          /* If we have
		      declaration-specifiers declarator decl-specs
		      then assume we have a missing semicolon, which would
		      give us:
		      declaration-specifiers declarator  decl-specs
			    			    ^
			    			    ;
		       <~~~~~~~~~ declaration ~~~~~~~~~~>
		        Use c_parser_require to get an error with a fix-it hint.  */
		       n_debug("静态声明分析 109： count:%d ", testcount);
	           c_parser_require (parser, CPP_SEMICOLON, "expected %<;%>");
	           parser->error = false;
	       }else{
	           /* This can appear in many cases looking nothing like a
		      function definition, so we don't give a more specific
		       error suggesting there was one.  */
	           c_parser_error (parser, "expected %<=%>, %<,%>, %<;%>, %<asm%> or %<__attribute__%>");
	       }
	       break;
	  }
	  n_debug("静态声明分析 110： count:%d declarator:%p ", testcount,declarator);
      tree fnbody = NULL_TREE;

      while (c_parser_next_token_is_not (parser, CPP_EOF)  && c_parser_next_token_is_not (parser, CPP_OPEN_BRACE)){
    	  n_debug("静态声明分析 111 循环了 count:%d declarator:%p ", testcount,declarator);
    	  parser_var_or_fun_declaration (self,specs, className,structTree,permi,isFinal);
      }
	  n_debug("静态声明分析 112：保存函数定义的参数据 count:%d declarator:%p ", testcount,declarator);
      store_parm_decls ();
      location_t startloc = c_parser_peek_token (parser)->location;
      DECL_STRUCT_FUNCTION (current_function_decl)->function_start_locus= startloc;
      location_t endloc = startloc;
      n_debug("静态声明分析 113：对函数内部的复合语句进行分析 count:%d declarator:%p ", testcount,declarator);
      aet_print_token(c_parser_peek_token(parser));
	  fnbody = c_c_parser_compound_statement (parser, &endloc);
      tree fndecl = current_function_decl;
      if (fnbody)
    	  add_stmt (fnbody);
      finish_function (endloc);
 	  if(isFuncOk){
		 func_mgr_set_static_func_decl(func_mgr_get(),fndecl,className);
		 func_mgr_set_static_func_premission(func_mgr_get(),className,fndecl,permi,isFinal);
 	  }
      /* Get rid of the empty stmt list for GIMPLE/RTL.  */
      if (specs->declspec_il != cdil_none)
	     DECL_SAVED_TREE (fndecl) = NULL_TREE;
      aet_print_token(c_parser_peek_token(parser));
      break;
    }//end while
}


tree  parser_static_create_temp_tree(ParserStatic *self,location_t loc,ClassName *className,char *orginalName)
{
    tree name=aet_utils_create_temp_func_name(className->sysName,orginalName);
    tree decl = build_decl (loc, FUNCTION_DECL, name, default_function_type);
    BINFO_FLAG_1(decl)=1;
    return decl;
}



////////////////////////////////////////////把static全部token收集--------------------------


/*
 * 专门分析结构体内的内容
 * struct中是不允许static关键字的，c_parser_declspecs第三个参数
 * bool scspec_ok原来是false 为了支持static 现在改为true
 */
static tree  staticDeclaration (ParserStatic *self,char *sysName,ClassPermissionType permission,nboolean isFinal)
{
    c_parser *parser=self->parser;
    ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),sysName);
    ClassType classType=classInfo->type;
    tree structTree=classInfo->record;
    ClassName *className=&classInfo->className;
    struct c_declspecs *specs;
    tree prefix_attrs;
    tree all_prefix_attrs;
    tree decls;
    location_t decl_loc;
    tree fieldGeneric=NULL_TREE;//函数返回值或变量声明是否是泛型
    tree funcGeneric=NULL_TREE; //是不是一个函数泛型
    if (c_parser_next_token_is_keyword (parser, RID_EXTENSION)){
    	error_at(decl_loc,"类不支持静态函数中的RID_EXTENSION。%qs",className->userName);
       return NULL_TREE;
    }
    if (c_parser_next_token_is_keyword (parser, RID_STATIC_ASSERT)){
       c_c_parser_static_assert_declaration_no_semi (parser);
       n_debug("新----staticDeclaration ---11 RID_STATIC_ASSERT 返回 %s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);
       return NULL_TREE;
    }

    if(c_parser_next_token_is_keyword (parser, RID_STATIC)){
        n_debug("新----staticDeclaration 是一个静态声明明符 编完class$后再处理 sysClassName:%s", className->sysName);
        error_at(input_location,"未知错误");
        n_error("未知错误");
    }

    specs = build_null_declspecs ();
    decl_loc = c_parser_peek_token (parser)->location;
    n_debug("新----staticDeclaration 内的声明 00 建立空的声明说明符 sysClassName:%s permission:%d\n", className->sysName,permission);
    aet_print_token(c_parser_peek_token (parser));
    c_parser_declspecs (parser, specs, true, true, true,true, false, true, true, cla_nonabstract_decl);
    if (parser->error)
       return NULL_TREE;
    if (!specs->declspecs_seen_p){
       c_parser_error (parser, "expected specifier-qualifier-list");
       return NULL_TREE;
    }
    finish_declspecs (specs);
    n_debug("新----staticDeclaration 内的声明 11 到这里specs已有声明说明符了比如 int 完成finish_declspecs permission:%d\n",permission);
	//static <T> void setdata 这种情况，<T>是存在specs中
	GenericModel *funcGenModel=generic_impl_pop_generic_from_declspecs(generic_impl_get(),specs);
	if(aet_utils_valid_tree(funcGeneric) || funcGenModel){
		error_at(decl_loc,"类不支持泛型静态函数。%qs",className->userName);
		return NULL_TREE;
	}
    parser_var_or_fun_declaration(self,specs,className,structTree,permission,isFinal);
   	return NULL_TREE;
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

static void c_parser_consume_pragma (c_parser *parser)
{
  gcc_assert (!parser->in_pragma);
  gcc_assert (parser->tokens_avail >= 1);
  gcc_assert (parser->tokens[0].type == CPP_PRAGMA);
  if (parser->tokens != &parser->tokens_buf[0])
    parser->tokens++;
  else if (parser->tokens_avail >= 2)
    {
      parser->tokens[0] = parser->tokens[1];
      if (parser->tokens_avail >= 3)
	parser->tokens[1] = parser->tokens[2];
    }
  parser->tokens_avail--;
  parser->in_pragma = true;
}

static void skipToEndBlockOrStatement (c_parser *parser,NString *codes,NPtrArray *tokenArray)
{
  unsigned nesting_depth = 0;
  bool save_error = parser->error;
  enum cpp_ttype previewType=0;
  while (true)
    {
      c_token *token;

      /* Peek at the next token.  */
      token = c_parser_peek_token (parser);

      switch (token->type)
	{
	case CPP_EOF:
	  return;

	case CPP_PRAGMA_EOL:
	  if (parser->in_pragma)
	    return;
	  break;

	case CPP_SEMICOLON:
	  /* If the next token is a ';', we have reached the
	     end of the statement.  */
	  if (!nesting_depth)
	    {
	      /* Consume the ';'.  */
		  char *source=aet_utils_convert_token_to_string(token);
		  c_token *clone = XOBNEW (&static_obstack, struct c_token);
		  aet_utils_copy_token(token,clone);
		  n_ptr_array_add(tokenArray,clone);
		  n_string_append(codes,source);
	      //c_parser_consume_token (parser);
	      goto finished;
	    }
	  break;

	case CPP_CLOSE_BRACE:
	  /* If the next token is a non-nested '}', then we have
	     reached the end of the current block.  */
	  if (nesting_depth == 0 || --nesting_depth == 0)
	    {
		  char *source=aet_utils_convert_token_to_string(token);
		  c_token *clone = XOBNEW (&static_obstack, struct c_token);
		  aet_utils_copy_token(token,clone);
		  n_ptr_array_add(tokenArray,clone);
		  n_string_append(codes,source);
	      c_parser_consume_token (parser);
	      token = c_parser_peek_token (parser);
	      aet_print_token(token);
	      if(token->type==CPP_SEMICOLON){
			  char *source=aet_utils_convert_token_to_string(token);
			  c_token *clone = XOBNEW (&static_obstack, struct c_token);
			  aet_utils_copy_token(token,clone);
			  n_ptr_array_add(tokenArray,clone);
			  n_string_append(codes,source);
	    	  n_string_append(codes,"\n");
		      c_parser_consume_token (parser);
	      }
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
      c_token *clone = XOBNEW (&static_obstack, struct c_token);
      aet_utils_copy_token(token,clone);
      n_ptr_array_add(tokenArray,clone);
      n_string_append(codes,source);
      aet_print_token(token);
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


void  parser_static_collect_token(ParserStatic *self,ClassPermissionType permission,nboolean isFinal)
{
	c_parser *parser=self->parser;
	StaticTokens *staticTokens=(StaticTokens *)n_slice_new(StaticTokens);
	staticTokens->permission=permission;
	staticTokens->isFinal=isFinal;
	staticTokens->codes=n_string_new("");
	staticTokens->tokens=n_ptr_array_new();
	NString *codes=n_string_new("");
	skipToEndBlockOrStatement(parser,staticTokens->codes,staticTokens->tokens);
	n_debug("parser_static_collect_token--:%s permission:%d\n",staticTokens->codes->str,permission);
	n_ptr_array_add(self->tokenArray,staticTokens);
}

/**
 * 取最后的,、;、 )
 */
static int getPos(NPtrArray *tokens,int avail)
{
	  int i;
	  for(i=avail-1;i>=0;i--){
		  c_token *item=n_ptr_array_index(tokens,i);
		  if(item->type==CPP_COMMA || item->type==CPP_SEMICOLON || item->type==CPP_CLOSE_PAREN){
			  return i+1;
		  }
	  }
	  return avail;
}

/**
 */
static int compileToken(ParserStatic *self,NPtrArray *tokens,int avail)
{
	  c_parser *parser=self->parser;
	  int tokenCount=parser->tokens_avail;
	  int tokenLen=avail;
	  int append=0;
	  if(tokens->len>avail){
		  append=2;
		  //查找avail倒数有,;)的token
		  avail=getPos(tokens,avail);
		  tokenLen=avail;
	  }

	  if(tokenCount+tokenLen+append>AET_MAX_TOKEN){
		 error("token太多了");
		 return -1;
	  }

	  int i;
	  for(i=tokenCount;i>0;i--){
			aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+tokenLen+append]);
	  }
	  for(i=0;i<tokenLen;i++){
		  c_token *item=n_ptr_array_index(tokens,i);
	      aet_utils_copy_token(item,&parser->tokens[i]);
	  }
	  if(append>0){
		 aet_utils_create_aet_goto_token(&parser->tokens[tokenLen],input_location);
		 aet_utils_create_number_token(&parser->tokens[tokenLen+1],GOTO_STATIC_CONTINUE_COMPILE);
	  }
	  parser->tokens_avail=tokenCount+tokenLen+append;
	  aet_print_token_in_parser("static compileToken  c-parser原有token:%d NPtrArray中的:%d 可以加的：%d" ,tokenCount,tokens->len,avail);
	  return tokenLen;
}

/**
 * 返回-1,可以装下全部
 */
static int getCount(ParserStatic *self,NPtrArray *tokens)
{
   c_parser *parser=self->parser;
   int i;
   int availcount=0;
   int tokenCount=parser->tokens_avail;
   int total=tokens->len;
   if(AET_MAX_TOKEN-tokenCount>=total){
	   n_debug("可以装入所有的token avail:%d tokens:%d\n",AET_MAX_TOKEN-tokenCount,total);
	   return -1;
   }
   int max=AET_MAX_TOKEN-tokenCount-1-2;
   int semiAppear=-1;
   for(i=0;i<total;i++){
	  c_token *token=n_ptr_array_index(tokens,i);
	  if(token->type==CPP_SEMICOLON){
          semiAppear=i;//semicolon出现的位置
	  }
      if(i>=max-1){
    	  n_debug("没有;但是i>max,退出:i:%d max:%d 分号出现位置:%d\n",i,max,semiAppear);
    	  break;
      }
   }
   availcount=i+1;
   if(semiAppear>0){
	   availcount=semiAppear+1;
   }
   n_debug("availcount :%d total:%d c-parser token:%d\n",availcount,total,tokenCount);
   return availcount;
}

void  parser_static_compile(ParserStatic *self,char *sysName)
{
	//printf("parser_static_compile--- %d\n",self->tokenArray->len);
	if(self->tokenArray->len==0)
		return;
	int i;
	self->compileIndex=-1;
	for(i=0;i<self->tokenArray->len;i++){
		self->compileIndex=i;
		StaticTokens *staticTokens=n_ptr_array_index(self->tokenArray,i);
		int avail=getCount(self,staticTokens->tokens);
		if(avail==-1){//可以全部装入，所以移走在staticTokens所有的token
			compileToken(self,staticTokens->tokens,staticTokens->tokens->len);
			n_ptr_array_remove_range(staticTokens->tokens,0,staticTokens->tokens->len);
		}else{
			avail=compileToken(self,staticTokens->tokens,avail);
			n_ptr_array_remove_range(staticTokens->tokens,0,avail);
		}
		ClassPermissionType permission=staticTokens->permission;//CLASS_PERMISSION_DEFAULT;
		staticDeclaration(self,sysName,permission,staticTokens->isFinal);
	}
}

void  parser_static_continue_compile(ParserStatic *self)
{
	StaticTokens *staticTokens=n_ptr_array_index(self->tokenArray,self->compileIndex);
	int avail=getCount(self,staticTokens->tokens);
	if(avail==-1){
		compileToken(self,staticTokens->tokens,staticTokens->tokens->len);
		n_ptr_array_remove_range(staticTokens->tokens,0,staticTokens->tokens->len);
	}else{
		avail=compileToken(self,staticTokens->tokens,avail);
		n_ptr_array_remove_range(staticTokens->tokens,0,avail);
	}
}

static void freeTokens_cb(StaticTokens *value)
{
	n_ptr_array_unref(value->tokens);
	n_string_free(value->codes,TRUE);
	n_slice_free(StaticTokens,value);
}

/**
 * 当编译完class后，清除所有的static
 */
void  parser_static_ready(ParserStatic *self)
{
	n_ptr_array_set_free_func(self->tokenArray,freeTokens_cb);
	n_ptr_array_unref(self->tokenArray);
	self->tokenArray=n_ptr_array_new();
}

//---------------------------------------以下是为函数指针赋值后重新选定类中静态域-------------------------
/**
 * 检查是不是函数指针赋值
 */
static nboolean checkType(tree lhs,tree rhs)
{
    tree type=TREE_TYPE(lhs);
    if(TREE_CODE(type)!=POINTER_TYPE)
        return FALSE;
    type=TREE_TYPE(type);
    if(TREE_CODE(type)!=FUNCTION_TYPE)
        return FALSE;
    if(TREE_CODE(rhs)!=FUNCTION_DECL && TREE_CODE(rhs)!=ADDR_EXPR && TREE_CODE(rhs)!=NOP_EXPR)
            return FALSE;
    if(TREE_CODE(rhs)==FUNCTION_DECL && BINFO_FLAG_1(rhs)==1){
        return TRUE;
    }else if(TREE_CODE(rhs)==ADDR_EXPR && BINFO_FLAG_1(TREE_OPERAND (rhs, 0))==1){
        return TRUE;
    }else if(TREE_CODE(rhs)==NOP_EXPR){
        tree add=   TREE_OPERAND (rhs, 0);
        if(TREE_CODE(add)!=ADDR_EXPR)
            return FALSE;
        tree func=TREE_OPERAND (add, 0);
        if(TREE_CODE(func)==FUNCTION_DECL && BINFO_FLAG_1(func)==1){
           return TRUE;
        }
    }
    return FALSE;
}

static tree getSelectedField(ParserStatic *self,tree lhs,tree rhs)
{
     nboolean isMatch=checkType(lhs,rhs);
     if(!isMatch){
         return rhs;
     }
     printf("parser_static_modify_or_init_func_pointer 00 isMatch:%d\n",isMatch);
     location_t loc=input_location;
     if(DECL_P(lhs))
        loc=DECL_SOURCE_LOCATION(lhs);
     else if(EXPR_P(lhs))
        loc=EXPR_LOCATION(lhs);
     if(loc>2*input_location){
         printf("getSelectedField的左值的位置大于当前输入位置，不正常!!! %u %u\n",loc,input_location);
         loc=input_location;
     }
     FuncPointerError *errors=NULL;
     tree result=select_field_modify_or_init_field(select_field_get(),loc,lhs,rhs,&errors);
     if(!aet_utils_valid_tree(result)){
         select_field_printf_func_pointer_error(errors);
         error_at(loc,"由于返回值或参数据不匹配，类的静态函数不能赋值给函数指针。");
         return rhs;
     }
     return result;
}

/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 *  1.AHashFunc func;
 *  func=AObject.strHash;
 *  2.AHashFunc func;
 *  func=5>3?AObject.strHash:xxx;
 *  3.AHashFunc func=AObject.strHash;
 *  4.AHashFunc func=5>3?AObject.strHash:xxx;
 */
tree parser_static_modify_or_init_func_pointer(ParserStatic *self,tree lhs,tree rhs)
{
   if(TREE_CODE(rhs)==COND_EXPR){
       tree first=   TREE_OPERAND (rhs, 1);
       tree second=  TREE_OPERAND (rhs, 2);
       first=getSelectedField(self,lhs,first);
       second=getSelectedField(self,lhs,second);
       TREE_OPERAND (rhs, 1)=first;
       TREE_OPERAND (rhs, 2)=second;
       return rhs;
   }else{
       return getSelectedField(self,lhs,rhs);
   }
}


void parser_static_set_parser(ParserStatic *self,c_parser *parser)
{
	 self->parser=parser;
}

ParserStatic *parser_static_get()
{
	static ParserStatic *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(ParserStatic));
		 parserStaticInit(singleton);
	}
	return singleton;
}




