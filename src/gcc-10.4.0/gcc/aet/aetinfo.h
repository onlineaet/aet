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
 * 与c-family/c-common.h enum rid对应
 */
static char *aet_rid_str[] ={

  /* Modifiers: */
  /* C, in empirical order of frequency.*/
  "RID_STATIC",
  "RID_UNSIGNED", "RID_LONG",    "RID_CONST", "RID_EXTERN",
  "RID_REGISTER", "RID_TYPEDEF", "RID_SHORT", "RID_INLINE",
  "RID_VOLATILE", "RID_SIGNED",  "RID_AUTO",  "RID_RESTRICT",
  "RID_NORETURN", "RID_ATOMIC",

  /* C extensions */
  "RID_COMPLEX", "RID_THREAD", "RID_SAT",

  /* C++ */
  "RID_FRIEND", "RID_VIRTUAL", "RID_EXPLICIT", "RID_EXPORT", "RID_MUTABLE",

  /* ObjC ("PQ" reserved words - they do not appear after a '@' and
     are keywords only in specific contexts) */
  "RID_IN", "RID_OUT", "RID_INOUT", "RID_BYCOPY", "RID_BYREF", "RID_ONEWAY",

    /* ObjC ("PATTR" reserved words - they do not appear after a '@'
     and are keywords only as property attributes) */
  "RID_GETTER", "RID_SETTER",
  "RID_READONLY", "RID_READWRITE",
  "RID_ASSIGN", "RID_RETAIN", "RID_COPY",
  "RID_NONATOMIC",

    /* C (reserved and imaginary types not implemented, so any use is a syntax error) */
   "RID_IMAGINARY",

     /* C */
   "RID_INT",     "RID_CHAR",   "RID_FLOAT",    "RID_DOUBLE", "RID_VOID",
  "RID_ENUM",    "RID_STRUCT", "RID_UNION",    "RID_IF",     "RID_ELSE",
  "RID_WHILE",   "RID_DO",     "RID_FOR",      "RID_SWITCH", "RID_CASE",
  "RID_DEFAULT", "RID_BREAK",  "RID_CONTINUE", "RID_RETURN", "RID_GOTO",
  "RID_SIZEOF",

    /* C extensions */
  "RID_ASM",       "RID_TYPEOF",   "RID_ALIGNOF",  "RID_ATTRIBUTE",  "RID_VA_ARG",
  "RID_EXTENSION", "RID_IMAGPART", "RID_REALPART", "RID_LABEL", "RID_CHOOSE_EXPR",
  "RID_TYPES_COMPATIBLE_P", "RID_BUILTIN_COMPLEX","RID_BUILTIN_SHUFFLE",
  "RID_BUILTIN_CONVERTVECTOR","RID_BUILTIN_TGMATH",
  "RID_BUILTIN_HAS_ATTRIBUTE",
  "RID_DFLOAT32", "RID_DFLOAT64", "RID_DFLOAT128",

    /* TS 18661-3 keywords, in the same sequence as the TI_* values.  RID_FLOATN_NX_FIRST = RID_FLOAT16,*/

  "RID_FLOAT16 or RID_FLOATN_NX_FIRST",
   /*RID_FLOATN_NX_FIRST = RID_FLOAT16,*/
  "RID_FLOAT32",
  "RID_FLOAT64",
  "RID_FLOAT128",
  "RID_FLOAT32X",
  "RID_FLOAT64X",
  "RID_FLOAT128X",
  "RID_FRACT", "RID_ACCUM",   "RID_AUTO_TYPE", "RID_BUILTIN_CALL_WITH_STATIC_CHAIN",

    /* "__GIMPLE", for the GIMPLE-parsing extension to the C frontend. */
  "RID_GIMPLE",

  /* "__PHI", for parsing PHI function in GIMPLE FE.  */
  "RID_PHI",

  /* "__RTL", for the RTL-parsing extension to the C frontend.  */
  "RID_RTL",

  /* C11 */
  "RID_ALIGNAS", "RID_GENERIC",

  /* This means to warn that this is a C++ keyword, and then treat it
     as a normal identifier.  */
  "RID_CXX_COMPAT_WARN",

  /* GNU transactional memory extension */
  "RID_TRANSACTION_ATOMIC", "RID_TRANSACTION_RELAXED", "RID_TRANSACTION_CANCEL",

  /* Too many ways of getting the name of a function as a string */
  "RID_FUNCTION_NAME", "RID_PRETTY_FUNCTION_NAME", "RID_C99_FUNCTION_NAME",

    /* C++ (some of these are keywords in Objective-C as well, but only
     if they appear after a '@') */
  "RID_BOOL",     "RID_WCHAR",    "RID_CLASS",
  "RID_PUBLIC",   "RID_PRIVATE",  "RID_PROTECTED",
  "RID_TEMPLATE", "RID_NULL",     "RID_CATCH",
  "RID_DELETE",   "RID_FALSE",    "RID_NAMESPACE",
  "RID_NEW",      "RID_OFFSETOF", "RID_OPERATOR",
  "RID_THIS",     "RID_THROW",    "RID_TRUE",
  "RID_TRY",      "RID_TYPENAME", "RID_TYPEID",
  "RID_USING",    "RID_CHAR16",   "RID_CHAR32",

    /* casts */
  "RID_CONSTCAST", "RID_DYNCAST", "RID_REINTCAST", "RID_STATCAST",

    /* C++ extensions */
  "RID_ADDRESSOF",               "RID_BASES",
  "RID_BUILTIN_LAUNDER",         "RID_DIRECT_BASES",
  "RID_HAS_NOTHROW_ASSIGN",      "RID_HAS_NOTHROW_CONSTRUCTOR",
  "RID_HAS_NOTHROW_COPY",        "RID_HAS_TRIVIAL_ASSIGN",
  "RID_HAS_TRIVIAL_CONSTRUCTOR", "RID_HAS_TRIVIAL_COPY",
  "RID_HAS_TRIVIAL_DESTRUCTOR",  "RID_HAS_UNIQUE_OBJ_REPRESENTATIONS",
  "RID_HAS_VIRTUAL_DESTRUCTOR",
  "RID_IS_ABSTRACT",             "RID_IS_AGGREGATE",
  "RID_IS_BASE_OF",              "RID_IS_CLASS",
  "RID_IS_EMPTY",                "RID_IS_ENUM",
  "RID_IS_FINAL",                "RID_IS_LITERAL_TYPE",
  "RID_IS_POD",                  "RID_IS_POLYMORPHIC",
  "RID_IS_SAME_AS",
  "RID_IS_STD_LAYOUT",           "RID_IS_TRIVIAL",
  "RID_IS_TRIVIALLY_ASSIGNABLE", "RID_IS_TRIVIALLY_CONSTRUCTIBLE",
  "RID_IS_TRIVIALLY_COPYABLE",
  "RID_IS_UNION",                "RID_UNDERLYING_TYPE",
  "RID_IS_ASSIGNABLE",           "RID_IS_CONSTRUCTIBLE",

   /* C++11 */
  "RID_CONSTEXPR", "RID_DECLTYPE", "RID_NOEXCEPT", "RID_NULLPTR", "RID_STATIC_ASSERT",

  /* C++20 */
  "RID_CONSTINIT", "RID_CONSTEVAL",

  /* char8_t */
  "RID_CHAR8",

  /* C++ concepts */
  "RID_CONCEPT", "RID_REQUIRES",

  /* C++ coroutines */
  "RID_CO_AWAIT", "RID_CO_YIELD", "RID_CO_RETURN",

  /* C++ transactional memory.  */
  "RID_ATOMIC_NOEXCEPT", "RID_ATOMIC_CANCEL", "RID_SYNCHRONIZED",

    /* Objective-C ("AT" reserved words - they are only keywords when
     they follow '@')  */
  "RID_AT_ENCODE",   "RID_AT_END",
  "RID_AT_CLASS",    "RID_AT_ALIAS",     "RID_AT_DEFS",
  "RID_AT_PRIVATE",  "RID_AT_PROTECTED", "RID_AT_PUBLIC",  "RID_AT_PACKAGE",
  "RID_AT_PROTOCOL", "RID_AT_SELECTOR",
  "RID_AT_THROW",	   "RID_AT_TRY",       "RID_AT_CATCH",
  "RID_AT_FINALLY",  "RID_AT_SYNCHRONIZED",
  "RID_AT_OPTIONAL", "RID_AT_REQUIRED", "RID_AT_PROPERTY",
  "RID_AT_SYNTHESIZE", "RID_AT_DYNAMIC",
  "RID_AT_INTERFACE",
  "RID_AT_IMPLEMENTATION",

    /* Named address support, mapping the keyword to a particular named address
     number.  Named address space 0 is reserved for the generic address.  If
     there are more than 254 named addresses, the addr_space_t type will need
     to be grown from an unsigned char to unsigned short.  */
  "RID_ADDR_SPACE_0",
  "RID_ADDR_SPACE_1",
  "RID_ADDR_SPACE_2",
  "RID_ADDR_SPACE_3",
  "RID_ADDR_SPACE_4",
  "RID_ADDR_SPACE_5",
  "RID_ADDR_SPACE_6",
  "RID_ADDR_SPACE_7",
  "RID_ADDR_SPACE_8",
  "RID_ADDR_SPACE_9",
  "RID_ADDR_SPACE_10",
  "RID_ADDR_SPACE_11",
  "RID_ADDR_SPACE_12",
  "RID_ADDR_SPACE_13",
  "RID_ADDR_SPACE_14",
  "RID_ADDR_SPACE_15",
  "RID_INT_N_0",
  "RID_INT_N_1",
  "RID_INT_N_2",
  "RID_INT_N_3",
  /*zclei aet-c*/
  "RID_AET_CLASS",    "RID_AET_ABSTRACT",  "RID_AET_INTERFACE",
  "RID_AET_EXTENDS", "RID_AET_IMPLEMENTS",        "RID_AET_REALIZE_CLASS",
  "RID_AET_FINAL","RID_AET_NEW","RID_AET_SUPER","RID_AET_PACKAGE",
  "RID_AET_PUBLIC","RID_AET_PROTECTED","RID_AET_PRIVATE","RID_AET_GENERIC_INFO",
  "RID_AET_GENERIC_BLOCK","RID_AET_GOTO","RID_AET_OBJECT","RID_AET_ENUM",
  "RID_MAX"
};

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
