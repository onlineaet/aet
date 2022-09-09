/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/



#ifndef __N_LOG_H__
#define __N_LOG_H__


#include <stdarg.h>

#include "nbase.h"




/* calculate a string size, guaranteed to fit format + args.
 */


#define N_LOG_LEVEL_USER_SHIFT  (6)



typedef enum
{
  /* log flags */
  N_LOG_FLAG_RECURSION          = 1 << 0,
  N_LOG_FLAG_FATAL              = 1 << 1,
  /* NLib log levels */
  N_LOG_LEVEL_ERROR             = 1 << 2,       /* always fatal */
  N_LOG_LEVEL_WARNING           = 1 << 3,
  N_LOG_LEVEL_INFO              = 1 << 4,
  N_LOG_LEVEL_DEBUG             = 1 << 5,
  N_LOG_LEVEL_MASK              = ~(N_LOG_FLAG_RECURSION | N_LOG_FLAG_FATAL)
} NLogLevelFlags;

/* GLib log levels that are considered fatal by default */
#define N_LOG_FATAL_MASK        (N_LOG_FLAG_RECURSION | N_LOG_LEVEL_ERROR)


typedef enum
{
  N_LOG_WRITER_HANDLED = 1,
  N_LOG_WRITER_UNHANDLED = 0,
} NLogWriterOutput;


typedef struct _NLogField NLogField;
struct _NLogField
{
  const nchar *key;
  nconstpointer value;
  nssize length;
};


typedef NLogWriterOutput (*NLogWriterFunc)(NLogLevelFlags   log_level,const NLogField *fields,nsize n_fields,npointer user_data);


void             n_log_structured     (NLogLevelFlags log_level,...);
void             n_log_structured_standard (NLogLevelFlags log_level,const nchar *file,const nchar *line,const nchar *func,const nchar *message_format,...);

void             n_log_set_writer_func (NLogWriterFunc func,npointer user_data,NDestroyNotify user_data_free);
void             n_log_set_use_stderr (nboolean use_stderr);
void             n_log_set_domain(char *domain);
void             n_log_set_level(int level);
void             n_log_set_file_func_line(nboolean need);
void             n_log_set_param_from_env();
void             n_log_append_domain(char *compileFile);
nboolean         n_log_is_debug();
nboolean         n_log_is_print_file(char *fileStr);




#define n_error(format...)   N_STMT_START {                                          \
                               n_log_structured_standard (N_LOG_LEVEL_ERROR, \
                                                          __FILE__, N_STRINGIFY (__LINE__), \
                                                          N_STRFUNC, format); \
                               for (;;) ;                                            \
                             } N_STMT_END

#define n_warning(format...)  n_log_structured_standard (N_LOG_LEVEL_WARNING, \
                                                         __FILE__, N_STRINGIFY (__LINE__), \
                                                         N_STRFUNC, format)
#define n_info(format...)     n_log_structured_standard (N_LOG_LEVEL_INFO, \
                                                         __FILE__, N_STRINGIFY (__LINE__), \
                                                         N_STRFUNC, format)
#define n_debug(format...)    n_log_structured_standard (N_LOG_LEVEL_DEBUG, \
                                                         __FILE__, N_STRINGIFY (__LINE__), \
                                                         N_STRFUNC, format)



void n_return_if_fail_warning (const char *pretty_function,const char *expression);

void n_warn_message (const char  *file,int line,const char *func,const char     *warnexpr);

#define n_warninn_once n_warning

#define n_assert(expr)                  N_STMT_START { (void) 0; } N_STMT_END



#define n_warn_if_reached() \
  do { \
    n_warn_message (__FILE__, __LINE__, N_STRFUNC, NULL); \
  } while (0)


#define n_warn_if_fail(expr) \
  do { \
    if N_LIKELY (expr) ; \
    else n_warn_message (__FILE__, __LINE__, N_STRFUNC, #expr); \
  } while (0)



#define n_return_if_fail(expr) \
  N_STMT_START { \
    if (N_LIKELY (expr)) \
      { } \
    else \
      { \
        n_return_if_fail_warning (\
                                  N_STRFUNC, \
                                  #expr); \
        return; \
      } \
  } N_STMT_END

#define n_return_val_if_fail(expr, val) \
  N_STMT_START { \
    if (N_LIKELY (expr)) \
      { } \
    else \
      { \
        n_return_if_fail_warning (\
                                  N_STRFUNC, \
                                  #expr); \
        return (val); \
      } \
  } N_STMT_END

#define n_return_if_reached() \
  N_STMT_START { \
	  n_log_structured (\
			  N_LOG_LEVEL_WARNING, \
           "file %s: line %d (%s): should not be reached", \
           __FILE__, \
           __LINE__, \
           N_STRFUNC); \
    return; \
  } N_STMT_END

#define n_return_val_if_reached(val) \
  N_STMT_START { \
	  n_log_structured (\
           N_LOG_LEVEL_WARNING, \
           "file %s: line %d (%s): should not be reached", \
           __FILE__, \
           __LINE__, \
           N_STRFUNC); \
    return (val); \
  } N_STMT_END


#endif /* __N_MESSAGES_H__ */
