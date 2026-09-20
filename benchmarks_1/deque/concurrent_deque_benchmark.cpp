//
// Created by deanprangenberg on 04/21/2026.
//

#include <algorithm>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <benchmark/benchmark.h>
#include <ventra/deque/concurrent_deque.hpp>

namespace {

constexpr std::size_t max_threads = 64;
constexpr std::uint32_t deque_capacity = 65536;
constexpr std::size_t usable_capacity = deque_capacity - 1;

constexpr int repetitions = 30;
constexpr double min_time_seconds = 1.0;
constexpr double warmup_time_seconds = 0.5;

template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
using bench_deque = ventra::concurrent_deque<
    int,
    max_threads,
    2,
    deque_capacity,
    Arena
>;


static void apply_common_config(
    benchmark::internal::Benchmark* benchmark
) {
    benchmark
        ->Repetitions(repetitions)

        // Wichtig:
        // Rohdaten bleiben im JSON erhalten.
        ->DisplayAggregatesOnly(true)

        ->MinTime(min_time_seconds)
        ->MinWarmUpTime(warmup_time_seconds)

        // Besonders wichtig für Multi-Thread-Benchmarks.
        ->UseRealTime()

        ->Unit(benchmark::kNanosecond);
}


static void construct_config(
    benchmark::internal::Benchmark* benchmark
) {
    benchmark->Threads(1);

    apply_common_config(benchmark);
}


static void single_thread_config(
    benchmark::internal::Benchmark* benchmark
) {
    benchmark
        ->ArgName("operations")
        ->Arg(1024)
        ->Arg(4096)
        ->Arg(16384)
        ->Arg(32768)
        ->Arg(49152)
        ->Threads(1);

    apply_common_config(benchmark);
}


static int available_benchmark_threads() {
    const unsigned int hardware_threads =
        std::thread::hardware_concurrency();

    if (hardware_threads == 0) {
        return 1;
    }

    return static_cast<int>(
        std::min<std::size_t>(
            max_threads,
            hardware_threads
        )
    );
}


static void add_thread_counts(
    benchmark::internal::Benchmark* benchmark
) {
    const int limit = available_benchmark_threads();

    int last = 0;

    for (int threads = 1; threads <= limit; threads *= 2) {
        benchmark->Threads(threads);
        last = threads;
    }

    // Beispiel:
    // CPU hat 24 Threads -> 1,2,4,8,16,24
    if (last != limit) {
        benchmark->Threads(limit);
    }
}


static void multi_thread_config(
    benchmark::internal::Benchmark* benchmark
) {
    benchmark
        // Gesamtzahl der Pushes über ALLE Threads.
        //
        // Dadurch bleibt der Workload bei unterschiedlicher
        // Threadzahl identisch.
        ->ArgName("total_operations")
        ->Arg(8192)
        ->Arg(32768)
        ->Arg(49152);

    add_thread_counts(benchmark);

    apply_common_config(benchmark);
}


template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
struct shared_deque_state {
    using deque_type = bench_deque<Arena>;

    std::unique_ptr<deque_type> deque;

    std::array<
        std::unique_ptr<std::barrier<>>,
        max_threads + 1
    > barriers{};

    std::array<
        std::once_flag,
        max_threads + 1
    > barrier_once{};


    std::barrier<>& barrier_for(int thread_count) {
        const auto index =
            static_cast<std::size_t>(thread_count);

        std::call_once(
            barrier_once[index],
            [this, thread_count, index]() {
                barriers[index] =
                    std::make_unique<std::barrier<>>(
                        thread_count
                    );
            }
        );

        return *barriers[index];
    }
};


template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
shared_deque_state<Arena>& state_for() {
    static shared_deque_state<Arena> state;
    return state;
}


// -----------------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------------

template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
static void construct_destroy(benchmark::State& state) {
    for (auto _ : state) {
        bench_deque<Arena> deque;

        benchmark::DoNotOptimize(&deque);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
        static_cast<std::int64_t>(
            state.iterations()
        )
    );
}


// -----------------------------------------------------------------------------
// Single-thread steady-state
// -----------------------------------------------------------------------------

template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
static void push_back_pop_front(
    benchmark::State& state
) {
    const std::size_t operations =
        static_cast<std::size_t>(state.range(0));

    bench_deque<Arena> deque;

    std::uint64_t failures = 0;

    for (auto _ : state) {
        for (std::size_t i = 0; i < operations; ++i) {
            const bool pushed =
                deque.push_back(
                    0,
                    static_cast<int>(i)
                );

            failures += !pushed;
            benchmark::DoNotOptimize(pushed);
        }

        int out = 0;

        for (std::size_t i = 0; i < operations; ++i) {
            const bool popped =
                deque.pop_front(0, out);

            failures += !popped;

            benchmark::DoNotOptimize(popped);
            benchmark::DoNotOptimize(out);
        }
    }

    benchmark::DoNotOptimize(failures);

    if (failures != 0) {
        state.SkipWithError(
            "Deque operation unexpectedly failed"
        );
        return;
    }

    state.SetItemsProcessed(
        static_cast<std::int64_t>(
            state.iterations() *
            operations *
            2
        )
    );
}


template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
static void push_front_pop_back(
    benchmark::State& state
) {
    const std::size_t operations =
        static_cast<std::size_t>(state.range(0));

    bench_deque<Arena> deque;

    std::uint64_t failures = 0;

    for (auto _ : state) {
        for (std::size_t i = 0; i < operations; ++i) {
            const bool pushed =
                deque.push_front(
                    0,
                    static_cast<int>(i)
                );

            failures += !pushed;
            benchmark::DoNotOptimize(pushed);
        }

        int out = 0;

        for (std::size_t i = 0; i < operations; ++i) {
            const bool popped =
                deque.pop_back(0, out);

            failures += !popped;

            benchmark::DoNotOptimize(popped);
            benchmark::DoNotOptimize(out);
        }
    }

    benchmark::DoNotOptimize(failures);

    if (failures != 0) {
        state.SkipWithError(
            "Deque operation unexpectedly failed"
        );
        return;
    }

    state.SetItemsProcessed(
        static_cast<std::int64_t>(
            state.iterations() *
            operations *
            2
        )
    );
}


// -----------------------------------------------------------------------------
// Multi-thread scalability
// -----------------------------------------------------------------------------

template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
>
static void concurrent_push_back_pop_front(
    benchmark::State& state
) {
    const std::size_t total_operations =
        static_cast<std::size_t>(state.range(0));

    const std::size_t thread_count =
        static_cast<std::size_t>(state.threads());

    const std::size_t thread_id =
        static_cast<std::size_t>(
            state.thread_index()
        );

    if (total_operations > usable_capacity) {
        state.SkipWithError(
            "Total operation count exceeds deque capacity"
        );
        return;
    }

    /*
     * Workload gleichmäßig verteilen.
     *
     * Beispiel:
     *
     * total_operations = 100
     * threads = 6
     *
     * 4 Threads -> 17
     * 2 Threads -> 16
     *
     * Summe bleibt exakt 100.
     */
    const std::size_t base_operations =
        total_operations / thread_count;

    const std::size_t remainder =
        total_operations % thread_count;

    const std::size_t operations_this_thread =
        base_operations +
        (thread_id < remainder ? 1 : 0);

    const std::size_t value_base =
        thread_id * base_operations +
        std::min(thread_id, remainder);


    auto& shared_state =
        state_for<Arena>();

    auto& barrier =
        shared_state.barrier_for(
            static_cast<int>(thread_count)
        );


    // -----------------------------------------------------------------
    // Setup
    // -----------------------------------------------------------------

    if (thread_id == 0) {
        shared_state.deque =
            std::make_unique<
                typename shared_deque_state<Arena>::deque_type
            >();
    }

    barrier.arrive_and_wait();


    std::uint64_t failures = 0;


    // -----------------------------------------------------------------
    // Measurement
    // -----------------------------------------------------------------

    for (auto _ : state) {

        // Push phase
        for (
            std::size_t i = 0;
            i < operations_this_thread;
            ++i
        ) {
            const bool pushed =
                shared_state.deque->push_back(
                    thread_id,
                    static_cast<int>(
                        value_base + i
                    )
                );

            failures += !pushed;

            benchmark::DoNotOptimize(pushed);
        }


        /*
         * Alle Pushes müssen abgeschlossen sein,
         * bevor die Pop-Phase beginnt.
         *
         * Dadurch sind fehlgeschlagene Pops wegen
         * einer temporär leeren Queue ausgeschlossen.
         */
        barrier.arrive_and_wait();


        int out = 0;

        for (
            std::size_t i = 0;
            i < operations_this_thread;
            ++i
        ) {
            const bool popped =
                shared_state.deque->pop_front(
                    thread_id,
                    out
                );

            failures += !popped;

            benchmark::DoNotOptimize(popped);
            benchmark::DoNotOptimize(out);
        }


        /*
         * Sicherstellen, dass die Deque wieder leer ist,
         * bevor die nächste Iteration startet.
         */
        barrier.arrive_and_wait();
    }


    benchmark::DoNotOptimize(failures);


    if (failures != 0) {
        state.SkipWithError(
            "Concurrent deque operation unexpectedly failed"
        );
    }


    // -----------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------

    if (thread_id == 0) {
        shared_state.deque.reset();
    }

    barrier.arrive_and_wait();


    /*
     * Nur Thread 0 trägt den globalen Workload ein.
     *
     * Das Plot-Script berechnet den Throughput zusätzlich
     * selbst aus real_time und total_operations und hängt
     * damit nicht von diesem Feld ab.
     */
    if (thread_id == 0) {
        state.SetItemsProcessed(
            static_cast<std::int64_t>(
                state.iterations() *
                total_operations *
                2
            )
        );
    }
}


// -----------------------------------------------------------------------------
// Wrappers
// -----------------------------------------------------------------------------

static void Deque_ConcurrentDequeNew_ConstructDestroy(
    benchmark::State& state
) {
    construct_destroy<memory_arena_new>(state);
}


static void Deque_ConcurrentDequeAllocator_ConstructDestroy(
    benchmark::State& state
) {
    construct_destroy<memory_arena_allocator>(state);
}


static void Deque_ConcurrentDequeMmap_ConstructDestroy(
    benchmark::State& state
) {
    construct_destroy<memory_arena_mmap>(state);
}


static void Deque_ConcurrentDequeNew_PushBackPopFront(
    benchmark::State& state
) {
    push_back_pop_front<memory_arena_new>(state);
}


static void Deque_ConcurrentDequeAllocator_PushBackPopFront(
    benchmark::State& state
) {
    push_back_pop_front<memory_arena_allocator>(state);
}


static void Deque_ConcurrentDequeMmap_PushBackPopFront(
    benchmark::State& state
) {
    push_back_pop_front<memory_arena_mmap>(state);
}


static void Deque_ConcurrentDequeNew_PushFrontPopBack(
    benchmark::State& state
) {
    push_front_pop_back<memory_arena_new>(state);
}


static void Deque_ConcurrentDequeAllocator_PushFrontPopBack(
    benchmark::State& state
) {
    push_front_pop_back<memory_arena_allocator>(state);
}


static void Deque_ConcurrentDequeMmap_PushFrontPopBack(
    benchmark::State& state
) {
    push_front_pop_back<memory_arena_mmap>(state);
}


static void Deque_ConcurrentDequeNew_ConcurrentPushBackPopFront(
    benchmark::State& state
) {
    concurrent_push_back_pop_front<memory_arena_new>(state);
}


static void Deque_ConcurrentDequeAllocator_ConcurrentPushBackPopFront(
    benchmark::State& state
) {
    concurrent_push_back_pop_front<memory_arena_allocator>(state);
}


static void Deque_ConcurrentDequeMmap_ConcurrentPushBackPopFront(
    benchmark::State& state
) {
    concurrent_push_back_pop_front<memory_arena_mmap>(state);
}

} // namespace


// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------

BENCHMARK(
    Deque_ConcurrentDequeNew_ConstructDestroy
)->Apply(construct_config);

BENCHMARK(
    Deque_ConcurrentDequeAllocator_ConstructDestroy
)->Apply(construct_config);

BENCHMARK(
    Deque_ConcurrentDequeMmap_ConstructDestroy
)->Apply(construct_config);


BENCHMARK(
    Deque_ConcurrentDequeNew_PushBackPopFront
)->Apply(single_thread_config);

BENCHMARK(
    Deque_ConcurrentDequeAllocator_PushBackPopFront
)->Apply(single_thread_config);

BENCHMARK(
    Deque_ConcurrentDequeMmap_PushBackPopFront
)->Apply(single_thread_config);


BENCHMARK(
    Deque_ConcurrentDequeNew_PushFrontPopBack
)->Apply(single_thread_config);

BENCHMARK(
    Deque_ConcurrentDequeAllocator_PushFrontPopBack
)->Apply(single_thread_config);

BENCHMARK(
    Deque_ConcurrentDequeMmap_PushFrontPopBack
)->Apply(single_thread_config);


BENCHMARK(
    Deque_ConcurrentDequeNew_ConcurrentPushBackPopFront
)->Apply(multi_thread_config);

BENCHMARK(
    Deque_ConcurrentDequeAllocator_ConcurrentPushBackPopFront
)->Apply(multi_thread_config);

BENCHMARK(
    Deque_ConcurrentDequeMmap_ConcurrentPushBackPopFront
)->Apply(multi_thread_config);