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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gimple-expr.h"
#include "tree-iterator.h"
#include "opts.h"
#include "c/c-tree.h"
#include "c/c-parser.h"

#include "aet-typeck.h"
#include "selectfield.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "accesscontrols.h"
#include "genericcall.h"
#include "funcpointer.h"

/**
 * 找到了类中的静态函数，现在可以把由parser_static_create_temp_tree创建并
 * BINFO_FLAG_1(decl)=1;的参数据替换了
 */
static void replaceFunctionPointer(CandidateFunc *candidate,vec<tree, va_gc> *exprlist)
{
    if(candidate==NULL)
        return;
    int i;
    for(i=0;i<candidate->funcPointerCount;i++){
       int index=candidate->funcPointers[i].paramNum;
       n_debug("select_field_replace_func_pointer:替换函数指针实参%d %d %d\n",index,exprlist->length(),candidate->funcPointerCount);
       exprlist->ordered_remove(index);//把self参数移走
       vec_safe_insert (exprlist, index, candidate->funcPointers[i].result);
    }
}

/**
 * 声明函数内有一个函数指针 call void func(AFunc call);
 * 调用时传递一个参数AObject.strHash是一个类的静态函数。 call->func(AObject.strHash);
 */
static nboolean isStaticFunc(tree actual,tree formalType)
{
    if(TREE_CODE(actual)!=ADDR_EXPR)
        return FALSE;
    tree func=   TREE_OPERAND (actual, 0);
    tree type=TREE_TYPE(actual);
    if(TREE_CODE(type)==POINTER_TYPE && TREE_CODE(func)==FUNCTION_DECL && BINFO_FLAG_1(func)==1){
             ;
    }else{
        return FALSE;
    }
    if(TREE_CODE(formalType)!=POINTER_TYPE)
        return FALSE;
    type=TREE_TYPE(formalType);
    if(TREE_CODE(type)!=FUNCTION_TYPE)
        return FALSE;
    //printf("通过回调CheckParamCallback 判断实参是一个类的静态函数。形参是函数指针。\n");
    return TRUE;
}


static tree checkCallback_cb(CheckParamCallback *callback,int type,location_t loc,tree function,tree origGenericDecl,tree val,
        nboolean npc,nboolean excessrecision)
{
    if(type==1){
        tree ret=generic_call_check_parm(generic_call_get(),loc,
                function,origGenericDecl,val,npc,excessrecision,callback->className,callback->generics);
        return ret;
    }
    return NULL_TREE;
}

//实参是一个函数指针，形参与时函数指针。实参来自类的静态变量。
static nboolean addFuncPointer_cb(CheckParamCallback *callback,int paramNum,tree actual,tree formal)
{
    if(!isStaticFunc(actual,formal)){
        return FALSE;
    }
//    printf("addFuncPointer_cb----%d\n",paramNum);
//    aet_print_tree_skip_debug(actual);
//    printf("addFuncPointer_cb- ssss---%d\n",paramNum);
//    aet_print_tree_skip_debug(formal);
    int index=callback->funcPointerCount;
    callback->funcPointers[index].paramNum=paramNum;
    callback->funcPointers[index].actual=actual;
    callback->funcPointers[index].formal=formal;
    callback->funcPointerCount++;
    return TRUE;
}

static void init_check_param_callback(SelectField *self)
{
    self->checkCallback->funcPointerCount=0;
}


static void selectFielInit(SelectField *self)
{
    self->checkCallback=(CheckParamCallback *)n_slice_new(CheckParamCallback);
    self->checkCallback->callback=checkCallback_cb;
    self->checkCallback->addFuncPointer=addFuncPointer_cb;
}

static void setParamsForCheckCallback(SelectField *self,ClassName *className,GenericModel *generics)
{
    self->checkCallback->className=className;
    self->checkCallback->generics=generics;
}

static CandidateFunc *createCandidate()
{
       CandidateFunc *candidate=(CandidateFunc *)n_slice_new0(CandidateFunc);
       candidate->sysName=NULL;
       candidate->implSysName=NULL;
       candidate->funcPointerCount=0;
       candidate->warnErr=NULL;
       return candidate;
}


static CandidateFunc *cloneCand(CandidateFunc *src)
{
    if(src==NULL)
        return NULL;
    CandidateFunc *candidate=createCandidate();
    candidate->classFunc=src->classFunc;
    candidate->sysName=n_strdup(src->sysName);
    if(src->implSysName)
      candidate->implSysName=n_strdup(src->implSysName);
    candidate->warnErr=xmalloc(sizeof(WarnAndError));
    candidate->warnErr->warnCount=src->warnErr->warnCount;
    int funcps=src->funcPointerCount;
     int i;
     for(i=0;i<funcps;i++){
         candidate->funcPointers[i].paramNum=src->funcPointers[i].paramNum;
         candidate->funcPointers[i].actual=src->funcPointers[i].actual;
         candidate->funcPointers[i].formal=src->funcPointers[i].formal;
         candidate->funcPointers[i].result=src->funcPointers[i].result;
     }
     candidate->funcPointerCount=src->funcPointerCount;
    return candidate;
}

static nint sortCandidateFunc_cb(nconstpointer  cand1,nconstpointer  cand2)
{
    CandidateFunc *p1 = *((CandidateFunc **) cand1);
    CandidateFunc *p2 = *((CandidateFunc **) cand2);
    int a=p1->warnErr->warnCount;
    int b=p2->warnErr->warnCount;
    return (a > b ? +1 : a == b ? 0 : -1);
}

static void freeCandidate_cb(CandidateFunc *item)
{
    if(item==NULL)
        return;
    if(item->sysName)
        n_free(item->sysName);
    if(item->implSysName)
         n_free(item->implSysName);
    n_free(item->warnErr);
    n_slice_free(CandidateFunc,item);
}

static nint warnCompare_cb(nconstpointer  cand1,nconstpointer  cand2)
{
    CandidateFunc *p1 = (CandidateFunc *)cand1;
    CandidateFunc *p2 = (CandidateFunc *)cand2;
    int a=p1->warnErr->warnCount;
    int b=p2->warnErr->warnCount;
    return (a > b ? +1 : a == b ? 0 : -1);
}

static int getParams(tree funcType,int *varargs)
{
      int count=0;
      for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
           tree type=TREE_VALUE(al);
           if(type == void_type_node){
               //printf("有void_type_node count:%d\n",count);
               *varargs=0;
               break;
           }
           count++;
      }
      return count;
}

/**
 * 找最好的，最好的定义为警告最少的。
 * 如果list中有两个警告数相同的，取不是泛型函数的那个。
 */
static CandidateFunc * filterGoodFunc(NList *okList)
{
  if(n_list_length(okList)==0){
    n_debug("filterGoodFunc 没有匹配的函数!!! ");
    return NULL;
  }
  okList=n_list_sort(okList,warnCompare_cb);
  int len=n_list_length(okList);
  if(len==1){
    CandidateFunc *cand=(CandidateFunc *)n_list_nth_data(okList,0);
    n_debug("找到了声明的函数 成功匹配参数，只有一个 xxx decl code:%s name:%s ",cand->classFunc->orgiName,cand->classFunc->mangleFunName);
    return cand;
  }else{
     int i;
     CandidateFunc *first=(CandidateFunc *)n_list_nth_data(okList,0);
     nboolean genericFunc=class_func_is_func_generic(first->classFunc);
     if(!genericFunc){
        return first;
     }
     CandidateFunc *compare=(CandidateFunc *)n_list_nth_data(okList,1);
     if(first->warnErr->warnCount==compare->warnErr->warnCount){
         return compare;
     }else{
         return first;
     }
  }
}

static CandidateFunc *findBest(CandidateFunc **arrays,int count,ClassName *className)
{
     //按从子类到父类的顺序取
    int i;
    for(i=0;i<count;i++){
        CandidateFunc *selected=arrays[i];
        if(!strcmp(selected->sysName,className->sysName)){
            return selected;
        }
    }
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(info->parentName.sysName==NULL){
        n_error("报告此错误。findBest count:%d sysName:%s",count,className->sysName);
    }
    return findBest(arrays,count,&info->parentName);
}


/**
 *找出warnCount是一样的，并且从子类开始找到父类
 */
static CandidateFunc *getBest(NPtrArray *selectArray,ClassName *className)
{
    CandidateFunc *first=n_ptr_array_index(selectArray,0);
    if(selectArray->len==1)
        return first;
    CandidateFunc *arrays[selectArray->len];//warnCount是一样的，并且是最少的。
    int i;
    int warnCount=0;
    int sameWarnCountDeclCount=0;
    for(i=0;i<selectArray->len;i++){
        CandidateFunc *item=n_ptr_array_index(selectArray,i);
        if(i==0){
            warnCount=item->warnErr->warnCount;
            arrays[i]=item;
        }else{
            if(item->warnErr->warnCount==warnCount)
                arrays[i]=item;
            else
                break;
        }
    }
    sameWarnCountDeclCount=i;
    if(sameWarnCountDeclCount==1)
        return first;
    //按从子类到父类的顺序取
    return findBest(arrays,sameWarnCountDeclCount,className);
}

static void printError()
{
   int i;
   for(i=0;i<argsFuncsInfo.warnCount;i++){
       printf("检查参数时出现的警告:%d warn:%d\n",i,argsFuncsInfo.warns[i]);
   }
   for(i=0;i<argsFuncsInfo.errorCount;i++){
       printf("检查参数时出现的错误:%d error:%d\n",i,argsFuncsInfo.errors[i]);
   }
}


///////////////////-----------------------以下是选取类中的静态函数------------------------

/**
 * 创建一个在选取类函数给函数指针赋值时出现的错误信息。
 */
static FuncPointerErrorInfo *createErrorInfoForFuncPointer(int errorNo,char *sysName,ClassFunc *classFunc,int paramNum,tree lhs)
{
    FuncPointerErrorInfo *info=(FuncPointerErrorInfo *)n_slice_new(FuncPointerErrorInfo);
    info->sysName=n_strdup(sysName);
    info->classFunc=classFunc;
    info->paramNum=paramNum;
    info->errorNo=errorNo;
    info->lhs=lhs;
    return info;
}

static CandidateFunc * checkStaticFuncParam(SelectField *self,location_t loc,ClassFunc *func,tree selectedField,tree lhs)
{
   tree decl=selectedField;
   int  rightVarargs=1;
   int  leftVarargs=1;
   tree lhsType=TREE_TYPE(lhs);//pointer_type
   tree lhsFunctionType=TREE_TYPE(lhsType); //function_type
   tree  funcType = TREE_TYPE (selectedField);
   int rightCount=getParams(funcType,&rightVarargs);
   int leftCount=getParams(lhsFunctionType,&leftVarargs);
   if(rightCount!=leftCount){
       printf("右边参数个数:%d 左边参数个数:%d 不等！！！\n",rightCount,leftCount);
   }
   aet_warn_and_error_reset();
   if(rightVarargs)
       n_debug("selectfield.c checkStaticFuncParam 开始匹配参数 decl code:%s name:%s 是否有可变参数：%d ",
                        get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),rightVarargs);
   tree value=build_unary_op (loc, ADDR_EXPR, decl, false);
   mark_exp_read (value);
   value= aet_typeck_func_param_compare(loc,lhsType,value,TREE_STATIC(lhs));
   if(value==error_mark_node){
      n_debug("selectfield.c checkStaticFuncParam 不能匹配参数 decl code:%s name:%s 错误数:%d warn:%d",
              get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),argsFuncsInfo.errorCount,argsFuncsInfo.warnCount);
      printError();
   }else{
      n_debug("checkStaticFuncParam 有错误吗? decl code:%s name:%s 错误数:%d warn:%d ",
            get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),
            argsFuncsInfo.errorCount,argsFuncsInfo.warnCount);
      if(argsFuncsInfo.errorCount==0){
        CandidateFunc *candidate=createCandidate();
        candidate->warnErr=aet_warn_and_error_copy();
        return candidate;
     }
  }
  return NULL;
}

static CandidateFunc *getStaticFuncFromClass(SelectField *self,location_t loc,ClassName *className,char *orgiFuncName,tree lhs,FuncPointerError *errors)
{
    NList *okList=NULL;
    NPtrArray *array=func_mgr_get_static_funcs(func_mgr_get(),className);
    if(array==NULL)
        return NULL;
    int i;
    for(i=0;i<array->len;i++){
        ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
        if(strcmp(item->orgiName,orgiFuncName))
            continue;
            //查找语句
        //printf("selectfield.c getStaticFuncFromClass index:%d %s orgiFuncName:%s field?:%p\n", i,className->sysName,orgiFuncName,item->fieldDecl);
        tree decl=NULL_TREE;
        if(aet_utils_valid_tree(item->fieldDecl)){
            decl=item->fieldDecl;
        }
        if(decl==NULL_TREE)
            continue;
        CandidateFunc *candidate=checkStaticFuncParam(self,loc,item,decl,lhs);
        if(candidate!=NULL){
              candidate->classFunc=item;
              candidate->sysName=n_strdup(className->sysName);
              int paramNum=0;
              int ok=func_pointer_check(lhs,decl,&paramNum);//aet检查函数指针与右值的返回值和参数是否匹配。
              if(ok!=0){
                  printf("selectfield.c getStaticFuncFromClass index:%d %s orgiFuncName:%s field?:%p 函数指针参数aet比较:%d\n",
                          i,className->sysName,orgiFuncName,item->fieldDecl,ok);
                  if(errors!=NULL){
                      FuncPointerErrorInfo *errInfo=createErrorInfoForFuncPointer(ok,className->sysName,item,paramNum,lhs);
                      errors->message[errors->count++]=errInfo;
                  }
                  freeCandidate_cb(candidate);
                  continue;
              }
              if(candidate->warnErr->errorCount==0 && candidate->warnErr->warnCount==0){
                    n_list_free_full(okList,freeCandidate_cb);
                    n_debug("checkParm 检查没有错误，没有警告，直接返回:%s %s className:%s\n",item->orgiName,item->mangleFunName,className->sysName);
                    return candidate;
              }
              okList=n_list_append(okList,candidate);
        }
    }
    CandidateFunc *okCand=filterGoodFunc(okList);
    CandidateFunc *result=cloneCand(okCand);
    n_list_free_full(okList,freeCandidate_cb);
    return result;
}

static CandidateFunc *getSelectedStaticFunc(SelectField *self,location_t loc,ClassName *className,char *orgiFuncName,tree lhs,FuncPointerError *errors)
{
    if(className==NULL || orgiFuncName==NULL || className->sysName==NULL)
        return NULL;
     NList *okList=NULL;
     //printf("getSelectedStaticFunc 00 类名：%s 函数名：%s\n",className->sysName,orgiFuncName);
     CandidateFunc *candidate=getStaticFuncFromClass(self,loc,className,orgiFuncName,lhs,errors);
     if(candidate!=NULL){
        if(candidate->warnErr->warnCount==0){
            //printf("getSelectedStaticFunc 找到没有警告的函数，直接返回。%s %s\n",className->sysName,orgiFuncName);
            return candidate;
        }
        okList=n_list_append(okList,candidate);
     }

   //如果是field要加入指针，否则访问不到
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    int i;
    for(i=0;i<info->ifaceCount;i++){
       ClassName *iface=&info->ifaces[i];
       n_debug("getSelectedStaticFunc 11 在类%s中的接口%s中找方法。函数名：%s",className->sysName,iface->sysName,orgiFuncName);
       CandidateFunc *ifaceCandidate=getStaticFuncFromClass(self,loc,iface,orgiFuncName,lhs,errors);
       n_debug("getSelectedStaticFunc 22 在类%s中的接口%s中找方法。函数名：%s",className->sysName,iface->sysName,orgiFuncName);
       if(ifaceCandidate!=NULL){
          if(candidate->warnErr->warnCount==0){
             // printf("getSelectedStaticFunc 接口 找到没有警告的函数，直接返回。%s %s\n",className->sysName,orgiFuncName);
              n_list_free_full(okList,freeCandidate_cb);
              return ifaceCandidate;
          }
          okList=n_list_append(okList,ifaceCandidate);
       }
    }
    CandidateFunc *okCand=filterGoodFunc(okList);
    CandidateFunc *result=cloneCand(okCand);
    n_list_free_full(okList,freeCandidate_cb);
    return result;
}


static void selectGoodStaticFunc(SelectField *self,location_t loc,ClassName *className,char *orgiFuncName,
                                      tree lhs,NPtrArray *selectedArray,FuncPointerError *errors)
{
    if(className==NULL || orgiFuncName==NULL || className->sysName==NULL){
        return;
    }
    CandidateFunc *result=getSelectedStaticFunc(self,loc,className,orgiFuncName,lhs,errors);
    if(result!=NULL){
        if(result->warnErr->warnCount==0){
            n_ptr_array_remove_range(selectedArray,0,selectedArray->len);
            n_ptr_array_add(selectedArray,result);
            return;
        }
        n_ptr_array_add(selectedArray,result);
    }
     ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
     if(info==NULL)
         return;
     n_debug("selectGoodStaticFunc 当前类%s找候选函数:%p，递归的向父类%s再找。",className->sysName,result,info->parentName.sysName);
     selectGoodStaticFunc(self,loc,&info->parentName,orgiFuncName,lhs,selectedArray,errors);
}


/**
 * 根据左值获取静态函数。
 */
static CandidateFunc *selectStaticFunc(SelectField *self,location_t loc,ClassName *className,char *orgiFuncName,tree lhs,FuncPointerError *errors)
{
    NPtrArray *selectArray=n_ptr_array_new_with_free_func(freeCandidate_cb);
    selectGoodStaticFunc(self,loc,className,orgiFuncName,lhs,selectArray,errors);
    if(selectArray->len==0){
        n_ptr_array_unref(selectArray);
        return NULL;
    }
    n_ptr_array_sort(selectArray,sortCandidateFunc_cb);
    CandidateFunc *result=getBest(selectArray,className);
    CandidateFunc *last=cloneCand(result);
    n_ptr_array_unref(selectArray);
    return last;
}

static tree modifyOrInitField(SelectField *self,location_t loc ,tree lhs,tree rhs,FuncPointerError *errors)
{
      tree ret=rhs;
      //以上检查左值是函数指针变量
      //现在检查右值是不是一个类中的静态函数
//      printf("modifyOrInitField 00\n");
//      aet_print_tree_skip_debug(lhs);
//      printf("\n\n\n");
//      aet_print_tree_skip_debug(rhs);
      if(TREE_CODE(ret)!=FUNCTION_DECL && TREE_CODE(ret)!=ADDR_EXPR && TREE_CODE(ret)!=NOP_EXPR){
          aet_print_tree_skip_debug(ret);
          n_error("selectfield.c 不未实现该类型。\n");
          return NULL;
      }
      if(TREE_CODE(ret)==ADDR_EXPR)
          ret=TREE_OPERAND (ret, 0);
      else if(TREE_CODE(ret)==NOP_EXPR){
          ret=TREE_OPERAND (ret, 0);
          if(TREE_CODE(ret)!=ADDR_EXPR){
              return NULL;
          }
          ret=TREE_OPERAND (ret, 0);
      }
      if(BINFO_FLAG_1(ret)!=1)
          return NULL;
      char *funcName=IDENTIFIER_POINTER(DECL_NAME(ret));
      char sysName[255];
      char origFuncName[255];
      int len=aet_utils_get_orgi_func_and_class_name(funcName,sysName,origFuncName);
      if(len==0)
           return NULL;
      //printf("modifyOrInitField 11 funcName:%s sysName:%s origFuncName:%s\n",funcName,sysName,origFuncName);
      ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
      CandidateFunc *item=selectStaticFunc(self,loc,className,origFuncName,lhs,errors);

      tree last=NULL_TREE;
          //如果是field要加入指针，否则访问不到
      if(item!=NULL){
           access_controls_access_method(access_controls_get(),loc,item->classFunc);
           last=item->classFunc->fieldDecl;
           if(TREE_CODE(rhs)==ADDR_EXPR){
               location_t exprLoc = EXPR_LOCATION (rhs);
               last= build_unary_op (exprLoc, ADDR_EXPR, last, false);
           }else if(TREE_CODE(rhs)==NOP_EXPR){
                location_t exprLoc = EXPR_LOCATION (rhs);
                last= build_unary_op (exprLoc, ADDR_EXPR, last, false);
                last = build1_loc (exprLoc,NOP_EXPR, TREE_TYPE (last),last);
           }else{
               n_error("selectfield.c 不未实现该类型。\n");
           }
           freeCandidate_cb(item);
      }
      return last;
}


/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 *  1.AHashFunc func;
 *  func=AObject.strHash;
 *  2.AHashFunc func;
 *  func=5>3?AObject.strHash:xxx;
 *  3.AHashFunc func=AObject.strHash;
 *  4.AHashFunc func=5>3?AObject.strHash:xxx;
 */
tree select_field_modify_or_init_field(SelectField *self,location_t loc ,tree lhs,tree rhs,FuncPointerError **errors)
{
      FuncPointerError *localError=NULL;
      if(errors!=NULL)
          localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
      tree last=modifyOrInitField(self,loc,lhs,rhs,localError);
      if(errors!=NULL)
        *errors=localError;
      return last;
}

/////////////////---------------------------------------------调用类中的方法--------------------------

#define NORMAL_FUNC     1     //类中函数
#define CTOR_FUNC       2     //构造函数
#define STATIC_FUNC     3     //类中静态函数
#define IMPLICITLY_FUNC 4     //类中隐藏函数

static tree createTempFunction(tree field,int funcType)
{
   tree funid=DECL_NAME(field);
   tree type=TREE_TYPE(field);
   tree functionType=TREE_TYPE(type);
   tree decl = build_decl (0, FUNCTION_DECL, funid, default_function_type);
   TREE_TYPE(decl)=functionType;
   if(funcType==NORMAL_FUNC)
      c_aet_copy_lang(decl,field);
   return decl;
}

/**
 * 从CheckParamCallback再取出正确的静态函数
 * AHashTable.strHash是一个静态函数，也可作为函数指针参数据。
 * 如果有多个AHashTable.strHash，需要根据调用函数如 var->getData(AHashTable.strHash)的声明
 * 从AHashTable中找出合适的strHash静态函数
 */
static nboolean completeStaticField(SelectField *self,CandidateFunc *candidate,FuncPointerError *errors)
{
   int funcps=self->checkCallback->funcPointerCount;
   if(funcps==0)
       return TRUE;
   int i;
   for(i=0;i<funcps;i++){
      tree actual=self->checkCallback->funcPointers[i].actual;
      tree formal=self->checkCallback->funcPointers[i].formal;
      location_t loc=EXPR_LOCATION(actual);
      tree id=aet_utils_create_ident("temp_var_for_select_static_func");
      tree var= build_decl (loc, VAR_DECL, id, formal);
      tree ret=modifyOrInitField(self,loc,var,actual,errors);//关键在这里
      //printf("completeStaticField :%d paramNum:%d candidate:%p\n",i,self->checkCallback->funcPointers[i].paramNum,candidate);
      //aet_print_tree_skip_debug(ret);
      candidate->funcPointers[i].paramNum=self->checkCallback->funcPointers[i].paramNum;
      candidate->funcPointers[i].actual=actual;
      candidate->funcPointers[i].formal=formal;
      candidate->funcPointers[i].result=ret;
      if(!aet_utils_valid_tree(ret)){
          return FALSE;
      }
   }
   candidate->funcPointerCount=funcps;
   return TRUE;
}


static CandidateFunc * checkCallParam(SelectField *self,ClassFunc *func,tree decl,vec<tree, va_gc> *exprlist,vec<tree, va_gc> *origtypes,
        vec<location_t> arg_loc,location_t expr_loc,int ctorStaticOrNoramlFuncType,FuncPointerError *errors)
{
   tree  funcType = TREE_TYPE (decl);
   int count=0;
   int varargs_p = 1;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
        tree type=TREE_VALUE(al);
        if(type == void_type_node){
            //n_debug("有void_type_node count:%d 函数名:%s",count,IDENTIFIER_POINTER(DECL_NAME(decl)));
            varargs_p=0;
            break;
        }
        count++;
   }
   //跳过FuncGenParmInfo info 形参 在泛型函数中abc(Abc *self,FuncGenParmInfoinfo,....);aet_check_funcs_param会判断是否要跳过参数
   if(ctorStaticOrNoramlFuncType==STATIC_FUNC){
         if(exprlist && count!=exprlist->length()){
             nboolean ok1=class_func_is_func_generic(func);
             nboolean ok2=class_func_is_query_generic(func);
             n_debug("checkCallParam 参数个数不匹配! 检查是不是泛型函数 形参：%d 实参:%d 是泛型函数：ok:%d 是带问号泛型参数的函数:%d",count,exprlist->length(),ok1,ok2);
             return NULL;
         }
   }else{
       if(exprlist && count!=exprlist->length() && varargs_p==0){
           nboolean ok1=class_func_is_func_generic(func);
           nboolean ok2=class_func_is_query_generic(func);
           n_debug("checkCallParam 参数个数不匹配! 检查是不是泛型函数 形参：%d 实参:%d 是泛型函数：ok:%d 是带问号泛型参数的函数:%d",count,exprlist->length(),ok1,ok2);
           if(!ok1 && !ok2){
              return NULL;
           }
       }
   }
   aet_warn_and_error_reset();
   if(varargs_p)
       n_debug("checkCallParam 开始匹配参数 decl code:%s name:%s 是否有可变参数：%d",
                        get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),varargs_p);
   tree value=decl;
   mark_exp_read (value);
   init_check_param_callback(self);
   value= aet_check_funcs_param (expr_loc, arg_loc, value,exprlist, origtypes,self->checkCallback);
   if(value==error_mark_node){
      n_debug("checkCallParam 不能匹配参数 decl code:%s name:%s ",
              get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)));
   }else{
      n_debug("checkCallParam 有错误吗? decl code:%s name:%s 错误数:%d warn:%d ",
            get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),
            argsFuncsInfo.errorCount,argsFuncsInfo.warnCount);
      if(argsFuncsInfo.errorCount==0){
        CandidateFunc *candidate=createCandidate();
        nboolean ok=completeStaticField(self,candidate,errors);//完成类中静态函数的选取
        if(!ok){
            freeCandidate_cb(candidate);
            return NULL;
        }
        candidate->warnErr=aet_warn_and_error_copy();
        return candidate;
     }
  }
  return NULL;
}

/**
 * 先在本类中找
 * allscope==TRUE 找定义、声明、field，否则只找field
 */
static CandidateFunc *getFuncFromClass(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,
        GenericModel *generics,NPtrArray *funcArray,int funcType,FuncPointerError *errors)
{
    NList *okList=NULL;
    int i;
    setParamsForCheckCallback(self,className,generics);
    for(i=0;i<funcArray->len;i++){
        ClassFunc *item=(ClassFunc *)n_ptr_array_index(funcArray,i);
        if(strcmp(item->orgiName,orgiFuncName))
          continue;
            //查找语句
        n_debug("getFuncFromClass index:%d %s allscope:%d orgiFuncName:%s field?:%d",
                i,className->sysName,allscope,orgiFuncName,aet_utils_valid_tree(item->fieldDecl));
        tree decl=NULL_TREE;
        if(allscope){
            if(aet_utils_valid_tree(item->fieldDecl)){
                decl=createTempFunction(item->fieldDecl,funcType);
            }else if(aet_utils_valid_tree(item->fromImplDefine)){
                decl=item->fromImplDefine;
            }else{
                decl=item->fromImplDecl;
            }
        }else{
            if(aet_utils_valid_tree(item->fieldDecl) && (funcType==NORMAL_FUNC || funcType==CTOR_FUNC)){
                decl=createTempFunction(item->fieldDecl,funcType);
            }else if(aet_utils_valid_tree(item->fieldDecl) && funcType==STATIC_FUNC){
                decl=item->fieldDecl;
            }else if(aet_utils_valid_tree(item->fromImplDefine) && funcType==IMPLICITLY_FUNC){
                decl=item->fromImplDefine;
            }
        }
        if(decl==NULL_TREE)
            continue;
        FuncPointerError *error;
        CandidateFunc *candidate=checkCallParam(self,item,decl,exprlist, origtypes,arg_loc,expr_loc,funcType,errors);
        if(candidate!=NULL){
              candidate->classFunc=item;
              candidate->sysName=n_strdup(className->sysName);
              if(candidate->warnErr->errorCount==0 && candidate->warnErr->warnCount==0){
                    n_list_free_full(okList,freeCandidate_cb);
                    n_debug("checkParm 检查没有错误，没有警告，直接返回:%s %s className:%s\n",item->orgiName,item->mangleFunName,className->sysName);
                    return candidate;
              }
              okList=n_list_append(okList,candidate);
        }
    }
    CandidateFunc *okCand=filterGoodFunc(okList);
    CandidateFunc *result=cloneCand(okCand);
    n_list_free_full(okList,freeCandidate_cb);
    return result;
}


/**
 * 从类中找出最好的函数。
 */
static CandidateFunc *selectFuncByLocal(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,GenericModel *generics,FuncPointerError *errors)
{
    if(className==NULL || orgiFuncName==NULL || className->sysName==NULL)
        return NULL;
     NList *okList=NULL;
     n_debug("select_field_get_func 00 类名：%s 函数名：%s",className->sysName,orgiFuncName);
     NPtrArray *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
     if(funcArray!=NULL && funcArray->len>0){
         CandidateFunc *candidate=getFuncFromClass(self,className,orgiFuncName,exprlist,origtypes,arg_loc,
                                          expr_loc,allscope,generics,funcArray,NORMAL_FUNC,errors);
         if(candidate!=NULL){
            if(candidate->warnErr->warnCount==0){
                n_list_free_full(okList,freeCandidate_cb);
                return candidate;
            }
            okList=n_list_append(okList,candidate);
         }
     }
    //如果是field要加入指针，否则访问不到
     ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
     int i;
     for(i=0;i<info->ifaceCount;i++){
        ClassName *iface=&info->ifaces[i];
        n_debug("getSelectedFunc 11 在类%s中的接口%s中找方法。函数名：%s",className->sysName,iface->sysName,orgiFuncName);
        NPtrArray *funcArray=func_mgr_get_funcs(func_mgr_get(),iface);
        if(funcArray==NULL || funcArray->len==0)
             continue;
        CandidateFunc *ifaceCandidate=getFuncFromClass(self,iface,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc,
                allscope,generics,funcArray,NORMAL_FUNC,errors);
        n_debug("getSelectedFunc 22 在类%s中的接口%s中找方法。函数名：%s",className->sysName,iface->sysName,orgiFuncName);
        if(ifaceCandidate!=NULL){
            ifaceCandidate->implSysName=n_strdup(className->sysName);//接口由那个类实现的
            if(ifaceCandidate->warnErr->warnCount==0){
                n_list_free_full(okList,freeCandidate_cb);
                return ifaceCandidate;
            }
            okList=n_list_append(okList,ifaceCandidate);
        }
     }
      CandidateFunc *okCand=filterGoodFunc(okList);
      CandidateFunc *result=cloneCand(okCand);
      n_list_free_full(okList,freeCandidate_cb);
      return result;
}


static void selectFuncByRecursion(SelectField *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,GenericModel *generics,
        NPtrArray *selectedArray,FuncPointerError *errors)
{
    if(className==NULL || orgiName==NULL || className->sysName==NULL)
        return;
    CandidateFunc *result=selectFuncByLocal(self,className,orgiName,exprlist,origtypes,arg_loc,expr_loc,allscope,generics,errors);
    if(result!=NULL){
        if(result->warnErr->warnCount==0){
            n_ptr_array_remove_range(selectedArray,0,selectedArray->len);
            n_ptr_array_add(selectedArray,result);
            return;
        }
        n_ptr_array_add(selectedArray,result);
    }
     ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
     if(info==NULL)
         return;
     n_debug("当前类%s找不到匹配的函数，递归的向父类再找。classInfo:%p",className->sysName,info);
     selectFuncByRecursion(self,&info->parentName,orgiName,exprlist,origtypes,arg_loc,expr_loc,allscope,generics,selectedArray,errors);
}

/**
 * 从类中找出最好的函数。
 */
CandidateFunc *select_field_get_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,GenericModel *generics,FuncPointerError **errors)
{
    FuncPointerError *localError=NULL;
     if(errors!=NULL)
        localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
     CandidateFunc *candidate=selectFuncByLocal(self,className,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc,allscope,generics,localError);
     replaceFunctionPointer(candidate,exprlist);
     if(errors!=NULL)
        *errors=localError;
     return candidate;
}

/**
 * 从当前类遍历父类和接口
 */
CandidateFunc *select_field_get_func_by_recursion(SelectField *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,
            vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,GenericModel *generics,FuncPointerError **errors)
{
    FuncPointerError *localError=NULL;
    if(errors!=NULL)
       localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
    NPtrArray *selectArray=n_ptr_array_new_with_free_func(freeCandidate_cb);
    selectFuncByRecursion(self,className,orgiName,exprlist,origtypes,arg_loc,expr_loc,allscope,generics,selectArray,localError);
    if(selectArray->len==0){
        n_ptr_array_unref(selectArray);
        if(errors!=NULL)
           *errors=localError;
        return NULL;
    }
    n_ptr_array_sort(selectArray,sortCandidateFunc_cb);
    CandidateFunc *result=getBest(selectArray,className);
    CandidateFunc *candidate=cloneCand(result);
    n_ptr_array_unref(selectArray);
    replaceFunctionPointer(candidate,exprlist);
    if(errors!=NULL)
       *errors=localError;
    return candidate;
}

/**
 * 从类中找出最好的构造函数。
 */
CandidateFunc *select_field_get_ctor_func(SelectField *self,ClassName *className,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
      if(className==NULL ||  className->sysName==NULL)
         return NULL;
       NPtrArray *ctorFuncArray=func_mgr_get_constructors(func_mgr_get(),className);
       if(ctorFuncArray==NULL || ctorFuncArray->len==0)
           return NULL;
       FuncPointerError *localError=NULL;
       if(errors!=NULL)
            localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
       CandidateFunc *candidate= getFuncFromClass(self,className,className->userName,exprlist,origtypes,arg_loc,expr_loc,
                                                      FALSE,NULL,ctorFuncArray,CTOR_FUNC,localError);
       n_debug("select_field_get_ctor_func 最终选择 ----candidate:%p\n",candidate);
       replaceFunctionPointer(candidate,exprlist);
       if(errors!=NULL)
          *errors=localError;
       return candidate;
}

/**
 * 从类中找出最好的静态函数。
 */
CandidateFunc *select_field_get_static_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
      if(className==NULL || orgiFuncName==NULL || className->sysName==NULL)
           return NULL;
       NPtrArray *staticArray=func_mgr_get_static_funcs(func_mgr_get(),className);
       if(staticArray==NULL || staticArray->len==0)
           return NULL;
       FuncPointerError *localError=NULL;
       if(errors!=NULL)
           localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
       n_debug("select_field_get_static_func %s %s",className->sysName,orgiFuncName);
       CandidateFunc *candidate=getFuncFromClass(self,className,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc,
                FALSE,NULL,staticArray,STATIC_FUNC,localError);
       //如果是field要加入指针，否则访问不到
       replaceFunctionPointer(candidate,exprlist);
       if(errors!=NULL)
          *errors=localError;
       return candidate;
}

/**
 * 找出隐藏的静态函数。
 */
CandidateFunc *select_field_get_implicitly_static_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
      if(className==NULL || orgiFuncName==NULL || className->sysName==NULL)
           return NULL;
      NPtrArray *staticArray=func_mgr_get_static_funcs(func_mgr_get(),className);
      if(staticArray==NULL || staticArray->len==0)
            return NULL;
      FuncPointerError *localError=NULL;
      if(errors!=NULL)
         localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
       n_debug("select_field_get_static_func %s %s",className->sysName,orgiFuncName);
       CandidateFunc *candidate=getFuncFromClass(self,className,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc,
                                                    FALSE,NULL,staticArray,IMPLICITLY_FUNC,localError);
       //如果是field要加入指针，否则访问不到
       replaceFunctionPointer(candidate,exprlist);
       if(errors!=NULL)
          *errors=localError;
       return candidate;
}

/**
 * 获取隐藏的函数,调用者在implicitlycall.c中，不需要处理在选择过程中出现的错误。
 */
CandidateFunc *select_field_get_implicitly_func(SelectField *self,ClassName *className,char *orgiFuncName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,GenericModel *generics,FuncPointerError **errors)
{
     if(className==NULL || orgiFuncName==NULL || className->sysName==NULL)
        return NULL;
     NPtrArray *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
     if(funcArray==NULL || funcArray->len==0)
        return NULL;
     FuncPointerError *localError=NULL;
     if(errors!=NULL)
       localError=(FuncPointerError *)n_slice_new0(FuncPointerError);
     CandidateFunc *candidate=getFuncFromClass(self,className,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc,
                                                          FALSE,generics,funcArray,IMPLICITLY_FUNC,localError);
     replaceFunctionPointer(candidate,exprlist);
     if(errors!=NULL)
        *errors=localError;
     return candidate;
}


void   select_field_free_candidate(CandidateFunc *candidate)
{
    freeCandidate_cb(candidate);
}

void  select_field_free_func_pointer_error(FuncPointerError *funcPointerError)
{
    if(funcPointerError==NULL)
        return;
    int i;
    for(i=0;i<funcPointerError->count;i++){
        FuncPointerErrorInfo *item=(FuncPointerErrorInfo *)funcPointerError->message[i];
        if(item->sysName)
            n_free(item->sysName);
        n_slice_free(FuncPointerErrorInfo,item);
    }
    n_slice_free(FuncPointerError,funcPointerError);
}

/**
 * 打印类方法赋值给函数指针时出现的错误。
 */
void  select_field_printf_func_pointer_error(FuncPointerError *funcPointerError)
{

}


SelectField *select_field_get()
{
	static SelectField *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(SelectField));
		 selectFielInit(singleton);
	}
	return singleton;
}


