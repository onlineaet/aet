
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
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "classmgr.h"
#include "funchelp.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "aet-c-parser-header.h"
#include "funcmgr.h"
#include "classutil.h"
#include "selectfield.h"

static void funcHelpInit(FuncHelp *self)
{
}

static tree createComponentRef(tree datum,tree selectedField,location_t loc)
{
    tree type = TREE_TYPE (datum);
    tree field = NULL;
    tree ref;
    bool datum_lvalue = lvalue_p (datum);
    field= tree_cons (NULL_TREE, selectedField, NULL_TREE);
    if(!field || field==NULL_TREE || field==error_mark_node)
	  return error_mark_node;

      /* Chain the COMPONENT_REFs if necessary down to the FIELD.
	 This might be better solved in future the way the C++ front
	 end does it - by giving the anonymous entities each a
	 separate name and type, and then have build_component_ref
	 recursively call itself.  We can't do that here.  */
    tree subdatum =TREE_VALUE (field);
//    printf("buildComponentRef 00 ---%p\n",subdatum);
//    printf("buildComponentRef 00xx ---%s\n",get_tree_code_name(TREE_CODE(subdatum)));
//    printf("buildComponentRef 00 field:%s subdatum:%s %s %s %d\n",
//			   get_tree_code_name(TREE_CODE(field)), get_tree_code_name(TREE_CODE(TREE_TYPE (subdatum))),__FILE__,__FUNCTION__,__LINE__);

    int quals;
    tree subtype;
    bool use_datum_quals;

		  /* If this is an rvalue, it does not have qualifiers in C
			 standard terms and we must avoid propagating such
			 qualifiers down to a non-lvalue array that is then
			 converted to a pointer.  */
	    use_datum_quals = (datum_lvalue || TREE_CODE (TREE_TYPE (subdatum)) != ARRAY_TYPE);
	    quals = TYPE_QUALS (strip_array_types (TREE_TYPE (subdatum)));
	    if (use_datum_quals)
	      quals |= TYPE_QUALS (TREE_TYPE (datum));
	   subtype = c_build_qualified_type (TREE_TYPE (subdatum), quals);

	    ref = build3 (COMPONENT_REF, subtype, datum, subdatum,NULL_TREE);
	   n_debug("在funchelp中buildComponentRef 11 sub:%p datum:%p subdatum:%p ref:%p field:%p type:%p %s %s %d\n",
			   subtype, datum, subdatum,ref,field,type,__FILE__,__FUNCTION__,__LINE__);
	   SET_EXPR_LOCATION (ref, loc);
	   if (TREE_READONLY (subdatum) || (use_datum_quals && TREE_READONLY (datum)))
	      TREE_READONLY (ref) = 1;
	   if (TREE_THIS_VOLATILE (subdatum) || (use_datum_quals && TREE_THIS_VOLATILE (datum)))
	      TREE_THIS_VOLATILE (ref) = 1;
	   datum = ref;
      return ref;
}

/**
 * parmOrVar是函数中的第一个参数，也就是self
 * 创建自身的指针引用
 */
tree  func_help_create_itself_deref(FuncHelp *self,tree parmOrVar,tree field,location_t loc)
{
	n_debug("func_help_create_itself_deref 00 创建本身的引用 :\n");
	tree datum=build_indirect_ref (loc,parmOrVar,RO_ARROW);
    tree value = createComponentRef (datum,field,loc);
    return value;
}



static ClassName *findInterfaceAtClass(ClassName *className,ClassName *iface)
{
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(info==NULL)
		return NULL;
	int i;
	for(i=0;i<info->ifaceCount;i++){
		ClassName *item=&info->ifaces[i];
		if(!strcmp(item->sysName,iface->sysName))
			return className;
	}
	return findInterfaceAtClass(&info->parentName,iface);
}

static tree createCastType(ClassName *className)
{
	tree recordId=aet_utils_create_ident(className->sysName);
	tree record=lookup_name(recordId);
	if(!aet_utils_valid_tree(record)){
		error_at(input_location,"没有找到 class %qs",className->sysName);
	}
	tree recordType=TREE_TYPE(record);
	tree pointer=build_pointer_type(recordType);
	return pointer;
}

/**
 * 转化成(&((Abc*)parmOrVal)->ifaceOpenDoor12345))
 * 分两步:
 * 1.接口所在的类，由参数强转(build1) 这里参数代表的类一定是parentName的子类,否则出错。
 * 2.强转成的castParent引用接口(ifaceVarInParentClass 这是一个field)
 */
static tree castInterfaceRef(tree parmOrVar,ClassName *parentName,ClassName *iface,location_t loc)
{
	    tree type=createCastType(iface);
		char ifaceVar[256];
		aet_utils_create_in_class_iface_var(iface->userName,ifaceVar);
		tree ifaceVarInParentClass=class_mgr_get_field_by_name(class_mgr_get(),parentName, ifaceVar);
		if(!aet_utils_valid_tree(ifaceVarInParentClass))
			return NULL_TREE;
		tree parentType=createCastType(parentName);
		tree castParent = build1 (NOP_EXPR, parentType, parmOrVar);
		protected_set_expr_location (castParent, loc);
		//父类引用接口
		tree datum=build_indirect_ref (loc,castParent,RO_ARROW);
	    tree value = createComponentRef (datum,ifaceVarInParentClass,loc);
	    tree refVar = build1 (ADDR_EXPR, type, value);
	    return refVar;
}


/**
 *第一种情况:item->className==className并且 className就是接口
 *又分两种:refVar与className相同，这时只需替换field,如果不同,从refVar递归找接口，
 *如果找不到返回error_mark_node,如果找到。
 *  Abc *abc=new$ Abc(); Abc parent是Second Zhong是接口，由Second实现
 *  ((Zhong*)((Second*)abc))->txyopen(1347582);或((Zhong*)abc)->txyopen(1347582); 匹配第二
 */

tree  func_help_component_ref_interface(FuncHelp *self,tree orgiComponentRef,tree parmOrVar,
		ClassName *refVarClassName,ClassName *className,ClassFunc *selectedFunc,location_t loc,tree *firstParm)
{
        tree selectedField=selectedFunc->fieldDecl;
        //相同并且是接口
		ClassName *iface=className;
		if(!strcmp(refVarClassName->sysName,iface->sysName)){
			tree indirect=TREE_OPERAND(orgiComponentRef,0);
			if(TREE_CODE(indirect)==COMPONENT_REF){ //这里匹配 形式: ((RandomGenerator*)getRandom())->evaluate();getRandom()返回是对象 然后转成接口RandomGenerator
			   // printf("func_help_component_ref_interface 000\n");
                TREE_TYPE(orgiComponentRef)=TREE_TYPE(selectedField);
                TREE_OPERAND(orgiComponentRef,1)= selectedField;
			    return orgiComponentRef;
			}else if(TREE_CODE(indirect)==INDIRECT_REF){//这里匹配 形式: iface->evaluate();iface是接口
#if 0
			    //这是原来的设计2020-4-20
                tree exprValue=TREE_OPERAND(indirect,0);
                tree funcCallVar=build_indirect_ref (loc,exprValue,RO_ARROW);
                tree interfaceCall = createComponentRef (funcCallVar,selectedField,loc);//这里可以现成的取出type,再附给老的
                TREE_TYPE(orgiComponentRef)=TREE_TYPE(interfaceCall);
                TREE_OPERAND(orgiComponentRef,0)= TREE_OPERAND(interfaceCall,0);
                TREE_OPERAND(orgiComponentRef,1)=selectedField;
                printf("func_help_compoent_ref_itself 00 是接口，换field返回 并且refVarClassName==className var:%s 匹配的类:%s\n",
                        refVarClassName->sysName,className->sysName);
                *firstParm=exprValue;
                return orgiComponentRef;
#endif
                 tree exprValue=TREE_OPERAND(indirect,0);
                 TREE_TYPE(orgiComponentRef)=TREE_TYPE(selectedField);
                 TREE_OPERAND(orgiComponentRef,1)=selectedField;
                 n_debug("func_help_compoent_ref_itself 00 是接口，换field返回 并且refVarClassName==className var:%s 匹配的类:%s\n",
                                      refVarClassName->sysName,className->sysName);
                *firstParm=exprValue;
                 return orgiComponentRef;

			}else{
			    aet_print_tree_skip_debug(orgiComponentRef);
			    n_error("调用接口:%s，还未支持。\n",iface->sysName);
			}
	    }else{
            //找到接口所在的父类，然后强转,如果找不到，不处理，返回原来的orgiComponentRef
	    	/**
	    	 * 第二
	    	 */
	    	ClassName *belongClass=findInterfaceAtClass(refVarClassName,iface);
		    printf("func_help_compoent_ref_itself 11 是接口，找接口所属类  var:%s 匹配的类:%s 所属类:%s\n",
		    		refVarClassName->sysName,className->sysName,belongClass->sysName);
	    	if(belongClass==NULL){
	    		return error_mark_node;
	    	}else{
	    		tree refVar=castInterfaceRef(parmOrVar,belongClass,iface,loc);
	    		if(!refVar || refVar==NULL_TREE || refVar==error_mark_node)
	    			return error_mark_node;
	    		tree funcCallVar=build_indirect_ref (loc,refVar,RO_ARROW);
	    	    tree interfaceCall = createComponentRef (funcCallVar,selectedField,loc);//这里可以现成的取出type,再附给老的
	    	    TREE_TYPE(orgiComponentRef)=TREE_TYPE(interfaceCall);
			    TREE_OPERAND(orgiComponentRef,0)= TREE_OPERAND(interfaceCall,0);
			    TREE_OPERAND(orgiComponentRef,1)=selectedField;
			    *firstParm=refVar;
			    return orgiComponentRef;
	    	}
		}
		return error_mark_node;

}

tree        func_help_cast_to_parent(FuncHelp *self,tree parmOrVar,ClassName *parentName)
{
	tree type=createCastType(parentName);
	tree castParent = build1 (NOP_EXPR, type, parmOrVar);
	return castParent;
}

/**
 * 递归生成点调用
 * getRandom().parentObject.parentObject
 */
static tree recursionDotCallLink(char *child,char *lastSysName,location_t loc,tree dot,int *find)
{
          if(!strcmp(child,lastSysName)){
              find=1;
              return dot;
          }
          ClassInfo *childInfo=class_mgr_get_class_info(class_mgr_get(),child);
          if(childInfo->parentName.sysName!=NULL){
              ClassName *parentClassName=&childInfo->parentName;
              char *dotFieldDecl=class_util_create_parent_class_var_name(parentClassName->userName);
              tree id=aet_utils_create_ident(dotFieldDecl);
              tree componentRef = build_component_ref (loc, dot,id, loc);
              if(!strcmp(lastSysName,parentClassName->sysName)){
                  *find=1;
                  return componentRef;
              }else{
                  return recursionDotCallLink(parentClassName->sysName,lastSysName,loc,componentRef,find);
              }
          }
          return NULL_TREE;
}

FuncHelp *func_help_new()
{
	FuncHelp *self = n_slice_alloc0 (sizeof(FuncHelp));
	funcHelpInit(self);
	return self;
}

//////////////////////////---------------------------------------

/**
 * 接口访问
 */
static tree  createInfaceAtClass(tree root,ClassName *parent,ClassName *iface,location_t loc)
{
    tree type=TREE_TYPE(root);
    char *sysName=class_util_get_class_name(type);
    int find=0;
    tree result=recursionDotCallLink(sysName,parent->sysName,loc,root,&find);
    if(!find){
        aet_print_tree_skip_debug(root);
        n_error("在类:%s中找不到父类:%s\n",sysName,parent->sysName);
    }
    ClassInfo *parentInfo=class_mgr_get_class_info(class_mgr_get(),parent->sysName);
    char ifaceVar[256];
    aet_utils_create_in_class_iface_var(iface->userName,ifaceVar);
    tree ifaceVarInParentClass=class_mgr_get_field_by_name(class_mgr_get(),parent, ifaceVar);
    if(!aet_utils_valid_tree(ifaceVarInParentClass)){
        n_error("在类:%s中找不到接口:%s\n",parent->sysName,iface->sysName);
        return NULL_TREE;
    }
    tree ifaceId=aet_utils_create_ident(ifaceVar);
    tree componentRef = build_component_ref (loc, result,ifaceId, loc);
    return componentRef;
}


tree  func_help_create_parent_iface_deref(FuncHelp *self,tree func,ClassName *parentName,ClassName *iface,
        tree ifaceField,location_t loc,tree *firstParm)
{
     int hasNamelessCall= class_util_has_nameless_call(func);
     n_debug("func_help_create_parent_iface_deref 00 nameless:%d 创建父类接口的引用 :parentName:%s iface:%s\n",hasNamelessCall,parentName->sysName,iface->sysName);
     char ifaceVar[256];
     aet_utils_create_in_class_iface_var(iface->userName,ifaceVar);
     tree ifaceVarInParentClass=class_mgr_get_field_by_name(class_mgr_get(),parentName, ifaceVar);
     if(!aet_utils_valid_tree(ifaceVarInParentClass)){
            n_error("在类:%s中找不到接口:%s\n",parentName->sysName,iface->sysName);
            return NULL_TREE;
     }
     tree ifacePointerType=createCastType(iface);

     if(TREE_CODE(func)==FUNCTION_DECL){
          tree selfTree=lookup_name(aet_utils_create_ident("self"));
          tree parentType=createCastType(parentName);
          tree castParent = build1 (NOP_EXPR, parentType, selfTree);//转成(Abc*)self
          tree datum=build_indirect_ref (loc,castParent,RO_ARROW);
          //转成(Abc*)self->iface23829
          tree firstRef= build_component_ref (loc, datum,DECL_NAME(ifaceVarInParentClass), loc);//
          tree ref= build_component_ref (loc, firstRef,DECL_NAME(ifaceField), loc);//
         *firstParm=build1 (ADDR_EXPR, ifacePointerType, firstRef);
          return ref;
     }
     tree value=TREE_OPERAND(func,0);
     if(TREE_CODE(value)==INDIRECT_REF)
         value=TREE_OPERAND (value, 0);
     tree valueType=TREE_TYPE(value);
     if(!hasNamelessCall || hasNamelessCall==1){
        //printf("func_help_create_parent_iface_deref 11 创建父类接口的引用 :parentName:%s iface:%s\n",parentName->sysName,iface->sysName);
        tree pointer=value;
        if(TREE_CODE(valueType)!=POINTER_TYPE){
            pointer = build_unary_op (loc, ADDR_EXPR, value, FALSE);//转指针
        }
        tree parentType=createCastType(parentName);
        tree castParent = build1 (NOP_EXPR, parentType, pointer);//转成(Parent*)value
        tree datum=build_indirect_ref (loc,castParent,RO_ARROW);
             //转成(Abc*)self->iface23829
        tree firstRef= build_component_ref (loc, datum,DECL_NAME(ifaceVarInParentClass), loc);//
        tree ref= build_component_ref (loc, firstRef,DECL_NAME(ifaceField), loc);//
       *firstParm=build1 (ADDR_EXPR, ifacePointerType, firstRef);
        return ref;
    }else if(hasNamelessCall==2){
        /* 有点引用在调用链中,不能进行任何地址转化 否则报
         * 单目‘&’的操作数必须是左值 &(getObject())是错的 lvalue required as unary ‘&’ operand
         * 错误。
         */
        tree ref=createInfaceAtClass(value,parentName,iface,loc);
        tree componentRef = build_component_ref (loc, ref,DECL_NAME(ifaceField), loc);
        *firstParm=null_pointer_node;
        return componentRef;
    }
    return NULL_TREE;
}

/**
 * abc->getData 转 ((Parent*)abc)->getData();
 */
tree  func_help_create_parent_deref(FuncHelp *self,tree func,ClassName *parent,tree parentField,location_t loc,tree *firstParm)
{
     int hasNamelessCall= class_util_has_nameless_call(func);
     n_debug("func_help_create_parent_deref 00 hasNamelessCall:%d",hasNamelessCall,parent->sysName);
     tree componentRef=NULL_TREE;
     if(TREE_CODE(func)==FUNCTION_DECL){
         tree selfTree=lookup_name(aet_utils_create_ident("self"));
         tree parentType=createCastType(parent);
         tree castParent = build1 (NOP_EXPR, parentType, selfTree);
         tree datum=build_indirect_ref (loc,castParent,RO_ARROW);
         componentRef= build_component_ref (loc, datum,DECL_NAME(parentField), loc);
         *firstParm=castParent;
          return componentRef;
     }
     tree value=TREE_OPERAND(func,0);
     tree valueType=TREE_TYPE(value);
     if(TREE_CODE(valueType)==POINTER_TYPE){
         tree type=createCastType(parent);
         tree castParent = build1 (NOP_EXPR, type, value);
         tree datum=build_indirect_ref (loc,castParent,RO_ARROW);
         componentRef= build_component_ref (loc, datum,DECL_NAME(parentField), loc);
         *firstParm=(hasNamelessCall==0)?castParent:null_pointer_node;
     }else{
         tree type=createCastType(parent);
         if(hasNamelessCall!=2){
             tree pointer = build_unary_op (loc, ADDR_EXPR, value, FALSE);
             tree castParent = build1 (NOP_EXPR, type, pointer);
             tree datum=build_indirect_ref (loc,castParent,RO_ARROW);
             componentRef= build_component_ref (loc, datum,DECL_NAME(parentField), loc);
             *firstParm=(hasNamelessCall==0)?castParent:null_pointer_node;
             //aet_print_tree(componentRef);
         }else{
               char *sysName=class_util_get_class_name(valueType);
               ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
               ClassInfo *parentInfo=class_mgr_get_class_info(class_mgr_get(),parent->sysName);
               int find=0;
               tree result=recursionDotCallLink(sysName,parent->sysName,loc,value,&find);
               n_debug("func_help_create_parent_deref 11 hasNamelessCall:%d sysName:%s parent->sysName:%sfind find ----%d",
                       hasNamelessCall,sysName,parent->sysName,find);
               if(find){
                   componentRef = build_component_ref (loc, result,DECL_NAME(parentField), loc);
                   *firstParm=null_pointer_node;
               }
         }

     }
     return componentRef;
}

/**
 * 转化成(&parmOrVal->ifaceOpenDoor12345)
 */
tree  func_help_create_itself_iface_deref(FuncHelp *self,tree func,ClassName *className,
        ClassName *iface,tree ifaceField,location_t loc,tree *firstParm)
{
     int hasNamelessCall= class_util_has_nameless_call(func);
     n_debug("func_help_create_itself_iface_deref 创建本身实现的接口的引用 ---%d %s %s\n",hasNamelessCall,className->sysName,iface->sysName);
     tree ifacePointerType=createCastType(iface);
     char ifaceVar[256];
     aet_utils_create_in_class_iface_var(iface->userName,ifaceVar);
     tree ifaceVarInClass=class_mgr_get_field_by_name(class_mgr_get(),className, ifaceVar);
     if(!aet_utils_valid_tree(ifaceVarInClass))
        return NULL_TREE;
     tree componentRef=NULL_TREE;
     if(TREE_CODE(func)==FUNCTION_DECL){
         //生成self->iface.func();
         tree selfTree=lookup_name(aet_utils_create_ident("self"));
         tree datum=build_indirect_ref (loc,selfTree,RO_ARROW);
         tree firstRef= build_component_ref (loc, datum,DECL_NAME(ifaceVarInClass), loc);
         componentRef= build_component_ref (loc, firstRef,DECL_NAME(ifaceField), loc);
         *firstParm=build1 (ADDR_EXPR, ifacePointerType, firstRef);
         return componentRef;
     }

     tree value=TREE_OPERAND(func,0);
     tree valueType=TREE_TYPE(value);
     if(TREE_CODE(valueType)==POINTER_TYPE){
         //从abc->getData变成 abc->iface.getData();
        tree datum=build_indirect_ref (loc,value,RO_ARROW);
        tree firstRef= build_component_ref (loc, datum,DECL_NAME(ifaceVarInClass), loc);
        componentRef= build_component_ref (loc, firstRef,DECL_NAME(ifaceField), loc);
       *firstParm=(hasNamelessCall==0)?build1 (ADDR_EXPR, ifacePointerType, firstRef):null_pointer_node;
     }else{
         //从abc.getData变成 abc.iface.getData();
         tree firstRef= build_component_ref (loc, value,DECL_NAME(ifaceVarInClass), loc);
         componentRef = build_component_ref (loc, firstRef,DECL_NAME(ifaceField), loc);
         *firstParm=(hasNamelessCall==0)?build1 (ADDR_EXPR, ifacePointerType, firstRef):null_pointer_node;
     }
     return componentRef;
}


/**
 * parmOrVar是函数中的第一个参数，也就是self
 * 创建自身的指针引用
 */
tree  func_help_create_itself_deref_new(FuncHelp *self,tree func,tree field,location_t loc,tree *firstParm)
{
        int hasNamelessCall= class_util_has_nameless_call(func);
        char *fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
        tree value=NULL_TREE;
        tree valueType=NULL_TREE;
        tree componentRef=NULL_TREE;
        if(TREE_CODE(func)==FUNCTION_DECL){
            n_debug("func_help_create_itself_deref_new 00 创建本身的引用 fieldname:%s 来自函数 :hasNamelessCall:%d\n",fieldName,hasNamelessCall);
            tree selfTree=lookup_name(aet_utils_create_ident("self"));
            tree datum=build_indirect_ref (loc,selfTree,RO_ARROW);
            componentRef= build_component_ref (loc, datum,DECL_NAME(field), loc);
            *firstParm=selfTree;
            return componentRef;
        }else{
            value=TREE_OPERAND(func,0);
            valueType=TREE_TYPE(value);
            n_debug("func_help_create_itself_deref_new 11 创建本身的引用 fieldname:%s 来自函数 :hasNamelessCall:%d\n",
                    fieldName,hasNamelessCall,get_tree_code_name(TREE_CODE(valueType)),hasNamelessCall);
        }
        if(TREE_CODE(valueType)==POINTER_TYPE){
            //从abc->getData变成 abc->getData();
            if(TREE_CODE(value)==INDIRECT_REF)
                componentRef= build_component_ref (loc, value,DECL_NAME(field), loc);
            else{
              tree datum=build_indirect_ref (loc,value,RO_ARROW);
              componentRef= build_component_ref (loc, datum,DECL_NAME(field), loc);
            }
         // *firstParm=(hasNamelessCall==0)?value:null_pointer_node;
          *firstParm=(hasNamelessCall==0)?*firstParm:null_pointer_node;

        }else{
            //从abc.getData变成 abc.getData();
            n_debug("func_help_create_itself_deref_new 22 POINTER_TYPE 如果 value是 indirect 它的类型是 record,是用RO_ARROW 创建的 %d",hasNamelessCall);
           nboolean change=FALSE;
           componentRef = build_component_ref (loc, value,DECL_NAME(field), loc);
           if(TREE_CODE(value)==INDIRECT_REF){
               tree op=TREE_OPERAND (value, 0);
               tree type=TREE_TYPE(op);
               if(TREE_CODE(type)==POINTER_TYPE){
                   if(*firstParm==NULL){
                       n_debug("func_help_create_itself_deref_new 33 POINTER_TYPE 如果 value是 indirect 它的类型是 record,是用RO_ARROW 创建的 %d",hasNamelessCall);

                     *firstParm=(hasNamelessCall==0)?op:null_pointer_node;
                     change=TRUE;
                   }
               }
           }
           if(!change){
               *firstParm=(hasNamelessCall==0)?*firstParm:null_pointer_node;
               if(*firstParm==NULL && hasNamelessCall==0){
                   n_debug("func_help_create_itself_deref_new 如果 value是 indirect 它的类型是 record,是用RO_ARROW 创建的 %d",hasNamelessCall);
                   *firstParm=build1 (ADDR_EXPR, build_pointer_type(valueType), value);
                }
           }


          //*firstParm=(hasNamelessCall==0)?build1 (ADDR_EXPR, build_pointer_type(valueType), value):null_pointer_node;
         // *firstParm=(hasNamelessCall==0)?*firstParm:null_pointer_node;
        }
        return componentRef;
}




