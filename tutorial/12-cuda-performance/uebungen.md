# Kapitel 12 — Übungen: CUDA II (Performance)

Erst selbst bearbeiten, dann mit [`loesungen.md`](loesungen.md) vergleichen.

**Die Aufgaben 12.1, 12.2, 12.3, 12.4, 12.6, 12.8, 12.9 und 12.10 sind reine Papieraufgaben**
— sie brauchen keine GPU und sind der klausurrelevante Kern. Für die Code-Aufgaben liegen
Programme, Makefile und Job-Skript in [`code/`](code/); sie laufen auf dem Ara-Cluster.

Wo nichts anderes steht, gilt für die A100 (wie auf Blatt 11):

$$P_{\text{peak}} = 19{,}5\ \text{TFLOP/s (FP32)}, \qquad B_{\text{peak}} = 1{,}5\ \text{TB/s}$$

Tags: `[Rechnen]` `[Code]` `[Analyse]` `[Klausur]`

---

## Aufgabe 12.1 `[Rechnen]` `[Klausur]` — Roofline: einordnen

*Zeit: ca. 20 min*

a) Berechne den Knickpunkt $I^\star$ der A100 und gib an, was er bedeutet. Wie viele FLOP sind
   das pro geladenem `float`?

Bestimme für die folgenden Kernel jeweils die FLOPs pro Element, die aus dem DRAM bewegten
Bytes pro Element, die arithmetische Intensität $I$, die erreichbare Leistung $P$, den
erreichbaren Anteil an $P_{\text{peak}}$ und die Klasse (speicher- oder rechengebunden).
Alle Felder sind `float`, es gibt keine Wiederverwendung außer der angegebenen.

```cpp
// (b)  Vektor-Kopie
y[i] = x[i];

// (c)  elementweises Produkt dreier Vektoren
d[i] = a[i] * b[i] * c[i];

// (d)  Norm der Differenz (nur die Schleife, die Reduktion sei kostenlos)
s += (x[i] - y[i]) * (x[i] - y[i]);

// (e)  Polynom 10. Grades nach Horner, Koeffizienten in Registern
y[i] = ((((((((( c10*x[i] + c9)*x[i] + c8)*x[i] + c7)*x[i] + c6)*x[i]
          + c5)*x[i] + c4)*x[i] + c3)*x[i] + c2)*x[i] + c1)*x[i] + c0;
```

f) Ist einer der vier Kernel rechengebunden? Welcher liegt dem Knickpunkt am nächsten, und ab
   welchem **Polynomgrad** wäre Kernel (e) gerade rechengebunden? Rechne die Gradschwelle aus.
g) Kernel (c) ist bisher als **zwei** Kernels implementiert (`t[i] = a[i]*b[i]`, danach
   `d[i] = t[i]*c[i]`). Bestimme $I$ und die Gesamtzeit dieser Variante und vergleiche mit dem
   verschmolzenen Kernel aus (c). Wie groß ist der Gewinn, und wo kommt er her?
h) Wie ändern sich $I$ und die Klasse aller Kernel, wenn man von `float` auf `double`
   umstellt — bei $P_{\text{peak}}^{\text{FP64}} = 9{,}7$ TFLOP/s und unveränderter
   Bandbreite? Was folgt daraus für die Wahl der Genauigkeit?

---

## Aufgabe 12.2 `[Rechnen]` — Transaktionen zählen

*Zeit: ca. 20 min*

Ein Warp (32 Threads, `t = 0 … 31`) liest `float`-Werte aus dem globalen Speicher. Die
Hardware bedient ausgerichtete **32-Byte-Sektoren**. Bestimme für jedes Muster die Zahl der
angeforderten Sektoren, die geholten Bytes und die Effizienz (gebrauchte/geholte Bytes).
`a` sei auf 128 Byte ausgerichtet.

| | Zugriff |
|---|---|
| a) | `a[t]` |
| b) | `a[t + 1]` |
| c) | `a[31 - t]` |
| d) | `a[2*t]` |
| e) | `a[16*t]` |
| f) | `a[0]` (alle Threads) |

g) Ein Partikelsystem ist als `struct P { float x, y, z; }; P part[N];` gespeichert, Thread
   `t` liest `part[t].x`. Bestimme Sektoren und Effizienz. Wie sieht es bei
   `struct P { float x[N], y[N], z[N]; }` und `part.x[t]` aus?
h) Ein Kernel liest **alle drei** Komponenten jedes Partikels. Ändert das die Antwort aus g)?
   Vergleiche mit der Situation auf einer CPU und erkläre, warum die Antworten
   auseinandergehen.
i) Wie hängt die Effizienz allgemein von der Schrittweite `s` ab? Ab welchem `s` wird es nicht
   mehr schlimmer, und warum?

---

## Aufgabe 12.3 `[Rechnen]` `[Klausur]` — Bankkonflikte

*Zeit: ca. 20 min*

Shared Memory hat 32 Bänke zu je 32 Bit; Wort `w` liegt in Bank `w mod 32`. Gib für jeden
Zugriff eines Warps den Konfliktgrad an (1 = konfliktfrei) und begründe.

```cpp
__shared__ float sm[1024];
int t = threadIdx.x;      // 0 .. 31 innerhalb des Warps

// (a) sm[t]
// (b) sm[2*t]
// (c) sm[3*t]
// (d) sm[8*t]
// (e) sm[32*t]
// (f) sm[t/2]
// (g) sm[5]          (alle Threads dasselbe Wort)
```

h) Leite die allgemeine Regel für `sm[s*t]` her und begründe sie.

Nun ein 2D-Tile:

```cpp
__shared__ float tile[32][32];
// (i)  tile[threadIdx.y][threadIdx.x]     mit einem Warp = konstantes y
// (j)  tile[threadIdx.x][threadIdx.y]     (transponiert gelesen)
```

k) Bestimme für (i) und (j) den Konfliktgrad — mit vollständiger Rechnung über den Wortindex.
l) Zeige durch Rechnung, dass `tile[32][33]` den Konflikt in (j) beseitigt. Wie viel
   zusätzlichen Shared Memory kostet das, absolut und relativ?
m) Warum ist `tile[32][34]` **keine** gute Wahl? Rechne nach.

---

## Aufgabe 12.4 `[Rechnen]` — Occupancy

*Zeit: ca. 25 min*

Für die A100 (Compute Capability 8.0) gilt pro SM: maximal **64 Warps** (2048 Threads),
maximal **32 Blöcke**, **65 536** 32-Bit-Register, **164 kB** nutzbarer Shared Memory.

a) Fülle die Tabelle aus. Bestimme für jede Konfiguration, wie viele Blöcke jede einzelne
   Ressource zulässt, welche die knappste ist, wie viele Warps resident sind und wie hoch
   die Occupancy ausfällt.

| # | `blockDim.x` | Register/Thread | Shared/Block |
|---|---|---|---|
| A | 256 | 32 | 8 kB |
| B | 256 | 64 | 8 kB |
| C | 256 | 32 | 48 kB |
| D | 1024 | 40 | 0 kB |
| E | 32 | 32 | 0 kB |

b) Konfiguration E braucht am wenigsten Ressourcen von allen und erreicht trotzdem keine
   100 %. Woran liegt es? Was folgt daraus als praktische Regel für die Wahl von `blockDim`?
c) Der Kernel aus Konfiguration B soll auf 100 % Occupancy gebracht werden, ohne `blockDim`
   zu ändern. Wie viele Register pro Thread darf er höchstens brauchen? Nenne zwei
   Möglichkeiten, wie man den Registerbedarf senkt, und je einen Nachteil.
d) Ein Kernel erreicht 25 % Occupancy und liegt trotzdem bei 88 % der Speicherbandbreite.
   Sollte man die Occupancy erhöhen? Begründe.
e) Mit welchem Compiler-Flag bekommt man den Register- und Shared-Memory-Verbrauch heraus?

---

## Aufgabe 12.5 `[Code]` — Reduktion: drei Wege

*Zeit: ca. 50 min*

Berechnet werden soll das Quadrat der euklidischen Distanz

$$s = \sum_{i=0}^{n-1} (x_i - y_i)^2$$

für `float`-Vektoren der Länge `n`.

a) Bestimme **vor** dem Programmieren die arithmetische Intensität und die erreichbare
   Leistung. Wie schnell kann der Kernel bestenfalls sein, und woran wirst du das nach der
   Messung erkennen?
b) **Fassung 1 (naiv):** Jeder Thread berechnet seinen Beitrag und addiert ihn per
   `atomicAdd` direkt auf das globale Ergebnis.
c) **Fassung 2 (Shared Memory):** Jeder Block reduziert seine Beiträge über eine
   Baumreduktion im Shared Memory und führt **ein** `atomicAdd` pro Block aus. Achte darauf,
   wo genau die `__syncthreads()` stehen müssen und warum.
d) **Fassung 3 (Warp-Shuffle):** Reduziere zuerst innerhalb jedes Warps mit
   `__shfl_down_sync`, sammle die (höchstens 32) Warp-Ergebnisse über Shared Memory und
   führe wieder ein `atomicAdd` pro Block aus.
e) Verifiziere alle drei gegen eine serielle CPU-Referenz. Warum darf man hier nicht auf
   Gleichheit prüfen? Was ist die richtige Prüfung?
f) Miss alle drei für `n = 10⁸` mit CUDA-Events und vergleiche. Welche Beschleunigung bringt
   Fassung 2 gegenüber 1, welche Fassung 3 gegenüber 2 — und warum sind die beiden
   Verhältnisse so unterschiedlich groß?
g) Berechne aus der Zeit von Fassung 3 die erreichte Bandbreite und vergleiche mit deiner
   Vorhersage aus a). Wenn du unter 80 % liegst: Woran könnte es liegen?
h) Fassung 2 benutzt `if (tid < s)`. Ersetze es testweise durch `if (tid % (2*s) == 0)` —
   dieselbe Rechnung, dieselbe Zahl an Additionen. Sage die Auswirkung **vorher** vorher, miss
   sie dann und erkläre die Differenz.

---

## Aufgabe 12.6 `[Rechnen]` — Roofline eines Stencils, naiv und gekachelt

*Zeit: ca. 30 min*

Ein 2D-Jacobi-Verfahren auf einem `N × N`-Gitter (`float`) rechnet pro innerem Punkt

$$u^{\text{neu}}_{i,j} = 0{,}25 \cdot \left(u_{i-1,j} + u_{i+1,j} + u_{i,j-1} + u_{i,j+1}\right)$$

Der naive Kernel gibt jedem Thread einen Gitterpunkt; jeder liest seine vier Nachbarn direkt
aus dem globalen Speicher.

a) Zähle die FLOPs pro Gitterpunkt.
b) Bestimme den globalen Speicherverkehr pro Gitterpunkt für den naiven Kernel (ohne jede
   Wiederverwendung), daraus $I$, $P$ und die Klasse.
c) Nun eine gekachelte Fassung: Jeder Block lädt eine `(T+2) × (T+2)`-Kachel (Halo!) in den
   Shared Memory und rechnet daraus `T × T` Ausgabepunkte. Bestimme den globalen Verkehr pro
   Ausgabepunkt als Funktion von `T` und daraus $I(T)$.
d) Werte $I(T)$ für `T = 8`, `16`, `32` aus. Gegen welchen Grenzwert läuft $I$ für `T → ∞`,
   und warum ist dieser Grenzwert eine harte Schranke — was genau verbietet ein größeres $I$?
e) Zeichne (oder beschreibe präzise) das Roofline-Diagramm mit dem Knickpunkt und den
   Betriebspunkten aus b) und d). Beschreibe die horizontale Verschiebung.
f) Vergleiche mit dem gekachelten Matrixprodukt, für das $I = T/4$ gilt. Warum kann das
   Kacheln dort die Klasse des Kernels ändern und beim Stencil nicht? Formuliere das
   allgemeine Kriterium.
g) Welche Beschleunigung darf man vom Kacheln beim Stencil realistisch erwarten? Lohnt sich
   der Aufwand?

---

## Aufgabe 12.7 `[Code]` — Gekacheltes Matrixprodukt

*Zeit: ca. 45 min*

a) Implementiere `matmul_naiv` (ein Thread pro Ausgabeelement, alle Zugriffe global) und
   `matmul_tiled` mit `TILE = 32` und Shared Memory.
b) Anders als in der Vorlesungsfassung soll dein gekachelter Kernel auch für `N`, das **kein**
   Vielfaches von `TILE` ist, korrekt sein. Was muss dafür hinzukommen, und an welchen Stellen?
c) Miss beide für `N = 1024`, `2048` und `4096` und rechne die erreichten GFLOP/s aus
   (`2N³` FLOP).
d) Trage beide Betriebspunkte auf der Roofline ein. Passen die gemessenen Werte zu den
   Vorhersagen $I = 0{,}25$ bzw. $I = T/4$? Falls nicht: Nenne zwei Gründe, warum der
   gemessene Wert unter der Roofline-Schranke liegt.
e) Prüfe mit `make regs` (also `-Xptxas -v`) den Register- und Shared-Memory-Verbrauch beider
   Kernel und rechne die Occupancy nach dem Schema aus Aufgabe 12.4 aus.
f) Untersuche im gekachelten Kernel, ob Bankkonflikte auftreten — beide Shared-Memory-Zugriffe
   der inneren Schleife einzeln. Teste, ob `As[TILE][TILE+1]` etwas ändert, und erkläre das
   Ergebnis.
g) Vergleiche mit `cublasSgemm`. Um welchen Faktor bist du langsamer? Nenne drei Techniken,
   die cuBLAS zusätzlich einsetzt.

---

## Aufgabe 12.8 `[Analyse]` — Fehlersuche im Shared-Memory-Kernel

*Zeit: ca. 25 min*

Der folgende Kernel soll die Summe eines `float`-Arrays berechnen. Gestartet wird mit
`blockDim.x = 256` und `blocks = (n + 255) / 256`; `*s` ist vorher auf `0.0f` gesetzt.

```cpp
__global__ void summe(const float* a, float* s, int n) {
    __shared__ float sm[256];
    int tid = threadIdx.x;
    int i   = blockIdx.x * blockDim.x + tid;

    sm[tid] = a[i];

    for (int k = 1; k < blockDim.x; k *= 2) {
        if (tid % (2 * k) == 0) {
            sm[tid] += sm[tid + k];
            __syncthreads();
        }
    }

    if (tid == 0) *s += sm[0];
}
```

a) Finde alle Fehler. Trenne dabei sauber zwischen **Korrektheitsfehlern** (das Ergebnis ist
   falsch) und **Performance-Fehlern** (das Ergebnis stimmt, der Kernel ist langsam). Es sind
   mindestens fünf.
b) Einer der Fehler zeigt sich **nicht**, wenn man mit `blockDim.x = 32` testet, und
   zuverlässig ab `blockDim.x = 64`. Welcher, und warum? Was lernt man daraus über das Testen
   von CUDA-Kernels?
c) Ein weiterer Fehler zeigt sich nur, wenn `n` kein Vielfaches von 256 ist. Welcher, und was
   ist die sauberste Reparatur?
d) Schreibe den Kernel korrekt und effizient auf.
e) Welches Werkzeug hätte welchen der Fehler automatisch gefunden? Ordne jedem Fehler zu:
   `CUDA_CHECK`, `compute-sanitizer`, Vergleich mit einer CPU-Referenz, `ncu`.

---

## Aufgabe 12.9 `[Rechnen]` — Streams

*Zeit: ca. 20 min*

Ein Programm überträgt Daten zur GPU, rechnet und holt das Ergebnis zurück. Gemessen wurde:

| Phase | Zeit |
|---|---|
| Host → Device | 80 ms |
| Kernel | 60 ms |
| Device → Host | 40 ms |

a) Wie lange dauert der Ablauf ohne Streams?
b) Die Arbeit wird in `c` gleich große Stücke zerlegt und über Streams als dreistufige
   Pipeline abgearbeitet (Kopieren hin, Rechnen, Kopieren zurück dürfen gleichzeitig laufen).
   Stelle die Gesamtzeit als Formel in `c` auf. Begründe die Formel — was ist Füllen und
   Leeren der Pipeline, was ist der Fließbetrieb?
c) Werte die Formel für `c = 1, 2, 4, 8, 16` aus und gib jeweils den Speedup an.
d) Gegen welchen Wert läuft die Gesamtzeit für `c → ∞`? Wie groß ist der maximale Speedup, und
   welche Phase bestimmt ihn?
e) Ab welchem `c` erreicht man 90 % des maximalen Speedups? Warum lohnt es sich nicht, `c`
   beliebig groß zu wählen?
f) Ein Kommilitone baut den Stream-Code exakt nach der Vorlesungsfolie, misst aber **exakt
   die Zeit aus a)**. Nenne die beiden wahrscheinlichsten Ursachen und je die Reparatur.
g) Welche Phase sollte man zuerst angehen, wenn man nach der Umstellung auf Streams weiter
   optimieren will? Begründe.

---

## Aufgabe 12.10 `[Klausur]` — Kurzfragen

*Zeit: ca. 20 min*

Je ein bis drei Sätze. Bei Richtig/Falsch ist die Begründung der eigentliche Teil der Antwort.

a) Ein Kernel erreicht 2 % der FP32-Spitzenleistung. Nenne die erste Frage, die man stellt,
   und nicht die naheliegende Schlussfolgerung.
b) Richtig oder falsch: „Wenn ich die Zahl der Rechenoperationen halbiere, wird ein
   speichergebundener Kernel doppelt so schnell."
c) Richtig oder falsch: „Zwei Threads eines Warps, die dieselbe Shared-Memory-Adresse lesen,
   erzeugen einen 2-Wege-Bankkonflikt."
d) Richtig oder falsch: „100 % Occupancy bedeutet, dass die GPU voll ausgelastet ist."
e) Warum ist SoA auf der GPU meist besser als AoS, obwohl auf der CPU oft das Gegenteil gilt?
f) Nenne die beiden Stellen im gekachelten Matrixprodukt, an denen `__syncthreads()` stehen
   muss, und begründe jede einzeln.
g) Warum darf `__syncthreads()` nicht in einem divergenten Zweig stehen?
h) Wieso ist das Ergebnis eines Kernels mit `atomicAdd` auf `float` von Lauf zu Lauf leicht
   verschieden, obwohl der Kernel korrekt ist?
i) Was hat sich ab Volta an der Behandlung divergenter Zweige geändert, und welche
   Konsequenz hat das für den Programmierer?
j) Ein Kernel ist coalesced, hat keine Divergenz, keine Bankkonflikte und 100 % Occupancy —
   und erreicht trotzdem nur 300 GFLOP/s auf einer A100. Ist er schlecht? Begründe.
k) Nenne die ersten drei Punkte der Optimierungs-Checkliste in der richtigen Reihenfolge und
   begründe genau diese Reihenfolge.

---

**Lösungen:** [`loesungen.md`](loesungen.md) — erst danach aufschlagen.
