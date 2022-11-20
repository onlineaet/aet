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
#include "libfile.h"
#include "makefileparm.h"
#include "middlefile.h"


#define LINE_SIZE 1024


static void libFileInit(LibFile *self)
{
	 self->resultArray=n_ptr_array_new_with_free_func(n_free);
}

static void appendData(char *src,char *buffer,int *offset)
{
    nchar**	items=n_strsplit(src," ",-1);
    int len=n_strv_length(items);
    int i;
    int a=0;
    int count=*offset;
    for(i=1;i<len;i++){
    	if(strlen(items[i])>0 && (items[i])[0]=='|')
    		break;
    	if(strlen(items[i])>0){
    	  sscanf(items[i], "%x", &a);
    	  buffer[count++]=a;
    	  //printf("oppend data is :count:%d %s %d\n",count,items[i],a);
    	}
    }
    *offset=count;
    n_strfreev(items);
}



static char *readContent(char *fileName,nint64 offset,int size,int *dataSize){
	//char *cmd="hexdump ./test -C -s 0x550 -n 28";
	char cmd[512];
	sprintf(cmd,"hexdump %s -C -s %#x -n %d",fileName,offset,size);
    FILE *fd = popen(cmd, "r");
    printf("readContent is 00 :%s %p\n",cmd,fd);
    char tempBuff[LINE_SIZE];
    char content[size+1];
    int dataLen=0;
	if(fd){
		while(TRUE){
		  char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
		  //printf("readContent ret is :%s\n",ret);
		  if(ret==NULL){
			 break;
		  }
		  if(ret!=NULL){
			  //printf("readContent bbbb 找到了变量 is 11 :%s\n",ret);
			  appendData(ret,content,&dataLen);
		  }
		}
		fclose(fd);
	}
	if(dataLen==0)
	  return NULL;
	char *data=(char *)n_malloc(dataLen);
	memcpy(data,content,dataLen);
	*dataSize=dataLen;
	return data;
}



typedef struct _SegmentData{
	char *name;
	nuint64 virt_addr;
	nuint64 offset;
	nuint64 size;
}SegmentData;

#define SEGMENT_NAME_DATA ".data"
#define SEGMENT_NAME_BSS ".bss"
#define SEGMENT_NAME_RODATA_1 ".rodata.str1.1"
#define SEGMENT_NAME_RODATA ".rodata"

static char *SEGMENT_NAME[4][2]={{".data","PROGBITS"},{".bss","NOBITS"},{".rodata.str1.1","PROGBITS"},{".rodata","PROGBITS"}};
static int SEGMENT_COUNT=4;


/**
 * 取.data段的地址
 * [22] .data             PROGBITS        0000000000004028 003028 000020 00  WA  0   0  8
 * [ 3] .bss              NOBITS          0000000000000000 000040 000000 00  WA  0   0  1

 */
static nint64 getSegmentVirtAdd(char *src,nint64 *dataOffset,nint64 *dataSize,char *prefix)
{
	if(src==NULL)
		return -1;
	NString *content=n_string_new(src);
	int index=n_string_indexof(content,prefix);
	if(index<0){
		n_string_free(content,TRUE);
		return -1;
	}
	//printf("getSegmentVirtAdd 00 is:%s\n",src);
	NString *sub=n_string_substring(content,index+strlen(prefix));
	n_string_trim(sub);
    nchar**	items=n_strsplit(sub->str," ",-1);
	n_string_free(content,TRUE);
	n_string_free(sub,TRUE);
    int len=n_strv_length(items);
    if(len==0)
    	return -1;
   // int i;
    //for(i=0;i<len;i++)
    	//printf("getSegmentVirtAdd info: is:%d %s\n",i,items[i]);
    nint64	add=n_ascii_strtoll(items[0],NULL,16);
    nint64	v1=n_ascii_strtoll(items[1],NULL,16);
    nint64	v2=n_ascii_strtoll(items[2],NULL,16);
    *dataOffset=v1;
    *dataSize=v2;
    return add;
}

/**
 * 从如下字符串中找出段名 .data
 * [ 2] .data             PROGBITS        0000000000000000 000040 000000 00  WA  0   0  1
 */
static char *getSegmentName(char *str)
{
	    NString *content=n_string_new(str);
		int index=n_string_indexof(content,".");
		if(index<0){
			n_string_free(content,TRUE);
			n_error("读段数据出错:%s 名字前不是.号\n",str);
			return NULL;
		}
		NString *sub=n_string_substring(content,index);
		index=n_string_indexof(sub," ");
		NString *last=n_string_substring_from(sub,0,index);
		char *result=n_strdup(last->str);
		n_string_free(content,TRUE);
		n_string_free(sub,TRUE);
		n_string_free(last,TRUE);
		return result;
}

static char *getPROGBITSorNOBITS(char *segName)
{
	int i;
	for(i=0;i<SEGMENT_COUNT;i++){
		if(strcmp(SEGMENT_NAME[i][0],segName)==0)
			return SEGMENT_NAME[i][1];
	}
	return NULL;
}

static SegmentData *createSegmentData(char *str)
{
	char *segName=getSegmentName(str);
	char *prefix=getPROGBITSorNOBITS(segName);
	if(prefix==NULL)
		return NULL;
	nint64 offset=0;
	nint64 size=0;
	nint64 virtAdd=getSegmentVirtAdd(str,&offset,&size,prefix);
	if(virtAdd<0){
		n_error("读段数据出错:%s\n",str);
	}
	SegmentData *data=(SegmentData *)n_slice_new0(SegmentData);
	data->virt_addr=virtAdd;
	data->offset=offset;
	data->size=size;
    data->name=segName;
    printf("段信息:%s virt:%llu offset:%llu size:%llu\n",data->name,data->virt_addr,data->offset,data->size);
	return data;
}

static NPtrArray *createSegmentFromFile(char *fileName)
{
		//char *cmd="readelf -S /home/sns/workspace/testblock/bin/libapptest.so | grep -P \"(bss|data)\"";
		char cmd[512];
		sprintf(cmd,"readelf -S -W %s | grep -P \"(bss|data)\"",fileName);
	    FILE *fd = popen(cmd, "r");
	    printf("findOffsetDataRoDataBss is 00 :%s %p\n",cmd,fd);
	    char tempBuff[LINE_SIZE];
	    NPtrArray *array=n_ptr_array_new();
		if(fd){
			while(TRUE){
			  char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
			  if(ret==NULL){
				 break;
			  }
			  if(ret!=NULL){
				  printf("readDataSegmentVirtAdd 找到了变量 is 11 :%s\n",ret);
				  SegmentData *data=createSegmentData(ret);
				  if(data!=NULL)
				     n_ptr_array_add(array,data);
			  }
			}
			fclose(fd);
		}
		return array;
}

typedef struct _VarInfo{
	char *name;
	nint64 virtAdd;
	nint64 size;
}VarInfo;

/**
 * 获取变量在data段的虚拟地址
 * src格式如下
 * 10: 0000000000004038     8 OBJECT  GLOBAL DEFAULT   22 _test_AObject_global_generic_name_abcd_1234_672$2_suiwtiwuer_ABdxxyyyyy
 */
static VarInfo *getVarInfo(char *src,char *varName)
{
	if(src==NULL)
		return NULL;
	NString *content=n_string_new(src);
	int index=n_string_indexof(content,":");
	if(index<0){
		n_string_free(content,TRUE);
		return NULL;
	}
	printf("getVarInfo 00 is:%s\n",src);
	NString *sub=n_string_substring(content,index+1);
	n_string_trim(sub);
    nchar**	items=n_strsplit(sub->str," ",-1);
	n_string_free(content,TRUE);
	n_string_free(sub,TRUE);
    int len=n_strv_length(items);
    if(len==0)
    	return NULL;
    int i;
    int size=0;
    for(i=1;i<len;i++){
    	//printf("getVarInfo is:%d %s\n",i,items[i]);
    	if(strlen(items[i])>0){
    		size=atoi(items[i]);
        	//printf("getVarInfo ----- is:%d %s %d\n",i,items[i],size);
    		break;
    	}
    }
	//printf("getVarInfo vir----- is:%s size:%d\n",items[0],size);
    nint64	add=n_ascii_strtoll(items[0],NULL,16);
    VarInfo *data=(VarInfo *)n_slice_new0(VarInfo);
    data->virtAdd=add;
    data->size=size;
    data->name=n_strdup(varName);
    return data;
}

/**
 * 从如下字符串中取出变量名 cyxx
 * 8: 0000000000000000     8 OBJECT  LOCAL  DEFAULT    6 cyxx
 * src来命令
 * readelf -s -W fileName | grep cyxx
 */
static char *getVarName(char *src)
{
    nchar**	items=n_strsplit(src," ",-1);
    int length=n_strv_length(items);
    if(length<=0)
    	return NULL;
    char *var=items[length-1];
    char *result=NULL;
    if(var && strlen(var)>0){
    	int len=strlen(var);
    	if(var[len-1]=='\n'){
          result=n_strndup(var,len-1);
    	}else{
          result=n_strdup(var);
    	}
    }
    n_strfreev(items);
    return result;
}

static VarInfo *createVarInfoFromFile(char *fileName,char *varName)
{
	    char cmd[512];
		sprintf(cmd,"readelf -s -W %s | grep %s",fileName,varName);
		FILE *fd = popen(cmd, "r");
		printf("createVarInfoFromFile is 00 :%s %p\n",cmd,fd);
		char tempBuff[LINE_SIZE];
		if(fd){
			while(TRUE){
			  char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
			  if(ret==NULL){
				 break;
			  }
			  if(ret!=NULL){
				  char *result=getVarName(ret);
				  if(result!=NULL && !strcmp(result,varName)){
				     printf("createVarInfoFromFile 找到了变量 is 11 :%s var:%s\n",ret,result);
				     VarInfo *info=getVarInfo(ret,varName);
				     n_free(result);
				     return info;
				  }
			  }
			}
			fclose(fd);
		}
		return NULL;
}

static SegmentData *getSegmentData(NPtrArray *array,char *name)
{
	int i;
	for(i=0;i<array->len;i++){
		SegmentData *item=n_ptr_array_index(array,i);
		if(strcmp(item->name,name)==0)
			return item;
	}
	return NULL;
}

static char *readOffset(char *fileName,nint64 offset,int size,int *dataSize){
	//char *cmd="hexdump ./test -C -s 0x550 -n 28";
	char cmd[512];
	sprintf(cmd,"hexdump %s -C -s %#x -n %d",fileName,offset,size);
    FILE *fd = popen(cmd, "r");
    //printf("readContent is 00 :%s %p\n",cmd,fd);
    char tempBuff[LINE_SIZE];
    char content[size+1];
    int dataLen=0;
	if(fd){
		while(TRUE){
		  char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
		 // printf("readContent ret is :%s\n",ret);
		  if(ret==NULL){
			 break;
		  }
		  if(ret!=NULL){
			  //printf("readContent bbbb 找到了变量 is 11 :%s\n",ret);
			  appendData(ret,content,&dataLen);
		  }
		}
		fclose(fd);
	}
	  //printf("readContent 数据多少:%d %s\n",dataLen,content);
	if(dataLen==0)
	  return NULL;
	char *data=(char *)n_malloc(dataLen);
	memcpy(data,content,dataLen);
	*dataSize=dataLen;
	return data;
}

static nboolean varInSegment(SegmentData *dataSeg,VarInfo *varInfo)
{
	   if(dataSeg==NULL)
		   return FALSE;
	//	   int_ary.virt_addr = 0x08049550
	//	   segment.data.virt_addr.begin = 0x0804954c
	//	   segmeng.data.virt_addr.end   = 0x0804954c + 0x034 = 0x08049580
        if(dataSeg->size==0)
        	return FALSE;
		nint64 begin=dataSeg->virt_addr;
		nint64 end=dataSeg->virt_addr+dataSeg->size;
		nint64 varAdd=varInfo->virtAdd+varInfo->size;
		if(varAdd>=begin && varAdd<=end){
			printf("说明变量:%s在%s段.11111",varInfo->name,dataSeg->name);
			return TRUE;
		}
		return FALSE;
}

static SegmentData *matchSegment(NPtrArray *segments,VarInfo *varInfo)
{
	int i;
	for(i=0;i<segments->len;i++){
		SegmentData *dataSeg=n_ptr_array_index(segments,i);
		nboolean in=varInSegment(dataSeg,varInfo);
		if(in)
			return dataSeg;
	}
	return NULL;
}

/**
 * 根据变量的地址加大小并不在任何段内。所以可能是一个指针变量。
 * 这里判断大小，如果变量大小是8(64位)或4(32位)
 */
static void guessPointerVar(char *fileName,NPtrArray *segmentArray,VarInfo *info)
{
	if(info->size==sizeof(long) && info->size>sizeof(int)){
		//64位
		;
	}else if(info->size==sizeof(int)){
		//32位
		;
	}else{
		return;
	}
	int i;
//	for(i=0;i<segmentArray->len;i++){
//		SegmentData *dataSeg=n_ptr_array_index(segmentArray,i);
//		nint64 varAdd=info->virtAdd - dataSeg->virt_addr+dataSeg->offset;//变量名本身的地址
//		printf("dddxxx : %s %lli\n",dataSeg->name,varAdd);
//		int dataLen=0;
//		char *valueVirtAddStr=readContent(fileName,varAdd,16,&dataLen);//变量名指向的初始值地址 00 20 00 00 00 00 00 00
//		if(dataLen==0){
//				n_error("getValue 从字符串取不出虚拟地址！%s %s",valueVirtAddStr,fileName);
//		}
//		nint64 valueVirtAdd=0;
//		memcpy(&valueVirtAdd,valueVirtAddStr,sizeof(nuint64));
//		printf("valueVirtAdd------valueVirtAdd:%#x %lli\n",valueVirtAdd,valueVirtAdd);
//		n_free(valueVirtAddStr);
//	}

	{
		//SegmentData *dataSeg=getSegmentData(segmentArray,".data");
		//SegmentData *dataSeg=getSegmentData(segmentArray,".data");
		SegmentData *dataSeg=getSegmentData(segmentArray,SEGMENT_NAME_RODATA_1);


		nint64 varAdd=info->virtAdd - dataSeg->virt_addr+dataSeg->offset;//变量名本身的地址
		printf("dddxxx : %s %lli\n",dataSeg->name,varAdd);
		int dataLen=0;
		char *valueVirtAddStr=readContent(fileName,varAdd,8,&dataLen);//变量名指向的初始值地址 00 20 00 00 00 00 00 00
		if(dataLen==0){
				n_error("getValue 从字符串取不出虚拟地址！%s %s",valueVirtAddStr,fileName);
		}
		nint64 valueVirtAdd=0;
		memcpy(&valueVirtAdd,valueVirtAddStr,sizeof(nuint64));
		printf("valueVirtAdd------valueVirtAdd:%#x %lli\n",valueVirtAdd,valueVirtAdd);
		n_free(valueVirtAddStr);

		SegmentData *roDataSeg=getSegmentData(segmentArray,SEGMENT_NAME_RODATA_1);
		nint64 valueAdd=valueVirtAdd - roDataSeg->virt_addr+roDataSeg->offset;

		    printf("valueVirtAdd 2222------valueVirtAdd:%#x %lli\n",valueAdd,valueAdd);

		    dataLen=0;
			char *valueStr=readContent(fileName,valueAdd,roDataSeg->size,&dataLen);//变量名指向的初始值地址 00 20 00 00 00 00 00 00
			if(dataLen==0){
				n_error("从字符串取不出虚拟地址！%s",fileName);
			}
			printf("last is:%s\n",valueStr);

	}

	//nint64 varAdd=info->virtAdd - dataSegmentVirtAdd+dataOffset;//变量名本身的地址
//	    printf("变量名本身的地址 varAdd------varAdd:%#x\n",varAdd);
//	    int dataLen=0;
//		char *valueVirtAddStr=readContent(fileName,varAdd,16,&dataLen);//变量名指向的初始值地址 00 20 00 00 00 00 00 00
//		if(dataLen==0){
//			n_error("getValue 从字符串取不出虚拟地址！%s %s",dataSegmentStr,fileName);
//		}
}

/**
 * 1. find the offset of .data .rodata .bss segment in executable-file
 * 2. find the virt-addr of global variable in executable-file
 * 8: 0000000000000000     8 OBJECT  LOCAL  DEFAULT    6 cyxx
 */
static char *getVarValue(char *varName,char *fileName,int *dataSize)
{
	//第一步
	NPtrArray *segmentArray=createSegmentFromFile(fileName);
    //第二步
	VarInfo *varInfo=createVarInfoFromFile(fileName,varName);
	if(varInfo==NULL){
		n_warning("在文件%s中找不到变量：%s",fileName,varName);
		return NULL;
	}
	//第三步
	SegmentData *dataSeg=matchSegment(segmentArray,varInfo);
	if(!dataSeg){
		n_warning("在四个段都没找到变量的值：%s\n",varName);
		//guessPointerVar(fileName,segmentArray,varInfo);
		return NULL;
	}
	//	   int_ary.virt_addr = 0x08049550
	//	   segment.data.virt_addr.begin = 0x0804954c
	//	   segmeng.data.virt_addr.end   = 0x0804954c + 0x034 = 0x08049580
	//第四步
	nint64 begin=dataSeg->virt_addr;
	nint64 varOffset=0;
	printf("说明变量:%s在%s段.\n",varInfo->name,dataSeg->name);
	nint64 segmentOffset = dataSeg->offset;
	varOffset= segmentOffset+ (varInfo->virtAdd - begin);
	char *rec=readOffset(fileName,varOffset,varInfo->size,dataSize);
	return rec;
}


/**
 * src字符串格式:
 * 0000000000004038 D _test_AObject_global_class_name
 */
static nboolean existsVarName(char *src,char *varName)
{
	NString *var=n_string_new(src);
	int index=n_string_indexof(var,varName);
    n_string_free(var,TRUE);
    return index>0;
}

		 //一个文件中会有多个类实现，所以先取所该文件所有匹配FUNC_CEHCK_GLOBAL_VAR_NAME_PREFIX的变量
static char *getFullVarName(char *content,char *varNamePrefix)
{
	 //printf("getFullVarName global is :%s\n",content);
	 //取类名
	 char *re=strstr(content,varNamePrefix);
	 if(re){
		char *sysName=re;//+strlen(varNamePrefix)+1;//1代表 _
		int len=strlen(sysName);
		char *last=NULL;
		if(sysName[len-1]=='\n'){
			last=n_strndup(sysName,len-1);
		}else{
			last=n_strdup(sysName);
		}
		return last;
	 }
	 return NULL;

}

/**
 * 从指定的文件找变量
 * 0000000000000040   447 OBJECT  LOCAL  DEFAULT    3 __var_func_track_e$_78_test_AMutex
 * 变量名 __var_func_track_e$_78_test_AMutex 只要找到 varName一部分就算成功。
 * 从文件中获得与varName匹配的全局变量
 * 0到n个
 */
static NPtrArray *getMatchVar(LibFile *self,char *fileName,char *varName)
{
	char cmd[512];
	sprintf(cmd,"readelf -s -W %s | grep %s",fileName,varName);
	FILE *fd = popen(cmd, "r");
	//printf("lib_file_get_global_var_from_file is 00 :%s %p\n",cmd,fd);
	char tempBuff[LINE_SIZE];
	NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
	if(fd){
		while(TRUE){
		  char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
		  if(ret==NULL){
			 break;
		  }
		  if(ret!=NULL && existsVarName(ret,varName)){
			  char *trueVarName=getFullVarName(ret,varName);
			  //printf("getMatchVar 找到了变量 is 11 :%s | %s\n",ret,trueVarName);
			  if(trueVarName)
			    n_ptr_array_add(array,trueVarName);
		  }
		}
		fclose(fd);
	}
	return array;
}


static void addLibPath(NPtrArray *array,char *newPath)
{
	int i;
	nboolean find=FALSE;
	for(i=0;i<array->len;i++){
		char *item=n_ptr_array_index(array,i);
		if(strcmp(item,newPath)==0){
			find=TRUE;
			break;
		}
	}
	if(!find){
		n_ptr_array_add(array,n_strdup(newPath));
	}
}

typedef struct _XmlData{
	char *varName;
	char *data;
	int size;
}XmlData;

static nboolean addLibData(LibFile *self,char *varName,char *value,int size)
{
    int i;
    for(i=0;i<self->resultArray->len;i++){
    	XmlData *item=n_ptr_array_index(self->resultArray,i);
    	if(!strcmp(item->varName,varName))
    		return FALSE;
    }
    XmlData *xml=(XmlData *)n_slice_new(XmlData);
    xml->varName=varName;
    xml->data=value;
    xml->size=size;
    n_ptr_array_add(self->resultArray,xml);
    return TRUE;
}

static void filter(LibFile *self,NList *list)
{
	int len=n_list_length(list);
	int i;
	for(i=0;i<len;i++){
		NFile *file=n_list_nth_data(list,i);
		char *fp=n_file_get_absolute_path(file);
		NString *f=n_string_new(fp);
		if(n_string_ends_with(f,".so")){
            NPtrArray *result= getMatchVar(self,fp,LIB_GLOBAL_VAR_NAME_PREFIX);
            //从文件取出全局变量的值
            int j;
            for(j=0;j<result->len;j++){
               char *varName=n_ptr_array_index(result,j);
               int size=0;
               char *value=getVarValue(varName,fp,&size);
               if(!addLibData(self,varName,value,size)){
            	   n_free(value);
               }
               //生成文本文件
            }
		}
		n_string_free(f,TRUE);
	}
}


/**
 * 如果库中有全局变量的前缀与GLOBAL_CLASS_NAME_PREFIX相同，就保留该文件
 */
static void createGlobalVar(LibFile *self,NPtrArray *libPathArray)
{
	int i;
	int len=libPathArray->len;
	for(i=0;i<len;i++){
		char *path=n_ptr_array_index(libPathArray,i);
		NFile *file=n_file_new(path);
		NList *list=n_file_list_files_to_list(file);
		filter(self,list);
		n_list_free(list);
		list=NULL;
	}
}

static void createGlobalVarBySingleFile(LibFile *self,NPtrArray *libPathArray)
{
	    int i;
		int len=libPathArray->len;
		for(i=0;i<len;i++){
			char *fp=n_ptr_array_index(libPathArray,i);
			NString *f=n_string_new(fp);
			if(n_string_ends_with(f,".so")){
				NPtrArray *result= getMatchVar(self,fp,LIB_GLOBAL_VAR_NAME_PREFIX);
				//从文件取出全局变量的值
				int j;
				for(j=0;j<result->len;j++){
				   char *varName=n_ptr_array_index(result,j);
				   int size=0;
				   char *value=getVarValue(varName,fp,&size);
				   if(!addLibData(self,varName,value,size)){
					   n_free(value);
				   }
				   //生成文本文件
				}
			}
			n_string_free(f,TRUE);
		}
}


static char *getStr(char *src,char *match)
{
	  NString *ss=n_string_new(src);
	  int of=n_string_indexof(ss,match);
	  char *result=NULL;
	  if(of>=0){
		   of=n_string_indexof(ss,"=");
		   if(of>0){
			   NString *r=n_string_substring(ss,of+1);
			   n_string_trim(r);
			   result=n_strdup(r->str);
			   n_string_free(r,TRUE);
		   }
	  }
	  n_string_free(ss,TRUE);
	  return result;
}

/**
 * 分三种情况
 * 1. -L
 * 2. -l
 * 3. --rpath-link
 * 只能在编译#define ADDITIONAL_MIDDLE_AET_FILE        "temp_func_track_45.c"
 * 时调用,取到的全局变量是用zip压缩的char型数据，保存在resultArray中
 * 参数 libFile=SAVE_LIB_PARM_FILE 有所有引用库的名字
 */
void lib_file_import_lib(LibFile *self,char *libFile)
{
	FILE *fp = fopen(libFile, "r");
	char buffer[4096];
	int rev=0;
	if(fp){
		char buffer[4096];
		rev= fread(buffer, sizeof(char), 4096, fp);
		fclose(fp);
		if(rev>0){
			buffer[rev]='\0';
		}
	}
	if(rev<=0)
		return;
	char **items=n_strsplit(buffer,"\n",-1);
	if(items==NULL)
	   return ;
	int len=n_strv_length(items);
	if(len==0)
		return;
	NPtrArray *pathArray=n_ptr_array_new_with_free_func(n_free);
	NPtrArray *fileNameArray=n_ptr_array_new_with_free_func(n_free);
	int i;
	for(i=0;i<len;i++){
	  char *str=items[i];
	  if(strlen(str)>2 && str[0]=='-' && str[1]=='L'){
		  char *str1=str+2;
		  //printf("lib_file_import_lib value ---- is:%d %s\n",i,str1);
		  addLibPath(pathArray,str1);
	  }else if(strstr(str,"--rpath-link") || strstr(str,"--rpath")){
		  char *str1=getStr(str,"--rpath-link");
		  if(str1==NULL)
			  str1=getStr(str,"--rpath");
		  if(str1!=NULL){
			  //printf("lib_file_import_lib value ---222- is:%d %s\n",i,str1);
			  addLibPath(pathArray,str1);
			  n_free(str1);
		  }
	  }else if(strlen(str)>2 && str[0]=='-' && str[1]=='l'){
		  char *str1=str+2;
		  //printf("lib_file_import_lib value ---333- is:%d %s\n",i,str);
		  addLibPath(fileNameArray,str1);
	  }
	}
	createGlobalVar(self,pathArray);
	createGlobalVarBySingleFile(self,fileNameArray);
	n_ptr_array_unref(pathArray);
	n_ptr_array_unref(fileNameArray);
}

/**
 * 把打开的各种库中有关AET的内容定入到AET_REF_LIB_FILE_NAME中
 */
void lib_file_write(LibFile *self,char *fileName)
{
	NString *codes=n_string_new("");
	int i;
	int len=self->resultArray->len;
	for(i=0;i<len;i++){
		XmlData *item=(XmlData *)n_ptr_array_index(self->resultArray,i);
		char *text=middle_file_decode(item->data,item->size);
		n_string_append(codes,text);
		n_string_append(codes,"\n");
	}
	//printf("lib_file_write 有内容吗----- is %s\n",codes->str);
	FILE *fp=fopen(fileName,"w");
	fwrite(codes->str,1,codes->len,fp);
	fclose(fp);
}


LibFile *lib_file_get()
{
	static LibFile *singleton = NULL;
	if (!singleton){
		 singleton =(LibFile *)n_slice_new0(LibFile);
		 libFileInit(singleton);
	}
	return singleton;
}



