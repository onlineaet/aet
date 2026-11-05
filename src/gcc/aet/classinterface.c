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
#include "tree-iterator.h"
#include "tree-cfg.h"
#include "tree-pretty-print.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"
#include "gimplify.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "classmgr.h"
#include "classinterface.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "genericimpl.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "genericmodel.h"
#include "classutil.h"

/**
 * 实现接口的方法
 * 1.在实现类的最后也就是退出”};”时
 * 2.检查如果类定义了实现接口如：Implements OpenDoor
 * 3.在实现类中的所有定义方法(ClassFunc fromImplDefine有效)中查找区匹配接口声明的方法
 *  匹配指: 接口与类的原函数名相同，参数除第一个（也就是self）外其它都相同（判断见getRemainParmString）
 *  类定义的方法与接口声明的函数返回值相同(compareFunctionType)
 * 4.不能用接口的函数指针指到类定义的方法上，因为类中实现每个参数是类的self,不是接口的self,所以要创建一个函数定义来转换
 * 5.类中的方法参数符号如何取得后传给要创建的函数是一个难点，这样转一道，性能也会有损失
 * 6.取参数符号见writeToFile
 * 7.在类实现中创建如下原代码_from_Abc表示从那个类实现的，之前的_Z5Zhong7txyopenEPN6_ZhongEw是接口的函数声明
 * static void  _Z5Zhong7txyopenEPN6_ZhongEw_from_Abc(Zhong * self,int parm1)
 *{
 *    Abc * randAbc123=(Abc *)container_of(self,Abc,ifaceZhong178827062);//把接口转成类
 *   _Z3Abc7txyopenEPN4_AbcEw(randAbc123,parm1);//这是类中定义的函数
 *}
 * 最后在类的初始化方法中把接口的函数指针指向_Z5Zhong7txyopenEPN6_ZhongEw_from_Abc
*Abc * Abc_init_1234ergR5678_Abc(Abc *self)
*{
*   AObject_init_1234ergR5678_AObject((AObject *)self);
*   self->_Z3Abc3AbcEPN4_AbcE=_Z3Abc3AbcEPN4_AbcE;
*   ((Zhong *)&self->ifaceZhong178827062)->_Z5Zhong7txyopenEPN6_ZhongEw=_Z5Zhong7txyopenEPN6_ZhongEw_from_Abc;
*   return self;
*}
*ifaceZhong178827062 系统创建的类中接口变量名，统一由aet_utils_create_in_class_iface_var创建。
*/

static void classInterfaceInit(ClassInterface *self)
{
}

/**
 * 产生式 id,id
 * 解析 implements$ Iface1,Iface2
 */
int  class_interface_parser(ClassInterface *self,ClassInfo *info,char **interface)
{
   c_parser *parser=self->parser->parser;
   int i;
   int count=0;
   for(i=0;i<20;i++){
      if (c_parser_next_token_is (parser, CPP_NAME)){
         tree id=c_parser_peek_token(parser)->value;
         c_parser_consume_token (parser);
         interface[count++]=n_strdup(IDENTIFIER_POINTER(id));
         if(c_parser_next_token_is (parser, CPP_LESS)){
            ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),IDENTIFIER_POINTER(id));
            if(className==NULL)
               n_error("找不到接口");
            GenericModel *model=generic_model_new(TRUE,GEN_FROM_CLASS_DECL);
            class_info_add_iface_generic_model(info,className,model);
         }

      }else if(c_parser_next_token_is (parser, CPP_COMMA)){
         c_parser_consume_token (parser);
         if (!c_parser_next_token_is (parser, CPP_NAME))
            error("逗号后是接口名。");
         tree id=c_parser_peek_token(parser)->value;
         c_parser_consume_token (parser);
         interface[count++]=n_strdup(IDENTIFIER_POINTER(id));
      }else if (c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
         break;
      }else{
         error("出错了。");
      }
      if(count>AET_MAX_INTERFACE){
         error("实现的接口不能超过%qd个。",AET_MAX_INTERFACE);
         return 0;
      }
   }
   for(i=0;i<count;i++){
      n_debug("interface is :%d %s\n",i,interface[i]);
   }
   return count;
}


/////////////////////////////////////新实现----------------------


static void eachInterface(ClassInterface *self,NString *codes,ClassName *from,ClassName *iface)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),iface);
   NPtrArray *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
   if(ifaceFuncsArray==NULL || ifaceFuncsArray->len==0)
      return NULL;
   char ifaceVar[255];
   aet_utils_create_in_class_iface_var(iface->userName,ifaceVar);//一定与classparser中创建的接口变量名相同
   int i=0;
   for(i=0;i<ifaceFuncsArray->len;i++){
      ClassFunc *interfaceMethod=(ClassFunc *)n_ptr_array_index(ifaceFuncsArray,i);
      if(class_func_is_interface_reserve(interfaceMethod)) //是接口需要保留的ref和unref
         continue;
      ClassFunc *func=func_mgr_get_func_by_raw_mangle(func_mgr_get(),from,interfaceMethod->rawMangleName);
      if(!func){
         n_error("接口%s函数%s没有在实现类%s中找到",iface->sysName,interfaceMethod->orgiName,from->sysName);
      }
      //n_string_append_printf(codes,"\t((%s *)&self->%s)->%s=self->%s;\n",
           //  iface->sysName,ifaceVar,interfaceMethod->mangleFunName,func->mangleFunName);
      n_string_append_printf(codes,"\tself->%s.%s=self->%s;\n",
                ifaceVar,interfaceMethod->mangleFunName,func->mangleFunName);
   }
   n_debug("class_interface 获取的接口赋值原代码：%s\n",codes->str);
}


/**
 * 创建接口方法赋值代码
 * Create interface method assignment code
 *并加入代码字符串到数组中
 *如:
 *((test_RandomGenerator *)&self->ifaceRandomGenerator2066046634)->_Z15RandomGenerator7nextIntEPN15RandomGeneratorE=_Z7ARandom7nextIntEPN7ARandomE;
 *最终加入到初始化代码中
 *void * test_ARandom_init_1234ergR5678_test_ARandom(test_ARandom *self)
 *
 */

/**
 * 生成代码:
 * self->ifaceRandomGenerator2066046634._iface_common_var._atClass1234=(void*)self;
 */
static void assignmentAtClass(ClassInfo *info,NString *codes)
{
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName iface=info->ifaces[i];
      ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
      char ifaceVar[255];
      aet_utils_create_in_class_iface_var(faceInfo->className.userName,ifaceVar);//一定与classparser中创建的接口变量名相同
      n_string_append_printf(codes,"\tself->%s.%s = (AObject *)self;\n",ifaceVar,IFACE_AT_CLASS);
   }
}

static char *getRefOrUnrefMangleName(ClassInfo *info,char *refOrUnref)
{
	NPtrArray  *array=func_mgr_get_funcs(func_mgr_get(),&info->className);
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *func=n_ptr_array_index(array,i);
		if(strcmp(func->orgiName,refOrUnref)==0)
			return func->mangleFunName;
	}
	return NULL;
}

/**
 * 给接口变量AET_MAGIC_NAME设置魔数AET_MAGIC_NAME_VALUE
 * 接口的magic是AET_MAGIC_NAME_VALUE+1
 */
static void assignmentMagic(ClassInfo *info,NString *codes)
{
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName iface=info->ifaces[i];
      ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
      char ifaceVar[255];
      aet_utils_create_in_class_iface_var(faceInfo->className.userName,ifaceVar);//一定与classparser中创建的接口变量名相同
      n_string_append_printf(codes,"\tself->%s.%s = %d;\n",
            ifaceVar,AET_MAGIC_NAME,AET_IFACE_MAGIC_NAME_VALUE);
   }
}

/**
 * 在classimpl最后把接口的方法找到可以给它赋值的类方法。
 */
char *class_interface_create_codes(ClassInterface *self,ClassName *className)
{
   if(class_mgr_is_interface(class_mgr_get(),className))
	   return NULL;
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info==NULL || info->ifaceCount<=0)
	   return NULL;
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<info->ifaceCount;i++){
	   ClassName iface=info->ifaces[i];
	   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&iface);
	   if(faceInfo==NULL){
	      error("在类%qs中实现接口%qs,但没有找到接口的声明。检查是否包含对应的头文件",className->userName,iface.userName);
		   return NULL;
	   }
	   eachInterface(self,codes,className,&iface);
   }

   //给接口设魔数    self->_aet_magic$_123=1725348961;
   assignmentMagic(info,codes);
   //给接口的变量 self->_atClass123=(void *)self;
   assignmentAtClass(info,codes);
   if(codes->len==0){
      n_string_free(codes,TRUE);
      return NULL;
   }
   return n_string_free(codes,FALSE);
}

/**
 * 判断是不是接口调用了ref方法
 */
static nboolean isCallRefOrUnrefByInterface(tree component,tree expr,char *refOrUnref)
{
  tree type=TREE_TYPE(expr);
  char *sysName=class_util_get_class_name(type);
  if(sysName!=NULL){
	  ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	  if(info && class_info_is_interface(info)
	  && TREE_CODE(component)==IDENTIFIER_NODE
	  && !strcmp(refOrUnref,IDENTIFIER_POINTER(component))){
		return TRUE;
	  }
  }
  return FALSE;
}

//是一个接口中声明的host device函数吗
nboolean class_interface_is_host_device_func(ClassInterface *self,ClassInfo *info,tree decls)
{
   if(info->type!=CLASS_TYPE_INTERFACE)
      return FALSE;
   //如果decls是在接口中声明的 host device方法，复制一份
   ClassFunc *func=  func_mgr_get_func(func_mgr_get(),decls);
   if(func!=NULL){
      if(!class_func_is_interface_reserve(func) && class_func_is_device(func) && class_func_is_host(func)){
         //这是一个 __host__ __device__ 接口方法，加入新的方法设为设备
         return TRUE;
      }
   }
   return FALSE;
}

//分裂接口的host device方法
tree class_interface_divide_host_device_func(ClassInterface *self,location_t loc,ClassInfo *info,tree decls)
{
   if(info->type!=CLASS_TYPE_INTERFACE)
      return NULL_TREE;
   //如果decls是在接口中声明的 host device方法，复制一份
   ClassFunc *func=  func_mgr_get_func(func_mgr_get(),decls);
   if(func==NULL)
      return NULL_TREE;

   if(!class_func_is_interface_reserve(func) && class_func_is_device(func) && class_func_is_host(func)){
      //这是一个 __host__ __device__ 接口方法，加入新的方法设为设备
      return func_mgr_divide_host_device_func(func_mgr_get(),loc,info,decls);
   }
   return NULL_TREE;
}


/**
 * Interface$ Face{
 *   void setData();
 * };
 * Class$ A implements$ Inter{
 *    ....
 * };
 * 变成
  * Class$ A implements$ Inter{
 *    void setData();
 * };
 * 把接口的方法加到实现接口的类中。
 */

static tree  copyInterfaceFiled(location_t loc,char *origName,tree interfaceFieldDecl,ClassInfo *classInfo)
{
   tree fieldType=TREE_TYPE(interfaceFieldDecl);
   tree fieldFunType=TREE_TYPE(fieldType);
   tree fieldReturnType=TREE_TYPE(fieldFunType);
   int count=0;
   tree paramTypes[30];
   for (tree al = TYPE_ARG_TYPES (fieldFunType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      paramTypes[count++]=type;
   }

   //这里 classInfo的 record recordTypeDecl还是空的。
   tree typedecl=lookup_name(get_identifier(classInfo->className.sysName));
   tree classRecord=TREE_TYPE(typedecl);
   paramTypes[0]=build_pointer_type(classRecord);
   tree param_type_list = NULL;
   int i;
   for(i=0;i<count;i++)
      param_type_list = tree_cons (NULL_TREE, paramTypes[i], param_type_list);
   //param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);//在函数声明的最后一个参数必须是void_type_node
   param_type_list = nreverse (param_type_list);
   tree fntype = build_function_type (fieldReturnType, param_type_list);
   tree decl = build_decl (loc,FIELD_DECL,get_identifier(origName),build_pointer_type(fntype));
   return decl;
}


int class_interface_add_to_class(ClassInterface *self,location_t loc,ClassInfo *info,tree *classMethods)
{
     /*本类的接口方法个数*/
     int i;
     int count=0;
     for(i=0;i<info->ifaceCount;i++){
        ClassName *iface=&(info->ifaces[i]);
        NPtrArray *funcArray=func_mgr_get_funcs(func_mgr_get(),iface);
        if(funcArray!=NULL){
           int j;
           for(j=0;j<funcArray->len;j++){
              ClassFunc *interfaceMethod=n_ptr_array_index(funcArray,j);
              if(class_func_is_interface_reserve(interfaceMethod)) //是接口需要保留的ref和unref
                    continue;
              ClassFunc *func =func_mgr_get_func_by_raw_mangle(func_mgr_get(),
                    &info->className,interfaceMethod->rawMangleName);
              if(func!=NULL){
                 n_info("重复的接口方法:接口:%s 函数名:%s 已在接口%s中声明。\n",
                       iface->sysName,func->orgiName,func->className->sysName);
                 continue;
              }
              //复制一份接口中的方法。
              tree decl= copyInterfaceFiled(loc,interfaceMethod->orgiName, interfaceMethod->fieldDecl,info);
              char *result[5];
              func_mgr_create_mangle_name(func_mgr_get(),decl,&info->className,result,interfaceMethod->parms);
              DECL_NAME(decl)=get_identifier(result[1]);
              finish_decl (decl, loc, NULL_TREE, NULL_TREE, NULL_TREE);
              ClassFunc  *cloneFunc=class_func_clone(interfaceMethod,decl,result,info->record,&info->className);
              n_debug("new name:%s %s %s %s\n",
                    cloneFunc->orgiName,cloneFunc->mangleFunName,cloneFunc->mangleNoSelfName,cloneFunc->rawMangleName);
              n_debug("old name:%s %s %s %s\n",
                    interfaceMethod->orgiName,interfaceMethod->mangleFunName,interfaceMethod->mangleNoSelfName,interfaceMethod->rawMangleName);
              cloneFunc->fromInterface = TRUE;
              func_mgr_add(func_mgr_get(),cloneFunc);
              classMethods[count++]=decl;
              //说明是一个由host device分裂函数。它的分裂源应该在本类中，而不是接口。
              if(class_func_is_divide(cloneFunc)){
                //divideSrc是接口的分裂源。
                 ClassFunc *divideSrc=class_func_get_divide_src(cloneFunc);
                 if(divideSrc==NULL){
                    n_error("应该有接口的分裂源 %s\n",cloneFunc->mangleFunName);
                 }
                 //找出在本类中名字是 rawManagleFunc的ClassFunc;
                 n_debug("类:%s 中找名字是:%s的func\n",info->className.sysName,divideSrc->rawMangleName);
                 ClassFunc  *src=func_mgr_get_func_by_raw_mangle(func_mgr_get(),info->className.sysName,divideSrc->rawMangleName);
                 if(!src){
                    n_error("应该有接口的分裂源 应该已复制到本类中 %s %s\n",info->className.sysName,cloneFunc->mangleFunName);
                 }
                 class_func_set_divide(cloneFunc,TRUE,src);
              }
           }
        }
     }
     return count;
}

ClassInterface *class_interface_new()
{
	ClassInterface *self = n_slice_alloc0 (sizeof(ClassInterface));
	classInterfaceInit(self);
	self->parser = aet_parser_get();
	return self;
}

//
//int _aet_magic$_123;
//AObject *_atClass123;
void class_interface_add_sys_field(ClassInterface *self,ClassName *iface,NString *codes)
{
   n_string_append_printf(codes,"int %s;\n",AET_MAGIC_NAME);
   n_string_append_printf(codes,"AObject * %s;\n",IFACE_AT_CLASS);
}


/*
 * 创建：
 *
 * static Iface *ref(Iface *iface)
 * {
 *     iface->_atClass123->ref();
 *     return iface;
 * }
 *
 * ifaceType       : Iface 的 RECORD_TYPE
 * atClassField    : Iface::_atClass123 的 FIELD_DECL
 * refMethodField  : AObject::ref 的函数指针 FIELD_DECL
 */
static tree  createIfaceRef (location_t loc,char *funcName,tree ifaceType, tree atClassField, tree refMethodField)
{
   /* -------------------------------------------------
   1. 准备类型
   ------------------------------------------------- */

   tree ifacePtrType = build_pointer_type (ifaceType);
   tree aobjectPtrType = TREE_TYPE (atClassField);
   /*
   * ref 函数的类型：
   * static Iface *ref(Iface *iface)
   */
   tree argTypes = tree_cons (NULL_TREE, ifacePtrType, void_list_node);
   tree funcType = build_function_type (ifacePtrType, argTypes);

   /* -------------------------------------------------
   2. 创建 FUNCTION_DECL
   ------------------------------------------------- */
   tree fndecl =  build_decl (loc, FUNCTION_DECL, get_identifier (funcName), funcType);
   TREE_STATIC (fndecl) = 1;
   TREE_PUBLIC (fndecl) = 0;
   DECL_EXTERNAL (fndecl) = 0;
   DECL_ARTIFICIAL (fndecl) = 1;
   DECL_CONTEXT (fndecl) = NULL_TREE;

   /* -------------------------------------------------
   3. 创建参数 iface
   ------------------------------------------------- */
   tree parm = build_decl (loc, PARM_DECL, get_identifier ("iface"),ifacePtrType);
   DECL_CONTEXT (parm) = fndecl;
   DECL_ARG_TYPE (parm) = ifacePtrType;
   DECL_ARGUMENTS (fndecl) = parm;

   /* -------------------------------------------------
   4. 创建返回值 RESULT_DECL
   ------------------------------------------------- */

   tree resultDecl = build_decl (loc, RESULT_DECL, NULL_TREE, ifacePtrType);
   DECL_CONTEXT (resultDecl) = fndecl;
   DECL_RESULT (fndecl) = resultDecl;

   /* -------------------------------------------------
   5. 构造 iface->_atClass123
   ------------------------------------------------- */

   /*
   * (*iface)
   */
   tree ifaceDeref = build_indirect_ref (loc, parm, RO_ARROW);
   /*
   * (*iface)._atClass123
   *
   * 类型：AObject *
   */
   tree aobjectPtr =  build_component_ref (loc,ifaceDeref,DECL_NAME(atClassField),loc,loc);

   if (aobjectPtr == error_mark_node)
      return error_mark_node;

   /* -------------------------------------------------
   6. 构造 iface->_atClass123->ref()
   ------------------------------------------------- */

   /*
   * *(iface->_atClass123)
   */
   tree aobjectDeref = build_indirect_ref (loc, aobjectPtr, RO_ARROW);

   /*
   * (*aobjectPtr).ref
   *
   * refMethodField 是 AObject 中的函数指针字段。
   */
   tree refFunc = build_component_ref (loc,aobjectDeref,DECL_NAME(refMethodField),loc,loc);

   if (refFunc == error_mark_node)
      return error_mark_node;

   /*
   * 调用参数：AObject *
   */
   tree callArgs[1];
   callArgs[0] = aobjectPtr;
   /*
   * ref() 的返回类型使用函数指针类型中的返回类型。
   */
   tree refFuncType = TREE_TYPE (refMethodField);

   tree refReturnType =TREE_TYPE (TREE_TYPE (refFuncType));

   tree callExpr = build_call_array_loc (loc,refReturnType,refFunc,1,callArgs);

   /* -------------------------------------------------
   7. 构造 return iface
   ------------------------------------------------- */
   /*
   * RESULT_DECL = iface
   */
   tree resultAssign = build2_loc (loc,MODIFY_EXPR,ifacePtrType,resultDecl,parm);

   /*
   * RETURN_EXPR
   */
   tree returnExpr = build1_loc (loc,RETURN_EXPR,void_type_node,resultAssign);

   /* -------------------------------------------------
   8. 构造函数体
   ------------------------------------------------- */
   tree stmtList = alloc_stmt_list ();
   /*
   * iface->_atClass123->ref();
   */
   append_to_statement_list (callExpr, &stmtList);

   /*
   * return iface;
   */
   append_to_statement_list (returnExpr, &stmtList);
   tree block = make_node (BLOCK);

   /* 创建最外层 BLOCK（必须） */
   BLOCK_VARS (block) = NULL_TREE;
   BLOCK_SUBBLOCKS (block) = NULL_TREE;
   BLOCK_SUPERCONTEXT (block) = fndecl;     // 必须
   BLOCK_CHAIN (block) = NULL_TREE;

   tree bindExpr =  build3_loc (loc,BIND_EXPR,void_type_node,NULL_TREE,stmtList,block);

   DECL_SAVED_TREE (fndecl) = bindExpr;
   DECL_INITIAL (fndecl) = block;           // ★ 关键！没有这句就会在 lower_function_body 崩
   /* -------------------------------------------------
   9. 注册函数
   ------------------------------------------------- */
   struct function *saved_cfun = cfun;
   tree saved_fndecl = current_function_decl;

   /* 7. （可选但推荐）让 GCC 后续能正确处理这个函数 */
   allocate_struct_function (fndecl, false);
   set_cfun (NULL, true);
   current_function_decl = NULL_TREE;

   /* 切换到新函数 */
   push_cfun (DECL_STRUCT_FUNCTION (fndecl));
   gimplify_function_tree (fndecl);
   pop_cfun ();
   cgraph_node *node = cgraph_node::get_create (fndecl);
   cgraph_node::finalize_function (fndecl, false);
   set_cfun (saved_cfun);
   current_function_decl = saved_fndecl;
   return fndecl;
}



/**
 * 每个接口都动态的创建一个ref函数定义
 * 函数名是:_iface_123
 */
tree class_interface_create_ref_function_define(ClassInterface *self,location_t loc,ClassName *className,char *funcName)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(!class_info_is_interface(info) || strcmp(funcName,"ref"))
      return NULL_TREE;

   char funcname[255];
   sprintf(funcname,"_%s_123",className->sysName);
   tree fndecl = lookup_name(get_identifier(funcname));
   if(fndecl)
      return fndecl;

   tree iface =lookup_name(get_identifier(className->sysName));
   tree ifacetype = TREE_TYPE(iface);
   tree atclassfield;
   for (atclassfield = TYPE_FIELDS (ifacetype); atclassfield; atclassfield = DECL_CHAIN (atclassfield)){
      char *name =  IDENTIFIER_POINTER(DECL_NAME(atclassfield));
      if(!strcmp(name,IFACE_AT_CLASS))
         break;
   }
   tree reffield = NULL_TREE;
   ClassInfo *rootinfo = class_mgr_get_root(class_mgr_get());
   NPtrArray    *arrays = func_mgr_get_funcs(func_mgr_get(),&rootinfo->className);
   for(int i=0;i<arrays->len;i++){
      ClassFunc *f=n_ptr_array_index(arrays,i);
      if(!strcmp(f->orgiName,"ref")){
         reffield = f->fieldDecl;
         break;
      }
   }
   fndecl = createIfaceRef (loc,funcname,ifacetype, atclassfield, reffield);
   return fndecl;
}



