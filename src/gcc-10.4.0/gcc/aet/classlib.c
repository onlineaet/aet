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
#include "gimple-expr.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"


#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "aetprinttree.h"
#include "classimpl.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "classinfo.h"
#include "classutil.h"
#include "classlib.h"
#include "makefileparm.h"
#include "middlefile.h"
#include "genericutil.h"




static void classLibInit(ClassLib *self)
{
	 self->xmlFile=NULL;
     self->ifaceArray=NULL;
     self->genericArray=NULL;
     self->funcCheckArray=NULL;
}

static void initFile(ClassLib *self)
{
	if(self->xmlFile==NULL){
       char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
       char fileName[255];
       sprintf(fileName,"%s/%s",objectPath,AET_REF_LIB_FILE_NAME);
       self->xmlFile=xml_file_new(fileName);
       NPtrArray *arrays[3];
       arrays[0]=arrays[1]=arrays[2]=NULL;
       xml_file_get_lib(self->xmlFile,arrays);//关键
       self->ifaceArray=arrays[0];
       self->genericArray=arrays[1];
       self->funcCheckArray=arrays[2];
	}
}

nboolean  class_lib_have_interface_static_define(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->ifaceArray==NULL)
		return FALSE;
	int i;
	for(i=0;i<self->ifaceArray->len;i++){
       IFaceSaveData *item=(IFaceSaveData *)n_ptr_array_index(self->ifaceArray,i);
       if(strcmp(item->sysName,sysName)==0)
    	   return TRUE;
	}
	return FALSE;
}

static BlockContent *getBlockContent(GenericSaveData *saveData)
{
  int i;
  if(saveData==NULL || saveData->blockCount==0){
	  return NULL;
  }
  BlockContent *bc=(BlockContent *)n_slice_new(BlockContent);
  bc->blocksCount=saveData->blockCount;
  bc->sysName=n_strdup(saveData->name);
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

BlockContent *class_lib_get_block_content(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->genericArray==NULL)
		return NULL;
	int i;
	for(i=0;i<self->genericArray->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(self->genericArray,i);
		if(strcmp(item->name,sysName)==0){
		   return getBlockContent(item);
		}
	}
	return NULL;
}

static NPtrArray *getUndefineObj(GenericSaveData *saveData)
{
	  if(saveData->undefineCount==0)
		  return NULL;
	  NPtrArray *dataArray=NULL;
	  if(saveData->undefine.key){
		  dataArray= generic_storage_relove_new_object(saveData->undefine.key,saveData->undefine.value);
	  }
	  if(dataArray==NULL)
		  return NULL;
	  if(dataArray->len==0){
		  n_ptr_array_unref(dataArray);
		  return NULL;
	  }
	  return dataArray;
}

NPtrArray    *class_lib_get_undefine_obj(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->genericArray==NULL)
		return NULL;
	int i;
	for(i=0;i<self->genericArray->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(self->genericArray,i);
		printf("class_lib_get_undefine_obj----%d id:%d %s %s\n",i,item->id,item->name,sysName);
		if(item->id==CLASS_GENERIC_TYPE && strcmp(item->name,sysName)==0){
		   return getUndefineObj(item);
		}
	}
	return NULL;
}

static NPtrArray *getDefineObj(GenericSaveData *saveData)
{
	  if(saveData->defineCount==0)
		  return NULL;
	  NPtrArray *dataArray=NULL;
	  if(saveData->define.key){
		  dataArray= generic_storage_relove_new_object(saveData->define.key,saveData->define.value);
	  }
	  if(dataArray==NULL)
		  return NULL;
	  if(dataArray->len==0){
		  n_ptr_array_unref(dataArray);
		  return NULL;
	  }
	  return dataArray;
}



NPtrArray    *class_lib_get_define_obj(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->genericArray==NULL)
		return NULL;
	int i;
	for(i=0;i<self->genericArray->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(self->genericArray,i);
		if(strcmp(item->name,sysName)==0){
		   return getDefineObj(item);
		}
	}
	return NULL;
}

NPtrArray    *class_lib_get_define_func_call(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->genericArray==NULL)
		return NULL;
	int i;
	for(i=0;i<self->genericArray->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(self->genericArray,i);
		if(strcmp(item->name,sysName)==0){
		}
	}
	return NULL;
}

NPtrArray    *class_lib_get_undefine_func_call(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->genericArray==NULL)
		return NULL;
	int i;
	for(i=0;i<self->genericArray->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(self->genericArray,i);
		if(strcmp(item->name,sysName)==0){
		}
	}
	return NULL;
}



char  *class_lib_get_file_class_located(ClassLib *self,char *sysName)
{
	initFile(self);
	if(self->genericArray==NULL)
		return NULL;
	int i;
	for(i=0;i<self->genericArray->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(self->genericArray,i);
		if(strcmp(item->name,sysName)==0){
		   return item->classfile.value;
		}
	}
	return NULL;
}




ClassLib *class_lib_get()
{
	static ClassLib *singleton = NULL;
	if (!singleton){
		 singleton =(ClassLib *)n_slice_new0(ClassLib);
		 classLibInit(singleton);
	}
	return singleton;
}



