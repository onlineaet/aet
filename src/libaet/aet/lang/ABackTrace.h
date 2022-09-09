

#ifndef __AET_LANG_A_BACK_TRACE_H__
#define __AET_LANG_A_BACK_TRACE_H__

#include <stdarg.h>
#include <objectlib.h>

package$ aet.lang;

/**
 * 打印栈中函数调用序列。
 * 当调用a_error时，会触发调用a_log_print_stack
 * 然后调到类ABackTrace中的backtrace方法。
 * 在类中注册了监听信号SIGSEGV(著名的段错误)，当发生segament fault时也会调用backtrace().
 * 如果用户调用a_log_print_stack也是可以的。
 * 要打印出栈中的行数和文件名。与编译参数据有关。
 * 不能有 -O1 O2 O3 只能时O0 不能调用压缩命令strip。
 * 必须加入 -g --rdynamic
 */

public$ class$ ABackTrace{
      auint64 *mapsAddress;
      auint len;
      void backtrace();
      private$ ABackTrace();
};




#endif /* __N_MEM_H__ */

