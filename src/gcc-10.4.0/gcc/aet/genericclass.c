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

static void genericClassInit(GenericClass *self)
{
	self->definesArray=n_ptr_array_new_with_free_func(generic_model_free);
	self->childs=n_ptr_array_new_with_free_func(generic_class_free);
	self->parents=n_ptr_array_new_with_free_func(generic_class_free);
	self->belongSysName=NULL;
	self->belongDefines=NULL;
	self->haveAddChilds=FALSE;
	self->haveAddParent=FALSE;
	self->childsMethod=n_ptr_array_new_with_free_func(generic_method_free);
	((GenericCM *)self)->type=PARENT_TYPE_CLASS;
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
 * GenericClass是否存在相同的泛型定义
 */
static nboolean existsDefine(GenericClass *self,GenericModel *defines)
{
   int i;
   for(i=0;i<self->definesArray->len;i++){
	   GenericModel *item=n_ptr_array_index(self->definesArray,i);
	   n_debug("GenericClass existsDefine --- %s %s %d\n",generic_model_tostring(item),generic_model_tostring(defines),compare(item,defines));
	   if(compare(item,defines)){
		   return TRUE;
	   }
   }
   return FALSE;
}



static GenericClass *getChild(GenericClass *self,char *sysName)
{
   int i;
   for(i=0;i<self->childs->len;i++){
	   GenericClass *item=n_ptr_array_index(self->childs,i);
	   if(generic_class_equal(item,sysName))
		   return item;
   }
   return NULL;
}


static GenericModel *getDefineFromObject(GenericClass *self,GenericModel *origDecl,GenericModel *src,GenericModel *dest)
{
	  GenericModel *destModel = generic_model_new_from_file();
	  int genCount=generic_model_get_count(dest);
	  int i;
	  for(i=0;i<genCount;i++){
		   GenericUnit *unit=generic_model_get(dest,i);
		   if(generic_unit_is_undefine(unit)){
				int index=generic_model_get_index(origDecl,unit->name);
				if(index<0){
					n_warning("类%s中没有声明此泛型单元 %s。",self->sysName,unit->name);
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

/**
 * 在外部文件如a.c中new$ Abc<int>对象
 * 在Abc类(b.c文件)中有一泛型未定义的类被创建如 AMutex *xx=new$ AMutex<E>()
 * 执行到这里，说明正在编译的是a.c文件，通过block_mgr_read_undefine_new_object知道
 * 在Abc类中有一个AMutex是未定义的。现在就把在a.c中创建Abc确定的泛型"设给"AMutex
 */
static NPtrArray *getDefineFromChild(GenericClass *self,GenericModel *model)
{
   ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),self->sysName);
   if(classInfo==NULL){
	   n_error("在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
	   return NULL;
   }
   NPtrArray *childDefines=n_ptr_array_new();
   tree t;
   int i,j;
   for(i=0;i<self->definesArray->len;i++){
	  GenericModel *src=n_ptr_array_index(self->definesArray,i);
	  n_debug("getDefineFromChild 类%s已确定的泛型有:i:%d %s\n",self->sysName,i,generic_model_tostring(src));
	  GenericModel *dest=getDefineFromObject(self, classInfo->genericModel,src,model);
	  if(dest!=NULL)
	     n_ptr_array_add(childDefines,dest);
   }
   return childDefines;
}


/**
 * 从子类获取到泛型定义，设到父类中
 * 比如 父类有泛型E 这里用子类的具体类型替换E
 */
static GenericModel *replaceGendericFromChild(GenericModel *parentGeneric,char *childSysName,GenericModel *childDefines)
{
	   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),childSysName);
	   GenericModel *parentDefinesList = generic_model_new_from_file();
	   int i;
	   for(i=0;i<parentGeneric->unitCount;i++){
		   GenericUnit *unit=parentGeneric->genUnits[i];
		   if(!unit->isDefine){
			    char *genStr=unit->name;
				int index=generic_model_get_index(info->genericModel,genStr);
				if(index<0){
					n_error("在父类的泛型声明,在子类中没有找到 %s %s",childSysName,genStr);
				}
				unit=generic_model_get(childDefines,index);
		   }
		   GenericUnit *clone=generic_unit_clone(unit);
		   generic_model_add_unit(parentDefinesList,clone);
	   }
	   return parentDefinesList;
}


static void addParent(GenericClass *self,NPtrArray *array,GenericClass *container)
{
	  ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),self->sysName);
	  if(info==NULL){
		  n_error("addParent 在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
		  return;
	  }
	  GenericModel *parentGeneric=info->parentGenericModel; //设父类的泛型
	  char *parentSysName=info->parentName.sysName;
	  if(parentSysName==NULL){
		  n_debug("addParent 11 子类%s的父类是空的，返回。\n",self->sysName);
		  return;
	  }
	  GenericClass *parent=NULL;
	  if(!parentGeneric){
	      n_debug("addParent 22  子类%s的父类%s没有泛型,把当前类的泛型设到父类的belong中\n",self->sysName,parentSysName);
		  parent=generic_class_new(parentSysName,NULL);
		  parent->belongSysName=n_strdup(self->sysName);
		  parent->belongDefines=self->definesArray;
	  }else{
		   parent=generic_class_new(parentSysName,NULL);
		   NPtrArray *last=self->definesArray;
		   char *belong=self->sysName;
		   if(self->definesArray->len==0){
			   if(self->belongDefines!=NULL){
				   last=self->belongDefines;
				   belong=self->belongSysName;
				   n_debug("addParent 33 给子类%s的父类%s设定泛型定义来自另外的类：%s",self->sysName,parent->sysName,belong);
			   }
		   }
		   int i;
		   if(last->len>0){
			 for(i=0;i<last->len;i++){
			    GenericModel *item=(GenericModel *)n_ptr_array_index(last,i);
			    GenericModel *define=replaceGendericFromChild(parentGeneric,belong,item);
			    n_debug("addParent 44  子类%s给父类%s设的泛型是:%s\n",self->sysName,parent->sysName,generic_model_tostring(define));
			    generic_class_add_defines(parent,define);
			 }
		   }else{
			   int un=generic_model_get_undefine_count(parentGeneric);
			   if(un>0){
				   n_error("addParent 类%s的父类%s泛型定义中有未定义的泛型！",self->sysName,parent->sysName);
			   }
			   n_debug("addParent 55  子类%s给父类%s设的泛型是:%s\n",self->sysName,parent->sysName,generic_model_tostring(parentGeneric));
		   }
	  }
	  n_ptr_array_add(array,parent);
	  addParent(parent,array,container);
}



static void printDefine(GenericClass *self,NString *printStr)
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



static void printAll(GenericClass *self,NString *printStr,int level,int from)
{
	char *fromStr="根泛型类";
	if(from==1)
	  fromStr="泛型未定义类";
	else if(from==2)
	  fromStr="父类";
	n_string_append_printf(printStr,"类名:%s level:%d 来自:%s ",self->sysName,level,fromStr);
	printDefine(self,printStr);
	n_string_append(printStr,"\n");
	int i;
	if(self->childs->len==0){
		n_string_append(printStr,"类中无未定的泛型类!\n");
	}else{
		n_string_append(printStr,"类中未定义泛型类如下：\n");
		for(i=0;i<self->childs->len;i++){
			GenericClass *child=(GenericClass *)n_ptr_array_index(self->childs,i);
		    n_string_append_printf(printStr,"%d、 %s。",i+1,child->sysName);
		    printDefine(child,printStr);
			n_string_append(printStr,"\n");
		}
	}
	if(self->parents->len>0){
		n_string_append(printStr,"类有父类：\n");
		for(i=0;i<self->parents->len;i++){
			GenericClass *parent=(GenericClass *)n_ptr_array_index(self->parents,i);
			n_string_append_printf(printStr,"%d、 %s。\n",i+1,parent->sysName);
			printDefine(parent,printStr);
			n_string_append(printStr,"\n");
		}
	}else{
		n_string_append(printStr,"类没有父类：\n");
	}
	if(((GenericCM *)self)->parent!=NULL){
		n_string_append_printf(printStr,"类的容器是:%p\n",((GenericCM *)self)->parent);
	}else{
		n_string_append(printStr,"类没有容器\n");
	}
	n_string_append(printStr,"\n");
}

static void recursionPrint(GenericClass *self,NString *printStr,int level,int from)
{
    printAll(self,printStr,level,from);
	int i;
    for(i=0;i<self->childs->len;i++){
	     GenericClass *child=(GenericClass *)n_ptr_array_index(self->childs,i);
	     recursionPrint(child,printStr,level+1,1);
	}
    for(i=0;i<self->parents->len;i++){
	     GenericClass *parent=(GenericClass *)n_ptr_array_index(self->parents,i);
	     recursionPrint(parent,printStr,level+1,2);
    }
}


static BlockFunc *addBlockFunc(NPtrArray *array,char *sysName,char *orgiFuncName,BlockDetail *detail,int index)
{
	int i;
	for(i=0;i<array->len;i++){
		BlockFunc *item=(BlockFunc *)n_ptr_array_index(array,i);
		if(block_func_is_same(item,orgiFuncName)){
			return item;
		}
	}
	BlockFunc *blockFunc=block_func_new(index,sysName,detail);
	n_ptr_array_add(array,blockFunc);
	return blockFunc;
}


/**
 * 生成
 * self->_gen_blocks_array_897[0]=_test_AObject__inner_generic_func_1_abs123;
 * 并生成BlockFunc
 */
static char *createBlockCodes(GenericClass *self,BlockContent *bc,char *varName,NPtrArray *blockFuncsArray)
{
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<bc->blocksCount;i++){
       char *orgiName=generic_util_create_block_func_name(bc->sysName,i+1);
       BlockFunc *blockFunc=addBlockFunc(blockFuncsArray,bc->sysName,orgiName,bc->content[i],i+1);
       n_free(orgiName);
       n_string_append_printf(codes,"%s->%s[%d]=%s;\n",varName,AET_GENERIC_BLOCK_ARRAY_VAR_NAME,i,blockFunc->funcDefineName);
   }
   char *result=n_strdup(codes->str);
   n_string_free(codes,TRUE);
   return result;
}

static char *createObjectBlockCodes(GenericClass *self,char *tempVarName,NPtrArray *blockFuncsArray)
{
	BlockContent *bc=block_mgr_read_block_content(block_mgr_get(),self->sysName);
	NString *blockCodes=n_string_new("");
	if(bc!=NULL){
	   char *objBlockCodes=createBlockCodes(self,bc,tempVarName,blockFuncsArray);
	   if(objBlockCodes){
		   n_string_append(blockCodes,objBlockCodes);
		   n_free(objBlockCodes);
	   }else{
		   n_error("有块但没有生成代码:%s\n",self->sysName);
	   }
	}
    char *result=NULL;
	if(blockCodes->len>0){
	    result=n_strdup(blockCodes->str);
	}
	n_string_free(blockCodes,TRUE);
	return result;
}



nboolean generic_class_equal(GenericClass *self,char *sysName)
{
	if(sysName==NULL)
		return FALSE;
	return strcmp(self->sysName,sysName)==0;
}

/**
 * 加泛型定义到GenericClass
 */
nboolean   generic_class_add_defines(GenericClass *self,GenericModel *defines)
{
	if(!defines)
		return FALSE;
    if(!existsDefine(self,defines)){
 	   n_ptr_array_add(self->definesArray,defines);
 	   return TRUE;
    }
    return FALSE;
}


/**
 * 设置GenericClass的父类，把层次关系的父类，变成水平的
 * AObject
 *      ----Abc
 *            -----Efg
 * Efg的父类变成如下存储:
 * Abc---AObject
 */

void   generic_class_free(GenericClass *self)
{
   if(self->sysName!=NULL){
     n_free(self->sysName);
     self->sysName=NULL;
   }
}



static NPtrArray *getDefineFromClassxxx(GenericClass *self,GenericModel *replace)
{
   ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),self->sysName);
   if(classInfo==NULL){
	   n_error("在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
	   return NULL;
   }
   NPtrArray *childDefines=n_ptr_array_new();
   tree t;
   int i,j;
   for(i=0;i<self->definesArray->len;i++){
	  GenericModel *src=n_ptr_array_index(self->definesArray,i);
	  n_debug("getDefineFromChild 类%s已确定的泛型有:i:%d %s\n",self->sysName,i,generic_model_tostring(src));
	  GenericModel *dest=getDefineFromObject(self, classInfo->genericModel,src,replace);
	  if(dest!=NULL)
	     n_ptr_array_add(childDefines,dest);
   }
   return childDefines;
}

static NPtrArray *getDefineForChildFuncGen(GenericClass *self,GenericCallFunc *item)
{
   ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),self->sysName);
   if(classInfo==NULL){
	   n_error("在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
	   return NULL;
   }
   ClassFunc *func=func_mgr_get_entity(func_mgr_get(),&classInfo->className,item->atFunc);
   if(class_func_is_func_generic(func)){
       n_debug("未定泛型的调用泛型函数，所在的类是%s，所在的函数是:%s,并且是一个泛型函数。\n",classInfo->className.userName,item->atFunc);
	   GenericModel *genModel=class_func_get_func_generic(func);
	   if(generic_model_include_decl(genModel,item->model)){
	       n_debug("所在的泛型函数的泛型单元包含了调用的泛型函数中的未定。所以由泛型函数处理.\n");
		   return NULL;
	   }else{
		   GenericModel  *classModel=class_info_get_generic_model(classInfo);
		   if(!generic_model_include_decl(classModel,item->model)){
			   n_error("调用的泛型函数的泛型单元是无效的。%s %s model:%s\n",item->sysName,item->funcName,generic_model_tostring(item->model));
			   return NULL;
		   }else{
			  return  getDefineFromClassxxx(self,item->model);
		   }
	   }
   }else{
		  return  getDefineFromClassxxx(self,item->model);
   }

}




static GenericMethod *getChildMethod(NPtrArray *childsMethod,char *sysName,char *funcName)
{
   int i;
   for(i=0;i<childsMethod->len;i++){
	   GenericMethod *item=n_ptr_array_index(childsMethod,i);
	   if(generic_method_equal(item,sysName,funcName))
		   return item;
   }
   return NULL;
}




static GenericClass *dyeClass(GenericClass *self,char *sysName,NPtrArray *defineArray)
{
	  printf("dyeClass ----- %s childs:%d\n",self->sysName,self->childs->len);
	  int i;
	  if(!strcmp(self->sysName,sysName)){
		   for(i=0;i<defineArray->len;i++){
			   GenericModel *define=n_ptr_array_index(defineArray,i);
			   printf("genericclass dyeClass 00 %p %s %s %s\n",self,self->sysName,sysName,generic_model_tostring(define));
				 if(existsDefine(self,define)){
		             n_ptr_array_remove(defineArray,define);
		             i--;
				 }
		   }
		  if(defineArray->len==0)
		     return self;
	  }
	  for(i=0;i<self->childs->len;i++){
	 	    GenericClass *item=n_ptr_array_index(self->childs,i);
	 	   n_debug("genericclass dyeClass 11  %d self:%p %s %s:item:%s\n",i,self,self->sysName,sysName,item->sysName);
	 	    GenericClass *cl=dyeClass(item,sysName,defineArray);
	 	    if(defineArray->len==0)
	 	 	   return cl;
	  }
	  return NULL;
}

void generic_class_dye_class(GenericClass *self,char *sysName,NPtrArray *defineArray)
{
          n_debug("dyeClass ----- %s childs:%d\n",self->sysName,self->childs->len);
		  int i;
		  if(!strcmp(self->sysName,sysName)){
			   for(i=0;i<defineArray->len;i++){
				   GenericModel *define=n_ptr_array_index(defineArray,i);
				   n_debug("genericclass dyeClass 00 %p %s %s %s\n",self,self->sysName,sysName,generic_model_tostring(define));
					 if(existsDefine(self,define)){
			             n_ptr_array_remove(defineArray,define);
			             i--;
					 }
			   }
			  if(defineArray->len==0)
			     return;
		  }

		  for(i=0;i<self->childsMethod->len;i++){
			  GenericMethod *item=n_ptr_array_index(self->childsMethod,i);
			  n_debug("genericclass dyeClass 11  %d self:%p %s %s:item:%s\n",i,self,self->sysName,sysName,item->sysName);
			  generic_method_dye_class(item,sysName,defineArray);
			  if(defineArray->len==0)
			 	 	   return;
		  }

		  for(i=0;i<self->childs->len;i++){
		 	    GenericClass *item=n_ptr_array_index(self->childs,i);
		 	   n_debug("genericclass dyeClass 22  %d self:%p %s %s:item:%s\n",i,self,self->sysName,sysName,item->sysName);
		 	    generic_class_dye_class(item,sysName,defineArray);
		 	    if(defineArray->len==0)
		 	 	   return;
		  }
		  return NULL;
}


static void getDyeClass(GenericClass *self,char *sysName,NPtrArray *defineArray)
{
	GenericCM *root=class_and_method_get_root((GenericCM*)self);
	if(root->type==PARENT_TYPE_CLASS){
		generic_class_dye_class((GenericClass *)root,sysName,defineArray);
	}
}

void generic_class_dye_method(GenericClass *self,char *sysName,char *funcName,NPtrArray *defineArray)
{
	  int i,j;
	  for(i=0;i<self->childsMethod->len;i++){
			GenericMethod *item=n_ptr_array_index(self->childsMethod,i);
			if(!strcmp(item->sysName,sysName) && !strcmp(item->funcName,funcName)){
				for(j=0;j<defineArray->len;j++){
					  GenericModel *define=n_ptr_array_index(defineArray,j);
					  if(generic_method_have_define(item,define)){
						n_ptr_array_remove(defineArray,define);
						j--;
					  }
				}
			}
	  }
	  if(defineArray->len==0)
	     return ;
	  for(i=0;i<self->childs->len;i++){
	 	    GenericClass *item=n_ptr_array_index(self->childs,i);
	 	    generic_class_dye_method(item,sysName,funcName,defineArray);
	 	    if(defineArray->len==0)
	 	 	   return ;
	  }
}

static void getDyeMethod(GenericClass *self,char *sysName,char *funcName,NPtrArray *defineArray)
{
	    GenericCM *root=class_and_method_get_root((GenericCM*)self);
		if(root==PARENT_TYPE_CLASS){
			generic_class_dye_method((GenericClass *)root,sysName,funcName,defineArray);
		}
}

static void  addNode(GenericClass *self,nboolean needDefine)
{
	NPtrArray *childs=NULL;
	if(!needDefine)
	   childs=block_mgr_read_undefine_new_object(block_mgr_get(),self->sysName);
	else
	   childs=block_mgr_read_define_new_object(block_mgr_get(),self->sysName);
	if(childs==NULL){
		n_debug("GenericClass addNode 00 在当前类：%s中并没有%s的对象\n",self->sysName,needDefine?"define":"undefine");
		return;
	}
	int i,j;
	for(i=0;i<childs->len;i++){
		 GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(childs,i);
		 if(!strcmp(self->sysName,item->sysName)){
		     n_debug("GenericClass addNode 11 i:%d 在当前类：%s中有相同的%s对象：file:%s\n",i,self->sysName,needDefine?"define":"undefine",in_fnames[0]);
			 continue;
		 }
		 NPtrArray *result=getDefineFromChild(self,item->model);
		 if(result->len==0){
		     n_debug("GenericClass addNode 22 i:%d 在当前类：%s中有%s的对象：%s 但没有泛型定义 file:%s\n",i,self->sysName,needDefine?"define":"undefine",item->sysName,in_fnames[0]);
			 continue;
		 }
		 n_debug("GenericClass addNode 33 i:%d 在当前类：%s中有%s的对象：%s %d file:%s\n",i,self->sysName,needDefine?"define":"undefine",item->sysName,result->len,in_fnames[0]);
		 int old=result->len;
		 getDyeClass(self,item->sysName,result);
		 if(result->len==0){
		     n_debug("GenericClass addNode 44 i:%d 在当前类：%s中有%s的对象：%s 但已经有泛型定义了。 file:%s\n",i,self->sysName,needDefine?"define":"undefine",item->sysName,in_fnames[0]);
			 continue;
		 }
		 for(j=0;j<result->len;j++){
			 GenericModel *defines=n_ptr_array_index(result,j);
			 GenericClass *child=getChild(self,item->sysName);
	 		 if(child==NULL){
	 			 child=generic_class_new(item->sysName,defines);
	 			 n_ptr_array_add(self->childs,child);
	 			 class_and_method_set_parent((GenericCM *)child,(GenericCM *)self);
	 		 }else{
				 generic_class_add_defines(child,defines);
	 		 }
	 		n_debug("GenericClass addNode 55 j:%d self:%p child:%p 在当前类：%s中创建类变量%s的GenericClass的对象:泛型是:%s file:%s\n",
							 j,self,child,self->sysName,item->sysName,generic_model_tostring(defines),in_fnames[0]);
		 }
		 n_ptr_array_unref(result);
	 }
}

static void  createUndefineAndDefineMethod(GenericClass *self,nboolean define)
{
	NPtrArray *genFuncArray=NULL;
	if(define)
		genFuncArray=block_mgr_read_define_func_call(block_mgr_get(),self->sysName);
	else
		genFuncArray=block_mgr_read_undefine_func_call(block_mgr_get(),self->sysName);

	if(genFuncArray==NULL || genFuncArray->len==0){
	    n_debug("GenericClass method 00 在类%s中没有调用%s泛型函数\n",self->sysName,define?"define":"undefine");
		return;
	}
	int i,j;
	for(i=0;i<genFuncArray->len;i++){
		 GenericCallFunc *item=(GenericCallFunc *)n_ptr_array_index(genFuncArray,i);
		 NPtrArray *result=NULL;
		 if(define){
			 result=n_ptr_array_new();
			 n_ptr_array_add(result,item->model);
		 }else{
			 result=getDefineForChildFuncGen(self,item);
		 }
		 if(result==NULL || result->len==0){
		     n_debug("GenericClass method 11 %d self sysName:%s define:%d item->sysName:%s item->atFunc:%s item->funcName:%s file:%s\n",
						 i,self->sysName,define,item->sysName,item->atFunc,item->funcName,in_fnames[0]);
			 continue;
		 }
		 getDyeMethod(self,item->sysName,item->funcName,result);
		 if(result->len==0){
		     n_debug("GenericClass method 22 i:%d define:%d 在当前类：%s中有%s的方法：%s 但已经有泛型定义了。 file:%s\n",
					 i,define,self->sysName,define?"define":"undefine",item->sysName,in_fnames[0]);
			 continue;
		 }
		 n_debug("GenericClass method 33 %d define:%d self sysName:%s item->sysName:%s item->atFunc:%s item->funcName:%s genCount:%d file:%s\n",
				 i,define,self->sysName,item->sysName,item->atFunc,item->funcName,result->len,in_fnames[0]);
		 for(j=0;j<result->len;j++){
			  GenericModel *defines=n_ptr_array_index(result,j);
			  GenericMethod *child=getChildMethod(self->childsMethod,item->sysName,item->funcName);
			  n_debug("GenericClass method 44 %d define:%d self sysName:%s item->sysName:%s item->atFunc:%s item->funcName:%s genCount:%d child:%p model:%s file:%s\n",
					  i,define,self->sysName,item->sysName,item->atFunc,item->funcName,result->len,child,generic_model_tostring(defines),in_fnames[0]);
			  if(!child){
				 child=generic_method_new(item->sysName,item->funcName,defines);
				 n_ptr_array_add(self->childsMethod,child);
				 class_and_method_set_parent((GenericCM *)child,(GenericCM *)self);
			  }else{
				 generic_method_add_defines(child,defines);
			  }
		 }
		 n_ptr_array_unref(result);
	 }
}

void  generic_class_create_node(GenericClass *self)
{
	  addParent(self,self->parents,self);
	  int i,j;
	  for(i=0;i<self->parents->len;i++){
		GenericClass *item=n_ptr_array_index(self->parents,i);
		ClassInfo *classInfo=class_mgr_get_class_info(class_mgr_get(),item->sysName);
		if(classInfo==NULL){
		     n_error("在文件%s中找不到类%s。检查是否包含了类%s所在的头文件。!!!",in_fnames[0],self->sysName,self->sysName);
		     return;
		}
		if(class_info_about_generic(classInfo)){
		    NPtrArray *result=item->definesArray;
			getDyeClass(self,item->sysName,result);
			if(result->len>0){
				GenericClass *child=getChild(self,item->sysName);
				if(child!=NULL){
					generic_class_add_defines_array(child,result);
					n_debug("把父组件加成 00 child:%d %s child:%p parent:%p self:%p %s\n",i,child->sysName,child,self,self,self->sysName);
				}else{
					class_and_method_set_parent((GenericCM *)item,(GenericCM *)self);
					n_ptr_array_add(self->childs,item);
					n_debug("把父组件加成 11 child:%d %s child:%p parent:%p self:%p %s\n",i,item->sysName,item,self,self,self->sysName);
				}
			}
		}
	  }
	  n_debug("generic_class_create_node %p %s %s\n",self,self->sysName,in_fnames[0]);
	  addNode(self,TRUE);
	  addNode(self,FALSE);
	  createUndefineAndDefineMethod(self,TRUE);
	  createUndefineAndDefineMethod(self,FALSE);
}

void generic_class_recursion_node(GenericClass *self)
{
	int i;
	for(i=0;i<self->childs->len;i++){
		GenericClass *child=(GenericClass *)n_ptr_array_index(self->childs,i);
		n_debug("generic_class_recursion_node 00 %d %p 子：%s 源：%s child->childs:%d %s\n",i,child,child->sysName,self->sysName,child->childs->len,in_fnames[0]);
		generic_class_create_node(child);
		n_debug("generic_class_recursion_node 11 %d %p 子：%s 源：%s child->childs:%d %s\n",i,child,child->sysName,self->sysName,child->childs->len,in_fnames[0]);
		generic_class_recursion_node(child);
	}
}

void generic_class_recursion_method(GenericClass *self)
{
	int i;
	for(i=0;i<self->childsMethod->len;i++){
		GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childsMethod,i);
		n_debug("generic_class_recursion_method 00 %d self:%s method: sysName:%s func:%s file:%s\n",i,self->sysName,child->sysName,child->funcName,in_fnames[0]);
		generic_method_create_node(child);
		generic_method_recursion_class(child);
		generic_method_recursion_node(child);
	}

	for(i=0;i<self->childs->len;i++){
		GenericClass *child=(GenericClass *)n_ptr_array_index(self->childs,i);
		n_debug("generic_class_recursion_method 11 %d self:%s method:%s %s\n",i,self->sysName,child->sysName,in_fnames[0]);
		generic_class_recursion_method(child);
	}
}


void generic_class_collect_method_class(GenericClass *self,NPtrArray *collectArrray)
{
	int i;
    for(i=0;i<self->childsMethod->len;i++){
    	GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childsMethod,i);
	    generic_method_collect_class(child,collectArrray);
    }
}

void generic_class_collect_method(GenericClass *self,NPtrArray *collectArrray)
{
	int i;
    for(i=0;i<self->childsMethod->len;i++){
    	GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childsMethod,i);
	    generic_method_collect(child,collectArrray);
    }
}

/**
 * 给类中的变量#define AET_SET_BLOCK_FUNC_VAR_NAME "_setGenericBlockFunc_123_varname"
 * 赋值。值是函数#define AET_SET_BLOCK_FUNC_CALLBACK_NAME "_setGenericBlockFunc567_cb"
 */
static void assignmentCodes(GenericClass *self,NString *codes,NPtrArray *blockFuncsArray)
{
	char tempName[255];
	sprintf(tempName,"_%s0",self->sysName);
	n_string_append_printf(codes,"     %s *%s=(%s *)object;\n",self->sysName,tempName,self->sysName);
	n_string_append       (codes,"     if(self!=object){\n");
    n_string_append_printf(codes,"        %s->%s=%s;\n",tempName,AET_SET_BLOCK_FUNC_VAR_NAME,AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	n_string_append(codes,"     }\n");

	char *objCodes=createObjectBlockCodes(self,tempName,blockFuncsArray);
	if(objCodes!=NULL){
		n_string_append(codes,objCodes);
	}else{
		n_string_append_printf(codes,"printf(\"%s类没有泛型块。\\n\");\n",self->sysName);
	}
}


char *generic_class_create_fragment(GenericClass *self,NPtrArray *blockFuncsArray)
{
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),self->sysName);
	nboolean aboutGen=class_mgr_about_generic(class_mgr_get(),className);
	if(!aboutGen){
		printf("不是泛型相关的，不生成代码，%s\n",self->sysName);
		return NULL;
	}
	n_debug("generic_class_create_fragment ,类名：%s\n",self->sysName);
	NString *codes=n_string_new("");
	n_string_append_printf(codes,"  if(strcmp(sysName,\"%s\")==0){\n",self->sysName);
	assignmentCodes(self,codes,blockFuncsArray);
	n_string_append       (codes,"  }\n");
	char *str=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	return str;
}



static GenericClass *getClass(NPtrArray *collectArrray,char *sysName)
{
   int i;
   for(i=0;i<collectArrray->len;i++){
	   GenericClass *item=n_ptr_array_index(collectArrray,i);
	   if(strcmp(item->sysName,sysName)==0){
		   return item;
	   }
   }
   return NULL;
}

void generic_class_collect_class(GenericClass *self,NPtrArray *collectArrray)
{
	GenericClass *collect=getClass(collectArrray,self->sysName);
    int i;
	if(collect!=NULL){
		for(i=0;i<self->definesArray->len;i++){
			GenericModel *item=n_ptr_array_index(self->definesArray,i);
			generic_class_add_defines(collect,item);
		}
	}else{
		n_ptr_array_add(collectArrray,self);
	}

    for(i=0;i<self->childsMethod->len;i++){
	    GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childsMethod,i);
	    generic_method_collect_class(child,collectArrray);
    }

    for(i=0;i<self->childs->len;i++){
	   GenericClass *child=(GenericClass *)n_ptr_array_index(self->childs,i);
	   generic_class_collect_class(child,collectArrray);
    }
}


void generic_class_collect_method_class99(GenericClass *self,NPtrArray *collectArrray)
{

	int i;
	n_debug("generic_class_collect_method_class99 00 self:%s childsMethod->len:%d child:%d\n",self->sysName,self->childsMethod->len,self->childs->len);
	for(i=0;i<self->childsMethod->len;i++){
		GenericMethod *child=(GenericMethod *)n_ptr_array_index(self->childsMethod,i);
		generic_method_collect(child,collectArrray);
	}
    for(i=0;i<self->childs->len;i++){
	   GenericClass *child=(GenericClass *)n_ptr_array_index(self->childs,i);
	   generic_class_collect_method_class99(child,collectArrray);
    }
}

void  generic_class_add_defines_array(GenericClass *self,NPtrArray *defines)
{
	int i;
	for(i=0;i<defines->len;i++){
		GenericModel *model=(GenericModel *)n_ptr_array_index(defines,i);
		generic_class_add_defines(self,model);
	}
}

nboolean       generic_class_have_define(GenericClass *self,GenericModel *define)
{
	return existsDefine(self,define);
}



GenericClass *generic_class_new(char *sysName,GenericModel *defines)
{
	GenericClass *self =n_slice_alloc0 (sizeof(GenericClass));
    genericClassInit(self);
    self->sysName=n_strdup(sysName);
    generic_class_add_defines(self,defines);
	return self;
}
