

#ifndef __AET_UTIL_A_QUEUE_H__
#define __AET_UTIL_A_QUEUE_H__

#include <stdarg.h>
#include <objectlib.h>
#include "AList.h"


package$ aet.util;


public$ class$ AQueue{
      private$ AList    *list;
      private$ uint      length;
      public$ aboolean   isEmpty();
      public$ auint      length ();
      /**
       * 先进先出。
       */
      public$ void push(apointer  data);
      /**
       * 数据加前面
       */
      public$ void     pushHead(apointer  data);
      public$ void     push(apointer data,int n);
      public$ apointer pop();
      public$ apointer pop(int n);
      public$ apointer popLast();
      public$ void     foreach(AFunc func,apointer userData);
      public$ aboolean remove(apointer data);
      public$ auint    removeAll(apointer data);
      public$ apointer peek();
      public$ apointer peek (auint index);
      public$ apointer peekLast();
      public$ void     clear(ADestroyNotify func);
      public$ void     clear();
      public$ AQueue  *clone();
      public$ void     sort(ACompareDataFunc compareFunc,apointer userData);
      public$ aboolean find(aconstpointer data);
      public$ aboolean find(aconstpointer data,ACompareFunc func);
      public$ apointer peekLastLink();
      public$ void     insertSorted(apointer data,ACompareDataFunc func,apointer userData);
      public$ void     reverse();
      public$ int      indexOf(apointer data);

};




#endif /* __N_MEM_H__ */


