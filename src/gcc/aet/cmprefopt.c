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
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "c/c-parser.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"
#include "tree-iterator.h"
#include "sbitmap.h"
#include "c/c-lang.h"
#include "explow.h"
#include "rtl.h"
#include "opts.h"

#include "aet-c-parser-header.h"
#include "classimpl.h"
#include "aetprinttree.h"
#include "funcmgr.h"
#include "aetutils.h"
#include "cmprefopt.h"
#include "classmgr.h"
#include "makefileparm.h"
#include "genericutil.h"

//为什么不能在funccall callItself 直接选择 fromImplDefine ，因为 fromImplDefine可能还没定义。
/*第一种情况:调用setData是父类方法，生成是component_ref 但第一个参数是no_expr 也即(TFirst*)self转化的
class$ TFirst{
  public$ void setData();
};

impl$ TFirst{
   void setData(){
   }
};
class$ Second extends$ TFirst{
};

impl$ Second{
   void test(){
      setData();
   }
};
*/

/*第二种情况:调用setData是本类的方法，生成component_ref 但第一个参数是PARAM_DECL 即self
 *如果 public$ void setData();改为 private$ void setData();生成的也是 component_ref,第一个参数也是self
class$ TFirst{
  public$ void setData();
};

impl$ TFirst{
   void setData(){
   }
   void test(){
     setData();
   }
};
*/

/*第三种情况:调用setData是本类的类方法，生成是ADD_EXPR 但第一个参数是PARAM_DECL
 * 注意：Second不声明setData,只是覆盖setData
 * 如果加入声明setData 生成component_ref 第一个参数是PARAM_DECL SELF
class$ TFirst{
  public$ void setData();
  public$ void getData();

};

impl$ TFirst{
   void setData(){
   }
   public$ void getData(){
      setData()
   }

};
class$ Second extends$ TFirst{
};

impl$ Second{
   void setData(){
   }
   void test(){
      getData();//这里调用的是Second的setData,getData是TFirst的，但getData中调用setData不是TFirst中的setData,而是
      //Second中的setData。
   }
};
*/


static nboolean isSameType(tree trueVar,ClassFunc *func,tree currentFuncDecl);


/**
 * 优化引用到函数调用
 * xxx->yyyy();
 * 优化为
 * yyyy();
 */
static void cmpRefOptInit(CmpRefOpt *self)
{
   self->hashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);
   self->noAtAetCallTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);
}

/**
 * 创建带泛型块函数的函数名，创建方法应与genericcode.c中的一样 insertFuncWithGb-->replaceFuncName
 */
static char *createFuncName(char *mangleName,GenericModel *mm)
{
   if(mm==NULL)
      return NULL;
   if(generic_model_get_undefine_count(mm)==0){
      int i;
      char ret[128];
      for(i=0;i<generic_model_get_count(mm);i++){
         GenericUnit  *unit=generic_model_get(mm,i);
         generic_unit_create_block_func_prefix(unit->name,unit->pointerCount,ret);
      }
      return n_strdup_printf("%s_%s",ret,mangleName);
   }
   return NULL;
}

/**
 * 是否是帯泛型块的函数,只有第二次编译，才知道
 */
static nboolean isFuncWithGb(CmpRefOpt *self,ClassFunc *classFunc,GenericModel *mm,tree *fnwithgb)
{
   //如果是第二次编译，再一次判断该调用可不可以优化
   if(makefile_parm_is_second_compile(makefile_parm_get()) && mm){
      n_debug("comprefopt.c 是第二次编译 %s\n",in_fnames[0]);
      char *funcName = createFuncName(classFunc->mangleFunName,mm);
      if(funcName!=NULL){
        // printf("查带泛型埠的函数是真实类型定义。%s\n",funcName);
         tree fndecl=lookup_name(get_identifier(funcName));
         if(fndecl){
           // printf("这是一个带泛型块函数的调用 无条件替换 %s\n",funcName);
           // aet_print_tree(fndecl);
            if(fnwithgb)
               *fnwithgb=fndecl;
            free(funcName);
            return TRUE;
         }
         free(funcName);
      }
   }
   return FALSE;
}

/**
 * 根据ClassFunc中的fieldDecl，构建一个函数声明。
 *
 */
static tree  createExternFuncDecl(location_t loc,ClassFunc *func)
{
   char *funName=func->mangleFunName;
   tree fieldDecl = func->fieldDecl;
   tree id = aet_utils_create_ident (funName);
   tree funcDecl=lookup_name(id);
   if(funcDecl && TREE_CODE(funcDecl)==FUNCTION_DECL){
      printf("createExternFuncDecl 00 已存在 %s\n",funName);
      aet_print_tree(funcDecl);
      return funcDecl;
   }
   //tree funcType=TREE_TYPE(fieldDecl);
   tree oldfntype= TREE_TYPE(TREE_TYPE(fieldDecl));
   tree retn = TREE_TYPE (oldfntype);
   tree parmList = TYPE_ARG_TYPES (oldfntype);
   tree funcType = build_function_type(retn,parmList);

   funcDecl = build_decl (loc, FUNCTION_DECL, id, funcType);
   TREE_STATIC (funcDecl) = 0;
   TREE_PUBLIC (funcDecl) = 1;
   DECL_EXTERNAL (funcDecl) = 1;
   DECL_CONTEXT(funcDecl) = NULL;
   pushdecl (funcDecl); //不能调用 finish_decl 否则出undefined reference to `_TSecond__superFuncAddressArray'
   //c_c_decl_bind_file_scope(funcDecl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   //finish_decl (funcDecl, loc, NULL_TREE,NULL_TREE, NULL_TREE);
 //  printf("createExternFuncDecl 11 创建新的extern 函数声明 %s\n",funName);

   return funcDecl;
}

/**
 * 可以被优化吗
 */
static nboolean canOptimized(ClassFunc *classFunc)
{
   if(class_func_is_static(classFunc) || !class_func_is_normal(classFunc))
      return FALSE;
   ClassName *className = classFunc->className;
   if(endswith(className->sysName,AET_ROOT_OBJECT) || endswith(className->sysName,AET_ROOT_CLASS))
      return FALSE;
   return TRUE;
}

/**
 * 判断是不是self->xxx调用
 * func被调用的函数
 */
static nboolean isSelfOrVarCall(tree func,char **funcName,GenericModel **model,tree *trueRef)
{
   if(TREE_CODE(func)!=COMPONENT_REF){
      return FALSE;
   }
   tree selfTree=TREE_OPERAND (func, 0);
   if(TREE_CODE(selfTree)!=INDIRECT_REF){
      return FALSE;
   }
   tree ret=NULL_TREE;
   tree selfParm=TREE_OPERAND (selfTree, 0);
   tree fieldDecl=TREE_OPERAND (func, 1);
   char *fieldName=IDENTIFIER_POINTER(DECL_NAME(fieldDecl));
   *funcName=fieldName;
   if(TREE_CODE(selfParm)==PARM_DECL){
      char *name=IDENTIFIER_POINTER(DECL_NAME(selfParm));
      if(!strcmp(name,"self")){
         ret = selfParm;
         goto out;
      }
   }else if(TREE_CODE(selfParm)==NOP_EXPR){
      selfParm=TREE_OPERAND (selfParm, 0);
      if(TREE_CODE(selfParm)==PARM_DECL){
         char *name=IDENTIFIER_POINTER(DECL_NAME(selfParm));
         if(!strcmp(name,"self")){
             ret = selfParm;
             goto out;
         }
      }
   }else if(TREE_CODE(selfParm)==VAR_DECL){
       tree type=TREE_TYPE(selfParm);
       //判断是不是aet的类
       char *name = class_util_get_class_name(type);
      // printf("是否找是一个类调用方法---%s\n",name);
       if(name){
          ret = selfParm;
          goto out;
       }
   }else if(TREE_CODE(selfParm)==COMPONENT_REF){
      //匹配类中的类变量
      tree fieldDecl=TREE_OPERAND (selfParm, 1);
      if(TREE_CODE(fieldDecl)==FIELD_DECL){
         tree type=TREE_TYPE(fieldDecl);
         char *name = class_util_get_class_name(type);
        // printf("是否找是一个类调用方法---%s\n",name);
         if(name){
            ret = selfParm;
            goto out;
         }
      }
   }else{
      aet_print_tree(func);
      n_warning("在CmpRefOpt中有不支持的类型\n");
   }
out:
   GenericModel *mm =  c_aet_get_generics_model(ret);
   *model = mm;
   if(trueRef)
      *trueRef = ret;
   return ret!=NULL_TREE;
}

static  vec<tree, va_gc> *createParm(tree callExpr)
{
   vec<tree, va_gc> *parmVec;
   parmVec = make_tree_vector ();
   int i = 0;
   tree arg;
   call_expr_arg_iterator iter;
   FOR_EACH_CALL_EXPR_ARG (arg, iter, callExpr){
      vec_safe_push (parmVec, arg);
      i++;
   }
   return parmVec;
}

typedef struct _WalkData{
    char *sysName;
    CmpRefOpt *self;
    tree currentFuncDecl;
}WalkData;

/**
 * 把self->xxx() 替换成 yyy()
 * 如果方法是private$才能替换，否则不允许，因为子类可能重载覆盖该方法.
 * 语意是在父类中调用了子类覆盖的方法
 *
 */
static tree link_cb (tree *tp, int *walk_subtrees, void *data)
{
   WalkData *dp = (WalkData *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR){
      walk_tree (&BIND_EXPR_BODY (t), link_cb, data, NULL);
   }else if(TREE_CODE(t)==CALL_EXPR){
      tree func=CALL_EXPR_FN(t);
      char *mangleName=NULL;
      GenericModel *mm=NULL;
      tree trueVar = NULL_TREE;
      nboolean ret= isSelfOrVarCall(func,&mangleName,&mm,&trueVar);
      if(ret){
         ClassFunc *classFunc=func_mgr_get_func_by_mangle(func_mgr_get(),mangleName);
         if(classFunc){
            ClassInfo *cinfo = class_mgr_get_class_info_by_class_name(class_mgr_get(),classFunc->className);
            tree fnwithgb = NULL_TREE;
            nboolean funcWithGb = FALSE;
            nboolean sameType = FALSE;
            if(canOptimized(classFunc) &&
                       !(class_func_is_private(classFunc)
                             || class_func_is_final(classFunc)
                             || class_info_is_final(cinfo))){
               funcWithGb = isFuncWithGb(dp->self,classFunc,mm,&fnwithgb);
               //printf("cmprefopt.c link_cb 00 不是 private final func final class 调用带泛型块的函数:%d\n",funcWithGb);
               if(!funcWithGb){
                  sameType = isSameType(trueVar, classFunc,dp->currentFuncDecl);
                  //printf("cmprefopt.c link_cb 11 不是 private final func final class 调用公共方法 sameType:%d\n",sameType);
               }
            }

            if(canOptimized(classFunc) &&
                  (class_func_is_private(classFunc)
                        || class_func_is_final(classFunc)
                        || class_info_is_final(cinfo) || funcWithGb || sameType)){
               tree last = classFunc->fromImplDefine;
//               if(!class_func_is_private(classFunc) && !aet_utils_valid_tree(last)){
//                  printf("cmprefopt.c link_cb 22 没有定义，只有域声明 %s\n",mangleName);
//                  last =  createExternFuncDecl(EXPR_LOCATION(t),classFunc);
//               }
               if(funcWithGb)
                  last= fnwithgb;
               if(last){
                  //如果是泛型类调用的函数，需要检查函数是不是带泛型块，并且并优化为具体类型了。
                  //只能在第二次编译时才能查找是否有存在具体类的函数
                  n_debug("cmprefopt.c 可以替换了  mangleName:%s funcWithGb:%d name:%s",
                        mangleName,funcWithGb,IDENTIFIER_POINTER(DECL_NAME(last)));
                  vec<tree, va_gc> *parms=createParm(t);
                  tree newCallExpr = c_build_function_call_vec (EXPR_LOCATION(t), vNULL,last,parms, NULL);
                 // aet_print_tree(newCallExpr);
                  *tp=newCallExpr;
                  release_tree_vector (parms);
               }
            }
         }
      }
   }
   return NULL_TREE;
}

/**
 * 链接到定义的函数中
 * 在impl$结尾处调用,现改为在文件结束后调用
 */
void  cmp_ref_opt_opt(CmpRefOpt *self)
{
   NHashTableIter iter;
   npointer key, value;
   n_hash_table_iter_init(&iter, self->hashTable);
   while (n_hash_table_iter_next(&iter, &key, &value)) {
      char *sysName = (char *)key;
      NPtrArray *array=(NPtrArray *)value;
      int i;
      n_debug("cmp_ref_opt_opt start:%s\n",sysName);
      for(i=0;i<array->len;i++){
        tree func=n_ptr_array_index(array,i);
        WalkData data;
        data.sysName=sysName;
        data.self= self;
        data.currentFuncDecl=func;
        walk_tree (&DECL_SAVED_TREE(func), link_cb, &data, NULL);
     }
  }
   //类外部调用
   n_hash_table_iter_init(&iter, self->noAtAetCallTable);
   while (n_hash_table_iter_next(&iter, &key, &value)) {
      char *funcName = (char *)key;
      n_debug("cmp_ref_opt_opt 类外部的函数:%s\n",funcName);
      tree func=(tree)value;
      WalkData data;
      data.sysName=NULL;
      data.self= self;
      data.currentFuncDecl = func;
      walk_tree (&DECL_SAVED_TREE(func), link_cb, &data, NULL);
   }
}

static tree print_body_cb (tree *tp, int *walk_subtrees, void *data)
{
   WalkData *dp = (WalkData *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
   else if (TREE_CODE (t) == BIND_EXPR){
      walk_tree (&BIND_EXPR_BODY (t), print_body_cb, data, NULL);
   }else if(TREE_CODE(t)==CALL_EXPR){
      tree func=CALL_EXPR_FN(t);
      printf("CmpRefOpt print_body_cb 00\n");
      aet_print_tree(t);
   }else{
      printf("CmpRefOpt print_body_cb 11\n");
      aet_print_tree(t);

   }
   return NULL_TREE;
}


void  cmp_ref_opt_print(CmpRefOpt *self,tree func)
{
   WalkData data;
   data.sysName=NULL;
   walk_tree (&DECL_SAVED_TREE(func), print_body_cb, &data, NULL);
}

/**
 * 加入被调用的函数声明
 */
void cmp_ref_opt_add(CmpRefOpt *self,tree func)
{
   if(!self->parser->isAet)
      return NULL_TREE;
   ClassImpl *impl=class_impl_get();
   char *sysName=impl->className->sysName;
   n_debug("cmp_ref_opt_add ---%s %s\n",sysName,IDENTIFIER_POINTER(DECL_NAME(func)));
   if(!n_hash_table_contains(self->hashTable,sysName)){
      NPtrArray *array=n_ptr_array_new();
      n_ptr_array_add(array,func);
      n_hash_table_insert (self->hashTable, n_strdup(sysName),array);
   }else{
      NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
      n_ptr_array_add(array,func);
   }
}


//////////////////////------------------------------------


/* 判断 tree 是否是目标变量（或它的 SSA 名，但 GENERIC 阶段通常还没有 SSA） */
static bool is_target_var(tree expr, tree target_var)
{
  if (expr == target_var)
    return true;
  /* 有时候左边是带转换的 */
  if (TREE_CODE(expr) == NOP_EXPR || TREE_CODE(expr) == CONVERT_EXPR)
    return is_target_var(TREE_OPERAND(expr, 0), target_var);
  return false;
}

/* 在一条语句中查找对 target_var 的赋值，返回 RHS */
static tree find_assign_in_stmt(tree stmt, tree target_var)
{
  if (!stmt || stmt == error_mark_node)
    return NULL_TREE;

  if (TREE_CODE(stmt) == MODIFY_EXPR || TREE_CODE(stmt) == INIT_EXPR){
      tree lhs = TREE_OPERAND(stmt, 0);
      tree rhs = TREE_OPERAND(stmt, 1);
      if (is_target_var(lhs, target_var))
        return rhs;          /* 找到了真正的初始化表达式 */
    }

  return NULL_TREE;
}

/* 递归遍历语句（支持 BIND_EXPR、STATEMENT_LIST、TRY、表达式语句等） */
static tree walk_stmts_for_init(tree stmt, tree target_var)
{
  tree result;
  if (!stmt || stmt == error_mark_node)
    return NULL_TREE;
  /* 直接是赋值语句 */
  result = find_assign_in_stmt(stmt, target_var);
  if (result)
    return result;

  switch (TREE_CODE(stmt)){
    case BIND_EXPR:
      /* 先看绑定变量的初始化，再看 body */
      {
        tree var;
        for (var = BIND_EXPR_VARS(stmt); var; var = DECL_CHAIN(var)){
            if (var == target_var && DECL_INITIAL(var)
                && TREE_CODE(DECL_INITIAL(var)) != INTEGER_CST)
              return DECL_INITIAL(var);
        }
        return walk_stmts_for_init(BIND_EXPR_BODY(stmt), target_var);
      }

    case STATEMENT_LIST:
      {
        tree_stmt_iterator i;
        for (i = tsi_start(stmt); !tsi_end_p(i); tsi_next(&i)){
            result = walk_stmts_for_init(tsi_stmt(i), target_var);
            if (result)
              return result;
        }
      }
      break;

    case TRY_FINALLY_EXPR:
    case TRY_CATCH_EXPR:
      result = walk_stmts_for_init(TREE_OPERAND(stmt, 0), target_var);
      if (result)
         return result;
      return walk_stmts_for_init(TREE_OPERAND(stmt, 1), target_var);

    case CLEANUP_POINT_EXPR:
    case EXPR_STMT:
    case CONVERT_EXPR:
    case NOP_EXPR:
    case NON_LVALUE_EXPR:
      return walk_stmts_for_init(TREE_OPERAND(stmt, 0), target_var);

    case COND_EXPR:
      result = walk_stmts_for_init(TREE_OPERAND(stmt, 1), target_var);
      if (result)
         return result;
      return walk_stmts_for_init(TREE_OPERAND(stmt, 2), target_var);

    case LOOP_EXPR:
      return walk_stmts_for_init(TREE_OPERAND(stmt, 0), target_var);

    default:
      break;
    }

  return NULL_TREE;
}


/*
* 主函数：在 GENERIC 阶段查找变量的真正初始化表达式
 *
 * @param fn_decl   当前函数的 FUNCTION_DECL
 * @param target_var  要查找的变量（例如 tempxx 的 VAR_DECL）
 * @return 真正的初始化表达式（很可能是 TARGET_EXPR），找不到返回 NULL_TREE
 */
static tree find_real_initial_generic(tree fn_decl, tree target_var)
{
  if (!fn_decl || !target_var)
    return NULL_TREE;
  if(TREE_CODE(target_var)!=VAR_DECL && TREE_CODE(target_var)!=PARM_DECL)
     return NULL_TREE;
  /* 1. 先看 DECL_INITIAL（有些情况下还在） */
  tree init = DECL_INITIAL(target_var);
  if (init && TREE_CODE(init) != INTEGER_CST && init != error_mark_node)
    return init;

  /* 2. 遍历函数体 */
  tree body = DECL_SAVED_TREE(fn_decl);
  if (!body)
    return NULL_TREE;
  //printf("find_real_initial_generic 00 %p %p\n",body,target_var);
  return walk_stmts_for_init(body, target_var);
}

/**
 * 获取一个变量的初始化值
 */
static tree getInitExpr(tree ref,tree currentFuncDecl)
{
   // 1. 先从 tyxf 出发，跳过强制转换，找到它来源于哪个变量
   tree origin = ref;
   if(TREE_CODE(ref)!=VAR_DECL && TREE_CODE(ref)!=PARM_DECL)
      return NULL_TREE;
   n_debug("cmprefopt.c getInitExpr 获取一个变量的初始化值 00\n");
   tree init = DECL_INITIAL(origin);
   while (init && (TREE_CODE(init) == NOP_EXPR || TREE_CODE(init) == CONVERT_EXPR)){
       origin = TREE_OPERAND(init, 0);
       if (TREE_CODE(origin) == VAR_DECL)
           init = DECL_INITIAL(origin);
       else
           break;
   }
   // 2. 现在 origin 应该是 tempxx，再用 find_real_initial_generic 找真正的 TARGET_EXPR
   tree real_init = find_real_initial_generic(currentFuncDecl, origin);
   return real_init;
}

/*
*trueVar是aet类，查找它的来源new$ XXX，是不是在存在并且类名是想同的
*func是被调用的方法,
*currentFuncDecl调用func所在的函数
*代码如下：目的是找出tyxf是来自new,这样保证被调用的func就是来自func.而不是子类的覆盖方法
*AObject *tempxx=new$ TFirst();
*TFirst *tyxf = (TFirst*)tempxx;
*tyxf->push();
*/
static nboolean isSameType(tree trueVar,ClassFunc *func,tree currentFuncDecl)
{
    tree ret = getInitExpr(trueVar,currentFuncDecl);
    //printf("isSameType  结果是----%p\n",ret);
    //aet_print_tree(ret);
   // aet_print_tree(trueVar);
    tree target=NULL_TREE;
    if(ret && TREE_CODE(ret)==NOP_EXPR){
       target =TREE_OPERAND(ret,0);
       if(TREE_CODE(target)==TARGET_EXPR){
      label:
         tree type=TREE_TYPE(target);
         char  *sysName1 = class_util_get_class_name(type);
         char  *sysName2 = class_util_get_class_name(TREE_TYPE(trueVar));
        // printf("isSameType  结果是 11----%p %s %s\n",ret,sysName1,sysName2);
         if(sysName1 && sysName2 && !strcmp(sysName1,sysName2)){
            return TRUE;
         }
       }
       return FALSE;
    }else if(ret && TREE_CODE(ret)==TARGET_EXPR){
       target = ret;
       goto label;
    }
    return FALSE;

}
/**
 * 不在isAet中调用类方法，是否可以优化为函数调用
 */
tree  cmp_ref_opt_outside(CmpRefOpt *self,tree compref,
      ClassFunc *classFunc,vec<tree, va_gc> *exprlist,location_t loc)
{
   if(self->parser->isAet || classFunc==NULL || !current_function_decl)
      return compref;
   if(!canOptimized(classFunc))
      return compref;
   char *mangleName=NULL;
   GenericModel *mm=NULL;
   tree trueVar = NULL_TREE;
   nboolean ret= isSelfOrVarCall(compref,&mangleName,&mm,&trueVar);
   n_debug("cmp_ref_opt_outside 00 -- %d %s model:%s\n",
         ret,mangleName,classFunc->mangleFunName,generic_model_tostring(mm));
   if(ret && !strcmp(mangleName,classFunc->mangleFunName)){
      char *currentName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
      if(!n_hash_table_contains(self->noAtAetCallTable,currentName)){
         n_debug("cmp_ref_opt_outside 11-- %s %s",currentName,mangleName);
         n_hash_table_insert (self->noAtAetCallTable, currentName,current_function_decl);
      }
   }
   return compref;
}


CmpRefOpt *cmp_ref_opt_new()
{
   CmpRefOpt *self =n_slice_alloc0 (sizeof(CmpRefOpt));
   cmpRefOptInit(self);
   self->parser = aet_parser_get();
   return self;
}



