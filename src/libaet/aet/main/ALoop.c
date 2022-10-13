#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../lang/AAssert.h"

#include "ALoop.h"

impl$ ALoop{

    ALoop (ASourceMgr *mgr,aboolean isRunning){
      if (!mgr)
          mgr = ASourceMgr.getDefault();
      self->mgr = mgr->ref();
      self->isRunning = isRunning != FALSE;
   }

    void run (){
      AThread *selfThread =AThread.current();
      //printf("ALoop 00 thread:%p\n",selfThread);
      if(!mgr->acquire()){
          aboolean gotOwnership = FALSE;
          /* 另外的线程正在使用这个管理器 */
          mgr->lock();
          a_atomic_int_set (&isRunning, TRUE);
          //printf("ALoop 11\n");

          while (a_atomic_int_get (&isRunning) && !gotOwnership)
              gotOwnership = mgr->wait();

          if(!a_atomic_int_get (&isRunning)){
              mgr->unlock();
              //printf("ALoop 22\n");
              if (gotOwnership)
                  mgr->release();
              return;
           }
           a_assert (gotOwnership);
      }else
          mgr->lock();
      //printf("g_main_loop_run 33\n");

      if (mgr->in_check_or_prepare){
          a_warning ("ALoop(): called recursively from within a source's "
             "check() or prepare() member, iteration not possible.");
          return;
      }
      //printf("ALoop 44\n");

      a_atomic_int_set (&isRunning, TRUE);
      while (a_atomic_int_get (&isRunning))
        mgr->iterate(TRUE, TRUE, selfThread);

      mgr->unlock();
      mgr->release();
    }

    void   quit(){
        mgr->lock();
        a_atomic_int_set (&isRunning, FALSE);
        mgr->wakupSignal();
        mgr->cond.broadcast ();
        mgr->unlock();
    }

    ASourceMgr *getSourceMgr(){
        return mgr;
    }

    aboolean isRunning (){
      return a_atomic_int_get (&isRunning);
    }

   ~ALoop(){
      // printf("free aloop %p\n",mgr);
	   if(mgr!=NULL){
	       mgr->removeAll();
	       mgr->unref();
	   }
    }


};
