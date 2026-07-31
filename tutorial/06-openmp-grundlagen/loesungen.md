# Kapitel 06 — Lösungen: OpenMP-Grundlagen

> Erst [`uebungen.md`](uebungen.md) selbst bearbeiten.

---

## Lösung 6.1 — Scoping vorhersagen

**a) Scoping der vier Variablen**

| Variable | Scope | Regel |
|---|---|---|
| `g` | **shared** | Globale Variable — außerhalb der Region deklariert |
| `h` | **shared** | In `main` vor der Region deklariert |
| `u` | **private** | Innerhalb des parallelen Blocks deklariert |
| `v` | **shared** | `static` liegt im statischen Speicher, **nicht** auf dem Thread-Stack |

`v` ist die Falle: Die Deklaration steht im Block, die Variable ist es aber nicht.

**b)** `u` hat in **jedem** Thread den Wert `1`. Jeder Thread hat seine eigene Kopie, die er
mit 0 initialisiert und einmal erhöht.

**c)** Möglich ist alles von `g=1 h=1` bis `g=4 h=4`, unabhängig voneinander. `g++` ist keine
atomare Operation, sondern drei Maschinenschritte:

```
load  r ← g
add   r ← r + 1
store g ← r
```

Lesen zwei Threads denselben Wert `g = 2`, schreiben beide `3` zurück — eine Erhöhung geht
verloren (*lost update*). Wie oft das passiert, hängt vom zufälligen Timing ab; deshalb ist das
Ergebnis bei jedem Lauf anders. `v` ist übrigens genauso betroffen, nur wird es nicht ausgegeben.

**d) Zwei Reparaturen**

```cpp
// 1) Reduktion — die schnellste Variante
#pragma omp parallel reduction(+ : g, h)
{ ... g++; h++; ... }

// 2) Atomare Updates — funktioniert auch, wenn der Wert währenddessen gebraucht wird
#pragma omp parallel
{
    #pragma omp atomic
    g++;
    #pragma omp atomic
    h++;
}
```

`#pragma omp critical` um beide Inkremente wäre eine dritte, aber langsamste Möglichkeit.

> **Typischer Fehler hier:** `v` als privat einzustufen, weil die Deklaration im Block steht.

---

## Lösung 6.2 — Threadzahl und Rangfolge

**a) Ausgabe** (mit `OMP_NUM_THREADS=6`)

```
A: 1 6
B: 2 6
C: 1 3
D: 3 3
```

| Zeile | `omp_get_num_threads()` | `omp_get_max_threads()` | Warum |
|---|---|---|---|
| A | 1 | 6 | serieller Bereich: Team = nur Master; max aus der Umgebungsvariablen |
| B | 2 | 6 | `num_threads(2)` bestimmt die Teamgröße; die ICV bleibt bei 6 |
| C | 1 | 3 | wieder seriell; `omp_set_num_threads(3)` hat die ICV geändert |
| D | 3 | 3 | Region ohne Klausel nimmt den ICV-Wert 3 |

**b)** Im seriellen Bereich besteht das aktuelle Team nur aus dem Master —
`omp_get_num_threads()` liefert daher immer `1`. Die Zahl, die man dort meist wissen will
(„mit wie vielen Threads läuft die nächste Region?"), liefert `omp_get_max_threads()`.

**c)** Die `num_threads`-Klausel wirkt **nur auf die Region, an der sie steht**. Sie verändert
die interne Kontrollvariable *nthreads-var* nicht. `omp_set_num_threads()` und
`OMP_NUM_THREADS` schreiben dagegen genau diese ICV — deshalb ändert sich `max_threads` in
Zeile C, aber nicht in Zeile B.

**d)** Die **ausgegebenen Zahlen bleiben identisch**, denn in beiden Fällen druckt genau ein
Thread. Zwei Unterschiede gibt es trotzdem:

- Bei `master` ist es garantiert Thread 0, bei `single` irgendein Thread des Teams.
- `single` hat eine **implizite Barriere** am Ende, `master` nicht — alle anderen Threads
  warten bei `single` also auf den Druckenden.

---

## Lösung 6.3 — Scheduling und Lastbalance

**a) Zuordnung bei `n = 24`, `p = 4`**

`schedule(static)` — 24/4 = 6 zusammenhängende Iterationen pro Thread:

| Thread | Iterationen |
|---|---|
| T0 | 0–5 |
| T1 | 6–11 |
| T2 | 12–17 |
| T3 | 18–23 |

`schedule(static, 2)` — 12 Blöcke à 2, reihum verteilt:

| Thread | Iterationen |
|---|---|
| T0 | 0,1, 8,9, 16,17 |
| T1 | 2,3, 10,11, 18,19 |
| T2 | 4,5, 12,13, 20,21 |
| T3 | 6,7, 14,15, 22,23 |

`schedule(dynamic, 3)` — 8 Blöcke à 3 (`{0–2}, {3–5}, …, {21–23}`), aber die Zuordnung ist
**nicht vorhersagbar**: Ein Thread holt sich den nächsten freien Block erst, wenn er mit dem
vorherigen fertig ist. Welcher das ist, hängt von der Ausführungsgeschwindigkeit ab und ändert
sich von Lauf zu Lauf. Garantiert ist nur: jeder Block wird genau einmal bearbeitet, und ein
Thread bekommt höchstens `⌈8/1⌉` Blöcke.

**b) Kosten `c(i) = i + 1`, `schedule(static)`**

Gesamtarbeit: $\sum_{i=0}^{23}(i+1) = \frac{24 \cdot 25}{2} = 300$ Zeiteinheiten $= T(1)$.

| Thread | Iterationen | Kosten | Summe |
|---|---|---|---|
| T0 | 0–5 | 1+2+3+4+5+6 | **21** |
| T1 | 6–11 | 7+…+12 = (7+12)·6/2 | **57** |
| T2 | 12–17 | 13+…+18 = (13+18)·6/2 | **93** |
| T3 | 18–23 | 19+…+24 = (19+24)·6/2 | **129** |

Probe: 21+57+93+129 = 300 ✓

Die Laufzeit ist das **Maximum**, nicht der Durchschnitt — alle warten auf T3:

$$ T(4) = 129, \quad S(4) = \frac{300}{129} = 2{,}33, \quad E(4) = \frac{2{,}33}{4} = 0{,}58 $$

**c)** Bei perfekter Verteilung: $T_{\text{ideal}} = 300/4 = 75$. Tatsächlich 129, also
$129/75 = 1{,}72$ — **72 % mehr Laufzeit** als nötig. Anders gesagt: 41,9 % der verfügbaren
Rechenzeit ($ (129-75)/129 $) verbringen die Threads mit Warten. T0 ist nach 21 von 129
Zeiteinheiten fertig und idlet danach 84 % der Zeit.

**d) `schedule(static, 2)`**

| Thread | Kosten der Iterationen | Summe |
|---|---|---|
| T0 | 1+2 + 9+10 + 17+18 | **57** |
| T1 | 3+4 + 11+12 + 19+20 | **69** |
| T2 | 5+6 + 13+14 + 21+22 | **81** |
| T3 | 7+8 + 15+16 + 23+24 | **93** |

Probe: 57+69+81+93 = 300 ✓

$$ T(4) = 93, \quad S(4) = \frac{300}{93} = 3{,}23, \quad E(4) = 0{,}81 $$

Die Verbesserung von $S = 2{,}33$ auf $S = 3{,}23$ entsteht, weil die Round-Robin-Verteilung
jedem Thread **billige und teure Iterationen mischt**, statt ihm einen zusammenhängenden
(und damit gleichmäßig teuren) Block zu geben. Entscheidend: Das kostet **keinen
Laufzeit-Overhead**, weil die Zuordnung wie bei `static` vorab feststeht — es wird kein
gemeinsamer Zähler abgefragt wie bei `dynamic`.

**e)** Gründe, trotzdem `static` zu wählen:

- **Reproduzierbarkeit**: Nur bei `static` bearbeitet derselbe Thread bei jedem Lauf dieselben
  Iterationen. Das ergibt bit-identische Gleitkommaergebnisse und macht Messungen vergleichbar.
- **NUMA-Lokalität** (*first touch*): Wenn ein Thread den Speicher initialisiert hat, den er
  später liest, liegen die Daten am richtigen Speicherkontroller — `dynamic` zerstört das.
- **Zwei Schleifen mit gleicher Aufteilung**: Nur bei explizitem `schedule(static)` mit
  gleicher Iterationszahl darf man sich darauf verlassen, dass Thread *t* in beiden Schleifen
  dieselben Indizes bekommt (siehe Aufgabe 6.7).

---

## Lösung 6.4 — Zwei Programme, zwei verschiedene Fehler

**a) P1: Race Condition (lost update)**

`sum += a[i]` zerfällt in drei Maschinenoperationen:

```
load  r ← sum          Thread 0: liest sum = 10.0
add   r ← r + a[i]     Thread 1: liest sum = 10.0   ← beide haben denselben Altwert
store sum ← r          Thread 0: schreibt 13.0
                       Thread 1: schreibt 12.0      ← Beitrag von Thread 0 ist weg
```

Zwischen `load` und `store` kann ein anderer Thread dazwischenfunken. Verloren gehen immer nur
Beiträge, nie kommen welche hinzu — deshalb ist das Ergebnis systematisch **zu klein** und bei
jedem Lauf anders.

**b) P2: False Sharing**

Jeder Thread schreibt nur in sein eigenes `partial[id]` — es gibt **keine** Race Condition, das
Ergebnis stimmt. Aber: `partial` ist `8 × 8 Byte = 64 Byte`, also **genau eine Cache-Line**.
Jeder Schreibzugriff eines Threads invalidiert die Kopie dieser Line in den Caches **aller
anderen sieben Kerne**. Bei jeder Iteration wandert die Line durch das Kohärenzprotokoll von
Kern zu Kern (*cache line ping-pong*). Statt eines L1-Treffers (~4 Zyklen) kostet jeder Zugriff
dann eher 50–200 Zyklen — das kann die parallele Version langsamer machen als die serielle.

**c)** Cache-Line typischerweise **64 Byte** → **8 `double`** oder 16 `int`. `partial` belegt
damit **genau eine** Cache-Line (bei 64-Byte-Alignment; ungünstig ausgerichtet zwei, was das
Problem nur unwesentlich abmildert).

**d) Reparaturen**

```cpp
/* P1 — Reduktion statt shared */
double sum = 0.0;
#pragma omp parallel for reduction(+ : sum)
for (int i = 0; i < n; i++) sum += a[i];
```

```cpp
/* P2, Lösung 1 — Padding: jedes Element auf eine eigene Cache-Line */
double partial[8][8];              /* 8 doubles = 64 Byte Abstand */
partial[id][0] += a[i];
```

```cpp
/* P2, Lösung 2 — strukturell: gar nicht erst geteilt schreiben */
#pragma omp parallel num_threads(8)
{
    double loc = 0.0;              /* private, liegt im Register */
    #pragma omp for
    for (int i = 0; i < n; i++) loc += a[i];
    #pragma omp atomic
    sum += loc;                    /* genau EIN geteilter Zugriff pro Thread */
}
```

Lösung 2 ist die bessere: Sie vermeidet das Problem, statt es zu umgehen — und ist genau das,
was `reduction` intern tut. Padding bleibt nötig, wenn man die Teilergebnisse wirklich
einzeln aufbewahren muss.

**e)** ThreadSanitizer findet **P1**: Dort greifen mehrere Threads unsynchronisiert auf
**dieselbe Adresse** zu, mindestens einer schreibend — genau die Definition eines *data race*,
und danach sucht der Detektor.

**P2 findet er nicht.** Dort schreibt jeder Thread ausschließlich auf seine **eigene Adresse**;
aus Sicht des Speichermodells ist alles korrekt synchronisiert. False Sharing ist kein
Korrektheits-, sondern ein Hardwareproblem eine Ebene tiefer (Kohärenz auf Cache-Line-Granularität).
Man findet es mit Performance-Countern, z. B. `perf c2c` oder auffällig hohen
`cache-misses`/HITM-Ereignissen bei gleichbleibender Arbeit.

> **Merksatz zur Aufgabe:** P1 = falsches Ergebnis, P2 = richtiges Ergebnis mit
> Performanceeinbruch. Diese Gegenüberstellung ist eine Standard-Klausurfrage.

---

## Lösung 6.5 — Vektor normalisieren in zwei Phasen

**a) Implementierung**

```cpp
#include <math.h>
#include <omp.h>

void normalize(double *x, int n) {
    double norm2 = 0.0;
    double inv;

    #pragma omp parallel default(none) shared(x, n, norm2, inv)
    {
        /* Phase 1: Norm berechnen — Reduktion */
        #pragma omp for reduction(+ : norm2)
        for (int i = 0; i < n; i++)
            norm2 += x[i] * x[i];
        /* implizite Barriere: hier ist norm2 vollständig */

        /* Kehrwert einmal berechnen */
        #pragma omp single
        inv = 1.0 / sqrt(norm2);
        /* implizite Barriere: hier ist inv für alle sichtbar */

        /* Phase 2: skalieren */
        #pragma omp for
        for (int i = 0; i < n; i++)
            x[i] *= inv;
    }
}
```

`i` ist jeweils in der Schleife deklariert und damit automatisch privat — deshalb reicht
`shared(...)` in der `default(none)`-Liste.

**b)** Synchronisiert werden muss **zwischen Phase 1 und Phase 2**. Die Teilsummen der
Reduktion werden erst an der impliziten Barriere am Ende der ersten `for`-Schleife zu `norm2`
zusammengeführt. Ohne diese Barriere könnte ein schneller Thread bereits Phase 2 beginnen und
mit einem `norm2` rechnen, in dem die Beiträge langsamerer Threads noch fehlen — der Vektor
würde teilweise mit einem falschen Faktor skaliert. Da einige Elemente dann mit dem richtigen
und andere mit einem falschen Faktor multipliziert wären, hätte das Ergebnis nicht einmal
Länge 1.

**c)** Genau deshalb darf an die erste Schleife **kein `nowait`**: Das würde die Barriere
entfernen, an der die Reduktion abgeschlossen wird. `nowait` an einer Schleife mit `reduction`
ist fast immer ein Fehler.

**d)** In ein `#pragma omp single`, und `inv` muss **shared** sein (sonst schriebe der eine
Thread in seine private Kopie, und die anderen sähen den Wert nie). `single` ist hier `master`
vorzuziehen, weil man dessen implizite Barriere braucht, damit `inv` für alle sichtbar ist.

Alternative ohne `single`: Jeder Thread berechnet `double inv = 1.0/sqrt(norm2);` selbst in eine
private Variable. Das kostet `p` statt eine Wurzel, spart aber eine Barriere — die Barriere der
`reduction`-Schleife reicht dann aus.

**e) Messung** (Größenordnung, 8 Kerne, `n = 10⁷`)

Der Speedup sättigt typischerweise bei 2–3 und wächst danach kaum noch. Grund: Die Aufgabe ist
**speicherbandbreitenlimitiert**. Pro Element werden in Phase 1 8 Byte geladen für 2 Flop
(Multiplikation + Addition), in Phase 2 8 Byte geladen und 8 geschrieben für 1 Flop. Die
arithmetische Intensität liegt also bei ≈ 0,06–0,25 Flop/Byte. Schon 2–3 Kerne sättigen die
Speicheranbindung; weitere Kerne bekommen keine Daten mehr nachgeliefert und warten. Das
Matrix-Vektor-Produkt lädt `A` zwar auch nur einmal, leistet dafür aber `2n` Flop pro geladener
Zeile — und skaliert deshalb besser. (Formal wird das im Roofline-Modell, Kapitel 10, behandelt.)

---

## Lösung 6.6 — Histogramm

**a)** Zwei Threads können Werte in **dieselbe** Klasse einsortieren. Dann führen beide
`hist[b]++` auf derselben Speicherstelle aus — Lesen/Erhöhen/Schreiben ohne Schutz, also eine
Race Condition. Die Zählung fällt zu niedrig aus, und zwar bei jedem Lauf anders. Zusätzlich
liegt bei kleinem `B` das gesamte Histogramm in ein bis zwei Cache-Lines → obendrein massives
false sharing.

**b) Variante A: `atomic`**

```cpp
#pragma omp parallel for default(none) shared(v, hist, n, B)
for (int i = 0; i < n; i++) {
    int b = (int)(v[i] * B);
    #pragma omp atomic
    hist[b]++;
}
```

`atomic` genügt, weil die zu schützende Operation **genau eine Speicheraktualisierung** der
Form `x++` ist — das passt exakt auf `atomic update`. Der entscheidende Vorteil gegenüber
`critical`: `atomic` wird auf eine Hardware-Instruktion (z. B. `lock inc`) abgebildet und
blockiert nur Zugriffe auf **dieselbe Adresse**. Ein `critical` wäre ein globales Lock — zwei
Threads, die völlig verschiedene Klassen hochzählen, würden sich gegenseitig ausbremsen.

**c) Variante B: private Histogramme**

```cpp
#pragma omp parallel default(none) shared(v, hist, n, B)
{
    // im Block deklariert -> private je Thread; RAII gibt den Speicher
    // am Ende der Region automatisch frei, auch bei einem frühen Ausstieg
    std::vector<long> lokal(B, 0);

    #pragma omp for nowait                    // nowait OK: lokal ist thread-lokal
    for (int i = 0; i < n; ++i)
        ++lokal[static_cast<int>(v[i] * B)];

    #pragma omp critical                      // Zusammenführung
    for (int b = 0; b < B; ++b)
        hist[b] += lokal[b];
}
```

Die Endphase sichert `critical` ab — hier ist es die richtige Wahl (und nicht `atomic`), weil
ein ganzer Schleifenblock geschützt werden soll. Alternativ `atomic` pro Element; bei `p`
Threads und `B` Klassen sind das `p·B` atomare Operationen statt `p` Lock-Übernahmen.

**d) Messung** (`n = 10⁸`)

| | `B = 8` | `B = 4096` |
|---|---|---|
| Variante A (`atomic`) | sehr langsam, oft langsamer als seriell | nahezu so schnell wie B |
| Variante B (privat) | gute Skalierung | gute Skalierung |

Der Grund liegt in der **Kollisionswahrscheinlichkeit**. Bei `B = 8` landen alle Threads
ständig in denselben acht Zählern: Jede atomare Operation kollidiert praktisch immer, und die
ein bis zwei Cache-Lines des Histogramms pendeln unaufhörlich zwischen den Kernen. Bei
`B = 4096` trifft es zwei Threads nur mit Wahrscheinlichkeit ≈ 1/4096 gleichzeitig auf
derselben Klasse; die atomare Operation läuft dann fast immer ungestört im lokalen Cache und
kostet kaum mehr als ein normales Inkrement.

**e)** Variante B braucht `p · B` Zähler Speicher, und die Zusammenführung kostet `O(p · B)`.
Bei `B = 10⁶` und `p = 8` sind das 64 MB — die lokalen Histogramme passen nicht mehr in den
Cache, sodass schon Phase 1 langsam wird, und die Merge-Phase kann die eigentliche Arbeit
dominieren. Faustregel: Variante B lohnt, solange `B · sizeof(long)` bequem in den L2-Cache
eines Kerns passt.

**f) Array-Reduktion (OpenMP ≥ 4.5)**

```cpp
#pragma omp parallel for default(none) shared(v, n, B) reduction(+ : hist[ : B])
for (int i = 0; i < n; i++)
    hist[(int)(v[i] * B)]++;
```

Das entspricht **exakt Variante B**: Die Laufzeit legt für jeden Thread eine private Kopie des
Arrays an, initialisiert sie mit dem neutralen Element 0 und addiert sie am Ende auf. Man
bekommt dieselbe Strategie in einer Zeile — und erbt damit auch denselben Nachteil bei großem
`B`.

---

## Lösung 6.7 — `nowait` richtig setzen

**Abhängigkeiten zuerst notieren:**

| Schleife | schreibt | liest | Abhängigkeit |
|---|---|---|---|
| 1 | `a[i]` | – | – |
| 2 | `b[i]` | `a[i]` | von 1, **gleicher Index** |
| 3 | `c[i]` | – | – |
| 4 | `d[i]` | `a[n-1-i]`, `c[i]` | von 1 (**anderer Index!**), von 3 (gleicher Index) |

**a)** Der kritische Punkt ist Schleife 4: Sie liest `a[n-1-i]`, also einen Wert, den in
Schleife 1 mit ziemlicher Sicherheit ein **anderer** Thread geschrieben hat. Deshalb muss
zwischen Schleife 1 und Schleife 4 **mindestens eine Barriere** liegen — welche, ist frei
wählbar.

- **Schleife 2: `nowait` ist sicher.** Schleife 3 berührt weder `a` noch `b`.
- **Schleife 3: `nowait` ist nicht sicher** (siehe b) — Schleife 4 liest `c[i]`; ohne
  Barriere gilt nur die Gleich-Index-Regel, und die greift nur unter Zusatzannahmen.
- **Schleife 1: `nowait` nur zusammen mit einer erhaltenen Barriere danach.** Wenn die
  Barriere von Schleife 2 oder 3 stehen bleibt, ist `a` vor Schleife 4 vollständig.
- **Schleife 4:** `nowait` wäre erlaubt, bringt aber nichts — die Barriere der `parallel`-Region
  folgt unmittelbar danach.

Empfohlene Lösung ohne Zusatzannahmen: **`nowait` an Schleife 2**, Barrieren nach 1 und 3
behalten.

**b)** Ja — **aber nur unter drei Bedingungen gleichzeitig**: beide Schleifen haben dieselbe
Iterationszahl, beide verwenden **explizit** `schedule(static)` mit derselben Chunk-Größe, und
beide gehören zum selben Team. Dann garantiert der Standard, dass Thread *t* in beiden
Schleifen dieselben Indizes bearbeitet — Thread *t* liest also in Schleife 2 nur `a[i]`, die er
in Schleife 1 selbst geschrieben hat. Ohne das explizite `schedule(static)` ist die
Voreinstellung **implementierungsabhängig**, und die Zusicherung gilt nicht.

Mit dieser Annahme lässt sich `nowait` zusätzlich an Schleife 1 setzen:

```cpp
#pragma omp parallel default(none) shared(a, b, c, d, n)
{
    #pragma omp for schedule(static) nowait
    for (int i = 0; i < n; i++) a[i] = f(i);
    #pragma omp for schedule(static) nowait
    for (int i = 0; i < n; i++) b[i] = a[i] * 2.0;
    #pragma omp for schedule(static)          /* Barriere: sichert a UND c für Schleife 4 */
    for (int i = 0; i < n; i++) c[i] = g(i);
    #pragma omp for schedule(static)
    for (int i = 0; i < n; i++) d[i] = a[n-1-i] + c[i];
}
```

**c)** Ursprünglich **5** implizite Barrieren: je eine am Ende der vier `for`-Konstrukte plus
eine am Ende der `parallel`-Region. Nach der konservativen Optimierung (nur Schleife 2)
bleiben **4**; mit der `schedule(static)`-Variante oben **3**.

**d)** Die Barriere am Ende der `parallel`-Region ist die *Join*-Operation selbst: Der Master
darf erst weiterlaufen, wenn alle Threads des Teams fertig sind, und die privaten Kopien der
Threads verlieren dort ihre Gültigkeit. Ohne diese Synchronisation wäre der serielle Code nach
der Region nicht mehr wohldefiniert. Der Standard erlaubt `nowait` deshalb ausschließlich bei
Work-Sharing-Konstrukten (`for`, `single`, `sections`), nie bei `parallel`.

---

## Lösung 6.8 — Laufendes Maximum

**a) Loop-carried dependence**

`m[i]` hängt von `m[i-1]` ab, das eine frühere Iteration schreibt. Eine parallele Ausführung
verletzt diese Reihenfolge; OpenMP prüft das nicht und erzeugt klaglos falschen Code.

Konkretes Gegenbeispiel, `n = 4`, `r = [1, 7, 2, 3]`, zwei Threads, `schedule(static)`
(T0 bekommt `i = 1`, T1 bekommt `i = 2, 3`):

| | seriell | parallel, T1 läuft zuerst |
|---|---|---|
| `m[0]` | 1 | 1 |
| `m[1]` | max(1,7) = 7 | wird von T0 später auf 7 gesetzt |
| `m[2]` | max(7,2) = **7** | T1 liest `m[1]` **bevor** T0 es schreibt (Wert 0/undefiniert) → max(0,2) = **2** |
| `m[3]` | max(7,3) = **7** | max(2,3) = **3** |

Ergebnis parallel `[1, 7, 2, 3]` statt korrekt `[1, 7, 7, 7]`.

**b) Geschlossene Form**

$$ m[i] = \max_{0 \le k \le i} r[k] $$

Das laufende Maximum ist ein **Präfix-Scan** mit dem Operator `max` — dieselbe Struktur wie die
Präfixsumme in Fragment c) von Blatt 6, nur mit einem anderen assoziativen Operator.

**c) Zweiphasige Lösung**

```cpp
/* Phase 1: die teuren Werte r[i] unabhängig berechnen — O(n·k), voll parallel */
#pragma omp parallel for default(none) shared(r, n) schedule(static)
for (int i = 0; i < n; i++)
    r[i] = teuer(i);                 /* Kosten O(k) pro Eintrag */

/* Phase 2: laufendes Maximum — O(n), seriell */
m[0] = r[0];
for (int i = 1; i < n; i++)
    m[i] = (m[i-1] > r[i]) ? m[i-1] : r[i];
```

Die gesamte teure Arbeit steckt in Phase 1 und ist vollständig unabhängig. Phase 2 ist ein
einziger billiger Durchlauf.

**d) Amdahl-Abschätzung**

Arbeit: Phase 1 kostet $n \cdot k$, Phase 2 kostet $n$. Serieller Anteil:

$$ f = \frac{n}{n \cdot k + n} = \frac{1}{k+1} = \frac{1}{101} \approx 0{,}0099 $$

$$ S_{\max} = \lim_{p \to \infty} \frac{1}{f + \frac{1-f}{p}} = \frac{1}{f} = k + 1 = \mathbf{101} $$

Der Wert hängt **nicht von `n`** ab — nur vom Kostenverhältnis `k`. Das ist die eigentliche
Botschaft: Solange der teure Teil parallelisierbar ist, schadet ein billiger serieller
Nachlauf kaum. Bei `k = 1` (billiges `r[i]`) wäre dagegen $S_{\max} = 2$ — dann lohnt sich
Teilaufgabe e).

**e) Vollständig parallele Phase 2**

```
Durchlauf 1 (parallel):  jeder Thread t bildet lokalMax[t] über seinen Block
Zwischenschritt (seriell, O(p)):  exklusives Präfix-Maximum über lokalMax[0..p-1]
                                  → carry[t] = max aller Blöcke vor Block t
Durchlauf 2 (parallel):  jeder Thread startet mit carry[t] und schreibt sein Block-Präfix
```

Sie braucht **zwei Durchläufe über die Daten** statt einem, also `2n/p` statt `n` Zeit
(plus `O(p)` für den seriellen Zwischenschritt). Sie lohnt sich damit ab `p > 2` — praktisch
aber erst, wenn Phase 2 gegenüber Phase 1 überhaupt ins Gewicht fällt, also bei kleinem `k`.
Bei der Wahl unbedingt an Padding für `lokalMax[]` denken, sonst holt man sich false sharing.

Seit OpenMP 5.0 gibt es das fertig als Direktive:

```cpp
#pragma omp parallel for reduction(inscan, max : run)
for (int i = 0; i < n; i++) {
    run = (run > r[i]) ? run : r[i];
    #pragma omp scan inclusive(run)
    m[i] = run;
}
```

---

## Lösung 6.9 — Messreihe und Auswertung

**a) Messmethodik**

```cpp
double best = 1e30;
for (int rep = 0; rep < 5; rep++) {
    double t0 = omp_get_wtime();
    matvec(A, x, y, m, n);
    double t1 = omp_get_wtime();
    if (t1 - t0 < best) best = t1 - t0;
}
```

**Minimum statt Mittelwert**, weil Störungen nur in **eine Richtung** wirken: Kontextwechsel,
Interrupts, andere Prozesse, Turbo-Takt-Absenkung und Cache-Verdrängung machen eine Messung
immer *langsamer*, nie schneller. Der kleinste Messwert ist deshalb der beste Schätzer für die
ungestörte Laufzeit. Ein Mittelwert misst mit, wie ausgelastet der Rechner nebenbei war. Vor
der Messreihe außerdem einmal ungemessen durchlaufen lassen (Caches füllen, Threads erzeugen).

**b) Beispielergebnis** (`m = n = 4096`, `double`, 8 Kerne — eigene Zahlen können abweichen)

| p | T(p) [s] | S(p) | E(p) |
|---|---|---|---|
| 1 | 0,0182 | 1,00 | 1,00 |
| 2 | 0,0098 | 1,86 | 0,93 |
| 4 | 0,0061 | 2,98 | 0,75 |
| 8 | 0,0049 | 3,71 | 0,46 |

Der Effizienzabfall ab 4 Threads ist erwartbar: Das Matrix-Vektor-Produkt liest `A` genau
einmal und leistet nur 2 Flop pro Matrixelement (8 Byte) — es wird bei größerem `p`
bandbreitenlimitiert. Bei 8 Threads kommen auf einem 4-Kern-Prozessor mit SMT zusätzlich
zwei Threads pro physischem Kern, die sich die Rechenwerke teilen.

**c)** `T(1)` der parallelen Version enthält den OpenMP-Overhead: Fork/Join der Region, die
Berechnung der Schleifengrenzen, und der Compiler kann die annotierte Schleife oft schlechter
optimieren (z. B. weniger aggressiv vektorisieren). Sie ist damit meist etwas langsamer als der
serielle Code.

Unterschieden werden:

- **relativer (Self-)Speedup** $S_{\text{rel}}(p) = T_{\text{par}}(1)/T_{\text{par}}(p)$ — misst,
  wie gut die Parallelisierung skaliert,
- **absoluter Speedup** $S_{\text{abs}}(p) = T_{\text{seriell,best}}/T_{\text{par}}(p)$ — misst,
  ob man gegenüber dem besten seriellen Programm überhaupt gewonnen hat.

Ehrlich ist der **absolute** Speedup; der relative sieht immer besser aus, weil er den
Overhead in den Nenner *und* in den Zähler schreibt. In einem Bericht gehört dazu, welche
Variante man angibt.

**d) `m = 64`, `n = 4096`**

Die Gesamtarbeit sinkt um den Faktor 64, die Zahl der verteilbaren Iterationen von 4096 auf 64.
Zwei Effekte:

- Der **Fork/Join-Overhead** (einige µs) fällt jetzt gegenüber der Rechenzeit ins Gewicht.
- Bei `p = 8` bekommt jeder Thread 8 Zeilen; ist `m` nicht durch `p` teilbar, bleibt sofort
  spürbare **Lastungleichheit** übrig (bei `m = 60, p = 8` etwa bearbeiten manche Threads 8,
  andere 7 Zeilen → 12,5 % Verlust).

Der Speedup fällt deutlich schlechter aus, die Effizienz bricht früher ein. Zusätzlich ist
`x` (32 KB) jetzt cache-resident, was die serielle Version relativ begünstigt.

**e) Rentabilitätsgrenze**

Man misst seriell und parallel für wachsendes `m·n` und sucht den Schnittpunkt. Auf einem
typischen 8-Kern-Laptop liegt er in der Größenordnung von einigen 10 000 Matrixelementen —
darunter dominiert die Thread-Erzeugung. Daraus folgt:

```cpp
#pragma omp parallel for if (m * (long)n > 50000) default(none) \
        shared(A, x, y, m, n) schedule(static)
for (int i = 0; i < m; i++) { ... }
```

Der Cast auf `long` verhindert einen Überlauf bei großen Matrizen. Wichtig ist, den Schwellwert
**auf der Zielmaschine zu messen** statt zu raten — er hängt von Kernzahl, Laufzeitbibliothek
und Compiler ab.
