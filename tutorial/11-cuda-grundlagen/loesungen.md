# Kapitel 11 — Lösungen: CUDA I

> Erst [`uebungen.md`](uebungen.md) selbst bearbeiten.

---

## Lösung 11.1 — Indexrechnung, hin und zurück

Gegeben: `<<<32, 128>>>`, also `gridDim.x = 32`, `blockDim.x = 128`, `n = 4000`.

**a)** Die Standardformel:

$$i = \text{blockIdx.x} \cdot \text{blockDim.x} + \text{threadIdx.x} = 11 \cdot 128 + 37 = 1408 + 37 = \boxed{1445}$$

Anschaulich: Die Blöcke 0 bis 10 haben zusammen 11 · 128 = 1408 Elemente abgedeckt; unser
Thread ist der 38. in Block 11.

**b)** Die Umkehrung ist Division mit Rest — man macht die Formel einfach rückwärts:

$$\text{blockIdx.x} = \left\lfloor \frac{i}{\text{blockDim.x}} \right\rfloor = \left\lfloor \frac{3000}{128} \right\rfloor = \lfloor 23{,}4375 \rfloor = 23$$
$$\text{threadIdx.x} = i \bmod \text{blockDim.x} = 3000 - 23 \cdot 128 = 3000 - 2944 = 56$$

Also **Block 23, Thread 56**. Probe: 23 · 128 + 56 = 3000. ✓

**c)** Gestartet werden `32 · 128 = 4096` Threads. Elemente gibt es 4000, also haben
**96 Threads** kein Element. Genau diese fängt `if (i < n)` ab.

**d)** Innerhalb eines Blocks werden die Threads in Warps zu 32 aufgeteilt, in der Reihenfolge
von `threadIdx.x`:

$$\text{Warp} = \left\lfloor 37/32 \right\rfloor = 1, \qquad \text{Lane} = 37 \bmod 32 = 5$$

Thread 37 ist also **Lane 5 in Warp 1** seines Blocks. Ein Block mit 128 Threads besteht aus
128/32 = 4 Warps.

**e)** Die arbeitslosen Threads haben `i ≥ 4000`. Der Block, in dem die Grenze liegt:

$$\lfloor 4000/128 \rfloor = 31{,}25 \rightarrow \text{Block } 31$$

Block 31 deckt `i = 3968 … 4095` ab. Davon sind `3968 … 3999` gültig — das sind genau
**32 Elemente**, also die Threads 0 … 31, und das ist **exakt Warp 0**.

Damit gilt:

| Warp in Block 31 | Threads | Status |
|---|---|---|
| 0 | 0–31 | alle 32 haben Arbeit |
| 1, 2, 3 | 32–127 | alle haben keine Arbeit |

**Es gibt keine Divergenz.** In jedem Warp ist die Bedingung `i < n` einheitlich — entweder
für alle 32 Lanes wahr oder für alle 32 falsch. Der Wächter kostet hier eine einzige
Vergleichsinstruktion und sonst nichts.

> Das ist kein Zufall, sondern die Regel: Divergenz durch den Wächter kann höchstens in
> **einem einzigen Warp** des **letzten** Blocks auftreten — nämlich dann, wenn `n` kein
> Vielfaches von 32 ist. Bei 32 Blöcken ist das im schlimmsten Fall 1 von 128 Warps. Der
> Wächter ist also gratis; wer ihn aus Performance-Gründen weglässt, hat sich verrechnet.

---

## Lösung 11.2 — Grid dimensionieren

`n = 1 000 000`.

**a)** Mit `t = 256`:

$$\text{blocks} = \left\lceil \frac{n}{t} \right\rceil = \frac{1\,000\,000 + 255}{256} = \frac{1\,000\,255}{256} = 3907{,}24\ldots \rightarrow \mathbf{3907}$$

(Die Ganzzahldivision schneidet ab, das Addieren von `t-1` vorher macht daraus das Aufrunden.)

Gestartete Threads: `3907 · 256 = 1 000 192`. Leer: `1 000 192 − 1 000 000 = 192`.

**b)** Der letzte Block hat `blockIdx.x = 3906` und deckt

$$i = 3906 \cdot 256 = 999\,936 \ \ldots\ 1\,000\,191$$

ab. Gültig davon sind `999 936 … 999 999`, das sind **64 Elemente** — die Threads 0 … 63.

$$64 = 2 \cdot 32$$

| Warp | Threads | Status |
|---|---|---|
| 0, 1 | 0–63 | vollständig aktiv |
| 2 … 7 | 64–255 | vollständig leer |
| — | — | **kein** teilweise aktiver Warp |

Also wieder **keine Divergenz**: Die Wächterkosten sind eine Vergleichsinstruktion pro Thread.
Ein teilweise aktiver Warp entstünde nur, wenn `n mod 32 ≠ 0` wäre; hier ist
`1 000 000 = 31 250 · 32`.

**c)** Mit `t = 250`:

$$\text{blocks} = \left\lceil \frac{1\,000\,000}{250} \right\rceil = 4000 \quad \text{(geht exakt auf)}$$

Aber: Ein Block mit 250 Threads belegt trotzdem `⌈250/32⌉ = 8` Warps, also **256 Lanes**.
`256 − 250 = 6` Lanes sind in **jedem einzelnen Block** dauerhaft ungenutzt.

$$\text{belegte Lanes} = 4000 \cdot 8 \cdot 32 = 1\,024\,000$$

Vergleich:

| Konfiguration | Blöcke | belegte Lanes | davon ungenutzt |
|---|---|---|---|
| `t = 256` | 3907 | 1 000 192 | **192** |
| `t = 250` | 4000 | 1 024 000 | **24 000** |

Das „passt besser" ist also genau falsch herum: 250 verschwendet 125-mal so viele Lanes.
Der Grund ist, dass die Hardware nicht in Threads, sondern in Warps abrechnet — sie kann
keinen halben Warp planen.

**d)** Mit `t = 1024`:

$$\text{blocks} = \left\lceil \frac{1\,000\,000}{1024} \right\rceil = 977, \qquad 977 \cdot 1024 = 1\,000\,448 \ \ (448 \text{ leer})$$

Zulässig ist das: 1024 ist genau das Maximum an Threads pro Block. Empfehlenswert ist es
trotzdem nicht — ein Block muss vollständig auf **einen** SM passen, und mit 1024 Threads
bleiben oft zu wenige Register bzw. zu wenig Shared Memory übrig, um mehrere Blöcke gleichzeitig
resident zu halten. Das senkt die Occupancy und damit die Fähigkeit zum Latency Hiding
(Kapitel 12). 128–512 ist die übliche Wahl.

**e)** `<<<1, 1000000>>>` überschreitet das Limit von **1024 Threads pro Block** um drei
Größenordnungen. Der Kernel wird gar nicht erst gestartet; `cudaGetLastError()` liefert
`cudaErrorInvalidConfiguration`, ausgegeben als **„invalid configuration argument"**.

Der springende Punkt ist der zweite Teil der Frage: Ohne Prüfung **passiert einfach nichts**.
Der Kernel-Start gibt keinen Wert zurück, das Programm läuft weiter, `cudaMemcpy` kopiert den
uninitialisierten Device-Puffer zurück, und man bekommt Nullen oder Müll — ohne eine einzige
Fehlermeldung. Deshalb `CUDA_CHECK(cudaGetLastError())` nach **jedem** Start.

**f)** Weil die Hardware Threads ausschließlich in Warps zu 32 plant: Jede Blockgröße, die
kein Vielfaches von 32 ist, erzeugt einen angebrochenen Warp, dessen überzählige Lanes die
gesamte Kernel-Laufzeit über mitlaufen, ohne etwas zu tun.

---

## Lösung 11.3 — Fehlersuche

**a) Die Fehler.**

| # | Zeile | Fehler | Wirkung |
|---|---|---|---|
| 1 | `int i = threadIdx.x;` | `blockIdx` fehlt im globalen Index | Jeder Block bearbeitet dieselben Elemente 0…255; 99,7 % des Vektors bleiben unberührt, und alle Blöcke schreiben konkurrierend auf dieselben Adressen |
| 2 | Kernel-Rumpf | kein `if (i < n)` | hier zufällig folgenlos (weil `i < 256 < n`), in der korrigierten Fassung aber zwingend — sonst Zugriff hinter das Array |
| 3 | `cudaMalloc(&d_x, n)` | `sizeof(float)` fehlt | es werden 100 000 **Byte** statt 400 000 Byte allokiert, also Platz für nur 25 000 `float`; jeder Zugriff darüber hinaus liegt außerhalb der Allokation |
| 4 | `cudaMemcpy(d_x, h_x.data(), …, cudaMemcpyDeviceToHost)` | Richtung falsch | die Eingabedaten kommen **nie** auf dem Device an; der Aufruf schlägt fehl bzw. tut nicht das Gemeinte, `d_x` bleibt uninitialisiert |
| 5 | `int blocks = n / threads;` | abgerundet statt aufgerundet | `100000/256 = 390` (statt 391) → `390 · 256 = 99 840`, die letzten **160 Elemente** blieben auch bei sonst korrektem Code unberechnet |
| 6 | `quadrat<<<threads, blocks>>>` | Argumente vertauscht | gestartet werden 256 Blöcke zu je 390 Threads — 390 ist zudem kein Vielfaches von 32 |
| 7 | `quadrat<<<…>>>(d_y, h_x.data(), n)` | **Host-Zeiger** an den Kernel | getrennte Adressräume; der Kernel dereferenziert eine für ihn ungültige Adresse → `an illegal memory access was encountered`, der Kernel bricht ab und der CUDA-Kontext ist danach unbrauchbar |
| 8 | überall | kein einziger Rückgabewert geprüft | genau deshalb meldet das Programm nichts, obwohl mehrere Aufrufe scheitern |
| 9 | Ende von `main` | `cudaFree` fehlt | Ressourcenleck; bei Prozessende zwar aufgeräumt, in Schleifen aber der sichere Weg in `out of memory` |

Zusatz: `h_y` ist als `std::vector<float>` wertinitialisiert, enthält also Nullen. Deshalb
gibt das Programm ausgerechnet ein *plausibel aussehendes* `y[7] = 0.000000` aus, statt zu
krachen — der schlechteste aller Fälle.

**b) Was ein `CUDA_CHECK` fängt.**

| # | wird gefangen? | Begründung |
|---|---|---|
| 1 | **nein** | ein logischer Fehler im Kernel-Code, kein API-Fehler — die Hardware hat nichts zu beanstanden |
| 2 | **indirekt** | erst wenn der Zugriff wirklich außerhalb liegt, dann als `illegal memory access` beim synchronisierenden Aufruf |
| 3 | **nein** (beim Allokieren) | 100 000 Byte zu allokieren ist völlig legal; der Fehler zeigt sich erst später als Zugriffsverletzung |
| 4 | **ja** | falsche Richtung → `cudaMemcpy` gibt einen Fehlercode zurück |
| 5 | **nein** | 390 Blöcke zu starten ist eine gültige Konfiguration; dass sie zu klein ist, kann die Runtime nicht wissen |
| 6 | **nein** | 256 × 390 ist ebenfalls gültig (390 ≤ 1024) — nur eben nicht gemeint |
| 7 | **ja** | `cudaGetLastError()` bleibt sauber (der Start selbst ist gültig), aber `cudaDeviceSynchronize()` meldet den Laufzeitfehler |
| 8 | — | ist der Grund, warum 4 und 7 unbemerkt bleiben |
| 9 | **nein** | ein Leck ist kein Fehlerzustand |

**Die Lehre daraus:** Fehlerprüfung ist notwendig, aber nicht hinreichend. Die drei
gefährlichsten Fehler (1, 5, 6) sind aus Sicht der Runtime völlig legaler Code. Gegen sie
hilft nur eine **Verifikation gegen eine serielle Referenz** — über den ganzen Vektor, nicht
an einer Stelle.

**c) Korrigiert.**

```cpp
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CUDA_CHECK(call)                                                     \
    do {                                                                     \
        cudaError_t err__ = (call);                                          \
        if (err__ != cudaSuccess) {                                          \
            std::fprintf(stderr, "CUDA error at %s:%d: %s\n",                \
                         __FILE__, __LINE__, cudaGetErrorString(err__));     \
            std::exit(EXIT_FAILURE);                                         \
        }                                                                    \
    } while (0)

__global__ void quadrat(float* y, const float* x, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;   // 1: blockIdx dazu
    if (i < n)                                       // 2: Waechter
        y[i] = x[i] * x[i];
}

int main() {
    const int n = 100000;
    const size_t bytes = n * sizeof(float);          // 3: sizeof nicht vergessen

    std::vector<float> h_x(n), h_y(n);
    for (int i = 0; i < n; ++i) h_x[i] = static_cast<float>(i);

    float *d_x = nullptr, *d_y = nullptr;
    CUDA_CHECK(cudaMalloc(&d_x, bytes));
    CUDA_CHECK(cudaMalloc(&d_y, bytes));

    CUDA_CHECK(cudaMemcpy(d_x, h_x.data(), bytes,
                          cudaMemcpyHostToDevice));  // 4: richtige Richtung

    const int threads = 256;
    const int blocks  = (n + threads - 1) / threads; // 5: aufrunden
    quadrat<<<blocks, threads>>>(d_y, d_x, n);       // 6+7: Reihenfolge, Device-Zeiger
    CUDA_CHECK(cudaGetLastError());                  // 8: Startfehler
    CUDA_CHECK(cudaDeviceSynchronize());             // 8: Laufzeitfehler

    CUDA_CHECK(cudaMemcpy(h_y.data(), d_y, bytes, cudaMemcpyDeviceToHost));

    // Verifikation ueber den GANZEN Vektor, nicht an einer Stelle
    int fehler = 0;
    for (int i = 0; i < n; ++i) {
        const float soll = h_x[i] * h_x[i];
        if (h_y[i] != soll) ++fehler;
    }
    std::printf("%s (%d Abweichungen)\n", fehler ? "FEHLER" : "OK", fehler);

    CUDA_CHECK(cudaFree(d_x));                       // 9
    CUDA_CHECK(cudaFree(d_y));
    return 0;
}
```

**d)** `y[7]` liegt im **allerersten** Block. Genau dieser eine Block wird von jeder der
verbleibenden Fehlerarten korrekt behandelt: Das Abrunden der Blockzahl trifft nur die letzten
160 Elemente, ein zu kleines `cudaMalloc` nur die oberen drei Viertel, ein fehlender Wächter
nur den letzten Block. Eine Stichprobe am Anfang des Vektors kann diese Fehler prinzipiell
nicht sehen.

Zuverlässig ist nur die Prüfung **aller** Elemente gegen eine serielle Referenz (wie oben).
Wer nur stichprobenartig prüfen will, muss mindestens den Anfang, die Mitte, das **letzte**
Element (`i = n-1`) und ein Element aus dem angebrochenen letzten Block ansehen — der Rand ist
immer die interessante Stelle.

---

## Lösung 11.4 — axpy auf der GPU

Vollständiges Programm: [`code/axpy.cu`](code/axpy.cu).

**a) Der Kernel.**

```cpp
__global__ void axpy(float* y, const float* x, float alpha, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        y[i] = alpha * x[i] + y[i];
}
```

Beachte: `y` wird gelesen **und** geschrieben — jeder Thread fasst genau sein eigenes `y[i]`
an, also gibt es trotzdem keine Race Condition. Genau das ist die Bedingung, unter der ein
Kernel überhaupt zulässig ist (Bernstein, Kapitel 07).

**b) Konfiguration.**

```cpp
const int threads = 256;
const int blocks  = (n + threads - 1) / threads;
```

Für beliebiges `n` korrekt, weil das Aufrunden nach oben und der Wächter im Kernel
zusammenspielen. `n` kommt aus `argv[1]`.

**c)/(d)** siehe [`code/axpy.cu`](code/axpy.cu). Geprüft wird gegen die serielle Referenz auf
dem Host, und zwar **alle** Elemente. Verglichen wird mit relativer Toleranz statt mit `==`:
Die GPU darf Multiplikation und Addition zu einem **FMA** zusammenziehen, das intern mit voller
Zwischenpräzision rechnet und deshalb ein *anderes* (genaueres!) Ergebnis liefert als die
getrennte CPU-Rechnung. Ein exakter Vergleich schlägt hier zu Recht fehl.

Wer nur stichprobenartig prüft, nimmt `i = 0`, `i = n/2`, `i = n-1` und ein Element aus dem
letzten, angebrochenen Block — die Ränder sind die Stellen, an denen Indexfehler sichtbar
werden.

**e)/(f) Was man sehen wird.** Die Größenordnungen lassen sich vorab ausrechnen; genau das
sollte man vor dem Messen tun, um zu wissen, ob die Messung plausibel ist.

Für `n = 2²⁶ ≈ 6{,}7 \cdot 10^7` (die Obergrenze des Sweeps in `axpy.cu`):

| Posten | Datenmenge | erwartete Zeit |
|---|---|---|
| H→D (x und y) | 2 · 4n = 537 MB | ≈ 45 ms bei 12 GB/s |
| Kernel | liest 8n, schreibt 4n = 805 MB | ≈ 0,5 ms bei 1555 GB/s |
| D→H (y) | 4n = 268 MB | ≈ 22 ms bei 12 GB/s |

**Der Transfer dauert also rund 130-mal so lange wie die Rechnung** — und das Verhältnis ist
größenunabhängig, weil beide Posten linear in `n` wachsen. Das ist keine schlechte
Implementierung, sondern die Physik des Problems.

Die erreichte Bandbreite rechnet man so aus:

$$\text{GB/s} = \frac{\text{bewegte Bytes}}{T_{\text{Kernel}}} = \frac{12n\ \text{Byte}}{T_{\text{Kernel}}}$$

— 12n, weil `axpy` pro Element `x` liest (4 B), `y` liest (4 B) und `y` schreibt (4 B). Bei
kleinem `n` wird man weit unter dem Datenblattwert bleiben: Unterhalb von etwa `n = 10⁶` ist
der Kernel-Start selbst (~5–10 µs) länger als die Rechnung, und die GPU ist nicht ansatzweise
ausgelastet. Erst ab `n ≳ 10⁷` nähert sich der Wert dem Maximum an.

> Wenn die gemessene Kernel-Zeit **nicht mit `n` wächst**, wurde das Synchronisieren
> vergessen — dann misst man das Einreihen des Kernels, nicht seine Ausführung.

**g) Vergleich mit der CPU.** `axpy` ist vollständig **speicherbandbreitenlimitiert**: 2 Flop
auf 12 bewegte Byte, also eine arithmetische Intensität von 1/6 Flop/Byte. Weder CPU noch GPU
rechnen hier — beide warten auf Speicher. Der Vergleich reduziert sich damit auf die
Bandbreiten:

| | Bandbreite | relative Erwartung |
|---|---|---|
| CPU (Laptop, DDR4) | ≈ 50 GB/s | 1× |
| A100 (HBM2) | ≈ 1555 GB/s | ≈ 31× |

Der **Kernel allein** ist also etwa 30-mal schneller — das ist ehrlich, aber irreführend.
**Mit Transfer** verliert die GPU: Die 8n Byte über PCIe bei 12 GB/s kosten mehr Zeit, als die
CPU für die gesamte Rechnung braucht. Für einen einzelnen `axpy`-Aufruf lohnt sich die GPU
**nie** (vgl. Aufgabe 11.7).

Sinnvoll wird sie erst, wenn die Daten **auf der GPU bleiben** und dort viele Kernels
nacheinander auf ihnen arbeiten — dann amortisiert sich der Transfer über alle Aufrufe. Das
ist der Grund, warum ein iterativer Löser auf der GPU gewinnt, obwohl jeder einzelne seiner
`axpy`-Schritte es für sich genommen nicht täte.

---

## Lösung 11.5 — Zweidimensionale Indexierung

`H = 1080` Zeilen, `W = 1920` Spalten, Block 16 × 16.

**a)**

$$\text{blocks.x} = \left\lceil \frac{W}{16} \right\rceil = \frac{1920}{16} = 120 \quad \text{(geht exakt auf)}$$
$$\text{blocks.y} = \left\lceil \frac{H}{16} \right\rceil = \left\lceil 67{,}5 \right\rceil = 68$$

Gestartete Threads:

$$(120 \cdot 16) \times (68 \cdot 16) = 1920 \times 1088 = 2\,088\,960$$

Pixel: `1080 · 1920 = 2 073 600`. Ohne Arbeit: `2 088 960 − 2 073 600 = 15 360` — genau
8 überzählige Zeilen à 1920 Threads (1088 − 1080 = 8). ✓

**b)** Pixel `(r, c) = (500, 1000)`:

| | Rechnung | Ergebnis |
|---|---|---|
| `blockIdx.x` (Spalte) | ⌊1000/16⌋ | 62 |
| `threadIdx.x` | 1000 − 62·16 = 1000 − 992 | 8 |
| `blockIdx.y` (Zeile) | ⌊500/16⌋ | 31 |
| `threadIdx.y` | 500 − 31·16 = 500 − 496 | 4 |

Also **Block (62, 31), Thread (8, 4)**.

Linearer Index bei Row-Major:

$$i = r \cdot W + c = 500 \cdot 1920 + 1000 = 960\,000 + 1000 = \boxed{961\,000}$$

**c)** Umgekehrt, aus `i = 1 000 000`:

$$r = \left\lfloor \frac{1\,000\,000}{1920} \right\rfloor = \lfloor 520{,}83 \rfloor = 520, \qquad c = 1\,000\,000 - 520 \cdot 1920 = 1\,000\,000 - 998\,400 = 1600$$

Also Pixel **(520, 1600)**. Probe: 520 · 1920 + 1600 = 1 000 000. ✓

**d) Der Kernel.**

```cpp
__global__ void korrigiere(float* B, const float* A,
                           const float* w, const float* b,
                           int H, int W) {
    int c = blockIdx.x * blockDim.x + threadIdx.x;   // .x ist die SPALTE
    int r = blockIdx.y * blockDim.y + threadIdx.y;   // .y ist die ZEILE
    if (r < H && c < W) {                            // Waechter in BEIDEN Dimensionen
        int i = r * W + c;                           // Row-Major-Linearisierung
        B[i] = A[i] * w[c] + b[r];
    }
}
```

Start:

```cpp
dim3 threads(16, 16);
dim3 blocks((W + 15) / 16, (H + 15) / 16);
korrigiere<<<blocks, threads>>>(d_B, d_A, d_w, d_b, H, W);
```

**e) Die vertauschte Fassung ist korrekt, aber deutlich langsamer.**

Korrekt: Ob man die Spalte aus `.x` oder aus `.y` zieht, ändert nur, *welcher* Thread welches
Pixel bekommt — solange `blocks` konsistent vertauscht wird und der Wächter mitwandert,
bearbeitet weiterhin genau ein Thread genau ein Pixel.

Langsam: Entscheidend ist, dass `threadIdx.x` **innerhalb eines Warps am schnellsten läuft**.
Die 32 Lanes eines Warps haben `threadIdx.x = 0…31` bei gleichem `threadIdx.y`. Also:

| Fassung | die 32 Lanes greifen zu auf | Adressabstand |
|---|---|---|
| `c` aus `.x` (richtig) | `A[r·W + c], c = c₀…c₀+31` | **4 Byte** — 32 aufeinanderfolgende `float` |
| `r` aus `.x` (vertauscht) | `A[r·W + c], r = r₀…r₀+31` | **W · 4 = 7680 Byte** |

Im ersten Fall liegen alle 32 Werte in **128 zusammenhängenden Byte** und werden zu einer
Handvoll Speichertransaktionen zusammengefasst (*coalescing*). Im zweiten Fall liegt jeder
Wert in einer eigenen Cache-Zeile — die Hardware muss **32 getrennte Transaktionen** ausführen
und holt dabei jedes Mal 32 Byte, von denen sie 4 braucht.

Erwartung: ein Faktor von etwa **8** (das Verhältnis 32 Byte geholt zu 4 Byte gebraucht),
je nach Cache-Wirkung auch mehr. Das Detail dazu ist Kapitel 12.

> **Merksatz:** `.x` ist immer die Dimension, die im Speicher am dichtesten liegt — bei
> Row-Major also die Spalte.

**f)** Praktische Messung, siehe [`code/bild2d.cu`](code/bild2d.cu). Erwartet wird der Faktor
aus e); wenn er deutlich kleiner ausfällt, liegt das am L2-Cache, der bei mehrfach
durchlaufenen Zeilen einen Teil der Verluste auffängt.

**g)** Drei Positionen, die verschiedene Fehlerklassen aufdecken:

| Position | deckt auf |
|---|---|
| `(0, 0)` | grobe Offset-Fehler; ist aber gegen fast alles blind, weil `0·W + 0 = 0` für **jede** Linearisierung gilt |
| `(H/2, W/2)` | vertauschte Zeilen/Spalten — bei quadratischen Bildern nicht, bei 1080×1920 sehr wohl |
| `(H−1, W−1)` | Abrunden der Blockzahl, fehlender Wächter, Off-by-one am Rand |

Eine Position reicht deshalb nicht, weil `(0,0)` unter *jeder* falschen Indexformel richtig
herauskommt: Der Test ist genau dort maximal unempfindlich, wo er am bequemsten ist. Ein
besonders wirksamer Trick ist außerdem, die Testdaten so zu wählen, dass das Sollergebnis den
Index verrät — etwa `A[r][c] = r`, `w[c] = 1`, `b[r] = 0`, sodass `B[r][c] = r` sein muss:
Vertauscht man Zeile und Spalte, fällt es sofort auf.

---

## Lösung 11.6 — Warp-Divergenz erkennen

Grundregel: Divergenz gibt es **nur innerhalb eines Warps**. Man prüft also für jedes
Fragment: *Ist die Bedingung für alle 32 Lanes eines Warps dieselbe?*

Die 32 Lanes eines Warps haben aufeinanderfolgende `threadIdx.x` (und damit auch
aufeinanderfolgende `i`).

**(a) `if (i % 2 == 0)` → Faktor 2.** Innerhalb jedes Warps sind 16 Lanes gerade und 16
ungerade. Beide Pfade werden nacheinander mit je 16 aktiven Lanes ausgeführt: doppelte Zeit,
halbe Auslastung. Das ist der Musterfall aus der Vorlesung (Folie 17).

**(b) `if (threadIdx.x < 64)` → Faktor 1, keine Divergenz.** Bei `blockDim.x = 256` gilt:
Warps 0 und 1 (Threads 0–63) nehmen komplett den `if`-Zweig, Warps 2–7 komplett den
`else`-Zweig. Die Grenze 64 liegt exakt auf einer Warp-Grenze (64 = 2 · 32). Verschiedene
Warps dürfen beliebig auseinanderlaufen — das kostet nichts.

**(c) `if ((threadIdx.x / 32) % 2 == 0)` → Faktor 1.** `threadIdx.x / 32` **ist** die
Warp-Nummer und damit innerhalb eines Warps konstant. Die geraden Warps machen `A`, die
ungeraden `B`. Das ist genau die Umformung, die man anstrebt.

**(d) `for (int k = 0; k < threadIdx.x; ++k)` → der Warp braucht so lange wie seine höchste
Lane.** In Warp `w` laufen die Threads `32w … 32w+31` mit Schleifenlängen `32w … 32w+31`. Die
Hardware führt die Schleife `32w+31`-mal aus und maskiert nach und nach die fertigen Lanes ab.

Nützliche Arbeit gegenüber aufgewendeten Warp-Instruktionen:

$$\text{Effizienz}(w) = \frac{\sum_{j=0}^{31} (32w + j)}{32 \cdot (32w + 31)} = \frac{1024w + 496}{1024w + 992}$$

| Warp | Effizienz |
|---|---|
| 0 | 496/992 = **50 %** |
| 1 | 1520/2016 = 75 % |
| 7 | 7664/8160 = 94 % |

Der erste Warp verschwendet die Hälfte, die späteren immer weniger — weil dort die relativen
Längenunterschiede kleiner werden.

**(e) `switch (threadIdx.x % 4)` → Faktor 4.** In jedem Warp kommen alle vier Restklassen
vor, also vier Pfade nacheinander mit je 8 aktiven Lanes.

**(f) `if (i < n)` mit `n = 100 000`, `blockDim.x = 256` → Faktor 1, keine Divergenz.**

$$\text{blocks} = \lceil 100000/256 \rceil = 391, \qquad \text{letzter Block } 390 \text{ deckt } 99\,840 \ldots 100\,095$$

Gültig sind davon `99 840 … 99 999`, also **160 Elemente** — und `160 = 5 · 32`. Die Warps 0–4
sind vollständig aktiv, die Warps 5–7 vollständig leer, **kein Warp ist geteilt**. Divergenz
durch den Wächter entstünde nur, wenn `n` kein Vielfaches von 32 wäre — und selbst dann in
genau einem Warp von 391 · 8 = 3128.

**g) Divergenzfrei umformulieren.** Die Idee: Nicht die Bedingung ändern, sondern die
**Zuordnung Thread → Element**, sodass die Bedingung warp-uniform wird. Die erste Hälfte der
Threads übernimmt alle geraden, die zweite alle ungeraden Indizes:

```cpp
int i = blockIdx.x * blockDim.x + threadIdx.x;
int h = n / 2;
if (i < h) {
    A(2 * i);            // gerade Elemente
} else if (i < n) {
    B(2 * (i - h) + 1);  // ungerade Elemente
}
```

Jetzt ist `i < h` innerhalb eines Warps konstant — außer im einen Warp, in den die Grenze
`h` zufällig hineinfällt. Statt Divergenz in **jedem** Warp gibt es sie in **einem**.

**Der Preis:** Die Lanes eines Warps greifen jetzt mit **Abstand 2** auf den Speicher zu statt
mit Abstand 1. Damit ist das Coalescing halbiert — die Hardware holt für 32 gebrauchte Werte
doppelt so viele Cache-Zeilen. Ob sich der Tausch lohnt, hängt davon ab, wie teuer `A` und `B`
gegenüber dem Speicherzugriff sind: Bei rechenintensiven Zweigen ja, bei einer einzelnen
Multiplikation nein.

> Das ist ein wiederkehrendes Muster in CUDA: **Divergenz und Coalescing zeigen oft in
> entgegengesetzte Richtungen.** Man optimiert nicht beides gleichzeitig, sondern das, was
> tatsächlich der Engpass ist.

**h)** Ein Warp ist die kleinste Einheit, die der Scheduler plant: Es gibt **einen**
Instruction Pointer für alle 32 Lanes. Ein Warp ist erst fertig, wenn die letzte Lane fertig
ist; vorher fertige Lanes werden lediglich maskiert und laufen leer mit. Deshalb zählt die
Zeit des langsamsten Threads, nicht der Durchschnitt.

> **Prinzip:** Innerhalb eines Warps kostet jede Arbeit so viel wie das **Maximum** über die
> 32 Lanes, nicht wie ihr Mittelwert. Ungleiche Arbeitsmengen gehören daher zwischen die
> Warps, niemals hinein.

---

## Lösung 11.7 — Lohnt sich die GPU?

**a)** $T_1 = 100$ s, serieller Anteil $f = 0{,}05$ → 5 s seriell, 95 s parallelisierbar.

$$T_{\text{GPU}} = 5 + \frac{95}{60} = 5 + 1{,}583 = 6{,}583\ \text{s} \qquad S = \frac{100}{6{,}583} = \mathbf{15{,}2}$$

Mit der Formel direkt:

$$S(60) = \frac{1}{0{,}05 + \frac{0{,}95}{60}} = \frac{1}{0{,}0658} = 15{,}2$$

Bei unendlich schneller GPU:

$$S_{\max} = \frac{1}{f} = \frac{1}{0{,}05} = \mathbf{20}$$

Von 60-fachem Kernel-Speedup kommen also nur 15,2 an — und mehr als 20 ist prinzipiell
unerreichbar.

**b)** Der Transfer ist **zusätzliche** Zeit, die in der CPU-Fassung nicht vorkommt:

$$T_{\text{GPU}} = 5 + 1{,}583 + 20 = 26{,}583\ \text{s} \qquad S = \frac{100}{26{,}583} = \mathbf{3{,}76}$$

Der Gewinn ist um den Faktor $15{,}2 / 3{,}76 = \mathbf{4{,}0}$ eingebrochen. Der Transfer
allein kostet mehr als das Vierfache der gesamten GPU-Rechenzeit (20 s gegenüber 1,58 s).

**c) Faktor 10 ist unerreichbar — mit jeder denkbaren GPU.**

Speedup 10 hieße $T_{\text{GPU}} = 10$ s. Aber schon die beiden Posten, die von der
Kernel-Geschwindigkeit **gar nicht abhängen**, summieren sich zu

$$5\ \text{s (seriell)} + 20\ \text{s (Transfer)} = 25\ \text{s}$$

Selbst bei unendlich schnellem Kernel bleibt

$$S_{\max} = \frac{100}{25} = \mathbf{4{,}0}$$

**Interpretation:** Sobald der Transfer im Spiel ist, wird er zum neuen „seriellen Anteil" im
Sinne von Amdahl — und zwar zu einem, der viermal größer ist als der echte. Die richtige
Reaktion ist deshalb nicht „schnellere GPU kaufen", sondern **den Transfer loswerden**: Daten
auf dem Device halten, mehr Arbeit pro Übertragung erledigen, Transfer und Rechnung
überlappen.

**d) Die allgemeine Bedingung.** Pro Element: 8 Byte über PCIe, `q` Flop.

$$T_{\text{GPU}}(n,q) = \underbrace{\frac{8n}{12 \cdot 10^9}}_{\text{Transfer}} + \underbrace{\frac{qn}{15 \cdot 10^{12}}}_{\text{Rechnung}}, \qquad T_{\text{CPU}}(n,q) = \frac{qn}{100 \cdot 10^9}$$

`n` kürzt sich heraus — **die Schwelle hängt nicht von der Problemgröße ab**, nur von der
Arbeit pro Element. Das ist die eigentliche Erkenntnis dieser Aufgabe.

**e)** $T_{\text{GPU}} < T_{\text{CPU}}$:

$$\frac{8}{12 \cdot 10^9} + \frac{q}{15 \cdot 10^{12}} < \frac{q}{10^{11}}$$
$$6{,}667 \cdot 10^{-10} + 6{,}667 \cdot 10^{-14}\, q < 1{,}0 \cdot 10^{-11}\, q$$
$$6{,}667 \cdot 10^{-10} < q \left(1{,}0 \cdot 10^{-11} - 6{,}667 \cdot 10^{-14}\right) = q \cdot 9{,}933 \cdot 10^{-12}$$
$$q > \frac{6{,}667 \cdot 10^{-10}}{9{,}933 \cdot 10^{-12}} = 67{,}1$$

Die GPU lohnt sich also erst ab etwa **68 Flop pro Element**.

Die Näherung, die man im Kopf machen kann: Der GPU-Rechenterm ist gegenüber dem CPU-Term
vernachlässigbar (Faktor 150), also grob

$$\frac{8}{12 \cdot 10^9} < \frac{q}{10^{11}} \Rightarrow q > \frac{8 \cdot 10^{11}}{12 \cdot 10^9} = 66{,}7$$

— derselbe Wert bis auf ein halbes Prozent. Er sagt: *Die CPU muss mindestens so lange
rechnen, wie das Kopieren dauert.*

**f)** `axpy` rechnet **q = 2** Flop pro Element (eine Multiplikation, eine Addition) — und
überträgt sogar 12n statt 8n Byte, weil `y` in beide Richtungen muss. Das ist rund **Faktor
34 unter der Schwelle**. Ein einzelner `axpy` lohnt sich auf der GPU nicht annähernd; das
bestätigt die Messung aus 11.4g.

Zum Vergleich: Eine dichte Matrixmultiplikation bewegt `3n²` Werte und rechnet `2n³` Flop,
also `q ≈ 2n/3` Flop pro Element — bei `n = 1000` sind das über 600. Deshalb ist GEMM das
Paradebeispiel für die GPU und `axpy` das Gegenbeispiel.

**g) Zwei Maßnahmen ohne Änderung am Kernel:**

1. **Daten auf der GPU lassen.** Wenn 50 Kernels nacheinander auf demselben Puffer arbeiten,
   fällt der Transfer einmal statt 50-mal an — die effektive Schwelle sinkt um Faktor 50.
   Das ist mit Abstand der größte Hebel und der Grund, warum iterative Verfahren auf der GPU
   gewinnen.
2. **Pinned (page-locked) Host-Speicher** über `cudaHostAlloc` statt `malloc`/`new`. Der
   Treiber muss dann nicht über einen Zwischenpuffer gehen und kann DMA direkt fahren; die
   PCIe-Bandbreite steigt typischerweise auf das Anderthalb- bis Doppelte.

Weitere Möglichkeiten: Transfer und Rechnung mit **Streams überlappen** (während Paket `k`
gerechnet wird, kommt Paket `k+1` schon herüber) oder in geringerer Präzision übertragen
(`float` statt `double` halbiert die Bytes).

---

## Lösung 11.8 — Grid-Stride-Loop

**a)**

```cpp
__global__ void axpy_stride(float* y, const float* x, float alpha, int n) {
    int i      = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;      // Gesamtzahl der Threads im Grid
    for (; i < n; i += stride)
        y[i] = alpha * x[i] + y[i];
}
```

Der Wächter steckt jetzt in der Schleifenbedingung — ein separates `if (i < n)` wäre doppelt
gemoppelt.

**b) Alle vier Konfigurationen liefern das korrekte Ergebnis.** Genau das ist der Sinn des
Idioms: Die Korrektheit hängt nicht mehr an der Startkonfiguration.

| Konfiguration | Threads | Elemente pro Thread | Erwartung |
|---|---|---|---|
| `<<<1,1>>>` | 1 | 10 000 000 | katastrophal — **ein** Thread, also 1 von 6912 Recheneinheiten, und keinerlei Latency Hiding. Um Größenordnungen langsamer als die CPU |
| `<<<1,256>>>` | 256 | 39 063 | ein einziger Block läuft auf **einem** SM; 107 von 108 SMs stehen still |
| `<<<432,256>>>` | 110 592 | ~91 | 432 = 4 · 108, also 4 Blöcke pro SM — gute Auslastung, nahe am Optimum |
| `<<<39063,256>>>` | 10 000 128 | 1 | ein Element pro Thread; ebenfalls voll ausgelastet, minimal mehr Start-Overhead |

Die letzten beiden liegen typischerweise dicht beieinander und sind
speicherbandbreitenlimitiert; die ersten beiden zeigen, wie sehr die GPU von der Anzahl der
Threads lebt.

**c)** `<<<4096, 256>>>`, `n = 10⁷`:

$$T = 4096 \cdot 256 = 1\,048\,576\ \text{Threads}, \qquad \frac{10^7}{1\,048\,576} = 9{,}537$$

Nein, **nicht alle Threads bearbeiten gleich viele Elemente.** Genau:

$$\lfloor 9{,}537 \rfloor = 9 \text{ Elemente als Grundlast}, \qquad 10^7 - 9 \cdot 1\,048\,576 = 10\,000\,000 - 9\,437\,184 = 562\,816$$

| Anzahl Threads | Elemente je Thread |
|---|---|
| 562 816 | 10 |
| 485 760 | 9 |

Probe: `562 816 · 10 + 485 760 · 9 = 5 628 160 + 4 371 840 = 10 000 000`. ✓

Der Unterschied ist genau **ein** Element von zehn und betrifft die Threads mit den kleinsten
Indizes — also die ersten Warps. Lastungleichgewicht im Sinne von Kapitel 06 entsteht dadurch
praktisch keines.

**d) Weil die Blockaufteilung das Coalescing zerstört.** Betrachte den ersten Schritt der
Schleife für die 32 Lanes eines Warps:

| Aufteilung | Lane 0 liest | Lane 1 liest | … | Abstand |
|---|---|---|---|---|
| Grid-Stride | `x[0]` | `x[1]` | `x[31]` | **4 Byte** |
| Block je Thread (`k = ⌈n/T⌉`) | `x[0]` | `x[k]` | `x[31k]` | **4k Byte** |

Im ersten Fall liegen die 32 Werte in 128 zusammenhängenden Byte und werden zu wenigen
Speichertransaktionen zusammengefasst. Im zweiten Fall liegt jeder Wert in einer eigenen
Cache-Zeile: **32 getrennte Transaktionen**, von denen jede 32 Byte holt, um 4 zu benutzen —
also 8-mal mehr Speicherverkehr als nötig. Bei einem bandbreitenlimitierten Kernel schlägt das
fast eins zu eins auf die Laufzeit durch.

Im zweiten und jedem weiteren Schleifenschritt bleibt es dabei: Die Lanes wandern parallel
weiter und behalten den Abstand `k`.

**e) Der Widerspruch löst sich über die Frage, *wann* die Zugriffe passieren.**

| | CPU (OpenMP, `schedule(static)`) | GPU (Warp) |
|---|---|---|
| Wer greift zu | jeder Kern **unabhängig**, zu eigenen Zeitpunkten | 32 Lanes **gleichzeitig**, in derselben Instruktion |
| Was ist gut | jeder Kern arbeitet einen **eigenen zusammenhängenden Bereich** ab → Prefetcher greift, eigener L1, kein false sharing | die 32 gleichzeitigen Adressen sollen in **eine** Transaktion passen → benachbarte Adressen |
| Was ist schlecht | verzahnte Aufteilung → benachbarte Kerne teilen Cache-Zeilen → **false sharing** | Blockaufteilung → 32 getrennte Transaktionen |

Auf der CPU ist Lokalität eine Eigenschaft **über die Zeit** (derselbe Kern kommt gleich
wieder in der Nähe vorbei). Auf der GPU ist Lokalität eine Eigenschaft **über die Threads**
(die 32 Lanes eines Warps sind zum selben Zeitpunkt in der Nähe). Deshalb ist das jeweils
Optimale genau die Aufteilung, die auf der anderen Architektur die schlechteste ist.

> Das ist einer der Punkte, an dem CUDA-Denken und OpenMP-Denken wirklich auseinandergehen —
> und eine beliebte Klausurfrage.

**f) Debuggen mit `<<<1,1>>>`.** Der Kernel läuft dann vollständig **seriell und
deterministisch** ab. Damit:

- verschwinden alle Wettlauf-Effekte; bleibt der Fehler trotzdem bestehen, ist es ein
  **Logikfehler**, verschwindet er, ist es eine **Race Condition**. Das halbiert den
  Suchraum in einem Schritt.
- ist die Ausführungsreihenfolge reproduzierbar, `printf` im Kernel wird lesbar, und man kann
  gezielt bisektieren.
- funktioniert derselbe Kernel danach **unverändert** in der schnellen Konfiguration — ohne
  Grid-Stride müsste man für den `<<<1,1>>>`-Test den Kernel umschreiben und würde damit
  womöglich genau den Fehler mit wegschreiben.

---

## Lösung 11.9 — Kurzfragen

**a) SIMD vs. SIMT.** SIMD ist ein *Instruktionsmodell*: **ein** Vektorregister mit z. B. 8
`double`, eine Instruktion arbeitet auf allen Bahnen, der Programmierer rechnet die Indizes
selbst aus und maskiert Verzweigungen von Hand. SIMT ist ein *Thread-Modell*: Es gibt viele
Threads mit **eigener Identität** (`threadIdx`) und eigenen Registern, jeder rechnet seinen
Index selbst und darf logisch einen eigenen Kontrollpfad nehmen.

„SIMD-Hardware mit SIMT-Programmiermodell" heißt: Darunter liegt weiterhin **eine**
Instruktionsdekodierung für 32 Lanes. Die versprochene Freiheit ist eine Illusion mit
Preisschild — nimmt ein Teil des Warps einen anderen Pfad, serialisiert die Hardware beide
Pfade und maskiert jeweils die unbeteiligten Lanes.

**b) Falsch.** Shared Memory existiert **pro Block** und ist außerhalb dieses Blocks weder
sichtbar noch adressierbar. Zwei Blöcke können sich nur über den globalen Speicher austauschen
— und selbst das ist heikel, weil es keine Reihenfolgegarantie und keine blockübergreifende
Barriere gibt. Der saubere Weg ist: Kernel beenden, zweiten Kernel starten.

**c) Richtig** (für die blockierende Standardform im Default-Stream). `cudaMemcpy` wird in
denselben Stream eingereiht wie der Kernel, arbeitet die Warteschlange der Reihe nach ab und
blockiert den Host, bis die Kopie fertig ist — der Kernel ist dann zwangsläufig durch. Ein
zusätzliches `cudaDeviceSynchronize()` ist überflüssig.

Zwei Einschränkungen, die zur vollständigen Antwort gehören: Für `cudaMemcpyAsync` gilt das
**nicht**, und bei **Unified Memory** ohne `cudaMemcpy` muss man selbst synchronisieren, bevor
der Host die Daten anfasst.

**d) Falsch.** Eine `__global__`-Funktion muss **immer `void`** zurückgeben — unabhängig
davon, wie viele Threads schreiben. Es gäbe auch niemanden, der den Wert entgegennähme: Der
Kernel-Start ist asynchron und kehrt sofort zurück, lange bevor irgendein Thread gerechnet
hat. Ergebnisse verlassen den Kernel ausschließlich über Zeiger auf Device-Speicher.

**e) Weil Kernel-Starts asynchron sind.** `kernel<<<…>>>(…)` reiht den Kernel nur ein und
kehrt sofort zurück; eine Uhr, die unmittelbar danach gestoppt wird, misst die **Einreihzeit**
von wenigen Mikrosekunden.

Woran man es an den Messwerten erkennt: **Die Zeit wächst nicht mit `n`.** Ob man 10³ oder
10⁸ Elemente verarbeitet, es kommen immer dieselben ~5 µs heraus — und diese Konstanz ist das
verräterische Signal. Abhilfe: `cudaDeviceSynchronize()` vor dem Stoppen, oder `cudaEvent_t`.

**f)** Allokieren (`cudaMalloc`) → kopieren H→D (`cudaMemcpy`) → Kernel starten (`<<<…>>>`) →
zurückkopieren D→H (`cudaMemcpy`) → freigeben (`cudaFree`).

**g)** `-arch=sm_80` weist `nvcc` an, Maschinencode für die **Compute Capability 8.0**
(Ampere, z. B. A100) zu erzeugen. Auf einer V100 (Compute Capability 7.0) scheitert der
Kernel-Start mit **„no kernel image is available for execution on the device"** — das Binärformat
passt nicht. Umgekehrt (für sm_70 übersetzt, auf A100 gestartet) funktioniert es dagegen meist
über den mitgelieferten PTX-Zwischencode, der zur Laufzeit nachübersetzt wird.

**h) Weil Blöcke bewusst unabhängig sind — und zwar aus Skalierbarkeitsgründen.** Der
Scheduler verteilt die Blöcke in beliebiger Reihenfolge auf die verfügbaren SMs; auf einer
kleinen GPU laufen sie größtenteils **nacheinander**. Eine Grid-weite Barriere würde deshalb
verlangen, dass ein bereits laufender Block auf einen Block wartet, der mangels freiem SM noch
gar nicht gestartet ist — und der nicht starten kann, weil der erste seinen SM belegt. Das ist
ein **Deadlock**. Die einzige globale Barriere ist deshalb das Kernel-Ende.

**i)** Die 6912 Recheneinheiten sind einzeln **langsamer** als ein CPU-Kern, haben keine
Sprungvorhersage und arbeiten nur in Gruppen zu 32 sinnvoll. Vor allem aber:

- **Amdahl**: Der serielle Rest bleibt auf der CPU und begrenzt alles.
- **PCIe**: Der Transfer ist Zusatzaufwand, den die CPU-Fassung nicht hat, und er ist um
  zwei Größenordnungen langsamer als der GPU-Speicher.
- **Speicherbandbreite**: Die meisten realen Kernels sind bandbreiten-, nicht rechenlimitiert.
  Dann zählen nicht die 6912 Einheiten, sondern das Verhältnis der Bandbreiten (≈ 30×).
- **Auslastung**: Unter etwa 10⁵ Threads liegt die GPU brach.

**j)** Der Job landet auf einem Knoten **ohne** GPU (oder ohne zugewiesene GPU). Der erste
CUDA-Aufruf scheitert mit `cudaErrorNoDevice` — **„no CUDA-capable device is detected"**.
Prüft das Programm die Rückgabewerte, steht das lesbar in `out.txt`. Prüft es sie nicht,
laufen alle CUDA-Aufrufe ins Leere, `cudaMemcpy` kopiert nichts, und das Programm gibt
Nullen aus und beendet sich mit Status 0 — also scheinbar erfolgreich. Das ist der praktische
Grund, warum das `CUDA_CHECK`-Makro nicht optional ist.

---

**Zurück:** [Theorie](theorie.md) · [Übungen](uebungen.md) · **Weiter:** Kapitel 12 — CUDA II
