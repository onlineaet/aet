/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */

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
