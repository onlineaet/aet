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
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "c-family/c-pragma.h"
#include "opts.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "c-aet.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "classutil.h"
#include "classparser.h"
#include "makefileparm.h"
#include "ifaceimpl.h"
#include "aetlib.h"
#include "accesscontrols.h"
#include "middlefile.h"


static void ifaceImplInit(IfaceImpl *self)
{
    self->saveIfaceFileName=NULL;
}

static int file_exists (const char *name)
{
  return access (name, R_OK) == 0;
}


#define SPERATOR "$#@"

/**
 * 正在编译的.c文件有接口实现（class_mgr_get_all_iface_info）
 * 保存在同位置的.ifaceimpl_new.o文件中，并把文件名存在saveIfaceFileName
 * 最终会写入.note.aet_prog
 */
void iface_impl_save(IfaceImpl *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("ifaceimpl.c 是第二次编译 %s 不需要写入任何接口信息。\n",in_fnames[0]);
      return;
   }
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   char newName[255];
   sprintf(newName,"%s.ifaceimpl_new.o",objfile);
   NPtrArray *array=class_mgr_get_all_iface_info(class_mgr_get());
   if(array==NULL){
      if(file_exists(newName)){
         remove(newName);//删除文件 xxx.ifacecheck.o
      }
      return;
   }
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<array->len;i++){
      ClassInfo *info=n_ptr_array_index(array,i);
      char *file=class_info_get_file(info);
      //接口声明在.h文件中，可以加入
      if(endswith(file,".h")){
         ClassName *className=&info->className;
         n_string_append_printf(codes,"%s%s%s%s%s%s%s\n",
               className->sysName,SPERATOR,className->package,SPERATOR,file,SPERATOR,objfile);
      }
   }
   n_ptr_array_unref(array);
   if(codes->len==0){
      n_string_free(codes,TRUE);
      if(file_exists(newName)){
         remove(newName);//删除文件 xxx.ifacecheck.o
      }
      return;
   }
   FILE *fp=fopen(newName,"w");
   fwrite(codes->str,1,codes->len,fp);
   fclose(fp);
   n_string_free(codes,TRUE);
   gcc_assert(self->saveIfaceFileName==NULL);
   self->saveIfaceFileName=n_strdup(newName);
   middle_file_modify(middle_file_get(),COMPILE_IFACE);
}

typedef struct _IfaceData{
    char *sysName;
    char *package;
    char *file;     //接口所在的文件 只处理在.h的接口
    char *quoteFile;//引用头文件的.o文件
    char *ifaces[100];
    int count;
}IfaceData;

/**
 * 引用接口的源文件存入在str条目中。
 */
static IfaceData *createIfaceData(char *str)
{
    nchar** items=n_strsplit(str,SPERATOR,-1);
    int len=n_strv_length(items);
    if(len!=4){
        return NULL;
    }
    IfaceData *data=(IfaceData *)n_slice_new0(IfaceData);
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
 * 生成头文件对应的.c文件源代码。
 */
static int writeSourceCodeToFile(IfaceData *ifaceData,char *destFile)
{
   NString *codes=n_string_new("\n");
   n_string_append_printf(codes,"#include \"%s\"\n",ifaceData->file);
   n_string_append(codes,"\n");
   n_string_append_printf(codes,"%s %d ",RID_AET_GOTO_STR,GOTO_IFACE_COMPILE);
   int i;
   n_string_append(codes,"\"");
   for(i=0;i<ifaceData->count;i++){
      n_string_append(codes,ifaceData->ifaces[i]);
      if(i<ifaceData->count-1)
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

static unsigned long long  getLastModified(char *file)
{
   struct stat64 sb;
   unsigned long long rv=0;
   if (stat64(file, &sb) == 0){
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


/**
 * buffer内容是:
 * n_string_append_printf(codes,"%s%s%s%s%s%s%s\n",
              className->sysName,SPERATOR,className->package,SPERATOR,file,SPERATOR,in_fnames[0]);
 */

/**
 * 在NPtrArray是否已存在GenericObj obj
 */
static nboolean exists(NPtrArray *newArray,IfaceData *item)
{
   int i;
   for(i=0;i<newArray->len;i++){
      IfaceData *next=n_ptr_array_index(newArray,i);
      if(next==item)
         return TRUE;
      if(strcmp(item->sysName,next->sysName)==0)
         return TRUE;
   }
   return FALSE;
}

/**
 * 移走有相同接口名的IfaceData;
 */
static void removeRepeat(NPtrArray *ifaceDataArray)
{
   int old=ifaceDataArray->len;
   if(old<=1)
      return;
   NPtrArray *newArray=n_ptr_array_new();
   int i,j;
   for(i=0;i<ifaceDataArray->len;i++){
      IfaceData *item=n_ptr_array_index(ifaceDataArray,i);
      if(!exists(newArray,item)){
         n_ptr_array_add(newArray,item);
      }
   }
   n_ptr_array_remove_range(ifaceDataArray,0,ifaceDataArray->len);
   for(i=0;i<newArray->len;i++)
      n_ptr_array_add(ifaceDataArray,n_ptr_array_index(newArray,i));
   n_ptr_array_unref(newArray);
}

static IfaceData *existsMerge(NPtrArray *newArray,IfaceData *item)
{
    int i;
    for(i=0;i<newArray->len;i++){
       IfaceData *next=n_ptr_array_index(newArray,i);
       if(next==item)
          return next;
       if(strcmp(item->file,next->file)==0)
          return next;
    }
    return NULL;
}

/**
 *头文件相同的接口合并到一个IfaceData中。
 */
static void merge( NPtrArray *ifaceDataArray)
{
   NPtrArray *newArray=n_ptr_array_new();
   int i,j;
   for(i=0;i<ifaceDataArray->len;i++){
      IfaceData *item=n_ptr_array_index(ifaceDataArray,i);
      IfaceData *exists=existsMerge(newArray,item);
      if(!exists){
         item->ifaces[item->count++]=item->sysName;
         n_ptr_array_add(newArray,item);
      }else{
         if(item!=exists){
            exists->ifaces[exists->count++]=item->sysName;
         }
      }
   }
   n_ptr_array_remove_range(ifaceDataArray,0,ifaceDataArray->len);
   for(i=0;i<newArray->len;i++)
      n_ptr_array_add(ifaceDataArray,n_ptr_array_index(newArray,i));
   n_ptr_array_unref(newArray);
}


/**
 * 移走库已实现的接口
 * Remove the implemented interfaces of the library
 * 如果一个.h中有多个接口声明，从库中移走一个，应该也能移走.h中的其它接口，否则报错。
 */
static void removeIfaceFromLib(NPtrArray *ifaceDataArray)
{
   int i,j;
   for(i=0;i<ifaceDataArray->len;i++){
      IfaceData *item=n_ptr_array_index(ifaceDataArray,i);
      int notequal=0,equal=0;
      for(j=0;j<item->count;j++){
         char *iface=item->ifaces[j];
         nboolean re=aet_lib_have_iface/*!class_lib_have_interface_static_define*/(aet_lib_get(),iface);
         if(re)
            equal++;
         else
            notequal++;
      }
      if((equal!=0 && equal!=item->count) || (notequal!=0 && notequal!=item->count)){
         n_error("库中实现文件:%s中的接口并不完整。",item->file);
      }
      if(equal!=0 && equal==item->count){
           printf("接口%s在库中已实现，不需要文件:%s再实现了。item->count:%d\n",item->sysName,item->file,item->count);
           n_ptr_array_remove_index(ifaceDataArray,i);
           i--;
      }
   }
}

/**
 * 移走缺失c或o文件
 */
static void removeLackOFileOrCFile(char *parentPath)
{
   NFile *file=n_file_new(parentPath);
   NList *list=n_file_list_files_to_list(file);
   int len=n_list_length(list);
   int i;
   char oFileSuffix[256];
   sprintf(oFileSuffix,"%s.o",IFACE_FILE_SUFFIX);
   char cFileSuffix[256];
   sprintf(oFileSuffix,"%s.c",IFACE_FILE_SUFFIX);
   for(i=0;i<len;i++){
      NFile *item=n_list_nth_data(list,i);
      char *fileName=n_file_get_absolute_path(item);
      //printf("removeLackOFileOrCFile fileNmae is %s %s\n",fileName,oFileSuffix);
      if(endswith(fileName,oFileSuffix)){
         char *cFile=n_strdup(fileName);
         cFile[strlen(cFile)-1]='c';
         if(!file_exists(cFile)){
            remove(fileName);//移走对应的.c不存在的.o文件。
         }
      }else if(endswith(fileName,cFileSuffix)){
         char *oFile=n_strdup(fileName);
         oFile[strlen(oFile)-1]='o';
         if(!file_exists(oFile)){
           remove(fileName);//移走对应的.c不存在的.o文件。
         }
      }
   }
}


/**
 * 创建.c源代码。返回生成的文件列表。
 */
static char *createCFileSource(NPtrArray *ifaceDataArray,char *parentPath)
{
   int i,j;
   NString *codes=n_string_new("");
   for(i=0;i<ifaceDataArray->len;i++){
      IfaceData *item=n_ptr_array_index(ifaceDataArray,i);
      //根据头文件名，生成hashcode
      unsigned int hashcode=aet_utils_create_hash(item->file,strlen(item->file));
      NFile *f=n_file_new(item->file);
      char *fn=n_file_get_name(f);
      char rawName[256];
      strncpy(rawName,fn,strlen(fn)-2);//减2 去除.h
      rawName[strlen(fn)-2]='\0';
      n_file_unref(f);
      char cFile[256];
      char oFile[256];
      sprintf(cFile,"%s/_%s_%u_%s.c",parentPath,rawName,hashcode,IFACE_FILE_SUFFIX);
      sprintf(oFile,"%s/_%s_%u_%s.o",parentPath,rawName,hashcode,IFACE_FILE_SUFFIX);
      char *headerFile=item->file;
      int action=compareFile(headerFile,oFile);
      n_debug("ifaceimpl.c 是否要编译接口文件:接口所在头文件:%s action:%d\n",headerFile,action);
      if(action==0){//headerFile不存在，删除.o文件
         remove(oFile);
      }else if(action==1){
         //头文件改变了 头文件比o文件新,重新编译。
         remove(oFile);
         writeSourceCodeToFile(item,cFile);//生成.c文件
         n_string_append(codes,cFile);
         n_string_append(codes,SPERATOR);
         n_string_append(codes,item->quoteFile);//关键 表示从.o文件来的接口，从.o可能取到编译参数
         n_string_append(codes,"\n");
      }else if(action==2){
         //头文件没有改变。.o文件也还存在
      }else if(action==3){
         //头文件不存在,o文件也没有。
      }
   }
   return n_string_free(codes,FALSE);
}

/**
 * buffer内容是:
 * n_string_append_printf(codes,"%s%s%s%s%s%s%s\n",
              className->sysName,SPERATOR,className->package,SPERATOR,file,SPERATOR,in_fnames[0]);
 */
static NPtrArray *readLocalFile(IfaceImpl *self,char *fileName)
{
   NPtrArray *array=n_ptr_array_new();
   FILE *fp=fopen(fileName,"r");
   char fileListBuffer[1024*50];
   int rev = fread(fileListBuffer,1,1024*50,fp);
   fclose(fp);
   if(rev<=0)
      return array;
   fileListBuffer[rev]='\0';
   nchar** fileItems=n_strsplit(fileListBuffer,"\n",-1);
   int len=n_strv_length(fileItems);
   int i;
   for(i=0;i<len;i++){
      if(!fileItems[i] || strlen(fileItems[i])==0)
         continue;
     // printf("readLocalFile----- %s %s\n",fileName,fileItems[i]);
      fp=fopen(fileItems[i],"r");
      char buffer[1024*50];
      int rev = fread(buffer,1,1024*50,fp);
      fclose(fp);
      buffer[rev]='\0';
      //printf("readLocalFile----- %s %s buffer:%s\n",fileName,fileItems[i],buffer);
      nchar** contents=n_strsplit(buffer,"\n",-1);
      int contentLen=n_strv_length(contents);
      int j;
      for(j=0;j<contentLen;j++){
         if(!contents[j] || strlen(contents[j])==0)
                  continue;
         IfaceData *data=createIfaceData(contents[j]);
         if(data==NULL){
            n_error("接口数据是错的,make clean后，重新编译。%s %s\n",buffer,fileItems[i]);
            return NULL;
         }
         n_ptr_array_add(array,data);
      }
      n_strfreev(contents);
   }
   n_strfreev(fileItems);
   return array;
}

/**
 * 处于正在编译temp_func_track_45.c中
 */
void iface_impl_compile_ready(IfaceImpl *self)
{
   char *fileName = getenv("GCC_AET_IFACE_IMPL_LIST_PATH");
   char compileFiles[256];
   sprintf(compileFiles,"%s.o",fileName);
   if(fileName==NULL ||strlen(fileName)==0){
      remove(compileFiles);
      return;
   }
   //取GCC_AET_IFACE_IMPL_LIST_PATH的路径
   NFile *f=n_file_new(fileName);
   NFile *parent=n_file_get_parent_file(f);
   NFile  *canonical=n_file_get_canonical_file(parent);
   const  char *parentPath = n_file_get_absolute_path(canonical);

   //读取文件 fileName 的内容并生成IfaceData数据
   NPtrArray *ifaceDataArray=readLocalFile(self,fileName);
   //删除接口名相同的IfaceData
   removeRepeat(ifaceDataArray);
   //合并接口到各自的.h文件中
   merge(ifaceDataArray);
   //如果外部库已有接口实现，移走当前接口实现文件
   removeIfaceFromLib(ifaceDataArray);
   //移走缺失的o或c文件
   removeLackOFileOrCFile(parentPath);
   char *fileList=createCFileSource(ifaceDataArray,parentPath);
   if(!fileList || strlen(fileList)==0){
      remove(compileFiles);
      return;
   }
   FILE *fp=fopen(compileFiles,"w");
   int ret=fwrite(fileList,1,strlen(fileList),fp);
   fclose(fp);
   n_debug("ifaceimpl.c iface_impl_compile_ready_new --%s\n接口文件列表:\n%s\n",fileName,fileList);
}


////////////////////////////////////////定义接口变量 生成两个函数的源代码------------------------------------
static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

static void createGlobalVar(char *varName,char *data,size_t length)
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
 *创建一个全局变量，保存这些接口名。
 *每个接口文件一个这样的变量
 *char *LIB_GLOBAL_IFACE_VAR_NAME_PREFIX_hash_random="iface start: ... iface end:"
 */
void iface_impl_compile(IfaceImpl *self,char *tokenString)
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
   if(lenv==0){
      n_error("编译接口实现文件出错，没有接口名。%s\n",in_fnames[0]);
      return;
   }
   ClassParser *classParser=class_parser_get();
   NString *codes=n_string_new("");
   int i;
   for(i=0;i<lenv;i++){
      char *sysName=sysNames[i];
      //我要实现这个接口
      ClassName *ifaceName=class_mgr_clone_class_name(class_mgr_get(),sysName);
      char *staticVar=var_mgr_define_class_static_var(var_mgr_get(),ifaceName);
      if(staticVar){
         n_string_append(codes,staticVar);
         n_free(staticVar);
      }
      char *createAClassFunc=class_build_create_codes(classParser->classBuild,ifaceName);
      if(createAClassFunc){
         n_string_append(codes,createAClassFunc);
         n_free(createAClassFunc);
      }
      class_init_create_init_define_for_interface(classParser->classInit,sysName,codes);
      access_controls_add_impl(access_controls_get(),sysName);//关键否则AClass的构造函数禁止访问。
      class_name_free(ifaceName);
   }
   n_string_append(codes,"\n");
   NString *names=n_string_new("");
   n_string_append(names,IFACE_START"\n");
   for(i=0;i<lenv;i++){
     n_string_append_printf(names,"%s\n",sysNames[i]);
   }
   n_string_append(names,IFACE_END"\n");
   unsigned int hashcode=aet_utils_create_hash(names->str,names->len);
   char globalIfaceVarName[256];
   nint number=class_util_get_random_number();
   unsigned int random=0;
   if(number<0)
      random=number*-1;
   else
      random=number;
   sprintf(globalIfaceVarName,"%s_%u_%u",LIB_GLOBAL_IFACE_VAR_NAME_PREFIX,hashcode,random);
   createGlobalVar(globalIfaceVarName,names->str,names->len);
   if(codes->len>0){
      n_debug("ifaceimpl.c 接口实现文件:%s 源代码:\n%s\n",in_fnames[0],codes->str);
      aet_utils_add_token(parse_in,codes->str,codes->len);
   }
   n_string_free(codes,TRUE);
}

/**
 * 接口声明在.c文件中，所以在该文件实现接口的静态变量和初始化函数
 * 不需要加入全局变量 LIB_GLOBAL_IFACE_VAR_NAME_PREFIX
 */
void iface_impl_compile_at_cfile(IfaceImpl *self,ClassInfo *classInfo)
{
   if(class_info_is_interface(classInfo)){
      char *file=class_info_get_file(classInfo);
      if(endswith(file,".c")){
           n_debug("ifaceimpl.c iface_impl_compile_at_cfile 在\n");
           NString *codes=n_string_new("");
           ClassParser *classParser=class_parser_get();
           //我要实现这个接口
           ClassName *ifaceName=&classInfo->className;
           char *staticVar=var_mgr_define_class_static_var(var_mgr_get(),ifaceName);
           if(staticVar){
              n_string_append(codes,staticVar);
              n_free(staticVar);
           }
           char *createAClassFunc=class_build_create_codes(classParser->classBuild,ifaceName);
           if(createAClassFunc){
              n_string_append(codes,createAClassFunc);
              n_free(createAClassFunc);
           }
           class_init_create_init_define_for_interface(classParser->classInit,ifaceName->sysName,codes);
           access_controls_add_impl(access_controls_get(),ifaceName->sysName);//关键否则AClass的构造函数禁止访问。
           aet_utils_add_token(parse_in,codes->str,codes->len);
           n_string_free(codes,TRUE);
      }
   }
}


IfaceImpl *iface_impl_get()
{
   static IfaceImpl *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(IfaceImpl));
      ifaceImplInit(singleton);
   }
   return singleton;
}

