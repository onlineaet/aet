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

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
//#include "c-family/c-objc.h"
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
#include "superdefine.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"

static void superDefineInit(SuperDefine *self)
{
}

static nboolean exitsFunc(NPtrArray *funcsArray,ClassFunc *childFunc)
{
	    int i=0;
	    for(i=0;i<funcsArray->len;i++){
	       ClassFunc *func=(ClassFunc *)n_ptr_array_index(funcsArray,i);
		   if(strcmp(func->rawMangleName,childFunc->rawMangleName)==0){
			    return TRUE;
		   }
	    }
		return FALSE;
}


static nboolean recursionFind(ClassName *childName,ClassName *parent,ClassFunc *childFunc)
{
   if(parent==NULL)
	   return FALSE;
   NPtrArray    *childFuncsArray=func_mgr_get_funcs(func_mgr_get(),childName);
   ClassInfo    *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parent);
   NPtrArray    *parentFuncsArray=func_mgr_get_funcs(func_mgr_get(),parent);
   nboolean find=exitsFunc(parentFuncsArray,childFunc);
   if(find)
	   return TRUE;
   int i;
   for(i=0;i<parentInfo->ifaceCount;i++){
        ClassName *iface=&(parentInfo->ifaces[i]);
        NPtrArray    *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
        find=exitsFunc(ifaceFuncsArray,childFunc);
        if(find)
            return TRUE;
   }
   if(parentInfo->parentName.sysName==NULL)
      return FALSE;
   return recursionFind(childName,&parentInfo->parentName,childFunc);
}


static nboolean findMethod(ClassName *childName,ClassFunc *childFunc)
{
      ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),childName);
      if(info==NULL){
   	     return FALSE;
      }
        /**是本类的接口方法的实现吗*/
	  int i;
	  for(i=0;i<info->ifaceCount;i++){
		 ClassName *iface=&(info->ifaces[i]);
		 NPtrArray    *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
		 nboolean find=exitsFunc(ifaceFuncsArray,childFunc);
		 if(find)
			 return TRUE;
	  }
	  if(info->parentName.sysName==NULL)
	      return FALSE;
	  return recursionFind(childName,&info->parentName,childFunc);
}

/**
 * 往AObject的superCallData加函数地址
 */
char *super_define_add(SuperDefine *self,ClassName *className)
{
	NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
	int i;
	NString *codes=n_string_new("");
	for(i=0;i<funcArray->len;i++){
        ClassFunc *func=n_ptr_array_index(funcArray,i);
        nboolean  canSuper= class_func_have_super(func);
        if(!canSuper)
        	continue;
		if(aet_utils_valid_tree(func->fieldDecl)){
			//决对要加super.如果是私有的不能加
			if(!class_func_is_private(func)){
			   if(aet_utils_valid_tree(func->fromImplDefine))
                   n_string_append_printf(codes,"%s((%s *)self,\"%s\",\"%s\",(unsigned long)%s);\n",
                		   ADD_SUPER_METHOD,AET_ROOT_OBJECT,className->sysName,func->rawMangleName,func->mangleFunName);
			   else{
				   n_warning("没有定义只有声明:%s %s\n",className->sysName,func->orgiName);
			   }
			}
		}else if(aet_utils_valid_tree(func->fromImplDefine)){
			//检查是覆盖父类的field方法或本类的接口或父类的接口
			nboolean find=findMethod(className,func);
			if(find){
	             n_string_append_printf(codes,"%s((%s *)self,\"%s\",\"%s\",(unsigned long)%s);\n",
	                		   ADD_SUPER_METHOD,AET_ROOT_OBJECT,className->sysName,func->rawMangleName,func->mangleFunName);
			}
		}
	}
	n_debug("生成的super源代码是:%s\n",codes->str);
	char *result=NULL;
	if(codes->len>0)
		result=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	return result;
}




SuperDefine *super_define_new()
{
	SuperDefine *self = n_slice_alloc0 (sizeof(SuperDefine));
	superDefineInit(self);
	return self;
}


