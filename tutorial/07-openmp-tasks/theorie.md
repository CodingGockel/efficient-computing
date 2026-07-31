# Kapitel 07 — OpenMP Tasks, Abhängigkeiten und kritischer Pfad

> **Quellen:** `vl/9-Lecture-Openmp_task.pdf` (Folien 1–23), Übungsblatt
> `excercises/uebung8.pdf` (Aufgabe 8.1 — 8.2 und 8.3 gehören zu Kapitel 10)
> **Zeitbedarf:** ca. 60–75 min lesen + 100 min Aufgaben
> **Voraussetzungen:** Kapitel 06 (Fork/Join, Scoping, `single`, Barrieren)

> **Codekonvention:** Aller Code hier ist C++17 (`g++ -std=c++17 -O2 -fopenmp`). Die
> parallelisierten Rechenschleifen sind bewusst roh gehalten — so stehen sie auch in der
> Klausur. Blöcke, die direkt aus den Folien stammen, sind mit `// Folie N` markiert und
> behalten den C-Stil der Vorlesung. Sprachmittel siehe
> [C++-Werkzeugkasten](../cpp-werkzeugkasten.md).

---

## 1. Warum Tasks?

`#pragma omp for` aus Kapitel 06 hat eine harte Voraussetzung: Die Zahl der Iterationen muss
**beim Betreten der Schleife bekannt** sein. Genau daran scheitert eine ganze Klasse von
Problemen:

```cpp
/* 1. Verkettete Liste — Länge unbekannt, kein Index */
while (p != NULL) { process(p); p = p->next; }

/* 2. Rekursion auf einem Baum — Struktur erst zur Laufzeit bekannt */
void traverse(node *p) { traverse(p->left); traverse(p->right); process(p); }

/* 3. Divide & Conquer — Quicksort, Mergesort, Strassen */
void qsort_rec(int *a, int lo, int hi) { ... qsort_rec(a, lo, m); qsort_rec(a, m+1, hi); }

/* 4. Stark unterschiedlich teure Arbeitspakete ohne bekannte Kosten */
while (!queue_empty()) { job = pop(); handle(job); }
```

Keiner dieser vier Fälle lässt sich in eine kanonische `for`-Schleife pressen. Man könnte die
Liste zuerst in ein Array umkopieren — aber bei einem Baum mit unbekannter Tiefe oder bei
Rekursion, die während der Ausführung neue Arbeit erzeugt, hilft auch das nicht.

**Die Antwort von OpenMP heißt `task`:** Ein Thread erzeugt ein Arbeitspaket und wirft es in
einen Pool; irgendein Thread des Teams holt es sich später und führt es aus. Wann und von wem,
entscheidet die Laufzeit.

> **Der Perspektivwechsel:** Bei `for` wird ein *bekannter Iterationsraum* auf Threads
> aufgeteilt. Bei `task` wird *dynamisch erzeugte Arbeit* eingesammelt und verteilt. Das erste
> ist statische Zerlegung, das zweite ist Work-Stealing.

---

## 2. Die Kernidee in drei Sätzen

1. `#pragma omp task` **erzeugt** ein Arbeitspaket, führt es aber nicht sofort aus — die
   Ausführung ist *deferred* und passiert irgendwann, irgendwo im Team.
2. Deshalb braucht man das Idiom **`parallel` + `single`**: Das Team wird einmal aufgebaut,
   **ein** Thread erzeugt die Tasks, **alle** arbeiten sie ab.
3. Synchronisiert wird mit `taskwait` (warte auf eigene Kinder), `taskgroup` (warte auf alle
   Nachkommen) oder `depend` (warte auf konkrete Daten).

---

## 3. Das Task-Konstrukt

### 3.1 Erzeugen ≠ Ausführen

```cpp
#pragma omp task
{
    /* Arbeitspaket */
}
```

Trifft ein Thread auf diese Direktive, passiert Folgendes:

1. Er packt den Codeblock zusammen mit einer **Kopie der benötigten Daten** (dazu Abschnitt 5)
   in eine Task-Struktur.
2. Er hängt die Task in die Task-Queue des Teams.
3. **Er läuft sofort weiter** — er hat die Task *nicht* ausgeführt.

Ausgeführt wird die Task später an einem *task scheduling point* von irgendeinem Thread des
Teams. Solche Punkte sind unter anderem: die Erzeugung einer Task, `taskwait`, `taskgroup`-Ende,
jede Barriere und das Ende einer Task.

**Konsequenz, die man verinnerlichen muss:** Die Ausführungsreihenfolge ist **nicht garantiert**.
Auch nicht, dass Task 1 vor Task 2 läuft, nur weil sie vorher erzeugt wurde.

### 3.2 Das `parallel`+`single`-Idiom

```cpp
#pragma omp parallel                 /* 1. Team aufbauen: p Threads */
{
    #pragma omp single               /* 2. NUR EINER erzeugt die Tasks */
    {
        #pragma omp task
        arbeit_a();
        #pragma omp task
        arbeit_b();
    }                                /* implizite Barriere von single */
}                                    /* implizite Barriere: alle Tasks fertig */
```

**Warum `single`?** Ohne `single` würde **jeder** der `p` Threads den Erzeugungscode ausführen —
es entstünden `p`-mal so viele Tasks wie gewollt, und die Arbeit würde `p`-fach erledigt. Das
ist der mit Abstand häufigste Anfängerfehler bei Tasks.

Die anderen `p-1` Threads laufen sofort zur impliziten Barriere des `single` — und **arbeiten
dort die Tasks ab**, statt untätig zu warten. Genau deshalb funktioniert das Muster.

> Häufig sieht man `#pragma omp single nowait`. Das ist erlaubt: Der erzeugende Thread muss am
> Ende von `single` nicht warten, sondern kann sofort mithelfen. Die Barriere am Ende der
> `parallel`-Region garantiert ohnehin, dass alle Tasks fertig sind, bevor es seriell weitergeht.

Kompaktschreibweise, wenn die Region nichts anderes tut:

```cpp
#pragma omp parallel
#pragma omp single
traverse(root);
```

### 3.3 Die Klauseln des Task-Konstrukts (Folie 4)

| Klausel | Bedeutung |
|---|---|
| `if(cond)` | Ist `cond` falsch, wird die Task **sofort und synchron** ausgeführt (*undeferred*) — der erzeugende Thread arbeitet sie selbst ab |
| `final(cond)` | Ist `cond` wahr, werden diese Task **und alle in ihr erzeugten Tasks** sofort ausgeführt — schneidet den ganzen Rekursionsteilbaum ab |
| `mergeable` | Erlaubt der Laufzeit, für eine undeferred/final Task keinen eigenen Datenkontext anzulegen |
| `depend(typ : liste)` | Datenabhängigkeit zu Geschwister-Tasks (Abschnitt 7) |
| `priority(wert)` | Hinweis: höherer Wert → möglichst frühere Ausführung. **Nur ein Hinweis**, keine Garantie |
| `untied` | Die Task darf nach einer Unterbrechung von einem *anderen* Thread fortgesetzt werden |

`if` und `final` sehen ähnlich aus, sind es aber nicht: `if(false)` führt **nur diese eine**
Task sofort aus, ihre Kinder werden wieder normal erzeugt. `final(true)` macht auch alle
Nachkommen final — es beendet die Task-Erzeugung ab hier vollständig. Für Rekursions-Cutoffs
ist `final` deshalb das schärfere Werkzeug (Abschnitt 6).

---

## 4. Zwei Standardbeispiele

### 4.1 Baumtraversierung (Folie 5)

```cpp
// Folie 5 — Original der Vorlesung (C-Stil)
struct node { struct node *left, *right; };
extern void process(struct node *);

void traverse(struct node *p)
{
    if (p->left)
        #pragma omp task            /* p ist per Default firstprivate */
        traverse(p->left);
    if (p->right)
        #pragma omp task
        traverse(p->right);
    process(p);
}
```

Aufgerufen wird das mit dem Idiom aus 3.2:

```cpp
#pragma omp parallel
#pragma omp single
traverse(root);
```

Der entscheidende Punkt: `p` ist **firstprivate** (Abschnitt 5) — jede Task bekommt ihre eigene
Kopie des Zeigers und damit ihren eigenen Knoten. Ohne diese Voreinstellung wäre der Code
kaputt, weil `p` sich beim Erzeuger längst weitergedreht hätte, bevor die Task läuft.

Jede Task erzeugt selbst wieder Tasks — der Task-Baum wächst mit dem Datenbaum. Das ist der
Grund, warum Tasks für **unbalancierte** Strukturen so gut passen: Wo der Baum tief ist,
entstehen viele Tasks, wo er flach ist, wenige. Die Lastverteilung ergibt sich von selbst.

### 4.2 Fibonacci (Folie 8)

```cpp
// Folie 8 — Original der Vorlesung (C-Stil)
int fib(int n)
{
    int i, j;
    if (n < 2) return n;

    #pragma omp task shared(i)
    i = fib(n - 1);
    #pragma omp task shared(j)
    j = fib(n - 2);
    #pragma omp taskwait            /* ohne das: Unsinn */
    return i + j;
}
```

Hier stecken drei Lehren:

- **`shared(i)` ist Pflicht.** `i` und `j` sind lokale Variablen und wären per Default
  *firstprivate* — die Task würde in ihre eigene Kopie schreiben, und der Erzeuger sähe das
  Ergebnis nie. `shared` macht die Variable auf dem Stack des Erzeugers sichtbar.
- **`taskwait` ist Pflicht.** Es wartet auf die **direkten Kinder**. Ohne es würde `return i+j`
  Werte lesen, die noch niemand geschrieben hat — und schlimmer: Der Stackframe von `fib`
  verschwände, während die Kinder noch darauf schreiben. Das ist ein Absturz, der nicht immer
  passiert.
- **So ist es viel zu langsam.** Für jede einzelne Addition wird eine Task erzeugt; die
  Verwaltung kostet ein Vielfaches der Arbeit. Das führt direkt zu Abschnitt 6.

---

## 5. Scoping für Tasks — die Sonderregeln

Für Threads galt (Kapitel 06): außen deklariert → shared. **Für Tasks gilt das nicht.**

Der Grund ist die zeitliche Entkopplung: Eine Task läuft eventuell erst, wenn der Erzeuger seine
Schleife längst weitergedreht hat oder die Funktion verlassen hat. Würde die Task auf die
Originalvariable zugreifen, läse sie einen falschen — oder gar keinen — Wert mehr. Deshalb
bekommt sie standardmäßig eine **Kopie zum Erzeugungszeitpunkt**.

### 5.1 Die vier Regeln (Folie 7)

1. Lokale Variablen sind **private**.
2. `static`- und globale Variablen sind **shared**.
3. **Alles, was die Task von außen benutzt, ist `firstprivate`** — es sei denn, es war im
   umgebenden Kontext bereits `shared`.
4. **Nur das Attribut `shared` wird vererbt.** War eine Variable in der umgebenden Region
   `private`, wird sie in der Task `firstprivate` (nicht `private`, nicht `shared`).
5. Variablen in *orphaned tasks* (Tasks in einer Funktion ohne umgebende `parallel`-Region im
   selben Gültigkeitsbereich) sind **firstprivate**.

### 5.2 Durchgerechnet: das Beispiel von Folie 6

```cpp
// Folie 6 — Original der Vorlesung (C-Stil)
static int a = 1;

int main() {
    int b = 3;
    int c = 3;
    #pragma omp parallel private(c)
    {
        int d = 4;
        #pragma omp task
        {
            int e = 5;
        }
    }
}
```

| Variable | in der `parallel`-Region | in der `task` | Begründung |
|---|---|---|---|
| `a` | **shared** | **shared** | `static` → shared; das Attribut `shared` wird vererbt |
| `b` | **shared** | **shared** | außerhalb deklariert → shared; `shared` wird vererbt |
| `c` | **private** | **firstprivate** | `private`-Klausel; `private` wird **nicht** vererbt → Regel 3 |
| `d` | **private** | **firstprivate** | im Block deklariert → private je Thread; in der Task Kopie davon |
| `e` | — | **private** | in der Task deklariert |

Die Zeile für `c` ist die Prüfungsfrage: `private` in der Region wird in der Task zu
`firstprivate`, **nicht** zu `private`.

### 5.3 Die praktische Konsequenz

```cpp
for (int i = 0; i < n; i++) {
    #pragma omp task              /* i ist firstprivate — Kopie des AKTUELLEN Werts */
    std::printf("%d\n", i);            /* korrekt: jede Task sieht ihr eigenes i */
}
```

Genau das rettet die Schleife. Wäre `i` shared, führen alle Tasks mit demselben (längst
hochgezählten oder ungültigen) Wert. In pthreads muss man diese Kopie von Hand anlegen — bei
OpenMP-Tasks ist sie die Voreinstellung.

Umgekehrt gilt: **Ergebnisse muss man explizit `shared` machen** (wie `i` und `j` bei
Fibonacci) — und dann selbst für Synchronisation sorgen.

---

## 6. Granularität: das eigentliche Performance-Problem

Eine Task kostet Verwaltung — Größenordnung **einige hundert Nanosekunden** für Erzeugung,
Einreihen, Datenkontext und Scheduling. Ist das Arbeitspaket kleiner als das, verliert man.

`fib(40)` naiv erzeugt über 300 Millionen Tasks für jeweils **eine Addition**. Das ist
typischerweise um mehrere Größenordnungen langsamer als die serielle Version.

### 6.1 Cutoff nach Rekursionstiefe

```cpp
int fib(int n, int depth)
{
    if (n < 2) return n;
    if (depth <= 0) return fib_serial(n);      /* ab hier seriell */

    int i, j;
    #pragma omp task shared(i)
    i = fib(n - 1, depth - 1);
    #pragma omp task shared(j)
    j = fib(n - 2, depth - 1);
    #pragma omp taskwait
    return i + j;
}
```

Faustregel für die Tiefe: so tief, dass **einige Tasks pro Thread** entstehen — genug für
Lastausgleich, nicht mehr. Bei 8 Threads sind `depth = 4…6` (16–64 Tasks) meist ein guter Wert.

### 6.2 Cutoff nach Problemgröße — mit `final`

```cpp
#pragma omp task final(n < 20) shared(i)
i = fib(n - 1);
```

`final(true)` schaltet nicht nur diese Task auf sofortige Ausführung, sondern **alle Tasks in
ihrem Teilbaum**: Sie werden *included tasks* — undeferred und vom antreffenden Thread direkt
abgearbeitet. Das spart Einreihen, Scheduling und Datenkontext.

**Aber Achtung — das allein genügt nicht.** Die `task`-Konstrukte werden weiterhin
*angetroffen*, und jedes Antreffen kostet auch als included task noch etwas. Bei `fib(32)`
sind das immer noch rund 7 Millionen Antreffpunkte:

| Variante (`fib(32)`, 8 Threads) | gemessen |
|---|---|
| seriell | 0,0031 s |
| nur `final(n < 22)` | 0,0335 s — **10× langsamer als seriell** |
| `final` + `omp_in_final()` | 0,0018 s — **1,7× schneller als seriell** |

Der Trick ist, im finalen Teilbaum gar keine Task-Konstrukte mehr zu durchlaufen. Dafür gibt es
`omp_in_final()`:

```cpp
long fib(int n, int k) {
    if (n < 2) return n;
    if (omp_in_final()) return fib_serial(n);   /* ab hier KEINE Konstrukte mehr */
    long i, j;
    #pragma omp task shared(i) final(n < k)
    i = fib(n - 1, k);
    #pragma omp task shared(j) final(n < k)
    j = fib(n - 2, k);
    #pragma omp taskwait
    return i + j;
}
```

`final` schaltet also die *Verzögerung* ab, `omp_in_final()` schaltet die *Erzeugung* ab. Man
braucht beides — oder gleich einen expliziten Tiefenzähler wie in 6.1, der dasselbe leistet
und leichter zu lesen ist.

### 6.3 Der Unterschied `if` / `final` in einem Satz

| Klausel | Wirkung auf diese Task | Wirkung auf ihre Nachkommen |
|---|---|---|
| `if(false)` | sofort, synchron ausgeführt | **keine** — Kinder werden normal als (verzögerte) Tasks erzeugt |
| `final(true)` | sofort, synchron ausgeführt | **alle** Nachkommen ebenfalls sofort — aber die Konstrukte werden weiterhin angetroffen |

---

## 7. Synchronisation: `taskwait`, `taskgroup`, `depend`

### 7.1 `taskwait` vs. `taskgroup`

```cpp
#pragma omp task
{
    #pragma omp task
    kind();                    /* Kind */
    #pragma omp task
    { #pragma omp task enkel(); }   /* Kind, das ein Enkelkind erzeugt */

    #pragma omp taskwait       /* wartet NUR auf die beiden Kinder */
}
```

- **`taskwait`** wartet auf die **direkten Kinder** der aktuellen Task — nicht auf Enkel.
- **`taskgroup`** wartet auf **alle Nachkommen** der Tasks, die im Block erzeugt wurden.

```cpp
#pragma omp taskgroup
{
    #pragma omp task
    traverse(root);            /* erzeugt beliebig tiefe Task-Hierarchien */
}                              /* hier ist der GESAMTE Teilbaum fertig */
```

Bei rekursiven Algorithmen, in denen Kinder wiederum Tasks erzeugen, ist `taskgroup` fast immer
das, was man eigentlich meint. Bei Fibonacci genügt `taskwait`, weil die Kinder ihre eigenen
`taskwait` haben — die Garantie pflanzt sich rekursiv fort.

Auch jede **Barriere** (explizit oder implizit am Ende von `parallel`, `for`, `single`) wartet
auf alle bis dahin erzeugten Tasks des Teams.

### 7.2 `depend` — Synchronisation über Daten

Statt global zu warten, kann man einzelne Abhängigkeiten benennen. Die Laufzeit baut daraus
selbstständig einen Abhängigkeitsgraphen zwischen **Geschwister-Tasks**:

| Typ | Bedeutung |
|---|---|
| `depend(in : x)` | Die Task **liest** `x` |
| `depend(out : x)` | Die Task **schreibt** `x` |
| `depend(inout : x)` | Die Task liest **und** schreibt `x` |

Zwischen zwei Geschwister-Tasks entsteht eine Kante, wenn ihre Listen kollidieren und
**mindestens eine** Seite schreibt:

| Reihenfolge im Programm | Konflikt | Name |
|---|---|---|
| `out` → `in` | Schreiben, dann Lesen | *true dependence* (RAW) |
| `in` → `out` | Lesen, dann Schreiben | *anti dependence* (WAR) |
| `out` → `out` | zweimal Schreiben | *output dependence* (WAW) |
| `in` → `in` | beide lesen nur | **keine** Abhängigkeit |

Die Reihenfolge ist die **Erzeugungsreihenfolge** der Tasks, nicht die Ausführungsreihenfolge.

### 7.3 Durchgerechnet: das Beispiel von Folie 10

```cpp
int x, y;
#pragma omp parallel num_threads(8)
#pragma omp single nowait
{
    #pragma omp task depend(out : x)              /* T1 */
    { x = 1; }

    #pragma omp task depend(in : x) depend(out : y)  /* T2 */
    { sleep(2); y = x + 1; }

    #pragma omp task depend(inout : x)            /* T3 */
    { x++; std::printf("task3(x): %d\n", x); }

    #pragma omp task depend(in : x, y)            /* T4 */
    { std::printf("task4 (x+y): %d\n", x + y); }
}
```

Kanten Schritt für Schritt:

| Paar | Warum |
|---|---|
| T1 → T2 | T1 schreibt `x` (`out`), T2 liest `x` (`in`) — RAW |
| T1 → T3 | T1 schreibt `x`, T3 schreibt `x` (`inout`) — WAW |
| **T2 → T3** | T2 liest `x`, T3 schreibt `x` — **WAR**, die überraschende Kante |
| T2 → T4 | T2 schreibt `y`, T4 liest `y` — RAW |
| T3 → T4 | T3 schreibt `x`, T4 liest `x` — RAW |

```
         T1 (out: x)
        ╱          ╲
   T2 (in x,        ╲  (WAW)
       out y)        ╲
        │  ╲          ╲
        │   ╲──(WAR)── T3 (inout: x)
        │                  │
        └──────► T4 (in: x, y) ◄──┘
```

**Ergebnis:** `x = 1` (T1), `y = 2` (T2), dann `x = 2` (T3) → Ausgabe

```
task3(x): 2
task4 (x+y): 4
```

**und beides erst nach etwa 2 Sekunden.** Das ist der überraschende Teil: T3 liest und schreibt
nur `x` und hat inhaltlich mit dem `sleep(2)` nichts zu tun — muss aber trotzdem warten, weil
die Anti-Abhängigkeit (T2 liest `x`, T3 überschreibt es) die Reihenfolge erzwingt. Anti- und
Output-Abhängigkeiten sind **echte Ausführungsbeschränkungen**, obwohl sie keinen Datenfluss
darstellen.

---

## 8. Die Bernstein-Bedingung

Woher weiß man überhaupt, welche Codeteile unabhängig sind? Die formale Antwort liefert die
Bernstein-Bedingung (Folie 13).

Seien `T₁` und `T₂` zwei Tasks. `I₁, I₂` sind die Mengen der **gelesenen** (Input-)Variablen,
`O₁, O₂` die Mengen der **geschriebenen** (Output-)Variablen. Dann dürfen `T₁` und `T₂`
parallel ausgeführt werden, genau dann wenn **alle drei** Bedingungen gelten:

$$ I_1 \cap O_2 = \emptyset \qquad (C_1) $$
$$ I_2 \cap O_1 = \emptyset \qquad (C_2) $$
$$ O_1 \cap O_2 = \emptyset \qquad (C_3) $$

Übersetzt:

| Bedingung | Verletzt heißt | Klassischer Name | OpenMP-Entsprechung |
|---|---|---|---|
| `I₂ ∩ O₁ = ∅` | `T₂` liest, was `T₁` schreibt | **true / flow dependence** (RAW) | `out` → `in` |
| `I₁ ∩ O₂ = ∅` | `T₁` liest, was `T₂` schreibt | **anti dependence** (WAR) | `in` → `out` |
| `O₁ ∩ O₂ = ∅` | beide schreiben dieselbe Variable | **output dependence** (WAW) | `out` → `out` |

Beachte: Gemeinsames **Lesen** (`I₁ ∩ I₂ ≠ ∅`) taucht nicht auf — es ist immer unbedenklich.
Das ist exakt dieselbe Aussage wie in Kapitel 06: „Nur Lesen ist immer sicher."

### 8.1 Durchgerechnet: das Beispiel von Folie 14

| Task | Anweisung | `Iᵢ` | `Oᵢ` |
|---|---|---|---|
| T1 | `c = d * e` | {d, e} | {c} |
| T2 | `m = g + c` | {g, c} | {m} |
| T3 | `a = b + c` | {b, c} | {a} |
| T4 | `c = l + m` | {l, m} | {c} |
| T5 | `f = g % e` | {g, e} | {f} |

Paarweise prüfen (in Programmreihenfolge, `i < j`):

| Paar | verletzte Bedingung | Typ | Kante |
|---|---|---|---|
| T1, T2 | `I₂ ∩ O₁ = {c}` | RAW | T1 → T2 |
| T1, T3 | `I₃ ∩ O₁ = {c}` | RAW | T1 → T3 |
| T1, T4 | `O₁ ∩ O₄ = {c}` | WAW | T1 → T4 |
| T1, T5 | alle drei erfüllt | — | keine |
| T2, T3 | alle drei erfüllt | — | keine |
| T2, T4 | `I₄ ∩ O₂ = {m}` **und** `O₂ ∩ ...` — RAW über `m` | RAW | T2 → T4 |
| T3, T4 | `I₃ ∩ O₄ = {c}` | WAR | T3 → T4 |
| T3, T5 | alle drei erfüllt | — | keine |
| T4, T5 | alle drei erfüllt | — | keine |
| T2, T5 | alle drei erfüllt | — | keine |

Daraus der Graph:

```
        T1              T5   (völlig unabhängig)
       ╱ │ ╲
     T2  T3 │ (WAW)
      │   │ │
      ▼   ▼ ▼
        T4
```

**Ablesbar:** T2 und T3 können parallel laufen (Bernstein erfüllt), T5 kann jederzeit laufen,
T4 muss auf T1, T2 und T3 warten. Als OpenMP-Code:

```cpp
#pragma omp parallel
#pragma omp single
{
    #pragma omp task depend(out : c)            /* T1 */
    c = d * e;
    #pragma omp task depend(in : c) depend(out : m)   /* T2 */
    m = g + c;
    #pragma omp task depend(in : c) depend(out : a)   /* T3 */
    a = b + c;
    #pragma omp task depend(in : m) depend(out : c)   /* T4 */
    c = l + m;
    #pragma omp task depend(out : f)            /* T5 */
    f = g % e;
}
```

Die Laufzeit rekonstruiert daraus **genau** den Graphen oben — inklusive der WAR-Kante
T3 → T4, die man leicht übersieht.

> **Merke:** WAR und WAW sind „unechte" Abhängigkeiten — sie entstehen nur durch
> Wiederverwendung derselben Variablen, nicht durch echten Datenfluss. Man kann sie durch
> **Umbenennung** auflösen (`c2 = l + m` statt `c = l + m`). Genau das tun Compiler bei der
> Registerallokation und Prozessoren beim *register renaming* (Kapitel 02).

---

## 9. Der Task-Abhängigkeitsgraph

### 9.1 Definition (Folien 16–19)

Ein Task-Abhängigkeitsgraph ist ein gerichteter azyklischer Graph `G = (V, E, w)`:

- Jeder Knoten `v ∈ V` ist eine Task.
- Eine Kante `e = (v, u) ∈ E` bedeutet: Die Eingabe von `u` hängt von der Ausgabe von `v` ab —
  `u` darf erst starten, wenn `v` fertig ist.
- Die Gewichtsfunktion `w : V → ℝ` gibt jeder Task ihre Kosten (typischerweise Laufzeit).

Ein **Pfad** `P = (v_{i₁}, …, v_{i_l})` mit `(v_{i_j}, v_{i_{j+1}}) ∈ E` hat die Länge

$$ L(P) = \sum_{v_i \in P} w(v_i) $$

Der **kritische Pfad** ist der Pfad **maximaler Länge**:

$$ P^* = \arg\max_{P \in \mathcal{P}} L(P) $$

**Seine Länge ist die untere Schranke für die Gesamtlaufzeit** — selbst mit unendlich vielen
Prozessoren kann das Programm nicht schneller fertig werden, weil diese Tasks zwingend
nacheinander laufen müssen.

### 9.2 Die vier Zeitgrößen

| Größe | Bedeutung |
|---|---|
| **ES(v)** | *Earliest Start* — frühestmöglicher Startzeitpunkt |
| **EF(v)** | *Earliest Finish* — `ES(v) + w(v)` |
| **LS(v)** | *Latest Start* — spätester Start, ohne das Gesamtende zu verzögern |
| **LF(v)** | *Latest Finish* — `LS(v) + w(v)` |
| **Slack(v)** | Puffer — `LS(v) − ES(v) = LF(v) − EF(v)` |

**Slack = 0 ⟺ die Task liegt auf dem kritischen Pfad.**

### 9.3 Das Rechenverfahren

**Vorwärtslauf** (in topologischer Reihenfolge, von den Quellen aus):

$$ ES(v) = \max_{u \in \text{pred}(v)} EF(u), \qquad EF(v) = ES(v) + w(v) $$

Knoten ohne Vorgänger: `ES = 0`. Die Gesamtlaufzeit ist `T∞ = max_v EF(v)`.

**Rückwärtslauf** (in umgekehrter topologischer Reihenfolge, von den Senken aus):

$$ LF(v) = \min_{u \in \text{succ}(v)} LS(u), \qquad LS(v) = LF(v) - w(v) $$

Knoten ohne Nachfolger: `LF = T∞`.

> **Zwei Fehler, die fast jeder einmal macht:** Vorwärts wird **maximiert** (man muss auf den
> langsamsten Vorgänger warten), rückwärts wird **minimiert** (man darf den frühesten
> Nachfolger nicht aufhalten). Und: Vorwärts arbeitet man mit `EF`, rückwärts mit `LS`.

### 9.4 Durchgerechnet: das Beispiel von Folie 20

```
        v1(3) ──► v3(3) ──► v4(1) ──► v5(2) ──► v6(5)
                     ▲        │  ╲       ▲        ▲
        v2(4) ───────┼────────┘   ╲──────┘        │
                     └──────────────────────────────┘
```

Vorgänger: `v1: —`, `v2: —`, `v3: v1`, `v4: v2, v3`, `v5: v3, v4`, `v6: v4, v5`.

| Task | w | Vorgänger | ES | EF | LS | LF | Slack |
|---|---|---|---|---|---|---|---|
| v1 | 3 | — | 0 | 3 | 0 | 3 | **0** |
| v2 | 4 | — | 0 | 4 | 2 | 6 | 2 |
| v3 | 3 | v1 | 3 | 6 | 3 | 6 | **0** |
| v4 | 1 | v2, v3 | 6 | 7 | 6 | 7 | **0** |
| v5 | 2 | v3, v4 | 7 | 9 | 7 | 9 | **0** |
| v6 | 5 | v4, v5 | 9 | 14 | 9 | 14 | **0** |

Nachrechnen an zwei Stellen:

- `ES(v4) = max(EF(v2), EF(v3)) = max(4, 6) = 6` — obwohl v2 schon bei 4 fertig ist, muss auf v3
  gewartet werden. Daher hat v2 Slack.
- `LF(v2) = LS(v4) = 6`, also `LS(v2) = 6 − 4 = 2`. v2 darf zwei Zeiteinheiten später starten,
  ohne dass sich etwas verzögert.

**Kritischer Pfad:** v1 → v3 → v4 → v5 → v6, Länge `3+3+1+2+5 = 14 = T∞`.

**Gesamtarbeit:** `T₁ = 3+4+3+1+2+5 = 18`.

### 9.5 Arbeit, Tiefe und was daraus folgt

| Größe | Definition | im Beispiel |
|---|---|---|
| **Arbeit `T₁`** | Summe aller Gewichte = Laufzeit auf 1 Prozessor | 18 |
| **Tiefe `T∞`** | Länge des kritischen Pfads = Laufzeit auf ∞ Prozessoren | 14 |
| **Parallelität** | `T₁ / T∞` — mittlere sinnvolle Prozessorzahl | 18/14 = 1,29 |

Daraus folgen die beiden fundamentalen Schranken für jede Ausführung auf `p` Prozessoren:

$$ T(p) \ge \max\left( T_\infty, \; \frac{T_1}{p} \right) $$

Die erste Schranke ist die **Abhängigkeitsschranke** (der kritische Pfad lässt sich nicht
umgehen), die zweite die **Arbeitsschranke** (mehr als `p`-fache Beschleunigung geht nie).
Zusätzlich garantiert jeder gierige Scheduler (**Satz von Brent**):

$$ T(p) \le \frac{T_1}{p} + T_\infty $$

Mehr Prozessoren als `T₁/T∞` einzusetzen bringt also fast nichts — im Beispiel wäre bei
Parallelität 1,29 schon der zweite Prozessor kaum ausgelastet. **Das ist die praktische
Aussage des kritischen Pfads:** Er sagt einem, ab wann sich weitere Hardware nicht mehr lohnt,
noch bevor man eine Zeile Code geschrieben hat.

> Beachte: `T₁/T∞` ist eng verwandt mit Amdahl (Kapitel 10). Der kritische Pfad ist gewissermaßen
> der „serielle Anteil" in strukturierter Form — nur dass er nicht als Prozentsatz geschätzt,
> sondern aus dem Graphen exakt berechnet wird.

### 9.6 Wie viele Prozessoren braucht man wirklich?

`T₁/T∞` ist ein Durchschnitt. Die Zahl der Prozessoren, die man tatsächlich braucht, um `T∞` zu
erreichen, liest man aus dem **ES-Zeitplan** ab: Man zeichnet alle Tasks in ein Zeitdiagramm bei
ihrem `ES` ein und bestimmt das Maximum der gleichzeitig laufenden Tasks.

```
Zeit:  0    2    4    6    8   10   12   14
v1     ████████████
v2     ████████████████
v3                 ████████████
v4                             ████
v5                                 ████████
v6                                         ████████████████████
             ↑
    hier laufen 2 Tasks gleichzeitig → p = 2 genügt für T∞ = 14
```

---

## 10. Typische Klausurfragen

- **Warum braucht man `single` beim Erzeugen von Tasks?** Sonst erzeugt jeder Thread den
  gesamten Task-Satz — `p`-fache Arbeit.
- **Was ist die Default-Datenumgebung einer Task?** `firstprivate`, außer die Variable war im
  umgebenden Kontext `shared` — dann wird `shared` vererbt.
- **Warum steht bei Fibonacci `shared(i)`?** Sonst schriebe die Task in ihre eigene Kopie, und
  der Erzeuger sähe das Ergebnis nicht.
- **Unterschied `taskwait` / `taskgroup`?** Direkte Kinder vs. alle Nachkommen.
- **Unterschied `if(false)` / `final(true)`?** Nur diese Task vs. der gesamte Teilbaum wird
  sofort ausgeführt.
- **Nenne die drei Bernstein-Bedingungen** und die zugehörigen Abhängigkeitstypen (RAW, WAR, WAW).
- **Warum ist gemeinsames Lesen unkritisch?** Es taucht in keiner der drei Bedingungen auf.
- **Was besagt der kritische Pfad?** Untere Schranke der Laufzeit, auch mit unendlich vielen
  Prozessoren.
- **Wie berechnet man ES/EF/LS/LF?** Vorwärtslauf mit Maximum, Rückwärtslauf mit Minimum.
- **Was bedeutet Slack = 0?** Die Task liegt auf dem kritischen Pfad.
- **Warum bringt es nichts, mehr als `T₁/T∞` Prozessoren einzusetzen?** Die Tiefe begrenzt;
  zusätzliche Prozessoren finden keine bereite Arbeit mehr.

---

## 11. Fallstricke auf einen Blick

| Fehler | Warum falsch | Richtig |
|---|---|---|
| Tasks ohne `single` erzeugen | Jeder Thread erzeugt alles → `p`-fache Arbeit | `#pragma omp parallel` + `#pragma omp single` |
| `#pragma omp task` ohne umgebendes `parallel` | Team der Größe 1 → alles seriell (kein Fehler, aber wirkungslos) | Region drumherum |
| Ergebnisvariable nicht `shared` | Task schreibt in ihre firstprivate-Kopie | `shared(i)` + `taskwait` |
| `taskwait` vergessen | Ergebnis noch nicht da; Stackframe kann verschwinden | `taskwait` vor dem Lesen |
| `taskwait` statt `taskgroup` bei tiefer Rekursion | Wartet nur auf Kinder, nicht auf Enkel | `taskgroup` |
| Kein Cutoff bei Rekursion | Task-Overhead > Nutzarbeit, oft 10–100× langsamer | `if`/`final`/Tiefenschranke |
| Zeigervariable geändert nach Task-Erzeugung | Die Kopie wird beim Erzeugen gemacht — der Wert danach ist egal | genau deshalb ist firstprivate richtig |
| WAR/WAW-Kanten in `depend` übersehen | Tasks serialisieren unerwartet | Bernstein systematisch prüfen |
| Rückwärtslauf mit Maximum gerechnet | LF ist das **Minimum** über die Nachfolger | siehe 9.3 |
| `priority` als Garantie verstanden | Es ist nur ein Hinweis an die Laufzeit | Reihenfolge nie darauf stützen |

---

## 12. Merkkasten

> **Kernaussagen des Kapitels**
> - Tasks lösen, was `for` nicht kann: Rekursion, Listen, unbekannte und unbalancierte Arbeit.
> - Idiom: `parallel` → `single` → `task`. Einer erzeugt, alle arbeiten.
> - Task-Scoping ist **firstprivate** per Default — nur `shared` wird vererbt.
> - Ergebnisse brauchen `shared` **und** `taskwait`.
> - Ohne Cutoff ist eine rekursive Task-Version meist langsamer als seriell.
> - Bernstein: `I₁∩O₂ = I₂∩O₁ = O₁∩O₂ = ∅`. Nur-Lesen ist immer erlaubt.
> - `depend` erzeugt auch WAR- und WAW-Kanten — nicht nur echten Datenfluss.
> - Vorwärtslauf: `ES = max(EF der Vorgänger)`. Rückwärtslauf: `LF = min(LS der Nachfolger)`.
> - Slack = 0 ⟺ kritischer Pfad. `T∞` = untere Schranke, `T₁/T∞` = sinnvolle Prozessorzahl.
> - `max(T∞, T₁/p) ≤ T(p) ≤ T₁/p + T∞`.

---

## 13. Verbindung zum Rest der Vorlesung

- **Kapitel 06 (OpenMP):** Direkter Vorgänger. Das Scoping ist dasselbe Prinzip mit anderen
  Defaults; `single` und die impliziten Barrieren kommen von dort.
- **Kapitel 02 (Rechnerarchitektur):** RAW/WAR/WAW sind genau die Hazards der Pipeline — nur
  eine Abstraktionsebene höher. *Register renaming* im Prozessor löst WAR und WAW auf demselben
  Weg auf wie das Umbenennen von Variablen hier.
- **Kapitel 08 (Barnes-Hut):** Die Baumtraversierung aus Abschnitt 4.1 in echt — ein
  unbalancierter Quadtree ist der Musterfall für Tasks.
- **Kapitel 10 (Amdahl/Gustafson):** `T₁/T∞` ist die strukturierte Variante des seriellen
  Anteils. Blatt 8 kombiniert beides in einer Aufgabe: 8.1 hier, 8.2/8.3 dort.
- **Kapitel 14 (Strassen):** Divide & Conquer mit sieben rekursiven Aufrufen — ein natürlicher
  Task-Graph, bei dem der Cutoff über Erfolg oder Misserfolg entscheidet.

---

**Weiter:** [Übungen](uebungen.md) → danach [Lösungen](loesungen.md) ·
Lauffähige Beispiele in [`code/`](code/)
