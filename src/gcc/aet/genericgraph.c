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
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "opts.h"

#include "aet-c-parser-header.h"

#include "c-aet.h"
#include "aetutils.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "classimpl.h"
#include "blockmgr.h"
#include "funcmgr.h"
#include "makefileparm.h"
#include "aetlib.h"
#include "middlefile.h"
#include "genericimpl.h"

#include "genericgraph.h"

/**
 * 泛型可达性算法
 * 1.输入两个对象:a.新建对象,b.调用泛型函数
 * 2.新建对象和调用泛型的过程中，创建对象RunGenericInfo,然后用RunGenericInfo创建GenericObj
 * 3.在RunGenericInfo 中有对象的引用关系。算法依赖对象(GenericObj)和引用（RunGenericInfo）。
 * 4.泛型完全定义的对象作为root,通过 RunGenericInfo 建立GenericObj之间的关系
 */
static void genericGraphInit(GenericGraph *self)
{
   self->collectGenArray=n_ptr_array_new();
   self->saveContent=NULL;
   self->outputArray=NULL;
}

/**
 * GenericObj 中的泛型单元全是定义的
 */
static nboolean fullDefine(GenericObj *obj)
{
   int i;
   for(i=0;i<obj->infoLen;i++)
      if(generic_unit_is_undefine(obj->infos[i]->genUnit))
         return FALSE;

   return TRUE;
}

/**
 * 调用泛型函数
 * atFunc  调用泛型函数所在的函数。如果不在类函数中调用，atFunc是空的
 * atClass 调用泛型函数时所在的类函数的self参数的。可能是是泛型类也可能不是。
 * atFun所属的类，应该就是self。用assert确定。
 * 加入在当前编译文件中的泛型函数调用
 */
void generic_graph_add_func_call(GenericGraph *self,RunGenericInfo **infos,
      ClassFunc *callee,ClassFunc* atFunc,ClassInfo *atClass)
{
   if(atFunc!=NULL && atClass!=NULL)
      gcc_assert(strcmp(atFunc->className->sysName,atClass->className.sysName)==0);
   GenericObj *obj=n_slice_new0(GenericObj);
   obj->type=GEN_FUNC;
   obj->infos=infos;
   obj->infoLen=generic_model_get_count(class_func_get_func_generic(callee));
   obj->callee=callee;
   obj->atFunc=atFunc;
   obj->atClass=atClass;
   obj->origModel= class_func_get_func_generic(callee);//泛型函数声明的泛型单元
   ClassInfo *calleeClass=class_mgr_get_class_info_by_class_name(class_mgr_get(),callee->className);
   if(calleeClass && calleeClass->file)
      obj->declClassFile=n_strdup(calleeClass->file);
   else
      obj->declClassFile="";
   n_ptr_array_add(self->collectGenArray,obj);
}

/**
 * 加入新建泛型类的信息到 collectGenArray
 */
void generic_graph_add_new_class(GenericGraph *self,RunGenericInfo **infos,
      ClassInfo *newObject,ClassFunc* atFunc,ClassInfo *atClass,nboolean isParent)
{
   if(atFunc!=NULL)
      gcc_assert(strcmp(atFunc->className->sysName,atClass->className.sysName)==0);
   GenericObj *obj=n_slice_new0(GenericObj);
   obj->type=isParent?PARENT_FROM_OBJECT:NEW_OBJECT;
   obj->infos=infos;
   obj->infoLen=generic_model_get_count(newObject->genericModel);
   obj->newObject=newObject;
   obj->atFunc=atFunc;
   obj->atClass=atClass;
   obj->origModel=class_info_get_generic_model(newObject);
   if(newObject->file)
      obj->declClassFile=n_strdup(newObject->file);
   else
      obj->declClassFile="";
   n_ptr_array_add(self->collectGenArray,obj);
}

static void printRunGenInfo(RunGenericInfo *info,int index)
{
   char *from="本身";
   char *detail="";
   if(info->from==UNIT_FROM_NEW_OBJECT){
      from="本身";
   }else if(info->from==UNIT_FROM_GENERIC_FUNC){
      from="泛型函数";
      detail=info->fromFunction->mangleFunName;
   }else if(info->from==UNIT_FROM_SELF_PARM){
      from="self参数";
      detail=info->fromClass->className.sysName;
   }else if(info->from==UNIT_FROM_CHILD){
      from="子类";
      detail=info->fromClass->className.sysName;
   }
   printf("序号:%d 位置:%d %s  %s unit:%s\n",index,info->fromPos,from,detail,generic_unit_tostring(info->genUnit));
}

static void printGenericObj(GenericObj *info)
{
   if(info->type==NEW_OBJECT){
      printf("新建对象:%s\n",info->newObject->className.sysName);
   }else if(info->type==GEN_FUNC){
      printf("调用泛型函数:%s\n",info->callee->mangleFunName);
   }else if(info->type==PARENT_FROM_OBJECT){
      printf("新建对象时对父类泛型的设置:%s\n",info->newObject->className.sysName);
   }
   if(info->type==NEW_OBJECT || info->type==GEN_FUNC){
      if(info->atFunc)
         printf("在函数中创建的对象:%s\n",info->atFunc->mangleFunName);
      if(info->atClass)
         printf("在类实现中创建的对象:%s\n",info->atClass->className.sysName);
      if(info->atFunc==NULL && info->atClass==NULL)
         printf("在外部创建的对象\n");
   }
   int i;
   for(i=0;i<info->infoLen;i++){
      RunGenericInfo *item=info->infos[i];
      printRunGenInfo(item,i);
   }
}

static void printRunGenInfo(RunGenericInfo *info,int index,char *tabs)
{
   char *from="本身";
   char *detail="";
   if(info->from==UNIT_FROM_NEW_OBJECT){
      from="本身";
   }else if(info->from==UNIT_FROM_GENERIC_FUNC){
      from="泛型函数";
      detail=info->fromFunction->mangleFunName;
   }else if(info->from==UNIT_FROM_SELF_PARM){
      from="self参数";
      detail=info->fromClass->className.sysName;
   }else if(info->from==UNIT_FROM_CHILD){
      from="子类";
      detail=info->fromClass->className.sysName;
   }
   printf("%s序号:%d 位置:%d %s  %s unit:%s file:%s\n",
         tabs,index,info->fromPos,from,detail,generic_unit_tostring(info->genUnit),info->file);
}

static void printGenericObj(GenericObj *info,int tab)
{
   NString *tabstr=n_string_new("");
   int i;
   for(i=0;i<tab;i++)
      n_string_append(tabstr,"\t");
   char *tabs=tabstr->str;
   if(info->type==NEW_OBJECT){
      printf("%s新建对象:%s\n",tabs,info->newObject->className.sysName);
   }else if(info->type==GEN_FUNC){
      printf("%s调用泛型函数:%s\n",tabs,info->callee->mangleFunName);
   }else if(info->type==PARENT_FROM_OBJECT){
      printf("%s新建对象时对父类泛型的设置:%s\n",tabs,info->newObject->className.sysName);
   }
   if(info->type==NEW_OBJECT || info->type==GEN_FUNC){
      if(info->atFunc)
         printf("%s在函数中创建的对象:%s\n",tabs,info->atFunc->mangleFunName);
      if(info->atClass)
         printf("%s在类实现中创建的对象:%s\n",tabs,info->atClass->className.sysName);
      if(info->atFunc==NULL && info->atClass==NULL)
         printf("%s在外部创建的对象\n",tabs);
   }
   for(i=0;i<info->infoLen;i++){
      RunGenericInfo *item=info->infos[i];
      printRunGenInfo(item,i,tabs);
   }
}

static void printSimpleGenericObj(GenericObj *info)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   NString *tabstr=n_string_new("");
   int i;
   if(info->type==GEN_FUNC){
      printf("调用泛型函数:%s\n",info->callee->mangleFunName);
   }else{
      printf("新建对象:%s\n",info->newObject->className.sysName);
   }
   for(i=0;i<info->infoLen;i++){
      RunGenericInfo *item=info->infos[i];
      printf("泛型单元:%d %s\n",i,generic_unit_tostring(item->genUnit));
   }
}

static void printData(NPtrArray *array)
{
   int len=array->len;
   if(len==0)
      printf("没有泛型对象\n");
   int i;
   for(i=0;i<len;i++){
      GenericObj *item=n_ptr_array_index(array,i);
      printf("%d: 使用泛型对象或泛型函数信息:\n",i);
      printGenericObj(item);
      printf("\n");
   }
}

void generic_graph_print(GenericGraph *self)
{
    printData(self->collectGenArray);
}

void generic_obj_print(GenericObj *self)
{
   printGenericObj(self);
}

/**
 * 比较两个ClassFunc是否相同，mangleFunName名字相同即可
 */
static nboolean equalClassFunc(ClassFunc *f1,ClassFunc *f2)
{
   return (f1!=NULL && f2!=NULL && !strcmp(f1->mangleFunName,f2->mangleFunName));
}

static nboolean equalClassInfo(ClassInfo *c1,ClassInfo *c2)
{
   return (c1!=NULL && c2!=NULL && !strcmp(c1->className.sysName,c2->className.sysName));
}

/**
 * 比较两个泛型单元是否相同
 */
static nboolean compareUnit(RunGenericInfo **r1,RunGenericInfo **r2,int infoLen,nboolean define)
{
   int i;
   for(i=0;i<infoLen;i++){
      RunGenericInfo *a=r1[i];
      RunGenericInfo *b=r2[i];
     // printf("compareUnit ----- %s %s %d %d\n",generic_unit_tostring(a->genUnit),generic_unit_tostring(b->genUnit),generic_unit_equal(a->genUnit,b->genUnit),define);
      if(!generic_unit_equal(a->genUnit,b->genUnit))
         return FALSE;
      if(define)
         continue;
      else{
         if(a->genUnit->isDefine
             || (equalClassFunc(a->fromFunction,b->fromFunction) || equalClassInfo(a->fromClass,b->fromClass)))
            continue;
         else
            return FALSE;
      }
   }
   return TRUE;
}

/**
 * 判断两个GenericObj是否相同。
 * define =true 采用定义比较方法 compareDefineUnit 否则用 compareUndefineUnit
 */
static nboolean objectEqual(GenericObj *c1,GenericObj *c2,nboolean define)
{
   if((c1->type==GEN_FUNC && c2->type!=GEN_FUNC) || (c1->type!=GEN_FUNC && c2->type==GEN_FUNC))
      return FALSE;
   if(c1->infoLen!=c2->infoLen)
      return FALSE;
   if(c1->type==GEN_FUNC && c2->type==GEN_FUNC){
      if(equalClassFunc(c1->callee,c2->callee)){ //是泛型函数
         if(compareUnit(c1->infos,c2->infos,c1->infoLen,define))
            return TRUE;
      }
   }else{
     // printf("objectEqual xxx-- %s %s\n",c1->newObject->className.sysName,c2->newObject->className.sysName);

      if(equalClassInfo(c1->newObject,c2->newObject)){ //是泛型父类,不区分新建或新建的父类
         //printf("objectEqual xxxyyyy-- %s %s\n",c1->newObject->className.sysName,c2->newObject->className.sysName);

         if(compareUnit(c1->infos,c2->infos,c1->infoLen,define))
            return TRUE;
      }
   }
   return FALSE;
}


/**
 * 在NPtrArray是否已存在GenericObj obj
 */
static nboolean exists(NPtrArray *newArray,GenericObj *obj,nboolean define)
{
    int i;
    for(i=0;i<newArray->len;i++){
       GenericObj *item=n_ptr_array_index(newArray,i);
       if(obj==item)
          return TRUE;
       if(objectEqual(obj,item,define))
          return TRUE;
    }
    return FALSE;
}

/**
 * 移走全定义相同的泛型对象。
 */
static void removeRepeat(NPtrArray *defineArray,nboolean define)
{
   int old=defineArray->len;
   if(old<=1)
      return;
   NPtrArray *newArray=n_ptr_array_new();
   int i,j;
   for(i=0;i<defineArray->len;i++){
      GenericObj *item=n_ptr_array_index(defineArray,i);
      if(!exists(newArray,item,define)){
         n_ptr_array_add(newArray,item);
      }
   }
   n_ptr_array_remove_range(defineArray,0,defineArray->len);
   for(i=0;i<newArray->len;i++)
      n_ptr_array_add(defineArray,n_ptr_array_index(newArray,i));
   n_ptr_array_unref(newArray);
}


typedef struct _GenericNode GenericNode;
struct _GenericNode
{
   GenericNode *parent;
   GenericNode *childs[150];
   int childCount;
   GenericObj *obj;
   //obj的第几个单元是确定的
   GenericObj *from;
};

static RunGenericInfo *cloneRunGenericInfo(RunGenericInfo *info)
{
   RunGenericInfo *n=n_slice_new0(RunGenericInfo);
   n->genUnit=generic_unit_clone(info->genUnit);
   n->fromFunction=info->fromFunction;
   n->fromClass=info->fromClass;
   n->fromPos=info->fromPos;
   n->from=info->from;
   return n;
}

static GenericObj *cloneGenericObj(GenericObj *src)
{
   GenericObj *n=n_slice_new0(GenericObj);
   n->type=src->type;
   int i=0;
   RunGenericInfo **infos=(RunGenericInfo **)xmalloc(src->infoLen*sizeof(unsigned long)) ;
   for(i=0;i<src->infoLen;i++){
      infos[i]=cloneRunGenericInfo(src->infos[i]);
   }
   n->infos=infos;
   n->infoLen=src->infoLen;
   n->callee=src->callee;
   n->newObject=src->newObject;
   n->atFunc=src->atFunc;
   n->atClass=src->atClass;
   n->origModel=src->origModel;
   n->declClassFile=n_strdup(src->declClassFile);
   return n;
}

/**
 * 生成对象树，关键是通过泛型单元来自的地方。
 */
static void reference(GenericNode *rootNode,NPtrArray *array)
{
   int i,j;
   for(i=0;i<array->len;i++){
      GenericObj *item=n_ptr_array_index(array,i);
      if(rootNode->from==item || item->ref){
        // printf("reference 重复加入 ---parent:rootNode:%p 是否引用过:%d item:%p rootNode->from==item:%d\n",
             //  rootNode,item->ref,item,rootNode->from==item);
//         printGenericObj(rootNode->obj);
//         printf("重复加入 ---child:\n");
//         printGenericObj(item);
         continue;
      }
      RunGenericInfo *info;
      GenericNode *childNode=NULL;
      for(j=0;j<item->infoLen;j++){
         RunGenericInfo *info=item->infos[j];
         if(!info->genUnit->isDefine){ //子泛型对象有非定义泛型单元，并且来自root
//            printf("item is ----i:%d rootNode:%p\n",i,rootNode);
//            generic_obj_print(item);
//            printf("item is ---xx-i:%d rootNode:%p\n",i,rootNode);
//            generic_obj_print(rootNode->obj);

            if(equalClassFunc(info->fromFunction,rootNode->obj->callee)
                  || equalClassInfo(info->fromClass,rootNode->obj->newObject)){
               if(childNode==NULL){
                  childNode=n_slice_new0(GenericNode);
                  childNode->parent=rootNode;
                  childNode->obj=cloneGenericObj(item);
                  childNode->from=item;
                  item->ref=TRUE;
                  rootNode->childs[rootNode->childCount++]=childNode;
               }
               GenericUnit *newUnit= generic_unit_clone(rootNode->obj->infos[info->fromPos]->genUnit);
               if(!newUnit->isDefine)
                  n_error("未知错误，泛型单元应该是定义的。%s\n",generic_unit_tostring(newUnit));
               childNode->obj->infos[j]->genUnit=newUnit;//从未定义转为定义泛型单元
//               printf("reference rootNode 加入了子node 00---j:%d rootNode:%p %p newUnit:%s\n",j,rootNode,childNode,generic_unit_tostring(newUnit));
//               generic_obj_print(rootNode->obj);
//               printf("reference rootNode 加入了子node 11 ---j:%d rootNode:%p %p newUnit:%s\n",j,rootNode,childNode,generic_unit_tostring(newUnit));
//               generic_obj_print(childNode->obj);

            }
         }
      }
      if(childNode!=NULL){
         reference(childNode,array);
      }
   }
}

static void printNode(GenericNode *node,nboolean isRoot,int *level)
{
   int j;
   int le=*level;
   if(isRoot)
      printf("根节点:\n");
   else{
      for(j=0;j<le;j++)
         printf("\t");
      printf("子节点:\n");
   }
   printGenericObj(node->obj,le);
   int i;
   int ll=*level+1;
   for(i=0;i<node->childCount;i++){
      GenericNode *child=node->childs[i];
      printNode(child,FALSE,&ll);
   }
}

/**
 * 根据GenericUnit的内容，字符串化。
 */
static void unitToString(GenericUnit *unit,NString *strs)
{
   n_string_append_printf(strs,"parent=%s\n",unit->parent.name?unit->parent.name:"");
   n_string_append_printf(strs,"name=%s\n",unit->name?unit->name:"");
   n_string_append_printf(strs,"size=%d\n",unit->size);
   n_string_append_printf(strs,"pointerCount=%d\n",unit->pointerCount);
   n_string_append_printf(strs,"genericType=%d\n",unit->genericType);
   n_string_append_printf(strs,"isDefine=%d\n",unit->isDefine?1:0);
}

static void runGenericInfoToString(RunGenericInfo *info,NString *strs)
{
   unitToString(info->genUnit,strs);
   n_string_append_printf(strs,"fromFunction=%s\n",info->fromFunction?info->fromFunction->mangleFunName:"");
   n_string_append_printf(strs,"fromClass=%s\n",info->fromClass?info->fromClass->className.sysName:"");
   n_string_append_printf(strs,"fromPos=%d\n",info->fromPos);
   n_string_append_printf(strs,"from=%d\n",info->from);
   n_string_append_printf(strs,"file=%s\n",info->file?info->file:"");

}

/**
 * GenericObj 字符串化 保存在在generic_obj文件中。
 */
static void genericObjToString(GenericObj *obj,NString *strs)
{
    n_string_append_printf(strs,"type=%d\n",obj->type);
    n_string_append_printf(strs,"infoLen=%d\n",obj->infoLen);

    int i;
    for(i=0;i<obj->infoLen;i++){
       runGenericInfoToString(obj->infos[i],strs);
    }
    n_string_append_printf(strs,"callee=%s\n",obj->callee?obj->callee->mangleFunName:"");
    //被调泛型函数所在的类名。生成编译单元时需要
    n_string_append_printf(strs,"calleeSysName=%s\n",obj->callee?obj->callee->className->sysName:"");
    n_string_append_printf(strs,"newObject=%s\n",obj->newObject?obj->newObject->className.sysName:"");
    n_string_append_printf(strs,"atFunc=%s\n",obj->atFunc?obj->atFunc->mangleFunName:"");
    n_string_append_printf(strs,"atClass=%s\n",obj->atClass?obj->atClass->className.sysName:"");
    n_string_append_printf(strs,"origModel=%s\n",generic_model_tostring(obj->origModel));
    n_string_append_printf(strs,"declClassFile=%s\n",obj->declClassFile);
}

typedef enum
{
   SAVE_ROOT, //源代码新建或调用全定义泛型模型单元的类或泛型函数
   SAVE_CHILD, //源代码新建或调用未全定义泛型模型单元的类或泛型函数
   SAVE_OUTPUT,//通过对象可达性算法后，输出的全定义泛型模型单元的类或泛型函数
}SaveType; //当字符串化GenericObj，需要指定类型

static void writeObject(GenericObj *obj,NString *strs,SaveType saveType)
{
   n_string_append_printf(strs,"%s\n",GENOBJ_START);
   n_string_append_printf(strs,"saveType=%d\n",saveType);
   genericObjToString(obj,strs);
   n_string_append_printf(strs,"%s\n",GENOBJ_END);
}

/**
 * 把泛型对象写入文件 xxx.genobj.o中，最后把 xxx.genobj.o 文件名写入索引文件GENERIC_MODEL_INDEX_FILE generic_model_index.o
 * 在编译完文件调用该方法。每个编译单元都会调用，
*/
void generic_graph_save(GenericGraph *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("genericgraph.c 是第二次编译 %s 不需要写入任何接口信息。\n",in_fnames[0]);
      return;
   }
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   char newName_new[512];
   sprintf(newName_new,"%s.genobj_new.o",objfile);
    //如果没有泛型对象，删除该文件 xxx.genobj.o
   if(self->collectGenArray->len==0){
      //printf("generic_graph_save 00 无泛型对象。\n");
      remove(newName_new);
      return;
   }
   //1.找出全定义泛型的GenericObj 作为根节点
   NPtrArray *rootArray=n_ptr_array_new();
   int i;
   for(i=0;i<self->collectGenArray->len;i++){
      GenericObj *item=n_ptr_array_index(self->collectGenArray,i);
      if(fullDefine(item)){
         n_ptr_array_add(rootArray,item);
         n_ptr_array_remove(self->collectGenArray,item);
         i--;
      }
   }
   //2.排除重复的定义。
   removeRepeat(rootArray,TRUE);
   removeRepeat(self->collectGenArray,FALSE);
   NString *strs=n_string_new("");
   if(rootArray->len>0){
      for(i=0;i<rootArray->len;i++){
         GenericObj *item=n_ptr_array_index(rootArray,i);
         writeObject(item,strs,SAVE_ROOT);
      }
   }

   if(self->collectGenArray->len>0){
      for(i=0;i<self->collectGenArray->len;i++){
         GenericObj *item=n_ptr_array_index(self->collectGenArray,i);
         writeObject(item,strs,SAVE_CHILD);
      }
   }

   FILE *fp=fopen(newName_new,"w");
   fwrite(strs->str,1,strs->len,fp);
   fclose(fp);
   gcc_assert(self->collectFileName==NULL);
   self->collectFileName=n_strdup(newName_new);
   middle_file_modify(middle_file_get(),COMPILE_NEW);
   n_string_free(strs,TRUE);

}

/**
* 从字符串
* genobj start:
* ...
* genobj end:
* 取出
* root=0
*  type=0
*  ...
*  atFunc=_Z6TFirst7setdataEPN6TFirstE
*  atClass=TFirst
*
*/
NPtrArray *generic_graph_read(char *buffer)
{
   NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
   char *c=buffer;
   while(strstr(c,GENOBJ_START)){
      char *start=strstr(c,GENOBJ_START);
      //printf("r0 is :%s\n",start);
      char *n=start+strlen(GENOBJ_START)+1;//加1跳过 GENOBJ_START 后的\n号
    //  printf("r1 is :%s\n",n);
      char *end=strstr(n,GENOBJ_END);
      //printf("r2 is :%s\n",end);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      //printf("generic_graph_read :%s\n",ret);
      n_ptr_array_add(array,ret);
      c = end+strlen(GENOBJ_END);
   }
   return array;
}

#define STR(X) strstr(strs[X],"=")+1

static inline ClassFunc *buildClassFunc(char *mangleFunName)
{
   if(mangleFunName==NULL || strlen(mangleFunName)==0){
      return NULL;
   }else{
      ClassFunc *func=n_slice_new0(ClassFunc);
      func->mangleFunName=mangleFunName;
      return func;
   }
}

static inline ClassInfo *buildClassInfo (char *sysName)
{
   if(sysName==NULL || strlen(sysName)==0){
      return NULL;
   }else{
      ClassInfo *info=n_slice_new0(ClassInfo);
      info->className.sysName=sysName;
      return info;
   }
}

static inline GenericModel *buildGenericModel(char *model)
{
   if(model==NULL || strlen(model)==0){
      return NULL;
   }else{
      char **getModelStr=n_strsplit(model,",",-1);
      int  genCount=n_strv_length(getModelStr);
      int i=0;
      GenericModel *genmodel=generic_model_new_from_file();
      for(i=0;i<genCount;i++){
         char *gen=getModelStr[i];
         int strLen=strlen(gen);
         if(gen!=NULL && strLen>0){
            char rr[strlen(gen)-1];
            memcpy(rr,gen,strlen(gen)-2);
            rr[strlen(gen)-2]='\0';
            char p=gen[strlen(gen)-1];
            int pointer=p-48;
            generic_model_add(genmodel,rr,pointer);
            //printf("genmodel is :%s %d\n",rr,pointer);
         }
      }
      return  genmodel;
   }
}

/**
 * 从字符串
 * genobj start:
 * root=0
 * type=0
 * ...
 * genobj end:
 * 创建 GenericObj;并设置存储类型
 */
static GenericObj *createGenObj(char *content,int *saveType)
{
   nchar  **strs= n_strsplit(content,"\n",-1);
   GenericObj *item=n_slice_new0(GenericObj);
   *saveType=atoi(STR(0));
   item->type=atoi(STR(1));
   item->infoLen=atoi(STR(2));
   RunGenericInfo **infos=(RunGenericInfo **)xmalloc(item->infoLen*sizeof(unsigned long));
   int i;
   int offset=3;
   for(i=0;i<item->infoLen;i++){
      RunGenericInfo *info=n_slice_new0(RunGenericInfo);
      info->genUnit=n_slice_new0(GenericUnit);
      info->genUnit->parent.name=STR(offset++);
      info->genUnit->name=STR(offset++);
      info->genUnit->size=atoi(STR(offset++));
      info->genUnit->pointerCount=atoi(STR(offset++));
      info->genUnit->genericType=atoi(STR(offset++));
      info->genUnit->isDefine=atoi(STR(offset++));
      char *mangleFunName= STR(offset++);
      info->fromFunction=buildClassFunc(mangleFunName);
      char *sysName= STR(offset++);
      info->fromClass=buildClassInfo(sysName);
      info->fromPos=atoi(STR(offset++));
      info->from=atoi(STR(offset++));
      info->file= STR(offset++);
      infos[i]=info;
   }
   item->infos=infos;
   {
      //被调泛型函数所在的类名。生成编译单元时需要
      char *mangleFunName= STR(offset++);
      item->callee=buildClassFunc(mangleFunName);
      char *calleeSysName= STR(offset++);
      if(item->callee){
         ClassName *cn=n_slice_new0(ClassName);
         cn->sysName=calleeSysName;
         item->callee->className=cn;
      }
   }
   item->newObject=buildClassInfo(STR(offset++));
   item->atFunc=buildClassFunc(STR(offset++));
   item->atClass=buildClassInfo(STR(offset++));
   item->origModel=buildGenericModel(STR(offset++));
   item->declClassFile=STR(offset++);
   item->ref=FALSE;
   return item;
}

/**
 * 输出所有泛型被确定了的对象。从树状变成一维
 */
static void output(GenericNode *node,NPtrArray *result)
{
   n_ptr_array_add(result,node->obj);
   int i;
   for(i=0;i<node->childCount;i++){
      GenericNode *child=node->childs[i];
      output(child,result);
   }
}

/**
 * 所有的有GenericObj对象的文件内容全部合并成一个字符串
 */
static char * readLocaFile(char *localFileList)
{
   nchar **items=n_strsplit(localFileList,"\n",-1);
   int length= n_strv_length(items);
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<length;i++){
      char *fn=items[i];
      FILE *fp=fopen(fn,"r");
      if(fp){
         char buffer[1024*150];
         int rev=fread(buffer,1,1024*150,fp);
         if(rev>0){
            buffer[rev]='\0';
            n_string_append(codes,buffer);
            n_string_append(codes,"\n");
         }
         fclose(fp);
      }
   }
   n_strfreev(items);
   return n_string_free(codes,FALSE);
}

/**
 * 泛型模型可达性算法。
 * 正在编译temp_func_track_45.c时调用这里
 * 1.从文件GENERIC_MODEL_INDEX_FILE读入文件列表，这些文件保存字符串化的泛型对象GenericObj
 * 2.从 rootArray和 childArray 中排除重复对象后，GenericObj字符串化，供保存到本项目的全局变量 LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX
 * 3.从库中取出所有的GenericObj,把非root类型的GenericObj加入到 childArray;
 * 4.从rootArray生成泛型对象关系图，用根创建一棵树型结构的树。每个节点都变成了定义的泛型对象
 * 5.输出每棵树的节点到一维数组，并删除重复的节点。
 */
void generic_graph_ready(GenericGraph *self)
{
   char *fileName = getenv("GCC_AET_NEW_GENERIC_LIST_PATH");
   if(fileName==NULL ||strlen(fileName)==0){
      return;
   }
   //1.从文件GENERIC_MODEL_COLLECT_FILE读入文件列表，这些文件保存字符串化的泛型对象GenericObj
   FILE *fp=fopen(fileName,"r");
   char fileList[10*1024];
   int rev=fread(fileList,1,10*1024,fp);
   fclose(fp);
   if(rev<=0)
      return;
   fileList[rev]='\0';
   char *content = readLocaFile(fileList);
   if(content==NULL || strlen(content)==0)
      return;
   //清除保存内容
   if(self->saveContent){
      free(self->saveContent);
      self->saveContent=NULL;
   }
   if(self->outputArray){
      n_ptr_array_unref(self->outputArray);
      self->outputArray=NULL;
   }
   //2.从 rootArray和 childArray 中排除重复对象后，GenericObj字符串化，供保存到本项目的全局变量 LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX 使用
   NPtrArray *rootArray=n_ptr_array_new();
   NPtrArray *childArray=n_ptr_array_new();

   NPtrArray *local=generic_graph_read(content);
   n_free(content);
   int i;
   for(i=0;i<local->len;i++){
      char *item=n_ptr_array_index(local,i);
      int isRoot=0;
      GenericObj *obj=createGenObj(item,&isRoot);
      n_debug("genericgraph.c generic_graph_ready 00 从字符串中生成root和child两类泛型对象 i:%d isRoot:%d str:%s\n",
            i,isRoot,item);
      if(isRoot==SAVE_ROOT)
         n_ptr_array_add(rootArray,obj);
      else
         n_ptr_array_add(childArray,obj);
   }
   n_ptr_array_unref(local);

   if(rootArray->len==0 && childArray->len==0)
      return;
   removeRepeat(rootArray,TRUE);
   removeRepeat(childArray,FALSE);
   NString *saveStr=n_string_new("");
   //保存本项目的root genericobj
   //保存本项目的child genericobj
   //保存本项目的output genericobj
   if(rootArray->len>0){
      for(i=0;i<rootArray->len;i++){
         GenericObj *item=n_ptr_array_index(rootArray,i);
         writeObject(item,saveStr,SAVE_ROOT);
      }
   }

   if(childArray->len>0){
      for(i=0;i<childArray->len;i++){
         GenericObj *item=n_ptr_array_index(childArray,i);
         writeObject(item,saveStr,SAVE_CHILD);
      }
   }
   //只有未定义的泛型，保存这些未定义的对象到全局变量LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX，并返回
   if(rootArray->len==0 && childArray->len>0){
      self->saveContent=n_string_free(saveStr,FALSE);
      return;
   }
   n_debug("genericgraph.c generic_graph_ready 11 root 和 child的内容：\n%s\n",saveStr->str);
   //3.从库中取出所有的三类GenericObj,（1）.root 不需要参与计算，（2）.child类型的泛型对象合并到当前项目childArray中。
   //（3）output类型的泛型对象，用来判断与本项目中的output类型的泛型对象是否重复。
   NPtrArray *libArray=aet_lib_get_generic_objs(aet_lib_get());
   //库中输出的泛型对象。
   NPtrArray *outArrayFromLib=n_ptr_array_new();
   n_debug("genericgraph.c generic_graph_ready 22 从库中取泛型对象\n");

   if(libArray!=NULL){
      for(i=0;i<libArray->len;i++){
         char *item=n_ptr_array_index(libArray,i);
         int saveType=0;
         GenericObj *obj=createGenObj(item,&saveType);
         if(saveType==SAVE_CHILD)
            n_ptr_array_add(childArray,obj);
         else if(saveType==SAVE_OUTPUT)
            n_ptr_array_add(outArrayFromLib,obj);
      }
   }
   //再一次排除重复的非定义泛型对象GenericObj;
   removeRepeat(childArray,FALSE);
//   printf("genericgraph.c generic_graph_ready 22 %d %d\n",rootArray->len,childArray->len);
//   for(i=0;i<rootArray->len;i++){
//      GenericObj *item=n_ptr_array_index(rootArray,i);
//      printf("打印root i:%d\n",i);
//      printGenericObj(item);
//   }
//
//   for(i=0;i<childArray->len;i++){
//      GenericObj *item=n_ptr_array_index(childArray,i);
//      printf("打印 child i:%d\n",i);
//      printGenericObj(item);
//   }


   //4.从rootArray生成泛型对象关系图，用每个根创建一棵树型结构的数据结构。每个节点都变成了定义的泛型对象
   GenericNode *rootNodes[rootArray->len];
   for(i=0;i<rootArray->len;i++){
      GenericObj *item=n_ptr_array_index(rootArray,i);
      GenericNode *rootNode=n_slice_new0(GenericNode);
      rootNode->obj=item;
      rootNodes[i] = rootNode;
      n_debug("genericgraph.c generic_graph_ready 33 i:%d %d %d\n",i,rootArray->len,childArray->len);
      reference(rootNode,childArray);
      int j;
      for(j=0;j<childArray->len;j++){
         GenericObj *child=n_ptr_array_index(childArray,j);
         child->ref=FALSE;//清除后要重新作为rootnode的child参与生成可达图
      }
   }
   if(n_log_is_debug_file(NULL,NULL)){
      n_debug("genericgraph.c generic_graph_ready 44 打印泛型对象关系图:\n");
      for(i=0;i<rootArray->len;i++){
         GenericNode *node=rootNodes[i];
         printf("第%d个根节点:\n",i);
         int level=0;
         printNode(rootNodes[i],TRUE,&level);
         printf("\n");
      }
   }
   //5.输出每棵树的节点到一维数组，并删除重复的节点。如果在库中的output节点也找到也要删除
   NPtrArray *outArray=n_ptr_array_new();
   for(i=0;i<rootArray->len;i++){
      GenericNode *node=rootNodes[i];
      output(node,outArray);
   }
   removeRepeat(outArray,TRUE);
   //删除库中重复的节点，按理不应该有重复的节点。
   removeRepeat(outArrayFromLib,TRUE);
   //如果在库中找到相同的对象，从outArray中移走，不再需要实现。
   int j;
   for(i=0;i<outArray->len;i++){
      GenericObj *exclude=n_ptr_array_index(outArray,i);
      for(j=0;j<outArrayFromLib->len;j++){
         GenericObj *item=n_ptr_array_index(outArrayFromLib,j);
         if(objectEqual(exclude,item,TRUE)){
            n_ptr_array_remove(outArray,exclude);
            i--;
            break;
         }
      }
   }
   n_debug("genericgraph.c generic_graph_ready 55 打印最后输出结果:outArray:%p %d\n",outArray,outArray->len);
   for(i=0;i<outArray->len;i++){
      GenericObj *item=n_ptr_array_index(outArray,i);
      n_debug("genericgraph.c 最终的泛型对象:%d\n",i);
      printSimpleGenericObj(item);
      writeObject(item,saveStr,SAVE_OUTPUT);
   }
   self->saveContent=n_string_free(saveStr,FALSE);
   self->outputArray=outArray;
}


char *generic_graph_get_output_string(GenericGraph *self)
{
   return self->saveContent;
}

NPtrArray *generic_graph_get_output_generic_obj(GenericGraph *self)
{
   return self->outputArray;
}

void generic_obj_free(GenericObj *self)
{
   if(!self)
      return;
}

GenericGraph *generic_graph_get()
{
   static GenericGraph *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(GenericGraph));
      genericGraphInit(singleton);
      singleton->aetParser = aet_parser_get();
   }
   return singleton;
}


