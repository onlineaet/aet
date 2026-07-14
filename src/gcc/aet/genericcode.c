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

//用编译单元生成编译文件。
typedef struct _CompileUnit CompileUnit;
struct _CompileUnit
{
   GenericInfo *info;
   NString *codes;
   NPtrArray *genObjArray; //创建代码 #include ...
   //生成代码 BlockFuncIndex
   //static BlockFuncData _bfi_value_0[]={
   //{(void*)_float_0_TFirst__gen_block_func_1,"_float_0_TFirst__gen_block_func_1","_float_0","TFirst",1}};
   NPtrArray *blockIndexArray;
   CompileUnit *merges[20];
   int mergeCount;
};

/**
 * 收集函数的信息，以便通过泛型单元，类名，块索引号找到对应的函数地址
 */
typedef struct _BlockFuncIndex
{
   char *funcName;
   char *genericModel;
   char *sysName;
   int index;
}BlockFuncIndex;


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
static BlockFuncIndex *createBlockFuncIndex(char *funcName,char *sysName,char *defineFuncUnits,char *defineClassUnits,int index)
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
   char **items=n_strsplit(paramsStr,",",-1);
   int len=n_strv_length(items);
   int i,j;
   NString *restoreParam=n_string_new("");
   for(i=0;i<len;i++){
      char *param=items[i];
      if(param!=NULL && generic_util_start_with_generic(param)){
         int strSize=strlen(param);
         char *genericType= generic_util_get_start_with_generic(param);
         int pointer=0;
         for(j=strlen(genericType);i<strSize;j++){
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
         char *origName=generic_util_get_orig_param_name(varName);
         if(origName==NULL){
            n_error("泛型块函数中的参数:%s不是有效的名字，报告此错误！\n",varName);
            return;
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
                 n_string_append(restoreParam,"=");
                 if(info->genUnit->pointerCount==0){
                     if(pointer==0)
                        n_string_append_printf(restoreParam,"*((%s *)%s)",gname,varName);
                     else
                        n_string_append_printf(restoreParam,"(%s *)%s",gname,varName);

                 }else{
                    n_string_append_printf(restoreParam,"(%s *)%s",gname,varName);

                 }
                 n_string_append(restoreParam,";\n");
              }
              n_debug("replaceParams info -- is:j:%d %s %s %d %d\n",
                    j,gname,info->genUnit->name,info->genUnit->pointerCount,info->genUnit->size);

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
*
*/
static char *createBlockFuncCode(GenericObj *genObj,GenericBlock *block,char *newFuncName)
{
   NString *codes=n_string_new("");
   char *replace= replaceParams(genObj,block->parms);

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
         n_string_append_printf(codes,"%s %d \n",RID_AET_GOTO_STR,GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC);
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

static CompileUnit *createCompileUnit(NPtrArray *compileUnitArray,GenericInfo *newInfo)
{
   int i;
   for(i=0;i<compileUnitArray->len;i++){
      CompileUnit *unit=n_ptr_array_index(compileUnitArray,i);
      if(unit->info==newInfo)
         return unit;
   }
   CompileUnit *unit=n_slice_new0(CompileUnit);
   unit->info=newInfo;
   unit->codes=n_string_new("");
   unit->genObjArray=n_ptr_array_new();
   unit->blockIndexArray=n_ptr_array_new();
   n_ptr_array_add(compileUnitArray,unit);
   return unit;
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

static void createHeader(CompileUnit *unit,NPtrArray *headerFileArray)
{
   int i,j;
   for(i=0;i<unit->genObjArray->len;i++){
      GenericObj *obj=n_ptr_array_index(unit->genObjArray,i);
     // printf("createHeader---- %s\n",obj->declClassFile);
      if(obj->declClassFile!=NULL && !existsHeaderFile(headerFileArray,obj->declClassFile) && endswith(obj->declClassFile,".h")){
          n_ptr_array_add(headerFileArray,obj->declClassFile);
      }
      for(j=0;j<obj->infoLen;j++){
         RunGenericInfo *info=obj->infos[j];
         if(info->file!=NULL && !existsHeaderFile(headerFileArray,info->file) && endswith(info->file,".h")){
            n_ptr_array_add(headerFileArray,info->file);
         }
      }
   }
}

/*
 * 生成BlockFuncData数据
static BlockFuncData _bfi_value_0[]={
{(void*)_double_0_com_ai_linear_SVD__gen_block_func_0,"_double_0_com_ai_linear_SVD__gen_block_func_0","_double_0","com_ai_linear_SVD",0},
*/
static void createFuncAddressCodes(NPtrArray *funcNameArray,NString *codes)
{
   int i;
   int len=funcNameArray->len;
   for(i=0;i<len;i++){
      BlockFuncIndex *b=n_ptr_array_index(funcNameArray,i);
      n_string_append_printf(codes,"{(void*)%s,",b->funcName);
      n_string_append_printf(codes,"\"%s\",",b->funcName);
      n_string_append_printf(codes,"\"%s\",",b->genericModel);
      n_string_append_printf(codes,"\"%s\",",b->sysName);
      n_string_append_printf(codes,"%d}",b->index);
      if(i<funcNameArray->len-1)
         n_string_append(codes,",\n");
   }
}

/*生成如下代码:
 * static BlockFuncData _bfi_value[]={
{(void*)_float_0_TFirst__gen_block_func_0,"_float_0_TFirst__gen_block_func_0","_float_0","TFirst",0}};

static __attribute__((constructor)) void load_generic_data()
{
   add_generic_data(_bfi_value);
}
 */

static int createBlockIndex(CompileUnit *unit,NString *codes,NString *addCodes,int total)
{
   int len=unit->blockIndexArray->len;
   int i;
   int count=total;
   for(i=0;i<len;i++){
       NPtrArray *sub=n_ptr_array_index(unit->blockIndexArray,i);
       n_string_append_printf(codes,"static BlockFuncData _bfi_value_%d[]={\n",count);
       createFuncAddressCodes(sub,codes);
       n_string_append(codes,"};\n");
       n_string_append(codes,"\n");
       n_string_append_printf(addCodes,"\tadd_generic_data(_bfi_value_%d,%d);\n",count,sub->len);
       count++;
   }
   return count;
}

static void createFuncAddress(NString *codes,CompileUnit *unit)
{
   int count=0;
   NString *ic1=n_string_new("");
   NString *ic2=n_string_new("");
   count=createBlockIndex(unit,ic1,ic2,count);
   int i;
   for(i=0;i<unit->mergeCount;i++){
      count=createBlockIndex(unit->merges[i],ic1,ic2,count);
   }
   n_string_append(codes,ic1->str);
   n_string_append(codes,"static __attribute__((constructor)) void load_generic_data()\n");
   n_string_append(codes,"{\n");
   n_string_append(codes,ic2->str);
   n_string_append(codes,"}\n");
   n_string_free(ic1,TRUE);
   n_string_free(ic2,TRUE);
}

/**
 * 加入函数代码到编译单元
 * funcNameArray 属于CompileUnit的所有函数名
 */
static void addCodes(CompileUnit *unit,GenericObj *genObj,char *blockCodes,NPtrArray *funcNameArray)
{
    n_string_append(unit->codes,blockCodes);
    n_ptr_array_add(unit->genObjArray,genObj);
    n_ptr_array_add(unit->blockIndexArray,funcNameArray);
}

static void addCodes(CompileUnit *unit,GenericObj *genObj,NPtrArray *dependClassGenObjArray,char *blockCodes,NPtrArray *funcNameArray)
{
    n_string_append(unit->codes,blockCodes);
    n_ptr_array_add(unit->genObjArray,genObj);
    int i;
    for(i=0;i<dependClassGenObjArray->len;i++)
       n_ptr_array_add(unit->genObjArray,n_ptr_array_index(dependClassGenObjArray,i));
    n_ptr_array_add(unit->blockIndexArray,funcNameArray);
}

/**
 * 在NPtrArray是否已存在GenericObj obj
 */
static CompileUnit *existsUnit(NPtrArray *newArray,CompileUnit *unit)
{
    int i;
    for(i=0;i<newArray->len;i++){
       CompileUnit *item=n_ptr_array_index(newArray,i);
       if(unit==item){
          n_error("有两个相同的编译单元。%s",item->info->className->sysName);
          return NULL;
       }
       char *a=unit->info->classDeclBelongFile;
       char *b=item->info->classDeclBelongFile;
       if(strcmp(a,b)==0 && endswith(a,".c"))
          return item;
    }
    return NULL;
}

/**
 * 移走全定义相同的泛型对象。
 */
static void mergeUnit(NPtrArray *unitArray)
{
   if(unitArray->len<=1)
      return;
   NPtrArray *newArray=n_ptr_array_new();
   int i,j;
   for(i=0;i<unitArray->len;i++){
      CompileUnit *item=n_ptr_array_index(unitArray,i);
      CompileUnit *exists=existsUnit(newArray,item);
      if(exists){
         exists->merges[exists->mergeCount++]=item;
      }else{
         n_ptr_array_add(newArray,item);
      }
   }
   n_ptr_array_remove_range(unitArray,0,unitArray->len);
   for(i=0;i<newArray->len;i++)
      n_ptr_array_add(unitArray,n_ptr_array_index(newArray,i));
   n_ptr_array_unref(newArray);
}

/**
 * 创建编译单元
 * index是索引号
 * 返回要编译的文件名。
 * 重点：如果要编译的内容加到源代码中编译，返回的文件名格式是:
 * 泛型块文件+","+类声明所在的.c文件+","类声明所在的.c文件的o文件
 */
static char *createCompileUnitFile(CompileUnit *unit,int index,char *parentPath)
{
   //生成include的内容
   int i,j;
   //writeCfile=true 表示在在项目.c文件后，追加编译 见 aetcollect.c
   nboolean writeCfile=unit->info->classDeclBelongFile && endswith(unit->info->classDeclBelongFile,".c");
   NPtrArray *headerFileArray=n_ptr_array_new();
   createHeader(unit,headerFileArray);
   for(i=0;i<unit->mergeCount;i++)
      createHeader(unit->merges[i],headerFileArray);

   NString *codes=n_string_new("\n");
   if(!writeCfile && unit->info->includeStr){
      n_string_append(codes,unit->info->includeStr);
      n_string_append(codes,"\n");
   }
   for(i=0;i<headerFileArray->len;i++){
      n_string_append_printf(codes,"#include \"%s\"\n",n_ptr_array_index(headerFileArray,i));
   }
   n_string_append(codes,"\n");
   n_string_append(codes,unit->codes->str);
   n_string_append(codes,"\n");
   for(i=0;i<unit->mergeCount;i++)
      n_string_append(codes,unit->merges[i]->codes->str);
   //生成函数地址变量
   createFuncAddress(codes,unit);
   //类声明在.c文件中。泛型块函数需要在该.c中编译。aet_collect 会删除对应的.o文件，并传递参数给该文件，说是第二次编译。
   //在编译完本文件后，追加下面的内容。
   char saveFile[256];
   sprintf(saveFile,"%s/%s_%d.c",parentPath,GENERIC_BLOCK_FILE_NAME,index);
   FILE *fp=fopen(saveFile,"w");
   fwrite(codes->str,1,codes->len,fp);
   fclose(fp);
   NString *returnFile=n_string_new("");
   n_string_append(returnFile,saveFile);
   if(writeCfile){
      n_string_append(returnFile,",");
      n_string_append(returnFile,unit->info->classDeclBelongFile);
      n_string_append(returnFile,",");
      n_string_append(returnFile,unit->info->oFile);
   }
   n_debug("创建的块内容 :返回的文件名:%s %s\n",returnFile->str,codes->str);
   n_string_free(codes,TRUE);
   return  n_string_free(returnFile,FALSE);
}

/**
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
 */
void generic_code_create_block_codes(GenericCode *self)
{
   char *fileName = getenv("GCC_AET_BLOCK_LIST_PATH");
   if(fileName==NULL){
      //n_error("进入这里不应该没GCC_AET_BLOCK_LIST_PATH文件名");
      return;
   }
   char saveFile[512];
   //这是与aetcollect的协议 在ifaceimpl.c中也有类似
   sprintf(saveFile,"%s.o",fileName);
   printf("generic_code_create_block_codes 11 saveFile:%s\n",saveFile);

   NPtrArray *genObjArray=generic_graph_get_output_generic_obj(generic_graph_get());
   //没有genericobj对象
   if(genObjArray==NULL || genObjArray->len==0){
      remove(saveFile);
      return;
   }
   NPtrArray *genInfoArray=block_mgr_get_output_generic_info(block_mgr_get());
   //没有泛型块
   if(genInfoArray==NULL || genInfoArray->len==0){
      remove(saveFile);
      return;
   }
   printf("generic_code_create_block_codes 22 saveFile:%s\n",saveFile);

   //现在我们有了定义的泛型对象和块代码，把块中的泛型单元替换为定义的泛型单元
   //并生成新函数代码
   //genObjArray分成泛型对象与泛型函数两个，分别处理
   NPtrArray *classGenObjArray=n_ptr_array_new();
   NPtrArray *funcGenObjArray=n_ptr_array_new();

   int i;
   for(i=0;i<genObjArray->len;i++){
      GenericObj *obj=n_ptr_array_index(genObjArray,i);
      if(obj->type==GEN_FUNC)
         n_ptr_array_add(funcGenObjArray,obj);
      else
         n_ptr_array_add(classGenObjArray,obj);
   }
   //按类名来分编译单元的。
   NPtrArray *compileUnitArray=n_ptr_array_new();

   for(i=0;i<funcGenObjArray->len;i++){
      GenericObj *funcGenObj=n_ptr_array_index(funcGenObjArray,i);
      GenericInfo *info=NULL;
     // char className[256];
      //char funcName[256];
     // int ok=func_mgr_get_orig_func_and_class_name(func_mgr_get(),funcGenObj->callee->mangleFunName,className,funcName);
      char *sysName=funcGenObj->callee->className->sysName;
      info=getGenericInfo(genInfoArray,sysName);
      n_debug("genericcode.c 泛型函数中的块代码 00 --- %p %s %s\n",info,sysName,   funcGenObj->callee->mangleFunName);
      if(n_log_is_debug_file(NULL,NULL))
         generic_obj_print(funcGenObj);
      if(info==NULL)
         continue;
      CompileUnit *unit=createCompileUnit(compileUnitArray,info);
      //收集所有引用到的泛型对象。
      NPtrArray *dependClassGenObjArray=n_ptr_array_new();
      //收集所有生成的新函数名
      NPtrArray *funcNameArray=n_ptr_array_new();
      char  *codes=createCodesForGenfunc(funcGenObj,info,classGenObjArray,dependClassGenObjArray,funcNameArray);
      n_debug("genericcode.c 泛型函数中的块代码 11--- %p %s\n",info,codes);

      if(codes){
         addCodes(unit,funcGenObj,dependClassGenObjArray,codes,funcNameArray);
         n_free(codes);
      }else{
         n_ptr_array_unref(funcNameArray);
      }
      n_ptr_array_unref(dependClassGenObjArray);
   }
   NFile *f=n_file_new(fileName);
   NFile *parent=n_file_get_parent_file(f);
   NFile *canon=n_file_get_canonical_file(parent);
   const char *parentPath=n_file_get_absolute_path(canon);


   for(i=0;i<classGenObjArray->len;i++){
      GenericObj *classGenObj=n_ptr_array_index(classGenObjArray,i);
      GenericInfo *info=NULL;
      info=getGenericInfo(genInfoArray,classGenObj->newObject->className.sysName);
      n_debug("genericcode.c  普通函数中的块 --- %p %p %s\n",classGenObj,info,classGenObj->newObject->className.sysName);
      if(info==NULL)
         continue;
      CompileUnit *unit=createCompileUnit(compileUnitArray,info);
      //收集所有生成的新函数名
      NPtrArray *funcNameArray=n_ptr_array_new();
      char *codes=createCodesForClass(classGenObj,info,funcNameArray);
      if(codes){
         addCodes(unit,classGenObj,codes,funcNameArray);
         n_free(codes);
      }else{
         n_ptr_array_unref(funcNameArray);
      }
   }
   mergeUnit(compileUnitArray);//合并同一个.c文件的所有编译单元。
   NString *compileFileList=n_string_new("");
   int unitIndex=0;
   //创建编译文件，并返回这些编译文件给aet_collect.c
   for(i=0;i<compileUnitArray->len;i++){
      CompileUnit *unit=n_ptr_array_index(compileUnitArray,i);
      char *compileFileName=createCompileUnitFile(unit,unitIndex++,parentPath);
      n_string_append_printf(compileFileList,"%s\n",compileFileName);
      n_free(compileFileName);
   }
   FILE *fp=fopen(saveFile,"w");
   fwrite(compileFileList->str,1,compileFileList->len,fp);
   fclose(fp);
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

