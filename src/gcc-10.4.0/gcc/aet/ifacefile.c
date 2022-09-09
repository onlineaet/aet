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
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "opts.h"
#include "toplev.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "c-aet.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "classparser.h"
#include "blockmgr.h"
#include "classimpl.h"
#include "makefileparm.h"
#include "ifacefile.h"
#include "middlefile.h"
#include "genericexpand.h"


static void ifaceFileInit(IfaceFile *self)
{
	   self->xmlFile=NULL;
	   self->recordCallIface=n_ptr_array_new();
}


static void initXmlFile(IfaceFile *self)
{
   if(self->xmlFile==NULL){
	   char *objectPath=makefile_parm_get_object_root_path(makefile_parm_get());
	   char fileName[255];
	   sprintf(fileName,"%s/%s",objectPath,IFACE_STATIC_VAR_FILE_NAME);
	   self->xmlFile=xml_file_new(fileName);
	   //说明可能要编译两次，把当前文件从makefile传来的参数也保存在与.o同目录的位置
	   makefile_parm_write_compile_argv(makefile_parm_get());
   }
}

static nboolean exists(IfaceFile *self,char *sysName)
{
	int i;
	for(i=0;i<self->recordCallIface->len;i++){
		char *item=(char *)n_ptr_array_index(self->recordCallIface,i);
		if(strcmp(item,sysName)==0)
			return TRUE;
	}
	return FALSE;
}

/**
 * 当分析到Abc.class时，记录是不是接口调用
 * 或当分析到调用静态变量时，记录是不是接口调用
 */
void iface_file_record(IfaceFile *self,ClassName *className)
{
	if(className==NULL)
		return;
	nboolean second= makefile_parm_is_second_compile(makefile_parm_get());
    if(second){
    	//到文件尾要做的事情
    	generic_expand_add_eof_tag_for_iface(generic_expand_get());
    	return;
    }
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(class_info_is_interface(info)){
		if(!exists(self,className->sysName)){
			n_ptr_array_add(self->recordCallIface,n_strdup(className->sysName));
			makefile_parm_write_compile_argv(makefile_parm_get());//为第二次编译作准备
		}
	}
}

void iface_file_record_by_sys_name(IfaceFile *self,char *sysName)
{
    if(sysName==NULL)
        return;
    nboolean second= makefile_parm_is_second_compile(makefile_parm_get());
    if(second){
        //到文件尾要做的事情
        generic_expand_add_eof_tag_for_iface(generic_expand_get());
        return;
    }
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    if(class_info_is_interface(info)){
        if(!exists(self,sysName)){
            n_ptr_array_add(self->recordCallIface,n_strdup(sysName));
            makefile_parm_write_compile_argv(makefile_parm_get());//为第二次编译作准备
        }
    }
}

static int fileExists (const char *name)
{
  return access (name, R_OK) == 0;
}

void  iface_file_save(IfaceFile *self)
{
	nboolean exists=fileExists(COMPILE_TRACK_FILE_NAME);//文件aet_object_path.tmp存在，说明有aet相关的源代码
	char *cFile=in_fnames[0];
	if(!exists){
		printf("不是AET相关的编译----- %s\n",cFile);
		return;
	}
	if(makefile_parm_is_second_compile(makefile_parm_get())){
		printf("是第二次编译 %s 不需要写入任何接口信息。",cFile);
		return;
	}

	initXmlFile(self);
    if(self->recordCallIface->len==0){
	   //移走所有调用接口的地方
    	nboolean re=xml_file_remove_iface(self->xmlFile,cFile);
    	if(re){
           middle_file_modify(middle_file_get());//改变ADDITIONAL_MIDDLE_AET_FILE文件，在collect2.c判断时间，可以编译
    	}
    }else{
	   //更新接口调用的地方
    	char  *oFile=makefile_parm_get_object_file(makefile_parm_get());
    	xml_file_add_iface(self->xmlFile,self->recordCallIface,cFile,oFile);
    	middle_file_modify(middle_file_get());//改变ADDITIONAL_MIDDLE_AET_FILE文件，在collect2.c判断时间，可以编译
    }
}


NPtrArray  *iface_file_read(IfaceFile *self)
{
   initXmlFile(self);
   return xml_file_get_iface_data(self->xmlFile);
}

void iface_file_write(IfaceFile *self,NPtrArray *ifaceSaveDataArray)
{
   initXmlFile(self);
   return xml_file_write_back_iface_data(self->xmlFile,ifaceSaveDataArray);
}

////////////////////////////////////////定义接口变量------------------------------------



static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

/**
 * 定义全局变量，声明extern 是在引入头文件时就做了
 * gotoStaticAndInit-->parser_static_compile
 */
static void defineGlobalVar(VarEntity *item)
{
	  tree decl;
	  tree org=item->decl;
	  tree init=item->init;
	  tree initOrginalTypes=item->init_original_type;
      tree name=DECL_NAME(org);
      tree type=TREE_TYPE(org);
      if(item->isConst){
    	  printf("ifacefile.c defineGlobalVar is const :%s\n",item->orgiName);
    	  type = c_build_qualified_type (type, TYPE_QUAL_CONST);
      }
      decl = build_decl (DECL_SOURCE_LOCATION (org), VAR_DECL, name, type);
      DECL_EXTERNAL(decl)=0;
      TREE_PUBLIC(decl)=1;
      TREE_STATIC(decl)=1;
      decl=class_util_define_var_decl(decl,TRUE);
	  printf("ifacefile.c defineGlobalVar is -----11  :%s %p\n",item->orgiName,init);
      location_t init_loc=DECL_SOURCE_LOCATION (org);
      warn_string_init (init_loc, TREE_TYPE (decl), init,item->original_code);
	  printf("ifacefile.c defineGlobalVar is -----22  :%s %p\n",item->orgiName,init);
	  if(!init)
	      finish_decl (decl, init_loc, NULL_TREE,NULL_TREE, NULL_TREE);
	  else
          finish_decl (decl, init_loc, init,initOrginalTypes, NULL_TREE);
      n_debug("类中静态变量全局定义: %s %s context:%p extern:%d static:%d pub:%d\n",
    		  item->mangleVarName,item->orgiName,DECL_CONTEXT(decl),DECL_EXTERNAL(decl),TREE_STATIC(decl),TREE_PUBLIC(decl));
}



static nboolean havedDefineStaticVar(VarEntity *item,char *sysName)
{
    char *varName=item->mangleVarName;
	tree id=aet_utils_create_ident(varName);
	tree decl=lookup_name(id);
	if(aet_utils_valid_tree(decl)){
	   n_debug("在接口%s，找到静态变量:%s orgiName:%s\n",sysName,varName,item->orgiName);
       n_debug("在接口%s 静态变量:%s orgiNam:%s context:%p extern:%d static:%d pub:%d\n",
    		   sysName,varName,item->orgiName,DECL_CONTEXT(decl),DECL_EXTERNAL(decl),TREE_STATIC(decl),TREE_PUBLIC(decl));
       if(DECL_EXTERNAL(decl)==1 && TREE_STATIC(decl)==0 && TREE_PUBLIC(decl)==1){
    	   return FALSE;
       }
       return TRUE;
	}else{
		n_error("在接口%s，没有找到静态变量:%s orgiName:%s\n",sysName,varName,item->orgiName);
	}
	return FALSE;
}

/**
 * 真正定义接口在的静态变量在当前.c文件
 */
static void defineIfaceStatic(NPtrArray *array,char *sysName)
{
	int i;
	for(i=0;i<array->len;i++){
		VarEntity *entity=n_ptr_array_index(array,i);
		if(entity->isStatic){
			nboolean have=havedDefineStaticVar(entity,sysName);
			if(!have){
				defineGlobalVar(entity);
			}
		}
	}
}

static void defineStaticVar(IfaceFile *self,char *sysName)
{
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	NPtrArray *array=var_mgr_get_vars(var_mgr_get(),sysName);
	printf("定义接口中的静态变量为全局变量并初始化 %s 当前文件:%s\n",sysName,in_fnames[0]);
	defineIfaceStatic(array,sysName);
}

void iface_file_impl_iface(IfaceFile *self)
{
	NPtrArray  *ifaceSaveDataArray=iface_file_read(self);
    int i;
    char *file=in_fnames[0];
    NString *codes=n_string_new("");
    ClassParser *classParser=class_parser_get();
    printf("iface_file_impl_iface ---%s\n",file);
    for(i=0;i<ifaceSaveDataArray->len;i++){
    	IFaceSaveData *item=(IFaceSaveData *)n_ptr_array_index(ifaceSaveDataArray,i);
    	if(strcmp(item->cFile,file)==0){
          //我要实现这个接口
    		defineStaticVar(self,item->sysName);
    		ClassName *ifaceName=class_mgr_clone_class_name(class_mgr_get(),item->sysName);
    		class_build_create_codes(classParser->classBuild,ifaceName,codes);
    		class_init_create_init_define_for_interface(classParser->classInit,item->sysName,codes);
    	}
    }
    if(codes->len>0){
    	printf("实现接口的初始化方法 ---实现的文件:%s\n源代码:\n%s\n",file,codes->str);
    	aet_utils_add_token(parse_in,codes->str,codes->len);
    }

//    char *test= "static AClass *_createAClass_test_NArrayCallback_123(test_NArrayCallback *self)\n\
//    {\n\
//            AClass *obj=new$ AClass();\n\
//         return obj;\n\
//           }\n";
//
//    char *test3= "static AClass *_createAClass_test_NArrayCallback_123(test_NArrayCallback *self)\n\
//    {\n\
//        static AClass *_aclasstest_NArrayCallback1623947410=NULL;\n\
//        if(_aclasstest_NArrayCallback1623947410!=NULL){\n\
//            return _aclasstest_NArrayCallback1623947410;\n\
//    }\n\
//            AClass *obj=new$ AClass();\n\
//         return _aclasstest_NArrayCallback1623947410;\n\
//           }\n";
//
//    char *test2= "static AClass *_createAClass_test_NArrayCallback_123(test_NArrayCallback *self)\n\
//    {\n\
//        static AClass *_aclasstest_NArrayCallback1623947410=NULL;\n\
//        if(_aclasstest_NArrayCallback1623947410!=NULL){\n\
//            return _aclasstest_NArrayCallback1623947410;\n\
//    }\n\
//               AClass *obj=new$ AClass();\n\
//        obj->name=strdup(\"NArrayCallback\");\n\
//        _aclasstest_NArrayCallback1623947410=obj;\n\
//         return _aclasstest_NArrayCallback1623947410;\n\
//           }\n";
//
//    char *test1="static  AClass *_createAClass_test_NArrayCallback_123(test_NArrayCallback *self)\n\
//    {\n\
//      return NULL;\n\
//}\n";
//
////               AClass *obj=new$ AClass();
////        obj->name=strdup("NArrayCallback");
////        _aclasstest_NArrayCallback1623947410=obj;
////         return _aclasstest_NArrayCallback1623947410;
////           }
//    //char *test="int abcuy=12;\n";
//	aet_utils_add_token(parse_in,test,strlen(test));


}


IfaceFile *iface_file_get()
{
	static IfaceFile *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(IfaceFile));
		 ifaceFileInit(singleton);
	}
	return singleton;
}


