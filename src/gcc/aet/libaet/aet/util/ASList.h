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

#ifndef __AET_UTIL_A_SLIST_H__
#define __AET_UTIL_A_SLIST_H__

#include "../../aet.h"

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

