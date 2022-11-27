

#ifndef __AET_UTIL_A_LIST_H__
#define __AET_UTIL_A_LIST_H__

#include <stdarg.h>
#include <objectlib.h>

package$ aet.util;

typedef struct _ListNode ListNode;
struct _ListNode{
      apointer data;
      ListNode *next;
      ListNode *prev;
};

public$ class$ ListIterator{
    private$  ListNode *node;
    private$  ListNode *prevNode;

    public$ aboolean  hasNext(apointer *value);
    public$ ListNode *getNode();
    public$ ListNode *getPrevNode();

};

public$ class$ AList{
    public$ static ListNode *next(ListNode *node);
    public$ static ListNode *prev(ListNode *node);
    public$ static ListNode *last (ListNode *node);
    public$ static apointer  nodeData(ListNode *node);
    public$ static void      nodeFree(ListNode *node);

    private$ int size;
    private$ ListNode  *first;
    protected$ ListNode  *last;
    private$ ADestroyNotify  destoryItemFunc;
    private$ ListIterator *iter;
    public$ AList();
    public$ AList(ADestroyNotify fun);
    public$ void     add(apointer value);
    public$ void     addFirst(apointer value);
    public$ void     addLast(apointer value);
    public$ auint    length();
    public$ aboolean remove(apointer data);
    public$ apointer remove(int index);
    public$ aboolean removeNode(ListNode *node);
    public$ apointer removeLast();
    public$ apointer removeFirst();
    public$ auint    removeAll(apointer data);

    public$ apointer getFirst();
    public$ apointer getLast();
    public$ apointer get(int index);
    public$ int      indexOf(apointer data);
    public$ int      indexOf(ListNode *node);//节点位置
    public$ void     insert(int index, apointer element);
    public$ void     insertSorted(apointer data,ACompareFunc  func);
    public$ void     insertSorted(apointer data,ACompareDataFunc func,apointer userData);
    public$ void     foreach (AFunc func,apointer  userData);
    public$ void     setDestroyFunc(ADestroyNotify func);
    public$ ListIterator *initIter();
    public$ void     clear();
    public$ void     clear(ADestroyNotify func);
    public$ void     sort(ACompareFunc compareFunc);
    public$ void     sort(ACompareDataFunc compareFunc,apointer userData);
    public$ aboolean find(aconstpointer data);
    public$ aboolean find(aconstpointer data,ACompareFunc func);
    public$  void    reverse();


};




#endif /* __N_MEM_H__ */

