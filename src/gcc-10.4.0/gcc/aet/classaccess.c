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
#include "classref.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classaccess.h"
#include "classinfo.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "aet-c-parser-header.h"
#include "genericimpl.h"
#include "genericmodel.h"

static tree getLastVar(tree decl,int *nest);


static void classAcessInit(ClassAccess *self)
{

}


/**
 * 从引用中获取引用字符串
 * 如:self->abc->efg;
 * 在tree 顺序是 efg->abc->self;
 */
static void getRecursionSelfParm(tree componentRef,NPtrArray *array)
{
	   tree indirect=TREE_OPERAND (componentRef, 0);
	   tree op      =TREE_OPERAND (componentRef, 1);
	   char *declName=IDENTIFIER_POINTER(DECL_NAME(op));
       n_ptr_array_add(array,n_strdup(declName));
	   tree compref=TREE_OPERAND (indirect, 0);
	   if(TREE_CODE(compref)==COMPONENT_REF){
		   getRecursionSelfParm(compref,array);
	   }else if(TREE_CODE(compref)==PARM_DECL || TREE_CODE(compref)==VAR_DECL){
		   char *declName=IDENTIFIER_POINTER(DECL_NAME(compref));
	       n_ptr_array_add(array,n_strdup(declName));
	   }else{
		   n_error("getRecursionSelfParm 不能处理的类型 %s",get_tree_code_name(TREE_CODE(compref)));
	   }
}




static tree setPOINTER_PLUS_EXPR(tree plusExpr,int *nest)
{
	   enum tree_code  nopexprTypeCode=TREE_CODE(TREE_TYPE(plusExpr));
	   tree op=TREE_OPERAND (plusExpr, 0);
	   if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==CALL_EXPR){

	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==VAR_DECL){
		   printf("setPOINTER_PLUS_EXPR 00 %d\n",*nest);
		   return getLastVar(op,nest);
	   }
	   return NULL_TREE;
}


static tree setNopExpr(tree nopexpr,int *nest)
{
	   enum tree_code  nopexprTypeCode=TREE_CODE(TREE_TYPE(nopexpr));
	   tree op=TREE_OPERAND (nopexpr, 0);
	   n_debug("class_ref setNopExpr 00 %s op:%s",get_tree_code_name(nopexprTypeCode),get_tree_code_name(TREE_CODE(op)));
	   if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==CALL_EXPR){
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==TARGET_EXPR){

	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==VAR_DECL){
		   return getLastVar(op,nest);
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==PARM_DECL){
		   //如果参数是 void  *返回NULL;
		   if(class_util_is_void_pointer(TREE_TYPE(op))){
			   n_debug("参数是void *,原来是直接返回，引起段错误。为什么原来是这样设计呢？？");
			   return NULL_TREE;
		   }
		   return getLastVar(op,nest);
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==POINTER_PLUS_EXPR){
		    return setPOINTER_PLUS_EXPR(op,nest);
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==ADDR_EXPR){
		   tree var=TREE_OPERAND (op, 0);
		   if(TREE_CODE(var)==VAR_DECL)
			   return var;
		   if(TREE_CODE(var)==ARRAY_REF)
			   return var;
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==COMPONENT_REF){
		   tree fieldDecl=TREE_OPERAND (op, 1);
		   return fieldDecl;
       }
	   return NULL_TREE;
}

static tree setAddrExpr(tree nopexpr,int *nest)
{
	   enum tree_code  nopexprTypeCode=TREE_CODE(TREE_TYPE(nopexpr));
	   tree op=TREE_OPERAND (nopexpr, 0);
	   //printf("class_ref setAddrExpr 00 %s op:%s\n",get_tree_code_name(nopexprTypeCode),get_tree_code_name(TREE_CODE(op)));
	   if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==VAR_DECL){
		   return getLastVar(op,nest);
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==COMPONENT_REF){
		   tree var=TREE_OPERAND (op, 1);
		   if(TREE_CODE(var)==FIELD_DECL){
			  tree type=TREE_TYPE(var);
			  char *className=class_util_get_class_name_by_record(type);
			  //printf("class_ref class_ref setAddrExpr 00 11 op是 COMPONENT_REF %s class:%s\n",IDENTIFIER_POINTER(DECL_NAME(var)),className);
			  if(className!=NULL)
				  return var;
		  }
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==ARRAY_REF){
		  tree componentref=TREE_OPERAND (op, 0);
		  if(TREE_CODE(componentref)==COMPONENT_REF){
			   tree var=TREE_OPERAND (componentref, 1);
			   if(TREE_CODE(var)==FIELD_DECL){
				  tree type=TREE_TYPE(var);
				  if(TREE_CODE(type)==ARRAY_TYPE)
					  type=TREE_TYPE(type);
				  char *className=class_util_get_class_name_by_record(type);
				  //printf("class_ref class_ref setAddrExpr 00 22 op是 COMPONENT_REF %s class:%s\n",IDENTIFIER_POINTER(DECL_NAME(var)),className);
				  if(className!=NULL)
					  return var;
			   }
		  }
	   }
	   return NULL_TREE;
}

static tree setComponentRef(tree componentRef,int *nest)
{
	   tree op=TREE_OPERAND (componentRef, 1);
	   if(TREE_CODE(op)==FIELD_DECL){
		   tree type=TREE_TYPE(op);
		   //printf("class_ref setComponentRef %s %s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(op)));
		   return op;
	   }
	   return NULL_TREE;
}

/**
 * 判断ret是不是void_type
 */
static nboolean isVoidType(tree ret)
{
   tree type=TREE_TYPE(ret);
   if(!aet_utils_valid_tree(type))
       return FALSE;
   int count=100;
   while(count>0){
       if(TREE_CODE(type)==POINTER_TYPE){
           type=TREE_TYPE(type);
       }else if(TREE_CODE(type)==VOID_TYPE){
           return TRUE;
       }else{
           return FALSE;
       }
       count--;
   }
}

static tree getLastVar(tree decl,int *nest)
{
	if(TREE_CODE(decl)==VAR_DECL){
		*nest=*nest+1;
		//检查返回的是不是NClass
	    tree init=DECL_INITIAL(decl);
		if(!aet_utils_valid_tree(init)){
			return decl;
		}
	    if(TREE_CODE(init)==NOP_EXPR){
	        //如果result的类型是void_type,说明可能是强制转化的，返回decl
	        //会引起错误：找不到引用的变量所属类,引用的函数名是:‘center’，变量名:‘network’
	    	tree result= setNopExpr(init,nest);
	    	if(result==NULL_TREE || isVoidType(result))
	    		return decl;
	    	return result;
	    }else if(TREE_CODE(init)==TARGET_EXPR){
	    	return decl;
	    }else if(TREE_CODE(init)==VAR_DECL){
	    	//setVarDecl(self,decl,init,declClassName);
	    }else if(TREE_CODE(init)==PARM_DECL){
	    	//setParmDecl(self,decl,init,declClassName);
	    }else if(TREE_CODE(init)==CALL_EXPR){
	    	//setCallExpr(self,decl,init,declClassName);
	    }
	}else if(TREE_CODE(decl)==PARM_DECL){
		//printf("class_ref getLastVar 22 TREE_CODE(decl)==PARM_DECL nest:%d\n",*nest);
		return decl;
	}else if(TREE_CODE(decl)==NOP_EXPR){
		n_debug("class_ref getLastVar 33 TREE_CODE(decl)==NOP_EXPR nest:%d",*nest);
		*nest=*nest+1;
    	tree ret= setNopExpr(decl,nest);
    	if(ret==NULL_TREE){
    		return decl;
    	}else{
    		return ret;
    	}
	}else if(TREE_CODE(decl)==NON_LVALUE_EXPR){
		n_debug("class_ref getLastVar 44 TREE_CODE(decl)==NON_LVALUE_EXPR nest:%d",*nest);
		*nest=*nest+1;
    	return setNopExpr(decl,nest);
	}else if(TREE_CODE(decl)==ADDR_EXPR){
		n_debug("class_ref getLastVar 55 TREE_CODE(decl)==ADDR_EXPR nest:%d",*nest);
		*nest=*nest+1;
    	return setAddrExpr(decl,nest);
	}else if(TREE_CODE(decl)==COMPONENT_REF){
		n_debug("class_ref getLastVar 66 TREE_CODE(decl)==COMPONENT_REF nest:%d",*nest);
		*nest=*nest+1;
		return setComponentRef(decl,nest);
	}else if(TREE_CODE(decl)==CALL_EXPR){
		n_debug("class_ref getLastVar 77 TREE_CODE(decl)==CALL_EXPR nest:%d",*nest);
		*nest=*nest+1;
		return TREE_TYPE(decl);//函数调用的返回值
	}else if(TREE_CODE(decl)==INDIRECT_REF){
	    tree op=TREE_OPERAND (decl, 0);
	    if(TREE_CODE(op)==POINTER_PLUS_EXPR){
	        op=TREE_OPERAND (op, 0);
	    }
        return op;
	}else if(TREE_CODE(decl)==ARRAY_REF){
        tree op=TREE_OPERAND (decl, 0);
        return op;
	}
	return decl;
}

static char *getClassFromVarAtAddrExpr(tree var)
{
		tree type=TREE_TYPE(var);
		return class_util_get_class_name_by_record(type);
}

/**
 * lows[0] 类名
 * lows[1] 变量名
 */
static char **getLastClassAndVar(ClassAccess *self,tree exprValue,char **lows)
{
	  lows[0]=lows[1]=NULL;
	  int nest=0;
	  tree result=getLastVar(exprValue,&nest);
	  if(TREE_CODE(result)==VAR_DECL || TREE_CODE(result)==PARM_DECL){
		  lows[0]=class_util_get_class_name(TREE_TYPE(result));
		  if(lows[0]==NULL && TREE_CODE(exprValue)==ADDR_EXPR){
			  lows[0]=getClassFromVarAtAddrExpr(result);
		  }else if(lows[0]==NULL && TREE_CODE(TREE_TYPE(result))==RECORD_TYPE){
			  lows[0]=class_util_get_class_name_by_record(TREE_TYPE(result));
		  }else if(lows[0]==NULL && TREE_CODE(TREE_TYPE(result))==ARRAY_TYPE){
		      //Abc *ax[2]; ax是var_decl 类型是 array_type array_type的类型才是Abc *
	          lows[0]=class_util_get_class_name(TREE_TYPE(TREE_TYPE(result)));
		  }else if(lows[0]==NULL && TREE_CODE(TREE_TYPE(result))==POINTER_TYPE && isVoidType(result)){
              // (ABC *)var var是一个void*类型
		      lows[0]=class_util_get_class_name(TREE_TYPE(exprValue));
		  }
		  lows[1]=IDENTIFIER_POINTER(DECL_NAME(result));
	  }else if(TREE_CODE(exprValue)==NOP_EXPR){
		  //printf("class_ref getLowClassNameAndVarName 00 TREE_CODE(exprValue)==NOP_EXPR:%s refVar:%s\n","NULL","NULL");
		  if(TREE_CODE(result)==ARRAY_REF){
      	     tree op=TREE_OPERAND (result, 0);
      	     if(TREE_CODE(op)==VAR_DECL){
			   lows[0]=class_util_get_class_name_by_record(TREE_TYPE(result));
			   lows[1]=IDENTIFIER_POINTER(DECL_NAME(op));
      	     }
		  }else if(TREE_CODE(result)==FIELD_DECL){
	      	   tree type=TREE_TYPE(exprValue);
			   lows[0]=class_util_get_class_name(type);
		       lows[1]=IDENTIFIER_POINTER(DECL_NAME(result));
		  }else if(TREE_CODE(result)==NOP_EXPR){
			   tree type=TREE_TYPE(exprValue);
			   lows[0]=class_util_get_class_name(type);
			   tree op=TREE_OPERAND (result, 0);
			   if(TREE_CODE(op)==VAR_DECL || TREE_CODE(op)==PARM_DECL)
				   lows[1]=IDENTIFIER_POINTER(DECL_NAME(op));
			   printf("由void *object 转成 (AObject*)object进这里 具体类是:%s %s\n",lows[0],lows[1]);
		  }
	  }else if(TREE_CODE(result)==FIELD_DECL){
		   tree type=TREE_TYPE(result);
		   if(TREE_CODE(type)==POINTER_TYPE || TREE_CODE(type)==RECORD_TYPE)
		     lows[0]=class_util_get_class_name(type);
		   else if(TREE_CODE(type)==ARRAY_TYPE)
			 lows[0]=class_util_get_class_name(TREE_TYPE(type));
		   lows[1]=IDENTIFIER_POINTER(DECL_NAME(result));
	  }else if(TREE_CODE(result)==POINTER_TYPE){
		   lows[0]=class_util_get_class_name(result);
	  }else if(TREE_CODE(result)==COMPONENT_REF){
          lows[0]=class_util_get_class_name(TREE_TYPE(result));
          tree fieldDecl=TREE_OPERAND (result, 1);
          lows[1]=IDENTIFIER_POINTER(DECL_NAME(fieldDecl));
	  }else if(TREE_CODE(result)==RECORD_TYPE){
          lows[0]=class_util_get_class_name(result);
	  }
	  //printf("class_ref getLowClassNameAndVarName 11 className:%s refVar:%s exprValue:%s result:%s\n",
			  //lows[0],lows[1],get_tree_code_name(TREE_CODE(exprValue)),result?get_tree_code_name(TREE_CODE(result)):"null");
	  return lows;
}


char *class_access_get_class_name(ClassAccess *self,tree exprValue)
{
  char *className=NULL;
  if(TREE_CODE(exprValue)==VAR_DECL || TREE_CODE(exprValue)==PARM_DECL){
	  className=class_util_get_class_name(TREE_TYPE(exprValue));
	  n_debug("class_ref exprValue 是变量或参数 %s className:%s",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==NOP_EXPR){
	  className=class_util_get_class_name(TREE_TYPE(exprValue));
	  tree op=TREE_OPERAND (exprValue, 0);
	  n_debug("class_ref exprValue 是NOP_EXPR %s className:%s",get_tree_code_name(TREE_CODE(op)),className);
  }else if(TREE_CODE(exprValue)==NON_LVALUE_EXPR){
	  className=class_util_get_class_name(TREE_TYPE(exprValue));
	  tree op=TREE_OPERAND (exprValue, 0);
	  n_debug("class_ref exprValue 是NON_LVALUE_EXPR %s className:%s",get_tree_code_name(TREE_CODE(op)),className);
  }else if(TREE_CODE(exprValue)==ADDR_EXPR){
	  tree op=TREE_OPERAND (exprValue, 0);
	  className=class_util_get_class_name(TREE_TYPE(op));
	  n_debug("class_ref exprValue 是ADDR_EXPR %s className:%s",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==COMPONENT_REF){
	  //表达式可能是self->obj或self->obj->obj1;要找的是最后一个obj
	  tree op=TREE_OPERAND (exprValue, 1);
	  className=class_util_get_class_name(TREE_TYPE(op));
	  n_debug("class_ref exprValue 是引用 %s className:%s",get_tree_code_name(TREE_CODE(op)),className);
  }else if(TREE_CODE(exprValue)==CALL_EXPR){
	  n_debug("class_ref exprValue 是CALL_EXPR %s className:%s",get_tree_code_name(TREE_CODE(exprValue)),className);
	  return class_util_get_class_name(TREE_TYPE(exprValue));
  }else if(TREE_CODE(exprValue)==INDIRECT_REF){
      tree op=TREE_OPERAND (exprValue, 0);
      className=class_util_get_class_name(TREE_TYPE(op));
      n_debug("class_ref exprValue 是INDIRECT_REF %s className:%s",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==ARRAY_REF){
      className=class_util_get_class_name(TREE_TYPE(exprValue));
  }
  return className;
}

void   class_access_set_parser(ClassAccess *self, c_parser *parser,VarCall *varCall,SuperCall *superCall,FuncCall *funcCall)
{
	  self->parser=parser;
	  self->varCall=varCall;
	  self->superCall=superCall;
	  self->funcCall=funcCall;
}

int class_access_get_last_class_and_var(ClassAccess *self,location_t loc,tree component,tree exprValue,char **result)
{
	 c_parser *parser =self->parser;
	 char *className=class_access_get_class_name(self,exprValue);
	 if(className==NULL){
		 //不应该出现这里
		 error_at(loc,"找不到引用的类(不应该出现这样的错误?),引用的函数名是:%qs",IDENTIFIER_POINTER(component));
		 return -1;
	 }
	 char *lows[2];
	 getLastClassAndVar(self,exprValue,lows);
	 if(lows[1]==NULL){
	     n_debug("lows[1]是空的 sysName:%s lows[0]:%s",className,lows[0]);
	 }
	 if(lows[0]==NULL){
        aet_print_tree_skip_debug(exprValue);
        //printf("找不到引用的变量所属类,引用的函数名是:%s，变量名:%s\n",IDENTIFIER_POINTER(component),lows[1]);
		//error_at(loc,"找不到引用的变量所属类,引用的函数名是:%qs，变量名:%qs",IDENTIFIER_POINTER(component),lows[1]);
        n_error("找不到引用的变量所属类,引用的函数名是:%s，变量名:%s 报告此错误。",IDENTIFIER_POINTER(component),lows[1]);
		return -1;
	 }
	 char *lowestClass=NULL;
	 ClassRelationship relationship= class_mgr_relationship(class_mgr_get(),lows[0],className);
	 if(relationship==CLASS_RELATIONSHIP_PARENT){
		 n_debug("说明lows 是父子关系 parent:%s child:%s",lows[0],className);
		 lowestClass=className;
	 }else if(relationship==CLASS_RELATIONSHIP_CHILD){
		 n_debug("说明lows 是子父关系 parent:%s child:%s",className,lows[0]);
		 lowestClass=lows[0];
	 }else{
		 n_debug("说明lows 其它关系 ship:%d lows[0]:%s className:%s 变量名:lows[1]:%s",relationship,lows[0],className,lows[1]);
		 lowestClass=className;
	 }
	 result[0]=lows[0];
	 result[1]=lows[1];
	 result[2]=lowestClass;
	 result[3]=className;
	 return relationship;
}

VarRefRelation *class_access_create_relation_ship(ClassAccess *self,location_t loc,tree component,tree exprValue)
{
	 c_parser *parser =self->parser;
	 char *lows[4];
	 int relationShip=class_access_get_last_class_and_var(self,loc,component,exprValue,lows);
	 if(relationShip<0)
		 return NULL;
	 char *lowestClass=lows[2];
	 char *className=lows[3];
	 VarRefRelation *item=(VarRefRelation *)n_slice_new0(VarRefRelation);
	 item->refClass=n_strdup(className);
	 item->lowClass=n_strdup(lows[0]);
	 item->varName=n_strdup(lows[1]);
	 item->relationship=relationShip;
	 return item;
}

tree class_access_create_temp_field(ClassAccess *self,char *sysName, tree component,location_t loc)
{
	 tree classId=aet_utils_create_ident(sysName);
	 tree record=lookup_name(classId);
	 if(!record || record==error_mark_node){
		 error_at(loc,"类%qs找不到",sysName);
		 return error_mark_node;
	 }
	 tree recordType=NULL_TREE;
	 if(TREE_CODE(record)==FUNCTION_DECL){
	     if(strcmp(IDENTIFIER_POINTER(DECL_NAME(record)),sysName)==0){
	         printf("说明在构造函数内调用了同类的方法:%s\n",sysName);
	         ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	         record=info->recordTypeDecl;
	     }else{
	         n_error("在函数class_access_create_temp_field发生不可知的错误。报告此错误。");
	         return NULL_TREE;
	     }
	 }
     recordType=TREE_TYPE(record);
	 tree type=build_pointer_type(recordType);
	 tree  field = build_decl (loc,FIELD_DECL, component, type);
	 return tree_cons (NULL_TREE, field, NULL_TREE);
}

/**
 * TREE_CODE(datum)!=VAR_DECL 是因为 引用了变量的地址也即变量转地址 (&varName)
 */
tree class_access_build_field_ref(ClassAccess *self,location_t loc, tree datum, tree field)
{
	if(TREE_CODE(datum)!=INDIRECT_REF && TREE_CODE(datum)!=VAR_DECL && TREE_CODE(datum)!=PARM_DECL &&
	        TREE_CODE(datum)!=COMPONENT_REF	&& TREE_CODE(datum)!=ARRAY_REF && TREE_CODE(datum)!=CALL_EXPR){
		n_warning("buildClassFieldRef ----不是 INDIRECT_REF VAR_DECL COMPONENT_REF ARRAY_REF 是:%s datum:%p",get_tree_code_name(TREE_CODE(datum)),datum);
		return error_mark_node;
	}
    tree type = TREE_TYPE (datum);
    tree ref;
    bool datum_lvalue = lvalue_p (datum);
      /* Chain the COMPONENT_REFs if necessary down to the FIELD.
	 This might be better solved in future the way the C++ front
	 end does it - by giving the anonymous entities each a
	 separate name and type, and then have build_component_ref
	 recursively call itself.  We can't do that here.  */
    tree subdatum =TREE_VALUE (field);
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
   //ref的DECL_LANG_SPECIFIC会是0x02 0x1003或0x80000等 引起判断空出错 struct c_lang 引发重新实现c-aet中DECL_LANG_SPECIFIC_GENERIC等宏。
   ref = build3 (COMPONENT_REF, subtype, datum, subdatum,NULL_TREE);
   SET_EXPR_LOCATION (ref, loc);
   if (TREE_READONLY (subdatum) || (use_datum_quals && TREE_READONLY (datum)))
	  TREE_READONLY (ref) = 1;
   if (TREE_THIS_VOLATILE (subdatum) || (use_datum_quals && TREE_THIS_VOLATILE (datum)))
	  TREE_THIS_VOLATILE (ref) = 1;
   datum = ref;
   return ref;
}


