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
#include "c/c-parser.h"
#include "aetinfo.h"
#include "aet-c-parser-header.h"
#include "aetprinttree.h"
#include "aetutils.h"

static c_parser *aetPrintParser=NULL;


#define tree_info(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_INFO,file,line,func,format, ##__VA_ARGS__);

#define tree_debug(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_DEBUG,file,line,func,format, ##__VA_ARGS__);

#define tree_warning(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_WARNING,file,line,func,format, ##__VA_ARGS__);

static void printNode(tree node)
{
	  if(!node || node==NULL_TREE){
		 printf("tree 是空的\n");
	     return;
	  }
	  if(node==error_mark_node){
	     printf("tree 是error_mark_node\n");
	     return;
	  }
	  int i;
	  dump_info di;
	  FILE *dump_orig;
	  dump_flags_t local_dump_flags;
	  dump_orig = get_dump_info (TDI_original, &local_dump_flags);
	  dump_orig = dump_begin (TDI_original, &local_dump_flags);
	  if(!dump_orig)
		  dump_orig=stderr;
	  dump_node(node,TDF_ALL_VALUES|local_dump_flags,dump_orig);
}

void  aet_print_specs_from(struct c_declspecs *specs,char *file,char *func,int linen)
{
	if(!n_log_is_debug())
	   return ;
    nboolean re=n_log_is_print_file(file);
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
				aet_c_storage_class_str[specs->storage_class],xloc.file, xloc.line, xloc.column);

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
	   nboolean re=n_log_is_print_file(file);
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
		   printf("vvdddxx %p\n",temp->u.id.id);
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

void  aet_print_tree_from(tree value,char *file,char *func,int linen)
{
	  if(!n_log_is_debug())
		   return ;
	  nboolean re=n_log_is_print_file(file);
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
      tree_info(file,func,line,"打印tree start: %s",get_tree_code_name(TREE_CODE(value)));
      printNode(value);
      tree_info(file,func,line,"打印tree end: %s",get_tree_code_name(TREE_CODE(value)));
}


static void printToken(c_token *ct,char *file,char *func,char *line)
{
   if(!ct)
	 return;
   tree value=ct->value;
   expanded_location xloc;
   xloc = expand_location(ct->location);
   const char *str1;
   char  numberStr[25];
   int isData=0;
   if(ct->type==CPP_NAME)
      str1=IDENTIFIER_POINTER (value);
   else if(ct->type==CPP_NUMBER){
	   wide_int result=wi::to_wide(value);
	   int v=result.to_shwi();
	   sprintf(numberStr,"%d",v);
	   isData=1;
   }else if(ct->type==CPP_STRING){
	   str1  = TREE_STRING_POINTER (value);
   }else{
	   str1="unknown";
   }
   //只有CPP_NAME才有IDENTIFIER_POINTER,否则编译app时出段错误。
  static int pp=1;
  tree_debug(file,func,line ,"c_token:[%d] name:%s cpp_ttype:%-12s c_id_kind:%-10s rid:%d ridStr:%-12s %d,%d %8s %u",
		  pp++, isData?numberStr:str1,aet_cpp_ttype_str[ct->type], aet_c_id_kind_str[ct->id_kind],
				  ct->type==CPP_KEYWORD?ct->keyword:-1,ct->type==CPP_KEYWORD?aet_utils_get_keyword_string(ct):"no",
		  xloc.line, xloc.column,xloc.file,ct->location);/* 打印符号值的基本信息 */
}

void aet_print_token_from(c_token *ct,char *file,char *func,int linen)
{
	  if(!ct)
		return;
	  if(!n_log_is_debug())
		   return ;
	  nboolean re=n_log_is_print_file(file);
	  if(!re){
		  return;
	  }
	  char line[20];
	  sprintf(line,"%d",linen);
	  printToken(ct,file,func,line);
}

void  aet_print_parser_from(char *file,char *func ,int linen,char *format,...)
{
	  if(!n_log_is_debug())
		   return ;
	  nboolean re=n_log_is_print_file(file);
	  if(!re){
		  return;
	  }
	  char line[20];
	  sprintf(line,"%d",linen);
	  tree_debug(file,func,line,"aet_print_parser 开始:");
	  va_list args;
	  int retval;
	  va_start (args, format);
	  retval = vprintf (format, args);
	  va_end (args);
	  printf("\n");
	  printToken(c_parser_peek_token (aetPrintParser),file,func,line);
	  printToken(c_parser_peek_2nd_token (aetPrintParser),file,func,line);
	  int total=aetPrintParser->tokens_avail;
	  int i;
	  for(i=3;i<=total;i++)
		  printToken(c_parser_peek_nth_token (aetPrintParser,i),file,func,line);
	  tree_debug(file,func,line,"aet_print_parser 完成");
}


void  aet_print_set_parser(c_parser *parser)
{
	aetPrintParser=parser;
}

void           aet_print_tree_skip_debug(tree node)
{
      if(!node || node==NULL_TREE){
    	  printf("printNode aet_print_skip_debug_tree tree is null tree\n");
    	  return;
      }
	  int i;
	  FILE *dump_orig;
	  dump_flags_t local_dump_flags;
	  dump_orig = get_dump_info (TDI_original, &local_dump_flags);
	  dump_orig = dump_begin (TDI_original, &local_dump_flags);
	  if(!dump_orig)
		  dump_orig=stderr;
	  dump_node(node,TDF_ALL_VALUES|local_dump_flags,dump_orig);
}

void           aet_print_token_skip_debug(c_token *ct)
{
	      if(!ct)
			return;
	      tree value=ct->value;
	       expanded_location xloc;
	       xloc = expand_location(ct->location);
	       const char *str1;
	       char  numberStr[25];
	       int isData=0;
	       if(ct->type==CPP_NAME)
	          str1=IDENTIFIER_POINTER (value);
	       else if(ct->type==CPP_NUMBER){
	    	   wide_int result=wi::to_wide(value);
	    	   int v=result.to_shwi();
	    	   sprintf(numberStr,"%d",v);
	    	   isData=1;
	       }else if(ct->type==CPP_STRING){
	    	   str1  = TREE_STRING_POINTER (value);
	       }else{
	    	   str1="unknown";
	       }
	       int pp=0;
	      printf("c_token:[%d] name:%s cpp_ttype:%-12s c_id_kind:%-10s rid:%d ridStr:%-12s %d,%d %8s\n",
	    		  pp++, isData?numberStr:str1,aet_cpp_ttype_str[ct->type], aet_c_id_kind_str[ct->id_kind],
	    				  ct->type==CPP_KEYWORD?ct->keyword:-1,ct->type==CPP_KEYWORD?aet_utils_get_keyword_string(ct):"no",
	    		  xloc.line, xloc.column,xloc.file);/* 打印符号值的基本信息 */
}

static inline  nuint64 getTime()
 {
	struct timeval tve;
	gettimeofday(&tve,NULL);
	return tve.tv_sec*1000+tve.tv_usec/1000;
 }

void  aet_print_time_from(char *file,char *func ,int linen,char *format,...)
{
//	  if(!n_log_is_debug())
//		   return ;
//	  nboolean re=n_log_is_print_file(file);
//	  if(!re){
//		  return;
//	  }
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


