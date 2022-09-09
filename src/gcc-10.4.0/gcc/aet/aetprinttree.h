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

#ifndef __GCC_AET_PRINT_TREE_H__
#define __GCC_AET_PRINT_TREE_H__

#include "nlib.h"




#define aet_print_token_in_parser(format...)  aet_print_parser_from(__FILE__,__FUNCTION__,__LINE__,format)
#define aet_print_tree(tree)    aet_print_tree_from (tree,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_token(ct)     aet_print_token_from (ct,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_specs(specs)  aet_print_specs_from (specs,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_declarator(declarator)  aet_print_declarator_from (declarator,__FILE__,__FUNCTION__,__LINE__)
#define aet_print_time(format...)  aet_print_time_from(__FILE__,__FUNCTION__,__LINE__,format)


void           aet_print_declarator_from(struct c_declarator *declarator,char *file,char *func,int line);
void           aet_print_specs_from(struct c_declspecs *specs,char *file,char *func,int line);
void           aet_print_tree_from(tree value,char *file,char *func,int line);
void           aet_print_token_from(c_token *ct,char *file,char *func,int line);
void           aet_print_parser_from(char *file,char *fun ,int line,char *format,...);
void           aet_print_set_parser(c_parser *parser);
void           aet_print_tree_skip_debug(tree node);
void           aet_print_token_skip_debug(c_token *ct);
void           aet_print_time_from(char *file,char *func ,int linen,char *format,...);

#endif

