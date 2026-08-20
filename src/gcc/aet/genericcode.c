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
#include <utime.h>


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
#include "ifaceimpl.h"
#include "genericgraph.h"
#include "blockmgr.h"
#include "genericutil.h"
#include "genericcode.h"


/**
 * 收集函数的信息，以便通过泛型单元，类名，块索引号找到对应的函数地址
 */
typedef struct _BlockFuncIndex
{
   char *funcName;  //泛型块函数名
   char *genericModel;//泛型定义 不是E 是具体的类型如 int
   char *sysName;  //所在类名
   int index; //泛型块在类中的序号，从0开始
}BlockFuncIndex;

typedef struct _GenericParams
{
    char **params;      /* 参数字符串数组 */
    int    count;       /* 参数个数 */
} GenericParams;


static char *replace_genericblock(const char *src,NPtrArray *blocks,int firstNumber);
static int extract_generic_params(const char *src, GenericParams *out);
static char *replaceFuncName(char *src,char *managleFuncName,char *genericMode);
static char *createFuncNameWithGb(char *managleFuncName,char *genericMode);

static void genericCodeInit(GenericCode *self)
{

}

static GenericInfo *getGenericInfo(NPtrArray *array,char *sysName)
{
   int i;
   for(i=0;i<array->len;i++){
      GenericInfo *item=n_ptr_array_index(array,i);
      if(strcmp(item->className->sysName,sysName)==0)
         return item;
   }
   return NULL;
}

/**
 * 格式化泛型单元
 * E_int_0_4
 * E 是单元声明 int 是实际类型 0 是指针数 4 是大小
 * 如果有多个单元用逗号分开
 * E_int_0_5,F_float_1_8 8刚好是指针大小。
 *  编译泛型块函数时的指令
 */
static char *formatGenericUnitForCompile(GenericObj *obj)
{
   int i;
   NString *codes=n_string_new("");
   //printf("formatGenericUnitForCompile --- %p %p %d\n",obj,obj->origModel,obj->infoLen);
   for(i=0;i<obj->infoLen;i++){
      RunGenericInfo *info=obj->infos[i];
      n_string_append_printf(codes,"%s_%s_%d_%d",obj->origModel->genUnits[i]->name,
      info->genUnit->name,info->genUnit->pointerCount,info->genUnit->size);
      if(i<obj->infoLen-1)
         n_string_append(codes,",");
   }
   return n_string_free(codes,FALSE);
}

/**
 * 创建建立块函数调用的索引信息
 * defineFuncUnits 泛型函数的泛型单元字符化 格式 如:_float_0
 * defineClassUnits 泛型类的泛型单元字会化  格式 如:_int_0
 */
static BlockFuncIndex *createBlockFuncIndex(char *funcName,char *sysName,
      char *defineFuncUnits,char *defineClassUnits,int index)
{
   BlockFuncIndex *f=n_slice_new0(BlockFuncIndex);
   NString *codes=n_string_new("");
   if(defineFuncUnits)
      n_string_append(codes,defineFuncUnits);
   if(defineClassUnits)
      n_string_append(codes,defineClassUnits);
   f->funcName=n_strdup(funcName);
   f->sysName=n_strdup(sysName);
   f->genericModel=n_string_free(codes,FALSE);
   f->index=index;
   return f;
}
/**
 * 泛型模型+原泛型块函数名组成新的函数名
 */
static char *formatGenericUnitForFuncName(GenericObj *obj)
{
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<obj->infoLen;i++){
      RunGenericInfo *info=obj->infos[i];
      char ret[128];
      generic_unit_create_block_func_prefix(info->genUnit->name,info->genUnit->pointerCount,ret);
      n_string_append(codes,ret);
   }
   return n_string_free(codes,FALSE);
}

/**
 * 用定义的泛型模型创建泛型块函数名
 */
static char *createFuncName(char *protoFunName,char *defineFuncUnits,char *defineClassUnits)
{
   NString *codes=n_string_new("");
   if(defineFuncUnits)
      n_string_append(codes,defineFuncUnits);
   if(defineClassUnits)
      n_string_append(codes,defineClassUnits);
   n_string_append(codes,protoFunName);
   return n_string_free(codes,FALSE);
}

/**
 * 创建泛型模型指令
 */
static char *createModelDirective(char *funcUnitsForCompile,char *classUnitsForCompile)
{
   NString *codes=n_string_new("");
   if(funcUnitsForCompile && classUnitsForCompile)
      n_string_append_printf(codes,"%s,%s",funcUnitsForCompile,classUnitsForCompile);
   else if(funcUnitsForCompile)
      n_string_append(codes,funcUnitsForCompile);
   else
      n_string_append(codes,classUnitsForCompile);
   return n_string_free(codes,FALSE);
}

/**
 * 代码如下:
 *  void _int_0_Abc__gen_block_func_0(Abc * self,aet_generic_T _aetGenNewParamPrefix_atcs)
 {
T atcs=*((T *)_aetGenNewParamPrefix_atcs);
 T x = acts * 5 ; printf ( "vvv :%d\n" , x ) ;
 paramsStr=Abc * self,aet_generic_T * _aetGenNewParamPrefix_atcs
 经过replaceParams 变成如下
 T atcs=*((T *)_aetGenNewParamPrefix_atcs);

 }
 */
static char * replaceParams(GenericObj *genObj,char *paramsStr)
{
   if(paramsStr==NULL)
      return NULL;
   //取出函数的参数
   char **items=n_strsplit(paramsStr,",",-1);
   int len=n_strv_length(items);
   int i,j;
   NString *restoreParam=n_string_new("");
   for(i=0;i<len;i++){
      char *param=items[i];
      //如果参数据类型是 aet_generic_E或T等
      if(param!=NULL && generic_util_start_with_generic(param)){
         int strSize=strlen(param);
         char *genericType= generic_util_get_start_with_generic(param);
         int pointer=0;
         for(j=strlen(genericType);j<strSize;j++){
            if(param[j] ==' ')
               continue;
            else if(param[j]=='*'){
               pointer++;
            }else
               break;
         }
         //获取变量名
         char varName[512];
         strncpy(varName,param+j,strSize-j);
         varName[strSize-j] ='\0';
         char *origName=generic_util_get_block_orig_param_name(varName);
         if(origName==NULL){
            n_error("泛型块函数中的参数:%s不是有效的名字，报告此错误！\n",varName);
            return NULL;
         }
           //printf("formatGenericUnitForCompile --- %p %p %d\n",obj,obj->origModel,obj->infoLen);

           for(j=0;j<genObj->infoLen;j++){
              RunGenericInfo *info=genObj->infos[j];
              char *gname=genObj->origModel->genUnits[j]->name;
              if(endswith(genericType,gname)){
                 n_string_append(restoreParam,gname);
                 n_string_append(restoreParam," ");
                 if(pointer==1)
                    n_string_append(restoreParam,"*");
                 else if(pointer==2)
                    n_string_append(restoreParam,"**");
                 else if(pointer==3)
                    n_string_append(restoreParam,"***");
                 n_string_append(restoreParam,origName);
                 n_string_append(restoreParam,"= ");
                 if(info->genUnit->pointerCount==0){
                     if(pointer==0)
                        n_string_append_printf(restoreParam,"*((%s *)%s)",gname,varName);
                     else
                        n_string_append_printf(restoreParam,"(%s *)%s",gname,varName);
                 }else{
                    n_string_append_printf(restoreParam,"(%s )%s",gname,varName);
                 }
                 n_string_append(restoreParam,";\n");
              }
             // printf("replaceParams info -- is:j:%d %s %s %d %d\n",
                 //   j,gname,info->genUnit->name,info->genUnit->pointerCount,info->genUnit->size);

           }

      }
   }
   if(restoreParam->len==0){
      n_string_free(restoreParam,TRUE);
      return NULL;
   }
   return n_string_free(restoreParam,FALSE);
}


/**
* 生成泛型块函数
* 重要的是两个地方
* 1.泛型模型指令:格式 泛型单元声明+定义的泛型单元+指针数+定义的泛型单元大小(如果有指针都是8)
* 2.用定义的泛型单元生成函数名 格式 _+定义的泛型单元+_+指针数
* E_float_0_4
  void _float_0_TFirst__gen_block_func_0(TFirst * self,int xe)
 {
    int f ;
  }
*/
static char *createBlockFuncCode(GenericObj *genObj,GenericBlock *block,char *newFuncName)
{
   NString *codes=n_string_new("");
   char *replace= replaceParams(genObj,block->parms);
   //printf("createBlockFuncCode return ---- %s\n",block->returnType);
   n_string_append(codes,"static inline ");
   n_string_append_printf(codes," %s ",block->returnType);
   n_string_append(codes,newFuncName);
   n_string_append_printf(codes,"(%s)\n",block->parms);
   n_string_append(codes," {\n");
   if(replace!=NULL)
      n_string_append(codes,replace);
   n_string_append_printf(codes,"%s\n",block->body);
   n_string_append(codes," }\n");
   n_debug("生成确定的泛型块函数 源代码是:\n%s\n",codes->str);
   return n_string_free(codes,FALSE);
}

/**
 * 泛型函数调用
 * 如果泛型函数中有块，块中的泛型单元可能来自泛型函数和所在泛型类中。
 * 所以生成的泛型块函数会多个。
 * 泛型块函数个数=泛型函数所在的类是泛型类=1*泛型对象 否则 =1
 * 比如：
 * 泛型函数的泛型模型是<E> =<int>
 * 泛型类是<F>=<float>或<F>=<char>...
 * 同一泛型块函数生成两次
 * (1)int,float
 * (2)int,char
 *
 */
static char *createCodesForGenfunc(GenericObj *funObj,GenericInfo *info,
      NPtrArray *classGenObjArray,NPtrArray *dependClassGenObj,NPtrArray *funcNameArray)
{
   int i,j;
   char *blockAtClassName=info->className->sysName;
   NPtrArray *funcContentArray=n_ptr_array_new();
   NString *codes=n_string_new("");

   char *genFunUnitsForCompile=formatGenericUnitForCompile(funObj);
   char *funcDefineUnits=formatGenericUnitForFuncName(funObj);//格式 name+"_"+pointercount+"_"
   nboolean havaeCodes=FALSE;
   if(!info->isGenericClass){
      //泛型函数所在的类不是泛型类
      char *directive=genFunUnitsForCompile;
      //重要标记，查找到块代码的指示
      n_string_append_printf(codes,"%s %d \n",RID_AET_GOTO_STR,GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC);
      n_string_append_printf(codes,"\"%s\"\n",directive);
      for(i=0;i<info->blocksCount;i++){
         GenericBlock *block=info->blocks[i];
         //块在泛型函数中 ，并且与genericobj的函数名相同。可以用泛型对象中的定义泛型单元来生成代码了。
         //如果块所在的类是泛型类，必能找到一个定义了泛型模型的泛型对象。如果找不到，是一个错误。
         //printf("where is ---xx00 %d %s %s\n",block->isFuncGeneric,block->belongFunc,funObj->callee->mangleFunName);

         if(block->isFuncGeneric && !strcmp(block->belongFunc,funObj->callee->mangleFunName)){
            char *newFuncName=createFuncName(block->name,funcDefineUnits,NULL);
            char *funcdefine=createBlockFuncCode(funObj,block,newFuncName);
            printf("createCodesForGenfunc 00 %s -- %s\n",newFuncName,funcdefine);
            n_string_append(codes,funcdefine);
            n_string_append(codes,"\n");
            gcc_assert(block->index==i);
            BlockFuncIndex *bi=createBlockFuncIndex(newFuncName,blockAtClassName,funcDefineUnits,NULL,block->index);
            n_ptr_array_add(funcNameArray,bi);
            n_free(funcdefine);
            havaeCodes=TRUE;
         }
      }
   }else{
      for(i=0;i<info->blocksCount;i++){
         GenericBlock *block=info->blocks[i];
         //块在泛型函数中 ，并且与泛型对象的函数名相同。可以用泛型对象中的定义泛型单元来生成代码了。
         //如果块所在的类是泛型类，必能找到一个定义了泛型模型的泛型对象。如果找不到，是一个错误。
        // printf("where is ---00 %d %s %s\n",block->isFuncGeneric,block->belongFunc,funObj->callee->mangleFunName);
         if(block->isFuncGeneric && !strcmp(block->belongFunc,funObj->callee->mangleFunName)){
            nboolean find=FALSE;
            for(j=0;j<classGenObjArray->len;j++){
               GenericObj *classObj=n_ptr_array_index(classGenObjArray,j);
               //printf("where is ---11 %d %s %s\n",j,classObj->newObject->className.sysName,blockAtClassName);
               if(!strcmp(classObj->newObject->className.sysName,blockAtClassName)){
                  find=TRUE;
                  char *classObjUnitForCompile=formatGenericUnitForCompile(classObj);
                  char *defineClassObjUnits=formatGenericUnitForFuncName(classObj);
                  char *directive=createModelDirective(genFunUnitsForCompile,classObjUnitForCompile);
                  n_string_append_printf(codes,"%s %d \n",RID_AET_GOTO_STR,GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC);
                  n_string_append_printf(codes,"\"%s\"\n",directive);
                  char *newFuncName=createFuncName(block->name,funcDefineUnits,defineClassObjUnits);
                  char *funcdefine=createBlockFuncCode(funObj,block,newFuncName);
                  n_string_append(codes,funcdefine);
                  n_string_append(codes,"\n");
                  gcc_assert(block->index==i);
                  BlockFuncIndex *bi=createBlockFuncIndex(newFuncName,
                        blockAtClassName,funcDefineUnits,defineClassObjUnits,block->index);
                  n_ptr_array_add(funcNameArray,bi);
                  n_ptr_array_add(dependClassGenObj,classObj);
                  n_free(classObjUnitForCompile);
                  n_free(defineClassObjUnits);
                  n_free(funcdefine);
                  havaeCodes=TRUE;
               }
            }
            if(!find){
               n_error("块在泛型函数中，泛型函数在泛型类中。但无定义的泛型类的泛型对象。%s\n",block->belongFunc,blockAtClassName);
            }
         }
      }
   }

   n_free(genFunUnitsForCompile);
   n_free(funcDefineUnits);
   if(havaeCodes)
      return n_string_free(codes,FALSE);
   else{
      n_string_free(codes,TRUE);
      return NULL;
   }
}

/**
 * 用泛型对象中的泛型模型生成泛型块函数代码。
 * GenericInfo中有泛型块原型。
 * 排除在泛型类中的泛型函数中的块。
 * E_float_0_4
  void _float_0_TFirst__gen_block_func_0(TFirst * self,int xe)
 {
    int f ;
  }
 */
static char *createCodesForClass(GenericObj *classGenObj,GenericInfo *info,NPtrArray *funcNameArray)
{
   int i;
   char *blockAtClassName=info->className->sysName;
   NPtrArray *classCodeArray=n_ptr_array_new();
   char *genClassUnitsForCompile=formatGenericUnitForCompile(classGenObj);
   char *classDefineUnits=formatGenericUnitForFuncName(classGenObj);//格式 name+"_"+pointercount+"_"
   char *directive=genClassUnitsForCompile;
   NString *codes=NULL;
   for(i=0;i<info->blocksCount;i++){
      GenericBlock *block=info->blocks[i];
      if(block->isFuncGeneric)
         continue;
      if(codes==NULL){
         codes=n_string_new("");
         //生成 aet_goto_compile$ 16
         n_string_append_printf(codes,"%s %d \n",RID_AET_GOTO_STR,GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC);
         //生成"E_int_0_4"
         n_string_append_printf(codes,"\"%s\"\n",directive);
      }
      char *newFuncName=createFuncName(block->name,NULL,classDefineUnits);
      char *blockFuncCode=createBlockFuncCode(classGenObj,block,newFuncName);
      n_string_append_printf(codes,"%s\n\n",blockFuncCode);
      gcc_assert(block->index==i);
      BlockFuncIndex *bi=createBlockFuncIndex(newFuncName,blockAtClassName,classDefineUnits,NULL,block->index);
      n_ptr_array_add(funcNameArray,bi);
   }
   n_free(genClassUnitsForCompile);
   n_free(classDefineUnits);
   if(codes)
      return n_string_free(codes,FALSE);
   else
      return NULL;
}


/**
 * 头文件是否存在
 */
static nboolean existsHeaderFile(NPtrArray *headerFileArray,char *file)
{
   int i,j;
   for(i=0;i<headerFileArray->len;i++){
      char *headerFile=n_ptr_array_index(headerFileArray,i);
      if(!strcmp(headerFile,file))
         return TRUE;
   }
   return FALSE;
}

/////////////////------------------开始新版------------------------
/**
 * 比较两个对象的泛型定义是否相同
 */
static nboolean isSameRunGenInfo(GenericObj *a,GenericObj *b)
{
   if(!a || !b)
      return FALSE;
   if(a->infoLen!=b->infoLen)
      return FALSE;
   int i;
   for(i=0;i<a->infoLen;i++){
      RunGenericInfo *ar=a->infos[i];
      RunGenericInfo *br=b->infos[i];
      if(!generic_unit_equal(ar->genUnit,br->genUnit))
         return FALSE;
   }
   return TRUE;
}

/**
 * 生成
 * static BlockFuncData _bfi_value_0[]={
    {(void*)_int_0_debug_ASecond__gen_block_func_0,"_int_0_debug_ASecond__gen_block_func_0","_int_0","debug_ASecond",0}};
 */
static void genBlockAddressVar(NPtrArray *funcNameArray,NPtrArray *blockFuncArressArray)
{
   int i;
   for(i=0;i<funcNameArray->len;i++){
      BlockFuncIndex *b=n_ptr_array_index(funcNameArray,i);
      char *address = n_strdup_printf("{(void*)%s,\"%s\",\"%s\",\"%s\",%d}",
            b->funcName,b->funcName,b->genericModel,b->sysName,b->index);
      n_ptr_array_add(blockFuncArressArray,address);
   }
}

typedef struct _CompileFile
{
   char *fileName;//new 对象和调用泛型函数的所在.c源文件
   char *oFile;//.c源文件的输出源文件
   //块函数定义
   NPtrArray *blockFuncArray;
   //带泛型块函数的定义
   NPtrArray *funcWithGbArray;
   //块函数的地址
   //{(void*)_int_0_TFirst__gen_block_func_0,"_int_0_TFirst__gen_block_func_0","_int_0","TFirst",0}
   NPtrArray *blockFuncAddressArray;
   //带泛型块函数的地址
   NPtrArray *funcWithGbAddressArray;
   NPtrArray *headerArray;//头文件
   GenericInfo *infos[50];//一个文件编译単元中关联的GenericInfo
   int infoCount;
}CompileFile;

static void addHeader(CompileFile *cf,GenericObj *obj)
{
   int i,j;
   // printf("createHeader---- %s\n",obj->declClassFile);
   if(obj->declClassFile!=NULL &&
   !existsHeaderFile(cf->headerArray,obj->declClassFile)
   && endswith(obj->declClassFile,".h")){
      n_ptr_array_add(cf->headerArray,obj->declClassFile);
   }
   for(j=0;j<obj->infoLen;j++){
      RunGenericInfo *info=obj->infos[j];
      if(info->file!=NULL && !existsHeaderFile(cf->headerArray,info->file) && endswith(info->file,".h")){
         n_ptr_array_add(cf->headerArray,info->file);
      }
   }
}

static void addGenInfo(CompileFile *cf,GenericInfo *info)
{
    int i;
    for(i=0;i<cf->infoCount;i++){
       if(cf->infos[i]==info)
          return;
    }
    cf->infos[cf->infoCount++] = info;
}


/**
 *生成带泛型块的真实函数
 */
static char *genFuncWithGbCode(GenericInfo *info,NPtrArray *blockIndexArray)
{
   char *sysName= info->className->sysName;
   int i,j;
   //类中带泛型块的函数
   FuncWithGbData *datas[100];
   int count=generic_parser_get_func(generic_parser_get(),sysName,datas);
   if(count==0)
      return NULL;
   NString *codes=n_string_new("");
   for(i=0;i<count;i++){
      FuncWithGbData *item = datas[i];
      char *code=item->code;
      BlockFuncIndex *b=n_ptr_array_index(blockIndexArray,0);
      char *newfunc = replace_genericblock(code,blockIndexArray,item->firstNumber);
      char *replace = replaceFuncName(newfunc,item->mangleFunName,b->genericModel);
      n_string_append(codes,"\n");
      n_string_append(codes,replace);
      n_string_append(codes,"\n");
      //printf("genFuncWithGbCode 11 %s %s %s\n",b->genericModel,item->mangleFunName,replace);
      free(newfunc);
      free(replace);
   }
   if(codes->len==0){
      n_string_free(codes,TRUE);
      return NULL;
   }else
      return n_string_free(codes,false);
}

/**
 * 生成带泛型块函数的变量
 * static BlockFuncData _fwgd_value_[]={
   {(void*)_int_0__Z7ASecond4pushEPN7ASecondEPv,"_int_0__Z7ASecond4pushEPN7ASecondEPv","_int_0","debug_ASecond",0},
   {(void*)_int_0__Z7ASecond4setaEPN7ASecondEPv,"_int_0__Z7ASecond4setaEPN7ASecondEPv","_int_0","debug_ASecond",1},
};
 *funcCount在类中的带泛型块数据的数量
 */
static void genFuncWithGbAddress(GenericInfo *info,NPtrArray *blockIndexArray,NPtrArray *funcWithGbAddressArray)
{
   char *sysName= info->className->sysName;
   int i,j;
   //类中带泛型块的函数结构体，count是个有多少个
   FuncWithGbData *datas[100];
   int count=generic_parser_get_func(generic_parser_get(),sysName,datas);
   if(count==0)
      return NULL;
   for(i=0;i<count;i++){
      FuncWithGbData *item = datas[i];
      BlockFuncIndex *b=n_ptr_array_index(blockIndexArray,0);
      char *newFuncName = createFuncNameWithGb(item->mangleFunName,b->genericModel);
      char *address = n_strdup_printf("{(void*)%s,\"%s\",\"%s\",\"%s\",%d}",
            newFuncName,newFuncName,b->genericModel,sysName,i);
      free(newFuncName);
      n_ptr_array_add(funcWithGbAddressArray,address);
   }
}

static void genClassCodes(CompileFile *cf,GenericObj *classGenObj)
{
   NPtrArray *genInfoArray=block_mgr_get_output_generic_info(block_mgr_get());
   n_debug("genericcode.c genClassCodes 00 %s\n",classGenObj->newObject->className.sysName);
   GenericInfo *info=getGenericInfo(genInfoArray,classGenObj->newObject->className.sysName);
   if(info==NULL){
//      printf("genClassCodes 00 %p\n",classGenObj);
//      printf("genClassCodes 11 %p\n",classGenObj->newObject);
//      printf("genClassCodes 22 %p\n",classGenObj->newObject->className.sysName);
      n_debug("genericcode.c genClassCodes 类%s中没有泛型块。%s\n",classGenObj->newObject->className.sysName);
      return;
   }
   n_debug("genericcode.c genClassCodes 00 普通函数中的块 genObj:%p info:%p\n",
         classGenObj,info);
   n_debug("genericcode.c genClassCodes 11 普通函数中的块 className:%s  atClassFunc:%p oFile:%s\n",
        classGenObj->newObject->className.sysName,classGenObj->atFunc,info->oFile);
   //收集生成的新函数名
   NPtrArray *funcNameArray=n_ptr_array_new();
   //1.生成泛型块代码
   char *codes=createCodesForClass(classGenObj,info,funcNameArray);
   n_ptr_array_add(cf->blockFuncArray,codes);
   //2.生成泛型块函数地址。
   genBlockAddressVar(funcNameArray,cf->blockFuncAddressArray);
   char *funcWithGbCodes=genFuncWithGbCode(info,funcNameArray);
   if(funcWithGbCodes){
      n_ptr_array_add(cf->funcWithGbArray,funcWithGbCodes);
      genFuncWithGbAddress(info,funcNameArray,cf->funcWithGbAddressArray);
   }
   n_ptr_array_unref(funcNameArray);
   //3.加头文件
   addHeader(cf,classGenObj);
   //4.加info
   addGenInfo(cf,info);
   //printf("生成的代码如下:-------------------\n");
  // printf("泛型块函数:\n%s\n",codes);
   //printf("带泛型块函数:\n%s\n",funcWithGbCodes);
}

/**
 * 获取一个源文件对应的一个CompileFile，如果不存在创建新的
 */
static CompileFile *createCompileFile(char *cFile,char *oFile)
{
   CompileFile *f=n_slice_new0(CompileFile);
   f->fileName=cFile;
   f->oFile = oFile;
   f->blockFuncArray=n_ptr_array_new();
   f->funcWithGbArray=n_ptr_array_new();
   f->blockFuncAddressArray=n_ptr_array_new();
   f->funcWithGbAddressArray=n_ptr_array_new();
   f->headerArray = n_ptr_array_new();
   f->infoCount = 0;
   return f;
}

/**
 * 返回输出的.o文件
 */
static void genGenericFuncCodes(CompileFile *cf,GenericObj *funcGenObj,NPtrArray *classGenObjArray)
{
   NPtrArray *genInfoArray=block_mgr_get_output_generic_info(block_mgr_get());
   GenericInfo *info=NULL;
   char *sysName=funcGenObj->callee->className->sysName;
   info=getGenericInfo(genInfoArray,sysName);
   if(info==NULL){
      n_debug("genericcode.c genGenericFuncCodes 类%s中没有泛型块。%s cf:%s\n",sysName,cf->fileName);
      return;
   }
   //收集所有引用到的泛型对象。
   NPtrArray *dependClassGenObjArray=n_ptr_array_new();
   //收集所有生成的新函数名
   NPtrArray *funcNameArray=n_ptr_array_new();
   char  *codes=createCodesForGenfunc(funcGenObj,info,classGenObjArray,dependClassGenObjArray,funcNameArray);
   n_ptr_array_add(cf->blockFuncArray,codes);
   //2.生成泛型块函数地址。
   genBlockAddressVar(funcNameArray,cf->blockFuncAddressArray);
   //3.加头文件
   addHeader(cf,funcGenObj);
   //4.加info
   addGenInfo(cf,info);
  // printf("生成的泛型函数代码如下:-------------------\n");
  // printf("泛型函数代码:\n%s\n",codes);
   n_ptr_array_unref(funcNameArray);
   n_ptr_array_unref(dependClassGenObjArray);
}

static char *genHeaderStr(CompileFile *cf)
{
   NString *codes=n_string_new("\n");
   int i;
   for(i=0;i<cf->infoCount;i++){
      n_string_append(codes,cf->infos[i]->includeStr);
      n_string_append(codes,"\n");
   }
   for(i=0;i<cf->headerArray->len;i++){
      n_string_append_printf(codes,"#include \"%s\"\n",n_ptr_array_index(cf->headerArray,i));
   }
   n_string_append(codes,"\n");
   return n_string_free(codes,FALSE);
}

/**
 * 生成最终代码，每个编译单元一个,并写入文件 _block_func__0.o
 * 返回三段文件 泛型块+带泛型块函数源代码，编译源代码，编译源代码的输出文件
 */
static char * createFinalCodes( NPtrArray *compileFileArray)
{
   char *fileName = getenv("GCC_AET_BLOCK_LIST_PATH");
   NFile *f=n_file_new(fileName);
   NFile *parent=n_file_get_parent_file(f);
   NFile *canon=n_file_get_canonical_file(parent);
   const char *parentPath=n_file_get_absolute_path(canon);

   int i,j;
   NString *listFile=n_string_new("");
   for(i=0;i<compileFileArray->len;i++){
      CompileFile *cf=n_ptr_array_index(compileFileArray,i);
      NString *codes=n_string_new("");
      n_string_append_printf(codes,"/*为文件 %s 生成泛型代码。*/\n",cf->fileName);
      /*
      char *header=genHeaderStr(cf);
      n_string_append(codes,header);
      free(header);
      */
      for(j=0;j<cf->blockFuncArray->len;j++){
         char *code=n_ptr_array_index(cf->blockFuncArray,j);
         n_string_append(codes,code);
         n_string_append(codes,"\n");
      }
      n_string_append(codes,"\n");
      n_string_append(codes,"static BlockFuncData _bfi_value[]={\n");
      for(j=0;j<cf->blockFuncAddressArray->len;j++){
         char *code=n_ptr_array_index(cf->blockFuncAddressArray,j);
         //{(void*)_int_0_TFirst__gen_block_func_0,"_int_0_TFirst__gen_block_func_0","_int_0","TFirst",0}
         n_string_append(codes,"\t");
         n_string_append(codes,code);
         if(j==cf->blockFuncAddressArray->len-1)
            n_string_append(codes,"\n");
         else
            n_string_append(codes,",\n");
      }
      n_string_append(codes,"};\n\n");
      n_string_append(codes,"static __attribute__((constructor(1))) void load_generic_data()\n");
      n_string_append(codes,"{\n");
      n_string_append_printf(codes,"\tadd_generic_data(_bfi_value,%d);\n",cf->blockFuncAddressArray->len);
      n_string_append(codes,"}\n");

      if(cf->funcWithGbArray->len>0){
         for(j=0;j<cf->funcWithGbArray->len;j++){
            char *code=n_ptr_array_index(cf->funcWithGbArray,j);
            n_string_append(codes,code);
            n_string_append(codes,"\n");
         }

         n_string_append(codes,"\n");
         n_string_append(codes,"static BlockFuncData _fwgb_value_[]={\n");
         for(j=0;j<cf->funcWithGbAddressArray->len;j++){
            char *code=n_ptr_array_index(cf->funcWithGbAddressArray,j);
            //{(void*)_int_0__Z7ASecond4pushEPN7ASecondEPv,"_int_0__Z7ASecond4pushEPN7ASecondEPv","_int_0","debug_ASecond",0},
            n_string_append(codes,"\t");
            n_string_append(codes,code);
            if(j==cf->funcWithGbAddressArray->len-1)
               n_string_append(codes,"\n");
            else
               n_string_append(codes,",\n");
         }
         n_string_append(codes,"};\n\n");
         n_string_append(codes,"static __attribute__((constructor(1))) void load_func_with_gb_data()\n");
         n_string_append(codes,"{\n");
         n_string_append_printf(codes,"\tadd_func_with_gb_data(_fwgb_value_,%d);\n",cf->funcWithGbAddressArray->len);
         n_string_append(codes,"}\n");
      }
      //printf("源文件的代码:%s\n%s\n",cf->fileName,codes->str);

      char saveFile[256];
      sprintf(saveFile,"%s/%s_%d.o",parentPath,GENERIC_BLOCK_FILE_NAME,i);
      FILE *fp=fopen(saveFile,"w");
      fwrite(codes->str,1,codes->len,fp);
      fclose(fp);
      n_string_free(codes,TRUE);
      n_string_append_printf(listFile,"%s,%s,%s\n",saveFile,cf->fileName,cf->oFile);
   }
   return n_string_free(listFile,FALSE);
}


/**
 * 每个编译文件都有一套源代码
 * 这是new对象
 * rootArray 中的 GenericObj有调用new Object或call genericfunc或父组件设泛型类型
 * 多个GnericObj可能是同一个sourcefile
 */
static CompileFile *createCodeForSingleFile(GraphData *graphData,
      NPtrArray *classGenObjArray,NPtrArray *genFuncGenObjArray)
{
   //这是每个编译单元的新建对象和调用泛型函数的具体泛型
   int i;
   CompileFile *cf=createCompileFile(graphData->cFile,graphData->oFile);
   n_debug("createCodeForSingleFile 为文件生成泛型块函数和fwgb函数源代码:%s classObj:%d genFuncObj:%d\n",
         graphData->cFile,classGenObjArray->len,genFuncGenObjArray->len);
   for(i=0;i<classGenObjArray->len;i++){
      GenericObj *classGenObj=n_ptr_array_index(classGenObjArray,i);
     // generic_obj_print(classGenObj);
      genClassCodes(cf,classGenObj);
   }

   for(i=0;i<genFuncGenObjArray->len;i++){
      GenericObj *funcGenObj=n_ptr_array_index(genFuncGenObjArray,i);
      //printf("打印具体的泛型函数000 %d\n",i);
      //generic_obj_print(funcGenObj);
      genGenericFuncCodes(cf,funcGenObj,classGenObjArray);
   }
   return cf;
}

/**
 * 调用该方法时处理编译temp_func_track_45.c
 * 生成编译单元
 * 1.一个类一个编译单元，如果类不是泛型类，但有泛型函数，也以类名生成编译单元。
 * 从 generic_graph_read.c generic_graph_ready 方法输出泛型对象。
 * 生成的函数如下："E_float_0_4"是指令。
 * "E_float_0_4"
 void _float_0_TFirst__gen_block_func_0(TFirst * self,int xe)
 {
 int f ;
 }
 *生成查找函数的辅助信息如下:
 *1. _float_0_int_1..
 *2.类名 TFirst
 *3.索引号
 *把生成的块函数源代码所在的源文件，还有块函数源文件的编译所依赖的项目文件，项目文件的输出.o文件
 *三者存入fileName.o文件，供aetcollect.c中进行第二次编译泛型块函数编译 。
 */
void generic_code_create_block_codes(GenericCode *self)
{
   //整个项目有泛型块文件列表的文件名。GCC_AET_BLOCK_LIST_PATH
   //生成的结果是 block,.c,.o格式的字符串
   //来自aetcollect
   //sprintf(blockListFileParam,"-Daetblocklist%s",strlen(blockfiles)>0?blockListFileName:"");
   char *fileName = getenv("GCC_AET_BLOCK_LIST_PATH");
   if(fileName==NULL){
      //n_error("进入这里不应该没GCC_AET_BLOCK_LIST_PATH文件名");
      return;
   }
   char saveFile[512];
   //这是与aetcollect的协议 在ifaceimpl.c中也有类似
   sprintf(saveFile,"%s.o",fileName);
   n_debug("generic_code_create_block_codes 11 saveFile:%s\n",saveFile);

   NPtrArray *graphArray=generic_graph_files_graph(generic_graph_get());
   //没有genericobj对象
   if(!graphArray || graphArray->len==0){
      remove(saveFile);
      return;
   }
   NPtrArray *genInfoArray=block_mgr_get_output_generic_info(block_mgr_get());
   //没有泛型块
   if(genInfoArray==NULL || genInfoArray->len==0){
      remove(saveFile);
      return;
   }
   n_debug("generic_code_create_block_codes 22 saveFile:%s\n",saveFile);
   NPtrArray *cfArray=n_ptr_array_new();
   int i,j;
   for(i=0;i<graphArray->len;i++){
      GraphData *item=n_ptr_array_index(graphArray,i);
      //现在我们有了定义的泛型对象和块代码，把块中的泛型单元替换为定义的泛型单元
      //并生成新函数代码
      //genObjArray分成泛型对象与泛型函数两个，分别处理
      NPtrArray *genObjArray = item->out;
      NPtrArray *classGenObjArray=n_ptr_array_new();
      NPtrArray *funcGenObjArray=n_ptr_array_new();
      for(j=0;j<genObjArray->len;j++){
         GenericObj *obj=n_ptr_array_index(genObjArray,j);
         if(obj->type==GEN_FUNC)
            n_ptr_array_add(funcGenObjArray,obj);
         else
            n_ptr_array_add(classGenObjArray,obj);

      }
      CompileFile *cf=createCodeForSingleFile(item,classGenObjArray,funcGenObjArray);
      n_ptr_array_add(cfArray,cf);
   }
   char *listFile = createFinalCodes(cfArray);
   FILE *fp=fopen(saveFile,"w");
   fwrite(listFile,1,strlen(listFile),fp);
   fclose(fp);
}

/**
 * 通过 unit index获取对应的块函数名
 * 每个泛型真实泛型类型对应一个NPtrArray
 * unit可能有1到多个真实泛型类型
 */
static char *getGenericBlockFuncName(NPtrArray *blocks,int index)
{
   int i;
   for(i=0;i<blocks->len;i++){
      BlockFuncIndex *b=n_ptr_array_index(blocks,i);
      if(b->index==index){
         return b->funcName;
      }
   }
   return NULL;
}

//创建带泛型块的函数的新名字
static char *createFuncNameWithGb(char *managleFuncName,char *genericMode)
{
   char *str=xmalloc(512);
   sprintf(str,"%s_%s",genericMode,managleFuncName);
   return str;
}

/**
 * 替换函数名
 * managleFuncName 函数混淆名
 * src像这样 void push ( E ab ) {... 来自genericparser.c中对源函数代码token化后字符串
 * genericMode:定义的泛型 如 _int_0
 */
static char *replaceFuncName(char *src,char *managleFuncName,char *genericMode)
{
   char sysName[512]; //类名
   char funcName[512];//原始函数名
   int ret=func_mgr_get_orig_func_and_class_name(func_mgr_get(),managleFuncName,sysName,funcName);
   if(!ret){
      n_error("不是有效的函数名，报告此错误。%s\n",managleFuncName);
      return NULL;
   }
   NString *re=n_string_new(src);
   char haveparm[512];
   sprintf(haveparm,"%s ( )",funcName);
   int indexof = n_string_indexof(re,haveparm);
   char find[512];
   sprintf(find,"%s (",funcName);
   char *newFuncName = createFuncNameWithGb(managleFuncName,genericMode);
   char replace[1024];
   if(indexof<0)
       sprintf(replace,"%s (%s *self,",newFuncName,sysName);
   else
      sprintf(replace,"%s (%s *self",newFuncName,sysName);
   n_string_replace(re,find,replace,1);
   static const char *specs="static ";
   n_string_insert_len(re,0,specs,strlen(specs));//zclei
   free(newFuncName);
   return n_string_free(re,FALSE);
}


///替换genericblock$
/* ======================== 提取 generic$(...) 参数 ======================== */


/* 判断是否是标识符字符 */
static int isIdchar(char c)
{
    return ISALNUM((unsigned char)c) || c == '_' || c == '$';
}

/* 跳过空白 */
static const char *skip_space(const char *p)
{
    while (*p && ISSPACE((unsigned char)*p))
       p++;
    return p;
}

/*
 * 从 pos 开始找到与之匹配的 };
 * 返回指向 }; 后面那个字符的指针，失败返回 NULL
 * 会正确处理嵌套大括号
 */
static const char *find_matching_end(const char *pos)
{
   int brace = 0;
   const char *p = pos;
   while (*p) {
      if (*p == '{') {
         brace++;
         p++;
      }else if (*p == '}') {
         brace--;
         p++;
         if (brace == 0) {
            /* 已经匹配到最外层 }，看后面是不是 ; */
            p = skip_space(p);
            if (*p == ';')
               return p + 1;   /* 指向 ; 后面 */
            else
               return NULL;    /* 没有 ;，格式不对 */
         }
      }else {
         p++;
      }
   }
   return NULL;   /* 没找到匹配的 */
}

/**
 * 泛型块被调用函数替换
 */
static char *createCall(char *funcName,GenericParams *gp)
{
   char *re=xcalloc(1,1024);
   strcat(re,funcName);
   if(gp->count>0)
      strcat(re,"(self,");
   else
      strcat(re,"(self");
   int i;
   for(i=0;i<gp->count;i++){
      strcat(re,gp->params[i]);
      if(i<gp->count-1)
        strcat(re,",");
   }
   strcat(re,");");
   return re;
}

/*
 * 把所有 genericblock$ ... }; 替换成 replacement
 * 返回新分配的字符串，调用者负责 free
 */
static char *replace_genericblock(const char *src,NPtrArray *blocks,int firstNumber)
{
   size_t src_len = strlen(src);
   /* 预估结果长度（给足空间） */
   size_t capacity = 5*src_len + 1;
   char *result = (char *)xmalloc(capacity);
   if (!result)
      return NULL;
   int show=0;
   size_t out = 0;               /* 结果当前写入位置 */
   const char *p = src;
   while (*p) {
      /* 尝试匹配 "genericblock$" */
      if (strncmp(p, "genericblock$", 13) == 0 && !isIdchar(p[13])) {          /* 确保是完整单词 */

         const char *start = p;
         const char *end = find_matching_end(p + 13);
         if (end) {
            /* 找到完整块，写入替换字符串 */
            int len=strlen(start)-strlen(end);
            char header[len+1];
            memcpy(header,start,len);
            header[len]='\0';
            /* 测试参数提取 */
            GenericParams gp;
            if (!extract_generic_params(header, &gp) == 0) {
               n_error("替换泛型块出错 %s\n",src);
               return 0;
            }
            char *funcName = getGenericBlockFuncName(blocks,firstNumber+show);
            char *replace = createCall(funcName,&gp);
            int rep_len=strlen(replace);
            memcpy(result + out, replace, rep_len);
            out += rep_len;
            show++;
            free(replace);
            p = end;   /* 跳过整个 genericblock$ ... }; */
            continue;
         }
         /* 没找到匹配的 };，就当普通字符处理 */
      }
      result[out++] = *p++;
   }

   result[out] = '\0';
   return result;
}


/* 释放参数 */
static void free_generic_params(GenericParams *gp)
{
    if (!gp) return;
    for (int i = 0; i < gp->count; i++)
        free(gp->params[i]);
    free(gp->params);
    gp->params = NULL;
    gp->count = 0;
}

/*
 * 从字符串中提取 generic$(parm1, parm2, ...) 的参数
 * 成功返回 0，并把结果放在 out 里（需要调用 free_generic_params 释放）
 * 失败返回 -1
 * 支持嵌套括号，例如：generic$(a, func(b,c), d)
 */
static int extract_generic_params(const char *src, GenericParams *out)
{
   if (!src || !out)
      return -1;
   memset(out, 0, sizeof(*out));
   const char *p = src;
   /* 查找 "generic$" */
   while (*p) {
      if (strncmp(p, "genericblock$", 13) == 0 && !isIdchar(p[13])) {
         p += 13;
         p = skip_space(p);
         if (*p == '(') {
            p++;   /* 跳过 '(' */
            break;
         }
      }
      p++;
   }

   if (*p == '\0')
      return -1;   /* 没找到 generic$( */

   /* 现在 p 指向第一个参数开始的位置 */
   const char *arg_start = p;
   int paren = 1;               /* 已经进入一层括号 */
   int capacity = 8;
   out->params = (char **)xmalloc(capacity * sizeof(char *));
   if (!out->params)
      return -1;

   while (*p && paren > 0) {
      if (*p == '(') {
         paren++;
         p++;
      } else if (*p == ')') {
         paren--;
         if (paren == 0) {
            /* 到达最外层的 ')'，收集最后一个参数 */
            const char *arg_end = p;
            /* 去掉尾部空白 */
            while (arg_end > arg_start && ISSPACE((unsigned char)arg_end[-1]))
               arg_end--;

            if (arg_end > arg_start) {
               if (out->count >= capacity) {
                  capacity *= 2;
                  char **tmp = (char **)xrealloc(out->params, capacity * sizeof(char *));
                  if (!tmp) {
                     free_generic_params(out);
                     return -1;
                  }
                  out->params = tmp;
               }
               out->params[out->count++] = xstrndup(arg_start, arg_end - arg_start);
            }
            break;
         }
         p++;
      } else if (*p == ',' && paren == 1) {
         /* 最外层逗号，说明一个参数结束 */
         const char *arg_end = p;
         while (arg_end > arg_start && ISSPACE((unsigned char)arg_end[-1]))
            arg_end--;

         if (out->count >= capacity) {
            capacity *= 2;
            char **tmp = (char **)xrealloc(out->params, capacity * sizeof(char *));
            if (!tmp) {
               free_generic_params(out);
               return -1;
            }
            out->params = tmp;
         }
         out->params[out->count++] = xstrndup(arg_start, arg_end - arg_start);

         p++;
         p = skip_space(p);
         arg_start = p;   /* 下一个参数开始 */
      } else {
         p++;
      }
   }

   if (paren != 0) {          /* 括号不匹配 */
      free_generic_params(out);
      return -1;
   }

   return 0;
}

GenericCode *generic_code_get()
{
   static GenericCode *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(GenericCode));
      genericCodeInit(singleton);
   }
   return singleton;
}

