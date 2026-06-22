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
 * AET was originally developed  by the zclei@sina.com
 */

#ifndef __AET_MTCS_COMPILER_H__
#define __AET_MTCS_COMPILER_H__

#include "../../aet.h"
#include "MtcsModule.h"

package$ aet.mtcs;

public$ abstract$ class$ MtcsCompiler {

   private$ char *bin;//二进制代码
   private$ int binSize;//大小
   protected$ MtcsModule *mtcsModule;
   public$ abstract$ void compile();
   public$ MtcsModule *getModule();
   public$ void setBin(char *bin,int binSize);
   public$ char *getBin(int *binSize);
};

#endif /* __N_MEM_H__ */

