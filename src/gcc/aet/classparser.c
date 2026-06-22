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
#include "toplev.h"
#include "attribs.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "incpath.h"
#include "aet-c-parser-header.h"

#include "c-aet.h"
#include "classparser.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "newobject.h"
#include "genericcall.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericquery.h"
#include "parserhelp.h"
#include "middlefile.h"
#include "ifaceimpl.h"
#include "enumparser.h"
#include "mtcsparser.h"
#include "aetmediator.h"
#include "mtcstypes.h"
#include "genericgraph.h"
#include "genericcode.h"
#include "makefileparm.h"

static void restoreToken(ClassParser *self);

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

static  void initAddressDiagnosticCallback()
{
   unsigned long add=aet_demangle_text;
   char *value=xmalloc(100);
   sprintf(value,"%lu",add);
   setenv("initAddressDiagnosticCallback",value,0);
}

static char *nLogGetCompileFileAndFunc_cb(char **func)
{
   char *ret = current_function_name();
   if(ret==NULL || !strcmp(ret,"(nofn)")){
       ret = NULL;
       tree current=current_function_decl;
       if(current && DECL_NAME(current)){
          ret=IDENTIFIER_POINTER(DECL_NAME(current));
       }
   }
  // printf("nLogGetCompileFileAndFunc_cb ----- %s %s\n",ret,in_fnames[0]);
   *func = ret;
   cpp_buffer *buffer= parse_in->buffer;
   if(!buffer)
      return in_fnames[0];//NULL;
   struct _cpp_file *file=buffer->file;
   if(!file)
      return in_fnames[0];//NULL;
   const char *fileName=_cpp_get_file_name (file);
   return fileName;
}


static void classParserInit(ClassParser *self)
{
   initAddressDiagnosticCallback();
   self->classCtor=class_ctor_new();
   self->classInterface=class_interface_new();
   self->classInit=class_init_new();
   self->classFinalize=class_finalize_new();
   self->classFinal=class_final_new();
   self->classPackage=class_package_new();
   self->classPermission=class_permission_new();
   self->classBuild=class_build_new();//AClass中的变量赋值代码生成器
   self->superCall=super_call_new();
   self->state=CLASS_STATE_STOP;
   self->currentClassName=NULL;
   n_log_set_param_from_env();
   n_log_set_compile_file_callback(nLogGetCompileFileAndFunc_cb);
   n_log_enter_front();
   self->expandMemory = FALSE;
   self->currentFuncModel=NULL;
   self->addAetHeader.added=FALSE;
   self->addAetHeader.backTokenCount=0;
   self->addAetHeader.running=FALSE;

}

/**
 * 把 getName( 重整为  getName(Abc *self
 */
static void addSelfToField(c_parser *parser,ClassName *className,int openParenPos)
{
   c_token *who=c_parser_peek_nth_token (parser,openParenPos+1);
   c_token *who1=c_parser_peek_nth_token (parser,openParenPos+2);
   int tokenCount=parser->tokens_avail;
   int i;
   nboolean dhaoAfter=FALSE;//逗号加后面还是前面
   if (who->keyword == RID_VOID  && who1->type == CPP_CLOSE_PAREN){
      //printf("class_parser 进这里了---000 who->keyword == RID_VOID  && who1->type == CPP_CLOSE_PAREN %d openParenPos:%d\n",tokenCount,openParenPos);
      //需要加2个token 因为 void被替换了
      //getName(void) getName 1 ( 2 void 3 )4
      parser->tokens_avail=tokenCount+2;
      for(i=tokenCount;i>openParenPos+1;i--){
         aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
      }
   }else{//()->(Abc *self)
      //需要加2个token 因为 void被替换了
      int offset=(who->type == CPP_CLOSE_PAREN)?3:4;
      //printf("class_parser 进这里了---111 who->type == CPP_CLOSE_PAREN %d openParenPos:%d offset:%d\n",tokenCount,openParenPos,offset);
      parser->tokens_avail=tokenCount+offset;
      for(i=tokenCount;i>openParenPos;i--){
         aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+offset]);
      }
      if(offset==4){
         aet_utils_create_token(&parser->tokens[openParenPos+3],CPP_COMMA,",",1);
         dhaoAfter=TRUE;
      }
   }
   aet_utils_create_token(&parser->tokens[openParenPos+2],CPP_NAME,"self",4);
   aet_utils_create_token(&parser->tokens[openParenPos+1],CPP_MULT,"*",1);
   aet_utils_create_token(&parser->tokens[openParenPos],CPP_NAME,className->sysName,strlen(className->sysName));
   parser->tokens[openParenPos].id_kind=C_ID_TYPENAME;//关键
   aet_print_token_in_parser("class_parser rearrangeMode className ---- sysName:%s userName:%s ",className->sysName,className->userName);
}

static void rearrangeMode(c_parser *parser,ClassName *className,int openParenPos)
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
   //第二步 把 (*getName(变成 (*getName)(
   openParenPos+=2;
   tokenCount=parser->tokens_avail;
   for(i=tokenCount;i>openParenPos-1;i--){
      aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
   }
   aet_utils_create_token(&parser->tokens[openParenPos-1],CPP_CLOSE_PAREN,")",1);
   parser->tokens_avail=tokenCount+1;
   openParenPos+=1;
   addSelfToField(parser,className,openParenPos);
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
 * gen是变量声明的泛型
 * 如果decl是函数，就是函数返回值的泛型
 * funcGen是泛型函数
 */
static void checkAndSetGeneric(ClassParser *self,tree decl,GenericModel *gen,GenericModel *funcGen,
      nboolean isFunc,struct c_declarator *declarator)
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
         n_debug("checkAndSetGeneric 11 field_decl是一个泛型函数 %s:decl:%p funcGeneric:%p gen：%p\n",
               IDENTIFIER_POINTER(DECL_NAME(decl)),decl,funcGen,gen);
         c_aet_set_func_generics_model(decl,funcGen);
      }
      generic_impl_check_func_at_field(generic_impl_get(),decl,declarator);
   }else{
      nboolean  ok= generic_impl_check_var(generic_impl_get(),decl,gen);
      n_debug("checkAndSetGeneric 22 检查带泛型的类变量 ok:%d\n",ok);
   }
}

/**
 * setData(Abc<?> *abc) ---->setData(Efg *self,Abc<?> *abc);
 * 专门分析结构体内的内容
 * struct中是不允许static关键字的，c_parser_declspecs第三个参数
 * bool scspec_ok原来是false 为了支持static 现在改为true
 */
static tree  c_parser_class_declaration (ClassParser *self,ClassInfo *classInfo,
      tree structTree,ClassType classType,int *isStatic)
{
   c_parser *parser=self->parser->parser;
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
      ext = aet_parser_disable_extension_diagnostics/*!disable_extension_diagnostics*/();
      c_parser_consume_token (parser);
      decl = c_parser_class_declaration (self,classInfo,structTree,classType,isStatic);
      aet_parser_restore_extension_diagnostics/*!restore_extension_diagnostics*/(self->parser,ext);
      n_debug("新----struct内的声明 ---00 RID_EXTENSION 返回 ");
      return decl;
   }
   if (c_parser_next_token_is_keyword (parser, RID_STATIC_ASSERT)){
      aet_parser_c_parser_static_assert_declaration_no_semi/*!c_parser_static_assert_declaration_no_semi*/(self->parser);
      n_debug("新----struct内的声明 ---11 RID_STATIC_ASSERT 返回 ");
      return NULL_TREE;
   }
   aet_print_token(c_parser_peek_token (parser));
   FieldDecorate dr=class_permission_try_parser(self->classPermission,classType);
   if(dr.isStatic){
      if(c_parser_next_token_is_keyword (parser, RID_AET_ENUM)){
         n_debug("新----struct内的声明 static enum$ sysClassName:%s", className->sysName);
         //在class xx{
             //static enum$ YYY
         //}
         //当作是 enum$ YYY 处理，相当于static修饰无用 由 c_parser_declspecs解析关键字 enum$
         dr.isStatic=FALSE;
      }else{
         n_debug("新----struct内的声明 是一个静态声明明符 11 编完class$后再处理 sysClassName:%s permission:%d", className->sysName,dr.permission);
         if(isStatic)
            *isStatic=1;
         parser_static_compile(parser_static_get(),classInfo->className.sysName,dr.permission,dr.isFinal);
         return NULL_TREE;
      }
   }
   n_debug("class_permission_set_decorate 11 %s fileName:%s state:%d permission:%d",
         self->currentClassName->sysName,self->fileName,self->state,dr.permission);
   class_permission_set_decorate(self->classPermission,&dr);
   if(c_parser_next_token_is (parser, CPP_NAME)){
      aet_print_token(c_parser_peek_token (parser));
      char *str=IDENTIFIER_POINTER(c_parser_peek_token (parser)->value);
      location_t loc=c_parser_peek_token (parser)->location;
      nboolean isMtcsAttr=mtcs_parser_is_attribute(mtcs_parser_get(),c_parser_peek_token (parser));
      if(isMtcsAttr){
         n_debug("class_permission_set_decorate 11 mtcs_types_init");
         mtcs_parser_add_attribute(mtcs_parser_get());
      }
      needCheckConstructors=!(class_ctor_parser_constructor(self->classCtor,className));
      if(!needCheckConstructors && isMtcsAttr){
          error_at(loc,"构造函数不能声明为核函数或设备函数");
      }
      c_token *token=c_parser_peek_token (parser);
      n_debug("新----struct内的声明 是一个泛型吗------00 needCheckConstructors:%d %s\n",needCheckConstructors,IDENTIFIER_POINTER(token->value));
      if(generic_util_valid_id(token->value))
         generic_impl_replace_token(generic_impl_get(),token);
   }else if(c_parser_next_token_is (parser, CPP_COMPL)){
      class_finalize_parser(self->classFinalize,className);
   }
   //检查是不是泛型方法
   //格式如下：public$ <T> void getName(); <T>在函数返回值之前。
   if(c_parser_next_token_is (parser, CPP_LESS)){
      funcGenericModel=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
      self->currentFuncModel=funcGenericModel;
      n_debug("新----struct内的声明 可以是一个泛型函数。");
      if(c_parser_next_token_is (parser, CPP_NAME)){
         char *str=IDENTIFIER_POINTER(c_parser_peek_token (parser)->value);
         location_t loc=c_parser_peek_token (parser)->location;
         nboolean isMtcsAttr=mtcs_parser_is_attribute(mtcs_parser_get(),c_parser_peek_token (parser));
         if(isMtcsAttr){
              mtcs_parser_add_attribute(mtcs_parser_get());
         }
         needCheckConstructors=!(class_ctor_parser_constructor(self->classCtor,className));
         if(!needCheckConstructors && isMtcsAttr){
            error_at(loc,"构造函数不能声明为核函数或设备函数");
         }
         c_token *token=c_parser_peek_token (parser);
         n_debug("新----struct内的声明 是一个泛型吗------11 %s\n",IDENTIFIER_POINTER(token->value));
         if(generic_util_valid_id(token->value))
            generic_impl_replace_token(generic_impl_get(),token);
      }else if(c_parser_next_token_is (parser, CPP_COMPL)){
         class_finalize_parser(self->classFinalize,className);
      }
   }

   specs = build_null_declspecs ();
   decl_loc = c_parser_peek_token (parser)->location;
   n_debug("新----struct内的声明 00 建立空的声明说明符 sysClassName:%s", className->sysName);
   aet_print_token(c_parser_peek_token (parser));
   //在这里匹配的是public$  __global__  int   setData(void *data);
   if(mtcs_parser_is_attribute(mtcs_parser_get(),c_parser_peek_token (parser))){
      n_debug("新----struct内的声明 token 是一个MTCS 属性 sysClassName:%s", className->sysName);
      mtcs_parser_add_attribute(mtcs_parser_get());
   }
   aet_parser_c_parser_declspecs/*!c_parser_declspecs*/(self->parser,
         specs, false, true, true,true, false, true, true, cla_nonabstract_decl);
   if (parser->error)
      return NULL_TREE;
   if (!specs->declspecs_seen_p){
      c_parser_error (parser, "expected specifier-qualifier-list");
      return NULL_TREE;
   }
   //在这里匹配的是public$  int  __global__  setData(void *data);
   if(mtcs_parser_is_attribute(mtcs_parser_get(),c_parser_peek_token (parser))){
        n_debug("新----struct内的声明 xxx token 是一个MTCS 属性 sysClassName:%s", className->sysName);
        aet_print_token(c_parser_peek_token (parser));
        mtcs_parser_add_attribute(mtcs_parser_get());
        aet_parser_c_parser_declspecs/*!c_parser_declspecs*/(self->parser,
              specs, false, true, true,true, false, true, true, cla_nonabstract_decl);
   }
   finish_declspecs (specs);
   n_debug("新----struct内的声明 11 到这里specs已有声明说明符了比如 int 完成finish_declspecs %p",specs);
   class_parser_complete_enum(self,specs,TRUE,dr.permission,className);
   //    if(TREE_CODE(specs->type)==ENUMERAL_TYPE){
   //         if(specs->typespec_kind==ctsk_tagdef){
   //             n_debug("为类中的枚举加入类型声明 %s\n",className->sysName);
   //             enum_parser_create_decl(enum_parser_get(),decl_loc,className,specs,dr.permission);
   //        }
   //    }
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
      //n_debug("新----struct内的声明 --11--把函数声明'int * getName('重整为'int *(*getName)( ... );
      /*匹配 * getName(*/
      checkConstructor(parser,c_parser_peek_2nd_token(parser),className,needCheckConstructors);
      rearrangeMode(parser,className,3);
   }else  if(c_parser_next_token_is (parser, CPP_NAME) && c_parser_peek_2nd_token(parser)->type==CPP_OPEN_PAREN){
      //n_debug("新----struct内的声明 -22-- 把函数声明'int  getName('重整为'int (*getName)( ...  );
      /*匹配 getName(*/
      checkConstructor(parser,c_parser_peek_token(parser),className,needCheckConstructors);
      rearrangeMode(parser,className,2);
   }else if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_MULT &&
      c_parser_peek_nth_token(parser,3)->type==CPP_NAME
      && !c_token_starts_typename(c_parser_peek_nth_token(parser,3)) && c_parser_peek_nth_token(parser,4)->type==CPP_OPEN_PAREN){
      //n_debug("新----struct内的声明 --33--把函数声明'int ** getName('重整为'int *(*getName)(  ...  );
      checkConstructor(parser,c_parser_peek_nth_token(parser,3),className,needCheckConstructors);
      /*匹配 ** getName(*/
      rearrangeMode(parser,className,4);
   }else if(c_parser_next_token_is (parser, CPP_MULT) && c_parser_peek_2nd_token(parser)->type==CPP_MULT
      && c_parser_peek_nth_token(parser,3)->type==CPP_MULT
      && c_parser_peek_nth_token(parser,4)->type==CPP_NAME
      && !c_token_starts_typename(c_parser_peek_nth_token(parser,4)) && c_parser_peek_nth_token(parser,5)->type==CPP_OPEN_PAREN){
      //n_debug("新----struct内的声明 --33--把函数声明'int ** getName('重整为'int **(*getName)(...);
      checkConstructor(parser,c_parser_peek_nth_token(parser,3),className,needCheckConstructors);
      /*匹配 ** getName(*/
      rearrangeMode(parser,className,5);
   }

   if (c_parser_next_token_is (parser, CPP_SEMICOLON) || c_parser_next_token_is (parser, CPP_CLOSE_BRACE)){
      //n_debug("新----struct内的声明 22 如果是CPP_SEMICOLON或CPP_CLOSE_BRACE 返回 ");
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
            self->currentFuncModel =NULL;
            return NULL_TREE;
         }
         ret = grokfield (c_parser_peek_token (parser)->location,build_id_declarator (NULL_TREE), specs,NULL_TREE, &attrs,NULL);
         if (ret)
            decl_attributes (&ret, attrs, 0);
      }
      self->currentFuncModel =NULL;
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
         aet_print_token(c_parser_peek_token (parser));
         declarator = aet_parser_c_parser_declarator/*!c_parser_declarator*/(self->parser,
               specs->typespec_kind != ctsk_none,C_DTR_NORMAL, &dummy);
         n_debug("新----struct内的声明 66aa declarator:%p\n", declarator);

      }
      if (declarator == NULL){
         n_debug("新----struct内的声明 77 出错了 break count:%d", testcount);
         aet_parser_c_parser_skip_to_end_of_block_or_statement/*!c_parser_skip_to_end_of_block_or_statement*/(self->parser);
         break;
      }
      //原型c-parser.c中有处理CPP_EQ,这里不需要
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
            width = aet_parser_c_parser_expr_no_commas/*!c_parser_expr_no_commas*/(self->parser, NULL).value;
         }
         if(c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE))
            postfix_attrs = aet_parser_c_parser_gnu_attributes/*!c_c_parser_gnu_attributes*/(self->parser);
         nboolean isQueryGenFunc=generic_query_is_function_declarator(generic_query_get(),className,declarator);
         if(!funcGenericModel && isQueryGenFunc){
            generic_query_add_parm_to_declarator(generic_query_get(),declarator);
         }
         struct c_arg_info *temp = declarator->u.arg_info;
         n_debug("新----struct内的声明 88 并在这里改名 grokfield className:%s isQueryFunction:%d structTree:%p\n",
               className->sysName,isQueryGenFunc,structTree);
         int errInfo=0;
         ClassFunc *newFunc=func_mgr_change_class_func_decl(func_mgr_get(),declarator,className,structTree,&errInfo);
         if(newFunc && errInfo==-2){
            n_debug("在类中声明与接口重复的函数了----%s\n",className->sysName);
            if(self->state==CLASS_STATE_ABSTRACT){
               newFunc->isAbstract=TRUE;
            }else{
               error_at(decl_loc,"类%qs声明了与接口重复的函数名%qs。",className->userName,newFunc->orgiName);
            }
            self->currentFuncModel =NULL;//到这里函数参数已解析完成了。
            return NULL_TREE;
         }
         if(!newFunc){
            n_debug("不是函数是变量 说明符中有静态变量替换常量 %s\n",className->sysName);
            parser_static_replace_dimension(parser_static_get(),declarator);
         }

         d = grokfield (c_parser_peek_token (parser)->location,declarator, specs, width, &all_prefix_attrs,NULL);
         decl_attributes (&d, chainon (postfix_attrs,all_prefix_attrs), 0);
         checkAndSetGeneric(self,d, fieldGenericModel,funcGenericModel,(newFunc && errInfo==0),declarator);
         DECL_CHAIN (d) = decls;
         decls = d;
         n_debug("新----struct内的声明 88 并在这里改名 newFunc:%p",newFunc);

         if(newFunc && errInfo==0)
            class_func_set_decl(newFunc,decls,STRUCT_DECL);
         else{//说明是变量
            char *varName=IDENTIFIER_POINTER(DECL_NAME(decls));
            if(isIface && strcmp(varName,IFACE_COMMON_STRUCT_VAR_NAME) && strcmp(varName,AET_MAGIC_NAME)){
               error_at(decl_loc,"接口中不能声明或定义变量。%qD",decls);
               self->currentFuncModel =NULL;//到这里函数参数已解析完成了。
               return NULL_TREE;
            }
            var_mgr_add(var_mgr_get(),className,decls);
         }
         if(c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE))
            all_prefix_attrs = chainon (aet_parser_c_parser_gnu_attributes/*!c_c_parser_gnu_attributes*/(self->parser),prefix_attrs);
         else
            all_prefix_attrs = prefix_attrs;
         if (c_parser_next_token_is (parser, CPP_COMMA))
            c_parser_consume_token (parser);
         else if (c_parser_next_token_is (parser, CPP_SEMICOLON) || c_parser_next_token_is (parser, CPP_CLOSE_BRACE)){
            /* Semicolon consumed in caller.  */
            n_debug("新----struct内的声明 99 下一个 token 是CPP_SEMICOLON or CPP_CLOSE_BRACE 退出循环 count:%d", testcount);
            break;
         }else{
            c_parser_error (parser, "expected %<,%>, %<;%> or %<}%>");
            break;
         }
      }else{
         c_parser_error (parser,"expected %<:%>, %<,%>, %<;%>, %<}%> or %<__attribute__%>");
         n_debug("新----struct内的声明 100 出错了 count:%d", testcount);
         break;
      }
   }//end while
   self->currentFuncModel =NULL;//到这里函数参数已解析完成了。
   n_debug("新----struct内的声明 101 over count:%d ", testcount);
   return decls;
}

static void parserExentdsAndImplements(ClassParser *self,ClassInfo *classInfo,char **parent,char **impls,int *implCount)
{
   c_parser *parser=self->parser->parser;
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
               error_at(start_loc,"关键字extends$只能有一个。");
               return;
            }
            if(implments==1){
               error_at(start_loc,"关键字implments$应在extends$后。");
               return;
            }
            c_parser_consume_token (parser);
            if(!c_parser_next_token_is (parser, CPP_NAME)){
               error_at(start_loc,"关键字extends$后应该是类名。");
               return;
            }
            extends=1;
            tree id=c_parser_peek_token(parser)->value;
            location_t parentLoc=c_parser_peek_token(parser)->location;
            c_parser_consume_token (parser);
            *parent=n_strdup(IDENTIFIER_POINTER(id));
            class_info_set_parent_location(classInfo,parentLoc);
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
            error_at(start_loc,"class$ Xxx extends$ Yyy格式");
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
   c_parser *parser=self->parser->parser;
   c_token *openBrace=c_parser_peek_token(parser); //"{"
   location_t loc=openBrace->location;
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
   parser->tokens[offset].location=loc;
   offset++;
   char *name=class_util_create_parent_class_var_name(parent->userName);
   aet_utils_create_token(&parser->tokens[offset],CPP_NAME,name,(int)strlen(name));
   parser->tokens[offset].location=loc;
   n_free(name);
   offset++;
   aet_utils_create_token(&parser->tokens[offset],CPP_SEMICOLON,";",1);
   parser->tokens[offset].location=loc;
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
      parser->tokens[offset].location=loc;
      //检查是不存在该类型
      offset++;
      char name[255];
      aet_utils_create_in_class_iface_var(ifaces[i]->userName,name);
      aet_utils_create_token(&parser->tokens[offset],CPP_NAME,name,(int)strlen(name));
      parser->tokens[offset].location=loc;
      offset++;
      aet_utils_create_token(&parser->tokens[offset],CPP_SEMICOLON,";",1);
      parser->tokens[offset].location=loc;
      offset++;
   }
   parser->tokens_avail=tokenCount+offset-1;
   if(move1){
      aet_utils_copy_token(move1,&parser->tokens[offset]);
      parser->tokens[offset].location=loc;
      offset++;
   }
   if(move2){
      aet_utils_copy_token(move2,&parser->tokens[offset]);
      parser->tokens[offset].location=loc;
      offset++;
   }
   aet_print_token_in_parser("add parent and interface---- parent %s",parent->sysName);
}

static void addExentdsAndImplements(ClassParser *self,ClassInfo *classInfo)
{
   c_parser *parser=self->parser->parser;
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
            error_at(start_loc,"没有引入父类%qs所在的头文件",parent);
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

static void printClass(tree record)
{
   tree field;
   tree fieldList=TYPE_FIELDS(record);
   for (field = fieldList; field; field = DECL_CHAIN (field)){
      tree name=DECL_NAME(field);
      if(!name){
         n_warning("类成员变量重复。");
         return;
      }
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

#define CREATE_CLASS_INIT_FUNC_TOKEN 1

/**
 * 当类声明结束，跳转到静态变量或函数以及类初始化函数声明;
 * ClassInit.c中的方法class_init_create_init_decl,在该方法内还要触发创建super初始化的函数声明。
 */
static void gotoStaticAndInit(ClassParser *self,location_t loc,char *sysName)
{
   c_parser *parser=self->parser->parser;
   c_token *semicolon = c_parser_peek_token (parser);//
   int addTokenCount=4;
   int tokenCount=parser->tokens_avail;
   if(tokenCount+addTokenCount>AET_MAX_TOKEN){
      error("token太多了");
      return FALSE;
   }
   int i;
   for(i=tokenCount;i>1;i--){
      aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+addTokenCount]);
   }
   aet_utils_create_aet_goto_token(&parser->tokens[1],input_location);
   aet_utils_create_number_token(&parser->tokens[2],GOTO_STATIC_VAR_FUNC);
   aet_utils_create_number_token(&parser->tokens[3],CREATE_CLASS_INIT_FUNC_TOKEN);
   aet_utils_create_string_token(&parser->tokens[4],sysName,strlen(sysName));
   for(i=1;i<=addTokenCount;i++)
      parser->tokens[i].location=loc;
   parser->tokens_avail=tokenCount+addTokenCount;
   aet_print_token_in_parser("gotoStaticAndInit ---- %s",sysName);
}


/**
 *解析类或接口声明，即 class$ Abc{ 或 interface$ Abc{
 */
struct c_typespec class_parser_parser_class_specifier (ClassParser *self)
{
   c_parser *parser=self->parser->parser;
   enter_aet=1;//进入了aet
   if(!self->expandMemory){
      param_ggc_min_expand = param_ggc_min_expand*4;
      param_ggc_min_heapsize = param_ggc_min_heapsize*4;
      self->expandMemory = TRUE;
   }

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
   struct_loc = c_parser_peek_token (parser)->location;
   c_parser_consume_token (parser);//消耗class 或Interface
   ident_loc = c_parser_peek_token (parser)->location; //class$ Abc 是 Abc的位置
   //printf("print indec-----\n");
  // aet_print_location(ident_loc);

   //原设计 返回是0
   have_std_attrs = aet_parser_c_parser_nth_token_starts_std_attributes/*!c_c_parser_nth_token_starts_std_attributes*/(self->parser, 1);
   if (have_std_attrs)
      std_attrs =aet_parser_c_parser_std_attribute_specifier_sequence/*!c_c_parser_std_attribute_specifier_sequence*/(self->parser);
   attrs =aet_parser_c_parser_gnu_attributes/*!c_c_parser_gnu_attributes*/(self->parser);//设计 返回是NULL_TREE
   n_debug("新 ---分析struct 00  have_std_attrs:%d---attrs==NULL_TREE:%d ",have_std_attrs,attrs==NULL_TREE);
   /* Set the location in case we create a decl now.  */
   aet_parser_c_parser_set_source_position_from_token/*!c_parser_set_source_position_from_token*/(c_parser_peek_token (parser));

   if (c_parser_next_token_is (parser, CPP_NAME)){
      ident = c_parser_peek_token (parser)->value;
      char sysClassName[255];//没有下划线的类名
      memcpy(sysClassName,IDENTIFIER_POINTER(ident)+1,IDENTIFIER_LENGTH(ident)-1);//去除下划线
      sysClassName[IDENTIFIER_LENGTH(ident)-1]='\0';
      c_parser_consume_token (parser);
      FieldDecorate dr= class_permission_get_decorate_by_class(self->classPermission,sysClassName,classType);
      n_debug("class_permission_stop 00 sysClassName:%s classType:%d isFinal:%d %d\n",sysClassName,classType,dr.isFinal,dr.permission);
      //printf("print indec-----22 \n");
     // aet_print_location(ident_loc);
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
         //检查类声明中是否有具体的泛型实现。
         generic_model_class_or_func_decl_check(model);
         class_info_set_generic_model(classInfo,model);
      }
      addExentdsAndImplements(self,classInfo);
   }
   if(classInfo==NULL){
      c_parser_error (parser, "类型不正确。classInfo是空的");
      return ret;
   }
   //检查泛型声明是否正确。
   class_info_check_generic_decl(classInfo);

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
      tree contents = NULL_TREE;
      location_t loc = c_parser_peek_token (parser)->location;
      c_parser_consume_token (parser);//consume {
      aet_print_token(c_parser_peek_token (parser));
      //加一个魔数变量作为类或接口的第一个变量  4 个token
      if(class_info_is_root(classInfo))
         parser_help_add_magic(loc);
      //如果是接口加入一个变量和两个方法   14个token
      if(classType==CLASS_TYPE_INTERFACE){
         n_debug("是接口声明:%s\n",classInfo->className.sysName);
         class_interface_add_var_ref_unref_method(self->classInterface,loc);
      }
      //加入所有接口的方法声明到这里
      /*
      if(classType!=CLASS_TYPE_INTERFACE && classInfo->ifaceCount>0){
         tree classMethods[100];
         int interfaceMethodCount=class_interface_add_to_class(self->classInterface,loc,classInfo,classMethods);
         int i;
         for(i=0;i<interfaceMethodCount;i++)
            contents = chainon (classMethods[i], contents);
      }
      */

      /* Parse the struct-declarations and semicolons.  Problems with
      semicolons are diagnosed here; empty structures are diagnosed
      elsewhere.  */
      self->state=CLASS_STATE_FIELD;//CLASS的状态是进入field
      while (true){
         tree decls;
         /* Parse any stray semicolon.  */
         if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
            n_debug("新 ---分析struct 33 下一个token是CPP_SEMICOLON  have_std_attrs:%d--- tokensavail:%d ",have_std_attrs,parser->tokens_avail);
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
            //在这里加入保存field函数实现的地址
            if(classType!=CLASS_TYPE_INTERFACE){
               location_t loc = c_parser_peek_token (parser)->location;
               if(class_info_is_root(classInfo)){
                  //加入变量 创建对象是源代码像这样 new$ Abc<<<gcn>>>() mtcs平台类型gcn的值就保存在变量mtcsPlatformType中
                  tree mtcsPlatformType=mtcs_parser_create_platform_type_var(mtcs_parser_get(),loc);
                  contents = chainon (mtcsPlatformType, contents);
               }
            }
            c_parser_consume_token (parser);
            break;
         }
         /* Parse some comma-separated declarations, but not the
         trailing semicolon if any.  */
         n_debug("新 ---分析struct 55 应该在这里mangle 如果结构体内没内容不会进这里，下一个token是什么? --- global_bindings_p() %d ",global_bindings_p());
         location_t loc=c_parser_peek_token (parser)->location;
         int isStatic=0;
         aet_print_token(c_parser_peek_token (parser));

         decls = c_parser_class_declaration(self,classInfo,type,classType,&isStatic);
         if(decls==NULL_TREE){//如果是一个枚举，并且没有声明变量，就会进这里
            class_permission_stop(self->classPermission);
            continue;
         }
         n_debug("新 ---分析struct 55-1 检查MTCS函数");
         mtcs_parser_check(mtcs_parser_get(),decls,TRUE);

         if(isStatic==0){
            contents = chainon (decls, contents);
            nboolean result=setFuncAbstractQual(self,decls,&classInfo->className,classType,loc);
            class_permission_set_field_decorate(self->classPermission,decls,&classInfo->className);
            class_permission_stop(self->classPermission);
            self->state=CLASS_STATE_FIELD;
            //正在解析的classInfo是接口，发现在接口中声明decls是 host device 方法，分裂decls，并加入分裂方法到接口中。
            if(class_interface_is_host_device_func(self->classInterface,classInfo,decls)){
               tree newdecl=class_interface_divide_host_device_func(self->classInterface,loc,classInfo,decls);
               if(newdecl)
                  contents = chainon (newdecl, contents);
            }else{
               //在类中声明的方法 decls 是 host device 方法,分裂该decls，并加入新的分裂方法到接口中。
               ClassFunc *func= func_mgr_get_func(func_mgr_get(),decls);
               if(func!=NULL && class_func_is_device(func) && class_func_is_host(func) && classType!=CLASS_TYPE_INTERFACE){
                  printf("类中声明的主机设备的方法-------:%s\n",func->orgiName);
                  if(class_func_is_device(func) && class_func_is_host(func)){
                     tree divide= func_mgr_divide_host_device_func(func_mgr_get(),loc,classInfo,decls);
                     if(divide)
                        contents = chainon (divide, contents);
                  }
               }
            }

            if(!result)
               break;
         }else{
            n_info("新 ---分析struct ---55-2 在类:的静态成员 永远不会进这里 isStatic==1 时 decls==NULL_TREE \n");
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

      //加入接口方法到类中,原本放在while前，引发BUG
      if(classType!=CLASS_TYPE_INTERFACE && classInfo->ifaceCount>0){
         tree classMethods[100];
         int interfaceMethodCount=class_interface_add_to_class(self->classInterface,loc,classInfo,classMethods);
         int i;
         for(i=0;i<interfaceMethodCount;i++)
            contents = chainon (classMethods[i], contents);
      }

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
       //生成两个类变量 _generic_1234_array 保存泛型类的泛型类弄 和 _gen_blocks_array_897 非泛型函数的泛型块指针
       //泛型函数中的泛型块用AObject.h中声明的全局变量 __thread generic_func_block_addr;
      int genericCount=class_info_get_generic_count(classInfo);
      if((genericCount>0 || func_mgr_have_generic_func(func_mgr_get(),&classInfo->className))
            && !class_info_is_interface(classInfo)){
         n_info("加一个aet_generic_info数组到类中，保存声明或定义的泛型模型信息");
         tree  genericArray=generic_impl_create_generic_info_array_field(generic_impl_get(),&classInfo->className, genericCount);
         contents = chainon (genericArray, contents);
         var_mgr_add(var_mgr_get(),&classInfo->className,genericArray);

         tree blockFuncs=generic_impl_create_generic_block_array_field(generic_impl_get());//void *blockFuncs[30];
         aet_print_tree(blockFuncs);
         contents = chainon (blockFuncs, contents);
         var_mgr_add(var_mgr_get(),&classInfo->className,blockFuncs);
      }

      postfix_attrs = aet_parser_c_parser_gnu_attributes/*!c_c_parser_gnu_attributes*/(self->parser);
      ret.spec = finish_struct(struct_loc, type, nreverse (contents),chainon (std_attrs,chainon (attrs, postfix_attrs)),struct_info);
      n_debug("新 ---分析struct 77 完成finish_struct %s %p",IDENTIFIER_POINTER(TYPE_NAME (ret.spec)),ret.spec);
      class_mgr_set_record(class_mgr_get(),&classInfo->className,ret.spec);
      printClass(ret.spec);
      ret.kind = ctsk_tagdef;
      ret.expr = NULL_TREE;
      ret.expr_const_operands = true;
      if (!c_parser_next_token_is (parser, CPP_SEMICOLON)){
         c_parser_error (parser, "类声明以分号结束");
      }else{
         /**
         * 加入初始化函数声明 Abc *Abc_AET_INIT_GLOBAL_METHOD_STRING_Abc(Abc *self);
         * c_parser_peek_token (parser)必须是;号。否则结构体声明不能结束
         */
         gotoStaticAndInit(self,input_location,classInfo->className.sysName);
      }
      class_build_replace_getclass_rtn(self->classBuild,self->currentClassName);
      //如果是接口，并且声明所在.c文件，加入实现代码。
      self->state=CLASS_STATE_STOP;
      self->currentClassName=NULL;
      return ret;
   }else if (!ident) {
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
   ret = parser_xref_tag (ident_loc, code, ident, have_std_attrs, std_attrs,FALSE);
   self->state=CLASS_STATE_STOP;
   self->currentClassName=NULL;
   return ret;
}

static char *createClassName(ClassParser *self,char *origName,char **package)
{
   cpp_buffer *buffer= parse_in->buffer;
   struct _cpp_file *file=buffer->file;
   const char *fileName=_cpp_get_file_name (file);
   cpp_dir *dir=cpp_get_dir (file);
   char *classPrefix=class_package_get_class_prefix(self->classPackage,fileName,dir->name);
   char *className=NULL;
   if(classPrefix!=NULL){
      className=n_strdup_printf("%s_%s",classPrefix,origName);
      *package=n_strdup(classPrefix);
   }else
      className=n_strdup(origName);
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
   c_parser *parser=self->parser->parser;
   parser_help_set_forbidden(TRUE);
   c_token *classSpec = c_parser_peek_token (parser);//"Class"
   classSpec->id_kind=C_ID_CLASSNAME;/*这是一个指示看	c-parser.c中的case RID_AET_CLASS:*/
   c_token *classNameToken=c_parser_peek_2nd_token(parser); //Abc
   parser_help_set_forbidden(FALSE);
   location_t orgLoc=classNameToken->location;//classSpec->location;
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
   //缺少token的其它信息导致找不到class
   tree sysValue=aet_utils_create_ident(sysClassName);
   parser->tokens[3].value=sysValue;
   aet_utils_create_token(&parser->tokens[2],CPP_NAME,buff,strlen(buff));
   aet_utils_create_struct_token(&parser->tokens[1],orgLoc);
   aet_utils_create_typedef_token(&parser->tokens[0],orgLoc);
   parser->tokens_avail=tokenCount+5;
   for(i=2;i<7;i++)
      parser->tokens[i].location=orgLoc;
   aet_print_token_in_parser("class_parser_replace ---- %s  %s package:%s sysClassName:%s",userClassName,buff,package,sysClassName);
   class_mgr_add(class_mgr_get(),sysClassName,userClassName,package);
   n_free(userClassName);
   n_free(sysClassName);
   if(package)
      n_free(package);
}

void class_parser_abstract_keyword(ClassParser *self)
{
   c_parser *parser=self->parser->parser;
   location_t  loc = c_parser_peek_token (parser)->location;
   if(self->parser->isAet){
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
   class_final_parser (self->classFinal,self->state,specs);
}

/**
 * 解析关键字public$ final$ static 等
 * 然后设状态为运行,等分析完class或interface后，再通过调用stop把运行状态变为FALSE
 */
void   class_parser_decorate(ClassParser *self)
{
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
   c_parser *parser=self->parser->parser;
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
      tok = c_parser_peek_token (parser);
      wide_int aw=wi::to_wide(tok->value);
      int action=aw.to_shwi();
      c_parser_consume_token (parser);//consume   GOTO_STATIC_VAR_FUNC
      if(action==CREATE_CLASS_INIT_FUNC_TOKEN){
         c_token *sysNameToken = c_parser_peek_token (parser);
         const char *sysName  = TREE_STRING_POINTER (sysNameToken->value);
         c_parser_consume_token (parser);//consume   "test_AObject"
         ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
         n_debug("class_parser_goto GOTO_STATIC_VAR_FUNC 编译类声明中的初始化部分:%s %p", sysName,info);
         class_init_create_init_decl(self->classInit,tok->location,&info->className);
         super_call_create_init_func_decl(self->superCall,tok->location,&info->className);
         //接口声明在.c文件中，直接实现静态变量和初始化函数
         if(class_info_is_interface(info) && endswith(class_info_get_file(info),".c")){
            n_debug("class_parser_goto GOTO_STATIC_VAR_FUNC iface_impl_compile_at_cfile:%s %p", sysName,info);
            iface_impl_compile_at_cfile(iface_impl_get(),info);
         }
      }
      return FALSE;
   }else if(pos==GOTO_CHECK_FUNC_DEFINE){
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_CHECK_FUNC_DEFINE
      tok = c_parser_peek_token (parser);
      wide_int aw=wi::to_wide(tok->value);
      int compileType=aw.to_shwi();
      c_parser_consume_token (parser);//consume   compileType
      n_debug("GOTO_CHECK_FUNC_DEFINE --compileType:%d COMPILE_IFACE_IMPL_CHECK:%d COMPILE_IFACE:%d block new %d\n",
            compileType,  (compileType & COMPILE_IFACE_IMPL_CHECK),(compileType & COMPILE_IFACE),
            ((compileType & COMPILE_BLOCK) || (compileType & COMPILE_NEW)));
      //进入这里属于编译temp_func_track_45.c 主要靠gcc.cc中传递的参数获取在aetcollect中收集的数据
      if(compileType & COMPILE_IFACE_IMPL_CHECK)
         middle_file_func_check(middle_file_get());
      if(compileType & COMPILE_IFACE)
         iface_impl_compile_ready(iface_impl_get());
      if((compileType & COMPILE_BLOCK) || (compileType & COMPILE_NEW)){
          generic_graph_ready(generic_graph_get());//通过对象可达性算法，生成输入泛型对象
          block_mgr_ready(block_mgr_get());
          generic_code_create_block_codes(generic_code_get());
          //创建的全局变量 LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX 变量的初始值是泛型相关的数据
          middle_file_create_global_var(middle_file_get());
      }
      if(compileType&COMPILE_MTCS_LINK)
         mtcs_parser_link_func(mtcs_parser_get());
      return FALSE;
   }else if(pos==GOTO_IFACE_COMPILE){
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_IFACE_COMPILE
      c_token *sysNameToken = c_parser_peek_token (parser);
      tree string=sysNameToken->value;
      const char *tokenString  = TREE_STRING_POINTER (string);
      char *sysNames=tokenString;
      c_parser_consume_token (parser);//consume   "test_Iface1,test_Iface2"
      n_debug("GOTO_IFACE_COMPILE --正在编译接口文件 %s %s\n",sysNames,in_fnames[0]);
      iface_impl_compile(iface_impl_get(),sysNames);
      return FALSE;
   }else if(pos==GOTO_MTCS_CREATE_OBJ){
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_MTCS_CREATE_OBJ
      *action=GOTO_MTCS_CREATE_OBJ;
   }else if(pos==GOTO_ADD_H_FILE){ //17是加头文件
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_MTCS_CREATE_OBJ
      parse_in->buffer->file=NULL;//重要 否则报错 编译器内部错误：在 linemap_add 中，于 libcpp/line-map.cc:588
      n_debug("加入头文件<aet.h>的过程结束。");
      restoreToken(self);
      input_location=self->addAetHeader.loc;
      self->addAetHeader.running=FALSE;
   }else{
      return block_mgr_parser_goto(block_mgr_get(),start_attr_ok,pos);
   }
   return FALSE;
}

/**
 * 分析AClass *getClass() 这时，还没有AClass 把AClass 替换成 void *
 */
nboolean   class_parser_exception(ClassParser *self,tree value)
{
   //printf("分析class_parser_exception 00 %s\n",IDENTIFIER_POINTER(value));
   c_parser *parser=self->parser->parser;
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

/**
 * 解析AObject.Enum形式。
 * ret中有类名。
 */
void  class_parser_parser_enum_dot(ClassParser *self,struct c_typespec *ret)
{
   parser_help_set_forbidden(TRUE);
   tree value=enum_parser_parser_dot(enum_parser_get(),ret);
   if(value!=NULL_TREE){
      ret->spec=value;
   }
   parser_help_set_forbidden(FALSE);
}

/**
 * 解析在文件或类中的枚举定义
 */
struct c_typespec class_parser_enum(ClassParser *self,location_t loc)
{
   c_parser *parser=self->parser->parser;
   parser_help_set_forbidden(TRUE);
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
   parser_help_set_forbidden(FALSE);
   return ret;
}

/**
 * 完成在文件中的枚举定义
 * 在c_parser_class_declaration方法中完成的是类中的枚举定义。
 */
void class_parser_complete_enum(ClassParser *self,struct c_declspecs *specs,
        nboolean haveAccessControl,ClassPermissionType permission,ClassName *className)
{
   if(TREE_CODE(specs->type)==ENUMERAL_TYPE){
      if(specs->typespec_kind==ctsk_tagdef){
         n_debug("class_parser_complete_enum 00 sysName:%s\n",className!=NULL?className->sysName:"null");
         if(!aet_utils_valid_tree(specs->expr) || TREE_CODE(specs->expr)!=IDENTIFIER_NODE)
            return;
         char *tag=IDENTIFIER_POINTER(specs->expr);
         if(strcmp(tag,"enum$"))
            return;
         specs->expr=NULL_TREE;
         char *sysName=className==NULL?"":className->sysName;
         EnumData *item=enum_parser_get_by_enum_name(enum_parser_get(),sysName,IDENTIFIER_POINTER(TYPE_NAME(specs->type)));
         if(item==NULL){
            n_error("找不到枚举，不应该出现的错误。");
         }
         n_debug("class_parser_complete_enum 11 sysName:%s %s\n",sysName,item->origName);
         location_t loc=item->loc;
         if(className==NULL){
            if(haveAccessControl && permission!=CLASS_PERMISSION_PUBLIC){
               error_at(loc,"文件中定义的枚举只能用public$修饰。");
               return;
            }
         }
         enum_parser_create_decl(enum_parser_get(),loc,className,specs,permission);
      }
   }
}

ClassName  *class_parser_get_class_name(ClassParser *self)
{
   return self->currentClassName;
}

GenericModel      *class_parser_get_func_generic_mode(ClassParser *self)
{
   return self->currentFuncModel;
}

/////////////////////---下面的功能是给编译的.c文件自动中入 aet.h-----------------------
static int addAetBuiltinCodes(cpp_reader *pfile, const char *str,size_t len,location_t loc)
{
   char *nbuf=xstrndup(str,len);
   void *restoreData=aet_utils_create_restore_location_data(pfile,loc);
   _cpp_file *oldfile =pfile->buffer->file;;
   cpp_buffer *newBuffer=cpp_push_buffer (pfile, (uchar *) nbuf, len, /* from_stage3 */ true);
   //newBuffer->file =oldfile 这句是关键，否则在files.cc _cpp_find_file中，下面的代码要报段错误，因为 pfile->buffer->file是空的。
   //file->implicit_preinclude = (kind == _cpp_FFK_PRE_INCLUDE || (pfile->buffer && pfile->buffer->file->implicit_preinclude)
   newBuffer->file =oldfile;
   aet_utils_write_cpp_buffer(newBuffer,restoreData);
   _cpp_clean_line (pfile);
   cpp_stop_forcing_token_locations (pfile);
   return 1;
}

/**
 * 要立即执行 cpp_push_buffer (pfile, (uchar *) nbuf...
 * 需要当前tokens_avail变为空，这样才有机会通过c_parser_peek_token 起动新的cpp_buffer
 * 所以在备份token代码中其它地方不能再调用c_parser_peek_token
 */
static void backupToken(ClassParser *self)
{
     c_parser *parser=self->parser->parser;
     int tokenCount=parser->tokens_avail;
     int i;
     c_token *token;
     for(i=0;i<tokenCount;i++){
        token=c_parser_peek_token (parser);
        aet_utils_copy_token(token,&self->addAetHeader.headBackTokens[i]);
        aet_print_token(token);
       c_parser_consume_token (parser);
     }
     self->addAetHeader.backTokenCount= tokenCount;
}

static void restoreToken(ClassParser *self)
{
     c_parser *parser=self->parser->parser;

     if(self->addAetHeader.backTokenCount==0)
         return;
     int tokenCount=parser->tokens_avail;
     if(tokenCount+self->addAetHeader.backTokenCount>AET_MAX_TOKEN){
         error("token太多了");
         return;
     }
     int i;
      for(i=0;i<self->addAetHeader.backTokenCount;i++){
         aet_utils_copy_token(&self->addAetHeader.headBackTokens[i],&parser->tokens[i+tokenCount]);
     }
     parser->tokens_avail=tokenCount+self->addAetHeader.backTokenCount;
     aet_print_token_in_parser("加aet.h restore ------");
}

static char *getCompileFile(ClassParser *self)
{
   c_parser *parser=self->parser->parser;
   cpp_buffer *buffer= parse_in->buffer;
   struct _cpp_file *file=buffer->file;
   cpp_dir *dir=cpp_get_dir (file);
   const char *fileName=_cpp_get_file_name (file);
   n_debug("getCompileFile 00 fileName:%s dir->name:%s filePath:%s",fileName,dir->name,cpp_get_path (file));
   NFile *nfile=n_file_new(cpp_get_path (file));
   NFile *canonicalFile=n_file_get_canonical_file(nfile);
   const char *canonicalPath=n_file_get_absolute_path(canonicalFile);
   char *cf=n_strdup(canonicalPath);
   n_file_unref(nfile);
   n_file_unref(canonicalFile);
   return cf;
}

static char *createRealName(char *file)
{
   // 获取canonicalize名
   char *realFile=xmalloc(PATH_MAX);
   if (realpath(file, realFile) != NULL) {
      ;
   }else{
      sprintf(realFile,"%s",file);
   }
   return realFile;
}

static char *getCompieFileFromToken( c_token *tok)
{
   location_t loc= tok->location;
   expanded_location xloc = expand_location(loc);
   return createRealName(xloc.file);
}

/**
 * 用户的编译参数中有-noaetinclude
 */
static nboolean isNoInclude()
{
   char *ok=getenv ("GCC_AET_NO_INCLUDE");
   return ok!=NULL;
}

/**
 * 参数 -I/xxx/yyy 的流程
 * c_common_handle_option(c-opts.cc)-->add_path(incpath.cc)-->c_common_post_options
 * -->register_include_chains(incpath.cc) -->cpp_set_include_chains
 * incpath.cc 中通过 add_path收集所有的 include,再通过 register_include_chains设收休的include
 * 到cpp_reader中的各个cpp_dir
 * 所以直接调用add_path 或 cpp_push_include  cpp_push_default_include 没有用处。
 */
static nboolean addIncludePath(ClassParser *self)
{
   const char *aetInclude = makefile_parm_get_aet_include_path(makefile_parm_get());
   n_debug("addIncludePath --- %s\n",aetInclude);
   if(aetInclude==NULL)
      return FALSE;
   cpp_reader *pfile=parse_in;
   cpp_dir *quote = pfile->bracket_include ;
   struct cpp_dir *p;
   for (p = quote;; p = p->next){
      if (!p)
         break;
      if(aetInclude && strcmp(p->name,aetInclude)==0){
         n_debug("用户编译参数或系统中已有include路径:%s\n",aetInclude);
         return TRUE;
      }
   }

   size_t pathlen = strlen (aetInclude);
   cpp_dir *newDir;
   newDir = XNEW (cpp_dir);
   newDir->next = NULL;
   newDir->name = n_strdup(aetInclude);
   newDir->len = pathlen;
   newDir->sysp = 0;
   newDir->construct = 0;
   newDir->user_supplied_p = true;

   cpp_dir* temp =quote; // 从头节点开始遍历链表找到最后一个节点
   while (temp->next != NULL){ // 遍历到末尾的节点（即下一个指针为NULL的节点）
      temp = temp->next;
   }
   temp->next = newDir; // 将最后一个节点的next指针指向新节点，实现添加到末尾
   return TRUE;
}

/*
执行的条件
1.是否已加过
2.检查用户传递的编译参数有 noinclude
3.调用class_parser_add_include时，正在编译的文件是in_fnames[0]
4.文件中不能包含有AObject.c
返回FALSE 说明不要处理关键词。等加完了<aet.h>再处理
*/
nboolean class_parser_add_include(ClassParser *self)
{
   //已经加过了
   if(self->addAetHeader.added){
      n_debug("class_parser_add_include 已经加过了:%d %s\n",self->addAetHeader.added,in_fnames[0]);
      return FALSE;
   }

   //编译参数中有 -noinclude
   if(isNoInclude()){
      n_debug("class_parser_add_include 编译参数中有 -noinclude %s\n",in_fnames[0]);
      self->addAetHeader.added=TRUE;
      return FALSE;
   }
   //在函数内
   if(current_function_decl){
      n_debug("class_parser_add_include 在函数内:%p %s\n",current_function_decl,in_fnames[0]);
      return FALSE;
   }
   c_parser *parser=self->parser->parser;
   c_token *tok=c_parser_peek_token(parser);
   location_t loc=tok->location;
   char *cf= getCompileFile(self);
   char *realName = createRealName(in_fnames[0]);
   char *tf= getCompieFileFromToken(tok);
   n_debug("class_parser_add_include 00 getCompileFile:%s %s %s\n",cf,realName,tf);
   if(cf==NULL){
      free(realName);
      free(tf);
      return FALSE;
   }
   if(!(strcmp(cf,realName)==0 && strcmp(cf,tf)==0)){
      n_free(cf);
      n_free(realName);
      n_free(tf);
      return FALSE;
   }
   n_free(cf);
   n_free(realName);
   n_free(tf);
   //已经加入过AObject.h
   ClassName *rootName= class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
   if(rootName!=NULL){
      n_debug("class_parser_add_include  rootName :%s %s\n",rootName,in_fnames[0]);
      self->addAetHeader.added=TRUE;
      return FALSE;
   }
   //加入#include <aet.h>
   self->addAetHeader.added=TRUE;
   //加入头文件搜索路径
   if(!addIncludePath(self))
      return FALSE;
   self->addAetHeader.running=TRUE;
   self->addAetHeader.loc=input_location;
   n_debug("class_parser_add_include 11 正在编译的文件有3个来源 : %s\n",in_fnames[0]);

   backupToken(self);
   //必须加入'\n'否则报有游离的#号错误
   char incstr[256];
   //sprintf(incstr,";\n#include <aet.h>\n %s %d\n",RID_AET_GOTO_STR,GOTO_ADD_H_FILE);
   sprintf(incstr,";\n#include <aet.h>\n \n#include <aet_mtcs.h>\n %s %d\n",RID_AET_GOTO_STR,GOTO_ADD_H_FILE);

   addAetBuiltinCodes(parse_in,incstr,strlen(incstr),loc);
   return TRUE;
}

//是否正在加入aet.h头文件
nboolean  class_parser_is_add_include(ClassParser *self)
{
   expanded_location xloc = expand_location(input_location);
   return (xloc.file && !strcmp(xloc.file,in_fnames[0]) && self->addAetHeader.running);
}

ClassParser *class_parser_get()
{
   static ClassParser *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(ClassParser));
      singleton->parser = aet_parser_get();
      classParserInit(singleton);
      mtcs_types_get();
   }
   return singleton;
}


