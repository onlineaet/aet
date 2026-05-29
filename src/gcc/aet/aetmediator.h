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

#ifndef __GCC_AET_MEDIATOR_H__
#define __GCC_AET_MEDIATOR_H__

#include "nlib.h"

/**
 * 使用中介者模式来解耦mtcsparser和mtcscompile
 */

typedef struct _AetMediator AetMediator;
typedef struct _AetMediatorUser AetMediatorUser;

struct _AetMediator{
   AetMediatorUser *users[10];
   int userCount;
   nboolean isCreateMtcsCompile;//是否创建了MtcsCompile对象
};


struct _AetMediatorUser{
   AetMediator *mediator;
   void     (*astEnd)    (AetMediatorUser *self,nboolean haveMtcs);   //语法分析树完成时通知mtcscompile
   /**
    * 获取声明_TSecond_parent__superDeviceAddressArray
    * 由于mtcsparser不能调用lookup_name,所以通过中介获得。
    */
   tree     (*getParentDeviceArrayDecl) (AetMediatorUser *self,char *sysName);
   int      (*getPromoteDeclId)(AetMediatorUser *self,tree decl);
   void     (*addLinkFunc)(AetMediatorUser *self,const char *linkFuncNames,int version,int isa,const char *platName);
   char    *(*getComputeVersion)(AetMediatorUser *self,char *platName,int version,int isa);
   char    *(*getAsmVarName)(AetMediatorUser *self,char *platName,int version,int isa,char *fileName);
   char    *(*getObjectFile)(AetMediatorUser *self);
   void     (*writeNote)(AetMediatorUser *self);

};

AetMediator *aet_mediator_get();
void         aet_mediator_add_user(AetMediator *self,AetMediatorUser *user);
void         aet_mediator_ast_end(AetMediator *self,nboolean haveMtcs,AetMediatorUser *send);
/**
 * 获取mtcs虚拟硬件架构。
 * 如果MtcsCompile还未创建，调用 aet_mediator_create_compile 创建
 */
char        *aet_mediator_get_compute_version(AetMediator *self,char *platName,int version,int isa,AetMediatorUser *send);
char        *aet_mediator_get_asm_var_name(AetMediator *self,char *platName,int version,int isa,char *fileName,AetMediatorUser *send);

//tree         aet_mediator_get_unit3_constructor(AetMediator *self,AetMediatorUser *send);
//根据 decl 获取 decl 对应的 id 局部变量提升到全局后，在mtcsparser中生成一个id号，以便汇编是降级为局部变量
//decl 是主机的decl 存在 MtcsVarNode中。MtcsVarNode 存在 MtcsVar 的 varArray 中。
int          aet_mediator_get_promote_decl_id(AetMediator *self,tree decl,AetMediatorUser *send);
tree         aet_mediator_get_parent_device_array_decl(AetMediator *self,char *sysName,AetMediatorUser *send);
void         aet_mediator_add_link_func(AetMediator *self,const char *linkFuncNames,
                     int version,int isa,const char *platName,AetMediatorUser *send);
//mtcs获取.o文件路径
char        *aet_mediator_get_object_file(AetMediator *self,AetMediatorUser *send);
nboolean     aet_mediator_create_compile();//由mtcscompile实现
void         aet_mediator_write_note(AetMediator *self,AetMediatorUser *send);

#endif /* ! __GCC_AET_MICRO_H__ */
