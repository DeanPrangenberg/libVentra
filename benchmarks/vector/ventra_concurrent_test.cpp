#include <benchmark/benchmark.h>
#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include <algorithm>
#include "ventra/vector/concurrent_atomic_vector.hpp"
#include "ventra/vector/concurrent_smart_vector.hpp"

static void BenchmarkConfig(benchmark::internal::Benchmark* b) {
    b->Arg(100000)
     ->Arg(10000)
     ->Arg(1000)
     ->Arg(100)
     ->Threads(1)
     ->Threads(2)
     ->Threads(4)
     ->Threads(8)
     ->Threads(12)
     ->Threads(16)
     ->Threads(24)
     ->Threads(32)
     ->Threads(48)
     ->Threads(64)
     ->Repetitions(10)
     ->ReportAggregatesOnly(true);
}

// =====================================================================
// TEST 1: Primitive Typ (int) Push Back reserved size
// =====================================================================

static void Vector_StdVectorSimpleMutex_Int_push_back_reserved(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static std::vector<int> shared_std_vec;
    static std::mutex mtx;

    if (state.thread_index() == 0) {
        shared_std_vec.clear();
        shared_std_vec.reserve(elements_per_thread * state.threads());
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::lock_guard lock(mtx);
            shared_std_vec.push_back(i);
        }
        benchmark::DoNotOptimize(shared_std_vec.data());
    }
}
BENCHMARK(Vector_StdVectorSimpleMutex_Int_push_back_reserved)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_Int_push_back_reserved(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_smart_vector<int>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_smart_vector<int>(elements_per_thread * state.threads());
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            shared_concurrent_vec->push_back(i);
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(Vector_VentraConcurrentSmartVector_Int_push_back_reserved)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentAtomicVector_Int_push_back_reserved(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_atomic_vector<int>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_atomic_vector<int>(elements_per_thread * state.threads());
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            shared_concurrent_vec->push_back(i);
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(Vector_VentraConcurrentAtomicVector_Int_push_back_reserved)->Apply(BenchmarkConfig);

// =====================================================================
// TEST 2: Complexe Typ (string 100char long + seed) Push Back
// =====================================================================

static void Vector_StdVectorSimpleMutex_String_push_back_reserved(benchmark::State& state) {
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

            std::lock_guard lock(mtx);
            shared_std_vec.push_back(std::move(heavy_payload));
        }
        benchmark::DoNotOptimize(shared_std_vec.data());
    }
}
BENCHMARK(Vector_StdVectorSimpleMutex_String_push_back_reserved)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_String_push_back_reserved(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_smart_vector<std::string>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_smart_vector<std::string>(elements_per_thread * state.threads());
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::string heavy_payload = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i);

            shared_concurrent_vec->push_back(std::move(heavy_payload));
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(Vector_VentraConcurrentSmartVector_String_push_back_reserved)->Apply(BenchmarkConfig);


// =====================================================================
// TEST 3: Primitive Typ (int) Push Back
// =====================================================================

static void Vector_StdVectorSimpleMutex_Int_push_back(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static std::vector<int> shared_std_vec;
    static std::mutex mtx;

    if (state.thread_index() == 0) {
        shared_std_vec.clear();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::lock_guard lock(mtx);
            shared_std_vec.push_back(i);
        }
        benchmark::DoNotOptimize(shared_std_vec.data());
    }
}
BENCHMARK(Vector_StdVectorSimpleMutex_Int_push_back)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_Int_push_back(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_smart_vector<int>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_smart_vector<int>();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            shared_concurrent_vec->push_back(i);
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(Vector_VentraConcurrentSmartVector_Int_push_back)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentAtomicVector_Int_push_back(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_atomic_vector<int>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_atomic_vector<int>();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            shared_concurrent_vec->push_back(i);
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(Vector_VentraConcurrentAtomicVector_Int_push_back)->Apply(BenchmarkConfig);


// =====================================================================
// TEST 4: Complexe Typ (string 100char long + seed) Push Back
// =====================================================================

static void Vector_StdVectorSimpleMutex_String_push_back(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static std::vector<std::string> shared_std_vec;
    static std::mutex mtx;

    if (state.thread_index() == 0) {
        shared_std_vec.clear();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::string heavy_payload = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i);

            std::lock_guard lock(mtx);
            shared_std_vec.push_back(std::move(heavy_payload));
        }
        benchmark::DoNotOptimize(shared_std_vec.data());
    }
}
BENCHMARK(Vector_StdVectorSimpleMutex_String_push_back)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_String_push_back(benchmark::State& state) {
    const int elements_per_thread = state.range(0);
    static ventra::concurrent_smart_vector<std::string>* shared_concurrent_vec = nullptr;

    if (state.thread_index() == 0) {
        if (shared_concurrent_vec != nullptr) delete shared_concurrent_vec;
        shared_concurrent_vec = new ventra::concurrent_smart_vector<std::string>();
    }

    for (auto _ : state) {
        for (int i = 0; i < elements_per_thread; ++i) {
            std::string heavy_payload = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i);

            shared_concurrent_vec->push_back(std::move(heavy_payload));
        }
        benchmark::DoNotOptimize(shared_concurrent_vec);
    }
}
BENCHMARK(Vector_VentraConcurrentSmartVector_String_push_back)->Apply(BenchmarkConfig);