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

#ifndef __GCC_MTCS_COMPILE__
#define __GCC_MTCS_COMPILE__

#include "../nlib.h"
#include "mtcstarget.h"
#include "cgraph.h"
#include "mtcspassmgr.h"
#include "../aetmediator.h"
#include "mtcsadjustpass.h"

typedef struct _MtcsCompile MtcsCompile;
struct _MtcsCompile
{
   AetMediatorUser mediatorUser; //实现中介者接口
   MtcsTarget *targets[10];
   int targetCount;
   MtcsTarget *currentMtcsTarget;
   nboolean haveMtcsFuncOrVar;
   nboolean running;
   //有些变量是因为优化内部生成的，如果有MTCS函数引用到这些变量，则这些变量也成为MTCS的变量
   NPtrArray *undecidedVarArray;
   //禁用一些pass
   MtcsAdjustPass *mtcsAdjustPass;
};

MtcsCompile  *mtcs_compile_get();
MtcsTarget   *mtcs_compile_get_current_target(MtcsCompile *self);
void          mtcs_compile_compile(MtcsCompile *self);
nboolean      mtcs_compile_is_compiling(MtcsCompile *self);
void          mtcs_compile_test_edge(MtcsCompile *self);

#endif
