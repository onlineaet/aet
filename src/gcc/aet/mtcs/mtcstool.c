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
 * base on stmt.cc
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

#include "mtcstool.h"
#include "mtcsasm.h"
#include "mtcscompile.h"
#include "../mtcsinfo.h"

#include "aet/aetutils.h"
#include "aet/aetprinttree.h"
#include "ptx/ptxtool.h"

nboolean mtcs_tool_is_mtcs_var(tree decl)
{
    tree att=DECL_ATTRIBUTES (decl);
    if(!aet_utils_valid_tree(att))
        return FALSE;
    const char* varName=IDENTIFIER_POINTER(DECL_NAME(decl));
    if (lookup_attribute (MTCS_DEVICE_STRING, att) || lookup_attribute (MTCS_CONSTANT_STRING, att)
            || lookup_attribute (MTCS_SHARED_STRING, att) || lookup_attribute ("texture", att)){
        return TRUE;
    }
    //如果是在核函数内，说明是local_mem,如何判断decl中核函数内?
    return FALSE;
}


static tree copy_decl_maybe_to_var (tree decl, copy_body_data *id)
{
    printf("copy_decl_maybe_to_var -----xx %p\n",decl);
  //if (TREE_CODE (decl) == PARM_DECL || TREE_CODE (decl) == RESULT_DECL)
  //  return copy_decl_to_var (decl, id);
  //else
    return copy_decl_no_change (decl, id);
}

/**
 * 复制变量 包括rtl
 */
tree mtcs_tool_copy_var(tree var)
{
    copy_body_data id;
    tree param;
    hash_map<tree, tree> decl_map;

    memset (&id, 0, sizeof (id));
    id.src_fn = current_function_decl;
    id.dst_fn = current_function_decl;
    // id.src_cfun = DECL_STRUCT_FUNCTION (fn);
    id.decl_map = &decl_map;

    id.copy_decl = copy_decl_maybe_to_var;//自定义copy var //tree-inline.h声明有两个 copy_decl_to_var;//copy_decl_no_change;
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
    n_debug("mtcs_tool_copy_var -----xx current_function_decl:%p\n",current_function_decl);

    /* We're not inside any EH region.  */
    id.eh_lp_nr = 0;
   // tree srcVar=var;
    //tree newVar =remap_decl(var,&id);//copy_decl_no_change(var,&id);
    tree newVar = copy_node (var);

    //walk_tree (&srcVar, copy_tree_body_r, &id, NULL);
   // tree newVar =srcVar;
    n_debug("mtcs_tool_copy_var 原始: newVar:%p orig:%p\n",newVar,var);
    n_debug("mtcs_tool_copy_var 新:\n");
    rtx origDeclRtl=DECL_WRTL_CHECK (var)->decl_with_rtl.rtl;
    if(origDeclRtl){
        n_debug("新变量在的rtx :%p\n",DECL_WRTL_CHECK (newVar)->decl_with_rtl.rtl);
        rtx newRtx=copy_rtx(origDeclRtl);
        print_rtl(stderr,origDeclRtl);
        fprintf(stderr,"\n");
        print_rtl(stderr,newRtx);
        fprintf(stderr,"\n");
        DECL_WRTL_CHECK (var)->decl_with_rtl.rtl=newRtx;
    }
    return newVar;
}

void  mtcs_tool_print_cfun_loop()
{
   int i;
   class loop *loop;
   for (auto loop : loops_list (cfun, LI_INCLUDE_ROOT)){
        n_debug("mtcstool.c mtcs_tool_print_cfun_loop  00 loop %p num:%d header:%p number_of_loops (cfun):%d superloops:%p\n",
            loop,loop->num, loop->header,number_of_loops (cfun),loop->superloops);
   }
   FOR_EACH_VEC_SAFE_ELT (get_loops (cfun), i, loop){
      if(loop)
         n_debug("mtcstool.c mtcs_tool_print_cfun_loop  11 loop %p num:%d header:%p number_of_loops (cfun):%d superloops:%p\n",
                 loop,loop->num, loop->header,number_of_loops (cfun),loop->superloops);
      else
         n_debug("mtcstool.c mtcs_tool_print_cfun_loop 22 是空的\n");
   }
}

static unsigned int createHashCode(const uchar *str,size_t len)
{
   const uchar *cur;
   unsigned int hash = HT_HASHSTEP (0, *str);
   cur=str+1;
   while (ISIDNUM (*cur)){
      hash = HT_HASHSTEP (hash, *cur);
      cur++;
   }
   hash = HT_HASHFINISH (hash, len);
   return hash;
}

//创建写入汇编代码的变量名，每个文件有1到n个，具体数量由需要编译的平台数决定
char    *mtcs_tool_create_asm_varname(char *platform,int isa,int version,char *fname)
{
   if(fname){
      unsigned int hashcode=createHashCode(fname,strlen(fname));
      char *ret=n_strdup_printf("%s_%s_%d_%d_%u",MTCS_ASM_VARNAME_PREFIX,platform,isa,version,hashcode);
      return ret;
   }else{
      unsigned int value=0;
      int random=get_random_seed (false);
      if(random<0)
         value=random*-1;
      else
         value=random;
      char *ret=n_strdup_printf("%s_%s_%d_%d_%d",MTCS_ASM_VARNAME_PREFIX,platform,isa,version,value);
      return ret;
   }
}
/**
 * 解析从gcc.cc写入的mtcs参数
 * -mtcs=sm_75,ptx
 * platfomr 平台 0 ptx 1 gcn 2...
 */
int mtcs_tool_get_isa_and_version(int **isaAndVersion,int platform)
{
   static const char *SEPARATION="#$%"; //与gcc.cc中 setAetArgv函数中定义的相同
   char *version=getenv ("GCC_AET_MTCS_ARGV");
   if(version==NULL)
      return 0;
   n_debug("从编译参数'-mtcs'传递平台版本号 version:%s\n",version);
   char** items= n_strsplit(version,SEPARATION,-1);
   int len=n_strv_length(items);
   int i,j;
   int count=0;
   for(i=0;i<len;i++){
      char *ret=items[i];
      if(!ret || strlen(ret)<=6 || strncmp(ret,"-mtcs=",6))
         continue;

      char *data=ret+6;
      int size=strlen(data);
      int pos=-1;
      for(j=0;j<size;j++)
         if(data[j]==','){
            pos=j;
            break;
         }
      char first[size];
      char *second=NULL;
      int result[2];
      nboolean ok=FALSE;
      if(pos!=-1){
         strncpy(first,data,pos);
         first[pos]='\0';
         second=data+pos+1;
         ok = ptx_tool_get_isa_and_version(first,second,result);
      }else{
         ok =ptx_tool_get_isa_and_version(data,second,result);
      }
      if(ok){
         //检查是否有重复的
         int z;
         nboolean repeat=FALSE;
         for(z=0;z<count;z++){
            if(isaAndVersion[z][0]==result[0] && isaAndVersion[z][1]==result[1]){
               repeat=TRUE;
               break;
            }
         }
         if(!repeat){
            isaAndVersion[count][0]=result[0];
            isaAndVersion[count][1]=result[1];
            count++;
         }
      }
   }
   return count;
}



