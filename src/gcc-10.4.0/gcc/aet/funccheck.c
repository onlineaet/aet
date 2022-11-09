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
#include "toplev.h"
#include "opts.h"

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
#include "aetutils.h"
#include "classmgr.h"
#include "funccheck.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "aet-c-parser-header.h"
#include "classfunc.h"
#include "genericutil.h"
#include "funcmgr.h"
#include "genericcall.h"
#include "blockmgr.h"
#include "genericquery.h"
#include "parserhelp.h"
#include "funccheck.h"
#include "makefileparm.h"
#include "classutil.h"
#include "middlefile.h"

#include <utime.h>

#define FUNC_CEHCK_SPERATOR_INFO "@#"

static void funcCheckInit(FuncCheck *self)
{
	self->xmlFile=NULL;
}

static char *FREE_CHILD_NAME="free_child";
static char *GET_CLASS="getClass";

/**
 * 不要检查保留的方法
 */
static nboolean reserveField(ClassFunc *func)
{
    if(!strcmp(func->orgiName,FREE_CHILD_NAME))
    	return TRUE;
    if(!strcmp(func->orgiName,GET_CLASS))
    	return TRUE;
    return FALSE;
}

static nboolean findDefine(ClassFunc *dest,ClassName *src)
{
	NPtrArray  *srcFuncs=func_mgr_get_funcs(func_mgr_get(),src);
	if(srcFuncs==NULL)
		return FALSE;
	 char *praw=dest->rawMangleName;
	 nboolean findDefine=FALSE;
	 int i;
	 for(i=0;i<srcFuncs->len;i++){
		 ClassFunc *compareFunc=(ClassFunc *)n_ptr_array_index(srcFuncs,i);
		 if(strcmp(compareFunc->rawMangleName,praw)==0){
			 if(aet_utils_valid_tree(compareFunc->fromImplDefine)){
				 findDefine=TRUE;
				 break;
			 }
		 }
	 }
	 return findDefine;
}

/**
 * 检查在class$中声明的方法，是否已实现
 * 不包括抽象方法和接口
 */
static nboolean checkSelfFuncDefine(ClassName *className)
{
	NPtrArray  *array=func_mgr_get_funcs(func_mgr_get(),className);
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *func=(ClassFunc *)n_ptr_array_index(array,i);
		if(aet_utils_valid_tree(func->fieldDecl) && !reserveField(func)){
			//如果有声明在这里必须有实现
			nboolean define=aet_utils_valid_tree(func->fromImplDefine);
			if(!define){
				if(func->isAbstract){
					printf("检查类方法:类%s的方法%s是抽象方法，可以不实现。\n",className->sysName,func->orgiName);
				}else{
					error_at(DECL_SOURCE_LOCATION(func->fieldDecl),"类%qs的方法%qs没有实现。",className->userName,func->orgiName);
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}

/**
 * 检果是否实现了父类的抽象方法，如果是抽象类可以不实现。
 */
static nboolean  checkParentAbstractFuncDefine(ClassName *className)
{
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(info->parentName.sysName==NULL){
        n_debug("检查父类方法 类%s没有父类，不需要检查方法实现。\n",className->sysName);
    	return TRUE;
    }
    ClassName *parentName=&info->parentName;
	ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parentName);
    if(!class_info_is_abstract_class(parentInfo)){
        n_debug("检查父类方法 类%s继承的父类%s不是抽象类，不需要检查是否实现父类的方法。\n",className->sysName,parentName->sysName);
    	return TRUE;
    }
    n_debug("检查父类方法 子类%s是%s，父类%s是抽象类，检查是否实现了父类中的抽象方法。\n",className->sysName,
    		class_info_is_abstract_class(info)?"抽象类":"普通类",parentName->sysName);
	NPtrArray  *parentArray=func_mgr_get_funcs(func_mgr_get(),parentName);
    //抽象方法是否要实现
	int i;
	for(i=0;i<parentArray->len;i++){
		ClassFunc *func=(ClassFunc *)n_ptr_array_index(parentArray,i);
		if(aet_utils_valid_tree(func->fieldDecl) && func->isAbstract){
		    n_debug("检查父类方法 父类%s的抽象方法:%s\n",parentName->sysName,func->orgiName);
             //检果名字，参数返回值
             nboolean find=findDefine(func,className);
			 if(!find){
				if(class_info_is_abstract_class(info)){
				    n_debug("检查父类方法 类%s是抽象类，父类%s有抽象方法%s,可以不实现父类的抽象方法。\n",className->sysName,parentName->sysName,func->orgiName);
				 }else{
			        error_at(DECL_SOURCE_LOCATION(func->fieldDecl),"类%qs没有实现父类%qs的抽象方法%qs。",className->userName,parentName->userName,func->orgiName);
			        return FALSE;
				 }
			 }else{
			     n_debug("检查父类方法 子类%s实现了父类%s的抽象方法%s。%s\n",
						 className->sysName,parentName->sysName,func->orgiName,class_info_is_abstract_class(info)?"子类是抽象类":"子类是普通类");
 			 }
		}
	}
	return TRUE;
}

/**
 * 检果是否实现了接口，如果是抽象类可以不实现。
 */
static nboolean eachInterface(ClassName *from,ClassName *iface)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),iface);
   NPtrArray *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
   if(ifaceFuncsArray==NULL || ifaceFuncsArray->len==0)
	   return TRUE;
   int i=0;
   for(i=0;i<ifaceFuncsArray->len;i++){
	   ClassFunc *interfaceMethod=(ClassFunc *)n_ptr_array_index(ifaceFuncsArray,i);
	   if(class_func_is_interface_reserve(interfaceMethod)) //是接口需要保留的ref和unref
		   continue;
	   char *atSysName=NULL;
	   ClassFunc    *impl=func_mgr_get_interface_impl(func_mgr_get(),from, interfaceMethod,&atSysName);//获得接口的实现类和方法
	   if(impl==NULL && !class_info_is_abstract_class(info)){
			error("类%qs没有实现接口%qs的方法%qs。",from->userName,iface->userName,interfaceMethod->orgiName);
			return FALSE;
	   }else if(impl==NULL && class_info_is_abstract_class(info)){
		    n_warning("抽象类%s没有实现接口%s的方法%s。",from->userName,iface->userName,interfaceMethod->orgiName);
	   }
   }
   return TRUE;
}

static nboolean checkInterfaceFuncDefine(ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info==NULL || info->ifaceCount<=0){
	   n_debug("检查接口方法 类%s没有要实现的接口，不需要检查方法实现。",className->sysName);
	   return TRUE;
   }
   int i;
   for(i=0;i<info->ifaceCount;i++){
	   ClassName iface=info->ifaces[i];
	   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
	   if(faceInfo==NULL){
		   error("在类%qs中实现接口%qs,但没有找到接口的声明。检查是否包含对应的头文件",className->userName,iface.userName);
		   return FALSE;
	   }
	   if(!eachInterface(className,&iface)){
		   return FALSE;
	   }
   }
   return TRUE;
}

////////////////////////以上完成当前类的函数实现匹配检查

/////////////////////以下是生成保存到aet_class_func_check_xml.tmp文件中的字符串-----三部份 1.类信息 2.类方法实现 3.未实现的方法--------------

/**
 * 普通类，父抽象的方法实现的，本身的方法与实现，本身的接口也实现了。
 * 本身没有要实现的方法，所以记录本身实现的方法
 */
static void createFuncImplStrForNormal(ClassName *className,NString *defineStr)
{
	NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
	int i;
	for(i=0;i<funcArray->len;i++){
		ClassFunc *func=(ClassFunc *)n_ptr_array_index(funcArray,i);
		if(aet_utils_valid_tree(func->fromImplDefine) && !func->isStatic){
            n_string_append(defineStr,func->rawMangleName);
            n_string_append(defineStr,",");
		}
	}
}

/**
 * 接口的方法是否在类中实现
 */
static nboolean ifaceFuncAtClass(NPtrArray *classArray,char *ifaceRawMangleName)
{
	int i;
	for(i=0;i<classArray->len;i++){
		ClassFunc *func=(ClassFunc *)n_ptr_array_index(classArray,i);
		 if(aet_utils_valid_tree(func->fromImplDefine) && !func->isStatic){
			 if(strcmp(func->rawMangleName,ifaceRawMangleName)==0){
				 return TRUE;
			 }
		}
	}
	return FALSE;
}

static nboolean isReserveRefOrUnrefMethod(char *method)
{
	 return (!strcmp(method,IFACE_REF_FIELD_NAME) ||  !strcmp(method,IFACE_UNREF_FIELD_NAME));
}

static void createFuncImplStrForAbstract(ClassName *className,NString *fieldDeclStr,NString *defineStr)
{
	char *sysName=(char *)className->sysName;
	NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
	int i;
	for(i=0;i<funcArray->len;i++){
		ClassFunc *func=(ClassFunc *)n_ptr_array_index(funcArray,i);
		if(aet_utils_valid_tree(func->fieldDecl) && func->isAbstract){
			printf("抽象类:%s的抽象方法%s",sysName,func->orgiName);
            n_string_append(fieldDeclStr,func->rawMangleName);
            n_string_append(fieldDeclStr,",");
		}
	}
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    //检查接口实现
	for(i=0;i<info->ifaceCount;i++){
		ClassName *ifaceName=&info->ifaces[i];
		NPtrArray    *ifaceArray=func_mgr_get_funcs(func_mgr_get(),ifaceName);
		int j;
		if(ifaceArray!=NULL){
			for(j=0;j<ifaceArray->len;j++){
				ClassFunc *func=(ClassFunc *)n_ptr_array_index(ifaceArray,j);
				if(aet_utils_valid_tree(func->fieldDecl) && !func->isStatic){
					//记录本类没有实现的接口方法。
					n_debug("记录本类没有实现的接口方法 %s %s\n",func->orgiName,func->rawMangleName);
					if(!ifaceFuncAtClass(funcArray,func->rawMangleName) && !isReserveRefOrUnrefMethod(func->orgiName)){
					    n_string_append(fieldDeclStr,func->rawMangleName);
					    n_string_append(fieldDeclStr,",");
					}
				}
			}
		}
	}
	for(i=0;i<funcArray->len;i++){
	   ClassFunc *func=(ClassFunc *)n_ptr_array_index(funcArray,i);
	   if(aet_utils_valid_tree(func->fromImplDefine) && !func->isStatic){
            n_string_append(defineStr,func->rawMangleName);
            n_string_append(defineStr,",");
		}
	}

}

/**
 * 类名+类型+FIELD+DEFINE+"\n"
 */
static void createFuncImplStr(ClassName *className,NString *total)
{
	char *sysName=(char *)className->sysName;
	NString *fieldDeclStr=n_string_new("");
	NString *defineStr=n_string_new("");
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(class_info_is_abstract_class(info)){
		createFuncImplStrForAbstract(className,fieldDeclStr,defineStr);
	}else{
		createFuncImplStrForNormal(className,defineStr);
	}
	n_string_append(total,sysName);
	n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	n_string_append_printf(total,"%d",info->type);
	n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	n_string_append(total,fieldDeclStr->str);
	n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	n_string_append(total,defineStr->str);
	n_string_append(total,"\n");
	n_string_free(fieldDeclStr,TRUE);
	n_string_free(defineStr,TRUE);
}

/**
 * 保存类，父类，接口信息
 * 类+类型+父类+类型+接口+接口+接口+....+\n
 */
static void ceateClassInfoStr(ClassName *className,NString *total)
{
	char *sysName=className->sysName;
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	n_string_append(total,sysName);
	n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	n_string_append_printf(total,"%d",info->type);
	n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	if(info->parentName.sysName){
		ClassInfo *parentInfo=class_mgr_get_class_info(class_mgr_get(),info->parentName.sysName);
		n_string_append(total,info->parentName.sysName);
		n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
		n_string_append_printf(total,"%d",parentInfo->type);
		n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	}else{
		n_string_append(total,"");
		n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
		n_string_append(total,"0");
		n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
	}
	int i;
    for(i=0;i<info->ifaceCount;i++){
    	ClassName *ifaceClassName=&info->ifaces[i];
    	n_string_append(total,ifaceClassName->sysName);
    	n_string_append(total,FUNC_CEHCK_SPERATOR_INFO);
    }
	n_string_append(total,"\n");
}


static void saveClassInfoAndDecl(FuncCheck *self,ClassName *className)
{
	NString *upperPortion=n_string_new("");
	ceateClassInfoStr(className,upperPortion);
	NString *lowPart=n_string_new("");
	createFuncImplStr(className,lowPart);
	xml_file_add_func_check(self->xmlFile,className->sysName,upperPortion->str,lowPart->str);
    n_string_free(upperPortion,TRUE);
    n_string_free(lowPart,TRUE);
}

static void initXmlFile(FuncCheck *self)
{
	   char *objPath=makefile_parm_get_object_root_path(makefile_parm_get());
	   if(objPath==NULL){
		  n_error("func_check_check_define 没有传递 -Dnclcompile参数");
	   }
	   char fileName[255];
	   sprintf(fileName,"%s/%s",objPath,CLASS_FUNC_CHECK_FILE);
	   self->xmlFile=xml_file_new(fileName);
}

/**
 * 检查类className的方法实现
 * 1.类本身中声明的方法
 * 2.类本身的接口
 * 3.检查父类的抽象方法
 * 4.父类的接口
 * 返回 varName和content,在classimpl.c中保存为全局变量
 * 当存当前.o文件到文件temp_func_track_45.c中
 */
void   func_check_check_define(FuncCheck *self,ClassName *className)
{
	if(self->xmlFile==NULL){
		initXmlFile(self);
	}
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	nboolean re=checkSelfFuncDefine(className);
	if(!re)
	  return;
	re=checkInterfaceFuncDefine(className);//class_interface_add_define(self->classInterface,self->className) 在做同样的事。
	if(!re)
		return;
	re=checkParentAbstractFuncDefine(className);
	if(!re)
		return;
	saveClassInfoAndDecl(self,className);
	middle_file_modify(middle_file_get());
}
///////////////////////////编译文件tmp.c触发检查类方法是否实现打开文件检查-------------------------
//static char *__var_func_track_e$_78_3331444019="/home/sns/workspace/ai/pc-build/test/AObject.o@#";
//aet_goto_compile$ 12

typedef struct _InnerClassInfo{
	char *sysName;
	ClassType type;
	char *parentName;
	ClassType parentType;
	char *ifaces[20];
	int ifaceCount;
}InnerClassInfo;


typedef struct _InnerClassImpl{
	char *sysName;
	ClassType type;
	char *fields[200];
	int fieldCount;
	char *impls[300];
	int implCount;
}InnerClassImpl;

typedef struct _OFile{
	char *file;
	char *sysName;
}OFile;

typedef struct _CollectInfo{
	InnerClassInfo *info;
	InnerClassImpl *impl;
}CollectInfo;

static void freeInnerClassInfo(InnerClassInfo *item)
{

}

static void freeInnerClassImpl(InnerClassImpl *item)
{

}

static void freeCollect(CollectInfo *collect)
{
    if(collect->info){
    	freeInnerClassInfo(collect->info);
    }
    if(collect->impl){
    	freeInnerClassImpl(collect->impl);
    }
    n_slice_free(CollectInfo,collect);
}


/**
 * 删除最后一个字符\n
 */
static void eraseLine(char *str)
{
	int len=strlen(str);
	if(str[len-1]=='\n')
	   str[len-1]='\0';
}


/**
 * 从FuncCheckData中的title取出类信息
 * title:格式如下:
 * test_ARandom@#1@#test_AObject@#0@#test_RandomGenerator@#
 * "test_ARandom":对应下面的info->sysName
 * "1"对应info->type类类型
 * "test_AObject"对应info->parentName
 * "0"对应info->parentType
 * "test_RandomGenerator"对应info->ifaces
 */
static InnerClassInfo *createClass(char *classInfoStr)
{
	  nchar **items=n_strsplit(classInfoStr,FUNC_CEHCK_SPERATOR_INFO,-1);
	  nuint length= n_strv_length(items);
	  if(length<4){
		  n_error("类信息格式不正确。%s\n",classInfoStr);
		  return NULL;
	  }
	  InnerClassInfo *info=n_slice_new0(InnerClassInfo);
	  info->sysName=n_strdup(items[0]);
	  info->type=atoi(items[1]);
	  if(items[2]!=NULL && strlen(items[2])>0){
		  info->parentName=n_strdup(items[2]);
		  info->parentType=atoi(items[3]);
	  }
	  int i;
	  for(i=4;i<length;i++){
		  if(items[i]!=NULL && strlen(items[i])>0){
            info->ifaces[info->ifaceCount++]=n_strdup(items[i]);
		  }
	  }
	  n_strfreev(items);
	  return info;
}


/**
 *  从FuncCheckData中的item取出方法信息
 *  如下格式:
 *  test_AMutex@#1@#_Z06gohomeEv,@#_Z05txaddEPv,_Z06AMutexEv,_Z026AMutex_finalize_2327176066Ev,_Z022AMutex_unref_552731270Ev
 *  对应的数据是:
 *  test_AMutex= info->sysName
 *  1=info->type
 *  _Z06gohomeEv=info->fields[0]
 *  _Z05txaddEPv=info->impls[0]
 */
static InnerClassImpl *createImpl(char *implInfoStr)
{
	  nchar **items=n_strsplit(implInfoStr,FUNC_CEHCK_SPERATOR_INFO,-1);
	  nuint length= n_strv_length(items);
	  if(length<4){
		  n_error("类方法实现的信息格式不正确。%s\n",implInfoStr);
		  return NULL;
	  }
	  InnerClassImpl *info=n_slice_new0(InnerClassImpl);
	  info->sysName=n_strdup(items[0]);
	  info->type=atoi(items[1]);
	  nchar **fields=n_strsplit(items[2],",",-1);
	  length= n_strv_length(fields);
	  int i;
	  for(i=0;i<length;i++){
		  if(fields[i]!=NULL && strlen(fields[i])>0)
            info->fields[info->fieldCount++]=n_strdup(fields[i]);
	  }
	  nchar **impls=n_strsplit(items[3],",",-1);
	  length= n_strv_length(impls);
 	  for(i=0;i<length;i++){
	 	 if(impls[i]!=NULL && strlen(impls[i])>0)
	        info->impls[info->implCount++]=n_strdup(impls[i]);
	  }
	  n_strfreev(items);
	  n_strfreev(fields);
	  n_strfreev(impls);
	  return info;
}

static CollectInfo *createCollect(char *upperPortion,char *lowerPart)
{
	  eraseLine(upperPortion);
	  eraseLine(lowerPart);
	 // printf("upperPostion:   %s\n",upperPortion);
	  //printf("lowerPart:   %s\n",lowerPart);
	  CollectInfo *collect=NULL;
	  InnerClassInfo *info=createClass(upperPortion);
	 // printf("InnerClassInfo %s %s %d %d %d\n",info->sysName,info->parentName,info->type,info->parentType,info->ifaceCount);
	  InnerClassImpl *impl=createImpl(lowerPart);
	  collect=n_slice_new(CollectInfo);
	  collect->info=info;
	  collect->impl=impl;
	  if(info->type==CLASS_TYPE_NORMAL){
		  n_debug("因为是普通类所以要检查从AObject到本身的方法是否已实现。%s",info->sysName);
	  }else if(info->type==CLASS_TYPE_ABSTRACT_CLASS){
	      n_debug("因为是抽象类所以不需要检查。%s\n",info->sysName);
	  }else{
		  n_error("接口不应该出现在这里:%s\n",info->sysName);
	  }
	  return collect;
}


static CollectInfo *getInfo(FuncCheck *self,char *sysName)
{
   int i;
   for(i=0;i<self->currentFuncArray->len;i++){
	   FuncCheckData *funccheck=(FuncCheckData *)n_ptr_array_index(self->currentFuncArray,i);
	   //printf("getInfo ---- %d %s %s\n",i,sysName,funccheck->sysName);
	   if(!strcmp(funccheck->sysName,sysName)){
	         n_debug("getInfo i:%d sysName:%s title:%s item:%s\n",i,sysName,funccheck->title,funccheck->item);
			 CollectInfo *collect=createCollect(funccheck->title,funccheck->item);
			 return collect;
	   }
   }
   return NULL;
}


/**
 * 生成类的链
 * 0是父类 1是子类，如
 * Abc extends$ AObject
 * 在array中。AObject在0索引位置。Abc在1索引位置。
 * 变成 AObject-->Abc;
 */
static void recursionLink(FuncCheck *self,CollectInfo *collect,NPtrArray *array)
{
	InnerClassInfo *info=collect->info;
	n_ptr_array_insert(array,0,collect);
    if(info->parentName==NULL){
	   return;
    }
   if(info->parentType ==CLASS_TYPE_ABSTRACT_CLASS){
       CollectInfo *newCollect=getInfo(self,info->parentName);
       n_debug("recursionLink 000 当前类是:%s 父类是:%s newCollect:%p\n",info->sysName,info->parentName,newCollect);
       if(newCollect==NULL){
    	   n_error("找不到类:%s,包含类的.c文件是否编译了。",info->parentName);
    	   return;
       }
       recursionLink(self,newCollect,array);
   }else{
	   return;
   }
}

/**
 * 在all数组中检查
 * 从from开始
 * rawFunc:要检查的函数名。
 */
static nboolean checkUp(char *rawFunc,int from ,NPtrArray *all,char *lowClass,char **implClass)
{
	int i;
	for(i=from;i<all->len;i++){
		CollectInfo *item=(CollectInfo *)n_ptr_array_index(all,i);
		InnerClassImpl *impl=item->impl;
		n_debug("checkUp--- i:%d 当前的类:%s 它的父类:%s impl->implCount:%d\n",i,item->info->sysName,lowClass,impl->implCount);
		int j;
		for(j=0;j<impl->implCount;j++){
			char *srcRawFunc=impl->impls[j];
			//printf("checkup 000 j:%d %s\n",j,srcRawFunc,rawFunc);
			if(!strcmp(srcRawFunc,rawFunc)){
			    n_debug("从第%d个类:%s的实现中找到了方法:%s与下层的类%s的方法匹配。\n",i,item->info->sysName,rawFunc,lowClass);
			   *implClass=n_strdup(item->info->sysName);
			   return TRUE;
			}
		}
	}
	return FALSE;
}



static void check(FuncCheck *self,CollectInfo *collect)
{
	NPtrArray *all=n_ptr_array_new();
	recursionLink(self,collect,all);
	int i;
	char *top=collect->info->sysName;
	for(i=0;i<all->len;i++){
		CollectInfo *collect=(CollectInfo *)n_ptr_array_index(all,i);
		InnerClassImpl *impl=collect->impl;
		//printf("check -----i:%d 最上层的类是:%s collect->info->sysName:%s impl:%s\n",i,top,collect->info->sysName,impl->sysName);
		int j;
		for(j=0;j<impl->fieldCount;j++){
			char *rawFunc=impl->fields[j];
			char *implClass=NULL;
			//printf("从第%d个检查类:%s的第%d个方法:%s有没有实现。\n",i,collect->info->sysName,j,rawFunc);
			if(!checkUp(rawFunc,i,all,collect->info->sysName,&implClass)){
				printf("从第%d个检查类:%s的第%d个方法:%s没有实现!!!!。\n",i,collect->info->sysName,j,rawFunc);
				n_error("类:%s没有实现类:%s中的方法(或者接口)%s。!",top,collect->info->sysName,rawFunc);
			}else{
				//printf("类:%s实现了类:%s中的方法:%s\n",implClass,collect->info->sysName,rawFunc);
				n_free(implClass);
			}
		}
	}
}

//static void check(FuncCheck *self,CollectInfo *collect)
//{
//	InnerClassInfo *info=collect->info;
//	//收集所有的fieldDecl，这里出现fieldDecl说明是抽象类中声明的field和抽象类中实现的接口。不是抽象类已经在类实现时就检查各个域和接口是否已实现。
//	if(info->type==CLASS_TYPE_ABSTRACT_CLASS)
//	    return;
//
//	NPtrArray *all=n_ptr_array_new();
//	recursionLink(self,collect,all);
//	int i;
//	char *top=collect->info->sysName;
//	for(i=0;i<all->len;i++){
//		CollectInfo *collect=(CollectInfo *)n_ptr_array_index(all,i);
//		InnerClassImpl *impl=collect->impl;
//		printf("check -----i:%d 最上层的类是:%s collect->info->sysName:%s impl:%s\n",i,top,collect->info->sysName,impl->sysName);
//		int j;
//		for(j=0;j<impl->fieldCount;j++){
//			char *rawFunc=impl->fields[j];
//			char *implClass=NULL;
//			printf("从第%d个检查类:%s的第%d个方法:%s有没有实现。\n",i,collect->info->sysName,j,rawFunc);
//			if(!checkUp(rawFunc,i,all,collect->info->sysName,&implClass)){
//				printf("从第%d个检查类:%s的第%d个方法:%s没有实现!!!!。\n",i,collect->info->sysName,j,rawFunc);
//				n_error("类:%s没有实现类:%s中的方法%s。!",top,collect->info->sysName,rawFunc);
//			}else{
//				printf("类:%s实现了类:%s中的方法:%s\n",implClass,collect->info->sysName,rawFunc);
//				n_free(implClass);
//			}
//		}
//	}
//}

static void checkStepThree(FuncCheck *self,NPtrArray *filter)
{
   int i;
   for(i=0;i<filter->len;i++){
	   CollectInfo *collect=(CollectInfo *)n_ptr_array_index(filter,i);
	   check(self,collect);
   }
}

/**
 * 过滤不需要的抽象类,文件不存在的类
 */
static NPtrArray *checkStepTwo(FuncCheck *self,NPtrArray *classNameArray)
{
   NPtrArray *filter=n_ptr_array_new();
   int i;
   for(i=0;i<classNameArray->len;i++){
	     FuncCheckData *funccheck=(FuncCheckData *)n_ptr_array_index(classNameArray,i);
		// printf("checkStepTwo i:%d sysName %s funccheck->title:%s\n",i,funccheck->sysName,funccheck->title);
		 CollectInfo *collect=createCollect(funccheck->title,funccheck->item);
		 if(collect->info->type==CLASS_TYPE_NORMAL){
			n_ptr_array_add(filter,collect);
		 }else{
			 freeCollect(collect);
		 }
   }
   return filter;
}

static NPtrArray *checkStepOne(FuncCheck *self)
{
	 NPtrArray *checkDataArray=xml_file_get_func_check(self->xmlFile);
	 return checkDataArray;
}

/**
 * 检查类中方法实现
 */
void func_check_check_all_define(FuncCheck *self)
{
	if(self->xmlFile==NULL){
		initXmlFile(self);
	}
	if(self->currentFuncArray!=NULL){
		n_ptr_array_unref(self->currentFuncArray);
		self->currentFuncArray=NULL;
	}
	n_debug("func_check_check_all_define 00 从文件aet_class_func_check_xml.tmp中读出FuncCheckData。");
	self->currentFuncArray=checkStepOne(self);
	if(self->currentFuncArray==NULL || self->currentFuncArray->len==0){
	    n_debug("func_check_check_all_define 11 从文件aet_class_func_check_xml.tmp中读出FuncCheckData是空的或长度是零。");
		return;
	}
	NPtrArray *filter=checkStepTwo(self,self->currentFuncArray);
	if(filter->len==0){
	    n_debug("func_check_check_all_define 22 过滤后没有需要检查的方法了。");
		return;
	}
	checkStepThree(self,filter);
}


FuncCheck *func_check_new()
{
	FuncCheck *self = n_slice_alloc0 (sizeof(FuncCheck));
	funcCheckInit(self);
	return self;
}


