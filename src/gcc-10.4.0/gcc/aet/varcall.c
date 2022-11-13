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
#include "varcall.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "accesscontrols.h"


static void varCallInit(VarCall *self)
{
}

/**
 * 当
 *  ARandom(int re){
  *   	self();
  *       path=NULL;
   *  }
   *  self()把当前lookup_name从类类型声明TYPE_DECL，换成了函数声明FUNCTION_DECL
   *  所以查找变量时，遇到该情况，改从info的record查找变量。
 */
static int findValAtClass(VarCall *self,ClassName *className,tree id)
{
	 if(className==NULL)
		return ID_NOT_FIND;
	 ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	 if(!info){
		return ID_NOT_FIND;
	 }
	 tree classId=aet_utils_create_ident(className->sysName);
	 tree classDecl=lookup_name(classId);
	 if(classDecl){
		if (TREE_CODE (classDecl) == TYPE_DECL){
			tree type = TREE_TYPE (classDecl);
			n_debug("findValAtClass 00 field type is:%s %s\n",get_tree_code_name(TREE_CODE(type)),className->sysName);
			nboolean ret=class_util_have_field (type, id);
			if(ret){
				n_debug("findValAtClass 11 找到了 className:%s id:%s\n",className->sysName,IDENTIFIER_POINTER(id));
				return ISAET_FIND_VAL;
			}else{
				return findValAtClass(self,&info->parentName,id);
			}
		}else{
			if(TREE_CODE (classDecl) ==FUNCTION_DECL){
				char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
				printf("在当前函数中调用了self(),lookup_name返回的是构造函数，不是类类型。%s 当前函数:%s\n",className->sysName,funcName);
				nboolean ret=class_util_have_field (info->record, id);
				if(ret){
				   n_debug("findValAtClass 33 找到了 className:%s id:%s\n",className->sysName,IDENTIFIER_POINTER(id));
				   return ISAET_FIND_VAL;
				}else{
				   return findValAtClass(self,&info->parentName,id);
				}
			}else{
			   return ID_NOT_FIND;
			}
		}
	 }else{
	    printf("findValAtClass 33 未找到class tree %s id:%s\n",className->sysName,IDENTIFIER_POINTER(id));
		return ID_NOT_FIND;
	 }
}

static int findStaticValAtClass(VarCall *self,ClassName *className,tree id)
{
	 if(className==NULL || !id)
		return ID_NOT_FIND;
	 ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	 if(!info){
		return ID_NOT_FIND;
	 }
	 tree classId=aet_utils_create_ident(className->sysName);
	 tree classDecl=lookup_name(classId);
	 if(classDecl){
		if (TREE_CODE (classDecl) == TYPE_DECL){
			n_debug("findStaticValAtClass 00 field type is:%s %s\n",get_tree_code_name(TREE_CODE(TREE_TYPE(classDecl))),className->sysName);
			tree ret=var_mgr_get_static_var (var_mgr_get(),&info->className, id);
			if(ret){
				n_debug("findStaticValAtClass 11 找到了 className:%s id:%s  %s %s %d\n",
						 className->sysName,IDENTIFIER_POINTER(id),__FILE__,__FUNCTION__,__LINE__);
				   return ISAET_FIND_STATIC_VAL;
			}else{
				 return findStaticValAtClass(self,&info->parentName,id);
			}

			n_debug("findStaticValAtClass 22 找不到了域 %s id:%s %s %s %d\n",className->sysName,IDENTIFIER_POINTER(id),__FILE__,__FUNCTION__,__LINE__);
		}else{
			return ID_NOT_FIND;

		}
	 }else{
	    printf("findStaticValAtClass 33 未找到class tree %s id:%s %s %s %d\n",className->sysName,IDENTIFIER_POINTER(id),__FILE__,__FUNCTION__,__LINE__);
		return ID_NOT_FIND;
	 }

}

static int findVal(VarCall *self,ClassName *className,tree id)
{
	int re=findValAtClass(self,className,id);
	if(re==ID_NOT_FIND)
		re=findStaticValAtClass(self,className,id);
	return re;
}

static tree createCastType(char *className)
{
	tree recordId=aet_utils_create_ident(className);
	tree record=lookup_name(recordId);
	if(!record || record==NULL_TREE || record==error_mark_node){
		printf("没有找到 %s\n",className);
		error("没有找到 class");
	}
	//printf("createCastType 00  %s\n",get_tree_code_name(TREE_CODE(record)));
	tree recordType=TREE_TYPE(record);
	//printf("createCastType 11  %p\n",recordType);
	//printf("createCastType 22  %s\n",get_tree_code_name(TREE_CODE(recordType)));
	tree pointer=build_pointer_type(recordType);
	return pointer;
}

static tree  castToParent(tree parmOrVar,char *parentName)
{
	tree type=createCastType(parentName);
	tree castParent = build1 (NOP_EXPR, type, parmOrVar);
	return castParent;
}


/**
 * 不知道右侧表达式的类型前，先选出变量
 */
tree var_call_pass_one_convert(VarCall *self,location_t component_loc,tree exprValue,tree component,VarRefRelation *item,tree *staticVar)
{
	enum tree_code code=TREE_CODE(exprValue);
	if(code!=VAR_DECL && code!=PARM_DECL && code!=NOP_EXPR
			&& code!=NON_LVALUE_EXPR && code!=ADDR_EXPR && code!=COMPONENT_REF){
		n_debug("var_call_pass_one_convert 不是VAR_DECL、PARM_DECL、NOP_EXPR、NON_LVALUE_EXPR、ADDR_EXPR、COMPONENT_REF 由系统处理 %s\n",
				get_tree_code_name(code));
		return exprValue;
	}
	n_debug("var_call_pass_one_convert 00 引用对象: %s 变量名：%s 引用的组件:%s 变量类型:%s",
			item->refClass,item->varName,IDENTIFIER_POINTER(component),get_tree_code_name(code));
    //从refclass开始查找到第一次遇到的组件所在的类
	ClassName *refClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),item->refClass);
	ClassName * passOneClass=class_mgr_get_class_by_component(class_mgr_get(),refClassName,component);
	if(passOneClass){
		//说明这个record里有component
		n_debug("var_call_pass_one_convert 11 从类%s开始找组件在其中一个类 %s 中找到 %s",refClassName->sysName,
				passOneClass->sysName,IDENTIFIER_POINTER(component));
	    if(!strcmp(refClassName->sysName, passOneClass->sysName)){
	    	n_debug("var_call_pass_one_convert 22 结果是相同的 class %s",passOneClass->sysName);
	    	access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),passOneClass->sysName);
	    	return exprValue;
	    }
	    tree result=castToParent(exprValue,passOneClass->sysName);
        access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),passOneClass->sysName);
	    return result;
	}else{
		  //找是不是引用的static变量
		 passOneClass=var_mgr_get_class_by_static_component(var_mgr_get(),refClassName,component);
		 if(passOneClass){
			n_debug("var_call_pass_one_convert 33 从类%s开始找组件在其中一个类 %s 中找到 并且是静态的 %s",refClassName->sysName,
					passOneClass->sysName,IDENTIFIER_POINTER(component));
		    if(!strcmp(refClassName->sysName, passOneClass->sysName)){
		    	n_debug("var_call_pass_one_convert 44 结果是相同的 并且是静态的 class %s",passOneClass->sysName);
	            access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),passOneClass->sysName);
			   *staticVar=var_mgr_get_static_var(var_mgr_get(),passOneClass,component);
			   return exprValue;
		    }
            access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),passOneClass->sysName);
		    tree result=castToParent(exprValue,passOneClass->sysName);
			*staticVar=var_mgr_get_static_var(var_mgr_get(),passOneClass,component);
		    return result;
		 }
	}
	return exprValue;
}

static tree createClassPointer(char *sysName)
{
	tree recordId=aet_utils_create_ident(sysName);
	tree record=lookup_name(recordId);
	if(!record || record==NULL_TREE || record==error_mark_node){
		return NULL_TREE;
	}
	tree recordType=TREE_TYPE(record);
	tree pointer=build_pointer_type(recordType);
	return pointer;
}

static tree castToParentAndPointerRef(ClassName *refClassName,ClassName *parent,tree exprValue)
{
	tree parentPointer=createClassPointer(parent->sysName);
	if(!parentPointer || parentPointer==NULL_TREE || parentPointer==error_mark_node){
		return exprValue;
	}
	tree childPointer=createClassPointer(refClassName->sysName);
	if(!childPointer || childPointer==NULL_TREE || childPointer==error_mark_node){
		return exprValue;
	}
    tree refVar = build1 (ADDR_EXPR, childPointer, exprValue);
	tree castParent = build1 (NOP_EXPR, parentPointer, refVar);
	tree result=build_indirect_ref (input_location,castParent,RO_ARROW);
	return result;
}

/**
 * 点访问如果在父组件找到变量，需要转化
 */
tree var_call_dot_visit(VarCall *self,tree exprValue,tree component,location_t component_loc,VarRefRelation *item,tree *staticVar)
{
	enum tree_code code=TREE_CODE(exprValue);
	if(code!=VAR_DECL && code!=PARM_DECL && code!=NOP_EXPR && code!=NON_LVALUE_EXPR && code!=ADDR_EXPR){
	    n_debug("var_call_dot_visit 不是变量类型 不是参数类型 不是NOP_EXPR 不是 NON_LVALUE_EXPR 不是 ADDR_EXPR 由系统处理 %s\n",
				get_tree_code_name(code));
		return exprValue;
	}
	n_debug("var_call_dot_visit 00 引用对象: %s 变量名：%s 引用的组件:%s 变量类型:%s\n",
			item->refClass,item->varName,IDENTIFIER_POINTER(component),get_tree_code_name(code));
    //从refclass开始查找到第一次遇到的组件所在的类
	ClassName *refClassName=class_mgr_get_class_name_by_sys(class_mgr_get(),item->refClass);
	ClassName * parentClassName=class_mgr_get_class_by_component(class_mgr_get(),refClassName,component);
	if(parentClassName){
		//说明这个record里有component
	    n_debug("var_call_dot_visit 11 从类%s开始找组件在其中一个类 %s 中找到 %s \n",refClassName->sysName,
				parentClassName->sysName,IDENTIFIER_POINTER(component));
	    if(!strcmp(refClassName->sysName, parentClassName->sysName)){
	        n_debug("var_call_dot_visit 33 结果是相同的 class %s\n",parentClassName->sysName);
            access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),refClassName->sysName);
	    	return exprValue;
	    }
        access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),parentClassName->sysName);
	    tree result=castToParentAndPointerRef(refClassName,parentClassName,exprValue);
	    return result;
	}else{
		  //找是不是引用的static变量
		 parentClassName=var_mgr_get_class_by_static_component(var_mgr_get(),refClassName,component);
		 if(parentClassName){
		     n_debug("var_call_dot_visit 33 从类%s开始找组件在其中一个类 %s 中找到 并且是静态的 %s \n",refClassName->sysName,
					parentClassName->sysName,IDENTIFIER_POINTER(component));
		    if(!strcmp(refClassName->sysName, parentClassName->sysName)){
		        n_debug("var_call_dot_visit 44 结果是相同的 并且是静态的 class %s\n",parentClassName->sysName);
	           access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),refClassName->sysName);
			   *staticVar=var_mgr_get_static_var(var_mgr_get(),parentClassName,component);
			   return exprValue;
		    }
            access_controls_access_var(access_controls_get(),component_loc,IDENTIFIER_POINTER(component),parentClassName->sysName);
		    tree result=castToParent(exprValue,parentClassName->sysName);
			*staticVar=var_mgr_get_static_var(var_mgr_get(),parentClassName,component);
		    return result;
		 }
	}
	return exprValue;
}


/**
 * 只在类中找变量。
 */
FuncAndVarMsg var_call_find_var_at_class(VarCall *self,tree id,ClassName *className)
{
         c_parser *parser=self->parser;
         if(parser->isAet){
            if(className==NULL)
                return ID_NOT_FIND;
            tree classId=aet_utils_create_ident(className->sysName);
            tree classDecl=lookup_name(classId);
            if(!classDecl || classDecl==NULL_TREE || classDecl==error_mark_node){
                return ID_NOT_FIND;
            }
            int re=findVal(self,className,id);
            return re;
         }
         return ID_NOT_FIND;
}


static FuncAndVarMsg findAtAet(VarCall *self,tree id,ClassName *className)
{
     c_parser *parser=self->parser;
     char *idStr=IDENTIFIER_POINTER(id);
     if(parser->isAet){
        if(className==NULL)
            return ID_NOT_FIND;
        tree classId=aet_utils_create_ident(className->sysName);
        tree classDecl=lookup_name(classId);
        if(!classDecl || classDecl==NULL_TREE || classDecl==error_mark_node){
            return ID_NOT_FIND;
        }
        int re=findVal(self,className,id);
        return re;
     }
     return ID_NOT_FIND;
}

/**
 * lookup_name找到由系统处理
 * 在aet中,在本类和父类中找，如果找到返回ISAET_FIND_VAL
 * 找不到,再找静态变量如果找到返回ISAET_FIND_STATIC_VAL,否则返回ID_NOT_FIND,也由系统处理。
 */
FuncAndVarMsg var_call_get_process_var_method(VarCall *self,location_t loc,tree id,ClassName *className)
{
      c_parser *parser=self->parser;
      char *idStr=IDENTIFIER_POINTER(id);
      tree decl = lookup_name (id);
      if(aet_utils_valid_tree(decl)){
          tree global = identifier_global_value (id);
          if(global==decl){
              FuncAndVarMsg ret=findAtAet(self,id,className);
              if(ret==ISAET_FIND_VAL || ret==ISAET_FIND_STATIC_VAL){
                   //如果decl的范围是文件，优先取类变量
                   location_t dloc=DECL_SOURCE_LOCATION(decl);
                   expanded_location xlocx = expand_location(dloc);
                   expanded_location xloc = expand_location(loc);
                   n_warning("%s:%d:%d 全局变量%s(%s:%d:%d)\n\t\t\t\t\t与类%s或它的父类中的变量名相同！,类变量优先。\n",
                           xloc.file,xloc.line, xloc.column,idStr,xlocx.file,xlocx.line, xlocx.column,className->userName);
                   return ret;
              }
          }
          return ID_EXISTS;
      }
      return findAtAet(self,id,className);
}

/**
 * var 重整为  self->var,
 */
void  var_call_add_deref(VarCall *self,tree id,ClassName *className)
{
	  c_parser *parser=self->parser;
      c_token *token=c_parser_peek_token(parser); //id
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+3>AET_MAX_TOKEN){
			error("token太多了");
			return;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+3]);
	  }
	  char *idstr=IDENTIFIER_POINTER(id);
	  aet_utils_create_token(&parser->tokens[0],CPP_NAME,"self",4);
	  aet_utils_create_token(&parser->tokens[1],CPP_DEREF,"->",2);
	  aet_utils_create_token(&parser->tokens[2],CPP_NAME,idstr,strlen(idstr));
	  parser->tokens_avail=tokenCount+3;
	  aet_print_token_in_parser("var_call_add_deref className ---- %d",className->sysName);
}


VarCall *var_call_new()
{
	VarCall *self = n_slice_alloc0 (sizeof(VarCall));
	varCallInit(self);
	return self;
}


