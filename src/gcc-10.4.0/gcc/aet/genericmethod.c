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
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "toplev.h"
#include "asan.h"
#include "opts.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "tree-iterator.h"
#include "fold-const.h"
#include "langhooks.h"
#include "aetutils.h"
#include "classmgr.h"
#include "parserhelp.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericclass.h"
#include "aet-typeck.h"
#include "aet-c-fold.h"
#include "classutil.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "funcmgr.h"
#include "genericmethod.h"



static void genericMethodInit(GenericMethod *self)
{
	self->definesArray=n_ptr_array_new();
	self->childs=n_ptr_array_new();
	self->haveAddChilds=FALSE;
	self->objectChilds=n_ptr_array_new();
	((GenericCM *)self)->type=PARENT_TYPE_METHOD;
	((GenericCM *)self)->parent=NULL;
}

static nboolean compare(GenericModel *src,GenericModel *dest)
{
	char *ss=generic_model_define_str(src);
	char *ds=generic_model_define_str(dest);
	nboolean eq=strcmp(ss,ds)==0;
	n_free(ss);
	n_free(ds);
	return eq;
}

/**
 * GenericMethod是否存在相同的泛型定义
 */
static nboolean existsDefine(GenericMethod *self,GenericModel *defines)
{
   int i;
   for(i=0;i<self->definesArray->len;i++){
	   GenericModel *item=n_ptr_array_index(self->definesArray,i);
	   if(compare(item,defines)){
		   return TRUE;
	   }
   }
   return FALSE;
}

static GenericMethod *getChild(GenericMethod *self,char *sysName,char *funcName)
{
   int i;
   for(i=0;i<self->childs->len;i++){
	   GenericMethod *item=n_ptr_array_index(self->childs,i);
	   if(generic_method_equal(item,sysName,funcName))
		   return item;
   }
   return NULL;
}

/**
 * 在外部文件如a.c中new$ Abc<int>对象
 * 在Abc类(b.c文件)中有一泛型未定义的类被创建如 AMutex *xx=new$ AMutex<E>()
 * 执行到这里，说明正在编译的是a.c文件，通过block_mgr_read_undefine_new_object知道
 * 在Abc类中有一个AMutex是未定义的。现在就把在a.c中创建Abc确定的泛型"设给"AMutex
 */
static GenericModel *getDefineFromObject(GenericMethod *self,GenericModel *origDecl,GenericModel *src,GenericModel *dest)
{
	      GenericModel *destModel = generic_model_new_from_file();
	      int genCount=generic_model_get_count(dest);
	      int i;
	      for(i=0;i<genCount;i++){
			   GenericUnit *unit=generic_model_get(dest,i);
			   if(generic_unit_is_undefine(unit)){
					int index=generic_model_get_index(origDecl,unit->name);
					if(index<0){
						n_warning("类%s中的方法%s没有声明此泛型单元 %s。",self->sysName,self->funcName,unit->name);
					}else{
					   GenericUnit *srcUnit=generic_model_get(src,index);
					   GenericUnit *define=generic_unit_clone(srcUnit);
					   generic_model_add_unit(destModel,define);
					}
			   }else{
				   generic_model_add_unit(destModel,unit);
			   }

	      }
		  if(destModel->unitCount==0){
			  generic_model_free(destModel);
			  return NULL;
		  }
		  return destModel;
}


static nboolean existsModel(NPtrArray *array,GenericModel *defines)
{
   int i;
   for(i=0;i<array->len;i++){
	   GenericModel *item=n_ptr_array_index(array,i);
	   if(generic_model_equal(item,defines)){
		   return TRUE;
	   }
   }
   return FALSE;
}


/**
 * 在sysName中创建的新对象(未定义泛型)，通过该方法获得定义的泛型。
 */
static NPtrArray *getDefineForChild(GenericMethod *self,GenericCallFunc *item)
{
   ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),self->sysName);
   if(classInfo==NULL){
	   n_error("在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
	   return NULL;
   }
   ClassFunc *func=func_mgr_get_entity(func_mgr_get(),&classInfo->className,self->funcName);
   GenericModel *orgiDecl=class_func_get_func_generic(func);

   NPtrArray *childDefines=n_ptr_array_new();
   tree t;
   int i,j;
   for(i=0;i<self->definesArray->len;i++){
	  GenericModel *src=n_ptr_array_index(self->definesArray,i);
	  GenericModel *destModel = getDefineFromObject(self,orgiDecl,src,item->model);
	  if(destModel!=NULL){
		  if(!existsModel(childDefines,destModel))
	         n_ptr_array_add(childDefines,destModel);
		  else
			 generic_model_free(destModel);
	  }
   }
   return childDefines;
}




static void printDefine(GenericMethod *self,NString *printStr)
{
	if(self->definesArray->len==0){
		n_string_append(printStr,"无泛型!");
	}else{
		int i;
		n_string_append(printStr,"泛型定义如下：");
		for(i=0;i<self->definesArray->len;i++){
		   GenericModel *defines=(GenericModel *)n_ptr_array_index(self->definesArray,i);
		   char     *var=generic_model_define_str(defines);
		   n_string_append_printf(printStr," %d、 %s。",i+1,var);
		   n_free(var);
		}
	}
}



static void printAll(GenericMethod *self,NString *printStr,int level,int from)
{

	n_string_append_printf(printStr,"类名:%s 方法名:%s level:%d ",self->sysName,self->funcName,level);
	printDefine(self,printStr);
	n_string_append(printStr,"\n");
	int i;
	if(self->childs->len==0){
		n_string_append(printStr,"类中无未定的泛型函数!\n");
	}else{
		n_string_append(printStr,"类中未定义泛型函数如下：\n");
		for(i=0;i<self->childs->len;i++){
			GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childs,i);
		    n_string_append_printf(printStr,"%d、 %s。",i+1,child->sysName);
		    printDefine(child,printStr);
			n_string_append(printStr,"\n");
		}
	}

	if(((GenericCM *)self)->parent!=NULL){
		n_string_append_printf(printStr,"类的容器是:%p\n",((GenericCM *)self)->parent);
	}else{
		n_string_append(printStr,"类没有容器\n");
	}
	n_string_append(printStr,"\n");
}

static void recursionPrint(GenericMethod *self,NString *printStr,int level,int from)
{
    printAll(self,printStr,level,from);
	int i;
    for(i=0;i<self->childs->len;i++){
	     GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childs,i);
	     recursionPrint(child,printStr,level+1,1);
	}
}

/**
 * 从collectArrray获取sysName对应的的所有确定的泛型集合
 * 如果sysName没有对应的collectArrray创建一个并加入到collectArrray
 */

nboolean generic_method_equal(GenericMethod *self,char *sysName,char *funcName)
{
	if(sysName==NULL || funcName==NULL)
		return FALSE;
	return strcmp(self->sysName,sysName)==0 && strcmp(self->funcName,funcName)==0;
}

/**
 * 加泛型定义到GenericMethod
 */
nboolean   generic_method_add_defines(GenericMethod *self,GenericModel *defines)
{
	if(!defines)
		return FALSE;
    if(!existsDefine(self,defines)){
 	   n_ptr_array_add(self->definesArray,defines);
 	   return TRUE;
    }
    return FALSE;
}


static NPtrArray *getDefineForObjectChild(GenericMethod *self,GenericNewClass *item)
{
   ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),self->sysName);
   if(classInfo==NULL){
	   n_error("在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
	   return NULL;
   }
   ClassFunc *func=func_mgr_get_entity(func_mgr_get(),&classInfo->className,self->funcName);
   GenericModel *orgiDecl=class_func_get_func_generic(func);

   NPtrArray *childDefines=n_ptr_array_new();
   tree t;
   int i,j;
   for(i=0;i<self->definesArray->len;i++){
	  GenericModel *src=n_ptr_array_index(self->definesArray,i);
	  GenericModel *destModel = getDefineFromObject(self,orgiDecl,src,item->model);
	  if(destModel!=NULL){
		  if(!existsModel(childDefines,destModel))
	         n_ptr_array_add(childDefines,destModel);
		  else
			 generic_model_free(destModel);
	  }
   }
   return childDefines;
}


void   generic_method_free(GenericMethod *self)
{
   if(self->sysName!=NULL){
     n_free(self->sysName);
     self->sysName=NULL;
   }
}


GenericMethod *generic_method_new(char *sysName,char *funcName,GenericModel *defines)
{
	GenericMethod *self =n_slice_alloc0 (sizeof(GenericMethod));
	genericMethodInit(self);
    self->sysName=n_strdup(sysName);
    self->funcName=n_strdup(funcName);
    generic_method_add_defines(self,defines);
	return self;
}

/**
 * 从collectArrray获取sysName对应的的所有确定的泛型集合
 * 如果sysName没有对应的collectArrray创建一个并加入到collectArrray
 */
/**
 * 从collectArrray找出与当前类名相同的CollectDefines
 * 然后加到self->definesArray
 */
void generic_method_collect_class(GenericMethod *self,NPtrArray *collectClass)
{
	  int i;
	  for(i=0;i<self->objectChilds->len;i++){
		 GenericClass *child=(GenericClass *)n_ptr_array_index(self->objectChilds,i);
		 generic_class_collect_class(child,collectClass);
	  }
	  for(i=0;i<self->childs->len;i++){
		GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childs,i);
		generic_method_collect_class(child,collectClass);
	  }
}


void generic_method_dye_method(GenericMethod *self,char *sysName,char *funcName,NPtrArray *defineArray)
{
	    int i,j;
		for(i=0;i<defineArray->len;i++){
		  GenericModel *define=n_ptr_array_index(defineArray,i);
		  if(generic_method_have_define(self,define)){
			  n_ptr_array_remove(defineArray,define);
			  i--;
		  }
		}

	    if(defineArray->len==0)
	       return ;

	    for(i=0;i<self->objectChilds->len;i++){
			GenericClass *item=n_ptr_array_index(self->objectChilds,i);
			generic_class_dye_method(item,sysName,funcName,defineArray);
			if(defineArray->len==0)
			   return ;
	    }

	    for(i=0;i<self->childs->len;i++){
	    	GenericMethod *item=n_ptr_array_index(self->childs,i);
	    	generic_method_dye_method(item,sysName,funcName,defineArray);
	 	    if(defineArray->len==0)
	 	 	   return ;
	    }
}


static void dyeMethod(GenericMethod *self,char *sysName,char *funcName,NPtrArray *defineArray)
{
        GenericCM *root=class_and_method_get_root((GenericCM*)self);
		if(root->type==PARENT_TYPE_CLASS){
			generic_class_dye_method((GenericClass *)root,sysName,funcName,defineArray);
		}else{
			generic_method_dye_method((GenericMethod *)root,sysName,funcName,defineArray);
		}
}

static void dyeClass(GenericMethod *self,char *sysName,NPtrArray *defineArray)
{

    GenericCM *root=class_and_method_get_root((GenericCM*)self);
	if(root->type==PARENT_TYPE_CLASS){
		generic_class_dye_class((GenericClass *)root,sysName,defineArray);
	}else{
		generic_method_dye_class((GenericMethod *)root,sysName,defineArray);
	}
}

static GenericClass *getChildObject(GenericMethod *self,char *sysName)
{
   int i;
   for(i=0;i<self->objectChilds->len;i++){
	   GenericClass *item=n_ptr_array_index(self->objectChilds,i);
	   if(generic_class_equal(item,sysName))
		   return item;
   }
   return NULL;
}

static void  createUndefineAndDefineMethod(GenericMethod *self,nboolean define)
{
	NPtrArray *genFuncArray=NULL;
	if(define)
		genFuncArray=block_mgr_read_define_func_call(block_mgr_get(),self->sysName);
	else
		genFuncArray=block_mgr_read_undefine_func_call(block_mgr_get(),self->sysName);

	if(genFuncArray==NULL || genFuncArray->len==0){
		n_debug("GenericMethod method 00 在类%s中没有调用%s泛型函数\n",self->sysName,define?"define":"undefine");
		return;
	}
	int i,j;
	for(i=0;i<genFuncArray->len;i++){
		 GenericCallFunc *item=(GenericCallFunc *)n_ptr_array_index(genFuncArray,i);
		 if(strcmp(item->atFunc,self->funcName))
			continue;
		 NPtrArray *result=NULL;
		 if(define){
			 result=n_ptr_array_new();
			 n_ptr_array_add(result,item->model);
		 }else{
			 result=getDefineForChild(self,item);
		 }

		 if(result==NULL || result->len==0){
		     n_debug("GenericMethod method 11 %d self sysName:%s define:%d item->sysName:%s item->atFunc:%s item->funcName:%s file:%s\n",
						 i,self->sysName,define,item->sysName,item->atFunc,item->funcName,in_fnames[0]);
			 continue;
		 }
		 dyeMethod(self,item->sysName,item->funcName,result);
		 if(result->len==0){
		     n_debug("GenericMethod method 22 i:%d define:%d 在当前类：%s中有%s的方法：%s 但已经有泛型定义了。 file:%s\n",
					 i,define,self->sysName,define?"define":"undefine",item->sysName,in_fnames[0]);
			 continue;
		 }
		 n_debug("GenericMethod method 33 %d define:%d self sysName:%s item->sysName:%s item->atFunc:%s item->funcName:%s genCount:%d file:%s\n",
				 i,define,self->sysName,item->sysName,item->atFunc,item->funcName,result->len,in_fnames[0]);
		 for(j=0;j<result->len;j++){
			  GenericModel *defines=n_ptr_array_index(result,j);
			  GenericMethod *child=getChild(self,item->sysName,item->funcName);
			  n_debug("GenericClass method 44 %d define:%d self sysName:%s item->sysName:%s item->atFunc:%s item->funcName:%s genCount:%d child:%p model:%s file:%s\n",
					  i,define,self->sysName,item->sysName,item->atFunc,item->funcName,result->len,child,generic_model_tostring(defines),in_fnames[0]);
			  if(!child){
				 child=generic_method_new(item->sysName,item->funcName,defines);
				 n_ptr_array_add(self->childs,child);
				 class_and_method_set_parent((GenericCM *)child,(GenericCM *)self);
			  }else{
				 generic_method_add_defines(child,defines);
			  }
		 }
		 n_ptr_array_unref(result);
	 }
}



static void  createObject(GenericMethod *self)
{
	NPtrArray *undefineArray=block_mgr_read_undefine_new_object(block_mgr_get(),self->sysName);
	if(undefineArray==NULL){
		printf("GenericMethod createObject 00 在当前类：%s中并没有undefine的对象\n",self->sysName);
		return;
	}
	int i,j;
	for(i=0;i<undefineArray->len;i++){
		 GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(undefineArray,i);
		 n_debug("GenericMethod createObject 11 %d sysName:%s funcName:%s id: %s\n",i,self->sysName,self->funcName,item->sysName);
		 if(strcmp(item->atFunc,self->funcName))
			 continue;
		 n_debug("GenericMethod createObject 22 %d sysName:%s funcName:%s id: %s\n",i,self->sysName,self->funcName,item->sysName);
		 NPtrArray *result=getDefineForObjectChild(self,item);
		 dyeClass(self,item->sysName,result);
		 if(result==NULL || result->len==0)
			 continue;
		 n_debug("GenericMethod createObject 33 i:%d 在当前类：%s %s 中有undefine的对象：%s %d\n",i,self->sysName,self->funcName,item->sysName,result->len);
		 for(j=0;j<result->len;j++){
			  GenericModel *defines=n_ptr_array_index(result,j);
			  GenericClass *child=getChildObject(self,item->sysName);
			  if(!child){
				 child=generic_class_new(item->sysName,defines);
				 n_ptr_array_add(self->objectChilds,child);
				 class_and_method_set_parent((GenericCM *)child,(GenericCM *)self);
				 n_debug("GenericMethod createObject 44 i:%d child:%p 在当前类%s中的泛型方法%s中创建类变量%s，泛型是:%s\n",
						 i,child,self->sysName,self->funcName,item->sysName,generic_model_tostring(defines));
			  }else{
			      n_debug("GenericMethod createObject 55 i:%d child:%p 在当前类%s中的泛型方法%s中加入新泛型%s到类:%s中\n",
				 						 i,child,self->sysName,self->funcName,generic_model_tostring(defines),item->sysName);
				 generic_class_add_defines(child,defines);
			  }
		 }
		 n_ptr_array_unref(result);
	 }
}


void generic_method_create_node(GenericMethod *self)
{
	createUndefineAndDefineMethod(self,TRUE);
	createUndefineAndDefineMethod(self,FALSE);
	createObject(self);
}

void generic_method_recursion_node(GenericMethod *self)
{
	int i;
	for(i=0;i<self->childs->len;i++){
		GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childs,i);
		generic_method_create_node(child);
		generic_method_recursion_node(child);
	}
}

void generic_method_recursion_class(GenericMethod *self)
{
	int i;
	for(i=0;i<self->objectChilds->len;i++){
		GenericClass *child=(GenericClass *)n_ptr_array_index(self->objectChilds,i);
		printf("generic_method_recursion_class %d child:%p %s %s %s %s\n",i,child,child->sysName,self->sysName,self->funcName,in_fnames[0]);
		generic_class_create_node(child);
		generic_class_recursion_node(child);
	}
}


static GenericMethod *getMethod(NPtrArray *collectArrray,char *sysName,char *funcName)
{
   int i;
   for(i=0;i<collectArrray->len;i++){
	   GenericMethod *item=n_ptr_array_index(collectArrray,i);
	   if(strcmp(item->sysName,sysName)==0 && strcmp(item->funcName,funcName)==0){
		   return item;
	   }
   }
   return NULL;

}

void generic_method_collect(GenericMethod *self,NPtrArray *collectArrray)
{
	GenericMethod *collect=getMethod(collectArrray,self->sysName,self->funcName);
    int i;
	if(collect!=NULL){
		for(i=0;i<self->definesArray->len;i++){
			GenericModel *item=n_ptr_array_index(self->definesArray,i);
			generic_method_add_defines(collect,item);
		}
	}else{
		n_ptr_array_add(collectArrray,self);
	}
    for(i=0;i<self->childs->len;i++){
    	GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childs,i);
    	generic_method_collect(child,collectArrray);
    }
}

void generic_method_add_defines_array(GenericMethod *self,NPtrArray *defines)
{
	int i;
	for(i=0;i<defines->len;i++){
		GenericModel *model=(GenericModel *)n_ptr_array_index(defines,i);
		generic_method_add_defines(self,model);
	}
}

nboolean  generic_method_have_define(GenericMethod *self,GenericModel *define)
{
	return existsDefine(self,define);
}


void generic_method_dye_class(GenericMethod *self,char *sysName,NPtrArray *defineArray)
{
	  int i;
	  for(i=0;i<self->objectChilds->len;i++){
	 	    GenericClass *item=n_ptr_array_index(self->objectChilds,i);
	 	    if(!strcmp(item->sysName,sysName)){
	 	    	 int j;
	 	    	 for(j=0;j<defineArray->len;j++){
	 				   GenericModel *define=n_ptr_array_index(defineArray,j);
	 	    	       if(generic_class_have_define(item,define)){
					      n_ptr_array_remove(defineArray,define);
					      j--;
	 	    	       }
	 	    	 }
	 	    }
	 	    if(defineArray->len==0)
	 	 	   return ;
	 	   generic_class_dye_class(item,sysName,defineArray);
	  }

	  for(i=0;i<self->childs->len;i++){
		    GenericMethod *item=n_ptr_array_index(self->childs,i);
		    generic_method_dye_class(item,sysName,defineArray);
	 	    if(defineArray->len==0)
	 	 	   return ;
	  }
}

