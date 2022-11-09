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
#include "c-family/c-pragma.h"
#include "opts.h"
#include "c/c-tree.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "c-aet.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "classparser.h"
#include "makefileparm.h"
#include "ifacefile.h"
#include "classlib.h"
#include "accesscontrols.h"
#include "xmlfile.h"



static void ifaceFileInit(IfaceFile *self)
{
    self->linkFile=NULL;
    self->parmFile=NULL;
}

static void initFile(IfaceFile *self)
{
   if(self->linkFile==NULL){
       char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
       char fileName[512];
       sprintf(fileName,"%s/%s",objectPath,IFACE_IMPL_INDEX_FILE);
       self->linkFile=link_file_new(fileName);
   }
   if(self->parmFile==NULL){
         char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
         char fileName[512];
         sprintf(fileName,"%s/%s",objectPath,IFACE_IMPL_PARM_FILE);
         self->parmFile=link_file_new(fileName);
   }
}

/**
 * aet_compile.c中编译头文件需要参数。
 * 编译的文件，在aet_compile.c中再替换成接口的c文件和目标文件。
 */
static void saveParm(IfaceFile *self)
{
     char *aetEnv=getenv ("GCC_AET_PARM");
     if(aetEnv!=NULL){
         link_file_lock_write(self->parmFile,aetEnv,strlen(aetEnv));
     }else{
         n_error("不是aet-gcc编译器,因为没有GCC_AET_PARM");
     }
}

#define SPERATOR "$#@"
/**
 * 该方法是各个正在编译的文件调用。
 */
void iface_file_save(IfaceFile *self)
{
    if(makefile_parm_is_second_compile(makefile_parm_get())){
           printf("ifacefile.c 是第二次编译 %s 不需要写入任何接口信息。\n",in_fnames[0]);
           return;
    }
    initFile(self);
    NPtrArray *array=class_mgr_get_all_iface_info(class_mgr_get());
    if(array==NULL)
        return;
    int i;
    NString *codes=n_string_new("");
    for(i=0;i<array->len;i++){
        ClassInfo *info=n_ptr_array_index(array,i);
        char *file=class_info_get_file(info);
        ClassName *className=&info->className;
        n_string_append_printf(codes,"%s%s%s%s%s%s%s\n",className->sysName,SPERATOR,className->package,SPERATOR,file,SPERATOR,in_fnames[0]);
    }
    n_ptr_array_unref(array);
    link_file_lock(self->linkFile);
    if(codes->len>0){
        char buffer[1024*150];
        int rev=  link_file_lock_read(self->linkFile,buffer,1024*150);
        if(rev>0){
            buffer[rev]='\0';
            n_string_append(codes,buffer);
        }
        link_file_lock_write(self->linkFile,codes->str,codes->len);
    }
    link_file_unlock(self->linkFile);
    n_string_free(codes,TRUE);
    saveParm(self);
}

typedef struct _IfaceData{
    char *sysName;
    char *package;
    char *file;
    char *quoteFile;//引用头文件的文件
}IfaceData;

typedef struct _IfaceHeader{
    char *headerFile;
    NPtrArray *sysNameArray;
}IfaceHeader;

/**
 * 引用接口的源文件存入在str条目中。
 */
static IfaceData *createRefIfaceData(char *str)
{
    nchar** items=n_strsplit(str,SPERATOR,-1);
    int len=n_strv_length(items);
    if(len!=4){
        n_error("接口数据是错的,make clean后，重新编译。%s\n",str);
    }
    IfaceData *data=n_slice_new(IfaceData);
    data->sysName=n_strdup(items[0]);
    data->package=n_strdup(items[1]);
    //转成canonical文件
    NFile *nfile=n_file_new(items[2]);
    NFile *canonicalFile=n_file_get_canonical_file(nfile);
    char *canonicalPath=n_file_get_absolute_path(canonicalFile);
    data->file=n_strdup(canonicalPath);
    n_file_unref(nfile);
    n_file_unref(canonicalFile);
    nfile=n_file_new(items[3]);
    canonicalFile=n_file_get_canonical_file(nfile);
    canonicalPath=n_file_get_absolute_path(canonicalFile);
    data->quoteFile=n_strdup(canonicalPath);
    n_file_unref(nfile);
    n_file_unref(canonicalFile);
    n_strfreev(items);
    return data;
}


/**
 * 读取iface_impl_index.tmp文件
 */
static NPtrArray *readFile(IfaceFile *self)
{
    initFile(self);
    NPtrArray *array=n_ptr_array_new();
    char buffer[1024*150];
    int rev=  link_file_lock_read(self->linkFile,buffer,1024*150);
    if(rev<=0)
        return array;
    buffer[rev]='\0';
    nchar** items=n_strsplit(buffer,"\n",-1);
    int len=n_strv_length(items);
    int i;
    for(i=0;i<len;i++){
      if(strlen(items[i])==0)
          continue;
      IfaceData *data=createRefIfaceData(items[i]);
      n_ptr_array_add(array,data);
    }
    n_strfreev(items);
    return array;
}


/**
 * 移走库已实现的接口
 * Remove the implemented interfaces of the library
 */
static void removeImpleIfaceOfLib(NPtrArray *ifaceDataArray)
{
    int i;
    for(i=0;i<ifaceDataArray->len;i++){
        IfaceData *item=n_ptr_array_index(ifaceDataArray,i);
        nboolean re=class_lib_have_interface_static_define(class_lib_get(),item->sysName);
        if(re){
            printf("接口%s在库中已实现，不需要文件:%s再实现了。\n",item->sysName,item->file);
            n_ptr_array_remove_index(ifaceDataArray,i);
            i--;
        }
    }
}

static int writeSourceCodeToFile(char *headerFile,char *destFile,NPtrArray *sysNameArray)
{
      NString *codes=n_string_new("");
      n_string_append_printf(codes,"#include \"%s\"\n",headerFile);
      n_string_append_printf(codes,"%s %d ",RID_AET_GOTO_STR,GOTO_IFACE_COMPILE);
      int i;
      n_string_append(codes,"\"");
      for(i=0;i<sysNameArray->len;i++){
          char *ifacSysName=n_ptr_array_index(sysNameArray,i);
          n_string_append(codes,ifacSysName);
          if(i<sysNameArray->len-1)
              n_string_append(codes,",");
      }
      n_string_append(codes,"\"");
      n_string_append(codes,"\n");
      FILE *fp = fopen(destFile, "w");
      if(!fp){
         if(errno==17){
           fp = fopen(destFile, "r+x");
         }
         if(!fp){
           n_error("打不开文件----:%s error:%d %s\n",destFile,errno,xstrerror(errno));
           return errno;
         }
      }
      int rev=fwrite(codes->str,1,strlen(codes->str),fp);
      fclose(fp);
      return rev;
}

/**
 * 根据接口所在的文件获取.c或.o文件
 */
static char *getIfaceOFileOrCFile(char *ifaceHeadFile,nboolean isCFile)
{
     char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
     NFile *f=n_file_new(ifaceHeadFile);
     char *fileName=n_file_get_name(f);
     char *newFile=n_strndup(fileName,strlen(fileName)-2);//裸文件名无路径无后缀
     char *ifaceFile=n_strdup_printf("%s/_%s%s",objectPath,newFile,isCFile?IFACE_SOURCE_FILE_SUFFIX:IFACE_OBJECT_FILE_SUFFIX);
     n_file_unref(f);
     n_free(newFile);
     return ifaceFile;
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

static int compareFile(char *headerFile,char *ofile)
{
    int fsrc=file_exists(headerFile);
    int fdest=file_exists(ofile);
    int action=0;//0 .删除目标 1.生成目标，2 不做任何事
    if(!fsrc && fdest){
        action=0;
    }else if(fsrc && !fdest){
        action=1;
    }else if(fsrc && fdest){
        //都存在,比较时间
        unsigned long long st= getLastModified(headerFile);
        unsigned long long dt= getLastModified(ofile);
        if(st>dt){
            action=1;
        }else{
            //不编译 加.o到ld中
            action=2;
        }
    }else{
        //都不存在,不编译，不加.o到ld中
        action=3;
    }
    return action;
}

static void addFile(NPtrArray *fileArray,char *headerFile,char *sysName)
{
       int i;
       for(i=0;i<fileArray->len;i++){
           IfaceHeader *item=n_ptr_array_index(fileArray,i);
           if(!strcmp(item->headerFile,headerFile)){
                int j;
                int find=FALSE;
                for(j=0;j<item->sysNameArray->len;j++){
                    char *ifacSysName=n_ptr_array_index(item->sysNameArray,j);
                    if(!strcmp(ifacSysName,sysName)){
                        find=TRUE;
                        break;
                    }
                }
                if(!find){
                    n_ptr_array_add(item->sysNameArray, n_strdup(sysName));
                }
                return;
           }
       }
       IfaceHeader *iface=n_slice_new(IfaceHeader);
       iface->headerFile=n_strdup(headerFile);
       iface->sysNameArray=n_ptr_array_new();
       n_ptr_array_add(iface->sysNameArray, n_strdup(sysName));
       n_ptr_array_add(fileArray, iface);
}

/**
 * 删除在objectRootPath目录下的所有接口实现.c文件
 */
static void deleteIfaceImplFile()
{
    char *objectRootPath=makefile_parm_get_object_root_path(makefile_parm_get());
    NFile *file=n_file_new(objectRootPath);
    NList *list=n_file_list_files_to_list(file);
    int len=n_list_length(list);
    int i;
    for(i=0;i<len;i++){
        NFile *item=n_list_nth_data(list,i);
        char *cFile=n_file_get_absolute_path(item);
        NString *f=n_string_new(cFile);
        if(n_string_ends_with(f,IFACE_SOURCE_FILE_SUFFIX))
             remove(cFile);
    }
}

/**
 * 准备要编译的头文件。调这里说明进入链接阶段了。但还未第二次编译泛型文件。
 * 选出接口.o文件.
 */
void iface_file_compile_ready(IfaceFile *self)
{
    /*从文件中取出引用的接口文件。内容在编译各个.c是同步写入iface_impl_index.tmp*/
    //删除所有的接口实现.c文件
    deleteIfaceImplFile();
    NPtrArray *ifaceDataArray=readFile(self);//
    printf("iface_file_compile_ready 00 ifaceDataArray:%p\n",ifaceDataArray);
    removeImpleIfaceOfLib(ifaceDataArray);
    int i;
    NPtrArray *fileArray=n_ptr_array_new();//去除重复的头文件
    for(i=0;i<ifaceDataArray->len;i++){
          IfaceData *item=n_ptr_array_index(ifaceDataArray,i);
          NString *fileStr=n_string_new(item->file);
          if(n_string_ends_with(fileStr,".c")){ //过滤.c文件
              n_string_free(fileStr,TRUE);
              continue;
          }
          n_string_free(fileStr,TRUE);
          addFile(fileArray,item->file,item->sysName);
    }
    printf("iface_file_compile_ready 11 fileArray:%d\n",fileArray->len);
    //创建源代码
    NString *fileCodes=n_string_new("");
    for(i=0;i<fileArray->len;i++){
       IfaceHeader *ifaceFile=n_ptr_array_index(fileArray,i);
       char *headerFile=ifaceFile->headerFile;
       char *oFile=getIfaceOFileOrCFile(headerFile,FALSE);
       int action=compareFile(headerFile,oFile);
       printf("是否要编译头文件:%s action:%d\n",headerFile,action);
       if(action==0){//headerFile不存在，删除.o文件
           remove(oFile);
       }else if(action==1){
           //头文件改变了,重新编译。
           remove(oFile);
           char *cFile=getIfaceOFileOrCFile(headerFile,TRUE);
           writeSourceCodeToFile(headerFile,cFile,ifaceFile->sysNameArray);//生成.c文件
           n_string_append(fileCodes,cFile);
           n_string_append(fileCodes,"\n");
       }else if(action==2){
           //头文件没有改变。.o文件也还存在
       }else if(action==3){
           //头文件不存在,o文件也没有。
       }
    }
    printf("iface_file_compile_ready 22 %s %d\n",fileCodes->str,fileCodes->len);
    link_file_lock_write(self->linkFile,fileCodes->str,fileCodes->len);
}


////////////////////////////////////////定义接口变量 生成两个函数的源代码------------------------------------

static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

/**
 * 定义全局变量，声明extern 是在引入头文件时就做了
 * gotoStaticAndInit-->parser_static_compile
 */
static void defineGlobalVar(VarEntity *item)
{
      tree decl;
      tree org=item->decl;
      tree init=item->init;
      tree initOrginalTypes=item->init_original_type;
      tree name=DECL_NAME(org);
      tree type=TREE_TYPE(org);
      if(item->isConst){
          n_debug("ifacefile.c defineGlobalVar is const :%s\n",item->orgiName);
          type = c_build_qualified_type (type, TYPE_QUAL_CONST);
      }
      decl = build_decl (DECL_SOURCE_LOCATION (org), VAR_DECL, name, type);
      DECL_EXTERNAL(decl)=0;
      TREE_PUBLIC(decl)=1;
      TREE_STATIC(decl)=1;
      decl=class_util_define_var_decl(decl,TRUE);
      n_debug("ifacefile.c defineGlobalVar is -----11  :%s %p\n",item->orgiName,init);
      location_t init_loc=DECL_SOURCE_LOCATION (org);
      warn_string_init (init_loc, TREE_TYPE (decl), init,item->original_code);
      n_debug("ifacefile.c defineGlobalVar is -----22  :%s %p\n",item->orgiName,init);
      if(!init)
          finish_decl (decl, init_loc, NULL_TREE,NULL_TREE, NULL_TREE);
      else
          finish_decl (decl, init_loc, init,initOrginalTypes, NULL_TREE);
      n_debug("类中静态变量全局定义: %s %s context:%p extern:%d static:%d pub:%d\n",
              item->mangleVarName,item->orgiName,DECL_CONTEXT(decl),DECL_EXTERNAL(decl),TREE_STATIC(decl),TREE_PUBLIC(decl));
}



static nboolean havedDefineStaticVar(VarEntity *item,char *sysName)
{
    char *varName=item->mangleVarName;
    tree id=aet_utils_create_ident(varName);
    tree decl=lookup_name(id);
    if(aet_utils_valid_tree(decl)){
       n_debug("在接口%s，找到静态变量:%s orgiName:%s\n",sysName,varName,item->orgiName);
       n_debug("在接口%s 静态变量:%s orgiNam:%s context:%p extern:%d static:%d pub:%d\n",
               sysName,varName,item->orgiName,DECL_CONTEXT(decl),DECL_EXTERNAL(decl),TREE_STATIC(decl),TREE_PUBLIC(decl));
       if(DECL_EXTERNAL(decl)==1 && TREE_STATIC(decl)==0 && TREE_PUBLIC(decl)==1){
           return FALSE;
       }
       return TRUE;
    }else{
        n_error("在接口%s，没有找到静态变量:%s orgiName:%s\n",sysName,varName,item->orgiName);
    }
    return FALSE;
}

/**
 * 真正定义接口在的静态变量在当前.c文件
 */
static void defineIfaceStatic(NPtrArray *array,char *sysName)
{
    int i;
    for(i=0;i<array->len;i++){
        VarEntity *entity=n_ptr_array_index(array,i);
        if(entity->isStatic){
            nboolean have=havedDefineStaticVar(entity,sysName);
            if(!have){
                defineGlobalVar(entity);
            }
        }
    }
}

static void defineStaticVar(char *sysName)
{
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    NPtrArray *array=var_mgr_get_vars(var_mgr_get(),sysName);
    n_debug("定义接口中的静态变量为全局变量并初始化 %s 当前文件:%s\n",sysName,in_fnames[0]);
    defineIfaceStatic(array,sysName);
}

/**
 * tokenString有双引号 如:"iface1,iface2"
 * 生成源代码
 * static  AClass *_createAClass_debug_RandomGenerator_123(debug_RandomGenerator *self)
 *{
 * ......
 *}
 *void * debug_AyInface_init_1234ergR5678_debug_AyInface(debug_AyInface *self)
 *{
 *   return (void *)_createAClass_debug_AyInface_123(self);
 *}
 *
 *
 */
void iface_file_compile(IfaceFile *self,char *tokenString)
{
    NString *rex=n_string_new(tokenString);
    n_string_replace(rex,"\"","",-1);
    char *ifaces=NULL;
    if(n_string_ends_with(rex,"\n")){
       NString *re= n_string_substring_from(rex,0,rex->len-1);
       ifaces=re->str;
    }else{
        ifaces=rex->str;
    }
    nchar **sysNames=n_strsplit(ifaces,",",-1);
    int lenv=n_strv_length(sysNames);
    ClassParser *classParser=class_parser_get();
    NString *codes=n_string_new("");
    int i;
    for(i=0;i<lenv;i++){
            char *sysName=sysNames[i];
          //我要实现这个接口
            defineStaticVar(sysName);
            ClassName *ifaceName=class_mgr_clone_class_name(class_mgr_get(),sysName);
            class_build_create_codes(classParser->classBuild,ifaceName,codes);
            class_init_create_init_define_for_interface(classParser->classInit,sysName,codes);
            access_controls_add_impl(access_controls_get(),sysName);//关键否则AClass的构造函数禁止访问。
            class_name_free(ifaceName);
    }
    if(codes->len>0){
        printf("IfaceFile.c 实现接口的初始化方法 ---实现的文件:%s\n源代码:\n%s\n",in_fnames[0],codes->str);
        aet_utils_add_token(parse_in,codes->str,codes->len);
    }
}

/////////////////////------------------------获取存入temp_func_track_45.c中的接口信息的xml字符串============。

/**
 * 从 .o文件中找到接口的类名(sysName)
 * void * debug_RandomGenerator_init_1234ergR5678_debug_RandomGenerator(debug_RandomGenerator *self)
 *
 */
static NPtrArray *getSysNameFromOFile(char *fileName)
{
    char cmd[512];
    sprintf(cmd,"readelf -s -W %s",fileName);
    FILE *fd = popen(cmd, "r");
    if(!fd)
        return NULL;
    //printf("lib_file_get_global_var_from_file is 00 :%s %p\n",cmd,fd);
    char tempBuff[1024];
    NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
    while(TRUE){
       char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
       if(ret==NULL){
           break;
        }
        if(ret!=NULL && strstr(ret,AET_INIT_GLOBAL_METHOD_STRING)){
            NString *str=n_string_new(ret);
            int index=n_string_indexof(str,AET_INIT_GLOBAL_METHOD_STRING);
            NString *sub=n_string_substring(str,index+strlen(AET_INIT_GLOBAL_METHOD_STRING));
            n_string_free(str,TRUE);
            NString *sub1=n_string_substring(sub,1);//跳动下划线_
            char *sysName=NULL;
            if(n_string_ends_with(sub1,"\n")){
                sysName=n_strndup(sub1->str,sub1->len-1);
            }else{
                sysName=n_strdup(sub1->str);
            }
            NString *sub2=n_string_new(sysName);
            if(!n_string_ends_with(sub2,"AClass")){//取接口名时限制一下AObject AClass两个字
                n_ptr_array_add(array,sysName);
                printf("读.oc文件 找到了接口 :%s\n",sysName);
            }else{
                n_free(sysName);
            }
            n_string_free(sub,TRUE);
            n_string_free(sub1,FALSE);
            n_string_free(sub2,FALSE);

        }
    }
    fclose(fd);
    return array;
}

static char *convertCFile(char *oFile)
{
    char *re=strstr(oFile,".o");
    int remainLen=strlen(oFile)-strlen(re);
    char remain[remainLen+1];
    memcpy(remain,oFile,remainLen);
    remain[remainLen]='\0';
    return n_strdup_printf("%s.c",remain);
}

static char *convertOFile(char *cFile)
{
    char *re=strstr(cFile,".c");
    int remainLen=strlen(cFile)-strlen(re);
    char remain[remainLen+1];
    memcpy(remain,cFile,remainLen);
    remain[remainLen]='\0';
    return n_strdup_printf("%s.o",remain);
}

/**
 * 获取本次实现的所有接口
 * 最终生成全局变量存入当前正在编译的文件temp_func_track_45.c中。
 */
static NPtrArray *getImplIfaceFromOFile()
{
    NPtrArray *ifaceSaveArray=n_ptr_array_new();
    char *objectRootPath=makefile_parm_get_object_root_path(makefile_parm_get());
    NFile *file=n_file_new(objectRootPath);
    NList *list=n_file_list_files_to_list(file);
    int len=n_list_length(list);
    int i;
    for(i=0;i<len;i++){
        NFile *item=n_list_nth_data(list,i);
        char *ofile=n_file_get_absolute_path(item);
        NString *f=n_string_new(ofile);
        if(n_string_ends_with(f,IFACE_OBJECT_FILE_SUFFIX)){
             n_debug("这是一个接口的.o文件:%s 通过.o找出实现的接口。\n",ofile);
             NPtrArray *array=getSysNameFromOFile(ofile);
             if(array==NULL || array->len==0){
                 n_error("文件:%s中没有找到实现的接口。",ofile);
             }
             int j;
             for(j=0;j<array->len;j++){
                 char  *sysName=n_ptr_array_index(array,j);
                 IFaceSaveData *saveData=n_slice_new(IFaceSaveData);
                 saveData->sysName=n_strdup(sysName);
                 saveData->oFile=n_strdup(ofile);
                 saveData->cFile=convertCFile(ofile);
                 n_ptr_array_add(ifaceSaveArray,saveData);
             }
             n_ptr_array_unref(array);
        }
    }
    return ifaceSaveArray;
}


static void createIFaceSaveData(char *cFile,NPtrArray *ifaceSaveDataArray)
{
       FILE *fp = fopen(cFile, "r");
       int rev=0;
       char buffer[1024];
       if(!fp){
          int errnum=errno;
          n_error("打开文件%s出错。错误:%s",cFile,xstrerror (errnum));
          return;
       }
       rev = fread(buffer, sizeof(char), 1024, fp);
       fclose(fp);
       if(rev>0){
           buffer[rev]='\0';
       }
       //取出字符串"Iface1,Iface2";
       NString *str=n_string_new(buffer);
       int index=n_string_indexof(str,RID_AET_GOTO_STR);
       if(index<0){
           n_error("文件%s中的内容由编译器生成，不能修改",cFile);
       }
       NString *ret=n_string_substring(str,index);
       n_string_free(str,TRUE);
       index=n_string_indexof(ret,"\"");
       int index2=n_string_indexof_from(ret,"\"",index+1);
       if(index<0 || index2<0){
           n_error("文件%s中的内容由编译器生成，不能修改",cFile);
       }
       NString *last=n_string_substring_from(ret,index+1,index2);
       n_string_free(ret,TRUE);
       nchar** items=n_strsplit(last->str,",",-1);//参见writeSourceCodeToFile用的是逗号分隔,iface_file_impl_file也用到逗号分隔信息
       int len=n_strv_length(items);
       int i;
       for(i=0;i<len;i++){
            if(items[i]==NULL || strlen(items[i])==0)
                continue;
            IFaceSaveData *saveData=n_slice_new(IFaceSaveData);
            saveData->sysName=n_strdup(items[i]);
            saveData->oFile=convertOFile(cFile);
            saveData->cFile=n_strdup(cFile);
            n_ptr_array_add(ifaceSaveDataArray,saveData);
       }
       n_string_free(last,TRUE);

}

/**
 * iface_impl_index.tmp取出要实现接口的.c文件
 */
static NPtrArray *getImplIfaceFromIndexFile(IfaceFile *self)
{
        initFile(self);
        NPtrArray *array=n_ptr_array_new();
        char buffer[1024*10];
        int rev=  link_file_lock_read(self->linkFile,buffer,1024*10);
        if(rev<=0)
            return array;
        buffer[rev]='\0';
        nchar** items=n_strsplit(buffer,"\n",-1);
        int len=n_strv_length(items);
        int i;
        for(i=0;i<len;i++){
           if(strlen(items[i])==0)
               continue;
           createIFaceSaveData(items[i],array);
        }
        n_strfreev(items);
        return array;
}

static void addLastArray(NPtrArray *array1,NPtrArray *lastArray)
{
       int i;
       for(i=0;i<array1->len;i++){
           IFaceSaveData *item=n_ptr_array_index(array1,i);
           int j;
           nboolean find=FALSE;
           for(j=0;j<lastArray->len;j++){
               IFaceSaveData *dest=n_ptr_array_index(lastArray,j);
               if(!strcmp(dest->sysName,item->sysName)){
                   find=TRUE;
                   break;
               }
           }
           if(!find){
               n_ptr_array_add(lastArray,item);
           }
       }
}

/**
 * 从当前.o文件和iface_impl_index.tmp生成要实现的接口的xml文件。
 * 从objectrootpath找到所有的.o文件和这次要实现的接口
 * 保存为IFaceSaveData.以便middle_file_create_global_var存入
 * temp_func_track_45.c 调用该方法时，正处理编译temp_func_track_45.c状态中。
 */
char *iface_file_get_xml_string(IfaceFile *self)
{
    NPtrArray *array1=getImplIfaceFromOFile();
    NPtrArray *array2=getImplIfaceFromIndexFile(self);
    //合并并移走重复的
    NPtrArray *lastArray=n_ptr_array_new();
    addLastArray(array1,lastArray);
    addLastArray(array2,lastArray);
    if(lastArray->len==0)
        return NULL;
    //生成xml文件
    char *ret=xml_file_create_iface_xml_string(lastArray);
    return ret;
}

/////////////////////------------------------结束,获取存入temp_func_track_45.c中的接口信息的xml字符串============


IfaceFile *iface_file_get()
{
    static IfaceFile *singleton = NULL;
    if (!singleton){
         singleton =n_slice_alloc0 (sizeof(IfaceFile));
         ifaceFileInit(singleton);
    }
    return singleton;
}



