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


#ifndef __GCC_MTCS_CFG_LOOPANAL__
#define __GCC_MTCS_CFG_LOOPANAL__

#include "../nlib.h"
#include "mtcscomponent.h"

typedef struct _MtcsCfgLoopanal MtcsCfgLoopanal;
struct _MtcsCfgLoopanal
{
   MtcsComponent parent;
   struct target_cfgloop *targetCfgLoop;
};

MtcsCfgLoopanal *mtcs_cfg_loopanal_new(MtcsMode *mtcsMode);
//原型 init_set_costs cfgloop.h cfgloopanal.cc
void mtcs_cfg_loopanal_init_set_costs (MtcsCfgLoopanal *self);

#endif
