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
#include "AFile.h"
#include "../lang/AString.h"
#include "../util/AArray.h"
#include "FileSystem.h"

#define SEPARATOR_CHAR '/'

#define  BA_EXISTS     0x01
#define  BA_REGULAR    0x02
#define  BA_DIRECTORY  0x04
#define  BA_HIDDEN     0x08

#define  SPACE_TOTAL 1
#define  SPACE_FREE  2
#define  SPACE_USABLE  3

/**
 * 以下是非AFile类的函数
 */
static int   canonicalize(char *original, char *resolved, int len);

static char *utf8ToUtf16(char *pathName,int *writtenLen)
{
	long len = 0;
	AError *error = NULL;
	char *ucs2_dst = (char*)a_utf8_to_utf16 (pathName, strlen (pathName), NULL,&len, &error);
  	if(error){
  		a_warning("nfile pathName err %s -- %s,%s %d",error->message,pathName,__FILE__,__LINE__);
  		a_error_free(error);
  		if(ucs2_dst)
  			a_free(ucs2_dst);
  		return NULL;
  	}
  	*writtenLen =(int)len;
  	return ucs2_dst;
}

static aboolean checkNamValid(char *pathName)
{
	int n=0;
	char *ucs2_dst = utf8ToUtf16(pathName,&n);
	if(ucs2_dst==NULL){
		return FALSE;
	}
	a_free(ucs2_dst);
	return TRUE;
}

static aboolean statMode(const char *path, int *mode)
{
	struct stat64 sb;
	if (stat64(path, &sb) == 0) {
		*mode = sb.st_mode;
		return TRUE;
	}
   return FALSE;
}

static aboolean setPermission(char *path,FileSystemAccessType access,aboolean enable,aboolean owneronly)
{
	aboolean rv = FALSE;
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


static auint64 getSpace(char *path,int type)
{
    auint64 rv = 0;
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

static int getCmdPath(char *buf)
{
	int count=1024;
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


static char * normalizeoff(const char *pathname, int len, int off)
{
  if (len == 0)
	return a_strdup(pathname);
  int n;
  char *ucs2_dst = utf8ToUtf16(pathname,&n);
  if(ucs2_dst == NULL)
	return ucs2_dst;
  int temp=n;
  int i;
  for (i = n-1; i >0; i--){
	 char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
	 if ((thisChar[0] == '/')){
			  temp--;
	 }else{
		 break;
	 }
   }
   n=temp;
   if (n == 0){
	   a_free(ucs2_dst);
		return "/";
   }
   AString g=new$ AString("");
   if(off>0){
	 for(i=0;i<off;i++){
		 char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
		 char *c =(char *)a_utf16_to_utf8 ((const aunichar2 *)thisChar,1, NULL, NULL, NULL);
		 g.append(c);
		 a_free(c);
	 }
   }
   char prevChar[2]={0};
   for (i = off; i < n; i++) {
		char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
		char *c =(char *)a_utf16_to_utf8 ((const aunichar2 *)thisChar,1, NULL, NULL, NULL);
		if ((prevChar[0] == '/') && (c[0] == '/')){
			a_free(c);
			continue;
		}
		g.append(c);
		prevChar[0] = c[0];
		prevChar[1] = c[1];
		a_free(c);

	}
	char *path=a_strdup(g.toString());
	a_free(ucs2_dst);
	return path;
}

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
   	    char *c =(char *)a_utf16_to_utf8 ((const aunichar2 *)thisChar,1, NULL, NULL, NULL);
   	    //printf("xxx is index:%d %d %d %d\n",i,c[0],c[1],'/');
        if ((prevChar[0] == '/') && (c[0] == '/')){
        	//printf("there is data --- %d\n",i);
            a_free(c);
            a_free(ucs2_dst);//need free
            return normalizeoff(pathName, n, i);
        }
        prevChar[0] = c[0];
        prevChar[1] = c[1];
        a_free(c);
     }
     a_free(ucs2_dst);//need free
     if (prevChar[0] == '/')
    	 return normalizeoff(pathName, n, n - 1);
     return a_strdup(pathName);
}


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
        char *c =(char *)a_utf16_to_utf8 ((const aunichar2 *)thisChar,1, NULL, NULL, NULL);
        if (c[0] == ch){
            a_free(c);
            a_free(ucs2_dst);
            return i;
        }
        a_free(c);
    }
    a_free(ucs2_dst);
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
    AString g=new$ AString("");
    for (i =start;i<end;i++) {
       	char thisChar[2]={ucs2_dst[i*2],ucs2_dst[i*2+1]};
        char *c =(char *)a_utf16_to_utf8 ((const aunichar2 *)thisChar,1, NULL, NULL, NULL);
        g.append(c);
        a_free(c);
    }
    //printf("last last %s\n",g->str);
    char *last=a_strdup(g.toString());
    a_free(ucs2_dst);
    return last;
}

static int prefixLength(char *pathname)
{
  if (strlen(pathname) == 0)
	  return 0;
  return (pathname[0]== '/') ? 1 : 0;
}

static AFile *fileNew(char *pathname, int prefixLength){
	AFile *file=new$ AFile();
	file->path=a_strdup(pathname);
	file->prefixLength = prefixLength;
	return file;
}

//*******************以下是类实现-------------------------

impl$ AFile{

    static char *resolve0(char *parent, char *child){
        if (strcmp(child,"")==0)
            return a_strdup(parent);
        if (child[0] == '/') {
            if (strcmp(parent,"/")==0)
                return a_strdup(child);
            return a_strdup_printf("%s%s",parent,child);
        }
        if (strcmp(parent,"/")==0){
            return a_strdup_printf("%s%s",parent,child);
        }
        return a_strdup_printf("%s/%s",parent,child);
    }

	AFile(){
		self->path=NULL;
		self->prefixLength=0;
		self->name=NULL;
		self->parent=NULL;
		self->absolutePath=NULL;
	}

	AFile(const char *pathname){
	    self();
		aboolean re=checkNamValid(pathname);
		if(!re)
			return NULL;
		self->path=normalize(pathname);
		self->prefixLength=prefixLength(path);
	}

	AFile(const char *parent, const char *child){
        self();
        if (child == NULL) {
            a_error("AFile child is NULL.");
            return NULL;
        }
        aboolean re=checkNamValid(child);
        if(!re)
              return NULL;
        char *lastPath;
        if (parent != NULL) {
            const char *path0=parent;
            if(strcmp(path0,"")==0){
                const char *parentStr=AFile.getDefaultParent();
                lastPath=resolve0(parentStr,child);
             } else {
                 char *c=normalize(child);
                 lastPath=resolve0(path0,c);
                 a_free(c);
              }
         } else {
             lastPath = normalize(child);
         }
        self->prefixLength = prefixLength(lastPath);
        self->path=lastPath;
	}


	AFile(AFile *parent, const char *child){
		 self(parent->getPath(),child);
	}

	const char *getName(){
		if(self->name==NULL){
			int index = lastIndexOf(path,SEPARATOR_CHAR);
			if (index <prefixLength){
				name=subString(path,prefixLength,-1);
				return name;
			}
			name=subString(path,index + 1,-1);
		}
		return name;
	}



	const char *getParent(){
		if(parent==NULL){
			int index = lastIndexOf(path,SEPARATOR_CHAR);
			int length;
			char *ucs2_dst = utf8ToUtf16(path,&length);
			if(ucs2_dst == NULL)
				return ucs2_dst;
			a_free(ucs2_dst);
			if (index < prefixLength){
				 if ((prefixLength > 0) && (length >prefixLength)){
					 parent=subString(path,0, prefixLength);
					 return parent;
				 }
				 return NULL;
			}
			parent=subString(path, 0,index);
		}
		return parent;
	}

	AFile *getParentFile(){
		const char *p = getParent();
		if (p == NULL)
			return NULL;
		AFile *file= fileNew(p, prefixLength);
		return file;
	}


	const char *getPath(){
		return self->path;
	}


	aboolean isAbsolute(){
		return prefixLength!=0;
	}




	char *getAbsolutePath(){
		if(isAbsolute())
			return self->path;
		if(self->absolutePath==NULL){
			char userDir[1024];
			getCmdPath(userDir);
			self->absolutePath =resolve0(userDir, path);
		}
		return self->absolutePath;
	 }

	AFile *getAbsoluteFile(){
		char *absPath = getAbsolutePath();
		AFile *file=fileNew(absPath, prefixLength(absPath));
		a_free(absPath);
		return file;
	}

	aboolean canRead(){
		return  access((const char *)path, R_OK)==0;
	}

	aboolean canWrite(){
		return  access(path, W_OK)==0;
	}

	static int getBooleanAttributes0(){
		int rv = 0;
		int mode;
		if (statMode(path, &mode)){
		   int fmt = mode & S_IFMT;
		   rv = (BA_EXISTS  | ((fmt == S_IFREG) ? BA_REGULAR : 0)  | ((fmt == S_IFDIR) ? BA_DIRECTORY : 0));
		}
		return rv;
	}

	static  int getBooleanAttributes(){
			int rv = getBooleanAttributes0();
			char *name = getName();
			aboolean hidden = (strlen(name) > 0) && (name[0] == '.');
			return rv | (hidden ? BA_HIDDEN : 0);
	}

	aboolean exists(){
		return (getBooleanAttributes() & BA_EXISTS) != 0;
	}

	aboolean isDirectory(){
		return (getBooleanAttributes() & BA_DIRECTORY)!= 0;
	}

	aboolean isFile(){
		return (getBooleanAttributes() & BA_REGULAR)!= 0;
	}

	aboolean isHidden(){
		return (getBooleanAttributes() & BA_HIDDEN)!= 0;
	}

	/**
	 * 返回最后修改的时间，单位 毫秒
	 */
	auint64 getLastModified(){
		struct stat64 sb;
		auint64 rv=0;
		if (stat64(path, &sb) == 0){
			rv = 1000 * sb.st_mtime;
		}
		return rv;
	}

	auint64 getCreateTime(){
		struct stat64 sb;
		auint64 rv=0;
		if (stat64(path, &sb) == 0){
			rv = sb.st_ctime;
		}
		return rv;
	}

	auint64  length(){
		struct stat64 sb;
		auint64 rv=0;
		if (stat64(path, &sb) == 0){
			rv = sb.st_size;
		}
		return rv;
	}


	aboolean createNewFile(){
		int fd;
		if (!strcmp (path, "/")) {
				fd = -111;    /* The root directory always exists */
		} else {
			 fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0666);
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

	aboolean delete(){
		if (remove((const char *)path) == 0){
		   return TRUE;
		}
		return FALSE;
	}

	char **list(){
		DIR *dir = NULL;
		struct dirent *ptr;
		struct dirent *result;
		dir = opendir(path);
		if (dir == NULL)
			return NULL;
		ptr = a_malloc(sizeof(struct dirent) + (PATH_MAX + 1));
		if (ptr == NULL){
            printf("heap allocation failed\n");
            closedir(dir);
            return NULL;
		}
		AArray *rv = new$ AArray<char *>();
		while ((readdir_r(dir, ptr, &result) == 0)  && (result != NULL)) {
            if (!strcmp(ptr->d_name, ".") || !strcmp(ptr->d_name, "..") || !ptr->d_name)
                continue;
            rv->add(a_strdup(ptr->d_name));
		}
		closedir(dir);
		free(ptr);
		int size=rv->size();
		char **str_array = a_new (char*, size + 1);
		str_array[size]=NULL;
		int i;
		for(i=0;i<size;i++){
			str_array[i]=rv->get(i);
			//printf("str_array[i] i:%d %s\n",i,str_array[i]);
		}
		rv->unref();
		return str_array;
	}


	AFile  **listFiles(){
        DIR *dir = NULL;
        struct dirent *ptr;
        struct dirent *result;
        dir = opendir(path);
        if (dir == NULL)
            return NULL;
        ptr = a_malloc(sizeof(struct dirent) + (PATH_MAX + 1));
        if (ptr == NULL){
            printf("heap allocation failed\n");
            closedir(dir);
            return NULL;
        }
        AArray<AFile *> *rv = new$ AArray<AFile *>();
        while ((readdir_r(dir, ptr, &result) == 0)  && (result != NULL)) {
            if (!strcmp(ptr->d_name, ".") || !strcmp(ptr->d_name, "..") || !ptr->d_name)
                continue;
            rv->add(new$ AFile(self,ptr->d_name)->ref());//new AFile()出while范围会被释放。所以加上一次引用。
        }
        closedir(dir);
        free(ptr);
        int size=rv->size();
        AFile **files= a_new (AFile*, size + 1);
        files[size]=NULL;
        int i;
        for(i=0;i<size;i++)
            files[i]=rv->get(i);
            //printf("str_array[i] i:%d %s\n",i,files[i]->getAbsolutePath());
        rv->unref();
        return files;
	}


	AList *toList(){
		char **filesPath=list();
		if(filesPath==NULL)
			return NULL;
		int size=a_strv_length(filesPath);
		AList *files =new$ AList();
		int i;
		for(i=0;i<size;i++){
			files->add(new$ AFile(self,filesPath[i])->ref());
		}
		a_strfreev(filesPath);
		return files;
	}

	aboolean makeDir(){
		if (mkdir(path, 0777) == 0){
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
	AFile *getCanonicalFile(){
		achar  *canonPath=FileSystem.getInstance()->getCanonicalizeFileName(path,NULL);
		AFile  *file=new$ AFile(canonPath);
		a_free(canonPath);
		return file;
	}

	aboolean makeDirs(){
		if (exists()){
			return FALSE;
		}
		if (makeDir()){
			return TRUE;
		}
		AFile *canonFile = getCanonicalFile();
		if(canonFile==NULL)
			return FALSE;
		AFile *parent = canonFile->getParentFile();
		aboolean re=(parent != NULL && (parent->makeDirs() || parent->exists()) && canonFile->makeDir());
		parent->unref();
		canonFile->unref();
		return re;
	}



	static int collapsible(char *names){
		char *p = names;
		int dots = 0, n = 0;

		while (*p) {
			if ((p[0] == '.') && ((p[1] == '\0') || (p[1] == '/') || ((p[1] == '.') && ((p[2] == '\0')	|| (p[2] == '/'))))) {
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



	static void splitNames(char *names, char **ix){
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



	static void joinNames(char *names, int nc, char **ix){
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



	static void collapse(char *path){
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



	static int canonicalize(char *original, char *resolved, int len){
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


	aboolean rename(AFile *dest){
		if(dest==NULL)
			return FALSE;
		return rename(path,dest->getPath())==0;
	}


	aboolean  setLastModified(long time){
		 if (time < 0)
			return FALSE;
		 aboolean rv = FALSE;
		 struct timeval tv[2];
		 struct stat64 sb;
		 if (stat64(path, &sb) == 0){
		   tv[0].tv_sec = sb.st_atime;
		   tv[0].tv_usec = 0;
		 }
		 tv[1].tv_sec = time / 1000;
		 tv[1].tv_usec = (time % 1000) * 1000;
		 if (utimes(path, tv) >= 0)
			rv =TRUE;
		 return rv;
	}


	aboolean setReadOnly(){
		 int mode;
		 if (statMode(path, &mode)){
			if (chmod(path, mode & ~(S_IWUSR | S_IWGRP | S_IWOTH)) >= 0){
					return TRUE;
			}
		 }
		 return FALSE;
	}


	aboolean setWritable(aboolean writable, aboolean ownerOnly){
		 return setPermission(path,ACCESS_WRITE, writable, ownerOnly);
	}


	aboolean setReadable(aboolean readable, aboolean ownerOnly){
		 return setPermission(path,ACCESS_READ, readable, ownerOnly);
	}


	aboolean setExecutable(aboolean executable, aboolean ownerOnly){
		 return setPermission(path,ACCESS_EXECUTE, executable, ownerOnly);
	}


	aboolean canExecute(){
		return  access((const char *)path, X_OK)==0;
	}

	auint64 getTotalSpace(){
	   return (auint64)getSpace(path,SPACE_TOTAL);
	}

	auint64 getTotalFree(){
		 return (auint64)getSpace(path,SPACE_FREE);
	}

	auint64 getTotalUsable(){
		 return (auint64)getSpace(path,SPACE_USABLE);
	}


	aboolean   equals(AFile *dest){
		 if(dest==NULL)
			 return FALSE;
		 if(self==dest)
			 return TRUE;
		 char *path=getAbsolutePath();
		 char *path1=dest->getAbsolutePath();
		 return strcmp(path,path1)==0;
	}


	const char *toString(){
		return getAbsolutePath();
	}

	~AFile(){
		if(path){
		  a_free(path);
		}
		if(name){
		   a_free(name);
		}
		if(parent){
			a_free(parent);
		}
		if(absolutePath){
		   a_free(absolutePath);
		}
	}
};

