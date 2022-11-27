#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define __USE_GNU
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <aet/lang/AString.h>
#include "APipe.h"


impl$ APipe{


    aboolean setError(AError **error,int savedErrno){
        a_error_set(error,0,0,AString.errToStr(savedErrno));
        errno = savedErrno;
        return FALSE;
    }

    aboolean  open(int flags,AError **error){
      int ecode;
      /* 仅支持FD_CLOEXEC */
      a_return_val_if_fail ((flags & (FD_CLOEXEC)) == flags, FALSE);
      {
        int pipe2_flags = 0;
        if (flags & FD_CLOEXEC)
          pipe2_flags |= O_CLOEXEC;
        /* Atomic */
        ecode = pipe2 (fds, pipe2_flags);
        if (ecode == -1 && errno != ENOSYS)
          return setError (error, errno);
        else if (ecode == 0)
          return TRUE;
        /* Fall through on -ENOSYS, we must be running on an old kernel */
      }
      ecode = pipe (fds);
      if (ecode == -1)
        return setError (error, errno);

      if (flags == 0)
        return TRUE;

      ecode = fcntl (fds[0], F_SETFD, flags);
      if (ecode == -1){
          int savedErrno = errno;
          close (fds[0]);
          close (fds[1]);
          return setError (error, savedErrno);
      }
      ecode = fcntl (fds[1], F_SETFD, flags);
      if (ecode == -1){
          int savedErrno = errno;
          close (fds[0]);
          close (fds[1]);
          return setError (error, savedErrno);
      }
      return TRUE;
    }

   aboolean setNonblocking(int fd,aboolean nonblock,AError **error){
       long fcntl_flags;
       fcntl_flags = fcntl (fd, F_GETFL);
       if (fcntl_flags == -1)
          return setError (error, errno);

       if (nonblock){
          fcntl_flags |= O_NONBLOCK;
        } else{
          fcntl_flags &= ~O_NONBLOCK;
        }
        if (fcntl (fd, F_SETFL, fcntl_flags) == -1)
          return setError (error, errno);
        return TRUE;
    }

    aboolean setNonblocking(aboolean nonblock,AError **error){
        aboolean re=setNonblocking(fds[0],nonblock,error);
        if(!re)
            return FALSE;
        return re=setNonblocking(fds[1],nonblock,error);
    }

    int  write(char *buf,asize  size,AError  **error){
        aboolean bWritten = FALSE;
        int writenLen = 0;
        int count = 0;
        int sendCount=0;
        static int maxbuffer=128*1024;
        while(1) {
           if(size<=maxbuffer)
                writenLen = write(fds[1], buf + count, size-count);
           else{
               int left=size-count;
               if(left>maxbuffer)
                   writenLen = write(fds[1], buf + count, maxbuffer);
               else
                   writenLen = write(fds[1], buf + count, left);
           }
           if (writenLen == -1){
               int errsv=errno;
               if (errno == EAGAIN){
                    // 对于nonblocking 的socket而言，这里说明了已经全部发送成功了
                     bWritten = TRUE;
                    break;
                } else if(errno == ECONNRESET){
                     // 对端重置,对方发送了RST
                     break;
                }else if (errno == EINTR){
                     // 被信号中断
                     a_warning("sendByNoblock EINTR continue:");
                     continue;
                 }else{
                    // 其他错误
                     setError (error, errsv);
                     break;
                }
            }

            if (writenLen == 0){
                 // 这里表示对端的socket已正常关闭.
                 break;
             }

             // 以下的情况是writenLen > 0
             count += writenLen;
             sendCount++;
             if (count<size){
                 // 可能还没有写完
                 a_debug("sendByNoblock 可能还没有写完 continue: write:%d size:%d sendCount:%d",(int)count,(int)size,sendCount);
                 continue;
             }else{
                 // 已经写完了
                 bWritten = TRUE;
                 break; // 退出while(1)
             }
         }

         if(bWritten){
           return count;
         }else{
           return -1;
         }
      }

      int read(char *buf,asize  size,AError  **error){
           int recvNum = 0;
           int count = 0;
           aboolean bReadOk = FALSE;
           int readCount=0;
           int maxSize=size;
           aboolean again=FALSE;
           while(1){
                // 确保sockfd是nonblocking的
                recvNum = read(fds[0], buf + count, maxSize);
                if(recvNum < 0){
                   int errsv =errno;
                   if(errno == EAGAIN){
                        // 由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可读
                        // 在这里就当作是该次事件已处理处.
                        bReadOk = TRUE;
                        break;
                    }else if (errno == ECONNRESET){
                           // 对方发送了RST
                        setError (error, errsv);
                        break;
                    }else if (errno == EINTR){
                       // 被信号中断
                      a_warning("revByBlock EINTR continue;");

                          continue;
                     }else{
                        //其他不可弥补的错误
                         setError (error, errsv);
                       break;
                   }
                }else if( recvNum == 0){
                    break;
                }
                count += recvNum;
                maxSize-=recvNum;
                readCount++;
                if(count==size){
                   a_warning("读满缓冲区了要处理 read count is readCount:%d count:%d size:%d",readCount,count,(int)size);
                    bReadOk=TRUE;
                    again=TRUE;
                    break;
                }

            }

            if(bReadOk){
               if(again){
                   int errsv =errno;
                   setError (error, errsv);
               }
               return count;
            }else{
              return -1;
            }
        }

};
