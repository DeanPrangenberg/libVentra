#include <benchmark/benchmark.h>
#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include <memory>
#include <array>
#include <barrier>
#include "ventra/vector/concurrent_atomic_vector.hpp"
#include "ventra/vector/concurrent_smart_vector.hpp"

static void BenchmarkConfig(benchmark::internal::Benchmark *b) {
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
// Shared States
// =====================================================================

template<typename T>
struct AtomicVectorState {
    std::unique_ptr<ventra::concurrent_atomic_vector<T> > vec;
    std::array<std::unique_ptr<std::barrier<> >, 65> barriers{};
    std::array<std::once_flag, 65> barrier_once{};

    std::barrier<> &barrier_for(const int thread_count) {
        std::call_once(barrier_once[thread_count], [this, thread_count] {
            barriers[thread_count] = std::make_unique<std::barrier<> >(thread_count);
        });

        return *barriers[thread_count];
    }
};

AtomicVectorState<int> atomicIntVecState;

template<typename T>
struct SmartVectorState {
    std::unique_ptr<ventra::concurrent_smart_vector<T> > vec;
    std::array<std::unique_ptr<std::barrier<> >, 65> barriers{};
    std::array<std::once_flag, 65> barrier_once{};

    std::barrier<> &barrier_for(const int thread_count) {
        std::call_once(barrier_once[thread_count], [this, thread_count] {
            barriers[thread_count] = std::make_unique<std::barrier<> >(thread_count);
        });

        return *barriers[thread_count];
    }
};

SmartVectorState<int> smartIntVecState;
SmartVectorState<std::string> smartStringVecState;

template<typename T>
struct StdVectorState {
    std::unique_ptr<std::vector<T> > vec;
    std::mutex mtx;
    std::array<std::unique_ptr<std::barrier<> >, 65> barriers{};
    std::array<std::once_flag, 65> barrier_once{};

    std::barrier<> &barrier_for(const int thread_count) {
        std::call_once(barrier_once[thread_count], [this, thread_count] {
            barriers[thread_count] = std::make_unique<std::barrier<> >(thread_count);
        });

        return *barriers[thread_count];
    }
};

StdVectorState<int> stdIntVecState;
StdVectorState<std::string> stdStringVecState;

// =====================================================================
// TEST 1: Primitive Typ (int) Push Back reserved size
// =====================================================================

static void Vector_StdVectorSimpleMutex_Int_push_back_reserved(benchmark::State &state) {
    const int items_per_thread = state.range(0);
    const std::size_t total_items = static_cast<std::size_t>(items_per_thread) * static_cast<std::size_t>(state.
                                        threads());

    auto &barrier = stdIntVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            stdIntVecState.vec = std::make_unique<std::vector<int> >();
            stdIntVecState.vec->reserve(total_items);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        const int value_base = items_per_thread * state.thread_index();
        for (int i = 0; i < items_per_thread; ++i) {
            std::lock_guard lock(stdIntVecState.mtx);
            stdIntVecState.vec->push_back(value_base + i);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(stdIntVecState.vec->size());
            benchmark::ClobberMemory();
            stdIntVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_StdVectorSimpleMutex_Int_push_back_reserved)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_Int_push_back_reserved(benchmark::State &state) {
    const int items_per_thread = state.range(0);
    const std::size_t total_items = static_cast<std::size_t>(items_per_thread) * static_cast<std::size_t>(state.
                                        threads());

    auto &barrier = smartIntVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            smartIntVecState.vec = std::make_unique<ventra::concurrent_smart_vector<int> >(total_items);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        const int value_base = items_per_thread * state.thread_index();
        for (int i = 0; i < items_per_thread; ++i) {
            smartIntVecState.vec->push_back(value_base + i);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(smartIntVecState.vec->size());
            benchmark::ClobberMemory();
            smartIntVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_VentraConcurrentSmartVector_Int_push_back_reserved)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentAtomicVector_Int_push_back_reserved(benchmark::State &state) {
    const int items_per_thread = state.range(0);
    const std::size_t total_items = static_cast<std::size_t>(items_per_thread) * static_cast<std::size_t>(state.
                                        threads());

    auto &barrier = atomicIntVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            atomicIntVecState.vec = std::make_unique<ventra::concurrent_atomic_vector<int> >(total_items);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        const int value_base = items_per_thread * state.thread_index();
        for (int i = 0; i < items_per_thread; ++i) {
            atomicIntVecState.vec->push_back(value_base + i);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(atomicIntVecState.vec->size());
            benchmark::ClobberMemory();
            atomicIntVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_VentraConcurrentAtomicVector_Int_push_back_reserved)->Apply(BenchmarkConfig);

// =====================================================================
// TEST 2: Complexe Typ (string 100char long + seed) Push Back
// =====================================================================

static void Vector_StdVectorSimpleMutex_String_push_back_reserved(benchmark::State &state) {
    const int items_per_thread = state.range(0);
    const std::size_t total_items = static_cast<std::size_t>(items_per_thread) * static_cast<std::size_t>(state.
                                        threads());

    auto &barrier = stdStringVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            stdStringVecState.vec = std::make_unique<std::vector<std::string> >();
            stdStringVecState.vec->reserve(total_items);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        for (int i = 0; i < items_per_thread; ++i) {
            std::lock_guard lock(stdStringVecState.mtx);
            stdStringVecState.vec->push_back(
            "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i)
            );
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(stdStringVecState.vec->size());
            benchmark::ClobberMemory();
            stdStringVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_StdVectorSimpleMutex_String_push_back_reserved)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_String_push_back_reserved(benchmark::State &state) {
    const int items_per_thread = state.range(0);
    const std::size_t total_items = static_cast<std::size_t>(items_per_thread) * static_cast<std::size_t>(state.
                                        threads());

    auto &barrier = smartStringVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            smartStringVecState.vec = std::make_unique<ventra::concurrent_smart_vector<std::string> >(total_items);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        for (int i = 0; i < items_per_thread; ++i) {
            smartStringVecState.vec->push_back(
            "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i)
            );
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(smartStringVecState.vec->size());
            benchmark::ClobberMemory();
            smartStringVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_VentraConcurrentSmartVector_String_push_back_reserved)->Apply(BenchmarkConfig);


// =====================================================================
// TEST 3: Primitive Typ (int) Push Back
// =====================================================================

static void Vector_StdVectorSimpleMutex_Int_push_back(benchmark::State &state) {
    const int items_per_thread = state.range(0);

    auto &barrier = stdIntVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            stdIntVecState.vec = std::make_unique<std::vector<int> >();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        const int value_base = items_per_thread * state.thread_index();
        for (int i = 0; i < items_per_thread; ++i) {
            std::lock_guard lock(stdIntVecState.mtx);
            stdIntVecState.vec->push_back(value_base + i);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(stdIntVecState.vec->size());
            benchmark::ClobberMemory();
            stdIntVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_StdVectorSimpleMutex_Int_push_back)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_Int_push_back(benchmark::State &state) {
    const int items_per_thread = state.range(0);

    auto &barrier = smartIntVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            smartIntVecState.vec = std::make_unique<ventra::concurrent_smart_vector<int> >();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        const int value_base = items_per_thread * state.thread_index();
        for (int i = 0; i < items_per_thread; ++i) {
            smartIntVecState.vec->push_back(value_base + i);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(smartIntVecState.vec->size());
            benchmark::ClobberMemory();
            smartIntVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_VentraConcurrentSmartVector_Int_push_back)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentAtomicVector_Int_push_back(benchmark::State &state) {
    const int items_per_thread = state.range(0);

    auto &barrier = atomicIntVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            atomicIntVecState.vec = std::make_unique<ventra::concurrent_atomic_vector<int> >();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        const int value_base = items_per_thread * state.thread_index();
        for (int i = 0; i < items_per_thread; ++i) {
            atomicIntVecState.vec->push_back(value_base + i);
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(atomicIntVecState.vec->size());
            benchmark::ClobberMemory();
            atomicIntVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_VentraConcurrentAtomicVector_Int_push_back)->Apply(BenchmarkConfig);


// =====================================================================
// TEST 4: Complexe Typ (string 100char long + seed) Push Back
// =====================================================================

static void Vector_StdVectorSimpleMutex_String_push_back(benchmark::State &state) {
    const int items_per_thread = state.range(0);

    auto &barrier = stdStringVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            stdStringVecState.vec = std::make_unique<std::vector<std::string> >();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        for (int i = 0; i < items_per_thread; ++i) {
            std::lock_guard lock(stdStringVecState.mtx);
            stdStringVecState.vec->push_back(
            "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i)
            );
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(stdStringVecState.vec->size());
            benchmark::ClobberMemory();
            stdStringVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_StdVectorSimpleMutex_String_push_back)->Apply(BenchmarkConfig);

static void Vector_VentraConcurrentSmartVector_String_push_back(benchmark::State &state) {
    const int items_per_thread = state.range(0);

    auto &barrier = smartStringVecState.barrier_for(state.threads());

    for (auto _: state) {
        state.PauseTiming();
        // Init Vector
        if (state.thread_index() == 0) {
            smartStringVecState.vec = std::make_unique<ventra::concurrent_smart_vector<std::string> >();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();

        // Operation
        for (int i = 0; i < items_per_thread; ++i) {
            smartStringVecState.vec->push_back(
            "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyzAB" + std::to_string(i)
            );
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.PauseTiming();

        // Settings and Reset before next iteration
        if (state.thread_index() == 0) {
            benchmark::DoNotOptimize(smartStringVecState.vec->size());
            benchmark::ClobberMemory();
            smartStringVecState.vec.reset();
        }

        // Wait for all threads
        barrier.arrive_and_wait();
        state.ResumeTiming();
    }

    if (state.thread_index() == 0) {
        state.SetItemsProcessed(
            static_cast<int64_t>(state.iterations()) *
            static_cast<int64_t>(items_per_thread) *
            static_cast<int64_t>(state.threads())
        );
    }
}

BENCHMARK(Vector_VentraConcurrentSmartVector_String_push_back)->Apply(BenchmarkConfig);
