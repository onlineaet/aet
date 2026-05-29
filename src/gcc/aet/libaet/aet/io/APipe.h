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

#ifndef __AET_IO_A_PIPE_H__
#define __AET_IO_A_PIPE_H__

#include "../../aet.h"

package$ aet.io;


public$ class$ APipe{
      private$ int fds[2];
      public$ aboolean  open(int flags,AError **error);
      public$ aboolean  setNonblocking(aboolean nonblock,AError **error);
      public$ int        write(char *buf,asize  size,AError  **error);
      public$ int       read(char *buf,asize  size,AError  **error);

};



#endif /* __N_FILE_H__*/




