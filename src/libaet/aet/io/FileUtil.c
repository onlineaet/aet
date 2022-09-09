
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "FileUtil.h"
#include "../lang/AString.h"
#include "../lang/AAssert.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

impl$ FileUtil{

    static aboolean  getContentsRegfile(const char *filename,struct stat  *stat_buf,int fd,
            char **contents,asize *length,AError  **error){
      char *buf;
      asize bytes_read;
      asize size;
      asize alloc_size;

      size = stat_buf->st_size;

      alloc_size = size + 1;
      buf = a_try_malloc (alloc_size);

      if (buf == NULL){
          a_error_printf(error,0,1,"读文件%s失败，因为不能分配足够的内存。%lu",filename,alloc_size);
          goto error;
      }

      bytes_read = 0;
      while (bytes_read < size){
          assize rc;
          rc = read (fd, buf + bytes_read, size - bytes_read);
          if (rc < 0){
              if (errno != EINTR){
                  int saveErrno = errno;
                  a_free (buf);
                  a_error_printf(error,0,1,"读文件%s失败，原因:%s",filename,AString.errToStr(saveErrno));
                  goto error;
              }
          }else if (rc == 0)
            break;
          else
            bytes_read += rc;
       }
      buf[bytes_read] = '\0';
      if (length)
        *length = bytes_read;
      *contents = buf;
      close (fd);
      return TRUE;

     error:
      close (fd);
      return FALSE;
   }

    static aboolean getContentsStdio(char *filename,FILE *f,char **contents,asize *length,AError **error){
      char buf[4096];
      asize bytes;  /* 总是 <= sizeof(buf) */
      char *str = NULL;
      asize totalBytes = 0;
      asize totalAllocated = 0;
      char *tmp;
      char *display_filename;

      a_assert (f!= NULL);

      while (!feof (f)){
          int saveErrno;
          bytes = fread (buf, 1, sizeof (buf), f);
          saveErrno = errno;
          if (totalBytes > A_MAXSIZE - bytes)
              goto file_too_large;

          /* 上面已经排除了溢出的可能性。*/
          while (totalBytes + bytes >= totalAllocated){
              if (str){
                  if (totalAllocated > A_MAXSIZE / 2)
                      goto file_too_large;
                  totalAllocated *= 2;
              }else{
                  totalAllocated = MIN (bytes + 1, sizeof (buf));
              }
              tmp = a_try_realloc (str, totalAllocated);
              if (tmp == NULL){
                  a_error_printf(error,0,1,"读文件%s失败，因为不能分配足够的内存。%lu",filename,totalAllocated);
                  goto error;
              }
              str = tmp;
          }

          if (ferror (f)){
             a_error_printf(error,0,1,"读文件%s失败，原因:%s",filename,AString.errToStr(saveErrno));
              goto error;
          }

          a_assert (str != NULL);
          memcpy (str + totalBytes, buf, bytes);
          totalBytes += bytes;
      }

      fclose (f);

      if (totalAllocated == 0){
          str = a_new (char, 1);
          totalBytes = 0;
      }
      str[totalBytes] = '\0';
      if (length)
        *length = totalBytes;
      *contents = str;
      return TRUE;

     file_too_large:
        a_error_printf(error,0,1,"读文件太大:%s",filename);
     error:
       a_free (str);
       fclose (f);
       return FALSE;
    }

   static aboolean getContents(char  *filename,char  **contents,asize *length,AError **error){
       a_return_val_if_fail (filename != NULL, FALSE);
       a_return_val_if_fail (contents != NULL, FALSE);
       *contents = NULL;
       if (length)
        *length = 0;
       struct stat stat_buf;
       int fd;
       fd = open (filename, O_RDONLY|O_BINARY);
       if (fd < 0){
           int saved_errno = errno;
           a_error_printf(error,0,saved_errno,"Failed to open file “%s”: %s",filename,AString.errToStr(saved_errno));
           return FALSE;
       }

       if (fstat (fd, &stat_buf) < 0){
           int saved_errno = errno;
           a_error_printf(error,0,saved_errno,"Failed to get attributes of file “%s”: fstat() failed: %s",filename,AString.errToStr(saved_errno));
           close (fd);
           return FALSE;
       }

       if (stat_buf.st_size > 0 && S_ISREG (stat_buf.st_mode)){
           aboolean retval = getContentsRegfile (filename,&stat_buf,fd,contents,length,error);
           return retval;
       }else{
           FILE *f;
           aboolean retval;
           f = fdopen (fd, "r");
           if (f == NULL){
               int saved_errno = errno;
               a_error_printf(error,0,saved_errno,"Failed to open file “%s”: fdopen() failed: %s",filename,AString.errToStr(saved_errno));
               return FALSE;
           }
           retval = getContentsStdio (filename, f, contents, length, error);
           return retval;
       }
    }

};

