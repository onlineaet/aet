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

#ifndef __AET_IO_A_FILE_H__
#define __AET_IO_A_FILE_H__

#include "../../aet.h"
#include "../util/AList.h"

package$ aet.io;


typedef enum{
  ACCESS_READ=     0x04,
  ACCESS_WRITE=    0x02,
  ACCESS_EXECUTE=  0x01,
}FileSystemAccessType;

public$ class$ AFile{

	public$ static int  lengthv(AFile **files){
		if(files==NULL)
			return 0;
	    int i=0;
	    while(files[i]!=NULL){
		   i++;
	    }
	   return i;
	}

	public$ static void  freev(AFile **files){
		if(files==NULL)
			return;
	    int i=0;
	    while(files[i]!=NULL){
	       AFile *item=(AFile*)files[i];
	       item->unref();
	       files[i]=NULL;
		   i++;
	    }
	    a_free(files);
	}

	static const char *getDefaultParent(){
		   return "/";
	}

	private$ char *path;
	private$ int   prefixLength;
	private$ char *name;
	private$ char *parent;
	private$ char *absolutePath;
	public$ AFile(const char *pathname);
	public$ AFile(AFile *parent, const char *child);
	public$ AFile(const char *parent, const char *child);

	public$ const char *getName();
	public$ const char *getParent();
	public$ const char *getPath();
	public$ AFile      *getParentFile();
	public$ aboolean    isAbsolute();
	public$ aboolean    exists();
	public$ char       *getAbsolutePath();
	public$ AFile      *getAbsoluteFile() ;
	public$ aboolean    canRead() ;
	public$ aboolean    canWrite() ;
	public$ aboolean    isDirectory();
	public$ aboolean    isFile();
	public$ aboolean    isHidden();
	public$ auint64     getLastModified();
	public$ auint64	    getCreateTime();
	public$ auint64     length();
	public$ aboolean    createNewFile();
	public$ aboolean    delete();
	public$ char      **list();
	public$ AFile     **listFiles();
	public$ AList      *toList();
	public$ aboolean    makeDir();
	public$ aboolean    makeDirs();
	public$ AFile 	   *getCanonicalFile();
	public$ aboolean 	canExecute();
	public$ aboolean    rename(AFile *dest);
	public$ aboolean    setReadOnly();
	public$ aboolean    setLastModified(long time);
	public$ aboolean    setWritable(aboolean writable, aboolean ownerOnly);
	public$ aboolean    setReadable(aboolean readable, aboolean ownerOnly);
	public$ aboolean    setExecutable(aboolean executable, aboolean ownerOnly);
	public$ auint64     getTotalSpace();
	public$ auint64     getTotalFree();
	public$ auint64     getTotalUsable();
	public$ aboolean    equals(AFile *dest);
	public$ const char *toString();
};



#endif /* __N_FILE_H__*/




