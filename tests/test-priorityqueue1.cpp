// test_stl_pq.cpp
#include <iostream>
#include <queue>
#include <vector>
#include <chrono>
#include <cstdlib>

using namespace std;
using namespace chrono;

int main00() {
    const int N = 1000000;

    // 生成随机数据
    vector<int> data(N);
    for (int i = 0; i < N; ++i) {
        data[i] = rand();
    }

    // 创建 STL 最小堆（用 greater），如果要测最大堆把模板参数去掉即可
   // priority_queue<int, vector<int>, greater<int>> pq;
   // std::priority_queue<int, std::vector<int>, std::greater<int>> pq;
    std::vector<int> v;
    v.reserve(N);
    std::priority_queue<int, std::vector<int>, std::less<int>> pq(
        std::less<int>(), std::move(v));
    // ---- 测试 Push ----
    auto start_push = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        pq.push(data[i]);
    }
    auto end_push = high_resolution_clock::now();
    long long push_time = duration_cast<microseconds>(end_push - start_push).count();

    cout << "[STL priority_queue] Push " << N << " elements: "
         << push_time << " us (" << push_time / 1000.0 << " ms)" << endl;


    // ---- 测试 Pop ----
    int n = pq.size();
    auto start_pop = high_resolution_clock::now();
    while (n-->0) {
    //while (!pq.empty()) {
        pq.pop();
    }
    auto end_pop = high_resolution_clock::now();
    long long pop_time = duration_cast<microseconds>(end_pop - start_pop).count();

    cout << "[STL priority_queue] Pop  " << N << " elements: "
         << pop_time << " us (" << pop_time / 1000.0 << " ms)" << endl;

    return 0;
}
