# `ventra::concurrent_atomic_vector<T>`

Append-only Container fuer Werte, die direkt in `std::atomic<T>` gespeichert werden koennen. Die Klasse ist fuer konkurrierende `push_back()`-Aufrufe und atomische Updates bereits gespeicherter Elemente gedacht.

## Header

```cpp
#include <ventra/vector/concurrent_atomic_vector.hpp>
```

## Kurzueberblick

- Append-only, kein `erase()`
- Mehrere Threads koennen gleichzeitig `push_back()` verwenden
- Bereits vorhandene Elemente lassen sich atomisch lesen und aktualisieren
- Lesefunktionen liefern bei Bedarf `std::optional<T>`
- Interner Speicher wird per `mmap()` als grosser virtueller Bereich reserviert

## API

### Konstruktion

| API | Beschreibung |
| --- | --- |
| `concurrent_atomic_vector()` | Leerer Container. |
| `concurrent_atomic_vector(size_t initial_capacity)` | Aktuell funktional identisch zum Default-Konstruktor. |
| `concurrent_atomic_vector(size_t count, const T& value)` | Erzeugt `count` initialisierte Elemente. |
| `concurrent_atomic_vector(std::initializer_list<T> init_list)` | Erzeugt den Container aus einer Liste. |
| Copy-Konstruktion / Copy-Assignment | Geloescht. |

### Zugriff

| API | Beschreibung |
| --- | --- |
| `load(size_t idx, memory_order)` | Laedt einen gueltigen Index ohne Bounds-Check. |
| `try_load(size_t idx, memory_order)` | Sicherer Lesezugriff mit `std::optional<T>`. |
| `front(memory_order)` / `back(memory_order)` | Erstes bzw. letztes publiziertes Element als `std::optional<T>`. |

### Kapazitaet

| API | Beschreibung |
| --- | --- |
| `size()` | Anzahl bereits angehaengter Elemente. |
| `capacity()` | Theoretisches Maximum `64ULL * 1024 * 1024 * 1024 / sizeof(T)`. |
| `empty()` | `true`, wenn keine Elemente vorhanden sind. |
| `reserve(size_t new_capacity)` | Prueft nur gegen das Maximum, reserviert aber keinen zusaetzlichen Speicher. |

### Aenderungen

| API | Beschreibung |
| --- | --- |
| `push_back(const T&)` / `push_back(T&&)` | Fuegt hinten an. |
| `store(idx, value, memory_order)` | Ueberschreibt ein vorhandenes Element. |
| `try_store(idx, value, memory_order)` | Sichere Variante mit `bool`-Rueckgabe. |
| `fetch_add/sub/and/or/xor(...)` | Atomische Read-Modify-Write-Operationen fuer vorhandene Elemente. |
| `try_fetch_add/sub/and/or/xor(...)` | Sichere Varianten mit `bool`-Rueckgabe. |
| `compare_exchange_weak/strong(...)` | Atomischer Compare-Exchange auf einem vorhandenen Element. |
| `try_compare_exchange_weak/strong(...)` | Sichere Varianten mit `std::optional<bool>`. |
| `clear_NOT_THREAD_SAFE()` | Entfernt alle Elemente. Darf nicht parallel zu anderen Operationen laufen. |

### Iteratoren

| API | Beschreibung |
| --- | --- |
| `begin()` / `end()` | Read-only Iteratoren. Dereferenzieren laedt den Wert by value. |

## Hinweise

- `T` muss mit `std::atomic<T>` funktionieren. Praktisch bedeutet das meist trivially copyable Typen.
- `capacity()` ist kein wachsender Runtime-Wert wie bei `std::vector`, sondern ein festes Oberlimit.
- Die nicht-`try_`-APIs setzen gueltige, bereits publizierte Indizes voraus.
- Fuer paralleles Lesen waehrend `push_back()` ist `try_load()` die sichere Variante.
- Die Klasse ist auf Linux-artige Umgebungen mit `mmap()` und `munmap()` ausgelegt.

## Beispiel

```cpp
#include <iostream>
#include <thread>
#include <vector>

#include <ventra/vector/concurrent_atomic_vector.hpp>

int main() {
    ventra::concurrent_atomic_vector<int> values;
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 100; ++i) {
                values.push_back(t * 100 + i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "size: " << values.size() << '\n';
}
```

## Benchmarks

<!-- AUTO-BENCHMARKS:BEGIN -->

### Int_push_back_reserved

![Benchmark Int_push_back_reserved](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Smart_Vector.Ventra_Concurrent_Atomic_Vector-Int_push_back_reserved.png)


### Int_push_back

![Benchmark Int_push_back](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Smart_Vector.Ventra_Concurrent_Atomic_Vector-Int_push_back.png)


### Int_push_back_reserved

![Benchmark Int_push_back_reserved](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Atomic_Vector.Ventra_Concurrent_Smart_Vector-Int_push_back_reserved.png)


### Int_push_back

![Benchmark Int_push_back](../benchmarks/Std_Vector_Simple_Mutex.Ventra_Concurrent_Atomic_Vector.Ventra_Concurrent_Smart_Vector-Int_push_back.png)

<!-- AUTO-BENCHMARKS:END -->
