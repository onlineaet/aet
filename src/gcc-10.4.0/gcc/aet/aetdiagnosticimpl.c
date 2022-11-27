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
#include "aetexpr.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "classmgr.h"
#include "classimpl.h"
#include "classbuild.h"


int    aet_demangle_text(const char *start,int length,char *newStr)
{
    char *cm=start;
    if(cm[0]=='_' && cm[1]=='Z'){
        ClassFunc  *func=func_mgr_get_entity_by_mangle(func_mgr_get(),start);
        //printf("是否找到了函数:%p %s\n",func,start);
        if(func){
           if(func->isCtor){
               sprintf(newStr,"构造函数 %s",func->orgiName);
               return strlen(newStr);
           }
           sprintf(newStr,"%s",func->orgiName);
           return strlen(newStr);
        }
    }
    return 0;
}




