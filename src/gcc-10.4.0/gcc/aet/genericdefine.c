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
#include "genericdefine.h"


static SetBlockHelp  *createSetBlockHelp()
{
	SetBlockHelp *help=n_slice_new(SetBlockHelp);
	help->codes=n_string_new("");
	help->setBlockFuncsArray=n_ptr_array_new();
	help->blockFuncsArray=n_ptr_array_new();
	help->implSetBlockFuncArray=n_ptr_array_new();
	help->collectDefines=n_ptr_array_new();
	help->collectFuncCallDefines=n_ptr_array_new();
	return help;
}


static void genericDefineInit(GenericDefine *self)
{
	self->newObjectArray=n_ptr_array_new();
	self->genericClassArray=n_ptr_array_new();

	self->funcCallArray=n_ptr_array_new();
	self->genericMethodArray=n_ptr_array_new();

	self->help=createSetBlockHelp();

}

static GenericClass *getGenericClass(NPtrArray *array,char *sysName)
{
   int i;
   for(i=0;i<array->len;i++){
	   GenericClass *item=n_ptr_array_index(array,i);
	   if(strcmp(item->sysName,sysName)==0)
		   return item;
   }
   return NULL;
}

static GenericMethod *getGenericMethod(NPtrArray *array,char *sysName,char *funcName)
{
   int i;
   for(i=0;i<array->len;i++){
	   GenericMethod *item=n_ptr_array_index(array,i);
	   if(generic_method_equal(item,sysName,funcName))
		   return item;
   }
   return NULL;
}


/**
 * 从help中生成所有块函数的声明代码。
 */
/**
 * 从help中生成所有块函数类型声明。
 */
/**
 * 从help中生成所有设置泛型函数的声明代码
 */
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
/**
 *  生成泛型块函数中的类型，从创建generic_define_create_generic_class开始。
 */
static void printDefine(GenericClass *genClass,NString *printStr)
{
	n_string_append_printf(printStr,"类名:%s ",genClass->sysName);
	if(genClass->definesArray->len==0){
		n_string_append(printStr,"无泛型!");
	}else{
		int i;
		n_string_append(printStr,"泛型定义如下：");
		for(i=0;i<genClass->definesArray->len;i++){
		   GenericModel *defines=(GenericModel *)n_ptr_array_index(genClass->definesArray,i);
		   char     *var=generic_model_define_str(defines);
		   n_string_append_printf(printStr," %d、 %s。",i+1,var);
		   n_free(var);
		}
	}
}

static void printMethodDefine(GenericMethod *method,NString *printStr)
{
	n_string_append_printf(printStr,"调用的泛型方法 :%s %s ",method->sysName,method->funcName);
	if(method->definesArray->len==0){
		n_string_append(printStr,"无泛型!");
	}else{
		int i;
		n_string_append(printStr,"泛型定义如下：");
		for(i=0;i<method->definesArray->len;i++){
		   GenericModel *defines=(GenericModel *)n_ptr_array_index(method->definesArray,i);
		   char     *var=generic_model_define_str(defines);
		   n_string_append_printf(printStr," %d、 %s。",i+1,var);
		   n_free(var);
		}
	}
}

static void printLastClass(NPtrArray *array)
{
    if(!n_log_is_debug())
        return;
	NString *cd=n_string_new("");
	int i;
	for(i=0;i<array->len;i++){
		GenericClass *item=n_ptr_array_index(array,i);
		printDefine(item,cd);
		n_string_append(cd,"\n");
	}
	printf("%s\n",cd->str);
}

static void printLastMethod(NPtrArray *array)
{
    if(!n_log_is_debug())
        return;
	NString *cd=n_string_new("");
	int i;
	for(i=0;i<array->len;i++){
		GenericMethod *item=n_ptr_array_index(array,i);
		printMethodDefine(item,cd);
		n_string_append(cd,"\n");
	}
	printf("%s\n",cd->str);
}

/**
 * 创建设置泛型块的回调函数 _setGenericBlockFunc_123_cb
 */
static NString *create_setGenericBlockFunc_CallBack_Codes(NPtrArray *collectClass,NPtrArray *blockFuncsArray)
{
    NString *codes=n_string_new("");
	n_string_append_printf(codes,"static void %s(void *self,void *object,char *sysName,int pointer)\n",AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	n_string_append(codes,"{\n");
	int i;
	for(i=0;i<collectClass->len;i++){
		GenericClass *item=n_ptr_array_index(collectClass,i);
		char *cc=generic_class_create_fragment(item,blockFuncsArray);
		if(cc==NULL){
			n_error("不应该出现这样的情况。%s\n",item->sysName);
		}
		n_string_append(codes,cc);
		if(i<collectClass->len-1){
			n_string_append(codes,"else ");
		}
		n_free(cc);
	}
	n_string_append(codes,"}\n");
	n_debug("生成的赋值函数是:%s\n",codes->str);
//    char *result=n_strdup(codes->str);
//    n_string_free(codes,TRUE);
//    return result;
    return codes;
}

static NPtrArray *mergeAtClass(NPtrArray *collectClass1,NPtrArray *collectClass2)
{
	NPtrArray *newArray=n_ptr_array_new();
	int i;
	for(i=0;i<collectClass1->len;i++){
		 GenericClass *item=n_ptr_array_index(collectClass1,i);
		 GenericClass *gc=getGenericClass(newArray,item->sysName);
		 if(gc!=NULL){
			 n_error("不应该有重复的类在这里00:%s\n",item->sysName);
		 }
	     n_ptr_array_add(newArray,item);
	}
	for(i=0;i<collectClass2->len;i++){
		 GenericClass *item=n_ptr_array_index(collectClass2,i);
		 GenericClass *gc=getGenericClass(newArray,item->sysName);
		 if(gc!=NULL){
			 n_warning("不应该有重复的类在这里11:%s",item->sysName);
			 generic_class_add_defines_array(gc,item->definesArray);
		 }else{
	         n_ptr_array_add(newArray,item);
		 }
	}
	return newArray;
}

static NPtrArray *mergeClassByFuncCall(NPtrArray *collectClass1,NPtrArray *collectClass2)
{
	NPtrArray *newArray=n_ptr_array_new();
	int i;
	for(i=0;i<collectClass1->len;i++){
		 GenericClass *item=n_ptr_array_index(collectClass1,i);
		 GenericClass *gc=getGenericClass(newArray,item->sysName);
		 if(gc!=NULL){
			 generic_class_add_defines_array(gc,item->definesArray);
		 }else{
	        n_ptr_array_add(newArray,item);
		 }
	}
	for(i=0;i<collectClass2->len;i++){
		 GenericClass *item=n_ptr_array_index(collectClass2,i);
		 GenericClass *gc=getGenericClass(newArray,item->sysName);
		 if(gc!=NULL){
			 generic_class_add_defines_array(gc,item->definesArray);
		 }else{
		     n_ptr_array_add(newArray,item);
		 }
	}
	return newArray;
}

static NPtrArray *mergeMethod(NPtrArray *collectClass1,NPtrArray *collectClass2)
{
	NPtrArray *newArray=n_ptr_array_new();
	int i;
	for(i=0;i<collectClass1->len;i++){
		 GenericMethod *item=n_ptr_array_index(collectClass1,i);
		 GenericMethod *gc=getGenericMethod(newArray,item->sysName,item->funcName);
		 if(gc!=NULL){
			 generic_method_add_defines_array(gc,item->definesArray);
		 }else{
	        n_ptr_array_add(newArray,item);
		 }
	}
	for(i=0;i<collectClass2->len;i++){
		GenericMethod *item=n_ptr_array_index(collectClass2,i);
		GenericMethod *gc=getGenericMethod(newArray,item->sysName,item->funcName);
		 if(gc!=NULL){
			 generic_method_add_defines_array(gc,item->definesArray);
		 }else{
	        n_ptr_array_add(newArray,item);
		 }
	}
	return newArray;
}

/**
 * 合并调用方法的类到收集的类集合中。这样不会漏掉非泛型类但有泛型函数的类的AET_SET_BLOCK_FUNC_CALLBACK_NAME   "_setGenericBlockFunc_123_cb
 */
static NPtrArray *mergeClassAndMethodToClass(NPtrArray *classCollect,NPtrArray *methodCollect)
{
	NPtrArray *newArray=n_ptr_array_new();
	int i;
	for(i=0;i<classCollect->len;i++){
		 GenericClass *item=n_ptr_array_index(classCollect,i);
		 GenericClass *gc=getGenericClass(newArray,item->sysName);
		 if(gc==NULL){
	        n_ptr_array_add(newArray,item);
		 }
	}
	for(i=0;i<methodCollect->len;i++){
		 GenericMethod *item=n_ptr_array_index(methodCollect,i);
		 GenericClass *gc=getGenericClass(newArray,item->sysName);
		 if(gc==NULL){
			 gc=generic_class_new(item->sysName,NULL);
		     n_ptr_array_add(newArray,gc);
		 }
	}
	return newArray;
}



static char *createBlockFuncDeclCodes(NPtrArray *blockFuncsArray)
{
  int i;
  NString *funcDeclStr=n_string_new("");
  for(i=0;i<blockFuncsArray->len;i++){
		BlockFunc *item=n_ptr_array_index(blockFuncsArray,i);
		if(!block_func_have_func_decl_tree(item)){
		   n_string_append_printf(funcDeclStr,"%s\n",item->funcDefineDecl);
		}else{
			n_error("不应该在这之前有函数块声明:%s funcName:%s!。",item->funcDefineDecl,item->funcDefineName);
		}
  }
  char *result=NULL;
  if(funcDeclStr->len>0){
	  result=n_strdup(funcDeclStr->str);
  }
  n_string_free(funcDeclStr,TRUE);
  return result;
}

static char *createBlockFuncTypedefCodes(NPtrArray *blockFuncsArray)
{
  int i;
  NString *typedefFuncDeclStr=n_string_new("");
  for(i=0;i<blockFuncsArray->len;i++){
	BlockFunc *item=n_ptr_array_index(blockFuncsArray,i);
	if(!block_func_have_typedef_func_decl(item)){
	   n_string_append_printf(typedefFuncDeclStr,"%s\n",item->typedefFuncDecl);
	}else{
		n_warning("不应该在这之前有函数类型声明:%s funcName:%s!。",item->typedefFuncDecl,item->typedefFuncName);
	}
  }
  char *result=NULL;
  if(typedefFuncDeclStr->len>0){
	  result=n_strdup(typedefFuncDeclStr->str);
  }
  n_string_free(typedefFuncDeclStr,TRUE);
  return result;
}

static char *createSetBlockFuncDecl()
{
	  int i;
	  NString *funcDeclStr=n_string_new("");
	  tree id=aet_utils_create_ident(AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	  tree func=lookup_name(id);
	  if(!aet_utils_valid_tree(func)){
	    n_string_append_printf(funcDeclStr,"static void %s(void *self,void *object,char *sysName,int pointer);\n",AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	  }else{
		n_warning("在文件%s中已经声明过设置泛型块函数的声明%s!。",in_fnames[0],AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	  }
	  char *result=NULL;
	  if(funcDeclStr->len>0){
		  result=n_strdup(funcDeclStr->str);
	  }
	  n_string_free(funcDeclStr,TRUE);
	  return result;
}

/**
 * 创建三种声明
 * 1.块函数声明
 * 2.块函数类型声明
 * 3.设置块函数的函数声明
 */
static char * createThreedeclarations(NPtrArray *blockFuncsArray)
{
   char *blocksDecl=createBlockFuncDeclCodes(blockFuncsArray);
   char *blocksTypedefFuncDecl=createBlockFuncTypedefCodes(blockFuncsArray);
   char *setBlockDecl=createSetBlockFuncDecl();

   //等上面的函数声明执行完成后，再加入块函数和设置块函数的实现代码
   NString *codes=n_string_new("");
   if(blocksTypedefFuncDecl!=NULL){
	   n_string_append(codes,blocksTypedefFuncDecl);
   }
   if(blocksDecl!=NULL){
	   n_string_append(codes,blocksDecl);
   }
   if(setBlockDecl!=NULL){
	   n_string_append(codes,setBlockDecl);
   }
   char *result=NULL;
   if(codes->len>0){
	   result=n_strdup(codes->str);
   }
   n_string_free(codes,TRUE);
   return result;
}

static void createByDefineClass(GenericDefine *self,NPtrArray **classOne,NPtrArray **classTwo,NPtrArray **funcCallFromClass)
{
	   int i;
	   for(i=0;i<self->newObjectArray->len;i++){
		 GenericNewClass *item=n_ptr_array_index(self->newObjectArray,i);
		 GenericClass *gc=getGenericClass(self->genericClassArray,item->sysName);
		 n_debug("createByDefineClass 00 i:%d 类名:%s 泛型定义:%s file:%s\n",i,item->sysName,generic_model_tostring(item->model),in_fnames[0]);
		 if(!gc){
			gc=generic_class_new(item->sysName,item->model);
			n_ptr_array_add(self->genericClassArray,gc);
		 }else{
			generic_class_add_defines(gc,item->model);
		 }
	   }
	   NPtrArray *collectClass1=n_ptr_array_new();
	   NPtrArray *collectClass2=n_ptr_array_new();
	   NPtrArray *collectClass3=n_ptr_array_new();
	   NPtrArray *collectClass4=n_ptr_array_new();

	   for(i=0;i<self->genericClassArray->len;i++){
			GenericClass *item=n_ptr_array_index(self->genericClassArray,i);
			n_debug("createByDefineClass 11 i:%d 类:%s item:%p file:%s\n",i,item->sysName,item,in_fnames[0]);
			generic_class_create_node(item);
			generic_class_recursion_node(item);
			n_debug("createByDefineClass 22 i:%d 类:%s item:%p 泛型函数 file:%s\n",i,item->sysName,item,in_fnames[0]);
			generic_class_recursion_method(item);
			n_debug("createByDefineClass 33 i:%d 类:%s item:%p 泛型函数 file:%s\n",i,item->sysName,item,in_fnames[0]);
			generic_class_collect_class(item,collectClass1);
			printLastClass(collectClass1);
			n_debug("createByDefineClass 44 i:%d 类:%s item:%p 泛型函数 file:%s\n",i,item->sysName,item,in_fnames[0]);
			generic_class_collect_method_class(item,collectClass2);
			generic_class_collect_method(item,collectClass3);
			generic_class_collect_method_class99(item,collectClass4);
	   }
	    n_debug("createByDefineClass 22 收集的new对象有：%d\n",collectClass1->len);
		printLastClass(collectClass1);
		n_debug("\n\n");
		n_debug("createByDefineClass 33 从泛型函数中收集的new对象有：%d\n",collectClass2->len);
		printLastClass(collectClass2);
		n_debug("\n\n");
		n_debug("createByDefineClass 44 从类中收集的调用泛型函数有：%d\n",collectClass3->len);
		printLastMethod(collectClass3);
		n_debug("createByDefineClass 55 从类中收集的调用泛型函数有：%d\n",collectClass4->len);
		printLastMethod(collectClass4);

		NPtrArray *methodArray=mergeMethod(collectClass3,collectClass4);


		*classOne=collectClass1;
		*classTwo=collectClass2;
		*funcCallFromClass=methodArray;//collectClass3;
		n_debug("createByDefineClass 66 从类中收集的调用泛型函数合并后有：%d\n",methodArray->len);
		printLastMethod(methodArray);

}

/**
 * 根据泛型函数调用，给未定泛型的类和泛型函数确值。
 * abc->getData<float>()，在函数内的类与函数被float替换
 */
static void createByDefineFuncCall(GenericDefine *self,NPtrArray **funcClass,NPtrArray **funcCallArray)
{
	int i;
    for(i=0;i<self->funcCallArray->len;i++){
		GenericCallFunc *item=n_ptr_array_index(self->funcCallArray,i);
		GenericMethod *gc=getGenericMethod(self->genericMethodArray,item->sysName,item->funcName);
		printf("createByDefineFuncCall 00 加入泛型定义到同一个对象上 i:%d GenericMethod:%p 类名:%s 泛型定义:%s\n",
				i,gc,item->sysName,generic_model_tostring(item->model));
		if(!gc){
			gc=generic_method_new(item->sysName,item->funcName,item->model);
			n_ptr_array_add(self->genericMethodArray,gc);
		}else{
			generic_method_add_defines(gc,item->model);
		}
   }
	   NPtrArray *collectClass1=n_ptr_array_new();
	   NPtrArray *collectClass2=n_ptr_array_new();
   for(i=0;i<self->genericMethodArray->len;i++){
		GenericMethod *item=n_ptr_array_index(self->genericMethodArray,i);
		generic_method_create_node(item);
		generic_method_recursion_node(item);
		printf("createByDefineFuncCall- 11 打印第%d个方法:%s %s 的详细信息----------%s\n\n",i,item->sysName,item->funcName,in_fnames[0]);
		generic_method_recursion_class(item);
		generic_method_collect_class(item,collectClass1);
		generic_method_collect(item,collectClass2);
   }

        n_debug("createByDefineFuncCall 22 收集的new对象有：%d\n",collectClass1->len);
   		printLastClass(collectClass1);
   		printf("\n\n");
   		n_debug("createByDefineFuncCall 33 从类中收集的调用泛型函数有：%d\n",collectClass2->len);
   		printLastMethod(collectClass2);
   		*funcClass=collectClass1;
   		*funcCallArray=collectClass2;

}

char *generic_define_create_generic_class(GenericDefine *self)
{
	   NPtrArray *classArray=NULL;
       NPtrArray *classArrayFromFuncCall=NULL;
       NPtrArray *classFuncCallArray=NULL;
       createByDefineClass(self, &classArray,&classArrayFromFuncCall,&classFuncCallArray);

	   NPtrArray *funcClassArray=NULL;
	   NPtrArray *funcCallArray=NULL;
	   createByDefineFuncCall(self,&funcClassArray,&funcCallArray);

	   NPtrArray *newClassArray=mergeAtClass(classArray,classArrayFromFuncCall);
	   NPtrArray *lastClassArray=mergeClassByFuncCall(newClassArray,funcClassArray);
	   NPtrArray *lastFuncCallArray=mergeMethod(classFuncCallArray,funcCallArray);
	   /*把收集的泛型类和泛型函数所在的类合并，泛型函数所在的类有可能不是泛型类，
	    * 但也需要设置泛型块的函数AET_SET_BLOCK_FUNC_CALLBACK_NAME _setGenericBlockFunc_123_cb
	    */
	   NPtrArray *allClassArray=mergeClassAndMethodToClass(lastClassArray,lastFuncCallArray);
	   NPtrArray *blockFuncsArray=n_ptr_array_new();
	   NString *setBlockFuncCodes=create_setGenericBlockFunc_CallBack_Codes(allClassArray,blockFuncsArray);
	   self->help->codes=setBlockFuncCodes;
	   self->help->blockFuncsArray=blockFuncsArray;
	   self->help->collectDefines=lastClassArray;
	   self->help->collectFuncCallDefines=lastFuncCallArray;
  	   n_debug("generic_define_create_generic_class 33 从类中收集的调用泛型函数有：%d\n",lastFuncCallArray->len);
	   if(!n_log_is_debug())
  		 printLastMethod(lastFuncCallArray);
	   char *declCodes=createThreedeclarations(blockFuncsArray);
	   return declCodes;
}



/**
 * 如果belongSysName==self 说明是在类中new$未定泛型类 如 Abc *abc=new$ Abc<E>();
 * self->_createAllGenericFunc567(efg,"Efg",0);
 */
void  generic_define_add_define_object(GenericDefine *self,ClassName *className,GenericModel * defines)
{
	GenericNewClass *obj=generic_new_class_new(className->sysName,defines);
	int i;
	for(i=0;i<self->newObjectArray->len;i++){
		GenericNewClass *item=n_ptr_array_index(self->newObjectArray,i);
		if(strcmp(item->id,obj->id)==0){
			generic_new_class_free(obj);
			return;
		}
	}
	n_ptr_array_add(self->newObjectArray,obj);
}

void generic_define_add_define_func_call(GenericDefine *self,ClassName *className,char *funcName,GenericModel * defines)
{
	GenericCallFunc *func=generic_call_func_new(className,funcName,defines);
	int i;
	for(i=0;i<self->funcCallArray->len;i++){
		GenericCallFunc *item=n_ptr_array_index(self->funcCallArray,i);
		if(strcmp(item->id,func->id)==0){
			generic_call_func_free(func);
			return;
		}
	}
	n_ptr_array_add(self->funcCallArray,func);
}

SetBlockHelp   *generic_define_get_help(GenericDefine *self)
{
	return self->help;
}

GenericDefine *generic_define_new()
{
	 GenericDefine *self = n_slice_alloc0 (sizeof(GenericDefine));
	 genericDefineInit(self);
	 return self;
}

