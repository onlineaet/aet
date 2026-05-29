/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */

#ifndef __AET_UTIL_A_KEY_FILE_H__
#define __AET_UTIL_A_KEY_FILE_H__

#include "../../aet.h"
#include "AList.h"
#include "AHashTable.h"
#include "../lang/AString.h"

package$ aet.util;

typedef struct _KeyFileGroup KeyFileGroup;
typedef struct _KeyFileKeyValuePair KeyFileKeyValuePair;

public$ class$ AKeyFile{
   public$ static void freeKeyFileGroup_cb(KeyFileGroup *data);
   public$ static void freeKeyFileKeyValuePair_cb(KeyFileKeyValuePair *item);
   public$ static auint ERROR_DOMAIN=1;
   public$ enum$ KeyFileError{
	   UNKNOWN_ENCODING,
	   PARSE,
	   NOT_FOUND,
	   KEY_NOT_FOUND,
	   GROUP_NOT_FOUND,
	   INVALID_VALUE
   };

   public$ enum$ KeyFileFlags{
     FILE_NONE              = 0,
     KEEP_COMMENTS     = 1 << 0,
     KEEP_TRANSLATIONS = 1 << 1
   } ;

   private$ AList        *groups;
   private$ AHashTable   *groupHash;
   private$ KeyFileGroup *startGroup;
   private$ KeyFileGroup *currentGroup;
   private$ AString       *strBuffer; /* 原始数据 */
   private$ char          list_separator;
   private$ KeyFileFlags  flags;
   private$ char        **locales;
   private$ aboolean      repeatGroup;//是否允许重复的组。

   public$          AKeyFile(const char *file,AError **error);
   public$          AKeyFile(const char *data,asize length,KeyFileFlags flags,AError **error);
   public$ char    *getString(const char  *groupName,const char *key,AError **error);
   public$ char   **getGroups(asize *ength);
   public$ char   **getKeys(const char *groupName,asize *length,AError **error);
   public$ char    *getComment(const char *groupName,const char *key,AError **error);
   public$ aboolean removeComment(const char *groupName,const char *key,AError **error);
   public$ aboolean setComment(const char *groupName,const char *key,const char  *comment,AError **error);
   public$ char    *getValue (const char  *groupName,const char *key,AError **error);
   public$ char    *getValue (int groupIndex,const char *key,AError **error);
   //获取第一个key对应的值。
   public$ char*    getFirstValue(const char *key,AError **error);
   public$ aboolean removeGroup(const char *groupName,AError  **error);
   public$ aboolean isEmpty();//是否是空的
   public$ auint    getGroupCount();//组数量
   public$ const char *getGroupName(int index);
   public$ aboolean isFirstGroup(const char *groupName);//第一个组的组名是不是指定的groupName
   public$ int      getInt(const char  *groupName,const char *key,AError **error);

};




#endif /* __N_MEM_H__ */

