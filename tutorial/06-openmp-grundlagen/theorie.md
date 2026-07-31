# Kapitel 06 — OpenMP-Grundlagen

> **Quellen:** `vl/6-Lecture-Openmp-Introduction.pdf` (Folien 1–56), Übungsblatt
> `excercises/uebung6.pdf`, eigene Abgabe `assignment6/`
> **Zeitbedarf:** ca. 60–75 min lesen + 90 min Aufgaben
> **Voraussetzungen:** Kapitel 04 (Cache-Hierarchie) hilft bei Abschnitt 9 (*false sharing*),
> ist aber nicht zwingend.

> **Codekonvention:** Aller Code hier ist C++17 (`g++ -std=c++17 -O2 -fopenmp`). Die
> parallelisierten Rechenschleifen sind bewusst roh gehalten — so stehen sie auch in der
> Klausur. Blöcke, die direkt aus den Folien stammen, sind mit `// Folie N` markiert und
> behalten den C-Stil der Vorlesung. Sprachmittel siehe
> [C++-Werkzeugkasten](../cpp-werkzeugkasten.md).

---

## 1. Warum OpenMP?

Ein moderner Prozessor wird seit ca. 2005 nicht mehr wesentlich schneller *pro Kern* — die
Taktfrequenz stagniert (siehe Kapitel 03, *Power Wall*). Der einzige verbliebene Weg zu mehr
Rechenleistung ist Parallelität: mehr Kerne, die gleichzeitig arbeiten. Der Rechner, auf dem
dieses Tutorial entsteht, hat 8 logische Kerne. Ein serielles Programm nutzt davon **einen**.
Es verschenkt also im besten Fall 87,5 % der Maschine.

Das Problem: Threads von Hand zu verwalten ist mühsam und fehleranfällig. Mit POSIX-Threads
sieht schon ein triviales „mach diese Schleife parallel" so aus:

```cpp
// pthreads: Struktur anlegen, Funktion auslagern, Bereiche ausrechnen,
// Threads erzeugen, joinen, aufräumen — für JEDE parallele Schleife neu.
typedef struct { double *y, *x; int lo, hi; } args_t;
void *worker(void *p) { args_t *a = p; for (int i = a->lo; i < a->hi; i++) ... }
pthread_t t[8]; args_t a[8];
for (int k = 0; k < 8; k++) { a[k] = ...; pthread_create(&t[k], NULL, worker, &a[k]); }
for (int k = 0; k < 8; k++) pthread_join(t[k], NULL);
```

Mit OpenMP wird daraus **eine Zeile über der Schleife**. Der Compiler erzeugt genau den
pthreads-Code oben für dich (Folie 9 zeigt diese Übersetzung explizit).

```cpp
#pragma omp parallel for
for (int i = 0; i < n; i++) y[i] = a * x[i] + y[i];
```

**Das ist die Kernidee von OpenMP:** Man schreibt weiterhin seriellen C/C++-Code und annotiert ihn. Ohne
`-fopenmp` ignoriert der Compiler alle `#pragma omp`-Zeilen und übersetzt ein korrektes
serielles Programm. Der Code bleibt also in beiden Welten lauffähig — das ist ein
Alleinstellungsmerkmal gegenüber pthreads oder MPI.

### 1.1 Shared Memory vs. Distributed Memory (Folie 3)

OpenMP setzt zwingend **shared memory** voraus. Der Unterschied ist klausurrelevant:

| | Distributed Memory | Shared Memory |
|---|---|---|
| Speicher | jeder Prozessor hat eigenen lokalen Speicher | alle Prozessoren teilen sich einen Adressraum |
| Kommunikation | explizite Nachrichten über Netzwerk (MPI) | implizit über gemeinsame Variablen |
| Kosten Datenbewegung | teuer (Netzwerklatenz) | „billig" (Cache/Speicher) |
| Skalierbarkeit | sehr gut, beliebig erweiterbar | begrenzt (Bus/Speicherbandbreite, Kohärenz) |
| Programmiermodell | MPI | **OpenMP**, pthreads |

> **Merke:** OpenMP skaliert auf *einem Knoten*. Für einen Cluster braucht man MPI — oder
> hybrid: MPI zwischen den Knoten, OpenMP innerhalb eines Knotens.

### 1.2 Was OpenMP genau ist (Folie 4)

OpenMP ist eine API für Shared-Memory-Parallelität in C, C++ und Fortran und besteht aus
**genau drei Bestandteilen** — diese Dreiteilung wird gern abgefragt:

1. **Compiler-Direktiven** — `#pragma omp parallel`, `#pragma omp for`, …
2. **Laufzeitbibliothek** — `omp_get_thread_num()`, `omp_get_wtime()`, … (`#include <omp.h>`)
3. **Umgebungsvariablen** — `OMP_NUM_THREADS`, `OMP_SCHEDULE`, …

---

## 2. Die Kernidee in drei Sätzen

1. Ein OpenMP-Programm startet **seriell** mit einem einzigen Thread (*master*); bei
   `#pragma omp parallel` spaltet es sich in ein *Team* von Threads auf (**Fork**) und
   vereinigt sich am Ende des Blocks wieder (**Join**).
2. Jeder Thread führt denselben Codeblock aus — erst eine **Work-Sharing-Direktive** wie
   `for` verteilt *unterschiedliche* Arbeit auf die Threads.
3. Ob das Ergebnis stimmt, entscheidet allein das **Scoping**: welche Variable ist `shared`
   (eine Speicherstelle für alle) und welche `private` (eine Kopie pro Thread).

---

## 3. Das Fork/Join-Modell

### 3.1 Hello World (Folien 5–7)

```cpp
#include <cstdio>
#include <omp.h>

int main() {
    #pragma omp parallel
    {
        std::printf("Hello World... von Thread %d\n", omp_get_thread_num());
    }
    return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp hello.cpp -o hello
./hello
```

Ausgabe (Beispiel, 4 Threads):

```
Hello World... von Thread 2
Hello World... von Thread 0
Hello World... von Thread 3
Hello World... von Thread 1
```

Zwei Beobachtungen, die man verinnerlicht haben muss:

- Der Block wird **vollständig von jedem Thread** ausgeführt — nicht aufgeteilt. Bei 4 Threads
  gibt es 4 Zeilen Ausgabe. `parallel` allein *repliziert*, es *verteilt* nicht.
- Die **Reihenfolge ist unbestimmt**. Es gibt keine Garantie, dass Thread 0 zuerst dran ist.
  Jede Aufgabe, deren Antwort „die Ausgabe ist ..." lautet, muss diese Nichtdeterminiertheit
  erwähnen.

### 3.2 Begriffe (Folie 12)

```
        Master
          │
          │   #pragma omp parallel {
     ┌────┼────┬────┐            Fork
     │    │    │    │
    T0   T1   T2   T3            Team  (T0 = Master)
     │    │    │    │
     └────┼────┴────┘            Join   }  ← implizite Barriere
          │
        Master
```

| Begriff | Bedeutung |
|---|---|
| **Master** | Der Thread, der die parallele Region betritt; hat im Team immer `omp_get_thread_num() == 0` |
| **Fork** | Erzeugen/Aktivieren des Teams beim Betreten der Region |
| **Team** | Die Menge aller Threads, die die Region ausführen (inkl. Master) |
| **Join** | Am Ende der Region warten alle aufeinander, dann läuft nur der Master weiter |

> **Wichtig:** Am Ende jeder `parallel`-Region steht eine **implizite Barriere**. Die kann man
> — anders als bei `for` oder `single` — **nicht** mit `nowait` entfernen.

### 3.3 Verschachtelte Parallelität (Folie 11)

```cpp
#pragma omp parallel        // Team der Größe p
{
    #pragma omp parallel    // jeder der p Threads forkt erneut
    { ... }                 // → potenziell p·p Threads
}
```

Nested Parallelism ist standardmäßig **deaktiviert** (die innere Region läuft dann mit einem
Thread, d. h. seriell). Einschalten über `OMP_NESTED=true` bzw. `omp_set_max_active_levels(n)`.
In der Praxis fast immer unerwünscht — Threads überzeichnen die Maschine (*oversubscription*).

---

## 4. Wie viele Threads? (Folien 13–18)

### 4.1 Die Laufzeitfunktionen

| Funktion | Bedeutung |
|---|---|
| `void omp_set_num_threads(int n)` | Setzt die Threadzahl für *nachfolgende* parallele Regionen ohne `num_threads`-Klausel |
| `int omp_get_num_threads(void)` | Anzahl Threads im **aktuellen Team** |
| `int omp_get_thread_num(void)` | ID des aufrufenden Threads im Team, `0 … num_threads-1` |
| `int omp_get_max_threads(void)` | Obere Schranke für die Teamgröße einer *künftigen* Region |
| `double omp_get_wtime(void)` | Vergangene **Wall-Clock**-Zeit in Sekunden |

**Die klassische Falle:** `omp_get_num_threads()` **außerhalb** einer parallelen Region liefert
immer `1` — denn das aktuelle Team besteht dort nur aus dem Master. Wer wissen will, mit wie
vielen Threads *gleich* gearbeitet wird, braucht `omp_get_max_threads()`.

```cpp
std::printf("%d\n", omp_get_num_threads());   // 1  — fast immer ein Bug
std::printf("%d\n", omp_get_max_threads());   // z.B. 8 — das war gemeint
```

### 4.2 Drei Wege, die Threadzahl zu setzen — und ihre Rangfolge

```bash
export OMP_NUM_THREADS=5          # 3. schwächste: Umgebungsvariable
```
```cpp
omp_set_num_threads(3);           // 2. überschreibt die Umgebungsvariable
#pragma omp parallel num_threads(7)   // 1. stärkste: gewinnt immer
```

**Rangfolge (stark → schwach): `num_threads`-Klausel > `omp_set_num_threads()` > `OMP_NUM_THREADS` > Implementierungsvorgabe.**

### 4.3 Durchgerechnet: das Beispiel von Folie 17

```cpp
// Folie 17 — Original der Vorlesung (C-Stil)
void report(int CP) {
    std::printf("CP %d (%d, %d, %d)\n", CP, omp_get_thread_num(),
           omp_get_num_threads(), omp_get_max_threads());
}
int main() {
    report(0);
    omp_set_num_threads(3);
    #pragma omp parallel
        report(1);
    report(2);
    omp_set_num_threads(5);
    #pragma omp parallel
        report(3);
}
```

Mit `OMP_NUM_THREADS=5` gestartet:

| Aufruf | thread_num | num_threads | max_threads | Warum |
|---|---|---|---|---|
| `CP 0` | 0 | **1** | **5** | seriell: Team = nur Master; max kommt aus `OMP_NUM_THREADS` |
| `CP 1` | 0…2 | 3 | 3 | 3× ausgegeben, Reihenfolge zufällig; `set_num_threads(3)` hat gewirkt |
| `CP 2` | 0 | **1** | 3 | wieder seriell; max ist jetzt 3 |
| `CP 3` | 0…4 | 5 | 5 | 5× ausgegeben |

---

## 5. Zeitmessung (Folie 19)

```cpp
double t0 = omp_get_wtime();
work();
double t1 = omp_get_wtime();
std::printf("%.6f s\n", t1 - t0);
```

`omp_get_wtime()` misst **Wall-Clock-Zeit**, nicht CPU-Zeit. Genau das will man bei
Parallelisierung: `clock()` aus `<time.h>` summiert die Zeit **über alle Threads** und wächst
beim Parallelisieren scheinbar an, obwohl das Programm schneller wird.

> **Falle (Folie 19):** Werden `start`/`end` außerhalb der parallelen Region deklariert, sind
> sie **shared**. Wenn dann alle Threads hineinschreiben, überschreiben sie sich gegenseitig —
> die Messung ist Unsinn (und formal ein *data race*). Messzeiten pro Thread gehören in
> `private`-Variablen, gemessen wird sonst **außerhalb** der Region.

Faustregeln für saubere Messungen:
- vor der Messung einmal „warmlaufen" lassen (Caches, Thread-Erzeugung),
- mehrfach messen und Minimum oder Median nehmen (nicht den Mittelwert — Ausreißer nach oben),
- Problemgröße groß genug wählen, dass der Fork/Join-Overhead (~µs) nicht dominiert.

---

## 6. Scoping — der wichtigste Abschnitt des Kapitels

Über 90 % aller OpenMP-Fehler sind Scoping-Fehler. Deshalb hier ausführlich.

### 6.1 Shared vs. private (Folie 21)

- **shared**: Alle Threads greifen auf **dieselbe Speicherstelle** zu. Schreibt einer, sehen es
  (irgendwann) alle. Notwendig für Ein-/Ausgabedaten — und gefährlich bei gleichzeitigen
  Schreibzugriffen.
- **private**: Jeder Thread bekommt eine **eigene Kopie** an eigener Adresse. Änderungen sind
  für andere unsichtbar; nach der Region ist die Kopie weg.

### 6.2 Die Default-Regeln (Folie 22) — auswendig können

1. Variablen, die **außerhalb** der parallelen Region deklariert wurden, sind **shared**.
2. Variablen, die **innerhalb** des parallelen Blocks deklariert werden, sind **private**.
3. **`static`**-Variablen sind **shared** — auch wenn sie im Block deklariert sind!
4. Die **Laufvariable** einer `omp for`-Schleife ist immer **private** (auch ohne Klausel).
5. Dynamisch allokierter Speicher: Der **Zeiger** folgt den Regeln oben, der **Speicher, auf
   den er zeigt, ist immer geteilt** — ein privater Zeiger auf denselben Heap-Block schützt
   gar nichts.

Regel 3 ist die unangenehme: `static int d = 0;` *sieht* lokal aus, liegt aber im
statischen Speicher und ist damit für alle Threads dieselbe Variable.

```cpp
// nach Folie 20
static int a = 0;                 // shared (global)
int main() {
    int b = 0;                    // shared (außerhalb deklariert)
    #pragma omp parallel
    {
        int c = 0;                // private
        static int d = 0;         // SHARED! trotz Deklaration im Block
        a++; b++; c++; d++;       // a, b, d: data race — c: harmlos
    }
}
```

Ergebnis mit 10 Threads: `c` ist bei jedem Thread genau `1`; `a`, `b` und `d` liegen
irgendwo zwischen 1 und 10 und sind bei jedem Lauf anders.

### 6.3 Die Scoping-Klauseln (Folie 23)

| Klausel | Wirkung |
|---|---|
| `private(a, b)` | Jeder Thread bekommt eine eigene, **uninitialisierte** Kopie |
| `shared(a, b)` | Eine gemeinsame Speicherstelle |
| `firstprivate(a)` | Private Kopie, **initialisiert** mit dem Wert von vor der Region |
| `lastprivate(a)` | Private Kopie; der Wert aus der **sequenziell letzten Iteration** wird nach außen zurückgeschrieben |
| `default(shared)` | Alles nicht Genannte ist shared (Voreinstellung in C/C++) |
| `default(none)` | **Erzwingt**, dass jede Variable explizit deklariert wird |

> **`default(none)` ist die beste Angewohnheit in diesem Kapitel.** Der Compiler weigert sich
> dann, Code zu übersetzen, in dem eine Variable nicht bewusst eingeordnet wurde. Genau das
> verlangt auch Übungsblatt 6 („specify every variable as shared or private").

### 6.4 `private` initialisiert **nicht** (Folien 24, 29)

```cpp
int i = -1;
#pragma omp parallel private(i)
{
    std::printf("%d\n", i);     // UNDEFINIERT — nicht -1!
    i = omp_get_thread_num();
}
std::printf("i = %d\n", i);     // -1 — die Originalvariable ist unberührt
```

Zwei Aussagen, die beide in Klausuren auftauchen:

- Eine `private`-Kopie startet **uninitialisiert** (in der Praxis oft 0 oder Müll — verlassen
  darf man sich auf nichts).
- Das Original **außerhalb** behält seinen alten Wert; nichts wird zurückgeschrieben.

### 6.5 `firstprivate` und `lastprivate` (Folien 26–31)

```cpp
int i = 10;
#pragma omp parallel firstprivate(i)
{
    std::printf("%d\n", i);            // jeder Thread: 10
    i = 1000 + omp_get_thread_num();
}
std::printf("i = %d\n", i);            // 10 — lastprivate fehlt, kein Rückschreiben
```

```cpp
int i = -1, j = -1;
#pragma omp parallel for firstprivate(i) lastprivate(j)
for (int k = 0; k < 2; k++) { j = k; }
std::printf("i = %d, j = %d\n", i, j);   // i = -1, j = 1
```

`j` bekommt den Wert aus der Iteration, die **im seriellen Ablauf die letzte wäre** (`k == 1`) —
und zwar unabhängig davon, welcher Thread diese Iteration tatsächlich ausgeführt hat und wann er
fertig war.

> **Merksatz:** `firstprivate` = Wert **hinein**, `lastprivate` = Wert **hinaus**,
> `private` = weder noch. Beides zusammen ist erlaubt.

### 6.6 Scoping-Entscheidungsbaum

```
Wird die Variable im parallelen Block geschrieben?
├─ nein  → shared (nur lesen ist immer sicher)
└─ ja
   ├─ Schreibt jeder Thread an eine EIGENE Stelle (z. B. y[i] mit eigenem i)?
   │     → shared  (das Array), kein Konflikt
   ├─ Braucht jeder Thread einen eigenen Zwischenwert (Zähler, temp, innerer Index)?
   │     → private   (+ firstprivate, falls der Startwert von außen gebraucht wird)
   ├─ Werden alle Beiträge am Ende zu EINEM Wert verknüpft (+, *, max, …)?
   │     → reduction(op : var)
   └─ Sonst (echter gemeinsamer Zustand, z. B. Histogramm-Bin)
         → shared + Synchronisation (atomic / critical)
```

---

## 7. Work-Sharing: `for`, `single`, `master`

### 7.1 `#pragma omp for` (Folien 34–35)

Erst hier wird Arbeit **verteilt** statt repliziert:

```cpp
#pragma omp parallel          // Fork: p Threads
{
    #pragma omp for           // Work-Sharing: Iterationen aufteilen
    for (int i = 0; i < n; i++) a[i] = i;
}                             // Join
```

Kurzform, wenn die Region nur aus der Schleife besteht:

```cpp
#pragma omp parallel for
for (int i = 0; i < n; i++) a[i] = i;
```

**Wann welche Form?** Die Langform lohnt, wenn mehrere Schleifen (oder Schleife + anderer Code)
in *einer* Region liegen — dann wird nur einmal geforkt statt zweimal:

```cpp
#pragma omp parallel                    // ein Fork für beide Schleifen
{
    #pragma omp for
    for (int i = 0; i < n; i++) a[i] = f(i);
    #pragma omp for nowait              // Barriere dazwischen bewusst entfernt
    for (int i = 0; i < n; i++) b[i] = g(i);
}
```

**Bedingungen an die Schleife** (sonst lehnt der Compiler ab): Es muss eine *kanonische*
`for`-Schleife sein — Laufvariable ganzzahlig, Schranke und Schrittweite während der Ausführung
konstant, kein `break`. Die Iterationszahl muss beim Betreten berechenbar sein. `continue` ist
erlaubt, `return`/`goto` aus der Schleife heraus nicht.

**Die inhaltliche Voraussetzung ist wichtiger:** Die Iterationen müssen **unabhängig** sein.
`a[i+1] = a[i]` ist es nicht — hier liegt eine *loop-carried dependence* vor, und OpenMP
liefert schlicht ein falsches Ergebnis, ohne zu warnen. Der Compiler prüft das **nicht**;
`#pragma omp for` ist ein Versprechen des Programmierers.

### 7.2 Geschachtelte Schleifen und `collapse`

```cpp
#pragma omp parallel for                 // nur die i-Schleife wird verteilt
for (int i = 0; i < m; i++)
    for (int j = 0; j < n; j++) ...      // läuft komplett in einem Thread
```

Das ist normalerweise genau richtig: Die äußere Schleife parallelisieren heißt große
Arbeitspakete, wenig Overhead, und jeder Thread arbeitet auf zusammenhängendem Speicher. Nur
wenn `m` kleiner ist als die Threadzahl, bleibt Arbeit liegen — dann verschmilzt `collapse(2)`
beide Schleifen zu einem Iterationsraum der Größe `m·n`:

```cpp
#pragma omp parallel for collapse(2)
for (int i = 0; i < m; i++)
    for (int j = 0; j < n; j++) C[i][j] = ...;   // nur bei unabhängigen Iterationen!
```

### 7.3 `single` und `master` (Folien 32–33)

| Direktive | Wer führt aus? | Implizite Barriere am Ende? |
|---|---|---|
| `#pragma omp single` | **irgendein** Thread (der erste, der ankommt) | **ja** (entfernbar mit `nowait`) |
| `#pragma omp master` | nur Thread 0 | **nein** |

Der Unterschied bei der Barriere ist der eigentliche Prüfungsinhalt. `single` eignet sich für
Initialisierung, deren Ergebnis alle brauchen (die Barriere garantiert die Sichtbarkeit);
`master` für Ausgaben, bei denen niemand warten soll.

> `master` gilt seit OpenMP 5.1 als *deprecated*; der Nachfolger heißt `masked`. In der
> Vorlesung wird `master` verwendet — beides erwähnen ist sicher.

### 7.4 `if`-Klausel (Folie 33)

```cpp
#pragma omp parallel if (n > 10000)
```

Ist die Bedingung falsch, wird die Region **seriell** ausgeführt (Team der Größe 1). Sinnvoll,
weil Fork/Join einige Mikrosekunden kostet: Bei `n = 100` ist die parallele Version langsamer
als die serielle. `omp_in_parallel()` sagt zur Laufzeit, welcher Fall eingetreten ist.

---

## 8. Reduktion (Folien 36–37)

### 8.1 Das Problem

```cpp
int s = 0;
#pragma omp parallel for shared(s)      // FALSCH
for (int i = 0; i < n; i++) s += a[i];
```

`s += a[i]` ist kein atomarer Schritt, sondern **Lesen → Addieren → Schreiben**. Zwei Threads
können denselben Altwert lesen, beide um ihren Anteil erhöhen und nacheinander zurückschreiben —
ein Beitrag geht verloren. Das Ergebnis ist zu klein und bei jedem Lauf anders.

### 8.2 Die Lösung

```cpp
int s = 0;
#pragma omp parallel for reduction(+ : s)
for (int i = 0; i < n; i++) s += a[i];
```

Was OpenMP daraus macht:

1. Jeder Thread bekommt eine **private Kopie** von `s`, initialisiert mit dem **neutralen
   Element** des Operators (bei `+`: 0, bei `*`: 1).
2. Jeder Thread summiert seinen Teil lokal — ohne jede Synchronisation, volle Geschwindigkeit.
3. Am Ende der Region werden alle Teilergebnisse **einmal** mit dem Operator kombiniert und mit
   dem Wert *vor* der Region verknüpft.

```
   s=0        s=0        s=0        s=0          private Kopien
    │          │          │          │
    ▼          ▼          ▼          ▼
    1          5          9         13           Teilsummen
    └──────────┴────┬─────┴──────────┘
                    ▼  +
                   45                            → shared s
```

### 8.3 Operatoren und ihre neutralen Elemente (Folie 36)

| Operator | Initialwert der Kopie | Kombination |
|---|---|---|
| `+` | `0` | `out += in` |
| `-` | `0` | `out -= in` |
| `*` | `1` | `out *= in` |
| `&` | `~0` (alle Bits 1) | `out &= in` |
| `\|` | `0` | `out \|= in` |
| `^` | `0` | `out ^= in` |
| `&&` | `1` | `out = in && out` |
| `\|\|` | `0` | `out = in \|\| out` |
| `max` | kleinstmöglicher Wert des Typs | `out = in > out ? in : out` |
| `min` | größtmöglicher Wert des Typs | `out = in < out ? in : out` |

> **Achtung bei Gleitkomma:** Die Reduktion ändert die **Summationsreihenfolge**. Da
> Gleitkommaaddition nicht assoziativ ist, weicht das Ergebnis in den letzten Stellen von der
> seriellen Summe ab — und kann von Lauf zu Lauf variieren. Das ist **kein Bug**. Beim Testen
> deshalb nie auf `==` prüfen, sondern auf `|parallel - seriell| < ε`.

Reduktion ist immer der Synchronisation (`critical`/`atomic`) vorzuziehen: Sie synchronisiert
nur einmal am Ende statt bei jeder Iteration.

---

## 9. Race Conditions und false sharing

### 9.1 Race Condition (Folien 46–47)

Eine *race condition* liegt vor, wenn

1. zwei oder mehr Threads auf dieselbe Speicherstelle zugreifen,
2. mindestens einer davon **schreibt**, und
3. der Zugriff **nicht** durch Scoping oder Synchronisation geschützt ist.

Dann hängt das Ergebnis von der zufälligen zeitlichen Verzahnung ab.

```cpp
int a = 0;
#pragma omp parallel num_threads(8)
{
    int b = a;      // lesen
    b = b + 1;      // rechnen
    a = b;          // schreiben
}
// erwartet: a == 8 — tatsächlich: irgendwas zwischen 1 und 8
```

Warum das so tückisch ist: Bei kleinen Problemgrößen, unter dem Debugger oder mit `-O0` kommt
oft „zufällig" das richtige Ergebnis heraus. Der Fehler zeigt sich dann erst in Produktion.
Werkzeug der Wahl ist deshalb ein Race-Detektor:

```bash
g++ -std=c++17 -O1 -g -fopenmp -fsanitize=thread prog.cpp -o prog && setarch -R ./prog
```

### 9.2 False Sharing (Folien 43–45)

Der subtilere Fall: **Das Ergebnis ist korrekt, die Performance bricht trotzdem ein.**

Die Cache-Kohärenz arbeitet nicht auf einzelnen Bytes, sondern auf **Cache-Lines** (typisch
64 Byte = 8 `double` oder 16 `int`). Schreibt Thread 0 in `sum[0]` und Thread 1 in `sum[1]`,
sind das zwar verschiedene Variablen — sie liegen aber in **derselben Cache-Line**. Jedes
Schreiben invalidiert die Kopie im Cache des anderen Kerns; die Line pendelt zwischen den
Kernen hin und her (*cache line ping-pong*).

```cpp
double sum[MAX_THREADS];                 // 8 doubles = genau eine Cache-Line
#pragma omp parallel
{
    int id = omp_get_thread_num();
    for (int i = id; i < n; i += nthreads)
        sum[id] += f(i);                 // false sharing bei JEDER Iteration
}
```

Drei Gegenmittel, in aufsteigender Eleganz:

```cpp
// 1. Padding: jedes Element auf eine eigene Cache-Line schieben
double sum[MAX_THREADS][8];              // 8 doubles Abstand = 64 Byte
sum[id][0] += f(i);

// 2. Alignment erzwingen
double *sum = aligned_alloc(64, nthreads * 64);

// 3. Am besten: gar nicht erst geteilt schreiben — lokal akkumulieren
double partial = 0.0;                    // private, liegt im Register/Stack
for (...) partial += f(i);
#pragma omp critical
full_sum += partial;                     // nur EINMAL pro Thread
```

Variante 3 ist genau der Schritt von Folie 41 („simple") zu Folie 42 („final") beim
π-Beispiel — und praktisch das, was `reduction` automatisch tut.

> **Prüfungsformulierung:** *Race Condition* → falsches Ergebnis. *False Sharing* → richtiges
> Ergebnis, schlechte Performance. Diese Unterscheidung wird gern abgefragt.

---

## 10. Synchronisation (Folien 48–55)

| Konstrukt | Bedeutung |
|---|---|
| `critical` | Der Block wird zu jedem Zeitpunkt von höchstens einem Thread ausgeführt |
| `atomic` | **Nur die Speicheroperation** der nächsten Anweisung wird atomar ausgeführt |
| `barrier` | Alle Threads warten, bis alle angekommen sind |
| `nowait` | Entfernt die implizite Barriere am Ende eines Work-Sharing-Konstrukts |
| `ordered` | Der Block wird in der Reihenfolge der seriellen Iterationen ausgeführt |

### 10.1 `critical` vs. `atomic`

```cpp
#pragma omp critical            // beliebig großer Block
{ int b = a; b = b + 1; a = b; }

#pragma omp atomic update       // nur die eine Speicheroperation
a++;
```

| | `critical` | `atomic` |
|---|---|---|
| Umfang | ganzer strukturierter Block | genau eine Speicheroperation |
| Umsetzung | Lock (Software) | Hardware-Instruktion (z. B. `lock xadd`) |
| Kosten | hoch | deutlich geringer |
| Anwendbar auf | beliebigen Code | nur `x++`, `x--`, `x binop= expr`, … |

**Vorsicht bei benannten `critical`-Regionen:** Ein unbenanntes `critical` ist *global* — zwei
inhaltlich unabhängige `critical`-Blöcke im Programm blockieren sich gegenseitig. Abhilfe:
`#pragma omp critical(name)`.

`atomic` kennt vier Klauseln (Folien 50–51): `update` (Voreinstellung), `read`, `write`,
`capture`. `capture` liest den alten oder neuen Wert atomar mit aus: `v = x++;`.

### 10.2 Barrieren — implizit und explizit

**Wo steht automatisch eine Barriere?**

| Konstrukt | Implizite Barriere am Ende | Mit `nowait` entfernbar? |
|---|---|---|
| `parallel` | ja | **nein** |
| `for` | ja | ja |
| `single` | ja | ja |
| `sections` | ja | ja |
| `master` / `masked` | **nein** | — |
| `critical`, `atomic` | nein | — |

`nowait` ist genau dann erlaubt, wenn die *nächste* Schleife nicht auf Daten der vorherigen
angewiesen ist:

```cpp
#pragma omp parallel
{
    #pragma omp for nowait                 // OK: b hängt nicht von a ab
    for (int i = 0; i < n; i++) a[i] = f(i);
    #pragma omp for
    for (int i = 0; i < n; i++) b[i] = g(i);
}
```

Gefährlich wird es, wenn Thread 1 in Schleife 2 `a[i]` liest, das Thread 0 in Schleife 1 noch
gar nicht geschrieben hat — dann ist `nowait` ein Datenfehler. Übungsblatt 6 verlangt
ausdrücklich, „die Anzahl impliziter Barrieren zu reduzieren" — gemeint ist genau diese
Abwägung, nicht das blinde Streuen von `nowait`.

### 10.3 `ordered`

```cpp
#pragma omp parallel for ordered
for (int i = 0; i < n; i++) {
    heavy_work(i);                 // parallel
    #pragma omp ordered
    std::printf("%d\n", i);             // in serieller Reihenfolge: 0,1,2,…
}
```

Nützlich für geordnete Ausgabe bei parallel berechneten Ergebnissen. Der `ordered`-Block
serialisiert — er sollte also möglichst klein sein.

---

## 11. Scheduling (Folie 56)

```cpp
#pragma omp parallel for schedule(type, chunk)
```

| Typ | Verteilung | Gut wenn |
|---|---|---|
| `static` | Zuteilung **vor** dem Schleifenstart, gleich große zusammenhängende Blöcke (ohne `chunk`) bzw. Blöcke der Größe `chunk` reihum | Alle Iterationen kosten gleich viel |
| `dynamic` | Threads holen sich Blöcke der Größe `chunk` (Voreinstellung 1) zur Laufzeit, wenn sie fertig sind | Iterationskosten schwanken stark |
| `guided` | Wie `dynamic`, aber die Blockgröße beginnt groß und schrumpft exponentiell bis auf `chunk` | Kosten schwanken, aber Overhead soll klein bleiben |
| `auto` | Compiler/Laufzeit entscheidet | — |
| `runtime` | Entscheidung über `OMP_SCHEDULE` zur Laufzeit | Experimentieren ohne Neuübersetzen |

Beispiel `n = 16`, `p = 4`:

```
static            T0: 0-3    T1: 4-7    T2: 8-11   T3: 12-15
static,2          T0: 0,1,8,9  T1: 2,3,10,11  T2: 4,5,12,13  T3: 6,7,14,15
dynamic,2         wer frei wird, nimmt den nächsten Zweierblock — Zuordnung nicht vorhersagbar
guided,2          erst große Blöcke (≈ Rest/p), am Ende Zweierblöcke
```

**Der Trade-off:** `static` hat **null** Laufzeit-Overhead (jeder Thread weiß sofort, was er zu
tun hat), verteilt aber blind. `dynamic` gleicht Last aus, kostet aber pro Block eine
Synchronisation an einem gemeinsamen Zähler.

Typisches Beispiel für schlechte Balance mit `static`: eine Dreiecksschleife

```cpp
#pragma omp parallel for schedule(dynamic, 16)   // ohne dynamic macht T3 4x so viel wie T0
for (int i = 0; i < n; i++)
    for (int j = i; j < n; j++) ...              // Arbeit wächst mit i
```

Ein weiterer Grund für `static`: **Reproduzierbarkeit**. Nur bei `static` bearbeitet derselbe
Thread bei jedem Lauf dieselben Iterationen — wichtig für NUMA-Lokalität (*first touch*) und
für bit-identische Gleitkommaergebnisse.

---

## 12. Durchgerechnet: die drei Fragmente von Blatt 6

Diese drei Fragmente sind die Blaupause für praktisch jede OpenMP-Klausuraufgabe. Sie zeigen
drei Stufen: unabhängig, reduzierbar, abhängig.

### a) Matrix-Vektor-Produkt — der einfache Fall

```cpp
for (i = 0; i < m; i++) {
    y[i] = 0;
    for (j = 0; j < n; j++)
        y[i] = y[i] + A[i][j] * x[j];
}
```

Jede Zeile `i` ist von allen anderen unabhängig — jeder Thread schreibt nur in sein eigenes
`y[i]`. Also: äußere Schleife parallelisieren.

```cpp
#pragma omp parallel for default(none) shared(A, x, y, m, n) private(i, j)
for (i = 0; i < m; i++) {
    y[i] = 0;
    for (j = 0; j < n; j++)
        y[i] += A[i][j] * x[j];
}
```

Entscheidend: **`j` muss private sein.** `j` ist außerhalb deklariert und damit per Default
shared — alle Threads würden denselben inneren Zähler hochzählen. Das ist der häufigste Fehler
in dieser Aufgabe. (Deklariert man `int j` innerhalb der Schleife, ist es automatisch private —
sauberer, aber die Klausur gibt den Code oft im C89-Stil vor.)

Warum nicht die innere Schleife parallelisieren? Dann gäbe es `m` Fork/Joins statt einem, und
`y[i] +=` wäre eine Reduktion. Viel mehr Overhead bei gleicher Arbeit.

### b) Zusätzlich die Summe aller Einträge — Reduktion

```cpp
s = 0;
for (i = 0; i < m; i++) { y[i] = 0; ...; s += y[i]; }
```

`s` wird von allen Threads geschrieben → Reduktion:

```cpp
#pragma omp parallel for default(none) shared(A, x, y, m, n) private(i, j) reduction(+ : s)
for (i = 0; i < m; i++) {
    y[i] = 0;
    for (j = 0; j < n; j++) y[i] += A[i][j] * x[j];
    s += y[i];
}
```

`shared(s)` + `critical` würde auch funktionieren, wäre aber deutlich langsamer: eine
Serialisierung pro Zeile statt einer pro Thread.

### c) Präfixsumme — der abhängige Fall

```cpp
y[0] = 0;
for (i = 1; i < m; i++) {
    y[i] = y[i-1];                                  // ← Abhängigkeit!
    for (j = 0; j < n; j++) y[i] += A[i][j] * x[j];
}
```

`y[i]` braucht `y[i-1]`. Ein `#pragma omp parallel for` darüber liefert **falsche Ergebnisse** —
und das ist die eigentliche Lehre der Aufgabe.

Der Trick: Die Rechnung in zwei Phasen zerlegen. Mit `r[i] = Σ_j A[i][j]·x[j]` (Zeilenprodukt)
gilt

$$ y[i] = \sum_{k=1}^{i} r[k] $$

also eine **Präfixsumme** über die Zeilenprodukte. Phase 1 ist voll parallel, Phase 2 ist ein
Scan:

```cpp
std::vector<double> r(m);                 // wird automatisch freigegeben

// Phase 1: unabhängig, voll parallel — hier steckt die gesamte O(m·n)-Arbeit
#pragma omp parallel for default(none) shared(A, x, r, m, n) schedule(static)
for (int i = 1; i < m; ++i) {
    double s = 0.0;                       // im Block deklariert -> automatisch private
    for (int j = 0; j < n; ++j) s += A[i * n + j] * x[j];
    r[i] = s;
}

// Phase 2: Präfixsumme, nur O(m) — seriell völlig ausreichend
y[0] = 0.0;
for (int i = 1; i < m; ++i) y[i] = y[i - 1] + r[i];
```

**Das Argument, das die Punkte bringt:** Phase 1 kostet `O(m·n)`, Phase 2 nur `O(m)`. Für
`n ≫ 1` ist der serielle Anteil verschwindend klein — nach Amdahl (Kapitel 10) bleibt der
Speedup fast ideal. Eine vollständig parallele Präfixsumme (`#pragma omp scan` seit OpenMP 5.0,
oder die zweiphasige Variante mit Thread-Teilsummen) ist möglich, lohnt sich hier aber nicht.

---

## 13. Typische Klausurfragen

- **Aus welchen drei Bestandteilen besteht OpenMP?** Direktiven, Laufzeitbibliothek,
  Umgebungsvariablen.
- **Was gibt `omp_get_num_threads()` außerhalb einer parallelen Region zurück?** `1`.
- **Unterschied `private` / `firstprivate` / `lastprivate`?** Keine Initialisierung / Wert von
  außen hinein / Wert der sequenziell letzten Iteration nach außen.
- **Warum ist eine `static`-Variable im parallelen Block shared?** Sie liegt im statischen
  Speicher, nicht auf dem Thread-Stack.
- **Wo stehen implizite Barrieren, und wo hilft `nowait`?** Siehe Tabelle 10.2 — `parallel`
  ist die Ausnahme, die man nicht entfernen kann.
- **Unterschied `critical` / `atomic`?** Block vs. einzelne Speicheroperation; Lock vs.
  Hardware-Instruktion.
- **Unterschied Race Condition / false sharing?** Falsches Ergebnis vs. korrektes Ergebnis mit
  Performanceeinbruch.
- **Wann `dynamic` statt `static`?** Bei ungleichen Iterationskosten.
- **Warum liefert `#pragma omp parallel for` bei `a[i] = a[i-1] + b[i]` Unsinn?**
  Loop-carried dependence; OpenMP prüft das nicht.
- **Warum ist `single` nicht dasselbe wie `master`?** Beliebiger Thread + Barriere vs.
  Thread 0 ohne Barriere.

---

## 14. Fallstricke auf einen Blick

| Fehler | Warum falsch | Richtig |
|---|---|---|
| Innerer Schleifenindex nicht privat | Alle Threads teilen den Zähler → Chaos | `private(j)` oder `for (int j = …)` |
| `shared(sum)` mit `sum += …` | Lost Update | `reduction(+ : sum)` |
| `private(x)` und Startwert erwartet | `private` initialisiert nicht | `firstprivate(x)` |
| Wert nach der Region auslesen | Private Kopien werden nicht zurückgeschrieben | `lastprivate(x)` oder `reduction` |
| `nowait` überall gesetzt | Zweite Schleife liest noch nicht geschriebene Daten | Nur bei nachweislicher Unabhängigkeit |
| `critical` in der innersten Schleife | Serialisiert das Programm vollständig | Lokal akkumulieren, einmal am Ende kombinieren |
| `clock()` zur Messung | Summiert CPU-Zeit über alle Threads | `omp_get_wtime()` |
| Parallelisieren trotz Abhängigkeit | Falsches Ergebnis ohne Warnung | Umformulieren (Scan, zwei Phasen) |
| `static` im parallelen Block | Ist shared, nicht privat | Normale lokale Variable |
| Gleitkomma-Ergebnis mit `==` testen | Reduktion ändert die Summationsreihenfolge | Toleranz `\|a-b\| < ε` |

---

## 15. Merkkasten

> **Kernaussagen des Kapitels**
> - `parallel` **repliziert**, erst `for` **verteilt**.
> - Default-Scoping: außen deklariert → shared, innen → private, `static` → shared,
>   Schleifenindex des `for`-Konstrukts → private.
> - `default(none)` erzwingt bewusste Entscheidungen — im Zweifel immer setzen.
> - `reduction` schlägt `critical` schlägt `atomic`-in-der-Schleife.
> - Implizite Barriere nach `parallel`, `for`, `single`, `sections`; **nicht** nach `master`.
>   `nowait` entfernt sie überall außer nach `parallel`.
> - Race Condition = falsches Ergebnis. False Sharing = richtiges Ergebnis, miese Performance.
> - `static` = kein Overhead; `dynamic` = Lastausgleich. Wähle nach der Varianz der
>   Iterationskosten.
> - OpenMP prüft keine Datenabhängigkeiten. Das ist deine Aufgabe.

---

## 16. Verbindung zum Rest der Vorlesung

- **Kapitel 04/05 (Cache):** *False sharing* ist ein reines Cache-Line-Phänomen; ohne das
  ideale Cache-Modell im Kopf ist es nicht erklärbar. Auch die Wahl „äußere Schleife
  parallelisieren" folgt der Cache-Lokalität.
- **Kapitel 07 (OpenMP Tasks):** `for` verlangt eine zählbare Schleife. Rekursion,
  Baumtraversierung und `while`-Schleifen brauchen `task` — die direkte Fortsetzung.
- **Kapitel 08/09 (Barnes-Hut, Ray Tracing):** Beides sind Anwendungsfälle mit stark
  schwankenden Iterationskosten — dort wird `schedule(dynamic)` wirklich gebraucht.
- **Kapitel 10 (Amdahl/Gustafson):** Liefert das Werkzeug, um zu bewerten, ob der gemessene
  Speedup gut ist. Fragment c) oben ist ein Amdahl-Argument in Reinform.
- **Kapitel 11/12 (CUDA):** Dasselbe Denken auf anderer Hardware — Team ↔ Block,
  Thread ↔ Thread, `reduction` ↔ Shared-Memory-Reduktion, false sharing ↔ uncoalesced access.
- **Eigene Abgabe:** `assignment6/src/` enthält die drei Fragmente aus Abschnitt 12,
  `project/` einen mit `parallel for` parallelisierten Ray Tracer inklusive Speedup-Messung.

---

**Weiter:** [Übungen](uebungen.md) → danach [Lösungen](loesungen.md) ·
Lauffähige Beispiele in [`code/`](code/)
