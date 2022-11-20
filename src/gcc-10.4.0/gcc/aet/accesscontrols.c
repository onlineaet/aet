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
#include "gcc-rich-location.h"
#include "opts.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "accesscontrols.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "classmgr.h"
#include "classbuild.h"
#include "classimpl.h"

#define CONTROL_VAR_AFTERFIX  "__access_controls_123"

typedef enum {
    NORMAL_VAR,
    STATIC_VAR,
    NORMAL_METHOD,
    STATIC_METHOD
}AccessType;

typedef struct _AccessCode{
    int serialNumber;//在class中的序号
    ClassPermissionType permission;
    AccessType type;//0 普通变量 1 静态变量 2 普通方法 3 静态方法
    char *name;
}AccessCode;

static void freeAccessCode_cb(AccessCode *item)
{
    n_free(item->name);
    n_slice_free(AccessCode,item);
}

static nint compareSafeCode_cb(nconstpointer  cand1,nconstpointer  cand2)
{
    AccessCode *p1 = *((AccessCode **) cand1);
    AccessCode *p2 = *((AccessCode **) cand2);
    int a=p1->serialNumber;
    int b=p2->serialNumber;
    nint r= (a > b ? +1 : a == b ? 0 : -1);
    return r;
}

static void accessControlsInit(AccessControls *self)
{
    self->accessArray = n_ptr_array_new ();
    self->implClassArray=n_ptr_array_new();//在一个文件中实现了几个类
}

static void printSafeCode(ClassName *className,NPtrArray *array)
{
    int i;
    NString *str=n_string_new("");
    for(i=0;i<array->len;i++){
        AccessCode *item=n_ptr_array_index(array,i);
        char *type="普通变量";
        if(item->type==STATIC_VAR)
            type="静态变量";
        else if(item->type==NORMAL_METHOD)
            type="普通方法";
        else if(item->type==STATIC_METHOD)
            type="静态方法";
        char *premission="缺省";
        if(item->permission==CLASS_PERMISSION_PUBLIC)
            premission="公共";
        else if(item->permission==CLASS_PERMISSION_PROTECTED)
            premission="保护";
        else if(item->permission==CLASS_PERMISSION_PRIVATE)
            premission="私有";
        n_string_append_printf(str,"%d\t%s\t%s\t%s\n",item->serialNumber,type,premission,item->name);
    }
    printf("类：%s 所有方法和变量如下\n",className->sysName);
    printf("序号\t类型\t权限\t名字\n");
    printf("%s\n",str->str);
}



static void setBit(unsigned char *data,int i)
{
    if (i == 0)
      data[0] = data[0] | 0x01;
    else if (i == 1)
      data[0] = data[0] | 0x02;
    else if (i == 2)
      data[0] = data[0] | 0x04;
    else if (i == 3)
      data[0] = data[0] | 0x08;
    else if (i == 4)
      data[0] = data[0] | 0x10;
    else if (i == 5)
      data[0] = data[0] | 0x20;
    else if (i == 6)
      data[0] = data[0] | 0x40;
    else if (i == 7)
      data[0] = data[0] | 0x80;
}

static void clearBit(unsigned char *data,int i)
{
    if (i == 0)
      data[0] = data[0] ^ (data[0] & 0x01);
    else if (i == 1)
      data[0] = data[0] ^ (data[0] & 0x02);
    else if (i == 2)
      data[0] = data[0] ^ (data[0] & 0x04);
    else if (i == 3)
      data[0] = data[0] ^ (data[0] & 0x08);
    else if (i == 4)
      data[0] = data[0] ^ (data[0] & 0x10);
    else if (i == 5)
      data[0] = data[0] ^ (data[0] & 0x20);
    else if (i == 6)
      data[0] = data[0] ^ (data[0] & 0x40);
    else if (i == 7)
      data[0] = data[0] ^ (data[0] & 0x80);
}

static void fill(unsigned char *data,ClassPermissionType type,int offset)
{
        if(type==CLASS_PERMISSION_PRIVATE){
           clearBit(data,offset);
           clearBit(data,offset-1);
        }else  if(type==CLASS_PERMISSION_PROTECTED){
           clearBit(data,offset);
           setBit(data,offset-1);
        }else  if(type==CLASS_PERMISSION_PROTECTED){
            setBit(data,offset);
            clearBit(data,offset-1);
        }else{
            //CLASS_PERMISSION_DEFAULT
            setBit(data,offset);
            setBit(data,offset-1);
        }

}

/**
 * 写入格式
 * 总数量(2个字节)+2位（类访问控制）+2位(方法或变量)+....+
 * private$   00
 * protected$ 01
 * public$    10
 * default    11
 *
 * typedef enum _ClassPermissionType
{
   CLASS_PERMISSION_PUBLIC,
   CLASS_PERMISSION_PROTECTED,
   CLASS_PERMISSION_PRIVATE,
   CLASS_PERMISSION_DEFAULT=-1,
} ClassPermissionType;
 */
static unsigned char *encode(ClassInfo *info,NPtrArray *array,int *length)
{
    int accessCount=1+array->len;
    int bytes=accessCount/4;
    if(bytes*4<accessCount)
        bytes+=1;
    unsigned char *data=xmalloc(2+bytes);
    short re=bytes;
    memcpy(data,&re,sizeof(short));
    int offset=2;
    int i;
    int pos=7;
    for(i=0;i<array->len+1;i++){
        if(i==0){
           fill(&data[offset],info->permission,pos);
        }else{
           AccessCode *item=n_ptr_array_index(array,i-1);
           fill(&data[offset],item->permission,pos);
        }
        pos-=2;
        if(i>0 && i%4==0){
            offset++;
            pos=7;
        }
    }
    *length=2+bytes;
    return data;
}

static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

static void createGlobalVar(char *varName,unsigned char *data,size_t length)
{
        location_t  loc = input_location;
        tree id=aet_utils_create_ident(varName);
        tree decl, type, init;
        type = build_array_type (unsigned_char_type_node,build_index_type (size_int (length)));
        //type = build_array_type (char_type_node,NULL_TREE);
        type = c_build_qualified_type (type, TYPE_QUAL_CONST);
        decl = build_decl (loc, VAR_DECL, id, type);
        DECL_EXTERNAL(decl)=0;
        TREE_PUBLIC(decl)=1;
        TREE_STATIC(decl)=1;
        decl=class_util_define_var_decl(decl,TRUE);
        init = build_string (length + 1, data);
        TREE_TYPE (init) = type;
        location_t init_loc=loc;
        warn_string_init (init_loc, TREE_TYPE (decl), init,STRING_CST);
        finish_decl (decl, init_loc, init,type, NULL_TREE);
}

/*
 * 写入访问控制符到全局变量
 * unsigned char  systemName_access_controls[10]={1,2,3}
 */
void access_controls_save_access_code(AccessControls *self,ClassName *className)
{

    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(class_info_is_interface(info))
        return;
    NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
    NPtrArray    *staticFuncArray=func_mgr_get_static_funcs(func_mgr_get(),className);
    NPtrArray    *varArray=var_mgr_get_vars(var_mgr_get(),className->sysName);
    NPtrArray    *retArray=n_ptr_array_new_with_free_func(freeAccessCode_cb);
    int i;
    if(funcArray!=NULL){
        for(i=0;i<funcArray->len;i++){
            ClassFunc *func=n_ptr_array_index(funcArray,i);
            AccessCode *code=(AccessCode *)n_slice_new(AccessCode);
            code->serialNumber=func->serialNumber;
            code->permission=func->permission;
            code->type=NORMAL_METHOD;
            code->name=n_strdup(func->mangleFunName);
            n_ptr_array_add(retArray,code);
        }
    }

    if(staticFuncArray!=NULL){
        for(i=0;i<staticFuncArray->len;i++){
           ClassFunc *func=n_ptr_array_index(staticFuncArray,i);
           AccessCode *code=(AccessCode *)n_slice_new(AccessCode);
           code->serialNumber=func->serialNumber;
           code->permission=func->permission;
           code->type=STATIC_METHOD;
           code->name=n_strdup(func->mangleFunName);
           n_ptr_array_add(retArray,code);
        }
    }

    if(varArray!=NULL){
         for(i=0;i<varArray->len;i++){
              VarEntity *var=n_ptr_array_index(varArray,i);
              AccessCode *code=(AccessCode *)n_slice_new(AccessCode);
              code->serialNumber=var->serialNumber;
              code->permission=var->permission;
              code->type=var->isStatic?STATIC_VAR:NORMAL_VAR;
              if(var->isStatic)
                  code->name=n_strdup_printf("%s(%s)",var->orgiName,var->mangleVarName);
              else
                  code->name=n_strdup(var->orgiName);
              n_ptr_array_add(retArray,code);
         }
     }
     n_ptr_array_sort(retArray,compareSafeCode_cb);
     if(n_log_is_debug())
        printSafeCode(className,retArray);
     int length=0;
     unsigned char *data=encode(info,retArray,&length);
     n_ptr_array_unref(retArray);
     char varName[255];
     sprintf(varName,"%s%s",info->className.sysName,CONTROL_VAR_AFTERFIX);
     createGlobalVar(varName,data,length);
     n_free(data);
}

/*---------------------------------------以下部分是访问控制----------------------------------*/

typedef enum{
    ACCESS_CLASS_FUNC,
    ACCESS_CLASS_VAR,
    ACCESS_CLASS_ENUM,
}AccessCtrlType;

typedef struct _AccessEnv
{
    nboolean isAet;
    ClassFunc *calleeFunc;//被调用的函数
    char *callSysName;//调用者所在的类
    char *calleeSysName;
    VarEntity *varEntity;
    location_t loc;
    AccessCtrlType accessType;
    char *atFuncName;//所在函数
    EnumData *enumData;
    char *enumVar;
    char *file;
}AccessEnv;

/**
 * 是否实现了sysName指定的类
 */
static nboolean exitsImpl(AccessControls *self,char *sysName)
{
   int i;
   for(i=0;i<self->implClassArray->len;i++){
       char *item=(char*)n_ptr_array_index(self->implClassArray,i);
       if(strcmp(item,sysName)==0)
           return TRUE;
   }
   return FALSE;
}


static void addCall(AccessControls *self,ClassFunc *calleeFunc,nboolean isAet,char *callSysName,location_t loc)
{
    AccessEnv *env=(AccessEnv *)n_slice_new(AccessEnv);
    char *currentFile=in_fnames[0];
    env->file=NULL;
    if(in_fnames[0])
        env->file=n_strdup(currentFile);
    env->atFuncName=NULL;
    if (aet_utils_valid_tree(current_function_decl)){
        char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
        env->atFuncName=n_strdup(funcName);
    }
    env->isAet=isAet;
    env->calleeFunc=calleeFunc;
    env->callSysName=n_strdup(callSysName);
    env->calleeSysName=NULL;
    env->varEntity=NULL;
    env->loc=loc;
    env->accessType=ACCESS_CLASS_FUNC;
    n_ptr_array_add(self->accessArray,env);
}

static void addVar(AccessControls *self,char  *calleeSystem,VarEntity *var,nboolean isAet,char *callSysName,location_t loc)
{
    AccessEnv *env=(AccessEnv *)n_slice_new(AccessEnv);
    char *currentFile=in_fnames[0];
    env->file=NULL;
    if(in_fnames[0])
        env->file=n_strdup(currentFile);
    env->atFuncName=NULL;
    if (aet_utils_valid_tree(current_function_decl)){
      char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
      env->atFuncName=n_strdup(funcName);
    }
    env->isAet=isAet;
    env->calleeFunc=NULL;
    env->callSysName=n_strdup(callSysName);
    env->calleeSysName=n_strdup(calleeSystem);
    env->varEntity=var;
    env->loc=loc;
    env->accessType=ACCESS_CLASS_VAR;
    n_ptr_array_add(self->accessArray,env);
}

static void addEnum(AccessControls *self,char  *calleeSystem,EnumData *enumData,char *enumVar,nboolean isAet,char *callSysName,location_t loc)
{
    AccessEnv *env=(AccessEnv *)n_slice_new(AccessEnv);
    char *currentFile=in_fnames[0];
    env->file=NULL;
    if(in_fnames[0])
        env->file=n_strdup(currentFile);
    env->atFuncName=NULL;
    if (aet_utils_valid_tree(current_function_decl)){
      char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
      env->atFuncName=n_strdup(funcName);
    }
    env->isAet=isAet;
    env->calleeFunc=NULL;
    env->callSysName=n_strdup(callSysName);
    env->calleeSysName=n_strdup(calleeSystem);
    env->varEntity=NULL;
    env->enumData=enumData;
    env->enumVar=n_strdup(enumVar);
    env->loc=loc;
    env->accessType=ACCESS_CLASS_ENUM;
    n_ptr_array_add(self->accessArray,env);
}

/**
 * 在由编译器创建的代码，如果访问了AClass类的方法和变量都要忽略
 * 比如 static  AClass *_createAClass_debug_AObject_123(debug_AObject *self)
 */
static nboolean filter(AccessControls *self)
{
    if (aet_utils_valid_tree(current_function_decl)){
        char *name=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
        int i;
        for(i=0;i<self->implClassArray->len;i++){
            char funcName[255];
            char *sysName=(char *)n_ptr_array_index(self->implClassArray,i);
            class_build_create_func_name_by_sys_name(sysName,funcName);
            if(!strcmp(funcName,name)){
              return TRUE;
            }
        }

    }
    return FALSE;
}

static char *getPackageFromClassImpl(AccessControls *self)
{
        int i;
        for(i=0;i<self->implClassArray->len;i++){
            char *sysName=(char *)n_ptr_array_index(self->implClassArray,i);
            ClassName *call=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
            if(call->package!=NULL)
                return call->package;
        }
        return NULL;
}



static nboolean isSamePackage(char *callee,char *call)
{
    if(callee==NULL || call==NULL )
       return FALSE;
    return strcmp(callee,call)==0;
}

static void checkMethod(AccessControls *self,AccessEnv *env)
{
       char *calleeFuncSysName=IDENTIFIER_POINTER(TYPE_NAME(env->calleeFunc->classTree));
       calleeFuncSysName=calleeFuncSysName+1;//去除下划线 _debug_ARandom
       if(exitsImpl(self,calleeFuncSysName)){
           //printf("在:%s中调用类:%s的方法:%s，并且调用类和被调用类的实现都在同一个文件中。",
                  // env->callSysName==NULL?"在文件":env->callSysName,calleeFuncSysName,env->calleeFunc->orgiName);
           return;
       }
       //检查类关系
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),calleeFuncSysName);
       if(info->permission==CLASS_PERMISSION_PRIVATE){ //如果被调用的类是private,判断是不是在一个文件内实现
          error_at(env->loc,"调用的类%qs是私有的，不能访问它的方法%qs",calleeFuncSysName,env->calleeFunc->orgiName);
          return;
       }else if(info->permission==CLASS_PERMISSION_PROTECTED || info->permission==CLASS_PERMISSION_DEFAULT){
           if(env->isAet){
               ClassName *callee=&info->className;//class_mgr_get_class_name_by_sys(class_mgr_get(),calleeFuncSysName);
               ClassName *call=class_mgr_get_class_name_by_sys(class_mgr_get(),env->callSysName);
               nboolean samePackage=isSamePackage(callee->package,call->package);
               ClassRelationship ship=   class_mgr_relationship(class_mgr_get(), env->callSysName,calleeFuncSysName);
               if(!samePackage && ship!=CLASS_RELATIONSHIP_PARENT){
                   if(!samePackage)
                      error_at(env->loc,"类%qs与调用类%qs不在同一个包内。",env->callSysName,calleeFuncSysName);
                   if(ship!=CLASS_RELATIONSHIP_PARENT)
                       error_at(env->loc,"类%qs与调用类%qs不是父子关系。",env->callSysName,calleeFuncSysName);
                   return;
               }
           }else{
               error_at(env->loc,"不能引用保护类型的类%qs。",calleeFuncSysName);
               return;
           }
       }
       //检查方法
       if(env->calleeFunc->permission==CLASS_PERMISSION_PUBLIC){
           return;
       }
       if(env->calleeFunc->permission==CLASS_PERMISSION_PRIVATE){
           error_at(env->loc,"在%qs中调用类%qs的私有方法%qs被禁止。",
                   env->callSysName==NULL?"在文件":env->callSysName,calleeFuncSysName,env->calleeFunc->orgiName);
       }
       if(env->isAet){
           //如果调用类与被调用类在同一个包或是继承关系可以调用。
           ClassName *callee=&info->className;
           ClassInfo *callInfo=class_mgr_get_class_info(class_mgr_get(),env->callSysName);
           ClassName *call=&callInfo->className;
           char *callPackage=call->package;
           //如果调用类没有包名，但类实现现其它类实现在一起，并且其中某个类实现声明有包名，则调用类使用这个包名
           if(callPackage==NULL)
               callPackage= getPackageFromClassImpl(self);
           if(isSamePackage(callee->package,callPackage)) //同一个包内
               return;
           ClassRelationship ship=   class_mgr_relationship(class_mgr_get(), env->callSysName,calleeFuncSysName);
           if(ship==CLASS_RELATIONSHIP_CHILD)
               return;
            error_at(env->loc,"在类%qs中调用类%qs的保护或缺省的方法%qs被禁止。因为两个类并不在同一个包内，调用类与被调用类也没有继承关系。",
                    env->callSysName,calleeFuncSysName,env->calleeFunc->orgiName);
       }else{
          //当前文件
           error_at(env->loc,"在文件中不能访问类%qs的保护或缺省的方法%qs。",calleeFuncSysName,env->calleeFunc->orgiName);
       }

}

static void checkVar(AccessControls *self,AccessEnv *env)
{
       char *calleeSysName=env->calleeSysName;
       if(exitsImpl(self,calleeSysName)){
           //printf("在:%s中调用类:%s的变量:%s，并且调用类和被调用类的实现都在同一个文件中。",
                 //  env->callSysName==NULL?"在文件":env->callSysName,calleeSysName,env->varEntity->orgiName);
           return;
       }
       //检查类关系
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),calleeSysName);
       if(!strcmp(env->varEntity->orgiName,SUPER_FUNC_ADDR_VAR_NAME) && !strcmp(info->className.userName,AET_ROOT_OBJECT))
           return;

       if(info->permission==CLASS_PERMISSION_PRIVATE){ //如果被调用的类是private,判断是不是在一个文件内实现
          error_at(env->loc,"调用的类%qs是私有的，不能访问它的变量%qs。",calleeSysName,env->varEntity->orgiName);
          return;
       }else if(info->permission==CLASS_PERMISSION_PROTECTED || info->permission==CLASS_PERMISSION_DEFAULT){
           if(env->isAet){
               ClassName *callee=&info->className;//class_mgr_get_class_name_by_sys(class_mgr_get(),calleeFuncSysName);
               ClassName *call=class_mgr_get_class_name_by_sys(class_mgr_get(),env->callSysName);
               nboolean samePackage=isSamePackage(callee->package,call->package);
               ClassRelationship ship=   class_mgr_relationship(class_mgr_get(), env->callSysName,calleeSysName);
               if(!samePackage && ship!=CLASS_RELATIONSHIP_PARENT){
                   if(!samePackage)
                      error_at(env->loc,"类%qs与调用类%qs不在同一个包内。",env->callSysName,calleeSysName);
                   if(ship!=CLASS_RELATIONSHIP_PARENT)
                       error_at(env->loc,"类%qs与调用类%qs不是父子关系。",env->callSysName,calleeSysName);
                   return;
               }
           }else{
               error_at(env->loc,"不能引用保护类型的类%qs。",calleeSysName);
               return;
           }
       }
       //检查变量
       if(env->varEntity->permission==CLASS_PERMISSION_PUBLIC){
           return;
       }
       if(env->varEntity->permission==CLASS_PERMISSION_PRIVATE){
           if(info->file && env->atFuncName!=NULL){
               tree funcNameId=aet_utils_create_ident(env->atFuncName);
               tree decl=lookup_name(funcNameId);
               if(aet_utils_valid_tree(decl)){
                   expanded_location xloc = expand_location (DECL_SOURCE_LOCATION (decl));
                   if (xloc.file){
                      const char *filename = lbasename (xloc.file);
                      NFile *nfile=n_file_new(info->file);
                      char *f=n_file_get_name(nfile);
                      if(strcmp(f,filename)==0){
                          n_file_unref(nfile);
                          return ;
                      }
                      n_file_unref(nfile);
                   }
               }
           }
           error_at(env->loc,"在%qs中调用类%qs的私有变量%qs被禁止。所在函数：%qs",
                   env->callSysName==NULL?"在文件":env->callSysName,calleeSysName,env->varEntity->orgiName,env->atFuncName);
           return;
       }
       if(env->isAet){
           //如果调用类与被调用类在同一个包或是继承关系可以调用。
           ClassName *callee=&info->className;
           ClassInfo *callInfo=class_mgr_get_class_info(class_mgr_get(),env->callSysName);
           ClassName *call=&callInfo->className;
           char *callPackage=call->package;
           //如果调用类没有包名，但类实现现其它类实现在一起，并且其中某个类实现声明有包名，则调用类使用这个包名
           if(callPackage==NULL)
               callPackage= getPackageFromClassImpl(self);
           if(isSamePackage(callee->package,callPackage)) //同一个包内
               return;
           ClassRelationship ship=   class_mgr_relationship(class_mgr_get(), env->callSysName,calleeSysName);
           if(ship==CLASS_RELATIONSHIP_CHILD)
               return;
            error_at(env->loc,"在类%qs中调用类%qs的保护或缺省的变量%qs被禁止。因为两个类并不在同一个包内，调用类与被调用类也没有继承关系。",
                    env->callSysName,calleeSysName,env->varEntity->orgiName);
       }else{
           //当前文件有实现了与被调用类相同的包的类。
           char *callPackage= getPackageFromClassImpl(self);
           if(callPackage!=NULL){
               if(isSamePackage(info->className.package,callPackage)) //同一个包内
                  return;
           }
           const char *file = LOCATION_FILE (env->loc);
           if(file!=NULL){
               if(class_info_is_decl_file(info,file))
                   return;
           }
           error_at(env->loc,"在文件中不能访问类%qs的保护或缺省的变量%qs。",calleeSysName,env->varEntity->orgiName);
       }
}

static void checkEnum(AccessControls *self,AccessEnv *env)
{
       char *calleeSysName=env->calleeSysName;
       if(exitsImpl(self,calleeSysName)){
          // printf("在:%s中调用类:%s的枚举:%s，并且调用类和被调用类的实现都在同一个文件中。",
                 //  env->callSysName==NULL?"在文件":env->callSysName,calleeSysName,env->enumData->origName);
           return;
       }
       //检查类关系
       ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),calleeSysName);
       if(info->permission==CLASS_PERMISSION_PRIVATE){ //如果被调用的类是private,判断是不是在一个文件内实现
          error_at(env->loc,"被调用类%qs是私有的，不能访问它的枚举%qs的元素%qs。",calleeSysName,env->enumData->origName,env->enumVar);
          return;
       }else if(info->permission==CLASS_PERMISSION_PROTECTED || info->permission==CLASS_PERMISSION_DEFAULT){
           if(env->isAet){
               ClassName *callee=&info->className;//class_mgr_get_class_name_by_sys(class_mgr_get(),calleeFuncSysName);
               ClassName *call=class_mgr_get_class_name_by_sys(class_mgr_get(),env->callSysName);
               nboolean samePackage=isSamePackage(callee->package,call->package);
               ClassRelationship ship=   class_mgr_relationship(class_mgr_get(), env->callSysName,calleeSysName);
               if(!samePackage && ship!=CLASS_RELATIONSHIP_PARENT){
                   if(!samePackage)
                      error_at(env->loc,"类%qs与被调用类%qs不在同一个包内。",env->callSysName,calleeSysName);
                   if(ship!=CLASS_RELATIONSHIP_PARENT)
                       error_at(env->loc,"类%qs与被调用类%qs不是父子关系。",env->callSysName,calleeSysName);
                   return;
               }
           }else{
               error_at(env->loc,"不能引用保护类型的类%qs。",calleeSysName);
               return;
           }
       }
       //检查枚举
      // printf("enum is -sss--- %s %s %d %s\n",env->enumData->sysName,env->enumData->origName,env->enumData->permission,env->enumVar);
       if(env->enumData->permission==CLASS_PERMISSION_PUBLIC){
           return;
       }
       if(env->enumData->permission==CLASS_PERMISSION_PRIVATE){
           error_at(env->loc,"在%qs中调用类%qs的私有枚举%qs被禁止。所在函数：%qs",
                   env->callSysName==NULL?"在文件":env->callSysName,calleeSysName,env->enumData->origName,env->atFuncName);
           return;
       }
       if(env->isAet){
           //如果调用类与被调用类在同一个包或是继承关系可以调用。
           ClassName *callee=&info->className;
           ClassInfo *callInfo=class_mgr_get_class_info(class_mgr_get(),env->callSysName);
           ClassName *call=&callInfo->className;
           char *callPackage=call->package;
           //如果调用类没有包名，但类实现现其它类实现在一起，并且其中某个类实现声明有包名，则调用类使用这个包名
           if(callPackage==NULL)
               callPackage= getPackageFromClassImpl(self);
           if(isSamePackage(callee->package,callPackage)) //同一个包内
               return;
           ClassRelationship ship=   class_mgr_relationship(class_mgr_get(), env->callSysName,calleeSysName);
           if(ship==CLASS_RELATIONSHIP_CHILD)
               return;
            error_at(env->loc,"在类%qs中调用类%qs的保护或缺省的枚举%qs被禁止。因为两个类并不在同一个包内，调用类与被调用类也没有继承关系。",
                    env->callSysName,calleeSysName,env->enumData->origName);
       }else{
           //当前文件有实现了与被调用类相同的包的类。
           char *callPackage= getPackageFromClassImpl(self);
           if(callPackage!=NULL){
               if(isSamePackage(info->className.package,callPackage)) //同一个包内
                  return;
           }
           const char *file = LOCATION_FILE (env->loc);
           if(file!=NULL){
               if(class_info_is_decl_file(info,file))
                   return;
           }
           error_at(env->loc,"在文件中不能访问类%qs的保护或缺省的枚举%qs。",calleeSysName,env->enumData->origName);
       }
}

/**
 * 有些访问只有到编译完文件后才能判断
 */
nboolean access_controls_access_method(AccessControls *self,location_t loc,ClassFunc *func)
{
    //如果调用是在类中，但是该类
    //第一种：公共方法，返回TRUE
    c_parser *parser=self->parser;
    n_debug("访问类方法---xxx- %s permission:%d parser->isAet:%d\n",func->mangleFunName,func->permission,parser->isAet);
    if(!aet_utils_valid_tree(func->classTree)){
        n_error("在访问控制时出错了,调用的函数:%s,没有类信息",func->mangleFunName);
        return FALSE;
    }
    char *calleeSysName=IDENTIFIER_POINTER(TYPE_NAME(func->classTree));// class_util_get_class_name(func->classTree);
    if(calleeSysName==NULL){
       aet_print_tree_skip_debug(func->classTree);
       n_error("在访问控制时出错了,调用的函数:%s,没有类信息",func->mangleFunName);
       return FALSE;
    }
    if(!aet_utils_valid_tree(func->fieldDecl)){
        //类中的私有方法。不可能访问到。
        n_debug("访问类方法---11- %s permission:%d parser->isAet:%d\n",func->mangleFunName,func->permission,parser->isAet);
        return TRUE;
    }
    calleeSysName=calleeSysName+1;//去除下划线 _debug_ARandom
    if(parser->isAet){
        ClassName *implClassName=class_impl_get_class_name(class_impl_get());
        char *sysName=implClassName->sysName;
        if(!strcmp(sysName,calleeSysName)){
            //1.在类实现中，调用的是本类的方法，不需要判断权限
            n_debug("访问类方法---22- %s permission:%d parser->isAet:%d\n",func->mangleFunName,func->permission,parser->isAet);
            return TRUE;
        }else{
            //2.在类实现中，调用的是其它类的方法，保存调用信息，当整个文件编译完后再判断权限
            n_debug("访问类方法---33- %s permission:%d parser->isAet:%d\n",func->mangleFunName,func->permission,parser->isAet);

            addCall(self,func,TRUE,sysName,loc);
        }
    }else{
        if(filter(self)) //过滤编译器内部创建的初始化代码
           return TRUE;
        n_debug("访问类方法---44- %s permission:%d parser->isAet:%d\n",func->mangleFunName,func->permission,parser->isAet);

        addCall(self,func,FALSE,NULL,loc);
    }
    return TRUE;
}

/**
 * 访问变量
 */
nboolean access_controls_access_var(AccessControls *self,location_t loc,char *varName,char *calleeSysName)
{
    //如果调用是在类中，但是该类
    //第一种：公共方法，返回TRUE
    c_parser *parser=self->parser;
    VarEntity *entity=var_mgr_get_var(var_mgr_get(),calleeSysName,varName);
    if(entity==NULL){
        //printf("access_controls_access_var ---找不到变量,可能是一个方法赋值。 %s %s\n",varName,calleeSysName);
        return TRUE;
    }
    if(aet_utils_compile_additional_code_status()){ //正在编译附加代码
       n_debug("访问的变量属于附加代码:%s %s\n",varName,calleeSysName);
       return TRUE;
    }
    char *atFuncName=NULL;
    if (aet_utils_valid_tree(current_function_decl)){
        atFuncName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
    }
    //printf("访问类变量---- var:%s calleeSysName:%s permission:%d parser->isAet:%d 所在函数:%s\n",
           // varName,calleeSysName,entity->permission,parser->isAet,atFuncName);
    if(parser->isAet){
        ClassName *implClassName=class_impl_get_class_name(class_impl_get());
        char *sysName=implClassName->sysName;
        if(!strcmp(sysName,calleeSysName)){
            //1.在类实现中，调用的是本类的方法，不需要判断权限
            return TRUE;
        }else{
            //2.在类实现中，调用的是其它类的方法，保存调用信息，当整个文件编译完后再判断权限
            addVar(self,calleeSysName,entity,TRUE,sysName,loc);
        }
    }else{
        if(filter(self)) //过滤编译器内部创建的初始化代码
           return TRUE;
        addVar(self,calleeSysName,entity,FALSE,NULL,loc);
    }
    return TRUE;
}

/**
 * 访问枚举 不用关心具体的枚举值
 */
nboolean access_controls_access_enum(AccessControls *self,location_t loc,EnumData *enumData,char *enumVar)
{
    //如果调用是在类中，但是该类
    //第一种：公共方法，返回TRUE
    c_parser *parser=self->parser;
    if(enumData==NULL){
        return TRUE;
    }
    if(aet_utils_compile_additional_code_status()){ //正在编译附加代码
       return TRUE;
    }
    nboolean atClass= class_mgr_is_class(class_mgr_get(),enumData->sysName);
    if(!atClass) //在文件中定义的枚举是公共的，所以直接返回
        return TRUE;
    char *calleeSysName=enumData->sysName;//等同于变量访问
    char *atFuncName=NULL;//调试用
    if (aet_utils_valid_tree(current_function_decl)){
        atFuncName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
    }
    //printf("访问枚举---- sysName:%s permission:%d parser->isAet:%d 所在函数:%s\n",
           // enumData->sysName,enumData->permission,parser->isAet,atFuncName);
    if(parser->isAet){
           ClassName *implClassName=class_impl_get_class_name(class_impl_get());
           char *sysName=implClassName->sysName;
           if(!strcmp(sysName,calleeSysName)){
               //1.在类实现中，调用的是本类的枚举，不需要判断权限
               return TRUE;
           }else{
               //2.在类实现中，调用的是其它类的方法，保存调用信息，当整个文件编译完后再判断权限
               addEnum(self,calleeSysName,enumData,enumVar,TRUE,sysName,loc);
           }
    }else{
           if(filter(self)) //过滤编译器内部创建的初始化代码
              return TRUE;
           addEnum(self,calleeSysName,enumData,enumVar,FALSE,NULL,loc);
    }
    return TRUE;
}


void access_controls_check(AccessControls *self)
{
   int i;
   for(i=0;i<self->accessArray->len;i++){
       AccessEnv *item=(AccessEnv*)n_ptr_array_index(self->accessArray,i);
       if(item->accessType==ACCESS_CLASS_FUNC)
           checkMethod(self,item);
       else  if(item->accessType==ACCESS_CLASS_VAR)
           checkVar(self,item);
       else
           checkEnum(self,item);
   }
}



/**
 * 加入在当前编译文件中实现的类的名称到AccessControls中.
 * 第二次编译接口时，也有接口的初始化方法和AClass的实现，也需要加入到AccessControls中
 */
void  access_controls_add_impl(AccessControls *self,char *sysName)
{
    if(exitsImpl(self,sysName))
        return;
    n_ptr_array_add(self->implClassArray,n_strdup(sysName));
}


void access_controls_set_parser(AccessControls *self,c_parser *parser)
{
   self->parser=parser;
}

AccessControls *access_controls_get()
{
    static AccessControls *singleton = NULL;
    if (!singleton){
         singleton =(AccessControls *)n_slice_alloc0 (sizeof(AccessControls));
         accessControlsInit(singleton);
    }
    return singleton;
}





