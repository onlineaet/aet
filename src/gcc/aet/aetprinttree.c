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
#include "attribs.h"
#include "toplev.h"
#include "stor-layout.h"
#include "c-family/c-pragma.h"
#include "c-family/c-common.h"
#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "tree-iterator.h"
#include "dumpfile.h"
#include "c-family/c-ubsan.h"
#include "tree-nested.h"
#include "context.h"
#include "langhooks.h"
#include "c/c-lang.h"

#include "aetinfo.h"
#include "aetprinttree.h"


/**
 * c/c-tree.h 存储说明符的类型
 * INLINE RID_NORETURN RID_THREAD 没有列出
*/
static char *storage_class_str[]={
  "csc_none",
  "csc_auto",
  "csc_extern",
  "csc_register",
  "csc_static",
  "csc_typedef"
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

#define tree_info(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_INFO,file,line,func,format, ##__VA_ARGS__);

#define tree_debug(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_DEBUG,file,line,func,format, ##__VA_ARGS__);

#define tree_warning(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_WARNING,file,line,func,format, ##__VA_ARGS__);

static void printNode(tree node)
{
    if(!node || node==NULL_TREE){
        fprintf(stderr,"tree 是空的\n");
        return;
    }
    if(node==error_mark_node){
        fprintf(stderr,"tree 是 error_mark_node\n");
        return;
    }
    int i;
    FILE *dump_orig;
    dump_flags_t local_dump_flags;
    dump_file_info *dfi;
    dfi = g->get_dumps ()->get_dump_file_info (TDI_original);
    dump_orig = dfi->pstream;
    local_dump_flags = dfi->pflags;
    dump_orig = dump_begin (TDI_original, &local_dump_flags);
    //if(!dump_orig)
        dump_orig=stderr;
    dump_node(node,TDF_ALL_VALUES|local_dump_flags,dump_orig);
}

void  aet_print_specs_from(struct c_declspecs *specs,char *file,char *func,int linen)
{
	if(!n_log_is_debug())
	   return ;
    nboolean re=n_log_is_debug_file(file,func);
    if(!re){
	  return;
    }
	char line[20];
	sprintf(line,"%d",linen);
    tree_debug(file,func,line,"打印 c_declspecs 开始。:%p",specs);
	location_t locations[cdw_number_of_elements];
    location_t loc;
    expanded_location xloc;
    int countStrogeClass=0;
    int countType=0;
	if(specs->storage_class != csc_none){
		loc=specs->locations[cdw_storage_class];//用一个枚举作为数组index c-tree.h中定义
	    xloc = expand_location(loc);
		tree_info(file,func,line,"是存储类型的声明说明符 count:%d %s %s %d %d\n",countStrogeClass++,
		        aet_print_get_storage_class_string(specs->storage_class),xloc.file, xloc.line, xloc.column);

	}
    if(specs->inline_p){
		loc=specs->locations[cdw_inline];
		xloc = expand_location(loc);
		tree_info(file,func,line,"是存储类型的声明说明符 INLINE count:%d  %s %d %d\n",countStrogeClass++,xloc.file, xloc.line, xloc.column);
    }
    if(specs->noreturn_p){
		loc=specs->locations[cdw_noreturn];
		xloc = expand_location(loc);
		tree_info(file,func,line,"是存储类型的声明说明符 NORETURN count:%d  %s %d %d\n",countStrogeClass++,xloc.file, xloc.line, xloc.column);
	}
    if(specs->thread_p){
		loc=specs->locations[cdw_thread];
		xloc = expand_location(loc);
		tree_info(file,func,line,"是存储类型的声明说明符 THREAD count:%d  %s %d %d\n",countStrogeClass++,xloc.file, xloc.line, xloc.column);
	}

    //类型说明符
    char *typespec_kind_str=NULL;
    if(specs->typespec_kind!=ctsk_none){
    	typespec_kind_str=aet_c_typespec_kind_str[specs->typespec_kind];
		loc=specs->locations[cdw_typespec];
	     xloc = expand_location(loc);
	     tree_info(file,func,line,"是类型的声明说明符  typespec_word:%s count:%d  typespec_kind:%s %s %d %d\n",
				aet_c_typespec_keyword_str[specs->typespec_word],countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
	}
    if(specs->long_p){
		loc=specs->locations[cdw_long];
	    xloc = expand_location(loc);
	    tree_info(file,func,line,"是类型的声明说明符 RID_LONG  count:%d  typespec_kind:%s %s %d %d\n",
	    		countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
    }

    if(specs->short_p){
		loc=specs->locations[cdw_short];
	    xloc = expand_location(loc);
	    tree_info(file,func,line,"是类型的声明说明符 RID_SHORT  count:%d  typespec_kind:%s %s %d %d\n",countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
    }

    if(specs->signed_p){
		loc=specs->locations[cdw_signed];
	    xloc = expand_location(loc);
	    tree_info(file,func,line,"是类型的声明说明符 RID_SIGNED  count:%d  typespec_kind:%s %s %d %d\n",
	    		countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
    }

    if(specs->unsigned_p){
		loc=specs->locations[cdw_unsigned];
	    xloc = expand_location(loc);
	    tree_info(file,func,line,"是类型的声明说明符 RID_UNSIGNED  count:%d  typespec_kind:%s %s %d %d\n",
	    		countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
    }

    if(specs->complex_p){
		loc=specs->locations[cdw_complex];
	    xloc = expand_location(loc);
		tree_info(file,func,line,"是类型的声明说明符 RID_COMPLEX  count:%d  typespec_kind:%s %s %d %d\n",
				countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
    }

    if(specs->saturating_p){
		loc=specs->locations[cdw_saturating];
	    xloc = expand_location(loc);
	    tree_info(file,func,line,"是类型的声明说明符 RID_SAT  count:%d  typespec_kind:%s %s %d %d\n",countType++,typespec_kind_str,xloc.file, xloc.line, xloc.column);
    }
    tree_debug(file,func,line,"c_declspecs的 type:");
    printNode(specs->type);
    tree_debug(file,func,line,"c_declspecs的 expr:");
    printNode(specs->expr);
    tree_debug(file,func,line,"c_declspecs的 decl_attr:");
    printNode(specs->decl_attr);
    tree_debug(file,func,line,"c_declspecs的 attrs:");
    printNode(specs->attrs);
    tree_debug(file,func,line,"打印 c_declspecs 结束。:%p",specs);
}

void     aet_print_declarator_from(struct c_declarator *declarator,char *file,char *func,int linen)
{
    if(!n_log_is_debug())
       return ;
    nboolean re=n_log_is_debug_file(file,func);
    if(!re){
       return;
    }
	char line[20];
	sprintf(line,"%d",linen);
	enum c_declarator_kind kind=declarator->kind;
	printf("第一个是 kind:%d %s declarator:%p\n",kind,aet_c_declarator_kind_str[kind],declarator);
	if(kind==cdk_pointer){
		struct c_declarator *second=declarator->declarator;
		enum c_declarator_kind secondKind=second->kind;
		printf("当declarator是指针类型，指向的是 %s 第二个是  kind:%d %s second:%p\n",aet_c_declarator_kind_str[kind],secondKind,aet_c_declarator_kind_str[secondKind],second);
		if(secondKind==cdk_function){
			struct c_declarator *three=second->declarator;
			enum c_declarator_kind threeKind=three->kind;
			printf("第一个是  %s 第二个是  kind:%s 第三个是:%d %s\n",
					aet_c_declarator_kind_str[kind],aet_c_declarator_kind_str[secondKind],threeKind,aet_c_declarator_kind_str[threeKind]);
			if(threeKind==cdk_pointer){
				struct c_declarator *four=three->declarator;
				enum c_declarator_kind foureKind=four->kind;
				printf("第四个是 kind:%d %s\n",four,aet_c_declarator_kind_str[foureKind]);
			}
		}else if(secondKind==cdk_pointer){
			struct c_declarator *three=second->declarator;
		    enum c_declarator_kind threeKind=three->kind;
			printf("第一个是x  %s 第二个是  kind:%s 第三个是:%d %s\n",
								aet_c_declarator_kind_str[kind],aet_c_declarator_kind_str[secondKind],threeKind,aet_c_declarator_kind_str[threeKind]);
						if(threeKind==cdk_pointer){
							struct c_declarator *four=three->declarator;
							enum c_declarator_kind foureKind=four->kind;
							printf("第四个是x kind:%d %s\n",four,aet_c_declarator_kind_str[foureKind]);
						}else if(threeKind==cdk_function){

						}
		}
	}

	int i;
	struct c_declarator *temp=declarator;
	for(i=0;i<100;i++){
		if(temp!=NULL){
		   enum c_declarator_kind kind=temp->kind;
		   printf("第%d个 %d %s declarator:%p\n",i,kind,aet_c_declarator_kind_str[kind],temp);
		   printNode(temp->u.id.id);
		   printf("arg_info:%p\n",temp->u.arg_info);
		   temp=temp->declarator;
		}else{
			break;
		}
	}
//	struct c_arg_info {
//	  /* A list of parameter decls.  */
//	  tree parms;
//	  /* A list of structure, union and enum tags defined.  */
//	  vec<c_arg_tag, va_gc> *tags;
//	  /* A list of argument types to go in the FUNCTION_TYPE.  */
//	  tree types;
//	  /* A list of non-parameter decls (notably enumeration constants)
//	     defined with the parameters.  */
//	  tree others;
//	  /* A compound expression of VLA sizes from the parameters, or NULL.
//	     In a function definition, these are used to ensure that
//	     side-effects in sizes of arrays converted to pointers (such as a
//	     parameter int i[n++]) take place; otherwise, they are
//	     ignored.  */
//	  tree pending_sizes;
//	  /* True when these arguments had [*].  */
//	  BOOL_BITFIELD had_vla_unspec : 1;
//	};

	if(kind==cdk_function){
  	    struct c_arg_info *argInfo=declarator->u.arg_info;
  	    printf("打印 declarator中的 c_arg_info\n");
  	    printf("others---\n");
  	    printNode(argInfo->others);
  	    printNode(argInfo->pending_sizes);
  	    if(argInfo->tags){
  	    	int len=vec_safe_length(argInfo->tags);
  	    	printf("tags is :%d\n",len);
  	    }
	}
}

static bool tree_node_has_type (tree t)
{
    if (t == NULL_TREE)
        return false;

    enum tree_code code = TREE_CODE (t);

    switch (code)
    {
        // 通常有类型的节点
        case VAR_DECL:
        case PARM_DECL:
        case RESULT_DECL:
        case FIELD_DECL:
        case FUNCTION_DECL:
        case CONST_DECL:
        case TYPE_DECL:
        case LABEL_DECL:
        case INTEGER_CST:
        case REAL_CST:
        case FIXED_CST:
        case STRING_CST:
        case COMPLEX_CST:
        case VECTOR_CST:
        case SSA_NAME:
        case COMPONENT_REF:
        case BIT_FIELD_REF:
        case INDIRECT_REF:
        case MEM_REF:
        case ARRAY_REF:
        case ARRAY_RANGE_REF:
        case COMPOUND_EXPR:
        case MODIFY_EXPR:
        case INIT_EXPR:
        case TARGET_EXPR:
        case COND_EXPR:
        case BIND_EXPR:
        case CALL_EXPR:
        case WITH_CLEANUP_EXPR:
        case CLEANUP_POINT_EXPR:
        case CONSTRUCTOR:
        case SAVE_EXPR:
        case ADDR_EXPR:
        case FDESC_EXPR:
        case COMPLEX_EXPR:
        case REALPART_EXPR:
        case IMAGPART_EXPR:
        case PREDECREMENT_EXPR:
        case POSTDECREMENT_EXPR:
        case POSTINCREMENT_EXPR:
        case VA_ARG_EXPR:
        case TRY_CATCH_EXPR:
        case TRY_FINALLY_EXPR:
        case EXIT_EXPR:
        case LOOP_EXPR:
        case NON_LVALUE_EXPR:
        case RANGE_EXPR:
        case CONVERT_EXPR:
        case FIXED_CONVERT_EXPR:
        case ADDR_SPACE_CONVERT_EXPR:
        case FLOAT_EXPR:
        case NOP_EXPR:
        case VIEW_CONVERT_EXPR:
        case NEGATE_EXPR:
        case ABS_EXPR:
        case BIT_NOT_EXPR:
        case TRUTH_NOT_EXPR:
        case PREINCREMENT_EXPR:
        case TRUTH_ANDIF_EXPR:
        case TRUTH_ORIF_EXPR:
        case TRUTH_AND_EXPR:
        case TRUTH_OR_EXPR:
        case TRUTH_XOR_EXPR:
        case LT_EXPR:
        case LE_EXPR:
        case GT_EXPR:
        case GE_EXPR:
        case EQ_EXPR:
        case NE_EXPR:
        case UNORDERED_EXPR:
        case ORDERED_EXPR:
        case UNLT_EXPR:
        case UNLE_EXPR:
        case UNGT_EXPR:
        case UNGE_EXPR:
        case UNEQ_EXPR:
        case LTGT_EXPR:
        case PLUS_EXPR:
        case MINUS_EXPR:
        case MULT_EXPR:
        case TRUNC_DIV_EXPR:
        case CEIL_DIV_EXPR:
        case FLOOR_DIV_EXPR:
        case ROUND_DIV_EXPR:
        case TRUNC_MOD_EXPR:
        case CEIL_MOD_EXPR:
        case FLOOR_MOD_EXPR:
        case ROUND_MOD_EXPR:
        case RDIV_EXPR:
        case EXACT_DIV_EXPR:
        case FIX_TRUNC_EXPR:
        case CONJ_EXPR:
            return TREE_TYPE (t) != NULL_TREE;

        // 类型节点本身
        case INTEGER_TYPE:
        case REAL_TYPE:
        case FIXED_POINT_TYPE:
        case COMPLEX_TYPE:
        case VECTOR_TYPE:
        case ARRAY_TYPE:
        case RECORD_TYPE:
        case UNION_TYPE:
        case QUAL_UNION_TYPE:
        case ENUMERAL_TYPE:
        case BOOLEAN_TYPE:
        case POINTER_TYPE:
        case REFERENCE_TYPE:
        case OFFSET_TYPE:
        case METHOD_TYPE:
        case FUNCTION_TYPE:
        case VOID_TYPE:
            // 类型节点有特殊的类型信息
            return true;

        // 通常没有类型的节点
        case ERROR_MARK:
        case IDENTIFIER_NODE:
        case TREE_LIST:
        case TREE_VEC:
        case BLOCK:
        case OFFSET_REF:
        case STATEMENT_LIST:
        case ASSERT_EXPR:
        case NOEXCEPT_EXPR:
        case EH_FILTER_EXPR:
        case GOTO_EXPR:
        case RETURN_EXPR:
        case SWITCH_EXPR:
        case CASE_LABEL_EXPR:
        case ASM_EXPR:
        case DEBUG_BEGIN_STMT:
        case PREDICT_EXPR:
        case OMP_PARALLEL:
        case OMP_TASK:
        case OMP_FOR:
        case OMP_SECTIONS:
        case OMP_SINGLE:
        case OMP_SECTION:
        case OMP_MASTER:
        case OMP_ORDERED:
        case OMP_CRITICAL:
        case OMP_ATOMIC:
        case OMP_ATOMIC_READ:
        case OMP_ATOMIC_CAPTURE_OLD:
        case OMP_ATOMIC_CAPTURE_NEW:
        default:
            return false;
    }
}

void  aet_print_tree_from(tree value,const char *file,const char *func,int linen)
{
	  if(!n_log_is_debug())
		   return ;
	  nboolean re=n_log_is_debug_file(file,func);
	  if(!re){
		  return;
	  }
	  char line[20];
	  sprintf(line,"%d",linen);
	  if(!value || value==NULL_TREE){
	     tree_info(file,func,line,"打印tree start: tree=NULL");
	     return;
	  }
	  if(value==error_mark_node){
	     tree_info(file,func,line,"打印tree start: tree=error_mark_node");
	     return;
	  }
	  tree type=NULL_TREE;//TREE_TYPE(value);
     bool has=tree_node_has_type(value);
	   if(has){
	      if(TYPE_P(value))
	         type=value;
	      else
	         type=TREE_TYPE(value);
	   }
      tree_info(file,func,line,"打印tree start: %s typeMode:%d",get_tree_code_name(TREE_CODE(value)),type?TYPE_MODE(type):-1);
      printNode(value);
      tree_info(file,func,line,"打印tree end: %s",get_tree_code_name(TREE_CODE(value)));
}

void  aet_print_tree_skip_debug(tree node)
{
      printNode(node);
}

static inline  nuint64 getTime()
 {
	struct timeval tve;
	gettimeofday(&tve,NULL);
	return tve.tv_sec*1000+tve.tv_usec/1000;
 }

void  aet_print_time_from(char *file,char *func ,int linen,char *format,...)
{
	  if(!n_log_is_debug())
		   return ;
	  nboolean re=n_log_is_debug_file(file,func);
	  if(!re){
		  return;
	  }
	  static nuint64 ctime=1;
	  char line[20];
	  sprintf(line,"%d",linen);
	  va_list args;
	  int retval;
	  va_start (args, format);
	  retval = vprintf (format, args);
	  va_end (args);
	  nuint64 time=getTime();
	  printf("  time:%llu,",time-ctime);
	  ctime=time;
	  printf("\n");
}

void    aet_print_location(location_t loc)
{
     expanded_location xloc;
     xloc = expand_location(loc);
     printf("位置是:%d %d %s\n",xloc.line, xloc.column,xloc.file);
}

char *aet_print_get_storage_class_string(int kind)
{
   return  storage_class_str[kind];
}

//zclei 测试用
void aet_print_micro()
{
    int value=0;
    #ifdef ASM_OUTPUT_EXTERNAL
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_EXTERNAL:%d\n",value);

    #ifdef BSS_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  BSS_SECTION_ASM_OP :%d\n",value);

    #ifdef ASM_OUTPUT_ALIGNED_BSS
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_ALIGNED_BSS :%d\n",value);


    #ifdef ASM_DECLARE_OBJECT_NAME
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_DECLARE_OBJECT_NAME :%d\n",value);

    #ifdef USE_SELECT_SECTION_FOR_FUNCTIONS
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  USE_SELECT_SECTION_FOR_FUNCTIONS :%d\n",value);


    #ifdef REGISTER_PREFIX
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  REGISTER_PREFIX :%d\n",value);

    #ifdef OVERLAPPING_REGISTER_NAMES
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  OVERLAPPING_REGISTER_NAMES :%d\n",value);

    #ifdef ADDITIONAL_REGISTER_NAMES
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ADDITIONAL_REGISTER_NAMES :%d\n",value);

    #ifdef DATA_ABI_ALIGNMENT
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  DATA_ABI_ALIGNMENT :%d\n",value);

    #ifdef DATA_ALIGNMENT
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  DATA_ALIGNMENT :%d\n",value);

    #ifdef RETURN_ADDRESS_POINTER_REGNUM
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  RETURN_ADDRESS_POINTER_REGNUM :%d\n",value);

    #ifdef ASM_DECLARE_REGISTER_GLOBAL
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_DECLARE_REGISTER_GLOBAL :%d\n",value);

    #ifdef DTORS_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  DTORS_SECTION_ASM_OP :%d\n",value);

    #ifdef CTORS_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  CTORS_SECTION_ASM_OP :%d\n",value);

    #ifdef ASM_OUTPUT_MAX_SKIP_ALIGN
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_MAX_SKIP_ALIGN :%d\n",value);

    #ifdef ASM_OUTPUT_FUNCTION_PREFIX
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_FUNCTION_PREFIX :%d\n",value);

    #ifdef ASM_DECLARE_FUNCTION_NAME
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_DECLARE_FUNCTION_NAME :%d\n",value);

    #ifdef ASM_DECLARE_FUNCTION_SIZE
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_DECLARE_FUNCTION_SIZE :%d\n",value);

    #ifdef ASM_DECLARE_COLD_FUNCTION_SIZE
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_DECLARE_COLD_FUNCTION_SIZE :%d\n",value);

    #ifdef ASM_NO_SKIP_IN_TEXT
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_NO_SKIP_IN_TEXT :%d\n",value);

    #ifdef ASM_OUTPUT_TLS_COMMON
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_TLS_COMMON :%d\n",value);

    #ifdef ASM_OUTPUT_ALIGNED_DECL_LOCAL
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_ALIGNED_DECL_LOCAL :%d\n",value);

    #ifdef ASM_OUTPUT_ALIGNED_LOCAL
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_ALIGNED_LOCAL :%d\n",value);


    #ifdef TRAMPOLINE_SECTION
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  TRAMPOLINE_SECTION :%d\n",value);

    #ifdef ASM_OUTPUT_SPECIAL_POOL_ENTRY
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_SPECIAL_POOL_ENTRY :%d\n",value);

    #ifdef ASM_OUTPUT_DEF
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_DEF :%d\n",value);

    #ifdef ASM_OUTPUT_POOL_PROLOGUE
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_POOL_PROLOGUE :%d\n",value);

    #ifdef ASM_OUTPUT_POOL_EPILOGUE
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_POOL_EPILOGUE :%d\n",value);

    #ifdef ASM_OUTPUT_FDESC
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_FDESC :%d\n",value);

    #ifdef ASM_WEAKEN_DECL
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_WEAKEN_DECL :%d\n",value);

    #ifdef ASM_WEAKEN_LABEL
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_WEAKEN_LABEL :%d\n",value);

    #ifdef ASM_OUTPUT_WEAK_ALIAS
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_WEAK_ALIAS :%d\n",value);

    #ifdef ASM_OUTPUT_WEAKREF
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_WEAKREF :%d\n",value);

    #ifdef ASM_OUTPUT_SYMVER_DIRECTIVE
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_SYMVER_DIRECTIVE :%d\n",value);

    #ifdef HAVE_GAS_HIDDEN
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  HAVE_GAS_HIDDEN :%d\n",value);

    #ifdef MAKE_DECL_ONE_ONLY
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  MAKE_DECL_ONE_ONLY :%d\n",value);

    #ifdef TEXT_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  TEXT_SECTION_ASM_OP :%d\n",value);

    #ifdef DATA_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  DATA_SECTION_ASM_OP :%d\n",value);

    #ifdef SDATA_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  SDATA_SECTION_ASM_OP :%d\n",value);

    #ifdef READONLY_DATA_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  READONLY_DATA_SECTION_ASM_OP :%d\n",value);

    #ifdef BSS_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  BSS_SECTION_ASM_OP :%d\n",value);

    #ifdef SBSS_SECTION_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  SBSS_SECTION_ASM_OP :%d\n",value);

    #ifdef MACH_DEP_SECTION_ASM_FLAG
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  MACH_DEP_SECTION_ASM_FLAG :%d\n",value);

    #ifdef GLOBAL_ASM_OP
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  GLOBAL_ASM_OP :%d\n",value);

    #ifdef ASM_OUTPUT_SOURCE_FILENAME
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ASM_OUTPUT_SOURCE_FILENAME :%d\n",value);

    #ifdef ELF_ASCII_ESCAPES
    value=1;
    #else
    value=0;
    #endif
    fprintf(stderr,"宏定义--  ELF_ASCII_ESCAPES :%d\n",value);

#if defined (OBJECT_FORMAT_ELF)
    value=1;
#else
    value=0;
#endif
    fprintf(stderr,"宏定义--  OBJECT_FORMAT_ELF :%d\n",value);

    fprintf(stderr,"宏定义--  TARGET_PECOFF :%d\n",TARGET_PECOFF);

#if defined HOST_WIDE_INT
    value=1;
#else
    value=0;
#endif
    fprintf(stderr,"宏定义--  HOST_WIDE_INT :%d\n",value);

#ifdef ONLY_FIXED_SIZE_MODES
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ONLY_FIXED_SIZE_MODES :%d\n",value);

#ifdef ASSEMBLER_DIALECT
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASSEMBLER_DIALECT :%d\n",value);

#ifdef GENERATOR_FILE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  GENERATOR_FILE :%d\n",value);

#ifdef ASM_OUTPUT_OPCODE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_OPCODE :%d\n",value);

#ifdef HAVE_LD_EH_GC_SECTIONS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  HAVE_LD_EH_GC_SECTIONS :%d\n",value);

  fprintf(stderr,"宏定义--  HAVE_COMDAT_GROUP :%d\n",HAVE_COMDAT_GROUP);

  fprintf(stderr,"宏定义--  HAVE_AS_LEB128 :%d\n",HAVE_AS_LEB128);

#ifdef GLOBAL_ASM_OP
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  GLOBAL_ASM_OP :%d\n",value);

#ifdef ASM_OUTPUT_REG_PUSH
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_REG_PUSH :%d\n",value);

#ifdef ASM_WEAKEN_LABEL
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_WEAKEN_LABEL :%d\n",value);

#ifdef ASM_OUTPUT_TYPE_DIRECTIVE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_TYPE_DIRECTIVE :%d\n",value);

#ifdef ASM_OUTPUT_CASE_LABEL
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_CASE_LABEL :%d\n",value);

#ifdef ASM_OUTPUT_ADDR_VEC_ELT
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_ADDR_VEC_ELT :%d\n",value);

#ifdef ASM_OUTPUT_ADDR_DIFF_ELT
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_ADDR_DIFF_ELT :%d\n",value);

#ifdef ASM_OUTPUT_CASE_END
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_CASE_END :%d\n",value);



#if defined(ASM_OUTPUT_ADDR_VEC)
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_ADDR_VEC :%d\n",value);


#if defined(ASM_OUTPUT_ADDR_DIFF_VEC)
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_ADDR_DIFF_VEC :%d\n",value);


#if defined(ASM_OUTPUT_ALIGNED_DECL_COMMON)
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_ALIGNED_DECL_COMMON :%d\n",value);

#if defined(ASM_OUTPUT_ALIGNED_COMMON)
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_ALIGNED_COMMON :%d\n",value);

#ifdef ASM_DECLARE_COLD_FUNCTION_NAME
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_DECLARE_COLD_FUNCTION_NAME :%d\n",value);


#ifdef REG_ALLOC_ORDER
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  REG_ALLOC_ORDER :%d\n",value);

#ifdef PUSH_ROUNDING
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  PUSH_ROUNDING :%d\n",value);

#ifdef INSN_SCHEDULING
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  INSN_SCHEDULING :%d\n",value);

#ifdef GO_IF_LEGITIMATE_ADDRESS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  GO_IF_LEGITIMATE_ADDRESS :%d\n",value);

#if TARGET_SUPPORTS_WIDE_INT
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  TARGET_SUPPORTS_WIDE_INT :%d\n",value);


#ifdef POINTERS_EXTEND_UNSIGNED
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  POINTERS_EXTEND_UNSIGNED :%d\n",value);

#ifdef POINTERS_EXTEND_UNSIGNED
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  POINTERS_EXTEND_UNSIGNED :%d\n",value);

#ifdef STACK_PARMS_IN_REG_PARM_AREA
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STACK_PARMS_IN_REG_PARM_AREA :%d\n",value);

#ifdef BLOCK_REG_PADDING
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  BLOCK_REG_PADDING :%d\n",value);


#ifdef SPARC_STACK_BOUNDARY_HACK
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  SPARC_STACK_BOUNDARY_HACK :%d\n",value);

#ifdef PCC_STATIC_STRUCT_RETURN
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  PCC_STATIC_STRUCT_RETURN :%d\n",value);

#ifdef INIT_CUMULATIVE_LIBCALL_ARGS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  INIT_CUMULATIVE_LIBCALL_ARGS :%d\n",value);

#ifdef REG_PARM_STACK_SPACE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  REG_PARM_STACK_SPACE :%d\n",value);

#ifdef STATIC_CHAIN_INCOMING_REGNUM
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STATIC_CHAIN_INCOMING_REGNUM :%d\n",value);

#ifdef STATIC_CHAIN_REGNUM
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STATIC_CHAIN_REGNUM :%d\n",value);

#ifdef CONFIG_SJLJ_EXCEPTIONS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  CONFIG_SJLJ_EXCEPTIONS :%d \n",value);

#ifdef DWARF2_UNWIND_INFO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  DWARF2_UNWIND_INFO :%d \n",value);

  //targetm_common.except_unwind_info (&global_options);

#ifdef STACK_CHECK_PROTECT
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STACK_CHECK_PROTECT :%d \n",value);

#ifdef INIT_EXPANDERS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  INIT_EXPANDERS :%d \n",value);

#ifdef STACK_REGS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STACK_REGS :%d \n",value);

#ifdef DWARF2_DEBUGGING_INFO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  DWARF2_DEBUGGING_INFO :%d \n",value);

#ifdef CTF_DEBUGGING_INFO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  CTF_DEBUGGING_INFO :%d \n",value);

#ifdef BTF_DEBUGGING_INFO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  BTF_DEBUGGING_INFO :%d \n",value);

 #ifdef VMS_DEBUGGING_INFO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  VMS_DEBUGGING_INFO :%d \n",value);

#ifdef DWARF2_LINENO_DEBUGGING_INFO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  DWARF2_LINENO_DEBUGGING_INFO :%d \n",value);

#if SWITCHABLE_TARGET
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  SWITCHABLE_TARGET :%d \n",value);

#ifdef ENABLE_LTO
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ENABLE_LTO :%d \n",value);

#ifdef OVERRIDE_ABI_FORMAT
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  OVERRIDE_ABI_FORMAT :%d \n",value);

#ifdef SUBALIGN_LOG
  value=1;
  fprintf(stderr,"SUBALIGN_LOG --- %d\n",SUBALIGN_LOG);
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  SUBALIGN_LOG :%d \n",value);

#ifdef PROMOTE_MODE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  PROMOTE_MODE :%d \n",value);

#ifdef STACK_BOUNDARY
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STACK_BOUNDARY :%d\n",value);

#ifdef INIT_CUMULATIVE_INCOMING_ARGS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  INIT_CUMULATIVE_INCOMING_ARGS :%d\n",value);

#ifdef INCOMING_REG_PARM_STACK_SPACE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  INCOMING_REG_PARM_STACK_SPACE :%d\n",value);

#ifdef EH_RETURN_STACKADJ_RTX
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  EH_RETURN_STACKADJ_RTX :%d\n",value);

#ifdef DWARF_FRAME_REGNUM
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  DWARF_FRAME_REGNUM :%d\n",value);

#ifdef APPLY_RESULT_SIZE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  APPLY_RESULT_SIZE :%d\n",value);

#ifdef RETURN_ADDR_RTX
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  RETURN_ADDR_RTX :%d\n",value);

#ifdef STACK_ADDRESS_OFFSET
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STACK_ADDRESS_OFFSET :%d\n",value);

#ifdef RED_ZONE_SIZE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  RED_ZONE_SIZE :%d\n",value);

#ifdef BLOCK_REG_PADDING
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  BLOCK_REG_PADDING :%d\n",value);

#ifdef DONT_USE_BUILTIN_SETJMP
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  DONT_USE_BUILTIN_SETJMP :%d\n",value);

#ifdef JMP_BUF_SIZE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  JMP_BUF_SIZE :%d\n",value);


#ifdef ROUND_TYPE_ALIGN
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ROUND_TYPE_ALIGN :%d\n",value);

#ifdef STRUCTURE_SIZE_BOUNDARY
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  STRUCTURE_SIZE_BOUNDARY :%d\n",value);

#ifdef STRUCTURE_SIZE_BOUNDARY
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  EMPTY_FIELD_BOUNDARY :%d\n",value);

#ifdef PROFILE_HOOK
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  PROFILE_HOOK :%d\n",value);

#ifdef BUFSIZ
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  BUFSIZ :%d\n",value);


#ifdef FRAME_POINTER_CFA_OFFSET
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  FRAME_POINTER_CFA_OFFSET :%d\n",value);

#ifdef CALL_POPS_ARGS
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  CALL_POPS_ARGS :%d\n",value);

#ifdef ENABLE_RTL_CHECKING
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ENABLE_RTL_CHECKING :%d\n",value);

#ifdef INCOMING_RETURN_ADDR_RTX
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  INCOMING_RETURN_ADDR_RTX :%d\n",value);

#ifdef EH_RETURN_TAKEN_RTX
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  EH_RETURN_TAKEN_RTX :%d\n",value);

#ifdef FLOAT_STORE_FLAG_VALUE
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  FLOAT_STORE_FLAG_VALUE :%d\n",value);

#ifdef AVOID_CCMODE_COPIES
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  AVOID_CCMODE_COPIES :%d\n",value);

#ifdef ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX :%d\n",value);

#ifdef ASM_OUTPUT_DWARF_DATAREL
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_DWARF_DATAREL :%d\n",value);

#ifdef ASM_OUTPUT_DWARF_PCREL
  value=1;
  #else
  value=0;
  #endif
  fprintf(stderr,"宏定义--  ASM_OUTPUT_DWARF_PCREL :%d\n",value);

#ifdef EH_FRAME_SECTION_NAME
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  EH_FRAME_SECTION_NAME :%d\n",value);

#ifdef DEBUG_DEBUG_STRUCT
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  DEBUG_DEBUG_STRUCT :%d\n",value);

#ifdef EH_FRAME_THROUGH_COLLECT2
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  EH_FRAME_THROUGH_COLLECT2 :%d\n",value);

#ifdef ASM_OUTPUT_DWARF_DELTA
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  ASM_OUTPUT_DWARF_DELTA :%d\n",value);

#ifdef CODEVIEW_DEBUGGING_INFO
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  CODEVIEW_DEBUGGING_INFO :%d\n",value);

#ifdef DW_LLE_view_pair
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  DW_LLE_view_pair :%d\n",value);


#ifdef LEAF_REG_REMAP
  value=1;
#else
  value=0;
#endif
fprintf(stderr,"宏定义--  LEAF_REG_REMAP :%d\n",value);



  struct asm_int_op *ops;
  ops = &targetm.asm_out.aligned_op;
  fprintf(stderr,"asm_int_op  aligned_op:\n");
  fprintf(stderr,"\thi:%s\n",ops->hi);
  fprintf(stderr,"\tpsi:%s\n",ops->psi);
  fprintf(stderr,"\tsi:%s\n",ops->si);
  fprintf(stderr,"\tpdi:%s\n",ops->pdi);
  fprintf(stderr,"\tdi:%s\n",ops->di);
  fprintf(stderr,"\tpti:%s\n",ops->pti);
  fprintf(stderr,"\tti:%s\n",ops->ti);
  ops = &targetm.asm_out.unaligned_op;
  fprintf(stderr,"asm_int_op  unaligned_op:\n");
  fprintf(stderr,"\thi:%s\n",ops->hi);
  fprintf(stderr,"\tpsi:%s\n",ops->psi);
  fprintf(stderr,"\tsi:%s\n",ops->si);
  fprintf(stderr,"\tpdi:%s\n",ops->pdi);
  fprintf(stderr,"\tdi:%s\n",ops->di);
  fprintf(stderr,"\tpti:%s\n",ops->pti);
  fprintf(stderr,"\tti:%s\n",ops->ti);
  fprintf(stderr,"targetm.asm_out.byte_op %s\n",targetm.asm_out.byte_op);


}

