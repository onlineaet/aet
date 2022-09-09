

#ifndef __A_ERROR_H__
#define __A_ERROR_H__

#include <stdarg.h>
#include "abase.h"

typedef struct _AError AError;

struct _AError
{
  auint       domain;
  aint        code;
  achar       *message;
};

AError*  a_error_new_printf(auint domain,aint  code, const achar   *format,...) ;
AError*  a_error_new(auint domain,aint    code,const achar   *message);
AError*  a_error_new_valist(auint domain, aint   code, const achar   *format, va_list args);
void     a_error_free(AError        *error);
AError*  a_error_copy(const AError  *error);
aboolean a_error_matches(const AError  *error, auint domain, aint code);

void     a_error_printf(AError  **err,auint domain, aint code,const achar *format,...) ;
void     a_error_set(AError  **err,auint domain, aint code,const achar  *message);

/* 转移src到dest.
 */
void     a_error_transfer(AError  **dest,AError *src);




#endif /* __A_ERROR_H__ */

