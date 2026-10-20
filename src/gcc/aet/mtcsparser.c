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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "plugin.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "aetinfo.h"
#include "mtcsparser.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "classmgr.h"
#include "classimpl.h"
#include "classbuild.h"
#include "mtcs/mtcstool.h"
#include "aetparser.h"
#include "mtcstypes.h"
#include "mtcsinfo.h"
#include "classparser.h"
#include "makefileparm.h"
#include "middlefile.h"

//提升shared 局部变量为 static 类型的全局变量
typedef struct  _PromoteSharedVar
{
   int id;
   nuint64 hostDeclAddress; //保存指针的数值，因为在mtcscompile中会移走hostDecl
}PromoteSharedVar;

static tree handle_mtcs_attribute (tree *node, tree name, tree args,int flags, bool *no_add_attrs);

static int  getPromoteDeclId_cb(AetMediatorUser *user,nuint64 hostDeclAddress)
{
   MtcsParser *self = (MtcsParser *)user;
   int len=self->mtcsPromoteLocalVarArray->len;
   int i;
   for(i=0;i<len;i++){
      PromoteSharedVar *item=(PromoteSharedVar *)n_ptr_array_index(self->mtcsPromoteLocalVarArray,i);
      if(item->hostDeclAddress == hostDeclAddress)
         return item->id;
   }
   return -1;
}

static void printAttrs (tree attrs)
{
   for (const_tree attr = attrs; attr; attr = TREE_CHAIN (attr)){
      char *name=IDENTIFIER_POINTER(TREE_PURPOSE (attr));
      n_debug("mtcsparser.c printAttrs 属性 %s\n",name);
      aet_print_tree(attr);
   }
}

/**
 * 获取声明_TSecond_parent__superDeviceAddressArray
 * 由于mtcsparser不能调用lookup_name,所以通过中介获得。
 */
static tree  getParentDeviceArrayDecl_cb(AetMediatorUser *user,char *sysName)
{
   MtcsParser *self = (MtcsParser *)user;
   SuperCall *superCall=class_impl_get()->superCall;
   return super_call_get_parent_device_decl(superCall,sysName);
}

//当完成mtcs的汇编生成，从mtcscompile发送需要链接的函数到前端，由前端保存。
static void  addLinkFunc_cb(AetMediatorUser *aetMediatorUser,const char *linkFuncNames,
      int version,int isa,const char *platName)
{
   MtcsParser *self = (MtcsParser *)aetMediatorUser;
   mtcs_link_add(self->mtcsLink,linkFuncNames,version,isa,platName);
}

/**
 * 完成主机asm，编译mtcs后 mtcscompile 发送消息到AST
 * 要求写入中间代码汇编文件(ptx)到.aetprog section中并追加到主机汇编的最后
 */
static void  writeNote_cb(AetMediatorUser *aetMediatorUser)
{
    middle_file_save_note(middle_file_get());
}

static char *getObjectFile_cb(AetMediatorUser *self)
{
   return makefile_parm_get_object_file(makefile_parm_get());
}

static tree  getMIIFunction_cb(AetMediatorUser *self,tree micFndecl)
{
    char *mangleFuncName = IDENTIFIER_POINTER(DECL_NAME(micFndecl));
    ClassFunc *micClassFunc = func_mgr_get_static_entity_by_mangle(func_mgr_get(),mangleFuncName);
    printf("这是不是一个mic函数名:%s %p\n",mangleFuncName,micClassFunc);
    ClassFunc *miifn = func_mgr_get_mii_func(func_mgr_get(),micClassFunc);
    printf("这是不是一个mic函数名ttt:%s %p\n",mangleFuncName,miifn);
    //aet_print_tree(miifn->fromImplDefine);
    if(miifn && miifn->fromImplDefine)
       return miifn->fromImplDefine;

    return NULL_TREE;
}


static void initMediator(MtcsParser *self)
{
   AetMediator *mediator = aet_mediator_get();
   AetMediatorUser *mediatorUser =(AetMediatorUser *)self;
   mediatorUser->mediator= mediator;
   mediatorUser->getPromoteDeclId=getPromoteDeclId_cb;
   mediatorUser->getParentDeviceArrayDecl=getParentDeviceArrayDecl_cb;
   mediatorUser->addLinkFunc=addLinkFunc_cb;
   mediatorUser->getObjectFile = getObjectFile_cb;
   mediatorUser->writeNote = writeNote_cb;
   mediatorUser->getMIIFunction = getMIIFunction_cb;
   aet_mediator_add_user(mediator,mediatorUser);
}

//定义在abase.h中
//#define __device__ __attribute__((device))
//#define __global__ __attribute__((global))
//#define __shared__ __attribute__((shared))
//#define __host__ __attribute__((host))
static void mtcsParserInit(MtcsParser *self)
{
   /*注册新的属性用于mtcs*/
   const struct attribute_spec *spx=lookup_attribute_spec (get_identifier(MTCS_GLOBAL_STR));
   if(!spx){
      struct attribute_spec *att =new attribute_spec {xstrdup(MTCS_GLOBAL_STR),
         0, -2, false,  false, false, false, handle_mtcs_attribute, NULL };
      register_attribute (att);
   }

   spx=lookup_attribute_spec (get_identifier(MTCS_DEVICE_STR));
   if(!spx){
      struct attribute_spec *att =new attribute_spec {xstrdup(MTCS_DEVICE_STR),
         0, -2, false,  false, false, false, handle_mtcs_attribute, NULL };
      register_attribute (att);
   }

   spx=lookup_attribute_spec (get_identifier(MTCS_HOST_STR));
   if(!spx){
      struct attribute_spec *att =new attribute_spec {xstrdup(MTCS_HOST_STR),
         0, -2, false,  false, false, false, handle_mtcs_attribute, NULL };
      register_attribute (att);
   }

   spx=lookup_attribute_spec (get_identifier(MTCS_SHARED_STR));
   if(!spx){
      struct attribute_spec *att =new attribute_spec {xstrdup(MTCS_SHARED_STR),
         0, -2, false,  false, false, false, handle_mtcs_attribute, NULL };
      register_attribute (att);
   }

   spx=lookup_attribute_spec (get_identifier(MTCS_MANAGED_STR));
   if(!spx){
      struct attribute_spec *att =new attribute_spec {xstrdup(MTCS_MANAGED_STR),
         0, -2, false,  false, false, false, handle_mtcs_attribute, NULL };
      register_attribute (att);
   }

   spx=lookup_attribute_spec (get_identifier(MTCS_CONSTANT_STR));
   if(!spx){
      struct attribute_spec *att =new attribute_spec {xstrdup(MTCS_CONSTANT_STR),
         0, -2, false,  false, false, false, handle_mtcs_attribute, NULL };
      register_attribute (att);
   }

   self->enterFuncBody=0;
   self->mtcsPromoteLocalVarArray = n_ptr_array_new();
   self->mtcsLanuch = mtcs_lanuch_new();
   self->mtcsBuiltinTree =mtcs_builtin_tree_new ();
   self->localVarAssembleNumber = 0;
   //中介者模式
   initMediator(self);
   self->mtcsLink=mtcs_link_new();
}

/* Attribute handler callback */
static tree handle_mtcs_attribute (tree *node, tree name, tree args,int flags, bool *no_add_attrs)
{
   return NULL_TREE;
}

/**
 * 进入编译MTCS函数体
 */
void  mtcs_parser_enter_function_body(MtcsParser *self)
{
    self->enterFuncBody++;
}

void  mtcs_parser_exit_function_body(MtcsParser *self)
{
    self->enterFuncBody--;
}

nboolean    mtcs_parser_is_compiling(MtcsParser *self)
{
   return self->enterFuncBody > 0;
}

/**
 * 当分析完源代码并生成语法树后，通知mtcscompile
 * 如果mtcsFuncArray中的ClassFunc只有声明，没有定义，mtcscompile不会创建。
 */
void mtcs_parser_ast_end(MtcsParser *self)
{
   //如果有mtcs函数，初始化mtcscmpile
   self->haveMtcsFunc = func_mgr_have_mtcs_func(func_mgr_get());
   if(self->haveMtcsFunc || self->haveMtcsVar){
      mtcs_builtin_tree_set_builtins_code(self->mtcsBuiltinTree);
      AetMediator *mediator=((AetMediatorUser *)self)->mediator;
      aet_mediator_ast_end(mediator,TRUE,(AetMediatorUser *)self);
   }
}

/**
 * 是否有mtcs方法或变量
 */
nboolean mtcs_parser_have_mtcs(MtcsParser *self)
{
   self->haveMtcsFunc = func_mgr_have_mtcs_func(func_mgr_get());
   return (self->haveMtcsFunc || self->haveMtcsVar);
}

/*
* 检查共享变量。
* 1.范围 文件，mtcs函数，主机函数
* 2.是否有static修饰
* 3.是否初始化
*/
static void checkSharedVar(MtcsParser *self,tree decl,nboolean finishDecl)
{
   //检查是否初始化了
   if(DECL_INITIAL(decl)){
      error_at(DECL_SOURCE_LOCATION(decl),"不能初始化共享内存%qD。",decl);
   }
   if(!self->enterFuncBody && current_function_decl){
      error_at(DECL_SOURCE_LOCATION(decl),"__shared__不允许在主机函数内声明%qD。",decl);
   }
}

static void checkManagedVar(MtcsParser *self,tree decl,nboolean finishDecl)
{
   if(self->enterFuncBody){
      if(!TREE_STATIC(decl) && !DECL_EXTERNAL(decl))
         error_at(DECL_SOURCE_LOCATION(decl),"函数内只能用\"static\"或\"extern\"修饰 __managed__ 变量。");
   }
}

static void checkConstantVar(MtcsParser *self,tree decl,nboolean finishDecl)
{
   if(self->enterFuncBody){
      if(!TREE_STATIC(decl) && !DECL_EXTERNAL(decl))
         error_at(DECL_SOURCE_LOCATION(decl),"函数内只能用\"static\"或\"extern\"修饰 __constant__ 变量。");

   }
}

static void checkDeviceVar(MtcsParser *self,tree decl,nboolean finishDecl)
{
   if(self->enterFuncBody){
      if(!TREE_STATIC(decl) && !DECL_EXTERNAL(decl))
         error_at(DECL_SOURCE_LOCATION(decl),"函数内只能用\"static\"或\"extern\"修饰 __device__ 变量。");
      else if(DECL_INITIAL(decl) && DECL_EXTERNAL(decl)){
         error_at(DECL_SOURCE_LOCATION(decl),"不能初始化extern设备变量%qD。",decl);
      }
   }
}

//为 int shared var 插入一条调用语句。该调用生成汇编代码 .shared .align 4 .u32 var.0;
//id 唯一的对应声明变量
static tree createSharedCall(MtcsParser *self,location_t loc,int id,int reserver)
{
   tree fndecl = mtcs_builtin_tree_create_shared_fndecl(self->mtcsBuiltinTree);
   vec<tree, va_gc> *arg_vec;
   vec_alloc (arg_vec, 2);
   tree tr = build_int_cst (integer_type_node, id);
   arg_vec->quick_push (tr);
   tree te = build_int_cst (integer_type_node, reserver);
   arg_vec->quick_push (te);
   tree call = build_call_expr_loc_vec (loc,fndecl,arg_vec);
   return call;
}

/**
 * 函数内声明的shared,managed类型的变量，设置汇编名
 * 命名规则：
 * 函数名+变量名+序号
 */
static void setAssembleName(MtcsParser *self,tree decl)
{
   char *fnname=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
   char *varname=IDENTIFIER_POINTER(DECL_NAME(decl));
   char name[255];
   sprintf(name,"%s_%s_%d",fnname,varname,self->localVarAssembleNumber++);
   SET_DECL_ASSEMBLER_NAME (decl, get_identifier(name));
}

static void checkVarSize(MtcsParser *self,tree decl,nboolean finishDecl)
{
    if(!finishDecl)
       return;
    n_debug("mtcsparser.c checkVarSize 00 检查变量大小\n");
    tree type=TREE_TYPE(decl);
    if(TREE_CODE(type)==ARRAY_TYPE){
       tree arrayType = TREE_TYPE (type);
       n_debug("mtcsparser.c checkVarSize 11\n");
       tree domain = TYPE_DOMAIN (type);
       if(!domain){
          n_debug("mtcsparser.c checkVarSize 22 没有指定数组大小\n");
          tree init = DECL_INITIAL(decl);
          if(!init){
             error_at(DECL_SOURCE_LOCATION(decl),"设备变量%qD数组，需要指定常数大小。",decl);
          }
       }
    }
}

/**
 * 有如下代码:MTCS_BLOCK是宏常量
 *  const int threads = MTCS_BLOCK;
 *  float __shared__  yxdx[threads];
 * 如果type的domain中有COMPOUND_EXPR op0无影响，op1是常数可以用op1替换 max
 * 执行下面代码相当于把 float __shared__  yxdx[threads]; 变成了 float __shared__  yxdx[MTCS_BLOCK];
 * 改变变量后一定要执行relayout_decl(decl);   // 重新计算布局
 * 否则改变不起作用。
 * -O3编译生成的tree COMPOUND_EXPR -O0生成的是 NOP_EXPR-->PLUS_EXPR-->NOP_EXPR-->VAR_DECL
 */
static nboolean replaceArrayTypeDomain(tree vardecl)
{
   tree type=TREE_TYPE(vardecl);
   tree domain = TYPE_DOMAIN (type);
   if(TREE_CODE(domain)!=INTEGER_TYPE)
      return FALSE;
   tree min = TYPE_MIN_VALUE (domain);
   if(TREE_CODE(min)!=INTEGER_CST)
      return FALSE;
   wide_int result=wi::to_wide(min);
   int value=result.to_shwi();
   if(value!=0)
      return FALSE;
   tree max = TYPE_MAX_VALUE (domain);

   if(TREE_CODE(max)!=INTEGER_CST){
      if(TREE_CODE(max)==COMPOUND_EXPR){
         tree lhs = TREE_OPERAND (max, 0);
         if (TREE_SIDE_EFFECTS (lhs)){
            return FALSE;
         }
         tree t = TREE_OPERAND (max, 1);
         if(TREE_CODE(t)==INTEGER_CST){
            wide_int result=wi::to_wide(t);
            int value=result.to_shwi()+1;
            tree new_type = build_array_type_nelts (TREE_TYPE (type),value);
            TREE_TYPE (vardecl) = new_type;
            return TRUE;
         }
      }else if(TREE_CODE(max)==NOP_EXPR){
         tree lhs = TREE_OPERAND (max, 0);
         if(TREE_CODE(lhs)==PLUS_EXPR){
            tree op = TREE_OPERAND (lhs, 0);
            if(TREE_CODE(op)==NOP_EXPR){
               tree var = TREE_OPERAND (op, 0);
               if(!TREE_SIDE_EFFECTS (lhs) && !TREE_SIDE_EFFECTS (op)
               && TREE_CODE(var)==VAR_DECL  && TREE_READONLY(var)){
                  //bug 081 修复
                  if(DECL_INITIAL(var)==error_mark_node)
                     return FALSE;
                  wide_int result=wi::to_wide(DECL_INITIAL(var));
                  int value=result.to_shwi()+1;
                  tree new_type = build_array_type_nelts (TREE_TYPE (type),value);
                  TREE_TYPE (vardecl) = new_type;
                  return TRUE;
               }
            }
         }
      }
   }
   return FALSE;
}

/**
 * 完成声明后检查变量
 * 如果声明的变量是数组，需要检查数组大小是不是常数。
*
*  #define MTCS_BLOCK 512
*  (1)float __shared__  var0[MTCS_BLOCK];
*  正常，由于加入static 修饰var0，不会成为函数内的局部变量
*  (2)const int threads = MTCS_BLOCK;
*     float __shared__  var0[threads];
*   虽然 threads是一个常数，修饰符static 并没有改变var0的作用域，还是变成了函数局部变量。
*   需要把常数变量 threads改为常数
*   如果threads不是常数需要报错。
 */

void mtcs_parser_set_mtcs_var(MtcsParser *self,tree decl)
{
  if(VAR_P(decl)){
      nboolean finishDecl = TRUE;
      MtcsVarType type = mtcs_info_get_and_check_var_type(decl,DECL_SOURCE_LOCATION(decl));
      if(type==MTCS_VAR_NOT)
         return;
      self->haveMtcsVar = TRUE;
      n_debug("mtcsparser.c 声明是一个变量 类型是:%d external:%d static:%d public:%d enterFuncBody:%d %d\n",type,
               DECL_EXTERNAL(decl),TREE_PUBLIC(decl),TREE_STATIC(decl),self->enterFuncBody,finishDecl);
      if(type==MTCS_VAR_SHARED){
         checkSharedVar(self,decl,finishDecl);
      }else  if(type==MTCS_VAR_MANAGED){
         checkManagedVar(self,decl,finishDecl);
      }else if(type==MTCS_VAR_CONSTANT){
         checkConstantVar(self,decl,finishDecl);
      }else if(type==MTCS_VAR_DEVICE){
         checkDeviceVar(self,decl,finishDecl);
      }
      //检查变量大小
      checkVarSize(self,decl,finishDecl);
      DECL_PRESERVE_P (decl) = 1;
      TREE_USED(decl)=1;
      if(self->enterFuncBody && !(DECL_EXTERNAL(decl) ||  TREE_PUBLIC(decl)) && type==MTCS_VAR_SHARED && finishDecl){
         //记录局部变量是 shared managed constant 可能提升为全局变量
         bool vmP=variably_modified_type_p (TREE_TYPE (decl), current_function_decl);
         n_debug("mtcsparser.c 声明是一个共享变量并在mtcs函数内 是局部变量，可能要提升。类型是:%d vmP:%d\n",type,vmP);
         if(vmP){
            //大多数情况不能提升,特别处理类型是 ARRAY_TYPE
            nboolean ret=FALSE;
            if (TREE_CODE (TREE_TYPE (decl)) == ARRAY_TYPE){
               ret=replaceArrayTypeDomain(decl);
               if(ret)
                  relayout_decl(decl);   //关键 重新计算布局
            }
            if(!ret)
               error_at(DECL_SOURCE_LOCATION(decl),"数组必须有一个常数变量%qD",decl);
         }

         TREE_STATIC(decl)=1;//promote
         DECL_PRESERVE_P (decl) = 1;//重要，否则生成的vnode的force_output是0
        // tree asmName = DECL_ASSEMBLER_NAME (decl);
         setAssembleName(self,decl);
         PromoteSharedVar *item=n_slice_new(PromoteSharedVar);
         item->id=self->mtcsPromoteLocalVarArray->len;
         item->hostDeclAddress = decl;
         n_ptr_array_add(self->mtcsPromoteLocalVarArray,item);
         tree call = createSharedCall(self,DECL_SOURCE_LOCATION(decl),item->id,0);
         n_debug("mtcsparser.c 声明是一个变量并在mtcs函数内 00 是局部变量，可能要提升。  类型是:%d hostdecl:%p\n",type,decl);
         call = add_stmt(call);
      }else if(self->enterFuncBody && TREE_STATIC(decl) && type==MTCS_VAR_MANAGED && finishDecl){
         n_debug("mtcsparser.c 声明是一个托管变量并在mtcs函数内 是静态变量，设其设置汇编名。 类型是:%d\n",type);
         DECL_PRESERVE_P (decl) = 1;//重要，否则生成的vnode的force_output是0
         //  tree asmName = DECL_ASSEMBLER_NAME (decl);
         setAssembleName(self,decl);
      }else if(self->enterFuncBody && TREE_STATIC(decl) && type==MTCS_VAR_CONSTANT && finishDecl){
         n_debug("mtcsparser.c 声明是一个constant并在mtcs函数内 是静态变量，设其设置汇编名。 类型是:%d\n",type);
         DECL_PRESERVE_P (decl) = 1;//重要，否则生成的vnode的force_output是0
         setAssembleName(self,decl);
      }else if(self->enterFuncBody && TREE_STATIC(decl) && type==MTCS_VAR_DEVICE && finishDecl){
         n_debug("mtcsparser.c 声明是一个device并在mtcs函数内 是静态变量，设其设置汇编名。 类型是:%d\n",type);
         DECL_PRESERVE_P (decl) = 1;//重要，否则生成的vnode的force_output是0
         setAssembleName(self,decl);
      }
   }
}

/**
 * 加入内建函数声明
 */
char *mtcs_parser_add_buitlins_tree(MtcsParser *self)
{
   self->haveMtcsFunc = func_mgr_have_mtcs_func(func_mgr_get());
   if(self->haveMtcsFunc || self->haveMtcsVar)
      return mtcs_builtin_tree_create_builtins_decl(self->mtcsBuiltinTree);
   return NULL;
}

//如果是mtcs内部变量，比如 matDim,unitDim,unitIdx,threadIdx
tree   mtcs_parser_vars_parser(MtcsParser *self,location_t loc,tree id)
{
   return mtcs_builtin_tree_parser(self->mtcsBuiltinTree,loc,id);
}

/**
 * 解析 <<<...,...>>>中的参数
 *共有14种参数组合
 */
void mtcs_parser_parser_launch_param(MtcsParser *self,vec<tree, va_gc> **lanuchList)
{
   mtcs_lanuch_parser_launch_param(self->mtcsLanuch,lanuchList);
}

/**
 * 把>>>替换为),这样可以用 c_parser_expr_list 方法把<<<...>>>中的内容处理成参数
 */
void mtcs_parser_replace_rshift_and_greater(MtcsParser *self)
{
   c_parser *parser=self->aetParser->parser;
   c_parser_consume_token (parser);//consume >>
   c_parser_consume_token (parser);//consume >
   int tokenCount=parser->tokens_avail;
   if(tokenCount+1>AET_MAX_TOKEN){
      error("token太多了");
      return ;
   }
   int i;
   for(i=tokenCount;i>0;i--){
      aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
   }

   aet_utils_create_token(&parser->tokens[0],CPP_CLOSE_PAREN,")",1);
   parser->tokens_avail=tokenCount+1;
   aet_print_token_in_parser("mtcs replaceToken ------");
}

/**
 * 检查 mtcs变量被修改。
 */
void mtcs_parser_modify_check(MtcsParser *self,location_t loc,tree lhs,tree rhs)
{
   if(lhs && VAR_P(lhs)){
      if(!self->enterFuncBody && current_function_decl){
         MtcsVarType type = mtcs_info_get_and_check_var_type(lhs,DECL_SOURCE_LOCATION(lhs));
         if(type==MTCS_VAR_SHARED){
           warning_at(loc,0,"__shared__ 共享变量%qD不能直接在主机函数内写入。",lhs);
         }
      }
   }
   if(rhs && VAR_P(rhs)){
      if(!self->enterFuncBody && current_function_decl){
           MtcsVarType type =  mtcs_info_get_and_check_var_type(rhs,DECL_SOURCE_LOCATION(rhs));
           if(type==MTCS_VAR_SHARED){
              //先进入 mtcs_parser_postfix_expression 才会到 modify
            // warning_at(loc,0,"变量%qD不能直接在主机函数内读入。",rhs);
           }
       }
   }
}

/**
 * 在aetparser.c的方法 c_parser_postfix_expression中调用
 */
void mtcs_parser_postfix_expression (MtcsParser *self,location_t loc,tree value)
{
   if(self->aetParser->isAet &&  self->enterFuncBody && VAR_P(value)){
      //检查变量是不是全局的
      if(DECL_EXTERNAL(value) || TREE_PUBLIC(value) || TREE_STATIC(value)  ){
         MtcsVarType varType =  mtcs_info_get_var_type(value);
         if(varType==MTCS_VAR_NOT){
            error_at(loc,"不是设备类型变量%qD，使用 __shared__、__managed__、__constant__修饰。",value);
         }
      }
   }else if(!self->enterFuncBody && VAR_P(value) && current_function_decl){
      //不在mtcs函数内，并且是变量
      c_parser *parser=self->aetParser->parser;
      if(!c_parser_next_token_is (parser, CPP_EQ)){
         MtcsVarType type = mtcs_info_get_and_check_var_type(value,DECL_SOURCE_LOCATION(value));
         if(type==MTCS_VAR_SHARED){
            warning_at(loc,0,"__shared__ 共享变量%qD不能直接在主机函数内读入或写入。",value);
         }else if(type==MTCS_VAR_CONSTANT){
            warning_at(loc,0,"__constant__ 共享变量%qD不能直接在主机函数内读入。",value);
         }
      }
   }
}

static tree getRtn(tree funcOrField,nboolean isField)
{
    tree rtn=NULL_TREE;
    rtn=TREE_TYPE(funcOrField);
    rtn=TREE_TYPE(rtn);
    return isField?TREE_TYPE(rtn):rtn;
}


////////////////////////-------生成核函数同位体字符串变量----------------------

static char * getCanAssignFunc(NPtrArray *childFuncsArray,ClassFunc *parentItem,char *parentSysName,char *childSysName)
{
   int i=0;
   for(i=0;i<childFuncsArray->len;i++){
      ClassFunc *childItem=(ClassFunc *)n_ptr_array_index(childFuncsArray,i);
      if(strcmp(childItem->rawMangleName,parentItem->rawMangleName)==0 &&
            aet_utils_valid_tree(childItem->fromImplDefine) && !childItem->isAbstract && class_func_is_mtcs(childItem)){
         n_debug("类的func定义与接口的函数名是相同的。 index:%d %s %s\n", i,parentItem->mangleFunName,childItem->mangleFunName);
         tree rtn=getRtn(childItem->fromImplDefine,FALSE);
         tree parentRtn=getRtn(parentItem->fieldDecl,TRUE);
         nboolean returnValue=c_tree_equal (rtn,parentRtn);
         if(!returnValue){
            location_t ploc = DECL_SOURCE_LOCATION (childItem->fromImplDefine);
            error_at(ploc,"aa---父类%qs声明的方法%qs与子类%qs实现的方法相同，但返回值不同。",parentSysName,parentItem->orgiName,childSysName);
            return NULL;
         }
         return childItem->mangleFunName;
      }
   }
   return NULL;
}

static void assignToParent(MtcsParser *self,ClassName *className,ClassName *parent,NString *codes)
{
   if(parent==NULL)
      return ;
   NPtrArray    *childFuncsArray=func_mgr_get_funcs(func_mgr_get(),className);
   ClassInfo    *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parent);
   NPtrArray    *parentFuncsArray=func_mgr_get_funcs(func_mgr_get(),parent);
   /**给父类的域声明赋值*/
   int i;
   for(i=0;i<parentFuncsArray->len;i++){
      ClassFunc *parentFunc=n_ptr_array_index(parentFuncsArray,i);
      if(aet_utils_valid_tree(parentFunc->fieldDecl) && class_func_is_mtcs(parentFunc)){
         char *implDefine=getCanAssignFunc(childFuncsArray,parentFunc,parent->sysName,className->sysName);
         if(implDefine!=NULL){
            char varName[512];
            mtcs_info_create_kernel_name(parentFunc->mangleFunName,varName);
            n_string_append_printf(codes,"\t((%s *)self)->%s=\"%s\";\n",parent->sysName,varName,implDefine);
         }
      }
   }

   /**给父类的接口赋值*/
   for(i=0;i<parentInfo->ifaceCount;i++){
      ClassName *iface=&(parentInfo->ifaces[i]);
      NPtrArray    *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
      char ifaceVarName[255];
      aet_utils_create_in_class_iface_var(iface->userName,ifaceVarName);//一定与classparser中创建的接口变量名相同
      int j;
      for(j=0;j<ifaceFuncsArray->len;j++){
         ClassFunc *ifaceFunc=n_ptr_array_index(ifaceFuncsArray,j);
         if(aet_utils_valid_tree(ifaceFunc->fieldDecl) && class_func_is_mtcs(ifaceFunc)){
            char *implDefine=getCanAssignFunc(childFuncsArray,ifaceFunc,iface->sysName,className->sysName);
            if(implDefine!=NULL){
               char varName[512];
               mtcs_info_create_kernel_name(ifaceFunc->mangleFunName,varName);
               //n_string_append_printf(codes,"(&((%s *)self)->%s)->%s=%s;\n",parent->sysName,ifaceVarName,func->mangleFunName,implDefine);
               n_string_append_printf(codes,"\t((%s *)self)->%s=\"%s\";\n",parent->sysName,varName,implDefine);
            }
         }
      }
   }

   if(parentInfo->parentName.sysName==NULL)
      return ;
   assignToParent(self,className,&parentInfo->parentName,codes);
}

//----------------------赋值设备函数到函数指针变量--------------------------

/**
 * 为AObject创建变量 AET_MTCS_PLATFORM_TYPE_VAR_NAME
 * 该变量的值来自 new对象时，用户传入的类型 <<<cuda>>>
 * XXX *obj=new$ XXX<<<cuda>>>();
 */
tree mtcs_parser_create_platform_type_var(MtcsParser *self,location_t loc)
{
   char *varName=AET_MTCS_PLATFORM_TYPE_VAR_NAME;
   tree id=aet_utils_create_ident(varName);
   tree decl = build_decl (loc,FIELD_DECL,id, integer_type_node);
   finish_decl (decl, loc, NULL_TREE, NULL_TREE, NULL_TREE);
   return decl;
}

/**
 * 创建全局设备函数地址变量以及初始值。
 *static __device__ void *_TFirst_deviceFuncPointers[]={_Z6TFirst10testkernelEPN6TFirstE};
 */
char *mtcs_parser_create_device_func_pointers_var(MtcsParser *self,ClassName *className)
{
   NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
   int i;
   int count=0;
   if(funcArray==NULL || funcArray->len==0)
      return NULL;
   NString *codes=n_string_new("");
   char varName[256];
   mtcs_info_create_device_func_pointer_var_name(varName,className->sysName);

   n_string_append_printf(codes,"static __device__ __attribute__ ((__used__)) void *%s[]={",varName);

   for(i=0;i<funcArray->len;i++){
      ClassFunc *func=n_ptr_array_index(funcArray,i);
      if(class_func_is_device(func) && aet_utils_valid_tree(func->fromImplDefine)){
         if(count>0)
            n_string_append(codes,",");
         n_string_append_printf(codes,"%s",func->mangleFunName);
         count++;
      }
   }

   n_string_append(codes,"};\n");
   if(count==0){
      n_string_free(codes,TRUE);
      return NULL;
   }
   return n_string_free(codes,FALSE);
}

/**
 * 在编译temp_func_track_45.c时调用这里
 * class_parser_goto-->mtcs_parser_link_func
 */
void mtcs_parser_link_func(MtcsParser *self,char *objRootPath,NPtrArray **arrays,int length,int pos)
{
   mtcs_link_link(self->mtcsLink,objRootPath,arrays,length,pos);
}

MtcsParser *mtcs_parser_get()
{
   static MtcsParser *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(MtcsParser));
      mtcsParserInit(singleton);
   }
   return singleton;
}
