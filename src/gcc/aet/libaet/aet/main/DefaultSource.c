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

#include "DefaultSource.h"

impl$ DefaultSource{

    DefaultSource(char *name){
        super$(name);
    }

    DefaultSource(EventSourceFuncs *funcs){
        super$(funcs);
    }

    int getFd(){
        return -1;
    }

    int getEvents(){
        return -1;
    }

    aboolean  prepare(int *timeout){
        if(funcs &&  funcs->prepare)
            return funcs->prepare(self,timeout);
      return FALSE;
    }

    aboolean  check(){
        if(funcs && funcs->check)
            return funcs->check(self);
        return FALSE;

    }

    aboolean  dispatch(){
        if(funcs && funcs->dispatch)
            return funcs->dispatch(self);
        return TRUE;
    }

};


