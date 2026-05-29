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

#ifndef __AET_MTCS_MEM_MGR_H__
#define __AET_MTCS_MEM_MGR_H__


#include "../../aet.h"
#include "../util/AHashTable.h"
#include "MtcsEventListener.h"

package$ aet.mtcs;



public$  class$ MtcsMemMgr implements$ MtcsEventListener{
    public$ static MtcsMemMgr *getInstance();
    private$ AHashTable *sourcesHash;
    public$ void add(int provider,int num,unsigned long add,auint64 size);
    public$ void get(unsigned long address,int *provider,int *devNum);
    public$ void remove(unsigned long add);
};

#endif /* __N_MEM_H__ */

