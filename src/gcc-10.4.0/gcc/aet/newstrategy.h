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

#ifndef __GCC_NEW_STRATEGY_H__
#define __GCC_NEW_STRATEGY_H__

#include "classinfo.h"
#include "classinit.h"
#include "nlib.h"

typedef enum{
	NEW_OBJECT_GLOBAL,
	NEW_OBJECT_LOCAL_STATIC,
	NEW_OBJECT_LOCAL,
}NewObjectType;

typedef struct _NewObjectInfo
{
	c_token       backTokens[10];
	int           backTokenCount; //变量赋值语句需要 var=new$ Abc(); Abc abc=new$ Abc()不需要
    ClassName    *name;
    char         *varName;
    NewObjectType varType;
}NewObjectInfo;


typedef struct _NewStrategy NewStrategy;
/* --- structures --- */
struct _NewStrategy
{
	c_parser *parser;
    ClassInit *classInit;
	int nest;
	NewObjectInfo  *privs[20];
};

char      *new_strategy_recursion_init_parent_generic_info(NewStrategy *self,char *refVarName,ClassName *refClassName,nboolean deref);
char      *new_strategy_get_replace_generic_codes_by_self_parm_with_var(NewStrategy *self,
		                   char *varName,tree  genericDefine,ClassName *className,nboolean deref);
void       new_strategy_backup_token(NewStrategy *self);
void       new_strategy_restore_token(NewStrategy *self);
int        new_strategy_get_back_token_count(NewStrategy *self);
void       new_strategy_set_var_type(NewStrategy *self,NewObjectType type);
NewObjectType  new_strategy_get_var_type(NewStrategy *self);
void       new_strategy_add_new(NewStrategy *self,ClassName *className,char *varName);
void       new_strategy_recude_new(NewStrategy *self);
ClassName *new_strategy_get_class_name(NewStrategy *self);
char      *new_strategy_get_var_name(NewStrategy *self);
void       new_strategy_add_close_brace(NewStrategy *self);
void       new_strategy_new_object(NewStrategy *self,char *tempVarName,GenericModel *genericDefine,ClassName *className,char *ctorStr,NString *codes,nboolean addSemision);
void       new_strategy_new_object_from_stack(NewStrategy *self,tree var,ClassName *className,NString *codes);
void       new_strategy_new_object_from_field_stack(NewStrategy *self,tree var,ClassName *className,NString *codes);
void       new_strategy_new_object_from_stack_no_name(NewStrategy *self,char *varName,GenericModel *genericDefine,
		                  char *ctorStr,ClassName *className,NString *codes);//完成如 return new Abc();返回的是对象而不是指针。

void       new_strategy_set_parser(NewStrategy *self,c_parser *parser);

#endif

