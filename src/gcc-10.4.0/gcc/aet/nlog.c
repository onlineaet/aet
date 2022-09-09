/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include "nlog.h"
#include "nmem.h"
#include "nprintf.h"
#include "nstrfuncs.h"
#include "natomic.h"
#include "nstring.h"
#include "nconvert.h"
#include "ncharset.h"


static NLogWriterOutput  defaultWriter(NLogLevelFlags log_level,const NLogField *fields, nsize n_fields, npointer user_data);
static void              structuredArray(NLogLevelFlags log_level,const NLogField *fields,nsize n_fields);
static NLogWriterOutput  standardStreams (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields,npointer user_data);
static nboolean          shouldDropMessage (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields);


/* --- variables --- */
//static NMutex         n_messages_lock;
//static NThreadSpecific n_log_structured_depth;
static NLogWriterFunc log_writer_func = defaultWriter;
static npointer       log_writer_user_data = NULL;
static NDestroyNotify log_writer_user_data_free = NULL;
static NLogLevelFlags n_log_msg_prefix = N_LOG_LEVEL_ERROR | N_LOG_LEVEL_WARNING ;//| N_LOG_LEVEL_DEBUG;
static NLogLevelFlags n_log_always_fatal = N_LOG_FATAL_MASK;


/* string size big enough to hold level prefix */
#define	STRING_BUFFER_SIZE	(FORMAT_UNSIGNED_BUFSIZE + 32)

#define	ALERT_LEVELS		(N_LOG_LEVEL_ERROR |  N_LOG_LEVEL_WARNING)
/* these are emitted by the default log handler */
#define DEFAULT_LEVELS (N_LOG_LEVEL_ERROR |  N_LOG_LEVEL_WARNING | N_LOG_LEVEL_INFO)
/* these are filtered by N_MESSAGES_DEBUG by the default log handler */
#define INFO_LEVELS (N_LOG_LEVEL_INFO | N_LOG_LEVEL_DEBUG)



static nboolean gmessages_use_stderr = FALSE;
static char *defaultDomain=NULL;
static char *appendDomain=NULL;
static nboolean need_FILE_FUN_LINE=FALSE;

static const nchar *log_level_to_color (NLogLevelFlags log_level,nboolean       use_color);
static const nchar *color_reset        (nboolean       use_color);

/* --- functions --- */

#define N_BREAKPOINT()        N_STMT_START{ __asm__ __volatile__ ("int $03"); }N_STMT_END


static void _n_log_abort (nboolean breakpoint);

static void _n_log_abort (nboolean breakpoint)
{
  nboolean debugger_present;
  /* Assume GDB is attached. */
  debugger_present = TRUE;
  if (debugger_present && breakpoint)
    N_BREAKPOINT();
  else
    n_abort ();
}


#define CHAR_IS_SAFE(wc) (!((wc < 0x20 && wc != '\t' && wc != '\n' && wc != '\r') || \
			    (wc == 0x7f) || \
			    (wc >= 0x80 && wc < 0xa0)))
     
static nchar* strdup_convert (const nchar *string,const nchar *charset)
{
   if (!n_utf8_validate (string, -1, NULL)){
      NString *gstring = n_string_new ("[Invalid UTF-8] ");
      nuchar *p;
      for (p = (nuchar *)string; *p; p++){
	     if (CHAR_IS_SAFE(*p) && !(*p == '\r' && *(p + 1) != '\n') && *p < 0x80)
	        n_string_append_c (gstring, *p);
	    else
	        n_string_append_printf (gstring, "\\x%02x", (nuint)(nuchar)*p);
	  }
      return n_string_free (gstring, FALSE);
  }else{
      NError *err = NULL;
      nchar *result = n_convert_with_fallback (string, -1, charset, "UTF-8", "?", NULL, NULL, &err);
      if (result)
	     return result;
      else{
	  /* Not thread-safe, but doesn't matter if we print the warning twice
	   */
	    static nboolean warned = FALSE;
	    if (!warned){
	        warned = TRUE;
	        _n_fprintf (stderr, "NLib: Cannot convert message: %s\n", err->message);
	    }
	    n_error_free (err);
	    return n_strdup (string);
	  }
   }
}

/* For a radix of 8 we need at most 3 output bytes for 1 input
 * byte. Additionally we might need up to 2 output bytes for the
 * readix prefix and 1 byte for the trailing NULL.
 */
#define FORMAT_UNSIGNED_BUFSIZE ((NLIB_SIZEOF_LONG * 3) + 3)

static void format_unsigned (nchar  *buf,nulong  num,nuint   radix)
{
  nulong tmp;
  nchar c;
  nint i, n;

  /* we may not call _any_ GLib functions here (or macros like n_return_if_fail()) */

  if (radix != 8 && radix != 10 && radix != 16)
    {
      *buf = '\000';
      return;
    }
  
  if (!num)
    {
      *buf++ = '0';
      *buf = '\000';
      return;
    } 
  
  if (radix == 16)
    {
      *buf++ = '0';
      *buf++ = 'x';
    }
  else if (radix == 8)
    {
      *buf++ = '0';
    }
	
  n = 0;
  tmp = num;
  while (tmp)
    {
      tmp /= radix;
      n++;
    }

  i = n;

  /* Again we can't use n_assert; actually this check should _never_ fail. */
  if (n > FORMAT_UNSIGNED_BUFSIZE - 3)
    {
      *buf = '\000';
      return;
    }

  while (num)
    {
      i--;
      c = (num % radix);
      if (c < 10)
	buf[i] = c + '0';
      else
	buf[i] = c + 'a' - 10;
      num /= radix;
    }
  
  buf[n] = '\000';
}



void n_log_set_use_stderr (nboolean use_stderr)
{
  //n_return_if_fail (n_thread_n_created () == 0);
  gmessages_use_stderr = use_stderr;
}

static FILE *mklevel_prefix (nchar level_prefix[STRING_BUFFER_SIZE],NLogLevelFlags log_level,nboolean use_color)
{
  nboolean to_stdout = !gmessages_use_stderr;
  /* we may not call _any_ NLib functions here */
  strcpy (level_prefix, log_level_to_color (log_level, use_color));
  switch (log_level & N_LOG_LEVEL_MASK)
    {
    case N_LOG_LEVEL_ERROR:
      strcat (level_prefix, "ERROR");
      to_stdout = FALSE;
      break;
    case N_LOG_LEVEL_WARNING:
      strcat (level_prefix, "WARNING");
      to_stdout = FALSE;
      break;
    case N_LOG_LEVEL_INFO:
      strcat (level_prefix, "INFO");
      break;
    case N_LOG_LEVEL_DEBUG:
      strcat (level_prefix, "DEBUG");
      break;
    default:
      if (log_level)
	{
	  strcat (level_prefix, "LOG-");
	  format_unsigned (level_prefix + 4, log_level & N_LOG_LEVEL_MASK, 16);
	}
      else
	strcat (level_prefix, "LOG");
      break;
    }

  strcat (level_prefix, color_reset (use_color));
  if (log_level & N_LOG_FLAG_RECURSION)
    strcat (level_prefix, " (recursed)");
  if (log_level & ALERT_LEVELS)
    strcat (level_prefix, " **");
  return to_stdout ? stdout : stderr;
}




/* Return value must be 1 byte long (plus nul byte).
 * Reference: http://man7.org/linux/man-pages/man3/syslog.3.html#DESCRIPTION
 */
static const nchar *log_level_to_priority (NLogLevelFlags log_level)
{
  if (log_level & N_LOG_LEVEL_ERROR)
    return "3";
  else if (log_level & N_LOG_LEVEL_WARNING)
    return "4";
  else if (log_level & N_LOG_LEVEL_INFO)
    return "5";
  else if (log_level & N_LOG_LEVEL_DEBUG)
    return "6";

  /* Default to LON_NOTICE for custom log levels. */
  return "5";
}

static FILE *log_level_to_file (NLogLevelFlags log_level)
{
  if (gmessages_use_stderr)
    return stderr;
  if (log_level & (N_LOG_LEVEL_ERROR | N_LOG_LEVEL_WARNING | N_LOG_LEVEL_INFO))
    return stderr;
  else
    return stdout;
}

static const nchar *log_level_to_color (NLogLevelFlags log_level,nboolean use_color)
{
  /* we may not call _any_ GLib functions here */
  if (!use_color)
    return "";
  if (log_level & N_LOG_LEVEL_ERROR)
    return "\033[1;31m"; /* red */
  else if (log_level & N_LOG_LEVEL_WARNING)
    return "\033[1;33m"; /* yellow */
  else if (log_level & N_LOG_LEVEL_INFO)
    return "\033[1;32m"; /* green */
  else if (log_level & N_LOG_LEVEL_DEBUG)
    return "\033[1;32m"; /* green */
  return "";
}

static const nchar *color_reset (nboolean use_color)
{
  /* we may not call _any_ GLib functions here */
  if (!use_color)
    return "";
  return "\033[0m";
}



//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wformat-nonliteral"


void n_log_structured (NLogLevelFlags  log_level,...)
{
  va_list args;
  nchar buffer[1025], *message_allocated = NULL;
  const char *format;
  const nchar *message;
  npointer p;
  nsize n_fields, i;
  NLogField stack_fields[16];
  NLogField *fields = stack_fields;
  NLogField *fields_allocated = NULL;
 // NArray *array = NULL;
  char *array=n_malloc(sizeof(NLogField)*100);
  int arrayCount=0;

  va_start (args, log_level);

  /* MESSAGE and PRIORITY are a given */
  n_fields = 2;

  for (p = va_arg (args, nchar *), i = n_fields;strcmp (p, "MESSAGE") != 0;p = va_arg (args, nchar *), i++){
      NLogField field;
      const nchar *key = p;
      nconstpointer value = va_arg (args, npointer);

      field.key = key;
      field.value = value;
      field.length = -1;

      if (i < 16)
        stack_fields[i] = field;
      else{
          /* Don't allow dynamic allocation, since we're likely
           * in an out-of-memory situation. For lack of a better solution,
           * just ignore further key-value pairs.
           */
          if (log_level & N_LOG_FLAG_RECURSION)
            continue;

          if (i == 16){
              //array = n_array_sized_new (FALSE, FALSE, sizeof (NLogField), 32);
              //n_array_append_vals (array, stack_fields, 16);
              memcpy(array,stack_fields,16*sizeof(NLogField));
              arrayCount=16;
          }
          memcpy(array+arrayCount*sizeof(NLogField),&field,sizeof(NLogField));
          arrayCount++;
         // n_array_append_val (array, field);
      }
   }

  n_fields = i;

 // if (array)
  //  fields = fields_allocated = (NLogField *) n_array_free (array, FALSE);

  if (arrayCount>0)
     fields = fields_allocated = (NLogField *) array;

  format = va_arg (args, nchar *);

  if (log_level & N_LOG_FLAG_RECURSION)
    {
      /* we use a stack buffer of fixed size, since we're likely
       * in an out-of-memory situation
       */
      nsize size N_GNUC_UNUSED;

      size = _n_vsnprintf (buffer, sizeof (buffer), format, args);
      message = buffer;
    }
  else
    {
      message = message_allocated = n_strdup_vprintf (format, args);
    }

  /* Add MESSAGE, PRIORITY and NLIB_DOMAIN. */
  fields[0].key = "MESSAGE";
  fields[0].value = message;
  fields[0].length = -1;

  fields[1].key = "PRIORITY";
  fields[1].value = log_level_to_priority (log_level);
  fields[1].length = -1;

  /* Log it. */
  structuredArray (log_level, fields, n_fields);
  n_free (fields_allocated);
  n_free (message_allocated);
  va_end (args);
}


//#pragma GCC diagnostic pop

static void write_string (FILE *stream, const nchar *string)
{
  fputs (string, stream);
}

static void write_string_sized (FILE *stream,const nchar *string,nssize       length)
{
  /* Is it nul-terminated? */
  if (length < 0)
    write_string (stream, string);
  else
    fwrite (string, 1, length, stream);
}

static NLogWriterOutput _n_log_writer_fallback (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields,npointer user_data)
{
  FILE *stream;
  nsize i;

  /* we cannot call _any_ GLib functions in this fallback handler,
   * which is why we skip UTF-8 conversion, etc.
   * since we either recursed or ran out of memory, we're in a pretty
   * pathologic situation anyways, what we can do is giving the
   * the process ID unconditionally however.
   */

  stream = log_level_to_file (log_level);

  for (i = 0; i < n_fields; i++)
    {
      const NLogField *field = &fields[i];

      /* Only print fields we definitely recognise, otherwise we could end up
       * printing a random non-string pointer provided by the user to be
       * interpreted by their writer function.
       */
      if (strcmp (field->key, "MESSAGE") != 0 &&
          strcmp (field->key, "MESSAGE_ID") != 0 &&
          strcmp (field->key, "PRIORITY") != 0 &&
          strcmp (field->key, "CODE_FILE") != 0 &&
          strcmp (field->key, "CODE_LINE") != 0 &&
          strcmp (field->key, "CODE_FUNC") != 0 &&
          strcmp (field->key, "ERRNO") != 0 &&
          strcmp (field->key, "SYSLOG_FACILITY") != 0 &&
          strcmp (field->key, "SYSLOG_IDENTIFIER") != 0 &&
          strcmp (field->key, "SYSLOG_PID") != 0 &&
          strcmp (field->key, "NLIB_DOMAIN") != 0)
        continue;

      write_string (stream, field->key);
      write_string (stream, "=");
      write_string_sized (stream, field->value, field->length);
    }

  {
    nchar pid_string[FORMAT_UNSIGNED_BUFSIZE];

    format_unsigned (pid_string, getpid (), 10);
    write_string (stream, "_PID=");
    write_string (stream, pid_string);
  }

  return N_LOG_WRITER_HANDLED;
}
static int n_log_structured_depth_count=0;
static void structuredArray (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields)
{
  NLogWriterFunc writer_func;
  npointer writer_user_data;
  nboolean recursion;
  nuint depth;
  if (n_fields == 0)
    return;

  /* Check for recursion and look up the writer function. */
  depth =n_log_structured_depth_count;// NPOINTER_TO_UINT (n_thread_specific_get (&n_log_structured_depth));
  recursion = (depth > 0);

  //n_mutex_lock (&n_messages_lock);
  writer_func = recursion ? _n_log_writer_fallback : log_writer_func;
  writer_user_data = log_writer_user_data;
  //n_mutex_unlock (&n_messages_lock);

  /* Write the log entry. */
 // n_thread_specific_set (&n_log_structured_depth, NUINT_TO_POINTER (++depth));
  n_log_structured_depth_count=depth;
  n_assert (writer_func != NULL);
  writer_func (log_level, fields, n_fields, writer_user_data);
  //n_thread_specific_set (&n_log_structured_depth, NUINT_TO_POINTER (--depth));
  n_log_structured_depth_count=depth;

  /* Abort if the message was fatal. */
  if (log_level & N_LOG_FATAL_MASK){
    _n_log_abort (!(log_level & N_LOG_FLAG_RECURSION));
  }
}


void n_log_structured_standard (NLogLevelFlags log_level,const nchar *file,const nchar *line,
                           const nchar *func, const nchar *message_format,...)
{
  NLogField fields[] =
    {
      { "PRIORITY", log_level_to_priority (log_level), -1 },
      { "CODE_FILE", file, -1 },
      { "CODE_LINE", line, -1 },
      { "CODE_FUNC", func, -1 },
      /* Filled in later: */
      { "MESSAGE", NULL, -1 },
    };
  nsize n_fields;
  nchar *message_allocated = NULL;
  nchar buffer[1025];
  va_list args;
  va_start (args, message_format);
  if (log_level & N_LOG_FLAG_RECURSION){
      /* we use a stack buffer of fixed size, since we're likely
       * in an out-of-memory situation
       */
      nsize size N_GNUC_UNUSED;
      size = _n_vsnprintf (buffer, sizeof (buffer), message_format, args);
      fields[4].value = buffer;
  }else{
      fields[4].value = message_allocated = n_strdup_vprintf (message_format, args);
  }
  va_end (args);
  n_fields = N_N_ELEMENTS (fields) ;
  structuredArray (log_level, fields, n_fields);
  n_free (message_allocated);
}

/**
 * fd 号是否支持颜色
 */
static nboolean isSupportColor(nint output_fd)
{
  n_return_val_if_fail (output_fd >= 0, FALSE);
  return isatty (output_fd);
}


static void escape_string (NString *string);

static nint64 n_get_real_time (void)
{
  struct timeval r;
  gettimeofday (&r, NULL);
  return (((nint64) r.tv_sec) * 1000000) + r.tv_usec;

}

static nchar *formatFields (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields,nboolean use_color)
{
  nsize i;
  const nchar *message = NULL;
  nchar level_prefix[STRING_BUFFER_SIZE];
  NString *gstring;
  nint64 now;
  time_t now_secs;
  struct tm *now_tm;
  nchar time_buf[128];

  /* Extract some common fields. */
  for (i = 0; (message == NULL) && i < n_fields; i++){
      const NLogField *field = &fields[i];
      if (n_strcmp0 (field->key, "MESSAGE") == 0)
        message = field->value;
  }
  /* Format things. */
  mklevel_prefix (level_prefix, log_level, use_color);
  gstring = n_string_new (NULL);
  if (log_level & ALERT_LEVELS)
    n_string_append (gstring, "\n");

  if ((n_log_msg_prefix & (log_level & N_LOG_LEVEL_MASK)) ==(log_level & N_LOG_LEVEL_MASK))
    {
      const nchar *prn_name =NULL;// n_get_prgname ();
      nulong pid =getpid ();
      if (prn_name == NULL)
        n_string_append_printf (gstring, "(PID:%lu): ", pid);
      else
        n_string_append_printf (gstring, "(%s:%lu): ", prn_name, pid);
    }

  n_string_append (gstring, level_prefix);
  n_string_append (gstring, ": ");
  /* Timestamp */
  now = n_get_real_time ();
  now_secs = (time_t) (now / 1000000);
  now_tm = localtime (&now_secs);
  strftime (time_buf, sizeof (time_buf), "%H:%M:%S", now_tm);
  n_string_append_printf (gstring, "%s%s.%03d%s: ",
                          use_color ? "\033[34m" : "",
                          time_buf, (nint) ((now / 1000) % 1000),
                          color_reset (use_color));
  if (message == NULL){
      n_string_append (gstring, "(NULL) message");
  }else{
      NString *msg;
      const nchar *charset;
      msg = n_string_new (message);
      escape_string (msg);
      if (n_get_console_charset (&charset)){
          /* charset is UTF-8 already */
          n_string_append (gstring, msg->str);
      }else{
          nchar *lstring = strdup_convert (msg->str, charset);
          n_string_append (gstring, lstring);
          n_free (lstring);
      }
      n_string_free (msg, TRUE);
   }
   if(need_FILE_FUN_LINE && n_fields>=3){
       n_string_append (gstring, " ");
       n_string_append (gstring, fields[1].value);
       n_string_append (gstring, " ");
       n_string_append (gstring, fields[3].value);
       n_string_append (gstring, " ");
       n_string_append (gstring, fields[2].value);
   }
   return n_string_free (gstring, FALSE);
}



static NLogWriterOutput standardStreams (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields,npointer user_data)
{
  FILE *stream;
  nchar *out = NULL;  /* in the current locale’s character set */
  n_return_val_if_fail (fields != NULL, N_LOG_WRITER_UNHANDLED);
  n_return_val_if_fail (n_fields > 0, N_LOG_WRITER_UNHANDLED);
  stream = log_level_to_file (log_level);
  if (!stream || fileno (stream) < 0)
    return N_LOG_WRITER_UNHANDLED;
  out = formatFields(log_level, fields, n_fields,isSupportColor (fileno (stream)));
  _n_fprintf (stream, "%s\n", out);
  fflush (stream);
  n_free (out);
  return N_LOG_WRITER_HANDLED;
}

static nboolean exitsFile(const char *fileStr)
{
	 if(!fileStr)
		return FALSE;
	 char fn[100];
	 char *ptr = strrchr(fileStr, '/');
	 if(ptr==NULL)
		sprintf(fn,"%s",fileStr);
	 else
		sprintf(fn,"%s",ptr+1);
	 return strstr(defaultDomain,fn);
}

/**
 * 把domain当作要保留的文件
 */
static  nboolean filterFile(const NLogField *fields,nsize n_fields)
{
	   if(defaultDomain==NULL || !strcmp(defaultDomain,"all") || !strcmp(defaultDomain,"ALL"))
		  return FALSE;
	   const nchar *fileStr=NULL;
	   int i;
	   for (i = 0; i < n_fields; i++){
		  if (n_strcmp0 (fields[i].key, "CODE_FILE") == 0){
			 fileStr = fields[i].value;
			 break;
		  }
	   }
	   nboolean re=exitsFile(fileStr);
	   if(re)
		   return FALSE;
	   re=exitsFile(appendDomain);
	   if(re)
		   return FALSE;
	   return TRUE;
}


static nboolean shouldDropMessage (NLogLevelFlags log_level,const NLogField *fields,nsize n_fields)
{
  if(!(log_level & n_log_msg_prefix))
	  return TRUE;
  if(log_level==N_LOG_LEVEL_ERROR)
	  return FALSE;
  if (!(log_level & DEFAULT_LEVELS) && !(log_level >> N_LOG_LEVEL_USER_SHIFT)){
      if ((log_level & INFO_LEVELS) == 0 || defaultDomain == NULL){
    	  //printf("shouldDropMessage 11 %d %d N_LOG_LEVEL_USER_SHIFT:%d domain:%s %d %d\n",
    			  //log_level,DEFAULT_LEVELS,N_LOG_LEVEL_USER_SHIFT,defaultDomain,(log_level & INFO_LEVELS),(log_level & DEFAULT_LEVELS));

          //return TRUE;
      }
   }
   return filterFile(fields,n_fields);
}


static NLogWriterOutput defaultWriter(NLogLevelFlags log_level,const NLogField *fields, nsize n_fields, npointer user_data)
{
  static nsize initialized = 0;

  n_return_val_if_fail (fields != NULL, N_LOG_WRITER_UNHANDLED);
  n_return_val_if_fail (n_fields > 0, N_LOG_WRITER_UNHANDLED);
  if (shouldDropMessage (log_level, fields, n_fields)){
	  //printf("drop message level:%d\n",log_level);
    return N_LOG_WRITER_HANDLED;
  }

  /* Mark messages as fatal if they have a level set in
   * n_log_set_always_fatal().
   */
  if ((log_level & n_log_always_fatal))
    log_level |= N_LOG_FLAG_FATAL;

  /* Try logging to the systemd journal as first choice. */
  if(initialized==0)
      initialized=1;

  if (standardStreams(log_level,fields, n_fields, user_data) == N_LOG_WRITER_HANDLED)
    goto handled;

  return N_LOG_WRITER_UNHANDLED;

handled:
  /* Abort if the message was fatal. */
  if (log_level & N_LOG_FLAG_FATAL){
      /* MessageBox is allowed on UWP apps only when building against
       * the debug CRT, which will set -D_DEBUG */
      _n_log_abort (!(log_level & N_LOG_FLAG_RECURSION));
  }
  return N_LOG_WRITER_HANDLED;
}


static void escape_string (NString *string)
{
  const char *p = string->str;
  nunichar wc;

  while (p < string->str + string->len)
    {
      nboolean safe;

      wc = n_utf8_get_char_validated (p, -1);
      if (wc == (nunichar)-1 || wc == (nunichar)-2)
	{
	  nchar *tmp;
	  nuint pos;

	  pos = p - string->str;

	  /* Emit invalid UTF-8 as hex escapes
           */
	  tmp = n_strdup_printf ("\\x%02x", (nuint)(nuchar)*p);
	  n_string_erase (string, pos, 1);
	  n_string_insert (string, pos, tmp);

	  p = string->str + (pos + 4); /* Skip over escape sequence */

	  n_free (tmp);
	  continue;
	}
      if (wc == '\r')
	{
	  safe = *(p + 1) == '\n';
	}
      else
	{
	  safe = CHAR_IS_SAFE (wc);
	}

      if (!safe)
	{
	  nchar *tmp;
	  nuint pos;

	  pos = p - string->str;

	  /* Largest char we escape is 0x0a, so we don't have to worry
	   * about 8-digit \Uxxxxyyyy
	   */
	  tmp = n_strdup_printf ("\\u%04x", wc);
	  n_string_erase (string, pos, n_utf8_next_char (p) - p);
	  n_string_insert (string, pos, tmp);
	  n_free (tmp);

	  p = string->str + (pos + 6); /* Skip over escape sequence */
	}
      else
	p = n_utf8_next_char (p);
    }
}

void n_return_if_fail_warning (const char *pretty_function,const char *expression)
{
  n_log_structured_standard (N_LOG_LEVEL_WARNING, __FILE__, N_STRINGIFY (__LINE__),  N_STRFUNC,
		  "%s: assertion '%s' failed",pretty_function,expression);
}

void n_warn_message (const char *file,int line,const char *func,const char *warnexpr)
{
  char *s, lstr[32];
  n_snprintf (lstr, 32, "%d", line);
  if (warnexpr)
    s = n_strconcat ("(", file, ":", lstr, "):",
                     func, func[0] ? ":" : "",
                     " runtime check failed: (", warnexpr, ")", NULL);
  else
    s = n_strconcat ("(", file, ":", lstr, "):",
                     func, func[0] ? ":" : "",
                     " ", "code should not be reached", NULL);
  n_log_structured (N_LOG_LEVEL_WARNING, "%s", s);
  n_free (s);
}

void  n_log_set_domain(char *domain)
{
	if(defaultDomain!=NULL){
		n_free(defaultDomain);
		defaultDomain=NULL;
	}
	if(domain!=NULL){
		defaultDomain=n_strdup(domain);
	}
}

void n_log_set_level(int level)
{
	//4 8 16 32
	if(level==0)
	    n_log_msg_prefix = N_LOG_LEVEL_ERROR ;
	else if(level==1)
		n_log_msg_prefix = N_LOG_LEVEL_ERROR | N_LOG_LEVEL_WARNING;
	else if(level==2)
		n_log_msg_prefix = N_LOG_LEVEL_ERROR | N_LOG_LEVEL_WARNING | N_LOG_LEVEL_INFO;
    else if(level==3)
    	n_log_msg_prefix = N_LOG_LEVEL_ERROR | N_LOG_LEVEL_WARNING | N_LOG_LEVEL_INFO	|N_LOG_LEVEL_DEBUG;
    else
    	n_log_msg_prefix = level;
}

void  n_log_set_file_func_line(nboolean need)
{
	need_FILE_FUN_LINE=need;
}

void  n_log_append_domain(char *domain)
{
	if(appendDomain!=NULL){
		n_free(appendDomain);
		appendDomain=NULL;
	}
	if(domain!=NULL){
	  appendDomain=n_strdup(domain);
	}
}


void  n_log_set_param_from_env()
{
	char *level=getenv("AET_LOG_LEVEL");
	if(level!=NULL){
		int re=atoi(level);
		n_log_set_level(re);
	}
	char *domain=getenv("AET_LOG_DOMAIN");
	if(domain!=NULL){
		n_log_set_domain(domain);
	}else{
		n_log_set_domain("all");
	}
	char *detail=getenv("AET_LOG_FILE_FUN_LINE");
	if(detail!=NULL && (!strcmp(detail,"true") || !strcmp(detail,"TRUE") || !strcmp(detail,"True"))){
		n_log_set_file_func_line(TRUE);
	}
	printf("n_log_set_param_from_env level:%s domain:%s detail:%s\n",level,domain,detail);
}

nboolean n_log_is_print_file(char *fileStr)
{
   if(defaultDomain==NULL || !strcmp(defaultDomain,"all") || !strcmp(defaultDomain,"ALL"))
	  return TRUE;
   nboolean re=exitsFile(fileStr);
   if(re)
	   return TRUE;
   re=exitsFile(appendDomain);
   return re;
}

void n_log_set_writer_func (NLogWriterFunc func,npointer user_data,NDestroyNotify user_data_free)
{
  n_return_if_fail (func != NULL);
  log_writer_func = func;
  log_writer_user_data = user_data;
  log_writer_user_data_free = user_data_free;
}


nboolean  n_log_is_debug()
{
	return (N_LOG_LEVEL_DEBUG & n_log_msg_prefix);
}


