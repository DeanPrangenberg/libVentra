# `ventra::array<T, N>`

Kleiner Header-only Container mit fester Groesse zur Compile-Zeit. `ventra::array<T, N>` ist nah an `std::array`, bleibt aber bewusst einfacher.

## Header

```cpp
#include <ventra/array/array.hpp>
```

## Kurzueberblick

- Feste Groesse `N`, keine dynamische Allokation
- Speicher ist zusammenhaengend
- Zugriff per `operator[]`, `at()`, `front()`, `back()` und `data()`
- Iteratoren sind rohe Pointer
- API ist klein und nicht vollstaendig deckungsgleich mit `std::array`

## API

### Konstruktion

| API | Beschreibung |
| --- | --- |
| `array(std::initializer_list<T> init_list)` | Initialisiert die ersten Werte aus der Liste. |

### Zugriff

| API | Beschreibung |
| --- | --- |
| `front()` / `back()` | Liefert erstes bzw. letztes Element. Bei `const` als Kopie, nicht als Referenz. |
| `operator[](size_t idx)` | Ungepruefter Zugriff. |
| `at(size_t idx)` | Gepruefter Zugriff, wirft `std::out_of_range` bei ungueltigem Index. |
| `data()` | Pointer auf den internen Speicher. |

### Kapazitaet

| API | Beschreibung |
| --- | --- |
| `size()` | Liefert immer `N`. |
| `max_size()` | Liefert immer `N`. |
| `empty()` | `true`, wenn `N == 0`. |

### Aenderungen

| API | Beschreibung |
| --- | --- |
| `fill(const T& value)` | Schreibt denselben Wert in alle Slots. |
| `swap(array& other)` | Tauscht alle Elemente paarweise. |

### Iteratoren

| API | Beschreibung |
| --- | --- |
| `begin()` / `end()` | Iteratoren als `T*`. |
| `cbegin()` / `cend()` | Aktuell ebenfalls rohe `T*`, nicht `const T*`. |

## Hinweise

- `front()`, `back()` und `data()` sind fuer `N == 0` nicht sinnvoll nutzbar.
- `at()` ist der sichere Zugriff fuer Grenzpruefungen.
- Bei elementaren Typen sollten nicht initialisierte Slots vor dem Lesen mit `fill()` oder einer vollstaendigen Initializer-Liste gesetzt werden.
- Die API ist nicht voll `std::array`-kompatibel: `size()`, `max_size()` und `empty()` sind nicht `const`, `cbegin()` und `cend()` liefern aktuell keine read-only Pointer.
- Die Klasse ist nicht thread-safe.

## Beispiel

```cpp
#include <iostream>

#include <ventra/array/array.hpp>

int main() {
    ventra::array<int, 4> values = {1, 2, 3, 4};

    values[1] = 20;

    for (int value : values) {
        std::cout << value << '\n';
    }
}
```
