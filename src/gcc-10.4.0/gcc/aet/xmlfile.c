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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "classmgr.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "makefileparm.h"
#include "classutil.h"

#include "xmlfile.h"
#include <utime.h>

#define IFACE_ID               "ifaceid"
#define GENERIC_ID             "genericid"
#define FUNCCHECK_ID           "funccheck"
#define CLASS_CONTENT          "<content>"
#define CLASS_CONTENT_HALF     "<content"
#define CLASS_CONTENT_END      "</content>"
#define CLASS_NAME             "<classname>"
#define CLASS_NAME_END         "</classname>"
#define CLASS_TITLE            "<title>"
#define CLASS_TITLE_END        "</title>"
#define CLASS_ITEM             "<item>"
#define CLASS_ITEM_END         "</item>"
#define OBJECT_FILE            "<objectfile>"
#define OBJECT_FILE_END        "</objectfile>"


#define CLASS_SECTION_HALF     "<section"
#define CLASS_SECTION_END      "</section>"

#define FILE_BUFFER_SIZE   512*1024


static void xmlFileInit(XmlFile *self)
{
	self->linkFile=NULL;
}

static char *dupStr(NString *src)
{
	nboolean r1=n_string_starts_with(src,"\n");
	nboolean r2=n_string_ends_with(src,"\n");
	if(r1 && r2){
		char *tt=src->str+1;
		return n_strndup(src->str+1,src->len-2);
	}else if(r1 && !r2)
		return n_strdup(src->str+1);
	else if(!r1 && r2)
		return n_strndup(src->str,src->len-1);
	else
    	return n_strdup(src->str);
}

/**
 * 从文件读出的数据创建成array
 */
static NPtrArray *createFuncCheckData(char *content)
{
    NPtrArray *array=n_ptr_array_new();
	NString *re=n_string_new(content);
	int pos=0;
	do{
		pos=n_string_indexof_from(re,CLASS_CONTENT,pos);
		if(pos>0){
			int pos1=n_string_indexof_from(re,CLASS_NAME,pos);
			if(pos1>0){
				int pos2=n_string_indexof_from(re,CLASS_NAME_END,pos1);
				NString *sysName=n_string_substring_from(re,pos1+strlen(CLASS_NAME),pos2);

				int pos3=n_string_indexof_from(re,CLASS_TITLE,pos2);
				int pos4=n_string_indexof_from(re,CLASS_TITLE_END,pos3);
				NString *title=n_string_substring_from(re,pos3+strlen(CLASS_TITLE),pos4);

				int pos5=n_string_indexof_from(re,CLASS_ITEM,pos4);
				int pos6=n_string_indexof_from(re,CLASS_ITEM_END,pos5);
				NString *item=n_string_substring_from(re,pos5+strlen(CLASS_ITEM),pos6);

				int pos7=n_string_indexof_from(re,OBJECT_FILE,pos6);
				int pos8=n_string_indexof_from(re,OBJECT_FILE_END,pos7);
				NString *objectFile=n_string_substring_from(re,pos7+strlen(OBJECT_FILE),pos8);


				FuncCheckData *check=n_slice_new(FuncCheckData);
				check->sysName=dupStr(sysName);
				check->title=dupStr(title);
				check->item=dupStr(item);
				check->objectFile=dupStr(objectFile);

				n_string_free(sysName,TRUE);
				n_string_free(title,TRUE);
				n_string_free(item,TRUE);
				n_string_free(objectFile,TRUE);
                pos=pos8;
                n_ptr_array_add(array,check);
			}
		}
	}while(pos>0);
	return array;
}

static void add(NPtrArray *array,char *sysName,char *title,char *item,char *objectFile)
{
	int i;
	for(i=0;i<array->len;i++){
		FuncCheckData *item=n_ptr_array_index(array,i);
    	if(strcmp(item->sysName,sysName)==0){
    		n_ptr_array_remove_index(array,i);
    		break;
    	}
	}
	FuncCheckData *check=n_slice_new(FuncCheckData);
	check->sysName=n_strdup(sysName);
	check->title=n_strdup(title);
	check->item=n_strdup(item);
	check->objectFile=n_strdup(objectFile);
	n_ptr_array_add(array,check);
}

/**
 * 写入以下形式的内容到文件
 */
static void save(XmlFile *self,NPtrArray *array)
{
	NString *codes=n_string_new("");
	int i;
	for(i=0;i<array->len;i++){
	   FuncCheckData *cc=n_ptr_array_index(array,i);
	   n_string_append_printf(codes,"%s\n",CLASS_CONTENT);
	   n_string_append_printf(codes,"%s%s%s\n",CLASS_NAME,cc->sysName,CLASS_NAME_END);
	   n_string_append_printf(codes,"%s\n%s\n%s\n",CLASS_TITLE,cc->title,CLASS_TITLE_END);
	   n_string_append_printf(codes,"%s\n%s\n%s\n",CLASS_ITEM,cc->item,CLASS_ITEM_END);
	   n_string_append_printf(codes,"%s\n%s\n%s\n",OBJECT_FILE,cc->objectFile,OBJECT_FILE_END);
	   n_string_append_printf(codes,"%s\n",CLASS_CONTENT_END);
	}
	char sec[255];
	sprintf(sec,"<section xml:id=\"%s\">\n",FUNCCHECK_ID);
	n_string_prepend(codes,sec);
	n_string_append(codes,"</section>\n");
	//printf("保存的类方法检查xml数据 :%s\n",codes->str);
	link_file_write(self->linkFile,codes->str,codes->len);
 	n_string_free(codes,TRUE);
}


/*
 * 加入要检查的类方法数据
 */
void xml_file_add_func_check(XmlFile *self,char *sysName,char *title,char *item)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	NPtrArray *array=createFuncCheckData(buffer);
	char  *objectFile=class_util_get_o_file();
	add(array,sysName,title,item,objectFile);
	save(self,array);
	n_ptr_array_unref(array);
	link_file_unlock(fd);
}

NPtrArray *xml_file_get_func_check(XmlFile *self)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	NPtrArray *array=createFuncCheckData(buffer);
	link_file_unlock(fd);
	return array;
}

#define XML_ID         "id"
#define XML_NAME       "name"
#define XML_SYS_NAME   "sysname"
#define LINE_BREAK      1  //所有的标签后都有一个换行符，所以取字符串时要多加一个换行符

////////////////////////////////////////////------------泛型----------------------------------------------------------

#define BLOCK_COUNT    "blockcount"
#define UNDEFINE_COUNT "undefine"
#define DEFINE_COUNT   "define"
static void appendGenericClass(XmlFile *self,int blockCount,int undefineObject,int defineObject,char *sysName,NString *codes)
{
	   n_string_append_printf(codes,"%s %s=0 %s=%s %s=%d %s=%d %s=%d>\n",
			   CLASS_CONTENT_HALF,XML_ID,XML_NAME,sysName,BLOCK_COUNT,blockCount,UNDEFINE_COUNT,undefineObject,DEFINE_COUNT,defineObject);
}


/**
 * 保存当前文件中new$泛型确定的对象
 * /home/sns/workspace/ai/src/test/ai.c=unknown$WWtx$test_IMatrix$WWtx$int0$WWtx$
 * unknown$WWtx$test_LU$WWtx$int0$WWtx$
 * 在.c编译完后调用
 */
static void appendInFileNew(XmlFile *self,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s %s=1 %s=%s>\n",CLASS_CONTENT_HALF,XML_ID,XML_NAME,key);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",CLASS_CONTENT_END);
}

/**
 * 保存非类中的泛型调用
 * 在.c编译完后调用
 */
static void appendInFileFuncCall(XmlFile *self,char *key,char *value,NString *codes)
{
   n_string_append_printf(codes,"%s %s=2 %s=%s>\n",CLASS_CONTENT_HALF,XML_ID,XML_NAME,key);
   n_string_append_printf(codes,"%s",value);
   n_string_append_printf(codes,"%s\n",CLASS_CONTENT_END);
}

#define BLOCK_ITEM_HALF "<blockitem"
#define BLOCK_ITEM_END "</blockitem>"

static void appendBlock(XmlFile *self,int order,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s key=%s order=%d>\n",BLOCK_ITEM_HALF,key,order);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",BLOCK_ITEM_END);
}



#define UNDEFINE_SOURCE_HALF "<undefine"
#define UNDEFINE_SOURCE_END "</undefine>"
static void appendUndefine(XmlFile *self,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s key=%s>\n",UNDEFINE_SOURCE_HALF,key);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",UNDEFINE_SOURCE_END);
}

#define DEFINE_SOURCE_HALF "<define"
#define DEFINE_SOURCE_END "</define>"
static void appendDefine(XmlFile *self,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s key=%s>\n",DEFINE_SOURCE_HALF,key);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",DEFINE_SOURCE_END);
}


#define UNDEFINE_FUNC_CALL_HALF "<funccallundefine"
#define UNDEFINE_FUNC_CALL_END "</funccallundefine>"
static void appendUndefineFuncCall(XmlFile *self,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s key=%s>\n",UNDEFINE_FUNC_CALL_HALF,key);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",UNDEFINE_FUNC_CALL_END);
}

#define DEFINE_FUNC_CALL_HALF "<funccalldefine"
#define DEFINE_FUNC_CALL_END "</funccalldefine>"
static void appendDefineFuncCall(XmlFile *self,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s key=%s>\n",DEFINE_FUNC_CALL_HALF,key);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",DEFINE_FUNC_CALL_END);
}

#define CLASS_BELONG_FILE_HALF "<classfile"
#define CLASS_BELONG_FILE_END "</classfile>"
static void appendClassFile(XmlFile *self,char *key,char *value,NString *codes)
{
	   n_string_append_printf(codes,"%s key=%s>\n",CLASS_BELONG_FILE_HALF,key);
	   n_string_append_printf(codes,"%s",value);
	   n_string_append_printf(codes,"%s\n",CLASS_BELONG_FILE_END);
}

///////////////////////////取数据---------------------

static char *getProperty(NString *codes,char *match,int *from)
{
		int pos1=n_string_indexof_from(codes,match,*from);
		char *result=NULL;
		if(pos1>0){
			pos1+=strlen(match)+1;//跳过=号
			int pos2=n_string_indexof_from(codes," ",pos1);
			int pos3=n_string_indexof_from(codes,">",pos1);
			if(pos2>pos3 || pos2<0)
				pos2=pos3;
			if(pos2<0)
				return NULL;
			NString *value=n_string_substring_from(codes,pos1,pos2);
			*from=pos2;
			result=value->str;
			n_string_free(value,FALSE);
		}
		return result;
}

static void fillBlock(NString *codes,int *from,int end,GenericSaveData *genData)
{
		int pos=n_string_indexof_from(codes,BLOCK_ITEM_HALF,*from);
		if(pos>=end)
			return;
		if(pos>0){
			char *key=getProperty(codes,"key",&pos);
			char *order=getProperty(codes,"order",&pos);
			pos=n_string_indexof_from(codes,">",pos);
			int pos1=n_string_indexof_from(codes,BLOCK_ITEM_END,pos);
			if(pos<0 || pos1<0 || pos1>end){
				n_error("出错了在%s\n",BLOCK_ITEM_END);
			}
			NString *value=n_string_substring_from(codes,pos+1+LINE_BREAK,pos1);
			*from=pos1+strlen(BLOCK_ITEM_END);
			int index=atoi(order);
			genData->blocks[index].key=key;
			genData->blocks[index].value=value->str;
			n_string_free(value,FALSE);
			n_free(order);
			fillBlock(codes,from,end,genData);
		}
}


static void fillOther(NString *codes,int *from,int end,char *half,char *endTag,GenericSaveData *genData)
{
		int pos=n_string_indexof_from(codes,half,*from);
		if(pos>=end)
			return;
		if(pos>0){
			char *key=getProperty(codes,"key",&pos);
			pos=n_string_indexof_from(codes,">",pos);
			int pos1=n_string_indexof_from(codes,endTag,pos);
			if(pos<0 || pos1<0 || pos1>end){
				n_error("出错了在 pos:%d pos1:%d end:%d %s\n",pos,pos1,end,endTag);
			}
			NString *value=n_string_substring_from(codes,pos+1+LINE_BREAK,pos1);
			*from=pos1+strlen(endTag);
			if(!strcmp(endTag,UNDEFINE_SOURCE_END)){
				genData->undefine.key=key;
				genData->undefine.value=value->str;
			}else if(!strcmp(endTag,DEFINE_SOURCE_END)){
				genData->define.key=key;
			    genData->define.value=value->str;
			}else if(!strcmp(endTag,UNDEFINE_FUNC_CALL_END)){
				genData->undefineFuncCall.key=key;
				genData->undefineFuncCall.value=value->str;
			}else if(!strcmp(endTag,DEFINE_FUNC_CALL_END)){
				genData->defineFuncCall.key=key;
				genData->defineFuncCall.value=value->str;
			}else if(!strcmp(endTag,CLASS_BELONG_FILE_END)){
				genData->classfile.key=key;
				genData->classfile.value=value->str;
		    }
			n_string_free(value,FALSE);
		}
}


/**
 * 从文件读出的数据创建成array
 */
static NPtrArray *createContent(char *content)
{
    NPtrArray *array=n_ptr_array_new();
	NString *codes=n_string_new(content);
	int pos=0;
	do{
		pos=n_string_indexof_from(codes,CLASS_CONTENT_HALF,pos);
		if(pos<0)
			break;
		int end=n_string_indexof_from(codes,CLASS_CONTENT_END,pos);
		if(end<0){
			n_error("出错在读content");
		}
		NString *value=n_string_substring_from(codes,pos,end+strlen(CLASS_CONTENT_END));
        n_ptr_array_add(array,value);
		pos=end+strlen(CLASS_CONTENT_END);
	}while(pos>0);
	return array;
}

/**
 * 顺序一定要与保存的一样。block-undefine-define-funccall-classfile
 */
static NPtrArray *createGenericData(char *content)
{
    NPtrArray *array=n_ptr_array_new_with_free_func(xml_file_free_generic_data);
	NString *codes=n_string_new(content);
	NPtrArray *contentArray=createContent(content);
	int i;
	for(i=0;i<contentArray->len;i++){
        NString *codes=(NString *)n_ptr_array_index(contentArray,i);
		int pos=n_string_indexof(codes,CLASS_CONTENT_HALF);
		int end=n_string_indexof_from(codes,CLASS_CONTENT_END,pos);
		GenericSaveData *genData=xml_file_create_generic_data();
		char *id= getProperty(codes,XML_ID,&pos);
		char *name= getProperty(codes,XML_NAME,&pos);
		genData->id=atoi(id);
		genData->name=name;
		n_free(id);
		if(genData->id==0){
			char *blockCount= getProperty(codes,BLOCK_COUNT,&pos);
			char *undefineCount= getProperty(codes,UNDEFINE_COUNT,&pos);
			char *defineCount= getProperty(codes,DEFINE_COUNT,&pos);
			genData->blockCount=atoi(blockCount);
			genData->undefineCount=atoi(undefineCount);
			genData->defineCount=atoi(defineCount);
			n_free(blockCount);
			n_free(undefineCount);
			n_free(defineCount);
			fillBlock(codes,&pos,end,genData);
			fillOther(codes,&pos,end,UNDEFINE_SOURCE_HALF,UNDEFINE_SOURCE_END,genData);
			fillOther(codes,&pos,end,DEFINE_SOURCE_HALF,DEFINE_SOURCE_END,genData);
			fillOther(codes,&pos,end,UNDEFINE_FUNC_CALL_HALF,UNDEFINE_FUNC_CALL_END,genData);
			fillOther(codes,&pos,end,DEFINE_FUNC_CALL_HALF,DEFINE_FUNC_CALL_END,genData);
			fillOther(codes,&pos,end,CLASS_BELONG_FILE_HALF,CLASS_BELONG_FILE_END,genData);
		}else{
			pos=n_string_indexof_from(codes,">",pos);
			NString *value=n_string_substring_from(codes,pos+1+LINE_BREAK,end);
			genData->value=n_strdup(value->str);
			n_string_free(value,TRUE);
		}
        n_ptr_array_add(array,genData);
	}
	return array;
}

static void saveGenericItem(XmlFile *self,GenericSaveData *item,NString *codes)
{
	  if(item->id==CLASS_GENERIC_TYPE){
		 appendGenericClass(self,item->blockCount,item->undefineCount,item->defineCount,item->name,codes);
		 int j;
		 for(j=0;j<item->blockCount;j++){
			 appendBlock(self,j,item->blocks[j].key,item->blocks[j].value,codes);
		 }
		 if(item->undefine.key){
			 appendUndefine(self,item->undefine.key,item->undefine.value,codes);
		 }
		 if(item->define.key){
			 appendDefine(self,item->define.key,item->define.value,codes);
		 }
		 if(item->undefineFuncCall.key){
			 appendUndefineFuncCall(self,item->undefineFuncCall.key,item->undefineFuncCall.value,codes);
		 }
		 if(item->defineFuncCall.key){
			 appendDefineFuncCall(self,item->defineFuncCall.key,item->defineFuncCall.value,codes);
		 }
		 if(item->classfile.key){
			appendClassFile(self,item->classfile.key,item->classfile.value,codes);
		 }
		 n_string_append_printf(codes,"%s\n",CLASS_CONTENT_END);
	 }else if(item->id==OUT_CLASS_NEW$_OBJECT){
		 appendInFileNew(self,item->name,item->value,codes);
	 }else if(item->id==OUT_LOCAL_CLASS_FUNC_CALL){
		 appendInFileFuncCall(self,item->name,item->value,codes);
	 }
}

static void saveGeneric(XmlFile *self,NPtrArray *array,GenericSaveData *addData)
{
     NString *codes=n_string_new("");
     int i;
     for(i=0;i<array->len;i++){
    	 GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(array,i);
    	 saveGenericItem(self,item,codes);
     }
     if(addData!=NULL){
    	 saveGenericItem(self,addData,codes);
     }
 	char sec[255];
 	sprintf(sec,"<section xml:id=\"%s\">\n",GENERIC_ID);
 	n_string_prepend(codes,sec);
 	n_string_append(codes,"</section>\n");
 	//printf("保存的泛型的xml数据 :%s\n",codes->str);
 	link_file_write(self->linkFile,codes->str,codes->len);
 	n_string_free(codes,TRUE);
}

void xml_file_add_generic(XmlFile *self,GenericSaveData *data)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	NPtrArray *array=createGenericData(buffer);
	int i;
	for(i=0;i<array->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(array,i);
		if(item->id==data->id && !strcmp(item->name,data->name)){
			n_ptr_array_remove_index(array,i);
			break;
		}
	}
	saveGeneric(self,array,data);
	n_ptr_array_unref(array);
	link_file_unlock(fd);
}

void  xml_file_remove_generic(XmlFile *self,int id,char *key)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	NPtrArray *array=createGenericData(buffer);
	int i;
	nboolean remove=FALSE;
	for(i=0;i<array->len;i++){
		GenericSaveData *item=(GenericSaveData *)n_ptr_array_index(array,i);
		if(item->id==id && !strcmp(item->name,key)){
			n_ptr_array_remove_index(array,i);
			remove=TRUE;
			break;
		}
	}
	if(remove)
		saveGeneric(self,array,NULL);
	n_ptr_array_unref(array);
	link_file_unlock(fd);
}

NPtrArray *xml_file_get_generic_data(XmlFile *self)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	NPtrArray *array=createGenericData(buffer);
	link_file_unlock(fd);
	return array;
}


GenericSaveData *xml_file_create_generic_data()
{
	GenericSaveData *data=n_slice_new0(GenericSaveData);
	data->undefine.key=NULL;
	data->undefine.value=NULL;
	return data;
}

void  xml_file_free_generic_data(GenericSaveData *data)
{
    int i;
    for(i=0;i<data->blockCount;i++){
    	n_free(data->blocks[i].key);
    	n_free(data->blocks[i].value);
    }
    if(data->undefine.key){
    	n_free(data->undefine.key);
    	n_free(data->undefine.value);
    }
    if(data->define.key){
    	n_free(data->define.key);
    	n_free(data->define.value);
    }
    if(data->undefineFuncCall.key){
    	n_free(data->undefineFuncCall.key);
    	n_free(data->undefineFuncCall.value);
    }

    if(data->defineFuncCall.key){
    	n_free(data->defineFuncCall.key);
    	n_free(data->defineFuncCall.value);
    }
    if(data->classfile.key){
    	n_free(data->classfile.key);
    	n_free(data->classfile.value);
    }
    if(data->value){
    	n_free(data->value);
    }
	n_free(data->name);
	n_slice_free(GenericSaveData,data);
}

///////////////////////////////////////////////以下是iface 全局变量------------------


#define IFACE_O_FILE        "ofile"
#define IFACE_C_FILE        "cfile"
#define IFACE_BE_QUOTED     "<bequoted>" //被引用
#define IFACE_BE_QUOTED_END "</bequoted>"
#define IFACE_IMPL_C_FILE     "<source>" //实现的源文件
#define IFACE_IMPL_C_FILE_END "</source>"
#define IFACE_IMPL_O_FILE     "<ofile>" //实现的源文件
#define IFACE_IMPL_O_FILE_END "</ofile>"

static void appendIfaceClass(XmlFile *self,char *sysName,NString *codes)
{
	   n_string_append_printf(codes,"%s %s=%s>\n",  CLASS_CONTENT_HALF,XML_SYS_NAME,sysName);
}

static void appendbeQuoted(XmlFile *self,char *beQuoted,NString *codes)
{
	   n_string_append_printf(codes,"%s%s%s\n",IFACE_BE_QUOTED,beQuoted,IFACE_BE_QUOTED_END);
}

static void appendCFile(XmlFile *self,char *cfile,NString *codes)
{
	   n_string_append_printf(codes,"%s%s%s\n",IFACE_IMPL_C_FILE,cfile,IFACE_IMPL_C_FILE_END);
}

static void appendOFile(XmlFile *self,char *ofile,NString *codes)
{
	   n_string_append_printf(codes,"%s%s%s\n",IFACE_IMPL_O_FILE,ofile,IFACE_IMPL_O_FILE_END);
}

IFaceSaveData *xml_file_create_iface_data()
{
	IFaceSaveData *data=n_slice_new0(IFaceSaveData);
	data->beQuotedArray=n_ptr_array_new_with_free_func(n_free);
	return data;
}

void  xml_file_free_iface_data(IFaceSaveData *data)
{
	    if(data->sysName){
	    	n_free(data->sysName);
	    }
	    if(data->cFile){
	    	n_free(data->cFile);
	    }
	    if(data->oFile){
	    	n_free(data->oFile);
	    }
	    n_ptr_array_unref(data->beQuotedArray);
	    n_slice_free(IFaceSaveData,data);
}



static void fillBeQuoted(NString *codes,int *from,int end,IFaceSaveData *ifaceData)
{
	int pos=n_string_indexof_from(codes,IFACE_BE_QUOTED,*from);
	if(pos>=end)
		return;
	if(pos>0){
		int pos1=n_string_indexof_from(codes,IFACE_BE_QUOTED_END,pos);
		if(pos1<0 || pos1>end){
			n_error("出错了在%s\n",IFACE_BE_QUOTED_END);
		}
		NString *value=n_string_substring_from(codes,pos+strlen(IFACE_BE_QUOTED),pos1);
		*from=pos1+strlen(IFACE_BE_QUOTED_END);
		n_ptr_array_add(ifaceData->beQuotedArray,value->str);
		n_string_free(value,FALSE);
		fillBeQuoted(codes,from,end,ifaceData);
	}
}

static void fillCFileOrOFile(NString *codes,int *from,int end,IFaceSaveData *ifaceData,nboolean cfile)
{
	int pos=n_string_indexof_from(codes,cfile?IFACE_IMPL_C_FILE:IFACE_IMPL_O_FILE,*from);
	if(pos>=end)
		return;
	if(pos>0){
		int pos1=n_string_indexof_from(codes,cfile?IFACE_IMPL_C_FILE_END:IFACE_IMPL_O_FILE_END,pos);
		if(pos1<0 || pos1>end){
			n_error("出错了在%s\n",cfile?IFACE_IMPL_C_FILE_END:IFACE_IMPL_O_FILE_END);
		}
		NString *value=n_string_substring_from(codes,pos+strlen(cfile?IFACE_IMPL_C_FILE:IFACE_IMPL_O_FILE),pos1);
		*from=pos1+strlen(cfile?IFACE_IMPL_C_FILE_END:IFACE_IMPL_O_FILE_END);
		if(cfile)
		  ifaceData->cFile=value->str;
		else
		  ifaceData->oFile=value->str;
		n_string_free(value,FALSE);
	}
}

/**
 *
 */
static NPtrArray *createIfaceData(char *content)
{
    NPtrArray *array=n_ptr_array_new_with_free_func(xml_file_free_iface_data);
	NString *codes=n_string_new(content);
	NPtrArray *contentArray=createContent(content);
	int i;
	for(i=0;i<contentArray->len;i++){
        NString *codes=(NString *)n_ptr_array_index(contentArray,i);
		int pos=n_string_indexof(codes,CLASS_CONTENT_HALF);
		int end=n_string_indexof_from(codes,CLASS_CONTENT_END,pos);
		IFaceSaveData *ifaceData=xml_file_create_iface_data();
		char *sysName= getProperty(codes,XML_SYS_NAME,&pos);
		ifaceData->sysName=sysName;
		fillCFileOrOFile(codes,&pos,end,ifaceData,TRUE);
		fillCFileOrOFile(codes,&pos,end,ifaceData,FALSE);
		fillBeQuoted(codes,&pos,end,ifaceData);
        n_ptr_array_add(array,ifaceData);
	}
	return array;
}

static void saveIface(XmlFile *self,NPtrArray *array)
{
     NString *codes=n_string_new("");
     int i;
     for(i=0;i<array->len;i++){
    	 IFaceSaveData *item=(IFaceSaveData *)n_ptr_array_index(array,i);
    	 appendIfaceClass(self,item->sysName,codes);
    	 if(item->cFile && item->oFile){
    		 appendCFile(self,item->cFile,codes);
    		 appendOFile(self,item->oFile,codes);
    	 }
		 int j;
		 for(j=0;j<item->beQuotedArray->len;j++){
			 char *cofile=(char *)n_ptr_array_index(item->beQuotedArray,j);
			 appendbeQuoted(self,cofile,codes);
		 }
		 n_string_append_printf(codes,"%s\n",CLASS_CONTENT_END);
     }
 	 char sec[255];
 	 sprintf(sec,"<section xml:id=\"%s\">\n",IFACE_ID);
 	 n_string_prepend(codes,sec);
 	 n_string_append(codes,"</section>\n");
 	 //printf("保存的泛型的xml数据 :%s\n",codes->str);
 	 link_file_write(self->linkFile,codes->str,codes->len);
 	 n_string_free(codes,TRUE);
}

static void removeBeQuotedFile(NPtrArray *array,char *cfile)
{
	int i;
	for(i=0;i<array->len;i++){
		IFaceSaveData *item=(IFaceSaveData *)n_ptr_array_index(array,i);
		int j;
		for(j=0;j<item->beQuotedArray->len;j++){
		   char *cofile=(char *)n_ptr_array_index(item->beQuotedArray,j);
		   if(strstr(cofile,cfile)){
			   n_ptr_array_remove_index(item->beQuotedArray,j);
			   break;
		   }
		}
	}
}

#define SEPARATION "#$%"

static void addBeQuotedFile(NPtrArray *array,char *cfile,char *ofile,char *sysName)
{
	nboolean hadAdd=FALSE;
	int i;
	for(i=0;i<array->len;i++){
		IFaceSaveData *item=(IFaceSaveData *)n_ptr_array_index(array,i);
		if(!strcmp(item->sysName,sysName)){
		   n_ptr_array_add(item->beQuotedArray,n_strdup(cfile));
		   hadAdd=TRUE;
		   break;
		}
	}
	if(!hadAdd){
		IFaceSaveData *ifaceData=xml_file_create_iface_data();
		ifaceData->sysName=n_strdup(sysName);
		char *ff=n_strdup_printf("%s%s%s",cfile,SEPARATION,ofile);
		n_ptr_array_add(ifaceData->beQuotedArray,ff);
		n_ptr_array_add(array,ifaceData);
	}
}

/**
 * 加iface
 */
void xml_file_add_iface(XmlFile *self,NPtrArray *ifaceSysName,char *cfile,char *ofile)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	NPtrArray *array=createIfaceData(buffer);
	if(strstr(buffer,cfile)){
		//说明文件内有该文件名 全部移走
		removeBeQuotedFile(array,cfile);
	}else{
		int i;
		for(i=0;i<ifaceSysName->len;i++){
			char *sysName=(char *)n_ptr_array_index(ifaceSysName,i);
			addBeQuotedFile(array,cfile,ofile,sysName);
		}
	}
	saveIface(self,array);
	n_ptr_array_unref(array);
	link_file_unlock(fd);
}

void xml_file_write_back_iface_data(XmlFile *self,NPtrArray *ifaceSaveData)
{
	saveIface(self,ifaceSaveData);
}


nboolean xml_file_remove_iface(XmlFile *self,char *cfile)
{
	int fd= link_file_lock(self->linkFile);
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		rev=0;
	buffer[rev]='\0';
	if(!strstr(buffer,cfile)){
		//说明文件内有该文件名 全部移走
		link_file_unlock(fd);
		return FALSE;
	}else{
		NPtrArray *array=createIfaceData(buffer);
		removeBeQuotedFile(array,cfile);
		saveIface(self,array);
	    n_ptr_array_unref(array);
	}
	link_file_unlock(fd);
	return TRUE;
}

NPtrArray   *xml_file_get_iface_data(XmlFile *self)
{
	char buffer[FILE_BUFFER_SIZE];
	int rev= link_file_read(self->linkFile,buffer,FILE_BUFFER_SIZE);
	if(rev<=0)
		return NULL;
	buffer[rev]='\0';
	NPtrArray *array=createIfaceData(buffer);
	return array;
}

void  xml_file_get_iface_file(char *src,char **cFile,char **oFile)
{
	char **items=n_strsplit(src,SEPARATION,-1);
	if(items==NULL)
		return;
	nuint len=n_strv_length(items);
	if(len!=2){
		n_strfreev(items);
		return;
	}
	*cFile=n_strdup(items[0]);
	*oFile=n_strdup(items[1]);
	 n_strfreev(items);
}

/////////////////////////-----------------------读#define AET_REF_LIB_FILE_NAME "aet_ref_lib_file.tmp"


static NPtrArray *createSection(char *content)
{
    NPtrArray *array=n_ptr_array_new();
	NString *codes=n_string_new(content);
	int pos=0;
	do{
		pos=n_string_indexof_from(codes,CLASS_SECTION_HALF,pos);
		if(pos<0)
			break;
		int end=n_string_indexof_from(codes,CLASS_SECTION_END,pos);
		if(end<0){
			n_error("出错在读content");
		}
		NString *value=n_string_substring_from(codes,pos,end+strlen(CLASS_SECTION_END));
        n_ptr_array_add(array,value);
		pos=end+strlen(CLASS_CONTENT_END);
	}while(pos>0);
	return array;
}

static void import(NPtrArray *src,NPtrArray *dest)
{
	int i;
	for(i=0;i<src->len;i++){
		npointer pp=n_ptr_array_index(src,i);
		n_ptr_array_add(dest,pp);
	}
}

void  xml_file_get_lib(XmlFile *self,NPtrArray **dataArrays)
{
	char *text=link_file_read_text(self->linkFile);
	if(text==NULL)
		return;
    NPtrArray *ifaceArray=n_ptr_array_new();
    NPtrArray *genericArray=n_ptr_array_new();
    NPtrArray *funcCheckArray=n_ptr_array_new();

	NPtrArray *contentArray=createSection(text);
	n_free(text);
	int i;
	for(i=0;i<contentArray->len;i++){
		NString *codes=(NString *)n_ptr_array_index(contentArray,i);
		int pos=n_string_indexof(codes,">");
	    NString *sectionHalf=n_string_substring_from(codes,0,pos);
		int iIdPos=n_string_indexof(sectionHalf,IFACE_ID);
		int gIdPos=n_string_indexof(sectionHalf,GENERIC_ID);
		int fIdPos=n_string_indexof(sectionHalf,FUNCCHECK_ID);
		if(iIdPos>0){
			NPtrArray *array=createIfaceData(codes->str);
			import(array,ifaceArray);
		}else if(gIdPos>0){
			NPtrArray *array=createGenericData(codes->str);
			import(array,genericArray);

		}else if(fIdPos>0){
			NPtrArray *array=createFuncCheckData(codes->str);
			import(array,funcCheckArray);
		}
	}
	dataArrays[0]=ifaceArray;
	dataArrays[1]=genericArray;
	dataArrays[2]=funcCheckArray;

}

XmlFile *xml_file_new(char *fileName)
{
	 XmlFile *self =n_slice_alloc0 (sizeof(XmlFile));
	 xmlFileInit(self);
	 self->linkFile=link_file_new(fileName);
	 return self;
}



