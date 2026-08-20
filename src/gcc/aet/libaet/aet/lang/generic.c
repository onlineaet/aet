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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "generic.h"


/******************以下实现generic.h中的声明方法***************************/
typedef struct _BlockFuncInfo
{
   BlockFuncData *data;
   int count;
}BlockFuncInfo;

__thread AetGenericFuncInfo _gen_func_block_addr_128347;//名字来自 aetmicro.h

static BlockFuncInfo blockFuncDataArray[200];
static int blockArray=0;
void add_generic_data(BlockFuncData *data,int count)
{
   blockFuncDataArray[blockArray].data=data;
   blockFuncDataArray[blockArray].count=count;
//   int i;
//   for(i=0;i<count;i++){
//         printf("add_generic_data---- %s %d addr:%p\n",data[i].funcName,data[i].index,data[i].address);
//   }
   blockArray++;

}

static void fillBlockAddress(char *sysName,char *generic,void **address)
{
   int i,j;
   for(i=0;i<blockArray;i++){
      BlockFuncInfo info=blockFuncDataArray[i];
      //printf("fill data sysName:%s generic:%s\n",info.data[0].sysName,generic);

         for(j=0;j<info.count;j++){
            if(!strcmp(info.data[j].sysName,sysName)
                  && info.data[j].genericModel && !strcmp(info.data[j].genericModel,generic)){
               address[info.data[j].index]=info.data[j].address;
               //printf("找到注册的块地址  j:%d index:%d sysName:%s generic:%s addr:%p\n",j,info.data[j].index,
                  //sysName,generic,info.data[j].address);
            }
         }

   }
  // printf("fillBlockAddress  %s %s\n",sysName,generic);
}

static inline void getModelDefine (aet_generic_info *info,int len,char *ret)
{
   int i;
   char buffer[256];
   int offset =0;
   for(i=0;i<len;i++){
      sprintf(buffer,"_%s_%d",info[i].typeName,info[i].pointerCount);
      memcpy(ret+offset,buffer,strlen(buffer));
      offset+=strlen(buffer);
   }
   ret[offset]='\0';
}

void aet_generic_class_fill_address(aet_generic_info *info,int len,char *sysName,void **address)
{
   char ret[1024];
   getModelDefine (info,len,ret);
   // printf("填充泛型类的块地址变量 _gen_blocks_array_897  sysName:%s generic:%s\n",sysName,ret);
   fillBlockAddress(sysName,ret,address);
}

/**
 * 获取泛型函数中的块函数地址。
 */
void *aet_generic_func_get_address(int index,aet_generic_info *classGenInfo,int classGenInfoCount)
{
   aet_generic_info *info=_gen_func_block_addr_128347._generic_1234_array;
   int len=_gen_func_block_addr_128347.unitCount;
   char *sysName=_gen_func_block_addr_128347.sysName;
   int i,j;
   char genericModel[1024];
   char buffer[256];
   int offset =0;
   for(i=0;i<len;i++){
      sprintf(buffer,"_%s_%d",info[i].typeName,info[i].pointerCount);
      memcpy(genericModel+offset,buffer,strlen(buffer));
      offset+=strlen(buffer);
   }

   for(i=0;i<classGenInfoCount;i++){
        sprintf(buffer,"_%s_%d",classGenInfo[i].typeName,classGenInfo[i].pointerCount);
        memcpy(genericModel+offset,buffer,strlen(buffer));
        offset+=strlen(buffer);
   }
   genericModel[offset]='\0';
  // printf("get_block_func_address 泛型单元  %s sysName:%s len:%d classGenInfoCount:%d\n",
       //  genericModel,sysName,len,classGenInfoCount);

   for(i=0;i<blockArray;i++){
      BlockFuncData *df=blockFuncDataArray[i].data;
      int dfc=blockFuncDataArray[i].count;
      for(j=0;j<dfc;j++){
         BlockFuncData *item=&df[j];
         if(!strcmp(item->sysName,sysName) && !strcmp(item->genericModel,genericModel) && item->index==index){
             return item->address;
         }
      }
   }

   printf("出错，找不到泛型函数中的块函数地址:%s %s index:%d\n",genericModel,sysName,index);
   return NULL;
}

static BlockFuncInfo funcWithGbArray[100];
static int funcWithGbCount=0;
void add_func_with_gb_data(BlockFuncData *data,int count)
{
   funcWithGbArray[funcWithGbCount].data=data;
   funcWithGbArray[funcWithGbCount].count=count;
//   int i;
//   for(i=0;i<count;i++){
//       printf("add_func_with_gb_data:index:%d sysName:%s func:%s mode:%s addr:%p\n",
//             data[i].index,data[i].sysName,data[i].funcName,data[i].genericModel,data[i].address);
//   }
   funcWithGbCount++;
}

static inline bool endswith (const char *str, const char *suffix)
{
   size_t str_len = strlen (str);
   size_t suffix_len = strlen (suffix);
   if (str_len < suffix_len)
      return false;
   return memcmp (str + str_len - suffix_len, suffix, suffix_len) == 0;
}

void *aet_geneirc_get_func_with_gb_address(aet_generic_info *info,int len,char *sysName,char *mangleName)
{
   char ret[1024];
   getModelDefine(info,len,ret);
  // printf("获取fwgb函数的真实方法 sysName:%s generic:%s 被覆盖的方法:%s funcWithGbCount:%d\n",
        // sysName,ret,mangleName,funcWithGbCount);
   int i,j;
   for(i=0;i<funcWithGbCount;i++){
      BlockFuncInfo info=funcWithGbArray[i];
      for(j=0;j<info.count;j++){
         if(!strcmp(sysName,info.data[j].sysName)
               && !strcmp(ret,info.data[j].genericModel)
               && endswith(info.data[j].funcName,mangleName)){
           // printf("用泛型真实类型覆盖原类方法 11 j:%d sysName:%s generic:%s 被覆盖的方法:%s add:%p\n",
                //  j,sysName,ret,mangleName,info.data[j].address);
           return info.data[j].address;
         }
      }
   }
   return NULL;
}


/*****************结束泛型***************************/

