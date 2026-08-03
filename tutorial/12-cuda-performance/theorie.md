# Kapitel 12 — CUDA II: Performance — den Kernel schnell machen

> **Quellen:** `vl/12-CUDA_Bosse-VL2.pdf` (Folien 1–54),
> `vl/extra-material/cuda_matrix_add_2d_hints.pdf`, Übungsblätter
> `excercises/uebung11.pdf` (Roofline N-Body), `excercises/uebung9.pdf` (Aufgabe 9.2,
> Skalarprodukt mit Shared Memory)
> **Zeitbedarf:** ca. 85–100 min lesen + 150 min Aufgaben
> **Voraussetzungen:** Kapitel 11 (Warps, Grid, Host-Ablauf), Kapitel 10 (Roofline),
> Kapitel 04/05 (Cache, Blocking — das gekachelte Matrixprodukt ist dieselbe Idee)

> **Codekonvention:** CUDA-C++17, `nvcc -std=c++17 -O2 -arch=sm_80`. Rechenkerne roh,
> Infrastruktur modern. Folien-Code ist mit `// Folie N` markiert und behält den C-Stil der
> Vorlesung.

> **Kein `nvcc` auf diesem Rechner.** Der Code in [`code/`](code/) ist für den Ara-Cluster
> geschrieben. Der klausurrelevante Kern — Roofline rechnen, Transaktionen zählen,
> Bankkonflikte bestimmen, Occupancy ausrechnen, Kernel-Fehler finden — ist **Papierarbeit**.

---

## 1. Der Unterschied zu Kapitel 11

Kapitel 11 endete mit einem Kernel, der **korrekt** ist. Dieses Kapitel beantwortet die
Anschlussfrage: *Ist er auch schnell — und woran liegt es, wenn nicht?*

Der Reflex ist meistens falsch. Angenommen, ein Kernel erreicht 250 GFLOP/s auf einer A100
mit 19 500 GFLOP/s Spitzenleistung. Das sind **1,3 % der Rechenleistung**, und die naheliegende
Reaktion wäre, an den Rechenoperationen zu sparen. Es hilft nichts: Der Kernel wartet gar
nicht auf die Recheneinheiten, sondern auf den Speicher — und läuft in Wahrheit bereits am
Anschlag.

Ohne ein Modell, das diesen Unterschied sichtbar macht, optimiert man ins Leere. Deshalb steht
das Roofline-Modell hier ganz am Anfang und nicht am Ende.

> **Die Reihenfolge, in der man vorgeht:**
> 1. Ist der Kernel überhaupt der Engpass? (Profiler — oder wenigstens die Uhr.)
> 2. Wodurch ist er begrenzt — Speicher oder Rechenwerk? (Roofline.)
> 3. Erst dann: die passende Optimierung.
>
> Wer Schritt 2 überspringt, optimiert mit 50 % Wahrscheinlichkeit die falsche Seite.

---

## 2. Die Kernidee in fünf Sätzen

1. Die **arithmetische Intensität** $I$ = FLOP pro aus dem DRAM bewegtem Byte entscheidet,
   ob ein Kernel speicher- oder rechengebunden ist.
2. **Coalescing**: Die 32 Lanes eines Warps sollen zusammenhängende, ausgerichtete Adressen
   treffen — sonst zerfällt ein Zugriff in bis zu 32 Transaktionen.
3. **Shared Memory** erhöht $I$, indem es einmal geladene Daten mehrfach wiederverwendet —
   das ist der einzige Weg, einen speichergebundenen Kernel rechengebunden zu machen.
4. **Occupancy** — genug residente Warps, um Latenz zu verdecken — ist ein *Mittel*, kein
   Ziel.
5. **Divergenz** innerhalb eines Warps serialisiert; **Bankkonflikte** im Shared Memory
   serialisieren ebenfalls. Beides sind Faktoren bis 32.

---

## 3. Das Roofline-Modell auf der GPU

### 3.1 Die zwei Größen und die eine Formel

**Definition.** Die *arithmetische* (oder *operationale*) *Intensität* eines Kernels ist

$$I = \frac{\text{FLOPs}}{\text{aus dem DRAM bewegte Bytes}} \qquad [\text{FLOP/Byte}]$$

Die erreichbare Leistung ist dann nach oben begrenzt durch

$$\boxed{P \le \min\left(P_{\text{peak}},\ B_{\text{peak}} \cdot I\right)}$$

mit der Spitzenrechenleistung $P_{\text{peak}}$ [GFLOP/s] und der Spitzenbandbreite
$B_{\text{peak}}$ [GB/s]. Im doppelt-logarithmischen Diagramm ist das eine Gerade der
Steigung 1, die in eine Waagerechte übergeht — daher der Name.

Der Übergang heißt **Knickpunkt** (*ridge point*):

$$I^\star = \frac{P_{\text{peak}}}{B_{\text{peak}}}$$

| Bereich | Bedingung | Diagnose | Was hilft |
|---|---|---|---|
| links vom Knick | $I < I^\star$ | **speichergebunden** | weniger Bytes bewegen: Coalescing, Wiederverwendung, Shared Memory, SoA, `float` statt `double` |
| rechts vom Knick | $I > I^\star$ | **rechengebunden** | weniger/bessere Operationen: Instruktionsmix, FMA, Tensor-Cores, Bibliotheken |

**Für die A100** (Zahlen wie auf Blatt 11): $P_{\text{peak}} = 19{,}5$ TFLOP/s (FP32),
$B_{\text{peak}} = 1{,}5$ TB/s.

$$I^\star = \frac{19\,500\ \text{GFLOP/s}}{1500\ \text{GB/s}} = \mathbf{13\ \text{FLOP/Byte}}$$

**Diese Zahl sollte man im Kopf haben.** Sie sagt: Ein Kernel muss **13 Rechenoperationen pro
Byte** leisten, damit die Recheneinheiten überhaupt zum Engpass werden. Pro geladenem `float`
(4 Byte) sind das **52 FLOP**. Das erreichen erschreckend wenige Kernel.

> **Die Ernüchterung, die man einmal ausgehalten haben muss:** Die allermeisten realen Kernel
> liegen weit links vom Knick. Die GPU ist als Rechenmaschine gebaut, wird aber fast immer als
> Speichermaschine betrieben.

### 3.2 Ein Beispiel, komplett durchgerechnet: SAXPY

$$y_i \leftarrow a\,x_i + y_i$$

Pro Element:

| | | |
|---|---|---|
| FLOPs | 1 Multiplikation + 1 Addition | **2 FLOP** |
| Bytes | $x_i$ lesen, $y_i$ lesen, $y_i$ schreiben — 3 Wörter à 4 B | **12 Byte** |

$$I = \frac{2}{12} = 0{,}167\ \text{FLOP/Byte}$$

$$P \le \min(19\,500,\ 1500 \cdot 0{,}167) = \min(19\,500,\ 250) = \mathbf{250\ \text{GFLOP/s}}$$

Das sind **1,3 % der Spitzenleistung** — und mehr ist physikalisch nicht drin. Ein Kernel, der
250 GFLOP/s erreicht, ist bei SAXPY **perfekt**. Wer hier an den Rechenoperationen
optimiert, gewinnt exakt nichts.

Was tatsächlich hilft: **weniger Bytes bewegen.** Wenn zwei aufeinanderfolgende
SAXPY-Aufrufe zu einer Operation **verschmolzen** werden (Thrust nennt das `saxpy_functor`),
entfallen ein Schreib- und ein Lesevorgang des Zwischenergebnisses — die Intensität steigt,
ohne dass sich an der Rechnung etwas ändert.

### 3.3 Ein Katalog zum Einordnen

Alle Werte für `float`, A100, $I^\star = 13$:

| Kernel | FLOP/Element | Byte/Element | $I$ | $P_{\max}$ | Klasse |
|---|---|---|---|---|---|
| Kopieren `y=x` | 0 | 8 | 0 | 0 | rein speichergebunden |
| SAXPY | 2 | 12 | 0,17 | 250 GFLOP/s | speichergebunden |
| Skalarprodukt | 2 | 8 | 0,25 | 375 GFLOP/s | speichergebunden |
| 5-Punkt-Stencil, naiv | 4 | 20 | 0,20 | 300 GFLOP/s | speichergebunden |
| 5-Punkt-Stencil, ideal gekachelt | 4 | 8 | 0,50 | 750 GFLOP/s | **immer noch** speichergebunden |
| GEMM naiv | 2 (pro k-Schritt) | 8 | 0,25 | 375 GFLOP/s | speichergebunden |
| GEMM gekachelt, `TILE=32` | — | — | **8** | 12 000 GFLOP/s | knapp speichergebunden |
| GEMM gekachelt, `TILE=64` | — | — | **16** | 19 500 GFLOP/s | **rechengebunden** |

Zwei Lehren stecken in dieser Tabelle:

- Beim **Stencil** verbessert Kacheln die Intensität um Faktor 2,5 — mehr ist strukturell nicht
  möglich, denn jeder Wert muss mindestens einmal gelesen und einmal geschrieben werden. Der
  Kernel bleibt speichergebunden, egal wie clever man ist.
- Beim **GEMM** verschiebt Kacheln den Betriebspunkt über den Knick hinweg. Hier ändert die
  Optimierung die *Klasse* des Kernels. Das ist der Grund, warum GEMM die Fallstudie der
  Vorlesung ist und nicht der Stencil.

Die Rechnung für gekacheltes GEMM (die man können sollte): Bei Kachelgröße $T$ wird jede
Kachel von $A$ und $B$ einmal geladen und von $T$ Threads benutzt. Global bewegt werden
$2N^3/T$ Wörter für $2N^3$ FLOP:

$$I = \frac{2N^3}{4 \cdot 2N^3/T} = \frac{T}{4}\ \text{FLOP/Byte}$$

Für $T = 32$ also 8, für $T = 64$ also 16. **Die Intensität wächst linear mit der
Kachelgröße** — und genau deshalb lohnt sich das Kacheln.

---

## 4. Globaler Speicher: Coalescing

### 4.1 Wie die Hardware liest

Der globale Speicher wird nicht pro Thread bedient, sondern **pro Warp**. Die Hardware
sammelt die 32 Adressen einer Instruktion ein und zerlegt sie in **ausgerichtete
Transaktionen** von 32, 64 oder 128 Byte. Bezahlt wird pro Transaktion — die volle Breite,
auch wenn nur 4 Byte davon gebraucht werden.

$$\text{Effizienz} = \frac{\text{gebrauchte Bytes}}{\text{geholte Bytes}}$$

Mit 32-Byte-Sektoren als Granularität und `float`-Zugriffen (4 B, also 128 gebrauchte Byte
pro Warp):

| Zugriffsmuster | Sektoren | geholt | Effizienz |
|---|---|---|---|
| `a[i]`, `i = base+t`, ausgerichtet | 4 | 128 B | **100 %** — ideal |
| Permutation innerhalb desselben Segments | 4 | 128 B | 100 % — auf modernen GPUs unproblematisch |
| `a[i+1]` (um 4 B fehlausgerichtet) | 5 | 160 B | 80 % |
| `a[2*i]` (Schrittweite 2) | 8 | 256 B | 50 % |
| `a[8*i]` | 32 | 1024 B | 12,5 % |
| `a[32*i]` | 32 | 1024 B | 12,5 % (Sättigung: mehr als 32 Sektoren gibt es nicht) |
| alle lesen `a[0]` | 1 | 32 B | Broadcast — kein Problem |

**Faustregel:** Thread `t` soll auf `base + t` zugreifen — Schrittweite 1 über den Warp.

Genau das leistet die Standardformel `i = blockIdx.x*blockDim.x + threadIdx.x` automatisch, und
genau das ist der tiefere Grund, warum der Grid-Stride-Loop aus Kapitel 11 die richtige Form
hat: Auch in seinem zweiten Durchlauf bleiben die 32 Lanes benachbart.

Und es ist der Grund, warum in Kapitel 11 die `.x`-Dimension die **Spalte** sein musste: Bei
Row-Major liegen benachbarte Spalten benachbart im Speicher, benachbarte Zeilen dagegen
`cols · 4` Byte auseinander.

### 4.2 AoS vs. SoA — eine Zeile, ein Faktor 8

```cpp
// Folie 15 — Original der Vorlesung (C-Stil)

/* Array of Structs (AoS) */          /* Struct of Arrays (SoA) */
struct P { float x, y, z; };          struct P { float x[N], y[N], z[N]; };
P part[N];                            P part;
// Thread t liest part[t].x           // Thread t liest part.x[t]
// -> x-Werte mit Schritt 3           // -> zusammenhaengend
//    -> strided!                     //    -> coalesced!
```

Bei **AoS** liegen die `x`-Werte 12 Byte auseinander: Ein Warp berührt 384 Byte, um 128 zu
benutzen — 12 Sektoren statt 4, Effizienz 33 %. Bei **SoA** sind es 4 Sektoren, Effizienz
100 %.

Es sind dieselben Daten und dieselbe Rechnung. Nur das Layout ist anders — und der Kernel ist
dreimal schneller.

> **Auf der GPU ist SoA fast immer das bessere Layout.** Auf der CPU ist die Antwort weniger
> eindeutig: Braucht ein Kern *alle drei* Komponenten eines Partikels, holt AoS sie in einer
> Cache-Zeile. Auf der GPU zählt dagegen, was die 32 Lanes **gleichzeitig** anfassen — und das
> ist dieselbe Komponente von 32 verschiedenen Partikeln.

Das ist derselbe Perspektivwechsel wie beim Grid-Stride-Loop in Kapitel 11: Lokalität über die
Zeit (CPU) gegen Lokalität über die Threads (GPU).

---

## 5. Divergenz, genauer betrachtet

Die Grundlage steht in Kapitel 11: Nehmen Threads *desselben* Warps verschiedene Pfade,
serialisiert die Hardware sie. Zwei Ergänzungen gehören zu diesem Kapitel.

### 5.1 Vor Volta und ab Volta

**Bis Pascal** hatte ein Warp **einen** Programmzähler. Divergente Zweige wurden ausgeführt und
liefen am nächsten *Reconvergence Point* zwangsweise wieder zusammen. Ein Thread konnte nicht
auf einen anderen desselben Warps warten — jedes Spin-Lock zwischen zwei Lanes war ein
garantierter Deadlock.

**Ab Volta** hat jeder Thread einen **eigenen Programmzähler**. Threads desselben Warps können
unabhängig Fortschritt machen; feingranulare Synchronisation zwischen Lanes wird möglich.

Der Haken: Die Threads laufen jetzt **nicht mehr automatisch wieder zusammen**. Wer nach einem
divergenten Abschnitt darauf baut, dass der Warp wieder im Gleichschritt ist — etwa beim
Austausch über Register oder Shared Memory —, muss das explizit sagen:

```cpp
__syncwarp();                 // Barriere fuer die Lanes eines Warps
__shfl_down_sync(0xffffffff, val, 16);   // die Maske benennt die Teilnehmer
```

Deshalb tragen alle modernen Warp-Primitive das Suffix `_sync` und eine **Maske**: Sie
verlangen die Angabe, welche Lanes teilnehmen. Die alten maskenlosen Varianten (`__shfl`,
`__any`) sind abgekündigt.

### 5.2 Was Divergenz nicht ist

Zwei Verwechslungen, die regelmäßig vorkommen:

- **Verschiedene Warps** dürfen beliebig auseinanderlaufen. Das kostet nichts.
- Ein `if`, dessen Bedingung im Warp **einheitlich** ist, kostet nichts — auch wenn es
  datenabhängig ist. Entscheidend ist nicht, ob verzweigt wird, sondern ob die 32 Lanes sich
  uneinig sind.

---

## 6. Shared Memory

### 6.1 Was es ist und wozu

Shared Memory ist ein **software-verwalteter Scratchpad** auf dem SM: schnell wie ein L1-Cache,
aber explizit beschrieben und gelesen. Er gehört **einem Block** und lebt so lange wie dieser.

```cpp
__global__ void kernel(...) {
    __shared__ float sm[256];        // statisch, Groesse zur Uebersetzungszeit
    ...
}

extern __shared__ float sm[];        // dynamisch:
kernel<<<blocks, threads, 256*sizeof(float)>>>(...);   // Groesse beim Start
```

Der Zweck ist immer derselbe: **eine Kachel globaler Daten einmal einlagern und dann vielfach
wiederverwenden.** Das senkt die aus dem DRAM bewegten Bytes und hebt damit die arithmetische
Intensität — der einzige Hebel, der einen speichergebundenen Kernel nach rechts über den
Knickpunkt schieben kann.

### 6.2 Die Regel, an der alles hängt

**Nach dem Schreiben in Shared Memory und vor dem Lesen durch andere Threads muss
`__syncthreads()` stehen.** Ohne die Barriere liest ein Thread möglicherweise einen Platz, den
sein Nachbar noch nicht beschrieben hat — eine klassische Race Condition, die bei kleinen
Blöcken (ein Warp!) sogar zufällig funktionieren kann und ab zwei Warps zuverlässig falsche
Ergebnisse liefert.

Und die Gegenregel: **`__syncthreads()` darf nicht in einem divergenten Zweig stehen.** Alle
Threads des Blocks müssen die Barriere erreichen.

```cpp
if (tid < s) {
    sm[tid] += sm[tid + s];
    __syncthreads();      // FALSCH -- die Threads mit tid >= s kommen nie an
}

if (tid < s)
    sm[tid] += sm[tid + s];
__syncthreads();          // richtig
```

### 6.3 Die Block-Reduktion

Der Standardbaustein — eine Baumreduktion in $\log_2(\text{blockDim})$ Schritten:

```cpp
// Folie 22 — Original der Vorlesung (C-Stil)
__global__ void smreduce(const float *a, float *b) {
    int tid = threadIdx.x;
    int id  = tid + blockIdx.x * blockDim.x;
    __shared__ float sm[512];
    sm[tid] = a[id];
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s /= 2) {
        if (tid < s) sm[tid] += sm[tid + s];
        __syncthreads();      // jeder Thread muss hier ankommen
    }
    if (tid == 0) b[blockIdx.x] = sm[0];
}
```

Drei Punkte, die man an diesem Muster verstanden haben muss:

1. **`s` halbiert sich**, die Zahl der aktiven Threads ebenso. Bei 256 Threads sind es
   8 Schritte statt 255 sequentieller Additionen — $O(\log n)$ Tiefe statt $O(n)$ (Kapitel 07).
2. **`if (tid < s)`** ist die divergenzarme Variante. Die naive Alternative
   `if (tid % (2*s) == 0)` rechnet dasselbe, verteilt die aktiven Threads aber *über alle
   Warps*: In jedem Schritt ist jeder Warp teilweise aktiv. Bei `tid < s` sind dagegen die
   ersten Warps voll aktiv und der Rest komplett inaktiv — **kein einziger divergenter Warp**,
   solange $s \ge 32$.
3. **Der letzte Schritt** schreibt ein Ergebnis **pro Block**. Für ein einzelnes Gesamtergebnis
   braucht es danach entweder einen zweiten Kernel oder ein `atomicAdd` (Abschnitt 8).

---

## 7. Bankkonflikte

### 7.1 Das Modell

Shared Memory ist in **32 Bänke** aufgeteilt. Aufeinanderfolgende 32-Bit-Wörter liegen in
aufeinanderfolgenden Bänken:

$$\text{Bank}(w) = w \bmod 32$$

Eine Bank liefert grob **ein Wort pro Takt**. Greifen zwei oder mehr Threads eines Warps auf
**verschiedene Wörter derselben Bank** zu, werden die Zugriffe **serialisiert** — das ist ein
*Bankkonflikt*. Ein $k$-Wege-Konflikt kostet Faktor $k$.

**Die Ausnahme:** Lesen mehrere Threads **dasselbe Wort**, macht die Hardware einen
**Broadcast**. Das ist konfliktfrei — verschiedene Wörter derselben Bank sind das Problem, nicht
dieselbe Adresse.

### 7.2 Die Regel für Schrittweiten

Bei `sm[s * threadIdx.x]` mit Schrittweite $s$ ist der Konfliktgrad

$$k = \gcd(s, 32)$$

| $s$ | $\gcd(s,32)$ | Konflikt |
|---|---|---|
| 1 | 1 | konfliktfrei |
| 2 | 2 | 2-Wege |
| 3 | 1 | **konfliktfrei** — jede ungerade Schrittweite ist es |
| 4 | 4 | 4-Wege |
| 16 | 16 | 16-Wege |
| 32 | 32 | **32-Wege** — der schlimmste Fall |

Merkhilfe: **Ungerade Schrittweiten sind immer konfliktfrei**, weil ungerade Zahlen teilerfremd
zu 32 sind. Genau darauf beruht die Standardabhilfe.

### 7.3 Padding

Der klassische Fall — ein quadratisches Tile, das **spaltenweise** gelesen wird:

```cpp
// Folie 27 — Original der Vorlesung (C-Stil)
__shared__ float tile[32][32];       // Spaltenzugriff -> 32-Wege-Konflikt
__shared__ float tile[32][32 + 1];   // +1 Padding    -> konfliktfrei
```

Warum das funktioniert, sollte man vorrechnen können. Bei `tile[32][32]` hat `tile[r][c]` den
Wortindex $32r + c$. Für eine feste Spalte $c$ und $r = 0 \ldots 31$:

$$\text{Bank} = (32r + c) \bmod 32 = c \quad \text{für alle } r$$

**Alle 32 Threads treffen dieselbe Bank** und lesen dabei *verschiedene* Wörter — also der
volle 32-Wege-Konflikt, Faktor 32.

Mit `tile[32][33]` ist der Wortindex $33r + c$:

$$\text{Bank} = (33r + c) \bmod 32 = (r + c) \bmod 32$$

Für $r = 0 \ldots 31$ durchläuft das **alle 32 Bänke genau einmal** — konfliktfrei. Die eine
zusätzliche Spalte verschiebt jede Zeile um genau eine Bank.

**Kosten:** 32 zusätzliche `float` = 128 Byte pro Tile. **Nutzen:** Faktor 32 auf den
Spaltenzugriffen. Das ist die billigste Optimierung in ganz CUDA.

> Dieselbe Idee wie das **Padding gegen false sharing** aus Kapitel 06: Man verschiebt Daten
> um ein Element, damit sie nicht mehr auf dieselbe Hardware-Ressource fallen. Dort war es die
> Cache-Zeile, hier die Bank.

---

## 8. Occupancy

### 8.1 Definition und Zweck

$$\text{Occupancy} = \frac{\text{aktive Warps pro SM}}{\text{maximale Warps pro SM}}$$

Der Sinn ist genau der aus Kapitel 11: Latenz wird verdeckt, indem viele residente Warps zum
Umschalten bereitstehen. Wartet einer auf den globalen Speicher, rechnet ein anderer weiter.

Die Zahl der residenten Blöcke pro SM ist durch die **knappste Ressource** begrenzt:

| Ressource | Limit auf A100 (CC 8.0) |
|---|---|
| Warps pro SM | 64 (= 2048 Threads) |
| Blöcke pro SM | 32 |
| Register pro SM | 65 536 (32-Bit) |
| Shared Memory pro SM | 164 kB nutzbar |
| Threads pro Block | 1024 |

### 8.2 Eine Occupancy-Rechnung

Das Schema ist immer dasselbe: **für jede Ressource ausrechnen, wie viele Blöcke passen; das
Minimum gewinnt.**

Beispiel: `blockDim = 256`, 32 Register pro Thread, 8 kB Shared Memory pro Block.

| Grenze | Rechnung | Blöcke/SM |
|---|---|---|
| Register | 65 536 / (256 · 32) = 65 536 / 8192 | 8 |
| Shared Memory | 164 kB / 8 kB | 20 |
| Warps | 64 / (256/32) = 64 / 8 | 8 |
| Blöcke | — | 32 |

Minimum: **8 Blöcke** = 8 · 8 = 64 Warps = **100 % Occupancy**.

Jetzt derselbe Kernel mit **64 Registern pro Thread** (etwa weil eine Schleife entrollt wurde):

$$65\,536 / (256 \cdot 64) = 4\ \text{Blöcke} = 32\ \text{Warps} = \mathbf{50\,\%}$$

Ein einziges `#pragma unroll` kann die Occupancy halbieren. Deshalb `nvcc -Xptxas -v`: Es zeigt
Register und Shared Memory pro Kernel — die beiden Zahlen, die man für diese Rechnung braucht.

### 8.3 Der Zielkonflikt — und warum Occupancy kein Ziel ist

Mehr Register pro Thread oder mehr Shared Memory pro Block bedeuten **weniger residente
Blöcke**. Umgekehrt zwingt eine hohe Occupancy zu sparsamen Kernels.

Die Auflösung: **Occupancy ist ein Mittel, kein Ziel.** Latenz lässt sich auf zwei Wegen
verdecken:

- **Thread-Parallelität (TLP)** — viele Warps, die abwechselnd laufen. Das misst die Occupancy.
- **Instruktionsparallelität (ILP)** — jeder Thread hat mehrere unabhängige Ladeoperationen
  gleichzeitig unterwegs. Das misst sie *nicht*.

Ein Kernel mit 25 % Occupancy, der pro Thread vier unabhängige Werte bearbeitet, kann die
Hardware genauso auslasten wie einer mit 100 % und einem Wert. Ab etwa 50 % ist der Zugewinn
meistens gering. **Die Zielgröße bleibt die erreichte Bandbreite bzw. GFLOP/s**, nicht die
Occupancy.

---

## 9. Warp-Primitive und Atomics

### 9.1 Warp-Shuffle

Die Lanes eines Warps können **direkt Register austauschen** — ohne Shared Memory, ohne
`__syncthreads()`:

```cpp
// Folie 33 — Original der Vorlesung (C-Stil)
__inline__ __device__ float warpReduceSum(float val) {
    for (int offset = 16; offset > 0; offset /= 2)
        val += __shfl_down_sync(0xffffffff, val, offset);
    return val;      // Lane 0 enthaelt die Summe des Warps
}
```

`__shfl_down_sync(mask, val, offset)` liefert jeder Lane `l` den Wert `val` von Lane
`l + offset`. Fünf Schritte (16, 8, 4, 2, 1) reduzieren 32 Werte auf einen. Die Maske
`0xffffffff` benennt alle 32 Lanes als Teilnehmer.

Vorteile gegenüber der Shared-Memory-Reduktion: kein Shared Memory belegt (bessere Occupancy),
keine Barriere, keine Bankkonflikte. Das übliche Muster für eine Block-Reduktion ist deshalb
**zweistufig**: erst je Warp per Shuffle, dann die (höchstens 32) Warp-Ergebnisse über Shared
Memory.

### 9.2 Atomics

Unteilbares Read-Modify-Write über Threads hinweg: `atomicAdd`, `atomicMax`, `atomicCAS`, …

```cpp
// Folie 34 — Original der Vorlesung (C-Stil)
__global__ void maxArray(const int *arr, int *maxVal, int size) {
    int id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id < size) atomicMax(maxVal, arr[id]);
}
```

Drei Dinge, die dazugehören:

1. **Contention ist das Problem, nicht die Korrektheit.** Treffen viele Threads dieselbe
   Adresse, serialisiert die Hardware sie. Ein `atomicAdd` pro Thread auf *eine* globale
   Variable ist der langsamste denkbare Weg, eine Summe zu bilden.
2. **Deshalb: erst lokal reduzieren, dann ein Atomic pro Block.** Aus $n$ Atomics werden
   $n/\text{blockDim}$ — bei 256 Threads Faktor 256.
3. **Gleitkomma-`atomicAdd` ist nicht assoziativ.** Die Reihenfolge der Additionen ist
   nichtdeterministisch, also unterscheiden sich die letzten Bits von Lauf zu Lauf. Ein Test
   auf `==` gegen eine CPU-Referenz schlägt zu Recht fehl — es braucht eine relative Toleranz.

Dasselbe Muster beim **Histogramm**:

```cpp
// Folie 35 — Original der Vorlesung (C-Stil)
__global__ void histogram(const unsigned char *data, int n, unsigned int *bins) {
    int i      = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (; i < n; i += stride)
        atomicAdd(&bins[data[i]], 1u);      // viele Threads, ein Bin
}
```

Korrekt, aber bei schiefer Verteilung langsam. Die Optimierung ist wieder dieselbe: pro Block
ein **privates Histogramm im Shared Memory** aufbauen und am Ende einmal in das globale
addieren. Das ist strukturell exakt die `reduction`-Klausel aus Kapitel 06 — private Kopie pro
Team, eine Zusammenführung am Ende.

---

## 10. Fallstudie: das gekachelte Matrixprodukt

### 10.1 Naiv

```cpp
// Folie 37 — Original der Vorlesung (C-Stil)
__global__ void MatMul(const float *A, const float *B, float *C, int N) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < N && col < N) {
        float acc = 0.0f;
        for (int k = 0; k < N; ++k)
            acc += A[row*N + k] * B[k*N + col];
        C[row*N + col] = acc;
    }
}
```

Korrekt — und speichergebunden. Jedes Element von `A` und `B` wird **N-mal** aus dem globalen
Speicher gelesen. Pro k-Schritt: 2 FLOP, 8 Byte, also $I = 0{,}25$ und höchstens 375 GFLOP/s
von 19 500.

### 10.2 Gekachelt

```cpp
// Folie 39 — Original der Vorlesung (C-Stil)
__global__ void MatMulTiled(const float *A, const float *B, float *C, int N) {
    __shared__ float As[TILE][TILE];
    __shared__ float Bs[TILE][TILE];
    int row = blockIdx.y*TILE + threadIdx.y;
    int col = blockIdx.x*TILE + threadIdx.x;
    float acc = 0.0f;
    for (int m = 0; m < N/TILE; ++m) {
        As[threadIdx.y][threadIdx.x] = A[row*N + (m*TILE + threadIdx.x)];
        Bs[threadIdx.y][threadIdx.x] = B[(m*TILE + threadIdx.y)*N + col];
        __syncthreads();                     // Kachel geladen
        for (int k = 0; k < TILE; ++k)
            acc += As[threadIdx.y][k] * Bs[k][threadIdx.x];
        __syncthreads();                     // vor dem Neuladen
    }
    C[row*N + col] = acc;
}
```

**Die beiden `__syncthreads()` sind beide nötig, und aus verschiedenen Gründen:**

- Das **erste** stellt sicher, dass die Kachel vollständig geladen ist, bevor irgendein Thread
  daraus liest.
- Das **zweite** stellt sicher, dass alle Threads mit der Kachel fertig sind, bevor sie im
  nächsten Durchlauf überschrieben wird. Wer es weglässt, bekommt einen Fehler, der bei einem
  Warp pro Block nicht auftritt und ab zwei Warps zuschlägt — genau die Sorte, die man nächtelang
  sucht.

**Was gewonnen wird:** Jedes geladene Element wird von `TILE` Threads benutzt statt von einem.
Der globale Verkehr sinkt um Faktor `TILE`, die Intensität steigt auf $T/4$ (Abschnitt 3.3).
Bei `TILE = 32`: von $I = 0{,}25$ auf $I = 8$ — Faktor 32, und der Betriebspunkt wandert auf
der Roofline nach rechts fast bis an den Knick.

Die Fassung oben setzt `N % TILE == 0` voraus (der Wächter fehlt in den Kachel-Ladezeilen) —
in der Klausur ist das in Ordnung, im eigenen Code nicht.

**Und die Bankkonflikte?** `As[threadIdx.y][k]`: Für festes `k` variiert `threadIdx.y`
innerhalb eines Warps nicht (bei `TILE = 32` hat ein Warp konstantes `y`) — alle Lanes lesen
**dasselbe Wort**, also Broadcast, konfliktfrei. `Bs[k][threadIdx.x]`: aufeinanderfolgende
`threadIdx.x`, also aufeinanderfolgende Bänke — ebenfalls konfliktfrei. Dieser Kernel braucht
kein Padding; nötig wird es, sobald man ein Tile **transponiert** liest.

> **Das ist dieselbe Idee wie das Cache-Blocking aus Kapitel 05**, nur mit einem Unterschied:
> Dort *hofft* man, dass die Kachel im Cache bleibt. Hier legt man sie **explizit** hinein.
> Shared Memory ist ein Cache, dessen Belegung man selbst bestimmt.

---

## 11. Streams: Kopieren und Rechnen überlappen

Der versteckte Engpass aus Kapitel 11 war PCIe. Solange `cudaMemcpy` synchron ist, läuft alles
streng nacheinander:

```
[--- H->D ---][--- Kernel ---][- D->H -]
```

Ein **Stream** ist eine Warteschlange: Innerhalb eines Streams bleibt die Reihenfolge erhalten,
**verschiedene Streams dürfen gleichzeitig laufen**. Da die GPU getrennte Kopier- und
Rechen-Engines hat, lässt sich die Zeitachse füllen:

```cpp
// Folie 44 — Original der Vorlesung (C-Stil)
cudaStream_t s[2];
cudaStreamCreate(&s[0]); cudaStreamCreate(&s[1]);
for (int c = 0; c < chunks; ++c) {
    int k = c & 1;                                   // Stream abwechseln
    cudaMemcpyAsync(d+off, h+off, bytes, cudaMemcpyHostToDevice, s[k]);
    kernel<<<grid, threads, 0, s[k]>>>(d+off, ...);
    cudaMemcpyAsync(h+off, d+off, bytes, cudaMemcpyDeviceToHost, s[k]);
}
```

```
Stream 0:  [H->D 0][Kern 0][D->H 0]        [H->D 2][Kern 2]...
Stream 1:          [H->D 1][Kern 1][D->H 1]        [H->D 3]...
```

**Zwei Voraussetzungen, ohne die nichts überlappt:**

1. **Pinned (page-locked) Host-Speicher** über `cudaMallocHost` / `cudaHostAlloc`. Nur dann
   kann der Treiber per DMA kopieren; mit gewöhnlichem `malloc`-Speicher ist
   `cudaMemcpyAsync` in Wahrheit synchron. Das ist der häufigste Grund, warum Streams
   „nichts bringen".
2. **Der vierte Parameter der Ausführungskonfiguration** `<<<grid, threads, smBytes, stream>>>`
   — ohne ihn landet der Kernel im Default-Stream und serialisiert alles.

Die Obergrenze des Gewinns ist die längste Einzelphase: Bei 80 ms Kopieren hin, 60 ms Rechnen
und 40 ms zurück sinkt die Gesamtzeit von 180 ms bestenfalls auf 80 ms, also Faktor 2,25.
**Streams verstecken die Übertragung, sie beseitigen sie nicht** (Aufgabe 12.9).

---

## 12. Bibliotheken und Werkzeuge

### 12.1 Das Rad nicht neu erfinden

| Bibliothek | wofür |
|---|---|
| **cuBLAS** | dichte lineare Algebra (GEMM, GEMV) — nutzt Tensor-Cores und architekturspezifische Kachelung |
| **cuSPARSE, cuSOLVER, cuFFT, cuRAND** | dünnbesetzt, Löser, FFT, Zufallszahlen |
| **Thrust** | STL-ähnliche parallele Algorithmen (`device_vector`, `transform`, `reduce`, `sort`, Scans) |
| **CUB** | Bausteine: Block- und Warp-Reduktionen, Scans |

**Faustregel: vor dem eigenen Kernel zur Bibliothek greifen.** cuBLAS bei GEMM zu schlagen
gelingt praktisch nie — der eigene gekachelte Kernel dient dem *Verständnis*, nicht der
Produktion.

### 12.2 Debuggen und Profilen

| Werkzeug | wofür |
|---|---|
| `nvcc -g -G` | Device-Debug-Symbole |
| `cuda-gdb` | Breakpoints, Einzelschritt, Zustand pro Thread |
| `compute-sanitizer` | Zugriffe außerhalb der Grenzen, Fehlausrichtung, Race Conditions (ersetzt `cuda-memcheck`) |
| `nsys` (Nsight Systems) | Zeitstrahl: Kopien vs. Kernel vs. CPU — **hier fängt man an** |
| `ncu` (Nsight Compute) | pro Kernel: Occupancy, Speicherdurchsatz, Stall-Gründe, Roofline |

`compute-sanitizer` ist das CUDA-Gegenstück zum ThreadSanitizer aus Kapitel 06 und findet
genau die Fehler, die ein `CUDA_CHECK` **nicht** findet.

### 12.3 Die Optimierungs-Checkliste

Die Reihenfolge ist nicht beliebig — sie geht von „größter Hebel" nach „kleinster":

1. Ist der Kernel überhaupt der Engpass? (Zuerst `nsys` — oft ist es der Transfer.)
2. Sind die globalen Zugriffe **coalesced**? Passt das Datenlayout (SoA)?
3. Werden wiederverwendbare Daten im **Shared Memory** eingelagert? Gibt es Bankkonflikte?
4. Gibt es **Divergenz** innerhalb von Warps?
5. Ist die **Occupancy** hoch genug, um Latenz zu verdecken?
6. Speicher- oder rechengebunden? (**Roofline**, `ncu`.)
7. Überlappen Kopien und Rechnung (**Streams**)?
8. Könnte eine **Bibliothek** es besser?

---

## 13. Typische Klausurfragen

- **Definiere die arithmetische Intensität und die Roofline-Schranke.** — $I$ = FLOP pro aus
  dem DRAM bewegtem Byte; $P \le \min(P_{\text{peak}}, B_{\text{peak}} \cdot I)$.
- **Was ist der Knickpunkt, wie berechnet man ihn?** — $I^\star = P_{\text{peak}}/B_{\text{peak}}$;
  er trennt speicher- von rechengebunden. A100: 13 FLOP/Byte.
- **Rechne die Intensität von SAXPY aus.** — 2 FLOP / 12 Byte = 0,17 → tief speichergebunden.
- **Wann ist ein Zugriff coalesced?** — Wenn die 32 Threads eines Warps ein oder wenige
  zusammenhängende, ausgerichtete Segmente treffen. Faustregel: Thread `t` → Element `base+t`.
- **AoS oder SoA auf der GPU, und warum?** — SoA: Die Threads eines Warps greifen auf
  benachbarte Elemente **desselben** Feldes zu; bei AoS liegen sie `sizeof(struct)` auseinander.
- **Was ist ein Bankkonflikt, wann tritt er auf, was hilft?** — Zwei Threads eines Warps
  greifen auf *verschiedene Wörter derselben* Bank zu → serialisiert. Bei Schrittweite $s$ ist
  der Grad $\gcd(s,32)$. Abhilfe: Padding (`[32][33]`).
- **Warum ist `tile[32][32]` bei Spaltenzugriff schlecht?** — Wortindex $32r+c$, Bank
  $\equiv c$ für alle $r$ → 32-Wege-Konflikt. Mit `[33]` wird die Bank $(r+c) \bmod 32$.
- **Definiere Occupancy und nenne die begrenzenden Ressourcen.** — Aktive/maximale Warps pro
  SM; begrenzt durch Register, Shared Memory und Hardware-Limits.
- **Rechne die Occupancy aus für …** — Für jede Ressource die Blockzahl bestimmen, Minimum
  nehmen, mal Warps pro Block, geteilt durch 64.
- **Warum ist hohe Occupancy nicht immer das Ziel?** — Sie ist ein Mittel zum Latency Hiding;
  Instruktionsparallelität leistet dasselbe. Zielgröße ist die erreichte Bandbreite.
- **Wo müssen im gekachelten GEMM die `__syncthreads()` stehen und warum je?** — Nach dem
  Laden (Kachel vollständig) und nach dem Rechnen (bevor überschrieben wird).
- **Wie summiert man effizient über ein ganzes Array?** — Baumreduktion im Shared Memory
  (`if (tid < s)`) oder per Warp-Shuffle, dann **ein** `atomicAdd` pro Block.
- **Warum ist Gleitkomma-`atomicAdd` nicht reproduzierbar?** — Nichtdeterministische
  Reihenfolge, Gleitkommaaddition ist nicht assoziativ.
- **Was braucht man, damit Streams tatsächlich überlappen?** — Pinned Host-Speicher,
  `cudaMemcpyAsync`, verschiedene Streams, Stream-Argument beim Kernel-Start.
- **Ab Volta: was hat sich bei der Divergenz geändert?** — Eigener Programmzähler pro Thread;
  keine automatische Reconvergence mehr, deshalb `__syncwarp()` und `_sync`-Primitive mit Maske.

---

## 14. Fallstricke

| Fehler | Warum falsch | Richtig |
|---|---|---|
| Rechenoperationen optimieren, ohne die Roofline zu prüfen | bei $I \ll I^\star$ wartet der Kernel auf Speicher, nicht auf ALUs | erst einordnen, dann optimieren |
| Bytes „im Cache" nicht mitzählen | $I$ zählt die aus dem **DRAM** bewegten Bytes | Wiederverwendung explizit berücksichtigen |
| AoS für Partikel/Vektoren | Zugriffe mit `sizeof(struct)`-Schritt, bis 3× mehr Transaktionen | SoA |
| `.y` als Spalte in 2D-Kernels | Warp läuft über `.x` → Zugriffe mit Zeilenabstand | `.x` = Spalte |
| `__syncthreads()` nach dem Schreiben vergessen | Race: Thread liest, was der Nachbar noch nicht geschrieben hat | Barriere zwischen Schreiben und Lesen |
| zweites `__syncthreads()` in der Kachelschleife vergessen | die Kachel wird überschrieben, während andere noch lesen | Barriere auch am Schleifenende |
| `__syncthreads()` in divergentem `if` | nicht alle Threads erreichen die Barriere → undefiniert/Hänger | vor das `if` ziehen |
| `if (tid % (2*s) == 0)` in der Reduktion | aktive Threads über alle Warps verteilt → jeder Warp divergent | `if (tid < s)` |
| `tile[32][32]` spaltenweise gelesen | 32-Wege-Bankkonflikt | `tile[32][33]` |
| ein `atomicAdd` pro Thread auf eine Adresse | volle Serialisierung | erst im Block reduzieren, dann eines pro Block |
| Gleitkommaergebnis auf `==` prüfen | Atomics/Reduktionen ändern die Reihenfolge | relative Toleranz |
| Occupancy als Zielgröße | verdeckt, dass ILP dasselbe leistet | GFLOP/s bzw. GB/s messen |
| `cudaMemcpyAsync` mit `malloc`-Speicher | ohne Pinning ist die Kopie faktisch synchron | `cudaMallocHost` |
| Kernel ohne Stream-Argument in „Stream-Code" | landet im Default-Stream und serialisiert alles | `<<<g, t, 0, stream>>>` |
| `N % TILE != 0` beim gekachelten GEMM | die Ladezeilen haben keinen Wächter → Zugriff außerhalb | Wächter oder Auffüllen |

---

## 15. Merkkasten

> **Kernaussagen**
> - **Erst messen, dann optimieren.** Roofline und Profiler sagen, welche Seite überhaupt der
>   Engpass ist.
> - $I = \text{FLOP} / \text{DRAM-Byte}$, $P \le \min(P_{\text{peak}}, B_{\text{peak}} I)$,
>   Knickpunkt $I^\star = P_{\text{peak}}/B_{\text{peak}}$ — **A100: 13 FLOP/Byte**.
> - Die meisten Kernel sind **speichergebunden**. Dann hilft nur, Bytes zu sparen.
> - **Coalescing**: Thread `t` → Element `base + t`. Schrittweite und Fehlausrichtung kosten
>   bis Faktor 8; **SoA statt AoS**.
> - **Shared Memory** ist der einzige Hebel, der die Intensität wirklich erhöht — er macht aus
>   Datenwiederverwendung eingesparte DRAM-Bytes.
> - Nach dem Schreiben in Shared Memory: `__syncthreads()`. Nie in einem divergenten Zweig.
> - **Bankkonflikt** = verschiedene Wörter derselben Bank; Grad $\gcd(s,32)$; Abhilfe Padding.
>   Dasselbe Wort ist Broadcast und kostet nichts.
> - **Occupancy** = aktive/maximale Warps pro SM, begrenzt durch Register und Shared Memory —
>   Mittel zum Latency Hiding, nicht Ziel.
> - **Atomics**: erst lokal reduzieren, dann eines pro Block. Gleitkomma ist nicht
>   reproduzierbar.
> - **Streams** überlappen Transfer und Rechnung — nur mit **pinned** Speicher.
> - Vor dem eigenen Kernel: **cuBLAS/Thrust** prüfen.

---

## 16. Verbindung zum Rest der Vorlesung

**Setzt voraus:** Kapitel 11 (Warps, Grid, Speicherhierarchie, Host-Ablauf) und Kapitel 10
(Roofline, arithmetische Intensität — hier nur auf GPU-Zahlen angewandt).

**Ist die GPU-Fassung von Kapitel 05:** Das gekachelte Matrixprodukt ist Cache-Blocking mit
explizit verwaltetem Cache. Die I/O-Komplexitätsanalyse aus dem idealen Cache-Modell überträgt
sich direkt — nur heißt der „Cache" hier Shared Memory und seine Größe steht im Quelltext.

**Ist die GPU-Fassung von Kapitel 06:** Die Block-Reduktion ist `reduction(+:s)`, das
private Shared-Memory-Histogramm ist die private Kopie pro Team, und Padding gegen
Bankkonflikte ist Padding gegen false sharing. Dieselben Probleme, andere Hardware-Ressource.

**Blatt 11** verlangt genau die Analyse aus diesem Kapitel für eine N-Body-Simulation: FLOPs
pro Interaktion zählen, Verkehr des naiven und des gekachelten Kernels bestimmen, $I$ und $P$
berechnen, den Knickpunkt einzeichnen und die horizontale Verschiebung des Betriebspunkts
beschreiben. Aufgabe 12.6 hier ist dieselbe Rechnung an einem anderen Kernel. Die
Barnes-Hut-Variante desselben Problems steht in Kapitel 08 — dort wird die Komplexität von
$O(n^2)$ auf $O(n \log n)$ gesenkt, statt die Konstante zu verbessern.

---

**Weiter:** [Übungen](uebungen.md) → danach [Lösungen](loesungen.md)
