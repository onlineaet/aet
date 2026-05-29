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
#include "gimple-expr.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"
#include "opts.h"

#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "classutil.h"
#include "genericblock.h"
#include "genericutil.h"
#include "funcmgr.h"
#include "classimpl.h"



static void genericBlockInit(GenericBlock *self)
{
	self->name=NULL;
	self->parms=NULL;
	self->body=NULL;
	self->returnType=NULL;
	self->className=NULL;
	self->isCompile=FALSE;
	self->isFuncGeneric=FALSE;//是不是泛型函数中的块
}

static char *getParmName(tree arg)
{
	if(TREE_CODE(arg)==VAR_DECL || TREE_CODE(arg)==PARM_DECL){
		tree id=DECL_NAME(arg);
		return IDENTIFIER_POINTER(id);
	}else if(TREE_CODE(arg)==ADDR_EXPR){
		tree vars=TREE_OPERAND (arg, 0);
		if(TREE_CODE(vars)==VAR_DECL){
			tree id=DECL_NAME(vars);
			return IDENTIFIER_POINTER(id);
		}
	}else if(TREE_CODE(arg)==COMPONENT_REF){
		tree field=TREE_OPERAND(arg,1);
		if(TREE_CODE(field)==FIELD_DECL){
			tree id=DECL_NAME(field);
			return IDENTIFIER_POINTER(id);
		}
	}else if(TREE_CODE(arg)==NOP_EXPR){
		tree op0=TREE_OPERAND (arg, 0);
		if(TREE_CODE(op0)==ADDR_EXPR){
			op0=TREE_OPERAND (op0, 0);
			if(TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL || TREE_CODE(op0)==FIELD_DECL){
				tree id=DECL_NAME(op0);
				return IDENTIFIER_POINTER(id);
			}
		}
	}else if(TREE_CODE(arg)==CALL_EXPR){
		//取一个随机的名字
		return NULL;
	}
	aet_print_tree_skip_debug(arg);
	error_at(input_location,"在genericblock中找不到参数名");
	return NULL;
}

/**
 * 参数的各种形式:
 * component_ref
 * 加参数FuncGenParmInfo *tempFgpi1234到第二个位置
 * var_decl 带有初始化的值
 */
void generic_block_set_parm(GenericBlock *self,vec<tree, va_gc> *exprlist)
{
   int ix;
   tree arg;
   NString *parmStr=n_string_new("");
   gcc_assert(exprlist!=NULL);
   char *parmName=NULL;
   for (ix = 0; exprlist->iterate (ix, &arg); ++ix){
      if(ix==0)
         parmName="self";
      else{
        parmName=getParmName(arg);
        if(parmName==NULL)
           parmName="temp";
      }
      char *typeStr=generic_util_get_type_str(arg);
      nboolean needFree=FALSE;
      if(typeStr==NULL){
         //试一下是不是 int zuiz[][5];//这种形式
         //typeStr="int";
         //parmName="zuiz[][5]";
         char *newParmName=NULL;
         nboolean ok=generic_util_get_array_type_and_parm_name(arg,&typeStr,&newParmName,parmName);
         if(ok){
            parmName=newParmName;
            needFree=TRUE;
         }else
            error_at(input_location,"在generic_block_set_parm出错。参数%qs找不到类型名。",parmName);
      }
      n_debug("generic_block_set_parm 00 有参数 %d %s %s\n",ix,typeStr,parmName);
      n_string_append(parmStr,typeStr);
      n_string_append(parmStr," ");
      n_string_append(parmStr,parmName);
      n_string_append(parmStr,",");
      n_free(typeStr);
      if(needFree)
         n_free(parmName);
   }

   if(n_string_ends_with(parmStr,","))
      self->parms=n_strndup(parmStr->str,parmStr->len-1);
   else
      self->parms=n_strdup(parmStr->str);
   n_string_free(parmStr,TRUE);
}

static tree getParmType(tree arg)
{
	tree type=TREE_TYPE(arg);
	if(TREE_CODE(type)==POINTER_TYPE){
		return getParmType(type);
	}else{
        tree typeName=TYPE_NAME(type);
        if(TREE_CODE(typeName)==TYPE_DECL){
        	tree parmType= TREE_TYPE(typeName);
        	return parmType;
        }else{
        	error("genericblock 未知类型");
        }
	}
	return NULL;
}



void generic_block_set_body(GenericBlock *self,char *source)
{
	self->body=n_strdup(source);
}

char  *generic_block_get_name(GenericBlock *self)
{
	return self->name;
}

void generic_block_set_loc(GenericBlock *self,location_t start,location_t end)
{
	self->startLoc=start;
	self->endLoc=end;
}

void generic_block_print(GenericBlock *self)
{
	n_debug("返回值:\n%s\n",self->returnType);
	n_debug("函数名: \n%s\n",self->name);
	n_debug("参数名: \n%s\n",self->parms);
	n_debug("函数体: \n%s\n",self->body);
}

/**
 * 获取块的源代码
 */
char *generic_block_create_codes(GenericBlock *self)
{
	NString *codes=n_string_new("");
	n_string_append_printf(codes," %s ",self->returnType);
	n_string_append(codes,self->name);
	n_string_append_printf(codes,"(%s)\n",self->parms);
	n_string_append(codes," {\n");
	n_string_append_printf(codes,"%s\n",self->body);
	n_string_append(codes," }\n");
	printf("block源代码是:\n%s\n",codes->str);
	return n_string_free(codes,FALSE);
}

/**
 * static xxxx_var *="belongFuncNEW_OBJECT_GENERIC_SPERATOR0NEW_OBJECT_GENERIC_SPERATORint Abc__inner_generic_func(Abc *self...."
 * 所属函数名，该函数是不是泛型函数
 * 以便保存在.o文件中，当生成库时可以从.so中取出
 */

#define BLOCK_START "block start:"
#define BLOCK_END   "block end:"

char *generic_block_create_save_codes(GenericBlock *self)
{
   NString *codes=n_string_new("");
   n_string_append(codes,BLOCK_START"\n");
   n_string_append_printf(codes,"%s\n",self->belongFunc);
   n_string_append_printf(codes,"%s\n",self->isFuncGeneric?"1":"0");
   n_string_append_printf(codes,"%d\n",self->index);
   n_string_append_printf(codes,"%s\n",self->returnType);
   n_string_append_printf(codes,"%s\n",self->name);
   n_string_append_printf(codes,"%s\n",self->parms);
   n_string_append_printf(codes,"%s\n",self->body);
   n_string_append(codes,BLOCK_END"\n");
   n_debug("保存block源代码是:\n%s\n",codes->str);
   return n_string_free(codes,FALSE);
}

/**
 * 从字符串读出GenericBlock;
 */
static NPtrArray *readBlock(char *buffer)
{
   NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
   char *c=buffer;
   while(strstr(c,BLOCK_START)){
      char *start=strstr(c,BLOCK_START);
      //printf("r0 is :%s\n",start);
      char *n=start+strlen(BLOCK_START)+1;//加1跳过 GENOBJ_START 后的\n号
    //  printf("r1 is :%s\n",n);
      char *end=strstr(n,BLOCK_END);
     // printf("r2 is :%s\n",end);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      //printf("readBlock :%s\n",ret);
      n_ptr_array_add(array,ret);
      c = end+strlen(BLOCK_END);
   }
   return array;
}

NPtrArray *generic_block_create_block(char *content)
{
   NPtrArray *array=readBlock(content);
   NPtrArray *blockArray=n_ptr_array_new();

   int i;
   for(i=0;i<array->len;i++){
      char *str=n_ptr_array_index(array,i);
     // printf("generic_block_create_block :%d %s\n",i,str);
      char **items=n_strsplit(str,"\n",7);
      GenericBlock *block=n_slice_new0(GenericBlock);
      block->belongFunc=n_strdup(items[0]);
      block->isFuncGeneric=atoi(items[1]);
      block->index=atoi(items[2]);
      block->returnType=n_strdup(items[3]);
      block->name=n_strdup(items[4]);
      block->parms=n_strdup(items[5]);
      block->body=n_strdup(items[6]);
      n_ptr_array_add(blockArray,block);
      n_strfreev(items);
   }
   n_ptr_array_unref(array);
   return blockArray;

}


void  generic_block_set_return_type(GenericBlock *self,tree lhs)
{
    if(!aet_utils_valid_tree(lhs)){
    	self->returnType=n_strdup("void");
    }else{
    	char *rnt=generic_util_get_type_str(lhs);
    	if(rnt==NULL)
    		error("在generic_block_set_return_type出错。");
    	self->returnType=rnt;
    }
}

void  generic_block_set_compile(GenericBlock *self,nboolean isCompile)
{
	self->isCompile=isCompile;
}

nboolean  generic_block_is_compile(GenericBlock *self)
{
	return self->isCompile;
}


nboolean generic_block_match_field_and_func(GenericBlock *self,tree funcdel,tree field)
{
	tree saved=DECL_SAVED_TREE (funcdel);
	if(!aet_utils_valid_tree(funcdel)){
		error_at(self->startLoc,"函数体不存在。%qs",self->name);
		return FALSE;
	}
	tree fieldRetn=TREE_TYPE(field);//得到的是pointer_type
	fieldRetn=TREE_TYPE(fieldRetn); //得到的是function_type
	fieldRetn=TREE_TYPE(fieldRetn); //得到的是retn
	printf("generic_block_check 11 %s\n",self->name);
	aet_print_tree(field);
	printf("generic_block_check 22 %s\n",self->name);
	aet_print_tree(fieldRetn);
	NPtrArray *array=n_ptr_array_new();
	c_aet_dump_return_expr(funcdel,array);
    tree expr=NULL_TREE;
	int i;
	for(i=array->len-1;i>=0;i--){
		expr=n_ptr_array_index(array,i);
		printf("return stamt 比较返回的表达式类型是否一样 :%d\n",i);
		aet_print_tree(expr);
	}
	n_ptr_array_unref(array);
	if(expr==NULL_TREE && TREE_CODE(fieldRetn)==VOID_TYPE)
		return TRUE;
	if(TREE_CODE(fieldRetn)==VOID_TYPE && expr!=NULL_TREE){
		//把field的返回类型改成 expr
		tree type=TREE_TYPE(expr);
		tree pointer=TREE_TYPE(field);//得到的是pointer_type
		tree functype=TREE_TYPE(pointer); //得到的是function_type
		TREE_TYPE(functype)=type;
		printf("把field的类型改成返回值的类型 %s\n",self->name);
		aet_print_tree(field);
        return TRUE;
	}
	if(TREE_CODE(fieldRetn)!=VOID_TYPE && expr==NULL_TREE){
		error_at(self->startLoc,"泛型块没有返回值。%qs",self->name);
		return FALSE;
	}
	if(TREE_CODE(fieldRetn)!=VOID_TYPE && expr!=NULL_TREE){
		tree type=TREE_TYPE(expr);
		nboolean re=c_tree_equal(fieldRetn,type);
		if(!re){
			error_at(self->startLoc,"泛型块的返回值与左表达式不匹配。%qs",self->name);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * 生成 typedef void (*func_1_typedecl)(MatrixOps *self,int abcts);
 * 要跳过self参数，它们在blockmgr中被加的。
 */

void generic_block_create_type_decl(GenericBlock *self,tree lhs,vec<tree, va_gc> *exprlist)
{
   char *fname=generic_util_create_block_func_type_decl_name(self->className->sysName,self->index);
   tree param_type_list = NULL;
   int ix;
   tree arg;
   for (ix = 0; exprlist->iterate (ix, &arg); ++ix){
      tree param_type=TREE_TYPE(arg);
      param_type_list = tree_cons (NULL_TREE, param_type, param_type_list);
   }

   param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);
   /* The 3 lists have been created in reverse order.  */
   param_type_list = nreverse (param_type_list);
   /* Build the function type.  */
   tree rtntype=void_type_node;
   if(aet_utils_valid_tree(lhs)){
      rtntype=TREE_TYPE(lhs);
   }
   tree fntype = build_function_type (rtntype, param_type_list);
   tree pointer=build_pointer_type(fntype);
   tree typeNameId=aet_utils_create_ident(fname);
   tree decl = build_decl (input_location, TYPE_DECL, typeNameId, pointer);
   //TYPE_NAME(pointer2)=decl;
   DECL_ARTIFICIAL (decl) = 1;
   DECL_CONTEXT(decl)=NULL_TREE;
   DECL_EXTERNAL(decl)=0;
   TREE_STATIC(decl)=0;
   TREE_PUBLIC(decl)=0;
   set_underlying_type (decl);
   record_locally_defined_typedef (decl);
   c_c_decl_bind_file_scope(decl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   finish_decl (decl, input_location, NULL_TREE,NULL_TREE, NULL_TREE);
   n_free(fname);
   self->funcTypeDecl=decl;
}

/**
 * 创建(* (setData_1_typedecl)self->_gen_blocks_array_897[0])(self,tempFgpi1234,5);
 *找当前所在函数中参数中是否有tempFgpi1234，如果没有，找全局变量
 * #define AET_GENERIC_BLOCK_ARRAY_VAR_NAME  "_gen_blocks_array_897" //类中的泛型块函数指针数组变量名称 void *_gen_blocks_array_897[xx];
 *  创建:
 *  (* (setData_1_typedecl)self->_gen_blocks_array_897[0])(self,tempFgpi1234,5);
 *  (* (_test_AMutex__inner_generic_func_1_typedecl)blockInfo->blockFuncs[blockIndex].blockFuncsPointer[index-1])(self,tempFgpi1234);
 */
void generic_block_create_call00(GenericBlock *self,int index,tree lhs,vec<tree, va_gc> *exprlist)
{
   tree arrayRefType=build_pointer_type(void_type_node);
   location_t loc=input_location;
   tree selfid=lookup_name(aet_utils_create_ident("self"));
   tree selfref=build_indirect_ref (loc,selfid,RO_ARROW);
   tree component1=aet_utils_create_ident(AET_GENERIC_BLOCK_ARRAY_VAR_NAME);
   tree componetRef = build_component_ref (loc,selfref,component1, loc,UNKNOWN_LOCATION);//
   tree arrayIndex=build_int_cst(integer_type_node,index-1);
   tree arrayRef = build4 (ARRAY_REF, arrayRefType, componetRef, arrayIndex, NULL_TREE,NULL_TREE);
   tree type=TREE_TYPE(self->funcTypeDecl);//typedef void (*setData_1_typedecl)(MatrixOps *self,int abcts)中的functype
   tree nopExpr= build1 (NOP_EXPR, type, arrayRef);
   tree rtn=void_type_node;
   if(aet_utils_valid_tree(lhs))
      rtn=TREE_TYPE(lhs);
   tree result = build_call_vec (rtn,nopExpr, exprlist);
   self->callTree=result;
}

/**
 * * 创建(* (setData_1_typedecl)self->_gen_blocks_array_897[0])(self,5);
 */
static void createCall(GenericBlock *self,tree lhs,vec<tree, va_gc> *exprlist)
{
   tree arrayRefType=build_pointer_type(void_type_node);
   location_t loc=input_location;
   tree selfid=lookup_name(aet_utils_create_ident("self"));
   tree selfref=build_indirect_ref (loc,selfid,RO_ARROW);
   tree component1=aet_utils_create_ident(AET_GENERIC_BLOCK_ARRAY_VAR_NAME);
   tree componetRef = build_component_ref (loc,selfref,component1, loc,UNKNOWN_LOCATION);//
   tree arrayIndex=build_int_cst(integer_type_node,self->index);
   tree arrayRef = build4 (ARRAY_REF, arrayRefType, componetRef, arrayIndex, NULL_TREE,NULL_TREE);
   tree type=TREE_TYPE(self->funcTypeDecl);//typedef void (*setData_1_typedecl)(MatrixOps *self,int abcts)中的functype
   tree nopExpr= build1 (NOP_EXPR, type, arrayRef);
   tree rtn=void_type_node;
   if(aet_utils_valid_tree(lhs))
      rtn=TREE_TYPE(lhs);
   tree result = build_call_vec (rtn,nopExpr, exprlist);
   self->callTree=result;
}

/**
 * 在泛型函数中调用块
 * aet_generic_func_get_address定义在generic.c中。
 * 创建(* (setData_1_typedecl)aet_generic_func_get_address(index))(self,5);
 */
static void createCallInGenericFunc(GenericBlock *self,tree lhs,vec<tree, va_gc> *exprlist)
{
   vec<tree, va_gc> *arg_vec;
   vec_alloc (arg_vec, 3);
   tree parm1=build_int_cst(integer_type_node,self->index);
   arg_vec->quick_push (parm1);

   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),self->className);
   //块在泛型类中，
   if(class_info_is_generic_class(info)){
      //找self 生成参数 self->_generic_1234_array
      tree selftree=lookup_name(get_identifier("self"));
      tree datum=build_indirect_ref (UNKNOWN_LOCATION,selftree,RO_ARROW);
      tree componentRef= build_component_ref (UNKNOWN_LOCATION, datum,get_identifier(AET_GENERIC_ARRAY), UNKNOWN_LOCATION,UNKNOWN_LOCATION);
      tree addrExpr = build1 (ADDR_EXPR, build_pointer_type(TREE_TYPE(componentRef)), componentRef);
      tree genericInfo=lookup_name(aet_utils_create_ident(AET_GENERIC_INFO_STRUCT_NAME));
      tree type=build_pointer_type(TREE_TYPE(genericInfo));
      tree parm2 = build1 (NOP_EXPR, type, addrExpr);
      int gencount=class_info_get_generic_count(info);
      tree parm3=build_int_cst(integer_type_node,gencount);
      arg_vec->quick_push (parm2);
      arg_vec->quick_push (parm3);

   }else{
      arg_vec->quick_push (null_pointer_node);
      arg_vec->quick_push (build_int_cst(integer_type_node,0));
   }

   tree rtntype=build_pointer_type(void_type_node);
   tree funcdecl=lookup_name(aet_utils_create_ident(AET_GENERIC_FUNC_GET_ADDRESS));
   tree funcType=TREE_TYPE(funcdecl);
   tree addrExpr = build1 (ADDR_EXPR, build_pointer_type(funcType), funcdecl);
   tree  getAddress = build_call_vec (rtntype, addrExpr, arg_vec);

   tree type=TREE_TYPE(self->funcTypeDecl);//typedef void (*setData_1_typedecl)(MatrixOps *self,int abcts)中的functype
   tree nopExpr= build1 (NOP_EXPR, type, getAddress);
   tree rtn=void_type_node;
   if(aet_utils_valid_tree(lhs))
      rtn=TREE_TYPE(lhs);
   tree result = build_call_vec (rtn,nopExpr, exprlist);
   self->callTree=result;
}

/**
 * 有两处调用块函数
 * 1.在泛型函数中有块 genericblock$()... 变成块函数调用
 * 2.在普通函数中有块 genericblock$()... 变成类中的块地址变量调用
 */
void generic_block_create_call(GenericBlock *self,tree lhs,vec<tree, va_gc> *exprlist)
{
   if(!self->isFuncGeneric)
      createCall(self,lhs,exprlist);
   else
      createCallInGenericFunc(self,lhs,exprlist);
}


tree   generic_block_get_call(GenericBlock *self)
{
	return self->callTree;
}

GenericBlock  *generic_block_new(ClassName *className,char *belongFunc,nboolean isFuncGeneric,int index)
{
	 GenericBlock *self = n_slice_alloc0 (sizeof(GenericBlock));
    char *fname=generic_util_create_block_func_name(className->sysName,index);
	 self->name=fname;
	 self->className=className;
	 self->belongFunc=n_strdup(belongFunc);
	 self->isFuncGeneric=isFuncGeneric;
	 self->index=index;
	 return self;
}


