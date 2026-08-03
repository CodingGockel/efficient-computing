# Kapitel 12 — Lösungen: CUDA II (Performance)

> Erst [`uebungen.md`](uebungen.md) selbst bearbeiten.
>
> A100: $P_{\text{peak}} = 19\,500$ GFLOP/s (FP32), $B_{\text{peak}} = 1500$ GB/s.

---

## Lösung 12.1 — Roofline: einordnen

**a) Der Knickpunkt.**

$$I^\star = \frac{P_{\text{peak}}}{B_{\text{peak}}} = \frac{19\,500\ \text{GFLOP/s}}{1500\ \text{GB/s}} = \mathbf{13\ \text{FLOP/Byte}}$$

Er trennt die beiden Bereiche: Links davon ($I < 13$) ist der Kernel **speichergebunden**,
rechts **rechengebunden**. Pro geladenem `float` (4 Byte) entspricht das

$$13 \cdot 4 = \mathbf{52\ \text{FLOP}}$$

Das ist die Zahl, die die Ernüchterung liefert: Ein Kernel muss über 50 Operationen mit jedem
geladenen Wert anstellen, bevor die Recheneinheiten überhaupt zum Engpass werden.

**b)–(e)**

| Kernel | FLOP | Byte | $I$ | $P = B \cdot I$ | Anteil an $P_{\text{peak}}$ | Klasse |
|---|---|---|---|---|---|---|
| (b) `y[i] = x[i]` | 0 | 8 (1 lesen, 1 schreiben) | 0 | — | 0 % | reiner Bandbreitentest |
| (c) `d = a*b*c` | 2 | 16 (3 lesen, 1 schreiben) | 0,125 | 187,5 GFLOP/s | **0,96 %** | speichergebunden |
| (d) `s += (x-y)²` | 3 | 8 (2 lesen) | 0,375 | 562,5 GFLOP/s | 2,9 % | speichergebunden |
| (e) Horner, Grad 10 | 20 | 8 (1 lesen, 1 schreiben) | 2,5 | 3750 GFLOP/s | 19,2 % | speichergebunden |

Zur Zählung im Einzelnen:

- **(c)** zwei Multiplikationen = 2 FLOP; gelesen werden `a`, `b`, `c`, geschrieben `d` — vier
  Wörter à 4 Byte.
- **(d)** eine Subtraktion, eine Multiplikation, eine Addition = 3 FLOP; gelesen `x` und `y`,
  `s` steckt in einem Register bzw. wird reduziert (laut Aufgabe kostenlos).
- **(e)** Horner mit Grad $d$: $d$ Multiplikationen und $d$ Additionen = $2d$ FLOP. Die
  Koeffizienten liegen in Registern und zählen nicht zum DRAM-Verkehr.

**(b)** ist der Sonderfall: 0 FLOP, also $I = 0$. Die Roofline ist hier nicht das richtige
Werkzeug — sinnvoll misst man nicht GFLOP/s, sondern die **erreichte Bandbreite in GB/s**.
Genau dafür benutzt man solche Kernel: als Referenz, wie schnell der Speicher wirklich ist.

**f) Keiner der vier ist rechengebunden.** Der höchste Wert ist $I = 2{,}5$ bei (e), und das
sind nicht einmal 20 % des Knickpunkts. Am nächsten kommt also (e).

Für Grad $d$ gilt $I = 2d/8 = d/4$. Rechengebunden heißt $I \ge 13$:

$$\frac{d}{4} \ge 13 \iff d \ge \mathbf{52}$$

Ein Polynom **52. Grades** — dann erst wäre der Kernel rechengebunden. Das ist die
anschaulichste Form der Aussage „$I^\star = 13$": Man muss mit jedem geladenen Wert
52-mal rechnen.

**g) Kernel-Fusion — Faktor 1,5 geschenkt.**

Zwei getrennte Kernels:

| | FLOP | Byte |
|---|---|---|
| `t[i] = a[i]*b[i]` | 1 | 12 (a, b lesen, t schreiben) |
| `d[i] = t[i]*c[i]` | 1 | 12 (t, c lesen, d schreiben) |
| **Summe** | **2** | **24** |

$$I_{\text{getrennt}} = \frac{2}{24} = 0{,}083 \Rightarrow P \le 125\ \text{GFLOP/s}$$

Verschmolzen (ein Kernel):

$$I_{\text{fusioniert}} = \frac{2}{16} = 0{,}125 \Rightarrow P \le 187{,}5\ \text{GFLOP/s}$$

$$\text{Gewinn} = \frac{24\ \text{Byte}}{16\ \text{Byte}} = \mathbf{1{,}5\times}$$

**Woher der Gewinn kommt:** Das Zwischenergebnis `t` wird nie in den DRAM geschrieben und
nie von dort gelesen — es bleibt in einem Register. Gespart werden also 8 der 24 Byte.

Das ist die wichtigste Optimierung für speichergebundene Kernel überhaupt und der Grund,
warum Thrust `saxpy_functor` anbietet statt zweier Aufrufe: **Bei $I \ll I^\star$ ist jede
eingesparte Speicherbewegung ein direkter Zeitgewinn, jede eingesparte Rechenoperation nicht.**

**h) `double`: alle Bytes verdoppeln sich, die FLOPs nicht.**

$$I_{\text{FP64}} = \frac{I_{\text{FP32}}}{2}, \qquad I^\star_{\text{FP64}} = \frac{9700}{1500} = 6{,}47\ \text{FLOP/Byte}$$

| Kernel | $I$ (FP32) | $I$ (FP64) | $P$ (FP32) | $P$ (FP64) |
|---|---|---|---|---|
| (c) | 0,125 | 0,0625 | 187,5 | 93,8 GFLOP/s |
| (d) | 0,375 | 0,1875 | 562,5 | 281,3 GFLOP/s |
| (e) | 2,5 | 1,25 | 3750 | 1875 GFLOP/s |

**Die Klasse ändert sich bei keinem** — beide, $I$ und $I^\star$, wandern nach links, und alle
Kernel bleiben weit links vom Knick. Aber die Leistung **halbiert sich exakt**.

Die entscheidende Erkenntnis: Bei einem speichergebundenen Kernel kostet `double` genau
Faktor 2, und zwar **unabhängig davon, wie viele FP64-Einheiten die Karte hat**. Der
berüchtigte FP64-Nachteil von Consumer-GPUs (Faktor 32 bis 64) trifft nur rechengebundene
Kernel. Praktische Regel: **`float` benutzen, wo die Genauigkeit reicht** — der Gewinn
kommt aus der halbierten Datenmenge, nicht aus den Recheneinheiten.

---

## Lösung 12.2 — Transaktionen zählen

Ein Warp braucht 32 × 4 = **128 Byte** Nutzdaten. Effizienz = 128 / geholte Bytes.

| | Zugriff | Byte-Offsets | Sektoren | geholt | Effizienz |
|---|---|---|---|---|---|
| a) | `a[t]` | 0 … 127 | **4** | 128 B | **100 %** |
| b) | `a[t+1]` | 4 … 131 | **5** | 160 B | **80 %** |
| c) | `a[31-t]` | 0 … 127 | **4** | 128 B | **100 %** |
| d) | `a[2*t]` | 0, 8, … 248 | **8** | 256 B | **50 %** |
| e) | `a[16*t]` | 0, 64, … 1984 | **32** | 1024 B | **12,5 %** |
| f) | `a[0]` | 0 | **1** | 32 B | Broadcast |

Zu den einzelnen Fällen:

- **(a)** Der Idealfall: 128 zusammenhängende, ausgerichtete Byte = genau 4 Sektoren.
- **(b)** Ein einziges Element Versatz: Der Bereich 4 … 131 überlappt fünf Sektoren, weil er
  weder am Anfang noch am Ende auf einer 32-Byte-Grenze liegt. **20 % Verlust durch eine
  Fehlausrichtung** — das ist der Grund, warum `cudaMalloc` immer auf 256 Byte ausrichtet und
  warum man Offsets in Kernel-Argumenten mit Vorsicht behandelt.
- **(c)** Eine **Permutation innerhalb desselben ausgerichteten Segments** ist auf modernen
  GPUs kostenlos. Die Hardware kümmert sich nicht um die Reihenfolge, nur um die Menge der
  berührten Sektoren. (Das war auf sehr alten Architekturen anders.)
- **(d)** Halbe Effizienz: Von jedem geholten Sektor werden nur 16 der 32 Byte gebraucht.
- **(e)** Jeder Zugriff landet in einem eigenen Sektor — 32 Transaktionen für 128 Nutzbyte.
- **(f)** Alle lesen dasselbe Wort: **Broadcast**, eine Transaktion. Die Effizienzformel ist
  hier irreführend (der Warp braucht nur 4 eindeutige Byte); wichtig ist, dass es der
  *bestmögliche* Fall ist, nicht der schlechteste.

**g) AoS vs. SoA.**

| Layout | Zugriff | Offsets | Sektoren | geholt | Effizienz |
|---|---|---|---|---|---|
| AoS `part[t].x` | Schritt 12 B | 0, 12, … 372 | **12** | 384 B | **33,3 %** |
| SoA `part.x[t]` | Schritt 4 B | 0, 4, … 124 | **4** | 128 B | **100 %** |

**Dieselben Daten, dieselbe Rechnung, Faktor 3 im Speicherverkehr** — allein durch die
Deklaration.

**h) Wenn alle drei Komponenten gebraucht werden, gleichen sich beide an.**

- **AoS:** Der Warp liest `part[t].x`, `.y`, `.z` — insgesamt die Byte 0 … 383, also 12
  Sektoren, und **alle 384 Byte werden gebraucht**: Effizienz 100 %.
- **SoA:** drei getrennte Zugriffe à 4 Sektoren = ebenfalls 12 Sektoren, 384 Byte, 100 %.

Es steht also unentschieden — der AoS-Nachteil verschwindet genau dann, wenn man die ganze
Struktur benutzt.

**Und der Unterschied zur CPU?** Dort gewinnt AoS in diesem Fall deutlich: **ein einzelner
Thread** braucht `x`, `y` und `z` desselben Partikels, und die liegen in AoS in *einer*
Cache-Zeile. Ein Zugriff, drei Werte.

Das ist wieder derselbe Perspektivwechsel wie beim Grid-Stride-Loop:

| | Wer greift gleichzeitig zu | Was soll benachbart liegen |
|---|---|---|
| CPU | **ein** Thread auf mehrere Felder eines Objekts | die Felder eines Objekts → **AoS** |
| GPU | **32** Lanes auf dasselbe Feld verschiedener Objekte | dasselbe Feld über Objekte → **SoA** |

**i) Effizienz über der Schrittweite `s` (in `float`).**

Die 32 Lanes berühren $\min(32,\ 4s)$ Sektoren:

| $s$ | 1 | 2 | 4 | 8 | 16 | 32 |
|---|---|---|---|---|---|---|
| Sektoren | 4 | 8 | 16 | 32 | 32 | 32 |
| Effizienz | 100 % | 50 % | 25 % | 12,5 % | 12,5 % | 12,5 % |

$$\text{Effizienz}(s) = \frac{128}{32 \cdot \min(32,\, 4s)} = \frac{4}{\min(32,\, 4s)}
= \max\left(\frac18,\ \min\left(1,\ \frac{1}{s}\right)\right)$$

Der einfache Ausdruck $1/s$ gilt also **nur bis $s = 8$**; danach greift die Sättigung bei
$1/8$.

**Ab $s = 8$ wird es nicht mehr schlimmer**, weil ein Warp aus 32 Threads besteht und damit
höchstens 32 verschiedene Sektoren anfordern kann. Schlechter als „jeder Thread ein eigener
Sektor" geht nicht.

Die gemessenen Bandbreitenkurven auf Folie 14 zeigen genau das: steiler Einbruch bis
Schrittweite ~8, danach ein flaches Plateau.

---

## Lösung 12.3 — Bankkonflikte

Bank$(w) = w \bmod 32$. Ein Konflikt entsteht nur bei **verschiedenen Wörtern in derselben
Bank**; dasselbe Wort ist Broadcast.

| | Zugriff | Bänke | Grad | Begründung |
|---|---|---|---|---|
| a) | `sm[t]` | 0 … 31 | **1** | jede Bank genau einmal |
| b) | `sm[2*t]` | 0, 2, … 62 mod 32 | **2** | Bank $b$ wird von $t$ und $t+16$ getroffen |
| c) | `sm[3*t]` | alle 32 | **1** | 3 ist teilerfremd zu 32 → Permutation |
| d) | `sm[8*t]` | 0, 8, 16, 24 | **8** | nur 4 verschiedene Bänke für 32 Threads |
| e) | `sm[32*t]` | alle Bank 0 | **32** | schlimmster Fall |
| f) | `sm[t/2]` | 0 … 15 | **1** | je zwei Threads lesen **dasselbe** Wort → Broadcast |
| g) | `sm[5]` | Bank 5 | **1** | alle dasselbe Wort → Broadcast |

Die Unterscheidung zwischen (d) und (f) ist der Kern der Sache: In beiden Fällen teilen sich
mehrere Threads eine Bank. In (d) wollen sie **verschiedene** Wörter (Konflikt), in (f)
**dasselbe** (Broadcast, gratis).

**h) Die allgemeine Regel.**

$$k = \gcd(s, 32)$$

Herleitung: Die getroffenen Bänke sind $\{s t \bmod 32 : t = 0 \ldots 31\}$. Diese Menge ist
die von $s$ erzeugte Untergruppe von $\mathbb{Z}_{32}$, also die Vielfachen von
$g = \gcd(s,32)$ — das sind $32/g$ verschiedene Bänke. Auf jede entfallen $32/(32/g) = g$
Threads, und die lesen verschiedene Wörter (solange $s \ne 0$).

**Folgerung: jede ungerade Schrittweite ist konfliktfrei**, denn ungerade Zahlen sind
teilerfremd zu 32. Genau darauf beruht das Padding.

**i)/k) Das 2D-Tile.** Bei `__shared__ float tile[32][32]` ist der Wortindex $32r + c$.

**(i) `tile[threadIdx.y][threadIdx.x]`** — innerhalb eines Warps ist `threadIdx.y` konstant
(bei `dim3 threads(32,32)` besteht ein Warp aus 32 aufeinanderfolgenden `threadIdx.x` mit
gleichem `y`), und `threadIdx.x` durchläuft 0 … 31:

$$\text{Bank} = (32y + x) \bmod 32 = x \bmod 32$$

Für $x = 0 \ldots 31$ sind das alle 32 Bänke genau einmal → **konfliktfrei**.

**(j) `tile[threadIdx.x][threadIdx.y]`** — jetzt variiert der *Zeilenindex* über den Warp:

$$\text{Bank} = (32x + y) \bmod 32 = y \bmod 32 \quad \text{für alle } x$$

Alle 32 Lanes treffen **dieselbe Bank** $y$ und wollen 32 **verschiedene** Wörter →
**32-Wege-Konflikt**, Faktor 32. Das ist der teuerste Fehler, den man im Shared Memory machen
kann.

**l) Padding auf `[33]`.** Wortindex $33r + c$, also

$$\text{Bank} = (33x + y) \bmod 32 = (32x + x + y) \bmod 32 = (x + y) \bmod 32$$

Für $x = 0 \ldots 31$ durchläuft $(x+y) \bmod 32$ **alle 32 Bänke genau einmal** →
**konfliktfrei**. Anschaulich: Die eine zusätzliche Spalte verschiebt jede Zeile um genau
eine Bank, sodass eine Spalte diagonal über alle Bänke läuft.

**Kosten:** 32 × 33 − 32 × 32 = 32 zusätzliche `float` = **128 Byte** pro Tile,
also 33/32 = **+3,1 %** Shared Memory. **Nutzen:** Faktor 32 auf den Spaltenzugriffen.

**m) `tile[32][34]` ist keine gute Wahl.** Wortindex $34x + y$:

$$\text{Bank} = (34x + y) \bmod 32 = (2x + y) \bmod 32$$

Die Schrittweite ist jetzt 2, also $\gcd(2,32) = 2$: **2-Wege-Konflikt**. Es sind zwar nur
noch Faktor 2 statt 32, aber eben nicht 1 — und es kostet doppelt so viel zusätzlichen
Speicher wie die richtige Lösung.

> **Merksatz:** Das Padding muss **ungerade** sein. `+1` ist die kanonische Wahl; `+2` ist der
> klassische Fehler beim Nachbauen aus dem Gedächtnis.

---

## Lösung 12.4 — Occupancy

**a)** Schema: für jede Ressource die Blockzahl bestimmen, **Minimum** nehmen, mit den Warps
pro Block multiplizieren, durch 64 teilen.

| # | `blockDim` | Warps/Block | Register erlaubt | Shared erlaubt | Warps erlaubt | Block-Limit | **knappste** | Blöcke | Warps | **Occupancy** |
|---|---|---|---|---|---|---|---|---|---|---|
| A | 256 | 8 | 8 | 20 | 8 | 32 | Register/Warps | **8** | 64 | **100 %** |
| B | 256 | 8 | 4 | 20 | 8 | 32 | **Register** | **4** | 32 | **50 %** |
| C | 256 | 8 | 8 | 3 | 8 | 32 | **Shared Memory** | **3** | 24 | **37,5 %** |
| D | 1024 | 32 | 1 | — | 2 | 32 | **Register** | **1** | 32 | **50 %** |
| E | 32 | 1 | 64 | — | 64 | 32 | **Block-Limit** | **32** | 32 | **50 %** |

Die Rechnungen im Einzelnen:

- **A:** Register $65\,536 / (256 \cdot 32) = 65\,536/8192 = 8$; Shared $164/8 = 20{,}5 \to 20$;
  Warps $64/8 = 8$. Minimum 8 Blöcke → 64 Warps → 64/64 = **100 %**.
- **B:** Nur die Register ändern sich: $65\,536/(256 \cdot 64) = 4$ Blöcke → 32 Warps →
  **50 %**. **Die Verdopplung des Registerbedarfs halbiert die Occupancy.**
- **C:** $164/48 = 3{,}4 \to 3$ Blöcke → 24 Warps → **37,5 %**. Shared Memory ist hier die
  knappste Ressource.
- **D:** $65\,536/(1024 \cdot 40) = 65\,536/40\,960 = 1{,}6 \to 1$ Block. Ein einziger Block
  mit 32 Warps → **50 %**. Das ist das Argument gegen `blockDim = 1024`: Sobald nicht einmal
  zwei Blöcke passen, verliert man die Hälfte — und bei einer Barriere steht der ganze SM.
- **E:** Register und Warps erlauben je 64 Blöcke, aber es dürfen höchstens **32 Blöcke** pro
  SM resident sein. 32 Blöcke à 1 Warp = 32 Warps → **50 %**.

**b) E scheitert am Block-Limit, nicht an den Ressourcen.** Ein Block mit 32 Threads ist genau
ein Warp; um 64 Warps unterzubringen, bräuchte man 64 residente Blöcke, erlaubt sind aber 32.

**Praktische Regel:** `blockDim` weder zu klein noch zu groß.

$$\text{blockDim} \ge \frac{2048}{32} = 64\ \text{Threads}$$

ist die untere Schranke, um überhaupt 100 % erreichen zu können. Nach oben begrenzen die
Register. **128 bis 512 ist der übliche Bereich**, 256 die sichere Standardwahl.

**c) B auf 100 % bringen.** Für 8 residente Blöcke bei `blockDim = 256`:

$$\frac{65\,536}{8} = 8192\ \text{Register pro Block} \Rightarrow \frac{8192}{256} = \mathbf{32\ \text{Register pro Thread}}$$

Zwei Wege, den Registerbedarf zu senken:

| Maßnahme | Nachteil |
|---|---|
| **Weniger entrollen** (`#pragma unroll 2` statt voll) | weniger Instruktionsparallelität — man tauscht ILP gegen TLP, was sich aufheben kann |
| **`__launch_bounds__(256)`** am Kernel — zwingt den Compiler unter das Registerlimit | er löst das notfalls durch **Spilling** ins lokale Memory (= globaler Speicher!), was schlimmer sein kann als niedrige Occupancy |

Ein dritter Weg: Zwischenwerte neu berechnen statt in Registern zu halten. Auf einer GPU ist
Rechnen billig und Speicher teuer, das lohnt sich öfter als man denkt.

**d) Nein — 88 % der Bandbreite ist praktisch das Maximum.** Occupancy ist ein **Mittel** zum
Latency Hiding, kein Ziel. Wenn der Speicherbus zu 88 % ausgelastet ist, ist die Latenz
offensichtlich bereits ausreichend verdeckt — hier durch **Instruktionsparallelität** (jeder
Thread hat mehrere unabhängige Ladeoperationen gleichzeitig unterwegs) statt durch viele
Warps.

Die Occupancy zu erhöhen würde bedeuten, Register einzusparen — und damit womöglich genau die
ILP zerstören, die das Ergebnis trägt. **Die Zielgröße ist GB/s bzw. GFLOP/s, nicht die
Occupancy.**

**e)** `nvcc -Xptxas -v` — gibt pro Kernel Register je Thread, Shared Memory je Block und
etwaiges Spilling aus. Im Makefile ist das `make regs`. Zur Gegenprobe zur Laufzeit:
`cudaOccupancyMaxActiveBlocksPerMultiprocessor()`.

---

## Lösung 12.5 — Reduktion: drei Wege

Vollständiges Programm: [`code/reduktion.cu`](code/reduktion.cu).

**a) Vorhersage vor dem Programmieren.**

Pro Element: eine Subtraktion, eine Multiplikation, eine Addition = **3 FLOP**; gelesen werden
`x[i]` und `y[i]` = **8 Byte**.

$$I = \frac{3}{8} = 0{,}375 \Rightarrow P \le 1500 \cdot 0{,}375 = 562{,}5\ \text{GFLOP/s}$$

Das sind 2,9 % der Spitzenleistung — tief **speichergebunden**. Woran man das nach der Messung
erkennt: **Die erreichte Bandbreite** ist die aussagekräftige Zahl, nicht GFLOP/s. Bei
$n = 10^8$ werden $8n = 800$ MB gelesen; bei 1500 GB/s wäre das

$$T_{\min} = \frac{0{,}8\ \text{GB}}{1500\ \text{GB/s}} \approx 0{,}53\ \text{ms}$$

Eine gute Implementierung landet bei 0,6–0,7 ms. Wer dort ankommt, ist fertig — mehr geht nicht.

**b) Fassung 1 — ein `atomicAdd` pro Thread.**

```cpp
__global__ void dist_atomic(const float* x, const float* y, float* s, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float d = x[i] - y[i];
        atomicAdd(s, d * d);          // 10^8 Atomics auf EINE Adresse
    }
}
```

Korrekt, aber der langsamste denkbare Weg: Alle $10^8$ Threads serialisieren an einer einzigen
Speicheradresse.

**c) Fassung 2 — Baumreduktion im Shared Memory.**

```cpp
__global__ void dist_shared(const float* x, const float* y, float* s, int n) {
    extern __shared__ float sm[];
    int tid    = threadIdx.x;
    int i      = blockIdx.x * blockDim.x + tid;
    int stride = blockDim.x * gridDim.x;

    float lokal = 0.0f;                    // Grid-Stride: mehr Arbeit pro Thread
    for (int k = i; k < n; k += stride) {
        float d = x[k] - y[k];
        lokal += d * d;
    }
    sm[tid] = lokal;
    __syncthreads();                       // (1) alle haben geschrieben

    for (int t = blockDim.x / 2; t > 0; t /= 2) {
        if (tid < t) sm[tid] += sm[tid + t];
        __syncthreads();                   // (2) Runde abgeschlossen
    }

    if (tid == 0) atomicAdd(s, sm[0]);     // EIN Atomic pro Block
}
```

**Wo die Barrieren stehen müssen und warum:**

- **(1)** zwischen dem Beschreiben von `sm` und dem ersten Lesen durch *andere* Threads. Ohne
  sie liest Thread 0 in der ersten Runde `sm[128]`, das Thread 128 vielleicht noch nicht
  geschrieben hat.
- **(2)** am Ende jeder Runde, **außerhalb** des `if`. Runde $t$ liest, was Runde $2t$
  geschrieben hat. Stünde die Barriere *innerhalb* des `if (tid < t)`, würden die Threads mit
  `tid >= t` sie nie erreichen — undefiniertes Verhalten, in der Praxis ein Hänger.

Der Grid-Stride-Vorlauf ist kein Beiwerk: Er sorgt dafür, dass jeder Thread viele Elemente
verarbeitet, bevor überhaupt reduziert wird. Der teure $\log$-Baum läuft damit einmal pro
Block statt einmal pro 256 Elementen.

**d) Fassung 3 — Warp-Shuffle.**

```cpp
__inline__ __device__ float warp_reduce(float val) {
    for (int offset = 16; offset > 0; offset /= 2)
        val += __shfl_down_sync(0xffffffff, val, offset);
    return val;                            // Lane 0 hat die Warp-Summe
}

__global__ void dist_shuffle(const float* x, const float* y, float* s, int n) {
    __shared__ float warp_summe[32];       // hoechstens 32 Warps pro Block
    int tid    = threadIdx.x;
    int i      = blockIdx.x * blockDim.x + tid;
    int stride = blockDim.x * gridDim.x;

    float lokal = 0.0f;
    for (int k = i; k < n; k += stride) {
        float d = x[k] - y[k];
        lokal += d * d;
    }

    lokal = warp_reduce(lokal);            // Stufe 1: innerhalb des Warps
    int lane = tid % 32, warp = tid / 32;
    if (lane == 0) warp_summe[warp] = lokal;
    __syncthreads();

    if (warp == 0) {                       // Stufe 2: die Warp-Ergebnisse
        lokal = (tid < blockDim.x / 32) ? warp_summe[lane] : 0.0f;
        lokal = warp_reduce(lokal);
        if (lane == 0) atomicAdd(s, lokal);
    }
}
```

Die ersten fünf Reduktionsschritte laufen **vollständig in Registern** — kein Shared Memory,
keine Barriere, keine Bankkonflikte. Nur die höchstens 32 Warp-Ergebnisse gehen durch den
Shared Memory.

**e) Verifikation.** Nicht auf Gleichheit prüfen, sondern mit **relativer Toleranz**. Zwei
Gründe, die beide unabhängig voneinander greifen:

1. Die GPU addiert in **anderer Reihenfolge** als die serielle CPU-Referenz, und
   Gleitkommaaddition ist nicht assoziativ.
2. `atomicAdd` auf `float` hat eine **nichtdeterministische** Reihenfolge — dasselbe Programm
   liefert bei zwei Läufen leicht verschiedene letzte Bits.

Bei $n = 10^8$ Summanden in `float` (24 Bit Mantisse) ist der Fehler auch nicht klein: Eine
relative Toleranz von $10^{-4}$ ist hier realistisch. Die numerisch bessere Referenz auf der
CPU rechnet in `double` — dann sieht man den Fehler der GPU-Rechnung überhaupt erst. Dass die
Baumreduktion dabei **genauer** ist als die serielle Summation (Fehler $O(\log n)$ statt
$O(n)$), ist ein angenehmer Nebeneffekt.

**f)/(g)/(h) Erwartete Ergebnisse.**

| Fassung | Erwartung | Grund |
|---|---|---|
| 1 (Atomic pro Thread) | um Größenordnungen langsam | $10^8$ serialisierte Zugriffe auf eine Adresse |
| 2 (Shared Memory) | nahe an der Bandbreitengrenze | ein Atomic pro Block, also ~$10^5$ statt $10^8$ |
| 3 (Shuffle) | wenige Prozent besser als 2 | spart Shared Memory und Barrieren — aber beide sind schon bandbreitenlimitiert |

**Warum die beiden Verhältnisse so verschieden groß sind, ist die eigentliche Lehre:** Der
Schritt 1 → 2 beseitigt einen **strukturellen** Engpass (Serialisierung) und bringt Faktoren.
Der Schritt 2 → 3 optimiert *innerhalb* eines Kernels, der bereits am Speicherbus hängt — und
kann deshalb fast nichts mehr bringen. Sobald die Roofline-Grenze erreicht ist, ist jede
weitere Mikrooptimierung wirkungslos. Genau dafür rechnet man Teil a) **vorher**.

**h)** `if (tid % (2*s) == 0)` rechnet dasselbe und macht genauso viele Additionen, verteilt
die aktiven Threads aber **über alle Warps**: In jeder Runde ist *jeder* Warp teilweise aktiv,
also divergent. Bei `if (tid < s)` sind die ersten Warps voll aktiv und der Rest komplett
inaktiv — solange $s \ge 32$ gibt es **keinen einzigen divergenten Warp**.

Vorhersage: Die Reduktionsphase wird um etwa Faktor 2 langsamer. **Auf die Gesamtzeit
schlägt das kaum durch**, weil der Grid-Stride-Vorlauf dominiert — was genau der Grund ist,
warum man ihn hat. Ohne ihn (ein Element pro Thread) wäre der Unterschied deutlich sichtbar.

---

## Lösung 12.6 — Roofline eines Stencils

**a)** $u_{i-1,j} + u_{i+1,j} + u_{i,j-1} + u_{i,j+1}$ sind **3 Additionen**, dazu **1
Multiplikation** mit 0,25:

$$\mathbf{4\ \text{FLOP pro Gitterpunkt}}$$

**b) Naiv.** Jeder Thread liest seine vier Nachbarn und schreibt ein Ergebnis — 5 Wörter,
keine Wiederverwendung:

$$\text{Verkehr} = 5 \cdot 4 = 20\ \text{Byte}, \qquad I = \frac{4}{20} = \mathbf{0{,}2}$$
$$P \le 1500 \cdot 0{,}2 = \mathbf{300\ \text{GFLOP/s}} \quad (1{,}5\ \% \text{ von } P_{\text{peak}})$$

Klar **speichergebunden**.

**c) Gekachelt.** Ein Block lädt $(T+2)^2$ Wörter (die $T \times T$-Kachel plus einen
**Halo**-Rand von einem Punkt auf jeder Seite) und schreibt $T^2$ Ergebnisse. Pro
Ausgabepunkt:

$$\text{Wörter} = \frac{(T+2)^2 + T^2}{T^2}, \qquad \text{Byte} = \frac{4\left[(T+2)^2 + T^2\right]}{T^2}$$

$$I(T) = \frac{4 T^2}{4\left[(T+2)^2 + T^2\right]} = \boxed{\frac{T^2}{(T+2)^2 + T^2}}$$

**d) Auswertung.**

| $T$ | $(T+2)^2$ | $I(T)$ | $P$ | Gewinn gegenüber naiv |
|---|---|---|---|---|
| naiv | — | 0,200 | 300 GFLOP/s | 1,00× |
| 8 | 100 | 0,390 | 585 GFLOP/s | 1,95× |
| 16 | 324 | 0,441 | 662 GFLOP/s | 2,21× |
| 32 | 1156 | 0,470 | 705 GFLOP/s | 2,35× |
| $\to \infty$ | — | **0,500** | **750 GFLOP/s** | **2,50×** |

Der Grenzwert: Für große $T$ wird der Halo vernachlässigbar, $(T+2)^2 \to T^2$, also
$I \to T^2/(2T^2) = 1/2$.

**Warum 0,5 eine harte Schranke ist:** Jeder Gitterpunkt muss **mindestens einmal gelesen**
und **mindestens einmal geschrieben** werden — das sind 8 Byte für 4 FLOP, und weniger geht
prinzipiell nicht. Kein Kacheln, kein Cache, keine Umsortierung kann darunter. $I = 0{,}5$ ist
die informationstheoretische Untergrenze des Speicherverkehrs für dieses Problem.

**e) Das Diagramm.** Doppelt logarithmisch, $I$ auf der x-Achse [FLOP/Byte], $P$ auf der
y-Achse [GFLOP/s]. Die Roofline steigt mit Steigung 1 (Gerade $P = 1500 \cdot I$) bis zum
Knickpunkt bei $I^\star = 13$, $P = 19\,500$, und verläuft danach waagerecht.

Die Betriebspunkte liegen bei $I = 0{,}2$ (naiv) und $I = 0{,}47$ ($T=32$) — **beide auf dem
schrägen Ast**, weit links vom Knick. Das Kacheln verschiebt den Punkt um gut den Faktor 2
nach rechts und entsprechend nach oben, aber **er bleibt auf der Schräge**: Der Kernel bleibt
speichergebunden.

**f) Der Unterschied zum Matrixprodukt.**

| | Stencil | Matrixprodukt |
|---|---|---|
| $I(T)$ | $\dfrac{T^2}{(T+2)^2+T^2} \to \dfrac{1}{2}$ | $\dfrac{T}{4} \to \infty$ |
| Wiederverwendung pro geladenem Wert | höchstens 5 (so viele Ausgaben brauchen ihn) — **konstant** | $T$ — **wächst mit der Kachel** |
| Kacheln ändert | die Konstante | die **Klasse** |

**Das allgemeine Kriterium:** Kacheln kann einen Kernel nur dann über den Knickpunkt schieben,
wenn die **Wiederverwendung pro geladenem Datenelement mit der Kachelgröße wächst**.

Der strukturelle Grund liegt in der Komplexität: Das Matrixprodukt macht $O(N^3)$ Arbeit auf
$O(N^2)$ Daten — das Verhältnis Arbeit/Daten wächst mit $N$, und die Kachelung schöpft das ab.
Der Stencil macht $O(N^2)$ Arbeit auf $O(N^2)$ Daten — das Verhältnis ist **konstant**, und
daran kann keine Umorganisation etwas ändern.

> **Merksatz:** Kacheln hilft, wenn Arbeit und Daten in verschiedenen Ordnungen wachsen.
> Sonst verbessert es nur die Konstante.

**g) Realistisch erwartbar: Faktor 2 bis 2,4.** Das ist ordentlich, aber deutlich weniger als
beim GEMM.

Der ehrliche Zusatz: Ein guter Teil dieser Wiederverwendung fällt beim Stencil **ohnehin schon
an**, weil der L2-Cache benachbarte Zeilen hält — die vier Nachbarn eines Punktes wurden vom
Nachbarthread gerade gelesen. Der gemessene Gewinn liegt deshalb oft deutlich unter dem
gerechneten. **Beim Stencil erst messen, dann kacheln;** beim GEMM lohnt es sich immer.

---

## Lösung 12.7 — Gekacheltes Matrixprodukt

Vollständiges Programm: [`code/matmul.cu`](code/matmul.cu).

**a)** Naiv wie auf Folie 37, gekachelt wie auf Folie 39 (beide im Code).

**b) Für `N % TILE != 0` fehlen der Vorlesungsfassung drei Dinge:**

1. **Wächter beim Laden der Kacheln.** Beide Ladezeilen greifen mit
   `m*TILE + threadIdx.x` bzw. `.y` auf `A` und `B` zu; im letzten Kachelschritt liegt das
   hinter dem Rand. Wer außerhalb liegt, muss **0 laden** — nicht etwa das Laden überspringen,
   denn der Wert wird in der Rechenschleife gelesen und muss neutral sein:

   ```cpp
   As[ty][tx] = (row < N && m*TILE + tx < N) ? A[row*N + m*TILE + tx] : 0.0f;
   Bs[ty][tx] = (col < N && m*TILE + ty < N) ? B[(m*TILE + ty)*N + col] : 0.0f;
   ```

2. **Aufrunden der Kachelzahl:** `for (int m = 0; m < (N + TILE - 1) / TILE; ++m)` statt
   `N/TILE` — sonst fehlt die letzte, angebrochene Kachel komplett.
3. **Wächter beim Schreiben:** `if (row < N && col < N) C[row*N + col] = acc;`

Der erste Punkt ist der subtile: Ein `if`, das das Laden überspringt, lässt den alten Inhalt
der vorigen Kachel stehen — und der geht dann als Müll in die Summe ein.

**c)** FLOPs $= 2N^3$; GFLOP/s $= 2N^3 / (T_{\text{Kernel}} \cdot 10^9)$ bei $T$ in Sekunden.

| $N$ | $2N^3$ |
|---|---|
| 1024 | 2,15 GFLOP |
| 2048 | 17,2 GFLOP |
| 4096 | 137,4 GFLOP |

**d) Erwartung und Realität.**

| | $I$ | Roofline-Schranke |
|---|---|---|
| naiv | 0,25 | 375 GFLOP/s |
| gekachelt, $T=32$ | 8 | 12 000 GFLOP/s |

Der naive Kernel wird die 375 GFLOP/s **überschreiten** — und das ist kein Widerspruch,
sondern der wichtigste Lerneffekt dieser Teilaufgabe: Die Rechnung $I = 0{,}25$ unterstellt
*gar keine* Wiederverwendung, aber der **L2-Cache** liefert einen Teil davon gratis. Die
Roofline mit DRAM-Bandbreite ist eine Schranke für den DRAM-Verkehr, nicht für den Verkehr,
den der Kernel *anfordert*.

Der gekachelte Kernel wird die 12 000 GFLOP/s dagegen **deutlich verfehlen** — typisch sind
2000–5000 GFLOP/s. Zwei Gründe:

1. **$P_{\text{peak}}$ setzt lauter FMA-Instruktionen voraus**, ohne Adressrechnung, ohne
   Schleifenverwaltung, ohne Shared-Memory-Zugriffe. Die innere Schleife macht jedoch pro FMA
   zwei Shared-Memory-Lesevorgänge — die Shared-Memory-Bandbreite wird zum neuen Engpass.
2. **Der Kernel ist nicht durchgehend am Rechnen**: `__syncthreads()` zweimal pro Kachel
   serialisiert die Phasen, und beim Laden rechnet niemand.

Deshalb ist die Roofline eine **obere Schranke**, kein Versprechen. Wer 25 % davon erreicht,
hat einen guten Kernel geschrieben.

**e)** Der gekachelte Kernel braucht $2 \cdot 32 \cdot 32 \cdot 4 = 8192$ Byte = **8 kB**
Shared Memory pro Block, bei `blockDim = 32 × 32 = 1024` Threads. Damit:

- Shared: $164/8 = 20$ Blöcke
- Warps: $64/32 = 2$ Blöcke
- Register (angenommen 40/Thread): $65\,536/(1024 \cdot 40) = 1$ Block

Es gewinnen die Register: **1 Block, 32 Warps, 50 % Occupancy.** Genau der Fall D aus
Aufgabe 12.4 — und ein Argument für `TILE = 16` (256 Threads pro Block), das oft besser
abschneidet, obwohl $I$ nur halb so groß ist. Die tatsächlichen Registerzahlen liefert
`make regs`.

**f) Bankkonflikte im gekachelten Kernel:** Beide Zugriffe der inneren Schleife sind
konfliktfrei.

- `As[threadIdx.y][k]`: Bei `TILE = 32` hat ein Warp konstantes `threadIdx.y`, und `k` ist
  ebenfalls für alle gleich → **alle 32 Lanes lesen dasselbe Wort** → Broadcast.
- `Bs[k][threadIdx.x]`: Wortindex $32k + t_x$, Bank $= t_x$ → alle 32 Bänke genau einmal →
  konfliktfrei.

`As[TILE][TILE+1]` ändert deshalb **nichts** (außer 128 Byte mehr Shared Memory). Padding
wird erst nötig, wenn man ein Tile **transponiert** liest — etwa in einem Transpose-Kernel
oder bei $A^T B$. Das ist der Punkt der Teilaufgabe: Padding ist kein Ritual, das man überall
hinschreibt, sondern die Antwort auf ein konkret nachgerechnetes Zugriffsmuster.

**g) cuBLAS.** Realistisch ist ein Faktor **3 bis 10**. Was cuBLAS zusätzlich tut:

1. **Register-Tiling / mehrere Ausgaben pro Thread** — jeder Thread berechnet ein
   4×4-Teilquadrat von `C` in Registern. Das erhöht die Wiederverwendung noch einmal um
   Faktor 4 *oberhalb* des Shared Memory und verbessert das Verhältnis Rechnen zu Laden
   drastisch.
2. **Tensor-Cores** (`TF32`/`FP16`-Pfade) — dedizierte Matrix-Multiply-Accumulate-Hardware
   mit einem Vielfachen der FP32-Rate.
3. **Software-Pipelining / asynchrones Kopieren** — die nächste Kachel wird geladen, während
   die aktuelle gerechnet wird, sodass die `__syncthreads()`-Pausen verschwinden. Dazu
   architekturspezifisch abgestimmte Kachelgrößen und Instruktionsplanung.

**Für dichtes GEMM ist ein selbst geschriebener Kernel praktisch nie konkurrenzfähig.** Der
eigene Kernel dient dem Verständnis — dieselbe Haltung wie beim `qsort` in Kapitel 07.

---

## Lösung 12.8 — Fehlersuche im Shared-Memory-Kernel

**a) Die Fehler.**

**Korrektheitsfehler:**

| # | Stelle | Fehler | Wirkung |
|---|---|---|---|
| K1 | `sm[tid] = a[i];` | **kein Wächter `i < n`** | Zugriff hinter das Array im letzten Block; zudem landet Müll in `sm` und damit in der Summe |
| K2 | `sm[tid] = a[i];` | **`__syncthreads()` danach fehlt** | die erste Reduktionsrunde liest `sm[tid+k]`, bevor Thread `tid+k` geschrieben hat → Race |
| K3 | `__syncthreads()` **innerhalb** des `if` | divergente Barriere | die Threads mit `tid % (2k) != 0` erreichen sie nie → undefiniertes Verhalten; die Runden sind zudem nicht mehr voneinander getrennt |
| K4 | `*s += sm[0];` | **kein `atomicAdd`** | alle Blöcke führen gleichzeitig Read-Modify-Write auf derselben Adresse aus → verlorene Aktualisierungen, Ergebnis von Lauf zu Lauf anders und immer zu klein |
| K5 | `__shared__ float sm[256];` | Größe fest verdrahtet | bricht still, sobald jemand mit `blockDim.x = 512` startet — dann schreiben die Threads 256…511 hinter das Shared-Memory-Array |

**Performance-Fehler:**

| # | Stelle | Fehler | Wirkung |
|---|---|---|---|
| P1 | `if (tid % (2*k) == 0)` | aktive Threads über alle Warps verstreut | in **jeder** Runde ist jeder Warp divergent; `if (tid < k)` mit absteigendem `k` macht dasselbe ohne Divergenz |
| P2 | ein Element pro Thread | der $\log$-Baum läuft einmal pro 256 Elementen | mit einem Grid-Stride-Vorlauf reduziert jeder Thread erst viele Elemente in einem Register |

**b) K2 (die fehlende Barriere nach dem Schreiben) versteckt sich bei `blockDim.x = 32`.**

Ein Block mit 32 Threads ist **genau ein Warp**. Vor Volta lief ein Warp zwangsweise im
Gleichschritt: Wenn Thread 0 `sm[1]` liest, hat Thread 1 in derselben Instruktion bereits
geschrieben — die Barriere ist implizit erfüllt. Ab 64 Threads sind es zwei Warps, die
unabhängig geplant werden, und die Race schlägt zu.

(Ab Volta ist selbst das nicht mehr garantiert — mit unabhängigem Thread-Scheduling können
schon die Lanes eines Warps auseinanderlaufen. Genau deshalb gibt es `__syncwarp()`.)

**Was man daraus lernt: Ein CUDA-Kernel, der mit einer Konfiguration funktioniert, ist nicht
getestet.** Mindestens zwei Blockgrößen und ein `n`, das nicht aufgeht — sonst hat man die
interessanten Fälle nie angefasst.

**c) K1 zeigt sich nur bei `n % 256 != 0`.** Die sauberste Reparatur ist **nicht**, das Laden
zu überspringen, sondern eine **neutrale Null** einzutragen:

```cpp
sm[tid] = (i < n) ? a[i] : 0.0f;
```

Würde man `if (i < n) sm[tid] = a[i];` schreiben, bliebe `sm[tid]` uninitialisiert und ginge
als Müll in die Reduktion ein — der Fehler wäre nur verschoben, nicht behoben. **Bei einer
Reduktion muss jeder Platz ein neutrales Element enthalten**; das ist dasselbe Argument wie
beim Identitätselement der `reduction`-Klausel in Kapitel 06.

**d) Korrekt und effizient.**

```cpp
__global__ void summe(const float* a, float* s, int n) {
    extern __shared__ float sm[];              // K5: Groesse beim Start festlegen
    int tid    = threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    float lokal = 0.0f;                        // P2: Grid-Stride-Vorlauf
    for (int i = blockIdx.x * blockDim.x + tid; i < n; i += stride)
        lokal += a[i];                         // K1: Waechter steckt in der Bedingung
    sm[tid] = lokal;
    __syncthreads();                           // K2

    for (int k = blockDim.x / 2; k > 0; k /= 2) {
        if (tid < k) sm[tid] += sm[tid + k];   // P1: divergenzarm
        __syncthreads();                       // K3: AUSSERHALB des if
    }

    if (tid == 0) atomicAdd(s, sm[0]);         // K4
}
```

Start mit `summe<<<blocks, threads, threads*sizeof(float)>>>(d_a, d_s, n);` — der dritte
Parameter ist der dynamische Shared Memory.

**e) Welches Werkzeug findet welchen Fehler.**

| Fehler | Werkzeug | Warum |
|---|---|---|
| K1 | **`compute-sanitizer`** | meldet den Zugriff außerhalb der Grenzen exakt mit Zeile |
| K2 | **`compute-sanitizer --tool racecheck`** | erkennt den Shared-Memory-Race — von Hand praktisch unauffindbar, weil er sporadisch ist |
| K3 | **`compute-sanitizer --tool synccheck`** | erkennt divergente Barrieren |
| K4 | **CPU-Referenz** | das Ergebnis ist reproduzierbar *zu klein*; ein Sanitizer sieht daran nichts Verbotenes |
| K5 | **`compute-sanitizer`**, sobald jemand mit >256 Threads startet | Shared-Memory-Überlauf |
| P1, P2 | **`ncu`** (Nsight Compute) | zeigt Warp-Effizienz und erreichte Bandbreite; ein korrektes Ergebnis sagt darüber nichts |

**Und `CUDA_CHECK`?** Findet **keinen einzigen** dieser Fehler direkt — außer indirekt K1,
wenn der Zugriff hart genug danebengeht, dass die Runtime abbricht. Das ist die Fortsetzung
der Lehre aus Aufgabe 11.3: Fehlerprüfung ist notwendig, aber nicht hinreichend. Für
Shared-Memory-Kernels ist `compute-sanitizer` das eigentliche Werkzeug.

---

## Lösung 12.9 — Streams

**a) Ohne Streams** läuft alles nacheinander:

$$T_{\text{seriell}} = 80 + 60 + 40 = \mathbf{180\ \text{ms}}$$

**b) Die Pipeline-Formel.** Bei $c$ Stücken dauert jede Stufe pro Stück $80/c$, $60/c$ bzw.
$40/c$ ms. Die Pipeline hat drei Stufen; der Durchsatz wird von der **langsamsten** Stufe
bestimmt, hier dem Kopieren hin mit $80/c$.

$$T(c) = \underbrace{\frac{80 + 60 + 40}{c}}_{\text{ein Stück komplett}} + \underbrace{(c-1) \cdot \frac{80}{c}}_{\text{die übrigen } c-1 \text{ im Takt der Engstelle}} = \frac{180}{c} + \frac{80(c-1)}{c}$$

vereinfacht:

$$T(c) = \frac{100}{c} + 80$$

**Die Interpretation der beiden Terme:**

- $80$ ms ist der **Fließbetrieb** — die Engstelle läuft von Anfang bis Ende durch und muss
  die gesamten 80 ms Kopierarbeit leisten, egal wie fein man stückelt.
- $100/c$ ms ist **Füllen und Leeren** der Pipeline: Am Anfang rechnet und kopiert-zurück noch
  niemand, am Ende kopiert niemand mehr hin. Dieser Anteil schrumpft mit feinerer Stückelung.

**c) Auswertung.**

| $c$ | $T(c)$ | Speedup |
|---|---|---|
| 1 | 180,0 ms | 1,00× |
| 2 | 130,0 ms | 1,38× |
| 4 | 105,0 ms | 1,71× |
| 8 | 92,5 ms | 1,95× |
| 16 | 86,25 ms | 2,09× |

**d)**

$$\lim_{c \to \infty} T(c) = \mathbf{80\ \text{ms}}, \qquad S_{\max} = \frac{180}{80} = \mathbf{2{,}25\times}$$

Bestimmt wird das von der **längsten Einzelphase**, hier Host → Device mit 80 ms. Das ist
exakt dieselbe Struktur wie bei Amdahl: Was nicht überlappt werden kann, bleibt stehen.

**e) 90 % des maximalen Speedups** heißt $S \ge 0{,}9 \cdot 2{,}25 = 2{,}025$, also
$T \le 180/2{,}025 = 88{,}9$ ms:

$$\frac{100}{c} + 80 \le 88{,}9 \iff \frac{100}{c} \le 8{,}9 \iff c \ge 11{,}2$$

Also **$c = 12$** (T = 88,3 ms, S = 2,04×).

**Warum man `c` nicht beliebig groß wählt:** Jedes Stück kostet festen Overhead — ein
Kernel-Start (~5–10 µs), zwei `cudaMemcpyAsync`-Aufrufe, Stream-Verwaltung. Ab einigen hundert
Stücken frisst dieser Overhead mehr, als das Füllen der Pipeline noch einbringt. Zudem wird
jedes Stück so klein, dass der Kernel die GPU nicht mehr auslastet (zu wenige Threads —
Kapitel 11, Abschnitt 1). **In der Praxis liegt das Optimum bei 4 bis 16 Stücken.**

**f) Zwei häufige Ursachen für „Streams bringen nichts":**

1. **Kein pinned Host-Speicher.** Mit gewöhnlichem `malloc`/`new`-Speicher ist
   `cudaMemcpyAsync` in Wahrheit **synchron** — der Treiber kann keine DMA-Übertragung auf
   auslagerbare Seiten fahren und kopiert erst in einen internen Zwischenpuffer.
   **Reparatur:** `cudaMallocHost` bzw. `cudaHostAlloc`.
2. **Kernel ohne Stream-Argument.** Steht dort `kernel<<<grid, threads>>>(…)` statt
   `kernel<<<grid, threads, 0, s[k]>>>(…)`, landet er im Default-Stream — und der
   synchronisiert gegen alle anderen Streams. Damit serialisiert wieder alles.
   **Reparatur:** das vierte Argument setzen (das dritte, `smBytes`, ist dann `0`).

Zur Diagnose ist `nsys` das richtige Werkzeug: Auf dem Zeitstrahl sieht man sofort, ob die
Kopier- und die Rechen-Engine gleichzeitig arbeiten oder brav abwechselnd.

**g) Zuerst die Host→Device-Phase.** Sie ist die Engstelle der Pipeline und bestimmt allein
die Grenze von 80 ms — jede Verbesserung an Kernel oder Rückweg ändert am Ergebnis **gar
nichts**, solange sie unter 80 ms bleiben.

Konkrete Ansätze: pinned Speicher (bis ~2× mehr PCIe-Bandbreite), in geringerer Präzision
übertragen, Daten auf dem Device halten und gar nicht erst kopieren, oder auf einem
NVLink-System die Verbindung wechseln.

> Das ist wieder das Amdahl-Denken aus Kapitel 10: **Optimiere die Engstelle, nicht das, was
> sich am leichtesten optimieren lässt.**

---

## Lösung 12.10 — Kurzfragen

**a) Die erste Frage lautet: *Ist der Kernel überhaupt rechengebunden?*** Also: arithmetische
Intensität bestimmen und mit $I^\star = 13$ vergleichen. Die naheliegende — und meist falsche —
Schlussfolgerung wäre, an den Rechenoperationen zu sparen. Bei $I \ll I^\star$ sind 2 % der
Spitzenleistung möglicherweise bereits **das Maximum**, und der Kernel ist perfekt.

**b) Falsch.** Bei einem speichergebundenen Kernel wartet die Hardware auf den Speicher, nicht
auf die ALUs — die Hälfte der Operationen zu streichen ändert die Laufzeit praktisch nicht.
Doppelt so schnell wird er, wenn man die **bewegten Bytes** halbiert (Fusion, `float` statt
`double`, Wiederverwendung im Shared Memory).

**c) Falsch.** Dieselbe **Adresse** ist der Broadcast-Fall und kostet nichts. Ein Konflikt
entsteht nur bei **verschiedenen Wörtern in derselben Bank**.

**d) Falsch — 100 % Occupancy heißt nur, dass die maximale Zahl von Warps resident ist.** Ob
diese Warps etwas Sinnvolles tun, sagt die Kennzahl nicht. Ein Kernel mit unkoaleszierten
Zugriffen kann bei 100 % Occupancy 10 % der Bandbreite erreichen. Occupancy ist ein Mittel zum
Latency Hiding; die Zielgröße ist die erreichte Bandbreite bzw. GFLOP/s.

**e) Weil auf der GPU 32 Lanes eines Warps *gleichzeitig* dasselbe Feld verschiedener Objekte
lesen** — die sollen benachbart liegen, also SoA. Auf der CPU liest **ein** Thread nacheinander
alle Felder desselben Objekts — die sollen in einer Cache-Zeile liegen, also AoS. Lokalität
über die Threads gegen Lokalität über die Zeit.

**f)** Nach dem **Laden** der Kacheln (damit alle Threads eine vollständig geladene Kachel
lesen) und nach der **Rechenschleife** (damit niemand die Kachel überschreibt, während andere
Threads noch daraus lesen). Die zweite wird häufiger vergessen und produziert einen Fehler,
der bei einem Warp pro Block nicht auftritt.

**g) Weil `__syncthreads()` eine Barriere für *alle* Threads des Blocks ist.** Erreichen sie
nur einige, ist das Verhalten undefiniert — in der Praxis ein Hänger oder stille Datenfehler.
Die Barriere gehört vor oder hinter das `if`, nie hinein.

**h) Weil die Reihenfolge der atomaren Additionen nichtdeterministisch ist und
Gleitkommaaddition nicht assoziativ.** Welcher Block zuerst zum Zug kommt, entscheidet der
Scheduler; damit ändert sich die Summationsreihenfolge und mit ihr die letzten Bits. Der
Kernel ist trotzdem korrekt — man muss nur mit relativer Toleranz vergleichen statt mit `==`.

**i) Ab Volta hat jeder Thread einen eigenen Programmzähler** (*independent thread
scheduling*). Threads desselben Warps können unabhängig Fortschritt machen, laufen dafür aber
**nicht mehr automatisch wieder zusammen**. Konsequenz: Wer nach einem divergenten Abschnitt
auf Gleichschritt baut — Registeraustausch, Shared-Memory-Kommunikation innerhalb eines Warps
—, muss `__syncwarp()` schreiben, und alle Warp-Primitive brauchen eine explizite Maske
(`__shfl_down_sync` statt `__shfl`).

**j) Nein, er ist möglicherweise optimal.** 300 GFLOP/s entsprechen bei
$B_{\text{peak}} = 1500$ GB/s einer Intensität von $I = 0{,}2$ — genau der Wert eines naiven
5-Punkt-Stencils. Der Kernel läuft dann **exakt auf seiner Roofline**, und mehr ist bei dieser
Intensität nicht möglich. Die richtige Prüfung ist: $I$ ausrechnen, $B \cdot I$ damit
vergleichen. Erst wenn deutlich Luft bleibt, lohnt weitere Optimierung.

**k) Die ersten drei Punkte:**

1. **Ist der Kernel überhaupt der Engpass?** (Profilen — oft dominieren die PCIe-Transfers,
   und dann ist jede Kernel-Optimierung wirkungslos.)
2. **Sind die globalen Zugriffe coalesced?** (Größter Einzelhebel: bis Faktor 8, und die
   Reparatur ist meist eine Zeile — Indexrollen tauschen oder AoS → SoA.)
3. **Werden wiederverwendbare Daten im Shared Memory eingelagert?** (Der einzige Weg, die
   Intensität wirklich zu erhöhen — dafür aber aufwendig und fehlerträchtig.)

**Die Begründung für genau diese Reihenfolge:** absteigender Hebel bei aufsteigendem Aufwand.
Es hat keinen Sinn, einen Kernel zu kacheln, der nur 15 % der Bandbreite bekommt, weil seine
Zugriffe strided sind — man optimiert dann sorgfältig an einem Engpass vorbei, der gar keiner
ist.

---

**Zurück:** [Theorie](theorie.md) · [Übungen](uebungen.md)
