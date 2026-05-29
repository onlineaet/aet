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

#ifndef __GCC_PTX_TOOL__
#define __GCC_PTX_TOOL__


#include "aet/nlib.h"
#include "ptx-common.h"


char        *ptx_tool_replace_dot (const char *name);
const char  *ptx_tool_section_for_sym (rtx sym);
nboolean     ptx_tool_get_isa_and_version(char *first,char *second,int *result);
nboolean     ptx_tool_valid_isa_version (PtxIsa isa,PtxVersion version);
const char  *ptx_tool_sm_version_to_string (PtxIsa sm);
const char  *ptx_tool_version_to_string (PtxVersion v);
PtxVersion   ptx_tool_get_first_version_supporting_sm (PtxIsa sm);

#endif

