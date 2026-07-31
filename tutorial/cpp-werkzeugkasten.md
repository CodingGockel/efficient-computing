# C++-Werkzeugkasten für dieses Tutorial

Aller Code hier ist C++ (`-std=c++17`, gebaut mit `g++`). Diese Seite sammelt die
Sprachmittel, die in den Kapiteln vorkommen — mit dem Schwerpunkt auf dem, was beim
**parallelen** Programmieren wirklich zählt.

**Die Stilregel des Tutorials:** Drumherum modernes C++ (`std::vector`, `<chrono>`, RAII,
Lambdas), aber die **parallelisierten Rechenschleifen bleiben roh** — rohe Indizes, rohe
`for`-Schleifen. So sieht man, was OpenMP tatsächlich verteilt, und so steht es auch in der
Klausur.

---

## 1. Das Minimum, um von C nach C++ zu kommen

| C | C++ | warum |
|---|---|---|
| `malloc`/`free` | `std::vector<double> v(n);` | wird automatisch freigegeben, kennt seine Größe |
| `double *a` als Parameter | `std::vector<double>& a` (schreibend) / `const std::vector<double>& a` (lesend) | keine Größe verlieren, keine versehentliche Kopie |
| `printf` | `std::printf` oder `std::cout` | `printf` bleibt erlaubt und ist für Tabellen oft praktischer |
| `#define N 100` | `constexpr int N = 100;` | typsicher, im Debugger sichtbar |
| `struct node { … }; struct node *p;` | `struct Node { … }; Node* p;` | `struct` beim Benutzen entfällt |
| Funktionszeiger | Lambda `[](int i){ return i*i; }` | kann lokale Variablen einfangen |
| `clock()` | `std::chrono::steady_clock` | typsicher, keine Verwechslung von Einheiten |

```cpp
#include <vector>

std::vector<double> x(n, 1.0);   // n Elemente, alle 1.0
x.size();                        // Anzahl (Typ: std::size_t)
x.data();                        // roher double* -- fuer OpenMP manchmal noetig
x[i];                            // ohne Bereichspruefung (schnell)
x.at(i);                         // mit Bereichspruefung (wirft std::out_of_range)
```

> **Wichtig für die Performance:** `std::vector` ist bei `-O2` **exakt so schnell** wie ein
> `new double[n]`. Der Zugriff `x[i]` wird zu genau derselben Adressrechnung. Es gibt keinen
> Grund, für Rechenkerne auf rohe Arrays auszuweichen.

---

## 2. Zeitmessung

```cpp
#include <chrono>

const auto t0 = std::chrono::steady_clock::now();
rechne();
const auto t1 = std::chrono::steady_clock::now();
const double sekunden = std::chrono::duration<double>(t1 - t0).count();
```

`steady_clock` ist die richtige Wahl: Sie läuft monoton und wird nicht von einer
Zeitumstellung oder NTP-Korrektur zurückgedreht (`system_clock` schon).

Im Tutorial steckt das in [`_common/bench.hpp`](_common/bench.hpp):

```cpp
#include "bench.hpp"

const double t = ec::best_of(5, [&] { matvec(A, x, y, m, n); });
```

`ec::best_of` nimmt das **Minimum** aus fünf Läufen — siehe Kapitel 06, Aufgabe 6.9a.

`omp_get_wtime()` aus der Vorlesung tut dasselbe und bleibt korrekt; die Kapitel erwähnen
beides. **Nicht** benutzen: `clock()` — das summiert CPU-Zeit über alle Threads und wächst
beim Parallelisieren scheinbar an.

---

## 3. Lambdas

Ein Lambda ist eine Funktion, die man mitten im Code definiert und die Variablen aus ihrer
Umgebung **einfangen** kann:

```cpp
double faktor = 2.0;

auto skaliere = [&](double v) { return faktor * v; };   // [&] = per Referenz einfangen
auto verdopple = [=](double v) { return faktor * v; };  // [=] = per Kopie einfangen
auto rein     = [ ](double v) { return 2.0 * v; };      // [ ] = nichts einfangen
```

Für Messungen ist `[&]` das Übliche: Man will auf die echten Daten zugreifen, nicht auf eine
Kopie.

> **Merkhilfe:** `[&]` verhält sich wie OpenMP-`shared`, `[=]` wie `firstprivate`. Das ist
> nicht nur eine Analogie — es ist dieselbe Frage: eigene Kopie oder gemeinsame Speicherstelle?

---

## 4. Referenzen statt Zeiger

```cpp
void skaliere(std::vector<double>& v, double a) {      // Referenz: veraendert das Original
    for (std::size_t i = 0; i < v.size(); ++i) v[i] *= a;
}

double summe(const std::vector<double>& v) {           // const-Referenz: nur lesen, keine Kopie
    double s = 0.0;
    for (double x : v) s += x;                         // range-for
    return s;
}
```

Ohne `&` würde der ganze Vektor **kopiert** — bei 4096² `double` sind das 128 MB pro Aufruf.
Das ist der häufigste Performance-Fehler von C++-Anfängern.

Faustregel: **`const T&` zum Lesen, `T&` zum Schreiben, `T` nur für kleine Werte** (`int`,
`double`, Zeiger).

---

## 5. RAII — Aufräumen passiert von selbst

*Resource Acquisition Is Initialization*: Eine Ressource gehört einem Objekt, und wenn das
Objekt seinen Gültigkeitsbereich verlässt, wird sie freigegeben. Deshalb braucht man kein
`free`, kein `delete`, kein `fclose`.

```cpp
{
    std::vector<double> gross(100'000'000);   // 800 MB belegt
    ...
}                                             // hier automatisch freigegeben --
                                              // auch bei return oder Exception
```

Für Bäume und Graphen (Kapitel 07, 08):

```cpp
#include <memory>

struct Node {
    std::unique_ptr<Node> left, right;   // Besitz: raeumt den Teilbaum selbst ab
    double val = 0.0;
};

auto wurzel = std::make_unique<Node>();
// kein freetree() noetig
```

Beim Weiterreichen an Funktionen gibt man dann den **rohen Beobachter-Zeiger** weiter, nicht
den Besitz:

```cpp
void traverse(Node* p);        // p besitzt nichts, schaut nur
traverse(wurzel.get());
```

> **Vorsicht bei tiefen Bäumen:** Der automatisch erzeugte Destruktor von `unique_ptr` ist
> rekursiv. Bei einer Kette von 100 000 Knoten läuft der Stack über. Für sehr tiefe oder
> entartete Bäume ist eine flache Allokation (alle Knoten in einem `std::vector<Node>`,
> Verweise über Indizes) sowohl schneller als auch sicherer — und cache-freundlicher.

---

## 6. `auto`, range-for, strukturierte Bindungen

```cpp
auto t0 = std::chrono::steady_clock::now();   // Typ waere unlesbar lang
for (double v : werte) summe += v;            // range-for: lesend
for (double& v : werte) v *= 2.0;             // range-for: schreibend (Referenz!)

std::pair<double, int> f();
auto [zeit, threads] = f();                   // strukturierte Bindung (C++17)
```

`auto` nur dort, wo der Typ aus der rechten Seite offensichtlich ist. In Rechenkernen lieber
`double` und `int` ausschreiben — dort will man genau wissen, womit gerechnet wird.

---

## 7. OpenMP und C++: die Stolpersteine

Das hier ist der eigentlich wichtige Teil dieser Seite.

### 7.1 Der Schleifenindex

```cpp
// GEHT NICHT vor OpenMP 3.0, und ist auch danach eine Falle:
#pragma omp parallel for
for (auto it = v.begin(); it != v.end(); ++it) ...   // braucht Random-Access-Iteratoren

// SICHER und in jeder Klausur richtig:
#pragma omp parallel for
for (int i = 0; i < static_cast<int>(v.size()); ++i) ...
```

`v.size()` hat den Typ `std::size_t` (**vorzeichenlos**). Vergleicht man `int i < v.size()`,
warnt der Compiler zu Recht — und bei leerem Vektor wird `size()-1` zu einer riesigen Zahl.
Deshalb: Größe einmal in ein `int` (oder `long`) ziehen.

```cpp
const int n = static_cast<int>(v.size());
#pragma omp parallel for schedule(static)
for (int i = 0; i < n; ++i) ...
```

**Ein range-for lässt sich nicht direkt parallelisieren** — er hat keinen Schleifenindex, den
OpenMP aufteilen könnte. Für die parallele Schleife also immer die Indexform.

### 7.2 Scoping in C++

Die Regeln aus Kapitel 06 gelten unverändert. Zwei C++-Besonderheiten:

```cpp
const int n = ...;
std::vector<double> y(n);

#pragma omp parallel for default(none) shared(y, A, x, n)
for (int i = 0; i < n; ++i) {
    double s = 0.0;        // im Block deklariert -> automatisch private. Bevorzugen!
    ...
    y[i] = s;
}
```

- **Variablen möglichst innerhalb der Schleife deklarieren** — dann sind sie automatisch
  privat, und man kann `private(...)` ganz weglassen. In C89-Code der Vorlesung geht das
  nicht, in C++ immer.
- `const`-Variablen und Klassenmember verhalten sich bei `default(none)` je nach
  Compilerversion unterschiedlich. Wenn `g++` eine `const`-Variable in `shared(...)`
  anmeckert: einfach weglassen, sie ist ohnehin nur lesbar und damit unkritisch.

### 7.3 Klassenmethoden und `this`

```cpp
struct Gitter {
    std::vector<double> daten;
    int n;

    void skaliere(double a) {
        // ACHTUNG: daten und n sind Member -> Zugriff laeuft ueber "this"
        // Bei default(none) muss man "this" nennen, nicht die Membernamen:
        #pragma omp parallel for default(none) shared(this, a)
        for (int i = 0; i < n; ++i) daten[i] *= a;
    }
};
```

Häufigster Fehler in objektorientiertem OpenMP-Code: `shared(daten)` statt `shared(this)`.
Der einfachere Weg ist, die Member vorher in lokale Variablen zu ziehen:

```cpp
double* d = daten.data();
const int nn = n;
#pragma omp parallel for default(none) shared(d, nn, a)
for (int i = 0; i < nn; ++i) d[i] *= a;
```

Das ist nicht nur klarer, sondern oft auch schneller — der Compiler muss dann nicht bei jedem
Zugriff über `this` gehen.

### 7.4 Exceptions dürfen die parallele Region nicht verlassen

```cpp
#pragma omp parallel for
for (int i = 0; i < n; ++i) {
    if (schlecht(i)) throw std::runtime_error("...");   // UNDEFINIERTES VERHALTEN
}
```

Der OpenMP-Standard verlangt, dass eine Exception **innerhalb derselben Region** gefangen
wird, in der sie geworfen wurde. In der Praxis stürzt das Programm ab oder hängt. Richtig:

```cpp
std::atomic<bool> fehler{false};
#pragma omp parallel for
for (int i = 0; i < n; ++i) {
    try { arbeite(i); }
    catch (...) { fehler = true; }        // Fehler nur merken
}
if (fehler) throw std::runtime_error("...");   // erst NACH der Region werfen
```

### 7.5 `std::vector<bool>` niemals parallel beschreiben

`std::vector<bool>` ist eine Bit-Spezialisierung: Acht Einträge teilen sich **ein Byte**.
Zwei Threads, die `v[0]` und `v[1]` schreiben, greifen also auf **dieselbe Speicherstelle** zu
— eine echte Race Condition, nicht nur false sharing.

```cpp
std::vector<bool>    b(n);   // FALSCH fuer paralleles Schreiben
std::vector<char>    c(n);   // richtig
std::vector<uint8_t> u(n);   // richtig
```

### 7.6 Reduktion über eigene Typen

Seit OpenMP 4.0 lassen sich eigene Reduktionsoperatoren deklarieren:

```cpp
struct Stat { double summe = 0.0; double max = -1e300; };

#pragma omp declare reduction(vereinige : Stat :                        \
        omp_out.summe += omp_in.summe,                                  \
        omp_out.max = std::max(omp_out.max, omp_in.max))                \
        initializer(omp_priv = Stat{})

Stat s;
#pragma omp parallel for reduction(vereinige : s)
for (int i = 0; i < n; ++i) { s.summe += x[i]; s.max = std::max(s.max, x[i]); }
```

`omp_out` ist das laufende Ergebnis, `omp_in` der Beitrag eines Threads, `omp_priv` die
private Kopie beim Start (siehe die Initialisierer-Tabelle in Kapitel 06, Abschnitt 8.3).

---

## 8. Übersetzen

```bash
g++ -std=c++17 -O2 -Wall -Wextra -fopenmp prog.cpp -o prog
```

| Flag | Wirkung |
|---|---|
| `-std=c++17` | Sprachstandard festnageln (nicht auf die Compiler-Vorgabe verlassen) |
| `-O2` | Optimierung. **Ohne das sind alle Messungen wertlos** |
| `-Wall -Wextra` | Warnungen — gerade bei `size_t`/`int`-Vergleichen sehr nützlich |
| `-fopenmp` | Aktiviert die `#pragma omp`-Direktiven. Ohne das werden sie **stillschweigend ignoriert** |
| `-march=native` | Erlaubt AVX2/AVX-512 der eigenen CPU (relevant ab Kapitel 05b) |
| `-fsanitize=thread` | Race-Detektor, siehe Kapitel 06 |
| `-g` | Debug-Symbole, nötig für brauchbare Sanitizer-Ausgaben |

> **Der häufigste Anfängerfehler:** `-fopenmp` vergessen. Das Programm übersetzt fehlerfrei,
> läuft korrekt — und ist einthreadig. Immer mit `omp_get_max_threads()` gegenprüfen.

Und ein C++-spezifischer: `#include <omp.h>` fehlt, aber `-fopenmp` ist gesetzt. Dann
funktionieren die Pragmas, aber `omp_get_thread_num()` ist unbekannt.

---

## 9. Was in diesem Tutorial bewusst NICHT vorkommt

- **`std::thread`, `std::async`, `std::jthread`** — die C++-eigene Nebenläufigkeit. Sie ist
  nützlich, aber die Vorlesung behandelt OpenMP, und für datenparallele Schleifen ist OpenMP
  die deutlich kürzere Schreibweise.
- **`std::execution::par`** (parallele STL-Algorithmen) — elegant, aber in gcc braucht es
  Intel TBB als Unterbau und ist damit weniger portabel als OpenMP.
- **Templates in Rechenkernen** — sie würden die Kerne genau da unleserlich machen, wo man
  sehen will, was OpenMP verteilt. In den Kapiteln stehen deshalb konkrete `double`-Kerne.

Wo diese Dinge trotzdem interessant sind, steht ein Hinweis im jeweiligen Kapitel.
