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

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "genericmodel.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "genericimpl.h"


static void genericModelInit(GenericModel *self)
{
   self->unitCount=0;
}


static void getExtendsClass(c_parser *parser,char **parentClass,tree *parentTree)
{
  if(c_parser_peek_token (parser)->keyword==RID_AET_EXTENDS){
	 c_parser_consume_token (parser);
	 c_token *token=c_parser_peek_token (parser);
	 if(token->id_kind!=C_ID_TYPENAME){
		 error_at (token->location, "未知的类 名字 %qE", token->value);
		 return NULL_TREE;
	 }
	 char *sysName=IDENTIFIER_POINTER(token->value);
	 ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	 if(info==NULL){
		 error_at (token->location, "未知的类 名字 %qE", sysName);
		 return;
	 }
	 c_parser_consume_token (parser);
	 *parentClass=n_strdup(sysName);
	 *parentTree=token->value;
  }
}

//GenericUnit parent;
//	char *name;
//	tree decl;
//	int size;
//	int pointerCount;
//	GenericType genericType;
//	nboolean isDecl;

/**
 * 泛型定义的tree 是PARM_DECL
 * 泛型声明的tree 是IDENTIFIER_NODE
 */
static GenericUnit *createGenUnit(tree generic,char *parentName,tree parentTree)
{
	   GenericUnit *unit=n_slice_new0(GenericUnit);
	   if(TREE_CODE(generic)!=IDENTIFIER_NODE){
		   tree type=TREE_TYPE(generic);
		   class_util_get_type_name(type,&unit->name);
		   tree cs=c_sizeof(0,type);
		   wide_int result=wi::to_wide(cs);
		   unit->size=result.to_shwi();
		   unit->pointerCount=class_util_get_pointers(type);
		   unit->genericType=generic_util_get_generic_type(type);
		   unit->isDefine=TRUE;
		   unit->decl=generic;
		   //printf("createGenUnit 00 是一个具体的类型 %s size:%d pointerCount:%d,genericType:%d\n",unit->name,unit->size,unit->pointerCount,unit->genericType);
	  }else{
		   unit->decl=generic;
		   unit->name=n_strdup(IDENTIFIER_POINTER(generic));
		   //printf("createGenUnit 00 是一个声明 %s parentName:%s\n",unit->name,parentName);
		   if(parentName!=NULL){
			   unit->parent.name=n_strdup(parentName);
			   unit->parent.decl=parentTree;
		   }
	  }
	  return unit;
}


static tree c_parser_parameter_declaration (c_parser *parser,tree *expr,char **parentClass,tree *parentTree,nboolean needIdentifierNode)
{
  struct c_declspecs *specs;
  struct c_declarator *declarator;
  tree prefix_attrs;
  tree postfix_attrs = NULL_TREE;
  bool dummy = false;

  if (!c_parser_next_token_starts_declspecs (parser) && !c_c_parser_nth_token_starts_std_attributes (parser, 1)){
      c_token *token = c_parser_peek_token (parser);
      if(token->type==CPP_QUERY){
         tree id=aet_utils_create_ident("all");
         c_parser_consume_token (parser);
         getExtendsClass(parser,parentClass,parentTree);
         n_debug("是一个问号泛型----parentClass:%s\n",*parentClass);
         return id;
      }
      if(needIdentifierNode){
    	 n_info("genericimpl 这是一个泛型类型 %s",IDENTIFIER_POINTER(token->value));
         tree id = token->value;
         if(!generic_util_valid_all(id)){
             error_at (token->location, "unknown type name %qE,如果需要泛型声明说明符：应该是A-Z。", token->value);
             return NULL_TREE;
         }
         c_parser_consume_token (parser);
         getExtendsClass(parser,parentClass,parentTree);
         return id;
      }
      error_at (token->location, "unknown type name %qE", token->value);
      return NULL_TREE;
  }
  //不能出现具体类型extends$ class.
  location_t start_loc = c_parser_peek_token (parser)->location;
  specs = build_null_declspecs ();
  c_parser_declspecs (parser, specs, true, true, true, true, false,false,true, cla_nonabstract_decl);
  finish_declspecs (specs);
  pending_xref_error ();
  prefix_attrs = specs->attrs;
  specs->attrs = NULL_TREE;
  declarator = c_parser_declarator (parser,specs->typespec_kind != ctsk_none,C_DTR_PARM, &dummy);
  if (declarator == NULL){
      c_parser_skip_until_found (parser, CPP_COMMA, NULL);
      return NULL_TREE;
  }

  location_t end_loc = parser->last_token_location;
  /* Find any cdk_id declarator; determine if we have an identifier.  */
  c_declarator *id_declarator = declarator;
  while (id_declarator && id_declarator->kind != cdk_id){
     id_declarator = id_declarator->declarator;
  }
  location_t caret_loc = (id_declarator->u.id.id ? id_declarator->id_loc : start_loc);
  location_t param_loc = make_location (caret_loc, start_loc, end_loc);
  struct c_parm *parm= build_c_parm (specs, chainon (postfix_attrs, prefix_attrs),declarator, param_loc);
  tree decl;
  decl = grokparm (parm,expr);
  return decl;
}

static int c_parser_parms_list_declarator (GenericModel *self,c_parser *parser,nboolean needIdentifierNode)
{
  tree expr=NULL;
  if (c_parser_next_token_is (parser, CPP_GREATER)){
      error_at (c_parser_peek_token (parser)->location,"泛型的具体类型缺失。");
      c_parser_consume_token (parser);
      return 0;
  }
  if (c_parser_next_token_is (parser, CPP_ELLIPSIS)){
      error_at (c_parser_peek_token (parser)->location,"泛型定义不支持'...'符号");
      c_parser_consume_token (parser);
      c_parser_skip_until_found (parser, CPP_GREATER,"expected %<)%>");
	  return 0;
  }

  int count=0;
  while (true){
      /* Parse a parameter.  */
   	  n_debug("循环分析每一个泛型参数 count:%d",count);
   	  char *parentClassName=NULL;
   	  tree parentTree=NULL_TREE;
      tree decl = c_parser_parameter_declaration (parser,&expr,&parentClassName,&parentTree,needIdentifierNode);
      if (decl == NULL_TREE){
	     c_parser_skip_until_found (parser, CPP_GREATER,"expected %<)%>");
	     return 0;
      }
      self->genUnits[count]=createGenUnit(decl,parentClassName,parentTree);
      count++;
      if(parentClassName)
    	  n_free(parentClassName);
      if (c_parser_next_token_is (parser, CPP_SEMICOLON)){
          error_at (c_parser_peek_token (parser)->location,"泛型定义中，不应有分号");
          return 0;
	  }
      if (c_parser_next_token_is (parser, CPP_GREATER)){
       	   n_info("分析完泛型了 count:%d",count);
	       c_parser_consume_token (parser);
	       return count;
	  }
      if (!c_parser_require (parser, CPP_COMMA,"expected %<;%>, %<,%> or %<)%>",UNKNOWN_LOCATION, false)){
	     c_parser_skip_until_found (parser, CPP_GREATER, NULL);
	     return 0;
	  }
      if (c_parser_next_token_is (parser, CPP_ELLIPSIS)){
          error_at (c_parser_peek_token (parser)->location,"泛型定义不支持'...'符号");
	      c_parser_consume_token (parser);
	      c_parser_skip_until_found (parser, CPP_GREATER,"expected %<)%>");
	      return 0;
	  }
  }//end while
  return 0;
}

static nboolean compareUnit(GenericUnit *unit1,GenericUnit *unit2)
{
   if(unit1->isDefine!=unit2->isDefine)
	   return FALSE;
   if(unit1->pointerCount!=unit2->pointerCount)
	   return FALSE;
   return strcmp(unit1->name,unit2->name)==0;
}

nboolean  generic_model_equal(GenericModel *self,GenericModel *comp)
{
   if(self==NULL && comp==NULL)
	   return TRUE;
   if((self!=NULL && comp==NULL) || (self==NULL && comp!=NULL))
	   return FALSE;

   if(self==comp)
		return TRUE;
   int c1=generic_model_get_count(self);
   int c2=generic_model_get_count(comp);
   if(c1!=c2)
	   return FALSE;
   int i;
   for(i=0;i<c1;i++){
	   GenericUnit *unit1=self->genUnits[i];
	   GenericUnit *unit2=comp->genUnits[i];
	   if(!compareUnit(unit1,unit2)){
		   return FALSE;
	   }

   }
   return TRUE;
}

int generic_model_get_count(GenericModel *self)
{
	if(self==NULL)
		return 0;
	return self->unitCount;
}

/**
 * self中泛型声明是否包含目标的泛型声明
 *
 */
nboolean  generic_model_include_decl(GenericModel *self,GenericModel *dest)
{
	if(self==NULL)
		return FALSE;
	int i,j;
	int destCount=generic_model_get_count(dest);
	//printf("generic_model_include_decl -- %p %p\n",self,dest);
	for(i=0;i<destCount;i++){
		GenericUnit *unit=dest->genUnits[i];
		if(!strcmp(unit->name,"all") || unit->isDefine)
			continue;
		nboolean find=FALSE;
		for(j=0;j<self->unitCount;j++){
			GenericUnit *selfUnit=self->genUnits[j];
			if(!selfUnit->isDefine && !strcmp(selfUnit->name,unit->name)){
				find=TRUE;
				break;
			}
		}
		if(!find)
			return FALSE;
	}
	return TRUE;
}

/**
 * 获取在self中没找到的GenericUtil的name
 */
char  *generic_model_get_no_include_decl_str(GenericModel *self,GenericModel *dest)
{
	if(self==NULL)
		return NULL;
	int i,j;
	int destCount=generic_model_get_count(dest);
	for(i=0;i<destCount;i++){
		GenericUnit *unit=dest->genUnits[i];
		if(!strcmp(unit->name,"all") || unit->isDefine)
			continue;
		nboolean find=FALSE;
		for(j=0;j<self->unitCount;j++){
			GenericUnit *selfUnit=self->genUnits[i];
			if(!selfUnit->isDefine && !strcmp(selfUnit->name,unit->name)){
				find=TRUE;
				break;
			}
		}
		if(!find)
			return unit->name;
	}
	return NULL;
}


/**
 * 是否包含 A-Z字符
 */
nboolean  generic_model_include_decl_by_str(GenericModel *self,char *genDeclStr)
{
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(unit->isDefine)
			continue;
		if(!strcmp(unit->name,genDeclStr))
		   return TRUE;
	}
	return FALSE;
}


int generic_model_get_undefine_count(GenericModel *self)
{
	if(!self)
		return 0;
	int i;
    int count=0;
	for(i=0;i<self->unitCount;i++){
		if(!self->genUnits[i]->isDefine)
			count++;
	}
	return count;
}

char         *generic_model_get_first_decl_name(GenericModel *self)
{
	if(self==NULL)
		return NULL;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!unit->isDefine)
			return unit->name;
	}
	return NULL;
}



void generic_model_add(GenericModel *self,char *typeName,int pointer)
{
   self->genUnits[self->unitCount++]=generic_unit_new(typeName,pointer);
}

int generic_model_fill_undefine_str(GenericModel *self,int *strs)
{
	if(self==NULL)
		return 0;
	int i;
	int count=0;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!unit->isDefine){
			strs[count]=unit->name[0];
			count++;
		}
	}
	return count;
}

int  generic_model_get_undefine_index(GenericModel *self,char genericName)
{
	if(self==NULL)
		return -1;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!unit->isDefine){
			char g=unit->name[0];
			if(g==genericName)
				return i;
		}
	}
	return -1;
}


/**
 * new$ Abc<int,char>();
 * 返回_int_char字符串
 */
char *generic_model_define_str(GenericModel *self)
{
    if(self==NULL)
		return NULL;
	NString *codes=n_string_new("");
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(unit->isDefine){
		   n_string_append_printf(codes,"_%s",unit->name);
		   if(unit->pointerCount>0){
			  n_string_append_printf(codes,"p%d",unit->pointerCount);
		   }
		}
	}
	char *re=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	return re;
}

int  generic_model_get_index(GenericModel *self,char *genericName)
{
	if(self==NULL)
		return -1;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!unit->isDefine){
			if(strcmp(unit->name,genericName)==0)
				return i;
		}
	}
	return -1;
}

static int getIndexByDefine(GenericModel *self,GenericUnit *defineUnit)
{
	if(self==NULL)
		return -1;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(generic_unit_equal(unit,defineUnit))
			return i;
	}
	return -1;
}


int   generic_model_get_index_by_unit(GenericModel *self,GenericUnit *unit)
{
	if(!unit->isDefine)
	   return  generic_model_get_index(self,unit->name);
	else
		return getIndexByDefine(self,unit);
}


GenericUnit  *generic_model_get(GenericModel *self,int index)
{
	    if(self==NULL || index<0 || index>self->unitCount-1)
			return NULL;
	    return self->genUnits[index];
}

void  generic_model_add_unit(GenericModel *self,GenericUnit *unit)
{
	   self->genUnits[self->unitCount++]=unit;
}



nboolean  generic_model_exits_ident(GenericModel *self,char *genIdent)
{
	if(!self || genIdent==NULL)
		return FALSE;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!unit->isDefine){
			if(strcmp(unit->name,genIdent)==0)
				return TRUE;
		}
	}
	return FALSE;
}

nboolean generic_model_have_query(GenericModel *self)
{
	if(!self)
		return FALSE;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!unit->isDefine){
			if(strcmp(unit->name,"all")==0)
				return TRUE;
		}
	}
	return FALSE;
}

char  *generic_model_tostring(GenericModel *self)
{
	   if(self==NULL)
			return NULL;
		NString *codes=n_string_new("");
		int i;
		for(i=0;i<self->unitCount;i++){
			GenericUnit *unit=self->genUnits[i];
			n_string_append_printf(codes,"%s_%d,",unit->name,unit->pointerCount);
		}
		sprintf(self->toString,"%s",codes->str);
		n_string_free(codes,TRUE);
		return self->toString;
}


/**
 * 1 解析 如:class$ Abc<E>形式
 * 2 解析 如 new$ Abc<int> 形式
 * 3 解析 如:extends$ AObject<int>形式
 * 4 解析 如:implements$ AObject<int>形式
 * 5 解析如 Abc<E,int> *abc; 或 Abc<T,G> abc;等
 * 6 解析类变量声明中的泛型或函数返回值中的泛型  如 Abc<E> *abc; Abc<E> *getName();
 * <T> void getData()
 * getData<int>();
 * 解析 如 abcd(Abc<E> *abc) 形式
 */
GenericModel *generic_model_new(nboolean needIdentifierNode,GenericFrom from)
{
	GenericModel *self = n_slice_alloc0 (sizeof(GenericModel));
    genericModelInit(self);
    c_parser *parser=generic_impl_get()->parser;
    gcc_assert (c_parser_next_token_is (parser, CPP_LESS));
    c_parser_consume_token (parser);
    //location_t saveLoc=input_location;
    int count=c_parser_parms_list_declarator (self,parser, needIdentifierNode);
    c_c_parser_set_source_position_from_token(c_parser_peek_token (parser));
    //input_location=saveLoc;
    self->unitCount=count;
    self->from=from;
	return self;
}

GenericModel *generic_model_new_from_file()
{
	GenericModel *self = n_slice_alloc0 (sizeof(GenericModel));
	genericModelInit(self);
	self->from=GEN_FROM_FILE;
	return self;
}

GenericModel *generic_model_new_with_parm(char *typeName,int pointerCount)
{
	GenericModel *self = n_slice_alloc0 (sizeof(GenericModel));
	genericModelInit(self);
	self->from=GEN_FROM_FILE;
	generic_model_add(self,typeName,pointerCount);
	return self;
}

/**
 * 根据self 创建默认的泛型定义
 */
GenericModel *generic_model_create_default(GenericModel *self)
{
    if(self==NULL)
		return NULL;
	GenericModel *defaultGen=generic_model_new_from_file();
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *unit=self->genUnits[i];
		if(!generic_unit_is_undefine(unit)){
			GenericUnit  *clone=generic_unit_clone(unit);
			generic_model_add_unit(defaultGen,clone);
		}else{
			if(unit->parent.name!=NULL){
			   generic_model_add(defaultGen,unit->parent.name,0);
			}else{
			   generic_model_add(defaultGen,"int",0);
			}
		}
	}
	return defaultGen;
}

static GenericModel *cloneModel(GenericModel *self)
{
	   if(self==NULL)
		   return NULL;
	   GenericModel *cloneModel = generic_model_new_from_file();
	   int i;
	   for(i=0;i<self->unitCount;i++){
		   GenericUnit *unit=self->genUnits[i];
		   GenericUnit *clone=generic_unit_clone(unit);
		   generic_model_add_unit(cloneModel,clone);
	   }
	   return cloneModel;
}

GenericModel *generic_model_clone(GenericModel *self)
{
	return cloneModel(self);
}


nboolean  generic_model_exits_unit(GenericModel *self,GenericUnit *unit)
{
	if(!self || unit==NULL)
		return FALSE;
	int i;
	for(i=0;i<self->unitCount;i++){
		GenericUnit *item=self->genUnits[i];
		if(generic_unit_equal(item,unit))
			return TRUE;
	}
	return FALSE;
}


/**
 * self与other合并成一个新的GenericModel
 * 过渡重复的声明包括问号
 */
GenericModel *generic_model_merge(GenericModel *self,GenericModel *other)
{
	if(self==NULL && other!=NULL){
		return cloneModel(other);
	}else if(self!=NULL && other==NULL){
		return cloneModel(self);
	}else if(self!=NULL && other!=NULL){
		GenericModel *cloneModel = generic_model_new_from_file();
		 int i;
	     for(i=0;i<self->unitCount;i++){
		    GenericUnit *unit=self->genUnits[i];
		    if(!generic_model_exits_unit(cloneModel,unit)){
		      GenericUnit *clone=generic_unit_clone(unit);
		      generic_model_add_unit(cloneModel,clone);
		   }
	     }
	     for(i=0;i<other->unitCount;i++){
			GenericUnit *unit=other->genUnits[i];
			if(!generic_model_exits_unit(cloneModel,unit)){
			  GenericUnit *clone=generic_unit_clone(unit);
			  generic_model_add_unit(cloneModel,clone);
		   }
		}
	    return cloneModel;
	}
	return NULL;
}



void   generic_model_free(GenericModel *self)
{


}

GenericUnit  *generic_unit_new(char *typeName,int pointer)
{
	GenericUnit *self = n_slice_alloc0 (sizeof(GenericUnit));
	if(generic_util_valid_by_str(typeName) && pointer==0){
		tree id=aet_utils_create_ident(typeName);
		self->decl=id;
		self->name=n_strdup(typeName);
		return self;
	}
	tree id=aet_utils_create_ident(typeName);
	tree type=lookup_name(id);
	if(!aet_utils_valid_tree(type)){
		type=get_typenode_from_name(typeName);
		if(!aet_utils_valid_tree(type))
		   n_error("没有找到类型:%s。",typeName);
	}
	class_util_get_type_name(type,&self->name);
	if(TREE_CODE(type)==TYPE_DECL)
		type=TREE_TYPE(type);
	tree cs=c_sizeof(0,type);
	wide_int result=wi::to_wide(cs);
	self->size=result.to_shwi();
	self->pointerCount=pointer;
	self->genericType=generic_util_get_generic_type(type);
	self->isDefine=TRUE;
	self->decl = build_decl (0,PARM_DECL,NULL_TREE, type);
	n_debug("createGenUnit 00 是一个具体的类型 %s size:%d pointerCount:%d,genericType:%d\n",self->name,self->size,self->pointerCount,self->genericType);
	return self;
}

GenericUnit  *generic_unit_new_undefine(char *idName)
{
	GenericUnit *self = n_slice_alloc0 (sizeof(GenericUnit));
	tree id=aet_utils_create_ident(idName);
	self->decl=id;
	self->name=n_strdup(idName);
	return self;

}


nboolean      generic_unit_equal(GenericUnit *self,GenericUnit *dest)
{
	return compareUnit(self,dest);
}

nboolean      generic_unit_is_undefine(GenericUnit *self)
{
	return generic_util_valid_all_by_str(self->name);
}

nboolean  generic_unit_is_query(GenericUnit *self)
{
     return strcmp(self->name,"all")==0;
}

GenericUnit  *generic_unit_clone(GenericUnit *unit)
{
   GenericUnit *dest=n_slice_new0(GenericUnit);
   if(unit->parent.name){
	   printf("generic_unit_clone 11 unit->parent.name %s\n",unit->parent.name);
	   dest->parent.name=n_strdup(unit->parent.name);
   }
   dest->parent.decl=unit->parent.decl;
   dest->name=n_strdup(unit->name);
   dest->size=unit->size;
   dest->pointerCount=unit->pointerCount;
   dest->genericType=unit->genericType;
   dest->decl=unit->decl;
   dest->isDefine=unit->isDefine;
   return dest;
}

char   *generic_unit_tostring(GenericUnit *self)
{
   NString *str=NULL;
   if(generic_unit_is_query(self))
     str=n_string_new("?");
   else
	 str=n_string_new(self->name);
   int i;
   for(i=0;i<self->pointerCount;i++){
	   if(i==0)
		   n_string_append(str," ");
	   n_string_append(str,"*");
   }
   sprintf(self->toString,"%s",str->str);
   n_string_free(str,TRUE);
   return self->toString;
}

/**
 * 是否可以转化到dest
 */
nboolean generic_model_can_cast(GenericModel *self,GenericModel *dest)
{
    if(self->unitCount!=dest->unitCount)
        return FALSE;
    int i;
    for(i=0;i<self->unitCount;i++){
        GenericUnit *unit1=self->genUnits[i];
        GenericUnit *unit2=dest->genUnits[i];
    }
    return TRUE;
}







