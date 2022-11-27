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
#include "tree-core.h"
#include "toplev.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "opts.h"
#include "tree-iterator.h"
#include "c/c-lang.h"

#include "aetinfo.h"
#include "aetutils.h"
#include "classutil.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "c-aet.h"
#include "genericutil.h"


char *class_util_get_class_name_by_record(tree type)
{
	if(TREE_CODE(type)!=RECORD_TYPE){
		//n_warning("class_util_get_class_name_by_record 00 不是record_type %s",get_tree_code_name(TREE_CODE(type)));
		if(TREE_CODE(type)==POINTER_TYPE){
			//n_warning("class_util_get_class_name_by_record 11 可能是 Abc **obj %s",get_tree_code_name(TREE_CODE(type)));
			return class_util_get_class_name_by_record(TREE_TYPE(type));
		}
		return NULL;
	}
	tree next=TYPE_NAME(type);
	if(!aet_utils_valid_tree(next))
	    return NULL;
	if(TREE_CODE(next)!=TYPE_DECL){
		//n_warning("class_util_get_class_name_by_record 11 不是TYPE_DECL %s",get_tree_code_name(TREE_CODE(next)));
		return NULL;
	}
	char *declClassName=IDENTIFIER_POINTER(DECL_NAME (next));
	if(!class_mgr_is_class(class_mgr_get(),declClassName)){
		//n_warning("class_util_get_class_name_by_record 22 找不类 %s",declClassName);
		return NULL;
	}
	return declClassName;
}

static tree getTypeFromPointer(tree pointerType)
{
	if(TREE_CODE(pointerType)!=POINTER_TYPE)
		return pointerType;
	pointerType=TREE_TYPE(pointerType);
	return getTypeFromPointer(pointerType);
}

char *class_util_get_class_name_by_pointer(tree pointerType)
{
	if(TREE_CODE(pointerType)!=POINTER_TYPE)
		return NULL;
	tree record=getTypeFromPointer(pointerType);
	return class_util_get_class_name_by_record(record);
}

char *class_util_get_class_name(tree type)
{
    if(!aet_utils_valid_tree(type))
        return NULL;
	if(TREE_CODE(type)==POINTER_TYPE)
		return class_util_get_class_name_by_pointer(type);
	else if(TREE_CODE(type)==RECORD_TYPE)
		return class_util_get_class_name_by_record(type);
	else if(TREE_CODE(type)==ARRAY_TYPE){
	    tree vt=TREE_TYPE(type);
	    if(TREE_CODE(vt)==POINTER_TYPE)
	       return class_util_get_class_name_by_pointer(vt);
	    else if(TREE_CODE(vt)==RECORD_TYPE)
	       return class_util_get_class_name_by_record(vt);
	}
	return NULL;
}

/**
 * 调用链中是否有无名函数调用：
 * getRandom().obj->getooo();
 * getRandom() 返回值是类对象，所以在这个 tree中有无名函数调用
 */

static int generatorCallLink(tree value,NPtrArray *callLink)
{
        if(TREE_CODE(value)==FUNCTION_DECL){
            n_ptr_array_add(callLink,value);
        }else if(TREE_CODE(value)==CALL_EXPR){
            tree fn= CALL_EXPR_FN (value);
            return generatorCallLink(fn,callLink);
        }else if(TREE_CODE(value)==COMPONENT_REF){
            tree ref=TREE_OPERAND (value, 0);
            n_ptr_array_add(callLink,value);
            return generatorCallLink(ref,callLink);
        }else{
            int length = TREE_OPERAND_LENGTH (value);
            if(length>0){
                tree ref=TREE_OPERAND (value, 0);
                return generatorCallLink(ref,callLink);
            }
        }
        return 0;
}

static int fillLink(tree call,NString *linkStr,nboolean tailed)
{
     int result=0;
     char *name=NULL;
     tree type=NULL_TREE;
     tree parm=NULL_TREE;
     nboolean isFunc=FALSE;
     if(TREE_CODE(call)==FUNCTION_DECL){
        name=IDENTIFIER_POINTER(DECL_NAME(call));
       // printf("函数返回值----xxx: %s\n",name);
        type=TREE_TYPE(call);
        parm=type;
        type=TREE_TYPE(type);//函数返回值
        isFunc=TRUE;
     }else if(TREE_CODE(call)==COMPONENT_REF){
         tree field=TREE_OPERAND (call, 1);
         if(!DECL_NAME(field)){
             printf("在classutil.c fillLink() TREE_CODE(call)==COMPONENT_REF field没有名字。\n");
             name=NULL;
         }else{
             name=IDENTIFIER_POINTER(DECL_NAME(field));
         }
         type=TREE_TYPE(field);
         parm=type;
         //printf("函数返回值:xxxxx %s\n",name);

         if(TREE_CODE(type)==POINTER_TYPE){
             type=TREE_TYPE(type);//函数返回值
             if(TREE_CODE(type)==FUNCTION_TYPE){
                 parm=type;
                 type=TREE_TYPE(type);//函数返回值
                 isFunc=TRUE;
             }
         }
         if(!isFunc)
             parm=field;
     }
     char *sysName=class_util_get_class_name(type);
     nboolean isPointer=TREE_CODE(type)==POINTER_TYPE;
     if(sysName!=NULL && isFunc){
            result=isPointer?1:2;
     }
     char *retn=generic_util_get_type_str(parm);
     if(tailed){ //最后一个不记入无名调用类型的判断
         result=0;
         if(isFunc)
             n_string_append_printf(linkStr,"(%s)%s()",retn,name);
         else
             n_string_append_printf(linkStr,"(%s)%s",retn,name);
     }else{
         if(isFunc)
             n_string_append_printf(linkStr,"(%s)%s()%s",retn,name,isPointer?"->":".");
         else
             n_string_append_printf(linkStr,"(%s)%s%s",retn,name,isPointer?"->":".");
     }
     return result;
}

/**
 * 无名调用的类型 0 无 1 指针类型 2 类对象
 */
int class_util_has_nameless_call(tree value)
{
    int result=0;
    NPtrArray *array=n_ptr_array_new();
    generatorCallLink(value,array);
    int i;
    NString *linkStr=n_string_new("");
    for(i=array->len-1;i>=0;i--){
         tree call=n_ptr_array_index(array,i);
         int r= fillLink(call,linkStr,i==0);
         if(r>result)
             result=r;
    }
   // printf("class_util_has_nameless_call 调用链:result:%d\n%s\n",result,linkStr->str);
    n_string_free(linkStr,TRUE);
    n_ptr_array_unref(array);
    return result;
}

int class_util_get_nameless_call_link(tree value,NPtrArray *array,char **link)
{
    int result=0;
    generatorCallLink(value,array);
    int i;
    NString *linkStr=n_string_new("");
    for(i=array->len-1;i>=0;i--){
         tree call=n_ptr_array_index(array,i);
         int r= fillLink(call,linkStr,i==0);
         if(r>result)
             result=r;
    }
    n_debug("class_util_has_nameless_call 调用链:result:%d\n%s\n",result,linkStr->str);
    if(link)
      *link=n_strdup(linkStr->str);
    n_string_free(linkStr,TRUE);
    return result;
}


/**
 * 转成指针形式的self
 * xxx.object
 * xxx->object
 * xxx.yyy->object;
 * 最终到要在funcall被真正的self替换。
 */
static tree findSelf(tree value)
{
	tree indirectRef=TREE_OPERAND (value, 0);
	tree selfTree=NULL_TREE;
	if(TREE_CODE(indirectRef)==INDIRECT_REF){
        selfTree=TREE_OPERAND (indirectRef, 0);
	}else if(TREE_CODE(indirectRef)==VAR_DECL || TREE_CODE(indirectRef)==PARM_DECL){
		tree type=TREE_TYPE(indirectRef);
		if(TREE_CODE(type)==RECORD_TYPE){
			//此种情况(&abc)->getName(5);
			char *className=class_util_get_class_name_by_record(type);
			n_debug("这是一个点引用。");
			if(className){
				selfTree = build_unary_op (input_location, ADDR_EXPR, indirectRef, false);
			}
		}
	}else if(TREE_CODE(indirectRef)==COMPONENT_REF){
        tree var=TREE_OPERAND (indirectRef, 0);
        tree field=TREE_OPERAND (indirectRef, 1);
		tree type=TREE_TYPE(indirectRef);
        if((TREE_CODE(var)==VAR_DECL || TREE_CODE(var)==PARM_DECL) && TREE_CODE(field)==FIELD_DECL && TREE_CODE(type)==RECORD_TYPE){
			//此种情况(&(abc.ytx))->getName(5);
        	char *className=class_util_get_class_name_by_record(type);
        	n_debug("这是一个点点引用 %s\n",className);
			if(className){
				selfTree = build_unary_op (input_location, ADDR_EXPR, indirectRef, false);
			}
        }else if(TREE_CODE(var)==INDIRECT_REF && TREE_CODE(field)==FIELD_DECL && TREE_CODE(type)==RECORD_TYPE){
         	char *className=class_util_get_class_name_by_record(type);
			//此种情况(&(self->ytx))->getName(5);
         	n_debug("这是一个指针到点引用 %s\n",className);
			if(className){
				selfTree = build_unary_op (input_location, ADDR_EXPR, indirectRef, false);
			}
        }
	}else if(TREE_CODE(indirectRef)==ARRAY_REF){
    		tree type=TREE_TYPE(indirectRef);
        	char *className=class_util_get_class_name_by_record(type);
        	n_debug("这是一个数组引用 %s\n",className);
			//此种情况(&(abc.ytx[0]))->getName(5);
			if(className){
				selfTree = build_unary_op (input_location, ADDR_EXPR, indirectRef, false);
			    aet_print_tree(selfTree);
			}
	}else  if(TREE_CODE(indirectRef)==CALL_EXPR){
	     tree type=TREE_TYPE(indirectRef);
         if(TREE_CODE(type)==RECORD_TYPE){
            char *sysName=class_util_get_class_name_by_record(type);
            n_debug("这是一个函数返回值是类对象的点调用 %s\n",sysName);
            if(sysName){
                selfTree = null_pointer_node;
                printf("单目‘&’的操作数必须是左值 &(getObject())是错的 lvalue required as unary ‘&’ operand\n");
            }
         }
	}else{
        printf("找不到self-----\n");
	    aet_print_tree_skip_debug(value);
        selfTree = null_pointer_node;
	}

	return selfTree;
}

/**
 * expr.value一定是一个component_ref
 */
nboolean  class_util_add_self(struct c_expr expr,vec<tree, va_gc> **exprlist,vec<tree, va_gc> **porigtypes,vec<location_t> *arg_loc)
{
	tree selfTree=findSelf(expr.value);
	if(!aet_utils_valid_tree(selfTree)){
	    aet_print_tree_skip_debug(expr.value);
	    error_at(expr.get_start (),"指针类型的调用变成了点调用。");
	    return FALSE;
	}
    if(*exprlist==NULL){
        vec<tree, va_gc> *ret;
        ret = make_tree_vector ();
        vec<tree, va_gc> *orig_types;
        orig_types = make_tree_vector ();
        vec_safe_push(ret,selfTree);
        vec_safe_push(orig_types,TREE_TYPE(selfTree));
        arg_loc->safe_push (input_location);
        *exprlist=ret;
        *porigtypes=orig_types;
       //printf("class_util_add_self 00 没有参数\n");
    }else{
        vec<tree, va_gc> *args=*exprlist;
//        int ix;
//        tree arg;
//	    for (ix = 0; args->iterate (ix, &arg); ++ix){
//	        printf("class_util_add_self 11 有参数 %d\n",ix);
//	        aet_print_tree(arg);
//	    }
	    vec_safe_insert(*exprlist,0,selfTree);
	    vec_safe_insert(*porigtypes,0,TREE_TYPE(selfTree));
	    arg_loc->safe_insert (0,input_location);
//	    args=*exprlist;
//	    arg=NULL_TREE;
//        printf("class_util_add_self 22 有参数\n");
//	    for (ix = 0; args->iterate (ix, &arg); ++ix){
//	        printf("class_util_add_self 33 有参数 %d\n",ix);
//	        aet_print_tree(arg);
//	    }
    }
    return TRUE;
}


CreateClassMethod class_util_get_create_method(tree selfexpr)
{
    if(TREE_CODE(selfexpr)==VAR_DECL)
    	return c_aet_get_create_method(selfexpr);
    else if(TREE_CODE(selfexpr)==ADDR_EXPR){
        tree selfTree=TREE_OPERAND (selfexpr, 0);
        if(TREE_CODE(selfTree)==VAR_DECL)
         	return c_aet_get_create_method(selfTree);
    }else if(TREE_CODE(selfexpr)==NOP_EXPR){
	   tree selfTree=TREE_OPERAND (selfexpr, 0);
	   if(TREE_CODE(selfTree)==VAR_DECL || TREE_CODE(selfTree)==PARM_DECL){
		  //printf("class_util_get_create_method---NOP_EXPR- %d\n",c_aet_get_create_method(selfTree));
		  return c_aet_get_create_method(selfTree);
	   }
    }else if(TREE_CODE(selfexpr)==PARM_DECL){
        //在构造函数里调用self(),后到这里
        return CREATE_CLASS_METHOD_UNKNOWN;
    }else{
    	aet_print_tree_skip_debug(selfexpr);
    	n_error("class_util_get_create_method 不知什么类型\n");
    }
    return CREATE_CLASS_METHOD_UNKNOWN;
}

nboolean class_util_is_class(struct c_declspecs *declspecs)
{
	if(declspecs->typespec_kind == ctsk_typedef){
		tree spec=declspecs->type;
		if(TREE_CODE(spec)==RECORD_TYPE){
			tree name=TYPE_NAME(spec);
			if(TREE_CODE(name)==TYPE_DECL){
				tree id=DECL_NAME(name);
				char *sysClassName=IDENTIFIER_POINTER(id);
				ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
				if(className!=NULL){
			    	n_debug("class_util_is_class name:%s", className->sysName);
					return TRUE;
				}
			}
		}
		aet_print_tree(spec);
	}
	return FALSE;
}


static unsigned int tempVarSerial=0;
char  *class_util_create_new_object_temp_var_name(char *sysClassName,CreateClassMethod method)
{
   char buffer[256];
   sprintf(buffer,"_notv%d_%d%s%d",method,strlen(sysClassName),sysClassName,tempVarSerial++);
   if(tempVarSerial>N_MAXUINT)
	   tempVarSerial=0;
   char *re=n_strdup(buffer);
   return re;
}

CreateClassMethod class_util_get_new_object_method(char *var)
{
	  if(var==NULL)
		  return CREATE_CLASS_METHOD_UNKNOWN;
	  int len=strlen(var);
	  char *temp=var;
	  //printf("class_util_get_new_object_method 00 %s\n",temp);
	  if(len<10 || strncmp(var,"_notv",5)){
		  return  CREATE_CLASS_METHOD_UNKNOWN;
	  }
	  temp=var+5;
	  //printf("class_util_get_new_object_method 11 %s %d\n",temp,temp[0]);
	  if(!n_ascii_isdigit(temp[0])){
		  return  CREATE_CLASS_METHOD_UNKNOWN;
	  }
	  CreateClassMethod method=n_ascii_digit_value(temp[0]);
	 // printf("class_util_get_new_object_method 22 %s %d\n",temp,method);

	  if(!(method>=CREATE_OBJECT_METHOD_STACK && method<=CREATE_OBJECT_USE_HEAP)){
		  return  CREATE_CLASS_METHOD_UNKNOWN;
	  }
	  temp=var+6;
	  //printf("class_util_get_new_object_method 33 %s %d\n",temp,method);

	  if(temp[0]!='_'){
		  return  CREATE_CLASS_METHOD_UNKNOWN;
	  }
	  temp=var+7;
	  //printf("class_util_get_new_object_method 44 %s %d\n",temp,method);

	  if(!n_ascii_isdigit(temp[0]))
		  return  CREATE_CLASS_METHOD_UNKNOWN;
	  char classNameLen[20];
	  int i=0;
	  while(n_ascii_isdigit(temp[i])){
	     classNameLen[i]=temp[i];
	     i++;
	  }
	  classNameLen[i]='\0';
	  //printf("class_util_get_new_object_method 55 %s %s\n",temp,classNameLen);

	  int objLen=atoi((const char*)classNameLen);
	  temp=var+7+i;
	  char className[255];
	  memcpy(className,temp,objLen);
	  className[objLen]='\0';
 	  ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),className);
 	  if(info==NULL)
 		  return CREATE_CLASS_METHOD_UNKNOWN;

	  temp=var+7+i+objLen;
	  int diff=len-(7+i+objLen);
	 // printf("class_util_get_new_object_method 66 %s %d\n",temp,diff);

	  for(i=0;i<diff;i++){
	    char a=temp[i];
	    if(!n_ascii_isdigit(a)){
		  return  CREATE_CLASS_METHOD_UNKNOWN;
	    }
	  }
      return method;
}

static char* getClassFromNopExpr(tree nopexpr)
{
	   enum tree_code  nopexprTypeCode=TREE_CODE(TREE_TYPE(nopexpr));
	   tree op=TREE_OPERAND (nopexpr, 0);
	   //printf("class_ref getClassFromNopExpr 00 %s %s\n",get_tree_code_name(nopexprTypeCode),get_tree_code_name(TREE_CODE(op)));
	   if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==CALL_EXPR){
		    printf("class_util 从设setNopExpr CALL_EXPR-----\n");
			return class_util_get_class_name_by_pointer(TREE_TYPE(nopexpr));
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==TARGET_EXPR){
		    printf("class_util 从设setNopExpr 再设setTargetExpr----- 不处理\n");
	       aet_print_tree(nopexpr);
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==VAR_DECL){
		    printf("class_util 从设setNopExpr 再设setVarDecl---eee--\n");
			return class_util_get_class_name_by_pointer(TREE_TYPE(nopexpr));
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==PARM_DECL){
			return class_util_get_class_name_by_pointer(TREE_TYPE(nopexpr));
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==POINTER_PLUS_EXPR){
		    printf("class_util 从设setNopExpr POINTER_PLUS_EXPR-----不处理 \n");
		    aet_print_tree(nopexpr);
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==ADDR_EXPR){
			return class_util_get_class_name_by_pointer(TREE_TYPE(nopexpr));
	   }
	   return NULL;
}

static char* getClassFromAddrExpr(tree addrexpr)
{
	   char *className=NULL;
	   enum tree_code  addrexprTypeCode=TREE_CODE(TREE_TYPE(addrexpr));
	   tree op=TREE_OPERAND (addrexpr, 0);
	   if(addrexprTypeCode==POINTER_TYPE && TREE_CODE(op)==VAR_DECL){
		  className=class_util_get_class_name_by_pointer(TREE_TYPE(addrexpr));
		  printf("class_ref getClassFromAddrExpr 00 op是 VAR_DECL %s %s class:%s\n",get_tree_code_name(addrexprTypeCode),
				  get_tree_code_name(TREE_CODE(op)),className);
	   }else if(addrexprTypeCode==POINTER_TYPE && TREE_CODE(op)==COMPONENT_REF){
		  tree var=TREE_OPERAND (op, 1);
		  if(TREE_CODE(var)==FIELD_DECL){
			  tree type=TREE_TYPE(var);
			   className=class_util_get_class_name_by_record(type);
			  printf("class_ref getClassFromAddrExpr 11 op是 COMPONENT_REF %s %s class:%s\n",get_tree_code_name(addrexprTypeCode),
						  get_tree_code_name(TREE_CODE(op)),className);
		  }
	   }else if(addrexprTypeCode==POINTER_TYPE && TREE_CODE(op)==ARRAY_REF){
		    tree type=TREE_TYPE(op);
			if(TREE_CODE(type)==RECORD_TYPE){
			   className=class_util_get_class_name_by_record(type);
				  printf("class_ref getClassFromAddrExpr 22 op是 ARRAY_REF %s %s class:%s\n",get_tree_code_name(addrexprTypeCode),
								  get_tree_code_name(TREE_CODE(op)),className);
			}
	   }

	   return className;
}

static char* getClassFromComponentRef(tree componentRef)
{
	   char *className=NULL;
	   tree op=TREE_OPERAND (componentRef, 1);
	   if(TREE_CODE(op)==FIELD_DECL){
		   tree type=TREE_TYPE(op);
		   className=class_util_get_class_name_by_pointer(type);
	   }
       return className;
}

static char *getClass(tree exprValue)
{
  char *className=NULL;
  if(TREE_CODE(exprValue)==VAR_DECL || TREE_CODE(exprValue)==PARM_DECL){
      tree pointerType=TREE_TYPE(exprValue);
	  return class_util_get_class_name_by_pointer(pointerType);
  }else if(TREE_CODE(exprValue)==NOP_EXPR){
	  className=getClassFromNopExpr(exprValue);
	  printf("class_ref exprValue 是NOP_EXPR %s className:%s\n",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==NON_LVALUE_EXPR){
	  className=getClassFromNopExpr(exprValue);
	  printf("class_ref exprValue 是NON_LVALUE_EXPR %s className:%s\n",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==ADDR_EXPR){
	  className=getClassFromAddrExpr(exprValue);
	  printf("class_ref exprValue 是ADDR_EXPR %s className:%s\n",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==COMPONENT_REF){
	  //表达式可能是self->obj或self->obj->obj1;要找的是最后一个obj
	  className=getClassFromComponentRef(exprValue);
	  printf("class_ref exprValue 是引用 %s className:%s\n",get_tree_code_name(TREE_CODE(exprValue)),className);
  }else if(TREE_CODE(exprValue)==CALL_EXPR){
	  aet_print_tree(TREE_TYPE(exprValue));
	  return class_util_get_class_name_by_pointer(TREE_TYPE(exprValue));
  }
  return className;
}

char *class_util_get_class_name_from_expr(tree exprValue)
{
	if(!aet_utils_valid_tree(exprValue)){
	   return NULL;
	}
	char *className=getClass(exprValue);
	return className;
}

/**
 * 类型的指针次数 如 int * var 一次 int **var两次....
 */
int      class_util_get_pointers(tree type)
{
    int i;
    for(i=0;i<100;i++){
    	if(TREE_CODE(type)!=POINTER_TYPE)
    		return i;
    	type=TREE_TYPE(type);
    }
    return i;
}

nboolean class_util_is_void_pointer(tree type)
{
    int i;
    nboolean isVoid=FALSE;
    for(i=0;i<100;i++){
    	if(TREE_CODE(type)!=POINTER_TYPE){
    		isVoid=TREE_CODE(type)==VOID_TYPE;
    		break;
    	}
    	type=TREE_TYPE(type);
    }
    return isVoid && i>0;
}

nboolean  class_util_is_function(struct c_declarator *declarator)
{
	struct c_declarator *temp=declarator;
	while(temp!=NULL){
	   if(temp->kind==cdk_function){
		   return TRUE;
	   }
	   temp=temp->declarator;
	}
	return FALSE;
}

struct c_declarator * class_util_get_function_declarator(struct c_declarator *declarator)
{
	struct c_declarator *temp=declarator;
	while(temp!=NULL){
	   if(temp->kind==cdk_function){
		   return temp;
	   }
	   temp=temp->declarator;
	}
	return NULL;
}

/**
 * 获得函数的ID，也即函数名
 */
struct c_declarator *class_util_get_function_id(struct c_declarator *funcdel)
{
    struct c_declarator *funid=funcdel->declarator;
    if(funid==NULL)
        return NULL;
    enum c_declarator_kind kind=funid->kind;
    if(kind==cdk_id)
        return funid;
    if(kind==cdk_pointer){
        funid=funid->declarator;
        if(funid==NULL)
            return NULL;
        kind=funid->kind;
        if(kind!=cdk_id)
            return NULL;
        return funid;
    }
    return NULL;
}


char *class_util_get_option()
{
	int i;
	for(i=0;i<save_decoded_options_count;i++){
		struct cl_decoded_option item=save_decoded_options[i];
		printf("class_util_get_option %d %s\n",i,item.warn_message);
		printf("class_util_get_option %d %s\n",i,item.arg);
		printf("class_util_get_option %d %s\n",i,item.orig_option_with_args_text);
		printf("class_util_get_option %d %s\n",i,item.canonical_option[0]);
		printf("class_util_get_option %d %s\n",i,item.canonical_option[1]);
		printf("class_util_get_option %d %s\n",i,item.canonical_option[2]);
		printf("class_util_get_option %d %s\n",i,item.canonical_option[3]);
	}
	return NULL;
}


nint class_util_get_random_number ()
{
  unsigned int ret = 0;
  int fd;
  fd = open ("/dev/urandom", O_RDONLY);
  if (fd >= 0)
    {
      read (fd, &ret, sizeof (int));
      close (fd);
      if (ret)
        return ret;
    }

    struct timeval tv;
    gettimeofday (&tv, NULL);
    ret = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return ret ^ getpid ();
}


tree class_util_create_null_tree()
{
	tree type=build_pointer_type(integer_type_node);
    tree re = build_int_cst (type, 0);
    return re;
}

/**
 * 取当前目标文件名。
 */
char *class_util_get_o_file()
{
	int i;
	for(i=0;i<save_decoded_options_count;i++){
		struct cl_decoded_option item=save_decoded_options[i];
		if(item.arg!=NULL){
			int len=strlen(item.arg);
			if(len>2 && item.arg[len-1]=='o' && item.arg[len-2]=='.')
				return item.arg;
		}
	}
	return NULL;
}

/**
 * formulaType是原参数的类型
 * actualType 是实参的类型
 * #define COMPARE_ARGUMENT_ORIG_PARM_SKIP   0
 * #define COMPARE_ARGUMENT_ORIG_PARM_ERROR -1
 * #define COMPARE_ARGUMENT_ORIG_PARM_OK     1
 */
int class_util_check_param_type(tree formulaType,tree actualType)
{
   enum tree_code code1=TREE_CODE(formulaType);
   enum tree_code code2=TREE_CODE(actualType);
   if((code1==POINTER_TYPE && code2!=POINTER_TYPE) || (code1!=POINTER_TYPE && code2==POINTER_TYPE))
	   return COMPARE_ARGUMENT_ORIG_PARM_SKIP;
   char *formulaSysName=class_util_get_class_name(formulaType);
   char *actualSysName=class_util_get_class_name(actualType);
   if(formulaSysName==NULL || actualSysName==NULL)
	   return COMPARE_ARGUMENT_ORIG_PARM_SKIP;
   if(!strcmp(formulaSysName,actualSysName)){
	   n_debug("实参%s与形参%s一样。",actualSysName,formulaSysName);
	   return COMPARE_ARGUMENT_ORIG_PARM_OK;
   }
   ClassRelationship  ship= class_mgr_relationship(class_mgr_get(),actualSysName,formulaSysName);
   if(ship==CLASS_RELATIONSHIP_CHILD || ship==CLASS_RELATIONSHIP_IMPL){
	   n_debug("实参%s是形参%s的子类或接口实现。",actualSysName,formulaSysName);
	   return COMPARE_ARGUMENT_ORIG_PARM_OK;
   }
   n_debug("实参%s与形参%s的关系是:%d。是错的！！！\n",actualSysName,formulaSysName,ship);
   n_debug("再检查一次 。如果actualSysName的父类实现了formulaSysName接口，也认为actualSysName实现了formulaSysName接口:%s %s %d\n",actualSysName,formulaSysName,ship);
   ClassInfo *formulaInfo=class_mgr_get_class_info(class_mgr_get(), formulaSysName);
   ClassInfo *actualInfo=class_mgr_get_class_info(class_mgr_get(), actualSysName);
   if(class_info_is_interface(formulaInfo) && !class_info_is_interface(actualInfo)){
	   ClassName *atClass= class_mgr_find_interface(class_mgr_get(), &actualInfo->className,&formulaInfo->className);
	   if(atClass!=NULL){
	       n_debug("在父类:%s找到了接口的实现。也可能不一定。\n",atClass->sysName,formulaSysName);
		   return COMPARE_ARGUMENT_ORIG_PARM_OK;
	   }
   }
   return COMPARE_ARGUMENT_ORIG_PARM_ERROR;
}



int class_util_get_type_name(tree type,char **result)
{
	if(TREE_CODE(type)==POINTER_TYPE)
		type=TREE_TYPE(type);
	if(TREE_CODE(type)==POINTER_TYPE)
		type=TREE_TYPE(type);
	if(TREE_CODE(type)==POINTER_TYPE)
		type=TREE_TYPE(type);
    if(TREE_CODE(type)==RECORD_TYPE){
		tree next=TYPE_NAME(type);
		if(TREE_CODE(next)!=TYPE_DECL){
			return 0;
		}
		char *declClassName=IDENTIFIER_POINTER(DECL_NAME (next));
		*result=n_strdup(declClassName);
		return strlen(declClassName);
    }else if(TREE_CODE(type)==TYPE_DECL){
    	char *declClassName=IDENTIFIER_POINTER(DECL_NAME (type));
       *result=n_strdup(declClassName);
    	return strlen(declClassName);
	}else{
		tree typeName=TYPE_NAME(type);
		if(aet_utils_valid_tree(typeName) && TREE_CODE(typeName)==IDENTIFIER_NODE){
			char *typeNameStr=IDENTIFIER_POINTER(typeName);
			*result=n_strdup(typeNameStr);
			return strlen(typeNameStr);
		}else if(aet_utils_valid_tree(typeName) && TREE_CODE(typeName)==TYPE_DECL){
			char *typeNameStr=IDENTIFIER_POINTER(DECL_NAME(typeName));
			*result=n_strdup(typeNameStr);
			return strlen(typeNameStr);
		}else{
			aet_print_tree_skip_debug(type);
			n_error("在classutil找不到类型名。报告此错误。\n");
		}
	}
	return 0;
}


tree class_util_define_var_decl(tree decl,nboolean initialized)
{
    tree tem;
  tree name=DECL_NAME(decl);
  tree type=TREE_TYPE(decl);
  //printf("class_util_define_var_decl ---00 %p %s %s\n",decl,get_tree_code_name(TREE_CODE(type)),IDENTIFIER_POINTER(DECL_NAME(decl)));
  if (initialized){
	if (TREE_TYPE (decl) == error_mark_node)
	  initialized = FALSE;
	else if (COMPLETE_TYPE_P (TREE_TYPE (decl))) {
	    /* A complete type is ok if size is fixed.  */
	    if (!poly_int_tree_p (TYPE_SIZE (TREE_TYPE (decl)))	|| C_DECL_VARIABLE_SIZE (decl)){
		   error ("variable-sized object may not be initialized");
		   initialized = FALSE;
	    }
	}else if (TREE_CODE (TREE_TYPE (decl)) != ARRAY_TYPE){
	    error ("variable %qD has initializer but incomplete type", decl);
	    initialized = FALSE;
	}else if (C_DECL_VARIABLE_SIZE (decl)){
	    /* Although C99 is unclear about whether incomplete arrays
	       of VLAs themselves count as VLAs, it does not make
	       sense to permit them to be initialized given that
	       ordinary VLAs may not be initialized.  */
	    error ("variable-sized object may not be initialized");
	    initialized = FALSE;
	 }
  }

  if (initialized){
      if (global_bindings_p())
	    TREE_STATIC (decl) = 1;

      /* Tell 'pushdecl' this is an initialized decl
	 even though we don't yet have the initializer expression.
	 Also tell 'finish_decl' it may store the real initializer.  */
      DECL_INITIAL (decl) = error_mark_node;
  }

  tem = pushdecl (decl);
  //printf("class_util_define_var_decl ---11 pushdecl decl:%p tree_type:%p\n",decl,TREE_TYPE(decl));
  if (initialized && DECL_EXTERNAL (tem)){
      DECL_EXTERNAL (tem) = 0;
      TREE_STATIC (tem) = 1;
  }
  return tem;
}

location_t class_util_get_declspecs_first_location(struct c_declspecs *specs)
{
	const enum c_declspec_word data[]= {
	  cdw_typespec /* A catch-all for a typespec.  */,
	  cdw_storage_class  /* A catch-all for a storage class */,
	  cdw_attributes,
	  cdw_typedef,
	  cdw_explicit_signed,
	  cdw_deprecated,
	  cdw_default_int,
	  cdw_long,
	  cdw_long_long,
	  cdw_short,
	  cdw_signed,
	  cdw_unsigned,
	  cdw_complex,
	  cdw_inline,
	  cdw_noreturn,
	  cdw_thread,
	  cdw_const,
	  cdw_volatile,
	  cdw_restrict,
	  cdw_atomic,
	  cdw_saturating,
	  cdw_alignas,
	  cdw_address_space,
	  cdw_gimple,
	  cdw_rtl,
	  cdw_number_of_elements /* This one must always be the last
				    enumerator.  */
	};
	const location_t *locations=specs->locations;
    location_t loc = UNKNOWN_LOCATION;
    //const enum c_declspec_word data[] ={cdw_typespec, cdw_storage_class, cdw_attributes, cdw_typedef,
			//cdw_saturating,cdw_address_space,cdw_atomic, cdw_number_of_elements};
    c_declspec_word *list=data;
    int count=0;
    while (*list != cdw_number_of_elements){
        location_t newloc = locations[*list];
        if (loc == UNKNOWN_LOCATION  || (newloc != UNKNOWN_LOCATION && newloc < loc)){
	        loc = newloc;
	        expanded_location xloc = expand_location(loc);
        	printf("有位置数据了------%d %s %d %d\n",count,xloc.file, xloc.line, xloc.column);
        }
        count++;
        list++;
    }
    return loc;
}

/**
 * 创建父类变量名
 */
char *class_util_create_parent_class_var_name(char *userName)
{
    char temp[255];
    sprintf(temp,"parent%s",userName);
    int hash=aet_utils_create_hash(temp,strlen(temp));
    return n_strdup_printf("%s%u",temp,hash);
}

/**
 * 从fielddecl取出所属类或接口
 */
char *class_util_get_class_name_from_field_decl(tree fieldDecl)
{
    if(!aet_utils_valid_tree(fieldDecl))
        return NULL;
    tree type=TREE_TYPE(fieldDecl);
    if(TREE_CODE(type)!=POINTER_TYPE)
        return NULL;
    type=TREE_TYPE(type);
    if(TREE_CODE(type)!=FUNCTION_TYPE)
        return NULL;
    tree parms=TYPE_ARG_TYPES (type);
    for (tree al = TYPE_ARG_TYPES (type); al; al = TREE_CHAIN (al)){
       tree type=TREE_VALUE(al);
       char *sysName=class_util_get_class_name(type);
       if(sysName)
           return sysName;
       break;
    }
    return NULL;
}

nboolean  class_util_is_function_field(tree fieldDecl)
{
   if(!aet_utils_valid_tree(fieldDecl))
       return FALSE;
   tree type=TREE_TYPE(fieldDecl);
   if(TREE_CODE(type)!=POINTER_TYPE)
       return FALSE;
   type=TREE_TYPE(type);
   if(TREE_CODE(type)==FUNCTION_TYPE)
       return TRUE;
   return FALSE;
}

/**
 * 域变量中有符号$,如_aet_magic$_123。
 * 在二分查找中找不到???
 */
static tree lookupField(tree type, tree component)
{
   tree field;
   if (TYPE_LANG_SPECIFIC (type) && TYPE_LANG_SPECIFIC (type)->s  && !seen_error ()){
      int bot, top, half;
      tree *field_array = &TYPE_LANG_SPECIFIC (type)->s->elts[0];
      field = TYPE_FIELDS (type);
      bot = 0;
      top = TYPE_LANG_SPECIFIC (type)->s->len;
      while (top - bot > 1){
         half = (top - bot + 1) >> 1;
         field = field_array[bot+half];
         if (DECL_NAME (field) == NULL_TREE){
            /* Step through all anon unions in linear fashion.  */
            while (DECL_NAME (field_array[bot]) == NULL_TREE){
              field = field_array[bot++];
              if (RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))) {
                  tree anon = lookupField (TREE_TYPE (field), component);
                  if (anon)
                     return field;
                 if (flag_plan9_extensions  && TYPE_NAME (TREE_TYPE (field)) != NULL_TREE  &&
                        (TREE_CODE (TYPE_NAME (TREE_TYPE (field)))== TYPE_DECL)  && (DECL_NAME (TYPE_NAME (TREE_TYPE (field))) == component))
                    break;
               }
            }

             /* Entire record is only anon unions.  */
            if (bot > top)
               return NULL_TREE;
             /* Restart the binary search, with new lower bound.  */
            continue;
         }
         if (DECL_NAME (field) == component)
             break;
         if (DECL_NAME(field) < component)
            bot += half;
         else
           top = bot + half;
     }
     if (DECL_NAME (field_array[bot]) == component)
         field = field_array[bot];
     else if (DECL_NAME (field) != component)
         return NULL_TREE;
   } else{
      for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
         if (DECL_NAME (field) == NULL_TREE && RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))){
             tree anon = lookupField (TREE_TYPE (field), component);
             if (anon)
               return field;
             if (flag_plan9_extensions  && TYPE_NAME (TREE_TYPE (field)) != NULL_TREE
                && TREE_CODE (TYPE_NAME (TREE_TYPE (field))) == TYPE_DECL  && (DECL_NAME (TYPE_NAME (TREE_TYPE (field))) == component))
              break;
         }
         if (DECL_NAME (field) == component)
              break;
     }
     if (field == NULL_TREE)
        return NULL_TREE;
   }
   return field;
}


/**
 * 在结构体或UNION中查找component
 */
nboolean  class_util_have_field(tree type, tree component)
{
    tree field= lookupField(type,component);
    return aet_utils_valid_tree(field);
}

