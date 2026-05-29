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
#include <errno.h>

#include "EventSource.h"

impl$ EventSource{

    EventSource(char *name){
       if(name!=NULL)
          self->name=a_strdup(name);
        priority=Priority.DEFAULT;
        flags = HookFlagMask.ACTIVE;
        //printf("flags is ---%d %d\n",flags,(flags & HookFlagMask.ACTIVE));
        ready_time = -1;
        eventMgr=NULL;
        havePoll=FALSE;
    }

    EventSource(EventSourceFuncs *funcs){
         self(NULL);
         self->funcs=funcs;
     }


    EventSource(){
         self(NULL);
     }

     void setId(auint id){
        self->id=id;
     }

     auint getId(){
         return id;
     }

     int   getPriority(){
         return priority;
     }

     void  setPriority(int newPrio){
         if(self->priority==newPrio)
             return;
         if(self->eventMgr)
             eventMgr->setPriority((apointer)self,newPrio);
         self->priority=newPrio;
     }


     aboolean  isActive(){
        return (flags & EventSource.HookFlagMask.ACTIVE) != 0;
     }

     aboolean canRecurse(){
       return (flags & Flags.CAN_RECURSE) != 0;
     }

     char *getName(){
        return name;
     }

     void  setCanRecurse(aboolean can){

         if (can)
           flags |= Flags.CAN_RECURSE;
         else
           flags &= ~Flags.CAN_RECURSE;
     }

     void setName(char *name){
         if(self->name){
             a_free(self->name);
             self->name=NULL;
         }
         if(name)
             self->name=a_strdup(name);
     }

     void  setEventMgr(EventMgr *eventMgr){
         self->eventMgr=eventMgr->ref();
     }

     void  setFuncs (EventSourceFuncs *funcs){
        a_return_if_fail (funcs != NULL);
        self->funcs = funcs;
     }

     void  destroy (){
        // printf("destroy destroyFunc self:%p\n",self);
         if(self->eventMgr){
             eventMgr->destroy((apointer)self);
         }else
             flags &= ~HookFlagMask.ACTIVE;
     }


     void setCallback(EventSourceCallback callback,apointer  data,ADestroyNotify  notify){
         self->callback=callback;
         self->callbackUserData=data;
         self->notify=notify;

     }

     aboolean havePoll(){
         return havePoll;
     }

     void setReadyTime(aint64 readyTime){
         if(self->eventMgr)
             eventMgr->setReadyTime((apointer)self,readyTime);
         else
             self->ready_time = readyTime;
     }

     aint64 getReadyTime(){
       return ready_time;
     }

     aint64   getTime(){
        if(eventMgr==NULL)
           return 0;
        return eventMgr->getTime();
     }

     aboolean isDestroyed(){
        aboolean is=(flags & HookFlagMask.ACTIVE) == 0;
//        if(is && eventMgr!=NULL){
//            a_warning("isDestroyed -----eventMgr没有设为空！！！\n");
//        }
        return is;
     }

     ~EventSource(){
        // printf("EventSource free---%p\n",self);
         if(self->eventMgr)
             eventMgr->unref();
         if(name)
             a_free(name);
     }
};


