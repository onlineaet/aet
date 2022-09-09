

#ifndef __AET_UTIL_A_KEY_FILE_H__
#define __AET_UTIL_A_KEY_FILE_H__

#include <stdarg.h>
#include <objectlib.h>
#include "AList.h"
#include "AHashTable.h"
#include "AString.h"

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
   public$ char   **getGroups(asize *length);
   public$ char   **getKeys(const char *groupName,asize *length,AError **error);
   public$ char    *getComment(const char *groupName,const char *key,AError **error);
   public$ aboolean removeComment(const char *groupName,const char *key,AError **error);
   public$ aboolean setComment(const char *groupName,const char *key,const char  *comment,AError **error);
   public$ char    *getValue (const char  *groupName,const char *key,AError **error);
   public$ aboolean removeGroup(const char *groupName,AError  **error);
   public$ aboolean isEmpty();//是否是空的
   public$ auint    getGroupCount();//组数量
   public$ aboolean isFirstGroup(const char *groupName);//第一个组的组名是不是指定的groupName
   public$ int      getInt(const char  *groupName,const char *key,AError **error);

};




#endif /* __N_MEM_H__ */

