# Kapitel 11 — CUDA I: Architektur, Ausführungsmodell und der erste Kernel

> **Quellen:** `vl/11-CUDA_Bosse-VL1.pdf` (Folien 1–47, Anhang 48–62),
> `vl/extra-material/Slurm_CUDA.pdf`, `vl/extra-material/cuda_matrix_add_2d_hints.pdf`,
> Übungsblatt `excercises/uebung9.pdf`
> **Zeitbedarf:** ca. 70–85 min lesen + 120 min Aufgaben
> **Voraussetzungen:** Kapitel 06 (Datenparallelität, Scoping), Kapitel 04 (Speicherlatenz),
> hilfreich: Kapitel 10 (Amdahl)

> **Codekonvention:** Aller Code hier ist CUDA-C++17 (`nvcc -std=c++17 -arch=sm_80`). Wie in
> den OpenMP-Kapiteln gilt: modernes C++ für die Infrastruktur, **rohe Indexrechnung im
> Kernel** — genau so steht sie in der Klausur. Blöcke direkt aus den Folien sind mit
> `// Folie N` markiert und behalten den C-Stil der Vorlesung.

> **Wichtig — nichts davon läuft auf diesem Rechner.** Es gibt lokal weder `nvcc` noch eine
> NVIDIA-GPU. Der Code in [`code/`](code/) ist für den **Ara-Cluster** geschrieben und kommt
> mit `job.sbatch`. Der klausurrelevante Kern dieses Kapitels — Indexrechnung,
> Grid-Dimensionierung, Divergenz, Fehlersuche — ist **Papierarbeit** und braucht keine GPU.

---

## 1. Warum GPUs? Eine Zahl, die alles erklärt

Ein Zugriff auf den globalen GPU-Speicher kostet **400–800 Taktzyklen**. Eine
Fused-Multiply-Add-Instruktion kostet **4 Zyklen**. Das Verhältnis ist etwa 1 : 125.

Auf einer CPU löst man dieses Problem mit *Caches*: Man versucht, den langsamen Speicher gar
nicht erst zu berühren (Kapitel 04). Die GPU geht den entgegengesetzten Weg — sie **verdeckt**
die Latenz, statt sie zu vermeiden. Wartet ein Thread auf Daten, schaltet der Scheduler
schlicht auf einen anderen, rechenbereiten Thread um. Die Latenz verschwindet nicht; sie fällt
nur nicht mehr auf.

Wie viel Parallelität braucht man dafür? Das sagt **Little's Law**:

$$\text{Bytes gleichzeitig unterwegs} = \text{Bandbreite} \times \text{Latenz}$$

Für eine A100 (im Ara-Cluster verfügbar) mit 1555 GB/s, ~1,41 GHz Takt und 500 Zyklen Latenz:

$$500\ \text{Zyklen} / 1{,}41 \cdot 10^9\ \text{Hz} = 355\ \text{ns}$$
$$1555 \cdot 10^9\ \text{B/s} \times 355 \cdot 10^{-9}\ \text{s} \approx 551\,000\ \text{B}$$

Bei 4 Byte pro Thread (ein `float`) müssen also **rund 138 000 Threads gleichzeitig eine
offene Ladeoperation haben**, damit der Speicherbus überhaupt ausgelastet ist. Eine A100 kann
maximal 108 SMs × 2048 = 221 184 Threads gleichzeitig resident halten — es passt, aber nicht
mit viel Luft.

> **Das ist die eigentliche Botschaft des Kapitels.** „Viele Threads" ist bei der GPU kein
> Luxus und kein Optimierungstrick, sondern die *Voraussetzung dafür, dass die Hardware
> überhaupt arbeitet*. Wer 2000 Threads startet, nutzt eine A100 zu unter 1 %.

Die zweite Zahl: Eine A100 hat 6912 FP32-Recheneinheiten, der Laptop hier 8 logische Kerne.
Aber jede einzelne GPU-Einheit ist **langsamer** als ein CPU-Kern und hat keine
Sprungvorhersage. Der Handel ist explizit:

| | CPU — Latenz-Maschine | GPU — Durchsatz-Maschine |
|---|---|---|
| Optimiert auf | einzelne Aufgabe schnell fertig | viele Aufgaben pro Zeit |
| Kerne | wenige, komplex | sehr viele, einfach |
| Chipfläche für | Caches, Branch Prediction, OoO | Recheneinheiten, Register |
| Latenz | wird durch Caches **vermieden** | wird durch Threads **verdeckt** |
| Schlecht für | massiv datenparallele Arbeit | serielle Logik, Verzweigungen |

---

## 2. Die Kernidee in vier Sätzen

1. **Ein Kernel ersetzt eine datenparallele Schleife**: Die Schleife verschwindet, ihr Rumpf
   wird zum Kernel, und jeder Thread führt genau eine Iteration aus.
2. Jeder Thread beantwortet selbst die Frage **„Welches Element bearbeite ich?"** —
   ausschließlich aus seiner Position im Grid: `i = blockIdx.x * blockDim.x + threadIdx.x`.
3. Der **Host orchestriert**: allokieren → kopieren → starten → zurückkopieren → freigeben.
4. Je 32 Threads laufen als **Warp** im Gleichschritt; nehmen sie verschiedene Pfade, werden
   die Pfade nacheinander abgearbeitet (*warp divergence*).

---

## 3. Die Hardware

### 3.1 Vom Chip zur Lane

Eine GPU besteht aus vielen **Streaming Multiprocessors (SMs)** — bei einer A100 sind es 108.
Jeder SM enthält:

- mehrere **Skalarprozessoren (SPs)**, aufgeteilt in typischerweise **4 Subpartitionen** mit
  je einem eigenen **Warp-Scheduler**,
- getrennte FP32- und INT32-Lanes, deutlich **weniger FP64-Einheiten**, dazu SFUs
  (Spezialfunktionen wie `sin`, `rsqrt`) und Load/Store-Einheiten,
- **Tensor-Cores** für Matrix-Multiply-Accumulate (KI/HPC),
- einen sehr großen **Registersatz** (z. B. 65 536 32-Bit-Register pro SM),
- **konfigurierbaren L1-Cache / Shared Memory** (mehrere zehn bis hunderte kB).

Die genauen Zahlen wechseln mit jeder Generation; die **Struktur** ist das Prüfungsrelevante.

Zwei Konsequenzen, die man sich sofort merken sollte:

- **FP64 ist teuer.** Historisch und bis heute sind die FP64-Einheiten in der Minderheit
  (bei Consumer-Karten drastisch: Faktor 32 bis 64 langsamer als FP32). Wer `double` benutzt,
  wo `float` reicht, verschenkt auf einer GeForce fast die gesamte Rechenleistung.
- **Register sind der eigentliche Reichtum der GPU.** Ein SM hält die Register *aller*
  residenten Threads gleichzeitig vor. Deshalb ist ein Threadwechsel gratis — es muss nichts
  gesichert werden. Genau das macht Latency Hiding erst bezahlbar.

### 3.2 Der Warp

**Definition:** Ein **Warp** ist eine Gruppe von **32 Threads**, die die Hardware gemeinsam
plant und ausführt. Alle 32 Lanes holen dieselbe Instruktion, arbeiten aber auf eigenen
Registern und damit auf eigenen Daten.

Der Warp ist die Einheit, in der die GPU wirklich denkt — nicht der Thread. Daraus folgt
fast alles Praktische:

| Beobachtung | Grund |
|---|---|
| `blockDim` sollte ein Vielfaches von 32 sein | ein Block mit 100 Threads belegt 4 Warps = 128 Lanes; 28 Lanes sind dauerhaft leer |
| Ein `if` kann die halbe Leistung kosten | Divergenz, siehe 3.3 |
| Benachbarte Threads sollten benachbarte Adressen lesen | die 32 Zugriffe eines Warps werden zu wenigen Speichertransaktionen zusammengefasst (*coalescing*, Kapitel 12) |
| Ein einzelner Thread ist bedeutungslos | er kostet dieselbe Instruktionsbandbreite wie 32 |

### 3.3 SIMT vs. SIMD und die Divergenz

In Flynns Klassifikation ist die GPU-Hardware **SIMD**: eine Instruktion, viele Datenpfade.
Das dem Programmierer gezeigte Modell heißt aber **SIMT** — *Single Instruction, Multiple
Threads*:

| | SIMD (z. B. AVX auf der CPU) | SIMT (GPU) |
|---|---|---|
| Programmierer sieht | ein **Vektorregister** mit 8 `double` | 32 **eigenständige Threads** |
| Index | man rechnet ihn selbst aus | jeder Thread hat eine eigene Identität |
| Verzweigung pro Element | nur über Masken von Hand | einfach `if (…)` schreiben |
| Kosten der Verzweigung | dieselben | dieselben |

Der letzte Punkt ist die Falle: SIMT **erlaubt** jedem Thread einen eigenen Pfad, aber die
Hardware ist weiterhin SIMD. Weichen die Pfade innerhalb eines Warps voneinander ab, führt sie
die Hardware **nacheinander** aus und maskiert dabei jeweils die nicht beteiligten Lanes.

```cpp
// Folie 17 — Original der Vorlesung (C-Stil)
if (threadIdx.x % 2) A(); else B();
```

Gerade Lanes machen `B()`, ungerade `A()` — **in jedem Warp**. Also läuft erst `A()` mit 16
aktiven Lanes, dann `B()` mit 16 aktiven Lanes: doppelte Zeit, halbe Auslastung.

Und die Gegenprobe, die man in der Klausur können muss:

```cpp
if (threadIdx.x / 32 % 2) A(); else B();   // keine Divergenz!
```

Hier ist die Bedingung **innerhalb jedes Warps konstant** — Warp 0 nimmt komplett den
`else`-Zweig, Warp 1 komplett den `if`-Zweig. Verschiedene Warps dürfen beliebig
auseinanderlaufen; das kostet **nichts**.

> **Merksatz:** Divergenz entsteht nur *innerhalb* eines Warps, nie zwischen Warps. Wer
> verzweigen muss, richtet die Verzweigung an Warp-Grenzen aus.

Der `if (i < n)`-Schutz am Kernel-Anfang ist übrigens auch Divergenz — aber nur im **letzten**
Block und dort nur in **einem** Warp. Bei 4000 Blöcken ist das irrelevant.

---

## 4. Das Programmiermodell

### 4.1 Die zwei Hierarchien

CUDA trennt bewusst, *wie man Parallelität ausdrückt*, von *wie die Hardware sie ausführt*:

| Software (CUDA) | Hardware |
|---|---|
| **Thread** | Lane einer SM-Subpartition |
| **Warp** (32 Threads) | 32 Lanes im Gleichschritt (SIMT) |
| **Block** (bis 1024 Threads) | resident auf **einem** SM |
| **Grid** (alle Blöcke) | verteilt über die SMs der GPU |

Diese Trennung ist der Grund, warum derselbe Code auf einer kleinen und einer großen GPU
läuft: **Blöcke sind voneinander unabhängig** und dürfen in beliebiger Reihenfolge, parallel
oder nacheinander, abgearbeitet werden. Eine GPU mit 2 SMs nimmt sich 2 Blöcke auf einmal,
eine mit 108 SMs entsprechend mehr — *automatische Skalierung*.

> **Die harte Regel, die daraus folgt:** Es gibt **keine Synchronisation zwischen Blöcken**.
> Wer sie braucht, muss den Kernel beenden und einen zweiten starten — das Kernel-Ende ist die
> einzige globale Barriere. Jede Annahme über die Reihenfolge, in der Blöcke laufen, ist ein
> Fehler.

Was ein Block dagegen kann: `__syncthreads()` (Barriere für alle Threads *dieses* Blocks) und
gemeinsamer **Shared Memory**.

### 4.2 Die eingebauten Variablen

Jeder Thread findet in vier vordefinierten Variablen vom Typ `dim3` (Felder `.x`, `.y`, `.z`),
wo er steht:

| Variable | Bedeutung | Beispiel bei `<<<40, 256>>>` |
|---|---|---|
| `threadIdx` | Index **im Block** | 0 … 255 |
| `blockIdx` | Index **im Grid** | 0 … 39 |
| `blockDim` | Threads **pro Block** | 256 |
| `gridDim` | Blöcke **im Grid** | 40 |

Grenzen (Compute Capability ≥ 3.0):

- Threads pro Block: **maximal 1024** (und `blockDim.x * blockDim.y * blockDim.z ≤ 1024`)
- `blockDim.z ≤ 64`
- Blöcke: `gridDim.x ≤ 2³¹ − 1`, `gridDim.y`, `gridDim.z ≤ 65 535`

### 4.3 Die drei Funktions-Qualifizierer

```cpp
__host__   void f();   // läuft auf der CPU, wird vom Host gerufen  (Standard)
__device__ int  g();   // läuft auf der GPU, wird vom Device gerufen
__global__ void k();   // Kernel: läuft auf der GPU, wird vom HOST gestartet
```

- `__global__` muss **immer `void`** zurückgeben — Ergebnisse kommen über Zeiger heraus.
- `__host__ __device__` zusammen ist erlaubt und sehr nützlich: Der Compiler erzeugt zwei
  Fassungen derselben Funktion, sodass sie in der CPU-Referenzimplementierung *und* im Kernel
  benutzt werden kann.
- Quelldateien haben die Endung **`.cu`** und dürfen Host- und Device-Code mischen; `nvcc`
  trennt sie und reicht den Host-Teil an den normalen C++-Compiler weiter.

---

## 5. Vom `for`-Loop zum Kernel

Das ist die zentrale Denkfigur des ganzen Kapitels. Ausgangspunkt — eine gewöhnliche
datenparallele Schleife:

```cpp
// Folie 22 — Original der Vorlesung (C-Stil)
int increment(int a) { return a + 1; }

void host(const int *a, int *b, int n) {
    for (int i = 0; i < n; ++i)
        b[i] = increment(a[i]);
}
```

Jede Iteration ist unabhängig — im Sinne von Kapitel 07: die Bernstein-Bedingungen sind für
je zwei Iterationen erfüllt. Und derselbe Rumpf als Kernel:

```cpp
// Folie 23 — Original der Vorlesung (C-Stil)
__host__ __device__ int increment(int a) { return a + 1; }

__global__ void kernel(const int *a, int *b, int n) {
    int tid  = threadIdx.x;             // Thread-Index im Block
    int bid  = blockIdx.x;              // Block-Index im Grid
    int bdim = blockDim.x;              // Threads pro Block
    int i    = tid + bid * bdim;        // globaler Index
    if (i < n)                          // Schutz: Grid kann zu gross sein
        b[i] = increment(a[i]);
}
```

Drei Dinge sind passiert, und man sollte sie einzeln benennen können:

1. **Die Schleife ist weg.** Der Schleifenzähler `i` wird nicht mehr hochgezählt, sondern aus
   der Thread-Identität *berechnet*.
2. **Ein Wächter ist dazugekommen.** `if (i < n)` — dazu gleich mehr.
3. **Die Funktion `increment` wurde `__host__ __device__`**, damit derselbe Rechenkern auf
   beiden Seiten benutzbar bleibt.

### 5.1 Der Start: die Ausführungskonfiguration

```cpp
// Folie 24 — Original der Vorlesung (C-Stil)
int threads = 256;                            // Threads pro Block
int blocks  = (n + threads - 1) / threads;    // Division mit Aufrundung
kernel<<<blocks, threads>>>(d_a, d_b, n);
```

Die Tripelklammern `<<< … >>>` sind kein C++ — es ist `nvcc`-Syntax für die **Geometrie des
Grids**. Die Vollform lautet `<<<grid, block, smBytes, stream>>>`; die letzten beiden Argumente
(dynamischer Shared Memory, Stream) sind optional und kommen in Kapitel 12.

**Die Aufrundungsdivision muss man auswendig können.** `(n + threads - 1) / threads` ist
gleichbedeutend mit ⌈n/threads⌉. Ein Zahlenbeispiel:

| n | threads | `n / threads` (falsch) | `(n+255)/256` (richtig) | gestartete Threads | ungenutzt |
|---|---|---|---|---|---|
| 1024 | 256 | 4 | 4 | 1024 | 0 |
| 1000 | 256 | 3 → **24 Elemente fehlen** | 4 | 1024 | 24 |
| 1 | 256 | 0 → **Kernel macht nichts** | 1 | 256 | 255 |

Abrunden ist der klassische Anfängerfehler: Der Code läuft, liefert keine Fehlermeldung und
lässt hinten einfach ein paar Elemente unberechnet.

### 5.2 Warum `if (i < n)` nicht optional ist

Sobald `blocks * threads > n` gilt — und durch das Aufrunden gilt es fast immer — existieren
Threads ohne zugehöriges Datenelement. Ohne den Wächter schreiben sie **hinter das Array**.
Das ist kein harmloser Schönheitsfehler:

- Im günstigen Fall überschreibt man fremde Daten auf dem Device und bekommt später falsche
  Ergebnisse — ohne jede Meldung.
- Im ungünstigen Fall ist es ein Zugriff außerhalb der Allokation, und der Kernel bricht mit
  `an illegal memory access was encountered` ab. Danach ist der **gesamte CUDA-Kontext
  unbrauchbar**: Jeder folgende CUDA-Aufruf im selben Prozess liefert denselben Fehler.

> **Merksatz:** Aufrunden beim Grid, Abfangen im Kernel. Die beiden gehören immer zusammen.

### 5.3 Die drei Kernelmuster

Fast jeder Kernel dieses Kapitels folgt einem von drei Mustern:

**(a) Ein Thread pro Element, 1D** — der Normalfall:

```cpp
__global__ void skaliere(float* y, const float* x, float alpha, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        y[i] = alpha * x[i];
}
```

**(b) Ein Thread pro Element, 2D** — für Bilder und Matrizen:

```cpp
__global__ void invertiere(unsigned char* img, int W, int H) {   // nach Folie 27
    int x = blockIdx.x * blockDim.x + threadIdx.x;   // Spalte
    int y = blockIdx.y * blockDim.y + threadIdx.y;   // Zeile
    if (x < W && y < H)
        img[y * W + x] = 255 - img[y * W + x];       // 2D -> 1D linearisiert
}
```

Gestartet mit:

```cpp
dim3 threads(16, 16);                                  // 256 Threads pro Block
dim3 blocks((W + 15) / 16, (H + 15) / 16);             // genug Bloecke fuer WxH
invertiere<<<blocks, threads>>>(d_img, W, H);
```

Drei Dinge, die hier regelmäßig schiefgehen:

- **`.x` ist die Spalte, `.y` ist die Zeile.** Das fühlt sich verdreht an, ist aber die
  Konvention — und sie ist die richtige: `threadIdx.x` läuft innerhalb des Warps am
  schnellsten, und bei Row-Major-Speicherung liegen benachbarte *Spalten* benachbart im
  Speicher. Vertauscht man x und y, liest jeder Warp 32 Elemente mit Abstand `cols`
  auseinander — korrekt, aber um ein Vielfaches langsamer (Kapitel 12).
- **Der Wächter muss zweidimensional sein:** `if (x < W && y < H)`. Beide Ränder können
  überstehen.
- **Die Linearisierung** `i = zeile * cols + spalte`: „überspringe `zeile` vollständige
  Zeilen zu je `cols` Einträgen, gehe dann `spalte` Positionen hinein". Für `cols = 700`
  liegt Eintrag (3, 5) an Index 3 · 700 + 5 = 2105.

**(c) Grid-Stride-Loop** — wenn mehr Daten als Threads da sind:

```cpp
// Folie 28 — Original der Vorlesung (C-Stil)
__global__ void scale(float *a, float s, int n) {
    int i      = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;         // = Gesamtzahl Threads
    for (; i < n; i += stride)
        a[i] *= s;
}
```

Warum das oft die bessere Fassung ist:

- Sie **entkoppelt die Problemgröße von der Startkonfiguration**. Derselbe Kernel läuft mit
  `<<<1,1>>>` (korrekt, aber langsam — gut zum Debuggen!) wie mit `<<<10000,256>>>`.
- Die Grid-Größe wird zum **Tuning-Parameter**: Man wählt sie so, dass die GPU gerade voll
  besetzt ist (z. B. 4 × Anzahl SMs), statt sie von `n` diktieren zu lassen.
- Die Zugriffe bleiben **coalesced**: In jedem Durchlauf lesen die 32 Lanes eines Warps
  weiterhin 32 aufeinanderfolgende Adressen. Die naive Alternative — „Thread `t` bearbeitet
  die Elemente `t*k` bis `t*k+k-1`" — zerstört genau das und ist deutlich langsamer.
- Der Wächter `i < n` steckt schon in der Schleifenbedingung.

---

## 6. Speicher und der Host-Ablauf

### 6.1 Die Speicherhierarchie

| Ebene | Sichtbar für | Geschwindigkeit | Verwaltet von |
|---|---|---|---|
| **Register** | einen Thread | am schnellsten (1 Zyklus) | Compiler |
| **Shared Memory** | einen Block | sehr schnell (~20–30 Zyklen) | **Programmierer** |
| **L1 / L2-Cache** | SM / ganze GPU | schnell | Hardware |
| **Globaler Speicher** | alle Threads + Host | langsam (400–800 Zyklen) | Programmierer |
| **Konstanten / Textur** | alle Threads (nur lesen) | gecacht | Programmierer |
| **Host-Speicher** | nur die CPU | eigener Adressraum! | Programmierer |

Zwei Dinge daran sind neu gegenüber der CPU:

1. **Shared Memory ist ein software-verwalteter Scratchpad**, kein Cache. Man schreibt selbst
   hinein und liest selbst heraus. Das ist der große Hebel in Kapitel 12.
2. **Host und Device haben getrennte Adressräume.** Ein Device-Zeiger sieht aus wie ein
   normaler `float*`, aber ihn auf dem Host zu dereferenzieren ist ein Segfault — und
   umgekehrt ist ein Host-Zeiger im Kernel ein illegaler Zugriff. Der Compiler warnt nicht.
   Deshalb die Namenskonvention `h_x` / `d_x`, die man wirklich durchhalten sollte.

> **Die Kunst der CUDA-Performance besteht darin, Daten möglichst weit oben in dieser
> Hierarchie zu halten.**

### 6.2 Die fünf Schritte

**Eselsbrücke: allokieren – kopieren – starten – zurückkopieren – freigeben.**

```cpp
// 1. Device-Speicher allokieren
float *d_x = nullptr, *d_y = nullptr;
cudaMalloc(&d_x, n * sizeof(float));
cudaMalloc(&d_y, n * sizeof(float));

// 2. Eingabe Host -> Device
cudaMemcpy(d_x, h_x.data(), n * sizeof(float), cudaMemcpyHostToDevice);

// 3. Kernel starten
int threads = 256;
int blocks  = (n + threads - 1) / threads;
skaliere<<<blocks, threads>>>(d_y, d_x, alpha, n);

// 4. Ergebnis Device -> Host
cudaMemcpy(h_y.data(), d_y, n * sizeof(float), cudaMemcpyDeviceToHost);

// 5. Freigeben
cudaFree(d_x);
cudaFree(d_y);
```

Zwei Details, die man beim Abschreiben verliert:

- `cudaMalloc(&d_x, …)` bekommt die **Adresse des Zeigers**, nicht den Zeiger. In der
  Vorlesungsfassung steht dafür `cudaMalloc((void**)&a_dev, …)` — der Cast ist in C++ nicht
  nötig, weil `cudaMalloc` als Template überladen ist.
- Die **Richtung** von `cudaMemcpy` ist explizit und wird nicht geprüft. `cudaMemcpyHostToDevice`
  mit vertauschten Argumenten kopiert fröhlich in die falsche Richtung.

Die Größe ist immer `anzahl * sizeof(T)` — nie die Elementzahl allein. Das ist der zweite
Klassiker: ein Kernel, der nur ein Viertel des Arrays sieht, weil `sizeof(float)` vergessen
wurde.

### 6.3 Unified Memory — die Abkürzung

```cpp
// Folie 33 — Original der Vorlesung (C-Stil)
int *a;
cudaMallocManaged(&a, n*sizeof(int));   // von Host UND Device adressierbar
fillA(a, n);                            // Host-Zugriff
kernel<<<blocks, threads>>>(a, a, n);   // Device-Zugriff
cudaDeviceSynchronize();                // vor erneutem Host-Zugriff!
checkResult(a, n);
cudaFree(a);
```

Ein Zeiger statt zwei, keine expliziten Kopien — das System migriert die Seiten bei Bedarf.
Weniger Code, weniger Fehlerquellen, ideal zum Einstieg.

Der Preis: Die Migration passiert **implizit** und ist nicht sichtbar. Ein Kernel, der auf
frisch beschriebene Host-Daten zugreift, löst tausende Page Faults aus. Für maximale Kontrolle
— und für alles, was gemessen werden soll — bleibt explizites `cudaMemcpy`.

Und: `cudaDeviceSynchronize()` **vor jedem Host-Zugriff nach einem Kernel** ist bei Unified
Memory Pflicht. Bei der expliziten Variante synchronisiert `cudaMemcpy` implizit, hier gibt
es nichts, das das für einen erledigt.

---

## 7. Synchronisation — wer wartet auf wen?

Die wichtigste Eigenschaft zuerst: **Kernel-Starts sind asynchron.** Die Zeile
`kernel<<<…>>>(…)` reiht den Kernel in eine Warteschlange ein und kehrt sofort zurück — der
Host läuft weiter, während die GPU noch rechnet.

| Mechanismus | Wer wartet | Worauf |
|---|---|---|
| `__syncthreads()` | alle Threads **eines Blocks** | dass alle den Punkt erreicht haben |
| Kernel-Ende | — | die **einzige** globale Barriere über alle Blöcke |
| `cudaDeviceSynchronize()` | der **Host** | dass das Device alles abgearbeitet hat |
| `cudaMemcpy` | der **Host** | synchronisiert implizit (blockiert bis fertig) |

Was es **nicht** gibt: eine Barriere über Blockgrenzen hinweg. Ein Block kann nicht auf einen
anderen warten — der andere ist vielleicht noch gar nicht gestartet, weil kein SM frei ist.
Warten würde deadlocken. Wer blockübergreifende Synchronisation braucht, teilt in zwei Kernels.

`__syncthreads()` hat außerdem eine scharfe Regel: **alle Threads des Blocks müssen es
erreichen.** Steht es in einem divergenten `if`, ist das Verhalten undefiniert — in der Praxis
ein Hänger.

```cpp
if (threadIdx.x < 100) {
    __syncthreads();        // FALSCH, wenn blockDim.x > 100
}
```

---

## 8. Messen und Fehler prüfen

### 8.1 Zeitmessung

Weil Kernel-Starts asynchron sind, misst eine naive Host-Uhr um den Kernel-Aufruf nur die
Zeit für das *Einreihen* — typischerweise wenige Mikrosekunden, völlig unabhängig von `n`.
**Wer eine Kernel-Zeit misst, die nicht mit `n` wächst, hat das Synchronisieren vergessen.**

Zwei korrekte Wege. Erstens `std::chrono` mit explizitem Synchronisieren:

```cpp
const auto t0 = std::chrono::steady_clock::now();
skaliere<<<blocks, threads>>>(d_y, d_x, alpha, n);
cudaDeviceSynchronize();                             // ohne diese Zeile misst man nichts
const auto t1 = std::chrono::steady_clock::now();
const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
```

Zweitens — und das ist die CUDA-Art — **Events**. Sie werden in den GPU-Strom eingereiht und
von der GPU selbst gestempelt; Overhead des Hosts fällt heraus:

```cpp
// Folie 42 — Original der Vorlesung (C-Stil)
float ms;
cudaEvent_t start, stop;
cudaEventCreate(&start);
cudaEventCreate(&stop);
cudaEventRecord(start);
kernel<<<grid, threads>>>(a_dev, b_dev, n);
cudaEventRecord(stop);
cudaEventSynchronize(stop);                  // auf GPU-Fertigstellung warten
cudaEventElapsedTime(&ms, start, stop);      // verstrichene Zeit in ms
```

`cudaEventElapsedTime` liefert **Millisekunden als `float`** — nicht Sekunden, nicht `double`.
Auflösung etwa 0,5 µs.

Drei Messhygiene-Regeln, die auf der GPU noch wichtiger sind als auf der CPU:

1. **Ersten Aufruf verwerfen.** Der erste CUDA-Aufruf im Prozess initialisiert den Kontext:
   Das kostet einmalig einige hundert Millisekunden und hat mit dem Kernel nichts zu tun.
2. **Mehrfach messen, Minimum nehmen** — dieselbe Begründung wie in `ec::best_of`.
3. **Getrennt ausweisen, was gemessen wird**: Kernel allein oder Kernel + Transfers? Beides
   ist legitim, aber der Unterschied ist oft ein Faktor 10 (Abschnitt 9).

### 8.2 Fehlerbehandlung

Jeder CUDA-Host-Aufruf gibt ein `cudaError_t` zurück (0 = `cudaSuccess`). Wer es nicht prüft,
merkt vom Fehlschlag **nichts** — der Kernel läuft einfach nicht, das Ergebnisarray enthält,
was vorher drinstand, und das Programm meldet Erfolg. Das ist die häufigste und
frustrierendste Anfängerfalle.

Kernel-Starts sind der Sonderfall: Sie geben gar nichts zurück, und weil sie asynchron sind,
gibt es **zwei** verschiedene Fehlerklassen:

```cpp
kernel<<<blocks, threads>>>(d_y, d_x, alpha, n);
cudaError_t err = cudaGetLastError();        // Start-Fehler (z. B. zu grosser Block)
if (err != cudaSuccess)
    std::fprintf(stderr, "Start: %s\n", cudaGetErrorString(err));
err = cudaDeviceSynchronize();               // Laufzeit-Fehler (z. B. illegaler Zugriff)
if (err != cudaSuccess)
    std::fprintf(stderr, "Lauf:  %s\n", cudaGetErrorString(err));
```

In der Praxis kapselt man das in ein Makro — die Fassung aus `Slurm_CUDA.pdf` (Folie 26):

```cpp
#define CUDA_CHECK(call)                                                     \
    do {                                                                     \
        cudaError_t err__ = (call);                                          \
        if (err__ != cudaSuccess) {                                          \
            std::fprintf(stderr, "CUDA error at %s:%d: %s\n",                \
                         __FILE__, __LINE__, cudaGetErrorString(err__));     \
            std::exit(EXIT_FAILURE);                                         \
        }                                                                    \
    } while (0)

CUDA_CHECK(cudaMalloc(&d_x, n * sizeof(float)));
CUDA_CHECK(cudaGetLastError());
CUDA_CHECK(cudaDeviceSynchronize());
```

Das `do { … } while (0)` ist kein Zierrat: Es macht das Makro zu einer einzelnen Anweisung,
sodass `if (x) CUDA_CHECK(…); else …` nicht zerbricht.

---

## 9. Wie viel bringt die GPU wirklich? Amdahl und der PCIe-Bus

Die GPU beschleunigt **nur den ausgelagerten Teil**. Mit dem seriellen Anteil $f$ und $p$
parallelen Einheiten gilt weiterhin Amdahl (Kapitel 10):

$$S(p) = \frac{1}{f + \frac{1-f}{p}} \xrightarrow{p \to \infty} S_{\max} = \frac{1}{f}$$

Bei $f = 0{,}05$ ist bei Speedup 20 Schluss — mit **beliebig vielen** Kernen.

Auf der GPU kommt ein zweiter, oft schwererer Posten dazu: **die Datenübertragung über PCIe.**
Sie ist reiner Zusatzaufwand, den die CPU-Fassung nicht hat. Rechnen wir das für unser
`y ← αx` durch (n Elemente, `float`):

| Posten | Datenmenge | Zeit bei 12 GB/s PCIe |
|---|---|---|
| x hinüber | 4n Byte | 4n / 12·10⁹ s |
| y zurück | 4n Byte | 4n / 12·10⁹ s |
| **Transfer gesamt** | **8n Byte** | **8n / 12·10⁹ s** |

Der Kernel selbst liest 4n und schreibt 4n Byte im Device-Speicher, bei ~1500 GB/s:

$$T_{\text{Kernel}} = \frac{8n}{1500 \cdot 10^9}\ \text{s}$$

Das Verhältnis ist $1500/12 = 125$. **Die Übertragung dauert 125-mal so lange wie die
Rechnung.** Für einen so einfachen Kernel ist die GPU damit chancenlos: Die CPU hätte das
Ergebnis längst, bevor die Daten überhaupt drüben sind.

> **Die Konsequenz, die man verstanden haben muss:** Ein Kernel lohnt sich erst, wenn er pro
> übertragenem Byte **genug Arbeit** leistet. `y ← αx` (2 Flop pro 8 Byte) lohnt nie. Eine
> Matrixmultiplikation (2n³ Flop pro 3n² übertragene Werte) lohnt fast immer. Das ist genau
> die **arithmetische Intensität** aus dem Roofline-Modell (Kapitel 10 und 12).
>
> Die zweite Konsequenz: Daten möglichst **auf der GPU lassen** und mehrere Kernels
> hintereinander darauf laufen lassen, statt nach jedem Schritt zurückzukopieren.

Deshalb steht in jedem ernstzunehmenden Benchmark **beides**: Kernel-Zeit *und* Zeit
inklusive Transfer. Wer nur die Kernel-Zeit zeigt, macht eine Aussage über die Hardware, nicht
über das Programm.

---

## 10. Praxis: Ara-Cluster und Slurm

CUDA-Code wird lokal geschrieben, aber **auf dem Cluster übersetzt und ausgeführt**. Auf dem
Login-Knoten läuft nichts Rechenintensives.

```bash
ssh <urz-kuerzel>@login1.ara.uni-jena.de

module avail                        # was gibt es?
module load nvidia/cuda/12.4        # Compiler + Runtime laden
nvcc -std=c++17 -O2 -arch=sm_80 vector_scale.cu -o vector_scale

sbatch job.sbatch                   # Job einreichen
squeue -u $USER                     # Warteschlange ansehen
scancel <jobid>                     # Job abbrechen
```

Das Job-Skript (nach `Slurm_CUDA.pdf`, Folie 7):

```bash
#!/bin/bash
#SBATCH --job-name=cuda_test
#SBATCH --output=out.txt
#SBATCH --time=00:10:00
#SBATCH --gres=gpu:1          # ohne diese Zeile bekommt man KEINE GPU
#SBATCH --cpus-per-task=4
#SBATCH --mem=4G

module load nvidia/cuda/12.4
./main
```

`--gres=gpu:1` ist die Zeile, die am häufigsten fehlt. Ohne sie läuft der Job auf einem Knoten
ohne GPU, und der erste CUDA-Aufruf scheitert mit `no CUDA-capable device is detected`.

Zum Interaktivarbeiten (Debuggen, kurze Tests):

```bash
salloc --gres=gpu:1 --time=00:30:00     # Ressourcen reservieren
srun ./main                             # darin starten
```

**Die richtige Architektur wählen.** `-arch=sm_XX` legt fest, für welche Compute Capability
übersetzt wird:

| GPU im Ara/Draco-Cluster | Architektur | Flag |
|---|---|---|
| Tesla P100 | Pascal | `-arch=sm_60` |
| Tesla V100 | Volta | `-arch=sm_70` |
| Tesla A100 | Ampere | `-arch=sm_80` |

Übersetzt man für eine zu neue Architektur, scheitert der Start mit `no kernel image is
available for execution on the device`. Nützlich noch: `nvcc … -Xptxas -v` zeigt den
Register- und Shared-Memory-Verbrauch pro Kernel (wichtig für Occupancy, Kapitel 12), und
`nvidia-smi` im Job-Skript verrät, welche Karte man tatsächlich bekommen hat.

---

## 11. Typische Klausurfragen

- **Wie berechnet ein Thread seinen globalen Index (1D)?** — `blockIdx.x * blockDim.x +
  threadIdx.x`.
- **Warum `(n + t - 1) / t` und nicht `n / t`?** — Aufrunden; sonst bleiben die letzten
  `n mod t` Elemente unberechnet.
- **Warum braucht es trotzdem `if (i < n)`?** — Weil das Aufrunden mehr Threads erzeugt als
  Elemente vorhanden sind; ohne Wächter Zugriff hinter das Array.
- **Was ist ein Warp, wie groß ist er?** — Die Ausführungseinheit der GPU, 32 Threads, die
  dieselbe Instruktion im Gleichschritt ausführen.
- **Was ist Warp-Divergenz und wann tritt sie auf?** — Threads *desselben* Warps nehmen
  verschiedene Pfade; die Pfade werden serialisiert. Zwischen Warps gibt es keine Divergenz.
- **Unterschied SIMD und SIMT?** — SIMD: ein Vektorregister, Index selbst rechnen. SIMT: viele
  Threads mit eigener Identität und eigenen Registern, dürfen logisch eigene Pfade nehmen —
  auf SIMD-Hardware ausgeführt.
- **Warum sind Blöcke unabhängig?** — Damit derselbe Code auf GPUs mit beliebig vielen SMs
  skaliert; der Scheduler verteilt sie in beliebiger Reihenfolge.
- **Welche Synchronisationsmöglichkeiten gibt es?** — `__syncthreads()` innerhalb eines Blocks,
  `cudaDeviceSynchronize()` für den Host, Kernel-Ende als einzige globale Barriere. Keine
  Barriere zwischen Blöcken.
- **Nenne die fünf Host-Schritte.** — Allokieren, kopieren, starten, zurückkopieren, freigeben.
- **Was bedeuten `__host__`, `__device__`, `__global__`?** — CPU / GPU-von-GPU / Kernel
  (GPU, vom Host gestartet, immer `void`).
- **Warum reicht eine Host-Zeitmessung um den Kernel-Aufruf nicht?** — Kernel-Starts sind
  asynchron; ohne Synchronisation misst man die Einreihzeit.
- **Warum lohnt sich die GPU bei `y ← αx` selten?** — Zu geringe arithmetische Intensität; die
  PCIe-Übertragung dominiert die Rechenzeit um zwei Größenordnungen.

---

## 12. Fallstricke

| Fehler | Warum falsch | Richtig |
|---|---|---|
| `int i = threadIdx.x;` als globaler Index | nur korrekt bei genau einem Block; sonst bearbeiten alle Blöcke dieselben Elemente | `blockIdx.x * blockDim.x + threadIdx.x` |
| `blocks = n / threads` | rundet ab, letzte Elemente bleiben unberechnet | `(n + threads - 1) / threads` |
| kein `if (i < n)` | Zugriff hinter das Array; stiller Datenfehler oder toter Kontext | Wächter immer schreiben |
| Host-Zeiger an Kernel übergeben | getrennte Adressräume; Compiler warnt nicht | `h_` / `d_`-Konvention durchhalten |
| `cudaMalloc(d_x, …)` statt `&d_x` | Zeiger wird by-value übergeben, bleibt draußen `nullptr` | Adresse des Zeigers übergeben |
| Größe ohne `sizeof(T)` | nur ein Viertel bzw. Achtel des Arrays allokiert/kopiert | `n * sizeof(float)` |
| Ergebnis lesen ohne Sync | Kernel läuft asynchron, Daten noch nicht fertig | `cudaMemcpy` (synchronisiert) oder `cudaDeviceSynchronize()` |
| Rückgabewerte nicht prüfen | Kernel scheitert still, Programm meldet Erfolg | `CUDA_CHECK`-Makro um **jeden** Aufruf |
| Nur `cudaGetLastError()` nach dem Start | fängt Start-, nicht Laufzeitfehler | zusätzlich synchronisierenden Aufruf prüfen |
| `blockDim.x = 100` | kein Vielfaches von 32; 28 von 128 Lanes dauerhaft leer | 128 / 256 / 512 |
| `blockDim.x = 2048` | Maximum ist 1024 Threads pro Block; Start scheitert | ≤ 1024, per `cudaGetLastError()` prüfen |
| `__syncthreads()` in divergentem `if` | nicht alle Threads erreichen die Barriere | vor das `if` ziehen |
| Annahme über Blockreihenfolge | Blöcke laufen in beliebiger Reihenfolge | zwei Kernels statt globaler Barriere |
| Host-Uhr ohne Sync um den Kernel | misst die Einreihzeit, nicht die Rechenzeit | Events oder `cudaDeviceSynchronize()` |
| Transferzeit im Benchmark weglassen | überzeichnet den Gewinn oft um Faktor 10+ | beides ausweisen |

---

## 13. Merkkasten

> **Kernaussagen**
> - Die GPU **verdeckt** Latenz durch massives Multithreading, statt sie wie die CPU per
>   Cache zu **vermeiden**. Deshalb ist „viele Threads" keine Optimierung, sondern die
>   Voraussetzung.
> - **Ein Kernel = eine datenparallele Schleife.** Die Schleife verschwindet, jeder Thread
>   fragt „Welches Element bearbeite ich?" und rechnet sein `i` selbst aus.
> - `i = blockIdx.x * blockDim.x + threadIdx.x` — die eine Formel, die immer da ist.
> - **Aufrunden beim Grid, Abfangen im Kernel.** `(n+t-1)/t` und `if (i < n)` gehören zusammen.
> - Software: Thread → Warp → Block → Grid. Hardware: Lane → 32 Lanes → SM → GPU.
> - **Warp = 32 Threads im Gleichschritt.** Divergenz entsteht nur innerhalb eines Warps.
> - **Blöcke sind unabhängig** und laufen in beliebiger Reihenfolge — das ist der Preis für
>   die Skalierbarkeit und der Grund, warum es keine globale Barriere gibt.
> - Host-Ablauf: **allokieren – kopieren – starten – zurückkopieren – freigeben.**
> - **Kernel-Starts sind asynchron.** Immer synchronisieren, mit Events messen, jeden Aufruf
>   auf Fehler prüfen.
> - Der Gewinn wird von Amdahl **und** vom PCIe-Bus begrenzt. Lohnend ist nur, was pro
>   übertragenem Byte viel rechnet.

---

## 14. Verbindung zum Rest der Vorlesung

**Setzt voraus:** Kapitel 06 — Datenparallelität und die Frage, wann Iterationen unabhängig
sind, ist dieselbe wie bei `#pragma omp parallel for`. Ein Kernel ist im Grunde ein
`parallel for` mit sehr vielen, sehr kleinen Iterationen. Kapitel 04 liefert das Verständnis
für Speicherlatenz, ohne das die Motivation der GPU nicht greifbar ist.

**Baut darauf auf:** Kapitel 12 (CUDA II) — dort geht es um die Frage, wie schnell ein
korrekter Kernel wirklich ist: *coalescing*, Shared Memory, Occupancy, Reduktionen,
`atomicAdd` und das Roofline-Modell auf der GPU. Blatt 9, Aufgabe 9.2 (Skalarprodukt mit
Shared Memory und `atomicAdd`) gehört bereits dorthin.

**Querverbindungen:** Kapitel 10 liefert Amdahl, Gustafson und Roofline — die Werkzeuge, mit
denen man in Abschnitt 9 überhaupt entscheiden kann, ob sich ein Kernel lohnt. Kapitel 05b
(SIMD) ist die CPU-seitige Verwandte von SIMT: dieselbe Hardware-Idee, ein anderes
Programmiermodell.

**Vergleich mit der eigenen Abgabe:** `assignment2/` hat `axpy` auf der CPU vermessen. Die
Aufgabe 11.4 in diesem Kapitel ist derselbe Kern auf der GPU — und die interessante Frage ist
nicht, ob der Kernel schneller ist, sondern warum er es trotz 6912 Recheneinheiten kaum sein
kann.

---

**Weiter:** [Übungen](uebungen.md) → danach [Lösungen](loesungen.md)
