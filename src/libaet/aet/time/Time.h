

#ifndef __AET_TIME_TIME_H__
#define __AET_TIME_TIME_H__

#include <objectlib.h>

package$ aet.time;

public$ class$ Time{
   public$ static auint UsecPerSec=1000000;
   public$ static aint64 currentTime();
   public$ static aint64 monotonic();
   private$  Time();
};

#endif /* __N_MEM_H__ */

