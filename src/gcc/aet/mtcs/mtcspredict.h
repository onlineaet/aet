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

#ifndef __GCC_MTCS_PREDICT__
#define __GCC_MTCS_PREDICT__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsPredict MtcsPredict;
struct _MtcsPredict
{
    MtcsComponent parent;


};

MtcsPredict *mtcs_predict_new(MtcsMode *mtcsMode);
//原型 optimize_insn_for_size_p predict.h predict.cc
optimize_size_level mtcs_predict_optimize_insn_for_size_p (MtcsPredict *self);
//原型 optimize_insn_for_speed_p predict.h predict.cc
bool mtcs_predict_optimize_insn_for_speed_p (MtcsPredict *self);


#endif

