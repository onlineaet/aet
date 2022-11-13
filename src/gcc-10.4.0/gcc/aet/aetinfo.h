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

#ifndef GCC_AET_INFO_H
#define GCC_AET_INFO_H


/**
 * 与c-parser.h中的c_id_kind对应
 * 标识符类型名称的字符串数组
 */
static char * aet_c_id_kind_str[] = {
"C_ID_ID",
"C_ID_TYPENAME",
"C_ID_CLASSNAME",
"C_ID_ADDRSPACE",
"C_ID_NONE"
};

/**
 * libcpp/cpplib中的TTYPE_TABLE对应
 */
static char *aet_cpp_ttype_str[]={
    "CPP_EQ", "CPP_NOT", "CPP_GREATER", "CPP_LESS", "CPP_PLUS", "CPP_MINUS","CPP_MULT", "CPP_DIV", "CPP_MOD","CPP_AND", "CPP_OR", "CPP_XOR","CPP_RSHIFT", "CPP_LSHIFT",/*14*/

    "CPP_COMPL", "CPP_AND_AND", "CPP_OR_OR", "CPP_QUERY", "CPP_COLON", "CPP_COMMA", "CPP_OPEN_PAREN", "CPP_CLOSE_PAREN", "CPP_EOF", "CPP_EQ_EQ", "CPP_NOT_EQ", "CPP_GREATER_EQ",
    "CPP_LESS_EQ","CPP_SPACESHIP",/*14*/
	  /* These two are unary + / - in preprocessor expressions.  */
    "CPP_PLUS_EQ", "CPP_MINUS_EQ",/*2*/

    "CPP_MULT_EQ", "CPP_DIV_EQ","CPP_MOD_EQ","CPP_AND_EQ", "CPP_OR_EQ", "CPP_XOR_EQ", "CPP_RSHIFT_EQ", "CPP_LSHIFT_EQ",/*8*/
	  /* Digraphs together, beginning with CPP_FIRST_DIGRAPH.  */
    "CPP_HASH","CPP_PASTE", "CPP_OPEN_SQUARE", "CPP_CLOSE_SQUARE","CPP_OPEN_BRACE", "CPP_CLOSE_BRACE",/*6*/
	 /* The remainder of the punctuation.	Order is not significant.  */
    "CPP_SEMICOLON", "CPP_ELLIPSIS", "CPP_PLUS_PLUS", "CPP_MINUS_MINUS", "CPP_DEREF", "CPP_DOT", "CPP_SCOPE", "CPP_DEREF_STAR", "CPP_DOT_STAR", "CPP_ATSIGN",/*10*/
    "CPP_NAME", "CPP_AT_NAME",  "CPP_NUMBER",/*3*/
    "CPP_CHAR", "CPP_WCHAR", "CPP_CHAR16", "CPP_CHAR32","CPP_UTF8CHAR","CPP_OTHER",/*6*/
    "CPP_STRING", "CPP_WSTRING", "CPP_STRING16", "CPP_STRING32","CPP_UTF8STRING","CPP_OBJC_STRING", "CPP_HEADER_NAME",/*7*/

    "CPP_CHAR_USERDEF","CPP_WCHAR_USERDEF","CPP_CHAR16_USERDEF","CPP_CHAR32_USERDEF","CPP_UTF8CHAR_USERDEF","CPP_STRING_USERDEF","CPP_WSTRING_USERDEF","CPP_STRING16_USERDEF",
    "CPP_STRING32_USERDEF", "CPP_UTF8STRING_USERDEF",/*10*/

    "CPP_COMMENT",/*1*/
    "CPP_MACRO_ARG","CPP_PRAGMA", "CPP_PRAGMA_EOL", "CPP_PADDING",/*4*/
    "CPP_MAX_TYPE",
    "CPP_KEYWORD"};

/**
 * 与c-family/c-pragma.h中的enum pragma_kind 对应
 *
 */
static char *aet_pragma_kind_str[]={
  "PRAGMA_NONE ",

  "PRAGMA_OACC_ATOMIC",
  "PRAGMA_OACC_CACHE",
  "PRAGMA_OACC_DATA",
  "PRAGMA_OACC_DECLARE",
  "PRAGMA_OACC_ENTER_DATA",
  "PRAGMA_OACC_EXIT_DATA",
  "PRAGMA_OACC_HOST_DATA",
  "PRAGMA_OACC_KERNELS",
  "PRAGMA_OACC_LOOP",
  "PRAGMA_OACC_PARALLEL",
  "PRAGMA_OACC_ROUTINE",
  "PRAGMA_OACC_SERIAL",
  "PRAGMA_OACC_UPDATE",
  "PRAGMA_OACC_WAIT",

  "PRAGMA_OMP_ATOMIC",
  "PRAGMA_OMP_BARRIER",
  "PRAGMA_OMP_CANCEL",
  "PRAGMA_OMP_CANCELLATION_POINT",
  "PRAGMA_OMP_CRITICAL",
  "PRAGMA_OMP_DECLARE",
  "PRAGMA_OMP_DEPOBJ",
  "PRAGMA_OMP_DISTRIBUTE",
  "PRAGMA_OMP_END_DECLARE_TARGET",
  "PRAGMA_OMP_FLUSH",
  "PRAGMA_OMP_FOR",
  "PRAGMA_OMP_LOOP",
  "PRAGMA_OMP_MASTER",
  "PRAGMA_OMP_ORDERED",
  "PRAGMA_OMP_PARALLEL",
  "PRAGMA_OMP_REQUIRES",
  "PRAGMA_OMP_SCAN",
  "PRAGMA_OMP_SECTION",
  "PRAGMA_OMP_SECTIONS",
  "PRAGMA_OMP_SIMD",
  "PRAGMA_OMP_SINGLE",
  "PRAGMA_OMP_TARGET",
  "PRAGMA_OMP_TASK",
  "PRAGMA_OMP_TASKGROUP",
  "PRAGMA_OMP_TASKLOOP",
  "PRAGMA_OMP_TASKWAIT",
  "PRAGMA_OMP_TASKYIELD",
  "PRAGMA_OMP_THREADPRIVATE",
  "PRAGMA_OMP_TEAMS",

  "PRAGMA_GCC_PCH_PREPROCESS",
  "PRAGMA_IVDEP",
  "PRAGMA_UNROLL",

  "PRAGMA_FIRST_EXTERNAL"
};

/**
 * c/c-tree.h 存储说明符的类型
 * INLINE RID_NORETURN RID_THREAD 没有列出
*/
static char *aet_c_storage_class_str[]={
  "csc_none",
  "csc_auto",
  "csc_extern",
  "csc_register",
  "csc_static",
  "csc_typedef"
};

/**
 * c/c-tree.h 类型说明符关键字
 */
static char *aet_c_typespec_keyword_str[]={
  "cts_none",
  "cts_void",
  "cts_bool",
  "cts_char",
  "cts_int",
  "cts_float",
  "cts_int_n",
  "cts_double",
  "cts_dfloat32",
  "cts_dfloat64",
  "cts_dfloat128",
  "cts_floatn_nx",
  "cts_fract",
  "cts_accum",
  "cts_auto_type"
};

static char *aet_c_typespec_kind_str[]= {
  /* No typespec.  This appears only in struct c_declspec.  */
  "ctsk_none",
  /* A reserved keyword type specifier.  */
  "ctsk_resword",
  /* A reference to a tag, previously declared, such as "struct foo".
     This includes where the previous declaration was as a different
     kind of tag, in which case this is only valid if shadowing that
     tag in an inner scope.  */
  "ctsk_tagref",
  /* Likewise, with standard attributes present in the reference.  */
  "ctsk_tagref_attrs",
  /* A reference to a tag, not previously declared in a visible
     scope.  */
  "ctsk_tagfirstref",
  /* Likewise, with standard attributes present in the reference.  */
  "ctsk_tagfirstref_attrs",
  /* A definition of a tag such as "struct foo { int a; }".  */
  "ctsk_tagdef",
  /* A typedef name.  */
  "ctsk_typedef",
  /* An ObjC-specific kind of type specifier.  */
  "ctsk_objc",
  /* A typeof specifier, or _Atomic ( type-name ).  */
  "ctsk_typeof"
};


/* The various kinds of declarators in C.  */
static char *aet_c_declarator_kind_str[]= {
  /* An identifier.  */
  "cdk_id",
  /* A function.  */
  "cdk_function",
  /* An array.  */
  "cdk_array",
  /* A pointer.  */
  "cdk_pointer",
  /* Parenthesized declarator with nested attributes.  */
  "cdk_attrs"
};

#endif /* ! GCC_C_AET_H */
