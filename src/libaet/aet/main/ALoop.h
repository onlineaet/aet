

#ifndef __AET_MAIN_A_LOOP_H__
#define __AET_MAIN_A_LOOP_H__

#include <stdarg.h>
#include <objectlib.h>
#include "ASourceMgr.h"

package$ aet.main;


public$ class$ ALoop{

    private$ ASourceMgr *mgr;
    private$ aboolean isRunning; /* 原子操作 */
    public$ void       run();
    public$ void       quit();
    public$ ASourceMgr *getSourceMgr();
    public$ ALoop(ASourceMgr *mgr,aboolean isRunning);
    public$ aboolean isRunning();

};




#endif /* __N_MEM_H__ */

