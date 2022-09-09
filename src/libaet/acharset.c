#include <string.h>
#include <stdio.h>
#include <langinfo.h>

#include "acharset.h"
#include "alog.h"
#include "astrfuncs.h"
#include "aslice.h"
#include "amem.h"
#include "athreadspecific.h"
#include "innerlock.h"






#define relocate(pathname) (pathname)

#define ALIB_CHARSETALIAS_DIR "/home/ai/" //LIBDIR



#ifndef DIRECTORY_SEPARATOR
# define DIRECTORY_SEPARATOR '/'
#endif

#ifndef ISSLASH
# define ISSLASH(C) ((C) == DIRECTORY_SEPARATOR)
#endif



/* The following static variable is declared 'volatile' to avoid a
   possible multithread problem in the function get_charset_aliases. If we
   are running in a threaded environment, and if two threads initialize
   'charset_aliases' simultaneously, both will produce the same value,
   and everything will be ok if the two assignments to 'charset_aliases'
   are atomic. But I don't know what will happen if the two assignments mix.  */
#if __STDC__ != 1
# define volatile /* empty */
#endif
/* Pointer to the contents of the charset.alias file, if it has already been
   read, else NULL.  Its format is:
   ALIAS_1 '\0' CANONICAL_1 '\0' ... ALIAS_n '\0' CANONICAL_n '\0' '\0'  */
static const char * volatile charset_aliases;

/* Return a pointer to the contents of the charset.alias file.  */
static const char *_a_locale_get_charset_aliases (void)
{
  const char *cp;

  cp = charset_aliases;
  if (cp == NULL)    {
      FILE *fp;
      const char *dir;
      const char *base = "charset.alias";
      char *file_name;

      dir = relocate (ALIB_CHARSETALIAS_DIR);

      /* Concatenate dir and base into freshly allocated file_name.  */
      {
	    size_t dir_len = strlen (dir);
	    size_t base_len = strlen (base);
	    int add_slash = (dir_len > 0 && !ISSLASH (dir[dir_len - 1]));
	    file_name = (char *) malloc (dir_len + add_slash + base_len + 1);
	    if (file_name != NULL){
	       memcpy (file_name, dir, dir_len);
	       if (add_slash)
	          file_name[dir_len] = DIRECTORY_SEPARATOR;
	       memcpy (file_name + dir_len + add_slash, base, base_len + 1);
	    }
      }

      if (file_name == NULL || (fp = fopen (file_name, "r")) == NULL)
	/* Out of memory or file not found, treat it as empty.  */
	    cp = "";
      else{
	     /* Parse the file's contents.  */
	     char *res_ptr = NULL;
	     size_t res_size = 0;
	     for (;;){
	        int c;
	        char buf1[50+1];
	        char buf2[50+1];
	        size_t l1, l2;
	        char *old_res_ptr;
	        c = getc (fp);
	        if (c == EOF)
		       break;
	        if (c == '\n' || c == ' ' || c == '\t')
		       continue;
	        if (c == '#'){
		      /* Skip comment, to end of line.  */
		       do
		         c = getc (fp);
		       while (!(c == EOF || c == '\n'));
		       if (c == EOF)
		         break;
		       continue;
		    }
	        ungetc (c, fp);
	        if (fscanf (fp, "%50s %50s", buf1, buf2) < 2)
		       break;
	        l1 = strlen (buf1);
	        l2 = strlen (buf2);
	        old_res_ptr = res_ptr;
	        if (res_size == 0){
		      res_size = l1 + 1 + l2 + 1;
		      res_ptr = (char *) malloc (res_size + 1);
		    }else{
		      res_size += l1 + 1 + l2 + 1;
		      res_ptr = (char *) realloc (res_ptr, res_size + 1);
		    }
	        if (res_ptr == NULL){
		      /* Out of memory. */
		      res_size = 0;
		      if (old_res_ptr != NULL)
		        free (old_res_ptr);
		      break;
		    }
	        strcpy (res_ptr + res_size - (l2 + 1) - (l1 + 1), buf1);
	        strcpy (res_ptr + res_size - (l2 + 1), buf2);
	    }
	    fclose (fp);
	    if (res_size == 0)
	      cp = "";
	    else {
	      *(res_ptr + res_size) = '\0';
	      cp = res_ptr;
	    }
	}

      if (file_name != NULL)
	     free (file_name);

      charset_aliases = cp;
  }
  return cp;
}

typedef struct _HashTable{
   char *keys[10];
   int keyCount;
   char *values[10];
}HashTable;

static char **lookup(HashTable *hash,char *key)
{
	int i;
	for(i=0;i<hash->keyCount;i++){
		if(strcmp(hash->keys[i],key)==0)
			return (char **)hash->values[i];
	}
	return NULL;
}

static void insert(HashTable *hash,char *key,char **value)
{
	int i;
	for(i=0;i<hash->keyCount;i++){
		if(strcmp(hash->keys[i],key)==0){
			hash->values[i]=value;
			return;
		}
	}
	int pos=hash->keyCount;
	hash->values[pos]=value;
	hash->keyCount++;
}

INNER_LOCK_DEFINE_STATIC (aliases);

static HashTable *get_alias_hash (void)
{
  static HashTable *alias_hash = NULL;
  const char *aliases;
  INNER_LOCK (aliases);
  if (!alias_hash){
      alias_hash = a_slice_new0(HashTable);
      aliases = _a_locale_get_charset_aliases ();
      while (*aliases != '\0'){
          const char *canonical;
          const char *alias;
          const char **alias_array;
          int count = 0;

          alias = aliases;
          aliases += strlen (aliases) + 1;
          canonical = aliases;
          aliases += strlen (aliases) + 1;
          alias_array = lookup (alias_hash, canonical);
          if (alias_array){
              while (alias_array[count])
                count++;
          }

          alias_array = a_renew (const char *, alias_array, count + 2);
          alias_array[count] = alias;
          alias_array[count + 1] = NULL;
          insert (alias_hash, (char *)canonical, alias_array);
        }
    }

  INNER_UNLOCK (aliases);

  return alias_hash;
}

/* As an abuse of the alias table, the following routines gets
 * the charsets that are aliases for the canonical name.
 */
const char ** _a_charset_get_aliases (const char *canonical_name)
{
  HashTable *alias_hash = get_alias_hash();
  return (const char **)lookup (alias_hash, canonical_name);
}

static aboolean a_utf8_get_charset_internal (const char  *raw_data,const char **a)
{
  const char *charset = getenv ("CHARSET");
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
  INNER_LOCK (aliases);
  charset = _a_locale_charset_unalias (raw_data);
  INNER_UNLOCK (aliases);

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

typedef struct _ACharsetCache ACharsetCache;

struct _ACharsetCache {
  aboolean is_utf8;
  achar *raw;
  achar *charset;
};

static void charset_cache_free (apointer data)
{
  ACharsetCache *cache = data;
  a_free (cache->raw);
  a_free (cache->charset);
  a_free (cache);
}

static const char *_a_locale_charset_raw (void)
{
  const char *codeset;
  /* Most systems support nl_langinfo (CODESET) nowadays.  */
  codeset = nl_langinfo (CODESET);
  return codeset;
}



aboolean a_get_charset (const char **charset)
{
  static AThreadSpecific cache_private = {NULL,(charset_cache_free)};
  ACharsetCache *cache = a_thread_specific_get (&cache_private);
  const achar *raw;

  if (!cache)
    cache = a_thread_specific_set_alloc0 (&cache_private, sizeof (ACharsetCache));

  INNER_LOCK (aliases);
  raw = _a_locale_charset_raw();
  INNER_UNLOCK (aliases);

  if (cache->raw == NULL || strcmp (cache->raw, raw) != 0)
    {
      const achar *new_charset;
      a_free (cache->raw);
      a_free (cache->charset);
      cache->raw = a_strdup (raw);
      cache->is_utf8 = a_utf8_get_charset_internal (raw, &new_charset);
      cache->charset = a_strdup (new_charset);
    }

  if (charset)
    *charset = cache->charset;

  return cache->is_utf8;
}


achar *a_get_codeset (void)
{
  const achar *charset;
  a_get_charset (&charset);
  return a_strdup (charset);
}

aboolean a_get_console_charset (const char **charset)
{

  /* assume the locale settings match the console encoding on non-Windows OSs */
  return a_get_charset (charset);
}


const char *_a_locale_charset_unalias (const char *codeset)
{
  const char *aliases;

  if (codeset == NULL)
    /* The canonical name cannot be determined.  */
    codeset = "";

  /* Resolve alias. */
  for (aliases = _a_locale_get_charset_aliases ();*aliases != '\0';aliases += strlen (aliases) + 1, aliases += strlen (aliases) + 1){
	  printf("_n_locale_charset_unalias 00 codeset:%s ---aliases:%s\n",codeset,aliases);

    if (strcmp (codeset, aliases) == 0	|| (aliases[0] == '*' && aliases[1] == '\0')){
	   codeset = aliases + strlen (aliases) + 1;
	   break;
    }
  }
  /* Don't return an empty string.  GNU libc and GNU libiconv interpret
     the empty string as denoting "the locale's character encoding",
     thus GNU libiconv would call this function a second time.  */
  if (codeset[0] == '\0')
    codeset = "ASCII";
  return codeset;
}








