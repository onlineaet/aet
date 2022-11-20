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
#include "opts.h"
#include "toplev.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "c-aet.h"
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
#include "genericcommon.h"
#include "makefileparm.h"
#include "genericfile.h"


static void genericFileInit(GenericFile *self)
{
	   self->secondCompileFile=NULL;
	   self->xmlFile=NULL;
	   self->genericDataArray=NULL;
}

static char *createFuncCallVarName()
{
	   char *fileName=in_fnames[0];
	   char temp[256];
	   sprintf(temp,"%s_func_call",fileName);
	   return n_strdup(temp);
}

/**
 * 当前文件使用了 new$ Abc<int>();
 * 把当前文件名记录在AET_NEED_SECOND_FILE_NAME中，第一次编译 生成目录时，根据AET_NEED_SECOND_FILE_NAME
 * 中的内容删除对应的.o文件（删除是在第一次make）
 */
void generic_file_use_generic(GenericFile *self)
{
	  if(self->secondCompileFile==NULL){
		   char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
		   char fileName[255];
		   sprintf(fileName,"%s/%s",objectPath,AET_NEED_SECOND_FILE_NAME);
		   self->secondCompileFile=link_file_new(fileName);
		   //说明要编译两次，把当前文件从makefile传来的参数也保存在与.o同目录的位置
		   makefile_parm_write_compile_argv(makefile_parm_get());
	  }
	  char *fileName=makefile_parm_get_object_file(makefile_parm_get());
	  link_file_add_second_compile_file(self->secondCompileFile,&fileName,1);
}


////////////////////////////////////////////新的方法----------------------------------

static void initXmlFile(GenericFile *self)
{
   if(self->xmlFile==NULL){
	   char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
	   char fileName[255];
	   sprintf(fileName,"%s/%s",objectPath,GENERIC_BLOCK_INTO_FILE_NAME);
	   self->xmlFile=xml_file_new(fileName);
   }
}


/**
 * 保存:1.块数与未定泛型的new对象。2.类实现中的块（块只能在类实现在）。3.类中未确定泛型的的new$ object。4.类中确定泛型的new$ object,5. 类中的泛型函数调用
 * 保存块所属的函数以及所属函数是否是泛型函数，如:_Z7AObject7AObjectEPN7AObjectE$WWtx$0$WWtx$
 * 在类impl完成后调用
 */
void generic_file_save(GenericFile *self,ClassName *className)
{
	   nboolean second=  makefile_parm_is_second_compile(makefile_parm_get());
	   if(second){
		 n_warning("第二次编译不需要再保存 %s 文件:%s\n",className->sysName,in_fnames[0]);
	     return;
	   }
	   initXmlFile(self);
	   int blockCount=block_mgr_get_block_count(block_mgr_get(),className);
	   int undefineObject=block_mgr_get_undefine_newobj_count(block_mgr_get(),className);
	   int defineObject=block_mgr_get_define_newobj_count(block_mgr_get(),className);
		GenericSaveData *genData=xml_file_create_generic_data();
		genData->id=0;
		genData->name=n_strdup(className->sysName);
		genData->blockCount=blockCount;
		genData->undefineCount=undefineObject;
		genData->defineCount=defineObject;
	   int i;
	   for(i=0;i<blockCount;i++){
		   char   **var=block_mgr_get_block_source(block_mgr_get(),className,i);
		   //var[0]=_test_AObject__inner_generic_func_1_create_block_var
		   genData->blocks[i].key=n_strdup(var[0]);
		   genData->blocks[i].value=n_strdup(var[1]);
		   genData->blocks[i].index=i;
		   n_strfreev(var);
	   }
	   //test_Abc_create_undefine_var ="test_Abc$WWtx$test_AObject$WWtx$E0$WWtx$"
	   char **undefineObjectSource=block_mgr_get_undefine_newobj_source_codes(block_mgr_get(),className);
	   if(undefineObjectSource!=NULL){
		   genData->undefine.key=n_strdup(undefineObjectSource[0]);
		   genData->undefine.value=n_strdup(undefineObjectSource[1]);
		   n_strfreev(undefineObjectSource);
	   }
	   //test_Abc_create_define_var ="test_Abc$WWtx$test_AObject$WWtx$E0$WWtx$"
	   char **defineObjectSource=block_mgr_get_define_newobj_source_codes(block_mgr_get(),className);
	   if(defineObjectSource!=NULL){
		   genData->define.key=n_strdup(defineObjectSource[0]);
		   genData->define.value=n_strdup(defineObjectSource[1]);
		   n_strfreev(defineObjectSource);
	   }

	   //保存未定义泛型函数调用
	   char  **undefineFuncCall=block_mgr_get_undefine_func_call_source_codes(block_mgr_get(),className);
	   if(undefineFuncCall!=NULL){
		   genData->undefineFuncCall.key=n_strdup(undefineFuncCall[0]);
		   genData->undefineFuncCall.value=n_strdup(undefineFuncCall[1]);
		   n_strfreev(undefineFuncCall);
	   }

	   char  **defineFuncCall=block_mgr_get_define_func_call_source_codes(block_mgr_get(),className);
	   if(defineFuncCall!=NULL){
		   genData->defineFuncCall.key=n_strdup(defineFuncCall[0]);
		   genData->defineFuncCall.value=n_strdup(defineFuncCall[1]);
		   n_strfreev(defineFuncCall);
	   }

	   //保存类属于那个文件
	  {
	    char *key=generic_util_create_class_key_in_file(className->sysName);
	    char *fileName=in_fnames[0];
		genData->classfile.key=n_strdup(key);
	    genData->classfile.value=n_strdup(fileName);
	    n_free(key);
	  }
	  if(blockCount==0 && undefineObject==0 && defineObject==0 && undefineFuncCall==NULL && defineFuncCall==NULL){
		  xml_file_remove_generic(self->xmlFile,0,className->sysName);
	  }else{
		   xml_file_add_generic(self->xmlFile,genData);
	  }
	  xml_file_free_generic_data(genData);
}

/**
 * 保存当前文件中new$泛型确定的对象
 * /home/sns/workspace/ai/src/test/ai.c=unknown$WWtx$test_IMatrix$WWtx$int0$WWtx$
 * unknown$WWtx$test_LU$WWtx$int0$WWtx$
 * 在.c编译完后调用
 */
void generic_file_save_define_new$(GenericFile *self,char *content)
{
   char *fileName=in_fnames[0];
   initXmlFile(self);
   if(content!=NULL){
	 GenericSaveData *genData=xml_file_create_generic_data();
	 genData->id=1;
	 genData->name=n_strdup(fileName);
	 genData->value=n_strdup(content);
	 xml_file_add_generic(self->xmlFile,genData);
	 xml_file_free_generic_data(genData);
   }else{
	 xml_file_remove_generic(self->xmlFile,1,fileName);
   }
}

/**
 * 保存非类中的泛型调用
 * 在.c编译完后调用
 */
void generic_file_save_out_func_call(GenericFile *self,char *content)
{
   char *varName=createFuncCallVarName();
   initXmlFile(self);
   n_info("编译完成后，保存非本类中的泛型函数调用 变量名：%s 内容:%s\n",varName,content);
   if(content!=NULL){
	  GenericSaveData *genData=xml_file_create_generic_data();
	  genData->id=2;
	  genData->name=n_strdup(varName);
	  genData->value=n_strdup(content);
	  xml_file_add_generic(self->xmlFile,genData);
	  xml_file_free_generic_data(genData);
   }else{
	  xml_file_remove_generic(self->xmlFile,2,varName);
   }
   n_free(varName);
}

////////////////////以上是保存，下面是从文件获取---------------------

static NPtrArray *getGendericSaveDataArray(GenericFile *self)
{
   initXmlFile(self);
   if(self->genericDataArray==NULL){
	   self->genericDataArray=xml_file_get_generic_data(self->xmlFile);
   }
   return self->genericDataArray;
}

static GenericSaveData *getGendericSaveData(GenericFile *self,NPtrArray *array,char *sysName)
{
	  if(array==NULL || sysName==NULL)
		  return NULL;
	  int i;
	  GenericSaveData *saveData=NULL;
	  for(i=0;i<array->len;i++){
		  GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(array,i);
		  if(strcmp(item->name,sysName)==0 && item->id==0){
			  saveData=item;
			  break;
		  }
	  }
	  return saveData;
}

BlockContent *generic_file_get_block_content(GenericFile *self,char *sysName)
{
	  NPtrArray *array=getGendericSaveDataArray(self);
	  int i;
	  GenericSaveData *saveData=getGendericSaveData(self,array,sysName);
	  if(saveData==NULL || saveData->blockCount==0){
		  return NULL;
	  }
	  BlockContent *bc=(BlockContent *)n_slice_new(BlockContent);
	  bc->blocksCount=saveData->blockCount;
	  bc->sysName=n_strdup(sysName);
	  for(i=0;i<saveData->blockCount;i++){
	 		  //_Z7AObject7AObjectEPN7AObjectE$WWtx$0$WWtx$ void _test_AObject__inner_generic_func_1(test_AObject
	 		  char *func=saveData->blocks[i].value;
	 		  nchar **items=n_strsplit(func,NEW_OBJECT_GENERIC_SPERATOR,-1);
	 		  BlockDetail *detail=(BlockDetail *)n_slice_new(BlockDetail);
	 		  detail->belongFunc=n_strdup(items[0]);
	 		  detail->isFuncGeneric=atoi(items[1]);
	 		  detail->codes=n_strdup(items[2]);
	 		  bc->content[i]=detail;
	 		  n_strfreev(items);
	   }
	   return bc;
}


NPtrArray *generic_file_get_undefine_new_object(GenericFile *self,char *belongSysName)
{
          NPtrArray *array=getGendericSaveDataArray(self);
		  GenericSaveData *saveData=getGendericSaveData(self,array,belongSysName);
		  if(saveData==NULL){
		 	 return NULL;
		  }
		  NPtrArray *dataArray=NULL;
		  if(saveData->undefine.key){
			  dataArray= generic_storage_relove_new_object(saveData->undefine.key,saveData->undefine.value);
		  }
		  return dataArray;
}

NPtrArray *generic_file_get_define_new_object(GenericFile *self,char *belongSysName)
{
	      NPtrArray *array=getGendericSaveDataArray(self);
		  GenericSaveData *saveData=getGendericSaveData(self,array,belongSysName);
		  if(saveData==NULL){
		 	 return NULL;
		  }
		  NPtrArray *dataArray=NULL;
		  if(saveData->define.key){
			  dataArray= generic_storage_relove_new_object(saveData->define.key,saveData->define.value);
		  }
		  return dataArray;
}

NPtrArray *generic_file_get_define_func_call(GenericFile *self,char *belongSysName)
{
	  NPtrArray *array=getGendericSaveDataArray(self);
	  GenericSaveData *saveData=getGendericSaveData(self,array,belongSysName);
	  if(saveData==NULL){
	 	 return NULL;
	  }
	  NPtrArray *dataArray=NULL;
	  if(saveData->defineFuncCall.key){
		  dataArray= generic_storage_relove_func_call(saveData->defineFuncCall.key,saveData->defineFuncCall.value);
	  }
	  return dataArray;
}

NPtrArray    *generic_file_get_undefine_func_call(GenericFile *self,char *belongSysName)
{
	  NPtrArray *array=getGendericSaveDataArray(self);
	  GenericSaveData *saveData=getGendericSaveData(self,array,belongSysName);
	  if(saveData==NULL){
	 	 return NULL;
	  }
	  NPtrArray *dataArray=NULL;
	  if(saveData->undefineFuncCall.key){
		  dataArray= generic_storage_relove_func_call(saveData->undefineFuncCall.key,saveData->undefineFuncCall.value);
	  }
	  return dataArray;
}



char *generic_file_get_file_class_located(GenericFile *self,char *sysName)
{
	  NPtrArray *array=getGendericSaveDataArray(self);
	  GenericSaveData *saveData=getGendericSaveData(self,array,sysName);
	  if(saveData==NULL){
		 return NULL;
	  }
	  char *fileName=NULL;
	  if(saveData->classfile.key && saveData->classfile.value){
		 fileName=n_strdup(saveData->classfile.value);
	  }
	  return fileName;
}

GenericFile *generic_file_get()
{
	static GenericFile *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(GenericFile));
		 genericFileInit(singleton);
	}
	return singleton;
}


