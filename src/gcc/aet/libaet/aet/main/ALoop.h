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

#ifndef __AET_MAIN_A_LOOP_H__
#define __AET_MAIN_A_LOOP_H__

#include "../../aet.h"
#include "ASourceMgr.h"

package$ aet.main;


public$ class$ ALoop{

    private$ ASourceMgr *mgr;
    private$ aboolean isRunning; /* 原子操作 */
    public$ void       run();
    public$ void       quit();
    public$ ASourceMgr *getSourceMgr();
    public$ ALoop(ASourceMgr *mgr,aboolean isRunning);
    public$ aboolean isRunning();

};




#endif /* __N_MEM_H__ */

