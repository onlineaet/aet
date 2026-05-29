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

#ifndef __GCC_SELECT_FIELD_H__
#define __GCC_SELECT_FIELD_H__


#include "nlib.h"
#include "classutil.h"
#include "classfunc.h"


typedef struct _CheckParamCallback CheckParamCallback;
struct _CheckParamCallback{
    tree  (*genericCheckArgument)(CheckParamCallback *self,location_t ploc, tree function, tree fundecl,
                  tree type, tree origtype, tree val, tree valtype,
                  bool npc, tree rname, int parmnum, int argnum,
                  bool excess_precision, int warnopt);
    nboolean (*addFuncPointer)(CheckParamCallback *self,int paramNum,tree actual,tree formal);//实参是一个函数指针，形参与时函数指针。实参来自类的静态变量。
    struct{
        int paramNum;
        tree actual;
        tree formal;//形参
    }funcPointers[10];
    int funcPointerCount;
    ClassName *className;//ClassName *className;
    GenericModel *generics;//GenericModel *generics
    ClassFunc *classFunc;
    int error;//生成函数调用时出现的错误数
    int warn;//生成函数调用时出现的警告数
    int state;//运行类函数选择，生成implicitly函数
};

typedef struct _SelectFunc{
    ClassFunc *classFunc;
    char *sysName;
    int sucessed;//被class中的的函数替换了为1
    int genericQuery;//一个标记，如果=1，执行在selectfield.c中的setGenericQuery()方法
    tree funcdecl;//用户调用的函数声明 object->setData funcdel就是setData 最终选择的是object混淆过的函数名如 ObjectSetDataVEx
    nboolean implicitlyFunc;//是不是隐藏函数
}SelectFunc;
/**
 * 选取在类中声明的静态函数
 */

typedef struct _SelectField SelectField;
/* --- structures --- */
struct _SelectField
{
    CheckParamCallback *checkCallback;
};

typedef struct _CandidateFunc
{
    ClassFunc *classFunc;
    char *sysName;
    char *implSysName;//implSysName实现了接口的类。
    int error;
    int warn;
    struct{
        int paramNum;
        tree actual;
        tree formal;
        tree result;
    }funcPointers[10];
    int funcPointerCount;
}CandidateFunc;

/**
 * 记录函数指针与类静态函数参数错误信息。
 */
typedef struct _FuncPointerErrorInfo
{
    char *sysName;//选择的类
    ClassFunc *classFunc; //选择的类方法
    int paramNum;//第几个参数
    tree lhs;//类方法要赋值给的函数指针
    int errorNo;
}FuncPointerErrorInfo;

typedef struct _FuncPointerError
{
    FuncPointerErrorInfo *message[10];
    int count;
}FuncPointerError;


SelectField       *select_field_get();
tree               select_field_modify_or_init_field(SelectField *self,location_t loc ,tree lhsType,tree rhs,FuncPointerError **errors);
CandidateFunc     *select_field_get_ctor_func(SelectField *self,ClassName *className,vec<tree, va_gc> *exprlist,
                          vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors);
/**
 * 在当前类中取方法
 */
CandidateFunc      *select_field_get_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,vec<tree, va_gc> *origtypes,
                                   vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,GenericModel *generics,FuncPointerError **errors);
/**
 * 从当前类遍历父类和接口
 */
CandidateFunc      *select_field_get_func_by_recursion(SelectField *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,
            vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,GenericModel *generics,FuncPointerError **errors);

CandidateFunc     *select_field_get_static_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
                          vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors);
/**
 * 找出隐藏的静态函数。
 */
CandidateFunc *select_field_get_implicitly_static_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors);
/**
 * 获取隐藏的函数
 */
CandidateFunc *select_field_get_implicitly_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,GenericModel *generics,FuncPointerError **errors);


void               select_field_free_candidate(CandidateFunc *candidate);
void               select_field_free_func_pointer_error(FuncPointerError *funcPointerError);
void               select_field_printf_func_pointer_error(FuncPointerError *funcPointerError);

tree               select_field_build_function_call_vec(SelectField *self,location_t loc, vec<location_t> arg_loc,
                                tree function, vec<tree, va_gc> *params, vec<tree, va_gc> *origtypes,SelectFunc *selectFunc);
/**
 * c-typeck.cc调用
 */
tree               select_field_call_back(location_t ploc, tree function, tree fundecl,
                          tree type, tree origtype, tree val, tree valtype,
                          bool npc, tree rname, int parmnum, int argnum,
                          bool excess_precision);

/**
 * 检查外部函数的参数与实参是否匹配
 */
nboolean select_field_match_outside_function(SelectField *self,tree funcdecl,vec<tree, va_gc> *exprList,
      vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc);

#endif

