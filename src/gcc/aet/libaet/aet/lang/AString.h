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

#ifndef __AET_LANG_A_STRING_H__
#define __AET_LANG_A_STRING_H__

#include "../../aet.h"

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

