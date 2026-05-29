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
#ifndef __AET_UTIL_A_QUEUE_H__
#define __AET_UTIL_A_QUEUE_H__

#include "../../aet.h"
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


