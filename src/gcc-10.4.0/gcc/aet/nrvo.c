/*
   Copyright (C) 2022 zclei@sina.com.

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
#include "attribs.h"
#include "toplev.h"

#include "stor-layout.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"
#include "c/c-tree.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classbuild.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classbuild.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"
#include "varmgr.h"

static void nrvoInit(Nrvo *self)
{
}

Nrvo *nvro_get()
{
    static Nrvo *singleton = NULL;
    if (!singleton){
         singleton =n_slice_alloc0 (sizeof(Nrvo));
         nrvoInit(singleton);
    }
    return singleton;
}



