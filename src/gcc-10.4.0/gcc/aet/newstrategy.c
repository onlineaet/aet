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

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"


#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "newstrategy.h"
#include "classutil.h"
#include "newheap.h"
#include "xmlfile.h"
#include "genericfile.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericinfo.h"
#include "genericcommon.h"
#include "genericexpand.h"
#include "makefileparm.h"
#include "funcmgr.h"
#include "classimpl.h"


static NewObjectInfo *createInfo(ClassName *className,char *varName)
{
    NewObjectInfo *data= (NewObjectInfo *)n_slice_new(NewObjectInfo);
    data->name=class_name_clone(className);
    data->varName=n_strdup(varName);
    data->varType=NEW_OBJECT_LOCAL;
    data->backTokenCount=0;
    return data;
}

static void  addTokenToBack(NewStrategy *self,int index,int order,c_token *src)
{
    NewObjectInfo *data=self->privs[index];
    aet_utils_copy_token(src,&data->backTokens[order]);
}

static int  getBackTokenCount(NewStrategy *self,int index)
{
    NewObjectInfo *data=self->privs[index];
    return data->backTokenCount;
}

static void freeNewData(NewStrategy *self,int index)
{
    NewObjectInfo *data=self->privs[index];
    class_name_free(data->name);
    n_free(data->varName);
    n_slice_free(NewObjectInfo,data);
    data->backTokenCount=0;
    self->privs[index]=NULL;
}


static void  setBackTokenCount(NewStrategy *self,int index,int count)
{
    NewObjectInfo *data=self->privs[index];
    data->backTokenCount=count;
}

static void  copyBackTokenToParser(NewStrategy *self,int index,int order,c_token *dest)
{
    NewObjectInfo *data=self->privs[index];
    aet_utils_copy_token(&data->backTokens[order],dest);
}

void  new_strategy_set_var_type(NewStrategy *self,NewObjectType type)
{
    int index=self->nest-1;
    NewObjectInfo *data=self->privs[index];
    data->varType=type;
}

NewObjectType  new_strategy_get_var_type(NewStrategy *self)
{
    int index=self->nest-1;
    NewObjectInfo *data=self->privs[index];
    return data->varType;
}

void new_strategy_add_new(NewStrategy *self,ClassName *className,char *varName)
{
    int index=self->nest;
    NewObjectInfo *data=createInfo(className,varName);
    self->privs[index]=data;
    self->nest++;
}

ClassName * new_strategy_get_class_name(NewStrategy *self)
{
    int index=self->nest-1;
    NewObjectInfo *data=self->privs[index];
    return data->name;
}

char * new_strategy_get_var_name(NewStrategy *self)
{
    int index=self->nest-1;
    NewObjectInfo *data=self->privs[index];
    return data->varName;
}

void new_strategy_recude_new(NewStrategy *self)
{
    int  index=self->nest-1;
    freeNewData(self,index);
    self->nest--;
}

int  new_strategy_get_back_token_count(NewStrategy *self)
{
    int  index=self->nest-1;
    return getBackTokenCount(self,index);
}

void new_strategy_backup_token(NewStrategy *self)
{
      c_parser *parser=self->parser;
      int tokenCount=parser->tokens_avail;
      int i;
      for(i=0;i<tokenCount;i++){
         c_token *token;
         if(i==0){
            token=c_parser_peek_token (parser);
         }else if(i==1){
            token=c_parser_peek_2nd_token (parser);
         }else{
            token=c_parser_peek_nth_token (parser,i);
         }
         addTokenToBack(self,self->nest-1,i,token);
      }
      for(i=0;i<tokenCount;i++){
         c_parser_consume_token (parser);
      }
      setBackTokenCount(self,self->nest-1,tokenCount);
}

void new_strategy_restore_token(NewStrategy *self)
{
   c_parser *parser=self->parser;
   int tokenCount=parser->tokens_avail;
   int backs=getBackTokenCount(self,self->nest-1);
   if(backs==0)
       return;
   if(tokenCount+backs>AET_MAX_TOKEN){
        error("token太多了");
        return;
   }
   int i;
   for(i=tokenCount;i>0;i--){
      aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+backs]);
   }
   parser->tokens_avail=tokenCount+backs;
   for(i=0;i<backs;i++){
       copyBackTokenToParser(self,self->nest-1,i,&parser->tokens[i]);
   }
   aet_print_token_in_parser("restoreToken:--------");
}

static ClassName *getClassNameBySelfParm(NewStrategy *self)
{
     c_parser *parser=self->parser;
     n_debug("new_strategy getClassNameBySelfParm--- %d %p\n",parser->isAet,current_function_decl);
     if(parser->isAet && current_function_decl){
         tree args= DECL_ARGUMENTS (current_function_decl);
         tree paramDecl;
         for (paramDecl = args; paramDecl; paramDecl = DECL_CHAIN (paramDecl)){
             if(TREE_CODE(paramDecl)==PARM_DECL){
                 tree declName=DECL_NAME(paramDecl);
                 char *name=IDENTIFIER_POINTER(declName);
                 if(name && strcmp(name,"self")==0){
                     tree type=TREE_TYPE(paramDecl);
                     if(TREE_CODE(type)==POINTER_TYPE){
                        char *sysName= class_util_get_class_name_by_pointer(type);
                        ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
                        if(className!=NULL){
                            // printf("new_strategy getClassNameBySelfParm---11 %s\n",className->sysName);
                            return className;
                        }
                     }
                 }
             }
             break;
          }
     }
     return NULL;
}

static GenericModel *getGenericFuncDecl(NewStrategy *self)
{
     c_parser *parser=self->parser;
     //printf("getGenericFuncDecl getClassNameBySelfParm--- %d %p\n",parser->isAet,current_function_decl);
     if(parser->isAet && current_function_decl){
        ClassName *className=class_impl_get()->className;
        ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
        GenericModel *classModel=info->genericModel;
        tree currentFunc=current_function_decl;
        char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
        ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,currentFuncName);
        GenericModel *funcModel=class_func_get_func_generic(entity);
        return  funcModel;
     }
     return NULL;
}

/**
 * 通过参数self，把未定的泛型的sizeof替换。
 * 在new对象时的泛型定义包含未定义的泛型E
 * 找出两个位置，一个是所在类环境的泛型位置
 * 另一个是对象泛型声明中的位置
 * class Abc<E> class Efg<F>
 * 如果在Abc内new$对象 Efg 必然是 Efg<E>而不是 Efg<F>
 * 所以E在Efg是找不到的。
 */
static char *replaceGenericBySelf(NewStrategy *self,char *varName,GenericModel *genericDefine,ClassName *className,int *replaces,int replaceCount)
{
     ClassName *selfParmClassName=getClassNameBySelfParm(self);
     if(!selfParmClassName)
         return NULL;
     if(generic_model_get_undefine_count(genericDefine)==0)
         return NULL;
     ClassInfo *newObjectInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
     ClassInfo *atInfo= class_mgr_get_class_info_by_class_name(class_mgr_get(),selfParmClassName);
     int i;
     NString *codes=n_string_new("");
     for(i=0;i<generic_model_get_count(genericDefine);i++){
         GenericUnit  *unit=generic_model_get(genericDefine,i);
         if(!generic_unit_is_undefine(unit))
             continue;
        int index1= i;
        int index2= generic_model_get_index(atInfo->genericModel,unit->name);
        n_debug("replaceGenericBySelfParm---- %d %d\n",index1,index2);
        if(index2<0)
            continue;
        int j;
        nboolean find=FALSE;
        for(j=0;j<replaceCount;j++){
            if(replaces[j]==i){
                n_debug("已经替换过了中泛型函数中\n");
                find=TRUE;
                break;
            }
        }
        if(find)
            continue;
        n_string_append_printf(codes,"memcpy(&(%s->%s[%d]),&(self->%s[%d]),sizeof(aet_generic_info));\n",
                varName,AET_GENERIC_ARRAY,index1,AET_GENERIC_ARRAY,index2);
     }
     char *result=NULL;
     if(codes->len>0){
         result=n_strdup(codes->str);
     }
     n_string_free(codes,TRUE);
     return result;
}

/**
 *如果是在泛型函数内创建对象并且用到了泛型函数的泛型声明。
 */
static char *replaceGenericBytempFgpi1234(NewStrategy *self,char *varName,GenericModel *genericDefine,ClassName *className,int *replaces,int *count)
{
     GenericModel *funcModel=getGenericFuncDecl(self);
     if(!funcModel)
         return NULL;
     int unDefineCount=generic_model_get_undefine_count(genericDefine);
     if(unDefineCount==0)
         return NULL;
     int unDefineStr[32];
     unDefineCount=generic_model_fill_undefine_str(genericDefine,unDefineStr);
     int i;
     NString *codes=n_string_new("");
     int total=0;
     for(i=0;i<unDefineCount;i++){
        int index1= generic_model_get_undefine_index(genericDefine,(char)unDefineStr[i]);
        int index2= generic_model_get_undefine_index(funcModel,(char)unDefineStr[i]);
        n_debug("replaceGenericBytempFgpi1234---- %d %d\n",index1,index2);
        if(index1<0 || index2<0)
            continue;
        n_string_append_printf(codes,"memcpy(&(%s->%s[%d]),&(%s.pms[%d]),sizeof(aet_generic_info));\n",
                varName,AET_GENERIC_ARRAY,index1,AET_FUNC_GENERIC_PARM_NAME,index2);
        replaces[total++]=i;
     }
     *count=total;
     char *result=NULL;
     if(codes->len>0){
         result=n_strdup(codes->str);
     }
     n_string_free(codes,TRUE);
     return result;
}

typedef struct _GenericData{
    char *varName;
    ClassInfo *info;
}GenericData;

static GenericData *findGenericData(char *genStr,NPtrArray *genericArray,int *index)
{
   int i;
   for(i=genericArray->len-1;i>=0;i--){
       GenericData *item= n_ptr_array_index(genericArray,i);
       if(class_info_is_generic_class(item->info)){
           int subscript=class_info_get_generic_index(item->info,genStr);
           if(subscript>=0){
               printf("findIndex findIndex -- %d %s genStr:%s\n",subscript,item->info->className.sysName,genStr);
               *index=subscript;
               return item;
           }
       }
   }
   return NULL;
}

static void fillGenericArray(NString *codes,char *destVar,int destIndex,char *srcVar,int srcIndex,nboolean deref)
{
     char *genericArrayName=AET_GENERIC_ARRAY;
     n_string_append_printf(codes,"strcpy(%s->%s[%d].typeName,%s%s%s[%d].typeName);\n",
             destVar,genericArrayName,destIndex,srcVar,deref?"->":".",genericArrayName,srcIndex);
     n_string_append_printf(codes,"%s->%s[%d].type=%s%s%s[%d].type;\n",
             destVar,genericArrayName,destIndex,srcVar,deref?"->":".",genericArrayName,srcIndex);
     n_string_append_printf(codes,"%s->%s[%d].pointerCount=%s%s%s[%d].pointerCount;\n",
             destVar,genericArrayName,destIndex,srcVar,deref?"->":".",genericArrayName,srcIndex);
     n_string_append_printf(codes,"%s->%s[%d].size=%s%s%s[%d].size;\n",
             destVar,genericArrayName,destIndex,srcVar,deref?"->":".",genericArrayName,srcIndex);
}

/**
 * genericCodesCount判断是否生成泛型代码。
 */
static void recursionInitParentInfo(NewStrategy *self,char *refVarName,ClassName *refClassName,
        NString *codes,NPtrArray *genericArray,nboolean deref,int *tick,int *genericCodesCount)
{
     ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),refClassName);
     if(info==NULL)
         return;
     ClassName *parentName=&info->parentName;
     if(!parentName || !parentName->sysName)
         return;
     GenericData *data=(GenericData *)n_slice_new0(GenericData);
     data->varName=n_strdup(refVarName);
     data->info=info;
     n_ptr_array_add(genericArray,data);
     ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parentName);
     char *genericArrayName=AET_GENERIC_ARRAY;
     char parentVar[128];
     sprintf(parentVar,"temp%s%s",parentName->userName,refVarName);
     if(parentInfo && class_info_is_generic_class(parentInfo)){
             if(info->parentGenericModel){ //说明info也有泛型声明
                 //Abc<E,F> extends AObject<E>
                 if(!info->genericModel){
                     error("定义了父类%qs，但自身%qs没有声明。",parentName->userName,refClassName->userName);
                     return ;
                 }
                 int parentGenCount=generic_model_get_count(info->parentGenericModel);
                 int parentDeclGenCount=class_info_get_generic_count(parentInfo);
                 if(parentGenCount!=parentDeclGenCount){
                     error("在子类%qs定义了父类%qs的泛型数据量与父类声明的数据不符。",refClassName->userName,parentName->userName);
                     return ;
                 }
                 nboolean dotref=(!deref && *tick==0);
                 if(!deref && *tick==0)
                    n_string_append_printf(codes,"%s *%s=(%s *)(&%s);\n",parentName->sysName,parentVar,parentName->sysName,refVarName);
                 else
                    n_string_append_printf(codes,"%s *%s=(%s *)%s;\n",parentName->sysName,parentVar,parentName->sysName,refVarName);
                 *tick=*tick+1;
                 *genericCodesCount+=1;
                 int i;
                 for(i=0;i<parentGenCount;i++){
                     GenericUnit *define=generic_model_get(info->parentGenericModel,i);
                     GenericUnit *decGen=generic_model_get(parentInfo->genericModel,i);
                     if(!define->isDefine && decGen->isDefine){
                         error("定义是一个泛型，但声明是一个具体的类型。错误");
                         return ;
                     }
                     if(define->isDefine && decGen->isDefine){
                         nboolean re= generic_unit_equal(define,decGen);
                         if(!re){
                           error("定义是一个具体类型，但声明也是一个具体的类型。但不相等!");
                           return ;
                         }
                     }
                     if(!define->isDefine && !decGen->isDefine){
                         //从子类中找到这个泛型声明所对应的index
                         char *genStr=define->name;
                         int index=class_info_get_generic_index(info,genStr);
                         if(index<0){
                            error("在子类%qs,找不到泛型声明%qs !",refClassName->userName,genStr);
                            return ;
                         }
                         fillGenericArray(codes,parentVar,i,refVarName,index,!dotref);
                     }else if (define->isDefine && !decGen->isDefine){

                           int size=define->size;
                           int pointerCount=define->pointerCount;
                           int genericType=define->genericType;
                           char *typeName=define->name;
                           char *genStr=decGen->name;
                           printf("设父类的泛型 ---- typeName:%s,genericName:%s type:%d pointerCount:%d size:%d\n",
                                   typeName,genStr,genericType,pointerCount,size);
                           n_string_append_printf(codes,"strcpy(%s->%s[%d].typeName,\"%s\");\n",parentVar,genericArrayName,i,typeName==NULL?"":typeName);
                           n_string_append_printf(codes,"%s->%s[%d].type=%d;\n",parentVar,genericArrayName,i,genericType);
                           n_string_append_printf(codes,"%s->%s[%d].pointerCount=%d;\n",parentVar,genericArrayName,i,pointerCount);
                           n_string_append_printf(codes,"%s->%s[%d].size=%d;\n",parentVar,genericArrayName,i,size);
                     }
                 }//end for
             }else{
                 int parentDeclGenCount=class_info_get_generic_count(parentInfo);
                 nboolean dotref=(!deref && *tick==0);
                 if(!deref && *tick==0)
                    n_string_append_printf(codes,"%s *%s=(%s *)(&%s);\n",parentName->sysName,parentVar,parentName->sysName,refVarName);
                 else
                    n_string_append_printf(codes,"%s *%s=(%s *)%s;\n",parentName->sysName,parentVar,parentName->sysName,refVarName);
                 printf("如果子类的泛型声明与父类的相同，用子类替换。%d\n",parentDeclGenCount);
                 int i;
                 for(i=0;i<parentDeclGenCount;i++){
                    GenericUnit *decGen=generic_model_get(parentInfo->genericModel,i);
                    if(!decGen->isDefine){
                       char *genStr=decGen->name;
                       int index=0;
                       GenericData *data=findGenericData(genStr,genericArray,&index);
                       printf("找子类，一直找到子类的%s为止。找到了吗：%d index:%d\n" ,genStr,data!=NULL,index);
                       if(data!=NULL){
                          fillGenericArray(codes,parentVar,i,data->varName,index,!dotref);
                       }
                    }
                 }//end for
                 *tick=*tick+1;
                 *genericCodesCount+=1;
            }
            recursionInitParentInfo(self,parentVar,parentName,codes,genericArray,deref,tick,genericCodesCount);
     }else if(parentInfo && !class_info_is_generic_class(parentInfo)){
#if 0
         if(!deref && *tick==0)
           n_string_append_printf(codes,"%s *%s=(%s *)(&%s);\n",parentName->sysName,parentVar,parentName->sysName,refVarName);
         else
           n_string_append_printf(codes,"%s *%s=(%s *)%s;\n",parentName->sysName,parentVar,parentName->sysName,refVarName);
#endif
         *tick=*tick+1;
         recursionInitParentInfo(self,parentVar,parentName,codes,genericArray,deref,tick,genericCodesCount);
     }
}

static void freeGenericData_cb(GenericData *item)
{
    n_free(item->varName);
    n_slice_free(GenericData,item);
}

/**
 * 让父类可能找到子类的泛型，比如父类泛型E,通过找子类的E,然后用子类的
 * _generic_1234_array的E对应的内容，替换父类的_generic_1234_array
 */
char *new_strategy_recursion_init_parent_generic_info(NewStrategy *self,char *refVarName,ClassName *refClassName,nboolean deref)
{
     NString *codes=n_string_new("");
     NPtrArray *genericArray=n_ptr_array_new_with_free_func(freeGenericData_cb);
     int tick=0;
     int genericCodesCount=0;
     recursionInitParentInfo(self,refVarName,refClassName,codes,genericArray,deref,&tick,&genericCodesCount);
     if(genericCodesCount==0){
         n_string_free(codes,TRUE);
         n_ptr_array_unref(genericArray);
         return NULL;
     }
     char *result=NULL;
     if(codes->len>0)
         result=n_strdup(codes->str);
     n_string_free(codes,TRUE);
     n_ptr_array_unref(genericArray);
     return result;
}

void new_strategy_add_close_brace(NewStrategy *self)
{
       c_parser *parser=self->parser;
       location_t  loc = c_parser_peek_token (parser)->location;
       c_token *semicolon = c_parser_peek_token (parser);//
       int tokenCount=parser->tokens_avail;
       if(tokenCount+1>AET_MAX_TOKEN){
            error("token太多了");
            return;
       }
       int i;
       for(i=tokenCount;i>1;i--){
          aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
       }
       parser->tokens_avail=tokenCount+1;
       aet_utils_create_token(&parser->tokens[1],CPP_CLOSE_BRACE,"}",1);
       aet_print_token_in_parser("addCloseBrace -----");
}

/**
 * fill对象的_generic_1234_array
 * typedef struct _aet_generic_info{
    char typeName[100];
    char genericName;
    char type;
    char pointerCount;
    int  size;
}aet_generic_info;
 */
static char *createGenericInfoArray(char *varName,ClassName *className,GenericModel *defines,nboolean deref)
{
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(info==NULL || class_info_get_generic_count(info)<=0)
        return NULL;
    char *genericArrayName=AET_GENERIC_ARRAY;
    NString *codes=n_string_new("");
    GenericModel *genModel=info->genericModel;
    int i;
    int genCount=generic_model_get_count(genModel);
    for(i=0;i<genCount;i++){
        GenericUnit  *genDecl=generic_model_get(genModel,i);
        GenericUnit  *define=generic_model_get(defines,i);
        char genericName=-1;
        int size=-1;
        char pointerCount=-1;
        char genericType=-1;
        char *typeName=NULL;
        /**
         * 原始的声明是E，不能被覆盖，但其它内容可以被覆盖
         * <E,F>声明 <char,int>定义genericName保持不变
         * <E,char>声明 <char,int>应该报错。
         */
        if(!genDecl->isDefine){
            char *g=genDecl->name;
            genericName=g[0];
        }
        if(define->isDefine){
           size=define->size;
           pointerCount=define->pointerCount;
           genericType=define->genericType;
           typeName=define->name;
          // printf("createGenericInfoArray ---- typeName:%s,genericName:%d type:%d pointerCount:%d size:%d\n",
                 //  typeName,genericName,genericType,pointerCount,size);
        }
        n_string_append_printf(codes,"strcpy(%s%s%s[%d].typeName,\"%s\");\n",
                varName,deref?"->":".",genericArrayName,i,typeName==NULL?"":typeName);
        n_string_append_printf(codes,"%s%s%s[%d].genericName=%d;\n",varName,deref?"->":".",genericArrayName,i,genericName);
        n_string_append_printf(codes,"%s%s%s[%d].type=%d;\n",varName,deref?"->":".",genericArrayName,i,genericType);
        n_string_append_printf(codes,"%s%s%s[%d].pointerCount=%d;\n",varName,deref?"->":".",genericArrayName,i,pointerCount);
        n_string_append_printf(codes,"%s%s%s[%d].size=%d;\n",varName,deref?"->":".",genericArrayName,i,size);
    }
    char *re=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return re;
}

static void addMiddleCodes(NewStrategy *self,char *varName,ClassName *className,
        GenericModel *genericDefine,char *undefineImplCodes, char *ctorStr,nboolean fromHeap,NString *codes)
{
    ClassInit *classInit=((NewStrategy *)self)->classInit;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(info->genericModel){
        char *genericArrayInitSourceCode=createGenericInfoArray(varName,className,info->genericModel,TRUE);
        n_debug("createInitCodes 填充类中声明的泛型:%s\n",genericArrayInitSourceCode);
        if(genericArrayInitSourceCode!=NULL){
           n_string_append(codes,genericArrayInitSourceCode);
           n_free(genericArrayInitSourceCode);
        }
     }
     if(genericDefine){
        char *genericArrayInitSourceCode=createGenericInfoArray(varName,className,genericDefine,TRUE);
        n_debug("createInitCodes 填充变量中定义的泛型:\n%s\n",genericArrayInitSourceCode);
        if(genericArrayInitSourceCode!=NULL){
           n_string_append(codes,genericArrayInitSourceCode);
           n_free(genericArrayInitSourceCode);
        }
     }
     //如果在泛型函数内用泛型函数的泛型参数替换，然后再用self
     int replaces[20];
     int replaceCount=0;
     char *genericFuncDefine=replaceGenericBytempFgpi1234(self,varName,genericDefine,className,replaces,&replaceCount);
     if(genericFuncDefine!=NULL){
         n_debug("createInitCodes 用参数tempFgpi1234中的泛型替换现有泛型中未定的泛型:%s\n",genericFuncDefine);
        n_string_append(codes,genericFuncDefine);
        n_free(genericFuncDefine);
     }
     char *replaceGenericBySelfParm=replaceGenericBySelf(self,varName,genericDefine,className,replaces,replaceCount);
     if(replaceGenericBySelfParm!=NULL){
         n_debug("createInitCodes 用参数self中的泛型替换现有泛型中未定的泛型:%s\n",replaceGenericBySelfParm);
           n_string_append(codes,replaceGenericBySelfParm);
           n_free(replaceGenericBySelfParm);
     }
     char *parentGenericInit=new_strategy_recursion_init_parent_generic_info((NewStrategy *)self,varName,className,TRUE);
     if(parentGenericInit!=NULL){
        n_debug("createInitCodes 初始化父类的泛型");
        n_string_append(codes,parentGenericInit);
        n_free(parentGenericInit);
     }
     char *initFuncName=aet_utils_create_init_method(className->sysName);
     n_string_append_printf(codes,"%s(%s);\n",initFuncName,varName);
    /**
      * class_init_override_parent_ref不再调用，该方法被classinit.c 中的modifyParentMethod替换，并且在
      * class_init_create_init_define中调用,比较重大的改变。解决BUG，子类定义的函数（没有声明的情况下）不能
      * 替换父类的同名同参数函数的问题。
      */
     char *override=NULL;//zclei 该方法被classinit.c 中的modifyParentMethod替换。class_init_override_parent_ref(classInit,className,varName);
     char *freeChild=class_init_modify_root_object_free_child(classInit,className,varName);
     if(override!=NULL){
       n_string_append(codes,override);
       n_free(override);
     }
     if(freeChild!=NULL){
       n_string_append(codes,freeChild);
       n_free(freeChild);
     }
     if(undefineImplCodes!=NULL)
        n_string_append(codes,undefineImplCodes);
     //如果不是堆分配的内存把objectSize设为0
     if(!fromHeap)
        n_string_append_printf(codes,"%s->objectSize=0;\n",varName);
     if(ctorStr!=NULL){
       n_string_append_printf(codes,"%s *tempObject123=%s->%s;\n",className->sysName,varName,ctorStr);
       n_string_append(codes,"if(tempObject123==NULL){\n");
       n_string_append_printf(codes,"printf(\"执行构造函数%s时，返回空值。\\n\");\n",className->userName);
       n_string_append_printf(codes,"if(%s->objectSize!=0){\n",varName);
       n_string_append_printf(codes,"%s->unref();\n",varName);
       n_string_append_printf(codes,"%s=NULL;\n",varName);
       n_string_append(codes,"}\n}\n");
     }else{
         //因该报错
         n_error("类%s没有构造函数，报告此错误。",className->userName);
     }
}


static void new_strategy_create_common_heap_codes(NewStrategy *self,char *varName,ClassName *className,
        GenericModel *genericDefine,char *undefineImplCodes, char *ctorStr,NString *codes,nboolean addSSemicolon)
{
     ClassInit *classInit=((NewStrategy *)self)->classInit;
     ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
     n_string_append_printf(codes,"({ %s *%s;\n",className->sysName,varName);
     n_string_append_printf(codes,"%s=%s.newObject(sizeof(%s));\n",varName,className->sysName,className->sysName);
     n_string_append_printf(codes,"%s->objectSize=sizeof(%s);\n",varName,className->sysName);
     n_string_append_printf(codes,"%s->%s=%d;\n",varName,AET_MAGIC_NAME,AET_MAGIC_NAME_VALUE);
     addMiddleCodes(self,varName,className,genericDefine,undefineImplCodes, ctorStr,TRUE,codes);
     n_string_append_printf(codes,"%s;})%s\n",varName,addSSemicolon?";":"");
}

static void fillBlockInfoList(NewStrategy *self,char *objectVarName,NString *codes)
{
    n_string_append_printf(codes,"%s->%s.data=&%s;\n",objectVarName,AET_BLOCK_INFO_LIST_VAR_NAME,FuncGenParmInfo_NAME);
    nboolean haveNext=FALSE;
    c_parser *parser=self->parser;
    if(parser->isAet){
       ClassName *atClassName=class_impl_get()->className;
       ClassInfo *atClassInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),atClassName);
       if(class_info_about_generic(atClassInfo)){
           //找一下有没能self
            tree selfParm=lookup_name(aet_utils_create_ident("self"));
            if(aet_utils_valid_tree(selfParm)){
                n_string_append_printf(codes,"%s->%s.next=&self->%s;\n",objectVarName,AET_BLOCK_INFO_LIST_VAR_NAME,AET_BLOCK_INFO_LIST_VAR_NAME);
                haveNext=TRUE;
            }
       }
    }
    if(!haveNext)
        n_string_append_printf(codes,"%s->%s.next=NULL;\n",objectVarName,AET_BLOCK_INFO_LIST_VAR_NAME);
}


static void setParentSetBlock(NewStrategy *self,char *objectVarName,char *sysClassName,NString *codes,int type)
{
     ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
     if(info->parentName.sysName==NULL){
         return;
     }
     ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&info->parentName);
     if(class_info_about_generic(parentInfo)){
        if(type==0){
            n_string_append_printf(codes,"((%s)self->%s)((void*)self,(void*)%s,\"%s\",0);\n",AET_SET_BLOCK_FUNC_TYPEDEF_NAME,
                    AET_SET_BLOCK_FUNC_VAR_NAME,objectVarName,parentInfo->className.sysName);
        }else if(type==1){
            n_string_append_printf(codes,"((%s)%s.%s)((void*)self,(void*)%s,\"%s\",0);\n",AET_SET_BLOCK_FUNC_TYPEDEF_NAME,
                    AET_FUNC_GENERIC_PARM_NAME,AET_SET_BLOCK_FUNC_VAR_NAME,objectVarName,parentInfo->className.sysName);
        }else if(type==2){
            n_string_append_printf(codes,"((%s)%s->%s)((void*)%s,(void*)%s,\"%s\",0);\n",AET_SET_BLOCK_FUNC_TYPEDEF_NAME,
                    objectVarName,AET_SET_BLOCK_FUNC_VAR_NAME,objectVarName,objectVarName,parentInfo->className.sysName);
        }else if(type==4){
                char newVarName[255];
                sprintf(newVarName,"((%s *)%s)",parentInfo->className.sysName,objectVarName);
                fillBlockInfoList(self,newVarName,codes);
        }
     }
     setParentSetBlock(self,objectVarName,parentInfo->className.sysName,codes,type);
}



/**
 * 创建undefineCodes 两句关键
 * 1.((_setGenericBlockFunc_123_typedef)abc.func)(&abc,&abc,"ttxxx",3);
 *   父类同样设置 self来自所在的类实现
 * 2.objectVarName->_block_info_list_var_name.data=&_inFileGlobalFuncGenParmInfo;
 * objectVarName->_block_info_list_var_name.next=NULL;
 * 或
 * objectVarName->_block_info_list_var_name.next=self->_block_info_list_var_name;
 */
static char *createUndefineCode(NewStrategy *self,char *objectVarName,char *sysClassName)
{
    NString *codes=n_string_new("");
    //((_setGenericBlockFunc_123_typedef)abc.func)(&abc,&abc,"ttxxx",3);
    n_string_append_printf(codes,"((%s)self->%s)((void*)self,(void*)%s,\"%s\",0);\n",AET_SET_BLOCK_FUNC_TYPEDEF_NAME,
            AET_SET_BLOCK_FUNC_VAR_NAME,objectVarName,sysClassName);
    setParentSetBlock(self,objectVarName,sysClassName,codes,0);
    fillBlockInfoList(self,objectVarName,codes);
    setParentSetBlock(self,objectVarName,sysClassName,codes,4);
    char *re=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return re;
}



/**
 * 回调函数中第二个参数AET_FUNC_GENERIC_PARM_NAME   "tempFgpi1234" 中
 */
static char *createUndefineCodeByGenericFunc(NewStrategy *self,char *objectVarName,char *sysClassName)
{
    NString *codes=n_string_new("");
    n_string_append_printf(codes,"((%s)%s.%s)((void*)self,(void*)%s,\"%s\",0);\n",AET_SET_BLOCK_FUNC_TYPEDEF_NAME,
            AET_FUNC_GENERIC_PARM_NAME,AET_SET_BLOCK_FUNC_VAR_NAME,objectVarName,sysClassName);
    setParentSetBlock(self,objectVarName,sysClassName,codes,1);
    fillBlockInfoList(self,objectVarName,codes);
    setParentSetBlock(self,objectVarName,sysClassName,codes,4);
    char *re=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return re;
}


static char *createDefineCode(NewStrategy *self,char *objectVarName,ClassName *className,GenericModel *genericDefines)
{
    NString *codes=n_string_new("");
    tree id=aet_utils_create_ident(AET_SET_BLOCK_FUNC_CALLBACK_NAME);
    tree funcdecl=lookup_name(id);
    if(!aet_utils_valid_tree(funcdecl)){
       n_string_append_printf(codes,"%s %d void %s(void *self,void *object,char *sysName,int pointer);\n",
               RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_DECL,AET_SET_BLOCK_FUNC_CALLBACK_NAME);
    }
    n_string_append_printf(codes,"%s->%s=%s;\n",objectVarName,AET_SET_BLOCK_FUNC_VAR_NAME,AET_SET_BLOCK_FUNC_CALLBACK_NAME);
    //((_setGenericBlockFunc_123_typedef)abc.func)(&abc,&abc,"ttxxx",3);
    n_string_append_printf(codes,"((%s)%s->%s)((void*)%s,(void*)%s,\"%s\",0);\n",AET_SET_BLOCK_FUNC_TYPEDEF_NAME,
            objectVarName,AET_SET_BLOCK_FUNC_VAR_NAME,objectVarName,objectVarName,className->sysName);
    setParentSetBlock(self,objectVarName,className->sysName,codes,2);
    fillBlockInfoList(self,objectVarName,codes);
    setParentSetBlock(self,objectVarName,className->sysName,codes,4);
    char *re=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return re;
}

/**
 * 选择泛型来自所在函数的self对象还是泛型函数
 */
static int selectGenericFromObjectOrGenFunc(NewStrategy *self,ClassName *className,GenericModel *genericDefine)
{
    c_parser *parser=self->parser;
    if(!parser->isAet){
        error_at(input_location,"泛型未定的类%qs只能在类中创建。",className->userName);
        return -1;
    }
    tree currentFunc=current_function_decl;
    if(!aet_utils_valid_tree(currentFunc)){
        error_at(input_location,"泛型未定的类%qs只能在类函数中创建。",className->userName);
        return -1;
    }
    char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
    ClassName *atClassName=class_impl_get()->className;
    ClassFunc *func=func_mgr_get_entity(func_mgr_get(),atClassName, currentFuncName);
    if(func==NULL){
        error_at(input_location,"新建类%qs对象时，在类%s中找不到函数%qs。",className->userName,atClassName->userName,currentFuncName);
        return -1;
    }

    if(class_func_is_func_generic(func)){
        GenericModel *funcModel=class_func_get_func_generic(func);
        if(generic_model_include_decl(funcModel,genericDefine)){
            return 0;
        }
    }
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),atClassName);
    if(class_info_is_generic_class(info)){
        GenericModel  *classModel=class_info_get_generic_model(info);
        if(generic_model_include_decl(classModel,genericDefine)){
            return 1;
        }
    }
    if(class_func_is_func_generic(func)){
        error_at(input_location,"新建类%qs对象时，未定泛型%qs在函数%qs中没找到。",className->userName,generic_model_tostring(genericDefine),func->orgiName);
    }else{
        error_at(input_location,"新建类%qs对象时，未定泛型%qs在类%qs的泛型声明中没找到。",className->userName,generic_model_tostring(genericDefine),atClassName->userName);
    }
    return -1;
}

static char *newObject(NewStrategy *self,char *tempVarName,GenericModel *genericDefine,ClassName *className)
{
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    char *undefineImplCodes=NULL;// Abc *tt=new$ Abc<E>() tt的<E>转成真实类型的代码
    nboolean aboutGeneric=class_mgr_about_generic(class_mgr_get(),className);
    //printf("new_strategy_new_object %s genericDefine:%p 是不是泛型相关的:%d\n",className->sysName,genericDefine,aboutGeneric);
    if(aboutGeneric){
        if(genericDefine==NULL && info->genericModel!=NULL){
            error_at(input_location,"类%qs是泛型相关的，但没有指定泛型。",className->userName);
            return NULL;
        }
        generic_expand_create_fggb_var_and_setblock_func(generic_expand_get());//生成static FuncGenParmInfo   _inFileGlobalFuncGenParmInfo={0};
        int undefine=generic_model_get_undefine_count(genericDefine);
        if(undefine>0){
            n_debug("new$对象的泛型是声明还未定义%s\n",className->sysName);
            //undefineImplCodes的内容如下
            //self->_createAllGenericFunc567((void*)_notv2_12test_AObject0,"test_AObject",0);
            int re=selectGenericFromObjectOrGenFunc(self,className,genericDefine);
            if(re==1)
               undefineImplCodes=createUndefineCode(self,tempVarName,className->sysName);
            else
               undefineImplCodes=createUndefineCodeByGenericFunc(self,tempVarName,className->sysName);
            block_mgr_add_undefine_new_object(block_mgr_get(),className,genericDefine);
        }else{
            nboolean second= makefile_parm_is_second_compile(makefile_parm_get());
            n_debug("编译的文件中调用了new$对象传递有泛型参数据-------是不是第二次:%d %s genericDefine:%p\n",second,className->sysName,genericDefine);
            block_mgr_add_define_new_object(block_mgr_get(),className,genericDefine);
            if(!second){
                generic_file_use_generic(generic_file_get());
            }else{
                undefineImplCodes=createDefineCode(self,tempVarName,className,genericDefine);
                generic_expand_add_define_object(generic_expand_get(),className,genericDefine);
                generic_expand_add_eof_tag(generic_expand_get());//加cpp_buffer到当前.c文件的prev，当编译.c结束时，可以加入块定义
            }
        }
    }
    return undefineImplCodes;
}

void new_strategy_new_object(NewStrategy *self,char *tempVarName,GenericModel *genericDefine,ClassName *className,char *ctorStr,NString *codes,nboolean addSemision)
{
    char *undefineImplCodes=newObject(self,tempVarName,genericDefine,className);
    new_strategy_create_common_heap_codes(self,tempVarName,className,genericDefine,undefineImplCodes,ctorStr,codes,addSemision);
    if(undefineImplCodes)
        n_free(undefineImplCodes);
}

/**
 * var已经是转化的 如:Abc *_notv3_3Abc=&self->abc;
 */
void new_strategy_new_object_from_field_stack(NewStrategy *self,tree var,ClassName *className,NString *codes)
{
    GenericModel *genericDefine= c_aet_get_generics_model(var);
    ClassInit *classInit=((NewStrategy *)self)->classInit;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    tree ctor=c_aet_get_ctor_codes(var);
    char *ctorStr=NULL;
    if(aet_utils_valid_tree(ctor)){
       ctorStr=IDENTIFIER_POINTER(ctor);
    }
    char *tempVarName=IDENTIFIER_POINTER(DECL_NAME(var));
    char *undefineImplCodes=newObject(self,tempVarName,genericDefine,className);
    addMiddleCodes(self,tempVarName,className,genericDefine,undefineImplCodes, ctorStr,FALSE,codes);
}

void new_strategy_new_object_from_stack(NewStrategy *self,tree var,ClassName *className,NString *codes)
{
    char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
    GenericModel *genericDefine= c_aet_get_generics_model(var);
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    tree ctor=c_aet_get_ctor_codes(var);
    char *ctorStr=NULL;
    if(aet_utils_valid_tree(ctor)){
       ctorStr=IDENTIFIER_POINTER(ctor);
    }
    char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_STACK);
    char *undefineImplCodes=newObject(self,tempVarName,genericDefine,className);
    n_string_append_printf(codes,"%s *%s=(%s *)(&%s);\n",className->sysName,tempVarName,className->sysName,varName);
    addMiddleCodes(self,tempVarName,className,genericDefine,undefineImplCodes, ctorStr,FALSE,codes);
}

/**
 * new$ Abc() 没有名字的栈对象
 */
void new_strategy_new_object_from_stack_no_name(NewStrategy *self,char *varName,GenericModel *genericDefine,
        char *ctorStr,ClassName *className,NString *codes)
{
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_STACK);
    char *undefineImplCodes=newObject(self,tempVarName,genericDefine,className);
    n_string_append_printf(codes,"%s *%s=(%s *)(&%s);\n",className->sysName,tempVarName,className->sysName,varName);
    addMiddleCodes(self,tempVarName,className,genericDefine,undefineImplCodes, ctorStr,FALSE,codes);
}

void  new_strategy_set_parser(NewStrategy *self,c_parser *parser)
{
     self->parser=parser;
     if(!self->classInit)
         self->classInit=class_init_new();
     self->classInit->parser=parser;
}
