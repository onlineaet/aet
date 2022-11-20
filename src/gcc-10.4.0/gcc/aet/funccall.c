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
#include "opts.h"

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


#include "aetutils.h"
#include "classmgr.h"
#include "funccall.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "genericcall.h"
#include "aet-c-parser-header.h"
#include "classfunc.h"
#include "genericutil.h"
#include "funcmgr.h"
#include "genericcall.h"
#include "blockmgr.h"
#include "genericquery.h"
#include "genericexpand.h"
#include "genericcheck.h"
#include "accesscontrols.h"


/**
 * 函数调用分两种情况
 * 在implimpl的没有加self->访问
 * 在implimpl加self->或其它对象的 如abc->
 */

static void funcCallInit(FuncCall *self)
{
    self->funcHelp=func_help_new();
}

/* Generate an implicit declaration for identifier FUNCTIONID at LOC as a
   function of type int ().  */
static tree createTempDeclare (location_t loc, tree functionid)
{
    tree decl = NULL_TREE;
    tree asmspec_tree;
    /* Not seen before.  */
    n_debug("createTempDeclare 103  id:%s ",IDENTIFIER_POINTER(functionid));
    decl = build_decl (loc, FUNCTION_DECL, functionid, default_function_type);
    DECL_EXTERNAL (decl) = 1;
    TREE_PUBLIC (decl) = 1;
    C_DECL_IMPLICIT (decl) = 1;
    //implicit_decl_warning (loc, functionid, 0);
    asmspec_tree = maybe_apply_renaming_pragma (decl, /*asmname=*/NULL);
    if (asmspec_tree)
       set_user_assembler_name (decl, TREE_STRING_POINTER (asmspec_tree));

    /* C89 says implicit declarations are in the innermost block.
     So we record the decl in the standard fashion.  */
    decl = pushdecl (decl);

     /* No need to call objc_check_decl here - it's a function type.  */
    rest_of_decl_compilation (decl, 0, 0);

     /* Write a record describing this implicit function declaration
     to the prototypes file (if requested).  */
    gen_aux_info_record (decl, 0, 1, 0);

    /* Possibly apply some default attributes to this implicit declaration.  */
    decl_attributes (&decl, NULL_TREE, 0);
    return decl;
}


static tree buildExternalRef (location_t loc, tree id)
{
    tree ref;
    ref = createTempDeclare (loc, id);
    if (TREE_TYPE (ref) == error_mark_node)
       return error_mark_node;

  /* Recursive call does not count as usage.  */
    if (ref != current_function_decl){
       TREE_USED (ref) = 1;
    }

    if (TREE_CODE (ref) == FUNCTION_DECL && !in_alignof){
    	n_debug("buildExternalRef 00 name:%s ",IDENTIFIER_POINTER(id));
       if (!in_sizeof && !in_typeof){
          n_debug("buildExternalRef 11 name:%s ",IDENTIFIER_POINTER(id));
	      C_DECL_USED (ref) = 1;
       }else if (DECL_INITIAL (ref) == NULL_TREE && DECL_EXTERNAL (ref) && !TREE_PUBLIC (ref)){
       	  n_debug("buildExternalRef 22 name:%s ",IDENTIFIER_POINTER(id));
	      aet_record_maybe_used_decl (ref);
       }
    }

    if (TREE_CODE (ref) == CONST_DECL){
    	n_debug("build_external_ref 33 name:%s ",IDENTIFIER_POINTER(id));
        used_types_insert (TREE_TYPE (ref));
        if (warn_cxx_compat  && TREE_CODE (TREE_TYPE (ref)) == ENUMERAL_TYPE  && C_TYPE_DEFINED_IN_STRUCT (TREE_TYPE (ref))){
	       inform (DECL_SOURCE_LOCATION (ref), "enum constant defined here");
	    }
        ref = DECL_INITIAL (ref);
        TREE_CONSTANT (ref) = 1;
    }else if (current_function_decl != NULL_TREE && !DECL_FILE_SCOPE_P (current_function_decl)
	   && (VAR_OR_FUNCTION_DECL_P (ref) || TREE_CODE (ref) == PARM_DECL)){
    	n_debug("build_external_ref 44 name:%s ",IDENTIFIER_POINTER(id));

       tree context = decl_function_context (ref);
       if (context != NULL_TREE && context != current_function_decl)
	      DECL_NONLOCAL (ref) = 1;
    }else if (current_function_decl != NULL_TREE
	   && DECL_DECLARED_INLINE_P (current_function_decl)
	   && DECL_EXTERNAL (current_function_decl)
	   && VAR_OR_FUNCTION_DECL_P (ref)
	   && (!VAR_P (ref) || TREE_STATIC (ref))
	   && ! TREE_PUBLIC (ref)
	   && DECL_CONTEXT (ref) != current_function_decl){
    	  /* C99 6.7.4p3: An inline definition of a function with external
    	     linkage ... shall not contain a reference to an identifier with
    	     internal linkage.  */
     	n_debug("build_external_ref 55 name:%s ",IDENTIFIER_POINTER(id));
        record_inline_static (loc, current_function_decl, ref, csi_internal);
    }

     return ref;
}

static tree createTempFunction(tree field)
{
   tree funid=DECL_NAME(field);
   tree type=TREE_TYPE(field);
   tree functionType=TREE_TYPE(type);
   tree decl = build_decl (0, FUNCTION_DECL, funid, default_function_type);
   TREE_TYPE(decl)=functionType;
   c_aet_copy_lang(decl,field);
   return decl;
}


static char *getLowClassName(tree field)
{
   tree pointerType=TREE_TYPE(field);
   tree type=TREE_TYPE(pointerType);
   tree next=TYPE_NAME(type);
   tree id=DECL_NAME(next);
   return IDENTIFIER_POINTER(id);
}

static int findFuncAtClassAndInterface(FuncCall *self,ClassName *className,char *orgiName)
{
	//在本类中找，找不到，再从父类中找，如果再找不到就从接口找
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(info==NULL)
		return ID_NOT_FIND;
	nboolean exists=func_mgr_func_exits(func_mgr_get(),className,orgiName);//在本类中有，但参数不一定配置
	n_debug("findFuncAtClassAndInterface findFuncAtClass 00 %s %s exists:%d",className->sysName,orgiName,exists);
	//取一个来匹配
	if(!exists){
		int i;
		for(i=0;i<info->ifaceCount;i++){
			ClassName interface=info->ifaces[i];
			nboolean exists=func_mgr_func_exits(func_mgr_get(),&interface,orgiName);//在本类中有，但参数不一定配置
			n_debug("findFuncAtClassAndInterface  index:%d 类:%s 接口:%s% 函数名:%s 是否找到:%d",i,className->sysName,interface.sysName,orgiName,exists);
			if(exists)
			   return ISAET_FIND_FUNC;
		}
		return findFuncAtClassAndInterface(self,&info->parentName,orgiName);
	}else{
	    return ISAET_FIND_FUNC;
	}
}

static int findStaticFuncAtClass(FuncCall *self,ClassName *className,char *orgiName)
{
	//在本类中找，找不到，再从父类中找，如果再找不到就从接口找
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(info==NULL)
		return ID_NOT_FIND;
	nboolean exists=func_mgr_static_func_exits(func_mgr_get(),className,orgiName);//在本类中有，但参数不一定配置
	n_debug("findStaticFuncAtClass findFuncAtClass 00 %s %s info:%p exists:%d",className->sysName,orgiName,exists);
	//取一个来匹配
	if(!exists){
		return findStaticFuncAtClass(self,&info->parentName,orgiName);
	}else{
	    return ISAET_FIND_STATIC_FUNC;
	}
}

static int findFunc(FuncCall *self,ClassName *className,char *orgiName)
{
	int re=findFuncAtClassAndInterface(self,className,orgiName);
	return re;
}

static int findFuncAtStatic(FuncCall *self,ClassName *className,char *orgiName)
{
	int re=findStaticFuncAtClass(self,className,orgiName);
	return re;
}

/**
 * 先找类函数，找不到再找类静态函数
 */
int fun_call_find_func(FuncCall *self,ClassName *className,tree id)
{
	 int re=findFunc(self,className,IDENTIFIER_POINTER(id));
     if(re==ID_NOT_FIND){
	   re=findFuncAtStatic(self,className,IDENTIFIER_POINTER(id));
     }
     return re;
}

/**
 *  ( 重整为 (self 或 (self,
 */
static void addSelfToFunc(c_parser *parser,ClassName *className)
{
	  c_token *openParen = c_parser_peek_token (parser);//"（*"
	  c_token *who=c_parser_peek_2nd_token(parser);
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+4>AET_MAX_TOKEN){
			error("token太多了");
			return;
	  }
	  int i;
	  if(who->type == CPP_CLOSE_PAREN){ //()->(Abc *self)
		  parser->tokens_avail=tokenCount+1;
		  for(i=tokenCount;i>1;i--){
			  aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
		  }
	  }else{
		 //(int->(Abc *self,int
		  parser->tokens_avail=tokenCount+2;
		  for(i=tokenCount;i>1;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]); //多一个逗号
		  }
  		  aet_utils_create_token(&parser->tokens[2],CPP_COMMA,(char *)",",1);
	  }
	  aet_utils_create_token(&parser->tokens[1],CPP_NAME,"self",4);
	  aet_print_token_in_parser("func_call addSelfToFunc className ---- %s",className->sysName);
}

/**
 * 从当前类遍历父类和接口
 */
static CandidateFunc *selectFunc(FuncCall *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean allscope,
        GenericModel *generics,GenericModel * funcGenericDefine,FuncPointerError **errors)
{
    CandidateFunc * item=select_field_get_func_by_recursion(select_field_get(),className,orgiName,
                exprlist,origtypes,arg_loc,expr_loc,allscope,generics,errors);
    if(item!=NULL){
        if(class_func_is_func_generic(item->classFunc) || class_func_is_query_generic(item->classFunc)){
            ClassName *selectClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),item->sysName);
            //printf("funcall selectFunc generic_call_add_fpgi_parm exprlist:%d\n",exprlist->length());
            generic_call_add_fpgi_parm(generic_call_get(),item->classFunc,selectClassName,exprlist,funcGenericDefine);
           // printf("funcall selectFunc generic_call_add_fpgi_parm exprlist:1---%d\n",exprlist->length());
        }
    }
    return item;
}


/**
 * 把泛型形参对应的实参转化为:实参--中间形参:如<int>---void*
 */
static void convertParmForGenerics(FuncCall *self,CandidateFunc *candidate,vec<tree, va_gc> *exprlist,
        location_t expr_loc,nboolean allscope,GenericModel *generics)
{
        ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),candidate->sysName);
		tree decl=NULL_TREE;
		ClassFunc *item=candidate->classFunc;
		n_debug("convertParmForGenerics xxx %s allscope:%d  field?:%d exprlist:%d\n",
			className->sysName,allscope,aet_utils_valid_tree(item->fieldDecl),exprlist->length());
		if(allscope){
			if(aet_utils_valid_tree(item->fieldDecl)){
				decl=createTempFunction(item->fieldDecl);
			}else if(aet_utils_valid_tree(item->fromImplDefine)){
				decl=item->fromImplDefine;
			}else{
				decl=item->fromImplDecl;
			}
		}else{
			if(aet_utils_valid_tree(item->fieldDecl)){
				decl=createTempFunction(item->fieldDecl);
			}
		}
		if(decl==NULL_TREE){
			return;
		}
		int count=generic_call_replace_parm(generic_call_get(),expr_loc,decl, exprlist,className,generics);
	   // printf("泛型转化成功的个数:%d\n",count);
}

/**
 * 加入self,从func调用
 */
static tree callParentOrParentIface(FuncCall *self,tree func,CandidateFunc *selected,vec<tree, va_gc> *exprlist,location_t loc,nboolean allscope)
{
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),selected->sysName);
    ClassFunc *item=selected->classFunc;
    tree decl=NULL_TREE;
    if(class_info_is_interface(info)){
        if(aet_utils_valid_tree(item->fromImplDefine)){
            error_at(DECL_SOURCE_LOCATION(item->fromImplDefine),"接口%qs不应有定义。",selected->sysName);
            return NULL_TREE;
        }
        if(aet_utils_valid_tree(item->fromImplDecl)){
            error_at(DECL_SOURCE_LOCATION(item->fromImplDecl),"接口%qs不应有声明。",selected->sysName);
            return NULL_TREE;
        }
        n_debug("callParentOrParentIface localClass 00 在这里创建父类接口方法的引用。接口所属类:%s 接口:%s\n",selected->implSysName,selected->sysName);
        tree firstParm=NULL_TREE;
        ClassName *implClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->implSysName);
        ClassName *iface=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->sysName);
        decl=func_help_create_parent_iface_deref(self->funcHelp,func,implClassName,iface,item->fieldDecl,loc,&firstParm);
        if(aet_utils_valid_tree(decl)){
            n_debug("callParentOrParentIface localClass 11 在这里创建父类接口方法的引用。接口所属类:%s 接口:%s\n",selected->implSysName,selected->sysName);
            exprlist->ordered_remove(0);
            vec_safe_insert (exprlist, 0, firstParm);
        }
    }else{
        if(aet_utils_valid_tree(item->fieldDecl)){
            //创建指指针引用
            n_debug("func_call_select parentclass 33 在这里创建父类的field引用 类:%s\n",selected->sysName);
            tree firstParm=NULL_TREE;
            ClassName *parentClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->sysName);
            decl=func_help_create_parent_deref(self->funcHelp,func,parentClassName,item->fieldDecl,loc,&firstParm);
            if(aet_utils_valid_tree(decl)){
               exprlist->ordered_remove(0);
               vec_safe_insert (exprlist, 0, firstParm);
            }
        }else   if(aet_utils_valid_tree(item->fromImplDefine) && allscope){
            n_debug("func_call_select parentClass 11 有定义直接用,说明两个类的实现都在同一个c文件 类:%s\n",selected->sysName);
            tree selfVarOrParm = (*exprlist)[0];
            ClassName *parentClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->sysName);
            tree castToParent=func_help_cast_to_parent(self->funcHelp,selfVarOrParm,parentClassName);
            exprlist->ordered_remove(0);
            vec_safe_insert (exprlist, 0, castToParent);
            decl=item->fromImplDefine;
        }else if(aet_utils_valid_tree(item->fromImplDecl) && allscope){
            n_debug("func_call_select parentClass 22 有声明直接用,说明两个类的实现都在同一个c文件 类:%s\n",selected->sysName);
            tree selfVarOrParm = (*exprlist)[0];
            ClassName *parentClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->sysName);
            tree castToParent=func_help_cast_to_parent(self->funcHelp,selfVarOrParm,parentClassName);
            exprlist->ordered_remove(0);
            vec_safe_insert (exprlist, 0, castToParent);
            decl=item->fromImplDecl;
        }
    }
    return decl;
}

static void test_print_exprlist(vec<tree, va_gc> *exprlist)
{
        int ix;
        tree arg;
        int count=0;
        for (ix = 0; exprlist->iterate (ix, &arg); ++ix){
            printf("print_exprlist -- %d\n",count++);
            aet_print_tree(arg);
        }
}

/**
 * 在interface找到的fromImplDefine和fromImplDecl都是空的
 * 需要检查此种情况，如果不为空报错。应该没有这种情况吧！
 * 如果是接口，要把exprlist的第一个参数类型是className,改成转化后的接口
 * exprlist->ordered_remove(0);
*  vec_safe_insert (exprlist, 0, firstParm);
 */
static tree callItself(FuncCall *self,tree func,CandidateFunc *selected,vec<tree, va_gc> *exprlist,location_t loc,nboolean allscope)
{
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),selected->sysName);
	ClassFunc *item=selected->classFunc;
    tree decl=NULL_TREE;
	if(class_info_is_interface(info)){
		if(aet_utils_valid_tree(item->fromImplDefine)){
			error_at(DECL_SOURCE_LOCATION(item->fromImplDefine),"接口%qs不应有定义。",selected->sysName);
			return NULL_TREE;
		}
		if(aet_utils_valid_tree(item->fromImplDecl)){
			error_at(DECL_SOURCE_LOCATION(item->fromImplDecl),"接口%qs不应有声明。",selected->sysName);
			return NULL_TREE;
	    }
		n_debug("func_call_select callItself 00 在这里创建本身接口方法的引用。接口所属类:%s 接口:%s\n",selected->implSysName,selected->sysName);
		tree firstParm=NULL_TREE;
		ClassName *implClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->implSysName);
		ClassName *iface=class_mgr_get_class_name_by_sys(class_mgr_get(),selected->sysName);
		decl=func_help_create_itself_iface_deref(self->funcHelp,func,implClassName,iface,item->fieldDecl,loc,&firstParm);
		if(aet_utils_valid_tree(decl)){
			exprlist->ordered_remove(0);
			vec_safe_insert (exprlist, 0, firstParm);
		}
	}else{
		if(aet_utils_valid_tree(item->fieldDecl)){
			//printf("func_call_select localClass 11 在这里创建本身的field引用 类:%s\n",selected->sysName);
            tree firstParm= NULL;//(*exprlist)[0];
            decl=func_help_create_itself_deref_new(self->funcHelp,func,item->fieldDecl,loc,&firstParm);
            //printf("func_call_select localClass 11ccccc 在这里创建本身的field引用 类:%s\n",selected->sysName);
            if(aet_utils_valid_tree(decl)){
                exprlist->ordered_remove(0);
                vec_safe_insert (exprlist, 0, firstParm);
            }
		}else if(aet_utils_valid_tree(item->fromImplDefine) && allscope){
		    //printf("func_call_select localClass 11 有定义直接用 类:%s\n",selected->sysName);
			decl=item->fromImplDefine;
		}else if(aet_utils_valid_tree(item->fromImplDecl) && allscope){
			//printf("func_call_select localClass 11 有声明直接用 类:%s\n",selected->sysName);
			decl=item->fromImplDecl;
		}
	}
	return decl;
}

/**
 * 1.调用的函数返回值是问号泛型
 * 2.函数有泛型参数
 *
 */
static void setGenericQuery(FuncCall *self,CandidateFunc *item,tree last)
{
	 if(class_func_is_query_generic(item->classFunc) || class_func_have_generic_parm(item->classFunc)){
	     n_debug("在funcall.c调用:%s 是问号泛型:%d 方法有泛型参数：%d\n",item->classFunc->orgiName,
				class_func_is_query_generic(item->classFunc),class_func_have_generic_parm(item->classFunc));
		nboolean ok=generic_query_have_query_caller(generic_query_get(),item->classFunc,last);
		//printf("在funcall.c是问号泛型---xx--- %d\n",ok);
		if(ok){
			nboolean isGenericClass=generic_query_is_generic_class(generic_query_get(),last);
			if(isGenericClass){ //真正的调用者是泛型类
				GenericModel *model=generic_query_get_call_generic(generic_query_get(),last);
				if(model==NULL){
					error_at(input_location,"调用方法%qs的对象，并没有定义泛型。",item->classFunc->orgiName);
					return;
				}
				if(generic_model_get_undefine_count(model)==0 && !generic_model_have_query(model)){
				    n_debug("调用转成new$形式的泛型定义对象 model:%s file:%s\n",generic_model_tostring(model),in_fnames[0]);
				   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),item->sysName);
				   block_mgr_add_define_new_object(block_mgr_get(),className,model);
				   generic_expand_add_define_object(generic_expand_get(),className,model);
				}else{
				   n_warning("不能作为泛型定义加入到defineObject中 undefineCount:%d query:%d",generic_model_get_undefine_count(model),generic_model_have_query(model));
				}
			}
		}
     }
}

/**
 * 从当前找到父类找静态的
 */
static CandidateFunc *selectStaticByRecursion(FuncCall *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,
        vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
     if(className==NULL || orgiName==NULL || className->sysName==NULL){
        return NULL;
     }
     CandidateFunc *result=select_field_get_static_func(select_field_get(),className,orgiName,exprlist,origtypes,arg_loc,expr_loc,errors);
     if(result==NULL){
        ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
        n_debug("func_help_select_static_func_by_recursion 再一次从父类找静态方法 selectFunc ---ttt %s %p",className->sysName,info);
        result=selectStaticByRecursion(self,&info->parentName,orgiName,exprlist,origtypes,arg_loc,expr_loc,errors);
     }
     return result;
}



/**
 * 指针调用形式
 */
tree func_call_deref_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
	BINFO_FLAG_6(func)=0;
	tree field=TREE_OPERAND(func,1);
	tree id=DECL_NAME (field);
	tree last=NULL_TREE;
	char currentClassName[256];
	char funName[256];
	int len=aet_utils_get_orgi_func_and_class_name(IDENTIFIER_POINTER(id),currentClassName,funName);
	if(len==0)
		return error_mark_node;
	char *fromLowClassName=getLowClassName(field);
	n_debug("func_call_deref_select 00 name:%s className:%s func:%s %d refVarClassName:%s func:%p %p\n",
			IDENTIFIER_POINTER(id),currentClassName,funName,len,fromLowClassName,func,c_aet_get_func_generics_model(func));
	tree indiect=TREE_OPERAND(func,0);
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),currentClassName);
	ClassName *lowClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),fromLowClassName);
	if(className==NULL || lowClassName==NULL)
		return error_mark_node;
	CandidateFunc *item=NULL;
	GenericModel *generics=generic_call_get_generic_from_component_ref(generic_call_get(),func);//对象是否有泛型的真实类型.
	GenericModel *funcGenericDefine=c_aet_get_func_generics_model(func);//如果funcGenericDefine是有效的说明这是一个泛型函数
    if(class_mgr_is_interface(class_mgr_get(),className)){
    	//这是接口引用，首先检查接口是不是在lowClassName范围内或lowClassName父类中
    	item=select_field_get_func(select_field_get(),className,funName,exprlist,origtypes,arg_loc,expr_loc,FALSE,generics,errors);
    	if(item==NULL){
    		error_at(expr_loc,"接口类%qs找不到%qs方法。",className,funName);
    		return last;
    	}
		if(class_func_is_func_generic(item->classFunc) || class_func_is_query_generic(item->classFunc)){
			printf("funccall.c 这里可能有问题。注意！！！！ sysName:%s func:%s\n",className->sysName,funName);
		   generic_call_add_fpgi_parm(generic_call_get(),item->classFunc,className,exprlist,funcGenericDefine);
		}
    	tree firstParm=NULL_TREE;
	    tree selfVarOrParm = (*exprlist)[0];
	   // printf("接口的第一个参数 self %s %s\n",lowClassName->sysName,className->sysName);
       // aet_print_tree(func);
       // aet_print_tree(selfVarOrParm);
	    last=func_help_component_ref_interface(self->funcHelp,func, selfVarOrParm,lowClassName,
	    		className,item->classFunc,expr_loc,&firstParm);
       // printf("接口的第一个参数 22 firstParm\n");
       // aet_print_tree(firstParm);
       // printf("接口的第一个参数 33 last\n");
       // aet_print_tree(last);

	    if(firstParm!=NULL_TREE){
			exprlist->ordered_remove(0);
			vec_safe_insert (exprlist, 0, firstParm);
	    }
    }else{
    	n_debug("func_call_deref_select bbb name:%s className:%s func:%s %d refVarClassName:%s generics:%p exprlist:%p %d\n",
    			IDENTIFIER_POINTER(id),currentClassName,funName,len,fromLowClassName,generics,exprlist,exprlist->length());
	   item=selectFunc(self,lowClassName,funName,exprlist,origtypes,arg_loc,expr_loc,FALSE,generics,funcGenericDefine,errors);
	   if(item!=NULL){
		   convertParmForGenerics(self,item,exprlist,expr_loc,FALSE,generics);//转化实参到void*
	       if(!strcmp(item->sysName,lowClassName->sysName)){
		       //printf("func_call_deref_select 找到了 11 方法就在本类中 class:%s  mangleFunName:%s lowClassName:%s\n",
				   //item->sysName,item->classFunc->mangleFunName,lowClassName->userName);
		      last=callItself(self,func,item,exprlist,expr_loc,FALSE);
	       }else{
		      //printf("func_call_deref_select 找到了 22 方法就在父类中 class:%s mangleFunName:%s lowClassName:%s\n",
					  // item->sysName,item->classFunc->mangleFunName,lowClassName->sysName);
		      last=callParentOrParentIface(self,func,item,exprlist,expr_loc,FALSE);
	       }
	       setGenericQuery(self,item,last);
		   generic_check_parm(item->sysName,item->classFunc,exprlist,last);
       }else{
    	    printf("func_call_deref_select 找静态函数\n");
			exprlist->ordered_remove(0);//把self参数移走
			origtypes->ordered_remove(0);//把self参数移走
			item=selectStaticByRecursion(self,className,funName,exprlist,origtypes,arg_loc,expr_loc,errors);
			if(item!=NULL)
				last=item->classFunc->fieldDecl;
       }
    }
    if(item!=NULL){
        access_controls_access_method(access_controls_get(),expr_loc,item->classFunc);
        select_field_free_candidate(item);
    }
	return last;
}

/**
 * 调用这里，是没有加指针的引用，如果类中找到该函数，就不再加self引用否则要加
 * 从className查找，如果只有一个函数，就不用再比较参数了
 * 但如果是在父类找到的要把第一个参数强转为父类
 * 如果是在接口找到的也要转强转为接口。
 * 不出错 第一个保障:从func里获取的函数名取出的className一定不是接口
 * 如果是静态函数，要把exprlist中的第一个参数移走，不加类引用而直接调用
 */
tree func_call_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
	BINFO_FLAG_2(func)=0;
	tree id=DECL_NAME (func);
	tree last=NULL_TREE;
	char currentClassName[256];
	char funName[256];
	int len=aet_utils_get_orgi_func_and_class_name(IDENTIFIER_POINTER(id),currentClassName,funName);
	if(len==0)
		return error_mark_node;
	n_debug("func_call_select 00 name:%s className:%s func:%s %d",IDENTIFIER_POINTER(id),currentClassName,funName,len);
	//第一步从本地找
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),currentClassName);
	if(className==NULL)
		return error_mark_node;
	CandidateFunc *item=NULL;
	GenericModel *generics=NULL;//在这里的调用都是在类实现中，不会有具体的泛型找到，所以是空的.
	GenericModel *funcGenericDefine=c_aet_get_func_generics_model(func);//如果funcGenericDefine是有效的说明这是一个泛型函数
	item=selectFunc(self,className,funName,exprlist,origtypes,arg_loc,expr_loc,TRUE,generics,funcGenericDefine,errors);
	//如果是field要加入指针，否则访问不到
	if(item!=NULL){
       n_debug("func_call_select is 找到了 00 class:%s mangleFunName:%s\n",item->sysName,item->classFunc->mangleFunName);
	   convertParmForGenerics(self,item,exprlist,expr_loc,TRUE,generics);//转化实参到void*
       if(!strcmp(item->sysName,className->sysName)){
    	   n_debug("func_call_select is 找到了 11 方法就在本类中 class:%s mangleFunName:%s\n",
        		   item->sysName,item->classFunc->mangleFunName);
           last=callItself(self,func,item,exprlist,expr_loc,TRUE);
       }else{
    	   last=callParentOrParentIface(self,func,item,exprlist,expr_loc,TRUE);
       }
       setGenericQuery(self,item,last);
	   generic_check_parm(item->sysName,item->classFunc,exprlist,last);
	}else{
		n_debug("func_call_select 找静态函数 找之前把self移走。\n");
		exprlist->ordered_remove(0);//把self参数移走
		origtypes->ordered_remove(0);//把self参数移走
		item=selectStaticByRecursion(self,className,funName,exprlist,origtypes,arg_loc,expr_loc,errors);
		printf("func_call_select 找静态函数 找之前把self移走。item:%p\n",item);
		if(item!=NULL)
			last=item->classFunc->fieldDecl;
	}
    if(item!=NULL){
        access_controls_access_method(access_controls_get(),expr_loc,item->classFunc);
        select_field_free_candidate(item);
    }
	return last;
}

/**
 * super调用形式
 */
tree func_call_super_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
	BINFO_FLAG_5(func)=0;
	tree field=TREE_OPERAND(func,1);
	tree id=DECL_NAME (field);
	tree last=NULL_TREE;
	char currentClassName[256];
	char funName[256];
	int len=aet_utils_get_orgi_func_and_class_name(IDENTIFIER_POINTER(id),currentClassName,funName);
	if(len==0)
		return error_mark_node;
	char *fromLowClassName=getLowClassName(field);
	//printf("func_call_super_select 00 name:%s className:%s func:%s %d refVarClassName:%s %s, %s, %d\n",
			//IDENTIFIER_POINTER(id),currentClassName,funName,len,fromLowClassName,__FILE__, __FUNCTION__, __LINE__);
   	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),currentClassName);
	ClassName *lowClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),fromLowClassName);
	if(className==NULL || lowClassName==NULL)
		return error_mark_node;
	CandidateFunc *item=NULL;
	GenericModel *generics=NULL;
	GenericModel *funcGenericDefine=c_aet_get_func_generics_model(func);//如果funcGenericDefine是有效的说明这是一个泛型函数
    if(class_mgr_is_interface(class_mgr_get(),className)){
    	//这是接口引用，首先检查接口是不是在lowClassName范围内或lowClassName父类中
		error_at(expr_loc,"接口类%qs的方法%qs不能通过super调用。",className->sysName,funName);
		return last;
    }else{
       item=selectFunc(self,lowClassName,funName,exprlist,origtypes,arg_loc,expr_loc,FALSE,generics,funcGenericDefine,errors);
	   if(item!=NULL){
		   //参数总是self，也是className的最子类
		  // printf("func_call_super_select 找到了 11 方法就在父类中 class:%s face:%s mangleFunName:%s lowClassName:%s\n",
					//	   item->sysName,item->iface,item->mangleEntity->mangleFunName,lowClassName);
		   last=callParentOrParentIface(self,func,item,exprlist,expr_loc,FALSE);
	   }else{
		  n_debug("func_call_super_select 找静态函数\n");
		  exprlist->ordered_remove(0);//把self参数移走
		  origtypes->ordered_remove(0);//把self参数移走
		  item=selectStaticByRecursion(self,lowClassName,funName,exprlist,origtypes,arg_loc,expr_loc,errors);
		  if(item!=NULL)
			 last=item->classFunc->fieldDecl;
	   }

    }
    if(item!=NULL){
        access_controls_access_method(access_controls_get(),expr_loc,item->classFunc);
        select_field_free_candidate(item);
    }
	return last;
}

tree func_call_static_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors)
{
	BINFO_FLAG_3(func)==0;
	tree id=DECL_NAME (func);
	tree last=NULL_TREE;
	char currentClassName[256];
	char funName[256];
	int len=aet_utils_get_orgi_func_and_class_name(IDENTIFIER_POINTER(id),currentClassName,funName);
	if(len==0)
		return error_mark_node;
	n_debug("func_call_static_select 00 name:%s className:%s func:%s %d\n",
			IDENTIFIER_POINTER(id),currentClassName,funName,len);
	//第一步从本地找
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),currentClassName);
	if(className==NULL)
		return error_mark_node;
	CandidateFunc *item=NULL;
	item=selectStaticByRecursion(self,className,funName,exprlist,origtypes,arg_loc,expr_loc,errors);
	//如果是field要加入指针，否则访问不到
	if(item!=NULL)
	   last=item->classFunc->fieldDecl;
    if(item!=NULL){
        access_controls_access_method(access_controls_get(),expr_loc,item->classFunc);
        select_field_free_candidate(item);
    }
	return last;
}

/**
 * 在没有加self->前的处理函数
 * 如果在implimpl field,interface有这个方法存在
 * 等参数解析完后再设self访问或不设
 */

FuncAndVarMsg func_call_get_process_express_method(FuncCall *self,tree id, ClassName *className)
{
      char *idStr=IDENTIFIER_POINTER(id);
      ClassName *info=class_mgr_get_class_name_by_user(class_mgr_get(),idStr);
      if(info!=NULL){
    	  n_debug("func_call_get_process_express_method 00 class:%s id:%s 是构造函数 ",
    			  className==NULL?"null":className->sysName,idStr);
          return ID_IS_CONSTRUCTOR;
      }

      tree decl = lookup_name (id);
      if(decl && decl != error_mark_node){
      	 n_debug("func_call_get_process_express_method 11 找到了声明的变量或函数返回后由系统build_external_ref处理 decl code:%s id:%s ",
      			get_tree_code_name(TREE_CODE(decl)),idStr);
    	  return ID_EXISTS;
      }
  	  c_parser *parser=self->parser;
	  if(className==NULL)
		 return ID_NOT_FIND;
  	  if(parser->isAet){
  		 tree classId=aet_utils_create_ident(className->sysName);
  		 tree classDecl=lookup_name(classId);
  		 if(!classDecl || classDecl==NULL_TREE || classDecl==error_mark_node){
  	     	 n_debug("func_call_get_process_express_method 22 在isAet中找不到 classname:%s id:%s 由系统build_external_ref处理",
  	     			 className->sysName,idStr);
  		     return ID_NOT_FIND;
  	     }
		 int re=fun_call_find_func(self,className,id);
 	     n_debug("func_call_get_process_express_method 33 在isAet中找函数结果是 %d classname:%s id:%s ",
 	    		 re,className->sysName,idStr);
 	     return re;
      }
      n_debug("func_call_get_process_express_method 44 找不到任何关于名称的信息 由系统build_external_ref处理 classname:%s id:%s ",
    		  className,idStr);
      return ID_NOT_FIND;
}

/**
 * 解析的上下文是在impl 不加self->
 * 创建临时的函数名，包含有className和原函数名
 * 格式如下:_A3Abc7getName序号
 * 最后在fun_call_select使用，特点是不加self->或.调用，并且只能在类实现中
 */
tree  func_call_create_temp_func(FuncCall *self,ClassName *className,tree id)
{
    c_parser *parser=self->parser;
	location_t  loc = c_parser_peek_token (parser)->location;
	tree result=NULL_TREE;
	addSelfToFunc(self->parser,className);
	tree newName=aet_utils_create_temp_func_name(className->sysName,IDENTIFIER_POINTER(id));
    result=buildExternalRef(loc,newName);
    BINFO_FLAG_2(result)=1;
	return result;
}


void  func_call_set_parser(FuncCall *self,c_parser *parser)
{
	self->parser=parser;
	self->funcHelp->parser=parser;
}


FuncCall *func_call_new()
{
	FuncCall *self = n_slice_alloc0 (sizeof(FuncCall));
	funcCallInit(self);
	return self;
}





