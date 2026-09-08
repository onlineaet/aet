
#ifndef __AET_UTIL_A_PRIORITY_QUEUE_H__
#define __AET_UTIL_A_PRIORITY_QUEUE_H__

#include "../../aet.h"

package$ aet.util;

typedef aboolean   (*ProrityCompare) (aconstpointer a,aconstpointer b);
//把比较函数作用泛型类型，主要是可以生成不同的push和pop代码
typedef aboolean   (*MaxHeapFunc) (aconstpointer a,aconstpointer b);
typedef aboolean   (*MinHeapFunc) (aconstpointer a,aconstpointer b);

public$ class$ APriorityQueue<E,F>{
   private$ auint  elementSize;//E的大小
   private$ final$ F compareFunc;
   public$ E *start;           // 起始位置 (begin)
   public$ E *finish;          // 当前写位置
   private$ E *end_of_storage;  // 容量结束位置
   //保证用户不能使用缺省的构造函数，必须使用带比较方法的构造函数
   private$ APriorityQueue();
   public$  APriorityQueue(int initialCapacity,F comparator);
   public$  APriorityQueue(F comparator);

   public$ void push(E e);
   public$ E pop();
   public$ E top();
   //获取元素个数
   public$ int size();
   public$ aboolean isEmpty();

   public$ void validData();
   public$ void printArray();
   private$ void maybeExpand(auint eleCount);
};

#endif /* __AET_UTIL_A_PRIORITY_QUEUE_H__ */

