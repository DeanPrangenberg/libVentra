#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>
#include <ventra/deque/concurrent_deque.hpp>

namespace {
  constexpr std::size_t max_threads = 64;
  constexpr std::uint32_t deque_capacity = 65536;
  constexpr std::size_t usable_capacity = deque_capacity - 1;

  /*
   * 30 echte Wiederholungen ergeben spaeter genuegend Samples
   * fuer Median, Standardabweichung, CV und Bootstrap-CI.
   */
  constexpr int repetitions = 30;

  /*
   * Google Benchmark fuehrt intern so viele Iterationen aus, bis
   * MinTime erreicht ist. Dadurch bleiben kleine Workloads vergleichbar,
   * ohne die grossen Thread-Matrizen unnoetig lange laufen zu lassen.
   */
  constexpr double min_time_seconds = 0.25;
  constexpr double warmup_time_seconds = 0.10;

  /*
   * Absichtlich unabhaengig von der vorhandenen Hardware.
   *
   * Werte oberhalb der Hardware-Threadzahl messen Oversubscription und
   * zeigen, wie stark die Implementierung unter Scheduling-Druck leidet.
   */
  constexpr std::array<int, 10> thread_counts{
    1,
    2,
    4,
    8,
    12,
    16,
    24,
    32,
    48,
    64
  };

  /*
   * Single-thread-Batches. Alle Werte bleiben unter der nutzbaren
   * concurrent_deque-Kapazitaet, damit Fehlschlaege nicht Teil der Messung
   * werden.
   */
  constexpr std::array<std::int64_t, 9> single_thread_workloads{
    512,
    1024,
    2048,
    4096,
    8192,
    16384,
    32768,
    49152,
    61440
  };

  /*
   * Strong scaling:
   *
   * Die Gesamtarbeit bleibt konstant und wird auf mehr Threads verteilt.
   */
  constexpr std::array<std::int64_t, 3> strong_scaling_workloads{
    8192,
    32768,
    61440
  };

  /*
   * Steady-state / weak scaling:
   *
   * Jeder Thread bekommt gleich viel Arbeit.
   * Die Gesamtarbeit waechst damit mit der Threadzahl.
   */
  constexpr std::array<std::int64_t, 3> steady_state_workloads{
    512,
    2048,
    8192
  };


  template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
  >
  using concurrent_bench_deque = ventra::concurrent_deque<
    int,
    max_threads,
    2,
    deque_capacity,
    Arena
  >;

  using concurrent_deque_new_type =
  concurrent_bench_deque<memory_arena_new>;

  using concurrent_deque_allocator_type =
  concurrent_bench_deque<memory_arena_allocator>;

  using concurrent_deque_mmap_type =
  concurrent_bench_deque<memory_arena_mmap>;


  template<typename T>
  class lock_based_deque {
  public:
    bool push_back(std::size_t, T &&value) {
      std::lock_guard lock(mutex_);
      deque_.push_back(std::move(value));
      return true;
    }

    bool push_back(std::size_t, const T &value) {
      std::lock_guard lock(mutex_);
      deque_.push_back(value);
      return true;
    }

    template<typename... Args>
    bool emplace_back(std::size_t, Args &&... args) {
      std::lock_guard lock(mutex_);
      deque_.emplace_back(std::forward<Args>(args)...);
      return true;
    }

    bool push_front(std::size_t, T &&value) {
      std::lock_guard lock(mutex_);
      deque_.push_front(std::move(value));
      return true;
    }

    bool push_front(std::size_t, const T &value) {
      std::lock_guard lock(mutex_);
      deque_.push_front(value);
      return true;
    }

    template<typename... Args>
    bool emplace_front(std::size_t, Args &&... args) {
      std::lock_guard lock(mutex_);
      deque_.emplace_front(std::forward<Args>(args)...);
      return true;
    }

    bool pop_back(std::size_t, T &out) {
      std::lock_guard lock(mutex_);
      if (deque_.empty()) {
        return false;
      }

      out = std::move(deque_.back());
      deque_.pop_back();
      return true;
    }

    bool pop_front(std::size_t, T &out) {
      std::lock_guard lock(mutex_);
      if (deque_.empty()) {
        return false;
      }

      out = std::move(deque_.front());
      deque_.pop_front();
      return true;
    }

  private:
    std::mutex mutex_;
    std::deque<T> deque_;
  };

  using lock_based_bench_deque = lock_based_deque<int>;


  struct arena_benchmark_node {
    std::atomic<std::uint32_t> previous_node_idx{0};
    std::atomic<std::uint32_t> next_node_idx{0};
    std::atomic<std::uint32_t> freelist_next{0};
    std::uint64_t version{0};
    alignas(int) std::byte value_storage[sizeof(int)]{};
  };


  template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
  >
  using bench_arena = Arena<
    arena_benchmark_node,
    max_threads,
    deque_capacity,
    2
  >;


  struct work_partition {
    std::size_t operations;
    std::size_t value_base;
  };


  static work_partition partition_work(
    std::size_t total_operations,
    std::size_t thread_count,
    std::size_t thread_id
  ) noexcept {
    const std::size_t base =
        total_operations / thread_count;

    const std::size_t remainder =
        total_operations % thread_count;

    return {
      .operations = base + (thread_id < remainder ? 1 : 0),
      .value_base = thread_id * base + std::min(thread_id, remainder)
    };
  }


  static std::int64_t processed_count(
    benchmark::IterationCount iterations,
    std::size_t operations_per_iteration
  ) noexcept {
    return static_cast<std::int64_t>(iterations)
           * static_cast<std::int64_t>(operations_per_iteration);
  }


  // -----------------------------------------------------------------------------
  // Configuration
  // -----------------------------------------------------------------------------

  static void apply_common_config(
    benchmark::internal::Benchmark *benchmark
  ) {
    benchmark
        ->Repetitions(repetitions)

        /*
         * Nur Konsole auf Aggregate reduzieren.
         *
         * Die einzelnen Repetitions bleiben im JSON erhalten.
         */
        ->DisplayAggregatesOnly(true)

        ->MinTime(min_time_seconds)
        ->MinWarmUpTime(warmup_time_seconds)

        /*
         * Fuer Concurrent-Benchmarks interessiert die reale vergangene Zeit
         * und nicht die aufsummierte CPU-Zeit aller Threads.
         */
        ->UseRealTime()

        ->Unit(benchmark::kNanosecond);
  }


  static void add_all_thread_counts(
    benchmark::internal::Benchmark *benchmark
  ) {
    for (const int threads: thread_counts) {
      benchmark->Threads(threads);
    }
  }


  static void construct_config(
    benchmark::internal::Benchmark *benchmark
  ) {
    benchmark->Threads(1);
    apply_common_config(benchmark);
  }


  static void single_thread_config(
    benchmark::internal::Benchmark *benchmark
  ) {
    benchmark->ArgName("operations");

    for (const auto operations: single_thread_workloads) {
      benchmark->Arg(operations);
    }

    benchmark->Threads(1);
    apply_common_config(benchmark);
  }


  static void strong_scaling_config(
    benchmark::internal::Benchmark *benchmark
  ) {
    benchmark->ArgName("total_operations");

    for (const auto operations: strong_scaling_workloads) {
      benchmark->Arg(operations);
    }

    add_all_thread_counts(benchmark);
    apply_common_config(benchmark);
  }


  static void steady_state_config(
    benchmark::internal::Benchmark *benchmark
  ) {
    benchmark->ArgName("operations_per_thread");

    for (const auto operations: steady_state_workloads) {
      benchmark->Arg(operations);
    }

    add_all_thread_counts(benchmark);
    apply_common_config(benchmark);
  }


  static void barrier_config(
    benchmark::internal::Benchmark *benchmark
  ) {
    add_all_thread_counts(benchmark);
    apply_common_config(benchmark);
  }


  // -----------------------------------------------------------------------------
  // Shared barriers
  // -----------------------------------------------------------------------------

  class barrier_registry {
  public:
    std::barrier<> &get(const int thread_count) {
      const auto index =
          static_cast<std::size_t>(thread_count);

      std::call_once(
        once_[index],
        [this, index, thread_count]() {
          barriers_[index] =
              std::make_unique<std::barrier<> >(
                thread_count
              );
        }
      );

      return *barriers_[index];
    }

  private:
    std::array<
      std::unique_ptr<std::barrier<> >,
      max_threads + 1
    > barriers_{};

    std::array<
      std::once_flag,
      max_threads + 1
    > once_{};
  };


  static barrier_registry &get_barrier_registry() {
    static barrier_registry registry;
    return registry;
  }


  // -----------------------------------------------------------------------------
  // Shared deque
  // -----------------------------------------------------------------------------

  template<typename Deque>
  struct shared_deque_state {
    std::unique_ptr<Deque> deque;
  };


  template<typename Deque>
  shared_deque_state<Deque> &state_for() {
    static shared_deque_state<Deque> state;
    return state;
  }


  // -----------------------------------------------------------------------------
  // Arena memory operations
  // -----------------------------------------------------------------------------

  template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
  >
  static void arena_construct_destroy(
    benchmark::State &state
  ) {
    using arena_type = bench_arena<Arena>;

    for (auto _: state) {
      arena_type arena;

      benchmark::DoNotOptimize(&arena);
      benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), 1)
    );
  }


  template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
  >
  static void arena_allocate_release_unpublished(
    benchmark::State &state
  ) {
    using arena_type = bench_arena<Arena>;
    using u32 = typename arena_type::u32;

    const auto operations =
        static_cast<std::size_t>(
          state.range(0)
        );

    arena_type arena;

    std::uint64_t checksum = 0;
    std::uint64_t failures = 0;

    for (auto _: state) {
      for (
        std::size_t i = 0;
        i < operations;
        ++i
      ) {
        const u32 idx = arena.allocate_idx(0);

        if (idx == arena_type::null_idx) {
          ++failures;
          continue;
        }

        benchmark::DoNotOptimize(arena.get(idx));
        checksum += idx;

        arena.release_unpublished_idx(idx);
      }
    }

    benchmark::DoNotOptimize(checksum);

    if (failures != 0) {
      state.SkipWithError(
        "Arena allocate/release unexpectedly failed"
      );
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), operations * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        operations * sizeof(arena_benchmark_node) * 2
      )
    );
  }


  template<
    template<typename, std::size_t, std::uint32_t, std::size_t>
    typename Arena
  >
  static void arena_allocate_retire_reclaim(
    benchmark::State &state
  ) {
    using arena_type = bench_arena<Arena>;
    using u32 = typename arena_type::u32;

    const auto operations =
        static_cast<std::size_t>(
          state.range(0)
        );

    if (operations > usable_capacity) {
      state.SkipWithError(
        "Workload exceeds arena capacity"
      );

      return;
    }

    arena_type arena;
    std::vector<u32> indices(operations);

    std::uint64_t checksum = 0;
    std::uint64_t failures = 0;

    for (auto _: state) {
      std::size_t allocated = 0;

      for (
        ;
        allocated < operations;
        ++allocated
      ) {
        const u32 idx = arena.allocate_idx(0);

        if (idx == arena_type::null_idx) {
          ++failures;
          break;
        }

        indices[allocated] = idx;
        checksum += idx;
        benchmark::DoNotOptimize(arena.get(idx));
      }

      failures += operations - allocated;

      for (
        std::size_t i = 0;
        i < allocated;
        ++i
      ) {
        arena.retire_node(0, indices[i]);
      }

      arena.reclaim_retired(0);
      benchmark::ClobberMemory();
    }

    benchmark::DoNotOptimize(checksum);

    if (failures != 0) {
      state.SkipWithError(
        "Arena allocate/retire/reclaim unexpectedly failed"
      );
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), operations * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        operations * sizeof(arena_benchmark_node) * 2
      )
    );
  }


  // -----------------------------------------------------------------------------
  // Deque construction
  // -----------------------------------------------------------------------------

  template<typename Deque>
  static void deque_construct_destroy(
    benchmark::State &state
  ) {
    for (auto _: state) {
      Deque deque;

      benchmark::DoNotOptimize(&deque);
      benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), 1)
    );
  }


  // -----------------------------------------------------------------------------
  // Single-thread deque operations
  // -----------------------------------------------------------------------------

  template<typename Deque>
  static void deque_push_back_pop_front(
    benchmark::State &state
  ) {
    const auto operations =
        static_cast<std::size_t>(
          state.range(0)
        );

    Deque deque;

    std::uint64_t failures = 0;
    std::uint64_t checksum = 0;

    for (auto _: state) {
      for (
        std::size_t i = 0;
        i < operations;
        ++i
      ) {
        bool pushed =
            deque.push_back(
              0,
              static_cast<int>(i)
            );

        failures += !pushed;
        benchmark::DoNotOptimize(pushed);
      }

      int out = 0;

      for (
        std::size_t i = 0;
        i < operations;
        ++i
      ) {
        bool popped =
            deque.pop_front(
              0,
              out
            );

        failures += !popped;
        checksum +=
            static_cast<std::uint64_t>(
              static_cast<std::uint32_t>(out)
            );

        benchmark::DoNotOptimize(popped);
        benchmark::DoNotOptimize(out);
      }
    }

    benchmark::DoNotOptimize(checksum);

    if (failures != 0) {
      state.SkipWithError(
        "Deque operation unexpectedly failed"
      );
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), operations * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        operations * sizeof(int) * 2
      )
    );
  }


  template<typename Deque>
  static void deque_push_front_pop_back(
    benchmark::State &state
  ) {
    const auto operations =
        static_cast<std::size_t>(
          state.range(0)
        );

    Deque deque;

    std::uint64_t failures = 0;
    std::uint64_t checksum = 0;

    for (auto _: state) {
      for (
        std::size_t i = 0;
        i < operations;
        ++i
      ) {
        bool pushed =
            deque.push_front(
              0,
              static_cast<int>(i)
            );

        failures += !pushed;
        benchmark::DoNotOptimize(pushed);
      }

      int out = 0;

      for (
        std::size_t i = 0;
        i < operations;
        ++i
      ) {
        bool popped =
            deque.pop_back(
              0,
              out
            );

        failures += !popped;
        checksum +=
            static_cast<std::uint64_t>(
              static_cast<std::uint32_t>(out)
            );

        benchmark::DoNotOptimize(popped);
        benchmark::DoNotOptimize(out);
      }
    }

    benchmark::DoNotOptimize(checksum);

    if (failures != 0) {
      state.SkipWithError(
        "Deque operation unexpectedly failed"
      );
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), operations * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        operations * sizeof(int) * 2
      )
    );
  }


  // -----------------------------------------------------------------------------
  // Strong scaling / phased contention
  //
  // Gleiche Gesamtarbeit bei jeder Threadzahl.
  //
  // Alle Threads pushen.
  // Barrier.
  // Alle Threads poppen.
  // Barrier.
  //
  // Dieser Test enthaelt absichtlich Synchronisationskosten.
  // -----------------------------------------------------------------------------

  template<typename Deque>
  static void deque_concurrent_phased_push_back_pop_front(
    benchmark::State &state
  ) {
    const auto total_operations =
        static_cast<std::size_t>(
          state.range(0)
        );

    const auto thread_count =
        static_cast<std::size_t>(
          state.threads()
        );

    const auto thread_id =
        static_cast<std::size_t>(
          state.thread_index()
        );

    if (total_operations > usable_capacity) {
      state.SkipWithError(
        "Workload exceeds deque capacity"
      );

      return;
    }

    const auto work =
        partition_work(
          total_operations,
          thread_count,
          thread_id
        );

    auto &shared = state_for<Deque>();

    auto &barrier =
        get_barrier_registry().get(
          state.threads()
        );

    if (thread_id == 0) {
      shared.deque =
          std::make_unique<Deque>();
    }

    barrier.arrive_and_wait();

    std::uint64_t failures = 0;
    std::uint64_t checksum = 0;

    for (auto _: state) {
      for (
        std::size_t i = 0;
        i < work.operations;
        ++i
      ) {
        bool pushed =
            shared.deque->push_back(
              thread_id,
              static_cast<int>(
                work.value_base + i
              )
            );

        failures += !pushed;
        benchmark::DoNotOptimize(pushed);
      }

      /*
       * Enthält Synchronisationskosten. Der separate barrier_overhead
       * Benchmark misst die reine Harness-Komponente dazu.
       */
      barrier.arrive_and_wait();

      int out = 0;

      for (
        std::size_t i = 0;
        i < work.operations;
        ++i
      ) {
        bool popped =
            shared.deque->pop_front(
              thread_id,
              out
            );

        failures += !popped;
        checksum +=
            static_cast<std::uint64_t>(
              static_cast<std::uint32_t>(out)
            );

        benchmark::DoNotOptimize(popped);
        benchmark::DoNotOptimize(out);
      }

      barrier.arrive_and_wait();
    }

    /*
     * Nach der letzten Pop-Barriere greift kein Thread mehr auf die Deque zu.
     */
    if (thread_id == 0) {
      shared.deque.reset();
    }

    barrier.arrive_and_wait();

    benchmark::DoNotOptimize(checksum);

    if (failures != 0) {
      state.SkipWithError(
        "Concurrent deque operation failed"
      );
    }

    /*
     * Jeder Thread meldet nur seine eigene Arbeit.
     * Google Benchmark aggregiert die Threads.
     */
    state.SetItemsProcessed(
      processed_count(state.iterations(), work.operations * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        work.operations * sizeof(int) * 2
      )
    );
  }


  // -----------------------------------------------------------------------------
  // Steady-state concurrent push_back / pop_front
  //
  // Keine Barrier innerhalb des gemessenen Loops.
  //
  // Jeder Thread:
  // push
  // pop
  // push
  // pop
  // ...
  //
  // Dadurch misst du primaer den konkurrierenden Datenstrukturzugriff und nicht
  // staendig den Barrier-Harness.
  // -----------------------------------------------------------------------------

  template<typename Deque>
  static void deque_concurrent_steady_push_back_pop_front(
    benchmark::State &state
  ) {
    const auto operations_per_thread =
        static_cast<std::size_t>(
          state.range(0)
        );

    const auto thread_id =
        static_cast<std::size_t>(
          state.thread_index()
        );

    auto &shared = state_for<Deque>();

    auto &barrier =
        get_barrier_registry().get(
          state.threads()
        );

    if (thread_id == 0) {
      shared.deque =
          std::make_unique<Deque>();
    }

    /*
     * Nur Setup synchronisieren.
     */
    barrier.arrive_and_wait();

    std::uint64_t failures = 0;
    std::uint64_t checksum = 0;

    for (auto _: state) {
      for (
        std::size_t i = 0;
        i < operations_per_thread;
        ++i
      ) {
        const int value =
            static_cast<int>(
              thread_id
              * operations_per_thread
              + i
            );

        bool pushed =
            shared.deque->push_back(
              thread_id,
              value
            );

        failures += !pushed;

        int out = 0;

        bool popped =
            shared.deque->pop_front(
              thread_id,
              out
            );

        failures += !popped;

        checksum +=
            static_cast<std::uint64_t>(
              static_cast<std::uint32_t>(out)
            );

        benchmark::DoNotOptimize(pushed);
        benchmark::DoNotOptimize(popped);
        benchmark::DoNotOptimize(out);
      }
    }

    benchmark::DoNotOptimize(checksum);

    /*
     * Erst warten, bis wirklich alle Threads aus ihrem Benchmark-Loop raus
     * sind. Sonst koennte Thread 0 die Deque zerstoeren, waehrend ein anderer
     * Thread noch darauf zugreift.
     */
    barrier.arrive_and_wait();

    if (thread_id == 0) {
      shared.deque.reset();
    }

    barrier.arrive_and_wait();

    if (failures != 0) {
      state.SkipWithError(
        "Concurrent deque operation failed"
      );
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), operations_per_thread * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        operations_per_thread * sizeof(int) * 2
      )
    );
  }


  // -----------------------------------------------------------------------------
  // Steady-state opposite direction
  // -----------------------------------------------------------------------------

  template<typename Deque>
  static void deque_concurrent_steady_push_front_pop_back(
    benchmark::State &state
  ) {
    const auto operations_per_thread =
        static_cast<std::size_t>(
          state.range(0)
        );

    const auto thread_id =
        static_cast<std::size_t>(
          state.thread_index()
        );

    auto &shared = state_for<Deque>();

    auto &barrier =
        get_barrier_registry().get(
          state.threads()
        );

    if (thread_id == 0) {
      shared.deque =
          std::make_unique<Deque>();
    }

    barrier.arrive_and_wait();

    std::uint64_t failures = 0;
    std::uint64_t checksum = 0;

    for (auto _: state) {
      for (
        std::size_t i = 0;
        i < operations_per_thread;
        ++i
      ) {
        const int value =
            static_cast<int>(
              thread_id
              * operations_per_thread
              + i
            );

        bool pushed =
            shared.deque->push_front(
              thread_id,
              value
            );

        failures += !pushed;

        int out = 0;

        bool popped =
            shared.deque->pop_back(
              thread_id,
              out
            );

        failures += !popped;

        checksum +=
            static_cast<std::uint64_t>(
              static_cast<std::uint32_t>(out)
            );

        benchmark::DoNotOptimize(pushed);
        benchmark::DoNotOptimize(popped);
        benchmark::DoNotOptimize(out);
      }
    }

    benchmark::DoNotOptimize(checksum);

    barrier.arrive_and_wait();

    if (thread_id == 0) {
      shared.deque.reset();
    }

    barrier.arrive_and_wait();

    if (failures != 0) {
      state.SkipWithError(
        "Concurrent deque operation failed"
      );
    }

    state.SetItemsProcessed(
      processed_count(state.iterations(), operations_per_thread * 2)
    );

    state.SetBytesProcessed(
      processed_count(
        state.iterations(),
        operations_per_thread * sizeof(int) * 2
      )
    );
  }


  // -----------------------------------------------------------------------------
  // Barrier baseline
  //
  // Gleiche zwei Barrier-Aufrufe wie der phased Benchmark.
  // Damit kannst du abschaetzen, wie viel des gemessenen Verhaltens vom
  // Benchmark-Harness selbst kommt.
  // -----------------------------------------------------------------------------

  static void barrier_overhead(
    benchmark::State &state
  ) {
    auto &barrier =
        get_barrier_registry().get(
          state.threads()
        );

    for (auto _: state) {
      barrier.arrive_and_wait();
      barrier.arrive_and_wait();
    }
  }


  // -----------------------------------------------------------------------------
  // Wrappers
  // -----------------------------------------------------------------------------

#define DEFINE_ARENA_MEMORY_BENCHMARKS(Name, Arena)                     \
                                                                        \
static void Arena_##Name##_ConstructDestroy(                            \
    benchmark::State& state                                             \
) {                                                                     \
    arena_construct_destroy<Arena>(state);                              \
}                                                                       \
                                                                        \
static void Arena_##Name##_AllocateReleaseUnpublished(                  \
    benchmark::State& state                                             \
) {                                                                     \
    arena_allocate_release_unpublished<Arena>(state);                   \
}                                                                       \
                                                                        \
static void Arena_##Name##_AllocateRetireReclaim(                       \
    benchmark::State& state                                             \
) {                                                                     \
    arena_allocate_retire_reclaim<Arena>(state);                        \
}


  DEFINE_ARENA_MEMORY_BENCHMARKS(
    ConcurrentDequeNew,
    memory_arena_new
  )

  DEFINE_ARENA_MEMORY_BENCHMARKS(
    ConcurrentDequeAllocator,
    memory_arena_allocator
  )

  DEFINE_ARENA_MEMORY_BENCHMARKS(
    ConcurrentDequeMmap,
    memory_arena_mmap
  )

#undef DEFINE_ARENA_MEMORY_BENCHMARKS


#define DEFINE_DEQUE_BENCHMARKS(Name, DequeType)                        \
                                                                        \
static void Deque_##Name##_ConstructDestroy(                            \
    benchmark::State& state                                             \
) {                                                                     \
    deque_construct_destroy<DequeType>(state);                          \
}                                                                       \
                                                                        \
static void Deque_##Name##_PushBackPopFront(                            \
    benchmark::State& state                                             \
) {                                                                     \
    deque_push_back_pop_front<DequeType>(state);                        \
}                                                                       \
                                                                        \
static void Deque_##Name##_PushFrontPopBack(                            \
    benchmark::State& state                                             \
) {                                                                     \
    deque_push_front_pop_back<DequeType>(state);                        \
}                                                                       \
                                                                        \
static void Deque_##Name##_ConcurrentPhasedPushBackPopFront(            \
    benchmark::State& state                                             \
) {                                                                     \
    deque_concurrent_phased_push_back_pop_front<DequeType>(state);      \
}                                                                       \
                                                                        \
static void Deque_##Name##_ConcurrentSteadyPushBackPopFront(            \
    benchmark::State& state                                             \
) {                                                                     \
    deque_concurrent_steady_push_back_pop_front<DequeType>(state);      \
}                                                                       \
                                                                        \
static void Deque_##Name##_ConcurrentSteadyPushFrontPopBack(            \
    benchmark::State& state                                             \
) {                                                                     \
    deque_concurrent_steady_push_front_pop_back<DequeType>(state);      \
}


  DEFINE_DEQUE_BENCHMARKS(
    ConcurrentDequeNew,
    concurrent_deque_new_type
  )

  DEFINE_DEQUE_BENCHMARKS(
    ConcurrentDequeAllocator,
    concurrent_deque_allocator_type
  )

  DEFINE_DEQUE_BENCHMARKS(
    ConcurrentDequeMmap,
    concurrent_deque_mmap_type
  )

  DEFINE_DEQUE_BENCHMARKS(
    LockBasedStdDeque,
    lock_based_bench_deque
  )

#undef DEFINE_DEQUE_BENCHMARKS
} // namespace


// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------

#define REGISTER_ARENA_MEMORY_BENCHMARKS(Name)                          \
                                                                        \
BENCHMARK(                                                              \
    Arena_##Name##_ConstructDestroy                                     \
)->Apply(construct_config);                                             \
                                                                        \
BENCHMARK(                                                              \
    Arena_##Name##_AllocateReleaseUnpublished                           \
)->Apply(single_thread_config);                                         \
                                                                        \
BENCHMARK(                                                              \
    Arena_##Name##_AllocateRetireReclaim                                \
)->Apply(single_thread_config)


REGISTER_ARENA_MEMORY_BENCHMARKS(
  ConcurrentDequeNew
);

REGISTER_ARENA_MEMORY_BENCHMARKS(
  ConcurrentDequeAllocator
);

REGISTER_ARENA_MEMORY_BENCHMARKS(
  ConcurrentDequeMmap
);

#undef REGISTER_ARENA_MEMORY_BENCHMARKS


#define REGISTER_DEQUE_BENCHMARKS(Name)                                 \
                                                                        \
BENCHMARK(                                                              \
    Deque_##Name##_ConstructDestroy                                     \
)->Apply(construct_config);                                             \
                                                                        \
BENCHMARK(                                                              \
    Deque_##Name##_PushBackPopFront                                     \
)->Apply(single_thread_config);                                         \
                                                                        \
BENCHMARK(                                                              \
    Deque_##Name##_PushFrontPopBack                                     \
)->Apply(single_thread_config);                                         \
                                                                        \
BENCHMARK(                                                              \
    Deque_##Name##_ConcurrentPhasedPushBackPopFront                     \
)->Apply(strong_scaling_config);                                        \
                                                                        \
BENCHMARK(                                                              \
    Deque_##Name##_ConcurrentSteadyPushBackPopFront                     \
)->Apply(steady_state_config);                                          \
                                                                        \
BENCHMARK(                                                              \
    Deque_##Name##_ConcurrentSteadyPushFrontPopBack                     \
)->Apply(steady_state_config)


REGISTER_DEQUE_BENCHMARKS(
  ConcurrentDequeNew
);

REGISTER_DEQUE_BENCHMARKS(
  ConcurrentDequeAllocator
);

REGISTER_DEQUE_BENCHMARKS(
  ConcurrentDequeMmap
);

REGISTER_DEQUE_BENCHMARKS(
  LockBasedStdDeque
);

#undef REGISTER_DEQUE_BENCHMARKS


BENCHMARK(
  barrier_overhead
)->Apply(barrier_config);
