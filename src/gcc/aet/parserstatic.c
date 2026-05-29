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
#include "toplev.h"
#include "asan.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c/c-parser.h"
#include "opts.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "classmgr.h"
#include "parserstatic.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "varmgr.h"
#include "parserhelp.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "genericimpl.h"
#include "genericmodel.h"
#include "newobject.h"
#include "aetmicro.h"
#include "selectfield.h"
#include "mtcsparser.h"

static void parserStaticInit(ParserStatic *self)
{

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

/**
 * 编历 struct c_declarator *declarator 在数组的维度
 * 如果是变量，看是否有初始值，如果有并且是常数，替换
 */
void parser_static_replace_dimension(ParserStatic *self,struct c_declarator *declarator)
{
   struct c_declarator *decl = declarator;
   while (decl){
      switch (decl->kind){
         case cdk_array:
         {
            tree size = decl->u.array.dimen;
            if(size && TREE_CODE(size)==VAR_DECL){
               char *name=IDENTIFIER_POINTER(DECL_NAME(size));
               char sysName[255];
               char varName[255];
               int ok =aet_utils_get_orgi_var_and_class_name(name,sysName,varName);
               n_debug("replaceDimension ----ok:%d %s %s\n",ok,sysName,varName);
               if(ok>0){
                  VarEntity *entity = var_mgr_get_entity_by_mangle(var_mgr_get(),sysName,name);
                  if(entity && entity->isStatic){
                     tree constValue= var_entity_get_const_init(entity);
                     if(constValue){
                        n_debug("replaceDimension ---被替换为常量-ok:%d %s %s %s\n",ok,sysName,varName,name);
                        decl->u.array.dimen = constValue;
                     }
                  }
               }
            }
         }
            break;
         default:
            break;
      }
      decl = decl->declarator;
   }
}

/*
 *原型 finish_function c-decl.cc
 *解决BUG
 *原因:当调用finish_function时，会调用cgraph_node::finalize_function (fndecl, false);再调用    ggc_collect ();
 *这时class声明还没有全部完成。所以class_parser_parser_class_specifier contents = chainon (decls, contents); contents
 *变成@1      ggc_freed        addr: 7f93550dbd10 内存出错。
*/
static void aet_finish_function (location_t end_loc)
{
   tree fndecl = current_function_decl;

   //  if (c_dialect_objc ())
   //    objc_finish_function ();

   if (TREE_CODE (fndecl) == FUNCTION_DECL  && targetm.calls.promote_prototypes (TREE_TYPE (fndecl))){
      tree args = DECL_ARGUMENTS (fndecl);
      for (; args; args = DECL_CHAIN (args)){
         tree type = TREE_TYPE (args);
         if (INTEGRAL_TYPE_P (type) && TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node))
            DECL_ARG_TYPE (args) = c_type_promotes_to (type);
      }
   }

   if (DECL_INITIAL (fndecl) && DECL_INITIAL (fndecl) != error_mark_node)
      BLOCK_SUPERCONTEXT (DECL_INITIAL (fndecl)) = fndecl;

   /* Must mark the RESULT_DECL as being in this function.  */

   if (DECL_RESULT (fndecl) && DECL_RESULT (fndecl) != error_mark_node)
      DECL_CONTEXT (DECL_RESULT (fndecl)) = fndecl;

   if (MAIN_NAME_P (DECL_NAME (fndecl)) && !TREE_THIS_VOLATILE (fndecl)
   && TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (fndecl)))  == integer_type_node && flag_isoc99){
      /* Hack.  We don't want the middle-end to warn that this return
      is unreachable, so we mark its location as special.  Using
      UNKNOWN_LOCATION has the problem that it gets clobbered in
      annotate_one_with_locus.  A cleaner solution might be to
      ensure ! should_carry_locus_p (stmt), but that needs a flag.
      */
      c_finish_return (BUILTINS_LOCATION, integer_zero_node, NULL_TREE);
   }

   /* Tie off the statement tree for this function.  */
   DECL_SAVED_TREE (fndecl) = pop_stmt_list (DECL_SAVED_TREE (fndecl));

   finish_fname_decls ();

   /* Complain if there's no return statement only if option specified on
   command line.  */
   if (warn_return_type > 0
   && TREE_CODE (TREE_TYPE (TREE_TYPE (fndecl))) != VOID_TYPE
   && !current_function_returns_value && !current_function_returns_null
   /* Don't complain if we are no-return.  */
   && !current_function_returns_abnormally
   /* Don't complain if we are declared noreturn.  */
   && !TREE_THIS_VOLATILE (fndecl)
   /* Don't warn for main().  */
   && !MAIN_NAME_P (DECL_NAME (fndecl))
   /* Or if they didn't actually specify a return type.  */
   && !C_FUNCTION_IMPLICIT_INT (fndecl)
   /* Normally, with -Wreturn-type, flow will complain, but we might
   optimize out static functions.  */
   && !TREE_PUBLIC (fndecl)
   && targetm.warn_func_return (fndecl)
   && warning (OPT_Wreturn_type, "no return statement in function returning non-void"))
      suppress_warning (fndecl, OPT_Wreturn_type);

   /* Complain about parameters that are only set, but never otherwise used.  */
   if (warn_unused_but_set_parameter){
      tree decl;

      for (decl = DECL_ARGUMENTS (fndecl); decl; decl = DECL_CHAIN (decl))
      if (TREE_USED (decl)
      && TREE_CODE (decl) == PARM_DECL
      && !DECL_READ_P (decl)
      && DECL_NAME (decl)
      && !DECL_ARTIFICIAL (decl)
      && !warning_suppressed_p (decl, OPT_Wunused_but_set_parameter))
         warning_at (DECL_SOURCE_LOCATION (decl),OPT_Wunused_but_set_parameter, "parameter %qD set but not used", decl);
   }

   /* Complain about locally defined typedefs that are not used in this
   function.  */
   maybe_warn_unused_local_typedefs ();

   /* Possibly warn about unused parameters.  */
   if (warn_unused_parameter)
      do_warn_unused_parameter (fndecl);

   /* Store the end of the function, so that we get good line number
   info for the epilogue.  */
   cfun->function_end_locus = end_loc;

   /* Finalize the ELF visibility for the function.  */
   c_determine_visibility (fndecl);

   /* For GNU C extern inline functions disregard inline limits.  */
   if (DECL_EXTERNAL (fndecl)
   && DECL_DECLARED_INLINE_P (fndecl)
   && (flag_gnu89_inline
   || lookup_attribute ("gnu_inline", DECL_ATTRIBUTES (fndecl))))
      DECL_DISREGARD_INLINE_LIMITS (fndecl) = 1;

   /* Genericize before inlining.  Delay genericizing nested functions
   until their parent function is genericized.  Since finalizing
   requires GENERIC, delay that as well.  */

   if (DECL_INITIAL (fndecl) && DECL_INITIAL (fndecl) != error_mark_node  /*!&& !undef_nested_function*/){
      if (!decl_function_context (fndecl)){
         c_genericize (fndecl);
         /* ??? Objc emits functions after finalizing the compilation unit.
         This should be cleaned up later and this conditional removed.  */
         if (symtab->global_info_ready){
            cgraph_node::add_new_function (fndecl, false);
            return;
         }
         cgraph_node::finalize_function (fndecl, true);
      } else{
         /* Register this function with cgraph just far enough to get it
         added to our parent's nested function list.  Handy, since the
         C front end doesn't have such a list.  */
         (void) cgraph_node::get_create (fndecl);
      }
   }

   /*!
   if (!decl_function_context (fndecl))
      undef_nested_function = false;
   */
   if (cfun->language != NULL){
      ggc_free (cfun->language);
      cfun->language = NULL;
   }

   /* We're leaving the context of this function, so zap cfun.
   It's still in DECL_STRUCT_FUNCTION, and we'll restore it in
   tree_rest_of_compilation.  */
   set_cfun (NULL);
   current_function_decl = NULL;
}

/**
 * 原型 c_parser_declaration_or_fndef c-parser.cc
 */
static void parser_var_or_fun_declaration(ParserStatic *self,struct c_declspecs *specs,
		ClassName *className,tree structTree,ClassPermissionType permi,nboolean isFinal)
{
   c_parser *parser=self->parser->parser;
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
      declarator = aet_parser_c_parser_declarator/*!c_parser_declarator*/(self->parser,
            specs->typespec_kind != ctsk_none,C_DTR_NORMAL, &dummy);
      aet_print_token(c_parser_peek_token (parser));
      if (declarator == NULL){
         n_debug("静态声明分析 55： count:%d declarator == NULL ", testcount);
         aet_parser_c_parser_skip_to_end_of_block_or_statement/*!c_parser_skip_to_end_of_block_or_statement*/(self->parser);
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
            aet_parser_c_parser_skip_to_end_of_block_or_statement/*!c_parser_skip_to_end_of_block_or_statement*/(self->parser);
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
            start_init (d, asm_name, global_bindings_p (),0, &richloc);
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
               init =aet_parser_c_parser_initializer/*!c_parser_initializer*/(self->parser,d);
               flag_sanitize = flag_sanitize_save;
               finish_init ();
            }
            aet_print_token(c_parser_peek_token (parser));
            n_debug("静态声明分析 99：完成初始化 token是等号 count:%d init:%p ",testcount,init.value);
            if (d != error_mark_node){
               maybe_warn_string_init (init_loc, TREE_TYPE (d), init);
               n_debug("静态声明分析 100 这里是关键，静态变量被声明成extern 但是定义要留到impl$中实现，所以把初始化保存起来。init:%p isStaticVarDecl:%d\n",
               init.value,isStaticVarDecl);
               //finish_decl (d, init_loc, init.value,init.original_type, asm_name);
               finish_decl (d, UNKNOWN_LOCATION, NULL_TREE,NULL_TREE, asm_name);
               if(isStaticVarDecl){
                  var_mgr_set_static_decl(var_mgr_get(),className,d,&init,permi,isFinal);
               }
            }
         }else{//c_parser_next_token_is (parser, CPP_EQ) 声明符右边不是等号
            n_debug("静态声明分析 101 start_decl 构建完整的 fun 函数声明语句的树结构 specs中有int"
                  "声明说明符树 declarator有fun及参数的树 count:%d structTree:%p", testcount,structTree);
            nboolean  isFuncOk=func_mgr_change_static_func_decl(func_mgr_get(),declarator,className,structTree);
            nboolean isStaticVarDecl=FALSE;
            if(!isFuncOk)
               isStaticVarDecl=var_mgr_change_static_decl(var_mgr_get(),className,specs,declarator);
            //判断是不是数组，如果是判断元素是不是一个静态变量
            if(isStaticVarDecl){
               parser_static_replace_dimension(self,declarator);
            }

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
                  func_mgr_set_static_func_decl(func_mgr_get(),d,className,FALSE);
                  func_mgr_set_static_func_premission(func_mgr_get(),className,d,permi,isFinal);
               }
               if(isStaticVarDecl){
                  var_mgr_set_static_decl(var_mgr_get(),className,d,NULL,permi,isFinal);
               }
               mtcs_parser_check(mtcs_parser_get(),d,TRUE);
            }
         }// end c_parser_next_token_is (parser, CPP_EQ)

         if(c_parser_next_token_is (parser, CPP_COMMA)){
            n_debug("静态声明分析 eee： count:%d declarator:%p ", testcount,declarator);
            c_parser_consume_token (parser);
            if (c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE))
               all_prefix_attrs = chainon (aet_parser_c_parser_gnu_attributes/*!c_c_parser_gnu_attributes*/(self->parser),prefix_attrs);
            else
               all_prefix_attrs = prefix_attrs;
            continue;
         }else if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
            n_debug("静态声明分析 token是 CPP_SEMICOLON 返回 104： count:%d ", testcount);
            //c_parser_consume_token (parser); //不屏掉，会报 "警告：结构或联合后没有分号"
            return;
         }else{
            c_parser_error (parser, "expected %<,%> or %<;%>");
            aet_parser_c_parser_skip_to_end_of_block_or_statement/*!c_parser_skip_to_end_of_block_or_statement*/(self->parser);
            return;
         }//end c_parser_next_token_is (parser, CPP_COMMA 分析完所有的EQ COMMA SEMICOLON RID_IN...
      }else if(c_parser_next_token_is (parser, CPP_OPEN_BRACE) ){
         n_debug("静态声明分析 105： 这是一个内联函数 count:%d ", testcount);
         specs->inline_p=TRUE;
         specs->storage_class = csc_static;
      }else{
         n_debug("静态声明分析 106： 出错了 ,出错的符号是 count:%d ", testcount);
         c_parser_error (parser, "expected %<=%>, %<,%>, %<;%>, %<asm%> or %<__attribute__%>");
         aet_parser_c_parser_skip_to_end_of_block_or_statement/*!c_parser_skip_to_end_of_block_or_statement*/(self->parser);
         return;
      }

      nboolean  isFuncOk=func_mgr_change_static_func_decl(func_mgr_get(),declarator,className,structTree);
      n_debug("静态声明分析 107： 分析函数定义 count:%d declarator:%p isFuncOk:%d ",testcount,declarator,isFuncOk);
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
      if(isFuncOk){
         tree currentFnDecl = current_function_decl;
         mtcs_parser_check(mtcs_parser_get(),currentFnDecl,TRUE);
         if(func_mgr_is_mtcs_func(func_mgr_get(),currentFnDecl))
            mtcs_parser_enter_function_body(mtcs_parser_get());
      }
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
      fnbody = aet_parser_c_parser_compound_statement/*!c_c_parser_compound_statement*/(self->parser, &endloc);
      tree fndecl = current_function_decl;
      if (fnbody)
         add_stmt (fnbody);
      aet_finish_function/*!finish_function*/(endloc);
      if(isFuncOk){
         func_mgr_set_static_func_decl(func_mgr_get(),fndecl,className,TRUE);
         func_mgr_set_static_func_premission(func_mgr_get(),className,fndecl,permi,isFinal);
         if(func_mgr_is_mtcs_func(func_mgr_get(),fndecl))
             mtcs_parser_exit_function_body(mtcs_parser_get());
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
    AET_LANG_FLAG_1(decl)=1;
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
   c_parser *parser=self->parser->parser;
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
      aet_parser_c_parser_static_assert_declaration_no_semi/*!c_parser_static_assert_declaration_no_semi*/(self->parser);
      n_debug("新----staticDeclaration ---11 RID_STATIC_ASSERT 返回 %s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);
      return NULL_TREE;
   }

   if(c_parser_next_token_is_keyword (parser, RID_STATIC)){
      n_debug("新----staticDeclaration 是一个静态声明明符 sysClassName:%s", className->sysName);
      error_at(input_location,"已经有一个静态关键字了");
      n_error("未知错误");
   }

   if(c_parser_next_token_is_keyword (parser, RID_AET_ENUM)){
      n_debug("新----staticDeclaration 是一个静态AET枚举 sysClassName:%s", className->sysName);
      error_at(input_location,"是一个静态AET枚举");
      n_error("是一个静态AET枚举");
   }

   specs = build_null_declspecs ();
   decl_loc = c_parser_peek_token (parser)->location;
   //检查是不是泛型方法
   //格式如下：public$ <T> void getName(); <T>在函数返回值之前。
   aet_parser_c_parser_declspecs_generic(self->parser,specs);

   n_debug("新----staticDeclaration 内的声明 00 建立空的声明说明符 sysClassName:%s permission:%d CPP_LESS:%d\n",
         className->sysName,permission,c_parser_next_token_is (parser, CPP_LESS));
   aet_print_token(c_parser_peek_token (parser));
   aet_parser_c_parser_declspecs/*!c_parser_declspecs*/(self->parser,
         specs, true, true, true,true, false, true, true, cla_nonabstract_decl);
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
      error_at(decl_loc,"类%qs不支持静态泛型函数。",className->userName);
      return NULL_TREE;
   }
   parser_var_or_fun_declaration(self,specs,className,structTree,permission,isFinal);
   return NULL_TREE;
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
    if(TREE_CODE(rhs)==FUNCTION_DECL && AET_LANG_FLAG_1(rhs)==1){
        return TRUE;
    }else if(TREE_CODE(rhs)==ADDR_EXPR && AET_LANG_FLAG_1(TREE_OPERAND (rhs, 0))==1){
        return TRUE;
    }else if(TREE_CODE(rhs)==NOP_EXPR){
        tree add=   TREE_OPERAND (rhs, 0);
        if(TREE_CODE(add)!=ADDR_EXPR)
            return FALSE;
        tree func=TREE_OPERAND (add, 0);
        if(TREE_CODE(func)==FUNCTION_DECL && AET_LANG_FLAG_1(func)==1){
           return TRUE;
        }
    }
    return FALSE;
}

static tree getSelectedField(ParserStatic *self,location_t loc,tree lhs,tree rhs)
{
     nboolean isMatch=checkType(lhs,rhs);
     if(!isMatch){
         return rhs;
     }
//     location_t loc=input_location;
//     if(DECL_P(lhs))
//        loc=DECL_SOURCE_LOCATION(lhs);
//     else if(EXPR_P(lhs))
//        loc=EXPR_LOCATION(lhs);
//     if(loc>2*input_location){
//         printf("getSelectedField的左值的位置大于当前输入位置，不正常!!! %u %u\n",loc,input_location);
//         loc=input_location;
//     }
     FuncPointerError *errors=NULL;
     tree result=select_field_modify_or_init_field(select_field_get(),loc,lhs,rhs,&errors);
     if(!aet_utils_valid_tree(result)){
         select_field_printf_func_pointer_error(errors);
         error_at(loc,"由于返回值或参数据不匹配，类静态函数不能赋值给函数指针。");
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
tree parser_static_modify_or_init_func_pointer(ParserStatic *self,location_t loc,tree lhs,tree rhs)
{
   if(TREE_CODE(rhs)==COND_EXPR){
      tree first=   TREE_OPERAND (rhs, 1);
      tree second=  TREE_OPERAND (rhs, 2);
      first=getSelectedField(self,loc,lhs,first);
      second=getSelectedField(self,loc,lhs,second);
      TREE_OPERAND (rhs, 1)=first;
      TREE_OPERAND (rhs, 2)=second;
      return rhs;
   }else{
      return getSelectedField(self,loc,lhs,rhs);
   }
}

void parser_static_compile(ParserStatic *self,char *sysName,ClassPermissionType permission,nboolean isFinal)
{
   staticDeclaration(self,sysName,permission,isFinal);
}

ParserStatic *parser_static_get()
{
   static ParserStatic *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(ParserStatic));
      parserStaticInit(singleton);
      singleton->parser = aet_parser_get();
   }
   return singleton;
}



