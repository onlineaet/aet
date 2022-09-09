

#ifndef __AET_UTIL_A_SLIST_H__
#define __AET_UTIL_A_SLIST_H__

#include <stdarg.h>
#include <objectlib.h>

package$ aet.util;

typedef struct _SListNode SListNode;

struct _SListNode{
      apointer data;
      SListNode *next;
};

public$ class$ ASList{
    private$ int size;
    private$ SListNode  *head;
    private$ ADestroyNotify  destoryItemFunc;
    public$ ASList();
    public$ ASList(ADestroyNotify fun);
    public$ ~ASList();
    public$ void     add(apointer value);
    public$ void     addFirst(apointer value);
    public$ auint    length();
    public$ aboolean remove(aconstpointer data);
    public$ auint    removeAll(aconstpointer data);
    public$ apointer getFirst();
    public$ apointer getLast();
    public$ apointer get(int index);
    public$ int      indexOf(apointer data);
    public$ void     insert(int index,apointer element);
    public$ void     foreach (AFunc func,apointer  userData);
    public$ void     reverse();
    public$ void     sort(ACompareFunc compareFunc);
    public$ void     sort(ACompareDataFunc compareFunc,apointer userData);
    public$ void     insertSorted(apointer data,ACompareFunc  func);
    public$ void     insertSorted(apointer data,ACompareDataFunc func,apointer userData);
    public$ aboolean find(aconstpointer data);
    public$ aboolean find(aconstpointer data,ACompareFunc func);
    public$ void     setDestroyFunc(ADestroyNotify func);

};




#endif /* __N_MEM_H__ */

