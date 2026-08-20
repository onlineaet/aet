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

#ifndef __AET_UTIL_A_LIST_H__
#define __AET_UTIL_A_LIST_H__

#include "../../aet.h"

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

