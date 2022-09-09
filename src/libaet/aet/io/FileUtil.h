#ifndef __AET_IO_FILE_UTIL_H__
#define __AET_IO_FILE_UTIL_H__

#include <stdarg.h>
#include <objectlib.h>
#include "../util/AList.h"


package$ aet.io;


public$ class$ FileUtil{

    public$  static aboolean getContents(char  *filename,char  **contents,asize *length,AError **error);
    private$ static aboolean getContentsStdio(char *filename,FILE *f,char **contents,asize *length,AError **error);
    private$ static aboolean getContentsRegfile(const char *filename,struct stat  *stat_buf,int fd,
                                     char **contents,asize *length,AError  **error);

};



#endif /* __N_FILE_H__*/




