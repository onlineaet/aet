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
#include "c/c-parser.h"
#include "dumpfile.h"
#include "c-family/c-ubsan.h"
#include "tree-nested.h"
#include "context.h"
#include "langhooks.h"
#include "c/c-lang.h"

#include "aetinfo.h"
#include "aet-c-parser-header.h"
#include "aetprinttoken.h"


static c_parser *aetPrintParser=NULL;

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



#define tree_debug(file,func,line,format,...)   \
		n_log_structured_standard(N_LOG_LEVEL_DEBUG,file,line,func,format, ##__VA_ARGS__);


static char *getKeyword(c_token *token)
{
    int i;
    for (i = 0; i < num_c_common_reswords; i++){
       const c_common_resword *resword = &c_common_reswords[i];
       if (token->keyword==resword->rid){
          return resword->word;
       }
    }
    return NULL;
}

/* Print to FILE a NODE representing a REAL_CST constant, including
   Infinity and NaN.  Be verbose when BFRIEF is false.  */
//原型 print_real_cst print-tree.cc
static void print_real_cst (char *dest, const_tree node, bool brief)
{
   NString *str=n_string_new("");
   if (TREE_OVERFLOW (node))
      n_string_append(str," overflow");

   REAL_VALUE_TYPE d = TREE_REAL_CST (node);
   if (REAL_VALUE_ISINF (d))
      n_string_append(str,REAL_VALUE_NEGATIVE (d) ? " -Inf" : " Inf");
   else if (REAL_VALUE_ISNAN (d)){
      /* Print a NaN in the format [-][Q]NaN[(significand[exponent])]
      where significand is a hexadecimal string that starts with
      the 0x prefix followed by 0 if the number is not canonical
      and a non-zero digit if it is, and exponent is decimal.  */
      unsigned start = 0;
      const char *psig = (const char *) d.sig;
      for (unsigned i = 0; i != sizeof d.sig; ++i)
         if (psig[i]){
            start = i;
            break;
         }

      n_string_append_printf(str, " %s%sNaN", d.sign ? "-" : "",d.signalling ? "S" : "Q");

      if (brief)
         goto out;

      if (start)
         n_string_append_printf(str, "(0x%s", d.canonical ? "" : "0");
      else if (d.uexp)
         n_string_append_printf(str, "(%s", d.canonical ? "" : "0");
      else if (!d.canonical){
         n_string_append(str, "(0)");
         goto out;
      }

      if (psig[start]){
         for (unsigned i = start; i != sizeof d.sig; ++i)
            if (i == start)
               n_string_append_printf(str,  "%x", psig[i]);
            else
               n_string_append_printf(str,  "%02x", psig[i]);
      }

      if (d.uexp)
         n_string_append_printf(str,  "%se%u)", psig[start] ? "," : "", d.uexp);
      else if (psig[start])
         n_string_append_c(str,')');
   }else{
      char string[64];
      real_to_decimal (string, &d, sizeof (string), 0, 1);
      n_string_append_printf(str,  " %s", string);
   }
out:
   sprintf(dest,"%s",str->str);
   n_string_free(str,TRUE);
}

static void printToken(c_token *ct,char *file,char *func,char *line)
{
   if(!ct)
	 return;
   tree value=ct->value;
   expanded_location xloc;
   xloc = expand_location(ct->location);
   const char *str1;
   char  numberStr[255];
   char *ridStr="no";
   int isData=0;
   if(ct->type==CPP_NAME)
      str1=IDENTIFIER_POINTER (value);
   else if(ct->type==CPP_NUMBER){
      if (TREE_CODE (value) == REAL_CST)
         print_real_cst(numberStr,value,true);
      else{
         wide_int result=wi::to_wide(value);
         int v=result.to_shwi();
         sprintf(numberStr,"%d",v);
      }
	   str1=numberStr;
   }else if(ct->type==CPP_STRING){
	   str1  = TREE_STRING_POINTER (value);
   }else if(ct->type==CPP_KEYWORD){
       str1="keyword";
       ridStr=getKeyword(ct);
   }else{
	   str1="unknown";
   }
   //只有CPP_NAME才有IDENTIFIER_POINTER,否则编译app时出段错误。
  static int pp=1;
  tree_debug(file,func,line ,"c_token:[%d] name:%s cpp_ttype:%s c_id_kind:%s rid:%d ridStr:%s %d,%d %8s %u",
		  pp++, str1,cpp_type2name(ct->type,0), aet_c_id_kind_str[ct->id_kind],
				  ct->type==CPP_KEYWORD?ct->keyword:-1,ridStr,
		  xloc.line, xloc.column,xloc.file,ct->location);/* 打印符号值的基本信息 */
}

void aet_print_token_from(c_token *ct,const char *file,const char *func,int linen)
{
	  if(!ct)
		return;
	  if(!n_log_is_debug())
		   return ;
	  nboolean re=n_log_is_debug_file(file,func);
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
	  nboolean re=n_log_is_debug_file(file,func);
	  if(!re){
		  return;
	  }
	  char line[20];
	  sprintf(line,"%d",linen);
	  tree_debug(file,func,line,"aet_print_token 开始:");
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
	  tree_debug(file,func,line,"aet_print_token 完成");
}


void  aet_print_set_parser(c_parser *parser)
{
	aetPrintParser=parser;
}




