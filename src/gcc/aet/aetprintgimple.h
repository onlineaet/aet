/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

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

#ifndef __GCC_AET_PRINT_GIMPLE_H__
#define __GCC_AET_PRINT_GIMPLE_H__

#include "nlib.h"




#define aet_print_gimple(gimple)        aet_print_gimple_from (gimple,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_seq(seq)              aet_print_seq_from (seq,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_cgraph_node(node)     aet_print_cgraph_node_from (node,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_block(bb)             aet_print_block_from (bb,__FILE__,__FUNCTION__,__LINE__)


void   aet_print_gimple_from(gimple *g,const char *file,const char *func,int line);
void   aet_print_seq_from(gimple_seq seq,const char *file,const char *func,int line);
void   aet_print_cgraph_node_from(struct cgraph_node *node,const char *file,const char *func,int line);
void   aet_print_block_from(basic_block bb,const char *file,const char *func,int line);
void   aet_print_gimple_skip_debug(gimple *g);

#endif

