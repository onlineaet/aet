#ifndef __AET_IO_A_PIPE_H__
#define __AET_IO_A_PIPE_H__

#include <stdarg.h>
#include <objectlib.h>

package$ aet.io;


public$ class$ APipe{
      private$ int fds[2];
      public$ aboolean  open(int flags,AError **error);
      public$ aboolean  setNonblocking(aboolean nonblock,AError **error);
      public$ int        write(char *buf,asize  size,AError  **error);
      public$ int       read(char *buf,asize  size,AError  **error);

};



#endif /* __N_FILE_H__*/




