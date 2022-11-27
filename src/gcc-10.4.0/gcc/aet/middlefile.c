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
#include "zlib.h"

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
#include "middlefile.h"
#include "classutil.h"
#include "ifacefile.h"
#include "classlib.h"
#include "libfile.h"

static void middleFileInit(MiddleFile *self)
{
   char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
   char fileName[255];
   sprintf(fileName,"%s/%s",objectPath,ADDITIONAL_MIDDLE_AET_FILE);
   self->linkFile=link_file_new(fileName);
}

/**
 * 写入标记到temp_func_track_45.c
 */
void middle_file_modify(MiddleFile *self)
{
      NString *content=n_string_new("");
      n_string_append_printf(content,"%s %d\n",RID_AET_GOTO_STR,GOTO_CHECK_FUNC_DEFINE);
      n_debug("写入aet_goto_compile$ 13 到文件temp_func_track_45.c中:file:%s data:%s",self->linkFile->fileName,content->str);
      link_file_lock_write(self->linkFile,content->str,content->len);
      n_string_free(content,TRUE);
}

static nuint64  fileLength(char *fileName)
{
    struct stat64 sb;
    nuint64 rv=0;
    if (stat64(fileName, &sb) == 0)
    {
        rv = sb.st_size;
    }
    return rv;
}

static char *readFile(char *fileName)
{
    FILE *fp=fopen(fileName,"r");
    if(!fp)
        return NULL;
    nuint64 size=fileLength(fileName);
    if(size==0){
        fclose(fp);
        return NULL;
    }
    char *data=(char *)n_malloc(size+1);
    int rev = fread(data, sizeof(char), size, fp);
    if(rev!=size){
        n_error("文件的大小是:%llu,读出的大小是:%d 文件:%s",size,rev,fileName);
        return NULL;
    }
    data[rev]='\0';
    return data;
}

/**
 * 三个数据要统一否则不能解压。
 * compressBuffer 装压缩后的数据
 * len =compressBuffer数据长度
 * uncompressBuffer解压后的数据
 * uncompressLen uncompressBuffer的长度
 * 以上四个都是一样的数据 calaLen
 */
static char *compressData(char *data,int *returnLen)
{
    int dataLen=strlen(data);
    int calaLen=dataLen+10;
    char *compressBuffer =(char *)n_malloc(calaLen);
    nulong compressLen=calaLen;
    int result=compress((unsigned char*)compressBuffer, &compressLen, (unsigned char*)data, dataLen + 1);
    if (Z_OK == result){
        nulong uncompressLen =calaLen;
        char *uncompressBuffer =(char *)n_malloc(calaLen);
        result= uncompress((unsigned char*)uncompressBuffer, &uncompressLen, (unsigned char*)compressBuffer, compressLen);
        if (Z_OK == result)
        {
            if(strcmp(data,uncompressBuffer)){
                n_error("解压缩后的数据与原数据不相等。%s\n",uncompressBuffer);
            }
        }else{
            n_error("解压缩数据时出错。error:%d\n%s\n",result,data);
        }
        n_free(uncompressBuffer);
    }else{
        n_error("压缩数据时出错。error:%d\n%s\n",result,data);
    }
    int total=sizeof(int)+sizeof(int)+compressLen;
    char *newData=(char *)n_malloc(total);
    int cl=(int)compressLen;
    memcpy(newData,&calaLen,sizeof(int));
    memcpy(newData+sizeof(int),&cl,sizeof(int));
    memcpy(newData+2*sizeof(int),compressBuffer,compressLen);
    n_free(compressBuffer);
    *returnLen=total;
    return newData;
}


static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

static void createGlobalGenericVar(char *varName,char *data,size_t length)
{
    location_t  loc = input_location;
    tree id=aet_utils_create_ident(varName);
    tree decl, type, init;
    type = build_array_type (char_type_node,build_index_type (size_int (length)));
    type = c_build_qualified_type (type, TYPE_QUAL_CONST);
    decl = build_decl (loc, VAR_DECL, id, type);
    DECL_EXTERNAL(decl)=0;
    TREE_PUBLIC(decl)=1;
    TREE_STATIC(decl)=1;
    decl=class_util_define_var_decl(decl,TRUE);
    init = build_string (length + 1, data);
    TREE_TYPE (init) = type;

    location_t init_loc=loc;
    warn_string_init (init_loc, TREE_TYPE (decl), init,STRING_CST);
    finish_decl (decl, init_loc, init,type, NULL_TREE);
}

/**
 * 创建全局变量 内容是方法检查，泛型，接口
 * 当前所在的.c文件是temp_func_track_45.c
 */
void middle_file_create_global_var(MiddleFile *self)
{
    //LIB_GLOBAL_VAR_NAME_PREFIX
    char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
    char fileName[255];
    sprintf(fileName,"%s/%s",objectPath,GENERIC_BLOCK_INTO_FILE_NAME);
    char *generic=readFile(fileName);
    sprintf(fileName,"%s/%s",objectPath,CLASS_FUNC_CHECK_FILE);
    char *funcCheck=readFile(fileName);
    char *ifaceImpl=iface_file_get_xml_string(iface_file_get());
    NString *codes=n_string_new("");
    n_debug("middle_file_create_global_var 00 %s\n",fileName);
    if(generic){
        n_debug("middle_file_create_global_var 11。");
       n_string_append(codes,generic);
       n_string_append(codes,"\n");
       n_free(generic);
    }
    if(funcCheck){
        n_debug("middle_file_create_global_var 22。");
       n_string_append(codes,funcCheck);
       n_string_append(codes,"\n");
       n_free(funcCheck);
    }
    if(ifaceImpl && strlen(ifaceImpl)<0){
        n_debug("middle_file_create_global_var 33。");
       n_string_append(codes,ifaceImpl);
       n_string_append(codes,"\n");
       n_free(ifaceImpl);
    }
    if(codes->len<=0)
        return;
    n_debug("middle_file_create_global_var 44。");
    int newDataLen=0;
    char *newData=compressData(codes->str,&newDataLen);
    char varName[255];
    nint number=class_util_get_random_number();
    sprintf(varName,"%s%d",LIB_GLOBAL_VAR_NAME_PREFIX,number>0?number:number*-1);
    createGlobalGenericVar(varName,newData,newDataLen);
}


static int file_exists (const char *name)
{
  return access (name, R_OK) == 0;
}

static unsigned long long  getLastModified(char *file)
{
    struct stat64 sb;
    unsigned long long rv=0;
    if (stat64(file, &sb) == 0)
    {
        rv = 1000 * sb.st_mtime;
    }
    return rv;
}

/**
 * 在aet_compile.c中把编译参数写入到SAVE_LIB_PARM_FILE(aet_collect2_argv.tmp)中
 * 如果没有AET_REF_LIB_FILE_NAME或时间不符，说明参数发生了改变。需要重新载入库。
 * SAVE_LIB_PARM_FILE相当于.c文件，AET_REF_LIB_FILE_NAME是.o文件。
 */
static int compareLibFileBySrcAndDest(char *objectRootPath)
{
    char src[255];
    sprintf(src,"%s/%s",objectRootPath,SAVE_LIB_PARM_FILE);
    char dest[255];
    sprintf(dest,"%s/%s",objectRootPath,AET_REF_LIB_FILE_NAME);
    int fsrc=file_exists(src);
    int fdest=file_exists(dest);
    int action=0;//0 .删除目标 1.生成目标，2 不做任何事
    if(!fsrc && fdest){
        action=0;
    }else if(fsrc && !fdest){
        action=1;
    }else if(fsrc && fdest){
        //都存在,比较时间
        unsigned long long st= getLastModified(src);
        unsigned long long dt= getLastModified(dest);
        if(st>dt){
            action=1;
        }else{
            //不编译 加.o到ld中
            action=2;
        }
    }else{
        //都不存在,不编译，不加.o到ld中
        action=2;
    }
    return action;
}

/**
 * 把库倒入到文件AET_REF_LIB_FILE_NAME中。
 * 当进行类中函数，全局变量检查时调用这个方法，
 * 在classparser.c ->class_parser_goto-> GOTO_CHECK_FUNC_DEFINE
 */
void  middle_file_import_lib(MiddleFile *self)
{
   char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
   int action=compareLibFileBySrcAndDest(objectPath);
   if(action==0){
       char fileName[255];
       sprintf(fileName,"%s/%s",objectPath,AET_REF_LIB_FILE_NAME);
       remove((const char *)fileName);
   }else if(action==1){
      char fileName[255];
      sprintf(fileName,"%s/%s",objectPath,SAVE_LIB_PARM_FILE);
      lib_file_import_lib(lib_file_get(),fileName);
      sprintf(fileName,"%s/%s",objectPath,AET_REF_LIB_FILE_NAME);
      lib_file_write(lib_file_get(),fileName);
   }else{
       printf("不需要生成库。。因为库是最新的。%s\n",AET_REF_LIB_FILE_NAME);
   }
}



MiddleFile *middle_file_get()
{
    static MiddleFile *singleton = NULL;
    if (!singleton){
         singleton =n_slice_alloc0 (sizeof(MiddleFile));
         middleFileInit(singleton);
    }
    return singleton;
}

/**
 * 解压数据
 */
char *middle_file_decode(char *value,int size)
{
    int calcLen=0;
    int compressLen=0;
    memcpy(&calcLen,value,sizeof(int));
    memcpy(&compressLen,value+sizeof(int),sizeof(int));
    char *compressBuffer=value+2*sizeof(int);
    nulong uncompressLen =calcLen;
    char *uncompressBuffer =(char *)n_malloc(calcLen);
    int result= uncompress(uncompressBuffer, &uncompressLen, compressBuffer, compressLen);
    if (Z_OK == result)
    {
        uncompressBuffer[uncompressLen]='\0';
    }else{
        n_error("解压缩数据时出错。error:%d\n",result);
    }
    return uncompressBuffer;
}





