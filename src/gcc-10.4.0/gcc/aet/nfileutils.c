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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#define S_ISLNK(x) 0
#define O_BINARY 0
#define O_CLOEXEC 0
#include <linux/magic.h>
#include <sys/vfs.h>
#include "nfileutils.h"
#include "nstrfuncs.h"
#include "nlog.h"
#include "nmem.h"
#include "nstring.h"

#define n_stat    stat
#define N_PATH_LENGTH 2048

nchar *n_path_get_basename (const nchar *file_name)
{
  nssize base;
  nssize last_nonslash;
  nsize len;
  nchar *retval;

  n_return_val_if_fail (file_name != NULL, NULL);

  if (file_name[0] == '\0')
    return n_strdup (".");

  last_nonslash = strlen (file_name) - 1;

  while (last_nonslash >= 0 && N_IS_DIR_SEPARATOR (file_name [last_nonslash]))
    last_nonslash--;

  if (last_nonslash == -1)
    /* string only containing slashes */
    return n_strdup (N_DIR_SEPARATOR_S);

  base = last_nonslash;
  while (base >=0 && !N_IS_DIR_SEPARATOR (file_name [base]))
    base--;

  len = last_nonslash - base;
  retval = n_malloc (len + 1);
  memcpy (retval, file_name + (base + 1), len);
  retval [len] = '\0';

  return retval;
}



nboolean n_path_is_absolute (const char *file_name)
{

  if (N_IS_DIR_SEPARATOR (file_name[0]))
    return TRUE;
  return FALSE;
}




nchar *n_get_current_dir (void)
{

  const nchar *pwd;
  nchar *buffer = NULL;
  nchar *dir = NULL;
  static nulong max_len = 0;
  struct stat pwdbuf, dotbuf;

  pwd = getenv ("PWD");
  if (pwd != NULL && n_stat (".", &dotbuf) == 0 && n_stat (pwd, &pwdbuf) == 0 &&
      dotbuf.st_dev == pwdbuf.st_dev && dotbuf.st_ino == pwdbuf.st_ino)
    return n_strdup (pwd);

  if (max_len == 0)
    max_len = (N_PATH_LENGTH == -1) ? 2048 : N_PATH_LENGTH;

  while (max_len < N_MAXULONG / 2)
    {
      n_free (buffer);
      buffer = n_new (nchar, max_len + 1);
      *buffer = 0;
      dir = getcwd (buffer, max_len);

      if (dir || errno != ERANGE)
        break;

      max_len *= 2;
    }

  if (!dir || !*buffer)
    {
      /* hm, should we g_error() out here?
       * this can happen if e.g. "./" has mode \0000
       */
      buffer[0] = N_DIR_SEPARATOR;
      buffer[1] = 0;
    }

  dir = n_strdup (buffer);
  n_free (buffer);

  return dir;

}

static nchar *n_build_path_va (const nchar  *separator, const nchar  *first_element,va_list *args,nchar **str_array)
{
  NString *result;
  nint separator_len = strlen (separator);
  nboolean is_first = TRUE;
  nboolean have_leading = FALSE;
  const nchar *single_element = NULL;
  const nchar *next_element;
  const nchar *last_trailing = NULL;
  int i = 0;

  result = n_string_new ("");

  if (str_array)
    next_element = str_array[i++];
  else
    next_element = first_element;

  while (TRUE){
      const nchar *element;
      const nchar *start;
      const nchar *end;
      if (next_element){
	    element = next_element;
	    if (str_array)
	      next_element = str_array[i++];
	    else
	      next_element = va_arg (*args, nchar *);
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

	      n_string_append_len (result, element, start - element);
	      have_leading = TRUE;
	    }else
	      single_element = NULL;
	  }

      if (end == start)
	    continue;

      if (!is_first)
	   n_string_append (result, separator);

      n_string_append_len (result, start, end - start);
      is_first = FALSE;
   }

   if (single_element){
        n_string_free (result, TRUE);
        return n_strdup (single_element);
   }else{
      if (last_trailing)
	    n_string_append (result, last_trailing);
      return n_string_free (result, FALSE);
   }
}

static nchar *n_build_filename_va (const nchar  *first_argument,va_list *args,nchar **str_array)
{
  nchar *str;
  str = n_build_path_va (N_DIR_SEPARATOR_S, first_argument, args, str_array);
  return str;
}

nchar *n_build_filename (const nchar *first_element,  ...)
{
  nchar *str;
  va_list args;
  va_start (args, first_element);
  str = n_build_filename_va (first_element, &args, NULL);
  va_end (args);

  return str;
}

const nchar *g_path_skip_root (const nchar *file_name)
{
  n_return_val_if_fail (file_name != NULL, NULL);
  /* Skip initial slashes */
  if (N_IS_DIR_SEPARATOR (file_name[0])){
      while (N_IS_DIR_SEPARATOR (file_name[0]))
        file_name++;
      return (nchar *)file_name;
   }
   return NULL;
}

char *n_canonicalize_filename (const nchar *filename,const nchar *relative_to)
{
  char *canon, *input, *output, *after_root, *output_start;
  n_return_val_if_fail (relative_to == NULL || n_path_is_absolute (relative_to), NULL);
  if (!n_path_is_absolute (filename)){
      nchar *cwd_allocated = NULL;
      const nchar  *cwd;
      if (relative_to != NULL)
        cwd = relative_to;
      else
        cwd = cwd_allocated = n_get_current_dir ();

      canon = n_build_filename (cwd, filename, NULL);
      n_free (cwd_allocated);
   }else{
      canon = n_strdup (filename);
   }

   after_root = (char *)g_path_skip_root (canon);
   if (after_root == NULL){
      /* This shouldn't really happen, as g_get_current_dir() should
         return an absolute pathname, but bug 573843 shows this is
         not always happening */
      n_free (canon);
      return n_build_filename (N_DIR_SEPARATOR_S, filename, NULL);
    }

   /* Find the first dir separator and use the canonical dir separator. */
    for (output = after_root - 1; (output >= canon) && N_IS_DIR_SEPARATOR (*output);output--)
       *output = N_DIR_SEPARATOR;

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
      n_assert (input > canon && N_IS_DIR_SEPARATOR (input[-1]));
      n_assert (output > canon && N_IS_DIR_SEPARATOR (output[-1]));
      n_assert (input >= output);
      /* Ignore repeated dir separators. */
      while (N_IS_DIR_SEPARATOR (input[0]))
       input++;

      /* Ignore single dot directory components. */
      if (input[0] == '.' && (input[1] == 0 || N_IS_DIR_SEPARATOR (input[1]))){
           if (input[1] == 0)
             break;
           input += 2;
      }else if (input[0] == '.' && input[1] == '.' && (input[2] == 0 || N_IS_DIR_SEPARATOR (input[2]))){
          /* Remove double-dot directory components along with the preceding
           * path component. */
          if (output > output_start){
              do{
                  output--;
              }while (!N_IS_DIR_SEPARATOR (output[-1]) && output > output_start);
          }
          if (input[2] == 0)
            break;
          input += 3;
      }else{
          /* Copy the input to the output until the next separator,
           * while converting it to canonical separator */
          while (*input && !N_IS_DIR_SEPARATOR (*input))
            *output++ = *input++;
          if (input[0] == 0)
            break;
          input++;
          *output++ = N_DIR_SEPARATOR;
      }
   }
   /* Remove a potentially trailing dir separator */
   if (output > output_start && N_IS_DIR_SEPARATOR (output[-1]))
       output--;
   *output = '\0';
   return canon;
}


