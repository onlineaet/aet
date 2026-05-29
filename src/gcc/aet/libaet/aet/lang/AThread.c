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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#define __USE_GNU
#include <pthread.h>
#include "AThread.h"
#include "AString.h"
#include "AAssert.h"
#include "../time/Time.h"

static void unrefThread_cb (apointer data);
static AThreadSpecific     globalThreadSpecific ={NULL,(unrefThread_cb)};


#define posix_check_err(err, name) A_STMT_START{			\
  int error = (err); 							\
  if (error)	 		 		 			\
    a_error ("file %s: line %d (%s): error '%s' during '%s'",		\
           __FILE__, __LINE__, A_STRFUNC,				\
           AString.errToStr(error), name);					\
  }A_STMT_END

#define posix_check_cmd(cmd) posix_check_err (cmd, #cmd)


/**
 * 执行用户的回调
 */
static apointer userExcute_cb(AThread *thread)
{
  a_assert(thread);
  if (thread->name){
      pthread_setname_np (pthread_self (), thread->name);
  }
  thread->retVal = thread->func (thread->userData);
  return NULL;
}

/**
 * 内核对线程的设置。
 */
static apointer kernelExcute_cb(apointer data)
{
  AThread *thread=(AThread *)data;
  static aboolean printedWarning= FALSE;
  a_thread_specific_set(&globalThreadSpecific, thread);
  /* 如果有调试，必须先设置*/
  if (thread->schedSettingAttr){
      pid_t tid = 0;
      auint flags = 0;
      int res;
      int errsv;
      tid = (pid_t) syscall (SYS_gettid);
      res = syscall (SYS_sched_setattr, tid, thread->schedSettingAttr->attr, flags);
      errsv = errno;
      if (res == -1 && a_atomic_int_compare_and_exchange (&printedWarning, FALSE, TRUE)){
        a_error ("Failed to set scheduler settings: %s", AString.errToStr(errsv));
      }else if (res == -1){
        a_debug ("Failed to set scheduler settings: %s", AString.errToStr(errsv));
      }
      printedWarning = TRUE;
   }
   return userExcute_cb(thread);
}

static void unrefThread_cb (apointer thread)
{
	((AThread *)thread)->unref();
}

impl$ AThread{

    static aboolean  getShedulerSettings(ASchedSettings *schedulerSettings){
         a_return_val_if_fail (schedulerSettings != NULL, FALSE);
         pid_t tid;
         int res;
         /*FIXME:结构定义似乎无法引入
         *通过任何正常的系统头文件，它只在
         *内核标头。这就是为什么我们现在在这里硬编码56*/
         auint size = 56; /* Size as of Linux 5.3.9 */
         auint flags = 0;

         tid = (pid_t) syscall (SYS_gettid);
         schedulerSettings->attr = a_malloc0 (size);
         do{
               int errsv;
               res = syscall (SYS_sched_getattr, tid, schedulerSettings->attr, size, flags);
               errsv = errno;
               if (res == -1){
                   if (errsv == EAGAIN){
                     continue;
                   }else if (errsv == E2BIG){
                     a_assert (size < A_MAXINT);
                     size *= 2;
                     schedulerSettings->attr = a_realloc (schedulerSettings->attr, size);
                     /* Needs to be zero-initialized */
                     memset (schedulerSettings->attr, 0, size);
                   }else{
                     a_debug ("Failed to get thread scheduler attributes: %s", AString.errToStr(errsv));
                     a_free (schedulerSettings->attr);
                     return FALSE;
                   }
               }
         }while (res == -1);

         /*尝试在当前线程上设置它们，以查看是否存在任何系统策略在不允许这样做的地方*/
         res = syscall (SYS_sched_setattr, tid, schedulerSettings->attr, flags);
         if (res == -1){
             int errsv = errno;
             a_debug ("Failed to set thread scheduler attributes: %s", AString.errToStr(errsv));
             a_free (schedulerSettings->attr);
             return FALSE;
         }
         return TRUE;
    }

    /**
     * 调用此函数的当调线程将被内核调度不再使用CPU
     */
    static void  yield(){
      sched_yield ();
    }


    static void  exit(apointer retval){
        AThread *thread=current();
        if A_UNLIKELY (!thread->ours)
           a_error ("尝试退出线程，但当前线程并不是aet创建的。");
         thread->retVal = retval;
         pthread_exit (NULL);
    }

   /**
    * 暂停当前线程 微秒
    */
   static void   sleep (aulong microseconds){
      struct timespec request, remaining;
      request.tv_sec = microseconds / 1000000;
      request.tv_nsec = 1000 * (microseconds % 1000000);
      while (nanosleep (&request, &remaining) == -1 && errno == EINTR)
        request = remaining;
    }

    AThread(const achar *name,AThreadFunc func,apointer userData,auint stackSize,ASchedSettings *schedSettings){
		  pthread_attr_t attr;
		  aint ret;
		  a_atomic_int_inc (&AThread.createdCount);
		  self->ours = TRUE;
		  self->ref();
		  self->joined=FALSE;
		  self->joinable = TRUE;
		  self->func = func;
		  self->userData = userData;
		  self->name = a_strdup (name);
		  self->schedSettingAttr = schedSettings;
		  posix_check_cmd(pthread_attr_init (&attr));
		  if (stackSize){
		#ifdef _SC_THREAD_STACK_MIN
			  long min_stack_size = sysconf (_SC_THREAD_STACK_MIN);
			  if (min_stack_size >= 0)
				  stackSize = MAX ((aulong) min_stack_size, stackSize);
		#endif /* _SC_THREAD_STACK_MIN */
			  /*此处没有错误检查，因为某些系统无法执行此操作
			  *我们只是不希望线程因此而失败*/
			  pthread_attr_setstacksize (&attr, stackSize);
		  }
		  if (!schedSettingAttr){
			  /* 虽然这是默认设置，但最好明确说明 */
			  pthread_attr_setinheritsched (&attr, PTHREAD_INHERIT_SCHED);
		  }
		  ret = pthread_create (&self->systemThread, &attr, kernelExcute_cb, (apointer)self);
		  posix_check_cmd (pthread_attr_destroy (&attr));
		  if (ret == EAGAIN){
			  a_warning("创建线程出错。%s",AString.errToStr(ret));
			  return NULL;
		  }
		  posix_check_err(ret, "pthread_create");
		  self->lock=new$ AMutex();
    }

    AThread(const achar *name,AThreadFunc func,apointer userData){
    	self(name,func,userData,0,NULL);
    }

    /**
     * 获取当前线程
     */
    static AThread *current(){
    	  AThread* thread = a_thread_specific_get (&globalThreadSpecific);
    	  if (!thread){
    		  printf("当前线程，不是aet系统创建的线程。\n");
    	      thread =new$ AThread();
    		  a_thread_specific_set(&globalThreadSpecific,thread);
    	  }
    	  return thread;
    }

    void wait(){
      lock.lock();
      if (!joined){
          posix_check_cmd (pthread_join(systemThread, NULL));
          joined = TRUE;
      }
      lock.unlock();
    }

    apointer join(){
      apointer retval;
      if(!ours)
    	  return NULL;
      wait();
      retval = self->retVal;
      /* 为了确保不再使用这个 */
      joinable = FALSE;
      return retval;
    }

     unsigned long int getSystemThread(){
         return systemThread;
     }

     public$ void setBindCpu(int bindCpu){
         self->bindCpu=bindCpu;
     }

     public$ int getBindCpu(){
        return  self->bindCpu;
     }



    ~AThread(){
      if(ours && !joined){
          pthread_detach (systemThread);
      }
      if(name)
          a_free(name);
    }

};
