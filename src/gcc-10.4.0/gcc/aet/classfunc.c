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
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"

#include "c-aet.h"
#include "aetutils.h"
#include "classmgr.h"
#include "funcmgr.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "aet-c-parser-header.h"
#include "classfunc.h"
#include "genericutil.h"
#include "classutil.h"


static void classFuncInit(ClassFunc *self)
{
	   self->haveGenericBlock=FALSE;
	   self->isQueryGenFunc=FALSE;
	   self->isGenericParmFunc=FALSE;
	   self->allParmIsQuery=FALSE;
	   self->isFinal=FALSE;

}

static tree getValid(ClassFunc *self)
{
	tree value=NULL_TREE;
    if(aet_utils_valid_tree(self->fieldDecl)){
    	value=self->fieldDecl;
    }else if(aet_utils_valid_tree(self->fromImplDefine)){
    	value=self->fromImplDefine;
    }else if(aet_utils_valid_tree(self->fromImplDecl)){
    	value=self->fromImplDecl;
    }
    return value;
}

static tree getRtn(tree value)
{
	tree type=TREE_TYPE(value);
    if(TREE_CODE(type)==POINTER_TYPE)
	    type=TREE_TYPE(type);
	if(TREE_CODE(type)==FUNCTION_TYPE){
		return TREE_TYPE (type);
	}
	return NULL_TREE;
}


nboolean class_func_is_same_but_rtn(ClassFunc *self,ClassFunc *compare,tree readyDecl)
{
	//printf("class_func_is_same_but_rtn %s %s %s %s\n",self->rawMangleName,
			//compare->rawMangleName,self->mangleFunName,compare->mangleFunName);
    if(strcmp(self->rawMangleName,compare->rawMangleName)==0){
        tree t1=getValid(self);
        tree t2=readyDecl;//getValid(compare);
       	//aet_print_tree(t1);
        //aet_print_tree(t2);
        if(t1 && t2){
        	//printf("class_func_is_same_but_rtn 11 %s %s\n",self->rawMangleName,compare->rawMangleName);
        	tree rtn1=getRtn(t1);
        	tree rtn2=getRtn(t2);
        	//aet_print_tree(rtn1);
        	//aet_print_tree(rtn2);
        	if(rtn1 && rtn2){
               nboolean re=c_tree_equal(rtn1,rtn2);
               return !re;
        	}else{
        		n_error("不应该出现这种情况.%s %s",self->orgiName,compare->orgiName);
        	}
        }else{
    		n_error("不应该出现这种情况.%s %s",self->orgiName,compare->orgiName);
        }
    }
    return FALSE;
}

static nboolean checkRtn(ClassFunc *self)
{
	//检查返回值
	int count=0;
	tree xx[3];
	if(aet_utils_valid_tree(self->fieldDecl)){
		xx[count++]=self->fieldDecl;
	}
	if(aet_utils_valid_tree(self->fromImplDefine)){
		xx[count++]=self->fromImplDefine;
	}
	if(aet_utils_valid_tree(self->fromImplDecl)){
		xx[count++]=self->fromImplDecl;
	}
	if(count<=1)
		return TRUE;
	int i;
	for(i=0;i<count;i++){
		tree t1=getRtn(xx[i]);
		tree t2=getRtn(xx[i+1]);
		//aet_print_tree(t1);
		//aet_print_tree(t2);

		nboolean re=c_tree_equal(t1,t2);

		//aet_print_tree(xx[i]);
		//aet_print_tree(xx[i+1]);
		if(!re){
			error_at(DECL_SOURCE_LOCATION(xx[i]),
					"在类中声明的函数或在类实现中声明或实现的函数，函数名和参数一样，但返回值不同。%qs",self->orgiName);
		}
		i+=1;
		if(i==count-1)
			break;
	}
	return TRUE;
}

nboolean class_func_set_decl(ClassFunc *self,tree decl,enum func_from_code code)
{
	if(code==STRUCT_DECL){
		self->fieldDecl=decl;
	}else if(code==CLASS_IMPL_DECL){
		self->fromImplDecl=decl;
	}else if(code==CLASS_IMPL_DEFINE){
		self->fromImplDefine=decl;
	}
	return checkRtn(self);
}



static nboolean compareGeneric(GenericModel *srcGen,tree args,int order)
{
	int i=0;
	tree parm;
	for (parm = args; parm; parm = DECL_CHAIN (parm)){
	   if(order==i){
		   GenericModel *destGen=c_aet_get_generics_model(parm);
		   if(!srcGen && !destGen)
			   return TRUE;
		   nboolean re=generic_model_equal(srcGen,destGen);
		   return re;
	   }
	   i++;
	}
	return FALSE;
}



/**
 * 比较field,funcdecl,funcdefine中的泛型类的泛型是否相同
 */
static nboolean compareGenericParm(ClassFunc *self)
{
	    //检查返回值
		int count=0;
		tree xx[3];
		if(aet_utils_valid_tree(self->fieldDecl) && aet_utils_valid_tree(self->fieldParms)){
			xx[count++]=self->fieldParms;
		}
		if(aet_utils_valid_tree(self->fromImplDefine)){
			xx[count++]=DECL_ARGUMENTS (self->fromImplDefine);
		}
		if(aet_utils_valid_tree(self->fromImplDecl)){
			xx[count++]=DECL_ARGUMENTS (self->fromImplDecl);
		}
		if(count<=1)
			return TRUE;
		int i;
		for(i=0;i<count;i++){
			tree args=xx[i];
			tree args1=xx[i+1];
			int order=0;
			nboolean result=TRUE;
			tree parm;
			for (parm = args; parm; parm = DECL_CHAIN (parm)){
				   char *className=class_util_get_class_name(TREE_TYPE(parm));
				   if(className!=NULL){
                      ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),className);
                      if(class_info_is_generic_class(info)){
               		      GenericModel *generic=c_aet_get_generics_model(parm);
                    	  result=compareGeneric(generic,args1,order);
                    	  if(!result)
                    		  break;
                      }
				   }
				   order++;
			}
			if(!result){
				error_at(DECL_SOURCE_LOCATION(xx[i]),
						"在类中声明的函数或在类实现中声明或实现的函数，函数名和参数类型一样，但参数的泛型不一样。%qs",self->orgiName);
			}
			i+=1;
			if(i==count-1)
				break;
		}
		return TRUE;
}

/**
 * 是否是相同的泛型函数
 */
static nboolean sameFuncGeneric(ClassFunc *self,nboolean funcFunc)
{
	//检查返回值
	int count=0;
	tree xx[3];
	if(aet_utils_valid_tree(self->fieldDecl)){
		xx[count++]=self->fieldDecl;
	}
	if(aet_utils_valid_tree(self->fromImplDefine)){
		xx[count++]=self->fromImplDefine;
	}
	if(aet_utils_valid_tree(self->fromImplDecl)){
		xx[count++]=self->fromImplDecl;
	}
	if(count<=1)
		return TRUE;
	int i;
	for(i=0;i<count;i++){
		GenericModel *t1=c_aet_get_func_generics_model(xx[i]);
		if(!funcFunc)
			t1=c_aet_get_generics_model(xx[i]);
		GenericModel *t2=c_aet_get_func_generics_model(xx[i+1]);
		if(!funcFunc)
				t2=c_aet_get_generics_model(xx[i+1]);
		nboolean fffxx=class_func_is_func_generic(self);
		//printf("vvv iss ----%d 泛型函数:%d %p %p %p %p %s\n",i,fffxx,xx[i],xx[i+1],t1,t2,self->orgiName);
		nboolean re=generic_model_equal(t1,t2);
		if(!re){
			printf("class_func_is_same_generic 不是相同的泛型函数 下面打印两个泛型 相等吗:%d %p %p %p %p\n",re,self->fieldDecl,self->fromImplDefine,t1,t2);
			printf("ssss t1 %s\n",generic_model_tostring(t1));
			printf("ssss t2 %s\n",generic_model_tostring(t2));
			return FALSE;
		}
		i+=1;
		if(i==count-1)
			break;
	}
	return TRUE;
}

static tree getValidDecl(ClassFunc *self)
{
		tree decl=NULL_TREE;
		if(aet_utils_valid_tree(self->fieldDecl)){
			decl=self->fieldDecl;
		}else if(aet_utils_valid_tree(self->fromImplDefine)){
			decl=self->fromImplDefine;
		}else if(aet_utils_valid_tree(self->fromImplDecl)){
			decl=self->fromImplDecl;
		}
		return decl;
}

/**
 * 1.泛型函数
 * 2.函数返回值
 * 3.参数中的泛型是不是相同
 */
nboolean class_func_is_same_generic(ClassFunc *self)
{
	nboolean re=sameFuncGeneric(self,TRUE);
	if(!re){
		tree decl=getValidDecl(self);
		error_at(DECL_SOURCE_LOCATION(decl),
							"在类中声明或类实现的函数是泛型函数，但它们的泛型并不一致。%qs",self->orgiName);
		return FALSE;
	}
	re=sameFuncGeneric(self,FALSE);
	if(!re){
		tree decl=getValidDecl(self);
		error_at(DECL_SOURCE_LOCATION(decl),
							"在类中声明或类实现的函数返回值是泛型类型的类，但它们的泛型并不一致。%qs",self->orgiName);
		return FALSE;
	}
	re=compareGenericParm(self);
	return re;
}


/**
 * 是否泛型函数
 */
nboolean class_func_is_func_generic(ClassFunc *self)
{
	GenericModel *funcGeneric=class_func_get_func_generic(self);
	return funcGeneric!=NULL;
}

GenericModel *class_func_get_func_generic(ClassFunc *self)
{
	if(self==NULL)
		return NULL;
	GenericModel *funcGeneric=NULL;
	if(!aet_utils_valid_tree(self->fieldDecl))
		return NULL;
    funcGeneric=c_aet_get_func_generics_model(self->fieldDecl);
	return funcGeneric;
}

static void freeParmInfo_cb(ParmGenInfo *item)
{
	if(item->str!=NULL){
		n_free(item->str);
	}
}


/**
 * 获取泛型在第几个参数上
 * ID 是 E、T等字符
 */
NPtrArray *class_func_get_generic_parm(ClassFunc *self,char *id)
{
	tree funcType=NULL_TREE;
	tree args=NULL_TREE;
	if(aet_utils_valid_tree(self->fieldDecl)){
		funcType=TREE_TYPE(self->fieldDecl);
		funcType=TREE_TYPE(funcType);
		args=self->fieldParms;
	}else if(aet_utils_valid_tree(self->fromImplDefine)){
		funcType=TREE_TYPE(self->fromImplDefine);
		args=DECL_ARGUMENTS(self->fromImplDefine);
	}else if(aet_utils_valid_tree(self->fromImplDecl)){
		funcType=TREE_TYPE(self->fromImplDecl);
		args=DECL_ARGUMENTS(self->fromImplDecl);
	}

	char *genStr=id;
	tree parm=NULL_TREE;
	int count=0;
	NPtrArray *array=n_ptr_array_new_with_free_func(freeParmInfo_cb);
	/**
	 * 跳过self与FuncGenParmInfo tempFgpi1234 两个参数
	 */
	for (parm = args; parm; parm = DECL_CHAIN (parm)){
		tree type=TREE_TYPE(parm);
		if(count==0 || count==1){
			count++;
			continue;
		}
	    nboolean isGeneric=generic_util_is_generic_pointer(type);
	    n_debug("class_func_get_generic_parm 11 现有参数:%d 是不是泛型 T,E等:%d\n",count,isGeneric);
		if(!isGeneric){
          GenericModel *varGen=c_aet_get_generics_model(parm);
  		  if(varGen){
  		    n_debug("class_func_get_generic_parm 22ccc 现有参数:%d 是不是泛型 T,E等:%d\n",count,isGeneric);
  			 nboolean exits=generic_model_exits_ident(varGen,genStr);
  			 if(exits){
  			   n_debug("class_func_get_generic_parm 22 参数:%d 泛型在对象中有泛型声明:%s\n",count,genStr);
			    ParmGenInfo *pgi=(ParmGenInfo *)n_slice_new(ParmGenInfo);
				pgi->str=n_strdup(genStr);
				pgi->index=count;
				pgi->independ=FALSE;
				pgi->object=type;
				pgi->unitPos=generic_model_get_index(varGen,genStr);
				n_ptr_array_add(array,pgi);
  			 }
  		  }
		}else{
			char *str=generic_util_get_generic_str(type);
			if(strcmp(str,genStr)==0){
			    n_debug("class_func_get_generic_parm 33 参数:%d 泛型声明:%s\n",count,str);
				ParmGenInfo *pgi=(ParmGenInfo *)n_slice_new(ParmGenInfo);
				pgi->str=n_strdup(str);
				pgi->index=count;
				pgi->independ=TRUE;
				pgi->object=NULL_TREE;
				n_ptr_array_add(array,pgi);
			}
		}
		//aet_print_tree(parm);
		count++;
	}
	return array;
}

/**
 * 在函数中是否有泛型块
 */
void   class_func_set_generic_block(ClassFunc *self,nboolean have)
{
   self->haveGenericBlock=have;
}

nboolean  class_func_have_generic_block(ClassFunc *self)
{
   return self->haveGenericBlock;
}

nboolean    class_func_is_query_generic(ClassFunc *self)
{
	return self->isQueryGenFunc;
}

nboolean    class_func_have_generic_parm(ClassFunc *self)
{
	return self->isGenericParmFunc;
}

void   class_func_set_final(ClassFunc *self,nboolean isFinal)
{
	   self->isFinal=isFinal;
}

nboolean    class_func_is_abstract(ClassFunc *self)
{
	return self->isAbstract;
}

nboolean    class_func_is_final(ClassFunc *self)
{
	return self->isFinal;
}

nboolean    class_func_have_super(ClassFunc *self)
{
	 if(!self->isCtor && !self->isAbstract &&!self->isFinalized && !self->isUnref){
           return TRUE;
	 }
	 return FALSE;
}

nboolean    class_func_have_all_query_parm(ClassFunc *self)
{
	return self->allParmIsQuery;
}

nboolean    class_func_is_public(ClassFunc *self)
{
   if(self==NULL)
	   return FALSE;
   return self->permission==CLASS_PERMISSION_PUBLIC;
}

nboolean    class_func_is_protected(ClassFunc *self)
{
	   if(self==NULL)
		   return FALSE;
	   return self->permission==CLASS_PERMISSION_PROTECTED;
}

nboolean    class_func_is_private(ClassFunc *self)
{
	   if(self==NULL)
		   return FALSE;
	   return self->permission==CLASS_PERMISSION_PRIVATE;
}

/**
 * 是不是接口保留的方法名。
 * 在class_parser.c中加入接口的两个方法是 ref和unref具体名字见下:
 *#define IFACE_REF_FIELD_NAME            "_iface_reserve_ref_field_123"
 *#define IFACE_UNREF_FIELD_NAME          "_iface_reserve_unref_field_123"
 */
nboolean    class_func_is_interface_reserve(ClassFunc *self)
{
	  if(self==NULL)
		  return FALSE;
	  n_debug("class_func_is_interface_reserve %s %s %s\n",self->orgiName,IFACE_REF_FIELD_NAME,IFACE_UNREF_FIELD_NAME);
	  return (!strcmp(self->orgiName,IFACE_REF_FIELD_NAME) || !strcmp(self->orgiName,IFACE_UNREF_FIELD_NAME));
}


ClassFunc *class_func_new()
{
	 ClassFunc *self =n_slice_alloc0 (sizeof(ClassFunc));
     classFuncInit(self);
	 return self;
}


