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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "c-aet.h"
#include "classparser.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "enumparser.h"
#include "accesscontrols.h"
#include "classimpl.h"
#include "classparser.h"

/**
 * 一、 public$ enum AeEnums{
        cysaa,
	 };

  二、public$ enum AeEnums{
        cysaa,
	 };

  三、public$ static void   testEnum(AObject.AeEnums aeeevv);
  四、public$ static void   testEnum(AeEnums aeeevv);
  五、static AeEnums ttxdy=cysaa;

 */
static void freeClass_cb(npointer userData)
{
  printf("enumParser free free %p\n",userData);
}

static void enumParserInit(EnumParser *self)
{
	self->hashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, freeClass_cb);
}


static EnumData *getEnumDataByEnumName(EnumParser *self,char *sysName,char *enumName)
{
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
	if(array==NULL)
		return NULL;
    int i;
    NPtrArray* data=n_ptr_array_new();
    for(i=0;i<array->len;i++){
    	EnumData *item=(EnumData *)n_ptr_array_index(array,i);
    	//printf("getEnumDataByEnumName --- %s %s %s\n",item->origName,item->typedefName,item->enumName);
		if(strcmp(item->enumName,enumName)==0){
		   return item;
		}
    }
    return NULL;
}

/**
 * 通过混淆的枚举名，找EnumData
 */
EnumData *enum_parser_get_by_enum_name(EnumParser *self,char *sysName,char *enumName)
{
	return getEnumDataByEnumName(self,sysName,enumName);
}


/**
 * 声明枚举
 * enum __xxxx{
 * };
 * typedef __xxx _xxx;
 */
void enum_parser_create_decl(EnumParser *self,location_t loc,ClassName *className,struct c_declspecs *specs,ClassPermissionType permission)
{
	tree nameTree=TYPE_NAME(specs->type);
	if(nameTree==NULL_TREE){
		if(className!=NULL)
		   error_at(loc,"在类%qs中定义的枚举，没有名字。",className->sysName);
		else
		   error_at(loc,"在文件中定义的枚举，没有名字。");
		return;
	}

	tree type=specs->type;
	char *enumName=IDENTIFIER_POINTER(nameTree);
	char *sysName=className?className->sysName:"";
	EnumData *data=getEnumDataByEnumName(self,sysName,enumName);
	if(data==NULL){
		n_error("EnumData=NULL,不应该出现的错误。");
	}
    char *typedefName=enumName+1;//跳过第一个下划线
    data->typedefName=n_strdup(typedefName);
    tree  idx=aet_utils_create_ident(typedefName);
    tree decl = build_decl (loc,TYPE_DECL,idx, type);
    DECL_ARTIFICIAL (decl) = 1;
    DECL_CONTEXT(decl)=NULL_TREE;
    DECL_EXTERNAL(decl)=0;
    TREE_STATIC(decl)=0;
    TREE_PUBLIC(decl)=0;
    set_underlying_type (decl);
    c_c_decl_bind_file_scope(decl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
    finish_decl (decl, loc, NULL_TREE,NULL_TREE, NULL_TREE);
	data->permission=permission;
	data->typeDecl=decl;
}

static EnumData *getEnumDataByName(EnumParser *self,char *sysName,char *name,nboolean orig)
{
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
	if(array==NULL)
		return NULL;
    int i;
    NPtrArray* data=n_ptr_array_new();
    for(i=0;i<array->len;i++){
    	EnumData *item=(EnumData *)n_ptr_array_index(array,i);
    	n_debug("getEnumDataByName --- %s %s %s orig:%d\n",name,item->origName,item->typedefName,orig);
    	if(orig){
      	   if(strcmp(item->origName,name)==0){
      		  return item;
      	   }
    	}else{
		   if(strcmp(item->typedefName,name)==0){
			   return item;
		   }
    	}
    }
    return NULL;
}

static int getIndex(EnumData **datas,int len,ClassName *className)
{
    if(className==NULL)
        return -1;
    int i;
    for(i=0;i<len;i++){
       EnumData *item=datas[i];
       if(!strcmp(item->sysName,className->sysName)){
           printf("ixxx iss %d :%s\n",i,className->sysName);
          return i;
       }
    }
    return -1;
}

static int getIndexByShip(EnumData **datas,int len,ClassName *className)
{
    if(className==NULL)
        return -1;
    int i;
    for(i=0;i<len;i++){
       EnumData *item=datas[i];
       ClassRelationship ship=class_mgr_relationship(class_mgr_get(),className->sysName,item->sysName);
       if(ship==CLASS_RELATIONSHIP_PARENT || ship==CLASS_RELATIONSHIP_IMPL){
            if(item->permission!=CLASS_PERMISSION_PRIVATE)
                return i;
       }
    }
    return -1;
}

/**
 * 在本类中找
 */
static int selectType(EnumParser *self,EnumData **datas,int len)
{
    c_parser *parser=self->parser;
    if(class_parser_is_parsering(class_parser_get())){
        ClassName  *className=class_parser_get_class_name(class_parser_get());
        int index=getIndex(datas,len,className);
        if(index>=0)
            return index;
    }

    if(parser->isAet){
        ClassName  *className=class_impl_get_class_name(class_impl_get());
        int index=getIndex(datas,len,className);
        if(index>=0)
          return index;
    }

    if(class_parser_is_parsering(class_parser_get())){
        ClassName  *className=class_parser_get_class_name(class_parser_get());
        int index=getIndexByShip(datas,len,className);
        if(index>=0)
           return index;
    }

    if(parser->isAet){
        ClassName  *className=class_impl_get_class_name(class_impl_get());
        int index=getIndexByShip(datas,len,className);
        if(index>=0)
           return index;
    }

    int i;
    for(i=0;i<len;i++){
        printf("data[i] %d %s\n",i,datas[i]->sysName);
        EnumData *item=datas[i];
        if(item->sysName==NULL || strlen(item->sysName)==0){
            return i;
        }
    }

    for(i=0;i<len;i++){
        printf("data[i] %d %s\n",i,datas[i]->sysName);
        EnumData *item=datas[i];
        if(item->sysName!=NULL && strlen(item->sysName)>0 && item->permission==CLASS_PERMISSION_PUBLIC){
            return i;
        }
    }

    return -1;
}

static nboolean validAccess(EnumParser *self,char *sysName)
{
    c_parser *parser=self->parser;
    printf("validAccess is :%s\n",sysName);
    if(class_parser_is_parsering(class_parser_get())){
        ClassName  *className=class_parser_get_class_name(class_parser_get());
        if(className && !strcmp(sysName,className->sysName)){
             return TRUE;
        }
    }

    if(parser->isAet){
        ClassName  *className=class_impl_get_class_name(class_impl_get());
        if(className && !strcmp(sysName,className->sysName)){
            return TRUE;
        }
    }

    if(sysName==NULL || strlen(sysName)==0)
        return TRUE;
    return FALSE;
}


/**
 * 由classimpl.c调用
 * 当参数是枚举类型时，当前名字替换成
 * 如果who是枚举，只能是两种情况。
 * who是全局的。who是在classparsering 和 isaet中的类的枚举
 * 只有这两种情况允许不用类.(AObject.Enum)此种方式访问。
 * 如果AObject.Enum这样的访问，不会进到这里。
 */
nboolean  enum_parser_set_enum_type(EnumParser *self,c_token *who)
{
	c_parser *parser=self->parser;
	tree orig=who->value;
	char *id=IDENTIFIER_POINTER(orig);
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->hashTable);
	int count=0;
	EnumData *datas[30];
    location_t loc=who->location;
	//printf("enum_parser_set_enum_type --id:%s\n",id);
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		EnumData *item=getEnumDataByName(self,sysName,id,TRUE);
		if(item!=NULL){
			datas[count++]=item;
		}
	}
	if(count==1){
	      EnumData *item=datas[0];
	      tree newValue=aet_utils_create_ident(item->typedefName);
	      who->value=newValue;
	      return TRUE;
	}else if(count>1){
	    int index=selectType(self,datas,count);
	    if(index>=0){
            EnumData *item=datas[index];
            tree newValue=aet_utils_create_ident(item->typedefName);
            who->value=newValue;
            printf("有多个枚举类型:。把token的名字改了-xxx---id:%s typedefName:%s file:%s item:%p index:%d\n",id,item->typedefName,in_fnames[0],item,index);
            return TRUE;
	    }else{
	        error_at(loc,"访问枚举:%qs。要加上类名。",id);
	        return FALSE;
	    }
	}
	return FALSE;
}

/**
 * 根据mangle的类型名，获取原来的枚举类型名
 */
char *enum_parser_get_orig_name(EnumParser *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->hashTable);
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		EnumData *item=getEnumDataByName(self,sysName,mangle,FALSE);
		if(item!=NULL){
			return item->origName;
		}
	}
	return NULL;
}

EnumData *enum_parser_get_enum(EnumParser *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->hashTable);
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		EnumData *item=getEnumDataByName(self,sysName,mangle,FALSE);
		if(item!=NULL){
			return item;
		}
	}
	return NULL;
}

/**
 * 创建类型名加一个下划线
 */
static tree createEnumName(tree ident,char *sysName)
{
	  char *enumName=IDENTIFIER_POINTER(ident);
	  char *enumTypeName=aet_utils_create_enum_type_name(sysName,enumName);
	  char _name[128];
	  sprintf(_name,"_%s",enumTypeName);
	  tree newIdent=aet_utils_create_ident(_name);
	  n_free(enumTypeName);
	  return newIdent;
}

/**
 * 创建枚举元素名
 */
static tree createElementName(tree ident,char *sysName)
{
	  char *origName=IDENTIFIER_POINTER(ident);
      char *newElement=aet_utils_create_enum_element_name(sysName,origName);
	  tree newIdent=aet_utils_create_ident(newElement);
	  n_free(newElement);
	  return newIdent;
}

static void addElement(EnumData *enumData,char *sysName,char *origName,tree value)
{
    char *newElement=aet_utils_create_enum_element_name(sysName,origName);
    EnumElement *elem=(EnumElement *)n_slice_new0(EnumElement);
    elem->sysName=n_strdup(sysName);
    elem->origName=n_strdup(origName);
    elem->mangleName=n_strdup(newElement);
    elem->value=value;
    enumData->elements[enumData->elementCount++]=elem;
    n_free(newElement);
}

static EnumData *createBaseData(char *sysName,char *origName,char *enumName)
{
	EnumData *item=(EnumData *)n_slice_new0(EnumData);
	item->sysName=n_strdup(sysName);
	item->origName=n_strdup(origName);
	item->permission=0;
	item->enumName=n_strdup(enumName);
	item->typeDecl=NULL_TREE;
	return item;
}

/**
 * 解析枚举并把类型名和元素名改为新的形式
 */
static struct c_typespec c_parser_enum_specifier (EnumParser *self,ClassName *className,EnumData **eData)
{
  c_parser *parser=self->parser;
  struct c_typespec ret;
  bool have_std_attrs;
  tree std_attrs = NULL_TREE;
  tree attrs;
  tree ident = NULL_TREE;
  location_t enum_loc;
  location_t ident_loc = UNKNOWN_LOCATION;  /* Quiet warning.  */
  gcc_assert (c_parser_next_token_is_keyword (parser, RID_AET_ENUM));
  c_parser_consume_token (parser);//consume enum$
  aet_print_token(c_parser_peek_token (parser));
  have_std_attrs = c_c_parser_nth_token_starts_std_attributes (parser, 1);
  if (have_std_attrs)
    std_attrs = c_c_parser_std_attribute_specifier_sequence (parser);
  attrs = c_c_parser_gnu_attributes (parser);
  enum_loc = c_parser_peek_token (parser)->location;
  /* Set the location in case we create a decl now.  */
  c_c_parser_set_source_position_from_token (c_parser_peek_token (parser));
  if (c_parser_next_token_is (parser, CPP_NAME)){
      ident = c_parser_peek_token (parser)->value;
      ident_loc = c_parser_peek_token (parser)->location;
      enum_loc = ident_loc;
      c_parser_consume_token (parser);
  }
  if(ident==NULL){
      c_parser_error (parser, "没有枚举类型名。expected %<{%>");
      return ret;
  }
  char *sysName=className?className->sysName:"";
  char *origName=n_strdup(IDENTIFIER_POINTER(ident));
  ident=createEnumName(ident,sysName);
  char *enumName=IDENTIFIER_POINTER(ident);
  EnumData *enumData=createBaseData(sysName,origName,enumName);
  n_free(origName);
  *eData=enumData;
  if (c_parser_next_token_is (parser, CPP_OPEN_BRACE)){
      /* Parse an enum definition.  */
      struct c_enum_contents the_enum;
      tree type;
      tree postfix_attrs;
      /* We chain the enumerators in reverse order, then put them in
	 forward order at the end.  */
      tree values;
      timevar_push (TV_PARSE_ENUM);
      type = start_enum (enum_loc, &the_enum, ident);
      values = NULL_TREE;
      c_parser_consume_token (parser);
      while (true){
		  tree enum_id;
		  tree enum_value;
		  tree enum_decl;
		  bool seen_comma;
		  c_token *token;
		  location_t comma_loc = UNKNOWN_LOCATION;  /* Quiet warning.  */
		  location_t decl_loc, value_loc;
		  if (c_parser_next_token_is_not (parser, CPP_NAME)){
			  /* Give a nicer error for "enum {}".  */
			  if (c_parser_next_token_is (parser, CPP_CLOSE_BRACE) && !parser->error){
				 error_at (c_parser_peek_token (parser)->location,"empty enum is invalid");
				 parser->error = true;
			  }else
				 c_parser_error (parser, "expected identifier");
			  c_parser_skip_until_found (parser, CPP_CLOSE_BRACE, NULL);
			  values = error_mark_node;
			  break;
		  }
		  token = c_parser_peek_token (parser);
		  enum_id = token->value;
		  char *elementOrigName=n_strdup(IDENTIFIER_POINTER(enum_id));
		  enum_id=createElementName(enum_id,sysName);
		  /* Set the location in case we create a decl now.  */
		  c_c_parser_set_source_position_from_token (token);
		  decl_loc = value_loc = token->location;
		  c_parser_consume_token (parser);
		  /* Parse any specified attributes.  */
		  tree std_attrs = NULL_TREE;
		  if (c_c_parser_nth_token_starts_std_attributes (parser, 1))
			std_attrs = c_c_parser_std_attribute_specifier_sequence (parser);
		  tree enum_attrs = chainon (std_attrs,c_c_parser_gnu_attributes (parser));
		  if (c_parser_next_token_is (parser, CPP_EQ)){
			  c_parser_consume_token (parser);
			  value_loc = c_parser_peek_token (parser)->location;
			  enum_value = c_c_parser_expr_no_commas (parser, NULL).value;
		  }else
			  enum_value = NULL_TREE;
		  enum_decl = build_enumerator (decl_loc, value_loc,&the_enum, enum_id, enum_value);
		  addElement(enumData,sysName,elementOrigName,enum_decl);
		  if (enum_attrs)
			 decl_attributes (&TREE_PURPOSE (enum_decl), enum_attrs, 0);
		  TREE_CHAIN (enum_decl) = values;
		  values = enum_decl;
		  seen_comma = false;
		  if (c_parser_next_token_is (parser, CPP_COMMA)){
			  comma_loc = c_parser_peek_token (parser)->location;
			  seen_comma = true;
			  c_parser_consume_token (parser);
		  }
		  if (c_parser_next_token_is (parser, CPP_CLOSE_BRACE)){
			  if (seen_comma)
				pedwarn_c90 (comma_loc, OPT_Wpedantic,"comma at end of enumerator list");
			  c_parser_consume_token (parser);
			  break;
		  }
		  if (!seen_comma){
			  c_parser_error (parser, "expected %<,%> or %<}%>");
			  c_parser_skip_until_found (parser, CPP_CLOSE_BRACE, NULL);
			  values = error_mark_node;
			  break;
		  }
	  }//end while
      postfix_attrs = c_c_parser_gnu_attributes (parser);
      ret.spec = finish_enum (type, nreverse (values),chainon (std_attrs,chainon (attrs, postfix_attrs)));
      ret.kind = ctsk_tagdef;
      ret.expr = NULL_TREE;
      ret.expr_const_operands = true;
      timevar_pop (TV_PARSE_ENUM);
      return ret;
    }else if (!ident){
      c_parser_error (parser, "expected %<{%>");
      ret.spec = error_mark_node;
      ret.kind = ctsk_tagref;
      ret.expr = NULL_TREE;
      ret.expr_const_operands = true;
      return ret;
    }
  /* Attributes may only appear when the members are defined or in
     certain forward declarations (treat enum forward declarations in
     GNU C analogously to struct and union forward declarations in
     standard C).  */
  if (have_std_attrs && c_parser_next_token_is_not (parser, CPP_SEMICOLON))
    c_parser_error (parser, "expected %<;%>");
  ret = parser_xref_tag (ident_loc, ENUMERAL_TYPE, ident, have_std_attrs,std_attrs);
  /* In ISO C, enumerated types can be referred to only if already
     defined.  */
  if (pedantic && !COMPLETE_TYPE_P (ret.spec)){
      gcc_assert (ident);
      pedwarn (enum_loc, OPT_Wpedantic,
	       "ISO C forbids forward references to %<enum%> types");
   }
   return ret;
}

static void addEnumData(EnumParser *self,ClassName *className,EnumData *item)
{
	char *sysName=className?className->sysName:"";
	if(!n_hash_table_contains(self->hashTable,sysName)){
		NPtrArray *array=n_ptr_array_sized_new(2);
	    n_ptr_array_add(array,item);
	    n_hash_table_insert (self->hashTable, n_strdup(sysName),array);
	}else{
		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
	    n_ptr_array_add(array,item);
	}
}

/**
 * 只处理以下两种，其它由系统处理
  * 以下几种形式:
 * 1.有类型名 abcd 有变量名 efg
 * class Abc{
 *   enum abcd{
 *     type1,
 *   }efg;
 *
  * 2.有类型名 abcd 无变量名
 * class Abc{
 *   enum abcd{
 *    type1,
 *   };
 */
struct c_typespec  enum_parser_parser(EnumParser *self,location_t loc,ClassName *className)
{
	c_parser *parser=self->parser;
	EnumData *data=NULL;
	struct c_typespec cc= c_parser_enum_specifier (self,className,&data);
	if(data!=NULL){
	    //printf("enum_parser_parser ---- %s %s %s %s %s %d\n",
	            //className?className->sysName:"",data->sysName,data->origName,data->typedefName,data->enumName,data->elementCount);
		addEnumData(self,className,data);
		data->loc=loc;
	}
	return cc;
}

static EnumData *findEnumType(EnumParser *self,char *sysName,char *origNameOrTypeName,nboolean orig)
{
	EnumData *item=getEnumDataByName(self,sysName,origNameOrTypeName,orig);
	if(item!=NULL){
		return item;
	}
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	if(info==NULL)
		return NULL;
	if(info->parentName.sysName==NULL)
		return NULL;
	return findEnumType(self,info->parentName.sysName,origNameOrTypeName,orig);
}

/**
 * 解析 AObject.Enum此种类的类型
 * 应转化成AObject中的一个枚举类型
 */
tree  enum_parser_parser_dot(EnumParser *self,struct c_typespec *specs)
{
	c_parser *parser=self->parser;
	tree type=specs->spec;
	char *sysName=class_util_get_class_name(type);
	if(sysName==NULL){
		tree v=DECL_NAME(type);
		if(v && TREE_CODE(v)==IDENTIFIER_NODE){
			sysName=IDENTIFIER_POINTER(v);
			if(!class_mgr_is_class(class_mgr_get(),sysName)){
				return NULL_TREE;
			}
		}
	}
	if(sysName==NULL)
		return NULL_TREE;
	c_token *t=c_parser_peek_2nd_token(parser);
    if(t->type!=CPP_NAME)
	   return NULL_TREE;
    char *origName=IDENTIFIER_POINTER(t->value);
    EnumData *item=findEnumType(self,sysName,origName,TRUE);
    if(item==NULL)
        item=findEnumType(self,sysName,origName,FALSE);
    if(item!=NULL){
    	//说明找到一个枚举类型
    	c_parser_consume_token (parser); //consume cpp_dot
     	c_parser_consume_token (parser); //consume cpp_name(enum 名字)
    	return item->typeDecl;
    }else{
        n_warning("找不到类%s的枚举类型%s！检查类型是否有错。",sysName,origName);
    }
	return NULL_TREE;
}

/**
 * 判断是不是一个枚举名被混淆的枚举
 * typedefName=_e012test_AObject7AeEnums300426128
 */
nboolean enum_parser_is_enum(EnumParser *self,char *typedefName)
{
	char *origName=enum_parser_get_orig_name(self,typedefName);
	return origName!=NULL;
}

/**
 * 判断AObject.Enum是不是一个枚举
 */
nboolean enum_parser_is_class_dot_enum(EnumParser *self,char *sysName,char *name)
{
	EnumData *item=findEnumType(self,sysName,name,TRUE);//把name当用户书写的枚举名。
	if(item==NULL)
		item=findEnumType(self,sysName,name,FALSE);//把name当系统混淆了的枚举名。
	//判断访问权限
	return item!=NULL;
}

static EnumElement *getElement(EnumData *enumData,char *origName)
{
	int i;
	for(i=0;i<enumData->elementCount;i++){
		EnumElement *item=enumData->elements[i];
		if(strcmp(item->origName,origName)==0)
			return item;
	}
	return NULL;
}

/**
 * 解析 var= Enum.element;
 */
void enum_parser_build_dot (EnumParser *self, location_t loc,struct c_expr *expr)
{
	c_parser *parser=self->parser;
	tree enumTree = c_parser_peek_token (parser)->value;
	char *typedefName=IDENTIFIER_POINTER(enumTree);
	tree component;
	c_parser_consume_token (parser);
	if (!c_parser_require (parser, CPP_DOT, "expected %<.%>")){
	   expr->set_error ();
	   return;
	}
	if (c_parser_next_token_is_not (parser, CPP_NAME)){
	   c_parser_error (parser, "expected identifier");
	   expr->set_error ();
	   return;
	}
	c_token *component_tok = c_parser_peek_token (parser);
	component = component_tok->value;
	location_t end_loc = component_tok->get_finish ();
	c_parser_consume_token (parser);
	EnumData *enumData=enum_parser_get_enum(self,typedefName);
	if(enumData==NULL){
	   c_parser_error (parser, "不是枚举类型。");
	   expr->set_error ();
	   return;
	}
	EnumElement *elem=getElement(enumData,IDENTIFIER_POINTER(component));
	if(elem==NULL){

		 error_at (end_loc, "枚举%qs没有%qs元素。xxxx",enumData->origName,IDENTIFIER_POINTER(component));
		 return;
	}
    n_debug("enum_parser_build_dot 访问枚举:%s %s %s\n",enumData->sysName,enumData->origName,enumData->enumName);
	access_controls_access_enum(access_controls_get(),loc, enumData,elem->origName);
	expr->value =TREE_VALUE(elem->value);
    set_c_expr_source_range (expr, loc, end_loc);
}

/**
 * 解析 var= AObject.Enum.element;
 */
void   enum_parser_build_class_dot_enum (EnumParser *self, location_t loc,char *sysName,char *origEnumName,struct c_expr *expr)
{
	c_parser *parser=self->parser;
	EnumData *enumData=findEnumType(self,sysName,origEnumName,TRUE);
	if(enumData==NULL)
		enumData=findEnumType(self,sysName,origEnumName,FALSE);
	if(enumData==NULL){
	   c_parser_error (parser, "不是枚举类型。");
	   expr->set_error ();
	   return;
	}
	aet_print_token(c_parser_peek_token (parser));
	c_token *token=c_parser_peek_token (parser);
	if(token->type==CPP_DOT){
	   c_parser_consume_token (parser);//consume CPP_DOT
	   if (c_parser_next_token_is_not (parser, CPP_NAME)){
		   c_parser_error (parser, "expected identifier");
		   expr->set_error ();
		   return;
		}
		token = c_parser_peek_token (parser);
		tree component = token->value;
		location_t end_loc = token->get_finish ();
		c_parser_consume_token (parser);//consume field
		EnumElement *elem=getElement(enumData,IDENTIFIER_POINTER(component));
		if(elem==NULL){
			 error_at (end_loc, "枚举%qs没有%qs元素。yyyy",enumData->origName,IDENTIFIER_POINTER(component));
			 return;
		}
	    access_controls_access_enum(access_controls_get(),loc, enumData,elem->origName);
		expr->value =TREE_VALUE(elem->value);
		set_c_expr_source_range (expr, loc, end_loc);
	}else if(token->type==CPP_NAME){
        error_at (input_location, "枚举%qs没有%qs元素。wwww",enumData->origName,IDENTIFIER_POINTER(token->value));
	}else{
	   c_parser_error (parser, "expected identifier");
	   expr->set_error ();
	   return;
	}
}


/**
 * 解析 var= AObject.Enum.element;
 */
void enum_parser_build_class_enum_dot (EnumParser *self, location_t loc,struct c_expr *expr)
{
	c_parser *parser=self->parser;
	tree enumTree = c_parser_peek_token (parser)->value;
	char *typedefName=IDENTIFIER_POINTER(enumTree);
	tree component;
	c_parser_consume_token (parser);
	if (!c_parser_require (parser, CPP_DOT, "expected %<.%>")){
	   expr->set_error ();
	   return;
	}
	if (c_parser_next_token_is_not (parser, CPP_NAME)){
	   c_parser_error (parser, "expected identifier");
	   expr->set_error ();
	   return;
	}
	c_token *component_tok = c_parser_peek_token (parser);
	component = component_tok->value;
	location_t end_loc = component_tok->get_finish ();
	c_parser_consume_token (parser);
	EnumData *enumData=enum_parser_get_enum(self,typedefName);
	if(enumData==NULL){
	   c_parser_error (parser, "不是枚举类型。");
	   expr->set_error ();
	   return;
	}
	EnumElement *elem=getElement(enumData,IDENTIFIER_POINTER(component));
	if(elem==NULL){
		 error_at (end_loc, "枚举%qs没有%qs元素。zzzz",enumData->origName,IDENTIFIER_POINTER(component));
		 return;
	}
	expr->value =TREE_VALUE(elem->value);
    set_c_expr_source_range (expr, loc, end_loc);
}


void  enum_parser_set_parser(EnumParser *self,c_parser *parser)
{
	 self->parser=parser;
}


EnumParser *enum_parser_get()
{
	static EnumParser *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(EnumParser));
		 enumParserInit(singleton);
	}
	return singleton;
}


