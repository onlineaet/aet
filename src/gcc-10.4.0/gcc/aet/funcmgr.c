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
#include "funcmgr.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "aet-c-parser-header.h"
#include "genericutil.h"
#include "classutil.h"
#include "varmgr.h"


static void freeClass_cb(npointer userData)
{
  printf("funcmgr free free %p\n",userData);
}

static void funcMgrInit(FuncMgr *self)
{
	self->hashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, freeClass_cb);
	self->staticHashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, freeClass_cb);
	self->mangle=aet_mangle_new();
}


static nboolean existsStaticFuncAtField(FuncMgr *self,char *funcName,ClassName *className)
{
	if(!n_hash_table_contains(self->hashTable,className->sysName)){
		return FALSE;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	int i;
	for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->mangleNoSelfName,funcName)==0){
		   printf("existsStaticFuncAtField ???class??????field??????????????????????????????: mangle???%s funcName:%s className:%s\n",
				   item->mangleFunName,funcName,className->sysName);
		   return TRUE;
	   }
	}
    return FALSE;
}

static nboolean existsSameStaticFunc(FuncMgr *self,char *newNameNoSelf,ClassName *className)
{
	if(!n_hash_table_contains(self->staticHashTable,className->sysName)){
		return FALSE;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->mangleFunName,newNameNoSelf)==0){
		   n_debug("existsSameStaticFunc ???????????????mangle  %s",item->mangleFunName);
		   return TRUE;
	   }
	}
    return FALSE;
}

/**
 * location_t ??? libcpp/line-map.h?????????:typedef unsigned int location_t;
 */
static location_t getDeclaratorLocation(struct c_declarator *declarator)
{
   location_t id_loc=declarator->id_loc;
   if(id_loc==0){
	   bool macroLoc=linemap_location_from_macro_expansion_p (line_table,id_loc);
	   if(macroLoc){
		   n_warning("getDeclaratorLocation is ----?????????????????? %u",id_loc);
		   return id_loc;
	   }
   }
   if(id_loc>2*input_location){
	   n_warning("struct c_declarator ?????????????????????????????????????????????!!! %u %u\n",id_loc,input_location);
	   id_loc=input_location;
	   declarator->id_loc=input_location;
   }
   expanded_location  xloc = expand_location(id_loc);
   if(xloc.line==0 && xloc.column==0){
	  expanded_location  xloc1 = expand_location(input_location);
	  n_info("getDeclaratorLocation ????????? ???input_location %d %d %d %d",xloc.line,xloc.column,xloc1.line,xloc1.column);
	  id_loc=input_location;
   }
   return id_loc;
}


static ClassFunc *getEntity(NPtrArray *array,char *mangle)
{
   int i;
   for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->mangleFunName,mangle)==0){
		   n_debug("getEntity ???????????????mangle %s %s",item->mangleFunName,mangle);
		   return item;
	   }
   }
   return NULL;
}

static ClassFunc *getClassFunc(FuncMgr *self,tree decl,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL){
		n_warning("????????????%s??????????????????",className->sysName);
		return NULL;
	}
	tree funName=DECL_NAME(decl); //?????????
	char *mangleName=IDENTIFIER_POINTER(funName);
	ClassFunc *my=getEntity(array,mangleName);
	if(my==NULL){
		n_debug("???%s????????????%s???????????????",className->sysName,mangleName);
		return NULL;
	}
	return my;
}

static ClassFunc *getClassFuncFromStatic(FuncMgr *self,tree decl,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	if(array==NULL){
		n_warning("????????????%s??????????????????",className->sysName);
		return NULL;
	}
	tree funName=DECL_NAME(decl); //?????????
	char *mangleName=IDENTIFIER_POINTER(funName);
	ClassFunc *my=getEntity(array,mangleName);
	if(my==NULL){
	    n_debug("???%s????????????%s???????????????",className->sysName,mangleName);
		return NULL;
	}
	return my;
}

static int getSerialNumber(FuncMgr *self,ClassName *className)
{
    int max1=var_mgr_get_max_serial_number(var_mgr_get(),className);
    int max2=func_mgr_get_max_serial_number(self,className);
    return max2>max1?(max2+1):(max1+1);
}

static ClassFunc *createEntity(tree decl,tree classTree,char *mangle,char *orgiName,enum func_from_code code,nboolean isCtor,
		                         nboolean isFinalized,nboolean isUnref,char *mangleNoSelfName,char *rawMangleName)
{
	ClassFunc *item=class_func_new();
	item->isAbstract=FALSE;
	item->isCtor=isCtor;
	item->isFinalized=isFinalized;
	item->isUnref=isUnref;
	item->orgiName=n_strdup(orgiName);
	item->mangleFunName=n_strdup(mangle);
	item->mangleNoSelfName=n_strdup(mangleNoSelfName);
	item->rawMangleName=n_strdup(rawMangleName);
    item->isStatic=(mangleNoSelfName==NULL)?TRUE:FALSE;
	item->fieldDecl=NULL_TREE;
	item->fromImplDecl=NULL_TREE;
	item->fromImplDefine=NULL_TREE;
    item->classTree=classTree;
	if(code==STRUCT_DECL){
	  item->fieldDecl=decl;
	}else if(code==CLASS_IMPL_DECL){
	  item->fromImplDecl=decl;
	}else if(code==CLASS_IMPL_DEFINE){
	  item->fromImplDefine=decl;
	}
	item->permission=CLASS_PERMISSION_DEFAULT;
	return item;
}

static nboolean setEntity(ClassFunc *item,tree decl,ClassName *className,tree classTree,char *mangle,char *orgiName,enum func_from_code code)
{
	 nboolean repeat=FALSE;
	 if(code==STRUCT_DECL){
		 if(!aet_utils_valid_tree(item->fieldDecl)){
			item->fieldDecl=decl;
			item->classTree=classTree;
		 }else{
			repeat=TRUE;
		 }
	 }else if(code==CLASS_IMPL_DECL){
		 if(!aet_utils_valid_tree(item->fromImplDecl)){
			  item->fromImplDecl=decl;
		 }else{
			 repeat=TRUE;
		 }
	 }else if(code==CLASS_IMPL_DEFINE){
		 if(!aet_utils_valid_tree(item->fromImplDefine)){
		    if(item->isAbstract){
			  error_at(input_location,"???%qs??????????????????%qs???????????????????????????",className->userName,orgiName);
			  return FALSE;
		    }
		    item->fromImplDefine=decl;
		  }else{
			  repeat=TRUE;
		  }
	 }
	 return repeat;
}



static nboolean  setFieldDecl(FuncMgr *self,tree decl,ClassName *className,enum func_from_code code)
{
	ClassFunc *func=getClassFunc(self,decl,className);
	if(func==NULL)
		return FALSE;
	return class_func_set_decl(func,decl,code);
}

static void fillGenericFuncType(tree args,int *isQueryFunc,int *haveGenericClass,int *allParmIsQuery)
{
	tree parm=NULL_TREE;
	int count=0;
	int queryCount=0;
	int noQueryCount=0;
	for (parm = args; parm; parm = DECL_CHAIN (parm)){
		GenericModel *gen=c_aet_get_generics_model(parm);
		if(gen && count>0){ //??????self
			nboolean find=generic_model_have_query(gen);
			n_debug("??????????????????????????????????????????:%d query:%d",count,find);
			if(find){
				*isQueryFunc=1;
				*haveGenericClass=1;
				 queryCount++;
				 //break;
			}else{
				tree type=TREE_TYPE(parm);
				char *className=class_util_get_class_name(type);
				if(className!=NULL){
					ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),className);
					if(class_info_is_generic_class(info)){
						*haveGenericClass=1;
						noQueryCount++;
					}
				}
			}
		}
		count++;
	}
	if(queryCount>0 && noQueryCount==0)
		*allParmIsQuery=1;
}

/**
 *  mangleNoself ??????mangle????????????self
 */
static int  addSubstitutions(FuncMgr *self,tree decl,tree classTree,ClassName *className,
		 char *mangle,char *orgiName,enum func_from_code code,char *mangleNoSelf,char *rawMangleName,tree parms)
{
	int ret=0;
	nboolean isCtor=strcmp(className->userName,orgiName)==0;
	nboolean isFinalized= aet_utils_is_finalize_name(className->userName,orgiName);
	nboolean isUnref= aet_utils_is_unref_name(className->userName,orgiName);
	nboolean isQueryFunc=0;
	nboolean isGenericParmFunc=0;
	nboolean allParmIsQuery=0;
	fillGenericFuncType(parms,&isQueryFunc,&isGenericParmFunc,&allParmIsQuery);
	//printf("addSubstitutions--ttt %s %d %d\n",orgiName,isQueryFunc,isGenericParmFunc);
	if(!n_hash_table_contains(self->hashTable,className->sysName)){
		ClassFunc *item=createEntity(decl,classTree,mangle,orgiName,code,isCtor,isFinalized,isUnref,mangleNoSelf,rawMangleName);
		item->isQueryGenFunc=isQueryFunc;
		item->isGenericParmFunc=isGenericParmFunc;
		item->allParmIsQuery=allParmIsQuery;
		item->serialNumber=getSerialNumber(self,className);
		if(code==STRUCT_DECL)
			item->fieldParms=parms;
		NPtrArray *array=n_ptr_array_sized_new(2);
		n_ptr_array_add(array,item);
		n_debug("addSubstitutions 00 ???????????? class:%s mangle:%s org:%s rawMangleName:%s code:%d item:%p NPtrArray:%p self:%p\n",
				className->sysName,mangle,orgiName,rawMangleName,code,item,array,self);
		n_hash_table_insert (self->hashTable, n_strdup(className->sysName),array);
	 }else{
		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
		ClassFunc *item=getEntity(array,mangle);
		n_debug("addSubstitutions 11 ???????????? class:%s mangle:%s org:%s rawMangleName:%s ???????????????code:%d item:%p array:%p\n",
				className->sysName,mangle,orgiName,rawMangleName,code,item,array);
		if(item==NULL){
			item=createEntity(decl,classTree,mangle,orgiName,code,isCtor,isFinalized,isUnref,mangleNoSelf,rawMangleName);
			item->isQueryGenFunc=isQueryFunc;
			item->isGenericParmFunc=isGenericParmFunc;
			item->allParmIsQuery=allParmIsQuery;
	        item->serialNumber=getSerialNumber(self,className);
			n_ptr_array_add(array,item);
		}else{
			nboolean repeat=setEntity(item,decl,className,classTree,mangle,orgiName,code);
			if(repeat){
				ret=-1;
			}
		}
		if(code==STRUCT_DECL)
		    item->fieldParms=parms;
	 }
	 return ret;
}


/**
 * ???????????????FuncGenParmInfo *tempFgpi1234
 */
static nboolean isAetFgpiParm(tree decl)
{
    tree  type = TREE_TYPE (decl);
    tree  id=DECL_NAME(decl);
    if(!aet_utils_valid_tree(id))
    	return FALSE;
    char *name=IDENTIFIER_POINTER(DECL_NAME(decl));
    if(!name || strcmp(name,AET_FUNC_GENERIC_PARM_NAME))
    	return FALSE;
    if(TREE_CODE(type)!=POINTER_TYPE)
        return FALSE;
    type=TREE_TYPE(type);
 	if(TREE_CODE(type)==RECORD_TYPE){
		tree typeName=TYPE_NAME(type);
		if(TREE_CODE(typeName)==TYPE_DECL){
			char *na=IDENTIFIER_POINTER(DECL_NAME(typeName));
			if(na && strcmp(na,AET_FUNC_GENERIC_PARM_STRUCT_NAME)==0){
				return TRUE;
			}
		}
	}
    return FALSE;
}

/**
 * ???????????????????????????????????????FuncGenParmInfo ???????????? tempFgpi1234
 * ???????????????????????????????????????????????????????????????FuncGenParmInfo ???????????? tempFgpi1234
 */
static nboolean findFuncGenericOrQueryParm(struct c_declarator *funcdecl)
{
	struct c_arg_info *argsInfo=funcdecl->u.arg_info;
	tree args=argsInfo->parms;
	tree parm=NULL_TREE;
	int count=0;
	for (parm = args; parm; parm = DECL_CHAIN (parm)){
		if(count==1){
			return isAetFgpiParm(parm);
		}
		count++;
	}
	return FALSE;

}

/**
 * ?????????????????????????????? FuncGenParmInfo *tempFgpi1234,?????????funcdecl????????????????????????????????????
 * ??????????????????????????????FuncGenParmInfo * tempFgpi1234????????????
 */
static nboolean addFunc(FuncMgr *self,tree structTree,ClassName *className,enum func_from_code fromType,
		location_t id_loc,struct c_declarator *funcdecl,struct c_declspecs *specs)
{
	    struct c_declarator *funid=class_util_get_function_id(funcdecl);
	    if(funid==NULL)
	    	return FALSE;
	    tree argTypes = funcdecl->u.arg_info->types;
	  	tree funName=funid->u.id.id;
	    nboolean exitsAetGenericInfoParm=findFuncGenericOrQueryParm(funcdecl);
	    char *orgiName=n_strdup(IDENTIFIER_POINTER(funName));
	   	char *newName=aet_mangle_create_skip(self->mangle,funName,argTypes,className->userName,exitsAetGenericInfoParm?2:-1);
        //????????????self?????????????????????static??????
	    tree noSelfArgTypes = TREE_CHAIN (argTypes);
	   	char *newNameNoSelf=aet_mangle_create_skip(self->mangle,funName,noSelfArgTypes,className->userName,exitsAetGenericInfoParm?1:-1);
	   	char *staticNewName=aet_mangle_create_skip(self->mangle,funName,noSelfArgTypes,className->sysName,exitsAetGenericInfoParm?1:-1);
	   	char *rawMangleName=aet_mangle_create_skip(self->mangle,funName,noSelfArgTypes,"",exitsAetGenericInfoParm?1:-1);

		//printf("addFunc dddd vvv %s %s %s %s %s fromType:%d\n",orgiName,newName,newNameNoSelf,staticNewName,rawMangleName,fromType);
		//?????????????????????????????????
	  	if(existsSameStaticFunc(self,staticNewName,className)){
	  		if(fromType==CLASS_IMPL_DEFINE){
	  			n_debug("????????????????????????????????????????????? %s newNameNoSelf:%s\n",className->userName,staticNewName);
	  			if(exitsAetGenericInfoParm){
		  	        error_at(id_loc,"??????%qs?????????????????????????????????????????????????????? %qs",className->userName,orgiName);
		  	        return FALSE;
	  			}
	  			//??????self???????????????????????????staticNewName
	  		    tree value = aet_utils_create_ident (staticNewName);
	  		    funid->u.id.id=value;
	  		  	struct c_arg_info *args=funcdecl->u.arg_info;
	  		    args->types=noSelfArgTypes;
	  		    args->parms=TREE_CHAIN (args->parms);
	  		    specs->storage_class=csc_none;
	  			return TRUE;
	  		}else if(fromType==CLASS_IMPL_DECL){
		  	    error_at(id_loc,"??????%qs?????????????????????????????????????????????????????? %qs???",className->userName,orgiName);
	  		}else{
	  	        error_at(id_loc,"??????%qs???????????????????????????????????? %qs",className->userName,orgiName);
	  		}
	  	  	n_free(orgiName);
	  	  	n_free(newName);
	  	  	n_free(newNameNoSelf);
	  	  	n_free(rawMangleName);
	  	  	return FALSE;
	  	}
	  	//printf("addFunc 0000 %s %p code:%d exitsAetGenericInfoParm:%d\n",orgiName,funcdecl->u.arg_info->parms,fromType,exitsAetGenericInfoParm);

	  	int ret=addSubstitutions(self,NULL_TREE,structTree,className,newName,orgiName,
	  			fromType,newNameNoSelf,rawMangleName,funcdecl->u.arg_info->parms);
	  	//printf("addFunc 1111 %s %p code:%d\n",orgiName,funcdecl->u.arg_info->parms,fromType);
	    //printf("addFunc 00 sysName:%s userName:%s newFunName:%s newNameNoSelf:%s fromType:%d repeat:%d %s %s %d\n",
	    		//className->sysName,className->userName,newName,newNameNoSelf,fromType,repeat,__FILE__,__FUNCTION__, __LINE__);
	    nboolean result=FALSE;
	  	if(ret==-1){
	  	    error_at(id_loc,"??????%qs?????????????????????????????? %qs",className->userName,orgiName);
	  	}else{
	  	   tree value = aet_utils_create_ident (newName);
		   funid->u.id.id=value;
		   result=TRUE;
	  	}
	  	n_free(orgiName);
	  	n_free(newName);
	  	n_free(newNameNoSelf);
  	  	n_free(rawMangleName);
	  	return result;
}

static nboolean  changeFuncDeclAndDefine(FuncMgr *self,struct c_declspecs *specs,struct c_declarator *declarator,
		                ClassName *className,enum func_from_code fromType)
{
	location_t id_loc=getDeclaratorLocation(declarator);
	struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
	if(!funcdel)
		return FALSE;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
  	return addFunc(self,info->record,className,fromType,id_loc,funcdel,specs);
}


static nboolean  setStaticDecl(FuncMgr *self,tree decl,ClassName *className,enum func_from_code code)
{
	ClassFunc *item=getClassFuncFromStatic(self,decl,className);
	if(item==NULL)
		return FALSE;
	if(code==STRUCT_DECL){
	   item->fieldDecl=decl;
	}else if(code==CLASS_IMPL_DEFINE){
	    //printf("set fromImplDefine-- setStaticDecl %p %s\n",decl,item->orgiName);
	   item->fromImplDefine=decl;
	}
	return TRUE;
}

static nboolean  setImplFuncdecl(FuncMgr *self,tree decl,ClassName *className,enum func_from_code fromType)
{
	enum tree_code code=TREE_CODE(decl);
	if(code!=FUNCTION_DECL){
        n_warning("????????????????????????! %s",get_tree_code_name(TREE_CODE(decl)));
        return FALSE;
	}
	if(className==NULL){
        n_warning("CLASS ???????????? !");
        return FALSE;
	}
    //????????????????????????
	nboolean ok= setFieldDecl(self,decl,className,fromType);
	if(!ok && fromType==CLASS_IMPL_DEFINE){
		n_info("?????????????????????????????????????????? %s",IDENTIFIER_POINTER(DECL_NAME(decl)));
		nboolean re=setStaticDecl(self,decl,className,fromType);
		return re;
	}
	ClassFunc *func=getClassFunc(self,decl,className);
	ok=class_func_is_same_generic(func);
	GenericModel *fungen=c_aet_get_func_generics_model(decl);
	if(fungen){
       if(!aet_utils_valid_tree(func->fieldDecl)){
           error_at(DECL_SOURCE_LOCATION(decl),"??????%s???????????????????????????%qs???",className->sysName,func->orgiName);
           return FALSE;
       }
	}
	return ok;
}

nboolean  func_mgr_change_class_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className,tree structTree)
{
	location_t id_loc=getDeclaratorLocation(declarator);
	struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
	if(!funcdel)
		return FALSE;
  	return addFunc(self,structTree,className,STRUCT_DECL,id_loc,funcdel,NULL);
}

static nboolean checkSameFuncButRtnNotEqual(FuncMgr *self,tree decl,ClassName *className,ClassFunc *compare)
{
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    ClassName *parent=&(info->parentName);
    if(parent==NULL || parent->sysName==NULL)
    	return FALSE;
    NPtrArray  *array=func_mgr_get_funcs(self,parent);
    if(array==NULL){
	   return checkSameFuncButRtnNotEqual(self,decl,parent,compare);
    }
    int i;
    for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(aet_utils_valid_tree(item->fieldDecl)){
		  nboolean re= class_func_is_same_but_rtn(item,compare,decl);
		  if(re){
             error_at(DECL_SOURCE_LOCATION(decl),"??????%s???????????????????????????????????????????????????%s???",parent->sysName,item->orgiName);
             return TRUE;
		  }
	   }
   }
   return checkSameFuncButRtnNotEqual(self,decl,parent,compare);
}


/**
 * ????????????????????????????????????????????????????????????????????????????????????.
 * ???????????????????????????????????????????????????????????????
 * ???func_mgr_change_class_func_decl????????????
 */
nboolean  func_mgr_set_class_func_decl(FuncMgr *self,tree decl,ClassName *className)
{
	enum tree_code code=TREE_CODE(decl);
	if(code!=FIELD_DECL){
        n_warning("?????????????????????!");
        return FALSE;
	}
	//???????????????
	ClassFunc *my=getClassFunc(self,decl,className);
	if(my==NULL){
        n_warning("??????%s??????????????????:%s!",className->sysName,IDENTIFIER_POINTER(DECL_NAME(decl)));
	    return FALSE;
	}
	nboolean errorInfo=checkSameFuncButRtnNotEqual(self,decl,className,my);
	if(errorInfo){
		return;
	}
	nboolean ok= setFieldDecl(self,decl,className,STRUCT_DECL);
	return ok;
}

/**
 * ??????????????????field????????????self->_Z3Abc7getName=_Z3Abc7getName;
 */
char * func_mgr_create_field_modify_token(FuncMgr *self,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL)
		return NULL;
	NString *str=n_string_new("");
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(aet_utils_valid_tree(item->fromImplDefine) && aet_utils_valid_tree(item->fieldDecl)){
		   n_string_append_printf(str,"self->%s=%s;\n",item->mangleFunName,item->mangleFunName);
		   if(class_func_have_super(item)){
			   //class???????????????????????????mangle?????? super_NObject__Z7NObject8killbillEPN8_NObjectEwPc
			   //zclei super 3???
//			   char newFuncName[256];
//			   aet_utils_create_super_field_name(className->userName,item->mangleFunName,newFuncName);
//			   n_string_append_printf(str,"self->%s=%s;\n",newFuncName,item->mangleFunName);
		   }
//		   n_string_append_printf(str,"printf(\"????????????---%s:",item->mangleFunName);
//		   n_string_append(str,"---%p \\n\",self->");
//		   n_string_append_printf(str,"%s);\n",item->mangleFunName);
	   }
	}
	char *re=n_strdup(str->str);
	n_string_free(str,TRUE);
	return re;
}

NPtrArray  *func_mgr_get_funcs(FuncMgr *self,ClassName *className)
{
	if(className==NULL || className->sysName==NULL){
      return NULL;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	return array;
}

static ClassFunc *getClassFuncByMangleName(FuncMgr *self,char *sysName,char *mangle)
{
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
	if(array==NULL)
		return NULL;
    int i;
    NPtrArray* data=n_ptr_array_new();
    for(i=0;i<array->len;i++){
    	ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->mangleFunName,mangle)==0){
      		return item;
      	}
    }
    return NULL;
}

ClassFunc *func_mgr_get_entity(FuncMgr *self,ClassName *className,char *mangle)
{
    return getClassFuncByMangleName(self,className->sysName,mangle);
}

ClassFunc *func_mgr_get_entity_by_sys_name(FuncMgr *self,char *sysName,char *mangle)
{
    return getClassFuncByMangleName(self,sysName,mangle);
}

ClassFunc *func_mgr_get_entity_by_mangle(FuncMgr *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->hashTable);
	int count=0;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		ClassFunc *func=getClassFuncByMangleName(self,sysName,mangle);
		if(func!=NULL)
			return func;
	}
	return NULL;
}

char   *func_mgr_get_class_name_by_mangle(FuncMgr *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->hashTable);
	int count=0;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		ClassFunc *func=getClassFuncByMangleName(self,sysName,mangle);
		if(func!=NULL)
			return sysName;
	}
	return NULL;
}


nboolean  func_mgr_change_class_impl_func_define(FuncMgr *self,struct c_declspecs *specs,struct c_declarator *declarator,ClassName *className)
{
	return changeFuncDeclAndDefine(self,specs,declarator,className,CLASS_IMPL_DEFINE);
}

nboolean  func_mgr_change_class_impl_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className)
{
	return changeFuncDeclAndDefine(self,NULL,declarator,className,CLASS_IMPL_DECL);
}

nboolean  func_mgr_set_class_impl_func_decl(FuncMgr *self,tree decl,ClassName *className)
{
	return setImplFuncdecl(self,decl,className,CLASS_IMPL_DECL);
}

nboolean  func_mgr_set_class_impl_func_define(FuncMgr *self,tree decl,ClassName *className)
{
	return setImplFuncdecl(self,decl,className,CLASS_IMPL_DEFINE);
}

/**
 * ?????????????????????field???????????????????????????????????????true
 */
nboolean func_mgr_func_exits(FuncMgr *self,ClassName *className,char *orgiName)
{
	if(className==NULL || orgiName==NULL)
		return FALSE;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL)
		return FALSE;
    int i;
    for(i=0;i<array->len;i++){
    	ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->orgiName,orgiName)==0){
      		return TRUE;
      	}
    }
    return FALSE;
}

char * func_mgr_get_mangle_func_name(FuncMgr *self,ClassName *className,char *orgiName)
{
	if(className==NULL || orgiName==NULL)
		return NULL;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL)
		return NULL;
    int i;
    for(i=0;i<array->len;i++){
    	ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->orgiName,orgiName)==0){
      		return item->mangleFunName;
      	}
    }
    return NULL;
}

NPtrArray *func_mgr_get_constructors(FuncMgr *self,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL || array->len==0)
		return NULL;
	int i;
	NPtrArray* data=n_ptr_array_new();
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
		if(strcmp(item->orgiName,className->userName)==0 && aet_utils_valid_tree(item->fieldDecl)){
			//printf("func_mgr_get_constructors %s %s\n",item->mangleFunName,className->sysName);
			n_ptr_array_add(data,item);
		}
	}
	return data;
}

int func_mgr_get_orgi_func_and_class_name(FuncMgr *self,char *newName,char *className,char *funcName)
{
	return  aet_mangle_get_orgi_func_and_class_name(self->mangle,newName,className,funcName);
}


//--------------------------------?????????????????????------------------------------------
nboolean func_mgr_set_static_func_premission(FuncMgr *self,ClassName *className,tree funDecl,ClassPermissionType permission,nboolean isFinal)
{
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
   if(array==NULL)
	   return FALSE;
   int i;
   for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(item->fieldDecl==funDecl){
		   item->permission=permission==CLASS_PERMISSION_DEFAULT?CLASS_PERMISSION_PROTECTED:permission;
		   item->isFinal=isFinal;
		   return TRUE;
	   }
   }
   return FALSE;
}

/**
 * ???????????????????????????????????????????????????field?????????????????????????????????
 */
char *func_mgr_create_static_var_name(FuncMgr *self,ClassName *className,tree varName,tree type)
{
	return aet_mangle_create_static_var_name(self->mangle,className,varName,type);
}

nboolean  func_mgr_change_static_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className,tree structTree)
{
	location_t id_loc=getDeclaratorLocation(declarator);
	struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
	if(!funcdel)
		return FALSE;
	struct c_declarator *funid=funcdel->declarator;
	if(funid==NULL)
		return FALSE;
	enum c_declarator_kind kind=funid->kind;
	if(kind!=cdk_id)
		return FALSE;
  	struct c_arg_info *args=funcdel->u.arg_info;
    tree argTypes = args->types;
  	tree funName=funid->u.id.id;
    char *orgiName=n_strdup(IDENTIFIER_POINTER(funName));
    char *newName=aet_mangle_create(self->mangle,funName,argTypes,className->userName);
    char *newSysName=aet_mangle_create(self->mangle,funName,argTypes,className->sysName);
   	char *rawMangleName=aet_mangle_create(self->mangle,funName,argTypes,"");
    if(existsSameStaticFunc(self,newSysName,className)){
        n_debug("func_mgr_change_static_func_decl 11 existsStaticFunc %s %s class:%s\n",newName,newSysName,className->sysName);
 	   error_at (id_loc,"%qE ?????????????????????????????????????????????????????? ???", funName);
 	   n_free(orgiName);
 	   n_free(newName);
 	   n_free(newSysName);
 	   n_free(rawMangleName);
 	   return FALSE;
    }

    if(existsStaticFuncAtField(self,newName,className)){
        n_debug("func_mgr_change_static_func_decl 22 existsStaticFunc %s %s class:%s\n",newName,newSysName,className->sysName);
 	   error_at (id_loc,"%qE ?????????????????????????????????????????????????????? ???", funName);
 	   n_free(orgiName);
 	   n_free(newName);
 	   n_free(newSysName);
 	   n_free(rawMangleName);
 	   return FALSE;
    }
	nboolean isQueryFunc=0;
	nboolean isGenericParmFunc=0;
	nboolean allParmIsQuery=0;
	fillGenericFuncType(args->parms,&isQueryFunc,&isGenericParmFunc,&allParmIsQuery);
    nboolean result=TRUE;
   	if(!n_hash_table_contains(self->staticHashTable,className->sysName)){
   	    ClassFunc *item=createEntity(NULL_TREE,structTree,newSysName,orgiName, STRUCT_DECL,FALSE,FALSE,FALSE,NULL,rawMangleName);
		item->isQueryGenFunc=isQueryFunc;
		item->isGenericParmFunc=isGenericParmFunc;
		item->allParmIsQuery=allParmIsQuery;
        item->serialNumber=getSerialNumber(self,className);
   		NPtrArray *array=n_ptr_array_sized_new(2);
   		n_ptr_array_add(array,item);
   		n_debug("func_mgr_change_static_func_decl 33 ???????????? class:%s mangle:%s org:%s code:%d item:%p NPtrArray:%p self:%p\n",
   				className->sysName,newSysName,orgiName,STRUCT_DECL,item,array,self);
   		n_hash_table_insert (self->staticHashTable, n_strdup(className->sysName),array);
   	 }else{
   		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
   		ClassFunc *item=getEntity(array,newSysName);
   		n_debug("func_mgr_change_static_func_decl 44 ???????????? class:%s mangle:%s org:%s ???????????????code:%d item:%p array:%p\n",
   				className->sysName,newSysName,orgiName,STRUCT_DECL,item,array);
   		if(item==NULL){
   	   	    item=createEntity(NULL_TREE,structTree,newSysName,orgiName, STRUCT_DECL,FALSE,FALSE,FALSE,NULL,rawMangleName);
   			item->isQueryGenFunc=isQueryFunc;
   			item->isGenericParmFunc=isGenericParmFunc;
   			item->allParmIsQuery=allParmIsQuery;
   	        item->serialNumber=getSerialNumber(self,className);
   			n_ptr_array_add(array,item);
   		}else{
   		    error_at (id_loc,"%qE ?????????????????????????????????????????????????????? ???", funName);
   		    result=FALSE;
   		}
   	 }
   	 if(result){
	   tree value = aet_utils_create_ident (newSysName);
       funid->u.id.id=value;
   	 }
     n_free(orgiName);
     n_free(newName);
     n_free(newSysName);
  	 n_free(rawMangleName);
  	 return result;
}

nboolean  func_mgr_set_static_func_decl(FuncMgr *self,tree funcDecl,ClassName *className)
{
	enum tree_code code=TREE_CODE(funcDecl);
	if(code!=FUNCTION_DECL){
		error_at (DECL_SOURCE_LOCATION (funcDecl),"%qD ?????????????????? ???", funcDecl);
        return FALSE;
	}
    //????????????????????????
	tree funName=DECL_NAME(funcDecl); //?????????
	char *mangleName=IDENTIFIER_POINTER(funName);
	n_debug("func_mgr_set_static_func_decl 00 %s funcDecl:%p",mangleName,funcDecl);
	aet_print_tree(funcDecl);
	return setStaticDecl(self,funcDecl,className,STRUCT_DECL);
}

nboolean func_mgr_static_func_exits(FuncMgr *self,ClassName *className,char *orgiName)
{
	if(className==NULL || orgiName==NULL)
		return FALSE;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	if(array==NULL)
		return FALSE;
    int i;
    for(i=0;i<array->len;i++){
        ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->orgiName,orgiName)==0){
      		return TRUE;
      	}
    }
    return FALSE;
}

NPtrArray    *func_mgr_get_static_funcs(FuncMgr *self,ClassName *className)
{
	if(className==NULL){
		return NULL;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	return array;
}

NPtrArray    *func_mgr_get_static_funcs_by_sys_name(FuncMgr *self,char *sysName)
{
    if(sysName==NULL){
        return NULL;
    }
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,sysName);
    return array;
}

nboolean func_mgr_static_func_exits_by_recursion(FuncMgr *self,ClassName *srcName,tree component)
{
	if(srcName==NULL || component==NULL_TREE)
		return FALSE;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),srcName);
	if(info==NULL)
		return FALSE;
	nboolean exits=func_mgr_static_func_exits(self,&info->className,IDENTIFIER_POINTER(component));
	if(exits)
		return TRUE;
	return func_mgr_static_func_exits_by_recursion(self,&info->parentName,component);
}

/**
 * ???????????????????????????????????????
 */
nboolean  func_mgr_have_generic_func(FuncMgr *self,ClassName *className)
{
	if(className==NULL)
		return FALSE;
    NPtrArray  *array=func_mgr_get_funcs(self,className);
    if(array==NULL)
    	return FALSE;
    int i;
    for(i=0;i<array->len;i++){
  	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
  	   if(aet_utils_valid_tree(item->fieldDecl)){
  		  GenericModel *gen=c_aet_get_func_generics_model(item->fieldDecl);
  		  if(gen)
  			 return TRUE;
  	   }
    }
    return FALSE;
}

nboolean  func_mgr_is_generic_func(FuncMgr *self,ClassName *className,char *mangleFuncName)
{
	   if(className==NULL)
			return FALSE;
	    NPtrArray  *array=func_mgr_get_funcs(self,className);
	    if(array==NULL)
	    	return FALSE;
	    int i;
	    for(i=0;i<array->len;i++){
	  	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	  	   if(!strcmp(item->mangleFunName,mangleFuncName)){
	  	      if(aet_utils_valid_tree(item->fieldDecl)){
	  		     GenericModel *gen=c_aet_get_func_generics_model(item->fieldDecl);
	  		     if(gen)
	  		   	   return TRUE;
	  	      }
	  	   }
	    }
	    return FALSE;
}

/**
 * ???function_type????????????????????????
 */
char *func_mgr_create_parm_string(FuncMgr *self,tree funcType)
{
   return aet_mangle_create_parm_string(self->mangle,funcType);
}

/**
 * ???????????????????????????????????????
 */
static nboolean compareFunctionType(tree define,tree field)
{
    tree returnType=NULL_TREE;
    if(TREE_CODE(define)!=FIELD_DECL){
        tree funType=TREE_TYPE(define);
        returnType=TREE_TYPE(funType);
    }else{
        tree fieldType0=TREE_TYPE(define);
        tree fieldFunType0=TREE_TYPE(fieldType0);
        returnType=TREE_TYPE(fieldFunType0);
    }
    tree fieldType=TREE_TYPE(field);
    tree fieldFunType=TREE_TYPE(fieldType);
    tree fieldReturnType=TREE_TYPE(fieldFunType);
    bool re=c_tree_equal (returnType,fieldReturnType);
    return re;
}

/**
 * ??????from???????????????????????????????????????.
 * from??????????????????????????????????????????AObject
 */
ClassFunc *func_mgr_get_interface_impl(FuncMgr *self,ClassName *from,ClassFunc *interfaceMethod,char **atClass)
{
	 NPtrArray  *srcFuncs=func_mgr_get_funcs(func_mgr_get(),from);
	 char *praw=interfaceMethod->rawMangleName;
	 int i;
	 if(srcFuncs!=NULL){
		 for(i=0;i<srcFuncs->len;i++){
			 ClassFunc *compareFunc=(ClassFunc *)n_ptr_array_index(srcFuncs,i);
			 if(strcmp(compareFunc->rawMangleName,praw)==0){
			     n_debug("??????????????????????????????fieldDecl:%p %s %s public:%d\n",compareFunc->fieldDecl,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
				 if(aet_utils_valid_tree(compareFunc->fromImplDefine)){
				     n_debug("??????????????????????????????????????????:%p %s %s public:%d\n",compareFunc->fromImplDefine,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
					 if(compareFunctionType(compareFunc->fromImplDefine,interfaceMethod->fieldDecl)){
						 *atClass=n_strdup(from->sysName);
						 return compareFunc;
					 }
				 }else if(aet_utils_valid_tree(compareFunc->fieldDecl) && class_func_is_public(compareFunc)){
				     n_debug("?????????????????????????????????????????????:%p %s %s public:%d\n",compareFunc->fieldDecl,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
					 if(compareFunctionType(compareFunc->fieldDecl,interfaceMethod->fieldDecl)){
						 *atClass=n_strdup(from->sysName);
						 return compareFunc;
					 }
				 }
			 }
		 }
	 }
	 ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
	 if(info==NULL)
		 return NULL;
	 if(info->parentName.sysName==NULL)
		 return NULL;
	 n_debug("??????????????????????????????praw:%s from:%s parent:%s\n",praw,from->sysName,info->parentName.sysName);
	 return func_mgr_get_interface_impl(self,&info->parentName,interfaceMethod,atClass);
}

/**
 * ??????????????????????????? ?????? AHashFunc var=Abc.strHashFunc?????????
* typedef auint (*AHashFunc) (aconstpointer  key);
 * AHashFunc var=Abc.strHashFunc;
 * class Abc{
 *   public$ static auint strHashFunc(aconstpointer key);
 * }
 */
ClassFunc *func_mgr_get_static_method(FuncMgr *self,char *sysName,char *mangle)
{
	if(sysName==NULL || mangle==NULL)
		return NULL;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,sysName);
	if(array==NULL)
			return NULL;
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
		if(strcmp(item->mangleFunName,mangle)==0){
			return item;
		}
	}
	return NULL;
}

static ClassFunc *getStaticClassFuncByMangleName(FuncMgr *self,char *sysName,char *mangle)
{
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,sysName);
	if(array==NULL)
		return NULL;
    int i;
    NPtrArray* data=n_ptr_array_new();
    for(i=0;i<array->len;i++){
    	ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->mangleFunName,mangle)==0){
      		return item;
      	}
    }
    return NULL;
}

/**
 * ??????mangle?????????????????????????????????????????????
 */
char   *func_mgr_get_static_class_name_by_mangle(FuncMgr *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->staticHashTable);
	int count=0;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		ClassFunc *func=getStaticClassFuncByMangleName(self,sysName,mangle);
		if(func!=NULL)
			return sysName;
	}
	return NULL;
}

int func_mgr_get_max_serial_number(FuncMgr *self,ClassName *className)
{
       NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
       NPtrArray *staticArray=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
       int max=-1;
       if(array!=NULL){
           int len=array->len;
           int i;
           for(i=0;i<len;i++){
               ClassFunc *item=n_ptr_array_index(array,i);
               if(item->serialNumber>max)
                   max=item->serialNumber;
           }
       }
       if(staticArray!=NULL){
          int len=staticArray->len;
          int i;
          for(i=0;i<len;i++){
             ClassFunc *item=n_ptr_array_index(staticArray,i);
             if(item->serialNumber>max)
                 max=item->serialNumber;
          }
       }
       return max;
}

void func_mgr_check_permission_decl_between_define(FuncMgr *self,location_t loc,ClassName *className,
        char *mangle,nboolean havePermission,ClassPermissionType permission)
{
    if(!havePermission)
        return;
    ClassFunc *func=func_mgr_get_entity_by_sys_name(self,className->sysName,mangle);
    if(func==NULL){
        func=func_mgr_get_static_method(self,className->sysName,mangle);
    }
    if(func==NULL)
        return ;
    if((func->permission==CLASS_PERMISSION_DEFAULT || func->permission==CLASS_PERMISSION_PROTECTED) &&
            (permission==CLASS_PERMISSION_DEFAULT || permission==CLASS_PERMISSION_PROTECTED))
        return;
    if(func->permission!=permission && aet_utils_valid_tree(func->fieldDecl)){
        warning_at(loc,0,"?????????????????????%qs???????????????????????????????????????????????????",func->orgiName);
    }
}

FuncMgr *func_mgr_get()
{
	static FuncMgr *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(FuncMgr));
		 funcMgrInit(singleton);
	}
	return singleton;
}





