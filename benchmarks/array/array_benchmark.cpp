//
// Created by deanprangenberg on 04/16/2026.
//

#include <benchmark/benchmark.h>
#include <ventra/array/array.hpp>

static void BM_array_array_construct(benchmark::State& state) {
    for (auto _ : state) {
        ventra::array<int> object;

        benchmark::DoNotOptimize(&object);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_array_array_construct);
