//#include "AArray.h"
#include <aet/util/AArray.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define N_PUSH     100000000UL
#define N_INSERT   150000UL
#define N_DELETE   500000UL

double now_ms() {
    return (double)clock() * 1000000.0 / CLOCKS_PER_SEC;
}

int main() {
    // ========== 1. 顺序添加（无预分配）==========
    {
        double s = now_ms();
        AArray<int> *a = new$ AArray<int>(16);
        for (int i = 0; i < N_PUSH; ++i)
            a->add(i);
        double e = now_ms();
        printf("[AArray] 顺序添加 %lu 个: %.1f us  (size=%u)\n",
               N_PUSH, e - s, a->size());
        a->unref();
    }

    // ========== 2. 预分配添加 ==========
    {
        AArray<int> *a = new$ AArray<int>(N_PUSH);   // 直接指定容量
        double s = now_ms();
        for (int i = 0; i < N_PUSH; ++i)
            a->addFast(i);
        double e = now_ms();
        printf("[AArray] 预分配添加 %lu 个: %.1f us  (size=%u)\n",
               N_PUSH, e - s, a->size());
        a->unref();
    }

    // ========== 3. 中间插入 ==========
    {
        AArray<int> *a = new$ AArray<int>(10000);
        for (int i = 0; i < 10000; ++i) a->add(i);

        double s = now_ms();
        for (int i = 0; i < N_INSERT; ++i) {
            int pos = a->size() / 2;
            a->insert(i, pos);
        }
        double e = now_ms();
        printf("[AArray] 中间插入 %lu 次: %.1f us  (最终size=%u)\n",
               N_INSERT, e - s, a->size());
        a->unref();
    }

    // ========== 4. 中间删除 ==========
    {
        AArray<int> *a = new$ AArray<int>(N_DELETE + 100000);
        for (int i = 0; i < N_DELETE + 100000; ++i)
            a->add(i);

        double s = now_ms();
        while (a->size() > 100000) {
            int pos = a->size() / 2;
            a->remove(pos);
        }
        double e = now_ms();
        printf("[AArray] 中间删除到剩10万: %.1f us  (最终size=%u)\n",
               e - s, a->size());
        a->unref();
    }

    // ========== 5. 尾部删除 ==========
    {
        AArray<int> *a = new$ AArray<int>(N_DELETE);
        for (int  i = 0; i < N_DELETE; ++i) a->add(i);
        volatile int sum=0;
        double s = now_ms();
        while (!a->isEmpty()) {
            sum+=a->back();
            a->popBack();
        }
        double e = now_ms();
        printf("[AArray] 尾部删除 %lu 个: %.1f us\n", N_DELETE, e - s);
        a->unref();
    }

    return 0;
}
