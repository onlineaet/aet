

#ifndef __AET_LANG_A_THREAD_H__
#define __AET_LANG_A_THREAD_H__

#include <stdarg.h>
#include <objectlib.h>
#include "aet/util/AMutex.h"

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

    public$ AThread(const achar *name,AThreadFunc func,apointer userData,auint stackSize,ASchedSettings *schedSettings);
    public$ AThread(const achar *name,AThreadFunc func,apointer userData);
    /**
     * 在调用join方法的线程中调用join方法，表示在调用join方法的线程要等join所在的AThread线程接行完成
     */
    public$ apointer join();
    public$  void    wait();
    public$  unsigned long int getSystemThread();

};




#endif /* __N_MEM_H__ */


