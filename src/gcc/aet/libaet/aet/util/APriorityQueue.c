
#include <assert.h>
#include "APriorityQueue.h"

#define DEFAULT_INITIAL_CAPACITY 16
#define MIN_ARRAY_SIZE  16
#define element_count()  (( (char*)finish - (char*)start ) / elementSize)

static auint nearestPow (auint num)
{
   auint n = num - 1;
   //a_assert (num > 0);
   n |= n >> 1;
   n |= n >> 2;
   n |= n >> 4;
   n |= n >> 8;
   n |= n >> 16;
   return n + 1;
}

impl$ APriorityQueue
{
   public$ APriorityQueue(F comparator) {
      self(DEFAULT_INITIAL_CAPACITY, comparator);
   }

   public$ APriorityQueue(int initialCapacity,F comparator) {
      if (initialCapacity < 1){
         a_error("初始化数据大小的参数值要大于1：当前:%d",initialCapacity);
      }
      if(comparator == NULL)
         a_error("比较函数不能为空");
      self->elementSize     = sizeof(E);
      self->compareFunc = comparator;
      maybeExpand((auint)initialCapacity);
   }

   void maybeExpand(auint eleCount) {
       // 当前已有元素个数
       auint currentCount = element_count();   // 即 (finish - start) / elementSize
       // 需要的总元素个数
       auint needCount = currentCount + eleCount;
       // 当前容量（元素个数）
       auint currentCapacity = 0;
       if (start != NULL)
           currentCapacity = ((char*)end_of_storage - (char*)start) / elementSize;
       // 如果容量足够，直接返回
       if (needCount <= currentCapacity)
           return;

       // 检查溢出
       if (A_UNLIKELY((A_MAXUINT - currentCount) < eleCount)) {
           a_error("加 %u 到数组溢出。\n", eleCount);
           return;
       }
       // 计算新的容量（字节）
       auint want_alloc = elementSize * needCount;
       want_alloc = nearestPow(want_alloc);
       want_alloc = MAX(want_alloc, MIN_ARRAY_SIZE);
       // 重新分配
       void *new_array = a_realloc(start, want_alloc);
       // 更新三个指针
       auint oldSizeBytes = (char*)finish - (char*)start;
       start          = new_array;
       finish         =(E *) ((char*)start + oldSizeBytes);
       end_of_storage = (E *)((char*)start + want_alloc);
   }

   private$ APriorityQueue() {
      a_error("APriorityQueue 不能调用缺省的构行函数。");
   }

   E get(int index){
      return genericblock$(index) {
         return start[index];  // 直接通过 start 指针访问
      };
   }

   void printArray(){
      int size =size();
      int i;
      for(i=0;i<size;i++){
         E v=get(i);
         printf("array data i:%d size:%d v:%d\n",i,size,*((int*)v));
      }
   }

   /**
    * 检查数据是否正确
    */
   void validData(){
      int size =size();
      for (int i = 0; i < size; i++) {
         int left = 2 * i + 1;
         int right = 2 * i + 2;
         if (left < size)
            assert(!((ProrityCompare)compareFunc)(get(i), get(left)));
         if (right < size)
            assert(!((ProrityCompare)compareFunc)(get(i), get(right)));
      }
   }

   public$ void push(E x) {
      genericblock$(x){
         if(finish<end_of_storage){
            label:
            E * astart = (E *)start;
            int k = (E *)finish - astart;
            while (k > 0) {
               //计算下标为 k 的节点的父节点下标，结果存到 parent 变量里。>>1相当于除2
               //父节点公式 parent=(k-1)/2,转为unsigned int
               //目的是保证使用逻辑右移（高位补 0），而不是有符号数可能出现的算术右移（高位补符号位），增加代码的安全性和可移植性。
               int parent = ((unsigned int)(k - 1)) >> 1;
               E e = astart[parent];
               if(((ProrityCompare)compareFunc)(&x,&e))
                  break ;
               astart[k] = e;
               k = parent;
            }
            astart[k] = x;
            finish =(void **)((char *)finish+sizeof(E));
         }else{
            maybeExpand(1);
            goto label;
         }
      };
   }

   //获取元素个数
   int size(){
      return  genericblock$() {
         return (( (char*)finish - (char*)start ) / sizeof(E));
      };
   }

   public$ E top(){
      return genericblock$(){
        return ((E *)start)[0];
      };
   }

   public$ E  pop() {
      if(finish<start)
         return NULL;

      return genericblock$(){
         E *  astart = (E *)start;
         E *  afinish = (E *)finish;
         E first =astart[0];
         unsigned n = afinish-astart-1;
         E last = astart[n];
         finish =(void **)(afinish-1);
         //取出最后一个数据，array的大小减1
         unsigned half = n >> 1;
         unsigned k =0;
         while (k < half) {
            int child = (k << 1) + 1;
            E c = astart[child];
            int right = child + 1;

            if (right < n) {
               E r = astart[right];
               if(((ProrityCompare)compareFunc)(&c,&r)){
                  child = right;
                  c = r;
               }
            }
            if(!((ProrityCompare)compareFunc)(&last,&c))
               break;
            astart[k]= c;
            k = child;
         }
         astart[k]= last;
         return first;
      };
   }

   aboolean isEmpty(){
      return finish==start;
   }

};
