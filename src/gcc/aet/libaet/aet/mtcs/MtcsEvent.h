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

#ifndef __AET_MTCS_EVENT_H__
#define __AET_MTCS_EVENT_H__


#include "../../aet.h"

package$ aet.mtcs;



public$  class$ MtcsEvent{
   public$ static enum$ EventType{
       MALLOC,
   };
   public$ EventType eventType;
   public$ int providerNum;
   public$ int devNum;
   public$ unsigned long address;
   public$ auint64 size;
   public$ MtcsEvent(EventType eventType,int providerNum,int devNum);

};

#endif /* __N_MEM_H__ */

