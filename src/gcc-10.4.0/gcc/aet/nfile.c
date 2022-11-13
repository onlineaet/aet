/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

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

//#define _LARGEFILE_SOURCE
//#define _LARGEFILE64_SOURCE
//#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE

#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <alloca.h>
#include <errno.h>
#include "nfile.h"
#include "nstring.h"
#include "nprintf.h"
#include "nmem.h"
#include "nslice.h"
#include "nlog.h"
#include "nstrfuncs.h"
#include "nptrarray.h"
#include "nlist.h"
#include "nerror.h"
#include "nfileutils.h"

#define SEPARATOR_CHAR '/'

#define  BA_EXISTS     0x01
#define  BA_REGULAR    0x02
#define  BA_DIRECTORY  0x04
#define  BA_HIDDEN     0x08

#define  SPACE_TOTAL 1
#define  SPACE_FREE  2
#define  SPACE_USABLE  3

struct _NFilePrivate
{
	char *path;
	int prefixLength;
	char *name;
	char *parent;
	char *absolutePath;
};

static void  finalize(NFile *self);
static int   canonicalize(char *original, char *resolved, int len);


static  void n_file_init (NFile *self)
{
	self->priv=n_slice_new(NFilePrivate);
	NFilePrivate *priv=self->priv;
	priv->path=NULL;
	priv->prefixLength=0;
	priv->name=NULL;
    priv->parent=NULL;
    priv->absolutePath=NULL;

}



static void finalize (NFile *self)
{
	NFilePrivate *priv=self->priv;
	if(priv->path){
		n_free(priv->path);
		priv->path=NULL;
	}
	if(priv->name){
			n_free(priv->name);
			priv->name=NULL;
    }
	if(priv->parent){
			n_free(priv->parent);
			priv->parent=NULL;
	}
	if(priv->absolutePath){
			n_free(priv->absolutePath);
			priv->absolutePath=NULL;
	}
}

static char *utf8ToUtf16(char *pathName,int *writtenLen)
{
	long len = 0;
	NError *error = NULL;
	char *ucs2_dst = (char*)n_utf8_to_utf16 (pathName, strlen (pathName), NULL,&len, &error);
  	if(error){
  		n_warning("nfile pathName err %s -- %s,%s %d",error->message,pathName,__FILE__,__LINE__);
  		n_error_free(error);
  		if(ucs2_dst)
  			n_free(ucs2_dst);
  		return NULL;
  	}
  	*writtenLen =(int)len;
  	return ucs2_dst;
}


static char * normalizeoff(const char *pathname, int len, int off)
{
	if (len == 0)
		return n_strdup(pathname);
	int n;
	char *ucs2_dst = utf8ToUtf16(pathname,&n);
	if(ucs2_dst == NULL)
		return ucs2_dst;
      int temp=n;
      int i;
      for (i = n-1; i >0; i--)
      {
       	 char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
         if ((thisChar[0] == '/')){
            	  temp--;
         }else{
        	 break;
         }
       }
       n=temp;
       if (n == 0){
    	   n_free(ucs2_dst);
        	return "/";
       }
       NString *g=n_string_new("");
       if(off>0){
         for(i=0;i<off;i++)
         {
           char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
	   	   char *c =(char *)n_utf16_to_utf8 ((const nunichar2 *)thisChar,1, NULL, NULL, NULL);
    	   n_string_append(g,c);
    	   n_free(c);
         }
       }
  	   char prevChar[2]={0};
       for (i = off; i < n; i++) {
            char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
 	   	    char *c =(char *)n_utf16_to_utf8 ((const nunichar2 *)thisChar,1, NULL, NULL, NULL);
            if ((prevChar[0] == '/') && (c[0] == '/')){
            	n_free(c);
            	continue;
            }
            n_string_append(g,c);
            prevChar[0] = c[0];
            prevChar[1] = c[1];
            n_free(c);

        }
        char *path=n_strdup(g->str);
        n_string_free(g,TRUE);
        n_free(ucs2_dst);
        return path;
}


/*
 * @func_name
 * normalize
 *
 * @brief:   
 *
 * @param:	const char *pathName a pointer param. 
 *
 * @return:	char *
 * Return value of a pointer type
 * Need to pay attention to the address of the pointer
 * If the return is an EObject
 * Need to pay attention to the reference count.
 *
 * @see  :	
 *
 * @Since: 1.0.0	
 */
static char *normalize(const char *pathName)
{
	int n = 0;
	char *ucs2_dst = utf8ToUtf16(pathName,&n);
	if(ucs2_dst == NULL)
		return ucs2_dst;
	 char prevChar[2]={0};
     int i;
     for (i = 0; i < n; i++) {
    	char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
   	    char *c =(char *)n_utf16_to_utf8 ((const nunichar2 *)thisChar,1, NULL, NULL, NULL);
   	    //printf("xxx is index:%d %d %d %d\n",i,c[0],c[1],'/');
        if ((prevChar[0] == '/') && (c[0] == '/')){
        	//printf("there is data --- %d\n",i);
            n_free(c);
            n_free(ucs2_dst);//need free
            return normalizeoff(pathName, n, i);
        }
        prevChar[0] = c[0];
        prevChar[1] = c[1];
        n_free(c);
     }
     n_free(ucs2_dst);//need free
     if (prevChar[0] == '/')
    	 return normalizeoff(pathName, n, n - 1);
     return n_strdup(pathName);
}

/*
 * @func_name
 * lastIndexOf
 *
 * @brief:   
 *
 * @param:	char *pathName a pointer param. 
 * @param:	char ch 
 *
 * @return:	int
 * 
 * @see  :	
 *
 * @Since: 1.0.0	
 */
static int lastIndexOf(char *pathName,char ch)
{
	int n;
	char *ucs2_dst = utf8ToUtf16(pathName,&n);
	if(ucs2_dst==NULL){
		return FALSE;
	}
    int i;
    for (i = n-1; i>0; i--) {
       	char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
        char *c =(char *)n_utf16_to_utf8 ((const nunichar2 *)thisChar,1, NULL, NULL, NULL);
        if (c[0] == ch){
            n_free(c);
            n_free(ucs2_dst);
            return i;
        }
        n_free(c);
    }
    n_free(ucs2_dst);
    return -1;
}


static char *subString(char *pathName,int start,int end)
{
	int n;
	char *ucs2_dst = utf8ToUtf16(pathName,&n);
	if(ucs2_dst == NULL)
		return ucs2_dst;
    int i;
    if(end<=0)
    	end=n;
    NString *g=n_string_new("");
    for (i =start;i<end;i++) {
       	char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
        char *c =(char *)n_utf16_to_utf8 ((const nunichar2 *)thisChar,1, NULL, NULL, NULL);
        n_string_append(g,c);
        n_free(c);
    }
    //printf("last last %s\n",g->str);
    char *last=n_strdup(g->str);
    n_string_free(g,TRUE);
    n_free(ucs2_dst);
    return last;
}



static int prefixLength(char *pathname)
{
      if (strlen(pathname) == 0)
    	  return 0;
      return (pathname[0]== '/') ? 1 : 0;
}


const char *n_file_get_name(NFile *self)
{
	NFilePrivate *priv=self->priv;
	if(priv->name==NULL){
	    int index = lastIndexOf(priv->path,SEPARATOR_CHAR);
	    if (index < priv->prefixLength){
	    	priv->name=subString(priv->path,priv->prefixLength,-1);
	    	return priv->name;
	    }
	    priv->name=subString(priv->path,index + 1,-1);
	}
	return priv->name;
}



const char *n_file_get_parent(NFile *self)
{
	    NFilePrivate *priv=self->priv;
		if(priv->parent==NULL){
		    int index = lastIndexOf(priv->path,SEPARATOR_CHAR);
		    int length;
			char *ucs2_dst = utf8ToUtf16(priv->path,&length);
			if(ucs2_dst == NULL)
				return ucs2_dst;
	        n_free(ucs2_dst);
		    if (index < priv->prefixLength){
		         if ((priv->prefixLength > 0) && (length >priv->prefixLength)){
		        	 priv->parent=subString(priv->path,0, priv->prefixLength);
		        	 return priv->parent;
		         }
		         return NULL;
		    }
		    priv->parent=subString(priv->path, 0,index);
		}
		return priv->parent;
}


static NFile *fileNew(char *pathname, int prefixLength)
{
	NFile *self= n_slice_new(NFile);
	n_file_init(self);
	NFilePrivate *priv=self->priv;
	priv->path=n_strdup(pathname);
	priv->prefixLength = prefixLength;
	return self;
}


NFile *n_file_get_parent_file(NFile *self)
{
	const char *p = n_file_get_parent(self);
	NFilePrivate *priv=self->priv;
	if (p == NULL)
		return NULL;
	NFile *file= fileNew(p, priv->prefixLength);
	return file;
}


const char *n_file_get_path(NFile *self)
{
	NFilePrivate *priv=self->priv;
	return priv->path;
}


nboolean n_file_is_absolute(NFile *self)
{
	NFilePrivate *priv=self->priv;
	return priv->prefixLength!=0;
}


static char *resolve0(char *parent, char *child)
{
        if (strcmp(child,"")==0)
        	return n_strdup(parent);
        if (child[0] == '/') {
            if (strcmp(parent,"/")==0)
            	return n_strdup(child);
            NString *g=n_string_new(parent);
            n_string_append(g,child);
            char *re=n_strdup(g->str);
            n_string_free(g,TRUE);
            return re;
        }
        if (strcmp(parent,"/")==0)
        {
        	NString *g=n_string_new(parent);
        	n_string_append(g,child);
        	char *re=n_strdup(g->str);
        	n_string_free(g,TRUE);
        	return re;
        }
        NString *g=n_string_new(parent);
        n_string_append_c(g,'/');
        n_string_append(g,child);
        char *re=n_strdup(g->str);
        n_string_free(g,TRUE);
        return re;
}

static int getCmdPath(char *buf)
{
	int count=1024;
	//char fileName[255];
//	sprintf(fileName,"/proc/%d/cwd",pid);
	char *fileName="/proc/self/exe";
    int rslt=readlink(fileName,buf,count-1); /*proc/pid/exe 是一个链接，用readlink读*/
    int i;
    if (rslt < 0 || (rslt >= count - 1))
    {
        return -1;
    }
    buf[rslt] = '\0';
    for (i = rslt; i >= 0; i--)
    {
        if (buf[i] == '/')
        {
            buf[i + 1] = '\0';
            break;
        }
    }
    return rslt;
}


static char * resolve(NFile *self)
{
	NFilePrivate *priv=self->priv;
    if (n_file_is_absolute(self))
    	return n_strdup(priv->path);
    char userDir[1024];
    getCmdPath(userDir);
    return resolve0(userDir, priv->path);
}


char *n_file_get_absolute_path(NFile *self)
{
	if(self==NULL)
		return NULL;
	NFilePrivate *priv=self->priv;
	if(n_file_is_absolute(self))
		return priv->path;
	if(priv->absolutePath==NULL){
	//	n_free(priv->absolutePath);
//		priv->absolutePath=NULL;
		priv->absolutePath=resolve(self);
	}
	//priv->absolutePath=resolve(self);
	return priv->absolutePath;
 }


NFile *n_file_get_absolute_file(NFile *self)
{
    char *absPath = n_file_get_absolute_path(self);
	NFile *file=fileNew(absPath, prefixLength(absPath));
	n_free(absPath);
	return file;
}


nboolean n_file_can_read(NFile *self)
{
	NFilePrivate *priv=self->priv;
	return  access((const char *)priv->path, R_OK)==0;
}



nboolean n_file_can_write(NFile *self)
{
	NFilePrivate *priv=self->priv;
	return  access(priv->path, W_OK)==0;
}


static nboolean statMode(const char *path, int *mode)
{
        struct stat64 sb;
        if (stat64(path, &sb) == 0) {
            *mode = sb.st_mode;
            return TRUE;
        }
       return FALSE;
}


static int getBooleanAttributes0(NFile *self)
{
	NFilePrivate *priv=self->priv;
    int rv = 0;
    int mode;
   if (statMode(priv->path, &mode))
   {

	   int fmt = mode & S_IFMT;
	   rv = (BA_EXISTS  | ((fmt == S_IFREG) ? BA_REGULAR : 0)  | ((fmt == S_IFDIR) ? BA_DIRECTORY : 0));
    }
    return rv;
}


static  int getBooleanAttributes(NFile *self)
{
        int rv = getBooleanAttributes0(self);
        char *name = n_file_get_name(self);
        nboolean hidden = (strlen(name) > 0) && (name[0] == '.');
        return rv | (hidden ? BA_HIDDEN : 0);
}


nboolean n_file_exists(NFile *self)
{
	return (getBooleanAttributes(self) & BA_EXISTS) != 0;
}


nboolean n_file_is_directory(NFile *self)
{
	return (getBooleanAttributes(self) & BA_DIRECTORY)!= 0;
}

nboolean n_file_is_file(NFile *self)
{
	return (getBooleanAttributes(self) & BA_REGULAR)!= 0;

}


nboolean n_file_is_hidden(NFile *self)
{
	return (getBooleanAttributes(self) & BA_HIDDEN)!= 0;

}

/**
 * 返回最后修改的时间，单位 毫秒
 */
nuint64 n_file_get_last_modified(NFile *self)
{
	NFilePrivate *priv=self->priv;
    struct stat64 sb;
    nuint64 rv=0;
    if (stat64(priv->path, &sb) == 0)
    {
	    rv = 1000 * sb.st_mtime;
	}
	return rv;
}

nuint64 n_file_get_create_time(NFile *self)
{
	NFilePrivate *priv=self->priv;
    struct stat64 sb;
    nuint64 rv=0;
    if (stat64(priv->path, &sb) == 0)
    {
	    rv = sb.st_ctime;
	}
	return rv;
}

nuint64  n_file_get_length(NFile *self)
{
	NFilePrivate *priv=self->priv;
    struct stat64 sb;
    nuint64 rv=0;
    if (stat64(priv->path, &sb) == 0)
    {
        rv = sb.st_size;
	}
	return rv;
}


nboolean n_file_create_new_file(NFile *self)
{
	NFilePrivate *priv=self->priv;
    int fd;
    if (!strcmp (priv->path, "/")) {
            fd = -111;    /* The root directory always exists */
    } else {
         fd = open(priv->path, O_RDWR | O_CREAT | O_EXCL, 0666);
    }
    if (fd < 0) {
       if (fd != -111) {
         return FALSE;
       }
       return FALSE;
    } else {
      close(fd);
      return TRUE;
    }
}


nboolean n_file_delete(NFile *self)
{
	NFilePrivate *priv=self->priv;
	if (remove((const char *)priv->path) == 0){
	            return TRUE;
	}
	return FALSE;
}


char **n_file_list(NFile *self)
{
	NFilePrivate *priv=self->priv;
	DIR *dir = NULL;
    struct dirent *ptr;
    struct dirent *result;
    NPtrArray *rv;
    dir = opendir(priv->path);
    if (dir == NULL)
	  	return NULL;
	ptr = n_malloc(sizeof(struct dirent) + (PATH_MAX + 1));
	if (ptr == NULL)
	{
	        printf("heap allocation failed\n");
	        closedir(dir);
	        return NULL;
	}
    rv = n_ptr_array_new();
    /* Scan the directory */
    while ((readdir_r(dir, ptr, &result) == 0)  && (result != NULL)) {
	        char *name;
	        if (!strcmp(ptr->d_name, ".") || !strcmp(ptr->d_name, "..") || !ptr->d_name)
	            continue;

	        name = n_strdup(ptr->d_name);
	        n_ptr_array_add(rv,name);
	}
    closedir(dir);
    free(ptr);
    int size=rv->len;
    char **str_array = n_new (char*, size + 1);
    str_array[size]=NULL;
    int i;
    for(i=0;i<size;i++)
    	str_array[i]=n_ptr_array_index (rv, i);
    n_ptr_array_unref (rv);
    return str_array;
}


NFile  **n_file_list_files(NFile *self)
{
	char **filesPath=n_file_list(self);
	if(filesPath==NULL)
		return NULL;
	int size=n_strv_length(filesPath);
    NFile **files = n_new (NFile *, size + 1);
    int i;
	for(i=0;i<size;i++){
            files[i]=n_file_new_by_parent(self,filesPath[i]);
	}
	files[size]=NULL;
	n_strfreev(filesPath);
	return files;
}


NList *n_file_list_files_to_list(NFile *self)
{
	char **filesPath=n_file_list(self);
	if(filesPath==NULL)
		return NULL;
	int size=n_strv_length(filesPath);
    NList *files =NULL;
    int i;
	for(i=0;i<size;i++){
		files=n_list_append(files,n_file_new_by_parent(self,filesPath[i]));
	}
	n_strfreev(filesPath);
	return files;
}



int   n_file_lengthv(NFile **self)
{
	if(self==NULL)
		return 0;
    int i=0;
    while(self[i]!=NULL){
	   i++;
   }
   return i;
}

void   n_file_freev(NFile **self)
{
	if(self==NULL)
		return;
    int i=0;
    while(self[i]!=NULL){
	   n_file_unref(self[i]);
	   i++;
    }
    n_free(self);
}


nboolean n_file_mkdir(NFile *self)
{
	NFilePrivate *priv=self->priv;
	if (mkdir(priv->path, 0777) == 0)
	{
       return TRUE;
	}
    return FALSE;
}

/*
*规范路径名是绝对路径名，并且是惟一的。规范路径名的准确定义与系统有关。如有必要，此方法首先将路径名转换成绝对路径名，
*这与调用 getAbsolutePath() 方法的效果一样，然后用与系统相关的方式将它映射到其惟一路径名。
*这通常涉及到从路径名中移除多余的名称（比如 "." 和 ".."）、
*分析符号连接（对于 UNIX 平台），以及将驱动器名转换成标准大小写形式（对于 Microsoft Windows 平台）。
*表示现有文件或目录的每个路径名都有一个惟一的规范形式。表示非存在文件或目录的每个路径名也有一个惟一的规范形式。
*非存在文件或目录路径名的规范形式可能不同于创建文件或目录之后同一路径名的规范形式。
*同样，现有文件或目录路径名的规范形式可能不同于删除文件或目录之后同一路径名的规范形式。
*/
NFile *n_file_get_canonical_file(NFile *self)
{
	NFilePrivate *priv=self->priv;
	nchar  *canonPath=n_canonicalize_filename (priv->path,NULL);
	NFile  *file=n_file_new(canonPath);
	n_free(canonPath);
	return file;
}


nboolean n_file_mkdirs(NFile *self)
{
	if (n_file_exists(self)){
	    return FALSE;
	}
	if (n_file_mkdir(self)){
 	    return TRUE;
 	}
    NFile *canonFile = NULL;
    canonFile = n_file_get_canonical_file(self);
    if(canonFile==NULL)
    	return FALSE;
	NFile *parent = n_file_get_parent_file(canonFile);
	nboolean re=(parent != NULL && (n_file_mkdirs(parent) || n_file_exists(parent)) &&
		n_file_mkdir(canonFile));
	n_file_unref(parent);
	n_file_unref(canonFile);
	return re;
}



static int collapsible(char *names)
{
    char *p = names;
    int dots = 0, n = 0;

    while (*p) {
        if ((p[0] == '.') && ((p[1] == '\0')
                              || (p[1] == '/')
                              || ((p[1] == '.') && ((p[2] == '\0')
                                                    || (p[2] == '/'))))) {
            dots = 1;
        }
        n++;
        while (*p) {
            if (*p == '/') {
                p++;
                break;
            }
            p++;
        }
    }
    return (dots ? n : 0);
}



static void splitNames(char *names, char **ix)
{
    char *p = names;
    int i = 0;
    while (*p) {
        ix[i++] = p++;
        while (*p) {
            if (*p == '/') {
                *p++ = '\0';
                break;
            }
            p++;
        }
    }
}



static void joinNames(char *names, int nc, char **ix)
{
    int i;
    char *p;
    for (i = 0, p = names; i < nc; i++) {
        if (!ix[i]) continue;
        if (i > 0) {
            p[-1] = '/';
        }
        if (p == ix[i]) {
            p += strlen(p) + 1;
        } else {
            char *q = ix[i];
            while ((*p++ = *q++));
        }
    }
    *p = '\0';
}



static void collapse(char *path)
{
    char *names = (path[0] == '/') ? path + 1 : path; /* Preserve first '/' */
    int nc;
    char **ix;
    int i, j;
    char *p, *q;

    nc = collapsible(names);
    if (nc < 2) return;         /* Nothing to do */
    ix = (char **)alloca(nc * sizeof(char *));
    splitNames(names, ix);

    for (i = 0; i < nc; i++) {
        int dots = 0;

        /* Find next occurrence of "." or ".." */
        do {
            char *p = ix[i];
            if (p[0] == '.') {
                if (p[1] == '\0') {
                    dots = 1;
                    break;
                }
                if ((p[1] == '.') && (p[2] == '\0')) {
                    dots = 2;
                    break;
                }
            }
            i++;
        } while (i < nc);
        if (i >= nc) break;

        /* At this point i is the index of either a "." or a "..", so take the
           appropriate action and then continue the outer loop */
        if (dots == 1) {
            /* Remove this instance of "." */
            ix[i] = 0;
        }
        else {
            /* If there is a preceding name, remove both that name and this
               instance of ".."; otherwise, leave the ".." as is */
            for (j = i - 1; j >= 0; j--) {
                if (ix[j]) break;
            }
            if (j < 0) continue;
            ix[j] = 0;
            ix[i] = 0;
        }
        /* i will be incremented at the top of the loop */
    }

    joinNames(names, nc, ix);
}



static int canonicalize(char *original, char *resolved, int len)
{
    if (len < PATH_MAX) {
        errno = EINVAL;
        return -1;
    }

    if (strlen(original) > PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    /* First try realpath() on the entire path */
    if (realpath(original, resolved)) {
        /* That worked, so return it */
        collapse(resolved);
        printf("realpath is %s\n",resolved);
        return 0;
    }
    else {
        /* Something's bogus in the original path, so remove names from the end
           until either some subpath works or we run out of names */
        char *p, *end, *r = NULL;
        char path[PATH_MAX + 1];

        strncpy(path, original, sizeof(path));
        if (path[PATH_MAX] != '\0') {
            errno = ENAMETOOLONG;
            return -1;
        }
        end = path + strlen(path);

        for (p = end; p > path;) {

            /* Skip last element */
            while ((--p > path) && (*p != '/'));
            if (p == path) break;

            /* Try realpath() on this subpath */
            *p = '\0';
            r = realpath(path, resolved);
            *p = (p == end) ? '\0' : '/';

            if (r != NULL) {
                /* The subpath has a canonical path */
                break;
            }
            else if (errno == ENOENT || errno == ENOTDIR || errno == EACCES) {
                /* If the lookup of a particular subpath fails because the file
                   does not exist, because it is of the wrong type, or because
                   access is denied, then remove its last name and try again.
                   Other I/O problems cause an error return. */
                continue;
            }
            else {
                return -1;
            }
        }

        if (r != NULL) {
            /* Append unresolved subpath to resolved subpath */
            int rn = strlen(r);
            if (rn + strlen(p) >= len) {
                /* Buffer overflow */
                errno = ENAMETOOLONG;
                return -1;
            }
            if ((rn > 0) && (r[rn - 1] == '/') && (*p == '/')) {
                /* Avoid duplicate slashes */
                p++;
            }
            strcpy(r + rn, p);
            collapse(r);
            return 0;
        }
        else {
            /* Nothing resolved, so just return the original path */
            strcpy(resolved, path);
            collapse(resolved);
            return 0;
        }
    }
}


nboolean n_file_rename(NFile *self,NFile *dest)
{
	NFilePrivate *priv=self->priv;
	return rename(priv->path,n_file_get_path(dest))==0;
}


nboolean  n_file_set_last_modified(NFile *self,long time)
{
	 NFilePrivate *priv=self->priv;
	 if (time < 0)
		return FALSE;
	 nboolean rv = FALSE;
     struct timeval tv[2];
     struct stat64 sb;
     if (stat64(priv->path, &sb) == 0){
       tv[0].tv_sec = sb.st_atime;
       tv[0].tv_usec = 0;
	 }
     tv[1].tv_sec = time / 1000;
     tv[1].tv_usec = (time % 1000) * 1000;
     if (utimes(priv->path, tv) >= 0)
        rv =TRUE;
     return rv;
}


nboolean n_file_set_read_only(NFile *self)
{
	 NFilePrivate *priv=self->priv;
     int mode;
	 if (statMode(priv->path, &mode))
	 {
	        if (chmod(priv->path, mode & ~(S_IWUSR | S_IWGRP | S_IWOTH)) >= 0){
	                return TRUE;
	        }
	 }
	 return FALSE;
}


static nboolean setPermission(char *path,FileSystemAccessType access,
                                          nboolean enable,
                                          nboolean owneronly)
{
        nboolean rv = FALSE;
        int amode, mode;
        switch (access) {
        case  ACCESS_READ:
            if (owneronly)
                amode = S_IRUSR;
            else
                amode = S_IRUSR | S_IRGRP | S_IROTH;
            break;
        case ACCESS_WRITE:
            if (owneronly)
                amode = S_IWUSR;
            else
                amode = S_IWUSR | S_IWGRP | S_IWOTH;
            break;
        case ACCESS_EXECUTE:
            if (owneronly)
                amode = S_IXUSR;
            else
                amode = S_IXUSR | S_IXGRP | S_IXOTH;
            break;
        default:
            assert(0);
        }
        if (statMode(path, &mode)) {
            if (enable)
                mode |= amode;
            else
                mode &= ~amode;
            if (chmod(path, mode) >= 0) {
                rv = TRUE;
            }
        }
      return rv;
}




nboolean n_file_set_writable(NFile *self,nboolean writable, nboolean ownerOnly)
{
	 NFilePrivate *priv=self->priv;
	 return setPermission(priv->path,ACCESS_WRITE, writable, ownerOnly);
}


nboolean n_file_set_readable(NFile *self,nboolean readable, nboolean ownerOnly)
{
	 NFilePrivate *priv=self->priv;
	 return setPermission(priv->path,ACCESS_READ, readable, ownerOnly);
}


nboolean n_file_set_executable(NFile *self,nboolean executable, nboolean ownerOnly)
{
	 NFilePrivate *priv=self->priv;
	 return setPermission(priv->path,ACCESS_EXECUTE, executable, ownerOnly);
}


nboolean n_file_can_execute(NFile *self)
{
	 NFilePrivate *priv=self->priv;
	return  access((const char *)priv->path, X_OK)==0;
}


static nuint64 getSpace(char *path,int type)
{
    nuint64 rv = 0;
    struct statvfs64 fsstat;
    memset(&fsstat, 0, sizeof(struct statvfs));
    if (statvfs64(path, &fsstat) == 0) {
            switch(type) {
            case SPACE_TOTAL:
                rv = fsstat.f_frsize*fsstat.f_blocks;
                break;
            case SPACE_FREE:
                rv = fsstat.f_frsize*fsstat.f_bfree;
                break;
            case SPACE_USABLE:
                rv = fsstat.f_frsize*fsstat.f_bavail;
                break;
            default:
                assert(0);
            }
     }
     return rv;
}


nuint64 n_file_get_total_space(NFile *self)
{
	 NFilePrivate *priv=self->priv;
	 return (nuint64)getSpace(priv->path,SPACE_TOTAL);
}

nuint64 n_file_get_total_free(NFile *self)
{
	 NFilePrivate *priv=self->priv;
	 return getSpace(priv->path,SPACE_FREE);
}

nuint64 n_file_get_total_usable(NFile *self)
{
	 NFilePrivate *priv=self->priv;
	 return (nuint64)getSpace(priv->path,SPACE_USABLE);
}


const char *n_file_get_default_parent()
{
        return "/";
}


nboolean   n_file_equals(NFile *self,NFile *dest)
{
	 if(self==NULL || dest==NULL)
		 return FALSE;
	 NFile *f=n_file_get_canonical_file(self);
	 NFile *f1=n_file_get_canonical_file(dest);

	 char *path=n_file_get_absolute_path(f);
	 char *path1=n_file_get_absolute_path(f1);
	 nboolean ret= strcmp(path,path1)==0;
	 n_file_unref(f);
	 n_file_unref(f1);
	 return ret;
}


const char *n_file_to_string(NFile *self)
{
	if(self==NULL)
		return NULL;
 	NFilePrivate *priv=self->priv;
 	return n_file_get_absolute_path(self);
}

static nboolean checkNamValid(char *pathName)
{
	int n=0;
	char *ucs2_dst = utf8ToUtf16(pathName,&n);
	if(ucs2_dst==NULL){
		return FALSE;
	}
	n_free(ucs2_dst);
	return TRUE;

}



NFile  *n_file_new(const char *pathname)
{
	nboolean re=checkNamValid(pathname);
	if(!re)
		return NULL;
    NFile *self= n_slice_new(NFile);
    n_file_init(self);
	NFilePrivate *priv=self->priv;
	priv->path=normalize(pathname);
	priv->prefixLength=prefixLength(priv->path);
	return self;
}


NFile      *n_file_new_by_parent(NFile *parent, const char *child)
{
	  if (child == NULL) {
		  n_warning("n_file_new_by_parent child null");
		  return NULL;
	  }
	  nboolean re=checkNamValid(child);
	  if(!re)
			return NULL;
	  char *lastPath;
	  if (parent != NULL) {
		  const char *path=n_file_get_path(parent);
		  if(strcmp(path,"")==0){
			  const char *parentStr=n_file_get_default_parent();
			  lastPath=resolve0(parentStr,child);
           } else {
        	   const char *parentPath=n_file_get_path(parent);
        	   char *c=normalize(child);
        	   lastPath=resolve0(parentPath,c);
        	   n_free(c);
            }
       } else {
    	   lastPath = normalize(child);
       }
       int prefixLength0 = prefixLength(lastPath);
       NFile *self= n_slice_new(NFile);
       n_file_init(self);
       	NFilePrivate *priv=self->priv;
       	priv->path=lastPath;
       	priv->prefixLength=prefixLength0;
       	return self;
}

void n_file_unref(NFile *self)
{
	finalize(self);
}

