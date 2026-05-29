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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "KeyFileUtil.h"


impl$ KeyFileUtil{

    static aboolean isComment(const char *line){
      return (*line == '#' || *line == '\0' || *line == '\n');
    }

    /* A group in a key file is made up of a starting '[' followed by one
     * or more letters making up the group name followed by ']'.
     */
    static aboolean isGroup (const char *line){
      achar *p;
      p = (achar *) line;
      if (*p != '[')
        return FALSE;
      p++;
      while (*p && *p != ']')
        p = a_utf8_find_next_char (p, NULL);
      if (*p != ']')
        return FALSE;
      /* silently accept whitespace after the ] */
      p = a_utf8_find_next_char (p, NULL);
      while (*p == ' ' || *p == '\t')
        p = a_utf8_find_next_char (p, NULL);

      if (*p)
        return FALSE;
      return TRUE;
    }

    static aboolean isGroupName(const char *name){
      char *p, *q;
      if (name == NULL)
        return FALSE;
      p = q = (char *) name;
      while (*q && *q != ']' && *q != '[' && !a_ascii_iscntrl (*q))
        q = a_utf8_find_next_char (q, NULL);

      if (*q != '\0' || q == p)
        return FALSE;

      return TRUE;
    }

    static aboolean isKeyValuePair(const char *line){
      char *p;
      p = (char *) a_utf8_strchr (line, -1, '=');
      if (!p)
        return FALSE;
      /* Key must be non-empty
       */
      if (*p == line[0])
        return FALSE;
      return TRUE;
    }

    static aboolean isKeyName(const char *name){
      char *p, *q;
      if (name == NULL)
        return FALSE;
      p = q = (char *) name;
      /* We accept a little more than the desktop entry spec says,
       * since gnome-vfs uses mime-types as keys in its cache.
       */
      while (*q && *q != '=' && *q != '[' && *q != ']')
        q = a_utf8_find_next_char (q, NULL);

      /* No empty keys, please */
      if (q == p)
        return FALSE;

      /* We accept spaces in the middle of keys to not break
       * existing apps, but we don't tolerate initial or final
       * spaces, which would lead to silent corruption when
       * rereading the file.
       */
      if (*p == ' ' || q[-1] == ' ')
        return FALSE;

      if (*q == '['){
          q++;
          while (*q && (a_unichar_isalnum (a_utf8_get_char_validated (q, -1)) || *q == '-' || *q == '_' || *q == '.' || *q == '@'))
            q = a_utf8_find_next_char (q, NULL);
          if (*q != ']')
            return FALSE;
          q++;
      }
      if (*q != '\0')
        return FALSE;
      return TRUE;
    }

    static char *getLocal(const char *key){
      char *locale;
      locale = a_strrstr (key, "[");
      if (locale && strlen (locale) <= 2)
        locale = NULL;

      if (locale)
        locale = a_strndup (locale + 1, strlen (locale) - 2);

      return locale;
    }
};


