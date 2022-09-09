

#ifndef __A_LOG_H__
#define __A_LOG_H__

#include <stdarg.h>
#include "abase.h"

/*
 * 计算字符串大小，保证符合格式+参数.
 */
#define A_LOG_LEVEL_USER_SHIFT  (6)



typedef enum
{
  A_LOG_FLAG_RECURSION          = 1 << 0,
  A_LOG_FLAG_FATAL              = 1 << 1,
  /* NLib log levels */
  A_LOG_LEVEL_ERROR             = 1 << 2,
  A_LOG_LEVEL_WARNING           = 1 << 3,
  A_LOG_LEVEL_INFO              = 1 << 4,
  A_LOG_LEVEL_DEBUG             = 1 << 5,
  A_LOG_LEVEL_MASK              = ~(A_LOG_FLAG_RECURSION | A_LOG_FLAG_FATAL)
} ALogLevelFlags;

#define A_LOG_FATAL_MASK        (A_LOG_FLAG_RECURSION | A_LOG_LEVEL_ERROR)

typedef enum
{
  A_LOG_WRITER_HANDLED = 1,
  A_LOG_WRITER_UNHANDLED = 0,
} ALogWriterOutput;


typedef struct _ALogField ALogField;
struct _ALogField
{
  const achar *key;
  aconstpointer value;
  assize length;
};


void a_log_structured(ALogLevelFlags log_level,...);
void a_log_structured_standard (ALogLevelFlags log_level,const achar *object,const achar *file,const achar *line,const achar *func,const achar    *message_format,...) ;

void  a_log_set_domain(char *domain);
void  a_log_set_level(int level);
void  a_log_set_file_func_line(aboolean need);
void  a_log_append_domain(char *domain);
void  a_log_set_param_from_env();
void  a_log_set_use_stderr (aboolean use_stderr);
void  a_printerr(const achar *format,...);//往stderr
void  a_print(const achar *format,...); //往stdout
ALogLevelFlags  a_log_set_always_fatal  (ALogLevelFlags  fatal_mask);



#define a_error(format...)   A_STMT_START {                                          \
                               a_log_structured_standard (A_LOG_LEVEL_ERROR, \
                                                          __OBJECT__,__FILE__, A_STRINGIFY (__LINE__), \
                                                          A_STRFUNC, format); \
                               for (;;) ;                                            \
                             } A_STMT_END

#define a_warning(format...)  a_log_structured_standard (A_LOG_LEVEL_WARNING, \
														__OBJECT__,__FILE__, A_STRINGIFY (__LINE__), \
                                                         A_STRFUNC, format)
#define a_info(format...)     a_log_structured_standard (A_LOG_LEVEL_INFO, \
		                                               __OBJECT__,__FILE__, A_STRINGIFY (__LINE__), \
                                                         A_STRFUNC, format)
#define a_debug(format...)    a_log_structured_standard (A_LOG_LEVEL_DEBUG, \
		                                               __OBJECT__,__FILE__, A_STRINGIFY (__LINE__), \
                                                         A_STRFUNC, format)

extern void a_log_print_stack();//当进入_a_log_abort时，打印函数调用的栈 由ABackTrace类实现。
void        a_log_pause(); //当打印栈内容时，暂停
void        a_log_resume();


void a_return_if_fail_warning (const char *pretty_function,const char *expression);
void a_warn_message (const char  *file,int line,const char *func,const char     *warnexpr);

#define a_warning_once a_warning

#define a_warn_if_reached() \
  do { \
    a_warn_message (__FILE__, __LINE__, A_STRFUNC, NULL); \
  } while (0)


#define a_warn_if_fail(expr) \
  do { \
    if A_LIKELY (expr) ; \
    else a_warn_message (__FILE__, __LINE__, A_STRFUNC, #expr); \
  } while (0)



#define a_return_if_fail(expr) \
  A_STMT_START { \
    if (A_LIKELY (expr)) \
      { } \
    else \
      { \
        a_return_if_fail_warning (\
                                  A_STRFUNC, \
                                  #expr); \
        return; \
      } \
  } A_STMT_END

#define a_return_val_if_fail(expr, val) \
  A_STMT_START { \
    if (A_LIKELY (expr)) \
      { } \
    else \
      { \
        a_return_if_fail_warning (\
                                  A_STRFUNC, \
                                  #expr); \
        return (val); \
      } \
  } A_STMT_END

#define a_return_if_reached() \
  A_STMT_START { \
	  a_log_structured (\
			  A_LOG_LEVEL_WARNING, \
           "file %s: line %d (%s): should not be reached", \
           __FILE__, \
           __LINE__, \
           A_STRFUNC); \
    return; \
  } A_STMT_END

#define a_return_val_if_reached(val) \
  A_STMT_START { \
	  a_log_structured (\
           A_LOG_LEVEL_WARNING, \
           "file %s: line %d (%s): should not be reached", \
           __FILE__, \
           __LINE__, \
           A_STRFUNC); \
    return (val); \
  } A_STMT_END


#endif /* __A_MESSAGES_H__ */
