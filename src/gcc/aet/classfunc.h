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

#ifndef __GCC_CLASS_FUNC_H__
#define __GCC_CLASS_FUNC_H__


#include "nlib.h"
#include "genericmodel.h"
#include "mtcsinfo.h"

enum func_from_code{
	STRUCT_DECL,
	CLASS_IMPL_DEFINE,
};

//typedef enum{
//   FROM_INTERFACE_TO_CLASS,   //来自接口，成为类成员
//   FROM_INTERFACE_AND_DEVICE_FUNC,//来自接口并且是接口中
//}ClassFuncCloneType;

typedef struct _ClassFunc ClassFunc;
/* --- structures --- */
struct _ClassFunc
{
	char *orgiName;//原来的名字
	char *mangleFunName;
	char *mangleNoSelfName;//不带self参数生成的函数名，用在static的比较中
	char *rawMangleName;//不带类名不带self的函数名 用在与父类的比较中
	tree classTree;
	tree fieldDecl;//域成员
	tree fromImplDefine;//来自implclass的定义
	nboolean isAbstract; //是否抽象方法
	nboolean isCtor; //是构造方法
	nboolean isFinalized; //是释放方法
	nboolean isUnref; //是反引用
	nboolean isStatic;//是不是静态函数
	nboolean isQueryGenFunc;    //是不是有问号泛型参数的函数。
	nboolean isGenericParmFunc;//是不是有泛型类参数的函数
	GenericModel *parmsGenModel[50];//方法中的参数的泛型 因为field只有类型没有参数，所以从struct c_declarator *declarator取出参数保存在这里
	int parsmGenModeCount; //泛型参数个数，如果self也是泛型类也包括。
	ClassPermissionType permission;
	int genBlockCount;//泛型块数量
	nboolean isFinal;
	nboolean allParmIsQuery;//是不是所有泛型类参数都是问号泛型的函数
   int serialNumber;//在class中的序号
   nboolean isMtcsFunc;//是否mtcs函数 根据是否有__global__或__device__属性来判断
   MtcsFuncType mtcsFuncType;
   ClassName *className;//所在的class或接口
   nboolean fromInterface;//是从接口clone来的
   nboolean isDivide;//是不是从host_device类型的ClassFunc分裂而来的。
   ClassFunc *divideSrc;//分裂的源头
   location_t endLoc;//函数定义的结束位置
};

typedef struct _ParmGenInfo
{
	char *str;
	int index; //在第几个参数上
	nboolean independ;//是独立的泛型声明，还是依赖对象的
	int unitPos;//泛型单元在类的泛型声明的位置
	tree object;
}ParmGenInfo;

ClassFunc  *class_func_new();
nboolean    class_func_is_same_but_rtn(ClassFunc *self,ClassFunc *compare,tree readyDecl);
nboolean    class_func_set_decl(ClassFunc *self,tree decl,enum func_from_code code);
nboolean    class_func_is_same_generic(ClassFunc *self);
//是还是泛型函数
nboolean    class_func_is_func_generic(ClassFunc *self);
GenericModel *class_func_get_func_generic(ClassFunc *self);
NPtrArray  *class_func_get_generic_parm(ClassFunc *self,char *id);
void        class_func_add_generic_block(ClassFunc *self);
nboolean    class_func_have_generic_block(ClassFunc *self);
nboolean    class_func_have_query_param(ClassFunc *self);//有没有问号参数
nboolean    class_func_have_generic_class_parm(ClassFunc *self);//有没有泛型类参数
void        class_func_set_final(ClassFunc *self,nboolean isFinal);
nboolean    class_func_is_abstract(ClassFunc *self);
nboolean    class_func_is_final(ClassFunc *self);
nboolean    class_func_have_super(ClassFunc *self);
nboolean    class_func_have_all_query_parm(ClassFunc *self);
nboolean    class_func_is_public(ClassFunc *self);
nboolean    class_func_is_protected(ClassFunc *self);
nboolean    class_func_is_private(ClassFunc *self);
nboolean    class_func_is_interface_reserve(ClassFunc *self);
nboolean    class_func_is_static(ClassFunc *self);
void        class_func_save_generic_model_for_field_decl(ClassFunc *self,tree args);
int         class_func_get_generic_param_count(ClassFunc *self);
const char *class_func_get_class_name(ClassFunc *self);
nboolean    class_func_is_mtcs(ClassFunc *self);
void        class_func_set_mtcs(ClassFunc *self,nboolean isMtcs);
void        class_func_set_mtcs_type(ClassFunc *self,MtcsFuncType type);
MtcsFuncType class_func_get_mtcs_type(ClassFunc *self);
nboolean    class_func_is_kernel(ClassFunc *self);
nboolean    class_func_is_device(ClassFunc *self);
nboolean    class_func_is_host(ClassFunc *self);
//ClassFunc 是否有类名
nboolean    class_func_have_class_name(ClassFunc *self);
//克隆是来自host_device classFunc
void        class_func_set_divide(ClassFunc *self,nboolean isDivide,ClassFunc *divideSrc);
//是从host device函数分裂出来的
nboolean    class_func_is_divide(ClassFunc *self);
ClassFunc  *class_func_get_divide_src(ClassFunc *self);
ClassFunc  *class_func_clone(ClassFunc *self,tree newFieldDecl,char **names,tree classTree ,ClassName *className);
//定义一个普通的方法
nboolean    class_func_is_normal(ClassFunc *self);
void        class_func_set_end_location(ClassFunc *self,location_t endLoc);

#endif




