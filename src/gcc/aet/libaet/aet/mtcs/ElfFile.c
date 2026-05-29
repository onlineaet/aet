/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */

#include "../lang/AString.h"
#include "ElfFile.h"


typedef struct _SegmentData{
	char *name;
	auint64 virt_addr;
	auint64 offset;
	auint64 size;
}SegmentData;

#define SEGMENT_NAME_DATA ".data"
#define SEGMENT_NAME_BSS ".bss"
#define SEGMENT_NAME_RODATA_1 ".rodata.str1.1"
#define SEGMENT_NAME_RODATA ".rodata"

static char *SEGMENT_NAME[4][2]={{".data","PROGBITS"},{".bss","NOBITS"},{".rodata.str1.1","PROGBITS"},{".rodata","PROGBITS"}};
static int SEGMENT_COUNT=4;


typedef struct _VarInfo{
	char *name;
	aint64 virtAdd;
	aint64 size;
}VarInfo;


//原型 aet编译器中的 mtcstool.h
#define MTCS_ASM_VARNAME_PREFIX "mtcs_asm_code" //写入汇编代码的变量名前缀

#define LINE_SIZE 1024

impl$  ElfFile {

   public$ ElfFile(char *fileName){
      self->fileName=a_strdup(fileName);
      segmentArray=NULL;
   }

   char *getFullVarName(char *content,char *varNamePrefix){
      //printf("getFullVarName global is :%s\n",content);
      //取类名
      char *re=strstr(content,varNamePrefix);
      if(re){
         char *sysName=re;//+strlen(varNamePrefix)+1;//1代表 _
         int len=strlen(sysName);
         char *last=NULL;
         if(sysName[len-1]=='\n'){
            last=a_strndup(sysName,len-1);
         }else{
            last=a_strdup(sysName);
         }
         return last;
      }
      return NULL;
   }

   /**
    * 通过变量前缀找到变量信息
    * varNamePrefix=mtcs_asm_code_
    * ret = 2875: 00000000006dff40  4720 OBJECT  LOCAL  DEFAULT   26 mtcs_asm_code_cuda_3_7_2962277237
    */
   AArray *getMatchVar(char *varNamePrefix){
      char cmd[512];
      sprintf(cmd,"readelf -s -W %s | grep %s",fileName,varNamePrefix);
      FILE *fd = popen(cmd, "r");
      //printf("getMatchVar 00 :%s %p fileName:%s\n",cmd,fd,fileName);
      char tempBuff[LINE_SIZE];
      AArray<char*> *array=new$ AArray<char*>();
      if(fd){
         while(TRUE){
            char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
            if(ret==NULL)
               break;
            if(ret!=NULL && strstr(ret,varNamePrefix)){
               char *trueVarName=getFullVarName(ret,varNamePrefix);
               //printf("getMatchVar--- trueVarName %s\n",trueVarName);
               if(trueVarName){
                  VarInfo *info=getVarInfo(ret,trueVarName);
                  array->add(info);
                  free(trueVarName);
               }
            }
         }
         fclose(fd);
      }
      return array;
   }

   /**
   * 从如下字符串中找出段名 .data
   * [ 2] .data             PROGBITS        0000000000000000 000040 000000 00  WA  0   0  1
   */
   char *getSegmentName(char *str){
      AString *content=new$ AString(str);
      int index=content->indexOf(".");
      if(index<0){
         content->unref();
         a_error("读段数据出错:%s 名字前不是.号\n",str);
         return NULL;
      }
      AString *sub=content->substring(index);
      index=sub->indexOf(" ");
      AString *last=sub->substring(0,index);
      char *result=a_strdup(last->toString());
      content->unref();
      sub->unref();
      last->unref();
      return result;
   }


   char *getPROGBITSorNOBITS(char *segName){
      int i;
      for(i=0;i<SEGMENT_COUNT;i++){
         if(strcmp(SEGMENT_NAME[i][0],segName)==0)
            return SEGMENT_NAME[i][1];
      }
      return NULL;
   }

   /**
   * 取.data段的地址
   * [22] .data             PROGBITS        0000000000004028 003028 000020 00  WA  0   0  8
   * [ 3] .bss              NOBITS          0000000000000000 000040 000000 00  WA  0   0  1
   */
   aint64 getSegmentVirtAdd(char *src,aint64 *dataOffset,aint64 *dataSize,char *prefix){
      if(src==NULL)
         return -1;
      AString *content=new$ AString(src);
      int index=content->indexOf(prefix);
      if(index<0){
         content->unref();
         return -1;
      }
      //printf("getSegmentVirtAdd 00 is:%s\n",src);
      AString *sub=content->substring(index+strlen(prefix));
      sub->trim();
      achar**  items=a_strsplit(sub->toString()," ",-1);
      content->unref();
      sub->unref();
      int len=a_strv_length(items);
      if(len==0)
         return -1;
      // int i;
      //for(i=0;i<len;i++)
      //printf("getSegmentVirtAdd info: is:%d %s\n",i,items[i]);
      aint64   add=a_ascii_strtoll(items[0],NULL,16);
      aint64   v1=a_ascii_strtoll(items[1],NULL,16);
      aint64   v2=a_ascii_strtoll(items[2],NULL,16);
      *dataOffset=v1;
      *dataSize=v2;
      return add;
   }

   SegmentData *createSegmentData(char *str){
      char *segName=getSegmentName(str);
      char *prefix=getPROGBITSorNOBITS(segName);
      if(prefix==NULL)
         return NULL;
      aint64 offset=0;
      aint64 size=0;
      aint64 virtAdd=getSegmentVirtAdd(str,&offset,&size,prefix);
      if(virtAdd<0){
         a_error("读段数据出错:%s\n",str);
      }
      SegmentData *data=(SegmentData *)a_slice_new0(SegmentData);
      data->virt_addr=virtAdd;
      data->offset=offset;
      data->size=size;
      data->name=segName;
      printf("段信息:%s virt:%llu offset:%llu size:%llu\n",data->name,data->virt_addr,data->offset,data->size);
      return data;
   }

   AArray *createSegmentFromFile(){
      //char *cmd="readelf -S /home/sns/workspace/testblock/bin/libapptest.so | grep -P \"(bss|data)\"";
      char cmd[512];
      sprintf(cmd,"readelf -S -W %s | grep -P \"(bss|data)\"",fileName);
      FILE *fd = popen(cmd, "r");
      //printf("findOffsetDataRoDataBss is 00 :%s %p\n",cmd,fd);
      char tempBuff[LINE_SIZE];
      AArray<SegmentData *> *array=new$ AArray<SegmentData *>();
      if(fd){
         while(TRUE){
            char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将FILE* stream的数据流读取到buf中
            if(ret==NULL)
               break;
            if(ret!=NULL){
               //printf("readDataSegmentVirtAdd 找到了变量 is 11 :%s\n",ret);
               SegmentData *data=createSegmentData(ret);
               if(data!=NULL)
                  array->add(data);
            }
         }
         fclose(fd);
      }
      return array;
   }

   /**
   * 从如下字符串中取出变量名 cyxx
   * 8: 0000000000000000     8 OBJECT  LOCAL  DEFAULT    6 cyxx
   * src来命令
   * readelf -s -W fileName | grep cyxx
   */
   char *getVarName(char *src){
      achar**  items=a_strsplit(src," ",-1);
      int length=a_strv_length(items);
      if(length<=0)
         return NULL;
      char *var=items[length-1];
      char *result=NULL;
      if(var && strlen(var)>0){
         int len=strlen(var);
         if(var[len-1]=='\n')
            result=a_strndup(var,len-1);
         else
            result=a_strdup(var);
      }
      a_strfreev(items);
      return result;
   }


   VarInfo *createVarInfoFromFile(char *varName){
      char cmd[512];
      sprintf(cmd,"readelf -s -W %s | grep %s",fileName,varName);
      FILE *fd = popen(cmd, "r");
      //printf("createVarInfoFromFile is 00 :%s %p\n",cmd,fd);
      char tempBuff[LINE_SIZE];
      if(fd){
         while(TRUE){
            char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
            if(ret==NULL)
               break;
            if(ret!=NULL){
               char *result=getVarName(ret);
               if(result!=NULL && !strcmp(result,varName)){
                  printf("createVarInfoFromFile 找到了变量 is 11 :%s var:%s varName:%s\n",ret,result,varName);
                  VarInfo *info=getVarInfo(ret,varName);
                  a_free(result);
                  return info;
               }
            }
         }
         fclose(fd);
      }
      return NULL;
   }

   /**
   * 获取变量在data段的虚拟地址
   * src格式如下
   * 10: 0000000000004038     8 OBJECT  GLOBAL DEFAULT   22 _test_AObject_global_generic_name_abcd_1234_672$2_suiwtiwuer_ABdxxyyyyy
   */
   VarInfo *getVarInfo(char *src,char *varName){
      if(src==NULL)
         return NULL;
      AString *content=new$ AString(src);
      int index=content->indexOf(":");
      if(index<0){
         content->unref();
         return NULL;
      }
      //printf("elfile.c  getVarInfo 00 is:%s varName:%s\n",src,varName);
      AString *sub=content->substring(index+1);
      sub->trim();
      achar** items=a_strsplit(sub->toString()," ",-1);
      content->unref();
      sub->unref();
      int len=a_strv_length(items);
      if(len==0)
         return NULL;
      int i;
      int size=0;
      for(i=1;i<len;i++){
         //printf("elfile.c  getVarInfo 11 i:%d %s\n",i,items[i]);
         if(strlen(items[i])>0){
            size=atoi(items[i]);
            if(size==0){
               char *end=NULL;
               long int num = strtol(items[i], &end, 16); // 转换并获取转换后的数值
               if (*end == '\0') { // 确保转换成功且没有多余字符
                  ;
               } else {
                  a_error("获取MTCS代码失败，代码大小字符串中有非法字符。\n",items[i]);
               }
               size=num;
            }
            //printf("elfile.c  getVarInfo  22 i:%d %s size:%d\n",i,items[i],size);
            break;
         }
      }
      //printf("elfile.c  getVarInfo 33 %s size:%d\n",items[0],size);
      aint64  add=a_ascii_strtoll(items[0],NULL,16);
      VarInfo *data=(VarInfo *)a_slice_new0(VarInfo);
      data->virtAdd=add;
      data->size=size;
      data->name=a_strdup(varName);
      //printf("elfile.c getVarInfo 44 size :%d %s\n",size,data->name);
      return data;
   }


   aboolean varInSegment(SegmentData *dataSeg,VarInfo *varInfo){
      if(dataSeg==NULL)
         return FALSE;
      //    int_ary.virt_addr = 0x08049550
      //    segment.data.virt_addr.begin = 0x0804954c
      //    segmeng.data.virt_addr.end   = 0x0804954c + 0x034 = 0x08049580
      if(dataSeg->size==0)
         return FALSE;
      aint64 begin=dataSeg->virt_addr;
      aint64 end=dataSeg->virt_addr+dataSeg->size;
      aint64 varAdd=varInfo->virtAdd+varInfo->size;
      if(varAdd>=begin && varAdd<=end){
         //printf("说明变量:%s在%s段.11111",varInfo->name,dataSeg->name);
         return TRUE;
      }
      return FALSE;
   }

   SegmentData *matchSegment(AArray *segments,VarInfo *varInfo){
      int i;
      for(i=0;i<segments->size();i++){
         SegmentData *dataSeg=segments->get(i);
         aboolean in=varInSegment(dataSeg,varInfo);
         if(in)
            return dataSeg;
      }
      return NULL;
   }

   void appendData(char *src,char *buffer,int *offset){
      achar**  items=a_strsplit(src," ",-1);
      int len=a_strv_length(items);
      int i;
      int a=0;
      int count=*offset;
      for(i=1;i<len;i++){
         if(strlen(items[i])>0 && (items[i])[0]=='|')
            break;
         if(strlen(items[i])>0){
            sscanf(items[i], "%x", &a);
            buffer[count++]=a;
         }
      }
      *offset=count;
      a_strfreev(items);
   }

   char *readOffset(char *fileName,aint64 offset,int size,int *dataSize){
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
            //printf("readContent ret is :%s\n",ret);
            if(ret==NULL)
               break;

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
      char *data=(char *)a_malloc(dataLen);
      memcpy(data,content,dataLen);
      *dataSize=dataLen;
      return data;
   }

   /**
   * 1. find the offset of .data .rodata .bss segment in executable-file
   * 2. find the virt-addr of global variable in executable-file
   * 8: 0000000000000000     8 OBJECT  LOCAL  DEFAULT    6 cyxx
   */
   char *getVarValue(VarInfo *varInfo,int *dataSize){
      //第一步
      if(segmentArray==NULL)
        segmentArray=createSegmentFromFile();
      //第二步
      SegmentData *dataSeg=matchSegment(segmentArray,varInfo);
      if(!dataSeg){
         a_warning("在四个段都没找到变量的值：%s\n",varInfo->name);
         //guessPointerVar(fileName,segmentArray,varInfo);
         return NULL;
      }
      //    int_ary.virt_addr = 0x08049550
      //    segment.data.virt_addr.begin = 0x0804954c
      //    segmeng.data.virt_addr.end   = 0x0804954c + 0x034 = 0x08049580
      //第三步
      aint64 begin=dataSeg->virt_addr;
      aint64 varOffset=0;
      //printf("说明变量:%s在%s段.\n",varInfo->name,dataSeg->name);
      aint64 segmentOffset = dataSeg->offset;
      varOffset= segmentOffset+ (varInfo->virtAdd - begin);
      char *rec=readOffset(fileName,varOffset,varInfo->size,dataSize);
      return rec;
   }

   //变量名的形式:mtcs_asm_code_"plat"_"isa"_"version_%u"
   //来自编译器 mtcstool.c mtcs_tool_create_asm_varname
   //char *ret=n_strdup_printf("%s_%s_%d_%d",MTCS_ASM_VARNAME_PREFIX,platform,isa_version,hashcode);
   //mtcs_asm_code_cuda_6_5_23428
   public$ AArray *getCode(char *platform){
      AArray *result= getMatchVar(MTCS_ASM_VARNAME_PREFIX);
      AArray<char *> *asmCodes=new$ AArray<char *>();
      //从文件取出全局变量的值
      int i;
      char *ret=NULL;
      for(i=0;i<result->size();i++){
         VarInfo *info=result->get(i);
         char *varName=info->name;
         char *name=varName+strlen(MTCS_ASM_VARNAME_PREFIX)+1;//'1个‘_’字符
         if(!strncmp(name,platform,strlen(platform))){
            int size=0;
            ret=getVarValue(info,&size);
            asmCodes->add(ret);
         }
      }
      result->unref();
      return asmCodes;
   }
};





