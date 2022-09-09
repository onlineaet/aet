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
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classpermission.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classpermission.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "varmgr.h"
#include "aet-c-parser-header.h"
#include "classfunc.h"
#include "funcmgr.h"

/**
 * 方法的访问控制：
            public protected default private
同类         T        T        T      T
同包         T        T
子类(不同包)  T        T
不同包中无继  T
承关系的类
 */


static void classPermissionPermission(ClassPermission *self)
{
	self->permission=CLASS_PERMISSION_DEFAULT;
	self->running=FALSE;
}

static nboolean  inHFile(ClassPermission *self)
{
	   c_parser *parser=self->parser;
       cpp_buffer *buffer= parse_in->buffer;
       struct _cpp_file *file=buffer->file;
       const char *fileName=_cpp_get_file_name (file);
       cpp_dir *dir=cpp_get_dir (file);
       NString *fileStr=n_string_new(fileName);
       nboolean in=n_string_ends_with(fileStr,".h");
   	   n_string_free(fileStr,TRUE);
       n_debug("class_permission_in_h_file 00 在头文件中：%d %s %s %s\n",in,fileName,dir->name,dir->canonical_name);
   	   return in;
}

/**
 * 只能是在class 或 interface的域里调用
 */
void class_permission_check_and_set(ClassPermission *self,ClassParserState state,ClassPermissionType type)
{
	  c_parser *parser=self->parser;
	  location_t  loc = c_parser_peek_token (parser)->location;
	  self->loc=loc;//把错误信息时需要用到位置
	  if(state!=CLASS_STATE_STOP && state!=CLASS_STATE_FIELD){
		  if(state!=CLASS_STATE_FIELD)
		    error_at(loc,"访问权限关键字只能出现在类的第一个字符！");
		  else
			error_at(loc,"访问权限关键字只能出现在方法或变量的第一个字符！");
		  return;
	 }
     self->permission=type;
}


ClassPermissionType class_permission_get_permission_type(ClassPermission *self,char *sysClassName,ClassType classType)
{
   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
   if(classType==CLASS_TYPE_NORMAL || classType==CLASS_TYPE_ABSTRACT_CLASS){
	   if(!inHFile(self)){
		  if(self->permission==CLASS_PERMISSION_PUBLIC || self->permission==CLASS_PERMISSION_PROTECTED){
			 error_at(self->loc,"在c文件中，类%qs只能用private$修饰，默认是private。",className->userName);
			 return CLASS_PERMISSION_PRIVATE;
		  }
	   }else{
	      if(self->permission==CLASS_PERMISSION_PRIVATE){
			 //error_at(self->loc,"类%qs只能用public$或protected$修饰。默认是protected。",className->userName);
		     return CLASS_PERMISSION_PRIVATE;
	      }
	   }
   }else{
	   if(!inHFile(self)){
		  error_at(self->loc,"接口%qs只能声明在h文件中。",className->userName);
		  return CLASS_PERMISSION_PRIVATE;
	   }else{
		   if(self->permission!=CLASS_PERMISSION_PUBLIC){
			  error_at(self->loc,"接口%qs只能用public$修饰。",className->userName);
			  return CLASS_PERMISSION_PRIVATE;
		   }
	   }
   }
   return self->permission==CLASS_PERMISSION_DEFAULT?CLASS_PERMISSION_PROTECTED:self->permission;
}

void  class_permission_reset(ClassPermission *self)
{
	self->permission=CLASS_PERMISSION_DEFAULT;
	self->loc=0;
}

static nboolean isFieldFunc(ClassPermission *self,tree field)
{
	//检查是不是field_decl
	if(TREE_CODE(field)!=FIELD_DECL)
		return FALSE;
	char *id=IDENTIFIER_POINTER(DECL_NAME(field));
	int len=IDENTIFIER_LENGTH(DECL_NAME(field));
	if(id==NULL || len<2 || id[0]!='_' || id[1]!='Z')
		return FALSE;
	tree type=TREE_TYPE(field);
	if(TREE_CODE(type)!=POINTER_TYPE)
		return FALSE;
	tree funtype=TREE_TYPE(type);
	if(TREE_CODE(funtype)!=FUNCTION_TYPE)
		return FALSE;
    return TRUE;
}

nboolean class_permission_set_field(ClassPermission *self,tree decls,ClassName *className)
{
	nboolean ok=FALSE;
	nboolean is=isFieldFunc(self,decls);
	if(!is){
		var_mgr_set_permission(var_mgr_get(),className,
				decls,self->permission==CLASS_PERMISSION_DEFAULT?CLASS_PERMISSION_PROTECTED:self->permission);
	}else{
		ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
		ClassType classType=classInfo->type;
	   if(classType==CLASS_TYPE_INTERFACE && self->permission!=CLASS_PERMISSION_PUBLIC && self->permission!=CLASS_PERMISSION_DEFAULT){
		  error_at(self->loc,"接口%qs的方法只能用public修饰。默认是public。",className->userName);
		  goto out;
	   }
	   char *id=IDENTIFIER_POINTER(DECL_NAME(decls));
	   ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,id);
	   if(entity==NULL){
		   error_at(self->loc,"在类:%qs，找不到mangle函数名。",className->userName);
		  goto out;
	   }
	   if(classType==CLASS_TYPE_INTERFACE){
		   entity->permission=CLASS_PERMISSION_PUBLIC;
	   }else{
	       entity->permission=self->permission;
	       entity->permission=self->permission==CLASS_PERMISSION_DEFAULT?CLASS_PERMISSION_PROTECTED:self->permission;
	   }
	   ok=TRUE;
    }
out:
    class_permission_reset(self);
	return ok;
}

////////////////////////=================


static nboolean isPermissionStaticFinal(c_token *token)
{
	int type=token->keyword;
	return (type== RID_AET_PUBLIC  || type== RID_AET_PROTECTED || type== RID_AET_PRIVATE
			|| type== RID_STATIC || type== RID_AET_FINAL);
}



static FieldDecorate createFieldDecorate(int keyword,int classType,location_t loc)
{
 	ClassPermissionType per=CLASS_PERMISSION_PROTECTED;
    if(classType==CLASS_TYPE_INTERFACE || classType==-1)
        per=CLASS_PERMISSION_PUBLIC;
	if(keyword==RID_STATIC){
		FieldDecorate dr={per,TRUE,FALSE,loc};
		return dr;
	}else if(keyword==RID_AET_FINAL){
		FieldDecorate dr={per,FALSE,TRUE,loc};
	    return dr;
	}else{
	    per=CLASS_PERMISSION_PROTECTED;
	    if(keyword==RID_AET_PUBLIC){
		   per=CLASS_PERMISSION_PUBLIC;
	    }else if(keyword==RID_AET_PROTECTED){
		   per=CLASS_PERMISSION_PROTECTED;
	    }else if(keyword==RID_AET_PRIVATE){
		   per=CLASS_PERMISSION_PRIVATE;
	    }
	    FieldDecorate dr={per,FALSE,FALSE,loc};
	    return dr;
	}
}

static nboolean equalType(int keyword,int keyword2)
{
	if(keyword==keyword2){
		return TRUE;
	}else{
		nboolean is1=(keyword== RID_AET_PUBLIC  || keyword== RID_AET_PROTECTED || keyword== RID_AET_PRIVATE);
		nboolean is2=(keyword2== RID_AET_PUBLIC  || keyword2== RID_AET_PROTECTED || keyword2== RID_AET_PRIVATE);
		return (is1 && is2);
	}
}

static int fillType(int keyword,FieldDecorate *dr)
{
    if(keyword==RID_STATIC){
		dr->isStatic=TRUE;
		return 1;
	}else if(keyword==RID_AET_FINAL){
		dr->isFinal=TRUE;
		return 2;
	}else{
		ClassPermissionType per=CLASS_PERMISSION_PROTECTED;
		if(keyword==RID_AET_PUBLIC){
		   per=CLASS_PERMISSION_PUBLIC;
		}else if(keyword==RID_AET_PROTECTED){
		   per=CLASS_PERMISSION_PROTECTED;
		}else if(keyword==RID_AET_PRIVATE){
		   per=CLASS_PERMISSION_PRIVATE;
		}
		dr->permission=per;
		return 0;
	}
}

static FieldDecorate createFieldDecorateSecond(int keyword,int keyword2,int classType,location_t loc)
{
	if(equalType(keyword,keyword2)){
		error_at(input_location,"重复的修饰。");
		FieldDecorate dr={CLASS_PERMISSION_PROTECTED,FALSE,FALSE,0};
		return dr;
	}
 	ClassPermissionType per=CLASS_PERMISSION_PROTECTED;
    if(classType==CLASS_TYPE_INTERFACE || classType==-1)
        per=CLASS_PERMISSION_PUBLIC;
	FieldDecorate dr;
	int pos1=fillType(keyword,&dr);
	int pos2=fillType(keyword2,&dr);
	if(pos1!=0 && pos2!=0){
		dr.permission=per;
	}else if(pos1!=1 && pos2!=1){
		dr.isStatic=FALSE;
	}else if(pos1!=2 && pos2!=2){
		dr.isFinal=FALSE;
	}
	dr.loc=loc;
	return dr;
}

static FieldDecorate createFieldDecorateThree(int keyword,int keyword2,int keyword3,location_t loc)
{
	nboolean re1=equalType(keyword,keyword2);
	nboolean re2=equalType(keyword,keyword3);
	nboolean re3=equalType(keyword2,keyword3);
	if(re1 || re2 || re3){
		error_at(input_location,"重复的修饰。");
		FieldDecorate dr={CLASS_PERMISSION_PROTECTED,FALSE,FALSE,0};
		return dr;
	}
	FieldDecorate dr;
	fillType(keyword,&dr);
	fillType(keyword2,&dr);
	fillType(keyword3,&dr);
	dr.loc=loc;
	return dr;
}

/**
 * 尝试解析token是不是public static final等，并且与classType绑定。
 * 如果没有public static final等，根据classType类型设一个缺省的
 */
FieldDecorate   class_permission_try_parser(ClassPermission *self,int classType)
{
    c_parser *parser=self->parser;
    int first=-1;
    int second=-1;
    int three=-1;
    location_t loc=c_parser_peek_token (parser)->location;
    if(isPermissionStaticFinal(c_parser_peek_token (parser))){
        first=c_parser_peek_token (parser)->keyword;
	    c_parser_consume_token (parser);
	    if(isPermissionStaticFinal(c_parser_peek_token (parser))){
	    	second=c_parser_peek_token (parser)->keyword;
		    c_parser_consume_token (parser);
		    if(isPermissionStaticFinal(c_parser_peek_token (parser))){
		    	three=c_parser_peek_token (parser)->keyword;
			    c_parser_consume_token (parser);
		    }
	    }
    }
    //printf("parserPermissionStaticFinal :%d %d %d\n",first,second,three);
    if(first==-1){
    	ClassPermissionType per=CLASS_PERMISSION_PROTECTED;
    	if(classType==CLASS_TYPE_INTERFACE || classType==-1)
    		per=CLASS_PERMISSION_PUBLIC;
    	//printf("没有public static final修饰。给一个默认的 %d\n",per);
    	FieldDecorate dr={per,FALSE,FALSE,0};
    	return dr;
    }else{
        if(second==-1){
            return createFieldDecorate(first,classType,loc);
        }else{
        	if(three==-1){
        		return  createFieldDecorateSecond(first,second,classType,loc);
        	}else{
        		return createFieldDecorateThree(first,second,three,loc);
        	}
        }
    }
}

void  class_permission_set_decorate(ClassPermission *self,FieldDecorate *dr)
{
    c_parser *parser=self->parser;
	if(self->running){
	   error_at(input_location,"关键字public$ final$等已存在。");
	   return;
	}
	memcpy(&self->currentDecorate,dr,sizeof(FieldDecorate));
	self->running=TRUE;
}

/**
 * 在解析完class interface获得修饰。
 * 如果running=FALSE.说明没有。编一个缺省的针对class的
 */
FieldDecorate class_permission_get_decorate_by_class(ClassPermission *self,char *sysClassName,ClassType classType)
{
    FieldDecorate dr={CLASS_PERMISSION_PUBLIC,FALSE,FALSE};
    ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysClassName);
    if(classType==CLASS_TYPE_NORMAL || classType==CLASS_TYPE_ABSTRACT_CLASS){
	   if(!inHFile(self)){ //在.c文件中 可以用任何修饰 public protected private
		  if(!self->running){
             //说明没有public final static 修饰
			  return dr;
		  }else{
			  if(self->currentDecorate.isStatic){
				  error_at(self->loc,"在c文件中，类%qs不能用static修饰。",className->userName);
			  }
			  return self->currentDecorate;
		  }
	   }else{
		  if(!self->running){
		      //说明没有public final static 修饰
		 	  return dr;
		  }else{
		      if(self->currentDecorate.permission!=CLASS_PERMISSION_PUBLIC){
				 error_at(self->loc,"在.h文件中，类%qs只能用public$修饰。",className->userName);
			  }
			  if(self->currentDecorate.isStatic){
				  error_at(self->loc,"类%qs不能用static修饰。",className->userName);
			  }
			  return self->currentDecorate;
		  }
	   }
   }else{
	   if(!inHFile(self)){
		  error_at(self->loc,"接口%qs只能声明在h文件中。",className->userName);
		  return dr;
	   }else{
		   if(!self->running){
		   	  //说明没有public final static 修饰
		   	  return dr;
		   }else{
			   if(self->currentDecorate.permission!=CLASS_PERMISSION_PUBLIC){
			 	   error_at(self->loc,"接口%qs只能用public$修饰。",className->userName);
			   }
			   if(self->currentDecorate.isStatic || self->currentDecorate.isFinal){
			  	   error_at(self->loc,"接口%qs不能用static或final修饰。",className->userName);
			   }
			   return self->currentDecorate;
		   }
	   }
   }
}

void class_permission_stop(ClassPermission *self)
{
	self->running=FALSE;
}


static nboolean findParentFinalField(ClassPermission *self,ClassName *className,ClassFunc *dest)
{
	ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(classInfo->parentName.sysName){
		NPtrArray  *funcs=func_mgr_get_funcs(func_mgr_get(), &classInfo->parentName);
		if(funcs!=NULL){
			int i;
			for(i=0;i<funcs->len;i++){
				ClassFunc *item=(ClassFunc *)n_ptr_array_index(funcs,i);
				if(strcmp(item->rawMangleName,dest->rawMangleName)==0){
					if(class_func_is_final(item)){
						  error_at(self->loc,"类%qs的父类%qs已经声明方法%qs为final$,子类不能继承。",
								  className->userName,classInfo->parentName.userName,item->orgiName);
						  return FALSE;
					}
				}
			}
		}
		return findParentFinalField(self,&classInfo->parentName,dest);
	}
	return TRUE;
}


/**
 * 给函数设权限
 */
void class_permission_set_field_decorate(ClassPermission *self,tree decls,ClassName *className)
{
	nboolean is=isFieldFunc(self,decls);
	FieldDecorate *dr=NULL;
	if(self->running){
		dr=&self->currentDecorate;
	}else{
	    FieldDecorate temp={CLASS_PERMISSION_PROTECTED,FALSE,FALSE,0};
		dr=&temp;
	}
	if(!is){
		var_mgr_set_permission(var_mgr_get(),className,decls,dr->permission);
		var_mgr_set_final(var_mgr_get(),className,decls,dr->isFinal);
	}else{
		ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
		ClassType classType=classInfo->type;
	    if(classType==CLASS_TYPE_INTERFACE){
		   if(dr->permission!=CLASS_PERMISSION_PUBLIC && self->running){
		      error_at(self->loc,"接口%qs的方法只能用public修饰。默认是public。",className->userName);
		      return;
		   }
	    }
	    char *id=IDENTIFIER_POINTER(DECL_NAME(decls));
	    ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,id);
	    if(entity==NULL){
		    error_at(self->loc,"在类:%qs，找不到mangle函数名。",className->userName);
	   		return;
	    }
	    if(classType==CLASS_TYPE_INTERFACE){
		   entity->permission=CLASS_PERMISSION_PUBLIC;
	    }else{
	       entity->permission=dr->permission;
	    }
	    if(class_func_is_abstract(entity) && dr->isFinal){
	   		error_at(self->loc,"类:%qs中的抽象方法:%qs不能用final$修饰。",className->userName,entity->orgiName);
	   		return;
	   	}
	    if(findParentFinalField(self,className,entity)){
	   	   class_func_set_final(entity,dr->isFinal);
	    }
    }
}


ClassPermission *class_permission_new()
{
	ClassPermission *self = n_slice_alloc0 (sizeof(ClassPermission));
	classPermissionPermission(self);
	return self;
}
