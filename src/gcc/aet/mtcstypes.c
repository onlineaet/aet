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

#include "config.h"
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "plugin.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"


#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "mtcstypes.h"


static void mtcsTypesInit(MtcsTypes *self)
{
   self->init=FALSE;
}

/**
 * 创建dim3结构体
 */
static tree build_mtcs_type (char *name,int dim,tree fieldType,int align)
{
   static const char *field_name[] = {"x", "y", "z","w"};
   tree field = NULL_TREE, field_chain = NULL_TREE;
   int i;
   tree type = make_node (RECORD_TYPE);
   for (i = 0; i < dim; ++i){
      field = build_decl (UNKNOWN_LOCATION, FIELD_DECL, get_identifier (field_name[i]), fieldType);
      if (field_chain != NULL_TREE)
         DECL_CHAIN (field) = field_chain;
      field_chain = field;
   }
   //char _name[255];
  // sprintf(_name,"_%s",name);
   tree alignType=NULL_TREE;
   if(align>0)
      alignType = build_aligned_type (type, align);
   finish_builtin_struct (type, name, field_chain, alignType);
   return type;
}

typedef struct _BuiltinType{
   char *name;
   int dim;
   tree fieldType;
   int align;
   tree record;
}BuiltinType;

static BuiltinType *dataTypes=NULL;
static int typesCount;
//原型 /nvptx/cuda-12.2-include/vector_types.h
static void initTypes()
{
   //不能定义在文件中 signed_char_type_node、... 指向的树还未初始化
   static BuiltinType builtTypes[]={
      {"char1",1,signed_char_type_node,0,NULL_TREE},
      {"uchar1",1,unsigned_char_type_node,0,NULL_TREE},
      {"char2",2,signed_char_type_node,2,NULL_TREE},
      {"uchar2",2,unsigned_char_type_node,2,NULL_TREE},
      {"char3",3,signed_char_type_node,0,NULL_TREE},
      {"uchar3",3,unsigned_char_type_node,0,NULL_TREE},
      {"char4",4,signed_char_type_node,4,NULL_TREE},
      {"uchar4",4,unsigned_char_type_node,4,NULL_TREE},

      {"short1",1,short_integer_type_node,0,NULL_TREE},
      {"ushort1",1,short_unsigned_type_node,0,NULL_TREE},
      {"short2",2,short_integer_type_node,4,NULL_TREE},
      {"ushort2",2,short_unsigned_type_node,4,NULL_TREE},
      {"short3",3,short_integer_type_node,0,NULL_TREE},
      {"ushort3",3,short_unsigned_type_node,0,NULL_TREE},
      {"short4",4,short_integer_type_node,8,NULL_TREE},
      {"ushort4",4,short_unsigned_type_node,8,NULL_TREE},

      {"int1",1,integer_type_node,0,NULL_TREE},
      {"uint1",1,unsigned_type_node,0,NULL_TREE},
      {"int2",2,integer_type_node,8,NULL_TREE},
      {"uint2",2,unsigned_type_node,8,NULL_TREE},
      {"int3",3,integer_type_node,0,NULL_TREE},
      {"uint3",3,unsigned_type_node,0,NULL_TREE},
      {"int4",4,integer_type_node,16,NULL_TREE},
      {"uint4",4,unsigned_type_node,16,NULL_TREE},

      {"long1",1,long_integer_type_node,0,NULL_TREE},
      {"ulong1",1,long_unsigned_type_node,0,NULL_TREE},
      {"long2",2,long_integer_type_node,8,NULL_TREE},
      {"ulong2",2,long_unsigned_type_node,8,NULL_TREE},
      {"long3",3,long_integer_type_node,0,NULL_TREE},
      {"ulong3",3,long_unsigned_type_node,0,NULL_TREE},
      {"long4",4,long_integer_type_node,16,NULL_TREE},
      {"ulong4",4,long_unsigned_type_node,16,NULL_TREE},

      {"float1",1,float_type_node,0,NULL_TREE},
      {"float2",2,float_type_node,8,NULL_TREE},
      {"float3",3,float_type_node,0,NULL_TREE},
      {"float4",4,float_type_node,16,NULL_TREE},

      {"longlong1",1,long_long_integer_type_node,0,NULL_TREE},
      {"ulonglong1",1,long_long_unsigned_type_node,0,NULL_TREE},
      {"longlong2",2,long_long_integer_type_node,16,NULL_TREE},
      {"ulonglong2",2,long_long_unsigned_type_node,16,NULL_TREE},
      {"longlong3",3,long_long_integer_type_node,0,NULL_TREE},
      {"ulonglong3",3,long_long_unsigned_type_node,0,NULL_TREE},
      {"longlong4",4,long_long_integer_type_node,16,NULL_TREE},
      {"ulonglong4",4,long_long_unsigned_type_node,16,NULL_TREE},

      {"double1",1,double_type_node,0,NULL_TREE},
      {"double2",2,double_type_node,16,NULL_TREE},
      {"double3",3,double_type_node,0,NULL_TREE},
      {"double4",4,double_type_node,16,NULL_TREE},

      {"dim3",3,unsigned_type_node,0,NULL_TREE},

   };
   int len = ARRAY_SIZE (builtTypes);
   int i;
   for(i=0;i<len;i++){
      BuiltinType *item=&builtTypes[i];
      tree record=lookup_name (get_identifier (item->name));
      if(!record){
         record = build_mtcs_type(item->name,item->dim,item->fieldType,item->align);
         pushdecl (TYPE_NAME(record));
      }
      if(TREE_CODE(record)==TYPE_DECL)
         item->record=record;//得到是TYPE_DECL
      else
         item->record=TYPE_NAME(record);//得到是TYPE_DECL
   }
   dataTypes = builtTypes;
   typesCount = len;
}

tree mtcs_types_get_record(MtcsTypes *self,char *name)
{
   int len = ARRAY_SIZE (dataTypes);
   fprintf(stderr,"mtcs_types_get_record --- len:%d %d\n",len,typesCount);
   int i;
   for(i=0;i<typesCount;i++){
      BuiltinType *item=&dataTypes[i];
      if(strcmp(item->name,name)==0)
         return item->record;
   }
   return NULL_TREE;
}

void mtcs_types_init(MtcsTypes *self)
{
   if(self->init)
      return;
   initTypes();
   self->init=TRUE;
}

nboolean mtcs_types_is_init(MtcsTypes *self)
{
   return self->init;
}


MtcsTypes *mtcs_types_get()
{
   static MtcsTypes *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(MtcsTypes));
      mtcsTypesInit(singleton);
   }
   return singleton;
}

