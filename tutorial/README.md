# Tutorial — Prüfungsvorbereitung Efficient Computing / PC 2

Eigenes Lern-Tutorial zur Vorlesung: jedes Thema noch einmal intensiv erklärt, dazu eigene
Übungsaufgaben mit ausführlichen Musterlösungen. Alles auf Deutsch, alles in Markdown.

**Schwerpunkt: OpenMP und CUDA** — Kapitel 06, 07, 11 und 12 sind bewusst die längsten und
haben die meisten Aufgaben.

> **📐 [Formelsammlung](formelsammlung.md)** — alle rechnerisch verwertbaren Formeln des Kurses
> an einer Stelle, nach Themen sortiert, mit Symbolerklärung, Referenzwerten (A100, Cache-Line,
> Warp, …) und einem Index „welche Formel für welche Frage?".

**Aller Code ist C++17.** Modernes C++ drumherum (`std::vector`, `<chrono>`, RAII, Lambdas),
aber die parallelisierten Rechenschleifen bleiben roh — so sieht man, was OpenMP verteilt, und
so steht es auch in der Klausur. Die Sprachmittel und die C++-spezifischen OpenMP-Fallstricke
stehen kompakt im **[C++-Werkzeugkasten](cpp-werkzeugkasten.md)**; den einmal durchlesen, bevor
es mit Kapitel 06 losgeht.

---

## Aufbau eines Kapitels

Es gibt zwei Kapitelarten.

**★ Schwerpunktkapitel** (OpenMP und CUDA) — vollständig ausgearbeitet nach dem Muster in
[`_vorlage/`](_vorlage/):

```
NN-thema/
  theorie.md      Erklärung: Motivation, Konzepte, Formeln, Code, Klausurfragen,
                  Fallstricke, Merkkasten, Verweise auf vl/*.pdf mit Seitenzahl
  uebungen.md     5–10 eigene Aufgaben, aufsteigend schwer — ohne Lösungen
  loesungen.md    Musterlösungen mit Rechenweg und Begründung, gleiche Nummerierung
  code/           lauffähiger Code (.cpp/.cu, Makefile, job.sbatch)
```

**Kompaktkapitel** (alle übrigen Themen) — eine einzige Datei, zum Nachschlagen und
Wiederholen:

```
NN-thema/
  theorie.md      Worum es geht · Kernpunkte mit Formeln · Klausur-Schnellcheck
                  (Frage → Antwort) · Merkkasten · Verbindung zu den anderen Kapiteln
```

Die Kompaktkapitel haben **keine** getrennten Aufgaben- und Lösungsdateien; ihr
Klausur-Schnellcheck ersetzt sie. Wer zu einem dieser Themen echte Übungsaufgaben braucht,
kann das Kapitel jederzeit auf das volle Format ausbauen.

Gemeinsame Helfer für alle Kapitel liegen in [`_common/`](_common/) und werden von den
Makefiles mit `-I../../_common` eingebunden:

| Datei | Inhalt |
|---|---|
| [`bench.hpp`](_common/bench.hpp) | OpenMP-Kapitel: `ec::best_of`, `ec::row`, `ec::nahe` |
| [`cuda_check.hpp`](_common/cuda_check.hpp) | CUDA-Kapitel: `CUDA_CHECK`, `CUDA_CHECK_KERNEL`, `ec::GpuTimer` (Events), `ec::gb_pro_s` |

Aufgaben und Lösungen liegen **absichtlich in getrennten Dateien** — erst selbst rechnen,
dann vergleichen.

Aufgabentypen sind getaggt:

| Tag | Bedeutung |
|---|---|
| `[Rechnen]` | Papieraufgabe mit Zahlenergebnis (Speedup, Cache-Misses, Roofline, …) |
| `[Code]` | Programm schreiben oder parallelisieren |
| `[Analyse]` | Begründen, vergleichen, Fehler finden, Verhalten erklären |
| `[Klausur]` | Im Format einer typischen Klausurfrage, kurz und ohne Hilfsmittel lösbar |

---

## Kapitelübersicht

| Kapitel | Umfang | Thema | Quellen |
|---|---|---|---|
| [`01-grundlagen-axpy`](01-grundlagen-axpy/) | kompakt | Effizienzbegriff, Zielfunktionen, Ebenen der Optimierung, axpy, richtig messen | VL 1, Blatt 2 |
| [`02-rechnerarchitektur`](02-rechnerarchitektur/) | kompakt | ISA, Befehlsausführung, **Pipelining-Formel**, Multiple Issue, OoO, SIMD-Überblick | VL 2, Blatt 3 |
| [`03-miniaturisierung`](03-miniaturisierung/) | kompakt | $C \sim L$, $f \sim 1/L$, $P = CU^2f$, **Dennard Scaling**, Power Wall, von-Neumann-Flaschenhals | VL 3 |
| [`04-ideales-cache-modell`](04-ideales-cache-modell/) | kompakt | Hierarchie, Cache-Lines, Zuordnung, **Adressaufteilung**, LRU/FIFO/LFU, ideales Cache-Modell, $Q(n;M,B)$ | VL 4, Ex-4, Blatt 5 |
| [`05-gemm-analyse`](05-gemm-analyse/) | kompakt | Row-/Column-Major, $Q_{\text{naiv}} = \Theta(n^3)$, **Blocking** $\Theta(n^3/(B\sqrt M))$, untere Schranke | VL 5, Ex-5 |
| [`05b-simd`](05b-simd/) | kompakt | Vektorinstruktionen, Latency/Throughput, **RaW**, In-Order vs. OoO, **Peak-Performance-Formel** | Ex-3-SIMD, Blatt 4 |
| **[`06-openmp-grundlagen`](06-openmp-grundlagen/)** | ★ **voll** | Fork/Join, `parallel for`, Scoping, Clauses, Reduktion, Scheduling, false sharing | VL 6, Blatt 6 |
| **[`07-openmp-tasks`](07-openmp-tasks/)** | ★ **voll** | Tasks, `depend`, `taskwait`, Bernstein, Task-Graph, kritischer Pfad, Arbeit/Tiefe | VL 9-task, Blatt 8.1 |
| [`08-barnes-hut`](08-barnes-hut/) | kompakt | n-Body $\Theta(n^2)$, Euler, Quadtree, **$l/d \le \theta$**, $\Theta(n\log n)$, Lastverteilung | VL 7, Blatt 7.1 |
| [`09-rendering-raytracing`](09-rendering-raytracing/) | kompakt | inverses Ray Tracing, Kamerabasis, **Ray-Dreieck-Schnitt** (Cramer), Lambert, Parallelisierung | VL 9-Render, Blatt 7.2 |
| [`10-amdahl-gustafson-roofline`](10-amdahl-gustafson-roofline/) | kompakt **+ [Vertiefung](10-amdahl-gustafson-roofline/vertiefung.md)** ⚑ | Speedup, Effizienz, **Amdahl**, **Gustafson**, strong/weak scaling, **Roofline** · Vertiefung: Herleitungen, Karp-Flatt, $\alpha \leftrightarrow \gamma$, Ceilings, 6 Aufgabentypen | VL 10, Blatt 8.2/8.3, Blatt 10 |
| **[`11-cuda-grundlagen`](11-cuda-grundlagen/)** | ★ **voll** | GPU-Architektur, SIMT, Warps, Thread/Block/Grid, erster Kernel, Indexrechnung, Speichertransfer, Slurm | VL 11, Slurm_CUDA, 2D-Hints, Blatt 9 |
| **[`12-cuda-performance`](12-cuda-performance/)** | ★ **voll** | Roofline auf der GPU, Coalescing, AoS/SoA, Shared Memory, Bankkonflikte, Occupancy, Warp-Shuffle, Atomics, Tiled GEMM, Streams | VL 12, Blatt 11 |
| [`13-pointer-jumping`](13-pointer-jumping/) | kompakt | PRAM (EREW/CREW/CRCW), List Ranking, **Pointer Jumping**, Arbeitseffizienz | VL 13 |
| [`14-strassen`](14-strassen/) | kompakt | Blockrekursion, **7 Produkte / 18 Additionen**, $\Theta(n^{2{,}807})$, Cutoff | VL 14 |

★ = Schwerpunktkapitel (Theorie + Übungen + Lösungen + Code) ·
⚑ = klausurrelevantestes der Kompaktkapitel

---

## Empfohlener Lernpfad

1. **Fundament** (schnell durch): 01 → 02 → 03
2. **Speicher verstehen** — die Grundlage für alles Weitere: 04 → 05 → 05b
3. **Parallelität auf der CPU** (Schwerpunkt): 06 → 07 → 08 → 09
4. **Performance-Modelle** — wird in CUDA sofort gebraucht: 10
5. **GPU** (Schwerpunkt): 11 → 12
6. **Theorie/Algorithmen**: 13 → 14

Kapitel 10 lohnt sich auch schon vor 06, wenn man Speedup-Aufgaben früh üben will.

**Zum Wiederholen kurz vor der Prüfung** reicht pro Kompaktkapitel der *Klausur-Schnellcheck*
plus der *Merkkasten* — das sind zusammen etwa zwei Bildschirmseiten je Thema. Bei den vier
Schwerpunktkapiteln übernimmt diese Rolle jeweils der Abschnitt „Typische Klausurfragen"
zusammen mit dem Merkkasten.

Die vier Rechnungen, die man in jedem Fall im Kopf haben sollte:

| Thema | Formel |
|---|---|
| Pipelining (Kap. 02) | $T_{\text{Pipe}} = (n+k-1)t$, $S \to k$ |
| Dennard (Kap. 03) | $P = CU^2f$, $P_2 = k\beta^2 P_1$, $k=2 \Rightarrow \beta \approx 0{,}7$ |
| Blocking (Kap. 05) | $Q$: $\Theta(n^3) \to \Theta\!\left(n^3/(B\sqrt M)\right)$ |
| Amdahl / Roofline (Kap. 10) | $S = \dfrac{1}{\alpha + (1-\alpha)/p}$ · $P \le \min(\pi, \beta I)$ |

---

## Praktische Hinweise

- **OpenMP** läuft lokal: `g++ -std=c++17 -O2 -Wall -Wextra -fopenmp prog.cpp -o prog`,
  8 Kerne verfügbar. In den Kapitelordnern erledigt das jeweils ein `Makefile`.
- **CUDA** läuft **nicht** lokal (kein `nvcc`, keine NVIDIA-GPU). Der Code in den
  CUDA-Kapiteln ist für den **Ara-Cluster** ausgelegt und kommt mit `job.sbatch`.
  Die Papieraufgaben sind ohne Cluster lösbar — sie sind der klausurrelevante Teil.
  In den CUDA-Lösungen stehen deshalb **hergeleitete Erwartungswerte statt Messwerten**;
  die echten Zahlen trägt man nach dem ersten Cluster-Lauf selbst nach.
- Plots/Benchmarks: `source ../.effcom-venv/bin/activate` (matplotlib, numpy, pandas).
