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

#ifndef __AET_CUDA_STREAM_H__
#define __AET_CUDA_STREAM_H__

#include <cuda_runtime.h>
#include "../../../aet.h"
#include "../MtcsStream.h"

package$ aet.mtcs.cuda;


public$  class$ CudaStream extends$ MtcsStream{

   CudaStream(int devNum);
   CudaStream(int devNum,int flag);

};

#endif /* __AET_CUDA_STREAM_H__ */

