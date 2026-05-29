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
#include "gimple-expr.h"
#include "tree-iterator.h"
#include "opts.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "tree-inline.h"

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
#include "genericutil.h"
#include "genericquery.h"
#include "blockmgr.h"

/**
 * 找到了类中的静态函数，现在可以把由parser_static_create_temp_tree创建并
 * AET_LANG_FLAG_1(decl)=1;的参数据替换了
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
    if(TREE_CODE(type)==POINTER_TYPE && TREE_CODE(func)==FUNCTION_DECL && AET_LANG_FLAG_1(func)==1){
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


//实参是一个函数指针，形参是函数指针。实参来自类的静态变量。
static nboolean addFuncPointer_cb(CheckParamCallback *callback,int paramNum,tree actual,tree formal)
{
   if(!isStaticFunc(actual,formal)){
      return FALSE;
   }
   int index=callback->funcPointerCount;
   callback->funcPointers[index].paramNum=paramNum;
   callback->funcPointers[index].actual=actual;
   callback->funcPointers[index].formal=formal;
   callback->funcPointerCount++;
   return TRUE;
}

static void selectFielInit(SelectField *self)
{
   self->checkCallback=(CheckParamCallback *)n_slice_new(CheckParamCallback);
}


static CandidateFunc *createCandidate()
{
   CandidateFunc *candidate=(CandidateFunc *)n_slice_new0(CandidateFunc);
   candidate->sysName=NULL;
   candidate->implSysName=NULL;
   candidate->funcPointerCount=0;
   candidate->error=0;
   candidate->warn=0;
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
   candidate->error=src->error;
   candidate->warn=src->warn;
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
   int a=p1->warn;
   int b=p2->warn;
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
   n_slice_free(CandidateFunc,item);
}

static nint warnCompare_cb(nconstpointer  cand1,nconstpointer  cand2)
{
   CandidateFunc *p1 = (CandidateFunc *)cand1;
   CandidateFunc *p2 = (CandidateFunc *)cand2;
   int a=p1->warn;
   int b=p2->warn;
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
      if(first->warn==compare->warn){
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
            warnCount=item->warn;
            arrays[i]=item;
        }else{
            if(item->warn==warnCount)
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

static tree func_param_compare(SelectField *self,location_t init_loc, tree decl, tree init,tree origtype)
{
   CheckParamCallback *checkCallback=self->checkCallback;
   checkCallback->funcPointerCount=0;
   checkCallback->error=0;
   checkCallback->warn=0;

   tree value, type;
   bool npc = false;
   bool int_const_expr = false;
   bool arith_const_expr = false;
   /* If variable's type was invalidly declared, just ignore it.  */
   type = TREE_TYPE (decl);
   if (TREE_CODE (type) == ERROR_MARK){
      global_dc->aetRunning=FALSE;
      global_dc->aetUserData=NULL;
      global_dc->aet_info=NULL;
      return error_mark_node;
   }

   /* Digest the specified initializer into an expression.  */
   if (init){
      npc = null_pointer_constant_p (init);
      int_const_expr = (TREE_CODE (init) == INTEGER_CST  && !TREE_OVERFLOW (init) && INTEGRAL_TYPE_P (TREE_TYPE (init)));
      /* Not fully determined before folding.  */
      arith_const_expr = true;
   }
   bool constexpr_p = (VAR_P (decl) && C_DECL_DECLARED_CONSTEXPR (decl));
   value = aet_digest_init (init_loc, decl,type, init, origtype, npc, int_const_expr,
         arith_const_expr, true,TREE_STATIC (decl) || constexpr_p, constexpr_p);
   global_dc->aetRunning=FALSE;
   global_dc->aetUserData=NULL;
   global_dc->aet_info=NULL;
   return value;
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
   if(rightVarargs)
       n_debug("selectfield.c checkStaticFuncParam 开始匹配参数 decl code:%s name:%s 是否有可变参数：%d ",
                        get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),rightVarargs);
   tree value=build_unary_op (loc, ADDR_EXPR, decl, false);
   mark_exp_read (value);
   value= func_param_compare(self,loc,lhs,value,NULL_TREE);

   if(value==error_mark_node){
      n_debug("selectfield.c checkStaticFuncParam 不能匹配参数 decl code:%s name:%s 错误数:%d warn:%d",
              get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),self->checkCallback->error,self->checkCallback->warn);
   }else{
      n_debug("checkStaticFuncParam 有错误吗? decl code:%s name:%s 错误数:%d warn:%d ",
            get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),
            self->checkCallback->error,self->checkCallback->warn);
      if(self->checkCallback->error==0){
        CandidateFunc *candidate=createCandidate();
        candidate->error=self->checkCallback->error;
        candidate->warn=self->checkCallback->warn;
        return candidate;
     }
  }
  return NULL;
}

static CandidateFunc *getStaticFuncFromClass(SelectField *self,location_t loc,ClassName *className,
        char *orgiFuncName,tree lhs,FuncPointerError *errors)
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
              //printf("selectfield.c func_pointer_check ok:%d\n",ok);
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
              if(candidate->error==0 && candidate->warn==0){
                    n_list_free_full(okList,freeCandidate_cb);
                    n_debug("checkParm 静态函数 检查没有错误，没有警告，直接返回:%s %s className:%s\n",item->orgiName,item->mangleFunName,className->sysName);
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
        if(candidate->warn==0){
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
          if(candidate->warn==0){
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
        if(result->warn==0){
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
      n_error("selectfield.c 还未实现该类型。\n");
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
   if(AET_LANG_FLAG_1(ret)!=1)
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
      }else if(TREE_CODE(rhs)==FUNCTION_DECL){
         ;
      }else{
         aet_print_tree_skip_debug(rhs);
         n_error("selectfield.c 还未实现该类型。\n");
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

/////////////////////////////////新功能-------------------------
#define CALLBACK_STOP 0
#define CALLBACK_SELECT_FIELD 1
#define CALLBACK_BUILD_FUNCTION_CALL 2

/**
 * c-typeck.cc调用
 * action = 0 不调用 convert_argument
 * action = 1 调用 convert_argument，但warnopt设为0
 * action = 2 调用 convert_argument,但warnopt设为1
 * action = - 1 出错。
 */
tree   select_field_call_back00(location_t ploc, tree function, tree fundecl,tree type, tree origtype,
        tree val, tree valtype, bool npc, tree rname, int parmnum, int argnum,
        bool excess_precision, int warnopt,int *action)
{
   SelectField *self=select_field_get();
   tree parmval=NULL_TREE;
   CheckParamCallback *checkCallback=self->checkCallback;
   int state=checkCallback->state;
   if(state==CALLBACK_STOP){
      *action=1;
      return parmval;
   }

   //function 调用函数 fundecl函数声明 type 函数参数类型  origtype 实参的原始类型 val具体的实参 valtype具体实参的类型 npc实参是否是空指针
   //rname =function parmnum当前第几个参数 argnum 总的参数个数+1 excess_precision 超精度 0 是否警告
   nboolean isGenericType=generic_util_is_generic_pointer(type);//参数是不是:setData(E data)中的E data aet_generic_E aet_generic_F等
   n_debug("select_field_call_back  origtype code:%s  parmnum:%d 是不是泛型:%d function:%p state:%d",
         origtype?get_tree_code_name(TREE_CODE(origtype)):"NULL", parmnum,isGenericType,function,state);
   aet_print_tree(type);
   aet_print_tree(val);
   aet_print_tree(valtype);
   aet_print_tree(origtype);
   n_debug("xxx--  %d %d %d %d\n",SCALAR_FLOAT_TYPE_P (type),INTEGRAL_TYPE_P (valtype),warn_traditional_conversion,warn_traditional);



   if(isGenericType){
      if(state==CALLBACK_SELECT_FIELD){
         parmval=generic_call_check_parm(generic_call_get(), ploc,function, fundecl,
         type,  origtype,  val,  valtype, npc, rname, parmnum,argnum,excess_precision,  warnopt,
         checkCallback->className,checkCallback->generics);
         if(parmval==error_mark_node){
            *action=-1;//出错
            n_debug("泛型检查结果 parmval==error_mark_node\n");
         }else if(parmval==NULL_TREE){
            *action=1;//由c-typeck.cc处理
            n_debug("泛型检查结果 parmval==NULL_TREE 由c-typeck.cc处理\n");
         }else{
            n_debug("泛型检查结果 parmval 正确 c-typeck.cc不处理\n");
            *action=0;//c-typeck.cc不再处理
         }
      }else if(state==CALLBACK_BUILD_FUNCTION_CALL){
         n_debug("替换泛型参数 generic_call_replace_parm_new 状态：CALLBACK_BUILD_FUNCTION_CALL call:%d\n",checkCallback->className->sysName);
         parmval=generic_call_replace_parm_new(generic_call_get(), ploc,function, fundecl,
         type,  origtype,  val,  valtype, npc, rname, parmnum,argnum,excess_precision,  warnopt,
         checkCallback->className,checkCallback->generics);
         aet_print_tree(parmval);
         if(parmval==error_mark_node){
            *action=-1;
         }else if(parmval==NULL_TREE){
            *action=1;//由c-typeck.cc处理
         }else{
            *action=0;//c-typeck.cc不再处理
            generic_func_check_parm(generic_func_get(),
                  checkCallback->className->sysName,checkCallback->classFunc,parmnum,parmval,function);
         }
      }else{
         *action=1;//由c-typeck.cc处理
      }
   }else{
      //           printf("check -----形参:\n");
      //           printNode(origtype);
      //             printf("check -----实参:\n\n");
      //             printNode(val);
      //             printf("check -----type:\n\n");
      //             printNode(type);
      //             printf("check -----valtype:\n\n");
      //             printNode(valtype);
      // printf("check -----is static func:%d\n\n",is);
      if(state==CALLBACK_SELECT_FIELD){
         if(addFuncPointer_cb(checkCallback,parmnum,val,type)){
            parmval=val;
            *action=0;
         }else{
            *action=1;//由c-typeck.cc处理 c-typeck.cc将调用 convert_argument
            // parmval=convert_argument(ploc, function, fundecl, type, origtype,val, valtype, npc, rname, parmnum, argnum,excess_precision, 0);
         }
      }else{
         *action=1;//由c-typeck.cc处理
         //parmval=convert_argument(ploc, function, fundecl, type, origtype,val, valtype, npc, rname, parmnum, argnum,excess_precision, 0);
      }
   }

   return parmval;
}

//aet_convert_argument最后一个参数是warnopt,在c-typeck.cc中默认是零，
//但是state==CALLBACK_SELECT_FIELD时，需要打开，以便生成警告信息。
tree   select_field_call_back(location_t ploc, tree function, tree fundecl,tree type, tree origtype,
        tree val, tree valtype, bool npc, tree rname, int parmnum, int argnum,
        bool excess_precision)
{
   SelectField *self=select_field_get();
   tree parmval=NULL_TREE;
   CheckParamCallback *checkCallback=self->checkCallback;
   int state=checkCallback->state;
   if(state==CALLBACK_STOP){
      return aet_convert_argument (ploc, function, fundecl, type, origtype,
                         val, valtype, npc, rname, parmnum, argnum,
                         excess_precision, 0/*warnopt=0*/);
   }

   //function 调用函数 fundecl函数声明 type 函数参数类型  origtype 实参的原始类型 val具体的实参 valtype具体实参的类型 npc实参是否是空指针
   //rname =function parmnum当前第几个参数 argnum 总的参数个数+1 excess_precision 超精度 0 是否警告
   nboolean isGenericType=generic_util_is_generic_pointer(type);//参数是不是:setData(E data)中的E data aet_generic_E aet_generic_F等
   n_debug("select_field_call_back  origtype code:%s  parmnum:%d 是不是泛型:%d function:%p state:%d",
         origtype?get_tree_code_name(TREE_CODE(origtype)):"NULL", parmnum,isGenericType,function,state);

   if(isGenericType){
      if(state==CALLBACK_SELECT_FIELD){
         parmval=generic_call_check_parm(generic_call_get(), ploc,function, fundecl,
         type,  origtype,  val,  valtype, npc, rname, parmnum,argnum,excess_precision,  0/*warnopt=0*/,
         checkCallback->className,checkCallback->generics);
         if(parmval==error_mark_node){
            n_debug("泛型检查结果 parmval==error_mark_node\n");
            error_at(ploc,"aet convert arguments error");
            return error_mark_node;
         }else if(parmval==NULL_TREE){
            n_debug("泛型检查结果 parmval==NULL_TREE 由c-typeck.cc处理\n");
            return aet_convert_argument (ploc, function, fundecl, type, origtype,
                                    val, valtype, npc, rname, parmnum, argnum,
                                    excess_precision, 1/*warnopt=0*/);
         }else{
            n_debug("泛型检查结果 parmval 正确 c-typeck.cc不处理\n");
            return parmval;
         }
      }else if(state==CALLBACK_BUILD_FUNCTION_CALL){
         n_debug("替换泛型参数 generic_call_replace_parm_new 状态：CALLBACK_BUILD_FUNCTION_CALL call:%d\n",checkCallback->className->sysName);
         parmval=generic_call_replace_parm_new(generic_call_get(), ploc,function, fundecl,
         type,  origtype,  val,  valtype, npc, rname, parmnum,argnum,excess_precision,  0/*warnopt=0*/,
         checkCallback->className,checkCallback->generics);
         aet_print_tree(parmval);
         if(parmval==error_mark_node){
            return error_mark_node;
         }else if(parmval==NULL_TREE){
            return aet_convert_argument (ploc, function, fundecl, type, origtype,
                                      val, valtype, npc, rname, parmnum, argnum,
                                      excess_precision, 0/*warnopt=0*/);
         }else{
            generic_func_check_parm(generic_func_get(),
                  checkCallback->className->sysName,checkCallback->classFunc,parmnum,parmval,function);
            return parmval;
         }
      }else{
         return aet_convert_argument (ploc, function, fundecl, type, origtype,
                                         val, valtype, npc, rname, parmnum, argnum,
                                         excess_precision, 0/*warnopt=0*/);
      }
   }else{
      if(state==CALLBACK_SELECT_FIELD){
         if(addFuncPointer_cb(checkCallback,parmnum,val,type)){
           return val;
         }else{
            return aet_convert_argument (ploc, function, fundecl, type, origtype,
                                    val, valtype, npc, rname, parmnum, argnum,
                                    excess_precision, 1/*warnopt=0*/);
         }
      }else{
         return aet_convert_argument (ploc, function, fundecl, type, origtype,
                                 val, valtype, npc, rname, parmnum, argnum,
                                 excess_precision, 0/*warnopt=0*/);
      }
   }

}


static void aet_info_cb(int kind,const char *gmsgid,void *userData)
{
   CheckParamCallback *checkCallback=(CheckParamCallback *)userData;
   //printf("aet_info_cb 回调 %d %d %d %d %d gmsgid:%s\n",kind,DK_ERROR,DK_PERMERROR,DK_WARNING,DK_PEDWARN,gmsgid);
   if(kind==DK_ERROR || kind==DK_PERMERROR)
      checkCallback->error++;
   if(kind==DK_WARNING || kind==DK_PEDWARN)
      checkCallback->warn++;
}

/**
 * 拷贝 vec<tree, va_gc> *params
 */
static vec<tree, va_gc> *cloneParams(vec<tree, va_gc> *exprlist)
{
   copy_body_data id;
   tree param;
   hash_map<tree, tree> decl_map;

   memset (&id, 0, sizeof (id));
   id.src_fn = current_function_decl;
   id.dst_fn = current_function_decl;
   // id.src_cfun = DECL_STRUCT_FUNCTION (fn);
   id.decl_map = &decl_map;

   id.copy_decl = copy_decl_no_change;
   id.transform_call_graph_edges = CB_CGE_DUPLICATE;
   id.transform_new_cfg = false;
   id.transform_return_to_modify = false;
   id.transform_parameter = true;
   // id.transform_lang_insert_block = NULL;

   /* Make sure not to unshare trees behind the front-end's back
   since front-end specific mechanisms may rely on sharing.  */
   id.regimplify = false;
   id.do_not_unshare = true;
   id.do_not_fold = false;

   /* We're not inside any EH region.  */
   id.eh_lp_nr = 0;
   vec<tree, va_gc> *clone;
   clone = make_tree_vector ();
   tree arg=NULL_TREE;
   int i;
   for (i = 0; exprlist->iterate (i, &arg); ++i){
      // printf("cloneParams------参数 %d\n",i);
      // printNode(arg);
      // tree newParams = remap_decl (arg, &id);
      // printf("cloneParams------新参数 %d\n",i);
      walk_tree (&arg, copy_tree_body_r, &id, NULL);
      tree newParams =arg;
      // printNode(newParams);
      vec_safe_push (clone, newParams);
   }
   return clone;
}

/**
 * 测试实参与函数参数是否匹配
 * c_build_function_call_vec 时调用到 c-typeck.cc convert_arguments
 * 在 convert_arguments 调用  select_field_call_back
 */
static tree testParam(SelectField *self,location_t loc, vec<location_t> arg_loc,tree function,
        vec<tree, va_gc> *params,vec<tree, va_gc> *origtypes,ClassName *className,GenericModel *generics)
{
   CheckParamCallback *checkCallback=self->checkCallback;
   checkCallback->className=className;
   checkCallback->generics=generics;
   checkCallback->funcPointerCount=0;
   checkCallback->error=0;
   checkCallback->warn=0;
   checkCallback->state=CALLBACK_SELECT_FIELD;
   vec<tree, va_gc> *exprList=params==NULL?NULL:cloneParams(params);
   global_dc->aetRunning=TRUE;
   global_dc->aetUserData=checkCallback;
   global_dc->aet_info=aet_info_cb;
   tree ret= c_build_function_call_vec (loc, arg_loc, function, exprList, origtypes);
   global_dc->aetRunning=FALSE;
   global_dc->aetUserData=NULL;
   global_dc->aet_info=NULL;
   checkCallback->state=CALLBACK_STOP;
   if(exprList!=NULL)
      release_tree_vector(exprList);
   return ret;
}

static CandidateFunc * checkCallParam(SelectField *self,ClassFunc *func,tree decl,vec<tree, va_gc> *exprlist,vec<tree, va_gc> *origtypes,
        vec<location_t> arg_loc,location_t expr_loc,int ctorStaticOrNoramlFuncType,FuncPointerError *errors,
        ClassName *className,GenericModel *generics)
{
   tree  funcType = TREE_TYPE (decl);
   int count=0;
   int varargs_p = 1;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      if(type == void_type_node){
         n_debug("有void_type_node count:%d 函数名:%s",count,IDENTIFIER_POINTER(DECL_NAME(decl)));
         varargs_p=0;
         break;
      }
      count++;
   }
   //跳过FuncGenParmInfo info 形参 在泛型函数中abc(Abc *self,FuncGenParmInfoinfo,....);aet_check_funcs_param会判断是否要跳过参数
   if(ctorStaticOrNoramlFuncType==STATIC_FUNC){
      if(exprlist && count!=exprlist->length()){
         nboolean ok1=class_func_is_func_generic(func);
         nboolean ok2=class_func_have_query_param(func);
         n_debug("checkCallParam 静态 参数个数不匹配! 检查是不是泛型函数 形参：%d 实参:%d 是泛型函数：ok:%d 是带问号泛型参数的函数:%d",
                  count,exprlist->length(),ok1,ok2);
         return NULL;
      }
   }else{
      if(exprlist && count!=exprlist->length() && varargs_p==0){
         nboolean ok1=class_func_is_func_generic(func);
         nboolean ok2=class_func_have_query_param(func);
         n_debug("checkCallParam 参数个数不匹配! 检查是不是泛型函数 形参：%d 实参:%d 是泛型函数：ok:%d 是带问号泛型参数的函数:%d",
                  count,exprlist->length(),ok1,ok2);
         if(!ok1 && !ok2){
            return NULL;
         }
      }
   }
   if(varargs_p)
      n_debug("checkCallParam 开始匹配参数 decl code:%s name:%s 是否有可变参数：%d",
            get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),varargs_p);
   tree value=decl;
   mark_exp_read (value);
   value= testParam (self,expr_loc, arg_loc, value,exprlist, origtypes,className,generics);
   if(value==error_mark_node){
      n_debug("checkCallParam 不能匹配参数 decl code:%s name:%s ",get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)));
   }else{
      n_debug("checkCallParam 有错误吗? decl code:%s name:%s 错误数:%d warn:%d ",
      get_tree_code_name(TREE_CODE(decl)),IDENTIFIER_POINTER(DECL_NAME(decl)),
      self->checkCallback->error,self->checkCallback->warn);
      if(self->checkCallback->error==0){
         CandidateFunc *candidate=createCandidate();
         nboolean ok=completeStaticField(self,candidate,errors);//完成类中静态函数的选取
         if(!ok){
            freeCandidate_cb(candidate);
            return NULL;
         }
         candidate->error=self->checkCallback->error;
         candidate->warn=self->checkCallback->warn;
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
   for(i=0;i<funcArray->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(funcArray,i);
      if(strcmp(item->orgiName,orgiFuncName))
         continue;
      //查找语句
      n_debug("getFuncFromClass index:%d %s allscope:%d orgiFuncName:%s field?:%d item->fromImplDefine:%p",
            i,className->sysName,allscope,orgiFuncName,aet_utils_valid_tree(item->fieldDecl),item->fromImplDefine);
      tree decl=NULL_TREE;
      if(allscope){
         if(aet_utils_valid_tree(item->fieldDecl)){
            decl=createTempFunction(item->fieldDecl,funcType);
         }else if(aet_utils_valid_tree(item->fromImplDefine)){
            decl=item->fromImplDefine;
         }else{
            error_at(expr_loc,"选择类方法时，没有找到声明和实现。%qs %qs",className->userName,orgiFuncName);
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
      CandidateFunc *candidate=checkCallParam(self,item,decl,exprlist, origtypes,
               arg_loc,expr_loc,funcType,errors,className,generics);
      if(candidate!=NULL){
         candidate->classFunc=item;
         candidate->sysName=n_strdup(className->sysName);
         if(candidate->error==0 && candidate->warn==0){
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
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,
        nboolean allscope,GenericModel *generics,FuncPointerError *errors)
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
         if(candidate->warn==0){
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
         if(ifaceCandidate->warn==0){
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
      if(result->warn==0){
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
   CandidateFunc *candidate= getFuncFromClass(self,
         className,className->userName,exprlist,origtypes,arg_loc,expr_loc,FALSE,NULL,ctorFuncArray,CTOR_FUNC,localError);
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
   CandidateFunc *candidate=getFuncFromClass(self,
         className,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc,FALSE,NULL,staticArray,STATIC_FUNC,localError);
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
   CandidateFunc *candidate=getFuncFromClass(self,
         className,orgiFuncName,exprlist,origtypes,arg_loc,expr_loc, FALSE,NULL,staticArray,IMPLICITLY_FUNC,localError);
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
 * 1.调用的函数返回值是问号泛型
 * 2.函数有泛型参数
 */
static void setGenericQuery(ClassFunc *classFunc,char *sysName,tree last)
{
   if(class_func_have_query_param(classFunc) || class_func_have_generic_class_parm(classFunc)){
      n_debug("在funcall.c调用:%s 是问号泛型:%d 方法有泛型参数：%d\n",classFunc->orgiName,
      class_func_have_query_param(classFunc),class_func_have_generic_class_parm(classFunc));
      nboolean ok=generic_query_have_query_caller(generic_query_get(),classFunc,last);
      //printf("在funcall.c是问号泛型---xx--- %d\n",ok);
      if(ok){
         nboolean isGenericClass=generic_query_is_generic_class(generic_query_get(),last);
         if(isGenericClass){ //真正的调用者是泛型类
            GenericModel *model=generic_query_get_call_generic(generic_query_get(),last);
            if(model==NULL){
               error_at(input_location,"调用方法%qs的对象，并没有定义泛型。",classFunc->orgiName);
               return;
            }
            if(generic_model_get_undefine_count(model)==0 && !generic_model_have_query(model)){
               n_debug("调用转成new$形式的泛型定义对象 model:%s file:%s\n",generic_model_tostring(model),in_fnames[0]);
            }else{
               n_warning("不能作为泛型定义加入到defineObject中 undefineCount:%d query:%d",
                     generic_model_get_undefine_count(model),generic_model_have_query(model));
            }
         }
      }
   }
}

/**
 * build 函数调用
 * setGenericQuery与10.4不一样。
 * setGenericQuery只在两个地方调用，
 * func_call_deref_select 和 func_call_select
 */
tree select_field_build_function_call_vec(SelectField *self,location_t loc, vec<location_t> arg_loc,
        tree function, vec<tree, va_gc> *params, vec<tree, va_gc> *origtypes,SelectFunc *selectFunc)
{
   //只针对var->setData() 获取的是var变量定义的泛型
   GenericModel *generics=generic_call_get_generic_from_component_ref(generic_call_get(),function);//对象是否有泛型的真实类型.
   GenericModel *funcGenericDefine=c_aet_get_func_generics_model(function);//如果funcGenericDefine是有效的说明这是一个泛型函数
   ClassFunc *classFunc=selectFunc->classFunc;
   char *sysName=selectFunc->sysName;
   if(sysName==NULL){
      n_error("在函数select_field_build_function_call_vec 缺少类名");
      return NULL_TREE;
   }
   if(classFunc==NULL){
      n_error("在函数select_field_build_function_call_vec找不到ClassFunc。%s",sysName);
      return NULL_TREE;
   }
   if(selectFunc->genericQuery==1)
      setGenericQuery(classFunc,sysName,function);
   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
   //printf("select_field_build_function_call_vec -- generics:%p funcGenericDefine:%p sysName:%s %s %d\n",
   // generics,funcGenericDefine,sysName,classFunc->orgiName,params?params->length():0);
   CheckParamCallback *checkCallback=self->checkCallback;
   checkCallback->className=className;
   checkCallback->generics=generics;
   checkCallback->classFunc=classFunc;
   checkCallback->state=CALLBACK_BUILD_FUNCTION_CALL;
   tree ret= c_build_function_call_vec (loc, arg_loc, function, params, origtypes);
   checkCallback->state=CALLBACK_STOP;
   return ret;
}

/**
 * 打印类方法赋值给函数指针时出现的错误。
 */
void  select_field_printf_func_pointer_error(FuncPointerError *funcPointerError)
{

}

/**
 * 检查外部函数的参数与实参是否匹配
 */
nboolean select_field_match_outside_function(SelectField *self,tree funcdecl,vec<tree, va_gc> *exprList,
      vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc)
{
      tree  funcType = TREE_TYPE (funcdecl);
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
      if(varargs_p)
         return TRUE;
      int expListCount=exprList==NULL?0:exprList->length();
       if(count!=expListCount)
         return FALSE;
      tree value=funcdecl;
      char *fn=IDENTIFIER_POINTER(DECL_NAME(funcdecl));
      mark_exp_read (value);
      value= testParam (self,expr_loc, arg_loc, value,exprList, origtypes,NULL,NULL);
      if(value==error_mark_node){
          n_debug("---checkCallParam 不能匹配参数 decl code:%s name:%s ",
                  get_tree_code_name(TREE_CODE(funcdecl)),IDENTIFIER_POINTER(DECL_NAME(funcdecl)));
          return FALSE;

       }else{
          n_debug("xxxcheckCallParam 有错误吗? decl code:%s name:%s 错误数:%d warn:%d ",
                get_tree_code_name(TREE_CODE(funcdecl)),IDENTIFIER_POINTER(DECL_NAME(funcdecl)),
                self->checkCallback->error,self->checkCallback->warn);
          if(self->checkCallback->error!=0){
             n_debug("参数不正确----- :%d\n",self->checkCallback->error);
            return FALSE;
         }
      }
      return TRUE;
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


