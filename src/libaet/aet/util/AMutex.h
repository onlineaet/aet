

#ifndef __AET_UTIL_A_MUTEX_H__
#define __AET_UTIL_A_MUTEX_H__

#include <stdarg.h>
#include <objectlib.h>

package$ aet.util;



public$ class$ AMutex{

	  private$ apointer p;
	  private$ auint i[2];
	  public$ void lock();
	  public$ void unlock();
	  public$ aboolean trylock ();
};




#endif /* AMutex */

