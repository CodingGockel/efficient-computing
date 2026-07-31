# Kapitel 07 — Übungen: OpenMP Tasks, Abhängigkeiten und kritischer Pfad

Erst selbst bearbeiten, dann mit [`loesungen.md`](loesungen.md) vergleichen. Für die
Code-Aufgaben liegt ein Makefile in [`code/`](code/):

```bash
cd code && make && ./bin/<programm>
```

Tags: `[Rechnen]` `[Code]` `[Analyse]` `[Klausur]`

---

## Aufgabe 7.1 `[Klausur]` — Task-Scoping vorhersagen

*Zeit: ca. 10 min*

```cpp
#include <cstdio>
#include <omp.h>

static int s = 1;
int g = 2;

int main() {
    int p = 3;
    int q = 4;
    #pragma omp parallel firstprivate(q) num_threads(2)
    {
        int r = 5;
        #pragma omp task
        {
            int t = 6;
            /* hier werden s, g, p, q, r und t benutzt */
        }
    }
    return 0;
}
```

a) Gib für `s`, `g`, `p`, `q`, `r`, `t` jeweils an, welches Datenattribut sie **in der
   `parallel`-Region** und welches sie **in der `task`** haben. Begründe jede Zeile mit der
   zutreffenden Regel.
b) Welche der sechs Variablen könnte die Task nach ihrem Erzeugungszeitpunkt noch verändert
   sehen, wenn der Erzeuger sie ändert? Warum ist das bei den anderen nicht so?
c) Nenne die eine Regel, die Task-Scoping vom Thread-Scoping aus Kapitel 06 unterscheidet,
   und erkläre, welches Problem sie löst.
d) Was ändert sich, wenn man `#pragma omp task shared(r)` schreibt? Nenne eine Situation, in
   der das ein Fehler wäre.

---

## Aufgabe 7.2 `[Analyse]` — Warum `single`?

*Zeit: ca. 15 min*

```cpp
#pragma omp parallel num_threads(4)
{
    for (int i = 0; i < 8; i++) {
        #pragma omp task
        arbeite(i);
    }
}
```

a) Wie oft wird `arbeite()` insgesamt aufgerufen? Begründe.
b) Repariere den Code mit `single`. Wie viele Tasks entstehen jetzt, und was machen die
   übrigen drei Threads währenddessen?
c) Jemand schlägt stattdessen `#pragma omp for` statt `single` vor — also jeder Thread
   erzeugt zwei der acht Tasks. Ist das korrekt? Nenne einen Vorteil und einen Nachteil
   gegenüber `single`.
d) In der Vorlesung steht oft `#pragma omp single nowait`. Was genau spart das ein, und warum
   bleibt der Code trotzdem korrekt?
e) Was passiert, wenn man das umgebende `#pragma omp parallel` weglässt und nur
   `#pragma omp task` schreibt? Ist das ein Übersetzungsfehler?

---

## Aufgabe 7.3 `[Rechnen]` `[Klausur]` — Kritischer Pfad

*Zeit: ca. 30 min*

Ein Programm ist in acht Tasks zerlegt:

| Task | A | B | C | D | E | F | G | H |
|---|---|---|---|---|---|---|---|---|
| Gewicht | 2 | 5 | 3 | 4 | 2 | 6 | 3 | 2 |
| Vorgänger | — | A | A | A | B, C | C, D | E | F, G |

a) Zeichne den Task-Abhängigkeitsgraphen mit Gewichten.
b) Führe Vorwärts- und Rückwärtslauf durch und gib ES, EF, LS, LF und Slack in einer Tabelle an.
c) Bestimme alle kritischen Pfade. (Achtung: Es ist mehr als einer.)
d) Berechne Arbeit `T₁`, Tiefe `T∞` und Parallelität `T₁/T∞`.
e) Wie viele Prozessoren braucht man **mindestens**, um `T∞` tatsächlich zu erreichen?
   Begründe mit einem Zeitdiagramm und gib einen konkreten Zeitplan an.
f) Berechne Speedup und Effizienz für diese Prozessorzahl.
g) Gib die untere Schranke `max(T∞, T₁/p)` für `p = 2` an. Zeige, dass diese Schranke hier
   **nicht erreichbar** ist, und begründe warum.

---

## Aufgabe 7.4 `[Rechnen]` `[Analyse]` — Bernstein-Bedingung

*Zeit: ca. 25 min*

Gegeben ist folgendes Codefragment (Anweisungen in Programmreihenfolge, Gewichte in Klammern):

```
S1 (2):  t = a + b
S2 (3):  u = t * c
S3 (4):  v = d - e
S4 (1):  z = t / 2
S5 (2):  t = u + v
```

a) Stelle für jede Anweisung die Mengen `Iᵢ` (gelesen) und `Oᵢ` (geschrieben) auf.
b) Prüfe alle 10 Paare auf die drei Bernstein-Bedingungen. Gib für jedes verletzte Paar an,
   **welche** Bedingung verletzt ist und ob es sich um RAW, WAR oder WAW handelt.
c) Zeichne den resultierenden Abhängigkeitsgraphen. Welche Anweisungen können echt parallel
   laufen?
d) Berechne `T₁`, `T∞` und die minimal nötige Prozessorzahl für `T∞`.
e) Schreibe das Fragment als OpenMP-Tasks mit `depend`-Klauseln.
f) Benenne die Zielvariable von S5 in `t2` um (und passe nachfolgende Verwendungen an).
   Welche Kanten verschwinden? Ändert sich der kritische Pfad? Erkläre, was daraus allgemein
   über unechte Abhängigkeiten folgt.

---

## Aufgabe 7.5 `[Code]` — Fibonacci und das Granularitätsproblem

*Zeit: ca. 25 min*

a) Implementiere `fib(n)` in drei Varianten: seriell rekursiv, mit Tasks ohne Cutoff, und mit
   Tasks plus Cutoff nach Rekursionstiefe.
b) Miss alle drei für `n = 32` mit 8 Threads (bei größerem `n` wird die Variante ohne Cutoff
   unangenehm langsam). Sie wird **deutlich langsamer** sein als die serielle. Um welchen
   Faktor? Erkläre die Ursache quantitativ: Wie viele Tasks entstehen bei `fib(32)` ungefähr,
   und wie viel Arbeit steckt in einer?
c) Variiere die Cutoff-Tiefe von 0 bis 20. Zeichne (oder tabelliere) die Laufzeit über der
   Tiefe. Bei welcher Tiefe liegt das Optimum, und warum steigt die Laufzeit auf **beiden**
   Seiten des Optimums?
d) Formuliere eine Faustregel, wie man den Cutoff in Abhängigkeit von der Threadzahl wählt.
e) Setze den Cutoff stattdessen mit `final(n < k)` um. Warum genügt hier `final` und nicht
   `if(n >= k)`?

---

## Aufgabe 7.6 `[Code]` `[Analyse]` — Unbalancierter Baum

*Zeit: ca. 25 min*

Gegeben ein Binärbaum, der bewusst **unbalanciert** aufgebaut ist: Der linke Teilbaum jedes
Knotens ist deutlich tiefer als der rechte. In jedem Knoten steckt gleich viel Rechenarbeit.

a) Warum lässt sich dieser Baum **nicht** sinnvoll mit `#pragma omp parallel for`
   traversieren? Nenne zwei unabhängige Gründe.
b) Implementiere die Traversierung mit Tasks. Wo genau setzt du die `task`-Direktiven, und
   warum braucht `process(p)` keine eigene?
c) Miss Laufzeit und Speedup für 1, 2, 4, 8 Threads.
d) Eine Alternative: Man sammelt zuerst alle Knotenzeiger in ein Array und macht dann ein
   `parallel for` darüber. Vergleiche beide Ansätze hinsichtlich Speicherbedarf, Lastausgleich
   und Anwendbarkeit. Wann ist die Array-Variante die bessere Wahl?
e) Ergänze die Task-Variante um einen Cutoff nach Teilbaumgröße.
f) Wiederhole die Messungen für **drei verschiedene Arbeitsmengen pro Knoten** (z. B. 2000,
   100 und 20 Schleifendurchläufe). Bei welcher Arbeitsmenge bringt der Cutoff nichts, bei
   welcher entscheidet er über Erfolg oder Misserfolg? Erkläre die Grenze quantitativ mit den
   ca. 300 ns Task-Overhead aus Aufgabe 7.5.
g) Beobachte in derselben Messreihe, wie sich der Anteil der seriellen Sammelphase bei der
   Array-Variante verändert. Was sagt das über deren Anwendbarkeit aus?

---

## Aufgabe 7.7 `[Analyse]` `[Klausur]` — `depend` vorhersagen

*Zeit: ca. 20 min*

```cpp
int a, b, c;
#pragma omp parallel num_threads(4)
#pragma omp single
{
    #pragma omp task depend(out : a)                    /* T1 */
    { a = 2; }

    #pragma omp task depend(out : b)                    /* T2 */
    { sleep(1); b = 3; }

    #pragma omp task depend(in : a) depend(out : c)     /* T3 */
    { c = a * 10; }

    #pragma omp task depend(in : b) depend(inout : a)   /* T4 */
    { a = a + b; }

    #pragma omp task depend(in : a, c)                  /* T5 */
    { std::printf("%d %d\n", a, c); }
}
```

a) Bestimme alle Kanten des Abhängigkeitsgraphen. Gib für jede Kante an, ob RAW, WAR oder WAW.
b) Zeichne den Graphen.
c) Welche Ausgabe erzeugt das Programm?
d) Wie lange läuft es ungefähr bei 4 Threads? Welche Tasks laufen dabei gleichzeitig?
e) Was passiert, wenn man in T4 `depend(inout : a)` zu `depend(in : a)` ändert? Ist das
   Programm dann noch korrekt? Begründe präzise.
f) Wie müsste man T2 und T4 ändern, damit T5 nicht mehr auf das `sleep(1)` warten muss —
   ohne die Semantik von `a = a + b` aufzugeben?

---

## Aufgabe 7.8 `[Rechnen]` — Arbeit und Tiefe von Mergesort

*Zeit: ca. 20 min*

Mergesort auf `n` Elementen: Die beiden Hälften werden rekursiv sortiert, danach wird
zusammengemischt. Das Mischen kostet `Θ(n)`.

a) Stelle die Rekursionsgleichung für die **Arbeit** `T₁(n)` auf und löse sie.
b) Die beiden rekursiven Aufrufe werden als Tasks parallel ausgeführt, das Mischen bleibt
   seriell. Stelle die Rekursionsgleichung für die **Tiefe** `T∞(n)` auf und löse sie.
c) Berechne die Parallelität `T₁/T∞`. Werte sie für `n = 2²⁰` konkret aus.
d) Interpretiere das Ergebnis: Lohnt sich diese Implementierung auf einer Maschine mit 8
   Kernen? Und auf einer mit 256 Kernen?
e) Nun werde auch das Mischen parallelisiert, sodass es Tiefe `Θ(log n)` hat. Wie ändern sich
   `T∞` und die Parallelität? Werte erneut für `n = 2²⁰` aus.
f) Gib für Teil b) und Teil e) jeweils die Brent-Schranke `T(p) ≤ T₁/p + T∞` für `p = 8` an
   und vergleiche.

---

## Aufgabe 7.9 `[Code]` — `taskwait` vs. `taskgroup`

*Zeit: ca. 20 min*

a) Schreibe ein kleines Programm, in dem eine Task zwei Kinder erzeugt und eines dieser Kinder
   selbst noch ein Enkelkind erzeugt. Baue Ausgaben so ein, dass man die
   Fertigstellungsreihenfolge sieht.
b) Zeige experimentell, dass `taskwait` **nicht** auf das Enkelkind wartet, `taskgroup`
   dagegen schon.
c) Warum funktioniert Fibonacci aus Abschnitt 4.2 der Theorie trotzdem korrekt mit bloßem
   `taskwait`, obwohl dort beliebig tiefe Rekursion vorkommt?
d) Implementiere Quicksort mit Tasks (Partition seriell, beide Hälften als Tasks) mit einem
   Cutoff für kleine Teilarrays. Prüfe die Korrektheit gegen `qsort()` aus der Standardbibliothek.
e) Braucht dein Quicksort `taskwait`, `taskgroup` oder gar keine Synchronisation an den
   rekursiven Aufrufen? Begründe. Was ändert sich, wenn der Aufrufer direkt nach der Sortierung
   auf das Array zugreifen will?
