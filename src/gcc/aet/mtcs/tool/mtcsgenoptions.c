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
 * AET was originally developed  by the onlineaet@163.com
 * base on genoptions.cc
 */

#include "bconfig.h"
#define INCLUDE_ALGORITHM
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "read-md.h"
#include "gensupport.h"
#include "mtcsgen.h"
#include "../../nlib.h"


//生成主机和MTCS设备的选项交集头文件mtcsoptionsitem.h
//输入 build-nvptx-gcc/gcc/options.h  build-host-gcc/gcc/options.h 两个文件

/**
 * 从options.h文件中的
 * struct GTY(()) gcc_options
#else
struct gcc_options
#endif
{
#endif
#ifdef GENERATOR_FILE
extern bool dump_base_name_prefixed;
#else
  bool x_dump_base_name_prefixed;
#define dump_base_name_prefixed global_options.x_dump_base_name_prefixed
....
转成
{
bool x_dump_base_name_prefixed;
bool x_exit_after_options;
bool x_flag_dump_all_passed;
 */
static char *removeStructGccOptionsItem(char *srcFileName)
{
     char *deleteRow[6]={"#ifdef GENERATOR_FILE","extern","#else","#define","#endif","#ifndef"};
     FILE *fp=fopen(srcFileName,"r");
     char buffer[4096*100];
     int len=fread(buffer,1,4096*100,fp);
     fclose(fp);
     buffer[len+1]='\0';
     //找出 struct gcc_options
     char *gccopt=strstr(buffer,"struct gcc_options");
     if(!gccopt){
         fprintf(stderr,"文件 :%s不是有效的options.h\n",srcFileName);
         exit(0) ;
     }
     gccopt=gccopt+strlen("struct gcc_options");//去除字符串struct gcc_options
     //第一个"};"是 struct gcc_options的结束标志
     char** items= n_strsplit(gccopt,"};",2);
     char *options=items[0];
     //fprintf(stderr,"fist ---- %s\n",options);
     NString *str=n_string_new("");
     char** ret=n_strsplit(options,"\n",-1);
     int re=n_strv_length(ret);
     int i,j;
     for(i=0;i<re;i++){
         ret[i]=n_strstrip(ret[i]);
         if(!strcmp(ret[i],"{"))
             continue;
         int find=0;
         for(j=0;j<6;j++){
            if(!strncmp(ret[i],deleteRow[j],strlen(deleteRow[j]))){
                find=1;
                break;
            }
         }
         if(!find){
             n_string_append(str,ret[i]);
             n_string_append(str,"\n");
         }
     }

     char *result=n_string_free(str,FALSE);
     return result;
}
/**
 * 用h文件名创建cc文件名。
 * 例如:../../gcc-153/gcc/aet/mtcs/ptx/ptx_options.h 生成 ../../gcc-153/gcc/aet/mtcs/ptx/ptx_options.cc
 */
static void createCCFile(char *hfile,char *ccFile)
{
   int slen = strlen(hfile);
   memcpy(ccFile,hfile,slen);
   ccFile[slen-1]='c';
   ccFile[slen]='c';
   ccFile[slen+1]='\0';
}

static int getFileSize(char *fileName)
{
   struct stat64 sb;
   nuint64 rv=0;
   if (stat64(fileName, &sb) == 0){
    rv = sb.st_size;
   }
   return (int)rv;
}

/**
 * 从 {
 *  {
bool x_dump_base_name_prefixed;
bool x_exit_after_options;
bool x_flag_dump_all_passed;
....
转成
   x_dump_base_name_prefixed
   x_exit_after_options
   x_flag_dump_all_passed
 */
static char *getFieldRawName(char *buffer/*!char *srcFileName*/)
{
    /*
        FILE *fp=fopen(srcFileName,"r");
        char buffer[4096*100];
        int len=fread(buffer,1,4096*100,fp);
        fclose(fp);
        buffer[len+1]='\0';
        */
         char** ret=n_strsplit(buffer,"\n",-1);
         int re=n_strv_length(ret);
         int i;
         NString *str=n_string_new("");
         for(i=0;i<re;i++){
               char** arrays=n_strsplit(ret[i]," ",-1);
               int alen=n_strv_length(arrays);
               if(arrays==NULL || alen==0)
                   continue;
               char *last=arrays[alen-1];
               last[strlen(last)-1]='\0';//去除分号;
               if(last[0]=='\n') //只是换行符，不加入
                   continue;
               if(last[0]=='*')
                   last=last+1; //去除*号
               n_string_append(str,last);
               n_string_append(str,"\n");
         }

         //printf("获得gcc_options 的域名字:%s\n",str->str);
         char *result=n_string_free(str,FALSE);
         return result;
}

//求交集
static char *intersection(char *strA,char *strB)
{
    char** a=n_strsplit(strA,"\n",-1);
    char** b=n_strsplit(strB,"\n",-1);
    int size1=n_strv_length(a);
    int size2=n_strv_length(b);
    NString *str=n_string_new("");
    int i,j;
    for(i=0;i<size1;i++){
        char *item=a[i];
        for(j=0;j<size2;j++){
           if(strcmp(item,b[j])==0){
               n_string_append(str,item);
               n_string_append(str,"\n");
               break;
           }
        }
    }
    //printf("交集是----- %s\n",str->str);
    char *result=n_string_free(str,FALSE);
    return result;
}


//补全交集的数据类型
//inter 的内容没有类型,用options.h中的struct gcc_options来补全
//inter多个struct gcc_options的交集的域名字
//options 任意一个平台的struct gcc_options的域
//char *optA=removeStructGccOptionsItem(fileA);
static char *completionInterseactionFieldType(char *inter,char *options)
{
    char** a=n_strsplit(inter,"\n",-1);
    char** b=n_strsplit(options,"\n",-1);
    int size1=n_strv_length(a);//域条目的名称
    int size2=n_strv_length(b);//域条目 enum debug_info_levels x_debug_info_level;
    NString *str=n_string_new("");
    int i,j;
    for(i=0;i<size1;i++){
        if(strlen(a[i])<=0) //跳过 空字符串
            continue;
         char item[300];
         sprintf(item,"%s;",a[i]);//加分号到最后
         int itemLen=strlen(item);
         char *find=NULL;

         for(j=0;j<size2;j++){
             if(strlen(b[j])<=0) //跳过 空字符串
                continue;
             int blen=strlen(b[j]);
             if(blen<itemLen)
                 continue;
             char *xbcmp=b[j]+blen-itemLen; //b[j]最后字符串
             if(!strcmp(xbcmp,item)){
                find=b[j];
                break;
            }
         }
        if(!find){
            fprintf(stderr,"出错了创建MtcsOptionsItem :%s\n",item);
            exit(0);
        }
        n_string_append(str,find);
        n_string_append(str,"\n");
    }

    //printf("创建 MtcsOptionsItem----- %s\n",str->str);
    char *result=n_string_free(str,FALSE);
    return result;
}

//求交集与源集合的差
//ptx/opitons.h集合与mtcsoptionsitem集合的差
//setA options.h的域
//setB mtcsoptionsitem的域
static char *setDifference(char *setA,char *setB)
{
        char** a=n_strsplit(setA,"\n",-1);
        char** b=n_strsplit(setB,"\n",-1);
        int size1=n_strv_length(a);//域条目的名称
        int size2=n_strv_length(b);//域条目 enum debug_info_levels x_debug_info_level;
        NString *str=n_string_new("");
        int i,j;
        for(i=0;i<size1;i++){
            if(strlen(a[i])<=0) //跳过 空字符串
                continue;
            int find=0;
            for(j=0;j<size2;j++){
                if(strlen(b[j])<=0) //跳过 空字符串
                   continue;
                if(!strcmp(a[i],b[j])){
                   find=1;
                   break;
               }
            }
            if(!find){
                n_string_append(str,a[i]);
                n_string_append(str,"\n");
            }

        }
        //printf("创建 集合A与集合B的差是----- %s\n",str->str);
        char *result=n_string_free(str,FALSE);
        return result;
}



static int getArray(char *init,char *ret)
{
     char *openBrace=strstr(init,"{");
     if(!openBrace)
         return 0 ;
     char *closeBrace=strstr(init,"}");
     if(!closeBrace)
           return 0 ;
     openBrace=openBrace+1;//去除大括号
     strncpy(ret,openBrace,strlen(openBrace)-strlen(closeBrace));
     ret[strlen(openBrace)-strlen(closeBrace)]='\0';
     return strlen(ret);
}

static  int getArrayName(char *srcName,char *destName)
{
      char *open=strstr(srcName,"[");
      if(!open)
          return 0 ;
      char *close=strstr(srcName,"]");
      if(!close)
            return 0 ;
      strncpy(destName,srcName,strlen(srcName)-strlen(open));
      destName[strlen(srcName)-strlen(open)]='\0';
      return strlen(destName);
}

static void writeArray(char *init,char *name,char *varName,int isCommon,NString *str)
{
   char ret[512];
   int len= getArray(init,ret);
   if(len==0){
       fprintf(stderr,"%s 初始值不是数组 :%s\n",name,init);
       exit(0);
   }
   char destName[512];
   len=getArrayName(name,destName);
   if(len==0)
       sprintf(destName,"%s",name);

   char** arrays=n_strsplit(ret,",",-1);
   int arrayLen=n_strv_length(arrays);
   int i;
   int count=0;
   for(i=0;i<arrayLen;i++){
       arrays[i]=n_strstrip(arrays[i]);
       if(!arrays[i] || strlen(arrays[i])==0)
           continue;
       if(isCommon){
         n_string_append_printf(str,"mtcsOptionsItem->%s[%d]=%s;",destName,count,arrays[i]);
         n_string_append(str,"\n");
       }else{
         n_string_append_printf(str,"%s->%s[%d]=%s;",varName,destName,count,arrays[i]);
         n_string_append(str,"\n");
       }
       count++;
   }
   fprintf(stderr,"数组1---- %s\n",init);
   fprintf(stderr,"数组2---- %s\n",ret);
}

static int isArray(char *init)
{
   char *openBrace=strstr(init,"{");
   if(!openBrace)
       return 0;
   char *closeBrace=strstr(init,"}");
   if(!closeBrace)
         return 0;
   return 1;
}

/**
 * 替换逗号为分号
 * false, /\* dump_base_name_prefixed *\/
 * 0, /\* exit_after_options *\/
 */
static void replaceComma(char *str)
{
   char old_char = ',';
   char new_char = ';';
   for (int i = 0; str[i] != '\0'; i++) {
      if (str[i] == old_char) {
         str[i] = new_char; // 直接通过下标赋值替换
      }
   }
}

/**
 * 取出注释中的名字 dump_base_name_prefixed或 exit_after_options
 * false, /\* dump_base_name_prefixed *\/
 * 0, /\* exit_after_options *\/
 */
static void getOptionsName(char *initValue,char *compName)
{
   char *pre=strstr(initValue,"/*");
   if(pre==NULL){
      printf("mtcsgenoptions.c 初始值没有注释 %s\n",initValue);
      exit(0);
   }
   pre=pre+2;
   char *after=strstr(pre,"*/");
   if(after==NULL){
      printf("mtcsgenoptions.c 初始值没有注释 %s\n",initValue);
      exit(0);
   }
   strncpy(compName,pre,strlen(pre)-strlen(after));
   compName[strlen(pre)-strlen(after)]='\0';
}


//为PtxOptionsItem设初始值
//deviceOptionsHFile是设备的头文件
//home/sns/workspace/gcc-14-20240421/src/build-nvptx-gcc/gcc/options.h
//初始值在options.cc中
static char * setInitValue(char *varName,char *deviceOptionsHFile,char *fieldRawNames,char *commonFiedlRawNames)
{
    char **rawNames=n_strsplit(fieldRawNames,"\n",-1);
    int rawNamesCount=n_strv_length(rawNames);

    char **commonRawNames=n_strsplit(commonFiedlRawNames,"\n",-1);
    int commonRawNamesCount=n_strv_length(commonRawNames);

    char ccFile[500];
    createCCFile(deviceOptionsHFile,ccFile);
    fprintf(stderr,"setInitValue 设备 file is :%s varName:%s\n",ccFile,varName);
    int filseSize=getFileSize(ccFile);
    char *buffer=xmalloc(filseSize+1);
    FILE *fp=fopen(ccFile,"r");
    int len=fread(buffer,1,filseSize+1,fp);
    fclose(fp);
    buffer[len]='\0';
    //找出 struct gcc_options
    char *gccopt=strstr(buffer,"const struct gcc_options global_options_init =");
    if(!gccopt){
        printf("文件 :%s不是有效的options.h\n",ccFile);
        exit(0) ;
    }
    gccopt=gccopt+strlen("const struct gcc_options global_options_init =");//去除字符串struct gcc_options
    //第一个"};"是 struct gcc_options的结束标志
    char** items= n_strsplit(gccopt,"};",2);
    char *options=items[0]+2;//跳过 {
    //printf("options is :%s\n",options);
    // printf("fist ---- %s\n",options);
    NString *str=n_string_new("");
    char** initValues=n_strsplit(options,"\n",-1);
    int initValueCount=n_strv_length(initValues);

    char *compName = xmalloc(512);
    int i,j,z;
    j=0;
    for(i=0;i<initValueCount;i++){
        initValues[i]=n_strstrip(initValues[i]);
        if(initValues[i]==NULL || strlen(initValues[i])==0 || strstr(initValues[i],"#undef")){
           continue;
        }
        getOptionsName(initValues[i],compName);
        replaceComma(initValues[i]);
        compName=n_strstrip(compName);
        char *initv = initValues[i];
         char *name=rawNames[j];
         if(!strstr(name,compName)){
             //初始值与域不匹配 j:1399 0, /* VAR_mmainkernel (private state) */ x_VAR_mmainkernel VAR_mmainkernel (private state)
             char** temps=n_strsplit(compName," ",-1);
             if(temps!=NULL && strstr(name,temps[0])){
                  ;
             }else{
               printf("初始值与域不匹配 j:%d %s %s %s\n",j,initValues[i],name,compName);
               exit(0);
             }
         }

        //判断这个名字在mtcsoptionsitem还是差集中
        int isCommon=0;
        for(z=0;z<commonRawNamesCount;z++){
            if(!strcmp(name,commonRawNames[z])){
                isCommon=1;
                break;
            }
        }
        fprintf(stderr,"options is :i:%d j:%d initValue:%s name:%s %d initv:%s\n",
              i,j,initValues[i],name,strlen(initValues[i]),initv);
        if(!isArray(initv)){
            if(isCommon){
                n_string_append_printf(str,"mtcsOptionsItem->%s=%s",name,initv);
                n_string_append(str,"\n");
            }else{
                n_string_append_printf(str,"%s->%s=%s",varName,name,initv);
                n_string_append(str,"\n");
            }
        }else{
            writeArray(initv,name,varName,isCommon,str);
        }
        j++;
    }
    //fprintf(stderr,"赋值------ %s\n",str->str);
    char *result=n_string_free(str,FALSE);
    return result;
}

//原型 struct GTY(()) cl_target_option options.h
static char *genClTargetOption()
{
  NString *src=n_string_new("");
  n_string_append(src,"typedef struct _MtcsClTargetOption\n{\n");
  n_string_append(src,"\t int x_target_flags;\n");
  n_string_append(src,"\t /* 0 members */\n");
  n_string_append(src,"\t int  explicit_mask_target_flags;\n");
  n_string_append(src,"}MtcsClTargetOption;\n");
  char *result=n_string_free(src,FALSE);
  return result;
}


/**
 * 生成mtcsoptionsitem.h文件
 */
static void genCommonOptionsItemH(char *commonFlags)
{
    NString *content=n_string_new("");
    char *explain="/* 该文件由mtcsgenoptions生成,是多个平台的options.h中gcc_options的交集。  */\n\n";
    n_string_append(content,explain);
    n_string_append(content,"#ifndef __GCC_MTCS_OPTIONS_ITEM__\n");
    n_string_append(content,"#define __GCC_MTCS_OPTIONS_ITEM__\n\n");
    n_string_append(content,"#include \"flag-types.h\"\n\n");

    n_string_append(content,"typedef struct _MtcsOptionsItem{\n");
    n_string_append(content,commonFlags);
    n_string_append(content,"\n}MtcsOptionsItem;\n\n");

    char *clTargetOption=genClTargetOption();//生成struct GTY(()) cl_target_option 因为nvptx比较简单，不再从opitons.h读取
    n_string_append(content,clTargetOption);
    n_string_append(content,"\n");

    n_string_append(content,"#endif\n");
    printf("%s\n",content->str);
}

/**
 * 从device的opt转为主机的opt
 * case PTX_OPT_SPECIAL_program_name: return OPT_SPECIAL_program_name;
    case PTX_OPT_SPECIAL_input_file: return OPT_SPECIAL_input_file;
    .....
 */
 static char *convertOptToHost(char *devicePrefix,char *opts)
 {
      char *upper=n_ascii_strup(devicePrefix,strlen(devicePrefix));
      NString *str=n_string_new("");
      char** ret=n_strsplit(opts,"\n",-1);
      int re=n_strv_length(ret);
      int i,j;
      for(i=0;i<re;i++){
          char *opt=n_strstrip(ret[i]);
          if(!opt || strlen(opt)==0)
              continue;
          n_string_append_printf(str,"\t\tcase %s_%s: return %s;\n",upper,opt,opt);
      }
      char *result=n_string_free(str,FALSE);
      return result;
 }

 //主机opt_code转为设备的opt_code
 static char *convertOptToDevice(char *devicePrefix,char *opts)
 {
      char *upper=n_ascii_strup(devicePrefix,strlen(devicePrefix));
      NString *str=n_string_new("");
      char** ret=n_strsplit(opts,"\n",-1);
      int re=n_strv_length(ret);
      int i,j;
      for(i=0;i<re;i++){
          char *opt=n_strstrip(ret[i]);
          if(!opt || strlen(opt)==0)
              continue;
          n_string_append_printf(str,"\t\tcase %s: return %s_%s;\n",opt,upper,opt);
      }
      char *result=n_string_free(str,FALSE);
      return result;
}

 //用设备的opts和opts_set设置主机的opts opts_Set
 static char *setOptions(char *commonFieldRawNames)
 {
          char** ret=n_strsplit(commonFieldRawNames,"\n",-1);
          int itemCount=n_strv_length(ret);
          NString *str=n_string_new("");
          int i;
          for(i=0;i<itemCount;i++){
             char *item=ret[i];
             if(!item || strlen(item)==0)
                 continue;
             n_string_append_printf(str,"\thostOpts->%s=deviceOpts->%s;\n",item,item);
          }
          n_string_append(str,"\n");
          for(i=0;i<itemCount;i++){
              char *item=ret[i];
              if(!item || strlen(item)==0)
                  continue;
              n_string_append_printf(str,"\thostOptsSet->%s=deviceOptsSet->%s;\n",item,item);
          }
          char *result=n_string_free(str,FALSE);
          return result;
 }

/**
 * 生成ptx-optionsitem.h文件
 * devicePrefix是ptx
 */
static void genDeviceOptionH(char *devicePrefix,char *deviceOptionsItem,char *enumOpts,char *deviceMicro,char *deviceClTargetOption)
{
    NString *content=n_string_new("");
    char *explain="/* 该文件由mtcsgenoptions生成*/\n\n";
    char *upper=n_ascii_strup(devicePrefix,strlen(devicePrefix));
    char *objectUpper=n_strdup(devicePrefix);
    objectUpper[0]=n_ascii_toupper(objectUpper[0]);
    n_string_append(content,explain);
    n_string_append_printf(content,"#ifndef __GCC_%s_OPTIONS_ITEM__\n",upper);
    n_string_append_printf(content,"#define __GCC_%s_OPTIONS_ITEM__\n\n",upper);
    n_string_append(content,"#include \"../../mtcsoptionsitem.h\"\n\n");

    //生成 PtxOptionsItem
    n_string_append_printf(content,"typedef struct _%sOptionsItem\n{\n",objectUpper);
    n_string_append(content,"MtcsOptionsItem parent;\n");
    n_string_append(content,deviceOptionsItem);
    n_string_replace(content,"enum ptx_isa","int",1); //特殊处理 todo
    n_string_replace(content,"enum ptx_version","int",1);  //特殊处理 todo
    n_string_append_printf(content,"\n}%sOptionsItem;\n\n",objectUpper);

    //生成 PtxClTargetOption MtcsClTargetOption
    n_string_append_printf(content,"typedef struct _%sClTargetOption\n{\n",objectUpper);
    n_string_append(content,"\tMtcsClTargetOption parent;\n");
    if(deviceClTargetOption!=NULL)
      n_string_append(content,deviceClTargetOption);
    n_string_append_printf(content,"\n}%sClTargetOption;\n\n",objectUpper);

    //生成 enum Ptx_opt_code
    n_string_append_printf(content,"enum %s_opt_code\n{\n",devicePrefix);
    n_string_append(content,enumOpts);
    n_string_append(content,"\n};\n\n");

    n_string_append(content,deviceMicro);
    n_string_append(content,"\n\n");

    n_string_append_printf(content,"void %s_options_item_init(%sOptionsItem *self);\n\n",devicePrefix,objectUpper);
    n_string_append_printf(content, "int %s_options_get_cl_count();\n",devicePrefix);
    n_string_append_printf(content, "struct cl_option * %s_options_get_cl_option();\n\n",devicePrefix);
    n_string_append_printf(content, "int %s_options_get_cl_lang_count();\n",devicePrefix);

    n_string_append_printf(content, "int %s_options_optcode_device_to_host(int deviceOptCode);\n\n",devicePrefix);
    n_string_append_printf(content, "int %s_options_optcode_host_to_device(int hostOptCode);\n\n",devicePrefix);
    n_string_append_printf(content, "void %s_cl_optimization_save(struct cl_optimization *ptr,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);\n\n",devicePrefix);
    n_string_append_printf(content, "void %s_cl_optimization_restore(MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr);\n\n",devicePrefix);
    n_string_append_printf(content, "void %s_options_set_gcc_options(MtcsOptionsItem *deviceOpts,"
            "MtcsOptionsItem *deviceOptsSet,struct gcc_options *hostOpts,struct gcc_options *hostOptsSet);\n",devicePrefix);

    n_string_append_printf(content, "struct cl_enum *%s_get_cl_enums();\n\n",devicePrefix);

    n_string_append(content,"#endif\n");
    printf("%s\n",content->str);
}



/**
 * 生成ptx-options.c文件
 * devicePrefix是ptx
 */
static void genDeviceOptionC(char *devicePrefix,char *init,char *deviceClOptions, char *deviceOptToHost,
      char *hostOptToDevice,char *clOptimizationSave,char *clOptimizationRestore,
      char *setOptionsStr,char *clEnum,int clLangCount)
{
    NString *content=n_string_new("");
    char *explain="/* 该文件由mtcsgenoptions生成*/\n\n";
    char *upper=n_ascii_strup(devicePrefix,strlen(devicePrefix));
    char *objectUpper=n_strdup(devicePrefix);
    objectUpper[0]=n_ascii_toupper(objectUpper[0]);
    n_string_append(content, explain);

     n_string_append(content, "#define IN_TARGET_CODE 1\n");
     n_string_append(content, "#include \"config.h\"\n");
     n_string_append(content, "#include \"system.h\"\n");
     n_string_append(content, "#include \"coretypes.h\"\n");
     n_string_append(content, "#include \"backend.h\"\n");
     n_string_append(content, "#include \"predict.h\"\n");
     n_string_append(content, "#include \"tree.h\"\n");
     n_string_append(content, "#include \"rtl.h\"\n");
     n_string_append(content, "#include \"alias.h\"\n");
     n_string_append(content, "#include \"varasm.h\"\n");
     n_string_append(content, "#include \"stor-layout.h\"\n");
     n_string_append(content, "#include \"calls.h\"\n");
     n_string_append(content, "#include \"memmodel.h\"\n");
     n_string_append(content, "#include \"tm_p.h\"\n");
     n_string_append(content, "#include \"flags.h\"\n");
     n_string_append(content, "#include \"insn-config.h\"\n");
     n_string_append(content, "#include \"expmed.h\"\n");
     n_string_append(content, "#include \"dojump.h\"\n");
     n_string_append(content, "#include \"explow.h\"\n");
     n_string_append(content, "#include \"emit-rtl.h\"\n");
     n_string_append(content, "#include \"stmt.h\"\n");
     n_string_append(content, "#include \"expr.h\"\n");
     n_string_append(content, "#include \"insn-codes.h\"\n");
     n_string_append(content, "#include \"optabs.h\"\n");
     n_string_append(content, "#include \"dfp.h\"\n");
     n_string_append(content, "#include \"output.h\"\n");
     n_string_append(content, "#include \"recog.h\"\n");
     n_string_append(content, "#include \"df.h\"\n");
     n_string_append(content, "#include \"resource.h\"\n");
     n_string_append(content, "#include \"reload.h\"\n");
     n_string_append(content, "#include \"diagnostic-core.h\"\n");
     n_string_append(content, "#include \"regs.h\"\n");
     //n_string_append(content, "#include \"tm-constrs.h\"\n");
     n_string_append(content, "#include \"ggc.h\"\n");
     n_string_append(content, "#include \"target.h\"\n\n");
     n_string_append(content, "#include \"cpplib.h\"\n");
     n_string_append(content, "#include \"diagnostic-color.h\"\n");
     n_string_append(content, "#include \"diagnostic-url.h\"\n");
     n_string_append(content, "#include \"diagnostic.h\"\n");
     n_string_append(content, "#include \"pretty-print.h\"\n");
     n_string_append(content, "#include \"opts.h\"\n");
     n_string_append(content, "#include \"intl.h\"\n");
     n_string_append(content, "#include \"insn-attr-common.h\"\n");
     if(strcmp(devicePrefix,"ptx")==0)
        n_string_append(content, "#include \"../ptx-common.h\"\n");

     n_string_append(content, "#include \"../../../nlib.h\"\n");
     n_string_append_printf(content, "#include \"%s-optionsitem.h\"\n\n",devicePrefix);
     //声明 static const char *devicePrefix="ptx";
     n_string_append_printf(content, "static const char *devicePrefix=\"%s\";\n\n",devicePrefix);
     n_string_append_printf(content, "#define %s_ISA_unset 0\n",upper); //临时解决编译时，找不到PTX_ISA_unset
     n_string_append_printf(content, "#define %s_VERSION_unset 0\n\n",upper);
     //生成 从 static const struct cl_enum_arg cl_enum_apx_features_data[] =
     //到 const unsigned int cl_enums_count = 的内容
     n_string_append(content, "\n");
     n_string_append(content,clEnum);
     n_string_append(content, "\n");
     //返回 struct ptx_cl_enums
     n_string_append_printf(content, "struct cl_enum * %s_get_cl_enums()\n",devicePrefix);
     n_string_append(content, "{\n");
     n_string_append_printf(content, "\t return %s_cl_enums;\n",devicePrefix);
     n_string_append(content, "}\n");
     n_string_append(content, "\n");

     n_string_append_printf(content, "void %s_options_item_init(%sOptionsItem *self)\n",devicePrefix,objectUpper);
     n_string_append(content, "{\n");
     n_string_append(content, "\tMtcsOptionsItem *mtcsOptionsItem=(MtcsOptionsItem*)self;\n");
     n_string_append(content, init);
     n_string_append(content, "}\n\n");

     n_string_append_printf(content, "static const unsigned int %s_cl_options_count =%s_N_OPTS;\n",devicePrefix,upper);
     n_string_append_printf(content, "static const unsigned int %s_cl_lang_count = %d;\n\n",devicePrefix,clLangCount);

     n_string_append_printf(content, "static const struct cl_option %s_cl_options[] =\n",devicePrefix);
     n_string_append(content, deviceClOptions);
     n_string_append(content, "};\n\n");

     n_string_append_printf(content, "int %s_options_get_cl_count()\n",devicePrefix);
     n_string_append(content, "{\n");
     n_string_append_printf(content, "\treturn  %s_cl_options_count;\n",devicePrefix);
     n_string_append(content, "}\n\n");

     n_string_append_printf(content, "struct cl_option * %s_options_get_cl_option()\n",devicePrefix);
     n_string_append(content, "{\n");
     n_string_append_printf(content, "\treturn %s_cl_options;\n",devicePrefix);
     n_string_append(content, "}\n\n");

     //生成函数int ptx_options_optcode_device_to_host(int deviceOptCode)
     n_string_append_printf(content, "int %s_options_optcode_device_to_host(int deviceOptCode)\n",devicePrefix);
     n_string_append(content, "{\n");
     n_string_append(content, "\tswitch(deviceOptCode){\n\n");
     n_string_append(content, deviceOptToHost);
     n_string_append(content, "\t\tdefault:\n");
     n_string_append(content, "\t\tn_info(\"设备 %s optcode 转主机失败。平台独有的opt_code:%d\\n\",devicePrefix,deviceOptCode);\n");
     n_string_append(content, "\t\treturn -1;\n");
     n_string_append(content, "\t}\n");
     n_string_append(content, "}\n\n");

     //生成函数int ptx_options_optcode_host_to_device(int hostOptCode)
      n_string_append_printf(content, "int %s_options_optcode_host_to_device(int hostOptCode)\n",devicePrefix);
      n_string_append(content, "{\n");
      n_string_append(content, "\tswitch(hostOptCode){\n\n");
      n_string_append(content, hostOptToDevice);
      n_string_append(content, "\t\tdefault:\n");
      n_string_append(content, "\t\tn_info(\"主机的opt_code与设备 %s 的opt_code 不对应:主机optcode:%d\\n\",devicePrefix,hostOptCode);\n");
      n_string_append(content, "\t\treturn -1;\n");
      n_string_append(content, "\t}\n");
      n_string_append(content, "}\n\n");
     //生成函数 ptx_cl_optimization_save(struct cl_optimization *ptr, MtcsOptionsItem *opts,tMtcsOptionsItem *opts_set)
      n_string_append_printf(content, "void %s_cl_optimization_save(struct cl_optimization *ptr,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set)\n",devicePrefix);
      n_string_append(content, "{\n");
      n_string_append(content, clOptimizationSave);
      n_string_append(content, "}\n\n");
      //生成函数 ptx_cl_optimization_restore(MtcsOptionsItem *opts, MtcsOptionsItem *opts_set,struct cl_optimization *ptr)
      n_string_append_printf(content, "void %s_cl_optimization_restore(MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr)\n",devicePrefix);
      n_string_append(content, "{\n");
      n_string_append(content, clOptimizationRestore);
      n_string_append_printf(content,"\n\t/* targetm.override_options_after_change ()在Mtcs%sOptions中调用 */\n",objectUpper);
      n_string_append(content, "}\n\n");

      //生成函数体void ptx_options_set_gcc_options(MtcsOptionsItem *deviceOpts,MtcsOptionsItem *deviceOptsSet,
      //struct gcc_options *hostOpts,struct gcc_options *hostOptsSet);\n",devicePrefix);
      n_string_append_printf(content, "void %s_options_set_gcc_options(MtcsOptionsItem *deviceOpts,MtcsOptionsItem *deviceOptsSet,\
              struct gcc_options *hostOpts,struct gcc_options *hostOptsSet)\n",devicePrefix);
      n_string_append(content, "{\n");
      n_string_append(content, setOptionsStr);
      n_string_append(content, "}\n\n");

      //函数 int %s_options_get_cl_lang_count();
      n_string_append_printf(content, "int %s_options_get_cl_lang_count()\n",devicePrefix);
      n_string_append(content, "{\n");
      n_string_append_printf(content, "\treturn %s_cl_lang_count;\n",devicePrefix);
      n_string_append(content, "}\n\n");

      printf("%s\n",content->str);
}

/**
 * enum opt_code
{
  OPT____ = 0,
  OPT__completion_ = 9,
  ....
  转成
  返回值内容是raw名
  OPT____
  OPT__completion_
  ...
  enumOpts内容是:
  PTX_OPT____ = 0,
  PTX_OPT__completion_ = 9,
          ....
*/
static char *createOptItem(char *devicePrefix,char *srcFileName,NString *enumOpts)
{
     FILE *fp=fopen(srcFileName,"r");
     char buffer[4096*150];
     int len=fread(buffer,1,4096*150,fp);
     fclose(fp);
     buffer[len+1]='\0';
     //找出 struct gcc_options
     char *gccopt=strstr(buffer,"enum opt_code");
     if(!gccopt){
         printf("文件 :%s不是有效的options.h\n",srcFileName);
         exit(0) ;
     }
     gccopt=gccopt+strlen("enum opt_code");//去除字符串struct gcc_options
     //第一个"};"是 struct gcc_options的结束标志
     char** items= n_strsplit(gccopt,"};",2);
     char *options=items[0];
    // printf("fist ---- %s\n",options);
     char *upper=n_ascii_strup(devicePrefix,strlen(devicePrefix));
     NString *str=n_string_new("");
     char** ret=n_strsplit(options,"\n",-1);
     int re=n_strv_length(ret);
     int i,j;
     for(i=0;i<re;i++){
         char *opt=n_strstrip(ret[i]);
         if(!strcmp(opt,"{") || strlen(opt)==0)
             continue;
         if(opt[0]=='/' && opt[1]=='*'){
             ;//跳过注释
         }else{
             char *optName=strstr(opt,"=");
             if(optName==NULL){
                 //fprintf(stderr,"opt没有赋值--- file:%s %s\n",srcFileName,opt);
                 if(strstr(opt,"N_OPTS") || strstr(opt,"OPT_SPECIAL_unknown")
                         || strstr(opt,"OPT_SPECIAL_ignore") || strstr(opt,"OPT_SPECIAL_warn_removed")
                         || strstr(opt,"OPT_SPECIAL_program_name") || strstr(opt,"OPT_SPECIAL_input_file")){
                     optName=strstr(opt,",");
                     if(!optName)
                         optName="";
                 }else{
                      exit(0);
                 }
             }
             char last[512];
             strncpy(last,opt,strlen(opt)-strlen(optName));
             last[strlen(opt)-strlen(optName)]='\0';
             //printf("last is :%s\n",last);
             n_string_append(str,last);
             n_string_append(str,"\n");
             n_string_append_printf(enumOpts,"%s_%s",upper,opt);
             n_string_append(enumOpts,"\n");
         }

     }
     char *result=n_string_free(str,FALSE);
     return result;
}

/*
 * 从options.cc中取出 const struct cl_option cl_options[]
 * 并替换OPT_ 为 upper(devicePrefix)_OPT_
    offsetof (struct gcc_options, x_param_max_predicted_iterations), 0, CLVC_INTEGER, 0, 0, 65536 },
    struct gcc_options替换成 MtcsOptionsItem
        offsetof (struct gcc_options, x_nvptx_experimental), 0, CLVC_INTEGER, 0, -1, -1 },
    struct gcc_options,替换成PtxOptionsItem
 *  注意：deviceOptionsHFile 是头文件 如ptx_options.h 转化生成ptx_options.cc文件,然后打开ptx_options.cc
 *  ptx_options.cc是从ptx平台的options.cc拷到mtcs/ptx/下改名为ptx_options.cc
 */
static char * createClOptions(char *devicePrefix,char *deviceOptionsHFile,char *deviceDifferrence)
{
   fprintf(stderr,"createClOptions 00 %s %s\n",devicePrefix,deviceOptionsHFile);
    char *objectUpper=n_strdup(devicePrefix);
    objectUpper[0]=n_ascii_toupper(objectUpper[0]);
    fprintf(stderr,"createClOptions 11 %s %s %s\n",devicePrefix,deviceOptionsHFile,objectUpper);
    char ccFile[500];
    createCCFile(deviceOptionsHFile,ccFile);
    fprintf(stderr,"createClOptions 22 %s %s n:%s\n",devicePrefix,deviceOptionsHFile,ccFile);
    int filseSize=getFileSize(ccFile);
    char *buffer=xmalloc(filseSize+1);
    FILE *fp=fopen(ccFile,"r");
    int len=fread(buffer,1,filseSize+1,fp);
    fclose(fp);
    buffer[len]='\0';
    //找出 struct gcc_options
    char *gccopt=strstr(buffer,"const struct cl_option cl_options[] =");
    if(!gccopt){
        fprintf(stderr,"文件 :%s不是有效的options.h\n",ccFile);
        exit(0) ;
    }
    fprintf(stderr,"createClOptions 44 %s %s\n",devicePrefix,deviceOptionsHFile);

    gccopt=gccopt+strlen("const struct cl_option cl_options[] =");//去除字符串const struct cl_option cl_options[] =
    //第一个"};"是 const struct cl_option cl_options[] =的结束标志
    char** items= n_strsplit(gccopt,"};",2);
    char *options=items[0];
    NString *str=n_string_new(options);
    char *upper=n_ascii_strup(devicePrefix,strlen(devicePrefix));
    char nopts[20];
    sprintf(nopts,"%s_N_OPTS",upper);
    char opts[20];
    sprintf(opts,"%s_OPT_",upper);
    n_string_replace(str,"N_OPTS",nopts,-1);
    n_string_replace(str,"OPT_",opts,-1);
    n_string_replace(str,"struct gcc_options","MtcsOptionsItem",-1);
    char** ret=n_strsplit(deviceDifferrence,"\n",-1);
    int re=n_strv_length(ret);
    int i,j;
    for(i=0;i<re;i++){
        char *optionFlag=n_strstrip(ret[i]);
        if(!optionFlag || strlen(optionFlag)==0)
            continue;
        char find[256];
        //struct gcc_options, x_nvptx_experimental 变成了 MtcsOptionsItem, x_nvptx_experimental
        sprintf(find,"MtcsOptionsItem, %s",optionFlag);
        char replace[256];
        sprintf(replace,"%sOptionsItem, %s",objectUpper,optionFlag);
        n_string_replace(str,find,replace,-1);
    }
    fprintf(stderr,"createClOptions 55 %s %s\n",devicePrefix,deviceOptionsHFile);

    char *result=n_string_free(str,FALSE);
    return result;
}

/*
 * 从optons.h中取出宏定义
#define MASK_ABI64 (1U << 0)
#define MASK_GOMP (1U << 1)
....
#define SET_TARGET_UNIFORM_SIMT(opts) opts->x_target_flags |= MASK_UNIFORM_SIMT
*/
static char *getDeviceMicro(char *devicePrefix,char *deviceOptionsH)
{
    if(strcmp(devicePrefix,"ptx")){
        fprintf(stderr,"%s平台还未实现。",devicePrefix);
        exit(0);
    }
    char *start="#define MASK_";
    char *end="#define SET_TARGET_UNIFORM_SIMT(opts) opts->x_target_flags |= MASK_UNIFORM_SIMT";
    FILE *fp=fopen(deviceOptionsH,"r");
    char buffer[4096*150];
    int len=fread(buffer,1,4096*150,fp);
    fclose(fp);
    buffer[len+1]='\0';
    //找出 struct gcc_options
    char *gccopt=strstr(buffer,start);
    if(!gccopt){
        printf("文件 :%s不是有效的options.h\n",deviceOptionsH);
        exit(0) ;
    }
    char *gccopt1=strstr(buffer,end);
   // char ret[1024*10];
    int strLen=strlen(gccopt)-strlen(gccopt1)+strlen(end);
  //  strncpy(ret,gccopt,strLen);
   // ret[strLen]='\0';
    NString *str=n_string_new("");
    n_string_append_len(str,gccopt,strLen);
    char *result=n_string_free(str,FALSE);
    return result;
}

/*-----以下生成 void ptx_cl_optimization_save (struct cl_optimization *ptr, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set)-----*/

//获取结构体struct GTY(()) cl_optimization中的域，主机与设备域相同 原型声明在 options.h options-save.cc

static char *getClOptimization(char *hostFileH)
{
       FILE *fp=fopen(hostFileH,"r");
       char buffer[4096*250];
       int len=fread(buffer,1,4096*250,fp);
       fclose(fp);
       buffer[len+1]='\0';
       //找出 struct gcc_options
       char *gccopt=strstr(buffer,"struct GTY(()) cl_optimization");
       if(!gccopt){
           fprintf(stderr,"文件 :%s不是有效的options.h\n",hostFileH);
           exit(0) ;
       }
       gccopt=gccopt+strlen("struct GTY(()) cl_optimization");//去除字符串const struct cl_option cl_options[] =
       //第一个"};"是 const struct cl_option cl_options[] =的结束标志
       char** items= n_strsplit(gccopt,"};",2);
       char *optimizs=items[0];
       char** ret=n_strsplit(optimizs,"\n",-1);
       int itemCount=n_strv_length(ret);
       int i,j;
       NString *str=n_string_new("");
       for(i=0;i<itemCount;i++){
          char *item=n_strstrip(ret[i]);
          if(!strcmp(item,"{") || strlen(item)==0 || (item[0]=='/' && item[1]=='*'))
              continue;
          char** whitespace= n_strsplit(item," ",-1);
          char *name=whitespace[n_strv_length(whitespace)-1];
          name=n_strstrip(name);
          if(name[0]=='*')
              name=name+1;
          n_string_append_len(str,name,strlen(name)-1);
          n_string_append(str,"\n");

       }
       char *result=n_string_free(str,FALSE);
       return result;
}

static char *getIN_RANGE(char *clOptimization)
{
    char** ret=n_strsplit(clOptimization,"\n",-1);
    int itemCount=n_strv_length(ret);
    NString *str=n_string_new("");
    int i,j;
    for(i=0;i<itemCount;i++){
       char *item=ret[i];
       if(!strcmp(item,"x_optimize")){
           n_string_append(str,item);
           n_string_append(str,"\n");
           for(j=i+1;j<itemCount;j++){
               char *item=ret[j];
               n_string_append(str,item);
               if(!strcmp(item,"x_debug_nonbind_markers_p")){
                   break;
               }
               n_string_append(str,"\n");
           }
           break;
       }
     }
    char *result=n_string_free(str,FALSE);
    return result;
}

static char *genIN_RANGE(char *inrange)
{
    char** ret=n_strsplit(inrange,"\n",-1);
    int itemCount=n_strv_length(ret);
    NString *str=n_string_new("");
    int i,j;
    for(i=0;i<itemCount;i++){
       char *item=ret[i];
       if(!strcmp(item,"x_optimize"))
          n_string_append_printf(str,"\tgcc_assert (IN_RANGE (opts->%s, 0, 255));\n",item);
       else if(!strcmp(item,"x_optimize_size"))
           n_string_append_printf(str,"\tgcc_assert (IN_RANGE (opts->%s, 0, 2));\n",item);
       else if(!strcmp(item,"x_optimize_debug") || !strcmp(item,"x_optimize_fast") )
           n_string_append_printf(str,"\tgcc_assert (IN_RANGE (opts->%s, 0, 1));\n",item);
       else
           n_string_append_printf(str,"\tgcc_assert (IN_RANGE (opts->%s, -128, 127));\n",item);
    }
    char *result=n_string_free(str,FALSE);
    return result;
}

//gcc-14 通成 explicit_mask[8] gcc-15生成explicit_mask[9]
//gcc-15比14多出x_ix86_unroll_only_small_loops
static char *genSave(char *optimizations)
{
    char** ret=n_strsplit(optimizations,"\n",-1);
    int itemCount=n_strv_length(ret);
    NString *str=n_string_new("");
    NString *endStr=n_string_new("");
    int i;
    for(i=0;i<itemCount;i++){
       char *item=ret[i];
       if(!item || strlen(item)==0 || !strcmp(item,"explicit_mask[9]") || !strcmp(item,"x_ix86_unroll_only_small_loops"))
           continue;
       if(!strcmp(item,"x_str_align_functions") || !strcmp(item,"x_str_align_jumps") || !strcmp(item,"x_str_align_labels")
               || !strcmp(item,"x_str_align_loops") || !strcmp(item,"x_flag_patchable_function_entry"))
          n_string_append_printf(endStr,"\tptr->%s=opts->%s;\n",item,item);
       else
           n_string_append_printf(str,"\tptr->%s=opts->%s;\n",item,item);
    }
    if(endStr->len>0)
        n_string_append(str,endStr->str);
    char *result=n_string_free(str,FALSE);
    return result;
}

static char *genExplicitMask(char *optimizations)
{
       char** ret=n_strsplit(optimizations,"\n",-1);
       int itemCount=n_strv_length(ret);
       NString *str=n_string_new("");
       n_string_append(str,"\tunsigned HOST_WIDE_INT mask = 0;\n");
       int i;
       int op=0;
       int index=0;
       for(i=0;i<itemCount;i++){
          char *item=ret[i];
          if(!item || strlen(item)==0 || !strcmp(item,"explicit_mask[9]") || !strcmp(item,"x_ix86_unroll_only_small_loops"))
              continue;
          if(!strcmp(item,"x_str_align_functions") || !strcmp(item,"x_str_align_jumps") || !strcmp(item,"x_str_align_labels")
                  || !strcmp(item,"x_str_align_loops") || !strcmp(item,"x_flag_patchable_function_entry"))
            continue;
          else{
              n_string_append_printf(str,"\tif(opts_set->%s) mask |= HOST_WIDE_INT_1U << %d;\n",item,op++);
              if(op==64){
                  op=0;
                  n_string_append_printf(str,"\tptr->explicit_mask[%d] = mask;\n",index++);
                  n_string_append(str,"\tmask=0;\n");
              }
          }
       }
       n_string_append_printf(str,"\tif(opts_set->x_str_align_functions) mask |= HOST_WIDE_INT_1U << %d;\n",op++);
       n_string_append_printf(str,"\tif(opts_set->x_str_align_jumps) mask |= HOST_WIDE_INT_1U << %d;\n",op++);
       n_string_append_printf(str,"\tif(opts_set->x_str_align_labels) mask |= HOST_WIDE_INT_1U << %d;\n",op++);
       n_string_append_printf(str,"\tif(opts_set->x_str_align_loops) mask |= HOST_WIDE_INT_1U << %d;\n",op++);
       n_string_append_printf(str,"\tif(opts_set->x_flag_patchable_function_entry) mask |= HOST_WIDE_INT_1U << %d;\n",op++);
       n_string_append_printf(str,"\tptr->explicit_mask[%d] = mask;\n",index++);
       n_string_append(str,"\tmask=0;\n");
       char *result=n_string_free(str,FALSE);
       return result;
}
//---------结束生成ptx_cl_optimization_save--------------

//生成struct GTY(()) cl_target_option中的域
static char *getDeviceClTargetOptions(char *deviceFileH)
{
    return NULL;
}

//生成函数 void cl_optimization_restore (struct gcc_options *opts, struct gcc_options *opts_set,struct cl_optimization *ptr)
 static char *genRestoreOptimizations(char *optimizations)
 {
     char** ret=n_strsplit(optimizations,"\n",-1);
     int itemCount=n_strv_length(ret);
     NString *str=n_string_new("");
     NString *endStr=n_string_new("");
     int i;
     for(i=0;i<itemCount;i++){
        char *item=ret[i];
        if(!item || strlen(item)==0 || !strcmp(item,"explicit_mask[9]") || !strcmp(item,"x_ix86_unroll_only_small_loops"))
            continue;
        if(!strcmp(item,"x_str_align_functions") || !strcmp(item,"x_str_align_jumps") || !strcmp(item,"x_str_align_labels")
                || !strcmp(item,"x_str_align_loops") || !strcmp(item,"x_flag_patchable_function_entry"))
           n_string_append_printf(endStr,"\topts->%s=ptr->%s;\n",item,item);
        else
            n_string_append_printf(str,"\topts->%s=ptr->%s;\n",item,item);
     }
     if(endStr->len>0)
         n_string_append(str,endStr->str);
     char *result=n_string_free(str,FALSE);
     return result;
 }

 //选项对应的枚举类型
 static char *restoreEnums[15][2]=
 {
    {"x_flag_lto_locality_cloning","lto_locality_cloning_model"},/*gcc15增加*/
    {"x_param_ranger_debug","ranger_debug"},
    {"x_param_threader_debug","threader_debug"},
    {"x_flag_excess_precision","excess_precision"},
    {"x_flag_fp_contract_mode","fp_contract_mode"},
    {"x_flag_harden_control_flow_redundancy_check_noreturn","hardcfr_noret"},
    {"x_flag_inline_stringops","ilsop_fn"},
    {"x_flag_ira_algorithm","ira_algorithm"},
    {"x_flag_ira_region","ira_region"},
    {"x_flag_live_patching","live_patching_level"},
    {"x_flag_reorder_blocks_algorithm","reorder_blocks_algorithm"},
    {"x_flag_simd_cost_model","vect_cost_model"},
    {"x_flag_stack_reuse","stack_reuse_level"},
    {"x_flag_auto_var_init","auto_init_type"},
    {"x_flag_vect_cost_model","vect_cost_model"},
 };

 static char *findEnum(char *option)
 {
     int i;
     for(i=0;i<15;i++){
         if(strcmp(restoreEnums[i][0],option)==0)
             return restoreEnums[i][1];
     }
     return NULL;
 }

 static char *genRestoreExplicitMask(char *optimizations)
 {
        char** ret=n_strsplit(optimizations,"\n",-1);
        int itemCount=n_strv_length(ret);
        NString *str=n_string_new("");
        n_string_append(str,"\tunsigned HOST_WIDE_INT mask = 0;\n");
        int i;
        int op=0;
        int index=0;
        for(i=0;i<itemCount;i++){
           char *item=ret[i];
           if(!item || strlen(item)==0 || !strcmp(item,"explicit_mask[9]") || !strcmp(item,"x_ix86_unroll_only_small_loops"))
               continue;
           if(!strcmp(item,"x_str_align_functions") || !strcmp(item,"x_str_align_jumps") || !strcmp(item,"x_str_align_labels")
                   || !strcmp(item,"x_str_align_loops") || !strcmp(item,"x_flag_patchable_function_entry"))
             continue;
           else{
               if(op==0){
                   n_string_append_printf(str,"\tmask = ptr->explicit_mask[%d];\n",index++);
               }
               char *enumOpt=findEnum(item);
               if(enumOpt!=NULL)
                  n_string_append_printf(str,"\topts_set->%s =(enum %s)((mask & 1) != 0);\n",item,enumOpt);
               else
                  n_string_append_printf(str,"\topts_set->%s = (mask & 1) != 0;\n",item);
               n_string_append(str,"\tmask >>= 1;\n");
               op++;
               if(op==64){
                   op=0;
               }
           }
        }
        n_string_append(str,"\topts_set->x_str_align_functions =  (mask & 1) ? \"\" : NULL;\n");
        n_string_append(str,"\tmask >>= 1;\n");
        n_string_append(str,"\topts_set->x_str_align_jumps =  (mask & 1) ? \"\" : NULL;\n");
        n_string_append(str,"\tmask >>= 1;\n");
        n_string_append(str,"\topts_set->x_str_align_labels =  (mask & 1) ? \"\" : NULL;\n");
        n_string_append(str,"\tmask >>= 1;\n");
        n_string_append(str,"\topts_set->x_str_align_loops =  (mask & 1) ? \"\" : NULL;\n");
        n_string_append(str,"\tmask >>= 1;\n");
        n_string_append(str,"\topts_set->x_flag_patchable_function_entry =  (mask & 1) ? \"\" : NULL;\n");
        n_string_append(str,"\tmask >>= 1;\n");
        char *result=n_string_free(str,FALSE);
        return result;
}

 /**
  * 获取设备的options.c，从 ”static const struct cl_enum_arg cl_enum_auto_init_type_data[] =”
  * 到 "const unsigned int cl_enums_count ="的内容
  */
static char *getDeviceOptionCFileClEnum (char *deviceHeadFileName,char *platName,int *clLangCount)
{
   char name[512];
   sprintf(name,"%s",deviceHeadFileName);
   name[strlen(deviceHeadFileName)-1]='c';
   name[strlen(deviceHeadFileName)]='c';
   name[strlen(deviceHeadFileName)+1]='\0';
   FILE *fd=fopen(name,"r");

   NString *buffer=n_string_sized_new(1024*1024*2);
   int size=fread(buffer->str,1,1024*1024*2,fd);
   buffer->str[size]='\0';
   buffer->len=size;

   char *clLangCountStr="const unsigned int cl_lang_count =";
   int pos=n_string_indexof(buffer,clLangCountStr);
   int pos1=n_string_indexof_from(buffer,";",pos);
   NString *num=n_string_substring_from(buffer,pos+strlen(clLangCountStr),pos1);
   fprintf(stderr,"genoptions-- %s\n",num->str);
   *clLangCount=atoi(num->str);
   n_string_free(num,TRUE);


   char *start ="static const struct cl_enum_arg cl_enum_auto_init_type_data[] =";
   char *end ="const unsigned int cl_enums_count =";
   pos=n_string_indexof(buffer,start);
   pos1=n_string_indexof(buffer,end);
   NString *sub=n_string_substring_from(buffer,pos,pos1);
   //需要重新设计，临时这样
   n_string_replace(sub,"enum ptx_isa","int",-1);
   n_string_replace(sub,"enum ptx_version","int",-1);
   char *clenumVar="const struct cl_enum cl_enums";
   char replace[128];
   sprintf(replace,"static const struct cl_enum %s_cl_enums",platName);
   n_string_replace(sub,clenumVar,replace,1);
   return n_string_free(sub,FALSE);
}

/*
有一个问题
如果编译的gcc指定目标是nvptx-none --target=nvptx-none 生成的gcc/options.h内容
与/home/sns/workspace/gcc153/src/gcc-153/gcc/aet/mtcs/ptx/ptx_options.h
是一样了，生成的ptx-optionsitem.h内容中有
typedef struct _PtxOptionsItem
{
MtcsOptionsItem parent;
}PtxOptionsItem;
无
typedef struct _PtxOptionsItem
{
MtcsOptionsItem parent;
int x_nvptx_alias;
int x_nvptx_experimental;
int x_fake_exceptions;
int x_nvptx_fake_ptx_alloca;
int x_nvptx_init_regs;
int x_ptx_isa_option;
int x_nvptx_optimize;
int x_nvptx_comment;
int x_ptx_version_option;
int x_nvptx_softstack_size;
int x_VAR_mmainkernel;

}PtxOptionsItem;
编译通不过。所以必须用主机的options.h
只能在aet.mk中硬编码主机的options.h
*/

int main (int argc, const char **argv)
{
  fprintf(stderr,"mtcsgenoptions 参数 %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);
  char *hostFile=argv[1];//主机 options.h
  char *deviceFile=argv[2];//设备 options.h 如:ptx_options.h
  char *devicePrefix=argv[3];//设备的前缀，如:ptx
  devicePrefix=devicePrefix+2;//去除-W字符串
  //生成文件类型
  //1.-h 共同的options头文件 mtcsoptionsitem.h
  //2.-c 平台的options.cc
  //3.-q 平台的options.h头文件
  char *genFileType=argv[4];//-h 共同的options头文件 mtcsoptionsitem.h -c 平台的options.cc -q 平台的options.h头文件

  //fprintf(stderr,"mtcsgenoptions 参数 11 %s 是:%s %s vv--:%s\n",argv[1],argv[2],argv[3],argv[4]);
 // fprintf(stderr,"mtcsgenoptions 参数 11aaee hostFile %s\n",hostFile);
  char *optA=removeStructGccOptionsItem(hostFile);
  char *optB=removeStructGccOptionsItem(deviceFile);
  char *optionsFieldRawNamesA=getFieldRawName(optA);
  char *optionsFieldRawNamesB=getFieldRawName(optB);
  //fprintf(stderr,"mtcsgenoptions 11 参数 optA:%s optB:%s optionsFieldRawNamesA:%s optionsFieldRawNamesB:%s\n",
       // optA,optB,optionsFieldRawNamesA,optionsFieldRawNamesB);
  fprintf(stderr,"mtcsgenoptions 参数 11 %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  //求交集
  char *commonFieldRawNames=intersection(optionsFieldRawNamesA,optionsFieldRawNamesB);
 // fprintf(stderr,"A与B的交集-- %s\n",commonFieldRawNames);
  char *commonOptionsItem=completionInterseactionFieldType(commonFieldRawNames,optB);//获取公共域的数据类型
  char *deviceDifferrence=setDifference(optionsFieldRawNamesB,commonFieldRawNames);
  char *deviceOptionsItem=completionInterseactionFieldType(deviceDifferrence,optB);//获取设备独有的域
  //fprintf(stderr,"mtcsOptoiions 集合 common与device之差--- %s\n",deviceDifferrence);
  fprintf(stderr,"mtcsgenoptions 参数 22 %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  NString *deviceEnumOpts=n_string_new("");
  fprintf(stderr,"mtcsgenoptions 参数 22aa %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  char *deviceOptRawName=createOptItem(devicePrefix,deviceFile,deviceEnumOpts);
  fprintf(stderr,"mtcsgenoptions 参数 22bb %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  NString *hostEnumOpts=n_string_new("");
  fprintf(stderr,"mtcsgenoptions 参数 22cc %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  char *hostOptRawName=createOptItem("",hostFile,hostEnumOpts);
  fprintf(stderr,"mtcsgenoptions 参数 22dd %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  char *deviceClOptions=createClOptions(devicePrefix,deviceFile,deviceDifferrence);//取options.cc中的const struct cl_option cl_options[] =
  fprintf(stderr,"mtcsgenoptions 参数 22ff %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  char *deviceMicro=getDeviceMicro(devicePrefix,deviceFile);
  fprintf(stderr,"mtcsgenoptions 参数 33 %s 是:%s %s %s\n",argv[1],argv[2],argv[3],argv[4]);

  if(strcmp(genFileType,"-h")==0){
     fprintf(stderr,"mtcsOptoiions 生成.h文件 00 %s %s %s\n",argv[2],argv[3],argv[4]);

      genCommonOptionsItemH(commonOptionsItem);//生成mtcsoptionsitem.h
      fprintf(stderr,"mtcsOptoiions 生成.h文件 11 %s %s %s\n",argv[2],argv[3],argv[4]);

  }else if(strcmp(genFileType,"-q")==0){
     fprintf(stderr,"mtcsOptoiions 生成-q文件 00aa %s %s %s\n",argv[2],argv[3],argv[4]);

      char *deviceClTargetOption=getDeviceClTargetOptions(deviceFile);
      genDeviceOptionH(devicePrefix,deviceOptionsItem,deviceEnumOpts->str,deviceMicro,deviceClTargetOption);//生成ptx-optionsitem.h
      fprintf(stderr,"mtcsOptoiions 生成-q文件 00bb %s %s %s\n",argv[2],argv[3],argv[4]);

  }else{
     fprintf(stderr,"mtcsOptoiions 生成.c文件 00aa %s %s %s\n",argv[2],argv[3],argv[4]);

      char *init=setInitValue("self",deviceFile,optionsFieldRawNamesB,commonFieldRawNames);
      char *commonOptRawName=intersection/*!setDifference*/(deviceOptRawName,hostOptRawName);
      char *deviceOptToHost=convertOptToHost(devicePrefix,commonOptRawName);
      char *hostOptToDevice=convertOptToDevice(devicePrefix,commonOptRawName);
      //生成 ptx_cl_optimization_save 三段内容
        char *optimizations=getClOptimization(hostFile);
        fprintf(stderr,"optimizations ---- %s\n",optimizations);
        char *inrange=getIN_RANGE(optimizations);
       fprintf(stderr,"inrange ---- %s\n",inrange);
        char *genInrange=genIN_RANGE(inrange);
       fprintf(stderr,"genInrange ---- %s\n",genInrange);
        char *gensave=genSave(optimizations);
        fprintf(stderr,"save ---- %s\n",gensave);
        char *genexplicitmask=genExplicitMask(optimizations);
        fprintf(stderr,"genexplicitmask ---- %s\n",genexplicitmask);
        NString *clOptimizationSave=n_string_new("");
        n_string_append(clOptimizationSave,genInrange);
        n_string_append(clOptimizationSave,gensave);
        n_string_append(clOptimizationSave,genexplicitmask);
        //生成函数 void ptx_cl_optimization_restore (struct gcc_options *opts, struct gcc_options *opts_set,struct cl_optimization *ptr)
        char *restoreOptimizations=genRestoreOptimizations(optimizations);
        fprintf(stderr,"genRestoreOptimizations ---- %s\n",restoreOptimizations);
        char *restoreExplicitMask=genRestoreExplicitMask(optimizations);
        fprintf(stderr,"genRestoreExplicitMask ---- %s\n",restoreExplicitMask);
        NString *clOptimizationRestore=n_string_new("");
        n_string_append(clOptimizationRestore,restoreOptimizations);
        n_string_append(clOptimizationRestore,restoreExplicitMask);
        //生成函数体void ptx_options_set_gcc_options(MtcsOptionsItem *deviceOpts,MtcsOptionsItem *deviceOptsSet,struct gcc_options *hostOpts,struct gcc_options *hostOptsSet);\n",devicePrefix);
        char *setOptionsStr=setOptions(commonFieldRawNames);
        //获取设德options.cc中的clenum内容
        int clLangCount=0;
        char *clEnum = getDeviceOptionCFileClEnum (deviceFile,devicePrefix,&clLangCount);

        genDeviceOptionC(devicePrefix,init,deviceClOptions,deviceOptToHost,hostOptToDevice,
              clOptimizationSave->str,clOptimizationRestore->str,setOptionsStr,clEnum,clLangCount);
  }
  fflush (stdout);
  return (ferror (stdout) != 0 ? FATAL_EXIT_CODE : SUCCESS_EXIT_CODE);
}

