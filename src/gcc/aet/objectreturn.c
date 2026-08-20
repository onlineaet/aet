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
#include "attribs.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "tree-iterator.h"
#include "bitmap.h"
#include "tree-nested.h"
#include "opts.h"

#include "aet-c-parser-header.h"

#include "c-aet.h"
#include "aetutils.h"
#include "classmgr.h"
#include "classimpl.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "parserstatic.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "newobject.h"
#include "funcmgr.h"
#include "genericutil.h"
#include "blockmgr.h"
#include "genericquery.h"
#include "classaccess.h"
#include "genericcall.h"
#include "parserhelp.h"
#include "classinit.h"
#include "objectreturn.h"

typedef struct _TlsData
{
   tree fndecl;//泛型块函数
   tree tlsVar;//在泛型块函数声明的tls变量
}TlsData;

static void objectReturnlInit(ObjectReturn *self)
{
   self->dataArray=n_ptr_array_new();
   self->classCast=class_cast_new();
   //泛型块函数中播入的tls
   self->tlsDataArray=n_ptr_array_new();
}


typedef struct _DataReturn{
	  tree function;
	  bitmap named_ret_val;
	  vec<tree, va_gc> *other_ret_val;
	  int gnat_ret;
}DataReturn;

static DataReturn *getDataReturn(ObjectReturn *self,tree func)
{
   int i;
   for(i=0;i<self->dataArray->len;i++){
      DataReturn *data=(DataReturn *)n_ptr_array_index(self->dataArray,i);
      if(data->function==func)
         return data;
   }
   return NULL;
}

static DataReturn *createDataReturn(tree func)
{
	DataReturn *item=(DataReturn *)n_slice_new(DataReturn);
	item->function=func;
	item->named_ret_val=BITMAP_GGC_ALLOC ();
	item->other_ret_val=make_tree_vector ();
	return item;
}

static void freeDataReturn(DataReturn *data)
{
   bitmap_clear(data->named_ret_val);
   release_tree_vector (data->other_ret_val);
   data->other_ret_val = NULL;
   n_slice_free(DataReturn,data);
}

static void removeDataReturn(ObjectReturn *self,tree func)
{
   int i;
   for(i=0;i<self->dataArray->len;i++){
      DataReturn *data=(DataReturn *)n_ptr_array_index(self->dataArray,i);
      if(data->function==func){
         n_ptr_array_remove(self->dataArray,data);
         freeDataReturn(data);
         return ;
      }
   }
}

static DataReturn *addDataReturn(ObjectReturn *self,tree func)
{
	DataReturn *data=getDataReturn(self,func);
	if(data==NULL){
		data=createDataReturn(func);
		n_ptr_array_add(self->dataArray,data);
	}
	return data;
}

struct nrv_data
{
  bitmap nrv;
  tree result;
  tree varReplace;
  int gnat_ret;
  vec<tree, va_gc> *other;
  hash_set<tree> *visited;
};

struct add_cleanup_data
{
  NPtrArray *dataArray;
  hash_set<tree> *visited;
};

/* Return true if T is a Named Return Value.  */
static inline nboolean is_nrv_p (bitmap nrv, tree t)
{
//    printf("is_nrv_p %p %p\n",nrv,t);
//    printf("is_nrv_p %p %d %d\n",nrv,DECL_UID (t),TREE_CODE (t) == VAR_DECL);
  return TREE_CODE (t) == VAR_DECL && bitmap_bit_p (nrv, DECL_UID (t));
}

/* Helper function for walk_tree, used by finalize_nrv below.  */
static tree prune_nrv_r (tree *tp, int *walk_subtrees, void *data)
{
  struct nrv_data *dp = (struct nrv_data *)data;
  tree t = *tp;
  /* No need to walk into types or decls.  */
  if (IS_TYPE_OR_DECL_P (t))
    *walk_subtrees = 0;
  if (is_nrv_p (dp->nrv, t))
    bitmap_clear_bit (dp->nrv, DECL_UID (t));

  return NULL_TREE;
}

/* Prune Named Return Values in BLOCK and return true if there is still a
   Named Return Value in BLOCK or one of its sub-blocks.  */

static bool prune_nrv_in_block (bitmap nrv, tree block)
{
  bool has_nrv = false;
  tree t;
  /* First recurse on the sub-blocks.  */
  for (t = BLOCK_SUBBLOCKS (block); t; t = BLOCK_CHAIN (t))
    has_nrv |= prune_nrv_in_block (nrv, t);

  /* Then make sure to keep at most one NRV per block.  */
  for (t = BLOCK_VARS (block); t; t = DECL_CHAIN (t))
    if (is_nrv_p (nrv, t)){
	   if (has_nrv)
	     bitmap_clear_bit (nrv, DECL_UID (t));
	   else
	     has_nrv = true;
    }
   return has_nrv;
}

/* Return 1 if the types T1 and T2 are compatible, i.e. if they can be
   transparently converted to each other.  */
static int gnat_types_compatible_p00 (tree t1, tree t2)
{
  enum tree_code code;

  /* This is the default criterion.  */
  if (TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    return 1;

  /* We only check structural equivalence here.  */
  if ((code = TREE_CODE (t1)) != TREE_CODE (t2))
    return 0;

  /* Vector types are also compatible if they have the same number of subparts
     and the same form of (scalar) element type.  */
  if (code == VECTOR_TYPE
      && known_eq (TYPE_VECTOR_SUBPARTS (t1), TYPE_VECTOR_SUBPARTS (t2))
      && TREE_CODE (TREE_TYPE (t1)) == TREE_CODE (TREE_TYPE (t2))
      && TYPE_PRECISION (TREE_TYPE (t1)) == TYPE_PRECISION (TREE_TYPE (t2)))
    return 1;

  /* Array types are also compatible if they are constrained and have the same
     domain(s), the same component type and the same scalar storage order.  */
  if (code == ARRAY_TYPE
      && (TYPE_DOMAIN (t1) == TYPE_DOMAIN (t2)
	  || (TYPE_DOMAIN (t1)
	      && TYPE_DOMAIN (t2)
	      && tree_int_cst_equal (TYPE_MIN_VALUE (TYPE_DOMAIN (t1)),
				     TYPE_MIN_VALUE (TYPE_DOMAIN (t2)))
	      && tree_int_cst_equal (TYPE_MAX_VALUE (TYPE_DOMAIN (t1)),
				     TYPE_MAX_VALUE (TYPE_DOMAIN (t2)))))
      && (TREE_TYPE (t1) == TREE_TYPE (t2)
	  || (TREE_CODE (TREE_TYPE (t1)) == ARRAY_TYPE
	      && gnat_types_compatible_p00 (TREE_TYPE (t1), TREE_TYPE (t2))))
      && TYPE_REVERSE_STORAGE_ORDER (t1) == TYPE_REVERSE_STORAGE_ORDER (t2))
    return 1;

  return 0;
}

/* Return true if EXPR is a useless type conversion.  */
static bool gnat_useless_type_conversion (tree expr)
{
  if (CONVERT_EXPR_P (expr)
      || TREE_CODE (expr) == VIEW_CONVERT_EXPR
      || TREE_CODE (expr) == NON_LVALUE_EXPR)
    return gnat_types_compatible_p00 (TREE_TYPE (expr),
				    TREE_TYPE (TREE_OPERAND (expr, 0)));

  return false;
}

static void walk_nesting_tree (struct cgraph_node *node, walk_tree_fn func, void *data)
{
   //  for (node = node->nested; node; node = node->next_nested)
   //    {
   //      walk_tree_without_duplicates (&DECL_SAVED_TREE (node->decl), func, data);
   //      walk_nesting_tree (node, func, data);
   //    }
   struct cgraph_node *cgn=node;
   for (cgn = first_nested_function (cgn); cgn; cgn = next_nested_function (cgn)){
      walk_tree_without_duplicates (&DECL_SAVED_TREE (cgn->decl), func, data);
      walk_nesting_tree (cgn, func, data);
   }
}


static nboolean isReturnExpr(vec<tree, va_gc> *other,tree t)
{
   if(TREE_CODE(t)!=TARGET_EXPR){
      n_debug("objectreurn isReturnExpr 不是 TARGET_EXPR 没有cleanup。");
      return FALSE;
   }
   tree iter;
   int ix;
   tree arg;
   for (ix = 0; other->iterate (ix, &iter); ++ix){
      if(t==iter)
         return TRUE;
   }
   return FALSE;
}

static tree getClearupCallFromTry(tree val)
{
   if(TREE_CODE(val)==ADDR_EXPR){
      tree op0=TREE_OPERAND (val, 0);
      if(TREE_CODE(op0)==VAR_DECL){
         return op0;
      }
   }else if(TREE_CODE(val)==NOP_EXPR){
      tree add=TREE_OPERAND (val, 0);
      if(TREE_CODE(add)==ADDR_EXPR){
         return getClearupCallFromTry(add);
      }
   }
   aet_print_tree_skip_debug(val);
   n_error("在clearup函数中a_object_cleanup_local_object_from_static_or_stack没找到类型。");
   return NULL_TREE;
}

static tree getCleanupCall(tree callExpr)
{
   tree funAdd=CALL_EXPR_FN (callExpr);
   if(TREE_CODE(funAdd)==ADDR_EXPR){
      tree func=TREE_OPERAND (funAdd, 0);
      if(TREE_CODE(func)==FUNCTION_DECL){
         char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
         if(strcmp(funcName,AET_CLEANUP_OBJECT_METHOD)==0){
            tree arg;
            call_expr_arg_iterator iter;
            FOR_EACH_CALL_EXPR_ARG (arg, iter, callExpr){
               return getClearupCallFromTry(arg);
            }
         }
      }
   }
   return NULL_TREE;
}

static nboolean findVar(tree src,tree dest)
{
   if(TREE_CODE(src)==TARGET_EXPR){
      tree op0=TREE_OPERAND (src, 1);
      if(TREE_CODE(op0)==BIND_EXPR){
         tree var=TREE_OPERAND (op0, 0);
         if(var==dest){
            return TRUE;
         }
      }
   }
   return FALSE;
}
/**
 * 变量是不是在other中
 */
static nboolean atReturnExpr(vec<tree, va_gc> *other,tree var)
{
   tree iter;
   int ix;
   for (ix = 0; other->iterate (ix, &iter); ++ix){
      if(findVar(iter,var))
         return TRUE;
   }
   return FALSE;
}

/**
 * 从TRY_FINALLY_EXPR开始遍历
 */
static tree walkCleaup_cb(tree *tp, int *walk_subtrees, void *data)
{
   struct nrv_data *dp = (struct nrv_data *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if(TREE_CODE (t) == CALL_EXPR){
      tree var=getCleanupCall(t);
      if(aet_utils_valid_tree(var)){
         nboolean e0= bitmap_bit_p (dp->nrv, DECL_UID (var));
         nboolean e1= atReturnExpr(dp->other,var);
         if(e0 || e1){
            char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
            n_debug("移走 clearup语句 bitmap:%d other:%d %s\n",e0,e1,varName);
            *tp =build_empty_stmt (EXPR_LOCATION (t));
         }else{
            aet_print_tree_skip_debug(t);
            n_error("在bitmpa和other中找不到返回的变量\n");
         }
      }
   }
   if (dp->visited->add (*tp))
      *walk_subtrees = 0;
   return NULL_TREE;
}

static tree finalize_remove_cleanup_r (tree *tp, int *walk_subtrees, void *data)
{
   struct nrv_data *dp = (struct nrv_data *)data;
   tree t = *tp;
  // printf("finalize_nrv_remove_cleanup 00 %s\n",get_tree_code_name(TREE_CODE(t)));
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR)
      walk_tree (&BIND_EXPR_BODY (t), finalize_remove_cleanup_r, data, NULL);
  else if (TREE_CODE (t) == RETURN_EXPR) {
	  //TREE_CODE (TREE_OPERAND (t, 0)) == INIT_EXPR
	  printf("finalize_nrv_remove_cleanup RETURN_EXPR 被替换 %s\n",get_tree_code_name(TREE_CODE(t)));
  }else if (TREE_CODE (t) == DECL_EXPR ){
	   /* Adjust the DECL_EXPR of NRVs to call the allocator and save the result
	      into a new variable.  */
//	  if(is_nrv_p (dp->nrv, DECL_EXPR_DECL (t))){
//        printf("zclei finalize_nrv_remove_cleanup 没有实现的代码 11。\n");
//        aet_print_tree_skip_debug(t);
//        printf("zclei finalize_nrv_remove_cleanup 没有实现的代码 11。tttt\n\n");
//        aet_print_tree_skip_debug(DECL_EXPR_DECL (t));
//
//	  }else{
//		printf("zclei finalize_nrv_remove_cleanup 不是====。\n");
//		aet_print_tree_skip_debug(t);
//		printf("zclei finalize_nrv_remove_cleanup 不是====xxxx 11。n\n");
//		aet_print_tree_skip_debug(DECL_EXPR_DECL (t));
//	  }

  }else if (is_nrv_p (dp->nrv, t)){
       /* And replace all uses of NRVs with the dereference of NEW_VAR.  */
	  n_debug("遍历至的树是其中的一个返回值，需要用参数来替换。");
  }else if(isReturnExpr(dp->other,t)){
      n_debug("遍历至的树是其中的一个返回值，需要用参数来替换0000");
  }else if(TREE_CODE(t)==TRY_FINALLY_EXPR){
      n_debug("遍历到TRY_FINALLY_EXPR0000");
      walk_tree (&t, walkCleaup_cb, data, NULL);
  }
  if (dp->visited->add (*tp))
    *walk_subtrees = 0;
  return NULL_TREE;
}


static void finalize_remove_cleanup(ObjectReturn *self,tree fndecl)
{
  struct nrv_data data;
  unsigned int i;
  tree iter;
  DataReturn *dataReturn=getDataReturn(self,fndecl);
  if(dataReturn==NULL){
     n_debug("finalize_remove_cleanup 00 dataReturn是空的，返回值声明。\n");
	  return;
  }
  bitmap nrv=dataReturn->named_ret_val;//变量声明 比如:Abc abc;
  vec<tree, va_gc> *other=dataReturn->other_ret_val;//表达式 比如TARGET_EXPR
  int gnat_ret=0;
  /* Prune the candidates that are referenced by other return values.  */
  data.nrv = nrv;
  data.result = NULL_TREE;
  data.gnat_ret = 0;
  data.visited = NULL;
  FOR_EACH_VEC_SAFE_ELT (other, i, iter)
    walk_tree_without_duplicates (&iter, prune_nrv_r, &data);
  /* Prune also the candidates that are referenced by nested functions.  */
  walk_nesting_tree (cgraph_node::get_create (fndecl), prune_nrv_r, &data);
  /* Extract a set of NRVs with non-overlapping live ranges.  */
  if (!prune_nrv_in_block (nrv, DECL_INITIAL (fndecl))){
     n_debug("finalize_remove_cleanup 11 !prune_nrv_in_block (nrv, DECL_INITIAL (fndecl))\n");
  }
  /* Adjust the relevant RETURN_EXPRs and replace the occurrences of NRVs.  */
  data.nrv = nrv;
  data.varReplace=NULL_TREE;
  data.result = NULL_TREE;//newParm;//indirect;//newParm;//DECL_RESULT (fndecl);
  data.other=other;
 // printf("finalize_remove_cleanup 22 返回值声明。\n");
  //aet_print_tree_skip_debug(data.result);
  data.gnat_ret = gnat_ret;
  data.visited = new hash_set<tree>;
  walk_tree (&DECL_SAVED_TREE (fndecl), finalize_remove_cleanup_r, &data, NULL);
  delete data.visited;
}

static tree finalize_test_iter (tree *tp, int *walk_subtrees, void *data)
{
   struct nrv_data *dp = (struct nrv_data *)data;
   tree t = *tp;
   if (TYPE_P (t)){
      printf("finalize_test_iter 00 TYPE_P (t) %s\n",get_tree_code_name(TREE_CODE(t)));
      aet_print_tree_skip_debug(t);
      *walk_subtrees = 0;
   }else if (TREE_CODE (t) == BIND_EXPR){
      printf("finalize_test_iter 11 BIND_EXPR %s\n",get_tree_code_name(TREE_CODE(t)));
      aet_print_tree_skip_debug(t);
      walk_tree (&BIND_EXPR_BODY (t), finalize_test_iter, data, NULL);
   }else if (TREE_CODE (t) == RETURN_EXPR) {
      //TREE_CODE (TREE_OPERAND (t, 0)) == INIT_EXPR
      printf("finalize_test_iter 22 RETURN_EXPR 被替换 %s\n",get_tree_code_name(TREE_CODE(t)));
      aet_print_tree_skip_debug(t);

   }else if (TREE_CODE (t) == DECL_EXPR ){
      printf("zclei 在返回值中的 DECL_EXPR 00。\n");
      aet_print_tree_skip_debug(t);
      printf("zclei 在返回值中的 DECL_EXPR 11。\n");
      aet_print_tree_skip_debug(DECL_EXPR_DECL (t));

   }else{
      printf("finalize_test_iter 44 is_nrv_p (dp->nrv, t) %s\n",get_tree_code_name(TREE_CODE(t)));
      aet_print_tree_skip_debug(t);
   }

   if (dp->visited->add (*tp))
      *walk_subtrees = 0;
   return NULL_TREE;
}


static void testStmt (ObjectReturn *self,tree fndecl)
{
  struct nrv_data data;
  walk_tree_fn func;
  printf("testStmt 00 fndecl:%p\n\n",fndecl);
  data.nrv = 0;
  data.varReplace=NULL_TREE;
  data.result = NULL_TREE;
  data.gnat_ret = 0;
  data.visited = new hash_set<tree>;
  func = finalize_test_iter;
  walk_tree (&DECL_SAVED_TREE (fndecl), func, &data, NULL);
  delete data.visited;
}

/* Return true if RET_VAL can be used as a Named Return Value for the
   anonymous return object RET_OBJ.  */
static bool return_value_ok_for_nrv_p (tree ret_obj, tree ret_val)
{
   if (TREE_CODE (ret_val) != VAR_DECL){
      n_debug("return_value_ok_for_nrv_p 00 不是变量:");
      return false;
   }

   if (TREE_THIS_VOLATILE (ret_val)){
      n_debug("return_value_ok_for_nrv_p 11 是一个变量，但被volatile修饰。");
      return false;
   }

   if (DECL_CONTEXT (ret_val) != current_function_decl){
      n_debug("return_value_ok_for_nrv_p 22 返回的变量不在当前函数内。");
      return false;
   }

   if (TREE_STATIC (ret_val)){
      n_debug("return_value_ok_for_nrv_p 33 返回的是一个静态变量。");
      return false;
   }
   /* For the constrained case, test for addressability.  */
   if (ret_obj && TREE_ADDRESSABLE (ret_val)){
      n_debug("return_value_ok_for_nrv_p 44 返回的是一个TREE_ADDRESSABLE (ret_val)。");
      //return false;
      return true;
   }
   /* For the constrained case, test for overalignment.  */
   if (ret_obj && DECL_ALIGN (ret_val) > DECL_ALIGN (ret_obj)){
      n_debug("return_value_ok_for_nrv_p 55 返回的变量以声明返回的对象未对齐。");
      return false;
   }

   /* For the unconstrained case, test for bogus initialization.  */
   if (!ret_obj && DECL_INITIAL (ret_val) && TREE_CODE (DECL_INITIAL (ret_val)) == NULL_EXPR){
      n_debug("return_value_ok_for_nrv_p 66");
      return false;
   }

   return true;
}

/**
 * 判断返回的是不是aet class
 */
static int aggregate_value_class(const_tree exp, const_tree fntype)
{
   const_tree type = (TYPE_P (exp)) ? exp : TREE_TYPE (exp);
   int i, regno, nregs;
   rtx reg;

   if (fntype)
      switch (TREE_CODE (fntype)){
         case CALL_EXPR:
         {
            tree fndecl = get_callee_fndecl (fntype);
            if (fndecl)
               fntype = TREE_TYPE (fndecl);
            else if (CALL_EXPR_FN (fntype))
               fntype = TREE_TYPE (TREE_TYPE (CALL_EXPR_FN (fntype)));
            else
               /* For internal functions, assume nothing needs to be
               returned in memory.  */
               return 0;
         }
            break;
         case FUNCTION_DECL:
            fntype = TREE_TYPE (fntype);
            break;
         case FUNCTION_TYPE:
         case METHOD_TYPE:
            break;
         case IDENTIFIER_NODE:
            fntype = NULL_TREE;
            break;
         default:
            /* We don't expect other tree types here.  */
            gcc_unreachable ();
      }

   if (VOID_TYPE_P (type))
      return 0;

   if(TREE_CODE(type)==RECORD_TYPE){
      char *sysName=class_util_get_class_name(type);
      if(sysName!=NULL){
         return 1;
      }
   }
   return 0;
}


static void  build_return_expr (ObjectReturn *self,tree ret_obj, tree ret_val)
{
   if (ret_val){
      /* The gimplifier explicitly enforces the following invariant:

      RETURN_EXPR
      |
      INIT_EXPR
      /        \
      /          \
      RET_OBJ        ...

      As a consequence, type consistency dictates that we use the type
      of the RET_OBJ as the operation type.  */
      tree operation_type = TREE_TYPE (ret_obj);
      tree old=ret_val;
      /* Convert the right operand to the operation type.  Note that this is
      the transformation applied in the INIT_EXPR case of build_binary_op,
      with the assumption that the type cannot involve a placeholder.  */
      if (operation_type != TREE_TYPE (ret_val)){
         n_debug("build_return_expr 00 operation_type != TREE_TYPE (ret_val)");
         ret_val = convert (operation_type, ret_val);
      }

      /* We always can use an INIT_EXPR for the return object.  */
      /* If the function returns an aggregate type, find out whether this is
      a candidate for Named Return Value.  If so, record it.  Otherwise,
      if this is an expression of some kind, record it elsewhere.  */
      if (AGGREGATE_TYPE_P (operation_type) && aggregate_value_class (operation_type, current_function_decl)){
         /* Strip useless conversions around the return value.  */
         if (gnat_useless_type_conversion (ret_val))
            ret_val = TREE_OPERAND (ret_val, 0);
         /* Now apply the test to the return value.  */
         if (return_value_ok_for_nrv_p (ret_obj, ret_val)){
            n_debug("加入的返回值是class对象变量 ，并且是一个可以优化的变量00 Id:%d ret_val:%p old:%p current_function_decl:%p",
                  DECL_UID (ret_val),ret_val,old,current_function_decl);
            DataReturn *data=getDataReturn(self,current_function_decl);
            if(data==NULL)
               data=addDataReturn(self,current_function_decl);
            bitmap_set_bit (data->named_ret_val, DECL_UID (ret_val));
         }else if (EXPR_P (ret_val)){
            /* Note that we need not care about CONSTRUCTORs here, as they are
            totally transparent given the read-compose-write semantics of
            assignments from CONSTRUCTORs.  */
            n_debug("加入的返回值是class对象变量 ，但不是一个变量11，可以是一个target表达式 ret_val:%p old:%p current_function_decl:%p\n",
                  ret_val,old,current_function_decl);
            DataReturn *data=getDataReturn(self,current_function_decl);
            if(data==NULL){
               data=addDataReturn(self,current_function_decl);
            }
            vec_safe_push (data->other_ret_val, ret_val);
         }
      }
   }
}


static tree getClearup(tree decl)
{
	  tree attr = lookup_attribute ("cleanup", DECL_ATTRIBUTES (decl));
	  if (attr){
		  printf("用户自设的cleanup %s\n",IDENTIFIER_POINTER(DECL_NAME(decl)));
		  return NULL_TREE;
	  }
	  tree cleanup_id = aet_utils_create_ident(AET_CLEANUP_OBJECT_METHOD);
	  tree cleanup_decl = lookup_name (cleanup_id);
	  tree cleanup;
	  vec<tree, va_gc> *v;

	  /* Build "cleanup(&decl)" for the destructor.  */
	  cleanup = build_unary_op (input_location, ADDR_EXPR, decl, false);
	  vec_alloc (v, 1);
	  v->quick_push (cleanup);
	  cleanup = c_build_function_call_vec (DECL_SOURCE_LOCATION (decl), vNULL, cleanup_decl, v, NULL);
	  vec_free (v);

	  /* Don't warn about decl unused; the cleanup uses it.  */
	  TREE_USED (decl) = 1;
	  TREE_USED (cleanup_decl) = 1;
	  DECL_READ_P (decl) = 1;

	  enum tree_code code;
	  tree stmt;

	  code = TRY_FINALLY_EXPR;
	  stmt = build_stmt (DECL_SOURCE_LOCATION (decl), code, NULL, cleanup);
	  return stmt;
}


static nboolean isClass(tree value)
{
   char *sysName=class_util_get_class_name(TREE_TYPE(value));
   return (sysName!=NULL && TREE_CODE(TREE_TYPE(value))==RECORD_TYPE);
}

static nboolean exitsVar(NPtrArray *dataArray,tree var)
{
   int i;
   for(i=0;i<dataArray->len;i++){
      tree item=n_ptr_array_index(dataArray,i);
      if(var==item){
         return TRUE;
      }
   }
   return FALSE;
}

/* This copy is not redundant; tsi_link_after will smash this
   STATEMENT_LIST into the end of the one we're building, and we
   don't want to do that with the original.
 */
static tree addTryFinallyExpr (tree *tp,NPtrArray *dataArray)
{
   tree_stmt_iterator oi, ni;
   tree new_tree,finallyNewStmtList;
   new_tree = alloc_stmt_list ();
   ni = tsi_start (new_tree);
   oi = tsi_start (*tp);
   TREE_TYPE (new_tree) = TREE_TYPE (*tp);
   finallyNewStmtList=new_tree;
   int count=0;
   for (; !tsi_end_p (oi); tsi_next (&oi)){
      tree stmt = tsi_stmt (oi);
      n_debug("objectreturn 打印语句--- %d\n",count++);
      if (TREE_CODE (stmt) == STATEMENT_LIST)
         addTryFinallyExpr (&stmt,dataArray);
      n_debug("objectreturn 再加入新的语句 ni：%p %s %p\n",&ni,get_tree_code_name(TREE_CODE(stmt)),stmt);
      tsi_link_after (&ni, stmt, TSI_CONTINUE_LINKING);
      if(TREE_CODE(stmt)==DECL_EXPR){
         tree var=DECL_EXPR_DECL (stmt);
         if(exitsVar(dataArray,var)){
            tree tryFinally=getClearup(var);
            append_to_statement_list_force (tryFinally, &finallyNewStmtList);
            tree list = alloc_stmt_list ();
            TREE_OPERAND (tryFinally, 0) = list;
            ni = tsi_start (list);
            finallyNewStmtList=list;
            n_debug("objectreturn 在这里加入表达式try_finaly_expr %d ni:%p %s",count,&ni,IDENTIFIER_POINTER(DECL_NAME(var)));
            n_ptr_array_remove(dataArray,var);
         }
      }
   }
   return new_tree;
}


static tree copyStmtList(tree *stmtList,NPtrArray *dataArray)
{
	  tree result=NULL_TREE;
	  tree_stmt_iterator oi;
	  oi = tsi_start (*stmtList);
	  nboolean find=FALSE;
	  for (; !tsi_end_p (oi); tsi_next (&oi)){
	       tree stmt = tsi_stmt (oi);
	       if (TREE_CODE (stmt) == DECL_EXPR ){
	       	   tree var=DECL_EXPR_DECL (stmt);
	       	   find=exitsVar(dataArray,var);
	       	   if(find)
	       		   break;
	       }
      }
	  if(find){
	     n_debug("aet_copy_statement_list 00 遇到一个变量声明。查到需要加入finally的变量了吗:%d\n",find);
		 result=addTryFinallyExpr(stmtList,dataArray);
	  }
	  return result;
}

static void removeReadyVar(NPtrArray *dataArray,tree var)
{
	int i;
	for(i=0;i<dataArray->len;i++){
		 tree item=n_ptr_array_index(dataArray,i);
		 if(var==item){
			 n_ptr_array_remove(dataArray,item);
			 printf("找到一个变量声明，DECL_EXPR 但已有try_finally_expr.所以不再加clearup\n");
			 aet_print_tree_skip_debug(item);
			 return;
		 }
	}
}

/**
 * 从try_finally_expr的 REE_OPERAND (funAdd, 1)中取出设置clearup的变量
 */
static tree getFinallyExprVar(tree call)
{
	if(TREE_CODE (call) == CALL_EXPR){
	   tree funAdd=CALL_EXPR_FN (call);
	   if(TREE_CODE(funAdd)==ADDR_EXPR){
		  tree func=TREE_OPERAND (funAdd, 0);
		  if(TREE_CODE(func)==FUNCTION_DECL){
			char *funcName=IDENTIFIER_POINTER(DECL_NAME(func));
			tree arg;
			call_expr_arg_iterator iter;
			FOR_EACH_CALL_EXPR_ARG (arg, iter, call){
				return getClearupCallFromTry(arg);
			}
		 }
	   }
	}
	return NULL_TREE;
}

/**
 * 给以下两种对象变量加上cleanup
 * Abc fb=getAbc();
 * Abc fb;
 * fb=getAbc();
 */
static tree readyCleanup_cb (tree *tp, int *walk_subtrees, void *data)
{
   struct add_cleanup_data *dp = (struct add_cleanup_data *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR)
      walk_tree (&BIND_EXPR_BODY (t), readyCleanup_cb, data, NULL);
   else if (TREE_CODE (t) == TRY_FINALLY_EXPR){
	   tree p0 = TREE_OPERAND (t, 1);
	   tree varByCleanup=getFinallyExprVar(p0);
       removeReadyVar(dp->dataArray,varByCleanup);
	   //printf("readyCleanup_cb 00 TRY_FINALLY_EXPR %p\n",varByCleanup);
   }else if(TREE_CODE(t)==MODIFY_EXPR){
	  tree lhs=TREE_OPERAND (t, 0);
	  tree rhs=TREE_OPERAND (t, 1);
	  nboolean is0=isClass(lhs);
	  nboolean is1=isClass(rhs);
	  if(is0 && is1){
		  if (TREE_CODE (rhs)==CALL_EXPR){
			   n_debug("变量声明是一个类对象。并且被函数调用赋值。");
			   aet_print_tree(t);
			   n_ptr_array_add(dp->dataArray,lhs);
		  }
	  }
   }else if (TREE_CODE (t) == DECL_EXPR ){
	   tree var=DECL_EXPR_DECL (t);
	   nboolean is0=isClass(var);
	   if(is0){
		   tree call=DECL_INITIAL (var);
		   if (call && TREE_CODE (call)==CALL_EXPR){
		       n_debug("变量声明是一个类对象。并且有初始化。");
			   if(isClass(call)){
			       n_debug("给变量加入clearup变量声明是一个类对象。并且有初始化。");
				   n_ptr_array_add(dp->dataArray,var);
			   }
		   }else{
			   char *varName=IDENTIFIER_POINTER(DECL_NAME(var));
			   if(!strcmp(varName,"zcys") || !strcmp(varName,"zcys1")){
			       n_debug("给变量加入clearup变量声明是一个类对象。并且有初始化xxxxxx。%s\n",varName);
			       n_ptr_array_add(dp->dataArray,var);
			   }

		   }
	   }
   }
   if (dp->visited->add (*tp))
    *walk_subtrees = 0;
   return NULL_TREE;
}

static tree addCleanupFunc_cb (tree *tp, int *walk_subtrees, void *data)
{
   struct add_cleanup_data *dp = (struct add_cleanup_data *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR)
      walk_tree (&BIND_EXPR_BODY (t), addCleanupFunc_cb, data, NULL);
   else if(TREE_CODE(t)==STATEMENT_LIST){
	  tree result= copyStmtList(&t,dp->dataArray);
	  if(result!=NULL_TREE){
		  *tp=result;
		  *walk_subtrees = 0;
	  }
   }
   if (dp->visited->add (*tp))
    *walk_subtrees = 0;
   return NULL_TREE;
}

/**
 * 如果函数返回值是aet class加入cleanup
 */
static void finalize_add_cleanup(ObjectReturn *self,tree fndecl)
{
   struct add_cleanup_data data;
   data.dataArray = n_ptr_array_new();
   data.visited = new hash_set<tree>;
   walk_tree (&DECL_SAVED_TREE (fndecl), readyCleanup_cb, &data, NULL);
   delete data.visited;
   struct add_cleanup_data data1;
   data1.dataArray = data.dataArray;
   data1.visited = new hash_set<tree>;
   walk_tree (&DECL_SAVED_TREE (fndecl), addCleanupFunc_cb, &data1, NULL);
   delete data1.visited;
}


void  object_return_finish_function(ObjectReturn *self,tree fndecl)
{
   if(!aet_utils_valid_tree(fndecl)){
      n_warning("object_return_finish_function fndecl无效");
      return;
   }
   n_debug("解析函数的返回值 --- %s\n",IDENTIFIER_POINTER(DECL_NAME(fndecl)));
   finalize_add_cleanup(self,fndecl);
   DataReturn *dataReturn=getDataReturn(self,fndecl);
   if(dataReturn!=NULL){
      finalize_remove_cleanup(self,fndecl);
      removeDataReturn(self,fndecl);
      return;
   }
}

void object_return_add_return(ObjectReturn *self,tree retExpr)
{
	 tree  decl = DECL_RESULT (current_function_decl);
	// printf("object_return_add_return 00 %s %p %d\n",IDENTIFIER_POINTER(DECL_NAME(current_function_decl)),current_function_decl,TREE_ADDRESSABLE (retExpr));
	 //TREE_ADDRESSABLE (retExpr)=0;
	 build_return_expr(self,decl,retExpr);
}

/**
 * 只有对象指针才能根据函数返回值声明转化
 * 并且只能转为接口
 */
tree   object_return_convert(ObjectReturn *self,location_t loc,tree retExpr)
{
   tree  decl = DECL_RESULT (current_function_decl);
   tree type=TREE_TYPE(decl);
   if(TREE_CODE(type)==POINTER_TYPE){
      type=TREE_TYPE(type);
      char *sysName=class_util_get_class_name_by_record(type);
      if(sysName!=NULL){
         ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
         if(info!=NULL && class_info_is_interface(info)){
            return class_cast_cast_for_return(self->classCast,loc,TREE_TYPE(decl),retExpr);
         }
      }
   }
   return retExpr;
}

//////////////泛型块返回值
static bool expr_is_pointer_p (tree expr)
{
   if (!expr || expr == error_mark_node)
      return false;

   tree type = TREE_TYPE (expr);
   if (!type || type == error_mark_node)
      return false;

   /* 去掉 typedef 糖衣 */
   type = TYPE_MAIN_VARIANT (type);
   n_debug("expr_is_pointer_p 00\n");
   aet_print_tree(type);
   return POINTER_TYPE_P (type);
}

/**
 * 是否存在泛型块tls
 */
static nboolean existsTls(ObjectReturn *self,tree function)
{
   int i;
   for(i=0;i<self->tlsDataArray->len;i++){
      TlsData *item = n_ptr_array_index(self->tlsDataArray,i);
      if(item->fndecl==function)
         return TRUE;
   }
   return FALSE;
}

static void addTlsData(ObjectReturn *self,tree fndecl,tree tlsVar)
{
   TlsData *data=n_slice_new0(TlsData);
   data->fndecl = fndecl;
   data->tlsVar = tlsVar;
   n_ptr_array_add(self->tlsDataArray,data);
}

static tree getTlsVar(ObjectReturn *self,tree fndecl)
{
   int i;
   for(i=0;i<self->tlsDataArray->len;i++){
      TlsData *item = n_ptr_array_index(self->tlsDataArray,i);
      if(item->fndecl==fndecl)
         return item->tlsVar;
   }
   return NULL_TREE;
}

/**
 * 创建 static tls 变量
 */
static tree make_tls_var (tree fndecl, tree type, const char *name)
{
  tree id  = get_identifier (name);
  tree var = build_decl (DECL_SOURCE_LOCATION (fndecl),
                         VAR_DECL, id, type);

  DECL_CONTEXT (var) = fndecl;
  DECL_ARTIFICIAL (var) = 1;
  TREE_USED (var) = 1;
  /* static __thread */
  TREE_STATIC (var) = 1;
  TREE_PUBLIC (var) = 0;
  DECL_EXTERNAL (var) = 0;
  set_decl_tls_model (var, TLS_MODEL_GLOBAL_DYNAMIC);
  tree init = build_zero_cst (type);
  /* 登记到当前作用域（在函数体内时） */
  var = pushdecl (var);
  /* 完成声明：布局、初始化、纳入编译 */
  finish_decl (var, DECL_SOURCE_LOCATION (fndecl),
               init, NULL_TREE, NULL_TREE);

  return var;
}

/**
 * 函数体已有语句（当前正在处理 return）。
 * 不创建 BIND_EXPR，只把 static __thread 声明插成第一条语句。
 * 返回创建的 VAR_DECL，供后面写 TLS / 取地址使用。
 */
static tree insert_thread_local_at_fn_start(tree fndecl, char *typeName, const char *name)
{
  tree body = DECL_SAVED_TREE (fndecl);
  gcc_assert (body != NULL_TREE);
  /* 解析类型 */
   tree type = lookup_name (get_identifier (typeName));
   if (type == NULL_TREE)
     type = integer_type_node; /* 或报错 */
   if (TREE_CODE (type) == TYPE_DECL)
     type = TREE_TYPE (type);
   type = TYPE_MAIN_VARIANT (type);
  tree var = make_tls_var (fndecl, type, name);
  tree decl_expr = build1 (DECL_EXPR, void_type_node, var);

  if (TREE_CODE (body) == STATEMENT_LIST){
      /* 已有语句列表：插到最前面 */
      tree_stmt_iterator i = tsi_start (body);
      tsi_link_before (&i, decl_expr, TSI_SAME_STMT);
    }else if (TREE_CODE (body) == BIND_EXPR){
      /* 少数情况已经是 BIND：挂 vars，并插到 BIND 的 body 最前 */
      TREE_CHAIN (var) = BIND_EXPR_VARS (body);
      BIND_EXPR_VARS (body) = var;
      if (BIND_EXPR_BLOCK (body) != NULL_TREE)
        BLOCK_VARS (BIND_EXPR_BLOCK (body)) = BIND_EXPR_VARS (body);

      tree *stmt_p = &BIND_EXPR_BODY (body);
      gcc_assert (*stmt_p != NULL_TREE);
      if (TREE_CODE (*stmt_p) == STATEMENT_LIST){
          tree_stmt_iterator i = tsi_start (*stmt_p);
          tsi_link_before (&i, decl_expr, TSI_SAME_STMT);
        }else{
          /* body 是单条语句：收成 list，声明在前 */
          tree sl = alloc_stmt_list ();
          append_to_statement_list (decl_expr, &sl);
          append_to_statement_list (*stmt_p, &sl);
          *stmt_p = sl;
        }
    }else{
      /* 单条语句（例如目前只有一个 RETURN_EXPR）：收成 list，声明在前 */
      tree sl = alloc_stmt_list ();
      append_to_statement_list (decl_expr, &sl);
      append_to_statement_list (body, &sl);
      DECL_SAVED_TREE (fndecl) = sl;
    }

  return var;
}

/**
 * 是否是正在编译泛型块函数
 */
static nboolean atCompileGenericBlock(ObjectReturn *self)
{
   return (current_function_decl
      && aet_parser_is_generic_state(self->parser)
      && generic_util_is_block_func_name(IDENTIFIER_POINTER(DECL_NAME(current_function_decl))));
}

/**
 * 泛型块函数内
 */
tree object_return_convert_block(ObjectReturn *self,tree expr)
{
   return expr;
   //是否进入编译泛型块函数
   if(!atCompileGenericBlock(self))
      return expr;
   printf("object_return_convert_block 00 \n");

   tree fndecl=current_function_decl;

   //返回表达式的类型是不是指针
   if(expr_is_pointer_p(expr))
      return expr;
   printf("object_return_convert_block 22 \n");

   //泛型块函数的返回值是不是指针
   tree type TREE_TYPE(TREE_TYPE(fndecl));
   if(!POINTER_TYPE_P (type))
      return expr;
   printf("object_return_convert_block 33 \n");

   //插入 __thread 根据泛型的定义，如果有多个泛型如何选择，判断返回值 如果是 E ，就用 E ， 如果用户返回的类型是 void *应报错。
   int pointerCount = 0;
   char *re = generic_util_get_type_str(type ,&pointerCount);
   char *genericDeclStr=  generic_util_get_generic_str(type);
   if(genericDeclStr==NULL){
      n_error("泛型块函数返回的不是泛型声明类型\n");
   }
   pointerCount= 0 ;
   char *trueTypeName = generic_parser_get_true_type(generic_parser_get(),genericDeclStr,&pointerCount);
   if(!trueTypeName)
      n_error("泛型声明没有定义 %s\n",genericDeclStr);
   //泛型真实类型是指针，不需要处理。
   if(pointerCount >0)
      return expr;
   tree tlsvar = NULL_TREE;
   if(!existsTls(self,fndecl)){
      char *fname = IDENTIFIER_POINTER(DECL_NAME(fndecl));
      unsigned int hash=aet_utils_create_hash(fname,strlen(fname));
      struct timeval tv;
      gettimeofday (&tv, NULL);
      long local_tick = (unsigned) tv.tv_sec * 1000 + tv.tv_usec / 1000;
      long randNumber=local_tick+rand ()+hash;
      char name[255];
      sprintf(name,"aet_tls_%ld",randNumber);
      tlsvar  = insert_thread_local_at_fn_start(fndecl,trueTypeName,name);
      addTlsData(self,fndecl,tlsvar);
   }else{
      tlsvar = getTlsVar(self,fndecl);
      gcc_assert(tlsvar);
   }

   /* 在 c_finish_return 之前 */
   tree store = build2 (MODIFY_EXPR, void_type_node, tlsvar, expr);
   printf("object_return_convert_block 44 %s %d %s trueTypeName:%s\n",re,pointerCount,genericDeclStr,trueTypeName);

   aet_print_tree(expr);

   add_stmt (store);   /* 追加到当前 stmt list（if 体、复合语句体等） */

   /* 再正常结束 return，返回 &tls_var */
   tree addr = build_fold_addr_expr (tlsvar);
   tree trueType = lookup_name (get_identifier (trueTypeName));
   aet_print_tree(trueType);
   if (trueType == NULL_TREE)
      trueType = integer_type_node; /* 或报错 */
   if (TREE_CODE (trueType) == TYPE_DECL)
      trueType = TREE_TYPE (trueType);
   trueType=build_pointer_type(trueType);
   addr = fold_convert (trueType, addr);  /* 按E 类型转 */
   printf("object_return_convert_block 55 %s %d %s\n",re,pointerCount,genericDeclStr);
   aet_print_tree(addr);
   aet_print_tree(trueType);
   return addr;
}



ObjectReturn *object_return_get()
{
	static ObjectReturn *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(ObjectReturn));
		 objectReturnlInit(singleton);
		 singleton->parser = aet_parser_get();
	}
	return singleton;
}



