
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "FileSystem.h"
#include "AFile.h"
#include "../lang/AString.h"
#include "../lang/AAssert.h"

#define IS_DIR_SEPARATOR(c) ((c) == A_DIR_SEPARATOR)

#define f_stat    stat
#define PATH_LENGTH 2048

impl$ FileSystem{
	static FileSystem *getInstance(){
	   static FileSystem *singleton = NULL;
		if (!singleton){
			 singleton =new$ FileSystem();
		}
		return singleton;
	}

	FileSystem(){

	}

	char *buildFileName(const char  *separator, const char  *first_element,va_list *args,char **str_array){

	  int separator_len = strlen (separator);
	  aboolean is_first = TRUE;
	  aboolean have_leading = FALSE;
	  const char *single_element = NULL;
	  const char *next_element;
	  const char *last_trailing = NULL;
	  int i = 0;
	  AString string = new$ AString("");
	  if (str_array)
	    next_element = str_array[i++];
	  else
	    next_element = first_element;

	  while (TRUE){
	      const char *element;
	      const char *start;
	      const char *end;
	      if (next_element){
		    element = next_element;
		    if (str_array)
		      next_element = str_array[i++];
		    else
		      next_element = va_arg (*args, char *);
		  }else
		    break;

	      /* Ignore empty elements */
	      if (!*element)
		    continue;

	      start = element;

	      if (separator_len){
		     while (strncmp (start, separator, separator_len) == 0)
		       start += separator_len;
	      }

	      end = start + strlen (start);

	      if(separator_len){
		     while (end >= start + separator_len && strncmp (end - separator_len, separator, separator_len) == 0)
		       end -= separator_len;
		    last_trailing = end;
		    while (last_trailing >= element + separator_len && strncmp (last_trailing - separator_len, separator, separator_len) == 0)
		       last_trailing -= separator_len;

		    if(!have_leading){
		      /* If the leading and trailing separator strings are in the
		       * same element and overlap, the result is exactly that element
		       */
		      if (last_trailing <= start)
			     single_element = element;

		      string.append (element, start - element);
		      have_leading = TRUE;
		    }else
		      single_element = NULL;
		  }

	      if (end == start)
		    continue;

	      if (!is_first)
	    	  string.append (separator);

	      string.append (start, end - start);
	      is_first = FALSE;
	   }
       char *result=NULL;
	   if (single_element){
	        result= a_strdup (single_element);
	   }else{
	      if (last_trailing)
	    	  string.append (last_trailing);
	      result=a_strdup(string.toString());
	   }
	   return result;
	}

	char *buildFileName (const char  *firstArgument,va_list *args,achar **strArray){
	     char *str;
	     str = buildFileName(A_DIR_SEPARATOR_S, firstArgument, args, strArray);
	     return str;
	}

	char *buildFileName(const char *firstElement,  ...){
	  char *str;
	  va_list args;
	  va_start (args, firstElement);
	  str = buildFileName (firstElement, &args, NULL);
	  va_end (args);
	  return str;
	}

	aboolean isAbsolute(const char *fileName){
	   if (IS_DIR_SEPARATOR (fileName[0]))
	      return TRUE;
	   return FALSE;
	}

	char *getCurrentDir(){
	  const char *pwd;
	  char *buffer = NULL;
	  char *dir = NULL;
	  static aulong max_len = 0;
	  struct stat pwdbuf, dotbuf;
	  pwd = getenv ("PWD");
	  if (pwd != NULL && f_stat (".", &dotbuf) == 0 && f_stat (pwd, &pwdbuf) == 0 &&
	      dotbuf.st_dev == pwdbuf.st_dev && dotbuf.st_ino == pwdbuf.st_ino)
	    return a_strdup (pwd);

	  if (max_len == 0)
	    max_len = (PATH_LENGTH == -1) ? 2048 : PATH_LENGTH;
	  while (max_len < A_MAXULONG / 2){
	      a_free (buffer);
	      buffer = a_new (char, max_len + 1);
	      *buffer = 0;
	      dir = getcwd (buffer, max_len);
	      if (dir || errno != ERANGE)
	        break;
	      max_len *= 2;
	  }
	  if (!dir || !*buffer){
	      /* hm, should we g_error() out here?
	       * this can happen if e.g. "./" has mode \0000
	       */
	      buffer[0] = A_DIR_SEPARATOR;
	      buffer[1] = 0;
	  }
	  dir = a_strdup (buffer);
	  a_free (buffer);
	  return dir;

	}


	const char *getPathSkipRoot(const char *file_name){
	  a_return_val_if_fail (file_name != NULL, NULL);
	  /* Skip initial slashes */
	  if (IS_DIR_SEPARATOR (file_name[0])){
	      while (IS_DIR_SEPARATOR (file_name[0]))
	        file_name++;
	      return (char *)file_name;
	   }
	   return NULL;
	}

    /**
     * 获得标准文件名。
     */
	char *getCanonicalizeFileName(const char *filename,const char *relative_to){
	  char *canon, *input, *output, *after_root, *output_start;
	  a_return_val_if_fail (relative_to == NULL || isAbsolute (relative_to), NULL);
	  if (!isAbsolute (filename)){
	      achar *cwd_allocated = NULL;
	      const achar  *cwd;
	      if (relative_to != NULL)
	        cwd = relative_to;
	      else
	        cwd = cwd_allocated = getCurrentDir ();

	      canon = buildFileName(cwd, filename, NULL);
	      a_free (cwd_allocated);
	   }else{
	      canon = a_strdup (filename);
	   }

	   after_root = (char *)getPathSkipRoot(canon);
	   if (after_root == NULL){
	      /* This shouldn't really happen, as g_get_current_dir() should
	         return an absolute pathname, but bug 573843 shows this is
	         not always happening */
	      a_free (canon);
	      return buildFileName (A_DIR_SEPARATOR_S, filename, NULL);
	    }

	   /* Find the first dir separator and use the canonical dir separator. */
	    for (output = after_root - 1; (output >= canon) && IS_DIR_SEPARATOR (*output);output--)
	       *output = A_DIR_SEPARATOR;

	    /* 1 to re-increment after the final decrement above (so that output >= canon),
	     * and 1 to skip the first `/` */
	    output += 2;

	  /* POSIX allows double slashes at the start to mean something special
	   * (as does windows too). So, "//" != "/", but more than two slashes
	   * is treated as "/".
	   */
	  if (after_root - output == 1)
	    output++;

	  input = after_root;
	  output_start = output;
	  while (*input){
	      /* input points to the next non-separator to be processed. */
	      /* output points to the next location to write to. */
	      a_assert (input > canon && IS_DIR_SEPARATOR (input[-1]));
	      a_assert (output > canon && IS_DIR_SEPARATOR (output[-1]));
	      a_assert (input >= output);
	      /* Ignore repeated dir separators. */
	      while (IS_DIR_SEPARATOR (input[0]))
	       input++;

	      /* Ignore single dot directory components. */
	      if (input[0] == '.' && (input[1] == 0 || IS_DIR_SEPARATOR (input[1]))){
	           if (input[1] == 0)
	             break;
	           input += 2;
	      }else if (input[0] == '.' && input[1] == '.' && (input[2] == 0 || IS_DIR_SEPARATOR (input[2]))){
	          /* Remove double-dot directory components along with the preceding
	           * path component. */
	          if (output > output_start){
	              do{
	                  output--;
	              }while (!IS_DIR_SEPARATOR (output[-1]) && output > output_start);
	          }
	          if (input[2] == 0)
	            break;
	          input += 3;
	      }else{
	          /* Copy the input to the output until the next separator,
	           * while converting it to canonical separator */
	          while (*input && !IS_DIR_SEPARATOR (*input))
	            *output++ = *input++;
	          if (input[0] == 0)
	            break;
	          input++;
	          *output++ = A_DIR_SEPARATOR;
	      }
	   }
	   /* Remove a potentially trailing dir separator */
	   if (output > output_start && IS_DIR_SEPARATOR (output[-1]))
	       output--;
	   *output = '\0';
	   return canon;
	}

};

