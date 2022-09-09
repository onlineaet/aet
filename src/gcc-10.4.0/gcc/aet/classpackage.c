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
#include "opts.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classpackage.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classpackage.h"
#include "aetprinttree.h"
#include "aet-c-parser-header.h"



typedef struct _PackageInfo
{
	char *packageName;//包名
	char *fileName;
	char *path;
	char *classPrefix;//class前缀 如 com_aet_Abc com_aet就是 Abc类的前缀
}PackageInfo;



static void freePackage_cb(npointer userData)
{
  printf("class_package free free %p\n",userData);
}

static void classPackagePackage(ClassPackage *self)
{
	self->packHashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, freePackage_cb);
}

void  class_package_parser00(ClassPackage *self)
{
	  c_parser *parser=self->parser;
	  location_t loc = c_parser_peek_token (parser)->location;
	  c_parser_consume_token (parser);
      NString *package=n_string_new("");
      while(TRUE){
  		 if(c_parser_next_token_is (parser, CPP_NAME)){
  			 tree value=c_parser_peek_token (parser)->value;
  			 char *re=IDENTIFIER_POINTER(value);
             n_string_append(package,re);
  		 }else if(c_parser_next_token_is (parser, CPP_DOT)){
             n_string_append(package,".");
  		 }else if(c_parser_next_token_is (parser, CPP_SEMICOLON)){
  	  		 c_parser_consume_token (parser);
  			 break;
  		 }else{
     	     n_string_free(package,TRUE);
  			 error_at(loc,"包名的语法不正确。可能缺分号或包含有非法的字符。%qs",package->str);
  			 return;
  		 }
  		  c_parser_consume_token (parser);
      }
      cpp_buffer *buffer= parse_in->buffer;
      struct _cpp_file *file=buffer->file;
      const char *fileName=_cpp_get_file_name (file);
      const char *filePath=  cpp_get_path (file);
      cpp_dir *dir=cpp_get_dir (file);
      n_debug("class_package_parser 22 fileName:%s dir->name:%s package->str:%s filePath:%s",fileName,dir->name,package->str,filePath);
      NFile *nfile=n_file_new(filePath);
      NFile *canonicalFile=n_file_get_canonical_file(nfile);
      char *canonicalPath=n_file_get_absolute_path(canonicalFile);
      nboolean exists=n_file_exists(nfile);
      n_debug("class_package_parser 33----exists:%d canonicalPath:%s in_fnames[0]:%s",exists,canonicalPath,in_fnames[0]);
       NString *fileStr=n_string_new(canonicalPath);
       if(!n_string_ends_with(fileStr,".h")){
    	    n_string_free(fileStr,TRUE);
 	        n_string_free(package,TRUE);
	        error_at(loc,"package$关键字只能使用在头文件,%qs",fileName);
	        n_file_unref(nfile);
	        n_file_unref(canonicalFile);
	        return;
       }
       char *dirName=canonicalPath;
       int pos=n_string_indexof(fileStr,"src");//src 目录是关键
       if(pos<0){
    	  if(dirName){
       	       NString *pathStr=n_string_new(dirName);
    	       int srcPos=n_string_indexof(pathStr,"src");
 	   	       n_string_free(pathStr,TRUE);
    	       if(srcPos<0){
    	     	  n_string_free(package,TRUE);
    	       	  error_at(loc,"源文件应在src目录下,%qs",fileName);
    	       	  return;
    	       }
    	  }else{
     	    n_string_free(fileStr,TRUE);
  	        n_string_free(package,TRUE);
    	    error_at(loc,"源文件应在src目录下,%qs",fileName);
    	    return;
    	  }
       }
       NString *path=fileStr;
       nboolean needFree=FALSE;
       if(pos>0){
          path=n_string_substring(fileStr,pos+3+1);
          needFree=TRUE;
       }else{
           int sp=n_string_indexof(fileStr,"/");
           if(sp<0){
        	    NString *pathStr=n_string_new(dirName);
        	    int srcPos=n_string_indexof(pathStr,"src");
                path=n_string_substring(pathStr,srcPos+3+1);
  	   	        n_string_free(pathStr,TRUE);
  	            needFree=TRUE;
           }
       }
       nchar **pathsv=n_strsplit(path->str,"/",-1);
       if(needFree>0)
      	   n_string_free(path,TRUE);
       int lenv=n_strv_length(pathsv);
       NString *pathdot=n_string_new("");
       NString *classPrefix=n_string_new("");

       int i;
       for(int i=0;i<lenv-1;i++){
    	   n_string_append(pathdot,pathsv[i]);
    	   n_string_append(classPrefix,pathsv[i]);
    	   if(i<lenv-1-1){
        	   n_string_append(pathdot,".");
        	   n_string_append(classPrefix,"_");
    	   }
       }
       if(strcmp(pathdot->str,package->str)){
		   error_at(loc,"包名与文件所在目录不匹配，正确的是:%qs",pathdot->str);
		   goto out;
       }else{
		   PackageInfo *info=n_slice_new0(PackageInfo);
		   info->fileName=n_strdup(fileName);
		   info->path=n_strdup(dir->name);
		   info->packageName=n_strdup(pathdot->str);
		   info->classPrefix=n_strdup(classPrefix->str);
		   char file[255];
		   sprintf(file,"%s%s",info->path,fileName);
		   if(n_hash_table_contains(self->packHashTable,file)){
			   error_at(loc,"一个头文件只能有一次package$ %qs",file);
			   goto out;
		   }
		   n_debug("packinfo is fileName:%s path:%s package:%s prefix:%s fullfile:%s\n",
				   info->fileName,info->path,info->packageName,info->classPrefix,file);
		   n_hash_table_insert (self->packHashTable, n_strdup(file),info);
       }
out:
       n_strfreev(pathsv);
  	   n_string_free(pathdot,TRUE);
  	   n_string_free(classPrefix,TRUE);
  	   n_string_free(fileStr,TRUE);
	   n_string_free(package,TRUE);
       n_file_unref(nfile);
       n_file_unref(canonicalFile);
}



void  class_package_parser(ClassPackage *self)
{
      c_parser *parser=self->parser;
      location_t loc = c_parser_peek_token (parser)->location;
      c_parser_consume_token (parser);
      NString *package=n_string_new("");
      while(TRUE){
         if(c_parser_next_token_is (parser, CPP_NAME)){
             tree value=c_parser_peek_token (parser)->value;
             char *re=IDENTIFIER_POINTER(value);
             n_string_append(package,re);
         }else if(c_parser_next_token_is (parser, CPP_DOT)){
             n_string_append(package,".");
         }else if(c_parser_next_token_is (parser, CPP_SEMICOLON)){
             c_parser_consume_token (parser);
             break;
         }else{
             n_string_free(package,TRUE);
             error_at(loc,"包名的语法不正确。可能缺分号或包含有非法的字符。%qs",package->str);
             return;
         }
          c_parser_consume_token (parser);
      }
      cpp_buffer *buffer= parse_in->buffer;
      struct _cpp_file *file=buffer->file;
      cpp_dir *dir=cpp_get_dir (file);
      const char *fileName=_cpp_get_file_name (file);
      n_debug("class_package_parser 00 fileName:%s dir->name:%s package->str:%s filePath:%s",fileName,dir->name,package->str,cpp_get_path (file));
      NFile *nfile=n_file_new(cpp_get_path (file));
      NFile *canonicalFile=n_file_get_canonical_file(nfile);
      //必须用canonicalFile 因为:nfile的文件名是:libaet/src/aet/lang/../util/AList.h
      //通过canonical后才能变成libaet/src/aet//util/AList.h
      char *canonicalPath=n_file_get_absolute_path(canonicalFile);
      if(!n_file_exists(nfile)){
          error_at(loc,"在包解析时，文件不存在,%qs",canonicalPath);
          return;

      }
      NString *nfileStr=n_string_new(canonicalPath);
      if(!n_string_ends_with(nfileStr,".h")){
             error_at(loc,"package$关键字只能使用在头文件,%qs",fileName);
            return;
      }
       //fileName是编译目录下包含有子目录的文件名 如 aet/lang/AObject.h aet/lang是子目录。所以包名应该是aet.lang
       char *rawFileName=n_file_get_name(nfile);
       NString *packToPath=n_string_new(package->str);
       n_string_replace(packToPath,".","/",-1);
       n_string_append(packToPath,"/");
       n_string_append(packToPath,rawFileName);
       if(!n_string_ends_with(nfileStr,packToPath->str)){
             error_at(loc,"包名%qs与文件所在目录%qs不匹配。",package->str,dir->name);
             return;
       }

       NString *classPrefixStr=n_string_new(package->str);
       n_string_replace(classPrefixStr,".","_",-1);
       PackageInfo *info=n_slice_new0(PackageInfo);
       info->fileName=n_strdup(fileName);
       info->path=n_strdup(dir->name);
       info->packageName=n_strdup(package->str);
       info->classPrefix=n_strdup(classPrefixStr->str);
       char filex[255];
       sprintf(filex,"%s%s",info->path,fileName);
       if(n_hash_table_contains(self->packHashTable,filex)){
           error_at(loc,"一个头文件只能有一次package$ %qs",filex);
       }
       n_debug("class_package_parser 11 fileName:%s path:%s package:%s prefix:%s fullfile:%s\n",
               info->fileName,info->path,info->packageName,info->classPrefix,filex);
       n_hash_table_insert (self->packHashTable, n_strdup(filex),info);
       n_string_free(classPrefixStr,TRUE);
       n_string_free(packToPath,TRUE);
       n_string_free(nfileStr,TRUE);
       n_file_unref(nfile);
       n_file_unref(canonicalFile);

}


char *class_package_get_class_prefix(ClassPackage *self,char *fileName,char *path)
{
    char file[255];
    sprintf(file,"%s%s",path,fileName);
    PackageInfo *item=(PackageInfo *)n_hash_table_lookup(self->packHashTable,file);
    if(item==NULL){
    	n_warning("class_package_get_class_prefix 找不到file:%s",file);
    	return NULL;
    }
    return item->classPrefix;
}

ClassPackage *class_package_new()
{
	ClassPackage *self = n_slice_alloc0 (sizeof(ClassPackage));
	classPackagePackage(self);
	return self;
}


