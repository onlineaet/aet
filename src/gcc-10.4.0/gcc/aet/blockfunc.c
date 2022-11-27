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
#include "langhooks.h"
#include "opts.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "genericimpl.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "classimpl.h"
#include "blockfunc.h"
#include "genericblock.h"
#include "genericinfo.h"
#include "genericfile.h"
#include "funcmgr.h"
#include "genericexpand.h"
#include "blockmgr.h"


typedef struct _BlockFuncParmInfo
{
	char *sysName;
	char *parmName;
	int pointer;
	NPtrArray *defineArray;
	NPtrArray *strArray;
	NPtrArray *strIndexArray;
	int genCount;
	nboolean isFuncGeneric;
}BlockFuncParmInfo;

static void blockFuncInit(BlockFunc *self)
{
}

static nboolean existsGen(NPtrArray *dest,GenericModel *compare)
{
	 int i;
	 for(i=0;i<dest->len;i++){
		GenericModel *item=(GenericModel *)n_ptr_array_index(dest,i);
		if(generic_model_equal(compare,item)){
			return TRUE;
		}
	 }
	 return FALSE;
}


/**
 * 当前文件中确定的泛型，保存在outGenModelArray,
 * 从中取出与参数类名相同的确定泛型加入到dest中
 * sysName 参数的类名
 */
static void copyOutGenModelArray(NPtrArray *outGenModelArray,NPtrArray *dest,char *sysName)
{
	NPtrArray *definesArray=NULL;
	int i;
	for(i=0;i<outGenModelArray->len;i++){
		GenericClass *item=(GenericClass *)n_ptr_array_index(outGenModelArray,i);
		if(strcmp(item->sysName,sysName)==0){
			definesArray=item->definesArray;
			break;
		}
	}
   if(definesArray!=NULL){
	  for(i=0;i<definesArray->len;i++){
		 GenericModel *item=(GenericModel *)n_ptr_array_index(definesArray,i);
		 if(!existsGen(dest,item)){
			n_ptr_array_add(dest,item);
		 }
	  }
   }
}

static void copyOutGenModelForFuncCall(NPtrArray *outGenModelArray,NPtrArray *dest,char *sysName,char *funcName)
{
	if(outGenModelArray==NULL)
		return;
	NPtrArray *definesArray=NULL;
	int i;
	for(i=0;i<outGenModelArray->len;i++){
		GenericMethod *item=(GenericMethod *)n_ptr_array_index(outGenModelArray,i);
		if(strcmp(item->sysName,sysName)==0 && strcmp(item->funcName,funcName)==0){
			definesArray=item->definesArray;
			break;
		}
	}
   if(definesArray!=NULL){
	  for(i=0;i<definesArray->len;i++){
		 GenericModel *item=(GenericModel *)n_ptr_array_index(definesArray,i);
		 if(!existsGen(dest,item)){
			n_ptr_array_add(dest,item);
		 }
	  }
   }
}

/**
 * 获取当前类中定义的泛型
 * block_func_ready_parm 中collectArray只有undefine的。不全
 */
static void copyDefineObject(BlockFunc *self,NPtrArray *dest,char *parmSysName)
{
	NPtrArray *childs=block_mgr_read_define_new_object(block_mgr_get(),self->sysClassName);
	if(childs==NULL){
	    n_debug("copyDefineObject--- 在当前类：%s中 没有define的对象,参数的类名是:%s 不加入泛型到参数中。%s",self->sysClassName,parmSysName,in_fnames[0]);
		return;
	}
	int i;
	for(i=0;i<childs->len;i++){
		GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(childs,i);
		if(strcmp(item->sysName,parmSysName)==0){
			 if(!existsGen(dest,item->model)){
				  n_ptr_array_add(dest,item->model);
			 }
		}else{
			//检查参数是不是继承关系
			ClassRelationship ship=class_mgr_relationship(class_mgr_get(), parmSysName,item->sysName);
			if(ship==CLASS_RELATIONSHIP_PARENT){
			    n_debug("copyDefineObject--- 在当前类：%s中 有define的对象,参数的类名是:%s 定义了泛型的类是:%s 它与参数的关系是儿子。",
						self->sysClassName,parmSysName,item->sysName);
				if(!existsGen(dest,item->model)){
					n_ptr_array_add(dest,item->model);
				}
			}
		}
	}

}

static void printDefines(NPtrArray *array,char *sysName)
{
	int i;
	int len=array->len;
	for(i=0;i<len;i++){
	   GenericModel *model=n_ptr_array_index(array,i);
	   printf("blockfunc 中收集的泛型 index:%d 类:%s 泛型:%s\n",i,sysName,generic_model_tostring(model));
	}
}



/**
 * 创建泛型块函数的参数
 * parmName：参数的名字。类型是Class$或IntegerFace$ 类
 */
static BlockFuncParmInfo *newGenericParmInfo(char *sysName,char *parmName,int pointer)
{
	 BlockFuncParmInfo *info=(BlockFuncParmInfo *)n_slice_new(BlockFuncParmInfo);
	 info->sysName=n_strdup(sysName);
	 info->parmName=n_strdup(parmName);
	 info->pointer=pointer;
	 info->defineArray=n_ptr_array_new();
	 info->strArray=n_ptr_array_new();
	 info->strIndexArray=n_ptr_array_new();
	 info->isFuncGeneric=FALSE;
	 info->genCount=0;
	 return info;
}

static BlockFuncParmInfo *createBlockFuncParmInfo(BlockFunc *self,char *sysName,char *name,tree parm,NPtrArray *outGenModelArray)
{
   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
   if(class_info_is_generic_class(info)){
	   int pointer=class_util_get_pointers(TREE_TYPE(parm));
	   BlockFuncParmInfo *pinfo=newGenericParmInfo(sysName,name,pointer);
	   pinfo->genCount=class_info_get_generic_count(info);
	   //获取在当前文件下的所有new对象时的泛型定义
	   copyOutGenModelArray(outGenModelArray,pinfo->defineArray,sysName);
	   copyDefineObject(self,pinfo->defineArray,sysName);
	   n_debug("创建泛型块函数中的参数 该参数是一个泛型类:%s 参数名：%s 收集的泛型类有几个 outGenModelArray:len:%d 该参数的泛型有几个:%d",
			   sysName,name,outGenModelArray->len,pinfo->defineArray->len);
	   if(pinfo->defineArray->len==0){
		   printDefines(pinfo->defineArray,sysName);
		   GenericModel *genModel=class_info_create_default_generic_define(info);
		   if(genModel==NULL){
			    n_error("块函数%s中的参数%s,它的类型是：%s,在当前文件找不到与该类相关的泛型定义:%s ",self->orgiFuncName,name,sysName,in_fnames[0]);
			    return NULL;
		   }
		   n_ptr_array_add(pinfo->defineArray,genModel);
		   n_warning("块函数%s中的参数%s,它的类型是：%s,在当前文件找不到与该类相关的泛型定义:%s，加一个缺省的int ",self->orgiFuncName,name,sysName,in_fnames[0]);
	   }
	   return pinfo;
   }
   return NULL;
}

/**
 * 创建泛型函数中的泛型块函数的第二个参数
 * 类型是结构体 FuncGenParmInfo
 */

static BlockFuncParmInfo *createBlockFuncParmInfoByGenFunc00(char *sysClassName,char *belongFunc,NPtrArray *outGenModelArray)
{
	   printf("createBlockFuncParmInfoByGenFunc 00 类:%s belongFunc：%s outGenModelArray:%p\n",sysClassName,belongFunc,outGenModelArray);
	   BlockFuncParmInfo *pinfo=newGenericParmInfo(AET_FUNC_GENERIC_PARM_STRUCT_NAME,AET_FUNC_GENERIC_PARM_NAME,0);
	   pinfo->isFuncGeneric=TRUE;
	   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
	   ClassFunc *func=func_mgr_get_entity(func_mgr_get(),className,belongFunc);
	   GenericModel *model=class_func_get_func_generic(func);
	   if(model==NULL){
		   n_error("不是泛型函数，不应该设置参数FuncGenParmInfo tempFgpi1234 %s %s\n",sysClassName,belongFunc);
		   return NULL;
	   }
	   copyOutGenModelForFuncCall(outGenModelArray,pinfo->defineArray,sysClassName,belongFunc);
	   if(pinfo->defineArray->len==0){
		   GenericModel *defaultModel=generic_model_create_default(model);
		   n_ptr_array_add(pinfo->defineArray,defaultModel);
		   n_warning("createBlockFuncParmInfoByGenFunc 11 为泛型函数的参数tempFgpi1234创建缺省的泛型 %s %s %s",
				   sysClassName,belongFunc,generic_model_tostring(defaultModel));
	   }
	   pinfo->genCount=generic_model_get_count(model);
	   return pinfo;
}

static BlockFuncParmInfo *createBlockFuncParmInfoByGenFunc(char *sysClassName,char *belongFunc,NPtrArray *outGenModelArray)
{
       n_debug("createBlockFuncParmInfoByGenFunc -----00 类有泛型:%s belongFunc：%s",sysClassName,belongFunc);
	   if(outGenModelArray==NULL || outGenModelArray->len==0){
		   n_warning("找不到泛型函数的调用 %s 泛型函数:%s %p",sysClassName,belongFunc,outGenModelArray);
		   return NULL;
	   }
	   n_debug("createBlockFuncParmInfoByGenFunc -----11 类有泛型:%s belongFunc：%s %d",sysClassName,belongFunc,outGenModelArray->len);
	   BlockFuncParmInfo *pinfo=newGenericParmInfo(AET_FUNC_GENERIC_PARM_STRUCT_NAME,AET_FUNC_GENERIC_PARM_NAME,0);
	   pinfo->isFuncGeneric=TRUE;
	   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
	   ClassFunc *func=func_mgr_get_entity(func_mgr_get(),className,belongFunc);
	   GenericModel *model=class_func_get_func_generic(func);
	   if(model==NULL){
		   n_error("不是泛型函数，不应该设置参数FuncGenParmInfo *tempFgpi1234 %s %s\n",sysClassName,belongFunc);
		   return NULL;
	   }
	   copyOutGenModelForFuncCall(outGenModelArray,pinfo->defineArray,sysClassName,belongFunc);
	   pinfo->genCount=generic_model_get_count(model);
	   return pinfo;
}


static char *createStr(char *name,int pointer,BlockFuncParmInfo *parm,int index)
{
	   char *parmName=parm->parmName;
	   int parmPointer=parm->pointer;
	   NString *codes=n_string_new("");
	   if(!strcmp(parm->sysName,AET_FUNC_GENERIC_PARM_STRUCT_NAME)){
		   n_string_append_printf(codes,"!strcmp(%s.pms[%d].typeName,\"%s\")",parmName,index,name);
		   n_string_append_printf(codes,"&& %s.pms[%d].pointerCount==%d\n",parmName,index,pointer);
	   }else{
	      n_string_append_printf(codes,"!strcmp(%s%s%s[%d].typeName,\"%s\")",parmName,parmPointer>0?"->":".",AET_GENERIC_ARRAY,index,name);
	      n_string_append_printf(codes," && %s%s%s[%d].pointerCount==%d\n",parmName,parmPointer>0?"->":".",AET_GENERIC_ARRAY,index,pointer);
	   }
	   char *re=n_strdup(codes->str);
	   n_string_free(codes,TRUE);
	   return re;
}

#define INDEX_CONNECT_STR "$YZt0@"
static char *createStrIndex(char *parmName,int index)
{
	   NString *codes=n_string_new("");
	   n_string_append_printf(codes,"%s_%d",parmName,index);
	   char *re=n_strdup(codes->str);
	   n_string_free(codes,TRUE);
	   return re;
}

static void convertGenericModelToStr(BlockFuncParmInfo *parm,GenericModel *defines,NString *codes,int genCount)
{
	int i;
	for(i=0;i<defines->unitCount;i++){
		GenericUnit *unit=defines->genUnits[i];
		char *cond=createStr(unit->name,unit->pointerCount,parm,i);
		n_string_append(codes,n_strdup(cond));
	    n_free(cond);
		if(i<genCount-1)
			n_string_append(codes," && ");

	}
}

/**
 * 生成两种代码
 *1、 !strcmp(txyz->_generic_1234_array[0].typeName,"float") && txyz->_generic_1234_array[0].pointerCount==0
 *2、 txyz_0
 *一个泛型块函数（BlockFunc）有0个或多个参数（BlockParmInfo）
 *每个参数（BlockParmInfo）有1个泛型定义组(defineArray)
 *定义组中有多个定义的类的泛型如：<int,float> <double,float>等
 *k是类的泛型定义个数
 *strIndex的内容是(参数名_索引号)如:txyz_0 通过0就可找到defineArray中的GenericUnit **
 */
static void produceCondCodes(BlockFunc *self)
{
   int i,j,k;
   for(i=0;i<self->parmInfoCount;i++){
	    BlockFuncParmInfo *info=(BlockFuncParmInfo *)self->parmInfos[i];
	    n_debug("produceCondCodes i:%d sysName:%s 参数:%s 泛型函数:%d genCount:%d len:%d",
	    		i,info->sysName,info->parmName,info->isFuncGeneric,info->genCount,info->defineArray->len);
		NPtrArray *array=info->defineArray;
		int len=array->len;
		for(j=0;j<len;j++){
    	   GenericModel *items=n_ptr_array_index(array,j);
    	   n_debug("produceCondCodes --- %d:%s %s %s isFuncGeneric:%d info->genCount:%d",
   	    		   j,info->sysName,info->parmName,generic_model_tostring(items),info->isFuncGeneric,info->genCount);
		   NString *gx=n_string_new("");
		   convertGenericModelToStr(info,items,gx,info->genCount);
           n_ptr_array_add(info->strArray,n_strdup(gx->str));
           char *strIndex=createStrIndex(info->parmName,j);
           n_ptr_array_add(info->strIndexArray,n_strdup(strIndex));
           n_string_free(gx,TRUE);
    	   n_free(strIndex);
		}
   }

    for(i=0;i<self->parmInfoCount;i++){
	    BlockFuncParmInfo *info=self->parmInfos[i];
 		int j;
 		for(j=0;j<info->strArray->len;j++){
  	       char *item=n_ptr_array_index(info->strArray,j);
  	       char *itemIndex=n_ptr_array_index(info->strIndexArray,j);
  	       //printf("item is i:%d j:%d %s\n",i,j,item);
  	       //printf("item xx is i:%d j:%d %s\n",i,j,itemIndex);

 		}
        n_ptr_array_add(self->condArray,info->strArray);
        n_ptr_array_add(self->condIndexArray,info->strIndexArray);
    }
}

/**
 * 组合成各种条件
 */
static NPtrArray *doExchange(NPtrArray *arr,char *connectStr,nboolean whiteSpace)
{
    int len = arr->len;
    //printf("doExchange len--- %d connectStr:%s\n",len,connectStr);
    if(len >= 2){
    	NPtrArray *a1=n_ptr_array_index(arr,0);
    	NPtrArray *a2=n_ptr_array_index(arr,1);
        int len1 = a1->len;
        int len2 = a2->len;
        n_debug("doExchange len--- %d connectStr:%s %d %d",len,connectStr,len1,len2);

        NPtrArray *items=n_ptr_array_new();
        int i,j;
        for(i=0; i<len1; i++){
            for(j=0; j<len2; j++){
           	    char temp[512];
           	    char *str1=n_ptr_array_index(a1,i);
           	    char *str2=n_ptr_array_index(a2,j);
           	    if(whiteSpace)
           	       sprintf(temp,"%s %s %s",str1,connectStr,str2);
           	    else
            	   sprintf(temp,"%s%s%s",str1,connectStr,str2);
           	    n_ptr_array_add(items,n_strdup(temp));
       	       //printf("mergs item is i:%d j:%d %s\n",i,j,temp);
            }
        }
        NPtrArray *newArr = n_ptr_array_new();
   	    n_ptr_array_add(newArr,items);
        for(i=2;i<len;i++){
        	NPtrArray *remain=n_ptr_array_index(arr,i);
       	    n_ptr_array_add(newArr,remain);
        }
        n_ptr_array_unref(arr);
        return doExchange(newArr,connectStr,whiteSpace);
    }else{
        return arr;
    }
}

static nboolean isNoReturnValue(BlockFunc *self)
{
	char *re1=strstr(self->returnStr,"void");
	char *re2=strstr(self->returnStr,"*");
	return (re1 && !re2);
}

static char *getRealParm(BlockFunc *self)
{
   char *funcName=self->funcDefineName;
   tree funcId=aet_utils_create_ident(funcName);
   tree fndecl=lookup_name(funcId);
   n_debug("getRealParm 00 %s %p",funcName,fndecl);
   if(!aet_utils_valid_tree(fndecl)){
	   error("getRealParm 没有声明函数:%qs",funcName);
	   n_error("在getRealParm未知错误");
   }
   tree functype=TREE_TYPE(fndecl);
   tree args=DECL_ARGUMENTS (fndecl);
   tree parm=NULL_TREE;
   NString *codes=n_string_new("(");
   for (parm = args; parm; parm = DECL_CHAIN (parm)){
	   char *name=IDENTIFIER_POINTER(DECL_NAME(parm));
	   n_string_append_printf(codes,"%s,",name);
   }
   char *result=NULL;
   if(n_string_ends_with(codes,",")){
		NString *temp=n_string_substring_from(codes,0,codes->len-1);
		n_string_free(codes,TRUE);
		codes=temp;
   }
   n_string_append(codes,")");
   result=n_strdup(codes->str);
   n_string_free(codes,TRUE);
   return result;
}

static void defaultReturn(BlockFunc *self,NString *codes)
{
	  if(strstr(self->returnStr,"*")){
		  //有指针
		  n_string_append(codes,"return NULL;\n");
		  return;
	  }else{
		  tree id=aet_utils_create_ident(self->returnStr);
		  tree typeDecl=lookup_name(id);
		  if(aet_utils_valid_tree(typeDecl)){
			  tree type=TREE_TYPE(typeDecl);
			  if(TREE_CODE(type)==POINTER_TYPE){
				  n_string_append(codes,"return NULL;\n");
				  return;
			  }
		  }
	  }
	  n_string_append(codes,"return 0;\n");
}

#define PASS_tempFgpi1234 "tempFgpi1234执行"
#define PASS_self         "self执行"

static void setExcuteStmt(BlockFunc *self,char *funcTypedef,char *realParm,nboolean bySelf,NString *codes)
{
	nboolean no=isNoReturnValue(self);
	n_string_append              (codes,"              if(blockIndex>=0){\n");
	if(no){ //没有返回值
		  n_string_append_printf(codes,"                  (* (%s)blockInfo->blockFuncs[blockIndex].blockFuncsPointer[index-1])%s;\n",funcTypedef,realParm);
	      n_string_append_printf(codes,"                  printf(\"通过%s 11 找到函数了 没有返回值 %s %s\\n\");\n",bySelf?PASS_self:PASS_tempFgpi1234,self->funcDefineName,in_fnames[0]);
		  n_string_append_printf(codes,"                  if(%s.excute==1);\n",AET_FUNC_GENERIC_PARM_NAME);
		  n_string_append(codes,       "                    return;\n");
	}else{
		  n_string_append_printf(codes,"                  %s temp=(* (%s)blockInfo->blockFuncs[blockIndex].blockFuncsPointer[index-1])%s;\n",self->returnStr,funcTypedef,realParm);
	      n_string_append_printf(codes,"                  printf(\"通过%s 22 找到函数了 有返回值%s %s\\n\");\n",bySelf?PASS_self:PASS_tempFgpi1234,self->funcDefineName,in_fnames[0]);
		  n_string_append_printf(codes,"                  if(%s.excute==1);\n",AET_FUNC_GENERIC_PARM_NAME);
		  n_string_append(codes,       "                     return temp;\n");
	}
	n_string_append              (codes,"              }else if(blockIndex==-1){\n");
    n_string_append_printf(codes,"                        printf(\"通过%s 33 自已调用自己xx %s %s\\n\");\n",bySelf?PASS_self:PASS_tempFgpi1234,self->funcDefineName,in_fnames[0]);
	n_string_append              (codes,"              }else{\n");
    n_string_append_printf(codes,"                        printf(\"通过%s 44 找不到函数 %s %s\\n\");\n",bySelf?PASS_self:PASS_tempFgpi1234,self->funcDefineName,in_fnames[0]);
	n_string_append              (codes,"              }\n");
}

/**
 * 创建泛型块函数的源代码。
 * 关键是各种泛型定义的组合
 *   printf("在test4.c count: %d\n",tempFgpi1234.countOfBlockClass);
        if(tempFgpi1234.countOfBlockClass==0){
     	   printf("FuncGenParmInfo 不能处理未知的泛型。\n");
        }else{
           int index=0;
     	   int i;
     	   char *className="test";
     	   for(i=0;i<tempFgpi1234.countOfBlockClass;i++){
     		   if(!memcmp(className,tempFgpi1234.blockFuncs[i].sysName,strlen(className))){
     	    	   printf("eexx xxxxx%d\n",5);
     	    	   return (* (_test_MatrixOps__inner_generic_func_1_typedecl)tempFgpi1234.blockFuncs[i].blockFuncsPointer[index])(self,tempFgpi1234,b);
     		   }
     	   }
        }
 *
 */

static void createCodes(BlockFunc *self,NString *codes)
{
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),self->sysClassName);
	   n_debug("createCodes----泛型参数有几个:%d self->condArray:len:%d",self->parmInfoCount,self->condArray->len);
	   produceCondCodes(self);
	   NPtrArray *result=doExchange(self->condArray,"&&",TRUE);
	   NPtrArray *resultIndex=doExchange(self->condIndexArray,INDEX_CONNECT_STR,FALSE);
	   NPtrArray *condArray=result->len>0?n_ptr_array_index(result,0):NULL;
	   NPtrArray *itemsIndex=resultIndex->len>0?n_ptr_array_index(resultIndex,0):NULL;
	   int i;
	   n_string_append_printf(codes,"static %s %s %s\n",self->returnStr,self->funcDefineName,self->parmStr);
	   n_string_append(codes,"{\n");
	   if(condArray && condArray->len>0){
		   for(i=0;i<condArray->len;i++){
			 char  *cond=n_ptr_array_index(condArray,i);
			 char  *genStr=n_ptr_array_index(itemsIndex,i);
			 if(i==0){
				 n_string_append_printf(codes,"if(%s){\n",cond);
			 }else{
				 n_string_append_printf(codes,"else if(%s){\n",cond);
			 }
			 n_string_append_printf(codes,"%s %d \"%s\"\n",RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_DEFINE_COND,genStr);
			 n_string_append_printf(codes,"   %s.excute=1;\n",AET_FUNC_GENERIC_PARM_NAME);
			 n_string_append(codes,self->body);
			 n_string_append(codes," }\n");
		  }
      }else{
		 n_string_append(codes,"if(5<3){\n");
		 n_string_append(codes,"   printf(\"没有条件\\n\");\n");
		 n_string_append(codes," }\n");
      }
	  nboolean noRtn=isNoReturnValue(self);
	  char *funcTypedef=generic_util_create_block_func_type_decl_name(self->sysClassName,self->index);
	  char *realParm=getRealParm(self);
	  n_string_append(codes,      "else{\n");
	  n_string_append_printf(codes,"   static const char *sysName=\"%s\";\n",self->sysClassName);
	  n_string_append_printf(codes,"   int index=%d;\n",self->index);
	  n_string_append_printf(codes,"   if(%s.globalBlockInfo){\n",AET_FUNC_GENERIC_PARM_NAME);
	  n_string_append_printf(codes,"         FuncGenBlockInfo *blockInfo=%s.globalBlockInfo;\n",AET_FUNC_GENERIC_PARM_NAME);
	  n_string_append(codes,       "         printf(\"FuncGenBlockInfo来自的文件是:%s\\n\",blockInfo->from);\n");
	  n_string_append_printf(codes,"         int blockIndex=get_block_func_pointer_from_info(blockInfo,index,sysName,%s);\n",self->funcDefineName);
	  setExcuteStmt(self,funcTypedef,realParm,FALSE,codes);
      n_string_append(codes,       "    }\n");
	  n_string_append_printf(codes,"   FuncGenBlockList *list=&self->%s;\n",AET_BLOCK_INFO_LIST_VAR_NAME);
	  n_string_append(codes,       "   while(list){\n");
	  n_string_append(codes,       "      FuncGenBlockInfo *blockInfo=list->data;\n");
	  n_string_append(codes,       "      printf(\"while FuncGenBlockInfo来自的文件是:%s\\n\",blockInfo->from);\n");
	  n_string_append_printf(codes,"      int blockIndex=get_block_func_pointer_from_info(blockInfo,index,sysName,%s);\n",self->funcDefineName);
	  setExcuteStmt(self,funcTypedef,realParm,TRUE,codes);
	  n_string_append(codes,       "      list=list->next;\n");
      n_string_append(codes,       "    }\n");
	  n_string_append(codes,       "}\n");
	  if(!noRtn)
		  defaultReturn(self,codes);
	  n_string_append(codes,"}\n");
}


char *block_func_create_codes(BlockFunc *self)
{
	NString *codes=n_string_new("");
	createCodes(self,codes);
	char *result=NULL;
	if(codes->len>0)
		result=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	//printf("泛型块实现的源代码:%s\n",result);
	return result;
}

/**
 * 块函数的参数信息
 * 第二个参数是 FuncGenParmInfo tempFgpi1234
 * collectArray 收集的泛型类GenericClass及所有的define泛型单元。
 * collectFuncCallArray 泛型函数
 */
void  block_func_ready_parm(BlockFunc *self,NPtrArray *collectArray,NPtrArray *collectFuncCallArray)
{
   char *funcName=self->funcDefineName;
   tree funcId=aet_utils_create_ident(funcName);
   tree fndecl=lookup_name(funcId);
   //printf("block_func_ready_parm 00 %s %p\n",funcName,fndecl);
   if(!aet_utils_valid_tree(fndecl)){
	   error("没有声明函数:%qs",funcName);
	   n_error("在block_func_ready_parm未知错误");
   }
   tree functype=TREE_TYPE(fndecl);
   tree args=DECL_ARGUMENTS (fndecl);
   tree parm=NULL_TREE;
   for (parm = args; parm; parm = DECL_CHAIN (parm)){
	   char *name=IDENTIFIER_POINTER(DECL_NAME(parm));
	   tree type=TREE_TYPE(parm);
	   char *sysClassName=class_util_get_class_name(type);
	   //printf("block_func_ready_parm 11 %s %s %s\n",funcName,name,sysClassName);
	   if(sysClassName!=NULL){
		   BlockFuncParmInfo *pinfo=createBlockFuncParmInfo(self,sysClassName,name,parm,collectArray);
		   if(pinfo!=NULL)
			   self->parmInfos[self->parmInfoCount++]=pinfo;
		}else{
		   if(TREE_CODE(type)==RECORD_TYPE){
			  tree decl=TYPE_NAME(type);
			  char *id=IDENTIFIER_POINTER(DECL_NAME(decl));
			  if(!strcmp(id,AET_FUNC_GENERIC_PARM_STRUCT_NAME) && !strcmp(name,AET_FUNC_GENERIC_PARM_NAME) && self->isFuncGeneric){
				  BlockFuncParmInfo *pinfo=createBlockFuncParmInfoByGenFunc(self->sysClassName,self->belongFunc,collectFuncCallArray);
				  n_debug("block_func_ready_parm 22 这是泛型函数中的块 pinfo:%p",pinfo);
				  if(pinfo!=NULL){
					 self->parmInfos[self->parmInfoCount++]=pinfo;
				  }
			  }
		   }
	   }
   }
}

static void fillStr(BlockFunc *self,char *sysName,int index,BlockDetail *detail)
{
   char *funcSource=detail->codes;
   char *orgiFuncName=generic_util_create_block_func_name(sysName,index);
   char *funcDefineName=generic_util_create_block_func_define_name(sysName,index);
   self->sysClassName=n_strdup(sysName);
   self->orgiFuncName=n_strdup(orgiFuncName);
   self->funcDefineName=n_strdup(funcDefineName);
   self->typedefFuncName=generic_util_create_block_func_type_decl_name(sysName,index);
   self->funcSource=n_strdup(detail->codes);
   n_free(orgiFuncName);
   n_free(funcDefineName);

}

static void init(BlockFunc *self,char *sysName,int index,BlockDetail *detail)
{
   fillStr(self,sysName,index,detail);
   self->condArray=n_ptr_array_new();
   self->condIndexArray=n_ptr_array_new();
   self->belongFunc=n_strdup(detail->belongFunc);
   self->isFuncGeneric=detail->isFuncGeneric;
   NString *re=n_string_new(detail->codes);
   int pos=n_string_indexof(re,"{");
   int lp=n_string_last_indexof(re,"}");
   NString *body=n_string_substring_from(re,pos+1,lp);
   self->body=n_strdup(body->str);
   NString  *f0=n_string_substring_from(re,0,pos);
   if(n_string_ends_with(f0,"\n")){
	   NString *temp=n_string_substring_from(f0,0,f0->len-1);
	   n_string_free(f0,TRUE);
	   f0=temp;
   }
   pos=n_string_indexof(re,self->orgiFuncName);
   NString *returnStr=n_string_substring_from(f0,0,pos);
   NString *parmDeclStr=n_string_substring(f0,pos+strlen(self->orgiFuncName));
   self->returnStr=n_strdup(returnStr->str);
   self->parmStr=n_strdup(parmDeclStr->str);

   /**
    * funcDefineDecl
    *static void  _test_AObject__inner_generic_func_1_abc123 (test_AObject *self,FuncGenParmInfo tempFgpi1234,int abc);
    */
   NString *fundecl=n_string_new("");
   n_string_append_printf(fundecl,"static %s %s %s;\n",returnStr->str,self->funcDefineName,parmDeclStr->str);
   self->funcDefineDecl=n_strdup(fundecl->str);

   NString *typeFunc=n_string_new("");
   n_string_append_printf(typeFunc,"typedef %s (*%s) %s;\n",returnStr->str,self->typedefFuncName,parmDeclStr->str);
   self->typedefFuncDecl=n_strdup(typeFunc->str);


   self->parmInfoCount=0;

   n_string_free(re,TRUE);
   n_string_free(f0,TRUE);
   n_string_free(returnStr,TRUE);
   n_string_free(parmDeclStr,TRUE);
   n_string_free(body,TRUE);
   n_string_free(fundecl,TRUE);
   n_string_free(typeFunc,TRUE);
}

/**
 * 当编译E时在当前函数内找到E所代表的真实数据类型
 */
GenericUnit *block_func_get_define(BlockFunc *self,char *parmName,char *genStr,int pos)
{
	 int i;
	 BlockFuncParmInfo *parmInfo=NULL;
	 for(i=0;i<self->parmInfoCount;i++){
		 BlockFuncParmInfo *item=(BlockFuncParmInfo *)self->parmInfos[i];
		if(strcmp(item->parmName,parmName)==0){
	       parmInfo=item;
           break;
		}
	 }
	 if(parmInfo==NULL){
		n_error("block_func_get_defines 找不到GenericParmInfo 未知错误！%s",parmName);
		return NULL;
	 }
	 int index=-1;
	 if(!parmInfo->isFuncGeneric){
		 ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),parmInfo->sysName);
		 if(info->genericModel){
		 	index= generic_model_get_index(info->genericModel,genStr);
		 }
		 if(index<0){
		     n_debug("在类%s中找不到泛型声明%s,是不是在泛型函数中的块:%d。belongFunc:%s",parmInfo->sysName,genStr,self->isFuncGeneric,self->belongFunc);
			 if(self->isFuncGeneric){
				 ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),self->sysClassName);
				 ClassFunc    *func=func_mgr_get_entity(func_mgr_get(),className,self->belongFunc);
				 GenericModel *model=class_func_get_func_generic(func);
				 index= generic_model_get_index(model,genStr);
			 }
		 }
	 }else{
		ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),self->sysClassName);
		ClassFunc *func=func_mgr_get_entity(func_mgr_get(),className,self->belongFunc);
		tree fieldDecl=func->fieldDecl;
		GenericModel *gendecl=c_aet_get_func_generics_model(fieldDecl);
		index= generic_model_get_index(gendecl,genStr);
	 }
	 if(index<0){
		 n_error("在类%s的第%d个泛型块中的泛型:%s，是错误的,当前块所在的函数是:%s。",self->sysClassName,self->index,genStr,self->belongFunc);
	 }
	 NPtrArray *defineArray=parmInfo->defineArray;
	 GenericModel *defines=n_ptr_array_index(defineArray,pos);
	 GenericUnit *define=    generic_model_get(defines,index);
	 return define;
}

nboolean  block_func_is_same(BlockFunc *self,char *orgiFuncName)
{
	return (orgiFuncName && strcmp(self->orgiFuncName,orgiFuncName)==0);
}

nboolean  block_func_have_func_decl_tree(BlockFunc *self)
{
	tree id=aet_utils_create_ident(self->funcDefineName);
	tree func=lookup_name(id);
	return aet_utils_valid_tree(func);
}

nboolean   block_func_have_typedef_func_decl(BlockFunc *self)
{
	tree id=aet_utils_create_ident(self->typedefFuncName);
	tree func=lookup_name(id);
	return aet_utils_valid_tree(func);
}


/**
 * index:类中的第几个泛型块 从1开始
 */

BlockFunc *block_func_new(int index,char *sysName,BlockDetail *detail)
{
	BlockFunc *self =n_slice_alloc0 (sizeof(BlockFunc));
	blockFuncInit(self);
	self->index=index;
	init(self,sysName,index,detail);
	return self;
}




