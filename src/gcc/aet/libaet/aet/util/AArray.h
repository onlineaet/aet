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
 * data 保存元素的内存
 * len  所有元素的个数
 * capacity 预分配多少个元素的空间
 * alloc 实际分配的内存
 * 如果元素是指针，保存的是指针地址，而不是指针指向地址的内容。
 */


public$ class$ AArray<E>{
    private$ auint elementSize;//一个元素的大小
    private$ aint8 *data;
    private$ auint len; //所有元素的个数
    private$ auint zero_terminated:1;
    private$ auint clear : 1;
    private$ auint capacity;
    private$ auint alloc;
    private$ ADestroyNotify clearFunc;
    private$ auint elementCount;
    private$ aboolean isPointer;

    public$ AArray(auint capacity);
    public$ AArray(auint capacity,ADestroyNotify clearFunc);

    public$ void addFirst(E data);
    public$ auint getElementSize ();

    public$ void remove (auint index);
    public$ void remove (auint index,auint removeCount);
    public$ aboolean remove(E data);
    public$ void removeAll();

    public$ void add(E value);
    public$ E get(int index);
    public$ auint size();
    public$ void insert(E data, int index);
    public$ aboolean isEmpty();
    public$ void foreach (AFunc func,apointer userData);
    public$ void sort(ACompareFunc compareFunc);
    public$ void sort(ACompareFunc compareFunc,apointer userData);

    public$ ~AArray();

};




#endif /* __N_MEM_H__ */


