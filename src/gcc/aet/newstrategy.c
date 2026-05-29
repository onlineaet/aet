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

#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "classutil.h"
#include "newstrategy.h"
#include "classutil.h"
#include "newheap.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericinfo.h"
#include "makefileparm.h"
#include "funcmgr.h"
#include "classimpl.h"
#include "genericgraph.h"


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
      c_parser *parser=self->parser->parser;
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
   c_parser *parser=self->parser->parser;
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
 * 生成为父类设置泛型类型的代码。
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
                           error("定义是一个具体类型，声明也是一个具体的类型。但不相等!");
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
         result=  n_string_free(codes,FALSE);
     else
        n_string_free(codes,TRUE);
     n_ptr_array_unref(genericArray);
     return result;
}


void new_strategy_add_close_brace(NewStrategy *self)
{
       c_parser *parser=self->parser->parser;
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


static void addMiddleCodes(NewStrategy *self,char *varName,ClassName *className,
        GenericModel *genericDefine,char *modifyGenericCodes, char *ctorStr,nboolean fromHeap,NString *codes)
{
    ClassInit *classInit=((NewStrategy *)self)->classInit;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(modifyGenericCodes!=NULL)
       n_string_append(codes,modifyGenericCodes);

     char *initFuncName=aet_utils_create_init_method(className->sysName);
     n_string_append_printf(codes,"\t%s(%s);\n",initFuncName,varName);
    /**
      * class_init_override_parent_ref不再调用，该方法被classinit.c 中的modifyParentMethod替换，并且在
      * class_init_create_init_define中调用,比较重大的改变。解决BUG，子类定义的函数（没有声明的情况下）不能
      * 替换父类的同名同参数函数的问题。
      */
     char *freeChild=class_init_modify_root_object_free_child(classInit,className,varName);
     if(freeChild!=NULL){
       n_string_append(codes,"\t");
       n_string_append(codes,freeChild);
       n_free(freeChild);
     }
     //如果不是堆分配的内存把objectSize设为0
     if(!fromHeap)
        n_string_append_printf(codes,"\t%s->objectSize=0;\n",varName);

     if(ctorStr!=NULL){
       n_string_append_printf(codes,"\t%s *tempObject123=%s->%s;\n",className->sysName,varName,ctorStr);
       n_string_append(codes,"\tif(tempObject123==NULL){\n");
       n_string_append_printf(codes,"\t\tprintf(\"执行构造函数%s时，返回空值。\\n\");\n",className->userName);
       n_string_append_printf(codes,"\t\tif(%s->objectSize>0){\n",varName);
       n_string_append_printf(codes,"\t\t\t%s->unref();\n",varName);
       n_string_append_printf(codes,"\t\t\t%s=NULL;\n",varName);
       n_string_append(codes,"\t\t}\n");
       n_string_append(codes,"\t}\n");
     }else{
         //因该报错
         n_error("类%s没有构造函数，报告此错误。",className->userName);
     }
}

/**
 * rtcs=({ TFirst *_notv2_6TFirst0;
_notv2_6TFirst0=TFirst.newObject(sizeof(TFirst));
 */
static void new_strategy_create_common_heap_codes(NewStrategy *self,char *varName,ClassName *className,
      GenericModel *genericDefine,char *undefineImplCodes,
      char *ctorStr,NString *codes,nboolean addSSemicolon,tree mtcsPlatType)
{
   ClassInit *classInit=((NewStrategy *)self)->classInit;
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   n_string_append_printf(codes,"({\n \t%s *%s;\n",className->sysName,varName);
   if(mtcsPlatType){
      printf("new_strategy_create_common_heap_codes---- %lu\n",(unsigned long)mtcsPlatType);
     n_ptr_array_add(self->mtcsPlatformAndDevnum,mtcsPlatType);
   }
   if(mtcsPlatType==NULL_TREE)
      n_string_append(codes,"\tunsigned int _mtcsPlatType0=0;\n");
   else
      n_string_append_printf(codes,"\tint _mtcsPlatType0=%s %d %lu;\n",
            RID_AET_GOTO_STR,GOTO_MTCS_CREATE_OBJ,(unsigned long)mtcsPlatType);
   nboolean atClassFunc=FALSE;
   if(self->parser->isAet){
      tree current=current_function_decl;
      ClassFunc    *func=func_mgr_get_func(func_mgr_get(),current);
      if(func){
         //说明在类函数中，并且不是静态函数
         atClassFunc=TRUE;
      }
   }
   /*
   if(atClassFunc){
      n_string_append(codes,"\tint _isMtcs=self->getClass()->isMtcsClass();\n");
   }else{
      n_string_append_printf(codes,"\tint _isMtcs=((AClass *)%s.class)->isMtcsClass();\n",className->sysName);
   }
   */
   n_string_append_printf(codes,"\tint _isMtcs=((AClass *)%s.class)->isMtcsClass();\n",className->sysName);
   if(atClassFunc){
      n_string_append(codes,"\tif(!_isMtcs)\n");
      n_string_append(codes,"\t\t_isMtcs=self->getClass()->isMtcsClass();\n");
   }

   n_string_append_printf(codes,"\t%s=%s.newObject(sizeof(%s),_isMtcs,_mtcsPlatType0,\"%s\");\n",
                  varName, className->sysName,className->sysName,className->userName);

   n_string_append_printf(codes,"\t%s->objectSize=sizeof(%s);\n",varName,className->sysName);
   n_string_append_printf(codes,"\t%s->%s=_mtcsPlatType0;\n",varName,AET_MTCS_PLATFORM_TYPE_VAR_NAME);
   n_string_append_printf(codes,"\t%s->%s=%d;\n",varName,AET_MAGIC_NAME,AET_MAGIC_NAME_VALUE);
   addMiddleCodes(self,varName,className,genericDefine,undefineImplCodes, ctorStr,TRUE,codes);
   n_string_append_printf(codes,"\t%s;\n",varName);
   n_string_append_printf(codes,"})%s\n",addSSemicolon?";":"");

}

/**
 * 为新对象的泛型变量AET_GENERIC_ARRAY赋值。如果新对象的泛型单元是定义的，
 * 用定义单元赋值，否则用当前self或泛型函数的泛型变量AET_GENERIC_ARRAY覆盖。
 */
static void modifyNewObjectGenericCodes(NString *codes,int i,char *varName,RunGenericInfo *info)
{
   char name[128];
   sprintf(name,"%s->%s[%d]",varName,AET_GENERIC_ARRAY,i);
   if(info->from==UNIT_FROM_NEW_OBJECT){ //来自对象本身的定义
      GenericUnit *genUnit = info->genUnit;
      n_string_append_printf(codes,"strcpy(%s.typeName,\"%s\");\n",name,genUnit->name==NULL?"":genUnit->name);
      n_string_append_printf(codes,"%s.genericName=-1;\n",name);
      n_string_append_printf(codes,"%s.type=%d;\n",name,genUnit->genericType);
      n_string_append_printf(codes,"%s.pointerCount=%d;\n",name,genUnit->pointerCount);
      n_string_append_printf(codes,"%s.size=%d;\n",name,genUnit->size);
   }else if(info->from==UNIT_FROM_GENERIC_FUNC){ //来自泛型函数
      n_string_append_printf(codes,"memcpy(&%s,&%s.%s[%d],sizeof(%s));\n",name,
      AET_GENERIC_FUNC_THREAD_BLOCK_ADDR,AET_GENERIC_ARRAY,info->fromPos,AET_GENERIC_INFO_STRUCT_NAME);
   }else { //来自self
      n_string_append_printf(codes,"memcpy(&%s,&self->%s[%d],sizeof(%s));\n",
            name, AET_GENERIC_ARRAY,info->fromPos,AET_GENERIC_INFO_STRUCT_NAME);
   }
}


static void modifyParentGenericCodes(NString *codes,int i,char *varName,RunGenericInfo *info,ClassInfo *child,ClassInfo *parentInfo)
{
   //((A*)varName)->_generic_1234_array[i]
   char name[128];
   sprintf(name,"((%s *)%s)->%s[%d]",parentInfo->className.sysName,varName,AET_GENERIC_ARRAY,i);
   if(info->from==UNIT_FROM_NEW_OBJECT){ //来自子类声明时的设定 class$ A extends B<int>
      GenericUnit *genUnit = info->genUnit;
      n_string_append_printf(codes,"strcpy(%s.typeName,\"%s\");\n", name,genUnit->name==NULL?"":genUnit->name);
      n_string_append_printf(codes,"%s.genericName=-1;\n",name);
      n_string_append_printf(codes,"%s.type=%d;\n",name,genUnit->genericType);
      n_string_append_printf(codes,"%s.pointerCount=%d;\n",name,genUnit->pointerCount);
      n_string_append_printf(codes,"%s.size=%d;\n",name,genUnit->size);
   }else if(info->from==UNIT_FROM_CHILD){ //来自子类
      char childName[128];
      sprintf(childName,"((%s *)%s)->%s[%d]",child->className.sysName,varName,AET_GENERIC_ARRAY,info->fromPos);
      n_string_append_printf(codes,"memcpy(&%s,&%s,sizeof(%s));\n",name,childName,AET_GENERIC_INFO_STRUCT_NAME);
   }else{
      gcc_unreachable();
   }
}

/**
 * 收集新建对象的父类的泛型信息。
 */
static void collectNewObjectParentGeneric(NewStrategy *self,char *varName,RunGenericInfo **childRunGenInfos,ClassName *child,NString *codes)
{
   ClassInfo *childInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),child);
  // printf("collectNewObjectParentGeneric --00 %s\n",childInfo->parentName.sysName);

   while(childInfo->parentName.sysName!=NULL){
      RunGenericInfo **parentGenericInfos=generic_impl_collect_parent_info(generic_impl_get(),childInfo,childRunGenInfos);
      ClassInfo *parentInfo=class_mgr_get_class_info(class_mgr_get(),childInfo->parentName.sysName);
      if(parentGenericInfos!=NULL){
         generic_graph_add_new_class(generic_graph_get(),parentGenericInfos,parentInfo,NULL,NULL,TRUE);
         int i;
         int parentUnitCount=generic_model_get_count(parentInfo->genericModel);
         for(i=0;i<parentUnitCount;i++){
            RunGenericInfo *item=parentGenericInfos[i];
            modifyParentGenericCodes(codes,i,varName,item,childInfo,parentInfo);
         }
      }
      //printf("collectNewObjectParentGeneric -- %s\n",parentInfo->className.sysName);
      childInfo=parentInfo;
      childRunGenInfos=parentGenericInfos;
   }
}

/**
 * 用new 对象时定义的泛型给类变量 AET_GENERIC_ARRAY 赋值
 */
static RunGenericInfo **collectNewObjectGeneric(NewStrategy *self,GenericModel *genericDefine,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(!class_info_is_generic_class(info))
      return NULL;

   if(genericDefine==NULL){
      error_at(input_location,"但没有输入泛型模型。",className->userName);
      return NULL;
   }
   int undefine=generic_model_get_undefine_count(genericDefine);
   if(undefine>0){ //有未定义的泛型
      if(!self->parser->isAet){
         error_at(input_location,"泛型未定的类%qs只能在类实现中创建。",className->userName);
         return NULL;
      }
   }

   //泛型函数声明的类型包含了genericDefine 如:func=<E> genericDefine = <int,E> 匹配。
   tree currentFunc=current_function_decl;
   if(undefine>0){ //有未定义的泛型
      if(!aet_utils_valid_tree(currentFunc)){
         error_at(input_location,"泛型未定的类%qs只能在类函数中创建。",className->userName);
         return NULL;
      }
   }
   char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
   ClassName *atClassName=class_impl_get()->className;
   ClassInfo *atInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),atClassName);
   ClassFunc *atFunc=func_mgr_get_entity(func_mgr_get(),atClassName, currentFuncName);
   if(undefine>0){ //有未定义的泛型
      if(atFunc==NULL){
         error_at(input_location,"只能在类函数中创新建泛型未定义的类对象。");
         return NULL;
      }
   }
   int errorUnit=-1;
   RunGenericInfo **infos=generic_impl_collect_info(generic_impl_get(),genericDefine,atFunc,atInfo,&errorUnit);
   if(errorUnit>0){
      GenericUnit *unit=generic_model_get(genericDefine,errorUnit);
      error_at(unit->loc,"未知的类型%qs。",unit->name);
   }
   return infos;

}

/**
 * 调用方法 aet_generic_class_fill_address 填充泛型类中的块函数地址 AET_GENERIC_BLOCK_ARRAY_VAR_NAME  "_gen_blocks_array_897"
 */
static void modifyFuncAddress(NString *codes,int genUnitCount,char *tempVarName,ClassName *className)
{
   n_string_append_printf(codes,"%s(%s->%s,%d,\"%s\",%s->%s);\n",
         AET_GENERIC_CLASS_FILL_ARRAY_ADDR,
         tempVarName,AET_GENERIC_ARRAY,genUnitCount,
         className->sysName,tempVarName,AET_GENERIC_BLOCK_ARRAY_VAR_NAME);
}

static void modifyParentFuncAddress(NString *codes,char *tempVarName,ClassName *child)
{
   ClassInfo *childInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),child);
   while(childInfo->parentName.sysName!=NULL){
      ClassInfo *parentInfo=class_mgr_get_class_info(class_mgr_get(),childInfo->parentName.sysName);
      if(class_info_is_generic_class(parentInfo)){
         int i;
         int parentUnitCount=generic_model_get_count(parentInfo->genericModel);
         n_string_append_printf(codes,"%s(((%s*)%s)->%s,%d,\"%s\",((%s *)%s)->%s);\n",
               AET_GENERIC_CLASS_FILL_ARRAY_ADDR,
               parentInfo->className.sysName, tempVarName,AET_GENERIC_ARRAY,
               parentUnitCount,
               parentInfo->className.sysName,
               parentInfo->className.sysName,tempVarName,AET_GENERIC_BLOCK_ARRAY_VAR_NAME);
      }
      //printf("collectNewObjectParentGeneric -- %s\n",parentInfo->className.sysName);
      childInfo=parentInfo;
   }
}


//新版 不需要第二次编译所在文件，加入确定的泛型类型，未定义的从self或泛型函数中取并设到当前对象中。
//1.未定泛型从当前所在函数或所在函数的self参数来
//如果所在函数是泛型函数，找出与未定泛型相同的泛型声明
static char *newObject(NewStrategy *self,char *tempVarName,GenericModel *genericDefine,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   //生成代码给对象_generic_1234_array赋值
   RunGenericInfo **infos = collectNewObjectGeneric(self,genericDefine,className);
   NString *codes=n_string_new("");
   if(infos!=NULL){
      ClassName *atClassName = class_impl_get()->className;
      ClassInfo *atInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),atClassName);
      ClassFunc *atFunc=NULL;
      if(current_function_decl)
         atFunc=func_mgr_get_entity(func_mgr_get(),atClassName, IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));

      generic_graph_add_new_class(generic_graph_get(),infos,info,atFunc,atInfo,FALSE);
      int destCount=generic_model_get_count(genericDefine);
      int i;
      for(i=0;i<destCount;i++){
         RunGenericInfo *item=infos[i];
         modifyNewObjectGenericCodes(codes,i,tempVarName,item);
      }
      modifyFuncAddress(codes,destCount,tempVarName,className);
   }
   collectNewObjectParentGeneric(self,tempVarName,infos,className,codes);
   modifyParentFuncAddress(codes,tempVarName,className);
   return n_string_free(codes,FALSE);
}

void new_strategy_new_object(NewStrategy *self,char *tempVarName, GenericModel *genericDefine,
      ClassName *className,char *ctorStr,NString *codes,nboolean addSemision,tree mtcsPlatType)
{
   char *modifyGenericCodes=newObject(self,tempVarName,genericDefine,className);
   new_strategy_create_common_heap_codes(self,tempVarName,className,genericDefine,
         modifyGenericCodes,ctorStr,codes,addSemision,mtcsPlatType);
   if(modifyGenericCodes)
      n_free(modifyGenericCodes);
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

/**
 * 创建栈对象
 * TFirst evvrtcs=new$ TFirst();
 * 代码是:
 * {
 *    TFirst *_notv1_6TFirst0=(TFirst *)(&evvrtcs);
      TFirst_init_1234ergR5678_TFirst(_notv1_6TFirst0);
      ...
 * codes中已有左大括号{
 * newstack.c中的方法setInitGlobalVar-->createInitCodes调用
 */
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
    tree mtcsPlatType =  c_aet_get_mtcs_plat_type(var);
    n_debug("在这里调用 new_strategy_new_object_from_stack mtcsplatformType:%p\n",mtcsPlatType);
    char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_STACK);
    char *undefineImplCodes=newObject(self,tempVarName,genericDefine,className);
    n_string_append_printf(codes,"\t%s *%s=(%s *)(&%s);\n",className->sysName,tempVarName,className->sysName,varName);
    if(mtcsPlatType==NULL_TREE)
       n_string_append(codes,"\tunsigned int _mtcsPlatType0=0;\n");
    else
       n_string_append_printf(codes,"\tint _mtcsPlatType0=%s %d %lu;\n",
             RID_AET_GOTO_STR,GOTO_MTCS_CREATE_OBJ,(unsigned long)mtcsPlatType);

    n_string_append_printf(codes,"\t%s->%s=_mtcsPlatType0;\n",tempVarName,AET_MTCS_PLATFORM_TYPE_VAR_NAME);
    addMiddleCodes(self,tempVarName,className,genericDefine,undefineImplCodes, ctorStr,FALSE,codes);
}

/**
 * new$ Abc() 没有名字的栈对象
 */
void new_strategy_new_object_from_stack_no_name(NewStrategy *self,char *varName,GenericModel *genericDefine,
        char *ctorStr,ClassName *className,NString *codes,tree mtcsPlatType)
{
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_STACK);
    char *undefineImplCodes=newObject(self,tempVarName,genericDefine,className);
    n_string_append_printf(codes,"\t%s *%s=(%s *)(&%s);\n",className->sysName,tempVarName,className->sysName,varName);
    if(mtcsPlatType==NULL_TREE)
       n_string_append(codes,"\tunsigned int _mtcsPlatType0=0;\n");
    else
       n_string_append_printf(codes,"\tint _mtcsPlatType0=%s %d %lu;\n",
             RID_AET_GOTO_STR,GOTO_MTCS_CREATE_OBJ,(unsigned long)mtcsPlatType);

    n_string_append_printf(codes,"\t%s->%s=_mtcsPlatType0;\n",tempVarName,AET_MTCS_PLATFORM_TYPE_VAR_NAME);
    addMiddleCodes(self,tempVarName,className,genericDefine,undefineImplCodes, ctorStr,FALSE,codes);
}

tree  new_strategy_get_mtcs_plat_and_dev(NewStrategy *self,unsigned long address)
{
   int len=self->mtcsPlatformAndDevnum->len;
   int i;
   for(i=0;i<len;i++){
      tree item=n_ptr_array_index(self->mtcsPlatformAndDevnum,i);
      printf("new_strategy_get_mtcs_plat_and_dev -- %lu %lu\n",address,((unsigned long)item));
      if(((unsigned long)item)==address){
         printf("new_strategy_get_mtcs_plat_and_dev xxx-- %lu %lu\n",address,((unsigned long)item));

         aet_print_tree(item);
         return item;
      }
   }
   return NULL_TREE;
}


void  new_strategy_init(NewStrategy *self)
{
     self->parser=aet_parser_get();
     if(!self->classInit)
         self->classInit=class_init_new();
     self->mtcsPlatformAndDevnum=n_ptr_array_new();

}
