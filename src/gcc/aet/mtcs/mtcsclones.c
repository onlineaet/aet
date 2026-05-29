/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the onlineaet@163.com
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "alloc-pool.h"
#include "timevar.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs-libfuncs.h"
#include "insn-config.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "varasm.h"
#include "stringpool.h"
#include "ipa-reference.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "gimple-ssa.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-ssanames.h"
#include "gimple.h"
#include "gimple-walk.h"

#include "mtcsclones.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"

static void testedge(struct cgraph_node *node,struct function *this_cfun,tree decl);
static void removeNewNodeFromHostSymtab(MtcsClones *self,struct cgraph_node *newNode);
static struct cgraph_node *getMtcsFuncNode(MtcsClones *self,tree hostDecl);
static void printNodeBB(struct cgraph_node *cnode);

typedef struct _MtcsDeclPair
{
   tree hostDecl;
   struct cgraph_node *mtcsNode;
}MtcsDeclNodePair;


static void mtcsClonesInit(MtcsClones *self)
{
   //主机的函数声明是否已用来创建新的cgraph_node decl对应一个唯一个的mtcs节点。
   self->hostDeclArray = n_ptr_array_new();
   self->mtcsNodeInfoArray = n_ptr_array_new();
}

/**
 * 打印当前符号表的节点，可能是主机也可能是mtcstarget
 */
static void printSymbolNode()
{
   cgraph_node *cnode;
   int i=0;
   FOR_EACH_FUNCTION (cnode){
      n_debug("mtcsclones.c printSymbolNode 当前符号表中的 i:%d node:%p name:%s decl:%p init:%p 是否定义:%d\n",
            i++,cnode,cnode->name(),cnode->decl,DECL_INITIAL(cnode->decl),cnode->definition);
      //if(strstr(cnode->name(),"activate") || strstr(cnode->name(),"forwardMTCS"))
      printNodeBB(cnode);
   }
}

/**
 * 用主机的decl创建cgraphe_node mtcsNode
 * addHostNode;
 */
static void addHostFuncNode(MtcsClones *self,tree decl,struct cgraph_node *mtcsNode)
{
   int len=self->hostDeclArray->len;
   int i;
   gcc_assert(decl);
   for(i=0;i<len;i++){
      MtcsDeclNodePair *pair=n_ptr_array_index(self->hostDeclArray,i);
      if(pair->hostDecl==decl){
         error("已为decl创建了mtcs 节点 %s",IDENTIFIER_POINTER(DECL_NAME(decl)));
         return ;
      }
   }

   MtcsDeclNodePair *pair=n_slice_new(MtcsDeclNodePair);
   pair->hostDecl=decl;
   pair->mtcsNode=mtcsNode;
   n_ptr_array_add(self->hostDeclArray,pair);
}

static struct cgraph_node *getHostFuncNode(MtcsClones *self,tree decl)
{
   if(!decl)
      return NULL;
   int len=self->hostDeclArray->len;
   int i;
   for(i=0;i<len;i++){
      MtcsDeclNodePair *pair=n_ptr_array_index(self->hostDeclArray,i);
      if(pair->hostDecl==decl)
         return pair->mtcsNode;
   }
   return NULL;
}

/**
 * 每克隆一个新的节点，需要查找之前是否用过该节点的主机声明，
 * 如果用过主机的节点创建的mtcsnode需要用现在克隆的节点中的
 * fndecl替换。这种情况出现在被调函数callee在调用函数之后实现。
 * void a(){
 *   b();
 * }
 * void b(){
 * }
 */
static void replaceHostDecl(MtcsClones *self,struct cgraph_node *hostNode,struct cgraph_node *mtcsNode)
{
   int i;
   cgraph_node *cnode;
   nboolean find=FALSE;
   FOR_EACH_FUNCTION (cnode){
      char *name=cnode->name();
      if(!cnode->definition && !strcmp(name,mtcsNode->name())){
         n_debug("mtcsclones.c replaceHostDecl 移走老节点 当前符号表中的 i:%d node:%p name:%s decl:%p init:%p 是否定义:%d\n",
         i++,cnode,cnode->name(),cnode->decl,DECL_INITIAL(cnode->decl),cnode->definition);
         find=TRUE;
         break;
      }
   }
   if(find){
      struct cgraph_node *old=cnode;
      tree olddecl=old->decl;
      n_debug("mtcsclones.c replaceHostDecl 节点 该节点是在rebuild_edges中创建的  %p %s\n",cnode,cnode->name());
      cnode->remove();

      FOR_EACH_FUNCTION (cnode){
         if(cnode==old)
            continue;
         basic_block bb;
         gimple_stmt_iterator gsi;
         tree fndecl=cnode->decl;
         struct function *fn=DECL_STRUCT_FUNCTION(fndecl);
         if(!fn)
            continue;
         FOR_EACH_BB_FN (bb, fn){
            for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
               gimple *stmt = gsi_stmt (gsi);
               tree decl;
               if (gcall *call_stmt = dyn_cast <gcall *> (stmt)){
                  decl = gimple_call_fndecl (call_stmt);
                  if(decl==olddecl){
                     gimple_call_set_fndecl(call_stmt,mtcsNode->decl);
                     printf("设新的节点:%p %p\n",decl,mtcsNode->decl);
                     cgraph_edge *e=cnode->create_edge (mtcsNode, call_stmt,bb->count);
                  }
               }
            }
         }
      }
   }
}

/**
 * 创建MTCS节点信息 只为有定义的mtcs核函数和设备函数创建
 */
typedef struct _MtcsNodeInfo
{
   struct cgraph_node *hostNode;
   struct cgraph_node *mtcsNode;
   tree hostDecl;
   tree mtcsDecl;
}MtcsNodeInfo;

/**
 * 新建的节点加入到数组
 */
static void addMtcsNodeInfo(MtcsClones *self,struct cgraph_node *hostNode,
     struct cgraph_node *mtcsNode)
{
   int i;
   for(i=0;i<self->mtcsNodeInfoArray->len;i++){
      MtcsNodeInfo *item=n_ptr_array_index(self->mtcsNodeInfoArray,i);
      if(item->hostNode==hostNode || item->mtcsNode==mtcsNode){
         n_error("重复加入节点:%s\n",hostNode->name());
         return;
      }
   }
   MtcsNodeInfo *info=n_slice_new0(MtcsNodeInfo);
   info->hostNode = hostNode;
   info->hostDecl = hostNode->decl;
   info->mtcsNode = mtcsNode;
   info->mtcsDecl = mtcsNode->decl;
   n_debug("加入主机 mtcs对应的节点:%p %p %p %p %s\n",hostNode,mtcsNode,hostNode->decl,mtcsNode->decl,hostNode->name());
   n_ptr_array_add(self->mtcsNodeInfoArray,info);
}

/**
 * 获取 hostdecl对应的 MTCS函数节点
 */
static struct cgraph_node *getMtcsFuncNode(MtcsClones *self,tree hostDecl)
{
   if(!hostDecl)
      return NULL;
   int i;
   for(i=0;i<self->mtcsNodeInfoArray->len;i++){
      MtcsNodeInfo *item=n_ptr_array_index(self->mtcsNodeInfoArray,i);
      if(item->hostDecl==hostDecl ){
         gcc_assert(item->hostNode->decl==hostDecl);//防止主机的hostNode中的decl被改了
         gcc_assert(item->mtcsNode->decl==item->mtcsDecl);//防止主机的hostNode中的decl被改了
         return item->mtcsNode;
      }
   }
   return NULL;
}

static void printNodeBB(struct cgraph_node *cnode)
{
   tree fndecl=  cnode->decl;
   struct function *nodeFun;
   nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   n_debug("mtcsclones.c printNodeBB 00 开始 name:%s nodeFun:%p cfun:%p\n",cnode->name(),nodeFun,cfun);
   if(!nodeFun)
      return;
   basic_block bb;
   FOR_EACH_BB_FN (bb, nodeFun){
      gimple_stmt_iterator gsi, seq_gsi;
      int i=0;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         enum gimple_code code = gimple_code (stmt);
         n_debug("mtcsclones.c printNodeBB 11 i:%d bb:%p name:%s stmt:%p\n",i++,bb,cnode->name(),stmt);
         aet_print_gimple(stmt);
         if(code==GIMPLE_CALL){
                bool internal=gimple_call_internal_p(stmt);
                bool builtin=gimple_call_builtin_p(stmt);
                tree decl=gimple_call_fndecl(stmt);
                n_debug("mtcsclones.c printNodeBB 22 i:%d 被调函数 %p %s 内部:%d 内建:%d 函数节点:%p definition:%d address_taken:%d\n",
                     i,decl,decl?IDENTIFIER_POINTER(DECL_NAME(decl)):"NULL",
                           internal,builtin,decl?decl->decl_with_vis.symtab_node:NULL,cnode->definition,cnode->address_taken);
         }
      }
   }
   n_debug("mtcsclones.c printNodeBB 33 结束 gimple bb:%p name:%s\n",bb,cnode->name());
}

static void printAttrs (tree attrs)
{
   for (const_tree attr = attrs; attr; attr = TREE_CHAIN (attr)){
      char *name=IDENTIFIER_POINTER(TREE_PURPOSE (attr));
      n_debug("mtcsclones.c printAttrs 属性 %s\n",name);
      aet_print_tree(attr);
   }
}

static tree remove_attributes(tree attrs, char *attrName)
{
   tree new_attrs = NULL_TREE;
   tree *ptr = &new_attrs;
   const_tree start = attrs;
   for (const_tree attr = attrs; attr; attr = TREE_CHAIN (attr)){
      // const attribute_spec *as = lookup_attribute_spec (TREE_PURPOSE (attr));
      char *name=IDENTIFIER_POINTER(TREE_PURPOSE (attr));
      n_debug("mtcsclones.c remove_attributes 00 %s 要移走的:%s\n",name,attrName);
      const_tree end;
      //if (!predicate (attr, as))
      if (!strcmp (name,attrName))
         end = attr;
      else if (start == attrs)
         continue;
      else
         end = TREE_CHAIN (attr);

      for (; start != end; start = TREE_CHAIN (start)){
         *ptr = tree_cons (TREE_PURPOSE (start),TREE_VALUE (start), NULL_TREE);
         TREE_CHAIN (*ptr) = NULL_TREE;
         ptr = &TREE_CHAIN (*ptr);
      }
      start = TREE_CHAIN (attr);
   }
   gcc_assert (!start || start == attrs);
   tree ret= start ? attrs : new_attrs;
   n_debug("mtcsclones.c remove_attributes 11 移走后的attributes---\n");
   printAttrs(ret);
   return ret;
}

/**
 * 从主机符号表中移走引用到newNode的caller或callee
 * 创建新的node,在主机的symtab中各个节点会成为newNode的调用者或被调用者，所以需要移走
 */
static void removeNewNodeFromHostSymtab(MtcsClones *self,struct cgraph_node *newNode)
{
   struct cgraph_node *cnode;
   FOR_EACH_FUNCTION (cnode){
      cgraph_edge *e;
      n_debug("mtcsclones.c removeNewNodeFromSymtab 00 cnode:%s %p\n",cnode->name(),cnode);
      int count=0;
      /* Update the call expr on the edges to call the new version.  */
      for (e = cnode->callers; e; e = e->next_caller){
         tree fndecl=  e->caller->decl;
         function *fn = DECL_STRUCT_FUNCTION (e->caller->decl);
         n_debug("mtcsclones.c removeNewNodeFromSymtab 11 调用者: count:%d %s fn:%p caller:%p node:%s\n",
         count++,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fn,e->caller,cnode->name());
         if(e->caller==newNode){
            n_debug("mtcsclones.c 节点:%s 的调用者 callers:%s存在,移走!\n",cnode->name(),newNode->name());
            cgraph_edge::remove(e);
            break;
         }
      }
      /* Update the call expr on the edges to call the new version.  */
      count=0;
      for (e = cnode->callees; e; e = e->next_callee){
         tree fndecl=  e->callee->decl;
         function *fn = DECL_STRUCT_FUNCTION (e->callee->decl);
         n_debug("mtcsclones.c removeNewNodeFromSymtab 22 被调者: count:%d %s fn:%p callee:%p node:%s\n",
         count++,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fn,e->callee,cnode->name());
         if(e->callee==newNode){
            n_debug("mtcsclones.c 节点:%s的被调用者 callee:%s存在,移走!\n",cnode->name(),newNode->name());
            cgraph_edge::remove(e);
            break;
         }
      }
   }
}

/* Hook called when NODE is duplicated and therefore should be
   excluded from removed_nodes.  DATA is a hash set with removed nodes.  */

static void duplicate_cgraph_node_to_order (cgraph_node *origNode, cgraph_node *newNode,void *data)
{
//  hash_set<cgraph_node *> *removed_nodes = (hash_set<cgraph_node *> *)data;
//  gcc_checking_assert (!removed_nodes->contains (node));
//  removed_nodes->remove (node2);
    n_debug("mtcsclones.c---- duplicate_cgraph_node_to_order\n");
}

/* Record references to typeinfos in the type list LIST.  */
//原型 record_type_list cgraphbuild.cc
static void record_type_list (cgraph_node *node, tree list)
{
  for (; list; list = TREE_CHAIN (list)){
      tree type = TREE_VALUE (list);
      if (TYPE_P (type))
          type = lookup_type_for_runtime (type);
      STRIP_NOPS (type);
      if (TREE_CODE (type) == ADDR_EXPR){
          type = TREE_OPERAND (type, 0);
          if (VAR_P (type)){
              varpool_node *vnode = varpool_node::get_create (type);
              node->create_reference (vnode, IPA_REF_ADDR);
          }
      }
  }
}

/* Record all references we will introduce by producing EH tables
   for NODE.  */
//原型 record_eh_tables cgraphbuild.cc
static void record_eh_tables (cgraph_node *node, function *fun)
{
  eh_region i;
  if (DECL_FUNCTION_PERSONALITY (node->decl)){
      tree per_decl = DECL_FUNCTION_PERSONALITY (node->decl);
      cgraph_node *per_node = cgraph_node::get_create (per_decl);
      node->create_reference (per_node, IPA_REF_ADDR);
      per_node->mark_address_taken ();
  }
  i = fun->eh->region_tree;
  if (!i)
    return;
  while (1){
      switch (i->type){
        case ERT_CLEANUP:
        case ERT_MUST_NOT_THROW:
          break;
        case ERT_TRY:
          {
            eh_catch c;
            for (c = i->u.eh_try.first_catch; c; c = c->next_catch)
              record_type_list (node, c->type_list);
          }
          break;
       case ERT_ALLOWED_EXCEPTIONS:
          record_type_list (node, i->u.allowed.type_list);
          break;
      }
      /* If there are sub-regions, process them.  */
      if (i->inner)
          i = i->inner;
      /* If there are peers, process them.  */
      else if (i->next_peer)
          i = i->next_peer;
      /* Otherwise, step back up the tree to the next peer.  */
      else{
          do{
              i = i->outer;
              if (i == NULL)
                  return;
          }while (i->next_peer == NULL);
          i = i->next_peer;
      }
  }
}

/**
 * 在当前符号表中查找decl对应的cgraph_node
 * bug 059
 */
static struct cgraph_node *findNode(tree decl)
{
   struct cgraph_node *cnode=NULL;
   FOR_EACH_FUNCTION (cnode){
      if(cnode->decl==decl){
         n_debug("mtcsclones.c findNode 找到节点 node:%p :%s decl:%p init:%p 是否定义:%d\n",
                cnode,cnode->name(),cnode->decl,DECL_INITIAL(cnode->decl),cnode->definition);
         return cnode;
      }
   }
   return NULL;
}

/**
 * 当克隆完定义的MTCS函数后，调用该方法。
 * 1.在该函数内的所有bb中查找gimple call,如果有，判断该gimple call引用的函数声明
 * a.查找该声明是不是MTCS函数并且有定义，那么就用函数的声明替换gimple call中的函数声明
 * class A{
 *   __global__ void setdata();
 * };
 * impl$ A{
 *   __device__ int getdata(){
 *     return 0;
 *   }
 *   __global__ void setdata(){
 *      int v=getdata();
 *      或者
 *      printf("ret is :%d\n",5);
 *   }
 * };
 * 克隆完setdata函数，调用 rebuild_edges 遍历 bb gimple call
 * 发现有一个函数调用 getdata。该声明是主机的函数getdata声明，getMtcsFuncNode 返回cgraph_node是
 * 克隆的getdata函数定义节点mtcsNode,在mtcsNode的decl就是getdata克隆后的声明，用该声明替换gimple call中的decl。
 * 如果 gimple call中的主机声明不是MTCS函数，可能是一个外部函数，比如printf。通过getMtcsNode查找有没有print节点
 * 如果没有，就创建新的printf节点，并保存该节点，然后也用新的printf节点中的decl替换gimple call中的主机节点。
 * 原型 cgraph_edge 成员 rebuild_edges cgraph.h cgraphbuild.cc
 * decl = gimple_call_fndecl (call_stmt);返回的 decl是主机的decl decl->decl_with_vis.symtab_node也是主机的
 * 如果decl->decl_with_vis.symtab_node 不为空,通过cgraph_node::get_create (decl);不能创建新的 cgraph_node
 * 调用该函数时，处在目标符号表中
 */
static unsigned int rebuild_edges (MtcsClones *self,struct cgraph_node *newNode)
{
   basic_block bb;
   gimple_stmt_iterator gsi;
   newNode->remove_callees ();
   newNode->remove_all_references ();
   tree fndecl=newNode->decl;
   struct function *fn=DECL_STRUCT_FUNCTION(fndecl);
   newNode->count = ENTRY_BLOCK_PTR_FOR_FN (fn/*!cfun*/)->count;
   NPtrArray *bufferCallDecl=n_ptr_array_new();
   FOR_EACH_BB_FN (bb, fn){
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         tree decl;
         if (gcall *call_stmt = dyn_cast <gcall *> (stmt)){
            decl = gimple_call_fndecl (call_stmt);
            n_debug("mtcsclones.c rebuild_edges 00 新节点调用主机的 decl:%p %s newNode:%p %s  call_stmt:%p\n",
            decl,decl?IDENTIFIER_POINTER(DECL_NAME(decl)):"NULL",newNode,newNode->name(),call_stmt);
            if (decl){
               struct cgraph_node *mtcsNode=getMtcsFuncNode(self,decl);
               n_debug("mtcsclones.c rebuild_edges 11 找到主机decl对应的mtcsNode吗:%p decl_with_vis.symtab_node:%p\n",
               mtcsNode,decl->decl_with_vis.symtab_node );
               if(mtcsNode==NULL){
                  mtcsNode = getHostFuncNode(self,decl);
                  if(mtcsNode==NULL){
                     mtcsNode = findNode(decl);
                     if(mtcsNode==NULL){
                        struct cgraph_node *host=cgraph_node::get (decl);
                        decl->decl_with_vis.symtab_node = NULL;
                        mtcsNode=cgraph_node::get_create (decl);
                        addHostFuncNode(self,decl,mtcsNode);
                        decl->decl_with_vis.symtab_node= host;//还原主机的 cgraph_node;
                        n_debug("mtcsclones.c rebuild_edges 22 新建节点 decl:%p %s newNode:%p %s call_stmt:%p mtcsNode decl:%p\n",
                        decl,decl?IDENTIFIER_POINTER(DECL_NAME(decl)):"NULL",
                        newNode,newNode->name(),call_stmt,mtcsNode->decl);
                     }
                  }
               }
               gimple_call_set_fndecl(call_stmt,mtcsNode->decl);
               cgraph_edge *e=newNode->create_edge (mtcsNode, call_stmt,bb->count);
               n_debug("mtcsclones.c rebuild_edges 33 e:%p callee:%p,n->callees:%p newNode->callees:%p\n",
                        e,mtcsNode,mtcsNode->callees,newNode->callees);
            }else if (gimple_call_internal_p (call_stmt))
               ;
            else
               newNode->create_indirect_edge (call_stmt,gimple_call_flags (call_stmt),bb->count);
         }
         newNode->record_stmt_references (stmt);
      }
      for (gsi = gsi_start_phis (bb); !gsi_end_p (gsi); gsi_next (&gsi))
      newNode->record_stmt_references (gsi_stmt (gsi));
   }
   record_eh_tables (newNode, fn);
   gcc_assert (!newNode->inlined_to);
   return 0;
}


/*
 *有些主机函数,如 put NVPTX平台没有，只能转为 printf 函数处理。
*/
static void replaceCallFndecl(MtcsClones *self,struct cgraph_node *newNode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsBuiltins  *mtcsBuiltins =mtcs_target_get_builtins(mtcsTarget);

   tree fndecl=newNode->decl;
   struct function *nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   basic_block bb;
   gimple_stmt_iterator gsi;
   tree  newDecl;
   FOR_EACH_BB_FN (bb, nodeFun){
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         n_debug("mtcsclones.c replaceCallFndecl 00 打印每条stmt:%p\n",stmt);
         aet_print_gimple(stmt);
         if (!is_gimple_call (stmt))
            continue;
         gcall *call_stmt = dyn_cast <gcall *> (stmt);
         tree origDecl = gimple_call_fndecl (call_stmt);
         if(!origDecl)
            continue;
         location_t loc = gimple_location(call_stmt);
         if(fndecl_built_in_p (origDecl)){
            nboolean convert= mtcs_builtins_replace_call(mtcsBuiltins,stmt);
            if(!convert){
               //aet_print_location(loc);
               //判断是否支持内置函数 例如 strstr 转成了 __builtin_strchr 在这里判断平台是否支持 __builtin_strchr
               n_debug("mtcsclones.c replaceCallFndecl 11 不成功 nodeFun:%p newNode:%s\n",nodeFun,newNode->name());
               if(!mtcs_builtins_support_builtin_fn(mtcsBuiltins,origDecl)){
                  error_at(loc,"核函数或设备函数不能调用主机函数 00x。");
                  aet_print_tree_skip_debug(origDecl);
                  return;
               }
            }
            nboolean ret = mtcs_builtins_convert_call(mtcsBuiltins,stmt);
            if(ret){
               n_debug("建立了新的调用---\n");
            }
         }else{
            //判断是不是核函数或设备函数
            MtcsFuncType mtcsFuncType =  mtcs_info_get_func_type(origDecl);
            MtcsFuncType callType =mtcs_info_get_func_type(fndecl);
            if(mtcsFuncType==MTCS_FUNC_NOT
            && !mtcs_info_is_internal_fn(origDecl)
            && !mtcs_info_is_builtin_fn(origDecl)){
               error_at(loc,"核函数或设备函数不能调用主机函数 11y 。");
               aet_print_tree_skip_debug(origDecl);
               return;
            }
            if(callType==MTCS_FUNC_KERNEL && mtcsFuncType==MTCS_FUNC_KERNEL){
               error_at(loc,"核函数不能调用核函数 。");
               return ;
            }

            if((callType==MTCS_FUNC_DEVICE || callType == MTCS_FUNC_DEVICE_HOST)
                  && mtcsFuncType==MTCS_FUNC_KERNEL){
               error_at(loc,"设备函数不能调用核函数 。");
               return ;
            }
         }
      }
   }
}

//原来定义中mtcsifn.h 现已删除文件mtcsifn.h mtcsifn.c mtcsptxifn.h mtcsptxifn.c
enum mtcs_ifn_unique_kind {
    MTCS_IFN_UNIQUE_FORK,
    MTCS_IFN_UNIQUE_JOIN,
    MTCS_IFN_UNIQUE_HEAD_MARK,
    MTCS_IFN_UNIQUE_TAIL_MARK,
    MTCS_IFN_UNIQUE_PRIVATE,
    MTCS_IFN_UNIQUE_CODES
};

/**
 *创建内部函数 * int .data_dep.6 = .UNIQUE (OACC_HEAD_MARK, .data_dep.6, 1, 68);
 */
static void createTestInternalFn (location_t loc, gimple_stmt_iterator *gsi,bool head)
{
    tree ddvar = create_tmp_var (integer_type_node, ".data_dep");
    unsigned count = 6;
    tree tofollow= build_int_cst (integer_type_node, count);
    int marker_kind = (head ? MTCS_IFN_UNIQUE_HEAD_MARK : MTCS_IFN_UNIQUE_TAIL_MARK);
    tree marker = build_int_cst (integer_type_node, marker_kind);
    int nargs = 2 + (tofollow != NULL_TREE);
    gcall *call = gimple_build_call_internal (IFN_UNIQUE, nargs,marker, ddvar, tofollow);
    gimple_set_location (call, loc);
    gimple_set_lhs (call, ddvar);
    gsi_insert_before (gsi, call, GSI_SAME_STMT);
}

/**
 * 加一条测试语句
 * int .data_dep.6 = .UNIQUE (OACC_HEAD_MARK, .data_dep.6, 1, 68);
 */
static void addTestGimpleStmt(MtcsClones *self,struct cgraph_node *newNode)
{
   struct function *nodeFun = DECL_STRUCT_FUNCTION (newNode->decl);
   basic_block bb;
   gimple_stmt_iterator gsi;
   FOR_EACH_BB_FN (bb, nodeFun){
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         location_t loc=gimple_location(stmt);
         n_debug("mtcsclones.c addTestGimpleStmt\n");
         createTestInternalFn(loc,&gsi,true);
         break;
      }
   }
}

static void copy_x_dom_computed(MtcsClones *self,struct cgraph_node *src,struct cgraph_node *dest)
{
   if(!src->decl || !dest->decl)
      return;
   struct function *srcFun = DECL_STRUCT_FUNCTION (src->decl);
   if(!srcFun)
      return;
   struct function *destFun = DECL_STRUCT_FUNCTION (dest->decl);
   if(!destFun)
      return;
   destFun->cfg->x_dom_computed[0] = srcFun->cfg->x_dom_computed[0] ;
   destFun->cfg->x_dom_computed[1] = srcFun->cfg->x_dom_computed[1] ;
}

static void create_bb_dom(MtcsClones *self,struct cgraph_node *dest)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   struct function *destFun = DECL_STRUCT_FUNCTION (dest->decl);
   if(!destFun)
      return;
   basic_block bb;
   int i=0;
   mtcs_func_push_cfun(mtcsFunc,destFun);

   FOR_ALL_BB_FN (bb, destFun){
      struct et_node *ed1= bb->dom[CDI_DOMINATORS-1];

      if (dom_info_available_p (CDI_DOMINATORS) && !ed1){
         n_debug("mtcsclones.c create_bb_dom 00 克隆的 bb 的----xxx dom i:%d bb:%p\n",i,bb);
         add_to_dominance_info (CDI_DOMINATORS, bb);
      }
      struct et_node *ed2= bb->dom[CDI_POST_DOMINATORS-1];
      if (dom_info_available_p (CDI_POST_DOMINATORS) && !ed2){
         n_debug("mtcsclones.c create_bb_dom 11 克隆的 bb 的----yyy dom i:%d bb:%p\n",i,bb);
         add_to_dominance_info (CDI_POST_DOMINATORS, bb);
      }
      n_debug("mtcsclones.c create_bb_dom 22 克隆的 bb 的 dom i:%d bb:%p ed1:%p ed2:%p\n",i,bb,ed1,ed2);
      ed1= bb->dom[CDI_DOMINATORS-1];
      ed2= bb->dom[CDI_POST_DOMINATORS-1];
      n_debug("mtcsclones.c create_bb_dom 33 克隆的 bb xxxx 的 dom i:%d bb:%p ed1:%p ed2:%p destFun:%p\n",i,bb,ed1,ed2,destFun);
      i++;
   }
   mtcs_func_pop_cfun(mtcsFunc);
   FOR_ALL_BB_FN (bb, destFun){
      n_debug("mtcsclones.c create_bb_dom 44 克隆的 bb CDI_DOMINATORS方向 bb:%p dom:%p destFun:%p\n",bb,bb->dom[CDI_DOMINATORS-1],destFun);
   }
}

/**
 * 重要：在克隆节点前，检查该节点(主机的)是否有gimple_call,如存在，检查gimle_call中的被调节点
 * 是不是mtcsNode,如果是mtcsnode,检查 address_taken=0，临时改为1
 * 为了修复 bug 071
 * 调用origNode->create_version_clone_with_body
 * 会进入 tree-inline.cc 的 copy_bb
 * gcc_assert (!dest->definition
               || dest->address_taken
               || !id->src_node->definition
               || !id->dst_node->definition);

 * dest->address_taken = 0 断言失败。
 * 这里dest就是被调用函数
 */
static int checkCall(MtcsClones *self,struct cgraph_node *cnode,struct cgraph_node **calleeHostNode)
{
   tree fndecl=  cnode->decl;
   struct function *nodeFun;
   nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   basic_block bb;
   int changeCount=0;
   FOR_EACH_BB_FN (bb, nodeFun){
      gimple_stmt_iterator gsi, seq_gsi;
      int i=0;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         enum gimple_code code = gimple_code (stmt);
         if(code==GIMPLE_CALL){
            bool internal=gimple_call_internal_p(stmt);
            bool builtin=gimple_call_builtin_p(stmt);
            tree decl=gimple_call_fndecl(stmt);
            if(!decl)
               continue;
            if(!internal && !builtin){
               struct cgraph_node *dest = cgraph_node::get(decl);
               //n_debug("mtcsclones.c checkCall 00 dest %p name:%s address_taken:%d\n",
               //dest,dest->name(), dest->address_taken);
               //aet_print_gimple(stmt);
               struct cgraph_node *mtcsNode=getMtcsFuncNode(self,decl);
               if(mtcsNode!=NULL){
                  n_debug("mtcsclones.c checkCall 11 dest name:%s address_taken:%d mtcsNode:%p\n",
                  dest->name(), dest->address_taken,mtcsNode);
                  if(dest->address_taken==0){
                     dest->address_taken=1;
                     calleeHostNode[changeCount++]=dest;
                  }
               }
            }
         }
      }
   }
   return changeCount;
}

/* Create a simd clone of OLD_NODE and return it.  If FORCE_LOCAL is true,
   create it as a local symbol, otherwise copy the symbol linkage and
   visibility attributes from OLD_NODE.  */
static struct cgraph_node *mtcsCloneCreate (MtcsClones *self,struct cgraph_node *origNode, bool force_local)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   symbol_table *dest=mtcsTarget->symtab;

   const char *platName=mtcs_target_get_platform_name(mtcsTarget);
   struct cgraph_node *new_node;
   if (origNode->definition){
      if (!origNode->has_gimple_body_p ()){
         n_info("mtcsclones.c mtcsCloneCreate 00 没有gimple 语句 不会创建新的节点 返回。 origNode->has_gimple_body_p=false\n");
         return NULL;
      }
      origNode->get_body ();//会执行ssa吗
      symbol_table *save=symtab;//备份主机symtab
      symtab=dest;
      n_debug("mtcsclones.c mtcsCloneCreate 11 克隆前目标符号表中的节点 %s\n",IDENTIFIER_POINTER(DECL_NAME (origNode->decl)));
      printSymbolNode();
      n_debug("mtcsclones.c mtcsCloneCreate 11aa 克隆前目标符号表中的节点 %s\n",IDENTIFIER_POINTER(DECL_NAME (origNode->decl)));

      cgraph_2node_hook_list *duplication_hook = symtab->add_cgraph_duplication_hook (duplicate_cgraph_node_to_order, (void*)self);
      n_debug("mtcsclones.c mtcsCloneCreate 11bb 克隆前目标符号表中的节点 %s\n",IDENTIFIER_POINTER(DECL_NAME (origNode->decl)));
      struct cgraph_node *calleeHostNode[200];
      int changeCount=checkCall(self,origNode,calleeHostNode);
      new_node = origNode->create_version_clone_with_body (vNULL, NULL, NULL,NULL, NULL,platName);
      n_debug("mtcsclones.c mtcsCloneCreate 11cc 克隆前目标符号表中的节点 %s\n",IDENTIFIER_POINTER(DECL_NAME (origNode->decl)));
      int i;
      for(i=0;i<changeCount;i++)
         calleeHostNode[i]->address_taken = 0;

      copy_x_dom_computed(self,origNode,new_node);
      create_bb_dom(self,new_node);
      //把名字替换成 xxxx+.cuda.+0，因为在create_version_clone_with_body中创建的名字可以是xxxx+.cuda.+0
      //把名字还原回原函数名 2025-08-19
      char newname[255];
      sprintf(newname,"%s",IDENTIFIER_POINTER(DECL_NAME (origNode->decl)));
      DECL_NAME (new_node->decl) = get_identifier (newname);
      SET_DECL_ASSEMBLER_NAME (new_node->decl, get_identifier (newname));

      n_debug("mtcsclones.c mtcsCloneCreate 22  node 进入新的符号表 new_node:%p %s\
            目标:cgraph_count:%d 源符号表：%p 目标符号表:%p origNode->aux:%p\n",
            new_node,new_node->name(),dest->cgraph_count,save,dest,origNode->aux);
      aet_print_cgraph_node(new_node);
      n_debug("mtcsclones.c mtcsCloneCreate 33 克隆后目标符号表中的节点 %s\n",IDENTIFIER_POINTER(DECL_NAME (origNode->decl)));
      printSymbolNode();
      symtab->remove_cgraph_duplication_hook (duplication_hook);
      symtab_node * hashed_node = symtab_node::get (new_node->decl);
      symtab=save; //恢复主机symtab
      symtab_node * hashed_node_1 = symtab_node::get (new_node->decl);
      n_debug("mtcsclones.c mtcsCloneCreate 44 原节点信息:clone出新节点后 origDecl:%p newDecl:%p hashed_node:%p hashed_node_1:%p\n",
            origNode->decl,new_node->decl,hashed_node,hashed_node_1);
      aet_print_cgraph_node(origNode);
      gcc_assert(new_node->decl->decl_with_rtl.rtl==NULL);
      addMtcsNodeInfo(self,origNode,new_node);
      n_debug("mtcsclones.c mtcsCloneCreate 55 从主机移走创建的新节点：%p %s\n",new_node,new_node->name());
      aet_print_cgraph_node(new_node);
      removeNewNodeFromHostSymtab(self,new_node);
      save=symtab;
      symtab=dest;
      //重要 make_ssa_name 需要cfun
      mtcs_func_push_cfun(mtcsFunc,DECL_STRUCT_FUNCTION (new_node->decl));
      n_debug("mtcsclones.c mtcsCloneCreate 66 新符号表中的节点数\n");
      printSymbolNode();
      aet_print_cgraph_node(new_node);
      replaceCallFndecl(self,new_node);
      mtcs_replace_switch(self->mtcsReplace,new_node);
      mtcs_replace_parent_device_func_array(self->mtcsReplace,new_node);
      //从新cgraph_node中的decl中找出function，再循环每个bb,bb中循环每条gimple语句，找出gcall，然后重新复制gcall中的fndecl.并为新的fndecl创建creage_node
      rebuild_edges(self,new_node);
      replaceHostDecl(self,origNode,new_node);
      n_debug("mtcsclones.c mtcsCloneCreate 77 重建 edges后，新符号表中的节点数\n");
      printSymbolNode();
      aet_print_cgraph_node(new_node);
      //addTestGimpleStmt(self,new_node);
      mtcs_func_pop_cfun(mtcsFunc);
      symtab=save;
   }else{
      tree old_decl = origNode->decl;
      tree new_decl = copy_node (origNode->decl);
      DECL_NAME (new_decl) = clone_function_name_numbered (old_decl,platName);
      SET_DECL_ASSEMBLER_NAME (new_decl, DECL_NAME (new_decl));
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,new_decl, NULL);
      DECL_STATIC_CONSTRUCTOR (new_decl) = 0;
      DECL_STATIC_DESTRUCTOR (new_decl) = 0;
      symbol_table *save=symtab;
      symtab=dest;
      new_node = origNode->create_version_clone (new_decl, vNULL, NULL);
      if (origNode->in_other_partition)
         new_node->in_other_partition = 1;
      symtab=save;
      n_debug("mtcsclones.c mtcsCloneCreate 88 node 进入新的符号表 new_node:%p 目标:cgraph_count:%d 源符号表：%p 目标符号表:%p\n",
            new_node,dest->cgraph_count,save,dest);
      n_debug("mtcsclones.c mtcsCloneCreate 不应该进入这里，报告此错误!\n");
   }
   if (new_node == NULL)
      return new_node;

   set_decl_built_in_function (new_node->decl, NOT_BUILT_IN, 0);
   if (force_local){
      TREE_PUBLIC (new_node->decl) = 0;
      DECL_COMDAT (new_node->decl) = 0;
      DECL_WEAK (new_node->decl) = 0;
      DECL_EXTERNAL (new_node->decl) = 0;
      DECL_VISIBILITY_SPECIFIED (new_node->decl) = 0;
      DECL_VISIBILITY (new_node->decl) = VISIBILITY_DEFAULT;
      DECL_DLLIMPORT_P (new_node->decl) = 0;
   }else{
      TREE_PUBLIC (new_node->decl) = TREE_PUBLIC (origNode->decl);
      DECL_COMDAT (new_node->decl) = DECL_COMDAT (origNode->decl);
      DECL_WEAK (new_node->decl) = DECL_WEAK (origNode->decl);
      DECL_EXTERNAL (new_node->decl) = DECL_EXTERNAL (origNode->decl);
      DECL_VISIBILITY_SPECIFIED (new_node->decl) = DECL_VISIBILITY_SPECIFIED (origNode->decl);
      DECL_VISIBILITY (new_node->decl) = DECL_VISIBILITY (origNode->decl);
      DECL_DLLIMPORT_P (new_node->decl) = DECL_DLLIMPORT_P (origNode->decl);
      if (DECL_ONE_ONLY (origNode->decl))
         make_decl_one_only (new_node->decl,DECL_ASSEMBLER_NAME (new_node->decl));

      /* The method cgraph_version_clone_with_body () will force the new
      symbol local.  Undo this, and inherit external visibility from
      the old node.  */
      new_node->local = origNode->local;
      new_node->externally_visible = origNode->externally_visible;
   }

   /* Mark clones with internal linkage as gc'able, so they will not be
   emitted unless the vectorizer can actually use them.  */
   if (!TREE_PUBLIC (new_node->decl))
      new_node->gc_candidate = true;
   new_node->gc_candidate = false;//mtcs_compile expand_all_functions  mark_functions_to_output需要赋值为false
   return new_node;
}


/**
 * 清除函数声明中参数的orig tree
 */
static void cleanParamOrig(tree fndecl)
{
   tree param;
   /* Remap the parameters and result and return them to the caller.  */
   for (param = DECL_ARGUMENTS (fndecl); param; param = DECL_CHAIN (param)){
      DECL_ABSTRACT_ORIGIN (param)=NULL_TREE;
   }
}


/* If the function in NODE is tagged as an elemental SIMD function,
   create the appropriate SIMD clones.  */
//原型 expand_simd_clones omp-simd-clone.cc
static struct cgraph_node *cloneKernel (MtcsClones *self,struct cgraph_node *origNode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   tree attr;
   bool explicit_p = true;
   printAttrs(DECL_ATTRIBUTES (origNode->decl));
   MtcsFuncType mtcsFuncType =  mtcs_info_get_func_type(origNode->decl);
   DECL_ATTRIBUTES (origNode->decl)=remove_attributes(DECL_ATTRIBUTES (origNode->decl),"noclone");
   gcc_assert(mtcsFuncType!=MTCS_FUNC_NOT);
   n_debug("mtcsclones.c cloneKernel 00 函数属性 原结点块数:%d\n",n_basic_blocks_for_fn (DECL_STRUCT_FUNCTION (origNode->decl)));
   aet_print_tree( DECL_ATTRIBUTES (origNode->decl));
   char *name=function_name(DECL_STRUCT_FUNCTION (origNode->decl));
   // explicit_p = false;
   /* Ignore
   #pragma omp declare simd
   extern int foo ();
   in C, there we don't know the argument types at all.  */
   if (!origNode->definition && TYPE_ARG_TYPES (TREE_TYPE (origNode->decl)) == NULL_TREE){
      n_debug("mtcsclones.c cloneKernel 11 node没有定义 返回空 %s\n",name);
      return NULL;
   }

   /* Call this before creating clone_info, as it might ggc_collect.  */
   if (origNode->definition && origNode->has_gimple_body_p ()){
      n_debug("mtcsclones.c cloneKernel 22 节点获取函数体 origNode->get_body ();%s\n",name);
      origNode->get_body ();
   }
    n_debug("mtcsclones.c cloneKernel 33 printSymbolNode name:%s num_ssa_names:%d \n",name,num_ssa_names);
    printSymbolNode();
    //n_debug("mtcsclones.c cloneKernel 44 printNodeBB name:%s num_ssa_names:%d \n",name,num_ssa_names);
   // printNodeBB(origNode);

   /* Only when we are sure we want to create the clone actually
   clone the function (or definitions) or create another
   extern FUNCTION_DECL (for prototypes without definitions).  */
   struct cgraph_node *cloneNode = mtcsCloneCreate(self,origNode, !explicit_p);
   n_debug("mtcsclones.c cloneKernel 55 克隆后的主机节点 name:%s\n",name);
   printSymbolNode();
   //printNodeBB(origNode);

   //symbol_table::remove_unreachable_nodes 会判断 DECL_ABSTRACT_ORIGIN (expr) 进入代码块。
   cloneNode->mark_force_output();
   DECL_ABSTRACT_ORIGIN (cloneNode->decl) = NULL_TREE;
   if (cloneNode == NULL){
      n_error("出错在 cloneKernel\n");
      return NULL;
   }

   tree origOptimization=DECL_FUNCTION_SPECIFIC_OPTIMIZATION (origNode->decl);
   tree newOptimization=DECL_FUNCTION_SPECIFIC_OPTIMIZATION (cloneNode->decl);

   //把主机的optimization_default_node赋给mtcs函数，在编译前会把mtcs平台的optimization_default_node再重新设给该函数。见mtcsport
   if (!DECL_FUNCTION_SPECIFIC_OPTIMIZATION (cloneNode->decl))
      DECL_FUNCTION_SPECIFIC_OPTIMIZATION (cloneNode->decl)  = optimization_default_node;

   n_debug("\nmtcsclones.c cloneKernel 66 Generated %s clone %s origOptimization:%p newOptimization:%p \n",
      (TREE_PUBLIC (cloneNode->decl) ? "global" : "local"),
      IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (cloneNode->decl)),origOptimization,newOptimization);

   n_debug("mtcsclones.c cloneKernel 77 清除原参数 decl:%p DECL_SAVED_TREE在这里已变为空的了\n",origNode->decl);
   cleanParamOrig(cloneNode->decl);

   struct function *origfn=DECL_STRUCT_FUNCTION (origNode->decl);
   struct function *newfn=DECL_STRUCT_FUNCTION (cloneNode->decl);
   n_debug("mtcsclones.c cloneKernel 88-1 function orig:%p new:%p\n",origfn,newfn);
   n_debug("mtcsclones.c cloneKernel 88-2 function origloops:%p newloops:%p\n",origfn->x_current_loops,newfn->x_current_loops);
   n_debug("mtcsclones.c cloneKernel 88-3 cfg orig:%p new:%p\n",origfn->cfg,newfn->cfg);
   n_debug("mtcsclones.c cloneKernel 88-4 cfg x_entry_block_ptr orig:%p new:%p\n",
         origfn->cfg->x_entry_block_ptr,newfn->cfg->x_entry_block_ptr);
   n_debug("mtcsclones.c cloneKernel 88-5 cfg x_entry_block_ptr count orig:%d new:%d\n",
         n_basic_blocks_for_fn(origfn),n_basic_blocks_for_fn(newfn));
   n_debug("mtcsclones.c cloneKernel 88-6 ssa_names host:%d mtcs:%d\n",
         vec_safe_length (origfn->gimple_df->ssa_names),vec_safe_length (newfn->gimple_df->ssa_names));

   //切换到设备的符号表
   symbol_table *save=symtab;
   symtab=mtcsTarget->symtab;
   n_debug("mtcsclones.c cloneKernel 99 克隆后的新符号表的节点 name:%s\n",name);
   printSymbolNode();
   //printNodeBB(cloneNode);
   //切换回主机的符号表
   symtab=save;
   return cloneNode;
}

void mtcs_clones_test_edge(MtcsClones *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   symbol_table *dest=mtcsTarget->symtab;
   struct cgraph_node *node;

   //切换到设备的符号表
   symbol_table *save=symtab;
   symtab=dest;
   n_debug("mtcsclones.c mtcs_clones_test_edget node 00 first_function:%p symtab:%p save:%p\n",symtab->nodes,symtab,save);
      struct cgraph_node *tx = symtab->first_function ();
   n_debug("mtcsclones.c mtcs_clones_test_edget node 11 first_function:%p symtab:%p save:%p\n",tx,symtab,save);
   if(!tx){
      symtab=save;
      return;
   }

   n_debug("mtcsclones.c mtcs_clones_test_edget node 22 first_function:%p %s\n",tx,tx->name());
   FOR_EACH_FUNCTION(node){
      struct cgraph_node *xx = cgraph_node::get (node->decl);
      n_debug("mtcsclones.c mtcs_clones_test_edget node:%p %s symtab:%p decl:%p declNode:%p\n",
            node,node->name(),symtab,node->decl,xx);
      //testedge(node,DECL_STRUCT_FUNCTION (node->decl),node->decl);
   }
   //切换到主机的符号表
   symtab=save;
}

static void testedge(struct cgraph_node *node,struct function *this_cfun,tree decl)
{
   if(!this_cfun)
      return;
   cgraph_edge *e;
   basic_block this_block;
   hash_set<gimple *> stmts;
   gimple_stmt_iterator gsi;
   bool error_found;

   /* Reach the trees by walking over the CFG, and note the
   enclosing basic-blocks in the call edges.  */
   FOR_EACH_BB_FN (this_block, this_cfun)     {
      for (gsi = gsi_start_phis (this_block);  !gsi_end_p (gsi); gsi_next (&gsi))
         stmts.add (gsi_stmt (gsi));
      for (gsi = gsi_start_bb (this_block); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         stmts.add (stmt);
         if (is_gimple_call (stmt)){
            cgraph_edge *e = node->get_edge (stmt);
            n_debug("mtcsclones.c testedge decl:%p this_cfun:%p node:%p e:%p stmt:%p\n",decl,this_cfun,node,e,stmt);
            aet_print_gimple(stmt);
         }
      }
   }
}

struct cgraph_node *mtcs_clones_clone_func (MtcsClones *self,struct cgraph_node *origNode)
{
   struct function *save=cfun;
   tree savec=current_function_decl;
   struct cgraph_node *ret=  cloneKernel(self,origNode);
   //替换内置函数为0
#undef cfun
   cfun=save;
#define cfun (cfun+0)
   current_function_decl =savec;
   return ret;
}

/**
 * 克隆主机全局变量到MTCS目标
 */
struct varpool_node *mtcs_clones_clone_var(MtcsClones *self,struct varpool_node *origNode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsVar *mtcsVar  = mtcs_target_get_var(mtcsTarget);

   struct varpool_node *cloneNode = mtcs_var_clone(mtcsVar,origNode);
   return cloneNode;
}


MtcsClones *mtcs_clones_new(MtcsMode *mtcsMode)
{
   MtcsClones *self = n_slice_alloc0 (sizeof(MtcsClones));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsClonesInit(self);
   self->mtcsReplace=mtcs_replace_new(mtcsMode);
   return self;
}
