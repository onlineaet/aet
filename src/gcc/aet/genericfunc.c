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
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "toplev.h"
#include "asan.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "opts.h"
#include "c/c-parser.h"
#include "tree-iterator.h"
#include "fold-const.h"
#include "langhooks.h"

#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericfunc.h"
#include "classutil.h"
#include "genericutil.h"
#include "classimpl.h"
#include "funcmgr.h"
#include "blockmgr.h"
#include "makefileparm.h"
#include "classutil.h"
#include "funcmgr.h"
#include "genericutil.h"
#include "classparser.h"

/**
 * 做两件事
 * 1.创建FuncGenParmInfo tempFgpi1234
 * 2.记录泛型类型，为生成泛型块函数的条件判断做好准备
 */
static void genericFuncInit(GenericFunc *self)
{

}

static char *getStringCst(tree type)
{
   if(TREE_CODE(type)==POINTER_TYPE)
	   return getStringCst(TREE_TYPE(type));
   tree typeName=  TYPE_NAME(type)	;
   if(TREE_CODE(typeName)==TYPE_DECL){
	   tree id=DECL_NAME(typeName);
	   char *idName=IDENTIFIER_POINTER(id);
	   return idName;
   }
   return NULL;
}


/**
 * 获取引用的类和类中的域
 * A->B->C->func;
 * 生成 C,B,A顺序
 */
static void refLink(tree func,NPtrArray *refArray)
{
	tree op0=TREE_OPERAND (func, 0);
	tree op1=TREE_OPERAND (func, 1);//域成员 函数名或变量名
	if(TREE_CODE(op0)==INDIRECT_REF){ //var->getName();
	   tree component=TREE_OPERAND(op0,0);
	   n_ptr_array_add(refArray,op1);
	   if(TREE_CODE(component)==COMPONENT_REF){
		   refLink(component,refArray);
	   }else{
		   n_ptr_array_add(refArray,component);
	   }
	}else if(TREE_CODE(op0)==VAR_DECL){ //var.getName();
		 n_ptr_array_add(refArray,op1);
		 n_ptr_array_add(refArray,op0);
	}
}

/**
 *返回域
 */
static tree getField(tree actual)
{
	enum tree_code code=TREE_CODE(actual);
	//获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
	//收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
	if(code==COMPONENT_REF){
		NPtrArray *refArray=n_ptr_array_new();
		refLink(actual,refArray);
     	char *sysClassName=NULL;
		char *varName=NULL;
		tree field=n_ptr_array_index(refArray,0);
		actual=field;
		n_ptr_array_unref(refArray);
	}
	return actual;
}



/**
 * 检查泛型函数声明的泛型
 */
static nboolean checkGenericFuncDecl(ClassFunc *func)
{
   GenericModel *funcGen=class_func_get_func_generic(func);
   int undefine=generic_model_get_undefine_count(funcGen);
   int genCount=generic_model_get_count( funcGen);
   if(undefine!=genCount){
      error_at(input_location,"泛型函数里不能有定义的类型。%qs",func->orgiName);
      return FALSE;
   }
   int i;
   for(i=0;i<genCount;i++){
      GenericUnit *id=generic_model_get(funcGen,i);
      if(!id || id->isDefine){
         error_at(input_location,"泛型函数里不能有定义的类型。%qs",func->orgiName);
         return FALSE;
      }
   }
   return TRUE;
}

#define USER_DEFINE 0
#define USER_DEFINE_GENERIC_DECL 1
#define PARM_DEFINE 2
#define PARM_REPLACE_BY_INT 3
#define PARM_UNDEFINE_FROM_GENERIC_FUNC 4
#define PARM_UNDEFINE_FROM_CLASS 5


typedef struct _GenDefineData{
	tree  types[10];
	char *unitName[10];
	int   from[10];//用户声明，用户定义为为T,E等 参数中取出，无参数用int代替
	int   undefine[10];//是不是未定义的泛型如T,E等。0 不是，1 是
	char *typeName[10];//实参的类型名如int ,char,aet_generic_A,aet_generic_T等
	int   count;
	GenericUnit *units[10];
	char *classOrFuncDecl[10];

}GenDefineData;

/**
 * 用户定义的：如set<int>()
 * funcGen 泛型函数声明的泛型类型如E、T等
 */
static nboolean createByUserDefine(ClassFunc *func,GenericModel *funcGenericDefine,GenDefineData *defineData)
{
    if(funcGenericDefine==NULL)
    	return TRUE;
    GenericModel *funcGen=class_func_get_func_generic(func);
	int genCount=generic_model_get_count(funcGen);
    int i;
    int count=0;
	for(i=0;i<genCount;i++){
		GenericUnit *id=generic_model_get(funcGen,i);
		GenericUnit *unit=generic_model_get(funcGenericDefine,i);
		n_debug("createByUserDefine 00 gen: %s unit name:%s\n",generic_model_tostring(funcGenericDefine),unit->name);
		aet_print_tree(unit->decl);
		defineData->from[count]=USER_DEFINE;
		defineData->units[count]=unit;
		if(!aet_utils_valid_tree(TREE_TYPE(unit->decl))){
			if(generic_util_valid_by_str(unit->name)){
				defineData->from[count]=USER_DEFINE_GENERIC_DECL;
			}else{
				error_at(input_location,"函数%qs中定义的泛型参数%qs是错的。",func->orgiName,unit->name);
				return FALSE;
			}
		}
		count++;
	}
	defineData->count=count;
	return TRUE;
}

static nboolean compareActualParmType(tree first,tree second)
{
	nboolean re=c_tree_equal(first,second);
    if(re)
		return TRUE;
	if(TREE_CODE(first)==TREE_CODE(second)){
		n_warning("c_tree_equal 不相等，但tree_code是相同的:%s",get_tree_code_name(TREE_CODE(first)));
		return TRUE;
	}else{
		return FALSE;
	}
}

/**
 * 比较实参，如果一个泛型对应多个实参。
 * 返回第一个实参的类型，即为这次调用泛型函数的类型。
 * 在classfunc中是 setData(self,tempFgpi1234,T abc)
 * ParmGenInfo中的index是从self算起的所以 abc的index=2
 * exprlist中只有(self,T abc);
 * array:同一个泛型单元有多少个参数对应，如:set(T a,T b) 这里T有两个参数对应分别是a,b
 *
 */
static tree getActualParmType(ClassFunc *func,NPtrArray *array,vec<tree, va_gc> *exprlist)
{
   int i;
   int len=array->len;
   tree first=NULL_TREE;
   for(i=0;i<len;i++){
      ParmGenInfo *item=(ParmGenInfo *)n_ptr_array_index(array,i);
      int index=item->index; //index永远是大于0的数，因为class_func_get_generic_parm 中跳过了self参数
      n_debug("getActualParmType 00 检查实参:i:%d 参数个数:%d index:%d independ:%d str:%s\n",i,array->len,index,item->independ,item->str);
      tree actual=(*exprlist)[index];
      if(item->independ==0){
         n_debug("是一个泛型类，取出泛型类中的泛型。");
         if(TREE_CODE(actual)==ADDR_EXPR) //setData(&abc)此种情况需要取出变量
            actual=TREE_OPERAND(actual,0);
         else if(TREE_CODE(actual)==COMPONENT_REF)
            actual=getField(actual);
         else if(TREE_CODE(actual)==CALL_EXPR){
            tree call=CALL_EXPR_FN (actual);
            actual=getField(call);
         }
         GenericModel *trueModel=c_aet_get_generics_model(actual);
         n_debug("getActualParmType 11  unitpos:%d %s\n\n",item->unitPos,generic_model_tostring(trueModel));

         GenericUnit  *unit=generic_model_get(trueModel,item->unitPos);
         if(generic_unit_is_undefine(unit)){
            if(generic_unit_is_query(unit)){
               n_debug("是问号泛型单元用ParmGenInfo中的参数代替 %s\n",item->str);
               if(generic_util_valid_by_str(item->str)){
                  tree type=generic_util_get_generic_type_by_str(item->str);
                  actual= build_decl (0,PARM_DECL,NULL_TREE, TREE_TYPE(type));
               }else{
                  aet_print_tree_skip_debug(item->object);
                  n_error("参数中定义的类中泛型不是A-Z在泛型声明。而是:%s\n",item->str);
               }
            }else{
               tree type=generic_util_get_generic_type_by_str(unit->name);
               actual= build_decl (0,PARM_DECL,NULL_TREE, TREE_TYPE(type));
            }
         }else{
            actual=unit->decl;
         }
      }
      if(i==0)
         first=actual;
      else{
         nboolean result=compareActualParmType(first,actual);
         if(!result){
            aet_print_tree_skip_debug(first);
            aet_print_tree_skip_debug(actual);
            error_at(input_location,"函数%qs中的两个泛型实参类型不相同。",func->orgiName);
            return NULL_TREE;
         }
      }
   }
   return first;
}

/**
 * 获取aet_generic_A aet_generic_E名字
 */
static char *getGenericDeclName(tree type)
{
	if(TREE_CODE(type)==POINTER_TYPE){
		tree typeName=TYPE_NAME(type);
		if(typeName!=NULL && TREE_CODE(typeName)==TYPE_DECL){
			tree id=DECL_NAME(typeName);
			if(id!=NULL){
				char *name=IDENTIFIER_POINTER(id);
				if(generic_util_is_generic_ident(name)){
				    n_debug("找到了:%s\n",name);
					return name;
				}
			}
		}
	}
	return NULL;
}

/**
 * 从参数取泛型定义：如set(5) set(Abc<E> *abx)
 * funcGen 泛型函数声明的泛型类型如E、T等
 */
static nboolean createByParm(ClassFunc *func,vec<tree, va_gc> *exprlist,GenDefineData *defineData)
{
   GenericModel *funcGen=class_func_get_func_generic(func);//泛型函数中原声明的泛型
   int genCount=generic_model_get_count( funcGen);
   int i=0;
   int count=0;
   for(i=0;i<genCount;i++){
      GenericUnit *unit=generic_model_get(funcGen,i);
      NPtrArray *array=class_func_get_generic_parm(func,unit->name);
      if(array->len==0){
         n_debug("createByParm 泛型函数%s，没有参数----默认设泛型类型为int。\n",func->orgiName);
         defineData->from[count]=PARM_REPLACE_BY_INT;
         GenericUnit  *unit=generic_unit_new("int",0);
         defineData->units[count]=unit;
      }else{
         tree parm=getActualParmType(func,array,exprlist);
         if(!aet_utils_valid_tree(parm)){
            error_at(input_location,"泛型函数里的实参类型是无效的。%qs",func->orgiName);
            return FALSE;
         }
         defineData->from[count]=PARM_DEFINE;
         char *typeName=getGenericDeclName(TREE_TYPE(parm));
         n_debug("createByParm  00 泛型单元:%d typeName:%s declUnit:%s\n",count,typeName,unit->name);
         if(typeName==NULL){
            char *typeName=NULL;
            class_util_get_type_name(TREE_TYPE(parm),&typeName);
            int pointerCount=class_util_get_pointers(TREE_TYPE(parm));
            GenericUnit  *newUnit=generic_unit_new(typeName,pointerCount);
            defineData->units[count]=newUnit;
            n_debug("createByParm  11 泛型单元:%d typeName:%s declUnit:%s pointerCount:%d %p\n",count,typeName,unit->name,pointerCount,defineData);
         }else{
            //从参数中获取到是T
            char genStr[2];
            genStr[0]=typeName[strlen(typeName)-1];
            genStr[1]='\0';
            n_debug("createByParm  22 从当前泛型函数中得到:%s\n",genStr);
            defineData->classOrFuncDecl[count]=n_strdup(genStr);
            if(TREE_CODE(parm)==PARM_DECL)
               defineData->from[count]=PARM_UNDEFINE_FROM_GENERIC_FUNC;
            else
               defineData->from[count]=PARM_UNDEFINE_FROM_CLASS;
            GenericUnit *uu=generic_unit_new_undefine(genStr);
            defineData->units[count]=uu;
         }
      }
      n_ptr_array_unref(array);
      count++;
   }
   defineData->count=count;
   return TRUE;
}

/**
 * 比较用户和参数的泛型
 */
static nboolean compareUserParm(GenDefineData *userDefine,GenDefineData *parmDefine)
{
	if(userDefine->count==0)
		return FALSE;
	int i;
	for(i=0;i<userDefine->count;i++){
		GenericUnit *ug=userDefine->units[i];
		GenericUnit *pg=parmDefine->units[i];
		if(parmDefine->from[i]==PARM_REPLACE_BY_INT){
			n_warning("比较用户定义与参数中的泛型，但参数没有声明为泛型。");
			continue;
		}
        if(generic_unit_is_undefine(ug) || generic_unit_is_undefine(pg)){
			n_warning("比较用户定义与参数中的泛型，但参数没有声明为泛型。ug:%s point:%d pg:%s point:%d",ug->name,ug->pointerCount,pg->name,pg->pointerCount);
			continue;
        }
		tree ut=TREE_TYPE(ug->decl);
		tree pt=TREE_TYPE(pg->decl);
		nboolean re=compareActualParmType(ut,pt);
		if(!re){
			if(ug->pointerCount>0 && pg->pointerCount>0){
				if(TREE_CODE(ut)==VOID_TYPE || TREE_CODE(pt)==VOID_TYPE){
					printf("都是指针，并且有一个是void类型。\n");
					return TRUE;
				}
			}
			printf("参数与用户定义的泛型不一致。\n");
			aet_print_tree_skip_debug(ut);
			aet_print_tree_skip_debug(pt);
			return FALSE;
		}
	}
	return TRUE;
}



/*
 * 检查参数中是否有A-Z泛型类型
 */
static int getUndefineCount(GenDefineData *gen)
{
	int i;
	int undefineCount=0;
	for(i=0;i<gen->count;i++){
		GenericUnit *unit=gen->units[i];
		if(generic_unit_is_undefine(unit)){
			undefineCount++;
		}
	}
	return undefineCount;
}

static nboolean validParm(GenDefineData *data)
{
	int i;
	for(i=0;i<data->count;i++){
		if(data->from[i]!=PARM_REPLACE_BY_INT)
			return TRUE;
	}
	return FALSE;
}

nboolean generic_func_check(GenericFunc *self,ClassFunc *func,ClassName *className,
      vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine)
{
   n_debug("generic_func_check 00 检查泛型函数的泛型声明 %s file:%s\n",func->orgiName,in_fnames[0]);
   if(!checkGenericFuncDecl(func))
      return FALSE;
   n_debug("generic_func_check 11 收集用户定义的泛型 %s\n",func->orgiName);
   tree actual=(*exprlist)[0];
   aet_print_tree(actual);

   GenDefineData userDefine;
   memset(&userDefine,0,sizeof(GenDefineData));
   if(!createByUserDefine(func,funcGenericDefine,&userDefine))
      return FALSE;
   n_debug("generic_func_check 22 从参数收集泛型 %s userDefine.count：%d file:%s\n",func->orgiName,userDefine.count,in_fnames[0]);

   GenDefineData parmDefine;
   memset(&parmDefine,0,sizeof(GenDefineData));
   if(!createByParm(func,exprlist,&parmDefine))
      return FALSE;

   if(funcGenericDefine==NULL && !validParm(&parmDefine)){
      error_at(input_location,"调用函数%qs时，没有定义泛型单元、从参数中也不能推断泛型单元类型。",func->orgiName);
      return FALSE;
   }

   n_debug("generic_func_check 33 比较用户定义的泛型与参数的类型是符匹配 %s %d %d file:%s\n",
         func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
   if(userDefine.count>0 && parmDefine.count>0){
      if(!compareUserParm(&userDefine,&parmDefine)){
         error_at(input_location,"调用函数%qs时，定义的泛型与参数的类型不相同。",func->orgiName);
         return FALSE;
      }
   }

   int undefineCount=0;
   if(userDefine.count>0){
      n_debug("generic_func_check 44 从用户定义中检查A-Z泛型%s %d %d file:%s\n",
            func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
      undefineCount=getUndefineCount(&userDefine);
   }else{
      n_debug("generic_func_check 55 从参数中检查A-Z泛型%s %d %d file:%s\n",
            func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
      undefineCount=getUndefineCount(&parmDefine);
   }
   int fromSelf=0; //不定的泛型最终是由self确定?
   int fromGenFunc=0;//不定的泛型最终是由所在的泛型函数确定?
   if(undefineCount>0){
      n_debug("generic_func_check 66 取未定义的泛型数据 %s %d %d\n",func->orgiName,parmDefine.count,userDefine.count);
      if(!aet_parser_get()->isAet){
         error_at(input_location,"在类外调用泛型函数%qs，但函数传参数没有定义泛型。",func->orgiName);
         return FALSE;
      }
   }

   return TRUE;
}

/**
 * 解析泛型函数中的参数如果是泛型声明，需要判断在当前泛型函数或类中声明了相同的泛型。
 * 例：setData(E value) 如果 E 在当前函数或类中的声明，替换为aet_generic_E
 */
void generic_func_parameter_declaration (GenericFunc *self)
{
   c_parser *parser=aet_parser_get()->parser;
   c_token *cur=c_parser_peek_token (parser);
   if(c_parser_next_token_is (parser, CPP_NAME)){//zclei
      if(generic_util_valid_id(cur->value)){
         if(aet_parser_get()->isAet || class_parser_is_parsering(class_parser_get())){
            //检查所在的函数是不是泛型函数并且包含有泛型声明cur->value
            //检查是不是一个泛型类并且包含有泛型声明cur->value
            ClassName      *belongClassName=NULL;
            GenericModel *belongGen=NULL;
            GenericModel *funcGen=NULL;
            if(aet_parser_get()->isAet)
               belongClassName=class_impl_get_class_name(class_impl_get());
            else
               belongClassName=class_parser_get_class_name(class_parser_get());
            if(belongClassName){
               ClassInfo *belongInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),belongClassName);
               belongGen= belongInfo->genericModel;
            }
            if(current_function_decl){
               funcGen=c_aet_get_func_generics_model(current_function_decl);
            }else{
               //正在classparser,是否正在解析泛型函数,如果是，应该有泛型函数的genericmode
               if(class_parser_is_parsering(class_parser_get())){
                  funcGen= class_parser_get_func_generic_mode(class_parser_get());
                 // printf("取fungen----%p\n",funcGen);
               }else if(aet_parser_get()->isAet){
                 // printf("取fungen---xxx-%p\n",funcGen);
                  funcGen= class_impl_get_func_generic_mode(class_impl_get());
               }
            }
            char *genericStr=IDENTIFIER_POINTER(cur->value);
            //printf("泛型函数中有吗xxx: %s %p %p\n",genericStr,funcGen,current_function_decl);
            nboolean find=generic_model_include_decl_by_str(funcGen ,genericStr);
            if(!find){
               find=generic_model_include_decl_by_str(belongGen,genericStr);
            }
            if(find){
               n_debug("c-parser.c 是一个泛型 把泛型替换成 aet_generic_E--- %s\n",genericStr);
               generic_impl_replace_token(generic_impl_get(),cur);
            }
         }
      }
   }else{
      if(c_parser_peek_2nd_token(parser)->type==CPP_DOT){
         class_impl_parser_package_dot_class(class_impl_get());
      }
   }

   if (c_parser_next_token_is_keyword (parser, RID_AET_GOTO)){
      n_debug("c-parsr.c 解析参数时遇到RID_AET_GOTO");
      //zclei
      class_parser_goto(class_parser_get(),FALSE,NULL);
   }
}


static GenericModel *getClassGenDecl(char *sysName)
{
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    if(info==NULL ){
      return NULL;
    }
    return info->genericModel;
}

static GenericModel *getClassGenDefine(tree caller)
{
   GenericModel *trueModel=c_aet_get_generics_model(caller);
   return trueModel;
}

/**
 * 调用泛型函数时定义的泛型存在
 */
static GenericModel *getGenericFuncDefine(tree initOrRhs)
{
   enum tree_code code=TREE_CODE(initOrRhs);
   //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
   //收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
   if(code==COMPONENT_REF){
      tree op0=TREE_OPERAND (initOrRhs, 0);
      if(TREE_CODE(op0)==INDIRECT_REF || TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL ){
         GenericModel *model=c_aet_get_func_generics_model(initOrRhs);
         //printf("getGenericFuncDefine --- %s\n",generic_model_tostring(model));
         return model;

      }else{
         return getGenericFuncDefine(op0);
      }
   }
   return NULL;
}

static GenericModel *getGenericFuncDecl(ClassFunc *func)
{
     return class_func_get_func_generic(func);
}


static void refLinkParm(tree func,NPtrArray *refArray)
{
   tree op0=TREE_OPERAND (func, 0);
   tree op1=TREE_OPERAND (func, 1);//域成员 函数名或变量名
   if(TREE_CODE(op0)==INDIRECT_REF){ //var->getName();
      tree component=TREE_OPERAND(op0,0);
      n_ptr_array_add(refArray,op1);
      if(TREE_CODE(component)==COMPONENT_REF){
         refLinkParm(component,refArray);
      }else{
         n_ptr_array_add(refArray,component);
      }
   }else if(TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL){ //var.getName();
       n_ptr_array_add(refArray,op1);
       n_ptr_array_add(refArray,op0);
   }else if(TREE_CODE(op0)==COMPONENT_REF){
      refLinkParm(op0,refArray);
   }
}


static tree getCaller(tree fn)
{
   enum tree_code code=TREE_CODE(fn);
   //获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
   if(code==COMPONENT_REF){
      NPtrArray *refArray=n_ptr_array_new();
      refLinkParm(fn,refArray);
      tree field=n_ptr_array_index(refArray,1);//0 getName 1 A
      fn=field;
      n_ptr_array_unref(refArray);
   }else if(code==ADDR_EXPR){
      tree op0=TREE_OPERAND (fn, 0);
      if(TREE_CODE(op0)==FUNCTION_DECL){
           char *functionName=IDENTIFIER_POINTER(DECL_NAME(op0));
         ClassFunc *func=func_mgr_get_func_by_mangle(func_mgr_get(),functionName);
         if(func){
            char *sysName= func_mgr_get_class_name_by_mangle(func_mgr_get(),functionName);
            fn=DECL_ARGUMENTS (op0);
         }
      }
   }
   return fn;
}




/**
 * 从泛型函数来判断参数是否匹配
 */
static int compare(GenericModel *genDecl,GenericModel *genDefine,GenericModel *formalModel,GenericModel *actualModel,
                   char *callObject,ClassFunc *func,char *parmClassName,nboolean isGenFunc)
{
   if(genDecl==NULL)
      return 0;
   int i;
   int count=generic_model_get_count(formalModel);
   for(i=0;i<count;i++){
      GenericUnit *unit=generic_model_get(formalModel,i);
      if(generic_unit_is_query(unit))
         continue;
      int index=generic_model_get_index_by_unit(genDecl,unit);
      if(index<0){
            n_warning("参数的声明%s在泛型函数的声明%s中没找到。 类:%s 函数:%s",
                  generic_model_tostring(genDecl),generic_model_tostring(formalModel),callObject,func->orgiName);
            return 0;
      }
   }
   if(isGenFunc && genDefine==NULL){
      n_warning("这里调用的泛型函数，并且没有定义泛型。由参数决定类型。");
      return 1;
   }


   //The method getxxx(Hello<E>) in the type Hello<E> is not applicable for the arguments (Hello<String>)
   //过了声明现在比实参
   for(i=0;i<count;i++){
      GenericUnit *unit=generic_model_get(formalModel,i);
      if(generic_unit_is_query(unit))
         continue;
      int index=generic_model_get_index_by_unit(genDecl,unit);
      GenericUnit *actualUnit=generic_model_get(actualModel,i);
      GenericUnit *defineUnit=generic_model_get(genDefine,index);
      //printf("compare ---dddd- %p %p %p %p %p %d %d\n",actualUnit,defineUnit,genDefine,actualModel,genDecl,i,index);
      //printf("实参是:%s %s isGenFunc:%d\n",actualUnit->name,defineUnit->name,isGenFunc);

        if(!generic_unit_equal(actualUnit,defineUnit)){
          nboolean q1=generic_unit_is_query(defineUnit);
          nboolean q2=generic_unit_is_query(actualUnit);
         if(q1 && !q2){
            continue;
         }else{
           if(!generic_unit_is_undefine(actualUnit) && generic_unit_is_undefine(defineUnit))
              return 1;
           char temp[1024];
           sprintf(temp,"方法%s中的参数%s<%s>不能从<%s>转<%s>",func->orgiName,parmClassName,
            generic_unit_tostring(unit),generic_unit_tostring(actualUnit),generic_unit_tostring(defineUnit));
           error_at(input_location,"%qs",temp);
           return -1;
         }
        }
   }
   return 1;

}


/**
 * 如果是泛型函数或函数中有问号泛型参数。跳过self与FuncGenParmInfo tempFgpi1234，否则跳过self
 * 如果调用类的方法中有泛型类参数，检查调用类定义的泛型与参数中的泛型是否匹配。
 * 例如:
 * class A<E>{
 *   setData(E *parm);
 * }
 * A<B *> *callObj;
 * B<int> *parm;
 * callObj->setData(parm);
 * 在这里检查parm中的泛型是不是与callObj定义的一样。
 *
 */
static tree getClassParm(char *sysName,int index,ClassFunc *func,GenericModel **genModel)
{
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    if(!class_info_is_generic_class(info) ){
        n_warning("方法%s中的参数不是泛型类%s，不检查该参数。\n",func->orgiName,sysName);
        return NULL_TREE;
    }
    //printf("processClassType ------%s %s %d\n",sysName,func->orgiName,index);
    tree parm=NULL_TREE;
    int count=0;
    if(aet_utils_valid_tree(func->fromImplDefine)){
          //printf("processClassType ------00 %s %s %d\n",sysName,func->orgiName,index);
          tree args=DECL_ARGUMENTS (func->fromImplDefine);
          for (parm = args; parm; parm = DECL_CHAIN (parm)){
              //printf("得到参数---%d index:%d\n\n",count,index);
              if(count==index){
                 *genModel=c_aet_get_generics_model(parm);
                 return parm;
              }
              count++;
          }
    }else if(aet_utils_valid_tree(func->fieldDecl)){
        //printf("processClassType ------11 %s %s %d\n",sysName,func->orgiName,index);
        tree funcType=TREE_TYPE(TREE_TYPE(func->fieldDecl));
        for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
              parm = TREE_VALUE (al);
              if(count==index){
                *genModel=func->parmsGenModel[count];
                 return TREE_TYPE(parm);
              }
              count++;
        }
    }else{
        n_error("getClassTypeParm 未知错误。%s %s\n",sysName,func->orgiName);
    }
    return NULL_TREE;
}

/**
 * formal 形参
 * actual 实参
 */
static void check_new(GenericModel **models,tree formal,GenericModel *formalModel,tree actual,char *callObject,ClassFunc *func)
{
   GenericModel *classGenDecl=models[0];
   GenericModel *classGenDefine=models[1];
   GenericModel *funcGenDecl=models[2];
   GenericModel *funcGenDefine=models[3];
   if(!aet_utils_valid_tree(formal))
      return;
   GenericModel *actualModel=c_aet_get_generics_model(actual);
   if(actualModel==NULL){
      if(TREE_CODE(actual)==CALL_EXPR){
         tree fn=CALL_EXPR_FN (actual);
         tree caller=getCaller(fn);
         actualModel=c_aet_get_generics_model(caller);
      }else if(TREE_CODE(actual)==PARM_DECL){
         char *parmName=IDENTIFIER_POINTER(DECL_NAME(actual));
         if(parmName && !strcmp(parmName,"self")){
            char *sysName=class_util_get_class_name(TREE_TYPE(actual));
            if(sysName && !strcmp(sysName,callObject)){
               //如果sysName是callObject的父类或子类，如何？
               actualModel=classGenDefine;
            }
         }
      }else if(TREE_CODE(actual)==COMPONENT_REF){
         NPtrArray *refArray=n_ptr_array_new();
         refLinkParm(actual,refArray);
         tree field=n_ptr_array_index(refArray,0);
         n_ptr_array_unref(refArray);
         actualModel=c_aet_get_generics_model(field);
      }else if(TREE_CODE(actual)==ADDR_EXPR){
         tree val=TREE_OPERAND (actual, 0);
         if(TREE_CODE(val)==VAR_DECL || TREE_CODE(val)==PARM_DECL || TREE_CODE(val)==FUNCTION_DECL){
            actualModel=c_aet_get_generics_model(val);
         }
      }else if(TREE_CODE(actual)==NOP_EXPR){
         tree val=TREE_OPERAND (actual, 0);
         if(TREE_CODE(val)==VAR_DECL || TREE_CODE(val)==PARM_DECL || TREE_CODE(val)==FUNCTION_DECL){
            actualModel=c_aet_get_generics_model(val);
         }else if(TREE_CODE(val)==ADDR_EXPR){
            val=TREE_OPERAND (val, 0);
            if(TREE_CODE(val)==VAR_DECL || TREE_CODE(val)==PARM_DECL || TREE_CODE(val)==FUNCTION_DECL){
               actualModel=c_aet_get_generics_model(val);
            }
         }
      }
      if(actualModel==NULL){
         aet_print_tree_skip_debug(actual);
         location_t loc=input_location;
         if(EXPR_P(actual)){
            loc=EXPR_LOCATION(actual);
            error_at(loc,"实参没有声明泛型%qE。",actual);
         }else if(DECL_P(actual)){
            loc=DECL_SOURCE_LOCATION(actual);
            error_at(loc,"实参没有声明泛型%qD。",actual);
         }else{
            error_at(loc,"实参没有声明泛型%s。",actual);
         }
         return;
      }
   }

   n_debug("generic_check_parm----00 最后检查 %s %s\n",generic_model_tostring(formalModel),generic_model_tostring(actualModel));
   tree type=formal;//TREE_TYPE(formal);
   aet_print_tree(type);
   char *sysName=class_util_get_class_name(type);
   int re=compare(funcGenDecl,funcGenDefine,formalModel,actualModel,callObject,func,sysName,TRUE);
   if(re==0){
      re=compare(classGenDecl,classGenDefine,formalModel,actualModel,callObject,func,sysName,FALSE);
   }
   n_debug("generic_check_parm----11 最后检查 %s %s\n",generic_model_tostring(formalModel),generic_model_tostring(actualModel));

}



nboolean  generic_func_check_parm(GenericFunc *self,char *callObject,ClassFunc *func,int paramNum,tree actual,tree function)
{
   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),callObject);
   if(!class_info_is_generic_class(info) && !class_func_is_func_generic(func)){
      n_debug("类%s不是泛型类，函数%s也不是泛型函数。不检查参数。\n",callObject,func->orgiName);
      return TRUE;
   }
   if(!class_func_have_generic_class_parm(func)){
      n_debug("类%s中方法%s没有泛型参数:不检查。\n",callObject,func->orgiName);
      return TRUE;
   }
   int skip=(class_func_is_func_generic(func) || class_func_have_query_param(func))?2:1;
   tree caller=getCaller(function);
   GenericModel *classGenDecl=getClassGenDecl(callObject);
   GenericModel *classGenDefine=getClassGenDefine(caller);
   GenericModel *funcGenDecl=getGenericFuncDecl(func);
   GenericModel *funcGenDefine=getGenericFuncDefine(function);
   char *belongFunc=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
   n_debug("generic_check_parm----11 4个泛型:class:%s %s genfunc:%s %s skip:%d 所在函数:%s 检查的函数:%s\n",generic_model_tostring(classGenDecl),generic_model_tostring(classGenDefine),
   generic_model_tostring(funcGenDecl),generic_model_tostring(funcGenDefine),skip,belongFunc,func->orgiName);
   if(classGenDecl && classGenDefine==NULL){
      classGenDefine=classGenDecl;
   }
   GenericModel *modes[4]={classGenDecl,classGenDefine,funcGenDecl,funcGenDefine};

   tree type=TREE_TYPE(actual);
   char *sysName=class_util_get_class_name(type);
   if(sysName){
      GenericModel *model=NULL;
      tree declParm=getClassParm(sysName,paramNum,func,&model);
      check_new(modes,declParm,model,actual,callObject,func);
   }
   return FALSE;
}

GenericFunc *generic_func_get()
{
   static GenericFunc *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(GenericFunc));
      genericFuncInit(singleton);
   }
   return singleton;
}
