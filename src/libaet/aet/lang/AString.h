

#ifndef __AET_LANG_A_STRING_H__
#define __AET_LANG_A_STRING_H__

#include <stdarg.h>
#include <objectlib.h>

package$ aet.lang;

public$ class$ AString{
  public$ static  const achar *errToStr(aint errnum);//错误号转成字符串

  private$ achar *str;
  private$ auint allocatedLen;
  private$ aint  length;
  volatile aint  refStrCount;

  public$ AString(const char *str);
  public$ AString(aint8 *data,auint len);
  public$ AString(aint dfl_size);


  public$ ~AString();
  public$ void append(const char *value);
  public$ void append(const char *val,assize len);
  public$ void append(char c);


  public$ aint length();
  public$ const char *getBytes();
  public$ void insert(assize pos,achar *val);
  public$ void insert(assize pos,const achar *val, assize len);
  public$ void insert(assize pos,achar c);

  public$ void appendVprintf(const achar *format,va_list args);
  public$ void appendPrintf(const achar *format, ...);
  public$ void printf (const achar *format,...);
  public$ void vprintf (const achar *format,va_list args);

  public$ AString *upper();
  public$ AString *lower();
  public$ int      charAt(int index);
  public$ aboolean equals(const AString *comp);
  public$ aboolean equals(const char *comp);
  public$ aboolean equalsIgnoreCase(AString *anotherString);
  public$ aboolean startsWith(const char *prefix);
  public$ aboolean startsWith(const char *prefix,int from);
  public$ aboolean endsWith(const char  *suffix);
  public$ int      indexOf(const char *str,int fromIndex);
  public$ int      indexOf(const char *str);
  public$ int      lastIndexOf(const char *str, int fromIndex);
  public$ int      lastIndexOf(const char *str);
  public$ aboolean isEmpty();
  public$ AString *substring(int begin);
  public$ AString *substring(int begin,int end);
  public$ void     trim();
  public$ AString *truncate (asize remain);
  public$ AString *erase (assize pos,assize len);
  public$ const char *toString();
  public$ const char *getString();
  public$ char       *unrefStr();//调用了这句，不释放str 但释放对象。


};




#endif /* __N_MEM_H__ */

