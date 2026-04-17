//
// Created by deanprangenberg on 04/14/2026.
//

#include <benchmark/benchmark.h>
#include <ventra/deque/deque.hpp>

static void BM_deque_deque_construct(benchmark::State& state) {
    for (auto _ : state) {
        ventra::deque<int> object;

        benchmark::DoNotOptimize(&object);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_deque_deque_construct);
