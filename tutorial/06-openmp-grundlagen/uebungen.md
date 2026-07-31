# Kapitel 06 — Übungen: OpenMP-Grundlagen

Erst selbst bearbeiten, dann mit [`loesungen.md`](loesungen.md) vergleichen. Für die
Code-Aufgaben liegt ein fertiges Makefile in [`code/`](code/) bereit:

```bash
cd code && make && ./bin/<programm>
```

Tags: `[Rechnen]` `[Code]` `[Analyse]` `[Klausur]`

---

## Aufgabe 6.1 `[Klausur]` — Scoping vorhersagen

*Zeit: ca. 10 min*

```cpp
#include <cstdio>
#include <omp.h>

int g = 0;

int main() {
    int h = 0;
    omp_set_num_threads(4);
    #pragma omp parallel
    {
        int u = 0;
        static int v = 0;
        g++; h++; u++; v++;
    }
    std::printf("g=%d h=%d\n", g, h);
    return 0;
}
```

a) Gib für jede der vier Variablen `g`, `h`, `u`, `v` an, ob sie shared oder private ist, und
   begründe es jeweils mit der zutreffenden Default-Regel.
b) Welchen Wert hat `u` am Ende des Blocks in jedem Thread?
c) Welche Werte kann die `printf`-Ausgabe annehmen? Gib den möglichen Wertebereich an und
   begründe, warum das Ergebnis nicht deterministisch ist.
d) Nenne zwei verschiedene Möglichkeiten, den Code so zu ändern, dass `g` und `h` am Ende
   garantiert den Wert 4 haben.

---

## Aufgabe 6.2 `[Klausur]` — Threadzahl und Rangfolge

*Zeit: ca. 10 min*

Das Programm wird mit `export OMP_NUM_THREADS=6` gestartet.

```cpp
#include <cstdio>
#include <omp.h>

int main() {
    std::printf("A: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    #pragma omp parallel num_threads(2)
    {
        #pragma omp master
        std::printf("B: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    }
    omp_set_num_threads(3);
    std::printf("C: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    #pragma omp parallel
    {
        #pragma omp single
        std::printf("D: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    }
    return 0;
}
```

a) Gib die vollständige Ausgabe an (Zeilen A bis D, jeweils beide Zahlen).
b) Begründe Zeile A: Warum ist die erste Zahl nicht 6?
c) Warum ändert `num_threads(2)` in Zeile B den Wert von `omp_get_max_threads()` **nicht**?
d) Was ändert sich an der Ausgabe, wenn man `master` durch `single` ersetzt — und was am
   Ablauf des Programms?

---

## Aufgabe 6.3 `[Rechnen]` — Scheduling und Lastbalance

*Zeit: ca. 20 min*

Gegeben sei eine Schleife mit `n = 24` Iterationen, ausgeführt von `p = 4` Threads.

a) Gib für jede der folgenden Klauseln an, welcher Thread welche Iterationen bearbeitet.
   Wo die Zuordnung nicht vorhersagbar ist, sage es und begründe warum.
   - `schedule(static)`
   - `schedule(static, 2)`
   - `schedule(dynamic, 3)`

b) Nun kostet Iteration `i` genau `i + 1` Zeiteinheiten (Iteration 0 kostet 1, Iteration 23
   kostet 24). Berechne für `schedule(static)` die Arbeit jedes Threads, die Gesamtlaufzeit
   `T(4)` (angenommen, alle Threads starten gleichzeitig) sowie Speedup und Effizienz
   gegenüber der seriellen Laufzeit.

c) Welche Laufzeit wäre bei perfekter Lastverteilung möglich? Wie groß ist der Verlust durch
   `static` in Prozent?

d) Berechne dieselben Größen für `schedule(static, 2)`. Erkläre, warum diese Variante hier
   besser abschneidet, ohne dass Laufzeit-Overhead entsteht.

e) Nenne einen Fall, in dem man trotz ungleicher Iterationskosten bewusst `static` wählt.

---

## Aufgabe 6.4 `[Analyse]` — Zwei Programme, zwei verschiedene Fehler

*Zeit: ca. 15 min*

```cpp
/* Programm P1 */
double sum = 0.0;
#pragma omp parallel for shared(sum, a) private(i)
for (i = 0; i < n; i++)
    sum += a[i];
```

```cpp
/* Programm P2 */
double partial[8];                       /* p = 8 Threads */
#pragma omp parallel num_threads(8)
{
    int id = omp_get_thread_num();
    partial[id] = 0.0;
    #pragma omp for
    for (int i = 0; i < n; i++)
        partial[id] += a[i];
}
double sum = 0.0;
for (int k = 0; k < 8; k++) sum += partial[k];
```

a) P1 liefert bei jedem Lauf ein anderes, zu kleines Ergebnis. Benenne das Problem und
   erkläre auf Ebene der drei Maschinenoperationen, wie der Fehler entsteht.
b) P2 liefert das **richtige** Ergebnis, ist aber bei 8 Threads teilweise langsamer als die
   serielle Version. Benenne das Problem und erkläre die Ursache.
c) Wie groß ist eine Cache-Line typischerweise, und wie viele `double` passen hinein? Wie
   viele Cache-Lines belegt `partial` in P2?
d) Repariere beide Programme. Gib für P2 zwei verschiedene Lösungen an: eine mit Padding und
   eine, die das Problem strukturell vermeidet.
e) Welche der beiden Fehlerklassen findet ein Race-Detektor wie ThreadSanitizer — und warum
   die andere nicht?

---

## Aufgabe 6.5 `[Code]` — Vektor normalisieren in zwei Phasen

*Zeit: ca. 20 min*

Gegeben ist ein Vektor `x ∈ ℝⁿ`. Er soll auf Länge 1 normiert werden:

$$ \|x\|_2 = \sqrt{\sum_{i=0}^{n-1} x_i^2}, \qquad x_i \leftarrow \frac{x_i}{\|x\|_2} $$

a) Schreibe eine OpenMP-Version, die **beide Phasen** (Norm berechnen, dann dividieren) in
   **einer einzigen** `parallel`-Region unterbringt — es soll also nur einmal geforkt werden.
   Verwende `default(none)` und gib jede Variable explizit an.
b) Zwischen den Phasen muss synchronisiert werden. Begründe: An welcher Stelle genau, und was
   passiert konkret ohne diese Synchronisation?
c) Warum darf man an die erste Schleife **kein** `nowait` schreiben?
d) Die Division durch die Norm soll nur einmal berechnet werden (`inv = 1.0 / norm`). In
   welchem Konstrukt platzierst du diese Zeile, und mit welchem Scoping?
e) Teste mit `n = 10⁷` und 1, 2, 4, 8 Threads. Warum ist der Speedup dieser Aufgabe deutlich
   schlechter als beim Matrix-Vektor-Produkt? (Stichwort: Verhältnis von Rechenoperationen zu
   geladenen Bytes.)

---

## Aufgabe 6.6 `[Code]` — Histogramm

*Zeit: ca. 25 min*

Gegeben `n` Messwerte in `double v[n]` mit Werten aus `[0, 1)`. Gesucht ist ein Histogramm mit
`B` gleich breiten Klassen: `hist[b]` zählt, wie viele Werte in Klasse `b = (int)(v[i] * B)`
fallen.

a) Warum ist `#pragma omp parallel for` über die Messwerte mit `shared(hist)` fehlerhaft?
b) Implementiere Variante **A** mit `#pragma omp atomic`. Warum genügt hier `atomic` und man
   braucht kein `critical`?
c) Implementiere Variante **B**: Jeder Thread führt ein **eigenes** lokales Histogramm und
   addiert es am Ende einmal auf das globale. Welches Konstrukt sichert diese Endphase ab?
d) Miss beide Varianten für `B = 8` und `B = 4096` bei `n = 10⁸`. Erkläre, warum der Abstand
   zwischen A und B bei kleinem `B` viel größer ist als bei großem `B`.
e) Variante B hat ihrerseits bei sehr großem `B` einen Nachteil. Welchen?
f) Seit OpenMP 4.5 gibt es `reduction(+ : hist[:B])` für Arrays. Formuliere die Lösung damit
   und ordne ein, welcher der beiden obigen Varianten das entspricht.

---

## Aufgabe 6.7 `[Analyse]` — `nowait` richtig setzen

*Zeit: ca. 15 min*

```cpp
#pragma omp parallel default(none) shared(a, b, c, d, n)
{
    #pragma omp for                       /* Schleife 1 */
    for (int i = 0; i < n; i++) a[i] = f(i);

    #pragma omp for                       /* Schleife 2 */
    for (int i = 0; i < n; i++) b[i] = a[i] * 2.0;

    #pragma omp for                       /* Schleife 3 */
    for (int i = 0; i < n; i++) c[i] = g(i);

    #pragma omp for                       /* Schleife 4 */
    for (int i = 0; i < n; i++) d[i] = a[n-1-i] + c[i];
}
```

a) An welche der vier Schleifen darf `nowait` geschrieben werden, ohne die Korrektheit zu
   gefährden? Begründe jede Entscheidung einzeln.
b) Schleife 2 liest `a[i]` — dieselbe Indexposition, die Schleife 1 geschrieben hat. Ist
   `nowait` an Schleife 1 damit unter `schedule(static)` sicher? Begründe.
c) Wie viele implizite Barrieren enthält der Code ursprünglich insgesamt (die Region
   mitgezählt)? Wie viele bleiben nach deiner Optimierung?
d) Warum lässt sich die Barriere am Ende der `parallel`-Region nicht entfernen?

---

## Aufgabe 6.8 `[Code]` `[Klausur]` — Laufendes Maximum

*Zeit: ca. 25 min*

```cpp
m[0] = r[0];
for (i = 1; i < n; i++)
    m[i] = (m[i-1] > r[i]) ? m[i-1] : r[i];     /* laufendes Maximum */
```

a) Warum liefert `#pragma omp parallel for` über diese Schleife falsche Ergebnisse? Benenne
   den Fachbegriff und gib ein konkretes Zahlenbeispiel mit `n = 4` und zwei Threads an, bei
   dem das Ergebnis nachweislich falsch ist.
b) Formuliere `m[i]` in geschlossener Form (ohne Rekursion) als Funktion der `r`-Werte.
c) Angenommen, jeder Wert `r[i]` müsste selbst erst teuer berechnet werden (Kosten `O(k)` pro
   Eintrag, `k ≫ 1`). Entwirf eine zweiphasige Lösung analog zu Fragment c) aus Abschnitt 12
   der Theorie und implementiere sie.
d) Gib Arbeit und seriellen Anteil deiner Lösung an. Wie groß ist der maximal erreichbare
   Speedup nach Amdahl für `n = 10⁶`, `k = 100` und `p → ∞`?
e) Skizziere zusätzlich eine **vollständig parallele** Variante der Phase 2 (Teilmaxima pro
   Thread, dann Präfix-Maximum über die Teilergebnisse). Wie viele Durchläufe über die Daten
   braucht sie, und ab wann lohnt sie sich?

---

## Aufgabe 6.9 `[Code]` — Messreihe und Auswertung

*Zeit: ca. 30 min*

Nimm das Matrix-Vektor-Produkt aus Abschnitt 12a der Theorie (`code/matvec.c`).

a) Miss die Laufzeit für `m = n = 4096` mit `p = 1, 2, 4, 8` Threads. Verwende
   `omp_get_wtime()`, wiederhole jede Messung fünfmal und nimm das Minimum. Begründe, warum
   Minimum statt Mittelwert.
b) Berechne Speedup `S(p) = T(1)/T(p)` und Effizienz `E(p) = S(p)/p`. Trage beides in eine
   Tabelle ein.
c) `T(1)` ist hier die Laufzeit der **parallelen** Version mit einem Thread. Warum ist das
   nicht dasselbe wie die Laufzeit der seriellen Version, und welche der beiden gehört
   eigentlich in den Nenner des Speedups?
d) Wiederhole die Messung mit `m = 64`, `n = 4096` (gleiche Gesamtarbeit pro Zeile, aber nur
   64 Zeilen). Erkläre das veränderte Skalierungsverhalten.
e) Ab welcher Problemgröße lohnt sich die Parallelisierung auf deiner Maschine überhaupt?
   Bestimme die Grenze experimentell und formuliere daraus eine passende `if`-Klausel.
