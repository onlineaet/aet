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

#ifndef __AET_MTCS_STREAM_H__
#define __AET_MTCS_STREAM_H__

#include "../../aet.h"

package$ aet.mtcs;


public$  abstract$ class$ MtcsStream{
   public$ static MtcsStream *buildStream();
   public$ static MtcsStream *buildStream(int devNum);
   public$ static MtcsStream *buildStream(int providerNum,int devNum);
   public$ static MtcsStream *buildStreamWithFlag(int flag);
   public$ static MtcsStream *buildStreamWithFlag(int devNum,int flag);
   public$ static MtcsStream *buildStreamWithFlag(int providerNum,int devNum,int flag);

   int providerNum;
   int devNum;
   void *stream;
   public$ abstract$ void sync();
   public$ void *getStream();

};

#endif /* __AET_MTCS_STREAM_H__ */

