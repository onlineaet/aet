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

#ifndef __AET_GENERIC_H__
#define __AET_GENERIC_H__


typedef enum {
	GENERIC_TYPE_CHAR,
	GENERIC_TYPE_SIGNED_CHAR,
	GENERIC_TYPE_UCHAR,
	GENERIC_TYPE_SHORT,
	GENERIC_TYPE_USHORT,
	GENERIC_TYPE_INT,
	GENERIC_TYPE_UINT,
	GENERIC_TYPE_LONG,
	GENERIC_TYPE_ULONG,
	GENERIC_TYPE_LONG_LONG,
	GENERIC_TYPE_ULONG_LONG,
	GENERIC_TYPE_FLOAT,
	GENERIC_TYPE_DOUBLE,
	GENERIC_TYPE_LONG_DOUBLE,
	GENERIC_TYPE_DECIMAL32_FLOAT,
	GENERIC_TYPE_DECIMAL64_FLOAT,
	GENERIC_TYPE_DECIMAL128_FLOAT,
	GENERIC_TYPE_COMPLEX_CHAR,
	GENERIC_TYPE_COMPLEX_UCHAR,
	GENERIC_TYPE_COMPLEX_SHORT,
	GENERIC_TYPE_COMPLEX_USHORT,
	GENERIC_TYPE_COMPLEX_INT,
	GENERIC_TYPE_COMPLEX_UINT,
	GENERIC_TYPE_COMPLEX_LONG,
	GENERIC_TYPE_COMPLEX_ULONG,
	GENERIC_TYPE_COMPLEX_LONG_LONG,
	GENERIC_TYPE_COMPLEX_ULONG_LONG,
	GENERIC_TYPE_COMPLEX_FLOAT,
	GENERIC_TYPE_COMPLEX_DOUBLE,
	GENERIC_TYPE_COMPLEX_LONG_DOUBLE,
	GENERIC_TYPE_FIXED_POINT,
	GENERIC_TYPE_ENUMERAL,
	GENERIC_TYPE_BOOLEAN,
	GENERIC_TYPE_STRUCT,
	GENERIC_TYPE_UNION,
	GENERIC_TYPE_CLASS,
	GENERIC_TYPE_UNKNOWN=-1,
}GenericType;

typedef struct _aet_generic_info{
	char typeName[100];
	char genericName;
	char type;
    char pointerCount;
	int  size;
}aet_generic_info;

#define generic_is_pointer(x) \
({ \
	aet_generic_info info=generic_info$(x); \
	int result=0; \
	if(info.pointerCount==-1) \
	    result= 0; \
	else \
	    result= info.pointerCount; \
    result;\
  })


#define generic_type(x) \
({ \
	aet_generic_info info=generic_info$(x); \
	GenericType result=info.type; \
    result;\
  })

#define generic_type_obj(obj,x) \
({ \
	aet_generic_info info=generic_info$(obj,x); \
	GenericType result=info.type; \
    result;\
  })

/*************************新版泛型********************************/
typedef struct _AetGenericFuncInfo
{
   aet_generic_info _generic_1234_array[10];
   int unitCount;
   char sysName[100];//泛型函数所在的类名
   int index;//泛型函数中的泛型块的序号
}AetGenericFuncInfo;

extern __thread AetGenericFuncInfo  _gen_func_block_addr_128347;//名字来自 aetmicro.h


//生成函数地址
typedef struct _BlockFuncData
{
   void *address;
   char *funcName;
   char *genericModel;
   char *sysName;
   int index;
}BlockFuncData;

void add_generic_data(BlockFuncData *data,int len);
//aet_generic_class_fill_address函数名来自aetmicro.h中的定义
void aet_generic_class_fill_address(aet_generic_info *info,int len,char *sysName,void **address);
//aet_generic_func_get_address 函数名来自aetmicro.h中的定义
void *aet_generic_func_get_address(int index,aet_generic_info *classInfo,int len);


#endif /* ! GCC_C_AET_H */
