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
#include "cgraph.h"
#include "attribs.h"
#include "toplev.h"
#include "asan.h"
#include "opts.h"

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"
#include "c-family/c-ubsan.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "classmgr.h"
#include "parserhelp.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"

#include "varmgr.h"
#include "parserstatic.h"
#include "classutil.h"
#include "aet-c-parser-header.h"
#include "newobject.h"
#include "funcmgr.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericfile.h"
#include "genericquery.h"
#include "classaccess.h"
#include "genericcall.h"
#include "makefileparm.h"

#define VOID_POINTER 1
#define BASE_POINTER 2
#define LAST_VOID_PARM 3

#define UNKNOWN_POINTER 0

static inline tree canonicalize_for_substitution (tree node)
{
  /* For a TYPE_DECL, use the type instead.  */
  if (TREE_CODE (node) == TYPE_DECL)
    node = TREE_TYPE (node);
  if (TYPE_P (node)  && TYPE_CANONICAL (node) != node   && TYPE_MAIN_VARIANT (node) != node){
      tree orig = node;
      if (TREE_CODE (node) == FUNCTION_TYPE)
	      node = build_qualified_type (TYPE_MAIN_VARIANT (node),TYPE_QUALS (node));
  }
  return node;
}

static int getQualCount(const tree type)
{
  int num_qualifiers = 0;
  int quals = TYPE_QUALS (type);
  if (quals & TYPE_QUAL_RESTRICT)
    {
      ++num_qualifiers;
    }
  if (quals & TYPE_QUAL_VOLATILE)
    {
      ++num_qualifiers;
    }
  if (quals & TYPE_QUAL_CONST)
    {
      ++num_qualifiers;
    }

  return num_qualifiers;
}

static nboolean compareQual(tree parm1,tree parm2)
{
	tree c1=canonicalize_for_substitution(parm1);
	tree c2=canonicalize_for_substitution(parm2);
	int q1 = TYPE_QUALS (c1);
	int q2 = TYPE_QUALS (c2);
	nboolean q1r=(q1 & TYPE_QUAL_RESTRICT);
	nboolean q2r=(q2 & TYPE_QUAL_RESTRICT);
	nboolean q1v=(q1 & TYPE_QUAL_VOLATILE);
	nboolean q2v=(q2 & TYPE_QUAL_VOLATILE);
	nboolean q1c=(q1 & TYPE_QUAL_CONST);
	nboolean q2c=(q2 & TYPE_QUAL_CONST);
	nboolean re=(q1r==q2r &&  q1v==q2v &&  q1c==q2c);
	return re;
}

static int getParmCount(tree funcType)
{
	int count=0;
	for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
	    count++;
	}
	return count;
}

static tree getParm(tree funcType,int index)
{
	int count=0;
	for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
	    tree parm = TREE_VALUE (al);
		if(count==index)
			return parm;
	    count++;
	}
	return NULL_TREE;
}


//两咱类型
//void *
static int getType(tree type)
{
  /* This gets set to nonzero if TYPE turns out to be a (possibly
     CV-qualified) builtin type.  */
   int is_builtin_type = 0;
   if (type == error_mark_node)
      return UNKNOWN_POINTER;
   type = canonicalize_for_substitution (type);
   if (getQualCount (type) > 0){
	  //printf("进这里 00 code:%s\n",get_tree_code_name(TREE_CODE(type)));
	  tree t = TYPE_MAIN_VARIANT (type);
	  //printf("进这里 11 code:%s code:%s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(t)));
	  type = TYPE_MAIN_VARIANT (t);
   }else{
      type = TYPE_MAIN_VARIANT (type);
   }
   if(TREE_CODE (type)==VOID_TYPE){
	   return LAST_VOID_PARM;
   }

   if(TREE_CODE (type)==POINTER_TYPE){
	 tree target = TREE_TYPE (type);
	 if(TREE_CODE (target)==VOID_TYPE){
		 return VOID_POINTER;
	 }else{
		 switch (TREE_CODE (target)){
		   case VOID_TYPE:
		   case BOOLEAN_TYPE:
		   case INTEGER_TYPE:  /* Includes wchar_t.  */
		   case REAL_TYPE:
		   case FIXED_POINT_TYPE:
		   case COMPLEX_TYPE:
		   case UNION_TYPE:
		   case RECORD_TYPE:
		   case ENUMERAL_TYPE:
			  return BASE_POINTER;
		}
	 }
   }
   return UNKNOWN_POINTER;
}

/**
 * 比较两个函数的参数是否相同
 * 见AHashTable
 */
nboolean parser_help_compare(tree funcType1,tree funcType2)
{
	int count1=getParmCount(funcType1);
	int count2=getParmCount(funcType2);
	if(count1!=count2)
		return FALSE;
	if(count1==0)
		return TRUE;
	int i;
	for(i=0;i<count1;i++){
		tree parm1=getParm(funcType1,i);
		tree parm2=getParm(funcType2,i);
		if(parm1==NULL_TREE || parm2==NULL_TREE )
			return FALSE;
		nboolean qual=compareQual(parm1,parm2);
		if(!qual)
			return FALSE;
		 int t1=getType(parm1);
		 int t2=getType(parm2);
		 if(t1==UNKNOWN_POINTER || t2==UNKNOWN_POINTER)
			 return FALSE;
	}
	return TRUE;
}









