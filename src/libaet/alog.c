#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include "alog.h"
#include "athreadspecific.h"
#include "amem.h"
#include "aprintf.h"
#include "astrfuncs.h"
#include "aatomic.h"
#include "aconvert.h"
#include "acharset.h"
#include "datastruct.h"
#include "aunicode.h"
#include "innerlock.h"




#define FORMAT_UNSIGNED_BUFSIZE ((ALIB_SIZEOF_LONG * 3) + 3)
#define	STRING_BUFFER_SIZE	(FORMAT_UNSIGNED_BUFSIZE + 32)

#define	ALERT_LEVELS   (A_LOG_LEVEL_ERROR |  A_LOG_LEVEL_WARNING)
#define DEFAULT_LEVELS (A_LOG_LEVEL_ERROR |  A_LOG_LEVEL_WARNING | A_LOG_LEVEL_INFO)
#define INFO_LEVELS    (A_LOG_LEVEL_INFO | A_LOG_LEVEL_DEBUG)


#define CHAR_IS_SAFE(wc) (!((wc < 0x20 && wc != '\t' && wc != '\n' && wc != '\r') || \
			    (wc == 0x7f) || \
			    (wc >= 0x80 && wc < 0xa0)))

static const achar     *log_level_to_color (ALogLevelFlags log_level,aboolean  use_color);
static const achar     *color_reset        (aboolean       use_color);
static void             escape_string (DString *string);
static ALogWriterOutput a_log_writer_default (ALogLevelFlags log_level,const ALogField *fields, asize a_fields, apointer user_data);
static void             a_log_structured_array (ALogLevelFlags log_level,const ALogField *fields,asize a_fields);

static aboolean use_stderr = FALSE;
static char *defaultDomain=NULL;
static char *appendDomain=NULL;
static aboolean need_FILE_FUN_LINE=FALSE;
static InnerMutex a_messages_lock;

/* --- variables --- */
static AThreadSpecific  a_log_structured_depth;
static AThreadSpecific  a_log_depth;

static apointer         log_writer_user_data = NULL;
static ALogLevelFlags   a_log_msg_prefix = A_LOG_LEVEL_ERROR | A_LOG_LEVEL_WARNING | A_LOG_LEVEL_DEBUG;
static ALogLevelFlags   a_log_always_fatal = A_LOG_FATAL_MASK;

/* --- functions --- */

#define A_BREAKPOINT()        A_STMT_START{ __asm__ __volatile__ ("int $03"); }A_STMT_END

static char *processName=NULL;
static aboolean pausePrint=FALSE;

static void _a_log_abort (aboolean breakpoint)
{
  aboolean debugger_present;
  debugger_present = TRUE;
  //printf("_a_log_abort --- %d %d\n",debugger_present,breakpoint);
  if (debugger_present && breakpoint){
      a_log_print_stack();
      abort ();//A_BREAKPOINT ();
  }else{
      a_log_print_stack();
      a_abort ();
  }
}

static char* getProcessName()
{
	if(processName==NULL){
	   int pid=getpid();
       char info[128];
       char *name = NULL;
	   sprintf(info, "/proc/%d/cmdline",pid);
	   FILE* f = fopen(info,"r");
	   if(f){
		 fread(info, sizeof(char), 100, f);
		 int i;
		 for (i = 0 ; i < 50; i++) {
			if (!i){
				name = a_strdup(info);
				break;
			}
		 }
		 fclose(f);
	  }
      if(name == NULL){
    	  processName=a_strdup("unknown");
    	  return processName;
      }
      int index=a_str_last_indexof(name,0,strlen(name),"/",0,1,strlen(name));
      char *re= a_str_substring(name,index+1,strlen(name));
      if(re==NULL)
    	  processName=a_strdup("unknown");
      else
    	  processName=re;
	}
      return processName;
}

static achar* strdup_convert (const achar *string,const achar *charset)
{
   if (!a_utf8_validate (string, -1, NULL)){
      DString *gstring = d_string_new ("[Invalid UTF-8] ");
      auchar *p;
      for (p = (auchar *)string; *p; p++){
	     if (CHAR_IS_SAFE(*p) && !(*p == '\r' && *(p + 1) != '\n') && *p < 0x80)
	    	d_string_append_c (gstring, *p);
	    else
	        d_string_append_printf (gstring, "\\x%02x", (auint)(auchar)*p);
	  }
      return d_string_free (gstring, FALSE);
  }else{
      AError *err = NULL;
      achar *result = a_convert_with_fallback (string, -1, charset, "UTF-8", "?", NULL, NULL, &err);
      if (result)
	     return result;
      else{

	    static aboolean warned = FALSE;
	    if (!warned){
	        warned = TRUE;
	        _a_fprintf (stderr, "ALib: Cannot convert message: %s\n", err->message);
	    }
	    a_error_free (err);
	    return a_strdup (string);
	  }
   }
}



static void format_unsigned (achar  *buf,aulong  num,auint   radix)
{
  aulong tmp;
  achar c;
  aint i, n;

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



void a_log_set_use_stderr (aboolean useStderr)
{
  a_return_if_fail (inner_thread_n_created () == 0);
  use_stderr = useStderr;
}

static FILE *mklevel_prefix (achar level_prefix[STRING_BUFFER_SIZE],ALogLevelFlags log_level,aboolean use_color)
{
  aboolean to_stdout = !use_stderr;

  strcpy (level_prefix, log_level_to_color (log_level, use_color));

  switch (log_level & A_LOG_LEVEL_MASK)
    {
    case A_LOG_LEVEL_ERROR:
      strcat (level_prefix, "ERROR");
      to_stdout = FALSE;
      break;
    case A_LOG_LEVEL_WARNING:
      strcat (level_prefix, "WARNING");
      to_stdout = FALSE;
      break;
    case A_LOG_LEVEL_INFO:
      strcat (level_prefix, "INFO");
      break;
    case A_LOG_LEVEL_DEBUG:
      strcat (level_prefix, "DEBUG");
      break;
    default:
      if (log_level)
	{
	  strcat (level_prefix, "LOG-");
	  format_unsigned (level_prefix + 4, log_level & A_LOG_LEVEL_MASK, 16);
	}
      else
	strcat (level_prefix, "LOG");
      break;
    }

  strcat (level_prefix, color_reset (use_color));

  if (log_level & A_LOG_FLAG_RECURSION)
    strcat (level_prefix, " (recursed)");
  if (log_level & ALERT_LEVELS)
    strcat (level_prefix, " **");
  return to_stdout ? stdout : stderr;
}


static const achar *log_level_to_priority (ALogLevelFlags log_level)
{
  if (log_level & A_LOG_LEVEL_ERROR)
    return "3";
  else if (log_level & A_LOG_LEVEL_WARNING)
    return "4";
  else if (log_level & A_LOG_LEVEL_INFO)
    return "5";
  else if (log_level & A_LOG_LEVEL_DEBUG)
    return "6";

  return "5";
}

static FILE *log_level_to_file (ALogLevelFlags log_level)
{
  if (use_stderr)
    return stderr;
  if (log_level & (A_LOG_LEVEL_ERROR | A_LOG_LEVEL_WARNING | A_LOG_LEVEL_INFO))
    return stderr;
  else
    return stdout;
}

static const achar *log_level_to_color (ALogLevelFlags log_level,aboolean       use_color)
{
  if (!use_color)
    return "";

  if (log_level & A_LOG_LEVEL_ERROR)
    return "\033[1;31m"; /* red */
  else if (log_level & A_LOG_LEVEL_WARNING)
    return "\033[1;33m"; /* yellow */
  else if (log_level & A_LOG_LEVEL_INFO)
    return "\033[1;32m"; /* green */
  else if (log_level & A_LOG_LEVEL_DEBUG)
    return "\033[1;32m"; /* green */

  return "";
}

static const achar *color_reset (aboolean use_color)
{
  if (!use_color)
    return "";
  return "\033[0m";
}



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"


void a_log_structured (ALogLevelFlags  log_level,...)
{
  if(pausePrint)
      return;
  va_list args;
  achar buffer[1025], *message_allocated = NULL;
  const char *format;
  const achar *message;
  apointer p;
  asize a_fields, i;
  ALogField stack_fields[16];
  ALogField *fields = stack_fields;
  ALogField *fields_allocated = NULL;
  DArray *array = NULL;

  va_start (args, log_level);

  /* MESSAGE and PRIORITY are a given */
  a_fields = 2;

  for (p = va_arg (args, achar *), i = a_fields;strcmp (p, "MESSAGE") != 0;p = va_arg (args, achar *), i++){
      ALogField field;
      const achar *key = p;
      aconstpointer value = va_arg (args, apointer);

      field.key = key;
      field.value = value;
      field.length = -1;
      if (i < 16){
        stack_fields[i] = field;
      }else{

          if (log_level & A_LOG_FLAG_RECURSION)
            continue;
          if (i == 16){
              array = d_array_sized_new (FALSE, FALSE, sizeof (ALogField), 32);
              d_array_append_vals (array, stack_fields, 16);
          }

          d_array_append_val (array, &field);
      }
   }

  a_fields = i;
  if (array)
    fields = fields_allocated = (ALogField *) d_array_free (array, FALSE);
  format = va_arg (args, achar *);

  if (log_level & A_LOG_FLAG_RECURSION)
    {

      asize size A_GNUC_UNUSED;
      size = _a_vsnprintf (buffer, sizeof (buffer), format, args);
      message = buffer;
    }
  else
    {
      message = message_allocated = a_strdup_vprintf (format, args);
    }
  fields[0].key = "MESSAGE";
  fields[0].value = message;
  fields[0].length = -1;

  fields[1].key = "PRIORITY";
  fields[1].value = log_level_to_priority (log_level);
  fields[1].length = -1;
  /* Log it. */
  a_log_structured_array (log_level, fields, a_fields);
  a_free (fields_allocated);
  a_free (message_allocated);
  va_end (args);
}


#pragma GCC diagnostic pop

static void write_string (FILE *stream,const achar *string)
{
  fputs (string, stream);
}

static void write_string_sized (FILE *stream,const achar *string,assize length)
{
  if (length < 0)
    write_string (stream, string);
  else
    fwrite (string, 1, length, stream);
}

/**
*只打印我们肯定能识别的字段，否则我们最终会
*打印用户提供的随机非字符串指针
*功能由其作者进行解释。
 */
static ALogWriterOutput _a_log_writer_fallback (ALogLevelFlags log_level,const ALogField *fields,asize a_fields,apointer user_data)
{
  FILE *stream;
  asize i;
  stream = log_level_to_file (log_level);
  printf("_a_log_writer_fallback--- a_fields:%ld\n",a_fields);
  for (i = 0; i < a_fields; i++)
    {
      const ALogField *field = &fields[i];
      if (strcmp (field->key, "MESSAGE") != 0 &&
          strcmp (field->key, "PRIORITY") != 0 &&
          strcmp (field->key, "CODE_OBJECT") != 0 &&
          strcmp (field->key, "CODE_FILE") != 0 &&
          strcmp (field->key, "CODE_LINE") != 0 &&
          strcmp (field->key, "CODE_FUNC") != 0)
        continue;

      write_string (stream, field->key);
      write_string (stream, "=");
      write_string_sized (stream, field->value, field->length);
    }

  {
    achar pid_string[FORMAT_UNSIGNED_BUFSIZE];
    format_unsigned (pid_string, getpid (), 10);
    write_string (stream, "_PID=");
    write_string (stream, pid_string);
  }

  return A_LOG_WRITER_HANDLED;
}

static void a_log_structured_array (ALogLevelFlags log_level,const ALogField *fields,asize a_fields)
{
  aboolean recursion;
  auint depth;
  if (a_fields == 0)
    return;

  depth = APOINTER_TO_UINT (a_thread_specific_get (&a_log_structured_depth));
  recursion = (depth > 0);
  a_thread_specific_set (&a_log_structured_depth, AUINT_TO_POINTER (++depth));
  if(recursion){
     _a_log_writer_fallback(log_level, fields, a_fields, log_writer_user_data);
  }else{
	  a_log_writer_default(log_level, fields, a_fields, log_writer_user_data);
  }
  a_thread_specific_set (&a_log_structured_depth, AUINT_TO_POINTER (--depth));
  if (log_level & A_LOG_FATAL_MASK){
    _a_log_abort (!(log_level & A_LOG_FLAG_RECURSION));
  }
}


void a_log_structured_standard (ALogLevelFlags log_level,const achar *object,const achar *file,const achar*line,const achar *func,const achar *message_format,...)
{
    if(pausePrint)
        return;
    ALogField fields[] ={
      { "PRIORITY", log_level_to_priority (log_level), -1 },
      { "CODE_OBJECT", object, -1 },
      { "CODE_FILE", file, -1 },
      { "CODE_LINE", line, -1 },
      { "CODE_FUNC", func, -1 },
      /* 晚些再填上: */
      { "MESSAGE", NULL, -1 },
    };
  asize a_fields;
  achar *message_allocated = NULL;
  achar buffer[1025];
  va_list args;
  va_start (args, message_format);
  if (log_level & A_LOG_FLAG_RECURSION){
	  /* 我们使用固定大小的堆栈缓冲区，因为
	   *在内存不足的情况下
	  */
      _a_vsnprintf (buffer, sizeof (buffer), message_format, args);
      fields[5].value = buffer;
  }else{
      fields[5].value = message_allocated = a_strdup_vprintf (message_format, args);
  }
  va_end (args);
  a_fields = A_N_ELEMENTS (fields);
  a_log_structured_array (log_level, fields, a_fields);
  a_free (message_allocated);
}


static aboolean setColor(aint output_fd)
{
  a_return_val_if_fail (output_fd >= 0, FALSE);
  return isatty (output_fd);
}


/**
 * 用gmtime_r替换strftime是不是性能更好呢？
 * struct tm today;
  time_t now0;
  aint64 now = (((aint64) r.tv_sec) * 1000000) + r.tv_usec;
  now0 =r.tv_sec;
  now0 = now0 + 28880; //因为我们在+8时区 后面要用gmtime分解时间 必须手工修正一下 就是60秒*60分钟*8小时 = 28880
  gmtime_r(&now0, &today);
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", today.tm_hour, today.tm_min, today.tm_sec);
 */

static achar *a_log_writer_format_fields (ALogLevelFlags log_level,const ALogField *fields,asize a_fields,aboolean use_color)
{
  asize i;
  const achar *message = NULL;
  const achar *log_domain = NULL;
  achar level_prefix[STRING_BUFFER_SIZE];
  DString *gstring;
  aint64 now;
  time_t now_secs;
  struct tm *now_tm;
  achar time_buf[128];

  for (i = 0; (message == NULL) && i < a_fields; i++)
    {
      const ALogField *field = &fields[i];
      if (a_strcmp0 (field->key, "MESSAGE") == 0)
        message = field->value;
    }

  mklevel_prefix (level_prefix, log_level, use_color);

  gstring = d_string_new (NULL);
  if (log_level & ALERT_LEVELS)
    d_string_append (gstring, "\n");
  if (!log_domain)
    d_string_append (gstring, "** ");

  if ((a_log_msg_prefix & (log_level & A_LOG_LEVEL_MASK)) ==(log_level & A_LOG_LEVEL_MASK))
    {
      const achar *pra_name = getProcessName ();
      aulong pid = getpid ();
      if (pra_name == NULL)
        d_string_append_printf (gstring, "(PID:%lu): ", pid);
      else
        d_string_append_printf (gstring, "(%s:%lu): ", pra_name, pid);
    }

  if (log_domain != NULL){
      d_string_append (gstring, log_domain);
      d_string_append_c (gstring, '-');
  }
  d_string_append (gstring, level_prefix);
  d_string_append (gstring, ": ");
  /* Timestamp */
  struct timeval r;
  gettimeofday (&r, NULL);
  now = (((aint64) r.tv_sec) * 1000000) + r.tv_usec;
  now_secs = (time_t) (now / 1000000);
  now_tm = localtime (&now_secs);
  strftime (time_buf, sizeof (time_buf), "%H:%M:%S", now_tm);
  d_string_append_printf (gstring, "%s%s.%03d%s: ",
                          use_color ? "\033[34m" : "",
                          time_buf, (aint) ((now / 1000) % 1000),
                          color_reset (use_color));
  if(!strcmp("CODE_OBJECT",fields[1].key)){
    d_string_append(gstring,fields[1].value);//这是__OBJECT__
    d_string_append(gstring," ");//这是__OBJECT__
  }
  if (message == NULL){
      d_string_append (gstring, "(NULL) message");
  }else{
      DString *msg;
      const achar *charset;
      msg = d_string_new (message);
      escape_string (msg);
      if (a_get_console_charset (&charset)){
          d_string_append (gstring, msg->str);
      }else{
          achar *lstring = strdup_convert (msg->str, charset);
          d_string_append (gstring, lstring);
          a_free (lstring);
      }
      d_string_free (msg, TRUE);
  }
  if(need_FILE_FUN_LINE && a_fields>=3){
      d_string_append (gstring, " ");
      d_string_append (gstring, fields[1].value);
      d_string_append (gstring, " ");
      d_string_append (gstring, fields[3].value);
      d_string_append (gstring, " ");
      d_string_append (gstring, fields[2].value);
  }
  return d_string_free (gstring, FALSE);
}



static ALogWriterOutput a_log_writer_standard_streams (ALogLevelFlags log_level,const ALogField *fields,asize a_fields,apointer user_data)
{
  FILE *stream;
  achar *out = NULL;

  a_return_val_if_fail (fields != NULL, A_LOG_WRITER_UNHANDLED);
  a_return_val_if_fail (a_fields > 0, A_LOG_WRITER_UNHANDLED);

  stream = log_level_to_file (log_level);
  if (!stream || fileno (stream) < 0)
    return A_LOG_WRITER_UNHANDLED;

  out = a_log_writer_format_fields (log_level, fields, a_fields,setColor(fileno (stream)));
  _a_fprintf (stream, "%s\n", out);
  fflush (stream);
  a_free (out);
  return A_LOG_WRITER_HANDLED;
}


static aboolean exitsFile(const char *fileStr)
{
	 if(!fileStr)
		return FALSE;
	 char fn[100];
	 char *ptr = strrchr(fileStr, '/');
	 if(ptr==NULL)
		sprintf(fn,"%s",fileStr);
	 else
		sprintf(fn,"%s",ptr+1);
	 return strstr(defaultDomain,fn)!=NULL;
}

/**
 * 把domain当作要保留的文件
 */
static  aboolean filterFile(const ALogField *fields,asize n_fields)
{
	   if(defaultDomain==NULL || !strcmp(defaultDomain,"all") || !strcmp(defaultDomain,"ALL"))
		  return FALSE;
	   const achar *fileStr=NULL;
	   int i;
	   for (i = 0; i < n_fields; i++){
		  if (a_strcmp0 (fields[i].key, "CODE_FILE") == 0){
			 fileStr = fields[i].value;
			 break;
		  }
	   }
	   aboolean re=exitsFile(fileStr);
	   if(re)
		   return FALSE;
	   re=exitsFile(appendDomain);
	   if(re)
		   return FALSE;
	   return TRUE;
}

static aboolean should_drop_message (ALogLevelFlags log_level,const ALogField *fields,asize a_fields)
{
  if(!(log_level & a_log_msg_prefix))
	  return TRUE;
  if(log_level==A_LOG_LEVEL_ERROR)
	  return FALSE;
  if (!(log_level & DEFAULT_LEVELS) && !(log_level >> A_LOG_LEVEL_USER_SHIFT)){
      if ((log_level & INFO_LEVELS) == 0 || defaultDomain == NULL){

      }
   }
   return filterFile(fields,a_fields);
}


static ALogWriterOutput a_log_writer_default (ALogLevelFlags log_level,const ALogField *fields, asize a_fields, apointer user_data)
{
  static asize initialized = 0;

  a_return_val_if_fail (fields != NULL, A_LOG_WRITER_UNHANDLED);
  a_return_val_if_fail (a_fields > 0, A_LOG_WRITER_UNHANDLED);

  if (should_drop_message (log_level, fields, a_fields))
    return A_LOG_WRITER_HANDLED;

  /* 如果在a_log_set_always_fatal中设置了级别，则将消息标记为致命消息
   */
  if ((log_level & a_log_always_fatal))
    log_level |= A_LOG_FLAG_FATAL;

  /* 作为首选，尝试登录到systemd日志。 */
  if (a_once_init_enter (&initialized))
    {
      a_once_init_leave (&initialized, TRUE);
    }


  if (a_log_writer_standard_streams (log_level, fields, a_fields, user_data) == A_LOG_WRITER_HANDLED)
    goto handled;

  return A_LOG_WRITER_UNHANDLED;

handled:
  if (log_level & A_LOG_FLAG_FATAL)
    {
      _a_log_abort (!(log_level & A_LOG_FLAG_RECURSION));
    }

  return A_LOG_WRITER_HANDLED;
}


static void escape_string (DString *string)
{
  const char *p = string->str;
  aunichar wc;
  while (p < string->str + string->len)
    {
      aboolean safe;

      wc = a_utf8_get_char_validated (p, -1);
      if (wc == (aunichar)-1 || wc == (aunichar)-2)
	{
	  achar *tmp;
	  auint pos;

	  pos = p - string->str;

	  /* Emit invalid UTF-8 as hex escapes
           */
	  tmp = a_strdup_printf ("\\x%02x", (auint)(auchar)*p);
	  d_string_erase (string, pos, 1);
	  d_string_insert (string, pos, tmp);

	  p = string->str + (pos + 4); /* Skip over escape sequence */

	  a_free (tmp);
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
	  achar *tmp;
	  auint pos;

	  pos = p - string->str;

	  tmp = a_strdup_printf ("\\u%04x", wc);
	  d_string_erase (string, pos, a_utf8_next_char (p) - p);
	  d_string_insert (string, pos, tmp);
	  a_free (tmp);

	  p = string->str + (pos + 6); /* Skip over escape sequence */
	}
      else
	p = a_utf8_next_char (p);
    }
}

void a_return_if_fail_warning (const char *pretty_function,const char *expression)
{
  a_log_structured_standard (A_LOG_LEVEL_WARNING, __OBJECT__,__FILE__, A_STRINGIFY (__LINE__),  A_STRFUNC,
		  "%s: assertion '%s' failed",pretty_function,expression);
}

static inline int g_bit_nth_msf_impl (aulong mask,int   nth_bit)
{
  if (nth_bit < 0 || A_UNLIKELY (nth_bit > ALIB_SIZEOF_LONG * 8))
    nth_bit = ALIB_SIZEOF_LONG * 8;
  while (nth_bit > 0)
    {
      nth_bit--;
      if (mask & (1UL << nth_bit))
        return nth_bit;
    }
  return -1;
}

static void fallbackHandler (ALogLevelFlags log_level, const char *message,apointer  unused_data)
{
  char level_prefix[STRING_BUFFER_SIZE];
  char pid_string[FORMAT_UNSIGNED_BUFSIZE];
  FILE *stream;
  stream = mklevel_prefix (level_prefix, log_level, FALSE);
  if (!message)
    message = "(NULL) message";
  format_unsigned (pid_string, getpid (), 10);
  write_string (stream, "\n** ");
  write_string (stream, "(process:");
  write_string (stream, pid_string);
  write_string (stream, "): ");
  write_string (stream, level_prefix);
  write_string (stream, ": ");
  write_string (stream, message);
}

static void warningHandler(ALogLevelFlags log_level,const char   *message,apointer  unused_data)
{
  ALogField fields[2];
  int n_fields = 0;
  if (log_level & A_LOG_FLAG_RECURSION){
      fallbackHandler (log_level, message, unused_data);
      return;
  }

  fields[0].key = "MESSAGE";
  fields[0].value = message;
  fields[0].length = -1;
  n_fields++;

  fields[1].key = "PRIORITY";
  fields[1].value = log_level_to_priority (log_level);
  fields[1].length = -1;
  n_fields++;
  a_log_structured_array (log_level & ~A_LOG_FLAG_FATAL, fields, n_fields);
}

#define g_bit_nth_msf(mask, nth_bit) g_bit_nth_msf_impl(mask, nth_bit)

void a_logv (ALogLevelFlags log_level,const char   *format,va_list args)
{
  aboolean was_fatal = (log_level & A_LOG_FLAG_FATAL) != 0;
  aboolean was_recursion = (log_level & A_LOG_FLAG_RECURSION) != 0;
  char buffer[1025], *msg, *msg_alloc = NULL;
  int i;
  log_level &= A_LOG_LEVEL_MASK;
  if (!log_level)
    return;

  if (log_level & A_LOG_FLAG_RECURSION){
      asize size A_GNUC_UNUSED;
      size = _a_vsnprintf (buffer, 1024, format, args);
      msg = buffer;
  }else
    msg = msg_alloc = a_strdup_vprintf (format, args);


  for (i = g_bit_nth_msf (log_level, -1); i >= 0; i = g_bit_nth_msf (log_level, i)){
      ALogLevelFlags test_level;
      test_level = 1L << i;
      if (log_level & test_level){
         ALogLevelFlags domain_fatal_mask;
         apointer data = NULL;
         aboolean masquerade_fatal = FALSE;
         auint depth;
         if (was_fatal)
             test_level |= A_LOG_FLAG_FATAL;
         if (was_recursion)
            test_level |= A_LOG_FLAG_RECURSION;

          inner_mutex_lock (&a_messages_lock);
          depth = APOINTER_TO_UINT (a_thread_specific_get (&a_log_depth));
          if (depth)
            test_level |= A_LOG_FLAG_RECURSION;
          depth++;
          domain_fatal_mask = A_LOG_FATAL_MASK;
          if ((domain_fatal_mask | a_log_always_fatal) & test_level){
            test_level |= A_LOG_FLAG_FATAL;
          }
          inner_mutex_unlock (&a_messages_lock);
          a_thread_specific_set (&a_log_depth, AUINT_TO_POINTER (depth));
          if (test_level & A_LOG_FLAG_RECURSION){
              fallbackHandler(test_level, msg, data);
          }else{
               warningHandler (test_level, msg, data);
          }

          if ((test_level & A_LOG_FLAG_FATAL) && !(test_level & A_LOG_LEVEL_ERROR)){
              printf("(test_level & A_LOG_FLAG_FATAL) && !(test_level & A_LOG_LEVEL_ERROR)\n");
          }

          if ((test_level & A_LOG_FLAG_FATAL) && !masquerade_fatal){
              _a_log_abort (!(test_level & A_LOG_FLAG_RECURSION));
           }
           depth--;
           a_thread_specific_set (&a_log_depth, AUINT_TO_POINTER (depth));
        }
     }
     a_free (msg_alloc);
}


static void a_log (ALogLevelFlags level,const char *format,...)
{
  va_list args;
  va_start (args, format);
  a_logv (level, format, args);
  va_end (args);
}

void a_warn_message (const char *file, int line,const char *func,const char *warnexpr)
{
  char *s, lstr[32];
  a_snprintf (lstr, 32, "%d", line);
  if (warnexpr)
    s = a_strconcat ("(", file, ":", lstr, "):",func, func[0] ? ":" : ""," 运行时检查失败！: (", warnexpr, ")", NULL);
  else
    s = a_strconcat ("(", file, ":", lstr, "):",func, func[0] ? ":" : ""," ", "代码不可能执行到这里", NULL);
  a_log (A_LOG_LEVEL_WARNING, "%s", s);
  a_free (s);
}

void  a_log_set_domain(char *domain)
{
	inner_mutex_lock (&a_messages_lock);
	if(defaultDomain!=NULL){
		a_free(defaultDomain);
		defaultDomain=NULL;
	}
	if(domain!=NULL){
		defaultDomain=a_strdup(domain);
	}
	inner_mutex_unlock (&a_messages_lock);
}

void a_log_set_level(int level)
{
	//4 8 16 32
	if(level==0)
	    a_log_msg_prefix = A_LOG_LEVEL_ERROR ;
	else if(level==1)
		a_log_msg_prefix = A_LOG_LEVEL_ERROR | A_LOG_LEVEL_WARNING;
	else if(level==2)
		a_log_msg_prefix = A_LOG_LEVEL_ERROR | A_LOG_LEVEL_WARNING | A_LOG_LEVEL_INFO;
    else if(level==3)
    	a_log_msg_prefix = A_LOG_LEVEL_ERROR | A_LOG_LEVEL_WARNING | A_LOG_LEVEL_INFO	|A_LOG_LEVEL_DEBUG;
    else
    	a_log_msg_prefix = level;
}

void  a_log_set_file_func_line(aboolean need)
{
	need_FILE_FUN_LINE=need;
}

void  a_log_append_domain(char *domain)
{
	if(appendDomain!=NULL){
		a_free(appendDomain);
		appendDomain=NULL;
	}
	if(domain!=NULL){
	  appendDomain=a_strdup(domain);
	}
}


void  a_log_set_param_from_env()
{
	char *level=getenv("A_LOG_LEVEL");
	if(level!=NULL){
		int re=atoi(level);
		a_log_set_level(re);
	}
	char *domain=getenv("A_LOG_DOMAIN");
	if(domain!=NULL){
		a_log_set_domain(domain);
	}else{
		a_log_set_domain("all");
	}
	char *detail=getenv("AET_LOG_FILE_FUN_LINE");
	if(detail!=NULL && (!strcmp(detail,"true") || !strcmp(detail,"TRUE") || !strcmp(detail,"True"))){
		a_log_set_file_func_line(TRUE);
	}

	printf("a_log_set_param_from_env level:%s domain:%s detail:%s\n",level,domain,detail);
}


void  a_printerr(const achar *format,...)
{
	  va_list args;
	  char *string;
	  a_return_if_fail (format != NULL);
	  va_start (args, format);
	  string = a_strdup_vprintf (format, args);
	  va_end (args);
	  const char *charset;
	  if (a_get_console_charset (&charset))
		fputs (string, stderr);
	  else{
		  char *lstring = strdup_convert (string, charset);
		  fputs (lstring, stderr);
		  a_free (lstring);
	  }
	  fflush (stderr);
	  a_free (string);
}

void  a_print(const achar *format,...)
{
	  va_list args;
	  char *string;
	  a_return_if_fail (format != NULL);
	  va_start (args, format);
	  string = a_strdup_vprintf (format, args);
	  va_end (args);
	  const char *charset;
	  if (a_get_console_charset (&charset))
		fputs (string, stdout); /* charset is UTF-8 already */
	  else{
		  char *lstring = strdup_convert (string, charset);
		  fputs (lstring, stdout);
		  a_free (lstring);
	  }
	  fflush (stdout);
	  a_free (string);
}

ALogLevelFlags  a_log_set_always_fatal  (ALogLevelFlags  fatal_mask)
{
    ALogLevelFlags old_mask;

    fatal_mask &= (1 << A_LOG_LEVEL_USER_SHIFT) - 1;
    fatal_mask |= A_LOG_LEVEL_ERROR;
    fatal_mask &= ~A_LOG_FLAG_FATAL;

    inner_mutex_lock (&a_messages_lock);
    old_mask = a_log_always_fatal;
    a_log_always_fatal = fatal_mask;
    inner_mutex_unlock (&a_messages_lock);
    return old_mask;
}
/**
 * 当打印栈内容时，暂停
 */
void a_log_pause(){
    pausePrint=TRUE;
}
void a_log_resume(){
    pausePrint=FALSE;

}

