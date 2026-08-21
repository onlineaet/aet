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

#ifndef __AET_UTIL_A_ARRAY_H__
#define __AET_UTIL_A_ARRAY_H__

#include "../../aet.h"


package$ aet.util;

/**
 * elementSize 每个元素的大小 比如：int的大小是sizeof(int)
 * capacity 预分配多少个元素的空间
 * 如果元素是指针，保存的是指针地址，而不是指针指向地址的内容。
 */
public$ class$ AArray<E>{
    private$ auint elementSize;//E的大小
    private$ ADestroyNotify clearFunc;
    private$ aboolean isPointer;
    private$ aboolean haveZero;//是否清零数据

    private$ E *start;           // 起始位置 (begin)
    private$ E *finish;          // 当前写位置
    private$ E *end_of_storage;  // 容量结束位置

    public$ AArray(auint capacity);
    public$ AArray(auint capacity,ADestroyNotify clearFunc);

    public$ void addFirst(E data);
    public$ void add(E value);
    public$ void addFast(E value);

    public$ auint getESize ();

    public$ void remove (auint index);
    public$ void remove (auint index,auint removeCount);
    public$ void removeRange (auint index,auint removeCount);
    public$ aboolean removeData(E data);
    public$ void removeAll();
    public$ void setSize(auint newEleCount);

    public$ E get(int index);
    public$ auint size();
    public$ void insert(E data, int index);
    public$ aboolean isEmpty();
    public$ void foreach (AFunc func,apointer userData);
    public$ void sort(ACompareFunc compareFunc);
    public$ void sort(ACompareFunc compareFunc,apointer userData);
    private$ void maybeExpand(auint eleCount);
    public$ void popBack();
    public$ E back();
    private$ void clear(int index);
    //新分配的内存是否清零
    public$ void setClearZero(aboolean need);

    public$ ~AArray();

};



#endif /* __N_MEM_H__ */


