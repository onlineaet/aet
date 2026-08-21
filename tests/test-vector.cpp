#include <vector>
#include <chrono>
#include <iostream>
#include <cstdio>
#include <random>
#include <algorithm>

using namespace std;
using namespace std::chrono;

const size_t N_PUSH     = 100000000;  // 1亿
const size_t N_INSERT   = 150000;     // 15万中间插入
const size_t N_DELETE   = 500000;     // 50万删除测试

long long ms(high_resolution_clock::time_point s, high_resolution_clock::time_point e) {
    return duration_cast<microseconds>(e - s).count();
}

int main() {
    // ========== 1. 顺序添加（无预分配）==========
    {
        auto s = high_resolution_clock::now();
        vector<int> v;
        for (size_t i = 0; i < N_PUSH; ++i) v.push_back((int)i);
        auto e = high_resolution_clock::now();
        printf("[vector] 顺序添加 %zu 个: %lld us  (size=%zu, cap=%zu)\n",
               N_PUSH, ms(s,e), v.size(), v.capacity());
    }

    // ========== 2. 预分配添加 ==========
    {
        auto s = high_resolution_clock::now();
        vector<int> v;
        v.reserve(N_PUSH);
        for (size_t i = 0; i < N_PUSH; ++i) v.push_back((int)i);
        auto e = high_resolution_clock::now();
        printf("[vector] 预分配添加 %zu 个: %lld us  (size=%zu, cap=%zu)\n",
               N_PUSH, ms(s,e), v.size(), v.capacity());
    }

    // ========== 3. 中间插入 ==========
    {
        vector<int> v;
        v.reserve(N_INSERT * 2);
        for (int i = 0; i < 10000; ++i) v.push_back(i);  // 先放一点底

        auto s = high_resolution_clock::now();
        for (size_t i = 0; i < N_INSERT; ++i) {
            size_t pos = v.size() / 2;
            v.insert(v.begin() + pos, (int)i);
        }
        auto e = high_resolution_clock::now();
        printf("[vector] 中间插入 %zu 次: %lld us  (最终size=%zu)\n",
               N_INSERT, ms(s,e), v.size());
    }

    // ========== 4. 删除测试 ==========
    {
        vector<int> v;
        v.reserve(N_DELETE + 100000);
        for (size_t i = 0; i < N_DELETE + 100000; ++i) v.push_back((int)i);

        auto s = high_resolution_clock::now();
        // 从中间反复删除
        while (v.size() > 100000) {
            size_t pos = v.size() / 2;
            v.erase(v.begin() + pos);
        }
        auto e = high_resolution_clock::now();
        printf("[vector] 中间删除到剩10万: %lld us  (最终size=%zu)\n",
               ms(s,e), v.size());
    }

    // ========== 5. 尾部删除（最快路径）==========
    {
        vector<int> v(N_DELETE);
        auto s = high_resolution_clock::now();
       // int ab = 0;
        volatile int sum=0;

        while (!v.empty()) {
           sum += v.back();
           v.pop_back();
        }
        auto e = high_resolution_clock::now();
        printf("[vector] 尾部删除 %zu 个: %lld us sum %d\n", N_DELETE, ms(s,e),sum);

    }

    return 0;
}
