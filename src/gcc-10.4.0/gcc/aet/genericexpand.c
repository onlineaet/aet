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
#include "toplev.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "opts.h"

#include "c-aet.h"
#include "aetutils.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "blockmgr.h"
#include "classimpl.h"
#include "funcmgr.h"
#include "genericexpand.h"

static void genericExpandInit(GenericExpand *self)
{
	self->genericParser=generic_parser_new();
	self->genericNeedFileEnd=FALSE;
	self->ifaceNeedFileEnd=FALSE;
	self->genericDefine=generic_define_new();

}

/**
 * 如果belongSysName==self 说明是在类中new$未定泛型类 如 Abc *abc=new$ Abc<E>();
 * self->_createAllGenericFunc567(efg,"Efg",0);
 */
void  generic_expand_add_define_object(GenericExpand *self,ClassName *className,GenericModel * genericDefines)
{
	generic_define_add_define_object(self->genericDefine,className,genericDefines);
}

void  generic_expand_add_define_func_call(GenericExpand *self,ClassName *className,char *funcName,GenericModel * defines)
{
	generic_define_add_define_func_call(self->genericDefine,className,funcName,defines);

}

static void printDefine(NPtrArray *definesArray,NString *printStr)
{
	if(definesArray->len==0){
		n_string_append(printStr,"无泛型!");
	}else{
		int i;
		n_string_append(printStr,"泛型定义如下：");
		for(i=0;i<definesArray->len;i++){
		   GenericModel *defines=(GenericModel *)n_ptr_array_index(definesArray,i);
		   char     *var=generic_model_define_str(defines);
		   n_string_append_printf(printStr," %d、 %s。",i+1,var);
		   n_free(var);
		}
	}
}

static void  printHelp(SetBlockHelp *help)
{
	  NString *printStr=n_string_new("\n");
	  n_string_append_printf(printStr,"设置泛型块的函数有:%d个。\n",help->setBlockFuncsArray->len);
	  n_string_append_printf(printStr,"总共实现了%d个。\n",help->implSetBlockFuncArray->len);
	  n_string_append_printf(printStr,"总的泛型块函数有%d个。\n",help->blockFuncsArray->len);
	  n_string_append(printStr,"\n");
	  int i;
	  for(i=0;i<help->setBlockFuncsArray->len;i++){
		  SetGenericBlockFuncInfo *item=(SetGenericBlockFuncInfo *)n_ptr_array_index(help->setBlockFuncsArray,i);
		  n_string_append_printf(printStr,"设置泛型块的函数有 %d:%s %s\n",i,item->sysName,item->funcName);
	  }
	  n_string_append(printStr,"\n");
	  for(i=0;i<help->blockFuncsArray->len;i++){
		  BlockFunc *item=(BlockFunc *)n_ptr_array_index(help->blockFuncsArray,i);
		  n_string_append_printf(printStr,"泛型块函数 %d:%s %s\n",i,item->sysClassName,item->orgiFuncName);
	  }
	  n_string_append(printStr,"\n");
	  for(i=0;i<help->implSetBlockFuncArray->len;i++){
		  char *item=(char *)n_ptr_array_index(help->implSetBlockFuncArray,i);
		  n_string_append_printf(printStr,"实现的设置泛型的函数 %d:%s\n",i,item);
	  }

	  n_string_append(printStr,"\n");
	  n_string_append_printf(printStr,"收集类的泛型：类：%d个。\n",help->collectDefines->len);
	  for(i=0;i<help->collectDefines->len;i++){
		  CollectDefines *item=(CollectDefines *)n_ptr_array_index(help->collectDefines,i);
		  n_string_append_printf(printStr," %s ",item->sysName);
		  printDefine(item->definesArray,printStr);
		  n_string_append(printStr,"\n");
	  }

	  n_string_append(printStr,"\n");
	  n_string_append_printf(printStr,"收集泛型函数的泛型：类：%d个。\n",help->collectFuncCallDefines->len);
	  for(i=0;i<help->collectFuncCallDefines->len;i++){
		  CollectDefines *item=(CollectDefines *)n_ptr_array_index(help->collectFuncCallDefines,i);
		  n_string_append_printf(printStr," %s %s",item->sysName,item->funcName);
		  printDefine(item->definesArray,printStr);
		  n_string_append(printStr,"\n");
	  }

	  printf("生成的代码信息如下:file:%s\n%s\n",in_fnames[0],printStr->str);
	  printf("实现的源代码是:file:%s\n%s\n",in_fnames[0],help->codes->str);
	  n_string_free(printStr,TRUE);
}

/**
 * 核心功能，如何引入头文件
 * 如果 new$ Abc<int>中具体的其它泛型类,如 Tee *temp=new$ Tee<double>()，也要把他们提取出来，当作在当前文件中new$ Tee<double>
 * 如果没有具体的而是new$ Tee<E> 不需要提取到当前文件。
 * 生成函数指针声明和函数声明以及赋值泛型块函数指针的函数声明
 * static  void  _test_AObject__inner_generic_func_1_abc123 (test_AObject *self,FuncGenParmInfo tempFgpi1234,int abc)
 * typedef  void  (*_test_AObject__inner_generic_func_1_typedecl) (test_AObject *self,FuncGenParmInfo tempFgpi1234,int abc);
 * static void _test_AObject__createAllGenericFunc567(test_AObject *self,void *object,char *sysName,int pointer)
 * 1.合并收集本文件中所有泛型确定的new$对象的的泛型 如 new$ Abc<int> new$ Abc<float> 把int float都加到同一个GenericClass
 * 2.每个GenericClass中递归类变量中undefine的类和undefine父类
 */
void generic_expand_compile_decl(GenericExpand *self)
{
	   //printf("generic_expand_create_generic_class 00 %s\n",in_fnames[0]);
	   char *result=generic_define_create_generic_class(self->genericDefine);
	   SetBlockHelp  *help=generic_define_get_help(self->genericDefine);
	   generic_parser_set_help(self->genericParser,help);
	   n_debug("generic_expand_create_generic_class 11 源代码:\n%s\n",result);
	   NString *codes=n_string_new("");
	   if(result!=NULL){
			n_string_append(codes,result);
	   }
	   n_string_append_printf(codes,"%s %d\n",RID_AET_GOTO_STR,GOTO_GENERIC_COMPILE_UNDEFINE_BLOCK_FUNC_DECL);
	   aet_utils_add_token(parse_in,codes->str,codes->len);
	   n_string_free(codes,TRUE);
	   n_free(result);
}




/*
static __attribute__((constructor)) void abcd_ctor()
{
	printf("test3--- constructor\n");
	memcpy(_inFileGlobalFuncGenParmInfo.blockFuncs[0].sysName,"test",4);
	_inFileGlobalFuncGenParmInfo.blockFuncs[0].blockFuncsPointer[0]=_test_MatrixOps__inner_generic_func_1_abc123;
	_inFileGlobalFuncGenParmInfo.countOfBlockClass=1;
}
*/
static void createGlobalFgpiCodes(char *sysName,int index,int blockCount,NString *codes)
{
  n_string_append_printf(codes,"memcpy(%s.blockFuncs[%d].sysName,\"%s\",%d);\n",FuncGenParmInfo_NAME,index,sysName,strlen(sysName));
  n_string_append_printf(codes,"%s.blockFuncs[%d].sysName[%d]='\\0';\n",FuncGenParmInfo_NAME,index,strlen(sysName));
  int i;
  for(i=0;i<blockCount;i++){
	   char *funcDefineName=generic_util_create_block_func_define_name(sysName,i+1);
	   n_string_append_printf(codes,"%s.blockFuncs[%d].blockFuncsPointer[%d]=%s;\n",FuncGenParmInfo_NAME,index,i,funcDefineName);
	   n_free(funcDefineName);
  }
  n_string_append_printf(codes,"%s.blockFuncs[%d].count=%d;\n",FuncGenParmInfo_NAME,index,blockCount);
}

/**
 * 创建.c文件中的初始化 static FuncGenParmInfo _inFileGlobalFuncGenParmInfo的代码
 * 利用gcc的construct特性
 * static __attribute__((constructor)) void _inner_fgpi_ctor_123()
{
memcpy(_inFileGlobalFuncGenParmInfo.blockFuncs[0].sysName,"com_ai_MatrixOps",16);
_inFileGlobalFuncGenParmInfo.blockFuncs[0].blockFuncsPointer[0]=_com_ai_MatrixOps__inner_generic_func_1_abc123;
_inFileGlobalFuncGenParmInfo.blockFuncs[0].blockFuncsPointer[1]=_com_ai_MatrixOps__inner_generic_func_2_abc123;
_inFileGlobalFuncGenParmInfo.blockFuncs[0].count=2;
memcpy(_inFileGlobalFuncGenParmInfo.blockFuncs[1].sysName,"com_ai_LU",9);
_inFileGlobalFuncGenParmInfo.blockFuncs[1].blockFuncsPointer[0]=_com_ai_LU__inner_generic_func_1_abc123;
_inFileGlobalFuncGenParmInfo.blockFuncs[1].count=1;
_inFileGlobalFuncGenParmInfo.countOfBlockClass=2;
}
 */
static char *createInitFgpiCodes(SetBlockHelp  *help)
{
	int i,j;
	NPtrArray *array=n_ptr_array_new();
	for(i=0;i<help->blockFuncsArray->len;i++){
	   BlockFunc *item=n_ptr_array_index(help->blockFuncsArray,i);
	   nboolean find=FALSE;
	   for(j=0;j<array->len;j++){
		   char *sysName=n_ptr_array_index(array,j);
		   if(strcmp(sysName,item->sysClassName)==0){
			   find=TRUE;
			   break;
		   }
	   }
	   if(!find){
		 n_ptr_array_add(array,item->sysClassName);
	   }
	}

//	tree id=aet_utils_create_ident(FuncGenParmInfo_NAME);
//	tree funcGenParmInfo=lookup_name(id);
//	aet_print_tree(funcGenParmInfo);
	NString *codes=n_string_new("");
    n_string_append(codes,"static __attribute__((constructor)) void _inner_fgpi_ctor_123()\n");
    n_string_append(codes,"{\n");
    nboolean haveCodes=FALSE;
	for(i=0;i<array->len;i++){
		  char *sysName=n_ptr_array_index(array,i);
		  int blockCount=0;
		  for(j=0;j<help->blockFuncsArray->len;j++){
			   BlockFunc *item=n_ptr_array_index(help->blockFuncsArray,j);
			   if(strcmp(sysName,item->sysClassName)==0){
				   blockCount++;
			   }
		  }
		  if(blockCount>0){
			haveCodes=TRUE;
		    createGlobalFgpiCodes(sysName,i,blockCount,codes);
		  }
	}
    n_string_append_printf(codes,"%s.countOfBlockClass=%d;\n",FuncGenParmInfo_NAME,array->len);
    n_string_append_printf(codes,"sprintf(%s.from,\"%s countOfBlockClass:%d\");\n",FuncGenParmInfo_NAME,in_fnames[0],array->len);
    n_string_append(codes,"}\n");
    char *result=NULL;
   // if(haveCodes){
	   result=n_strdup(codes->str);
    //}
    n_string_free(codes,TRUE);
    n_ptr_array_unref(array);
    return result;
}

/**
 * 生成块定义代码 设置泛型块的函数定义代码以及初始化FuncGenParmInfo代码三部分
 * help->collectDefines 中确定的泛型，但不包括item->sysClassName类中创建新
 * 确定泛型。所以在生成块代码时，要把本类中创建的泛型也加入进去
 */
void generic_expand_compile_define(GenericExpand *self)
{
   SetBlockHelp  *help=generic_define_get_help(self->genericDefine);
   NPtrArray     *collectDefines=help->collectDefines;//装的是GenericClass
   NPtrArray     *collectFuncCallDefines=help->collectFuncCallDefines;//装的是GenericMethod
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<help->blockFuncsArray->len;i++){
	   BlockFunc *item=n_ptr_array_index(help->blockFuncsArray,i);
	   n_debug("generic_expand_compile_define 00 %s %s\n",item->sysClassName,item->orgiFuncName);
	   block_func_ready_parm(item,collectDefines,collectFuncCallDefines);
	   char *cc=block_func_create_codes(item);
	   if(cc!=NULL){
		   n_string_append(codes,cc);
		   n_free(cc);
	   }
   }
   n_string_append(codes,"\n\n");
   n_string_append(codes,help->codes->str);

   char *initFgpiCodes=createInitFgpiCodes(help);
   if(initFgpiCodes!=NULL){
	  n_string_append(codes,initFgpiCodes);
	  n_free(initFgpiCodes);
   }
   n_debug("最后一步编译泛型声明和定义函数的源代码:%p prev:%p 所在文件:%s\n%s\n", parse_in->buffer,parse_in->buffer->prev,in_fnames[0],codes->str);
   if(codes->len>0){
	   _cpp_pop_buffer(parse_in);
	   	//printf("结束了------wwwwwwwwwww \n");
	   	cpp_buffer *buffer = parse_in->buffer;
	   	//printf("结束了------111 %p %p\n",buffer,buffer->prev);
	   	buffer->prev=NULL;
	    self->parser->isGenericState=true;
        aet_utils_add_token(parse_in,codes->str,codes->len);
   }
   n_string_free(codes,TRUE);
}

/**
 * 当在newheap.c生成对象时，需要内部声明函数_createAllGenericFunc567
 * self->_createAllGenericFunc567(efg,"Efg",0);
 */
void generic_expand_produce_set_block_func_decl(GenericExpand *self,nboolean start_attr_ok)
{
	  c_parser *parser=self->parser;
	  struct c_declspecs *specs; //c-tree.h定义
	  tree prefix_attrs;
	  tree all_prefix_attrs;
	  location_t here = c_parser_peek_token (parser)->location;
	  n_debug("generic_expand_produce_set_block_func_decl 00:创建一个空的声明说明符 ：\n");
	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
	  c_parser_consume_token (parser);//consume   GOTO_GENERIC_BLOCK_FUNC_DECL
	  specs = build_null_declspecs ();
	  c_parser_declspecs (parser, specs, true, true, start_attr_ok,true, true, start_attr_ok, true, cla_nonabstract_decl);
	  finish_declspecs (specs);
	  prefix_attrs = specs->attrs;
	  all_prefix_attrs = prefix_attrs;
	  specs->attrs = NULL_TREE;
	  while (true){
		  n_debug("generic_expand_produce_set_block_func_decl 11:\n");
	      struct c_declarator *declarator;
	      bool dummy = false;
	      declarator = c_parser_declarator (parser, specs->typespec_kind != ctsk_none,C_DTR_NORMAL, &dummy);
	  	  aet_print_token(c_parser_peek_token (parser));
	      if (c_parser_next_token_is (parser, CPP_EQ)
		      || c_parser_next_token_is (parser, CPP_COMMA)
		      || c_parser_next_token_is (parser, CPP_SEMICOLON)
		      || c_parser_next_token_is_keyword (parser, RID_ASM)
		      || c_parser_next_token_is_keyword (parser, RID_ATTRIBUTE)
		      || c_parser_next_token_is_keyword (parser, RID_IN)){
		      tree asm_name = NULL_TREE;
		      tree postfix_attrs = NULL_TREE;
	          tree d = start_decl (declarator, specs, false,chainon (postfix_attrs,all_prefix_attrs));
		      if (d  && TREE_CODE (d) == FUNCTION_DECL && DECL_ARGUMENTS (d) == NULL_TREE && DECL_INITIAL (d) == NULL_TREE){
				  const struct c_declarator *decl = declarator;
				  const struct c_declarator *last_non_id_attrs = NULL;
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
				   if (last_non_id_attrs   && last_non_id_attrs->kind == cdk_function){
					  n_debug("generic_expand_produce_set_block_func_decl 22:设参数  DECL_ARGUMENTS (d) ：\n");
					  DECL_ARGUMENTS (d) = last_non_id_attrs->u.arg_info->parms;
				   }
			   }
  	           if (d){
					tree args=DECL_ARGUMENTS (d);
					tree parm;
					for (parm = args; parm; parm = DECL_CHAIN (parm)){
						DECL_CONTEXT(parm)=NULL_TREE;
					}
					c_c_decl_bind_file_scope(d);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
					DECL_CONTEXT(d)=NULL_TREE;
					DECL_EXTERNAL(d)=1;
					TREE_STATIC(d)=0;
					TREE_PUBLIC(d)=0;
					finish_decl (d, UNKNOWN_LOCATION, NULL_TREE,NULL_TREE, asm_name);
			   }
		       if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
				  n_debug("generic_expand_produce_set_block_func_decl 33:结束了：\n");
				  c_parser_consume_token (parser);
				  return;
		       }else {
				 c_parser_error (parser, "expected %<,%> or %<;%>");
				 return;
		      }
	      }//end c_parser_next_token_is (parser, CPP_COMMA 分析完所有的EQ COMMA SEMICOLON RID_IN...
	    }//end while
}

/**
 * 当编译块函数实现时，会进入每一个条件块中的第一条语名。aet_goto_compile$ 6 model_0$YZt0@b_0 这是每个参数在
 * 该条件块时的泛型定义
 * 处理 aet_goto_compile$ 6 model_0$YZt0@b_0 model是参数名 0是参数对应的泛型定义所在的索引号
 */
void generic_expand_produce_cond_define(GenericExpand *self)
{
	generic_parser_produce_cond_define(self->genericParser);
}

void generic_expand_parser_typeof(GenericExpand *self)
{
	generic_parser_parser_typeof(self->genericParser);
}

void  generic_expand_cast_by_token(GenericExpand *self,c_token *token)
{
	generic_parser_cast_by_token(self->genericParser,token);
}

void  generic_expand_replace(GenericExpand *self,char *genStr)
{
	generic_parser_replace(self->genericParser,genStr);
}

/**
 * 创建声明static FuncGenBlockInfo   _inFileGlobalFuncGenParmInfo={0};
 */
static void createFggbVar(GenericExpand *self)
{
    tree id=aet_utils_create_ident(FuncGenParmInfo_NAME);
    tree decl=lookup_name(id);
    if(aet_utils_valid_tree(decl)){
    	return;
    }
	c_parser *parser=self->parser;
	location_t  loc = input_location;
    tree type, init;
    type = generic_util_get_fggb_record_type();
    decl = build_decl (loc, VAR_DECL, id, type);
    TREE_STATIC (decl) = 1;
    TREE_READONLY (decl) = 0;
    DECL_ARTIFICIAL (decl) = 0;
    init =NULL_TREE;//  build_int_cst (integer_type_node,0);
    DECL_INITIAL (decl) = init;
    TREE_USED (decl) = 0;
    c_c_decl_bind_file_scope(decl);
	DECL_CONTEXT(decl)=NULL_TREE;
    DECL_EXTERNAL(decl)=0;
	TREE_STATIC(decl)=1;
	TREE_PUBLIC(decl)=0;
    finish_decl (decl, loc, init, NULL_TREE, NULL_TREE);
}

/**
 * 创建
 * static void _setGenericBlockFunc567_cb(void *self,void *object ,char *sysName,int pointerCount);
 */
static void createSetBlockCallback(GenericExpand *self)
{
    tree id=aet_utils_create_ident(AET_SET_BLOCK_FUNC_CALLBACK_NAME);
    tree declxx=lookup_name(id);
    if(aet_utils_valid_tree(declxx)){
    	return;
    }
	tree param_type_list = NULL;
	//第一个参数 void *self
	tree selftype=build_pointer_type(void_type_node);
    param_type_list = tree_cons (NULL_TREE, selftype, param_type_list);
    //加入第二个参数 void objectFuncGenParmInfo
	tree object=build_pointer_type(void_type_node);
	param_type_list = tree_cons (NULL_TREE, object, param_type_list);

	 //加入第三个参数 char *sysName
     tree name=build_pointer_type(char_type_node);
	 param_type_list = tree_cons (NULL_TREE, name, param_type_list);
	 //加入第四个参数 int pointerCount
     tree pointerCount=integer_type_node;
	 param_type_list = tree_cons (NULL_TREE, pointerCount, param_type_list);
	 //结束参数
     param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);

	 param_type_list = nreverse (param_type_list);
	 tree rtntype=void_type_node;
	 tree fntype = build_function_type (rtntype, param_type_list);

	 tree functionId=aet_utils_create_ident(AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	 tree decl = build_decl (input_location, FUNCTION_DECL, functionId, fntype);
	 TREE_STATIC (decl) = 1;
	 TREE_READONLY (decl) = 0;
	 DECL_ARTIFICIAL (decl) = 0;
	 TREE_USED (decl) = 0;
	 c_c_decl_bind_file_scope(decl);
	 DECL_CONTEXT(decl)=NULL_TREE;
	 DECL_EXTERNAL(decl)=0;
	 TREE_STATIC(decl)=1;
	 TREE_PUBLIC(decl)=0;
	 finish_decl (decl, input_location, NULL_TREE,NULL_TREE, NULL_TREE);
}


void generic_expand_create_fggb_var_and_setblock_func(GenericExpand *self)
{
	createFggbVar(self);
	createSetBlockCallback(self);
}



static nboolean exists(NPtrArray *array,char *sysName)
{
	int i;
	for(i=0;i<array->len;i++){
		char *item=n_ptr_array_index(array,i);
		if(strcmp(item,sysName)==0)
			return TRUE;
	}
	return FALSE;
}

/**
 * 引用有泛型块的类
 */
NPtrArray *generic_expand_get_ref_block_class_name(GenericExpand *self)
{
   SetBlockHelp  *help=generic_define_get_help(self->genericDefine);
   if(help==NULL)
	   return NULL;
   NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
   int i;
   for(i=0;i<help->blockFuncsArray->len;i++){
	   BlockFunc *item=n_ptr_array_index(help->blockFuncsArray,i);
	   if(!exists(array,item->sysClassName)){
		   n_ptr_array_add(array,n_strdup(item->sysClassName));
	   }
   }
   return array;
}


///////////////////////////////////////加标记，eof后编译新的块代码------------------------


static cpp_buffer * createBuffer (cpp_reader *pfile, const uchar *str, size_t len, int from_stage3)
{
   char *buffer;
   buffer=n_strdup(str);
   len=strlen(buffer);
   cpp_buffer *new_buffer = XOBNEW (&pfile->buffer_ob, cpp_buffer);
   memset (new_buffer, 0, sizeof (cpp_buffer));
   new_buffer->next_line = new_buffer->buf = buffer;
   new_buffer->rlimit = buffer + len;
   new_buffer->from_stage3 = from_stage3;
   new_buffer->need_line = true;
   return new_buffer;
}

static char* getCurrentFile(c_parser *parser)
{
       cpp_buffer *buffer= parse_in->buffer;
       if(buffer==NULL){
           printf("getCurrentFile 00 buffer:%p %s\n",buffer,in_fnames[0]);
    	   return NULL;
       }
       struct _cpp_file *file=buffer->file;
       if(file==NULL){
           printf("getCurrentFile 11 file:%p %s\n",file,in_fnames[0]);
    	   return NULL;
       }
       const char *fileName=_cpp_get_file_name (file);
       if(strcmp(fileName,in_fnames[0])){
           printf("getCurrentFile 22 %s %s\n",file,in_fnames[0]);
    	   return NULL;
       }
       return fileName;
}

static nboolean addToCppBuffer00(c_parser *parser,char *tag)
{
	char *fileName=getCurrentFile(parser);
	if(fileName!=NULL){
		 NString *fstr=n_string_new(fileName);
		 nboolean ok=(n_string_ends_with(fstr,".c") || n_string_ends_with(fstr,".cpp"));
		 n_string_free(fstr,TRUE);
		 printf("addToCppBuffer ---- 00 %s ok:%d tag:%s len:%d\n",fileName,ok,tag,strlen(tag));
		 if(ok){
			cpp_buffer *newBuffer=createBuffer(parse_in,tag,strlen(tag),TRUE);
			printf("addToCppBuffer ---- 11 ---当前.c的buffer %p prev:%p newBuffer:%p\n",parse_in->buffer,parse_in->buffer->prev,newBuffer);
			parse_in->buffer->prev=newBuffer;
			return TRUE;
		 }
	}else{
		 n_error("当前编译的内容来自缓存，不是来自文件%s。",in_fnames[0]);
	}
	return FALSE;
}

/**
 * 在files.c把read_file_guts函数中的16改成64,留出空间，追加字符串
 * RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE
 * 不需再存在prev里。
 */
static nboolean addToCppBuffer(c_parser *parser,char *tag)
{
	char *fileName=getCurrentFile(parser);
	if(fileName!=NULL){
		 NString *fstr=n_string_new(fileName);
		 nboolean ok=(n_string_ends_with(fstr,".c") || n_string_ends_with(fstr,".cpp"));
		 n_string_free(fstr,TRUE);
		 n_debug("addToCppBuffer ---- 00 %s ok:%d tag:%s len:%d\n",fileName,ok,tag,strlen(tag));
		 if(ok){
			 strncat(parse_in->buffer->buf,tag,strlen(tag));
			 parse_in->buffer->rlimit=parse_in->buffer->rlimit+strlen(tag);
			 return TRUE;
		 }
	}else{
		 n_error("当前编译的内容来自缓存，不是来自文件%s。",in_fnames[0]);
	}
	return FALSE;
}


void generic_expand_add_eof_tag(GenericExpand *self)
{
	c_parser *parser=self->parser;
	if(self->genericNeedFileEnd)
		return;
	 char tag[256];
	 if(self->ifaceNeedFileEnd){
		sprintf(tag,"%s %d \n %s %d \n",
				RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE,
				RID_AET_GOTO_STR,GOTO_IFACE_COMPILE);
	 }else{
		sprintf(tag,"%s %d \n",
							RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE);
	 }
		printf("generic_expand_add_eof_tag %s pid:%d %s\n",tag,getpid(),in_fnames[0]);
	 self->genericNeedFileEnd=addToCppBuffer(self->parser,tag);
}

void generic_expand_add_eof_tag_for_iface(GenericExpand *self)
{
	c_parser *parser=self->parser;
	if(self->ifaceNeedFileEnd)
		return;
	char tag[256];
	if(self->genericNeedFileEnd){
		sprintf(tag,"%s %d \n %s %d \n",
				RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE,RID_AET_GOTO_STR,GOTO_IFACE_COMPILE);
	}else{
		sprintf(tag,"%s %d \n",RID_AET_GOTO_STR,GOTO_IFACE_COMPILE);
	}
	printf("generic_expand_add_eof_tag_for_iface--- %s pid:%d %s\n",tag,getpid(),in_fnames[0]);
	self->ifaceNeedFileEnd=addToCppBuffer(self->parser,tag);
}

void generic_expand_set_parser(GenericExpand *self,c_parser *parser)
{
   self->parser=parser;
   generic_parser_set_parser(self->genericParser,parser);
}


GenericExpand *generic_expand_get()
{
	static GenericExpand *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(GenericExpand));
		 genericExpandInit(singleton);
	}
	return singleton;
}






