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
#include "tree-iterator.h"
#include "tree-cfg.h"
#include "tree-pretty-print.h"

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
#include "classinterface.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "aet-c-parser-header.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "genericmodel.h"
#include "classutil.h"

/**
 * 实现接口的方法
 * 1.在实现类的最后也就是退出”};”时
 * 2.检查如果类定义了实现接口如：Implements OpenDoor
 * 3.在实现类中的所有定义方法(ClassFunc fromImplDefine有效)中查找区匹配接口声明的方法
 *  匹配指: 接口与类的原函数名相同，参数除第一个（也就是self）外其它都相同（判断见getRemainParmString）
 *  类定义的方法与接口声明的函数返回值相同(compareFunctionType)
 * 4.不能用接口的函数指针指到类定义的方法上，因为类中实现每个参数是类的self,不是接口的self,所以要创建一个函数定义来转换
 * 5.类中的方法参数符号如何取得后传给要创建的函数是一个难点，这样转一道，性能也会有损失
 * 6.取参数符号见writeToFile
 * 7.在类实现中创建如下原代码_from_Abc表示从那个类实现的，之前的_Z5Zhong7txyopenEPN6_ZhongEw是接口的函数声明
 * static void  _Z5Zhong7txyopenEPN6_ZhongEw_from_Abc(Zhong * self,int parm1)
 *{
 *    Abc * randAbc123=(Abc *)container_of(self,Abc,ifaceZhong178827062);//把接口转成类
 *   _Z3Abc7txyopenEPN4_AbcEw(randAbc123,parm1);//这是类中定义的函数
 *}
 * 最后在类的初始化方法中把接口的函数指针指向_Z5Zhong7txyopenEPN6_ZhongEw_from_Abc
*Abc * Abc_init_1234ergR5678_Abc(Abc *self)
*{
*   AObject_init_1234ergR5678_AObject((AObject *)self);
*   self->_Z3Abc3AbcEPN4_AbcE=_Z3Abc3AbcEPN4_AbcE;
*   ((Zhong *)&self->ifaceZhong178827062)->_Z5Zhong7txyopenEPN6_ZhongEw=_Z5Zhong7txyopenEPN6_ZhongEw_from_Abc;
*   return self;
*}
*ifaceZhong178827062 系统创建的类中接口变量名，统一由aet_utils_create_in_class_iface_var创建。
*/

static void classInterfaceInit(ClassInterface *self)
{
}


typedef struct _ImplIface{
	 char *className;
	 char *classMangleName;
	 char *iface;
	 char *ifaceOrgiName;
	 char *ifaceMangleName;
	 int error;
	 tree implDefine;
}ImplIface;



static void freeFaceData_cb(ImplIface *item)
{
	if(item->className!=NULL){
		n_free(item->className);
	}
	if(item->classMangleName!=NULL){
		n_free(item->classMangleName);
	}
	if(item->iface!=NULL){
		n_free(item->iface);
	}
	if(item->ifaceOrgiName!=NULL){
		n_free(item->ifaceOrgiName);
	}
	if(item->ifaceMangleName!=NULL){
		n_free(item->ifaceMangleName);
	}
	n_slice_free(ImplIface,item);
}


/**
 * 产生式 id,id
 * 解析 implements$ Iface1,Iface2
 */
int  class_interface_parser(ClassInterface *self,ClassInfo *info,char **interface)
{
	  c_parser *parser=self->parser;
	  int i;
	  //char  *interface[20];
	  int count=0;
	  for(i=0;i<20;i++){
		  if (c_parser_next_token_is (parser, CPP_NAME)){
			  tree id=c_parser_peek_token(parser)->value;
			  c_parser_consume_token (parser);
			  interface[count++]=n_strdup(IDENTIFIER_POINTER(id));
		      if(c_parser_next_token_is (parser, CPP_LESS)){
		    	 ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),IDENTIFIER_POINTER(id));
		    	 if(className==NULL)
		    		 n_error("找不到接口");
		    	 GenericModel *model=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
		    	 class_info_add_iface_generic_model(info,className,model);
		      }

		  }else if(c_parser_next_token_is (parser, CPP_COMMA)){
			  c_parser_consume_token (parser);
			  if (!c_parser_next_token_is (parser, CPP_NAME)){
 	             error("逗号后是接口名。");
			  }
			  tree id=c_parser_peek_token(parser)->value;
			  c_parser_consume_token (parser);
			  interface[count++]=n_strdup(IDENTIFIER_POINTER(id));
		  }else if (c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
			  break;
		  }else{
	          error("出错了。");
		  }
		  if(count>AET_MAX_INTERFACE){
	             error("实现的接口不能超过%qd个。",AET_MAX_INTERFACE);
	             return 0;

		  }
	  }
	  for(i=0;i<count;i++){
	      n_debug("interface is :%d %s\n",i,interface[i]);
	  }
	  return count;
}


/////////////////////////////////////新实现----------------------

typedef struct _ImplIfaceFunc{
	 ClassName *from;       //声明接口来自那个类
	 char      *atSysName;  //找到接口实现在那个类
	 ClassFunc *implFunc;    //实现接口的类方法
	 ClassName *iface;
	 ClassFunc *interfaceMethod; //接口声明的方法
}ImplIfaceFunc;

static void addImplMethod(NPtrArray *array,ClassName *from,char *atSysName,ClassFunc *implFunc,ClassName *iface,ClassFunc *interfaceMethod)
{
	ImplIfaceFunc *data=(ImplIfaceFunc *)n_slice_new0(ImplIfaceFunc);
	data->from=from;
	data->atSysName=atSysName;
	data->implFunc=implFunc;
	data->iface=iface;
	data->interfaceMethod=interfaceMethod;
	n_ptr_array_add(array,data);
}

/**
 * 从当前类实现中找到接口的实现所在的类和函数定义或域声明
 */
static void eachInterface(ClassInterface *self,NPtrArray *array,ClassName *from,ClassName *iface)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),iface);
   NPtrArray *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
   if(ifaceFuncsArray==NULL || ifaceFuncsArray->len==0)
	   return;
   int i=0;
   for(i=0;i<ifaceFuncsArray->len;i++){
	   ClassFunc *interfaceMethod=(ClassFunc *)n_ptr_array_index(ifaceFuncsArray,i);
	   if(class_func_is_interface_reserve(interfaceMethod)) //是接口需要保留的ref和unref
		   continue;
	   char *atSysName=NULL;
	   ClassFunc    *impl=func_mgr_get_interface_impl(func_mgr_get(),from, interfaceMethod,&atSysName);//获得接口的实现类和方法
	   if(impl==NULL && !class_info_is_abstract_class(info)){
			error("类%qs没有实现接口%qs的方法%qs。",from->userName,iface->userName,interfaceMethod->orgiName);
	   }else if(impl==NULL && class_info_is_abstract_class(info)){
		    n_warning("抽象类%s没有实现接口%s的方法%s。",from->userName,iface->userName,interfaceMethod->orgiName);
	   }else{
		   addImplMethod(array,from,atSysName,impl,iface,interfaceMethod);
	   }
   }
}

/**
 * 创建接口方法赋值代码
 * Create interface method assignment code
 *并加入代码字符串到数组中
 *这此代码比如:
 *((test_RandomGenerator *)&self->ifaceRandomGenerator2066046634)->_Z15RandomGenerator7nextIntEPN15RandomGeneratorE=_Z7ARandom7nextIntEPN7ARandomE;
 *((test_RandomGenerator *)&self->ifaceRandomGenerator2066046634)->super_RandomGenerator__Z15RandomGenerator7nextIntEPN15RandomGeneratorE=_Z7ARandom7nextIntEPN7ARandomE;
 *最终加入到初始化代码中
 *void * test_ARandom_init_1234ergR5678_test_ARandom(test_ARandom *self)
 *
 */
static void createInterfaceMethodAssignmentCode(ImplIfaceFunc *item,NPtrArray *array)
{
	//给接口函数赋值
	//形式如下
	 char ifaceVar[255];
	 aet_utils_create_in_class_iface_var(item->iface->userName,ifaceVar);//一定与classparser中创建的接口变量名相同
 	 //创建为super引用的函数地址
	 //zclei super
 	 //char superFieldName[255];
 	 //aet_utils_create_super_field_name(item->iface->userName,item->interfaceMethod->mangleFunName,superFieldName);
	 NString *modify=n_string_new("");
	 char *funcName=NULL;
	 if(strcmp(item->from->sysName,item->atSysName)==0){
		 if(!aet_utils_valid_tree(item->implFunc->fromImplDefine)){
			 funcName=n_strdup_printf("self->%s",item->implFunc->mangleFunName);
		 }else{
			 funcName=n_strdup(item->implFunc->mangleFunName);
		 }
	 }else{
		 if(!aet_utils_valid_tree(item->implFunc->fromImplDefine)){
			 funcName=n_strdup_printf("((%s *)self)->%s",item->atSysName,item->implFunc->mangleFunName);
		 }else{
			 funcName=n_strdup(item->implFunc->mangleFunName);
		 }
	 }
	 n_string_append_printf(modify,"((%s *)&self->%s)->%s=%s;\n",item->iface->sysName,ifaceVar,
			 item->interfaceMethod->mangleFunName,funcName);
     //zclei super 1行
	 //n_string_append_printf(modify,"((%s *)&self->%s)->%s=%s;\n",item->iface->sysName,ifaceVar, superFieldName,funcName);
	 n_free(funcName);
//
//	 n_string_append_printf(modify,"((%s *)&self->%s)->%s",item->iface->sysName,ifaceVar,item->ifaceMangleName);
//	 n_string_append_printf(modify,"=%s;\n",item->classMangleName);
// 	 //创建为super引用的函数地址
// 	 char superFieldName[255];
// 	 aet_utils_create_super_field_name(item->iface->userName,item->ifaceMangleName,superFieldName);
//	 n_string_append_printf(modify,"((%s *)&self->%s)->%s",item->iface->sysName,ifaceVar,superFieldName);
//	 n_string_append_printf(modify,"=%s;\n",item->classMangleName);
 	 n_debug("class_interface 获取的接口赋值原代码：%s\n",modify->str);
 	 IFaceSourceCode *code=(IFaceSourceCode *)n_slice_new(IFaceSourceCode);
 	 code->define=NULL;
 	 code->modify=n_strdup(modify->str);
 	 n_string_free(modify,TRUE);
 	 n_ptr_array_add(array,code);
}

static void freeImplIfaceFunc_cb(ImplIfaceFunc *item)
{
	if(item->atSysName!=NULL){
		n_free(item->atSysName);
	}
	n_slice_free(ImplIfaceFunc,item);
}

/**
 * 生成代码:
 * self->ifaceRandomGenerator2066046634._iface_common_var._atClass1234=(void*)self;
 */
static void assignmentAtClass(ClassInfo *info,NPtrArray *array)
{
	 int i;
	 for(i=0;i<info->ifaceCount;i++){
		   ClassName iface=info->ifaces[i];
		   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
		   char ifaceVar[255];
		   aet_utils_create_in_class_iface_var(faceInfo->className.userName,ifaceVar);//一定与classparser中创建的接口变量名相同
		   IFaceSourceCode *code=(IFaceSourceCode *)n_slice_new(IFaceSourceCode);
		   code->define=NULL;
		   code->modify=n_strdup_printf("self->%s.%s.%s=(void *)self;\n",ifaceVar,IFACE_COMMON_STRUCT_VAR_NAME,IFACE_AT_CLASS);
		   n_ptr_array_add(array,code);
	 }
}

static char *getRefOrUnrefMangleName(ClassInfo *info,char *refOrUnref)
{
	NPtrArray  *array=func_mgr_get_funcs(func_mgr_get(),&info->className);
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *func=n_ptr_array_index(array,i);
		if(strcmp(func->orgiName,refOrUnref)==0)
			return func->mangleFunName;
	}
	return NULL;

}

/**
 * 给接口方法ref和unref赋值
 * object->ifaceVar._iface_reserve_ref_field_123=_iface_reserve_ref_func_define_123;
 * object->ifaceVar._iface_reserve_unref_field_123=_iface_reserve_unref_func_define_123;
 */
static void assignmentIfaceRefFunction(ClassInfo *info,NPtrArray *array)
{
	 int i;
	 for(i=0;i<info->ifaceCount;i++){
		   ClassName iface=info->ifaces[i];
		   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
		   char ifaceVar[255];
		   aet_utils_create_in_class_iface_var(faceInfo->className.userName,ifaceVar);//一定与classparser中创建的接口变量名相同

		   char *refMangle=getRefOrUnrefMangleName(faceInfo,IFACE_REF_FIELD_NAME);
		   char *unRefMangle=getRefOrUnrefMangleName(faceInfo,IFACE_UNREF_FIELD_NAME);
		   if(refMangle==NULL || unRefMangle==NULL){
			   n_error("报告此错误，在接口%s找不到方法 %s %s",faceInfo->className.userName,IFACE_REF_FIELD_NAME,IFACE_UNREF_FIELD_NAME);
			   return;
		   }
           //zclei super
//		   char superRefFieldName[255];
//		   aet_utils_create_super_field_name(faceInfo->className.userName,refMangle,superRefFieldName);
//		   char superUnrefFieldName[255];
//		   aet_utils_create_super_field_name(faceInfo->className.userName,unRefMangle,superUnrefFieldName);



		   IFaceSourceCode *code=(IFaceSourceCode *)n_slice_new(IFaceSourceCode);
		   code->define=NULL;
		   code->modify=n_strdup_printf("self->%s.%s=%s;\n",ifaceVar,refMangle,IFACE_REF_FUNC_DEFINE_NAME);
		   n_ptr_array_add(array,code);
           //zclei super
//		   code=n_slice_new(IFaceSourceCode);
//		   code->define=NULL;
//		   code->modify=n_strdup_printf("self->%s.%s=%s;\n",ifaceVar,superRefFieldName,IFACE_REF_FUNC_DEFINE_NAME);
//		   n_ptr_array_add(array,code);

		   code=(IFaceSourceCode *)n_slice_new(IFaceSourceCode);
		   code->define=NULL;
		   code->modify=n_strdup_printf("self->%s.%s=%s;\n",ifaceVar,unRefMangle,IFACE_UNREF_FUNC_DEFINE_NAME);
		   n_ptr_array_add(array,code);

		   //zclei super
//		   code=n_slice_new(IFaceSourceCode);
//		   code->define=NULL;
//		   code->modify=n_strdup_printf("self->%s.%s=%s;\n",ifaceVar,superUnrefFieldName,IFACE_UNREF_FUNC_DEFINE_NAME);
//		   n_ptr_array_add(array,code);
	 }
}

/**
 * 给接口变量AET_MAGIC_NAME设置魔数AET_MAGIC_NAME_VALUE
 * 接口的magic是AET_MAGIC_NAME_VALUE+1
 */
static void assignmentMagic(ClassInfo *info,NPtrArray *array)
{
        int i;
        for(i=0;i<info->ifaceCount;i++){
              ClassName iface=info->ifaces[i];
              ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
              char ifaceVar[255];
              aet_utils_create_in_class_iface_var(faceInfo->className.userName,ifaceVar);//一定与classparser中创建的接口变量名相同
              IFaceSourceCode *code=(IFaceSourceCode *)n_slice_new(IFaceSourceCode);
              code->define=NULL;
              code->modify=n_strdup_printf("self->%s.%s.%s=%d;\n",ifaceVar,IFACE_COMMON_STRUCT_VAR_NAME,AET_MAGIC_NAME,AET_IFACE_MAGIC_NAME_VALUE);
              n_ptr_array_add(array,code);
        }
}

/**
 * 在classimpl最后把接口的方法找到可以给它赋值的类方法。
 */
NPtrArray *class_interface_add_define(ClassInterface *self,ClassName *className)
{
   if(class_mgr_is_interface(class_mgr_get(),className))
	   return NULL;
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info==NULL || info->ifaceCount<=0)
	   return NULL;
   int i;
   NPtrArray *array=n_ptr_array_new();//循环处理每个接口
   for(i=0;i<info->ifaceCount;i++){
	   ClassName iface=info->ifaces[i];
	   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
	   if(faceInfo==NULL){
		   n_ptr_array_set_free_func(array,freeFaceData_cb);
		   n_ptr_array_unref(array);
	       error("在类%qs中实现接口%qs,但没有找到接口的声明。检查是否包含对应的头文件",className->userName,iface.userName);
		   return NULL;
	   }
	   eachInterface(self,array,className,&iface);
   }
   NPtrArray *ifaceResult=n_ptr_array_new();
   int errorCount=0;
   for(i=0;i<array->len;i++){
	   ImplIfaceFunc *item=(ImplIfaceFunc *)n_ptr_array_index(array,i);
	   createInterfaceMethodAssignmentCode(item,ifaceResult);
   }
   assignmentMagic(info,ifaceResult);//给接口设魔数
   assignmentAtClass(info,ifaceResult);//给接口的变量_iface_common_var的域成员_atClass1234赋值=(void*)self;
   assignmentIfaceRefFunction(info,ifaceResult);//给接口的变量_iface_common_var的域成员ref赋值=_iface_ref_function_123
   n_ptr_array_set_free_func(array,freeImplIfaceFunc_cb);
   n_ptr_array_unref(array);
   return ifaceResult;
}

/**
 * 加二个变量二个方法
 * 1.private$ int _aet_magic$_123;
 * 2.private$ IfaceCommonData1234 _iface_common_var;
 * 3.private$ IfaceCommonData1234 *_iface_reserve_ref_function_123();
 * 4.private$ void _iface_reserve_unref_function_123();
 */
void class_interface_add_var_ref_unref_methodxxrrrr0000(ClassInterface *self)
{
  c_parser *parser=self->parser;
  location_t  loc = c_parser_peek_token(parser)->location;
  int tokenCount=parser->tokens_avail;
  if(tokenCount+17>AET_MAX_TOKEN){
		error("token太多了");
		return FALSE;
  }
  int i;
  for(i=tokenCount;i>0;i--){
     aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+17]);
  }
  /**
   * AET_MAGIC_NAME                       "_aet_magic$_123"
   * 在AObject.h中定义了函数
   * IFACE_REF_FUNCTION_NAME            "_iface_reserve_ref_function_123"
   * IFACE_UNREF_FUNCTION_NAME          "_iface_reserve_unref_function_123"
   */

   aet_utils_create_int_token(&parser->tokens[0],input_location);
   aet_utils_create_token(&parser->tokens[1],CPP_NAME,AET_MAGIC_NAME,strlen(AET_MAGIC_NAME));
   aet_utils_create_token(&parser->tokens[2],CPP_SEMICOLON,";",1);

   aet_utils_create_token(&parser->tokens[3],CPP_NAME,IFACE_COMMON_STRUCT_NAME,strlen(IFACE_COMMON_STRUCT_NAME));
   parser->tokens[3].id_kind=C_ID_TYPENAME;//关键
   aet_utils_create_token(&parser->tokens[4],CPP_NAME,IFACE_COMMON_STRUCT_VAR_NAME,strlen(IFACE_COMMON_STRUCT_VAR_NAME));
   aet_utils_create_token(&parser->tokens[5],CPP_SEMICOLON,";",1);

   aet_utils_create_token(&parser->tokens[6],CPP_NAME,IFACE_COMMON_STRUCT_NAME,strlen(IFACE_COMMON_STRUCT_NAME));
   parser->tokens[6].id_kind=C_ID_TYPENAME;//关键
   aet_utils_create_token(&parser->tokens[7],CPP_MULT,"*",1);
   aet_utils_create_token(&parser->tokens[8],CPP_NAME,IFACE_REF_FIELD_NAME,strlen(IFACE_REF_FIELD_NAME));
   aet_utils_create_token(&parser->tokens[9],CPP_OPEN_PAREN,"(",1);
   aet_utils_create_token(&parser->tokens[10],CPP_CLOSE_PAREN,")",1);
   aet_utils_create_token(&parser->tokens[11],CPP_SEMICOLON,";",1);

   aet_utils_create_void_token(&parser->tokens[12],input_location);
   aet_utils_create_token(&parser->tokens[13],CPP_NAME,IFACE_UNREF_FIELD_NAME,strlen(IFACE_UNREF_FIELD_NAME));
   aet_utils_create_token(&parser->tokens[14],CPP_OPEN_PAREN,"(",1);
   aet_utils_create_token(&parser->tokens[15],CPP_CLOSE_PAREN,")",1);
   aet_utils_create_token(&parser->tokens[16],CPP_SEMICOLON,";",1);
   parser->tokens_avail=tokenCount+17;
   aet_print_token_in_parser("class_interface_add_var_ref_unref_method ----");
   return TRUE;
}

/**
 * interface$ XXX{
 * };
 * 以classparser.c中调用该方法给接口加入第一个变量IfaceCommonData123。
 * 所以接口名XXX也可强转成IfaceCommonData123.
 * 生成的接口内部声明如下:
 * interface$ IFACE{
 *   IfaceCommonData123 _iface_common_var;
 *   IfaceCommonData123 *_iface_reserve_ref_field_123();
 *   void _iface_reserve_unref_field_123();
 *   //以面是用户的代码。
 * }
 */
void class_interface_add_var_ref_unref_method(ClassInterface *self)
{
  c_parser *parser=self->parser;
  location_t  loc = c_parser_peek_token(parser)->location;
  int tokenCount=parser->tokens_avail;
  if(tokenCount+14>AET_MAX_TOKEN){
        error("token太多了");
        return FALSE;
  }
  int i;
  for(i=tokenCount;i>0;i--){
         aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+14]);
  }


  aet_utils_create_token(&parser->tokens[0],CPP_NAME,IFACE_COMMON_STRUCT_NAME,strlen(IFACE_COMMON_STRUCT_NAME));
  parser->tokens[0].id_kind=C_ID_TYPENAME;//关键
  aet_utils_create_token(&parser->tokens[1],CPP_NAME,IFACE_COMMON_STRUCT_VAR_NAME,strlen(IFACE_COMMON_STRUCT_VAR_NAME));
  aet_utils_create_token(&parser->tokens[2],CPP_SEMICOLON,";",1);

  aet_utils_create_token(&parser->tokens[3],CPP_NAME,IFACE_COMMON_STRUCT_NAME,strlen(IFACE_COMMON_STRUCT_NAME));
  parser->tokens[3].id_kind=C_ID_TYPENAME;//关键
  aet_utils_create_token(&parser->tokens[4],CPP_MULT,"*",1);
  aet_utils_create_token(&parser->tokens[5],CPP_NAME,IFACE_REF_FIELD_NAME,strlen(IFACE_REF_FIELD_NAME));
  aet_utils_create_token(&parser->tokens[6],CPP_OPEN_PAREN,"(",1);
  aet_utils_create_token(&parser->tokens[7],CPP_CLOSE_PAREN,")",1);
  aet_utils_create_token(&parser->tokens[8],CPP_SEMICOLON,";",1);

  aet_utils_create_void_token(&parser->tokens[9],input_location);
  aet_utils_create_token(&parser->tokens[10],CPP_NAME,IFACE_UNREF_FIELD_NAME,strlen(IFACE_UNREF_FIELD_NAME));
  aet_utils_create_token(&parser->tokens[11],CPP_OPEN_PAREN,"(",1);
  aet_utils_create_token(&parser->tokens[12],CPP_CLOSE_PAREN,")",1);
  aet_utils_create_token(&parser->tokens[13],CPP_SEMICOLON,";",1);
  parser->tokens_avail=tokenCount+14;
  aet_print_token_in_parser("class_interface_add_var_ref_unref_method ----");
  return TRUE;
}

/**
 * 生成
 * ((AObject*)expr->_iface_common_var._atClass123);
 * 或
 *  ((AObject*)expr._iface_common_var._atClass123);
 *  最后调用->unref();
 */
static tree buildInterfaceToAObject(tree expr,nboolean isRef)
{
	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
	tree id=aet_utils_create_ident(rootClassName->sysName);
	tree aobjectType=lookup_name(id);
    aobjectType=TREE_TYPE(aobjectType);
    tree pointerType=build_pointer_type(aobjectType);
	location_t loc=input_location;
	tree rootref=expr;
	if(isRef)
	  rootref=build_indirect_ref (loc,expr,RO_ARROW);
	tree _iface_common_var=aet_utils_create_ident(IFACE_COMMON_STRUCT_VAR_NAME);
    tree componetRefTwo = build_component_ref (loc,rootref,_iface_common_var, loc);//
	tree atClassVarName=aet_utils_create_ident(IFACE_AT_CLASS);
    tree componetRefOne = build_component_ref (loc,componetRefTwo,atClassVarName, loc);//
	tree nopExpr= build1 (NOP_EXPR, pointerType, componetRefOne);
	return nopExpr;
}

/**
 * 生成
 * ((IfaceCommonData123*)&expr->_iface_common_var);
 * 或
 * ((IfaceCommonData123*)&expr._iface_common_var);
 *  最后调用->unref();
 */
static tree buildInterfaceToIfaceCommonData123(tree expr,nboolean isRef)
{
	tree id=aet_utils_create_ident(IFACE_COMMON_STRUCT_NAME);
	tree ifaceCommonType=lookup_name(id);
	ifaceCommonType=TREE_TYPE(ifaceCommonType);
    tree pointerType=build_pointer_type(ifaceCommonType);
	location_t loc=input_location;
	tree rootref=expr;
	if(isRef)
	  rootref=build_indirect_ref (loc,expr,RO_ARROW);
	tree _iface_common_var=aet_utils_create_ident(IFACE_COMMON_STRUCT_VAR_NAME);
    tree componetRef= build_component_ref (loc,rootref,_iface_common_var, loc);//
	tree nopExpr= build1 (NOP_EXPR, pointerType, componetRef);
	return nopExpr;
}

/**
 * 判断是不是接口调用了ref方法
 */
static nboolean isCallRefOrUnrefByInterface(tree component,tree expr,char *refOrUnref)
{
  tree type=TREE_TYPE(expr);
  char *sysName=class_util_get_class_name(type);
  if(sysName!=NULL){
	  ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	  if(info && class_info_is_interface(info) && TREE_CODE(component)==IDENTIFIER_NODE && !strcmp(refOrUnref,IDENTIFIER_POINTER(component))){
		return TRUE;
	  }
  }
  return FALSE;
}

/**
 * 把接口引用的ref unref替换为_iface_reserve_ref_field_123或_iface_reserve_unref_field_123
 */
static tree replaceRefOrUnref(tree component,tree ref)
{
	if(isCallRefOrUnrefByInterface(component,ref,"ref")){
		tree id=aet_utils_create_ident(IFACE_REF_FIELD_NAME);
		return id;
	}else if(isCallRefOrUnrefByInterface(component,ref,"unref")){
	    tree id=aet_utils_create_ident(IFACE_UNREF_FIELD_NAME);
	    return id;
	}else{
		return component;
	}
}

/**
 * Iface *var;
 * var->ref();
 * 或
 * Iface var;
 * var.unref();
 * ref或unref变成 需要转成
 * #define IFACE_REF_FIELD_NAME            "_iface_reserve_ref_field_123"
 *#define IFACE_UNREF_FIELD_NAME          "_iface_reserve_unref_field_123"
 */
tree  class_interface_transport_ref_and_unref(ClassInterface *self,tree component,tree expr)
{
	c_parser *parser=self->parser;
	c_token *closeParen=c_parser_peek_2nd_token (parser);//")";
	if(closeParen->type!=CPP_CLOSE_PAREN)
		return component;
	tree type=TREE_TYPE(expr);
    nboolean isPointer=(TREE_CODE(type)==POINTER_TYPE);
	if(TREE_CODE(expr)==PARM_DECL || TREE_CODE(expr)==VAR_DECL){
		return replaceRefOrUnref(component,expr);
	}else if(TREE_CODE(expr)==COMPONENT_REF){
		tree field=TREE_OPERAND (expr, 1);
		return replaceRefOrUnref(component,field);
	}
    return component;
}


ClassInterface *class_interface_new()
{
	ClassInterface *self = n_slice_alloc0 (sizeof(ClassInterface));
	classInterfaceInit(self);
	return self;
}





