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
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "c-family/c-common.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "c-family/c-pragma.h"
#include "c/c-parser.h"
#include "tree-inline.h"
#include "cfgloop.h"


#include "aetmediator.h"
#include "mtcs/mtcscompile.h"
#include "mtcsparser.h"

//生成语法树编译阶段完成 aetparser发消息给mtcscompile
//aet_mediator_create_compile 创建MtcsCompile对象。
void  aet_mediator_ast_end(AetMediator *self,nboolean haveMtcs,AetMediatorUser *send)
{
   if(!self->isCreateMtcsCompile){
      self->isCreateMtcsCompile = aet_mediator_create_compile();
   }
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send)
         self->users[i]->astEnd(self->users[i],haveMtcs);
   }
}

tree  aet_mediator_get_parent_device_array_decl(AetMediator *self,char *sysName,AetMediatorUser *send)
{
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send){
         tree  decl= self->users[i]->getParentDeviceArrayDecl(self->users[i],sysName);
         if(decl)
            return decl;
      }
   }
   return NULL_TREE;
}


//根据 decl 获取 decl 对应的 id 局部变量提升到全局后，在mtcsparser中生成一个id号，以便汇编是降级为局部变量
//decl 是主机的decl 存在 MtcsVarNode中。MtcsVarNode 存在 MtcsVar 的 varArray 中。
int   aet_mediator_get_promote_decl_id(AetMediator *self,tree decl,AetMediatorUser *send)
{
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send){
         int id = self->users[i]->getPromoteDeclId(self->users[i],decl);
         if(id>=0)
            return id;
      }
   }
   return -1;
}

//由mtcscompile发消息给mtcsparser
void  aet_mediator_add_link_func(AetMediator *self,const char *linkFuncNames,
      int version,int isa,const char *platName,AetMediatorUser *send)
{
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send){
         self->users[i]->addLinkFunc(self->users[i],linkFuncNames,version,isa,platName);
      }
   }
}

void   aet_mediator_write_note(AetMediator *self,AetMediatorUser *send)
{
   int i;
     for(i=0;i<self->userCount;i++){
        if(self->users[i]!=send){
           self->users[i]->writeNote(self->users[i]);
        }
     }
}


static void aetMediatorInit(AetMediator *self)
{
   self->userCount = 0;
}

void aet_mediator_add_user(AetMediator *self,AetMediatorUser *user)
{
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]==user)
         return;
   }
   self->users[self->userCount++]=user;
}

/**
 * 获取mtcs虚拟硬件架构。
 * 如果MtcsCompile还未创建，调用 aet_mediator_create_compile 创建
 */
char *aet_mediator_get_compute_version(AetMediator *self,char *platName,int version,int isa,AetMediatorUser *send)
{
   if(!self->isCreateMtcsCompile){
      self->isCreateMtcsCompile = aet_mediator_create_compile();
   }
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send)
        return self->users[i]->getComputeVersion(self->users[i],platName,version,isa);
   }
   return NULL;
}

/**
 * 返回汇编变量名
 */
char *aet_mediator_get_asm_var_name(AetMediator *self,char *platName,int version,int isa,char *fileName,AetMediatorUser *send)
{
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send)
        return self->users[i]->getAsmVarName(self->users[i],platName,version,isa,fileName);
   }
   return NULL;
}

//mtcs获取.o文件路径
char  *aet_mediator_get_object_file(AetMediator *self,AetMediatorUser *send)
{
   int i;
   for(i=0;i<self->userCount;i++){
      if(self->users[i]!=send)
        return self->users[i]->getObjectFile(self->users[i]);
   }
   return NULL;
}
/**
 * 获取AetMediator
 * 实现声明在aetmediator.h中
 */
AetMediator *aet_mediator_get()
{
   static AetMediator *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(AetMediator));
      aetMediatorInit(singleton);
   }
   return singleton;
}

