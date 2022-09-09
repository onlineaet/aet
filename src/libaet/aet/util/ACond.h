

#ifndef __AET_UTIL_A_COND_H__
#define __AET_UTIL_A_COND_H__

#include <stdarg.h>
#include <objectlib.h>
#include "AMutex.h"

package$ aet.util;



public$ class$ ACond{

	  private$ apointer p;
	  private$ auint i[2];
	  public$ void wait(AMutex *mutex);
	  public$ void signal();
	  public$ void  broadcast ();
	  public$ aboolean wait(AMutex *mutex,aint64  endTime);


};




#endif /* AMutex */

