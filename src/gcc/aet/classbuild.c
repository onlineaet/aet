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
#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "classmgr.h"
#include "classbuild.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "classbuild.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "varmgr.h"

static void classBuildBuild(ClassBuild *self)
{
}

static void createVar(char *className,char *result)
{
   char temp[255];
   sprintf(temp,"aclass%s",className);
   unsigned int hash=aet_utils_create_hash(temp,strlen(temp));
   sprintf(result,"_%s%u",temp,hash);
}

/**
 * 创建生成aclass的函数名，如:
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
       obj->name=strdup("ARandom");//这里是具体内容
       obj->packageName=strdup("com.dnn");
      _aclassdebug_ARandom3488912659=obj;
       return _aclassdebug_ARandom3488912659;
    }
 */

/**
 * 生成类或接口的静态get_class代码
 */
char *class_build_create_get_aclass_codes(ClassBuild *self,ClassName *className)
{
   ClassName *oName=class_mgr_get_class_name_by_user(class_mgr_get(), AET_ROOT_OBJECT);
   ClassName *cName=class_mgr_get_class_name_by_user(class_mgr_get(), AET_ROOT_CLASS);
   if(strcmp(oName->package,cName->package)){
       n_error("%s与%s的包名，应该是一样的。%s %s",AET_ROOT_OBJECT,AET_ROOT_CLASS,oName->package,cName->package);
       return NULL;
   }

   ClassFunc * ret = func_mgr_get_static_get_class(func_mgr_get(),className);

   char *funcName = ret->mangleFunName;
   char *varName="aclassIntance";
   NString *codes=n_string_sized_new(512);

   ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   nboolean isMtcsClass =class_mgr_is_mtcs_class(class_mgr_get(),className);//是不是mtcs类
   char *varInitCodes=class_info_create_class_code(classInfo);
   char *initMethod=aet_utils_create_init_method(cName->sysName);
   n_string_append_printf(codes,"%s *%s(%s *self)\n",cName->sysName,funcName,className->sysName);
   n_string_append(codes,       "{\n");
   n_string_append_printf(codes,"\tstatic AClass *%s=NULL;\n",varName);
   n_string_append_printf(codes,"\tif(%s!=NULL){\n",varName);
   n_string_append_printf(codes,"\t\treturn %s;\n",varName);
   n_string_append(codes,       "\t}\n");
   n_string_append_printf(codes,"\t%s *obj=({ %s *temp;\n",cName->sysName,cName->sysName);
   n_string_append_printf(codes,"\t\t temp=%s.newObject(sizeof(%s),0,0,\"%s\");\n",
         cName->sysName,cName->sysName,className->sysName);
   n_string_append_printf(codes,"\t\t temp->objectSize=sizeof(%s);\n",cName->sysName);
   n_string_append_printf(codes,"\t\t %s(temp);\n",initMethod);//调用初始化方法
   n_string_append(codes,"\t\t temp->AClass();\n");
   n_string_append(codes,"\t\t temp;});\n");
   n_string_append(codes,varInitCodes);
   n_string_append_printf(codes,"\tobj->isMtcs=%d;\n",isMtcsClass);//设是不是mtcs类
   n_string_append_printf(codes,"\tobj->size=sizeof(%s);\n",className->sysName);//设是不是mtcs类
   n_string_append_printf(codes,"\tobj->%s=%d;\n",AET_MAGIC_NAME,AET_MAGIC_NAME_VALUE);//设魔数
   n_string_append_printf(codes,"\t%s=obj;\n",varName);
   n_string_append_printf(codes,"\treturn %s;\n",varName);
   n_string_append(codes,"}\n");
   return n_string_free(codes,FALSE);
}

/**
 * Abc.class转成调用get_class方法
 * 创建一个调用类或接口的获取AClass的调用
 * debug_AClass *_Z6TFirst9get_classEPN6TFirstE(TFirst *self)
 */
tree class_build_build_get_class_call(ClassBuild *self,location_t loc,ClassName *className)
{
   ClassFunc *fun = func_mgr_get_static_get_class(func_mgr_get(),className);
   tree selfTree = null_pointer_node;
   vec<tree, va_gc> *parms;
   parms = make_tree_vector ();
   vec_safe_push(parms,selfTree);

   tree newCallExpr = c_build_function_call_vec (loc, vNULL, fun->fieldDecl,parms, NULL);
   release_tree_vector (parms);
   return newCallExpr;
}

/**
 * 接口类也会有一个初始化方法aet_lang_Abc_init_1234ergR5678_aet_lang_Abc，该方法
 * 只有一个功能，就是返回接口的AClass
 */
ClassBuild *class_build_new()
{
	ClassBuild *self = n_slice_alloc0 (sizeof(ClassBuild));
	classBuildBuild(self);
	self->parser = aet_parser_get();
	return self;
}





