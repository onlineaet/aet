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

#ifndef __GCC_MTCS_MACHINE__
#define __GCC_MTCS_MACHINE__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "targetmemtag.h"
#include "targetc.h"
#include "targetvectorize.h"
#include "targetaddrspace.h"
#include "targetoption.h"
#include "targetcommon.h"
#include "targetemutls.h"
#include "targetasmout.h"
#include "targetcalls.h"
#include "targetrtx.h"


typedef struct _MtcsMachine MtcsMachine;
struct _MtcsMachine
{
    MtcsComponent    parent;
    TargetMemTag    *memTag;
    TargetC         *c;
    TargetVectorize *vectorize;
    TargetAddrSpace *addrSpace;
    TargetOption    *option;
    TargetCommon    *common;
    TargetEmutls    *emutls;
    TargetAsmOut    *asmOut;
    TargetCalls     *calls;
    TargetRtx       *tmrtx;

};

MtcsMachine *mtcs_machine_new(MtcsMode *mtcsMode);
void         mtcs_machine_set_vectorize(MtcsMachine *self,TargetVectorize *vectorize);
void         mtcs_machine_set_addr_space(MtcsMachine *self,TargetAddrSpace *addrSpace);
void         mtcs_machine_set_option(MtcsMachine *self,TargetOption *option);
void         mtcs_machine_set_common(MtcsMachine *self,TargetCommon *common);
void         mtcs_machine_set_asm_out(MtcsMachine *self,TargetAsmOut *asmOut);
void         mtcs_machine_set_calls(MtcsMachine *self,TargetCalls *calls);
void         mtcs_machine_set_tmrtx(MtcsMachine *self,TargetRtx *tmrtx);

#endif
