# `ventra::concurrent_atomic_vector<T>`

Short description.

## Header

```cpp
#include <ventra/concurrent_atomic_vector.hpp>
```

## Overview

- TODO describe purpose
- TODO describe important properties

## Example

```cpp
ventra::concurrent_atomic_vector<int> object;
```

## Benchmarks

<!-- AUTO-BENCHMARKS:BEGIN -->

### Int Push Back

![Benchmark Int Push Back](../benchmarks/benchmark-ConcurrentAtomicVector-StdVector-IntPushBack.png)

### Int Push Back Reserved

![Benchmark Int Push Back Reserved](../benchmarks/benchmark-ConcurrentAtomicVector-StdVector-IntPushBackReserved.png)

### String Push Back

![Benchmark String Push Back](../benchmarks/benchmark-ConcurrentAtomicVector-StdVector-StringPushBack.png)

### String Push Back Reserved

![Benchmark String Push Back Reserved](../benchmarks/benchmark-ConcurrentAtomicVector-StdVector-StringPushBackReserved.png)


### Int_push_back_reserved

![Benchmark Int_push_back_reserved](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Smart_Vector.Ventra_Concurrent_Atomic_Vector-Int_push_back_reserved.png)


### Int_push_back

![Benchmark Int_push_back](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Smart_Vector.Ventra_Concurrent_Atomic_Vector-Int_push_back.png)

<!-- AUTO-BENCHMARKS:END -->
