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
#include "toplev.h"
#include "c-aet.h"
#include "genericmodel.h"

typedef struct _ObjectNewInfo
{
	  char createMethod;
	  char modify;
	  tree ctor;
	  tree sysClassName;
	  GenericModel *funcGenericsModel;
	  GenericModel *genericsModel;
	  location_t loc;//构造函数的位置
}ObjectNewInfo;

typedef struct _TreeLangDecl
{
	 tree dest;
	 ObjectNewInfo info;
}TreeLangDecl;

static NPtrArray *treeLangDeclArray=NULL;

static TreeLangDecl *getTreeLangDecl(tree dest)
{
	if(treeLangDeclArray==NULL)
		treeLangDeclArray=n_ptr_array_new();
	int i;
	for(i=0;i<treeLangDeclArray->len;i++){
		TreeLangDecl *langDecl=(TreeLangDecl *)n_ptr_array_index(treeLangDeclArray,i);
		if(langDecl->dest==dest)
			return langDecl;
	}
	return NULL;
}

static TreeLangDecl *createTreeLangDecl(tree dest)
{
	TreeLangDecl *lang=(TreeLangDecl *)n_slice_new(TreeLangDecl);
	lang->dest=dest;
	memset(&(lang->info),0,sizeof(ObjectNewInfo));
	lang->info.createMethod=CREATE_CLASS_METHOD_UNKNOWN;
	return lang;
}

static TreeLangDecl *getTreeLangDeclNeedCreate(tree dest)
{
	    if(treeLangDeclArray==NULL)
	    	treeLangDeclArray=n_ptr_array_new();
	    TreeLangDecl *lang=getTreeLangDecl(dest);
		if(lang==NULL){
			lang=createTreeLangDecl(dest);
			n_ptr_array_add(treeLangDeclArray,lang);
		}
		return lang;
}

/**
 * loc是构造函数的位置
 */
void c_aet_set_ctor(tree dest,tree ctor,tree sysClassName,location_t loc)
{
	 TreeLangDecl *lang=getTreeLangDeclNeedCreate(dest);
	 lang->info.ctor=ctor;
	 lang->info.sysClassName=sysClassName;
	 lang->info.loc=loc;
}

tree c_aet_get_ctor_codes(tree dest)
{
	 TreeLangDecl *lang=getTreeLangDecl(dest);
	 if(lang==NULL)
	    return NULL_TREE;
	 return lang->info.ctor;
}

location_t c_aet_get_ctor_location(tree dest)
{
     TreeLangDecl *lang=getTreeLangDecl(dest);
     if(lang==NULL)
        return 0;
     return lang->info.loc;
}

tree c_aet_get_ctor_sys_class_name(tree dest)
{
	 TreeLangDecl *lang=getTreeLangDecl(dest);
	 if(lang==NULL)
	    return NULL_TREE;
	 return lang->info.sysClassName;
}


void c_aet_set_create_method(tree dest,CreateClassMethod method)
{
	 TreeLangDecl *lang=getTreeLangDeclNeedCreate(dest);
	 lang->info.createMethod=(char)method;
}

int c_aet_get_create_method(tree dest)
{
	 TreeLangDecl *lang=getTreeLangDecl(dest);
	 if(lang==NULL)
	    return CREATE_CLASS_METHOD_UNKNOWN;
	 return lang->info.createMethod;
}

void c_aet_set_modify_stack_new(tree dest,int end)
{
	 TreeLangDecl *lang=getTreeLangDeclNeedCreate(dest);
	 lang->info.modify=end;
}

int c_aet_get_is_modify_stack_new(tree dest)
{
	 if(!dest || dest==NULL_TREE || dest==error_mark_node)
		return 0;
	 if(!VAR_P(dest) && TREE_CODE(dest)!=COMPONENT_REF)
		return 0;
	 tree type=TREE_TYPE(dest);
	 if(TREE_CODE(type)!=RECORD_TYPE && TREE_CODE(type)!=POINTER_TYPE)
		return 0;
	 if(TREE_CODE(type)==POINTER_TYPE){
		type=TREE_TYPE(type);
		if(TREE_CODE(type)!=RECORD_TYPE)
		  return 0;
	 }
	 TreeLangDecl *lang=getTreeLangDecl(dest);
	 if(lang==NULL)
	    return 0;
	 return lang->info.modify;
}

void c_aet_copy_lang(tree dest,tree src)
{
	 TreeLangDecl *srcLang=getTreeLangDecl(src);
	 if(srcLang){
		 TreeLangDecl *destLang=getTreeLangDeclNeedCreate(dest);
		 destLang->info.createMethod=srcLang->info.createMethod;
		 destLang->info.modify=srcLang->info.modify;
		 destLang->info.ctor=srcLang->info.ctor;
		 destLang->info.sysClassName=srcLang->info.sysClassName;
		 destLang->info.genericsModel=srcLang->info.genericsModel;
		 destLang->info.funcGenericsModel=srcLang->info.funcGenericsModel;
     }
}

 void c_aet_set_decl_method(tree dest,int declMethod)
{
	 c_aet_set_create_method(dest,declMethod);
}

 ///////////////////////////////genericmodel------
 void * c_aet_get_func_generics_model(tree dest)
 {
	     TreeLangDecl *lang=getTreeLangDecl(dest);
		 if(lang==NULL)
		    return NULL;
		 return ((void *)lang->info.funcGenericsModel);
 }

 void *c_aet_get_generics_model(tree dest)
 {
	 TreeLangDecl *lang=getTreeLangDecl(dest);
	 if(lang==NULL)
	    return NULL_TREE;
	 return (void *)lang->info.genericsModel;
 }

 void c_aet_set_generics_model(tree dest,void *model)
 {
 	 TreeLangDecl *lang=getTreeLangDeclNeedCreate(dest);
 	 lang->info.genericsModel=model;
 }

 void c_aet_set_func_generics_model(tree dest,void *model)
 {
	 TreeLangDecl *lang=getTreeLangDeclNeedCreate(dest);
 	 lang->info.funcGenericsModel=model;
 }







