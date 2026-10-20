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
#include "aet-c-parser-header.h"

#include "c-aet.h"
#include "aetutils.h"
#include "classmgr.h"
#include "funcmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "classfunc.h"
#include "genericutil.h"
#include "classutil.h"
#include "mtcsinfo.h"


static void classFuncInit(ClassFunc *self)
{
   self->genBlockCount=0;
   self->isQueryGenFunc=FALSE;
   self->isGenericParmFunc=FALSE;
   self->allParmIsQuery=FALSE;
   self->isFinal=FALSE;
   self->className = NULL;
   self->classTree= NULL_TREE;
   self->mtcsFuncType = MTCS_FUNC_NOT;
   self->isMtcsFunc = FALSE;
   self->fromInterface = FALSE;
   self->isDivide=FALSE;
   int i;
   for(i=0;i<50;i++)
      self->parmsGenModel[i]=NULL;

}

static tree getValid(ClassFunc *self)
{
	tree value=NULL_TREE;
    if(aet_utils_valid_tree(self->fieldDecl)){
    	value=self->fieldDecl;
    }else if(aet_utils_valid_tree(self->fromImplDefine)){
    	value=self->fromImplDefine;
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
   if(!aet_utils_valid_tree(self->fieldDecl) || !aet_utils_valid_tree(self->fromImplDefine))
      return TRUE;
   tree t1=getRtn(self->fieldDecl);
   tree t2=getRtn(self->fromImplDefine);
   nboolean re=c_tree_equal(t1,t2);
   if(!re){
      error_at(DECL_SOURCE_LOCATION(self->fieldDecl),
      "类声明和定义函数，名称和参数一样，但返回值不同。%qs",self->orgiName);
      return FALSE;
   }
   return TRUE;
}

nboolean class_func_set_decl(ClassFunc *self,tree decl,enum func_from_code code)
{
	if(code==STRUCT_DECL){
		self->fieldDecl=decl;
	}else if(code==CLASS_IMPL_DEFINE){
		self->fromImplDefine=decl;
	}
	class_func_set_mtcs_info(self,DECL_SOURCE_LOCATION(decl),decl);
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
 * fieldParms已从classfunc中移走
 */
static nboolean compareGenericParm(ClassFunc *self)
{
   //检查返回值
   if(!aet_utils_valid_tree(self->fieldDecl) || !aet_utils_valid_tree(self->fromImplDefine))
      return TRUE;
   tree parm;
   tree funcType=TREE_TYPE(TREE_TYPE(self->fieldDecl));
   int order=0;
   tree define=DECL_ARGUMENTS (self->fromImplDefine);
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree parm = TREE_VALUE (al);
      char *className=class_util_get_class_name(parm);
      if(className!=NULL){
         ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),className);
         if(class_info_is_generic_class(info)){
            GenericModel *generic=self->parmsGenModel[order];
            nboolean result=compareGeneric(generic,define,order);
            if(!result){
               error_at(DECL_SOURCE_LOCATION(define),"在类中声明的方法与类实现中的方法名和参数类型一样，但参数的泛型不一样。%qs",self->orgiName);
               return FALSE;
            }
         }
      }
      order++;
   }
   return TRUE;
}

/**
 * 是否是相同的泛型函数
 */
static nboolean sameFuncGeneric(ClassFunc *self,nboolean funcFunc)
{
   if(!aet_utils_valid_tree(self->fieldDecl) || !aet_utils_valid_tree(self->fromImplDefine))
      return TRUE;
   GenericModel *t1=c_aet_get_func_generics_model(self->fieldDecl);
   if(!funcFunc)
      t1=c_aet_get_generics_model(self->fieldDecl);
   GenericModel *t2=c_aet_get_func_generics_model(self->fromImplDefine);
   if(!funcFunc)
      t2=c_aet_get_generics_model(self->fromImplDefine);
   if(t2==NULL)//泛型函数定义可以不加泛型类型修饰。
      return TRUE;
   nboolean fffxx=class_func_is_func_generic(self);
   //printf("vvv iss ----%d 泛型函数:%d %p %p %p %p %s\n",i,fffxx,xx[i],xx[i+1],t1,t2,self->orgiName);
   nboolean re=generic_model_equal(t1,t2);
   if(!re){
      printf("class_func_is_same_generic 不是相同的泛型函数 下面打印两个泛型 相等吗:%d %p %p %p %p\n",re,self->fieldDecl,self->fromImplDefine,t1,t2);
      printf("ssss t1 %s\n",generic_model_tostring(t1));
      printf("ssss t2 %s\n",generic_model_tostring(t2));
      return FALSE;
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
static void addGenericParm(char *genStr,tree type,int count,NPtrArray *array,GenericModel *varGen)
{
   nboolean isGeneric=generic_util_is_generic_pointer(type);
   n_debug("class_func_get_generic_parm 11 现有参数:%d 是不是泛型 T,E等:%d",count,isGeneric);
   if(!isGeneric){
      if(varGen){
         n_debug("class_func_get_generic_parm 22ccc 现有参数:%d 是不是泛型 T,E等:%d",count,isGeneric);
         nboolean exits=generic_model_exits_ident(varGen,genStr);
         if(exits){
            n_debug("class_func_get_generic_parm 22 参数:%d 泛型在对象中有泛型声明:%s",count,genStr);
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
      const char *str=generic_util_get_generic_decl_string(type);
      if(strcmp(str,genStr)==0){
         n_debug("class_func_get_generic_parm 33 参数:%d 泛型声明:%s",count,str);
         ParmGenInfo *pgi=(ParmGenInfo *)n_slice_new(ParmGenInfo);
         pgi->str=n_strdup(str);
         pgi->index=count;
         pgi->independ=TRUE;
         pgi->object=NULL_TREE;
         n_ptr_array_add(array,pgi);
      }
   }
}

/**
 * 跳过self参数
 */
NPtrArray *class_func_get_generic_parm(ClassFunc *self,char *id)
{
   int count=0;
   tree parm=NULL_TREE;
   NPtrArray *array=n_ptr_array_new_with_free_func(freeParmInfo_cb);
   if(aet_utils_valid_tree(self->fieldDecl)){
      tree funcType=TREE_TYPE(TREE_TYPE(self->fieldDecl));
      int order=0;
      for (parm = TYPE_ARG_TYPES (funcType); parm; parm = TREE_CHAIN (parm)){
         tree type = TREE_VALUE (parm);
         if(count==0){
            count++;
            continue;
         }
         GenericModel *varGen=self->parmsGenModel[count];
         addGenericParm(id,type,count,array,varGen);
         count++;
      }
   }else if(aet_utils_valid_tree(self->fromImplDefine)){
      tree args=DECL_ARGUMENTS(self->fromImplDefine);
      for (parm = args; parm; parm = DECL_CHAIN (parm)){
         tree type=TREE_TYPE(parm);
         if(count==0){
            count++;
            continue;
         }
         GenericModel *varGen=c_aet_get_generics_model(parm);
         addGenericParm(id,type,count,array,varGen);
         count++;
      }
   }
   return array;
}

/**
 * 在函数中是否有泛型块
 */
void   class_func_add_generic_block(ClassFunc *self)
{
   self->genBlockCount++;
}

nboolean  class_func_have_generic_block(ClassFunc *self)
{
   return self->genBlockCount>0;
}

nboolean    class_func_have_query_param(ClassFunc *self)
{
	return self->isQueryGenFunc;
}

nboolean    class_func_have_generic_class_parm(ClassFunc *self)
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

/**
 * 函数是否支持super调用
 */
nboolean  class_func_have_super(ClassFunc *self)
{
   if(!strcmp(self->className->userName,AET_ROOT_OBJECT) && !strcmp(self->orgiName,"getClass"))
      return FALSE;
   if(!self->isCtor /*&& !self->isAbstract*/
         &&!self->isFinalized && !self->isUnref
         && !class_func_is_interface_reserve(self)){
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
   //n_debug("class_func_is_interface_reserve %s %s %s",self->orgiName,IFACE_REF_FIELD_NAME,IFACE_UNREF_FIELD_NAME);
   return (!strcmp(self->orgiName,IFACE_REF_FIELD_NAME) || !strcmp(self->orgiName,IFACE_UNREF_FIELD_NAME));
}

nboolean  class_func_is_static(ClassFunc *self)
{
   return self->isStatic;
}


/**
 * 从类声明(也就是field_decl)的参数中取出genericmodel存入classFunc中的parmsGenModel;
 */
void class_func_save_generic_model_for_field_decl(ClassFunc *self,tree args)
{
    tree parm;
    int count=0;
    int genCount=0;
    for (parm = args; parm; parm = DECL_CHAIN (parm)){
        GenericModel *genModel=(GenericModel *)c_aet_get_generics_model(parm);
        if(genModel!=NULL){
           //把参数中的泛型声明清除，因为会重用parm
           c_aet_set_generics_model(parm,NULL);
           genCount++;
        }else{
           if(count==0){
              //self没有GenericModel 但也可能是泛型类，所以泛型参数个数据也要加上self
               char *sysName= class_func_get_class_name(self);
                ClassName *className = class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
                nboolean selfAboutGen = class_mgr_about_generic(class_mgr_get(),className);
                if(selfAboutGen)
                   genCount++;
           }
        }
        self->parmsGenModel[count++]=genModel;
    }
    self->parsmGenModeCount=genCount;
}

/**
 * 获取泛型参数个数
 */
int class_func_get_generic_param_count(ClassFunc *self)
{
   if(self->parsmGenModeCount>0)
      return self->parsmGenModeCount;
   int count=0;
   if(self->fromImplDefine){
      tree args = DECL_ARGUMENTS (self->fromImplDefine);
      tree parm=NULL_TREE;
      for (parm = args; parm; parm = DECL_CHAIN (parm)){
         GenericModel *genModel=(GenericModel *)c_aet_get_generics_model(parm);
         n_debug("class_func_get_generic_param_count 00 获取泛型参数个数 %p\n",genModel);
         if(genModel!=NULL)
            count++;
         else{
            if(count==0){
               //self没有GenericModel 但也可能是泛型类，所以泛型参数个数据也要加上self
               char *sysName= class_func_get_class_name(self);
               ClassName *className = class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
               nboolean selfAboutGen = class_mgr_about_generic(class_mgr_get(),className);
               if(selfAboutGen)
                  count++;
            }
         }
      }
   }
   return count;
}

//ClassFunc 是否有类名
nboolean    class_func_have_class_name(ClassFunc *self)
{
   return (self->classTree!=NULL ||  self->className!=NULL);
}

const char *class_func_get_class_name(ClassFunc *self)
{
   if(self->classTree==NULL && self->className==NULL){
      n_error("函数:%s,没有类信息",self->mangleFunName);
   }
   if(self->className!=NULL){
      return self->className->sysName;
   }else{
      const char *className=IDENTIFIER_POINTER(TYPE_NAME(self->classTree));// class_util_get_class_name(func->classTree);
      className=className+1;//去除下划线 _debug_ARandom
      return className;
   }
}

nboolean  class_func_is_mtcs(ClassFunc *self)
{
   return self->isMtcsFunc;
}

void  class_func_set_mtcs(ClassFunc *self,nboolean isMtcs)
{
   self->isMtcsFunc = isMtcs;
}

void   class_func_set_mtcs_type(ClassFunc *self,MtcsFuncType type)
{
   if(self->isMtcsFunc)
      self->mtcsFuncType = type;
   else
      gcc_unreachable ();
}

MtcsFuncType class_func_get_mtcs_type(ClassFunc *self)
{
   return self->mtcsFuncType;
}

nboolean class_func_is_kernel(ClassFunc *self)
{
   return self->mtcsFuncType==MTCS_FUNC_KERNEL;
}

nboolean class_func_is_device(ClassFunc *self)
{
   return self->mtcsFuncType==MTCS_FUNC_DEVICE || self->mtcsFuncType==MTCS_FUNC_DEVICE_HOST;

}

nboolean class_func_is_host(ClassFunc *self)
{
   return self->mtcsFuncType==MTCS_FUNC_DEVICE_HOST;
}

ClassFunc  *class_func_clone(ClassFunc *self,tree newFieldDecl,char **names,tree classTree ,ClassName *className)
{
   ClassFunc *dest = class_func_new();
   dest->orgiName =names[0];//原来的名字
   dest->mangleFunName=names[1];
   dest->mangleNoSelfName=names[2];
   dest->rawMangleName=names[3];
   dest->classTree = classTree;
   dest->fieldDecl=newFieldDecl;//域成员
   dest->isAbstract =self->isAbstract; //是否抽象方法
   dest->isCtor=self->isCtor; //是构造方法
   dest->isFinalized=self->isFinalized; //是释放方法
   dest->isUnref=self->isUnref; //是反引用
   dest->isStatic=self->isStatic;//是不是静态函数
   dest->isQueryGenFunc=self->isQueryGenFunc;    //是不是有问号泛型参数的函数。
   dest->isGenericParmFunc=self->isGenericParmFunc;//是不是有泛型类参数的函数
   //方法中的参数的泛型 因为field只有类型没有参数，所以从struct c_declarator *declarator取出参数保存在这里
   int i;
   for(i=0;i<self->parsmGenModeCount;i++)
      dest->parmsGenModel[i]=generic_model_clone(self->parmsGenModel[i]);
   dest->parsmGenModeCount=self->parsmGenModeCount; //泛型参数个数，如果self也是泛型类也包括。
   dest->permission=self->permission;
   dest->genBlockCount=self->genBlockCount;//是否有泛型块
   dest->isFinal=self->isFinal;
   dest->allParmIsQuery=self->allParmIsQuery;//是不是所有泛型类参数都是问号泛型的函数
   dest->serialNumber=self->serialNumber;//在class中的序号
   dest->isMtcsFunc=self->isMtcsFunc;//是否mtcs函数 根据是否有__global__或__device__属性来判断
   dest->mtcsFuncType=self->mtcsFuncType;
   dest->isDivide =self->isDivide;
   dest->divideSrc =self->divideSrc;
   dest->className = class_name_clone(className);//所在的class或接口
   dest->parms = self->parms;
   return dest;
}

//克隆是来自host_device classFunc
//现在作为设备函数，原来的作为主机函数
void   class_func_set_divide(ClassFunc *self,nboolean isDivide,ClassFunc *divideSrc)
{
   self->isDivide = isDivide;
   self->divideSrc= divideSrc;
}

ClassFunc *class_func_get_divide_src(ClassFunc *self)
{
   return self->divideSrc;
}


nboolean class_func_is_divide(ClassFunc *self)
{
   return self->isDivide;
}

/**
 * 是不是一个普通方法
 */
nboolean    class_func_is_normal(ClassFunc *self)
{
   if(!self)
      return FALSE;
   return (!self->isAbstract &&
   !self->isCtor &&
   !self->isFinalized &&
   !self->isUnref &&
   !self->isMtcsFunc);
}

void class_func_set_end_location(ClassFunc *self,location_t endLoc)
{
    if(!self)
       return;
    self->endLoc = endLoc;
}

//获取参数个数据
int   class_func_get_param_count(ClassFunc *self)
{
   if(!self)
      return 0;
   tree funcType=NULL;
   if(self->fieldDecl){
      tree fieldType=TREE_TYPE(self->fieldDecl);
      funcType=TREE_TYPE(fieldType);
   }else if(self->fromImplDefine){
      funcType=TREE_TYPE(self->fromImplDefine);
   }else{
      n_error("classfunc无声明和定义%s\n",self->orgiName);
   }

   int count=0;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      if(type == void_type_node)
         break;
      count++;
   }
   return count;
}

static void addMiiAttribute(ClassFunc *func,tree fndecl)
{
   ClassName *cn=func->className;
   ClassInfo *info = class_mgr_get_class_info_by_class_name(class_mgr_get(),func->className);
   if(class_info_is_mii(info) &&
         class_func_is_static(func) &&
         class_func_get_mtcs_type(func) == MTCS_FUNC_DEVICE &&
         !lookup_attribute (MTCS_INTRINSIC_FUNCTION_IMPLEMENTS, DECL_ATTRIBUTES (fndecl))){
      //说明函数已有属性 mii
//      tree fnImplAtt=lookup_attribute (MTCS_INTRINSIC_FUNCTION_IMPLEMENTS, DECL_ATTRIBUTES (fndecl));
//      if(fnImplAtt)
//         return;
      tree miiAtt=  class_info_get_mii_attribute(info);
      gcc_assert(miiAtt);
      tree intrinsic = NULL_TREE;
      tree list = NULL_TREE;
      //classinfo中的属性 __attribute__((mii("cuda","sm_35"))) 复制到函数中
       int ix=0;
       for (tree a = TREE_VALUE (miiAtt); a; a = TREE_CHAIN (a)){
          tree d = TREE_VALUE (a);
          list=tree_cons (NULL_TREE, d, list);
          ix++;
       }
       if(ix==0){
          list=tree_cons (NULL_TREE, get_identifier("default"), list);
          list=tree_cons (NULL_TREE, get_identifier("default"), list);
       }else if(ix==1){
          list=tree_cons (NULL_TREE, get_identifier("default"), list);
       }
       list = nreverse(list);
       intrinsic = build_tree_list (get_identifier(MTCS_INTRINSIC_FUNCTION_IMPLEMENTS),list);
       tree attrs = DECL_ATTRIBUTES (fndecl);
       if(!attrs)
          DECL_ATTRIBUTES (fndecl) = tree_cons (intrinsic,NULL_TREE,DECL_ATTRIBUTES (fndecl));
       else{
          attrs=chainon (attrs, intrinsic);
       }
       /*
       tree mtcs1=lookup_attribute (MTCS_INTRINSIC_FUNCTION_IMPLEMENTS, DECL_ATTRIBUTES (fndecl));
       printf("测试是不是实现----\n");
       aet_print_tree(DECL_ATTRIBUTES (fndecl));
       aet_print_tree(mtcs1);
       */
   }
}

/**
 * 是否有可变参数
 */
static nboolean haveVariableParam (tree decl)
{
   tree funcType=TREE_TYPE(decl);
   if(TREE_CODE(funcType)==POINTER_TYPE)
      funcType=TREE_TYPE(funcType);
   if(TREE_CODE(funcType)==FUNCTION_TYPE){
      for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
         tree type=TREE_VALUE(al);
         if(type == void_type_node)
            return FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

static MtcsFuncType getMtcsFuncType(tree decl,location_t loc)
{
   tree g1 = lookup_attribute (MTCS_GLOBAL_STR, DECL_ATTRIBUTES (decl));
   tree g2 = lookup_attribute (MTCS_DEVICE_STR, DECL_ATTRIBUTES (decl));
   tree g3 = lookup_attribute (MTCS_HOST_STR, DECL_ATTRIBUTES (decl));
   if(g1){
      if(g2){
         error_at(loc,"核函数不能定义为设备函数。111");
      }
      if(g3){
         error_at(loc,"核函数不能定义为主机函数。222");
      }
      return MTCS_FUNC_KERNEL;//这是一个核函数
   }

   if(g2){
      if(g3){
         return MTCS_FUNC_DEVICE_HOST;//即是设备函数也是主机函数
      }else{
         return MTCS_FUNC_DEVICE;
      }
   }
   return MTCS_FUNC_NOT;
}

static void appendMtcsAttributes(tree decl,int mtcsFuncType)
{
   if(mtcsFuncType == MTCS_FUNC_KERNEL)
      DECL_ATTRIBUTES (decl) = tree_cons (get_identifier (MTCS_GLOBAL_STR),NULL_TREE, DECL_ATTRIBUTES (decl));
   else if(mtcsFuncType == MTCS_FUNC_DEVICE_HOST){
      DECL_ATTRIBUTES (decl) = tree_cons (get_identifier (MTCS_DEVICE_STR),NULL_TREE, DECL_ATTRIBUTES (decl));
      DECL_ATTRIBUTES (decl) = tree_cons (get_identifier (MTCS_HOST_STR),NULL_TREE, DECL_ATTRIBUTES (decl));
   }else
      DECL_ATTRIBUTES (decl) = tree_cons (get_identifier (MTCS_DEVICE_STR),NULL_TREE, DECL_ATTRIBUTES (decl));
}


static nboolean setMtcsInfo(ClassFunc *self,location_t loc,tree decl,int mtcsFuncType)
{
   //找父类有没有相同的函数声明。如果有用父类的mtcs类型，给当前函数赋值
   if(mtcsFuncType==MTCS_FUNC_NOT){
      ClassName *className=self->className;
      ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
      while(info->parentName.sysName!=NULL){
         ClassFunc *parentFunc=func_mgr_get_func_by_raw_mangle(func_mgr_get(),
         info->parentName.sysName,self->rawMangleName);
         if(parentFunc && parentFunc->fieldDecl && class_func_is_mtcs(parentFunc)){
            if(!class_func_is_private(parentFunc)){
               mtcsFuncType = class_func_get_mtcs_type(parentFunc);
               appendMtcsAttributes(decl,mtcsFuncType);
               self->permission = parentFunc->permission;
               n_debug("从父类获取mtcs函数类型 %s %s %d\n",self->mangleFunName,info->parentName.sysName,mtcsFuncType);
            }
            break;
         }
         info=class_mgr_get_class_info(class_mgr_get(),info->parentName.sysName);
      }
   }
   if(mtcsFuncType==MTCS_FUNC_NOT)
      return FALSE;

   class_func_set_mtcs(self,TRUE);
   class_func_set_mtcs_type(self,(MtcsFuncType)mtcsFuncType);
   addMiiAttribute(self,decl);
   n_debug("classfunc.c setMtcsInfo 设函数类型 %s\n",mtcs_info_get_mtcs_type_string((MtcsFuncType)mtcsFuncType));
   if(self->permission!=CLASS_PERMISSION_PRIVATE){ //
      DECL_ATTRIBUTES (decl) = tree_cons (get_identifier (MTCS_FN_VISIBLE),NULL_TREE, DECL_ATTRIBUTES (decl));
   }
   nboolean isFuncGeneric = class_func_is_func_generic(self);
   if(isFuncGeneric){
      error_at(loc,"核函数或设备函数不能声明为泛型函数%qD",decl);
   }
   int genParamCount = class_func_get_generic_param_count(self);
   char *sysName= class_func_get_class_name(self);
   ClassName *className = class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
   nboolean selfAboutGen = class_mgr_about_generic(class_mgr_get(),className);
   //printf("是不是泛型func xx:%d genParamCount:%d className:%s selfAboutGen:%d\n",isFuncGeneric,genParamCount,sysName,selfAboutGen);
   if(selfAboutGen && genParamCount-1>0){
      error_at(loc,"核函数或设备函数的不能有泛型参数%qD",decl);
   }
   if(mtcsFuncType==MTCS_FUNC_KERNEL){
      tree rtn = getRtn(decl);
      if(rtn!=void_type_node){
         error_at(loc,"核函数的返回值必须是void类型%qD",decl);
      }
   }
   if(haveVariableParam(decl)){
      error_at(loc,"核函数或设备函数不能有可变参数%qD",decl);
   }
   TREE_USED(decl) = 1;
   DECL_PRESERVE_P (decl) = 1; //重要,否则经过gimple后，函数被移走
   //bug 073
   if(mtcsFuncType==MTCS_FUNC_KERNEL && TREE_CODE(decl)==FUNCTION_DECL){
      tree attrs = DECL_ATTRIBUTES (decl);
      //加 noinline
      if (!lookup_attribute ("noinline", attrs))
         attrs = tree_cons (get_identifier ("noinline"), NULL, attrs);
      //加 noclone
      if (!lookup_attribute ("noclone", attrs))
         attrs = tree_cons (get_identifier ("noclone"), NULL, attrs);
      DECL_ATTRIBUTES (decl) = attrs;
      // 核心标志位：确保优化器尊重这些属性
      DECL_UNINLINABLE (decl) = 1;
   }
   //如果编译选项有 -fsanitize=address 屏蔽，因为ptx不支持
   if(flag_sanitize &&
      (mtcsFuncType==MTCS_FUNC_KERNEL || mtcsFuncType==MTCS_FUNC_DEVICE ||  mtcsFuncType==MTCS_FUNC_DEVICE_HOST)
      && TREE_CODE(decl)==FUNCTION_DECL)
      add_no_sanitize_value (decl, "all");
   return TRUE;
}

/**
 * 设mtcs函数信息
 */
nboolean class_func_set_mtcs_info(ClassFunc *self,location_t loc,tree decl)
{
   int mtcsFuncType = MTCS_FUNC_NOT;
   if(self->fieldDecl){
      mtcsFuncType =getMtcsFuncType(self->fieldDecl,loc);
      if(self->fromImplDefine){
         int defineType =getMtcsFuncType(self->fromImplDefine,loc);
         if(mtcsFuncType!=defineType && defineType!=MTCS_FUNC_NOT){
            error_at(loc,"函数声明是:%s,定义是:%s 声明与定义不匹配。",
                          mtcs_info_get_mtcs_type_string(mtcsFuncType),mtcs_info_get_mtcs_type_string(defineType));
            return FALSE;
         }
      }
   }else{
      mtcsFuncType = getMtcsFuncType(self->fromImplDefine,loc);
   }
   return  setMtcsInfo(self,loc,decl,mtcsFuncType);
}


static MtcsFuncType getMtcsFuncTypeFromAttrs(tree attrs,location_t loc)
{
   if(!attrs)
      return MTCS_FUNC_NOT;
   tree g1 = lookup_attribute (MTCS_GLOBAL_STR, attrs);
   tree g2 = lookup_attribute (MTCS_DEVICE_STR, attrs);
   tree g3 = lookup_attribute (MTCS_HOST_STR, attrs);
   if(g1){
      if(g2){
         error_at(loc,"核函数不能定义为设备函数。111");
      }
      if(g3){
         error_at(loc,"核函数不能定义为主机函数。222");
      }
      return MTCS_FUNC_KERNEL;//这是一个核函数
   }

   if(g2){
      if(g3){
         return MTCS_FUNC_DEVICE_HOST;//即是设备函数也是主机函数
      }else{
         return MTCS_FUNC_DEVICE;
      }
   }
   return MTCS_FUNC_NOT;
}


void class_func_set_mtcs_type(ClassFunc *self,location_t loc,tree allPrefixAttrs)
{
   MtcsFuncType type =  getMtcsFuncTypeFromAttrs(allPrefixAttrs,loc);
   MtcsFuncType mtcsFuncType = MTCS_FUNC_NOT;
   if(self->mtcsFuncType == MTCS_FUNC_NOT && type == MTCS_FUNC_NOT){
      ClassName *className=self->className;
      ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
      while(info->parentName.sysName!=NULL){
         ClassFunc *parentFunc=func_mgr_get_func_by_raw_mangle(func_mgr_get(),
                     info->parentName.sysName,self->rawMangleName);
         if(parentFunc && parentFunc->fieldDecl && class_func_is_mtcs(parentFunc)){
            if(!class_func_is_private(parentFunc)){
               mtcsFuncType = class_func_get_mtcs_type(parentFunc);
               n_debug("从父类获取mtcs函数类型 %s %s %d\n",self->mangleFunName,info->parentName.sysName,mtcsFuncType);
            }
            break;
         }
         info=class_mgr_get_class_info(class_mgr_get(),info->parentName.sysName);
      }
      if(mtcsFuncType!= MTCS_FUNC_NOT){
         class_func_set_mtcs(self,TRUE);
         class_func_set_mtcs_type(self,mtcsFuncType);
      }
   }else if(self->mtcsFuncType != MTCS_FUNC_NOT && type == MTCS_FUNC_NOT){
      ;
   }else if(self->mtcsFuncType == type){
      ;
   }else if(self->mtcsFuncType == MTCS_FUNC_NOT && type!=MTCS_FUNC_NOT){
      class_func_set_mtcs(self,TRUE);
      class_func_set_mtcs_type(self,type);
   }else{
      error_at(loc,"原函数的mtcs类型是:%s,新的类型是:%s 不匹配。",
                    mtcs_info_get_mtcs_type_string(self->mtcsFuncType),mtcs_info_get_mtcs_type_string(type));
   }
}

ClassFunc *class_func_new()
{
   ClassFunc *self =n_slice_alloc0 (sizeof(ClassFunc));
   classFuncInit(self);
   return self;
}


