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

#include "IdleSource.h"




impl$ IdleSource{

    IdleSource(char *name){
        super$(name);
        setPriority(Priority.DEFAULT_IDLE);
    }

    IdleSource(EventSourceFuncs *funcs){
        super$(funcs);
        setPriority(Priority.DEFAULT_IDLE);
        setName("IdleSource");
    }

    IdleSource(){
        self("IdleSource");
    }

    int getFd(){
        return -1;
    }

    int getEvents(){
        return -1;
    }


    aboolean  prepare(int *timeout){
        *timeout = 0;
        return TRUE;
    }

    aboolean check(){
        return TRUE;
    }

    aboolean  dispatch(){
         aboolean again;
         if (!callback){
             a_warning ("IdleSource 没有回调函数。");
             return FALSE;
         }
         again = callback (callbackUserData);
         return again;
    }

};


