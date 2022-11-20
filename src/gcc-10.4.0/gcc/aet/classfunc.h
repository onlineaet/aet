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

enum func_from_code{
	STRUCT_DECL,
	CLASS_IMPL_DECL,
	CLASS_IMPL_DEFINE,
};


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
	tree fromImplDecl; //来自implclass的声明
	tree fromImplDefine;//来自implclass的定义
	nboolean isAbstract; //是否抽象方法
	nboolean isCtor; //是构造方法
	nboolean isFinalized; //是释放方法
	nboolean isUnref; //是反引用
	nboolean isStatic;//是不是静态函数
	nboolean isQueryGenFunc;//是不是带问号泛型参数的函数。
	nboolean isGenericParmFunc;//是不是有泛型参数
	tree fieldParms;//因为field只有类型没有参数，所以从struct c_declarator *declarator取出参数保存在这里
	ClassPermissionType permission;
	nboolean haveGenericBlock;//是否有泛型块
	nboolean isFinal;
	nboolean allParmIsQuery;//所有的参数都是问号泛型参数
    int serialNumber;//在class中的序号
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
nboolean    class_func_is_func_generic(ClassFunc *self);
GenericModel *class_func_get_func_generic(ClassFunc *self);
NPtrArray  *class_func_get_generic_parm(ClassFunc *self,char *id);
void        class_func_set_generic_block(ClassFunc *self,nboolean have);
nboolean    class_func_have_generic_block(ClassFunc *self);
nboolean    class_func_is_query_generic(ClassFunc *self);
nboolean    class_func_have_generic_parm(ClassFunc *self);
void        class_func_set_final(ClassFunc *self,nboolean isFinal);
nboolean    class_func_is_abstract(ClassFunc *self);
nboolean    class_func_is_final(ClassFunc *self);
nboolean    class_func_have_super(ClassFunc *self);
nboolean    class_func_have_all_query_parm(ClassFunc *self);
nboolean    class_func_is_public(ClassFunc *self);
nboolean    class_func_is_protected(ClassFunc *self);
nboolean    class_func_is_private(ClassFunc *self);
nboolean    class_func_is_interface_reserve(ClassFunc *self);



#endif




