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
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "c-aet.h"
#include "aetutils.h"
#include "genericquery.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "classimpl.h"
#include "genericexpand.h"
#include "blockmgr.h"
#include "funcmgr.h"


static void genericQueryInit(GenericQuery *self)
{

}

/**
 * 获取引用的类和类中的域
 * A->B->C->func;
 * 生成 C,B,A顺序
 */
static void refLink(tree func,NPtrArray *refArray)
{
    tree op0=TREE_OPERAND (func, 0);
    tree op1=TREE_OPERAND (func, 1);//域成员 函数名或变量名
    if(TREE_CODE(op0)==INDIRECT_REF){ //var->getName();
       tree component=TREE_OPERAND(op0,0);
       n_ptr_array_add(refArray,op1);
       if(TREE_CODE(component)==COMPONENT_REF){
           refLink(component,refArray);
       }else{
           n_ptr_array_add(refArray,component);
       }
    }else if(TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL){ //var.getName();
         n_ptr_array_add(refArray,op1);
         n_ptr_array_add(refArray,op0);
    }else if(TREE_CODE(op0)==COMPONENT_REF){
        printf("refLink-----33 \n");
         refLink(op0,refArray);
    }
}

static tree getField(tree actual)
{
    enum tree_code code=TREE_CODE(actual);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
    if(code==COMPONENT_REF){
        NPtrArray *refArray=n_ptr_array_new();
        refLink(actual,refArray);
        tree field=n_ptr_array_index(refArray,0);
        actual=field;
        n_ptr_array_unref(refArray);
    }
    return actual;
}


static tree getFieldType(tree field)
{
       tree type=TREE_TYPE(field);
       if(TREE_CODE(type)==POINTER_TYPE){
           tree functype=TREE_TYPE(type);
           if(TREE_CODE(functype)==FUNCTION_TYPE){
               type=TREE_TYPE(functype); //得到的是retn
           }
       }
       return type;
}

//如果在isAet中并且时self,不需要判断是否定义了泛型
static nboolean isSelf(GenericQuery *self,tree field)
{
    c_parser *parser=self->parser;
    if(parser->isAet || parser->isGenericState || parser->isTestGenericBlockState){
        if(TREE_CODE(field)==PARM_DECL){
          char *fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
          if(strcmp(fieldName,"self")==0)
             return TRUE;

        }
    }
    return FALSE;
}

static tree getFieldInfo(tree field,char *fN,tree *lastField)
{
      char *fieldName=NULL;
      tree type=NULL_TREE;
      if(TREE_CODE(field)==FIELD_DECL){
            type=getFieldType(field);
            fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
      }else if(TREE_CODE(field)==VAR_DECL || TREE_CODE(field)==PARM_DECL){
            type=TREE_TYPE(field);
            fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
      }else if(TREE_CODE(field)==CALL_EXPR){
            tree call=CALL_EXPR_FN (field);
            field=getField(call);
            type=getFieldType(field);
            if(TREE_CODE(field)==ADDR_EXPR){
                tree func=TREE_OPERAND (field, 0);
                if(TREE_CODE(func)==FUNCTION_DECL){
                    GenericModel *mm=c_aet_get_generics_model(func);
                    printf("在genericquery.c getFieldInfo mode iss model:%s\n",generic_model_tostring(mm));
                    field=func;
                }
            }
            fieldName=IDENTIFIER_POINTER(DECL_NAME(field));//调用类中的函数，返回值中的泛型存在field中，而不是返回的type中。
      }else if(TREE_CODE(field)==COMPONENT_REF){
          tree func=TREE_OPERAND (field, 1);
          type=TREE_TYPE(field);
          type=getFieldType(type);
          fieldName=IDENTIFIER_POINTER(DECL_NAME(func));
          field=func;
      }else if(TREE_CODE(field)==NON_LVALUE_EXPR){
          type=TREE_TYPE(field);
          type=getFieldType(type);
          tree func=TREE_OPERAND (field, 0);
          if(TREE_CODE(func)==COMPONENT_REF){
              func=TREE_OPERAND (func, 1);
              fieldName=IDENTIFIER_POINTER(DECL_NAME(func));
              field=func;
          }else{
              n_error("在genericquery.c中getFieldInfo,没有处理类型:%s",get_tree_code_name(TREE_CODE(func)));
          }
      }else{
           return type;
      }
      if(fieldName!=NULL)
          sprintf(fN,"%s",fieldName);
      *lastField=field;
      return type;
}

/**
 * 检查
 * A->B->getName
 * 中A/B是否带问号泛型,如果有，这是错误的。
 * 返回有多少个泛型类在调用栈中
 */
static int queryFieldCall(GenericQuery *self,NPtrArray *refArray,char **errorClassName,char **errorFieldName)
{
       int i;
       for(i=refArray->len-1;i>=0;i--){
          tree field=n_ptr_array_index(refArray,i);
          char fieldName[256];
          tree last=NULL_TREE;
          tree type=getFieldInfo(field,fieldName,&last);
          field=last;
          if(type==NULL_TREE){
              aet_print_tree_skip_debug(field);
              n_error("在genericquery.c中的queryFieldCall有未处理的类型。请报告此错误。");
              return 0;
          }
          char *sysClassName=class_util_get_class_name(type);
          n_debug("queryFieldCall ---00 %d sysClassName:%s fieldName:%s\n",i,sysClassName,fieldName);
          if(sysClassName!=NULL){
              ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
              if(class_info_is_generic_class(info)){
                  GenericModel *gen=c_aet_get_generics_model(field);
                  if(!gen){
                      if(isSelf(self,field)){
                          continue;
                      }
                     *errorClassName=n_strdup(sysClassName);
                     *errorFieldName=n_strdup(fieldName);
                      return -1;//说明域没有声明泛型
                  }
                  if(generic_model_have_query(gen)){
                       if(TREE_CODE(field)==PARM_DECL || TREE_CODE(field)==FIELD_DECL){
                            continue;
                       }
                       *errorClassName=n_strdup(sysClassName);
                       *errorFieldName=n_strdup(fieldName);
                       return -2;
                  }
              }
          }
       }
       return 0;
}

static nboolean haveQueryCaller(GenericQuery *self,NPtrArray *refArray)
{
       int i;
       for(i=refArray->len-1;i>=0;i--){
          tree field=n_ptr_array_index(refArray,i);
          char fieldName[256];
          tree last=NULL_TREE;
          tree type=getFieldInfo(field,fieldName,&last);
          field=last;
          if(type==NULL_TREE){
              aet_print_tree_skip_debug(field);
              n_error("在genericquery.c中的haveQueryCaller有未处理的类型。请报告此错误。");
              return FALSE;
          }
          char *sysClassName=class_util_get_class_name(type);
          n_debug("queryFieldCall ---00 %d sysClassName:%s fieldName:%s\n",i,sysClassName,fieldName);
          if(sysClassName!=NULL){
              ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
              if(class_info_is_generic_class(info)){
                  GenericModel *gen=c_aet_get_generics_model(field);
                  if(!gen){
                      if(isSelf(self,field)){
                          continue;
                      }
                      n_error("%s是泛型类%s，但没有定义泛型。",fieldName,sysClassName);
                      return FALSE;
                  }
                  if(generic_model_have_query(gen)){
                       if(TREE_CODE(field)==PARM_DECL || TREE_CODE(field)==FIELD_DECL){
                            continue;
                       }
                       return TRUE;
                  }
              }
          }
       }
       return FALSE;
}


/**
 * 检查调用的类中是否有问号泛型，如果有，不能调用带泛型参数的方法
 * 如a->getName; 检查的是a是否带有问号泛型
 * component_ref 树的TREE_OPERAND (func, 1);总时指向最后一个被调用的元素。
 * 比如a->b->c->getName getName总时等于REE_OPERAND (func, 1)
 * 所以要查找的变量是a->b->c三个中是否存在问号泛型，如果存在返回false
 */
nboolean generic_query_have_query_caller00(GenericQuery *self,ClassFunc *classFunc,tree callFunc)
{
    enum tree_code code=TREE_CODE(callFunc);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
    if(code==COMPONENT_REF){
        NPtrArray *refArray=n_ptr_array_new();
        refLink(callFunc,refArray);
        char *sysClassName=NULL;
        char *varName=NULL;
        int re=queryFieldCall(self,refArray,&sysClassName,&varName);
        if(re<0){
            if(re==-1)
               error_at(input_location,"类%qs中变量%qs没有定义泛型，不能访问带泛型参数的方法:%qs!",sysClassName,varName,classFunc->orgiName);
            else if(re==-2){
               //检查classFunc中所有的参数都是?可以调用否则不能调用
                nboolean  all=  class_func_have_all_query_parm(classFunc);
                if(!all)
                   error_at(input_location,"类%qs中变量%qs定义的是通用泛型<?>，只能访问全部带问号泛型参数的方法:%qs!",sysClassName,varName,classFunc->orgiName);
            }
        }
        n_ptr_array_unref(refArray);
        return re==0;
    }
    return FALSE;
}

nboolean generic_query_have_query_caller(GenericQuery *self,ClassFunc *classFunc,tree callFunc)
{
    enum tree_code code=TREE_CODE(callFunc);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
    if(code==COMPONENT_REF){
        NPtrArray *refArray=n_ptr_array_new();
        refLink(callFunc,refArray);
        nboolean re=haveQueryCaller(self,refArray);
        n_ptr_array_unref(refArray);
    }
    return FALSE;
}


/**
 * 获取引用的类和类中的域
 * A->B->C->func;
 * 生成 C,B,A顺序
 */
static void getVar(tree func,NPtrArray *typeArray,NPtrArray *fieldArray)
{
    tree op0=TREE_OPERAND (func, 0);
    tree op1=TREE_OPERAND (func, 1);//域成员 函数名或变量名
    if(TREE_CODE(op0)==INDIRECT_REF){
       n_ptr_array_add(typeArray,op0);
       n_ptr_array_add(fieldArray,op1);
       tree component=TREE_OPERAND(op0,0);
       if(TREE_CODE(component)==COMPONENT_REF){
           getVar(component,typeArray,fieldArray);
       }
    }
}

/**
 * 获取调用者的泛型
 */
/**
 * A->B->getName
 * refArray中 0是getName 1是B 2是A
 */
static GenericModel *getGenModel(GenericQuery *self,NPtrArray *refArray)
{
          tree field=n_ptr_array_index(refArray,1);
          char fieldName[256];
          tree last=NULL_TREE;
          tree type=getFieldInfo(field,fieldName,&last);
          field=last;
          GenericModel *gen=NULL;
          if(type==NULL_TREE){
              aet_print_tree_skip_debug(field);
              n_error("在getGenModel有未处理的类型。请报告此错误。");
          }
          char *sysClassName=class_util_get_class_name(type);
          printf("getGenModel ---00 sysClassName:%s fieldName:%s\n",sysClassName,fieldName);
          if(sysClassName!=NULL){
              ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
              if(class_info_is_generic_class(info)){
                  gen=c_aet_get_generics_model(field);
                  if(!gen){
                      if(isSelf(self,field)){
                           gen=info->genericModel;
                      }
                  }
              }
          }
          return gen;
}


/**
 * 获取调用者的泛型
 */
GenericModel *generic_query_get_call_generic(GenericQuery *self,tree callFunc)
{
    enum tree_code code=TREE_CODE(callFunc);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
    GenericModel *gen=NULL;
    if(code==COMPONENT_REF){
        NPtrArray *refArray=n_ptr_array_new();
        refLink(callFunc,refArray);
        gen=getGenModel(self,refArray);
    }
    return gen;
}

/**
 * A->B->getName
 * refArray中 0是getName 1是B 2是A
 */
static nboolean isGenericClass(GenericQuery *self,NPtrArray *refArray)
{
          tree field=n_ptr_array_index(refArray,1);
          char fieldName[256];
          tree last=NULL_TREE;
          tree type=getFieldInfo(field,fieldName,&last);
          field=last;
          if(type==NULL_TREE){
              aet_print_tree_skip_debug(field);
              n_error("在isGenericClass有未处理的类型。请报告此错误。");
          }
          char *sysClassName=class_util_get_class_name(type);
          printf("getGenModel ---00 sysClassName:%s fieldName:%s\n",sysClassName,fieldName);
          if(sysClassName!=NULL){
              ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
              if(class_info_is_generic_class(info)){
                return TRUE;
              }
          }
          return FALSE;
}

/**
 * 判断调用者是不是一个泛型类
 * A->B->getName
 * refArray中 0是getName 1是B 2是A
 */
nboolean generic_query_is_generic_class(GenericQuery *self,tree callFunc)
{
    enum tree_code code=TREE_CODE(callFunc);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
    GenericModel *gen=NULL;
    if(code==COMPONENT_REF){
        NPtrArray *refArray=n_ptr_array_new();
        refLink(callFunc,refArray);
        return isGenericClass(self,refArray);
    }
    return FALSE;
}


static struct c_declarator *getFunId(struct c_declarator *funcdel)
{
    struct c_declarator *funid=funcdel->declarator;
    if(funid==NULL)
        return NULL;
    enum c_declarator_kind kind=funid->kind;
    if(kind==cdk_id)
        return funid;
    if(kind==cdk_pointer){
        funid=funid->declarator;
        if(funid==NULL)
            return NULL;
        kind=funid->kind;
        if(kind!=cdk_id)
            return NULL;
        return funid;
    }
    return NULL;
}


/////////////////////////以下是加参数到问号泛型函数--------------------------------------
///* setData(Abc<?> *abc) ---->setData(Efg *self,FuncGenParmInfo  tempFgpi1234,Abc<?> *abc);
static tree createQueryParmType()
{
     tree queryRecordId=aet_utils_create_ident(AET_FUNC_GENERIC_PARM_STRUCT_NAME);
     tree record=lookup_name(queryRecordId);
     if(!record || record==error_mark_node){
         return NULL_TREE;
     }
     tree recordType=TREE_TYPE(record);
     return recordType;
}

static tree createQueryParmDecl()
{
     tree recordType=createQueryParmType();
     if(!recordType || recordType==error_mark_node){
         return NULL_TREE;
     }
     tree id=aet_utils_create_ident(AET_FUNC_GENERIC_PARM_NAME);
     tree decl = build_decl (input_location,PARM_DECL,id, recordType);
     return decl;
}


static nboolean have_query_by_declarator(struct c_declarator *declarator)
{
    struct c_declarator *funcdecl=class_util_get_function_declarator(declarator);
    if(funcdecl==NULL){
        n_error("不应该出现的错误，have_query_by_declarator");
        return FALSE;
    }
    struct c_arg_info *argInfo=funcdecl->u.arg_info;
    tree args=argInfo->parms;
    tree parm=NULL_TREE;
    int count=0;
    nboolean find=FALSE;
    for (parm = args; parm; parm = DECL_CHAIN (parm)){
        //printf("have_query_by_declarator ----\n");
        //aet_print_tree(parm);
        GenericModel *gen=c_aet_get_generics_model(parm);
        if(gen && count>0){ //跳过self
            //printf("找到带有问号作为参数的函数了:%d\n",count);
            //aet_print_tree(parm);
            //aet_print_tree(gen);
            find=generic_model_have_query(gen);
            if(find)
              break;
        }
        count++;
    }
    return find;
}



nboolean generic_query_is_function_declarator(GenericQuery *self,ClassName *className,struct c_declarator *declarator)
{
    struct c_declarator *funcdecl=class_util_get_function_declarator(declarator);
    if(funcdecl==NULL)
        return FALSE;
    struct c_declarator *funid=getFunId(funcdecl);
    if(funid==NULL)
        return FALSE;
    tree argTypes = funcdecl->u.arg_info->types;
    tree funName=funid->u.id.id;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(!class_info_is_generic_class(info))
        return FALSE;
    nboolean ret=have_query_by_declarator(funcdecl);
    return ret;
}

/**
 * 加入参数到函数中，该函数中包含有问号泛型的参数
 * setData(Abc<?> *abc) ---->setData(Efg *self,FuncGenParmInfo *tempFgpi1234,Abc<?> *abc);
 * 为什么是指针呢？因为可以传空的参数
 */
void generic_query_add_parm_to_declarator(GenericQuery *self,struct c_declarator *declarator)
{
    struct c_declarator *funcdecl=class_util_get_function_declarator(declarator);
    if(funcdecl==NULL){
        n_error("不应该出现的错误，generic_query_add_parm_to_declarator");
        return ;
    }
    struct c_declarator *funid=getFunId(funcdecl);
    if(funid==NULL){
        n_error("不应该出现的错误，generic_query_add_parm_to_declarator");
        return ;
    }
    tree funName=funid->u.id.id;
    struct c_arg_info *argsInfo=funcdecl->u.arg_info;
    tree args=argsInfo->parms;
    tree parm=NULL_TREE;
//  int count=0;
//  for (parm = args; parm; parm = DECL_CHAIN (parm)){
//      printf("generic_query_add_parm_to_declarator argsInfo->parms 00 现有参数:%d\n",count);
//      aet_print_tree(parm);
//      count++;
//  }
    tree parms;
    parms = DECL_CHAIN (argsInfo->parms);
    tree parmdecl=createQueryParmDecl();
    if(!aet_utils_valid_tree(parmdecl)){
         error_at(input_location,"找不到%qs。检查generic.h是否定义了结构体。",AET_FUNC_GENERIC_PARM_STRUCT_NAME);
         return ;
    }
    /**
     * 如果不加这句DECL_ARG_TYPE (parmdecl) = TREE_TYPE(parmdecl);把以下错误
     * 编译器内部错误：在 adjust_one_expanded_partition_var 中，于 cfgexpand.c:1463
   11 |     void setDataxx(FuncGenParmInfo wwyy,AObject<?> *tt){
      |          ^~~~~~~~~
0x5dfdd1 adjust_one_expanded_partition_var
    ../../gcc/cfgexpand.c:1463
0x5dfdd1 execute
    ../../gcc/cfgexpand.c:6458
       *请提交一份完整的错误报告，
      *如有可能请附上经预处理后的源文件。
     */
    DECL_ARG_TYPE (parmdecl) = TREE_TYPE(parmdecl);
    /* 在self和第二个参数之间插入aet_generic_info *aetGenericInfo1234 */
    DECL_CHAIN (parmdecl) = parms;
    parms = parmdecl;
    DECL_CHAIN (argsInfo->parms) = parms;
    args=argsInfo->parms;
    parm=NULL_TREE;
//  count=0;
//  for (parm = args; parm; parm = DECL_CHAIN (parm)){
//      printf("generic_query_add_parm_to_declarator argsInfo->parms 11 现有参数:%d\n",count);
//      aet_print_tree(parm);
//      count++;
//  }

    n_debug("c_declarator的c_arg_info中的types加入QueryCallRecord类型 %s\n",IDENTIFIER_POINTER(funName));

        parms = TREE_CHAIN (argsInfo->types);
        tree genericInfoType=createQueryParmType();
        tree newType = make_node (TREE_LIST);
        TREE_VALUE (newType) = genericInfoType;
        TREE_CHAIN (newType) = parms;
        parms = newType;
        TREE_CHAIN (argsInfo->types) = parms;
        args=argsInfo->types;
//      count=0;
//      for (tree al = args; al; al = TREE_CHAIN (al)){
//          tree type=TREE_VALUE(al);
//          printf("generic_query_add_parm_to_declarator argsInfo->parms 22 现有参数:%d\n",count);
//          aet_print_tree(type);
//          count++;
//      }
}

static tree getCaller(tree fn)
{
    enum tree_code code=TREE_CODE(fn);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    if(code==COMPONENT_REF){
        NPtrArray *refArray=n_ptr_array_new();
        refLink(fn,refArray);
        tree field=n_ptr_array_index(refArray,1);//0 getName 1 A
        fn=field;
        n_ptr_array_unref(refArray);
    }else if(code==ADDR_EXPR){
        tree op0=TREE_OPERAND (fn, 0);
        if(TREE_CODE(op0)==FUNCTION_DECL){
           char *functionName=IDENTIFIER_POINTER(DECL_NAME(op0));
           ClassFunc *func=func_mgr_get_entity_by_mangle(func_mgr_get(),functionName);
           if(func){
               char *sysName= func_mgr_get_class_name_by_mangle(func_mgr_get(),functionName);
               fn=DECL_ARGUMENTS (op0);
               n_debug("--getcaller--- %s\n",sysName);
           }
        }
    }
    return fn;
}

/**
 * 左右值的泛型单元相同，检查调用者是不是问号泛型
 */
static nboolean checkSame(tree initOrRhs,tree decl,GenericUnit *lhsUnit,char *errorInfo,int from)
{
   if(TREE_CODE(initOrRhs)==CALL_EXPR){
       //取变量
       tree fn=CALL_EXPR_FN (initOrRhs);
       tree caller=getCaller(fn);
       GenericModel *trueModel=c_aet_get_generics_model(caller);
       char *lhsSysName=class_util_get_class_name(TREE_TYPE(decl));
       if(!generic_model_have_query(trueModel))
           return TRUE;
       if(!generic_unit_is_undefine(lhsUnit))
          return TRUE;
       if(generic_unit_is_query(lhsUnit))
          return TRUE;
       char *sysName=class_util_get_class_name(TREE_TYPE(caller));
       char *varOrParmName=IDENTIFIER_POINTER(DECL_NAME(caller));
       if(sysName==NULL){
           sprintf(errorInfo,"checkSame 失败!原因:不是aet class。");
           return FALSE;
       }
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
       if(info==NULL || !class_info_is_generic_class(info)){
           sprintf(errorInfo,"checkSame 失败!原因:%s不是aet类、或不是泛型类:%s %p\n",sysName,info);
           return FALSE;
       }
       GenericModel *classGenDecl=info->genericModel;
       if(!generic_model_exits_unit(classGenDecl,lhsUnit)){
           sprintf(errorInfo,"checkSame 失败!原因:类%s的原始声明的泛型%s中，并没有泛型单元:%s\n",sysName,generic_model_tostring(classGenDecl),lhsUnit->name);
           return FALSE;
       }
       int index=generic_model_get_index_by_unit(classGenDecl,lhsUnit);
       GenericUnit *unit=generic_model_get(trueModel,index);
       if(generic_unit_is_query(unit) && from!=1){
           sprintf(errorInfo,"checkSame 失败!原因:不能转化%s<?>到类%s<%s>。",sysName,lhsSysName,lhsUnit->name);
           return FALSE;
       }
       return TRUE;
   }else if(TREE_CODE(initOrRhs)==VAR_DECL || TREE_CODE(initOrRhs)==PARM_DECL){
       return TRUE;
   }else if(TREE_CODE(initOrRhs)==NON_LVALUE_EXPR){
        return TRUE;
   }else{
       n_error("类型%s没有处理，在函数checkAgain中。\n",get_tree_code_name(TREE_CODE(initOrRhs)));
   }
   return FALSE;
}

static GenericModel *getClassGenDecl(tree caller)
{
    char *sysName=class_util_get_class_name(TREE_TYPE(caller));
    if(sysName==NULL){
       return NULL;
    }
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    if(info==NULL ){
       return NULL;
    }
    return info->genericModel;
}

static GenericModel *getClassGenDefine(tree caller)
{
   GenericModel *trueModel=c_aet_get_generics_model(caller);
   return trueModel;
}

/**
 * 调用泛型函数时定义的泛型存在
 */
static GenericModel *getGenericFuncDefine(tree initOrRhs)
{
    enum tree_code code=TREE_CODE(initOrRhs);
    //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
    //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
    if(code==COMPONENT_REF){
        tree op0=TREE_OPERAND (initOrRhs, 0);
        if(TREE_CODE(op0)==INDIRECT_REF || TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL ){
            GenericModel *model=c_aet_get_func_generics_model(initOrRhs);
            //printf("getGenericFuncDefine --- %s\n",generic_model_tostring(model));
            return model;

        }else{
            return getGenericFuncDefine(op0);
        }
    }
    return NULL;
}

static GenericModel *getGenericFuncDecl(char *fieldName)
{
       ClassFunc *func=func_mgr_get_entity_by_mangle(func_mgr_get(),fieldName);
       if(func && class_func_is_func_generic(func)){
          return class_func_get_func_generic(func);
       }
       return NULL;
}

static nboolean findDefineAtClassOrGenericFunc(GenericModel *model,GenericUnit *unit,int index)
{
    if(model==NULL)
        return FALSE;
    if(generic_model_get_count(model)<index+1)
        return FALSE;
    GenericUnit *unit0=generic_model_get(model,index);
    return generic_unit_equal(unit0,unit);
}

static int findDeclAtClassOrGenericFunc(GenericModel *model,GenericUnit *dest)
{
    if(model==NULL)
        return -1;
    int i;
    for(i=0;i<generic_model_get_count(model);i++){
        GenericUnit *unit=generic_model_get(model,i);
        if(generic_unit_equal(unit,dest)){
            return i;
        }
    }
    return -1;
}

/**
 * from 来自泛型函数还是泛型类
 * 检查声明的返回值。如果没有错,再与实际的返回值比较。
 */
static nboolean checkRhsModel(tree initOrRhs,char *fieldName,GenericModel *rhsModel,char *errorInfo,int *from,nboolean isCast)
{
   if(TREE_CODE(initOrRhs)==CALL_EXPR){
       //取变量
       tree fn=CALL_EXPR_FN (initOrRhs);
       tree caller=getCaller(fn);
       GenericModel *classGenDecl=getClassGenDecl(caller);//调用函数的变量所属类中的泛型声明
       GenericModel *classGenDefine=getClassGenDefine(caller);//调用函数的变量定义的泛型类型
       GenericModel *funcGenDecl=getGenericFuncDecl(fieldName);
       GenericModel *funcGenDefine=getGenericFuncDefine(fn);
       n_debug("4个泛型:class:%s %s genfunc:%s %s\n",generic_model_tostring(classGenDecl),generic_model_tostring(classGenDefine),
               generic_model_tostring(funcGenDecl),generic_model_tostring(funcGenDefine));
       if(classGenDecl && classGenDefine==NULL)
           classGenDefine=classGenDecl;
       int i;
       for(i=0;i<generic_model_get_count(rhsModel);i++){
           GenericUnit *rhsUnit=generic_model_get(rhsModel,i);
           n_debug("泛型单元是具体类型 index:%d :%s 指针:%d\n",i,rhsUnit->name,rhsUnit->pointerCount);
           if(!generic_unit_is_undefine(rhsUnit) && !generic_unit_is_query(rhsUnit)){
               nboolean atFuncGen=findDefineAtClassOrGenericFunc(funcGenDefine,rhsUnit,i);
               nboolean atClassGen=findDefineAtClassOrGenericFunc(classGenDefine,rhsUnit,i);
               n_debug("右边泛型单元是具体类型 index:%d:%s 指针:%d atFuncGen:%d atClassGen:%d\n",
                       i,rhsUnit->name,rhsUnit->pointerCount,atFuncGen,atClassGen);
               if(!atFuncGen && !atClassGen){
                   if(funcGenDefine && classGenDefine){
                       printf("在类和泛型函数中没有定义泛型单元:%s %d\n",rhsUnit->name,rhsUnit->pointerCount);
                   }else if(funcGenDefine && !classGenDefine){
                       printf("在泛型函数中没有定义泛型单元:%s %d\n",rhsUnit->name,rhsUnit->pointerCount);
                   }else if(!funcGenDefine && classGenDefine){
                       printf("在类中没有定义泛型单元:%s %d\n",rhsUnit->name,rhsUnit->pointerCount);
                   }else{
                       printf("不是泛型类也不是泛型函数:%s %d\n",rhsUnit->name,rhsUnit->pointerCount);
                   }
                   if(isCast){
                       *from=1;
                       return TRUE;
                   }
                   return FALSE;
               }
               if(atFuncGen)
                  *from=1;
               else if(atClassGen)
                  *from=2;
           }else if(generic_unit_is_undefine(rhsUnit) && !generic_unit_is_query(rhsUnit)){
               int atFuncIndex= findDeclAtClassOrGenericFunc(funcGenDecl,rhsUnit);
               int atClassIndex= findDeclAtClassOrGenericFunc(classGenDecl,rhsUnit);
               //printf("checkRhsModel 00 %d %d\n",atFuncIndex,atClassIndex);
               if(atFuncIndex>=0){
                   GenericUnit *funUnit=generic_model_get(funcGenDefine,atFuncIndex);
                   //printf("checkRhsModel 11 %d %d\n",atFuncIndex,atClassIndex);
//                 if(!generic_unit_is_undefine(funUnit)){
//                     printf("泛型函数不能转化<%s>到<%s>\n",rhsUnit->name,funUnit->name);
//                     return FALSE;
//                 }
                   //printf("checkRhsModel 22 %d %d %s %s\n",atFuncIndex,atClassIndex,rhsUnit->name,funUnit->name);
                   if(!generic_unit_equal(funUnit,rhsUnit)){
                       n_debug("用泛型函数中的定义替换<%s>为<%s>\n",rhsUnit->name,funUnit->name);
                      n_free(rhsUnit->name);
                      rhsUnit->name=n_strdup(funUnit->name);
                      rhsUnit->isDefine=funUnit->isDefine;
                      rhsUnit->pointerCount=funUnit->pointerCount;
                   }
                  *from=1;
               }else if(atClassIndex>=0){
                   GenericUnit *classUnit=generic_model_get(classGenDefine,atClassIndex);
                   //printf("checkRhsModel 33 %d %d %s\n",atFuncIndex,atClassIndex,classUnit->name);
                   if(!generic_unit_is_undefine(classUnit) && !generic_unit_is_query(classUnit)){
                       //classDefine是一个具体类型，如果右值是一个声明，并且不是问号泛型可以替换。
                       if(generic_unit_is_undefine(rhsUnit)){
                           n_free(rhsUnit->name);
                           rhsUnit->name=n_strdup(classUnit->name);
                           rhsUnit->pointerCount=classUnit->pointerCount;
                           rhsUnit->isDefine=TRUE;
                       }else{
                           n_debug("类不能转化<%s>到<%s>\n",rhsUnit->name,classUnit->name);
                           return FALSE;
                       }
                   }else{
                       if(!generic_unit_equal(classUnit,rhsUnit)){
                           n_debug("用对象中的定义替换<%s>为<%s>",rhsUnit->name,classUnit->name);
                          n_free(rhsUnit->name);
                          rhsUnit->name=n_strdup(classUnit->name);
                       }
                   }
                 *from=2;
               }else{
                   n_debug("在类和泛型函数中没有定义泛型单元:%s\n",rhsUnit->name);
                   return FALSE;
               }

           }else if(generic_unit_is_query(rhsUnit)){
               GenericUnit *classUnit=generic_model_get(classGenDefine,i);
               if(!classUnit){
                     n_debug("类中没有定义泛型:%s\n",rhsUnit->name);
                     return FALSE;
               }
               printf("genericunit --generic_unit_is_query(rhsUnit)-- %s\n",classUnit->name);
           }
       }
       return TRUE;
   }else if(TREE_CODE(initOrRhs)==VAR_DECL || TREE_CODE(initOrRhs)==PARM_DECL){
       char *sysName=class_util_get_class_name(TREE_TYPE(initOrRhs));
       char *varName=IDENTIFIER_POINTER(DECL_NAME(initOrRhs));
       if(sysName==NULL){
           printf("在变量或参数的类型不是aet class。 %s\n",varName);
           return FALSE;
       }
       return TRUE;
   }else if(TREE_CODE(initOrRhs)==COMPONENT_REF){
       return TRUE;
   }else if(TREE_CODE(initOrRhs)==NON_LVALUE_EXPR){
       return TRUE;
   }else{
       n_error("类型%s没有处理，在函数checkRhsModel。\n",get_tree_code_name(TREE_CODE(initOrRhs)));
   }
   return FALSE;
}

/**
 * 检查形式
 * Abc<?> *abc=getData();
 * getData返回的值必须是?
 *  Abc<E> *abc=getData();getData返回值只能是E和?
 */
void  generic_query_check_var_and_parm(GenericQuery *self,tree decl,tree initOrRhs)
{
   if(TREE_CODE(decl)==VAR_DECL || TREE_CODE(decl)==PARM_DECL){

       GenericModel *lhsModel=c_aet_get_generics_model(decl);
       if(!lhsModel)//变量或参数不是泛型，返回。
           return;
       char *name=IDENTIFIER_POINTER(DECL_NAME(decl));
       if(!aet_utils_valid_tree(initOrRhs))
           return;

       char fieldName[256];
       tree last=NULL_TREE;
       tree type=getFieldInfo(initOrRhs,fieldName,&last);
       n_debug("generic_query_check_var_and_parm 00 变量:%s的泛型声明是:%s initOrRhs:%s type:%p\n",
                     name,generic_model_tostring(lhsModel),get_tree_code_name(TREE_CODE(initOrRhs)),type);
       if(!aet_utils_valid_tree(type))
          return;
       location_t loc=input_location;
       if(EXPR_P(initOrRhs))
           loc=EXPR_LOCATION(initOrRhs);
       if(DECL_P(initOrRhs))
           loc=DECL_SOURCE_LOCATION(initOrRhs);

       GenericModel *rhsModel=c_aet_get_generics_model(last);//声明的返回值泛型
       n_debug("generic_query_check_var_and_parm 11 变量:%s的泛型声明是:%s initOrRhs:%s rhsModel:%p\n",
                         name,generic_model_tostring(lhsModel),get_tree_code_name(TREE_CODE(initOrRhs)),rhsModel);
       if(rhsModel==NULL){
           if(TREE_CODE(last)==VAR_DECL || TREE_CODE(last)==PARM_DECL){
               char *sysName=class_util_get_class_name(TREE_TYPE(last));
               ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
               if(info!=NULL){
                   rhsModel=info->genericModel;
               }
           }
           if(rhsModel==NULL){
              error_at(loc,"变量%qs定义的泛型是%qs,但右值没有泛型。",name,generic_model_tostring(lhsModel));
              return;
           }
       }
       if(generic_model_get_count(lhsModel)!=generic_model_get_count(rhsModel)){
           error_at(loc,"变量%qs定义的泛型是%qs,但右值的泛型是:%qs。泛型单元数量不等。",name,generic_model_tostring(lhsModel),generic_model_tostring(rhsModel));
           return;
       }
       n_debug("generic_query_check_var_and_parm 11 变量:%s的右值泛型声明是:%s fieldName:%s\n",name,generic_model_tostring(rhsModel),fieldName);
       //检查有没有强转的泛型
       GenericModel *castModel=generic_impl_get_cast_model(generic_impl_get(),initOrRhs);
       nboolean isCast=FALSE;
       if(castModel!=NULL){
           n_debug("generic_query_check_var_and_parm 22 变量:%s的右值泛型声明是(来自强转的):%s fieldName:%s\n",name,generic_model_tostring(castModel),fieldName);
           if(!generic_model_can_cast(rhsModel,castModel)){
               error_at(loc,"变量%qs不能强转泛型%qs %qs",name,generic_model_tostring(rhsModel),generic_model_tostring(castModel));
               return ;
           }
           rhsModel=castModel;
           if(generic_model_get_count(lhsModel)!=generic_model_get_count(rhsModel)){
               error_at(loc,"强转后的泛型，变量%qs定义的泛型是%qs,但右值的泛型是:%qs。泛型单元数量不等。",
                       name,generic_model_tostring(lhsModel),generic_model_tostring(rhsModel));
               return;
           }
           isCast=TRUE;
       }
       rhsModel=generic_model_clone(rhsModel);
       //声明的泛型，应该被类中定义的泛型替换
       //声明的泛型，应该被泛型函数中的泛型替换。
       char errorInfo[1024];
       int from=0;
       if(!checkRhsModel(initOrRhs,fieldName,rhsModel,errorInfo,&from,isCast)){
           error_at(loc,"变量%qs定义的泛型是%qs,但右值的泛型是:%qs。",name,generic_model_tostring(lhsModel),generic_model_tostring(rhsModel));
           return;
       }
       n_debug("generic_query_check_var_and_parm  33 %s\n",generic_model_tostring(lhsModel));
       int i;
       for(i=0;i<generic_model_get_count(lhsModel);i++){
           GenericUnit *lhsUnit=generic_model_get(lhsModel,i);
           GenericUnit *rhsUnit=generic_model_get(rhsModel,i);
           if(!generic_unit_equal(lhsUnit,rhsUnit)){
               nboolean q1=generic_unit_is_query(lhsUnit);
               nboolean q2=generic_unit_is_query(rhsUnit); //如果是问号泛型，报错。返回值是问号泛型的只能赋值给同是问号泛型的变量或参数
               if(!q1 && q2){
                   error_at(loc,"不能从<?>转化到变量%qs<%qs>。",name,lhsUnit->name);
                   return;
               }else if(q1 && !q2){
                   continue;
               }else if(!q1 && !q2){
                   error_at(loc,"不能从<%qs>转到变量%qs<%qs>。!!!",rhsUnit->name,name,lhsUnit->name);
               }
           }else{
              char errorInfo[1024];
              //printf("tostring---- %s %s\n",generic_unit_tostring(lhsUnit),generic_unit_tostring(rhsUnit));
              if(!checkSame(initOrRhs,decl,lhsUnit,errorInfo,from)){
                   error_at(loc,"%qs",errorInfo);
                   n_error("generic_query_check_var_and_parm checkSame----%s",errorInfo);
              }
           }
       }
       generic_model_free(rhsModel);
   }else{
       ;
   }
}


void generic_query_set_parser(GenericQuery *self,c_parser *parser)
{
   self->parser=parser;
}

GenericQuery *generic_query_get()
{
    static GenericQuery *singleton = NULL;
    if (!singleton){
         singleton =n_slice_alloc0 (sizeof(GenericQuery));
         genericQueryInit(singleton);
    }
    return singleton;
}


