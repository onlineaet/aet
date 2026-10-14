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
#include "gimple.h"
#include "alloc-pool.h"
#include "timevar.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs-libfuncs.h"
#include "insn-config.h"
#include "ira.h"
#include "recog.h"
#include "cgraph.h"
#include "coverage.h"
#include "diagnostic.h"
#include "varasm.h"
#include "tree-inline.h"
#include "realmpfr.h"   /* For GMP/MPFR/MPC versions, in print_version.  */
#include "version.h"
#include "flags.h"
#include "insn-attr.h"
#include "output.h"
#include "toplev.h"
#include "expr.h"
#include "intl.h"
#include "tree-diagnostic.h"
#include "reload.h"
#include "lra.h"
#include "dwarf2asm.h"
#include "debug.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "cfgloop.h" /* for init_set_costs */
#include "hosthooks.h"
#include "opts.h"
#include "opts-diagnostic.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tsan.h"
#include "plugin.h"
#include "context.h"
#include "pass_manager.h"
#include "auto-profile.h"
#include "dwarf2out.h"
#include "ipa-reference.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-utils.h"
#include "gcse.h"
#include "omp-offload.h"
#include "edit-context.h"
#include "tree-pass.h"
#include "dumpfile.h"
#include "ipa-fnsummary.h"
#include "dump-context.h"
#include "print-tree.h"
#include "optinfo-emit-json.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "ipa-param-manipulation.h"
#include "dbgcnt.h"
#include "gcc-urlifier.h"

#include "tree-pass.h"
#include "stringpool.h"
#include "gimple-ssa.h"
#include "cgraph.h"
#include "coverage.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "output.h"
#include "cfgcleanup.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "tree-into-ssa.h"
#include "tree-ssa.h"
#include "langhooks.h"
#include "toplev.h"
#include "debug.h"
#include "symbol-summary.h"
#include "tree-ssanames.h"
#include "tree-dfa.h"
#include "tree-phinodes.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "tree-iterator.h"
#include "ssa-iterators.h"
#include "gimple.h"
#include "tree-outof-ssa.h"
#include "tree-eh.h"

#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "gimple-pretty-print.h"
#include "plugin.h"
#include "ipa-fnsummary.h"
#include "ipa-utils.h"
#include "except.h"
#include "cfgloop.h"
#include "context.h"
#include "pass_manager.h"
#include "tree-nested.h"
#include "dbgcnt.h"
#include "lto-section-names.h"
#include "stringpool.h"
#include "attribs.h"
#include "ipa-inline.h"
#include "omp-offload.h"
#include "symtab-thunks.h"
#include "ipa-reference.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "tree-ssa-operands.h"
#include "cfghooks.h"

#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"
#include "mtcsprintrtl.h"

#include "mtcstool.h"
#include "mtcsasm.h"

#include "aet/aetprinttree.h"
#include "ptx/ptxtool.h"
#include "mtcstree.h"

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
 * -mtcs=ptx:sm_52@7.8,sm_75@7.8
 * platfomr 平台 0 ptx 1 gcn 2...
 */
#define MAX_TARGETS 8
#define MAX_ARCHS   32

typedef struct {
   char name[32];
   struct {
      char name[32];
      int major;
      int minor;
   } archs[MAX_ARCHS];
   int arch_count;
} TargetData;


typedef struct
{
   TargetData targets[MAX_TARGETS];
   int target_count;
} TargetConfig;

static nboolean parse_version(char *str, int *major, int *minor)
{
   char *endchar;
   char *dot = strchr(str,'.');
   if (!dot){
      *major = strtol(str,&endchar,10);
      if (*major == 0 && str[0] != '0')
         return FALSE;
      *minor = 0;
      return TRUE;
   }
   *dot = 0;
   // 不允许多个 '.'
   if (strchr(dot + 1,'.'))
      return FALSE;
   if (*str == 0 || *(dot+1)==0)
      return FALSE;
   *major = strtol(str,&endchar,10);
   *minor = strtol(dot+1,&endchar,10);
   return TRUE;
}


static nboolean parse_arch(char *text,TargetData *target)
{
   if (*text == 0)
      return FALSE;
   if (target->arch_count >= MAX_ARCHS)
      return FALSE;
   char *at = strchr(text,'@');
   int major = 0;
   int minor = 0;
   if (at){
      *at = 0;
      // 防止 sm_50@7.5@1
      if (strchr(at+1,'@'))
         return FALSE;
      if (!parse_version(at+1,&major,&minor))
         return FALSE;
   }
   // arch 名不能为空
   if (*text == 0)
      return FALSE;
   strcpy(target->archs[target->arch_count].name,text);
   target->archs[target->arch_count].major = major;
   target->archs[target->arch_count].minor = minor;
   target->arch_count++;
   return TRUE;
}

static nboolean parse_target(char *text, TargetData *target)
{
    char *colon = strchr(text,':');
    if (!colon)
        return FALSE;
    *colon = 0;
    // backend不能为空
    if (*text == 0)
        return FALSE;
    strcpy(target->name,text);
    char *arch_list = colon + 1;
    if (*arch_list == 0)
        return FALSE;
    char *saveptr;
    char *arch = strtok_r(arch_list, ",", &saveptr);
    int count = 0;
    while (arch){
        if (!parse_arch(arch,target))
            return FALSE;
        count++;
        arch = strtok_r(NULL, ",", &saveptr);
    }
    // 防止 "ptx:,"
    if (count == 0)
        return FALSE;
    return TRUE;
}


static nboolean parse_mtcs_target(char *option,TargetConfig *config)
{
   memset(config,0,sizeof(*config));
   char *eq=strchr(option,'=');
   if (!eq || *(eq+1)==0)
      return FALSE;
   char *value=eq+1;
   char *saveptr;
   char *target=strtok_r(value, ";", &saveptr);
   int count=0;
   while(target){
      if (config->target_count >= MAX_TARGETS)
         return FALSE;
      if (!parse_target(target, &config->targets[config->target_count]))
         return FALSE;
      config->target_count++;
      count++;
      target=strtok_r(NULL, ";", &saveptr);
   }

   if(count==0)
      return FALSE;
   return TRUE;
}

static nboolean check_duplicate_arch(TargetConfig *config)
{
   for (int i = 0; i < config->target_count; i++){
      TargetData *target = &config->targets[i];
      for (int j = 0; j < target->arch_count; j++){
         for (int k = j + 1; k < target->arch_count; k++){
            if (strcmp(target->archs[j].name,target->archs[k].name) == 0){
               /*
               * 同一个 ISA
               */
               if (target->archs[j].major == target->archs[k].major
               && target->archs[j].minor == target->archs[k].minor){
                  error("duplicate mtcs target: %s:%s@%d.%d\n",
                     target->name,
                     target->archs[j].name,
                     target->archs[j].major,
                     target->archs[j].minor);

               }else{
                  error("conflicting mtcs target: %s:%s has different ptx version %d.%d and %d.%d\n",
                     target->name,
                     target->archs[j].name,
                     target->archs[j].major,
                     target->archs[j].minor,
                     target->archs[k].major,
                     target->archs[k].minor);
               }
               return FALSE;
            }
         }
      }
   }
   return TRUE;
}

static nboolean findVersionInfo(int target,int isa,int version,int **mtcsVersionInfo,int len)
{
   for(int i=0;i<len;i++){
      int *temp=mtcsVersionInfo[i];
      if(temp[0]==target && temp[1]==isa && temp[2]==version)
         return TRUE;
   }
   return FALSE;
}

static int  convertIsaAndPtxVersion(TargetConfig *config,int **mtcsVersionInfo)
{
   int count = 0;
   for(int i=0;i<config->target_count;i++){
      TargetData *t=&config->targets[i];
      if(!strcmp(t->name,"ptx")){
         printf("target=%s\n",t->name);
         for(int j=0;j<t->arch_count;j++){
            int isa = 0;
            int version = 0;
            nboolean ret =  ptx_tool_get_isa_and_version(t->archs[j].name,t->archs[j].major,
                     t->archs[j].minor,&isa,&version);
            if(!ret){
               error("编译参数中的架构与ptx版本不匹配 target:%s isa:%s ptx version %d.%d\n",
                  t->name,
                  t->archs[j].name,
                  t->archs[j].major,
                  t->archs[j].minor);
               return 0;
            }
            if(!findVersionInfo(TARGET_PTX,isa,version,mtcsVersionInfo,count)){
               int *temp=  mtcsVersionInfo[count];
               temp[0] = TARGET_PTX;
               temp[1] = isa;
               temp[2] = version;
               count++;
            }
         }
      }
   }
   return count;
}


int mtcs_tool_parser_mtcs_option(int **mtcsVersionInfo)
{
   //example: "-mtcs=ptx:sm_50,sm_75@7.3sms;gcn:gcn_32@7.5";
   static const char *SEPARATION="#$%"; //与gcc.cc中 setAetArgv函数中定义的相同
   char *options=getenv ("GCC_AET_MTCS_ARGV");
   if(options==NULL)
      return 0;
   char *option = NULL;
   char** items= n_strsplit(options,SEPARATION,-1);
   int len=n_strv_length(items);
   int count=0;
   for(int i=0;i<len;i++){
      char *ret=items[i];
      if(ret && strlen(ret)>=6 && !strncmp(ret,"-mtcs=",6)){
         option = n_strdup(ret);
         break;
      }
   }
   n_strfreev(items);
   if(option==NULL)
      return 0;
   n_debug("编译参数'-mtcs':%s\n",option);
   TargetConfig config;
   nboolean ret = parse_mtcs_target(option,&config);
   if(!ret){
      error("invalid -mtcs option: %s",option);
      return 0;
   }
   for(int i=0;i<config.target_count;i++){
      TargetData *t=&config.targets[i];
      printf("target=%s\n",t->name);
      for(int j=0;j<t->arch_count;j++){
         printf("  %s %d.%d\n",
             t->archs[j].name,
             t->archs[j].major,
             t->archs[j].minor);
      }
   }
   free(option);
   if (!check_duplicate_arch(&config)){
      return 0;
   }
   //todo 需要合并一目标，现在暂时不实现。
   return convertIsaAndPtxVersion(&config,mtcsVersionInfo);
}
