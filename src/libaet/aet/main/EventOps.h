
#ifndef __AET_MAIN_EVENT_OPS_H__
#define __AET_MAIN_EVENT_OPS_H__

#include <alib.h>
#include <objectlib.h>


package$ aet.main;




public$ interface$ EventOps{

    aboolean  prepare(int *timeout);
    aboolean  check();
    aboolean  dispatch();
};


#endif /* __N_MEM_H__ */

