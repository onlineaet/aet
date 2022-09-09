

#ifndef __AET_UTIL_KEY_FILE_UTIL_H__
#define __AET_UTIL_KEY_FILE_UTIL_H__

#include <stdarg.h>
#include <objectlib.h>


package$ aet.util;


public$ class$ KeyFileUtil{

    protected$ static aboolean isComment(const char *line);
    protected$ static aboolean isGroup (const char *line);
    protected$ static aboolean isGroupName(const char *name);
    protected$ static aboolean isKeyValuePair(const char *line);
    protected$ static aboolean isKeyName(const char *name);
    protected$ static char *getLocal(const char *key);

};




#endif /* __N_MEM_H__ */

