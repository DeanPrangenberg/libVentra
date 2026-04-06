//
// Created by deanprangenberg on 4/6/26.
//

#include <benchmark/benchmark.h>
#include <vector>
#include "ventra/vector/vector.hpp"

static void BM_StdVector_PushBack(benchmark::State& state) {
    const int num_elements = state.range(0);

    for (auto _ : state) {
        std::vector<int> vec;

        for (int i = 0; i < num_elements; ++i) {
            vec.push_back(i);
        }

        benchmark::DoNotOptimize(vec.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_StdVector_PushBack)->Arg(1000)->Arg(10000)->Arg(100000);

static void BM_VentraVector_PushBack(benchmark::State& state) {
    const int num_elements = state.range(0);

    for (auto _ : state) {
        ventra::vector<int> vec;

        for (int i = 0; i < num_elements; ++i) {
            vec.push_back(i);
        }

        benchmark::DoNotOptimize(vec);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VentraVector_PushBack)->Arg(1000)->Arg(10000)->Arg(100000);


static void BM_StdVector_PushBackReserved(benchmark::State& state) {
    const int num_elements = state.range(0);

    for (auto _ : state) {
        std::vector<int> vec;
        vec.reserve(num_elements);

        for (int i = 0; i < num_elements; ++i) {
            vec.push_back(i);
        }

        benchmark::DoNotOptimize(vec.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_StdVector_PushBack)->Arg(1000)->Arg(10000)->Arg(100000);

static void BM_VentraVector_PushBackReserved(benchmark::State& state) {
    const int num_elements = state.range(0);

    for (auto _ : state) {
        ventra::vector<int> vec;
        vec.reserve(num_elements);

        for (int i = 0; i < num_elements; ++i) {
            vec.push_back(i);
        }

        benchmark::DoNotOptimize(vec);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VentraVector_PushBack)->Arg(1000)->Arg(10000)->Arg(100000);