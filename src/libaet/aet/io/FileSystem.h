#ifndef __AET_IO_FILE_SYSTEM_H__
#define __AET_IO_FILE_SYSTEM_H__

#include <stdarg.h>
#include <objectlib.h>
#include "../util/AList.h"


package$ aet.io;


public$ class$ FileSystem{

	static FileSystem *getInstance();
	char *getCanonicalizeFileName(const char *filename,const char *relative_to);

};



#endif /* __N_FILE_H__*/




