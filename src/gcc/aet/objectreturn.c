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
#include "../libcpp/include/cpplib.h"
#include "../libcpp/internal.h"

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
   self->classCast=class_cast_new();
   //泛型块函数中播入的tls
   self->tlsDataArray=n_ptr_array_new();
   self->returnValArray = n_ptr_array_new();
   //记录有初始的类变量
   self->varArray = n_ptr_array_new();
}

struct add_cleanup_data
{
  NPtrArray *dataArray;
  hash_set<tree> *visited;
};

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
   n_error("查找cleanup变量失败。");
   return NULL_TREE;
}

static tree createClearupStmt(tree decl)
{
   tree attr = lookup_attribute ("cleanup", DECL_ATTRIBUTES (decl));
   if (attr){
      printf("用户自设的cleanup %s\n",IDENTIFIER_POINTER(DECL_NAME(decl)));
      return NULL_TREE;
   }
   tree varPointer = build_unary_op (DECL_SOURCE_LOCATION (decl), ADDR_EXPR, decl, false);
   tree cleanupExpr = class_util_build_unref_call(DECL_SOURCE_LOCATION (decl),varPointer);
   /* Don't warn about decl unused; the cleanup uses it.  */
   TREE_USED (decl) = 1;
   DECL_READ_P (decl) = 1;
   enum tree_code code;
   tree stmt;
   code = TRY_FINALLY_EXPR;
   stmt = build_stmt (DECL_SOURCE_LOCATION (decl), code, NULL, cleanupExpr);
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
            tree tryFinally=createClearupStmt(var);
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
 * 查找带有cleanup的变量
 */
static tree getFinallyExprVar(tree call)
{
   if(TREE_CODE (call) == CALL_EXPR){
      tree compref=CALL_EXPR_FN (call);
      if(TREE_CODE(compref)==COMPONENT_REF){
         tree field=TREE_OPERAND (compref, 1);
         if(TREE_CODE(field)==FIELD_DECL){
            char *fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
            ClassFunc *unref = func_mgr_get_unref(func_mgr_get());
            if(!strcmp(unref->mangleFunName,fieldName)){
               call_expr_arg_iterator iter;
               tree arg =first_call_expr_arg(call,&iter);
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
  // else if (TREE_CODE (t) == BIND_EXPR)
      //walk_tree (&BIND_EXPR_BODY (t), readyCleanup_cb, data, NULL);
   else if (TREE_CODE (t) == TRY_FINALLY_EXPR){
	   tree p0 = TREE_OPERAND (t, 1);
	   tree varByCleanup=getFinallyExprVar(p0);
      removeReadyVar(dp->dataArray,varByCleanup);
   }else if(TREE_CODE(t)==MODIFY_EXPR){
	  tree lhs=TREE_OPERAND (t, 0);
	  tree rhs=TREE_OPERAND (t, 1);
	  nboolean is0=isClass(lhs);
	  nboolean is1=isClass(rhs);
	  if(is0 && is1){
		  if (TREE_CODE (rhs)==CALL_EXPR){
			   n_debug("objectreturn.c readyCleanup_cb 11 变量声明是一个类对象。并且被函数调用赋值。");
			   n_ptr_array_add(dp->dataArray,lhs);
		  }
	  }
   }else if (TREE_CODE (t) == DECL_EXPR ){
	   tree var=DECL_EXPR_DECL (t);
	   nboolean is0=isClass(var);
	   if(is0){
		   tree call=DECL_INITIAL (var);
		   if (call && TREE_CODE (call)==CALL_EXPR){
		       n_debug("objectreturn.c readyCleanup_cb 22 变量声明是一个类对象。并且有初始化。");
			   if(isClass(call)){
			       n_debug("objectreturn.c readyCleanup_cb 33 给变量加入clearup变量声明是一个类对象。并且有初始化。");
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
 * TFirst var;
 * (&var)->unref(&var)
 */
static tree buildUnrefCall(tree var)
{
   tree varPointer = build_unary_op (DECL_SOURCE_LOCATION (var), ADDR_EXPR, var, false);
   return class_util_build_unref_call(DECL_SOURCE_LOCATION (var),varPointer);
}

/**
 * 把：
 * ab = get_object()
 * 改写为：
 * (ab = get_object(), ab.unref(), ab)
 * 保留原 MODIFY_EXPR，避免重复执行右值表达式。
 */
static tree build_owned_assignment_expr (tree modifyExpr)
{
    location_t loc;
    tree lhs;
    tree unrefExpr;
    tree firstExpr;
    tree secondExpr;
    tree result;

    if (!modifyExpr || TREE_CODE (modifyExpr) != MODIFY_EXPR)
        return modifyExpr;

    lhs = TREE_OPERAND (modifyExpr, 0);

    if (!lhs || !VAR_P (lhs))
        return modifyExpr;

    loc = EXPR_LOCATION (modifyExpr);
    /*
     * 构造：
     * (&var)->unref(&var)
     */
    unrefExpr = buildUnrefCall (lhs);
    if (!unrefExpr)
        return modifyExpr;
    /*
     * 第一个 COMPOUND_EXPR：
     *((var = get_object(), (&var)->unref(&var))
     */
    firstExpr = build2_loc (loc, COMPOUND_EXPR,TREE_TYPE (unrefExpr),modifyExpr, unrefExpr);

    /*
     * 第二个 COMPOUND_EXPR：
     * ((var = get_object(), (&var)->unref(&var)), var)
     */
    result = build2_loc (loc, COMPOUND_EXPR,TREE_TYPE (lhs),firstExpr,lhs);
    return result;
}

/**
 * 1.TFirst var=get_object();
 * 2.TFirst var;
 *   var =  get_object();
 * 3.TFirst var;
 *   var = cond?obj:get_object();
 * 4 TFirst var = cond?obj:get_object();
 * 5.TFirst var;
 *   if(((var=get_object)).xxx=3)
 */

static tree addUnref_cb00 (tree *tp, int *walk_subtrees, void *data)
{
   struct add_cleanup_data *dp = (struct add_cleanup_data *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if(TREE_CODE(t)==MODIFY_EXPR){
     tree lhs=TREE_OPERAND (t, 0);
     tree rhs=TREE_OPERAND (t, 1);
     nboolean is0=isClass(lhs);
     nboolean is1=isClass(rhs);
     printf("addUnref_cb 00 %d %d\n",is0,is1);
     aet_print_tree(lhs);
     aet_print_tree(rhs);

     if(is0 && is1){
        if (TREE_CODE (rhs)==CALL_EXPR){
            n_debug("objectreturn.c addUnref_cb 11 变量声明是一个类对象。并且被函数调用赋值。");
            *tp = build_owned_assignment_expr(t);
        }
     }
   }else if (TREE_CODE (t) == DECL_EXPR ){
      tree var=DECL_EXPR_DECL (t);
      nboolean is0=isClass(var);
      if(is0){
         tree call=DECL_INITIAL (var);
         if (call && TREE_CODE (call)==CALL_EXPR){
             n_debug("objectreturn.c addUnref_cb 22 有初始化。var:%p call:%p",var,call);
            if(isClass(call)){
                n_debug("objectreturn.c addUnref_cb 33 给变量加入clearup变量声明是一个类对象。并且有初始化。");
            }
         }
      }
   }
   if (dp->visited->add (*tp))
    *walk_subtrees = 0;
   return NULL_TREE;
}

/**
 * 处理变量初始化：
 * TFirst var = getdata();
 * 转换为：
 * TFirst var = getdata();
 * var.unref();
 * 注意：DECL_EXPR 不能直接传给 build_owned_assignment_expr。
 */
static tree build_owned_decl_expr (tree declExpr)
{
    location_t loc;
    tree var;
    tree init;
    tree unrefExpr;
    tree stmtList = NULL_TREE;
    if (!declExpr || TREE_CODE (declExpr) != DECL_EXPR)
        return declExpr;

    var = DECL_EXPR_DECL (declExpr);
    if (!var || !VAR_P (var))
        return declExpr;

    /*
     * 获取变量初始化表达式。
     */
    init = DECL_INITIAL (var);
    /*
     * 目前先处理直接函数调用。
     */
    if (TREE_CODE (init) != CALL_EXPR)
        return declExpr;

    loc = EXPR_LOCATION (declExpr);

    n_debug ("objectreturn.c build_owned_decl_expr "
             "变量初始化需要调整引用计数 var:%p init:%p",
             var, init);
    /*
     * 构造：
     * (&var)->unref(&var)
     */
    unrefExpr = buildUnrefCall (var);
    if (!unrefExpr)
        return declExpr;
    /*
     * 保留原始声明。
     */
    append_to_statement_list (declExpr, &stmtList);
    /*
     * 声明初始化完成之后执行 unref。
     */
    append_to_statement_list (unrefExpr, &stmtList);
    return stmtList;
}

static tree addUnref_cb (tree *tp, int *walk_subtrees, void *data)
{
   struct add_cleanup_data *dp =  (struct add_cleanup_data *) data;
   tree t = *tp;
   if (!t)
      return NULL_TREE;

   if (TYPE_P (t)){
      *walk_subtrees = 0;
      return NULL_TREE;
   }

   /*
   * 先判断当前节点是否已经访问过。
   */
   if (dp->visited->add (t)){
      *walk_subtrees = 0;
      return NULL_TREE;
   }
   /*
    * TFirst var;
    * var = getdata();
    */
   if (TREE_CODE (t) == MODIFY_EXPR){
      tree lhs = TREE_OPERAND (t, 0);
      tree rhs = TREE_OPERAND (t, 1);

      nboolean is0 = isClass (lhs);
      nboolean is1 = isClass (rhs);

      if (is0 && is1 && TREE_CODE (rhs) == CALL_EXPR){
         n_debug ("objectreturn.c addUnref_cb 00 modify_expr。");
         tree newExpr = build_owned_assignment_expr (t);
         if (newExpr && newExpr != t){
            *tp = newExpr;
            /*
            * 关键：
            * 不再遍历新生成的 COMPOUND_EXPR。
            *
            * 它内部包含原来的 MODIFY_EXPR，
            * 如果继续遍历，会再次触发替换。
            */
            *walk_subtrees = 0;
            return NULL_TREE;
         }
      }
   }else if (TREE_CODE (t) == DECL_EXPR){
      /*
      * TFirst var = getdata();
      */
      if (TREE_CODE (t) == DECL_EXPR){
         tree var = DECL_EXPR_DECL (t);
         if (var && VAR_P (var) && isClass (var)){
            tree init = DECL_INITIAL (var);
            if (init && TREE_CODE (init) == CALL_EXPR && isClass (init)){
               n_debug("objectreturn.c addUnref_cb 11 变量有初始值。");
               tree newExpr = build_owned_decl_expr (t);
               if (newExpr != t){
                  *tp = newExpr;
                  *walk_subtrees = 0;
                  return NULL_TREE;
               }
            }
         }
      }
   }
   return NULL_TREE;
}


//////////////泛型块返回值--------------------------------------
/**
 * E ret = genericblock$(){
 *    E a= ...
 *    return a;
 * }
 * 如果E =int 返回的a = int 但接收的返回值是 E = aet_void_E,类型不匹配，在genericblock内部生成
 * tls 变量 static __thread 类型 aet_tls_xxx这是一个变量不是数组也不是指针，如果类型是结构体，变量类型
 * 就是结构体，static __thread int  aet_tls_xxx 或 static __thread struct abc aet_tls_xxx
 */

static bool expr_is_pointer_p (tree expr)
{
   if (!expr || expr == error_mark_node)
      return false;

   tree type = TREE_TYPE (expr);
   if (!type || type == error_mark_node)
      return false;

   type = TYPE_MAIN_VARIANT (type);
   n_debug("expr_is_pointer_p 00\n");
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
 * 泛型块函数内 返回tls
 */
static tree convertBlock(ObjectReturn *self,tree expr)
{
   //是否进入编译泛型块函数
   if(!atCompileGenericBlock(self))
      return expr;
   tree fndecl=current_function_decl;
   //返回表达式的类型是不是指针
   if(expr_is_pointer_p(expr))
      return expr;
   //泛型块函数的返回值是不是指针
   tree type TREE_TYPE(TREE_TYPE(fndecl));
   if(!POINTER_TYPE_P (type))
      return expr;
   //插入 __thread 根据泛型的定义，如果有多个泛型如何选择，判断返回值 如果是 E ，就用 E ， 如果用户返回的类型是 void *应报错。
   int pointerCount = 0;
   const char *re = aet_utils_get_const_type_string(type,&pointerCount);
   const char *genericDeclStr=  generic_util_get_generic_decl_string(re);
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
   n_debug("object_return_convert_block 44 %s %d %s trueTypeName:%s\n",re,pointerCount,genericDeclStr,trueTypeName);
   add_stmt (store);   /* 追加到当前 stmt list（if 体、复合语句体等） */
   /* 再正常结束 return，返回 &tls_var */
   tree addr = build_fold_addr_expr (tlsvar);
   tree trueType = lookup_name (get_identifier (trueTypeName));
   if (trueType == NULL_TREE)
      trueType = integer_type_node; /* 或报错 */
   if (TREE_CODE (trueType) == TYPE_DECL)
      trueType = TREE_TYPE (trueType);
   trueType=build_pointer_type(trueType);
   addr = fold_convert (trueType, addr);  /* 按E 类型转 */
   return addr;
}


////////////////////-------------类变量返回值所有权转化-----------------------

/**
 * 只有对象指针才能根据函数返回值声明转化
 * 并且只能转为接口
 * Iface *getData(){
 *    TFirst *a;//这里TFirst实现了Iface接口
 *    return a;//object_return_convert把a转化为iface接口
 * }
 */
static tree   convertToIface(ObjectReturn *self,location_t loc,tree retExpr)
{
   tree  decl = DECL_RESULT (current_function_decl);
   tree type=TREE_TYPE(decl);
   if(TREE_CODE(type)==POINTER_TYPE){
      ClassInfo *info = class_mgr_get_class_info_by_record(class_mgr_get(), TREE_TYPE(type));
      if(info!=NULL && class_info_is_interface(info)){
         return class_cast_cast_for_return(self->classCast,loc,type,retExpr);
      }
   }
   return retExpr;
}

/**
 * retVal 返回值
 * 收集与函数返回值类型相同的aet类变量
 */
static void collectReturnValue(ObjectReturn *self,tree retVal)
{
   tree  decl = DECL_RESULT (current_function_decl);
   tree type=TREE_TYPE(decl);
   //函数声明的返回类型
   ClassInfo *declInfo = class_mgr_get_class_info_by_record(class_mgr_get(), type);
   if(declInfo){
      printf("transferOwnership 00 %s\n",declInfo->className.sysName);
      ClassInfo *returnInfo = class_mgr_get_class_info_by_record(class_mgr_get(), TREE_TYPE(retVal));
      if(returnInfo && returnInfo ==declInfo){
         printf("transferOwnership 11 %s\n",returnInfo->className.sysName);
         //查找retExpr是不是自带cleanup
         aet_print_tree(retVal);
         n_ptr_array_add(self->returnValArray,retVal);
      }
   }
}

/**
 * 带cleanup的变量缓存到 arrays
 */
static void addVarWithCleanup(NPtrArray *arrays,tree var)
{
   int i;
   nboolean find = FALSE;
   for(i=0;i<arrays->len;i++){
      tree item=n_ptr_array_index(arrays,i);
      if(var==item){
         find = TRUE;
         break;
      }
   }
   if(!find)
      n_ptr_array_add(arrays,var);
}

/**
*  找TRY_FINALLY_EXPR中清除语句
*/
static tree collectVarWithCleanup_cb (tree *tp, int *walk_subtrees, void *data)
{
   NPtrArray *arrays= (NPtrArray *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == TRY_FINALLY_EXPR){
      tree p1 = TREE_OPERAND (t, 1);
      tree varWithCleanup=getFinallyExprVar(p1);
      if(varWithCleanup)
         addVarWithCleanup(arrays,varWithCleanup);
   }
   return NULL_TREE;
}

/**
 * 是不是带cleanup的变量
 */
static nboolean isWithCleanupVar(tree var,NPtrArray *arrays)
{
   int i;
   for(i=0;i<arrays->len;i++){
      tree cleanupVar = n_ptr_array_index(arrays,i);
      if(cleanupVar == var)
         return TRUE;
   }
   return FALSE;
}

static tree build_ref_return_expr (tree var)
{
   location_t loc = DECL_SOURCE_LOCATION(var);
   ClassFunc *refFunc = func_mgr_get_ref(func_mgr_get());
   tree ref_field = refFunc->fieldDecl;
   tree tfirst_type =TREE_TYPE(var);

   /* var.ref */
   tree ref_component = build3_loc (loc,COMPONENT_REF,TREE_TYPE (ref_field),var,ref_field,NULL_TREE);

   /* var.ref(var) */
   vec<tree, va_gc> *argvec = NULL;
   //转指针
   tree varpointer=  build_unary_op (loc, ADDR_EXPR, var, FALSE);
   //转成(AObject*)value
   tree aobjectParm = build1 (NOP_EXPR, build_pointer_type(TREE_TYPE (ref_field)), varpointer);

   vec_safe_push (argvec, aobjectParm);

   tree ref_return_type = TREE_TYPE (TREE_TYPE (ref_component));


   tree ref_ptr_type = TREE_TYPE (ref_component);
   gcc_assert (TREE_CODE (ref_ptr_type) == POINTER_TYPE);

   tree ref_func_type = TREE_TYPE (ref_ptr_type);
   gcc_assert (TREE_CODE (ref_func_type) == FUNCTION_TYPE);

   ref_return_type = TREE_TYPE (ref_func_type);

   tree ref_call = build_call_vec (
       ref_return_type,
       ref_component,
       argvec);


   /* (TFirst *)var.ref(var) */
   tree tfirst_ptr_type = build_pointer_type (tfirst_type);
   aet_print_tree(tfirst_type);
   aet_print_tree(ref_call);
   tree cast_expr = fold_convert_loc (loc,tfirst_ptr_type,ref_call);

   /* *((TFirst *)var.ref(var)) */
   tree result = build1_loc (loc,INDIRECT_REF,tfirst_type,cast_expr);
   return result;
}

/**
 * 处理一个直接返回的变量。
 *
 * 只有同时满足以下条件才增加引用：
 *
 * 1. 是 VAR_DECL；
 * 2. 变量具有 cleanup；
 * 3. 变量类型与函数返回类型属于同一个 AET 类。
 *
 * 不处理任意表达式内部的变量。
 */
static tree transferReturnVar (tree expr,NPtrArray *cleanupArrays,tree returnType)
{
  ClassInfo *varInfo;
  ClassInfo *returnInfo;
  if (!expr || !VAR_P (expr))
    return expr;

  if (!isWithCleanupVar (expr, cleanupArrays))
    return expr;
  varInfo = class_mgr_get_class_info_by_record (class_mgr_get (),TREE_TYPE (expr));
  returnInfo = class_mgr_get_class_info_by_record (class_mgr_get (),returnType);
  if (!varInfo || !returnInfo || varInfo != returnInfo)
    return expr;

  /*
   * 构造：
   *
   *     *((T *) var.ref(...))
   */
  return build_ref_return_expr (expr);
}

/**
 * 只剥离常见的透明转换节点，获取实际返回变量。
 * 目前不在这里修改树，只用于识别表达式。
 */
static tree getReturnVar (tree expr)
{
   while (expr){
      switch (TREE_CODE (expr)){
         case NOP_EXPR:
         case CONVERT_EXPR:
         case NON_LVALUE_EXPR:
         case VIEW_CONVERT_EXPR:
            expr = TREE_OPERAND (expr, 0);
            break;

         default:
            return VAR_P (expr) ? expr : NULL_TREE;
      }
   }
   return NULL_TREE;
}

/**
 * 处理返回表达式的右值。
 *
 * 支持：
 *
 *     return temp;
 *
 *     return condition ? temp1 : temp2;
 *
 * 对于透明转换包装，先识别其内部变量。
 *
 * 不递归任意表达式，避免错误处理：
 *
 *     return foo(temp);
 *     return temp1 + temp2;
 *     return temp.member;
 */
static tree rewriteReturnValueExpr (tree expr, NPtrArray *cleanupArrays,tree returnType)
{
   tree var;
   if (!expr)
      return expr;

   /*
   * 条件表达式：
   *     condition ? trueExpr : falseExpr
   * 分别处理两个返回分支。
   */
   if (TREE_CODE (expr) == COND_EXPR){
      tree trueExpr = TREE_OPERAND (expr, 1);
      tree falseExpr = TREE_OPERAND (expr, 2);
      TREE_OPERAND (expr, 1) = rewriteReturnValueExpr (trueExpr,cleanupArrays,returnType);
      TREE_OPERAND (expr, 2) = rewriteReturnValueExpr (falseExpr,cleanupArrays,returnType);
      return expr;
   }

   /*
   * 先检查是不是直接变量。
   */
   if (VAR_P (expr))
      return transferReturnVar (expr,cleanupArrays,returnType);

   /*
   * 处理透明转换包装。
   *
   * 例如：
   *
   *     NOP_EXPR
   *       VAR_DECL temp
   *
   * 这里保留原来的转换节点，只替换内部变量。
   */
   var = getReturnVar (expr);

   if (var){
      tree replacement;
      replacement = transferReturnVar (var,cleanupArrays,returnType);
      if (replacement != var){
         /*
         * 不能直接把 INDIRECT_REF 放进原来的转换节点，
         * 因为原转换节点的类型可能与 replacement 不同。
         *
         * 这里直接用新表达式替换整个转换表达式。
         * 仅适用于当前已经确认返回类型相同的情况。
         */
         return replacement;
      }
   }
   return expr;
}

/**
 * 遍历函数体，处理 RETURN_EXPR。
 */
static tree transferOwnership_cb (tree *tp, int *walk_subtrees, void *data)
{
   NPtrArray *cleanupArrays = (NPtrArray *) data;
   tree t = *tp;
   if (!t)
      return NULL_TREE;
   if (TYPE_P (t)){
      *walk_subtrees = 0;
      return NULL_TREE;
   }

   if (TREE_CODE (t) != RETURN_EXPR)
      return NULL_TREE;

   {
      tree expr = TREE_OPERAND (t, 0);
      tree lhs;
      tree rhs;
      tree returnType;

      if (!expr)
         return NULL_TREE;

      /*
      * 当前 GCC C 前端返回值通常表现为：
      *
      *     RETURN_EXPR
      *       MODIFY_EXPR
      *         RESULT_DECL
      *         VAR_DECL
      *
      * 也兼容 INIT_EXPR。
      */
      if (TREE_CODE (expr) != MODIFY_EXPR  && TREE_CODE (expr) != INIT_EXPR)
         return NULL_TREE;

      lhs = TREE_OPERAND (expr, 0);
      rhs = TREE_OPERAND (expr, 1);

      if (!lhs || TREE_CODE (lhs) != RESULT_DECL)
         return NULL_TREE;

      returnType = TREE_TYPE (lhs);
      /*
      * 处理右值。
      */
      TREE_OPERAND (expr, 1) = rewriteReturnValueExpr (rhs,cleanupArrays,returnType);
   }

   /*
   * RETURN_EXPR 内部已经完成处理，不再向下递归。
   * 避免再次进入刚刚生成的 ref() 调用。
   */
   *walk_subtrees = 0;
   return NULL_TREE;
}

/**
 * 在 finish_function 阶段执行。
 *
 * 处理过程：
 *
 * 1. 如果函数没有需要处理的 AET 类返回值，直接返回。
 * 2. 收集所有具有 cleanup 的局部变量。
 * 3. 遍历 RETURN_EXPR。
 * 4. 对返回的 cleanup 变量增加一次引用。
 * 5. 保留原来的 TRY_FINALLY_EXPR。
 *
 * 因此函数退出时，局部变量仍然正常执行 unref()。
 */
void  object_return_finish_function(ObjectReturn *self,tree fndecl)
{
   //改变所有板的返回值变量，在当前函数中没有返回值是类变量的，什么都不做
   if(self->returnValArray->len>0){
      NPtrArray *cleanupArrays = n_ptr_array_new();
      walk_tree (&DECL_SAVED_TREE (fndecl), collectVarWithCleanup_cb, cleanupArrays, NULL);
      if(cleanupArrays->len>0){
         //现在我们已经有了带cleanup的变量，并且也有了返回值变量，可以在return_expr中转移所有权了
         walk_tree (&DECL_SAVED_TREE (fndecl), transferOwnership_cb, cleanupArrays, NULL);
      }
      n_ptr_array_remove_range(self->returnValArray,0,self->returnValArray->len);
      n_ptr_array_unref(cleanupArrays);
   }
   //接收所有权转移的变量
   if(self->varArray->len>0){
      NPtrArray *arras=n_ptr_array_new();
      struct add_cleanup_data data;
      data.dataArray = arras;
      data.visited = new hash_set<tree>;
      walk_tree (&DECL_SAVED_TREE (fndecl), readyCleanup_cb, &data, NULL);
      delete data.visited;
      struct add_cleanup_data data1;
      data1.dataArray = data.dataArray;
      data1.visited = new hash_set<tree>;
      walk_tree (&DECL_SAVED_TREE (fndecl), addCleanupFunc_cb, &data1, NULL);
      delete data1.visited;

      struct add_cleanup_data data2;
      data2.dataArray = data.dataArray;
      data2.visited = new hash_set<tree>;
      walk_tree (&DECL_SAVED_TREE (fndecl), addUnref_cb, &data2, NULL);
      delete data2.visited;
      n_ptr_array_remove_range(self->varArray,0,self->varArray->len);
      n_ptr_array_unref(arras);

   }
}

tree   object_return_convert(ObjectReturn *self,location_t loc,tree retVal)
{
   retVal = convertBlock(self,retVal);
   retVal = convertToIface(self,loc,retVal);
   collectReturnValue(self,retVal);
   return retVal;
}

void object_return_add_var(ObjectReturn *self,location_t loc,tree var)
{
   if(!VAR_P(var))
      return;
   ClassInfo *info = class_mgr_get_class_info_by_record(class_mgr_get(),TREE_TYPE(var));
   if(info){
      int i;
      nboolean find = FALSE;
      for(i=0;i<self->varArray->len;i++){
         tree item=n_ptr_array_index(self->varArray,i);
         if(item==var){
            find=TRUE;
            break;
         }
      }
      if(!find)
         n_ptr_array_add(self->varArray,var);
   }
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
