# `ventra::deque<T, Alloc>`

Chunk-basierter Double-Ended-Container fuer guenstige `push_front()`- und `push_back()`-Operationen. `ventra::deque` bietet Random Access, speichert die Elemente aber nicht zusammenhaengend wie `ventra::vector`.

## Header

```cpp
#include <ventra/deque/deque.hpp>
```

## Kurzueberblick

- Waechst an beiden Enden
- Logischer Zufallszugriff per Index
- Eigene Random-Access-Iteratoren
- Konfigurierbare Chunk-Groesse, Standard `64`
- Header-only, aber nicht thread-safe

## API

### Konstruktion

| API | Beschreibung |
| --- | --- |
| `deque(size_t chunk_size = 64)` | Leeres Deque mit gewaehlter Chunk-Groesse. `chunk_size == 0` wirft `std::invalid_argument`. |
| `deque(const deque& other)` | Tiefe Kopie. |
| `deque(deque&& other)` | Uebernimmt den Inhalt. |
| `deque(std::initializer_list<T> init_list)` | Baut das Deque aus einer Liste auf. |
| `operator=(...)` | Copy- und Move-Assignment sind verfuegbar. |

### Zugriff

| API | Beschreibung |
| --- | --- |
| `front()` | Erstes Element. Auf leerem Deque ungeprueft. |
| `back()` | Letztes Element. Wirft auf leerem Deque `std::out_of_range`. |
| `operator[](size_t idx)` | Ungepruefter Zugriff per logischem Index. |
| `at(size_t idx)` | Gepruefter Zugriff, wirft `std::out_of_range`. |

### Kapazitaet

| API | Beschreibung |
| --- | --- |
| `size()` | Anzahl aktuell gespeicherter Elemente. |
| `empty()` | `true`, wenn keine Elemente vorhanden sind. |
| `capacity()` | Anzahl aktuell allokierter Slots ueber alle Chunks. |
| `resize(size_t new_size)` | Verkleinert oder erweitert mit default-konstruierten Werten. |
| `resize(size_t new_size, const T& value)` | Verkleinert oder erweitert mit dem angegebenen Wert. |
| `shrink_to_fit()` | Gibt ungenutzte Chunks frei und kompaktiert die Chunk-Tabelle. |

### Aenderungen

| API | Beschreibung |
| --- | --- |
| `assign(size_t amount, const T& value)` | Ersetzt den gesamten Inhalt. |
| `push_back(...)` / `push_front(...)` | Fuegt am Ende oder Anfang ein. |
| `emplace_back(...)` / `emplace_front(...)` | Konstruiert direkt am Ende oder Anfang. |
| `pop_back()` / `pop_front()` | Entfernt hinten oder vorne. Auf leerem Deque wird `std::out_of_range` geworfen. |
| `insert(iterator pos, ...)` | Fuegt vor `pos` ein. |
| `emplace(iterator pos, ...)` | Konstruiert vor `pos`. |
| `erase(iterator pos)` | Entfernt das Element an `pos`. |
| `clear()` | Entfernt alle Elemente, behaelt den Container aber nutzbar. |
| `swap(deque& other)` | Tauscht Inhalt und internen Zustand. |

### Iteratoren

| API | Beschreibung |
| --- | --- |
| `begin()` / `end()` | Random-Access-Iteratoren ueber logische Indizes. |
| `cbegin()` / `cend()` | Konstante Varianten. |
| `iterator::index()` | Liefert den logischen Index des Iterators. |

## Hinweise

- Der Speicher ist nicht zusammenhaengend. `data()` gibt es deshalb bewusst nicht.
- `insert()`, `erase()`, `resize()` und andere Struktur-Aenderungen koennen Iteratoren, Referenzen und Positionen ungueltig machen.
- `capacity()` beschreibt allokierte Chunk-Slots, nicht einen zusammenhaengenden Block wie bei `std::vector`.
- `shrink_to_fit()` bringt ein leeres Deque wieder auf genau einen Chunk zurueck.

## Beispiel

```cpp
#include <iostream>

#include <ventra/deque/deque.hpp>

int main() {
    ventra::deque<int> values;

    values.push_back(2);
    values.push_front(1);
    values.push_back(4);
    values.insert(values.begin() + 2, 3);

    for (int value : values) {
        std::cout << value << '\n';
    }
}
```
