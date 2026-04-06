#include <benchmark/benchmark.h>
#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include "ventra/vector/concurrent_v1_vector.hpp"

// =====================================================================
// TEST 1: Primitive Typen (int) - Der Malloc-Flaschenhals
// =====================================================================

static void BM_StdVector_Int(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static std::vector<int> shared_std_vec;
    static std::mutex mtx;

    if (state.thread_index() == 0) {
        shared_std_vec.clear();
        shared_std_vec.reserve(elements_per_thread * state.threads());
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            shared_std_vec.push_back(i);
        }
        benchmark::DoNotOptimize(shared_std_vec.data());
    }
}
BENCHMARK(BM_StdVector_Int)->Arg(1000)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(12);

static void BM_VentraVector_Int(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_v1_vector<int>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_v1_vector<int>();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            shared_concurrent_vec->push_back(i);
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(BM_VentraVector_Int)->Arg(1000)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(12);


// =====================================================================
// TEST 2: Komplexe Typen (std::string) - Die echte Lock Contention
// =====================================================================

static void BM_StdVector_String(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static std::vector<std::string> shared_std_vec;
    static std::mutex mtx;

    if (state.thread_index() == 0) {
        shared_std_vec.clear();
        shared_std_vec.reserve(elements_per_thread * state.threads());
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::string heavy_payload = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i);
            
            std::lock_guard<std::mutex> lock(mtx);
            shared_std_vec.push_back(std::move(heavy_payload));
        }
        benchmark::DoNotOptimize(shared_std_vec.data());
    }
}
BENCHMARK(BM_StdVector_String)->Arg(1000)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(12);

static void BM_VentraVector_String(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_v1_vector<std::string>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_v1_vector<std::string>();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::string heavy_payload = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i);
            
            shared_concurrent_vec->push_back(std::move(heavy_payload));
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(BM_VentraVector_String)->Arg(1000)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(12);