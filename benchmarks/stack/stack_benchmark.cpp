//
// Created by deanprangenberg on 04/14/2026.
//

#include <benchmark/benchmark.h>
#include <ventra/stack/stack.hpp>

static void BM_stack_stack_construct(benchmark::State& state) {
    for (auto _ : state) {
        ventra::stack<int> object;

        benchmark::DoNotOptimize(&object);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_stack_stack_construct);
