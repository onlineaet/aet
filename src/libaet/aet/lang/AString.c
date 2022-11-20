#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "AString.h"
#include "../util/AHashTable.h"
#include "../util/AMutex.h"

#define MY_MAXSIZE ((asize)-1)

static inline asize nearest_power (asize base, asize num)
{
  if (num > MY_MAXSIZE / 2){
     return MY_MAXSIZE;
  }else{
     asize n = base;
     while (n < num)
        n <<= 1;
     return n;
  }
}

/**
 * 把错误号转成字符串，因为要把字符串存在Hashtable中。所以需要锁。
 */
static AMutex strErrorLock=new$ AMutex();

impl$ AString{

  void maybeExpand (asize len){
      if (self->length + len >= self->allocatedLen){
        self->allocatedLen = nearest_power (1, self->length + len + 1);
        self->str = a_realloc (self->str, self->allocatedLen);
      }
  }

  void  append(const char  *val,assize len){
	  return insert (-1, val,len);
  }

  void  append(const char  *val){
	  return insert (-1, val, -1);
  }

  void append(char c){
      insert(-1,c);
  }

  void insert(assize pos,achar c){
    asize pos_unsigned;
    maybeExpand (1);
    if (pos < 0)
      pos = length;
    else{
        if(pos >length)
            return;
    }
    pos_unsigned = pos;
    /* If not just an append, move the old stuff */
    if (pos_unsigned < length)
      memmove (str + pos_unsigned + 1,str + pos_unsigned, length - pos_unsigned);

    str[pos_unsigned] = c;
    length += 1;
    str[length] = 0;
  }

  const char *getBytes(){
	  return str;
  }

  aint length(){
	  return length;
  }


  AString *upper(){
      achar *newStr= a_utf8_strup (str,-1);
      AString *newString=new$ AString(newStr);
      a_free(newStr);
      return newString;
  }

  AString *lower(){
      achar *newStr= a_utf8_strdown (str,-1);
      AString *newString=new$ AString(newStr);
      a_free(newStr);
      return newString;
  }

  int   charAt(int index){
	 if (index < 0 || index >= length) {
		  a_warning("给定的位置%d不在字符范围内！",index);
		  return -1;
	 }
	 char *re=a_utf8_offset_to_pointer(str,index);
	 auint32  c = a_utf8_get_char (re);
	 return c;
  }

  aboolean equals(const AString *comp){
	 if(comp==NULL)
	    return FALSE;
	 achar *p, *q;
	 asize i = self->length;
	 if (i != comp->length)
	    return FALSE;

	 p = self->str;
	 q = comp->str;
	 while (i){
	      if (*p != *q)
	        return FALSE;
	      p++;
	      q++;
	      i--;
	 }
	 return TRUE;
  }

  aboolean equals(const char *comp){
	 if(comp==NULL)
		return FALSE;
	 return strcmp(str,comp)==0;
  }

  aboolean equalsIgnoreCase(AString *anotherString){
	 if(anotherString==NULL)
	    return FALSE;
	 int n=length;
	 if(n!=anotherString->length)
		  return FALSE;
	 int i=0;
	 while(n-->0){
	    auint32 svalue= charAt(i);
	    auint32 cvalue= anotherString->charAt(i);
	    i++;
	    aunichar v1= a_unichar_toupper (svalue);
	    aunichar v2= a_unichar_toupper (cvalue);
	    if(v1==v2)
		   continue;
	    v1= a_unichar_tolower (svalue);
	    v2= a_unichar_tolower (cvalue);
	    if(v1==v2)
		   continue;
	    return FALSE;
	 }
	 return TRUE;
  }


  aboolean isEmpty() {
      return self->length == 0;
  }

  /**
   * 移走前和后的空格
   * char内存不变
   */
  void trim(){
      str=a_strstrip(str);
      self->length=strlen(str);
  }



  AString *truncate (asize len){
	  self->length = MIN (len, self->length);
	  self->str[self->length] = 0;
      return self;
  }


  AString *erase (assize pos,assize len){
    asize len_unsigned, pos_unsigned;
    a_return_val_if_fail (pos >= 0, self);
    pos_unsigned = pos;
    a_return_val_if_fail (pos_unsigned <= self->length, self);
    if (len < 0)
      len_unsigned = self->length - pos_unsigned;
    else
      {
        len_unsigned = len;
        a_return_val_if_fail (pos_unsigned + len_unsigned <= self->length, self);

        if (pos_unsigned + len_unsigned < self->length)
          memmove (self->str + pos_unsigned,
        		  self->str + pos_unsigned + len_unsigned,
				  self->length - (pos_unsigned + len_unsigned));
      }

    self->length -= len_unsigned;
    self->str[self->length] = 0;
    return self;
  }

  const char *toString(){
	  return str;
  }

  const char *getString(){
      return str;
  }


  void insert(assize pos,achar *val){
	  insert(pos,val,-1);
  }
  void insert(assize pos,const achar *val, assize len){
    asize len_unsigned, pos_unsigned;
    if (len == 0 || val == NULL)
    	return;
    if (len == 0)
      return;
    if (len < 0)
      len = strlen (val);
    len_unsigned = len;
    if (pos < 0)
      pos_unsigned = self->length;
    else{
        pos_unsigned = pos;
        if(pos_unsigned > self->length)
        	return;
    }
    /* Check whether val represents a substring of string.
     * This test probably violates chapter and verse of the C standards,
     * since ">=" and "<=" are only valid when val really is a substring.
     * In practice, it will work on modern archs.
     */
    if (A_UNLIKELY (val >= self->str && val <= self->str + self->length)){
        asize offset = val - self->str;
        asize precount = 0;
        maybeExpand (len_unsigned);
        val = self->str + offset;
        /* At this point, val is valid again.  */

        /* Open up space where we are going to insert.  */
        if (pos_unsigned < self->length)
          memmove (self->str + pos_unsigned + len_unsigned,self->str + pos_unsigned, self->length - pos_unsigned);

        /* Move the source part before the gap, if any.  */
        if (offset < pos_unsigned){
            precount = MIN (len_unsigned, pos_unsigned - offset);
            memcpy (self->str + pos_unsigned, val, precount);
         }

        /* Move the source part after the gap, if any.  */
        if (len_unsigned > precount)
          memcpy (self->str + pos_unsigned + precount,
                  val + /* Already moved: */ precount +
                        /* Space opened up: */ len_unsigned,
                  len_unsigned - precount);
    }else{
    	maybeExpand (len_unsigned);
        /* If we aren't appending at the end, move a hunk
         * of the old string to the end, opening up space
         */
        if (pos_unsigned < self->length)
          memmove (self->str + pos_unsigned + len_unsigned,self->str + pos_unsigned, self->length - pos_unsigned);

        /* insert the new string */
        if (len_unsigned == 1)
        	self->str[pos_unsigned] = *val;
        else
            memcpy (self->str + pos_unsigned, val, len_unsigned);
    }
    self->length += len_unsigned;
    self->str[self->length] = 0;
  }


  aboolean startsWith(const char *prefix,int toffset){
  	  char *temp=self->str+toffset;
  	  char * prefixStr = strstr (temp, prefix);
  	  if(!prefixStr)
  	   		return FALSE;
  	  return (prefixStr-temp)==0;
  }


  aboolean startsWith(const char *prefix){
	 	char * prefixStr = strstr (self->str, prefix);
	   	if(!prefixStr)
	   		return FALSE;
	   	return (prefixStr-self->str)==0;
  }

  aboolean endsWith(const char  *suffix){
 	 char * res = a_strrstr (self->str, suffix);
 	 if(!res)
 		 return FALSE;
      return ((res-self->str)+strlen(suffix)==self->length);
  }

  static int indexofFromNew(const char *str,int fromIndex)
  {
 	     if(str==NULL)
 	    	 return -1;
          if (fromIndex >= self->length)
              return (strlen(str) == 0 ? self->length : -1);
          if (fromIndex < 0)
              fromIndex = 0;

 	    char *temp=self->str+fromIndex;
 	    char *ttcc=strstr(temp,str);
 	    if(!ttcc)
 	    	return -1;
 	    return (ttcc-temp)+fromIndex;
  }



  int indexOf(const char *str,int fromIndex){
      return indexofFromNew(str,fromIndex);
  }

  int indexOf(const char *str){
 	 return indexofFromNew(str,0);
  }



  static achar *lastIndexOfString (const achar *haystack,const achar *needle,int from)
  {
    asize i;
    asize needle_len;
    asize haystack_len;
    const achar *p;

    needle_len = strlen (needle);
    haystack_len = strlen (haystack);

    if (needle_len == 0)
      return (achar *)haystack;

    if (haystack_len < needle_len)
      return NULL;

    p = haystack + haystack_len - needle_len;
    p = haystack + from+1- needle_len;

    while (p >= haystack)
      {
        for (i = 0; i < needle_len; i++)
          if (p[i] != needle[i])
            goto next;

        return (achar *)p;

      next:
        p--;
      }

    return NULL;
  }

  int lastIndexOf(const char *str)
  {
	    char *res= lastIndexOfString(self->str, str, self->length);
	    if(res==NULL)
	    	return -1;
	    return  (res-self->str);
  }

  int lastIndexOf(const char *str,int from)
  {
	    char *res= lastIndexOfString(self->str, str, from);
	    if(res==NULL)
	    	return -1;
	    return  (res-self->str);
  }

  static AString *substringNew(int beginIndex,int endIndex)
  {
      if (beginIndex < 0) {
          a_warning("给定的位置超出字符的的长度！%d",beginIndex);
          return NULL;
      }
      if (endIndex > strlen(self->str)) {
          a_warning("给定的位置超出字符的的长度！%d",endIndex);
          return NULL;
      }
      if ( endIndex-beginIndex<0) {
          a_warning("给定的位置超出字符的的长度！%d", endIndex-beginIndex);
          return NULL;
      }
      char *out = a_malloc (endIndex - beginIndex+1);
      memcpy (out, self->str+beginIndex, endIndex - beginIndex);
      out[endIndex - beginIndex] = '\0';
      AString *newStr=new$ AString(out);
      a_free(out);
      return newStr;
  }


  AString *substring(int begin,int end) {
  	   return substringNew(begin,end);
  }

  AString *substring(int begin){
  	return substring(begin,self->length);
  }

  void appendVprintf(const achar *format,va_list args)
  {
    achar *buf;
    aint len;
    a_return_if_fail (format != NULL);
    len = a_vasprintf (&buf, format, args);
    if (len >= 0){
        maybeExpand (len);
        memcpy (self->str + self->length, buf, len + 1);
        self->length += len;
        a_free (buf);
     }
  }

  void appendPrintf(const achar *format,...){
    va_list args;
    va_start (args, format);
    appendVprintf (format, args);
    va_end (args);
  }

  void vprintf (const achar *format,va_list args){
    truncate (0);
    appendVprintf (format, args);
  }

  void printf (const achar *format,...){
    va_list args;
    truncate (0);
    va_start (args, format);
    appendVprintf (format, args);
    va_end (args);
  }



  void initSize(asize dfl_size){
    self->allocatedLen = 0;
    self->length   = 0;
    self->str   = NULL;
    maybeExpand (MAX (dfl_size, 2));
    self->str[0] = 0;
    self->refStrCount=0;
  }

  AString (aint dfl_size){
	  initSize(dfl_size);
  }


  AString(aint8 *init,auint len){
	    if (len < 0)
	     	initSize(2);
	    else{
	  	  initSize(len);
	        if (init)
	          append((const char *)init, len);

	    }
  }

  AString(const char  *init){
     if (init == NULL || *init == '\0')
     	initSize(2);
     else{
         aint len;
         len = strlen (init);
         initSize(len + 2);
         append(init, len);
     }
   }

  /**
   * 调用了这句，不释放str
   */
  public$ char *unrefStr(){
      a_atomic_int_inc (&refStrCount);
      char *str=self->str;
      unref();
      return str;
  }


  ~AString(){
  	 if(str!=NULL){
  	   if(a_atomic_int_get(&refStrCount)==0){
  	     a_free(str);
  	     str=NULL;
  	   }
  	 }
  	 length=0;
  }

  /**
   * 错误号转成字符串。
   */
  static  const achar *errToStr(aint errnum){
    static AHashTable *errors=NULL;
    const achar *msg;
    aint saved_errno = errno;
    strErrorLock.lock();
    if (errors)
      msg = errors->get(AINT_TO_POINTER (errnum));
    else{
      errors = new$ AHashTable(NULL, NULL);
      msg = NULL;
    }

    if (!msg){
        achar buf[1024];
        a_strlcpy (buf, strerror (errnum), sizeof (buf));
        msg = buf;
//        if (!g_get_console_charset (NULL))
//          {
//            msg = a_locale_to_utf8 (msg, -1, NULL, NULL, &error);
//            if (error)
//              g_print ("%s\n", error->message);
//          }
//        else if (msg == (const achar *)buf)
          msg = a_strdup (buf);
        errors->put(AINT_TO_POINTER (errnum), (char *) msg);
     }
      strErrorLock.unlock();
      errno = saved_errno;
      return msg;
   }

};
