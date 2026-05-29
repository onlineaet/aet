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

#ifndef __AET_LANG_A_THREAD_H__
#define __AET_LANG_A_THREAD_H__

#include "../../aet.h"
#include "../util/AMutex.h"

package$ aet.lang;

typedef apointer (*AThreadFunc) (apointer data);


typedef struct
{
  struct sched_attr *attr;
} ASchedSettings;

public$ class$ AThread{
    public$  static final$ int MIN_PRIORITY=0;
    public$  static final$ int NORMAL_PRIORITY=1;
    public$  static final$ int MAX_PRIORITY=2;
    public$  static final$ int URGENT_PRIORITY=3;
    private$ static volatile int createdCount=0;//总的创建线程数
    public$  static AThread *current();//获取当前线程
    public$  static void  exit(apointer retval);//的当前线程中退出当前线程
    public$  static void  yield();//调用此函数的当调线程将被内核调度不再使用CPU
    public$  static void  sleep (aulong microseconds);
    public$  static aboolean  getShedulerSettings(ASchedSettings *schedulerSettings);
	private$ AThreadFunc func;
    private$ apointer    userData;
    private$ aboolean    joinable;
    private$ int         priority;
    private$ aboolean    ours;
    private$ char       *name;
    private$ apointer    retVal;
    private$ pthread_t   systemThread;
    private$ aboolean    joined;
    private$ AMutex      lock;
    private$ ASchedSettings *schedSettingAttr; //调用内核的调度功能。
    private$ int bindCpu;
    public$ AThread(const achar *name,AThreadFunc func,apointer userData,auint stackSize,ASchedSettings *schedSettings);
    public$ AThread(const achar *name,AThreadFunc func,apointer userData);
    /**
     * 在调用join方法的线程中调用join方法，表示在调用join方法的线程要等join所在的AThread线程接行完成
     */
    public$ apointer join();
    public$  void    wait();
    public$  unsigned long int getSystemThread();
    public$ void     setBindCpu(int bindCpu);
    public$ int      getBindCpu();
};




#endif /* __N_MEM_H__ */


