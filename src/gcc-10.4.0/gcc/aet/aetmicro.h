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


typedef struct _WarnAndError
{
	int errors[100];
	int errorCount;
	int warns[100];
	int warnCount;
	int aboutNumber;
}WarnAndError;

extern WarnAndError argsFuncsInfo;

static inline void aet_warn_and_error_reset()
{
	argsFuncsInfo.errorCount=0;
	argsFuncsInfo.warnCount=0;
}

static inline WarnAndError * aet_warn_and_error_copy()
{
	WarnAndError *info=xmalloc(sizeof(WarnAndError));
	memcpy(info,&argsFuncsInfo,sizeof(WarnAndError));
	return info;
}

enum warn_error_info{
	too_many_arguments_to_function,
	too_few_arguments_to_function,
	too_few_arguments_to_built_in_function_xx_expecting_xx,
	too_many_arguments_to_built_in_function_expecting,
	called_object_is_not_a_function_or_function_pointer,
	has_an_incomplete_type,
	invalid_use_of_void_expression,
	invalid_use_of_flexible_array_member,
	invalid_use_of_array_with_unspecified_bounds,
	invalid_use_of_undefined_type,
	invalid_use_of_incomplete_typedef,
	implicit_conversion_from_xx_to_xx_when_passing_argument_to_function,
	invalid_func_diag,
	type_of_formal_parameter_xx_is_incomplete,
	passing_argument_xx_of_xx_as_integer_rather_than_complex_due_to_prototype,
	passing_argument_xx_of_xx_as_integer_rather_than_floating_due_to_prototype,
		passing_argument_xx_of_xx_as_complex_rather_than_floating_due_to_prototype,
		passing_argument_xx_of_xx_as_floating_rather_than_integer_due_to_prototype,
		passing_argument_xx_of_xx_as_complex_rather_than_integer_due_to_prototype,
		passing_argument_xx_of_xx_as_floating_rather_than_complex_due_to_prototype,
		passing_argument_xx_of_xx_as_float_rather_than_double_due_to_prototype,
		passing_argument_xx_of_xx_as_xx_rather_than_xx_due_to_prototype,
		passing_argument_xx_of_xx_with_different_width_due_to_prototype,
		passing_argument_xx_of_xx_as_unsigned_due_to_prototype,
	    passing_argument_xx_of_xx_as_signed_due_to_prototype,
		void_value_not_ignored_as_it_ought_to_be,
		built_in_function_xx_must_be_directly_called,
		cannot_pass_rvalue_to_reference_parameter,
		global_register_variable_xx_used_in_nested_function,
		register_variable_xx_used_in_nested_function,
		address_of_global_register_variable_xx_requested,
		address_of_register_variable_xx_requested,
		invalid_conv_diag,
		conversion_to_non_scalar_type_requested,
		cannot_convert_to_a_pointer_type,
		pointer_value_used_where_a_floating_point_was_expected,
		aggregate_value_used_where_a_floating_point_was_expected,
		conversion_to_incomplete_type,
		cannot_convert_a_vector_of_type_xx_to_type_xx_which_has_different_size,
		aggregate_value_used_where_an_integer_was_expected,
		pointer_value_used_where_a_complex_was_expected,
		aggregate_value_used_where_a_complex_was_expected,
		cannot_convert_a_value_of_type_xx_to_vector_type_xx_which_has_different_size,
		cannot_convert_value_to_a_vector,
		aggregate_value_used_where_a_fixed_point_was_expected,
		function_called_through_a_non_compatible_type,
		function_with_qualified_void_return_type_called,
		xx_argument_x_type_is_xx_where_xx_is_expected_in_a_call_to_built_in_function_declared_without_prototype,
		xx_argument_x_promotes_to_xx_where_xx_is_expected_in_a_call_to_built_in_function_declared_without_prototype,
		int_to_real_or_real_to_int,
		assignment_to_x_from_y_makes_pointer_from_integer_without_a_cast,
		initialization_of_x_from_y_makes_pointer_from_integer_without_a_cast,
		returning_x_from_a_function_with_return_type_y_makes_pointer_from_integer_without_a_cast,
		passing_argument_x_of_y_makes_pointer_from_integer_without_a_cast,

		assignment_to_x_from_y_makes_integer_from_pointer_without_a_cast,
		initialization_of_x_from_y_makes_integer_from_pointer_without_a_cast,
		returning_x_from_a_function_with_return_type_y_makes_integer_from_pointer_without_a_cast,
		passing_argument_x_of_y_makes_integer_from_pointer_without_a_cast,
		enum_conversion_when_passing_argument_x_of_y_is_invalid_in_Cplusplus,
		enum_conversion_from_x_to_y_in_assignment_is_invalid_in_Cplusplus,
		enum_conversion_from_x_to_y_in_initialization_is_invalid_in_Cplusplus,
		enum_conversion_from_x_to_y_in_return_is_invalid_in_Cplusplus,
		implicit_conversion_from_x_to_y,
		ISO_C_prohibits_argument_conversion_to_union_type,
		request_for_implicit_conversion_from_x_to_t_not_permitted_in_Cplusplus,
		passing_argument_x_of_y_from_pointer_to_non_enclosed_address_space,
		assignment_from_pointer_to_non_enclosed_address_space,
		initialization_from_pointer_to_on_enclosed_address_space,
		return_from_pointer_to_non_enclosed_address_space,
		argument_x_of_y_might_be_a_candidate_for_a_format_attribute,
		assignment_left_hand_side_might_be_a_candidate_for_a_format_attribute,
		initialization_left_hand_side_might_be_a_candidate_for_a_format_attribute,
		return_type_might_be_a_candidate_for_a_format_attribute,
		pointer_targets_in_passing_argument_x_of_y_differ_in_signedness,
		pointer_targets_in_assignment_from_x_to_y_differ_in_signedness,
		pointer_targets_in_initialization_of_x_from_y_differ_in_signedness,
		pointer_targets_in_returning_x_from_a_function_with_return_type_y_differ_in_signedness,
		passing_argument_x_of_y_from_incompatible_pointer_type,
		assignment_to_x_from_pointer_to_y_with_incompatible_type_z,
		assignment_to_x_from_incompatible_pointer_type_y,
		initialization_of_x_from_pointer_to_y_with_incompatible_type_z,
		initialization_of_x_from_incompatible_pointer_type_y,
		returning_pointer_to_x_of_type_y_from_a_function_with_incompatible_type_z,
		returning_x_from_a_function_with_incompatible_return_type_y,
		invalid_use_of_non_lvalue_array,
		incompatible_type_for_argument_x_of_y,
		incompatible_types_when_assigning_to_type_x_from_type_y,
		incompatible_types_when_initializing_type_x_using_type_y,
		incompatible_types_when_returning_type_x_but_y_was_expected,
		taking_address_of_packed_member_of_x_may_result_in_an_unaligned_pointer_value,
		converting_a_packed_x_pointer_alignment_y_to_a_x_pointer_alignment_y_may_result_in_an_unaligned_pointer_value,
		conversion_from_x_to_y_changes_value_from_x_to_y,
		conversion_from_x_to_y_changes_the_value_of_x,
		conversion_to_x_from_y_may_change_the_sign_of_the_result,
		conversion_from_x_to_y_discards_imaginary_component,
		conversion_from_x_to_y_may_change_value,
		unsigned_conversion_from_x_to_y_changes_the_value_of_x,
		overflow_in_conversion_from_x_to_y_changes_value_from_x_to_y,
		overflow_in_conversion_from_x_to_y_changes_the_value_of_z,
		conversion_to_x_from_boolean_expression,
		unsigned_conversion_from_x_to_y_changes_value_from_x_to_y,
		invalid_op_diag,
		wrong_type_argument_to_unary_plus,
		wrong_type_argument_to_unary_minus,
		x_on_a_boolean_expression_did_you_mean_to_use_logical_not,
		ISO_C_does_not_support_x_for_complex_conjugation,
		wrong_type_argument_to_bit_complement,
		wrong_type_argument_to_abs,
		wrong_type_argument_to_absu,
		wrong_type_argument_to_conjugation,
		wrong_type_argument_to_unary_exclamation_mark,
		used_array_that_cannot_be_converted_to_pointer_where_scalar_is_required,
		used_struct_type_value_where_scalar_is_required,
		used_union_type_value_where_scalar_is_required,
		used_vector_type_where_scalar_is_required,
		increment_of_enumeration_value_is_invalid_in_Cplusplus,
		decrement_of_enumeration_value_is_invalid_in_Cplusplus,
		increment_of_a_boolean_expression,
		decrement_of_a_boolean_expressio,
		ISO_C_does_not_support_x_and_y_on_complex_types,
		wrong_type_argument_to_increment,
		wrong_type_argument_to_decrement,
		increment_of_pointer_to_an_incomplete_type_x,
		decrement_of_pointer_to_an_incomplete_type_x,
		taking_address_of_expression_of_type_void,
		cannot_take_address_of_bit_field_x,
		cannot_take_address_of_scalar_with_reverse_storage_order,
		address_of_array_with_reverse_scalar_storage_order_requested,
	    passing_argument_xx_of_xx_from_incompatible_pointer_type,
		assignment_to_$type$_from_pointer_to_$bltin$_with_incompatible_type_$rhstype$,
		assignment_to_$type$_from_incompatible_pointer_type_$rhstype$,
		initialization_of_$type$_from_pointer_to_$bltin$_with_incompatible_type_$rhstype$,
		initialization_of_$type$_from_incompatible_pointer_type_$rhstype$,
		returning_pointer_to_$bltin$_of_type_$rhstype$_from_a_function_with_incompatible_type_$type$,
		returning_$rhstype$_from_a_function_with_incompatible_return_type_$type$,
		compare_aet_class_argument_orig_parm_error,//比较AET CLASS
		go_unreachable,
	    error_or_warn_unknown,
};


#define AET_MAX_TOKEN 30
#define AET_ROOT_OBJECT                    "AObject"
#define AET_ROOT_CLASS                     "AClass"
#define AET_CLEANUP_OBJECT_METHOD          "a_object_cleanup_local_object_from_static_or_stack"

#define AET_MAX_INTERFACE 5 //一个类同时只能实现5个接口
#define AET_INIT_GLOBAL_METHOD_STRING      "init_1234ergR5678"
#define AET_INNER_ARRAY_VARIABLE_NAME      "innerArrayRef0302"

#define AET_GENERIC_ARRAY                  "_generic_1234_array" //类中的泛型数组名称
//当new一个带未定义的泛型的类时，在该类调用构造函数前，在所在的类self调用该方法，可以把未定义的泛型变成具体的类型
#define AET_SET_BLOCK_FUNC_CALLBACK_NAME   "_setGenericBlockFunc_123_cb"
#define AET_SET_BLOCK_FUNC_VAR_NAME        "_setGenericBlockFunc_123_varname"
#define AET_SET_BLOCK_FUNC_TYPEDEF_NAME    "_setGenericBlockFunc_123_typedef"
//接口声明加入接口时在那个类中被实现的
#define IFACE_AT_CLASS                     "_atClass123"
#define IFACE_COMMON_STRUCT_NAME           "IfaceCommonData123"
#define IFACE_COMMON_STRUCT_VAR_NAME       "_iface_common_var"
#define IFACE_REF_FIELD_NAME               "_iface_reserve_ref_field_123"
#define IFACE_UNREF_FIELD_NAME             "_iface_reserve_unref_field_123"
#define IFACE_REF_FUNC_DEFINE_NAME         "_iface_reserve_ref_func_define_123"
#define IFACE_UNREF_FUNC_DEFINE_NAME       "_iface_reserve_unref_func_define_123"
//super方法实现 在AObject.h中定义
#define ADD_SUPER_METHOD                   "add_super_method"
#define SUPER_FUNC_ADDR_VAR_NAME           "_superFuncAddr123"
#define SUPER_GET_FUNC_ADDRESS             "_get_super_address123"
#define SUPER_FILL_ADDRESS_FUNC             "_fill_super_method_address123"
#define SUPER_INIT_DATA_FUNC               "_init_super_data_123" //初始化superdata的方法，定义在AObject.h中
#define SUPER_CONSTRUCTOR_TEMP_VAR         "_check_super_return_null_var_1234" //调用super$()时变成新语句 XXX *_check_super_return_null_var_1234=....


#define AET_GENERIC_BLOCK_ARRAY_VAR_NAME  "_gen_blocks_array_897" //类中的泛型块函数指针数组变量名称 void *_gen_blocks_array_897[xx];
#define AET_MAX_GENERIC_BLOCKS             30


#define AET_GENERIC_TYPE_NAME_PREFIX       "aet_generic_" //泛型类型名的前缀，后缀是A-Z
#define AET_GET_GENERIC_INFO_FUNC_NAME     "generic_info$" //该方法定义在aobject.h头文件中
#define AET_GENERIC_INFO_STRUCT_NAME       "aet_generic_info"
#define AET_FUNC_GENERIC_PARM_NAME         "tempFgpi1234" //参数名
#define AET_FUNC_GENERIC_PARM_STRUCT_NAME  "FuncGenParmInfo" //泛型函数加入的第二参数的类型名
#define FuncGenParmInfo_NAME                "_inFileGlobalFuncGenParmInfo" //每个实现泛型块函数的.c文件有一个
#define AET_FUNC_GENERIC_GLOBAL_BLOCK_STRUCT_NAME  "FuncGenBlockInfo" //泛型函数加入的第二参数的类型名
//当new一个泛型相关的对象时，把全局变量FuncGenParmInfo_NAME赋值给类中的变量
#define AET_BLOCK_INFO_LIST_VAR_NAME       "_block_info_list_var_name"
#define AET_BLOCK_INFO_LIST_STRUCT_NAME    "FuncGenBlockList"


#define RID_AET_GOTO_STR                   "aet_goto_compile$" //在c_parser_declaration_or_fndef 需要跳转处理的关键字
typedef enum{
        GOTO_GENERIC_BLOCK_FUNC_DECL =1,
		GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START =2, //把泛型块定义成内部函数编译
		GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END =3, //把泛型块定义成内部函数编译结束
		GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE =4, //配合关键字aet_goto_compile$编译块函数定义结束
		GOTO_GENERIC_BLOCK_FUNC_DEFINE_COND =6, //条件 获取泛型定义
		GOTO_GENERIC_COMPILE_UNDEFINE_BLOCK_FUNC_DECL=8, //编译未定义泛型类的块函数声明
		GOTO_STATIC_VAR_FUNC=10, //在类中的静态变量或函数
		GOTO_STATIC_CONTINUE_COMPILE=11, //在类中的静态变量或函数太长，分几段来加token,GOTO_STATIC_CONTINUE_COMPILE就是继续加token的触发器
		GOTO_CHECK_FUNC_DEFINE=12, //检查类方法的实现 在临时文件中使用temp_func_track_45.c
		GOTO_IFACE_COMPILE=13, //检查类方法的实现 在临时文件中使用temp_func_track_45.c
}AetGotoTag;

#define LIB_GLOBAL_VAR_NAME_PREFIX         "_global_aet_A_$_789_" //把class_func_check_file、aet_generic_block_xml.tmp的内容编译进文件


//////////////////编译期生成的临时文件---------------------------------
#define GENERIC_BLOCK_INTO_FILE_NAME       "aet_generic_block_xml.tmp" //在编译期中存放new$泛型的有关信息。重要
#define IFACE_STATIC_VAR_FILE_NAME         "aet_iface_static_var_xml.tmp"      //存放接口的变量名
#define CLASS_FUNC_CHECK_FILE              "aet_class_func_check_xml.tmp"//存放类的方法，是一个xml文件

#define SAVE_LIB_PARM_FILE                  "aet_collect2_argv.tmp"
#define ADDITIONAL_MIDDLE_AET_FILE          "temp_func_track_45.c"
#define COMPILE_TRACK_FILE_NAME             "aet_object_path.tmp"
#define AET_REF_LIB_FILE_NAME               "aet_ref_lib_file.tmp"
#define AET_NEED_SECOND_FILE_NAME           "aet_need_second_compile.tmp" //存放需要第二次编译的文件名


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

extern int enter_aet;//是否进入aet 如果classparser中





#endif /* ! __GCC_AET_MICRO_H__ */
