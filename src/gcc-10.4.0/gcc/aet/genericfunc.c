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
#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericfunc.h"
#include "aet-typeck.h"
#include "aet-c-fold.h"
#include "classutil.h"
#include "genericutil.h"
#include "aet-c-parser-header.h"
#include "classimpl.h"
#include "funcmgr.h"
#include "blockmgr.h"
#include "genericexpand.h"
#include "makefileparm.h"
#include "genericfile.h"

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
 * 检查
 * A->B->getName
 * 中A/B是否带问号泛型,如果有，这是错误的。
 * 返回有多少个泛型类在调用栈中
 */


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
 * 为了生成参数最后多加一个")"
 */
static char *createCodes(NString *defineStr,NString *undefineStr,int count,
		int undefineCount,char *sysName,char *funcName,int fromSelf,int fromGenFunc)
{
	  NString *codes=n_string_new("");
	  n_string_append_printf(codes,"({ %s info;\n",AET_FUNC_GENERIC_PARM_STRUCT_NAME);
	  if(defineStr!=NULL)
		n_string_append(codes,defineStr->str);
	  if(undefineStr!=NULL)
		n_string_append(codes,undefineStr->str);
	  n_string_append_printf(codes,"info.count=%d;\n",count);
	  tree id=aet_utils_create_ident(FuncGenParmInfo_NAME);
	  tree decl=lookup_name(id);
	  if(aet_utils_valid_tree(decl)){
		  n_string_append_printf(codes,"info.globalBlockInfo=&%s;\n",FuncGenParmInfo_NAME);
	  }else{
		  n_string_append(codes,"info.globalBlockInfo=NULL;\n");
	  }
	  n_string_append(codes,"info.excute=0;\n");
	  if(undefineCount==0){
		  n_string_append_printf(codes,"info.%s=%s;\n",AET_SET_BLOCK_FUNC_VAR_NAME,AET_SET_BLOCK_FUNC_CALLBACK_NAME);
		  n_debug("GenericFunc 00 因为调用的泛型函数所有的泛型单元是确定的%s %s,所以在当前文件%s，自动生成设置块函数:\n",sysName,funcName,in_fnames[0],AET_SET_BLOCK_FUNC_CALLBACK_NAME);
	  }else{
          if(fromSelf && !fromGenFunc){
              n_debug("GenericFunc 11 因为调用的泛型函数不确定的泛型单元是由self确定的：%s %s 所在文件:%s\n",sysName,funcName,in_fnames[0]);
    		  n_string_append_printf(codes,"info.%s=self->%s;\n",AET_SET_BLOCK_FUNC_VAR_NAME,AET_SET_BLOCK_FUNC_VAR_NAME);
          }else if(!fromSelf && fromGenFunc){
              n_debug("GenericFunc 22 因为调用的泛型函数不确定的泛型单元是由所在的泛型函数确定的：%s %s 所在文件:%s\n",sysName,funcName,in_fnames[0]);
    		  n_string_append_printf(codes,"info.%s=%s.%s;\n",AET_SET_BLOCK_FUNC_VAR_NAME,AET_FUNC_GENERIC_PARM_NAME,AET_SET_BLOCK_FUNC_VAR_NAME);
          }else{
              n_debug("GenericFunc 33 因为调用的泛型函数不确定的泛型单元是由所在的泛型函数和self确定的：%s %s 所在文件:%s\n",sysName,funcName,in_fnames[0]);
    		  n_error("不知如何处理这种情况-----redo\n");
          }
	  }
	  n_string_append(codes,"info;\n");
	  n_string_append(codes,"}))\n");
	  char *result=n_strdup(codes->str);
	  n_string_free(codes,TRUE);
	  return result;
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
		n_debug("createByUserDefine 从用户设定的泛型定义中创建tempFgpi1234 gen: %s unit name:%s\n",generic_model_tostring(funcGenericDefine),unit->name);
		aet_print_tree(unit->decl);
		defineData->from[count]=USER_DEFINE;
		defineData->units[count]=unit;
		if(!aet_utils_valid_tree(TREE_TYPE(unit->decl))){
			if(generic_util_valid_by_str(unit->name)){
				defineData->from[count]=USER_DEFINE_GENERIC_DECL;
			}else{
				error_at(input_location,"函数%qs中的用户定义的泛型参数%qs是错的。",func->orgiName,unit->name);
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
        int index=item->index-1;//跳过self
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
 *
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

static int findGenDeclAtClass(GenericModel *model,char *unitName)
{
	 if(model==NULL)
	    return -1;
	 int i;
	 for(i=0;i<model->unitCount;i++){
		GenericUnit *unit=model->genUnits[i];
		if(!unit->isDefine && !strcmp(unit->name,unitName)){
			return i;
		}
	 }
	 return -1;
}

static int findGenDecl(GenericModel *model,char *unitName)
{
	 if(model==NULL)
	    return -1;
	 int i;
	 for(i=0;i<model->unitCount;i++){
		GenericUnit *unit=model->genUnits[i];
		if(!unit->isDefine && !strcmp(unit->name,unitName)){
			return i;
		}
	 }
	 return -1;
}

/**
 * 复制当前函数中的参数tempFgpi1234到被调用的泛型函数的参数info中
 */
static void createFpgiByFunc(int index,int genIndex,NString *codes)
{
   tree id=aet_utils_create_ident(AET_FUNC_GENERIC_PARM_NAME);
   tree parm=lookup_name(id);
   if(aet_utils_valid_tree(parm) && TREE_CODE(parm)==PARM_DECL){
	   n_string_append_printf(codes,"memcpy(&info.pms[%d],&(%s.pms[%d]),sizeof(aet_generic_info));\n",genIndex,AET_FUNC_GENERIC_PARM_NAME,index);
   }
}

static void createFpgiBySelf(int index,int genIndex,NString *codes)
{
   tree id=aet_utils_create_ident("self");
   tree parm=lookup_name(id);
   if(aet_utils_valid_tree(parm) && TREE_CODE(parm)==PARM_DECL){
	   n_string_append_printf(codes,"memcpy(&info.pms[%d],&(self->%s[%d]),sizeof(aet_generic_info));\n",genIndex,AET_GENERIC_ARRAY,index);
   }
}

/**
 * 当调用的泛型参数中有过定义的泛型如:E、T等
 *1.在当前函数中找，如果当前函数是泛型函数且声明的泛型与被调用的泛型数据未声明的相同，
 *则从当前函数的参数FuncGenParmInfo tempFgpi1234中给创建的tempFgpi1234赋值。
 *如果在当前函数找到E,T从当前类中找，如果找到从当前函数的参数self给被调用的函数的参数tempFgpi1234赋值。
 */
static NString *createUndefine(GenDefineData *gens,int *fromSelf,int *fromGenFunc)
{
	   ClassName *className=class_impl_get()->className;
	   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	   GenericModel *classModel=info->genericModel;
	   tree currentFunc=current_function_decl;
	   char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
	   ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,currentFuncName);
	   GenericModel *funcModel=class_func_get_func_generic(entity);
	   int i;
	   int count=gens->count;
	   NString *codes=n_string_new("");
	   for(i=0;i<count;i++){
		   GenericUnit *unit=gens->units[i];
		   if(!generic_unit_is_undefine(unit))
			   continue;
		   char *unitName=unit->name;
		   int findAtCurrentFunc=findGenDecl(funcModel,unitName) ;
		   int findAtClass=findGenDecl(classModel,unitName) ;
		   n_debug("在当前类和函数中找泛型声明:find:%d %d %s\n",findAtCurrentFunc,findAtClass,unitName);
		   if(findAtCurrentFunc>=0){
		       n_debug("在当前泛型函数%s中找到泛型定义%s,在当前泛型函数的第%d个位置定义了%s\n",currentFuncName,unitName,findAtCurrentFunc,unitName);
			   createFpgiByFunc(findAtCurrentFunc,i,codes);
			   *fromGenFunc=1;
		   }else if(findAtClass>=0){
		       n_debug("在当前函数%s中的参数self中找到泛型定义%s,在self的第%d个位置定义了%s\n",currentFuncName,unitName,findAtClass,unitName);
			   createFpgiBySelf(findAtClass,i,codes);
			   *fromSelf=1;
		   }else{
			   findAtCurrentFunc=findGenDecl(funcModel,gens->classOrFuncDecl[i]) ;
	    	   findAtClass=findGenDecl(classModel,gens->classOrFuncDecl[i]) ;
			   if(findAtCurrentFunc>=0){
				   createFpgiByFunc(findAtCurrentFunc,i,codes);
				   *fromGenFunc=1;
			   }else if(findAtClass>=0){
				   createFpgiBySelf(findAtClass,i,codes);
				   *fromSelf=1;
			   }else{
				   error_at(input_location,"调用泛型时参数是类型E、T等泛型声明。但在类%qs和函数%qs中没有找到泛型声明:%qs",className->userName,currentFuncName,unitName);
				   return NULL;
			   }
		   }
   	   }
	   return codes;
}

static NString *createDefine(GenDefineData *gens,ClassFunc *func)
{
	GenericModel *funcGen=class_func_get_func_generic(func);//泛型函数中原声明的泛型
	NString *codes=n_string_new("");
    location_t loc=input_location;
	int i;
	int genericCount=gens->count;
	for(i=0;i<genericCount;i++){
		GenericUnit *unit=gens->units[i];
		GenericUnit *unitDecl=generic_model_get(funcGen,i);
		n_debug("createdefine :%d %s %p\n",i,unit->name,gens);
		if(generic_unit_is_undefine(unit))
			  continue;
		int index=i;
		char *genStr=unitDecl->name;
		char *typeName=NULL;
		tree type=TREE_TYPE(unit->decl);
		class_util_get_type_name(type,&typeName);
		int genericName=genStr[0];
		int typeEnum=generic_util_get_generic_type(type);
		if(typeEnum==-1){
		   n_warning("找不到类型对应的泛型。%s",get_tree_code_name(TREE_CODE(type)));
		}
		int pointerCount=unit->pointerCount;
		tree value=c_sizeof(loc,type);
		wide_int result=wi::to_wide(value);
		int size=result.to_shwi();
		n_string_append_printf(codes,"strcpy(info.pms[%d].typeName,\"%s\");\n",index,typeName);
		n_string_append_printf(codes,"info.pms[%d].genericName=%d;\n",index,genericName);
		n_string_append_printf(codes,"info.pms[%d].type=%d;\n",index,typeEnum);
		n_string_append_printf(codes,"info.pms[%d].pointerCount=%d;\n",index,pointerCount);
		n_string_append_printf(codes,"info.pms[%d].size=%d;\n",index,size);
	}
	return codes;
}

static void compile(GenericFunc *self,vec<tree, va_gc> *exprlist,char *codes)
{
	tree target=generic_util_create_target(codes);
	vec_safe_insert(exprlist,1,target);
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


/**
 * 1.检查泛型函数的泛型声明
 * 2.收集用户定义的泛型
 * 3.从参数收集泛型
 * 4.比较用户定义的泛型与参数的类型是否匹配
 * 5.取未定义的泛型数据
 * 6.生成创建参数tempFgpi1234的代码
 * 7.编译代码
 * 8.保存定义的泛型
 * 9.生成过定义的泛型依赖
 */
void generic_func_add_fpgi_actual_parm(GenericFunc *self,ClassFunc *func,ClassName *className,vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine)
{
    n_debug("generic_func_add_fpgi_actual_parm 00 检查泛型函数的泛型声明 %s file:%s\n",func->orgiName,in_fnames[0]);
	if(!checkGenericFuncDecl(func)){
		return NULL;
	}
	n_debug("generic_func_add_fpgi_actual_parm 11 收集用户定义的泛型 %s\n",func->orgiName);
    tree actual=(*exprlist)[0];
    aet_print_tree(actual);

	GenDefineData userDefine;
	memset(&userDefine,0,sizeof(GenDefineData));
	if(!createByUserDefine(func,funcGenericDefine,&userDefine)){
       return NULL;
	}
	n_debug("generic_func_add_fpgi_actual_parm 22 从参数收集泛型 %s userDefine.count：%d file:%s\n",func->orgiName,userDefine.count,in_fnames[0]);

	GenDefineData parmDefine;
	memset(&parmDefine,0,sizeof(GenDefineData));
	if(!createByParm(func,exprlist,&parmDefine)){
       return NULL;
	}

	if(funcGenericDefine==NULL && !validParm(&parmDefine)){
	  error_at(input_location,"调用函数%qs时，没有定义泛型单元、从参数中也不能推断泛型单元类型。",func->orgiName);
	  return NULL;
	}

	n_debug("generic_func_add_fpgi_actual_parm 33 比较用户定义的泛型与参数的类型是符匹配 %s %d %d file:%s\n",func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
    if(userDefine.count>0 && parmDefine.count>0){
	   if(!compareUserParm(&userDefine,&parmDefine)){
   		  error_at(input_location,"调用函数%qs时，定义的泛型与参数的类型不相同。",func->orgiName);
   		  return NULL;
	   }
    }

    int undefineCount=0;
    if(userDefine.count>0){
        n_debug("generic_func_add_fpgi_actual_parm 44 从用户定义中检查A-Z泛型%s %d %d file:%s\n",func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
       undefineCount=getUndefineCount(&userDefine);
    }else{
        n_debug("generic_func_add_fpgi_actual_parm 55 从参数中检查A-Z泛型%s %d %d file:%s\n",func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
       undefineCount=getUndefineCount(&parmDefine);
    }

    NString *undefineStr=NULL;
    int fromSelf=0; //不定的泛型最终是由self确定?
    int fromGenFunc=0;//不定的泛型最终是由所在的泛型函数确定?
    if(undefineCount>0){
		c_parser *parser=class_impl_get()->parser;
		n_debug("generic_func_add_fpgi_actual_parm 66 取未定义的泛型数据 %s %d %d\n",func->orgiName,parmDefine.count,userDefine.count);
        if(!parser->isAet){
			error_at(input_location,"在类外调用泛型函数%qs，但函数传参数没有定义泛型。",func->orgiName);
			return NULL;
        }
        if(userDefine.count>0)
        	undefineStr=createUndefine(&userDefine,&fromSelf,&fromGenFunc);
        else
        	undefineStr=createUndefine(&parmDefine,&fromSelf,&fromGenFunc);
    }
    n_debug("generic_func_add_fpgi_actual_parm 77 生成创建参数tempFgpi1234的代码 %s %d %d file:%s\n",func->orgiName,parmDefine.count,userDefine.count,in_fnames[0]);
    NString *defineStr=NULL;
    if(userDefine.count>0){
       defineStr=createDefine(&userDefine,func);
    }else{
       defineStr=createDefine(&parmDefine,func);
    }
    char *codes=NULL;
    if(userDefine.count>0){
        codes=createCodes(defineStr,undefineStr,userDefine.count,undefineCount,className->sysName,func->mangleFunName,fromSelf,fromGenFunc);
    }else{
        codes=createCodes(defineStr,undefineStr,parmDefine.count,undefineCount,className->sysName,func->mangleFunName,fromSelf,fromGenFunc);
    }
    if(defineStr!=NULL)
  		n_string_free(defineStr,TRUE);
  	if(undefineStr!=NULL)
  		n_string_free(undefineStr,TRUE);
  	n_debug("generic_func_add_fpgi_actual_parm 88 undefineCount:%d %s %s 编译代码 \n%s\n",undefineCount,func->orgiName,in_fnames[0],codes);
    if(undefineCount<=0){
    	//没有未定义的泛型
        if(userDefine.count>0){
            n_debug("generic_func_add_fpgi_actual_parm 99 用户定义的 %s %s\n",func->orgiName,in_fnames[0]);
           block_mgr_add_define_func_call(block_mgr_get(),className,func,funcGenericDefine);
    	   generic_expand_add_define_func_call(generic_expand_get(),className,func->mangleFunName,funcGenericDefine);
        }else{
           int i;
    	   GenericModel *newModel=generic_model_new_from_file();
           for(i=0;i<parmDefine.count;i++){
        	  generic_model_add_unit(newModel,parmDefine.units[i]);
           }
           n_debug("generic_func_add_fpgi_actual_parm 99 参数定义的 %s %s model:%s\n",func->orgiName,in_fnames[0],generic_model_tostring(newModel));
      	   block_mgr_add_define_func_call(block_mgr_get(),className,func,newModel);
   	       generic_expand_add_define_func_call(generic_expand_get(),className,func->mangleFunName,newModel);
        }
    	nboolean second= makefile_parm_is_second_compile(makefile_parm_get());
    	n_debug("编译的文件中调用了泛型函数-----是不是第二次:%d %s %s genericDefine:%s file:%s\n",
				second,className->sysName,func->orgiName,generic_model_tostring(funcGenericDefine),in_fnames[0]);
		if(!second){
			generic_file_use_generic(generic_file_get());//这是编第二次的关键
		}else{
			generic_expand_add_eof_tag(generic_expand_get());//加cpp_buffer到当前.c文件的prev，当编译.c结束时，可以加入块定义
		}
	    compile(self,exprlist,codes);
    }else{
        compile(self,exprlist,codes);
        if(userDefine.count>0)
        	block_mgr_add_undefine_func_call(block_mgr_get(),className,func,funcGenericDefine);
        else{
		    int i;
		    GenericModel *newModel=generic_model_new_from_file();
		    for(i=0;i<parmDefine.count;i++){
			  generic_model_add_unit(newModel,parmDefine.units[i]);
		    }
		    block_mgr_add_undefine_func_call(block_mgr_get(),className,func,newModel);
        }

    }
    return NULL;
}


GenericFunc *generic_func_new()
{
	 GenericFunc *self =n_slice_alloc0 (sizeof(GenericFunc));
	 return self;
}


