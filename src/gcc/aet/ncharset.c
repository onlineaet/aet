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

#include "ncharset.h"

#include "nhash.h"
#include "nlog.h"
#include "nstrfuncs.h"
#include "nmem.h"
#include "nptrarray.h"

#include "localcharset.h"
#include <string.h>
#include <stdio.h>



static NHashTable *get_alias_hash (void)
{
  static NHashTable *alias_hash = NULL;
  const char *aliases;

 // N_LOCK (aliases);

  if (!alias_hash)
    {
      alias_hash = n_hash_table_new (n_str_hash, n_str_equal);

      aliases = _n_locale_get_charset_aliases ();
      while (*aliases != '\0')
        {
          const char *canonical;
          const char *alias;
          const char **alias_array;
          int count = 0;

          alias = aliases;
          aliases += strlen (aliases) + 1;
          canonical = aliases;
          aliases += strlen (aliases) + 1;

          alias_array = n_hash_table_lookup (alias_hash, canonical);
          if (alias_array)
            {
              while (alias_array[count])
                count++;
            }

          alias_array = n_renew (const char *, alias_array, count + 2);
          alias_array[count] = alias;
          alias_array[count + 1] = NULL;

          n_hash_table_insert (alias_hash, (char *)canonical, alias_array);
        }
    }

  //N_UNLOCK (aliases);

  return alias_hash;
}

/* As an abuse of the alias table, the following routines gets
 * the charsets that are aliases for the canonical name.
 */
const char ** _n_charset_get_aliases (const char *canonical_name)
{
  NHashTable *alias_hash = get_alias_hash ();
  return n_hash_table_lookup (alias_hash, canonical_name);
}

static const nchar *n_env_get_env (const nchar *variable)
{
  n_return_val_if_fail (variable != NULL, NULL);
  return getenv (variable);
}


static nboolean n_utf8_get_charset_internal (const char  *raw_data,const char **a)
{
  const char *charset = n_env_get_env ("CHARSET");
  if (charset && *charset)
    {
      *a = charset;

      if (charset && strstr (charset, "UTF-8"))
        return TRUE;
      else
        return FALSE;
    }

  /* The libcharset code tries to be thread-safe without
   * a lock, but has a memory leak and a missing memory
   * barrier, so we lock for it
   */
 // N_LOCK (aliases);
  charset = _n_locale_charset_unalias (raw_data);
 // N_UNLOCK (aliases);

  if (charset && *charset)
    {
      *a = charset;

      if (charset && strstr (charset, "UTF-8"))
        return TRUE;
      else
        return FALSE;
    }

  /* Assume this for compatibility at present.  */
  *a = "US-ASCII";

  return FALSE;
}

typedef struct _NCharsetCache NCharsetCache;

struct _NCharsetCache {
  nboolean is_utf8;
  nchar *raw;
  nchar *charset;
};

static void charset_cache_free (npointer data)
{
  NCharsetCache *cache = data;
  n_free (cache->raw);
  n_free (cache->charset);
  n_free (cache);
}


nboolean n_get_charset (const char **charset)
{
  //static NThreadSpecific cache_private = N_THREAD_SPECIFIC_INIT (charset_cache_free);
  static NCharsetCache *cache =NULL;// n_thread_specific_get (&cache_private);
  const nchar *raw;

  if (!cache)
    cache = n_malloc0(sizeof (NCharsetCache));//n_thread_specific_set_alloc0 (&cache_private, sizeof (NCharsetCache));

  //N_LOCK (aliases);
  raw = _n_locale_charset_raw();
 // N_UNLOCK (aliases);

  if (cache->raw == NULL || strcmp (cache->raw, raw) != 0)
    {
      const nchar *new_charset;
      n_free (cache->raw);
      n_free (cache->charset);
      cache->raw = n_strdup (raw);
      cache->is_utf8 = n_utf8_get_charset_internal (raw, &new_charset);
      cache->charset = n_strdup (new_charset);
    }

  if (charset)
    *charset = cache->charset;

  return cache->is_utf8;
}


nchar *n_get_codeset (void)
{
  const nchar *charset;
  n_get_charset (&charset);
  return n_strdup (charset);
}

nboolean n_get_console_charset (const char **charset)
{

  /* assume the locale settings match the console encoding on non-Windows OSs */
  return n_get_charset (charset);
}


/* read an alias file for the locales */
static void read_aliases (const nchar *file,NHashTable  *alias_table)
{
  FILE *fp;
  char buf[256];

  fp = fopen (file,"r");
  if (!fp)
    return;
  while (fgets (buf, 256, fp))
    {
      char *p, *q;

      n_strstrip (buf);

      /* Line is a comment */
      if ((buf[0] == '#') || (buf[0] == '\0'))
        continue;

      /* Reads first column */
      for (p = buf, q = NULL; *p; p++) {
        if ((*p == '\t') || (*p == ' ') || (*p == ':')) {
          *p = '\0';
          q = p+1;
          while ((*q == '\t') || (*q == ' ')) {
            q++;
          }
          break;
        }
      }
      /* The line only had one column */
      if (!q || *q == '\0')
        continue;

      /* Read second column */
      for (p = q; *p; p++) {
        if ((*p == '\t') || (*p == ' ')) {
          *p = '\0';
          break;
        }
      }

      /* Add to alias table if necessary */
      if (!n_hash_table_lookup (alias_table, buf)) {
        n_hash_table_insert (alias_table, n_strdup (buf), n_strdup (q));
      }
    }
  fclose (fp);
}


static char * unalias_lang (char *lang)
{
  static NHashTable *alias_table = NULL;
  char *p;
  int i;

  //if (n_once_init_enter (&alias_table))
      if (alias_table==NULL)

    {
      NHashTable *table = n_hash_table_new (n_str_hash, n_str_equal);
      read_aliases ("/usr/share/locale/locale.alias", table);
      alias_table=table;
      //n_once_init_leave (&alias_table, table);
    }

  i = 0;
  while ((p = n_hash_table_lookup (alias_table, lang)) && (strcmp (p, lang) != 0))
    {
      lang = p;
      if (i++ == 30)
        {
          static nboolean said_before = FALSE;
          if (!said_before)
            n_warning ("Too many alias levels for a locale, "
                       "may indicate a loop");
          said_before = TRUE;
          return lang;
        }
    }
  return lang;
}

/* Mask for components of locale spec. The ordering here is from
 * least significant to most significant
 */
enum
{
  COMPONENT_CODESET =   1 << 0,
  COMPONENT_TERRITORY = 1 << 1,
  COMPONENT_MODIFIER =  1 << 2
};

/* Break an X/Open style locale specification into components
 */
static nuint
explode_locale (const nchar *locale,
                nchar      **language,
                nchar      **territory,
                nchar      **codeset,
                nchar      **modifier)
{
  const nchar *uscore_pos;
  const nchar *at_pos;
  const nchar *dot_pos;

  nuint mask = 0;

  uscore_pos = strchr (locale, '_');
  dot_pos = strchr (uscore_pos ? uscore_pos : locale, '.');
  at_pos = strchr (dot_pos ? dot_pos : (uscore_pos ? uscore_pos : locale), '@');

  if (at_pos)
    {
      mask |= COMPONENT_MODIFIER;
      *modifier = n_strdup (at_pos);
    }
  else
    at_pos = locale + strlen (locale);

  if (dot_pos)
    {
      mask |= COMPONENT_CODESET;
      *codeset = n_strndup (dot_pos, at_pos - dot_pos);
    }
  else
    dot_pos = at_pos;

  if (uscore_pos)
    {
      mask |= COMPONENT_TERRITORY;
      *territory = n_strndup (uscore_pos, dot_pos - uscore_pos);
    }
  else
    uscore_pos = dot_pos;

  *language = n_strndup (locale, uscore_pos - locale);

  return mask;
}


static void append_locale_variants (NPtrArray *array,const nchar *locale)
{
  nchar *language = NULL;
  nchar *territory = NULL;
  nchar *codeset = NULL;
  nchar *modifier = NULL;

  nuint mask;
  nuint i, j;

  n_return_if_fail (locale != NULL);

  mask = explode_locale (locale, &language, &territory, &codeset, &modifier);

  /* Iterate through all possible combinations, from least attractive
   * to most attractive.
   */
  for (j = 0; j <= mask; ++j)
    {
      i = mask - j;

      if ((i & ~mask) == 0)
        {
          nchar *val = n_strconcat (language,
                                    (i & COMPONENT_TERRITORY) ? territory : "",
                                    (i & COMPONENT_CODESET) ? codeset : "",
                                    (i & COMPONENT_MODIFIER) ? modifier : "",
                                    NULL);
          n_ptr_array_add (array, val);
        }
    }

  n_free (language);
  if (mask & COMPONENT_CODESET)
    n_free (codeset);
  if (mask & COMPONENT_TERRITORY)
    n_free (territory);
  if (mask & COMPONENT_MODIFIER)
    n_free (modifier);
}

/**
 * n_get_locale_variants:
 * @locale: a locale identifier
 *
 * Returns a list of derived variants of @locale, which can be used to
 * e.g. construct locale-dependent filenames or search paths. The returned
 * list is sorted from most desirable to least desirable.
 * This function handles territory, charset and extra locale modifiers. See
 * [`setlocale(3)`](man:setlocale) for information about locales and their format.
 *
 * @locale itself is guaranteed to be returned in the output.
 *
 * For example, if @locale is `fr_BE`, then the returned list
 * is `fr_BE`, `fr`. If @locale is `en_GB.UTF-8@euro`, then the returned list
 * is `en_GB.UTF-8@euro`, `en_GB.UTF-8`, `en_GB@euro`, `en_GB`, `en.UTF-8@euro`,
 * `en.UTF-8`, `en@euro`, `en`.
 *
 * If you need the list of variants for the current locale,
 * use n_get_language_names().
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly
 *   allocated array of newly allocated strings with the locale variants. Free with
 *   n_strfreev().
 *
 * Since: 2.28
 */
nchar **n_get_locale_variants (const nchar *locale)
{
  NPtrArray *array;

  n_return_val_if_fail (locale != NULL, NULL);

  array = n_ptr_array_sized_new (8);
  append_locale_variants (array, locale);
  n_ptr_array_add (array, NULL);

  return (nchar **) n_ptr_array_free (array, FALSE);
}

/* The following is (partly) taken from the gettext package.
   Copyright (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.  */

static const nchar *guess_category_value (const nchar *category_name)
{
  const nchar *retval;

  /* The highest priority value is the 'LANGUAGE' environment
     variable.  This is a GNU extension.  */
  retval = n_env_get_env ("LANGUAGE");
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  /* 'LANGUAGE' is not set.  So we have to proceed with the POSIX
     methods of looking to 'LC_ALL', 'LC_xxx', and 'LANG'.  On some
     systems this can be done by the 'setlocale' function itself.  */

  /* Setting of LC_ALL overwrites all other.  */
  retval = n_env_get_env ("LC_ALL");
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  /* Next comes the name of the desired category.  */
  retval = n_env_get_env (category_name);
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  /* Last possibility is the LANG environment variable.  */
  retval = n_env_get_env ("LANG");
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  return NULL;
}

typedef struct _NLanguageNamesCache NLanguageNamesCache;

struct _NLanguageNamesCache {
  nchar *languages;
  nchar **language_names;
};

static void language_names_cache_free (npointer data)
{
  NLanguageNamesCache *cache = data;
  n_free (cache->languages);
  n_strfreev (cache->language_names);
  n_free (cache);
}


const nchar * const *n_get_language_names (void)
{
  return n_get_language_names_with_category ("LC_MESSAGES");
}


const nchar * const *n_get_language_names_with_category (const nchar *category_name)
{
   //static NThreadSpecific cache_private = N_THREAD_SPECIFIC_INIT ((void (*)(npointer)) n_hash_table_unref);
   static NHashTable *cache =NULL;// n_thread_specific_get (&cache_private);

  const nchar *languages;
  NLanguageNamesCache *name_cache;

  n_return_val_if_fail (category_name != NULL, NULL);

  if (!cache)
    {
      cache = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, language_names_cache_free);
      //n_thread_specific_set (&cache_private, cache);
    }

  languages = guess_category_value (category_name);
  if (!languages)
    languages = "C";

  name_cache = (NLanguageNamesCache *) n_hash_table_lookup (cache, category_name);
  if (!(name_cache && name_cache->languages &&
        strcmp (name_cache->languages, languages) == 0))
    {
      NPtrArray *array;
      nchar **alist, **a;

      n_hash_table_remove (cache, category_name);

      array = n_ptr_array_sized_new (8);

      alist = n_strsplit (languages, ":", 0);
      for (a = alist; *a; a++)
        append_locale_variants (array, unalias_lang (*a));
      n_strfreev (alist);
      n_ptr_array_add (array, n_strdup ("C"));
      n_ptr_array_add (array, NULL);

      name_cache = n_new0 (NLanguageNamesCache, 1);
      name_cache->languages = n_strdup (languages);
      name_cache->language_names = (nchar **) n_ptr_array_free (array, FALSE);
      n_hash_table_insert (cache, n_strdup (category_name), name_cache);
    }

  return (const nchar * const *) name_cache->language_names;
}

