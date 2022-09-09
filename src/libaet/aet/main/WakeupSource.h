

#ifndef __AET_MAIN_WAKEUP_SOURCE_H__
#define __AET_MAIN_WAKEUP_SOURCE_H__

#include <objectlib.h>
#include "EventSource.h"

package$ aet.main;




public$  class$ WakeupSource extends$ EventSource {
    int fd;
    WakeupSource(char *name);
    void signal();
    void acknowledge();

};




#endif /* __N_MEM_H__ */

