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
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
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
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"

#include "ptxtool.h"
#include "ptx-common.h"

/* Return NULL if NAME contains no dot.  Otherwise return a copy of NAME
   with the dots replaced with dollar signs.  */

char *ptx_tool_replace_dot (const char *name)
{
  n_debug("ptxtool.c -----nvptx.cc -----5-- nvptx_replace_dot name:%s\n",name);
  if (strchr (name, '.') == NULL)
    return NULL;
  char *p = xstrdup (name);
  for (size_t i = 0; i < strlen (p); ++i)
    if (p[i] == '.')
      p[i] = '$';
  return p;
}

/* Return the PTX name of the data area in which SYM should be
   placed.  The symbol must have already been processed by
   nvptx_encode_seciton_info, or equivalent.  */
const char *ptx_tool_section_for_sym (rtx sym)
{
  ptx_data_area area = PTX_SYMBOL_DATA_AREA (sym);
  /* Same order as nvptx_data_area enum.  */
  static char const *const areas[] =
    {"", ".global", ".shared", ".local", ".const", ".param"};
  return areas[area];
}

/**
 * 原型 static enum ptx_version first_ptx_version_supporting_sm (enum ptx_isa sm) nvptx.cc
 */
PtxVersion ptx_tool_get_first_version_supporting_sm (PtxIsa sm)
{
  switch (sm)
    {
    case MTCS_PTX_ISA_SM_30:
      return /* PTX_VERSION_3_0 not defined */ MTCS_PTX_VERSION_3_1;
    case MTCS_PTX_ISA_SM_35:
      return MTCS_PTX_VERSION_3_1;
    case MTCS_PTX_ISA_SM_37:
    case MTCS_PTX_ISA_SM_52:
      return MTCS_PTX_VERSION_4_1;
    case MTCS_PTX_ISA_SM_53:
      return MTCS_PTX_VERSION_4_2;
    case MTCS_PTX_ISA_SM_61:
      return MTCS_PTX_VERSION_6_0;
    case MTCS_PTX_ISA_SM_70:
      return MTCS_PTX_VERSION_6_0;
    case MTCS_PTX_ISA_SM_75:
      return MTCS_PTX_VERSION_7_8;
    case MTCS_PTX_ISA_SM_80:
      return MTCS_PTX_VERSION_7_8;
    case MTCS_PTX_ISA_SM_89:
      return MTCS_PTX_VERSION_7_8;
    default:
      gcc_unreachable ();
    }
}

/**
 * 原型 static enum ptx_version default_ptx_version_option (void) nvptx.cc
 */
PtxVersion ptx_tool_get_default_version (PtxIsa sm)
{
  PtxVersion first = ptx_tool_get_first_version_supporting_sm (sm);
  /* Pick a version that supports the sm.  */
  PtxVersion res = first;

  /* Pick at least 6.0, to enable using bar.warp.sync to have a way to force
     warp convergence.  */
  res = MAX (res, MTCS_PTX_VERSION_6_0);

  /* Pick at least 6.3, to enable PTX '.alias'.  */
  res = MAX (res, MTCS_PTX_VERSION_6_3);

  /* For sm_52+, pick at least 7.3, to enable PTX 'alloca'.  */
  if (sm >= MTCS_PTX_ISA_SM_52)
    res = MAX (res, MTCS_PTX_VERSION_7_3);

  /* Verify that we pick a version that supports the sm.  */
  gcc_assert (first <= res);
  return res;
}

/**
 * static const char *ptx_version_to_string (enum ptx_version v) nvptx.cc
 */
const char *ptx_tool_version_to_string (PtxVersion v)
{
  switch (v)
    {
    case MTCS_PTX_VERSION_3_1:
      return "3.1";
    case MTCS_PTX_VERSION_4_1:
      return "4.1";
    case MTCS_PTX_VERSION_4_2:
      return "4.2";
    case MTCS_PTX_VERSION_6_0:
      return "6.0";
    case MTCS_PTX_VERSION_6_3:
      return "6.3";
    case MTCS_PTX_VERSION_7_0:
      return "7.0";
    case MTCS_PTX_VERSION_7_3:
      return "7.3";
    case MTCS_PTX_VERSION_7_8:
      return "7.8";
    default:
      gcc_unreachable ();
    }
}

/**
 * unsigned int ptx_version_to_number (enum ptx_version v, bool major_p) nvptx.cc
 */
unsigned int ptx_tool_version_to_number (PtxVersion v, bool major_p)
{
  switch (v)
    {
    case MTCS_PTX_VERSION_3_1:
      return major_p ? 3 : 1;
    case MTCS_PTX_VERSION_4_1:
      return major_p ? 4 : 1;
    case MTCS_PTX_VERSION_4_2:
      return major_p ? 4 : 2;
    case MTCS_PTX_VERSION_6_0:
      return major_p ? 6 : 0;
    case MTCS_PTX_VERSION_6_3:
      return major_p ? 6 : 3;
    case MTCS_PTX_VERSION_7_0:
      return major_p ? 7 : 0;
    case MTCS_PTX_VERSION_7_3:
      return major_p ? 7 : 3;
    case MTCS_PTX_VERSION_7_8:
      return major_p ? 7 : 8;
    default:
      gcc_unreachable ();
    }
}

/**
 * 原型 static const char *sm_version_to_string (enum ptx_isa sm) nvptx.cc
 */
const char *ptx_tool_sm_version_to_string (PtxIsa sm)
{
   switch (sm){
      case MTCS_PTX_ISA_SM_30:
         return "30";
      case MTCS_PTX_ISA_SM_35:
         return "35";
      case MTCS_PTX_ISA_SM_37:
         return "37";
      case MTCS_PTX_ISA_SM_52:
         return "52";
      case MTCS_PTX_ISA_SM_53:
         return "53";
      case MTCS_PTX_ISA_SM_70:
         return "70";
      case MTCS_PTX_ISA_SM_75:
         return "75";
      case MTCS_PTX_ISA_SM_80:
         return "80";
      case MTCS_PTX_ISA_SM_89:
         return "89";
      default:
         gcc_unreachable ();
   }
}

static int getIsa(char *arg)
{
   if(arg==NULL || strlen(arg)<4)
      return -1;
   if(strncmp(arg,"sm_",3))
      return -1;

   if(!strcmp(arg,"sm_30"))
      return MTCS_PTX_ISA_SM_30;
   else if(!strcmp(arg,"sm_35"))
      return MTCS_PTX_ISA_SM_35;
   else if(!strcmp(arg,"sm_37"))
      return MTCS_PTX_ISA_SM_37;
   else if(!strcmp(arg,"sm_52"))
      return MTCS_PTX_ISA_SM_52;
   else if(!strcmp(arg,"sm_53"))
      return MTCS_PTX_ISA_SM_53;
   else if(!strcmp(arg,"sm_70"))
      return MTCS_PTX_ISA_SM_70;
   else if(!strcmp(arg,"sm_75"))
      return MTCS_PTX_ISA_SM_75;
   else if(!strcmp(arg,"sm_80"))
      return MTCS_PTX_ISA_SM_80;
   else if(!strcmp(arg,"sm_89"))
      return MTCS_PTX_ISA_SM_89;
   else
      return -1;
}

/**
 * PTX ISA 版本
 */
static int getVersion(char *arg)
{
   if(arg==NULL || strlen(arg)==0)
      return -1;
   if(!strcmp(arg,"3.0") || !strcmp(arg,"3") )
      return MTCS_PTX_VERSION_3_1;
   else if(!strcmp(arg,"3.1"))
      return MTCS_PTX_VERSION_3_1;
   else if(!strcmp(arg,"4.2"))
      return MTCS_PTX_VERSION_4_2;
   else if(!strcmp(arg,"6.0") || !strcmp(arg,"6"))
      return MTCS_PTX_VERSION_6_0;
   else if(!strcmp(arg,"6.3"))
      return MTCS_PTX_VERSION_6_3;
   else if(!strcmp(arg,"7.0") || !strcmp(arg,"7"))
      return MTCS_PTX_VERSION_7_0;
   else if(!strcmp(arg,"7.3"))
      return MTCS_PTX_VERSION_7_3;
   else if(!strcmp(arg,"7.8"))
      return MTCS_PTX_VERSION_7_8;
   else
      return -1;
}
/**
 * 原型 static void handle_ptx_version_option (void) nvptx.cc
 */
nboolean ptx_tool_valid_isa_version (PtxIsa isa,PtxVersion version)
{
   PtxVersion first = ptx_tool_get_first_version_supporting_sm (isa);
   if (version < first)
     return FALSE;
   return TRUE;
}

/**
 * 编译器gcc收集到的运行参数如下：
 * -mtcs=sm_50,3.1
 */
nboolean ptx_tool_get_isa_and_version(char *first,char *second,int *result)
{
   if(first==NULL && second==NULL)
      return FALSE;
   int isa1=getIsa(first);
   int isa2=getIsa(second);
   int isa,version;
   nboolean ok=FALSE;
   if(isa1>0){
     version = getVersion(second);
     if(version==-1){
        version = ptx_tool_get_default_version(isa1);
     }
     isa=isa1;
     ok=TRUE;
   }else if(isa2>0){
      version = getVersion(first);
      if(version==-1){
         version = ptx_tool_get_default_version(isa2);
      }
      isa=isa2;
      ok=TRUE;
   }
   if(ok){
       PtxVersion first = ptx_tool_get_first_version_supporting_sm ((PtxIsa)isa);
       if (!ptx_tool_valid_isa_version((PtxIsa)isa,(PtxVersion)version)){
         // error ("PTX version (%<-mptx%>) needs to be at least %s to support selected"
              //     " %<-misa%> (sm_%s)", ptx_tool_version_to_string (first),
                //   ptx_tool_sm_version_to_string ((PtxIsa)isa));
          return FALSE;
       }
       result[0]=isa;
       result[1]=version;
   }
   return ok;

}
