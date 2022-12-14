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
#include "classbuild.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classbuild.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"
#include "varmgr.h"

static void classBuildBuild(ClassBuild *self)
{
}

static void createVar(char *className,char *result)
{
	char temp[255];
    sprintf(temp,"aclass%s",className);
	int hash=aet_utils_create_hash(temp,strlen(temp));
	sprintf(result,"_%s%u",temp,hash);
}

/**
 * ????????????aclass??????????????????:
 * static  AClass *_createAClass_debug_ARandom_123(debug_ARandom *self)
   {
       static AClass *_aclassdebug_ARandom3488912659=NULL;
       if(_aclassdebug_ARandom3488912659!=NULL){
           return _aclassdebug_ARandom3488912659;
       }
       debug_AClass *obj=({ debug_AClass *temp;
           temp=debug_AClass.newObject(sizeof(debug_AClass));
           temp->objectSize=sizeof(debug_AClass);
           debug_AClass_init_1234ergR5678_debug_AClass(temp);
           temp->AClass();temp;});
       obj->name=strdup("ARandom");//?????????????????????
       obj->packageName=strdup("com.dnn");
      _aclassdebug_ARandom3488912659=obj;
       return _aclassdebug_ARandom3488912659;
    }
 */
void class_build_create_func_name(ClassName *className,char *buf)
{
	 sprintf(buf,"_createAClass_%s_123",className->sysName);
}

void class_build_create_func_name_by_sys_name(char *sysName,char *buf)
{
     sprintf(buf,"_createAClass_%s_123",sysName);
}

/**
 * ??????????????????impl????????????????????????????????????????????????
 * ???????????????????????????????????????self??????????????????
 * AClass *obj=new$ AClass()??????:
 * AObject *obj=({ test_AClass *_notv2_11test_AClass1;
_notv2_11test_AClass1=test_AClass.newObject(sizeof(test_AClass));
_notv2_11test_AClass1->objectSize=sizeof(test_AClass);
test_AClass_init_1234ergR5678_test_AClass(_notv2_11test_AClass1);
_notv2_11test_AClass1->AClass();
_notv2_11test_AClass1;});
*????????????int aet_utils_add_token(cpp_reader *pfile, const char *str,size_t len);
*?????????????????????????????????????????????iface????????????????????????????????????aet_utils_add_token??????????????????????????????cpp_buffer?????????????????????
*double free or corruption (!prev)????????????????????????AClass???new$ ACLass();???????????????
 */
void class_build_create_codes(ClassBuild *self,ClassName *className,NString *codes)
{
   char funcName[255];
   class_build_create_func_name(className,funcName);
   char varName[255];
   createVar(className->sysName,varName);
   ClassName *oName=class_mgr_get_class_name_by_user(class_mgr_get(), AET_ROOT_OBJECT);
   ClassName *cName=class_mgr_get_class_name_by_user(class_mgr_get(), AET_ROOT_CLASS);
   if(strcmp(oName->package,cName->package)){
	    n_error("%s???%s?????????????????????????????????%s %s",AET_ROOT_OBJECT,AET_ROOT_CLASS,oName->package,cName->package);
	    return;
   }
   ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   char *varInitCodes=class_info_create_class_code(classInfo);
   char *initMethod=aet_utils_create_init_method(cName->sysName);
   n_string_append_printf(codes,"static  AClass *%s(AObject *self)\n",funcName);
   n_string_append(codes,       "{\n");
   n_string_append_printf(codes,"    static AClass *%s=NULL;\n",varName);
   n_string_append_printf(codes,"    if(%s!=NULL){\n",varName);
   n_string_append_printf(codes,"         return %s;\n",varName);
   n_string_append(codes,       "    }\n");
   n_string_append_printf(codes,"    %s *obj=({ %s *temp;\n",cName->sysName,cName->sysName);
   n_string_append_printf(codes,"           temp=%s.newObject(sizeof(%s));\n",cName->sysName,cName->sysName);
   n_string_append_printf(codes,"           temp->objectSize=sizeof(%s);\n",cName->sysName);
   n_string_append_printf(codes,"           %s(temp);\n",initMethod);//?????????????????????
   n_string_append(codes,"                  temp->AClass();\n");
   n_string_append(codes,"                  temp;});\n");
   n_string_append(codes,varInitCodes);
   n_string_append_printf(codes,"    obj->%s=%d;\n",AET_MAGIC_NAME,AET_MAGIC_NAME_VALUE);//?????????
   n_string_append_printf(codes,"    %s=obj;\n",varName);
   n_string_append_printf(codes,"     return %s;\n",varName);
   n_string_append(codes,"}\n");
}


/**
 * Abc.class?????????????????????????????????self???????????????
 * test_ARandom_init_1234ergR5678_test_ARandom(NULL);
 */
tree class_build_get_func(ClassBuild *self,ClassName *className)
{
	  char *funcName=aet_utils_create_init_method(className->sysName);
	  tree id=aet_utils_create_ident(funcName);
	  tree funcdel=lookup_name(id);
	  if(!aet_utils_valid_tree(funcdel)){
		 n_error("???????????????:%s,?????????????????????????????????????????????",funcName);
	  }
	  n_free(funcName);
      c_parser *parser=self->parser;
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+9>AET_MAX_TOKEN){
			error("token?????????");
			return NULL_TREE;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
		 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+9]);
	  }
	   aet_utils_create_token(&parser->tokens[0],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_token(&parser->tokens[1],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_token(&parser->tokens[2],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_void_token(&parser->tokens[3],input_location);
	   aet_utils_create_token(&parser->tokens[4],CPP_MULT,"*",1);
	   aet_utils_create_token(&parser->tokens[5],CPP_CLOSE_PAREN,")",1);
	   aet_utils_create_number_token(&parser->tokens[6],0);
	   aet_utils_create_token(&parser->tokens[7],CPP_CLOSE_PAREN,")",1);
	   aet_utils_create_token(&parser->tokens[8],CPP_CLOSE_PAREN,")",1);
	   parser->tokens_avail=tokenCount+9;
	   aet_print_token_in_parser("class_build_get_func ------");
	   return funcdel;
}

static tree createRtnType(ClassName *className)
{
	tree recordId=aet_utils_create_ident(className->sysName);
	tree record=lookup_name(recordId);
	if(!record || record==NULL_TREE || record==error_mark_node){
		printf("???????????? %s\n",className);
		error("???????????? class");
	}
	tree recordType=TREE_TYPE(record);
	tree pointer=build_pointer_type(recordType);
	return pointer;
}


static nboolean relaceGetClassFieldRtn(tree fieldDecl,ClassInfo *aclassInfo)
{

    tree fieldType=TREE_TYPE(fieldDecl);
    tree fieldFunType=TREE_TYPE(fieldType);
    tree fieldReturnType=TREE_TYPE(fieldFunType);
    int count=0;
    tree selftype=NULL_TREE;
    for (tree al = TYPE_ARG_TYPES (fieldFunType); al; al = TREE_CHAIN (al)){
       tree type=TREE_VALUE(al);
	   if(type == void_type_node){
          break;
	   }else{
		   selftype=type;
	   }
	   count++;
    }
    if(count!=1)
    	return FALSE;
	tree param_type_list = NULL;
    param_type_list = tree_cons (NULL_TREE, selftype, param_type_list);
    param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);//?????????????????????????????????????????????void_type_node
    param_type_list = nreverse (param_type_list);
    tree rtntype=createRtnType(&aclassInfo->className);
	tree fntype = build_function_type (rtntype, param_type_list);
    TREE_TYPE(fieldType)=fntype;
    return TRUE;
}

/**
 * ??????AObject getClass???????????????????????????????????????AClass????????????
 * ?????? AClass *getClass ???????????? void *getClass
 * ??????AClass????????????????????????void *getClass?????????AClass *getClass
 * ?????????????????????????????????
 */
void class_build_replace_getclass_rtn(ClassBuild *self,ClassName *className)
{
   if(className==NULL || strcmp(className->userName,AET_ROOT_CLASS))
	   return;
   ClassInfo *aclassInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   char *package0=className->package;
   if(aclassInfo->parentName.sysName && !strcmp(aclassInfo->parentName.userName,AET_ROOT_OBJECT)){
	   ClassInfo *parent=class_mgr_get_class_info_by_class_name(class_mgr_get(),&aclassInfo->parentName);
	   char *package=parent->className.package;
	   char *mangleFuncName=NULL;
	   if((package==NULL && package0==NULL) || (package!=NULL && package0!=NULL && !strcmp(package,package0))){
		   NPtrArray  *funcs=func_mgr_get_funcs(func_mgr_get(), &aclassInfo->parentName);
		   int i;
		   for(i=0;i<funcs->len;i++){
			   ClassFunc *fun=(ClassFunc *)n_ptr_array_index(funcs,i);
               if(strcmp(fun->orgiName,"getClass")==0){
            	   if(relaceGetClassFieldRtn(fun->fieldDecl,aclassInfo)){
            		   mangleFuncName=fun->mangleFunName;
            		   break;
            	   }
               }
		   }

	      //??????  super_AObject__Z7AObject8getClassEPN7AObjectE
	      if(mangleFuncName!=NULL){
	    	//zclei super
//			tree field;
//			tree fieldList=TYPE_FIELDS(parent->record);
//			for (field = fieldList; field; field = DECL_CHAIN (field)){
//				tree name=DECL_NAME(field);
//				char newFuncName[256];
//				aet_utils_create_super_field_name(parent->className.userName,mangleFuncName,newFuncName);
//				if(strcmp(IDENTIFIER_POINTER(name),newFuncName)==0){
//					n_debug("??????getClass????????????:%s",IDENTIFIER_POINTER(name));
//					relaceGetClassFieldRtn(field,aclassInfo);
//					break;
//				}
//			}
	     }
	   }
   }
}

/**
 * ???????????????????????????????????????aet_lang_Abc_init_1234ergR5678_aet_lang_Abc????????????
 * ??????????????????????????????????????????AClass
 */
ClassBuild *class_build_new()
{
	ClassBuild *self = n_slice_alloc0 (sizeof(ClassBuild));
	classBuildBuild(self);
	return self;
}





