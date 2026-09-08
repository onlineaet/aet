#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef LOCAL_HEAD
#include "APriorityQueue.h"
#else
#include <aet/util/APriorityQueue.h>
#endif

// 比较函数：最小堆
static aboolean intComparator_cb(aconstpointer  a,aconstpointer  b)
{
   int ia = *(int*)a;
   int ib = *(int*)b;
   return ia>ib;
}

int main() {
    const int N = 1000000;

    // 准备数据（栈上数组，取地址传给 APriorityQueue）
    int* data = (int*)malloc(N * sizeof(int));
    for (int i = 0; i < N; ++i) {
        data[i] = rand();
    }

    // 创建优先队列（最小堆）
    APriorityQueue<int,MinHeapFunc> *apq = new$ APriorityQueue(N,intComparator_cb);
    // ---- 测试 Push ----
    clock_t start_push = clock();
    for (int i = 0; i < N; ++i) {
        apq->push(data[i]);
    }
    clock_t end_push = clock();
    double push_time = ((double)(end_push - start_push)) / CLOCKS_PER_SEC * 1000.0;
    printf("[APriorityQueue] Push %d elements: %.3f ms\n", N, push_time);
    apq->validData();
   // ---- 测试 Pop ----
    int n=apq->size();
    clock_t start_pop = clock();
    //while (!apq->isEmpty()) {
    while (n-->0) {
        apq->pop();
    }
    clock_t end_pop = clock();
    double pop_time = ((double)(end_pop - start_pop)) / CLOCKS_PER_SEC * 1000.0;
    printf("[APriorityQueue] Pop  %d elements: %.3f ms\n", N, pop_time);
    free(data);
    return 0;
}
