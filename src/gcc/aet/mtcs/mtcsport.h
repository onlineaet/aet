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
 * base on output.cc
 */

#ifndef __GCC_MTCS_PORT__
#define __GCC_MTCS_PORT__

#include "../nlib.h"
#include "mtcscomponent.h"

typedef struct _MtcsPort MtcsPort;

struct _MtcsPort
{
    MtcsComponent parent;
    nboolean hostToDevice;//从主机的tree移到设备
    int processing_debug_stmt;
};


MtcsPort *mtcs_port_new(MtcsMode *mtcsMode);
void      mtcs_port_port(MtcsPort *self);
tree      mtcs_port_host_to_device(MtcsPort *self,tree hostTree);
void      mtcs_port_host_to_device(MtcsPort *self,tree *hostTree,int count);
tree      mtcs_port_device_to_host(MtcsPort *self,tree hostTree);
void      mtcs_port_device_to_host(MtcsPort *self,tree *hostTree,int count);

#endif
