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
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "namelesscall.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "classutil.h"

static void namelessCallInit(NamelessCall *self)
{

}

/**
 * 调用的是不是一个类中方法
 */
static nboolean isFieldDecl(tree op)
{
    if(TREE_CODE(op)==FIELD_DECL){
          tree type=TREE_TYPE(op);
          if(TREE_CODE(type)==POINTER_TYPE){
              type=TREE_TYPE(type);
              if(TREE_CODE(type)==FUNCTION_TYPE)
                  return TRUE;
          }
     }
     return FALSE;
}

static tree createVarDecl(tree varType,char *name,location_t loc)
{
    tree id=aet_utils_create_ident(name);
    tree varDecl=build_decl (loc, VAR_DECL, id, varType);
    //DECL_INITIAL(varDecl)=null_pointer_node;
    DECL_CONTEXT(varDecl)=current_function_decl;
    TREE_USED (varDecl)=1;
    return varDecl;
}



typedef struct _NamelessBuffer{
    tree modifys[20];
    int count;
    tree fields[20];
    int fieldsCount;
}NamelessBuffer;

static tree getType(char *sysName,nboolean  pointer)
{
   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
   if(info==NULL){
       n_error("在namelesscall.c 找不到类%s的classInfo",sysName);
   }
   return pointer?build_pointer_type(TREE_TYPE(info->recordTypeDecl)):TREE_TYPE(info->recordTypeDecl);

}
/**
 * 把varDecl转成能引用fieldDec的类或接口
 */
static tree convertDest(location_t loc,tree varDecl,tree fieldDecl)
{
    tree varType=TREE_TYPE(varDecl);
    nboolean isPointer=TREE_CODE(varType)==POINTER_TYPE;
    char *sysName=class_util_get_class_name(varType);
    char *fieldSysName=class_mgr_find_field(class_mgr_get(),sysName,IDENTIFIER_POINTER(DECL_NAME(fieldDecl)));
   // printf("convertDest is :%s %s\n",IDENTIFIER_POINTER(DECL_NAME(fieldDecl)),fieldSysName);
    if(!fieldSysName){
       //如果是变量
//       tree type=TREE_TYPE(fieldDecl);
//       fieldSysName=class_util_get_class_name(type);
       if(!fieldSysName){
          aet_print_tree_skip_debug(fieldDecl);
          n_error("未知错误：请报告。");
       }
    }
    ClassRelationship ship=class_mgr_relationship(class_mgr_get(),sysName,fieldSysName);
    //printf("relation ship---- %d isPointer:%d %s %s fieldName:%s\n",ship,isPointer,sysName,fieldSysName,IDENTIFIER_POINTER(DECL_NAME(fieldDecl)));
    if(ship==CLASS_RELATIONSHIP_CHILD){//把varDecl转成fieldSysName
       //转成父类
       if(isPointer){
           tree parentType=  getType(fieldSysName,TRUE);
           varDecl = build1 (NOP_EXPR, parentType, varDecl);
       }else{
           tree add=build_unary_op (loc, ADDR_EXPR, varDecl, FALSE);
           tree parentType=  getType(fieldSysName,TRUE);
           varDecl = build1 (NOP_EXPR, parentType, add);
       }
    }else if(ship==CLASS_RELATIONSHIP_IMPL){
        ClassName *at=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
        ClassName *iface=class_mgr_get_class_name_by_sys(class_mgr_get(),fieldSysName);
        ClassName *belong=class_mgr_find_interface(class_mgr_get(),at,iface);
        if(strcmp(at->sysName,belong->sysName)){
                  //转成父类
//           if(isPointer){
//              tree parentType=  getType(belong->sysName,TRUE);
//              tree value = build1 (NOP_EXPR, parentType, varDecl);
//              tree datum=build_indirect_ref (loc,value,RO_ARROW);
//              tree firstRef= build_component_ref (loc, datum,DECL_NAME(ifaceVarInClass), loc);
//           }else{
//              tree add=build_unary_op (loc, ADDR_EXPR, varDecl, FALSE);
//              tree parentType=  getType(belong->sysName,TRUE);
//              varDecl = build1 (NOP_EXPR, parentType, add);
//           }
        }


    }
    return varDecl;
}

/**
 * 生成self参数
 */
static tree getSelfParm(tree varDecl,location_t loc)
{
           tree varType=TREE_TYPE(varDecl);
           nboolean isPointer=TREE_CODE(varType)==POINTER_TYPE;
           char *sysName=class_util_get_class_name(varType);
           ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
           tree firstParms=NULL_TREE;
           if(class_info_is_interface(info)){
               printf("createNewCall00 是一个接口 恢复实现它的类作为self参数\n");
               if(TREE_CODE(varDecl)==COMPONENT_REF){
                   tree indirect=TREE_OPERAND (varDecl, 0);
                   tree indirectType=TREE_TYPE(varDecl);
                   nboolean pointer=TREE_CODE(indirectType)==POINTER_TYPE;
                   firstParms=pointer?indirect:build_unary_op (loc, ADDR_EXPR, indirect, FALSE);
                   return firstParms;
               }
               if(isPointer){
                   tree datum=build_indirect_ref (loc,varDecl,RO_ARROW);
                   tree id=aet_utils_create_ident(IFACE_COMMON_STRUCT_VAR_NAME);//
                   tree ref= build_component_ref (loc, datum,id, loc);
                   id=aet_utils_create_ident(IFACE_AT_CLASS);//
                   firstParms= build_component_ref (loc, ref,id, loc);
               }else{
                   tree id=aet_utils_create_ident(IFACE_COMMON_STRUCT_VAR_NAME);//
                   tree ref= build_component_ref (loc, varDecl,id, loc);
                   id=aet_utils_create_ident(IFACE_AT_CLASS);//
                   firstParms= build_component_ref (loc, ref,id, loc);
               }
           }else{
               firstParms=isPointer?varDecl:build_unary_op (loc, ADDR_EXPR, varDecl, FALSE);
           }
           return firstParms;
}

static tree createNewCall00(tree call,location_t loc,tree varDecl,tree fieldDecl)
{
        varDecl=convertDest(loc,varDecl,fieldDecl);
        tree varType=TREE_TYPE(varDecl);
        nboolean isPointer=TREE_CODE(varType)==POINTER_TYPE;
        tree firstParms=getSelfParm(varDecl,loc);
        if(!aet_utils_valid_tree(firstParms)){
            n_error("不能转换出第一个参数self\n");
        }
        tree type=TREE_TYPE(call);//返回值类型
        tree fndecl;
        int nargs=call_expr_nargs (call);
        tree args[nargs];
        int i;
        for(i=0;i<nargs;i++){
            if(i==0)
                args[i]=firstParms;
            else
                args[i]=CALL_EXPR_ARG (call, i);
        }
        //printf("createNewCall00------isPointer:%d\n",isPointer);
        if(isPointer){
            tree datum=build_indirect_ref (loc,varDecl,RO_ARROW);
            fndecl= build_component_ref (loc, datum,DECL_NAME(fieldDecl), loc);
        }else{
            fndecl = build_component_ref (loc, varDecl,DECL_NAME(fieldDecl), loc);
        }
        tree newCall=build_call_array_loc (loc, type,fndecl, nargs, args);
        return newCall;
}


static tree addDebug(tree modify,location_t loc)
{
        tree varDecl=TREE_OPERAND (modify, 0);
        tree type=TREE_TYPE(varDecl);
        if(TREE_CODE(type)!=POINTER_TYPE)
            varDecl = build_unary_op (loc, ADDR_EXPR, varDecl, FALSE);

        tree args[1];
        args[0]=varDecl;
        tree fndecl=lookup_name(aet_utils_create_ident("printgen"));
        tree add = build_unary_op (loc, ADDR_EXPR, fndecl, FALSE);
        tree newCall=build_call_array_loc (loc, void_type_node,add, 1, args);
        return newCall;
}


/////////////////////////------------------------------------------------
static tree createModify_new(tree call,int index,location_t loc)
{
    tree type=TREE_TYPE(call);
    char name[255];
    sprintf(name,"tempVarName_%d",index);
    tree varDecl= createVarDecl(type,name,loc);
    tree modifyVarDecl= build2 (MODIFY_EXPR, TREE_TYPE(varDecl), varDecl, call);
    n_debug("nameless_call_parser createModify ---- index:%d\n",index);
    return modifyVarDecl;
}

static int isAetCall(tree call)
{
    tree type=NULL_TREE;
    nboolean isFunc=FALSE;
    int result=0;
    if(TREE_CODE(call)==FUNCTION_DECL){
        type=TREE_TYPE(call);
        type=TREE_TYPE(type);//函数返回值
        isFunc=TRUE;
    }else if(TREE_CODE(call)==COMPONENT_REF){
        tree field=TREE_OPERAND (call, 1);
        type=TREE_TYPE(field);
        if(TREE_CODE(type)==POINTER_TYPE){
            type=TREE_TYPE(type);//函数返回值
            if(TREE_CODE(type)==FUNCTION_TYPE){
                type=TREE_TYPE(type);//函数返回值
                isFunc=TRUE;
            }
        }
    }
    char *sysName=class_util_get_class_name(type);
    nboolean isPointer=TREE_CODE(type)==POINTER_TYPE;
    if(sysName!=NULL && isFunc){
       result=isPointer?1:2;
    }
    return result;
}


static int generatorCallLink(tree value,NPtrArray *callLink)
{
        if(TREE_CODE(value)==FUNCTION_DECL){
            n_ptr_array_add(callLink,value);
        }else if(TREE_CODE(value)==CALL_EXPR){
            tree fn= CALL_EXPR_FN (value);
            n_ptr_array_add(callLink,value);
            return generatorCallLink(fn,callLink);
        }else if(TREE_CODE(value)==COMPONENT_REF){
            tree ref=TREE_OPERAND (value, 0);
            n_ptr_array_add(callLink,value);
            return generatorCallLink(ref,callLink);
        }else{
            int length = TREE_OPERAND_LENGTH (value);
            if(length>0){
                tree ref=TREE_OPERAND (value, 0);
                n_ptr_array_add(callLink,value);
                return generatorCallLink(ref,callLink);
            }
        }
        return 0;
}

/**
 * 通过function_decl找到调用者即call_expr
 */
static tree findCall(tree call,tree func,enum tree_code code)
{
   NPtrArray *array=n_ptr_array_new();
   generatorCallLink(call,array);
   int i;
   int pos=-1;
   for(i=0;i<array->len;i++){
       tree item=n_ptr_array_index(array,i);
       if(item==func){
           n_debug("namelesscall.c findCall ----00  pos %d\n",i);
           pos=i;
           break;
       }
   }
   for(i=pos;i>=0;i--){
         tree item=n_ptr_array_index(array,i);
         n_debug("namelesscall.c findCall ----11  pos %d\n",i);
         if(TREE_CODE(item)==code){
             n_ptr_array_unref(array);
             return item;
         }
   }
   n_ptr_array_unref(array);
   return NULL_TREE;
}

static tree createNewRef(location_t loc,tree varDecl,tree fieldDecl)
{
        varDecl=convertDest(loc,varDecl,fieldDecl);
        tree varType=TREE_TYPE(varDecl);
        nboolean isPointer=TREE_CODE(varType)==POINTER_TYPE;
        tree ref;
        if(isPointer){
            tree datum=build_indirect_ref (loc,varDecl,RO_ARROW);
            ref= build_component_ref (loc, datum,DECL_NAME(fieldDecl), loc);
        }else{
            ref = build_component_ref (loc, varDecl,DECL_NAME(fieldDecl), loc);
        }
        return ref;
}

/**
 * 处理component_ref分两种情况
 * 1.域是变量 2 域是函数
 */
static tree processComponentRef(tree call,tree varVal,tree item,location_t loc,int *result)
{
      tree fieldDecl=TREE_OPERAND (item, 1);
      //fieldDecl是变量
      if(class_util_is_function_field(fieldDecl)){ //是一个函数调用
          tree callExpr=findCall(call,item,CALL_EXPR);//找到调用这个域的call_expr,然后可以取到参数
         // printf("processComponentRef ----\n");
          tree newCall=createNewCall00(callExpr,loc,varVal,fieldDecl);
          *result=1;
          return newCall;
      }else{
         // printf("processComponentRef 引用域 :%s\n",IDENTIFIER_POINTER(DECL_NAME(fieldDecl)));
          tree ref= createNewRef(loc,varVal,fieldDecl);
          *result=2;
          return ref;
      }
}

/**
 * 创建变量声明
 */
static tree createVarDeclStmt(tree rvalue,NamelessBuffer *bufs,location_t loc,tree *stmtList)
{
    tree modifyVarDecl=createModify_new(rvalue, bufs->count,loc);
    tree stmt0 = build_stmt (loc, DECL_EXPR, void_type_node);
    append_to_statement_list_force (stmt0, stmtList);
    append_to_statement_list_force (modifyVarDecl, stmtList);
    tree decl=  TREE_OPERAND (modifyVarDecl, 0);
    //printf("createVarDeclStmt --- %d %d %d %d %d\n",VAR_P (decl),DECL_SEEN_IN_BIND_EXPR_P (decl),TREE_STATIC (decl),DECL_EXTERNAL (decl),
    //        decl_function_context (decl) == current_function_decl);
    tree debugFunc=addDebug(modifyVarDecl,loc);
    append_to_statement_list_force (debugFunc, stmtList);
    tree varVal=TREE_OPERAND (modifyVarDecl, 0);
    bufs->modifys[bufs->count]=modifyVarDecl;
    bufs->count++;
    return varVal;
}

static tree process(tree call)
{
    NPtrArray *array=n_ptr_array_new();
    char *link=NULL;
    class_util_get_nameless_call_link(call,array,&link);
    n_debug("process nameless 生成target 原来调用的链是:%s\n",link);
    location_t loc=EXPR_LOCATION (call);
    tree rtnType=TREE_TYPE(call);//返回值类型
    nboolean hasRtn=(TREE_CODE(rtnType)!=VOID_TYPE);//调用是否有返回值
    tree hideVarDecl=hasRtn?create_tmp_var_raw(rtnType):NULL_TREE;
    tree  stmtList = alloc_stmt_list ();

    NamelessBuffer bufs;
    bufs.count=0;
    bufs.fieldsCount=0;

    tree lastCall=NULL_TREE;
    tree prevVal=NULL_TREE;
    int i;
    for(i=array->len-1;i>=0;i--){
        tree item=n_ptr_array_index(array,i);
        int result=isAetCall(item);
        n_debug("生成target语句:isAetCall吗? %d i:%d array->len:%d\n",result,i,array->len);
        aet_print_tree(item);
        if(result>0){
            if(TREE_CODE(item)==FUNCTION_DECL){
                tree ref=findCall(call,item,CALL_EXPR);
                prevVal=createVarDeclStmt(ref,&bufs,loc,&stmtList);
            }else if(TREE_CODE(item)==COMPONENT_REF){
                if(!aet_utils_valid_tree(prevVal)){
                    tree ref=findCall(call,item,CALL_EXPR);
                    prevVal=createVarDeclStmt(ref,&bufs,loc,&stmtList);
                }else{
                //用prevVal调和field
                    int continueRef=0;
                    tree newCallOrNewRef=processComponentRef(call,prevVal,item,loc,&continueRef);
                    n_debug("引用 COMPONENT_REF 生成函数调用call_expr i:%d\n",i);
                    aet_print_tree(newCallOrNewRef);
                    if(i!=0){
                        if(continueRef==1)
                           prevVal=createVarDeclStmt(newCallOrNewRef,&bufs,loc,&stmtList);
                        else
                           prevVal=newCallOrNewRef;
                    }else{
                        lastCall=newCallOrNewRef;
                    }
                }
            }
        }else{
            if(TREE_CODE(item)==COMPONENT_REF){
                if(!aet_utils_valid_tree(prevVal)){
                    //printf("还未有初始变量----\n");
                     continue;
                }
                 int continueRef=0;
                 tree newCallOrNewRef=processComponentRef(call,prevVal,item,loc,&continueRef);
                 //printf("引用 COMPONENT_REF 但返回值不是类 call_expr i:%d\n",i);
                 if(i!=0){
                     if(continueRef==1){
                        n_error("还未处理此种情况。-----i:%d\n",i);
                     }else{
                        prevVal=newCallOrNewRef;
                     }
                 }else{
                     lastCall=newCallOrNewRef;
                 }
            }
        }
    }
    if(hasRtn){
        tree modifyExpr= build2 (MODIFY_EXPR, void_type_node, hideVarDecl, lastCall);
        append_to_statement_list_force (modifyExpr, &stmtList);
    }else{
        append_to_statement_list_force (lastCall, &stmtList);
    }

    tree bindExpr = build3 (BIND_EXPR, void_type_node, NULL_TREE, stmtList, NULL);
    for(i=bufs.count-1;i>=0;i--){
           tree modify=bufs.modifys[i];
           tree decl=TREE_OPERAND (modify, 0);
           DECL_CHAIN (decl) = BIND_EXPR_VARS (bindExpr);
           BIND_EXPR_VARS (bindExpr) = decl;
    }
    if(!hasRtn)
        return bindExpr;
    tree target = build4 (TARGET_EXPR, rtnType, hideVarDecl,bindExpr, NULL_TREE, NULL_TREE);
    return target;
}

tree nameless_call_parser(NamelessCall *self,tree call)
{
   c_parser *parser=self->parser;
   if(TREE_CODE(call)!=COMPONENT_REF && TREE_CODE(call)!=CALL_EXPR && TREE_CODE(call)!=NOP_EXPR)
       return call;
   c_token *token1=c_parser_peek_token (parser);
   c_token *token2=c_parser_peek_2nd_token (parser);
   /**
    * 匹配 int v2=   ((AObject*)ARandom.getInstance())->getooo();
    */
   if(token1->type==CPP_DOT || token1->type==CPP_DEREF || (token1->type==CPP_CLOSE_PAREN && (token2->type==CPP_DOT || token2->type==CPP_DEREF)) ){
       return call;
   }
   int hasNamelessCall= class_util_has_nameless_call(call);
   if(hasNamelessCall==0)
       return call;
   n_debug("nameless_call_parser --00-- %d\n",hasNamelessCall);
   tree result=process(call);
   n_debug("nameless_call_parser --11-- %d\n",hasNamelessCall);
   return result;
}


void  nameless_call_set_parser(NamelessCall *self,c_parser *parser)
{
	 self->parser=parser;
}

NamelessCall *nameless_call_get()
{
	static NamelessCall *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(NamelessCall));
		 namelessCallInit(singleton);
	}
	return singleton;
}


