

#ifndef __A_CHARSET_H__
#define __A_CHARSET_H__


#include "abase.h"

aboolean        a_get_console_charset (const char **charset);
const char **  _a_charset_get_aliases (const char *canonical_name);
achar          *a_get_codeset (void);
aboolean        a_get_charset (const char **charset);
const char    *_a_locale_charset_unalias (const char *codeset);


#endif  /* __A_CHARSET_H__ */

