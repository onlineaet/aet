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

#ifndef __GCC_AET_MICRO_H__
#define __GCC_AET_MICRO_H__

#define AET_MAX_TOKEN                       30
#define AET_ROOT_OBJECT                    "AObject"
#define AET_ROOT_CLASS                     "AClass"
#define AET_CLEANUP_OBJECT_METHOD          "a_object_cleanup_local_object_from_static_or_stack"
#define AET_CLEANUP_NAMELESS_OBJECT_METHOD "a_object_cleanup_nameless_object" //释放实参是new$ Object()的变量

#define AET_MAX_INTERFACE                   5 //一个类同时只能实现5个接口
//为每个类生成一个全局方法 com.aet.类名_init_object_类名的hash值
#define AET_INIT_GLOBAL_METHOD_STRING      "init_object"
#define AET_INNER_ARRAY_VARIABLE_NAME      "innerArrayRef0302"


#define AET_GENERIC_ARRAY                  "_generic_1234_array" //类中的泛型模型数组变量名
#define AET_GENERIC_BLOCK_ARRAY_VAR_NAME  "_gen_blocks_array_897" //类中的泛型块函数指针数组变量名 void *_gen_blocks_array_897[xx];
#define AET_MAX_GENERIC_BLOCKS             30  //类中最大块数

#define AET_GENERIC_TYPE_NAME_PREFIX       "aet_generic_" //泛型类型名的前缀，后缀是A-Z
#define AET_GET_GENERIC_INFO_FUNC_NAME     "generic_info$" //该方法定义在aobject.h头文件中
#define AET_GENERIC_INFO_STRUCT_NAME       "aet_generic_info"
#define AET_GENERIC_FUNC_THREAD_BLOCK_ADDR  "_gen_func_block_addr_128347" //泛型函数中块函数的地址 __thread _gen_func_block_add_128347
//新加 11-20
//填充泛型类中的泛型块变量地址 AET_GENERIC_BLOCK_ARRAY_VAR_NAME 在generic.c中定义
#define AET_GENERIC_CLASS_FILL_ARRAY_ADDR   "aet_generic_class_fill_address"
//获取在泛型函数中的块函数地址 在generic.c中定义
#define AET_GENERIC_FUNC_GET_ADDRESS        "aet_generic_func_get_address"
//2025-11-03 新加
#define GENERIC_BLOCK_FILE_NAME             "_block_func_" //泛型块函数输出文件名


//接口声明中的变量，由aet加入，变量指向实现该接口的类对象 IfaceCommonData123 定义在AObject.h中。
#define IFACE_AT_CLASS                     "_atClass123"
#define IFACE_COMMON_STRUCT_NAME           "IfaceCommonData123"
#define IFACE_COMMON_STRUCT_VAR_NAME       "_iface_common_var"
#define IFACE_REF_FIELD_NAME               "_iface_reserve_ref_field_123"
#define IFACE_UNREF_FIELD_NAME             "_iface_reserve_unref_field_123"
#define IFACE_REF_FUNC_DEFINE_NAME         "_iface_reserve_ref_func_define_123"
#define IFACE_UNREF_FUNC_DEFINE_NAME       "_iface_reserve_unref_func_define_123"
//super方法实现 在AObject.h中定义
#define AET_SUPER_ADDRESS_ARRAY             "_superAddressArray" //用来保存类方法实现的地址
#define AET_SUPER_FUNC_NAME_ARRAY           "_superFuncNameArray" //用来保存类方法实现的混淆函数名

//第三版 super实现
#define AET_SUPER_FUNC_ADDRESS_ARRAY               "_superFuncAddressArray"       //用来保存类方法实现的地址
#define AET_SUPER_KERNEL_NAME_ARRAY                "_superKernelNameArray"        //用来保存类核方法实现的函数名
#define AET_SUPER_DEVICE_ADDRESS_ARRAY             "_superDeviceAddressArray"      //用来保存类设备方法实现的地址

#define AET_INIT_GLOBAL_SUPER_FUNC_NAME            "init_global_super_data_1"    //全局初始化本类的super数据函数名
#define AET_INIT_INNER_SUPER_FUNC_NAME             "init_inner_super_data_2"      //内部初始化本类的super数据的函数名

//加入判断是不是MTCS_CLASS的变量
//static char _IS_MTCS_CLASS
#define AET_IS_MTCS_CLASS_VAR_NAME                 "_IS_MTCS_CLASS"
//实现在MtcsSystem.c中 复制__device__ 修饰的设备函数的地址
#define AET_MTCS_COPY_DEVICE_FUNC_ADDRESS_FUNC_NAME "mtcs_copy_device_func_address"
#define AET_MTCS_COPY_DEVICE_TO_SUPER_FUNC_NAME     "mtcs_copy_device_address_to_super"

#define AET_MTCS_PLATFORM_TYPE_VAR_NAME             "mtcsPlatformType" //平台变量名 在AObject.h中声明
#define AET_MTCS_DEVICE_FUNC_POINTERS_VAR_NAME      "deviceFuncPointers" //保存设备函数地址的变量名


#define RID_AET_GOTO_STR                   "aet_goto_compile$" //在c_parser_declaration_or_fndef 需要跳转处理的关键字，内部使用
typedef enum{
   GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START =2, //把泛型块定义成内部函数编译
   GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END =3, //把泛型块定义成内部函数编译结束
   GOTO_STATIC_VAR_FUNC=10, //在类中的静态变量或函数
   GOTO_CHECK_FUNC_DEFINE=12, //检查类方法的实现 在临时文件中使用temp_func_track_45.c
   GOTO_IFACE_COMPILE=13, //检查类方法的实现 在临时文件中使用temp_func_track_45.c
   GOTO_MTCS_CREATE_OBJ=14,//创建MTCS对象时<<<cuda,1>>>生成mtcsPlatType树作为变量的初始值。
   GOTO_READY_COMPILE_GENERIC_BLOCK_FUNC=15,//在每个类实现完成时，通过makefileparm.c makefile_parm_insert_block_func_codes 调用。
   GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC=16,//开始编译泛型块函数。
   GOTO_ADD_H_FILE=17,//自动加入头文件
}AetGotoTag;

#define LIB_GLOBAL_IFACE_VAR_NAME_PREFIX   "_global_aet_iface_A_$_"
#define LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX "_global_aet_generic_A_$_123_" //用来保存泛型对象和块函数。

//11-08
#define IFACE_FILE_SUFFIX            "_impl_iface" //接口实现源文件或.o文件的后缀

#define AET_MAGIC_NAME                       "_aet_magic$_123"  //存放魔数的变量名
#define AET_MAGIC_NAME_VALUE                 1725348960 //类魔数
#define AET_IFACE_MAGIC_NAME_VALUE           (AET_MAGIC_NAME_VALUE+1)//接口魔数
#define AET_VAR_OF_FUNC_NAME                 "varof_object_or_interface"  //varof具体实现。定义在AObject.h。
#define AET_DYNAMIC_CLASS_TO_IFACE_FUNC_NAME     "dynamic_class_to_iface"  //类转接口的动态实现函数，定义在AObject.h。
#define AET_DYNAMIC_IFACE_TO_CLASS_FUNC_NAME     "dynamic_iface_to_class"  //接口转类的动态实现函数，定义在AObject.h。

typedef enum{
   COMPILE_IFACE=1<<1,//文件引用了接口
   COMPILE_BLOCK=1<<2,//类中有泛型块
   COMPILE_NEW=1<<3, //新建泛型对象或调用泛型函数
   COMPILE_MTCS_LINK=1<<4,//链接MTCS函数
   COMPILE_IFACE_IMPL_CHECK=1<<5,//检查文件
}CompileType;

typedef enum _FuncAndVarMsg
{
   ID_EXISTS,//通过 lookup_name (id);找到了
   ID_IS_CONSTRUCTOR,//是一个构造函数
   ID_NOT_FIND,
   ISAET_FIND_FUNC,
   ISAET_FIND_VAL,//在implimpl class 中，但找到变量
   ISAET_FIND_STATIC_VAL,//在implimpl class 中，但找到变量
   ISAET_FIND_STATIC_FUNC,//静态函数
} FuncAndVarMsg;

typedef enum _ClassParserState
{
   CLASS_STATE_START,
   CLASS_STATE_ABSTRACT,
   CLASS_STATE_FIELD,//进入了结构体内file分析状态
   CLASS_STATE_STOP,
} ClassParserState;

typedef enum _ClassPermissionType
{
   CLASS_PERMISSION_PUBLIC,
   CLASS_PERMISSION_PROTECTED,
   CLASS_PERMISSION_PRIVATE,
   CLASS_PERMISSION_DEFAULT=-1,
} ClassPermissionType;


typedef enum _CreateClassMethod
{
   CREATE_OBJECT_METHOD_STACK=1,
   CREATE_OBJECT_METHOD_HEAP=2,
   CREATE_OBJECT_METHOD_FIELD_STACK=3,
   CREATE_OBJECT_METHOD_FIELD_HEAP=4,
   CREATE_OBJECT_METHOD_NO_DECL_HEAP=5,
   CREATE_OBJECT_USE_STACK=6,
   CREATE_OBJECT_USE_HEAP=7,
   CREATE_CLASS_METHOD_UNKNOWN=-1,
} CreateClassMethod;

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


extern int enter_aet;//是否进入aet 定义在classparser中

#define ELF_MAGIC 0x61746531 //aet1的数字化



#endif /* ! __GCC_AET_MICRO_H__ */
