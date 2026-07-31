# Tutorial — Prüfungsvorbereitung Efficient Computing / PC 2

Eigenes Lern-Tutorial zur Vorlesung: jedes Thema noch einmal intensiv erklärt, dazu eigene
Übungsaufgaben mit ausführlichen Musterlösungen. Alles auf Deutsch, alles in Markdown.

**Schwerpunkt: OpenMP und CUDA** — Kapitel 06, 07, 11 und 12 sind bewusst die längsten und
haben die meisten Aufgaben.

**Aller Code ist C++17.** Modernes C++ drumherum (`std::vector`, `<chrono>`, RAII, Lambdas),
aber die parallelisierten Rechenschleifen bleiben roh — so sieht man, was OpenMP verteilt, und
so steht es auch in der Klausur. Die Sprachmittel und die C++-spezifischen OpenMP-Fallstricke
stehen kompakt im **[C++-Werkzeugkasten](cpp-werkzeugkasten.md)**; den einmal durchlesen, bevor
es mit Kapitel 06 losgeht.

---

## Aufbau eines Kapitels

Jedes Kapitel ist ein eigener Ordner nach dem Muster in [`_vorlage/`](_vorlage/):

```
NN-thema/
  theorie.md      Erklärung: Motivation, Konzepte, Formeln, Code, Klausurfragen,
                  Fallstricke, Merkkasten, Verweise auf vl/*.pdf mit Seitenzahl
  uebungen.md     5–8 eigene Aufgaben, aufsteigend schwer — ohne Lösungen
  loesungen.md    Musterlösungen mit Rechenweg und Begründung, gleiche Nummerierung
  code/           optional: lauffähiger Code (.cpp/.cu, Makefile, job.sbatch)
```

Gemeinsame Mess- und Prüf-Helfer für alle Kapitel liegen in
[`_common/bench.hpp`](_common/bench.hpp) (`ec::best_of`, `ec::row`, `ec::nahe`); die Makefiles
binden sie mit `-I../../_common` ein.

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

| Status | Kapitel | Thema | Quellen |
|---|---|---|---|
| [ ] | `01-grundlagen-axpy` | Einführung, Effizienzbegriff, axpy, Zeitmessung | VL 1, Blatt 2 |
| [ ] | `02-rechnerarchitektur` | Rechnerarchitektur-Prinzipien, Pipelining, Hazards, ILP | VL 2, Blatt 3 |
| [ ] | `03-miniaturisierung` | Transistoren, Moore, Dennard, Power Wall | VL 3 |
| [ ] | `04-ideales-cache-modell` | Cache-Hierarchie, Ideal-Cache-Modell, Stride, Pointer Chasing | VL 4, Ex-4, Blatt 5 |
| [ ] | `05-gemm-analyse` | GEMM, Memory Layout, Blocking, I/O-Komplexität | VL 5, Ex-5 |
| [ ] | `05b-simd` | SIMD, Vektorregister, Inline-Assembly, externe Kernel | Ex-3-SIMD, Blatt 4 |
| [x] | **[`06-openmp-grundlagen`](06-openmp-grundlagen/)** ★ | Fork/Join, `parallel for`, Scoping, Clauses, Reduktion, Scheduling, false sharing | VL 6, Blatt 6 |
| [x] | **[`07-openmp-tasks`](07-openmp-tasks/)** ★ | Tasks, `depend`, `taskwait`, Bernstein, Task-Graph, kritischer Pfad, Arbeit/Tiefe | VL 9-task, Blatt 8.1 |
| [ ] | `08-barnes-hut` | n-Body, Quadtree, Approximation, Lastverteilung | VL 7, Blatt 7.1 |
| [ ] | `09-rendering-raytracing` | Ray Tracing, Ray-Dreieck-Schnitt, Shading, Parallelisierung | VL 9-Render, Blatt 7.2 |
| [ ] | `10-amdahl-gustafson-roofline` | Speedup, Effizienz, Amdahl, Gustafson, Sun-Ni, Roofline, arithmetische Intensität | VL 10, Blatt 8.2/8.3, Blatt 10 |
| [ ] | **`11-cuda-grundlagen`** ★ | GPU-Architektur, SIMT, Warps, Thread/Block/Grid, erster Kernel, Speichertransfer, Slurm | VL 11, Slurm_CUDA, Blatt 9 |
| [ ] | **`12-cuda-performance`** ★ | Coalescing, Shared Memory, Occupancy, Divergenz, Reduktion, Roofline auf GPU | VL 12, CUDA-2D-Hints, Blatt 11 |
| [ ] | `13-pointer-jumping` | PRAM-Modell, Pointer Jumping, List Ranking, Präfixsumme | VL 13 |
| [ ] | `14-strassen` | Matrixmultiplikation, Strassen, Komplexität, Rekursion | VL 14 |

★ = Schwerpunktkapitel

---

## Empfohlener Lernpfad

1. **Fundament** (schnell durch): 01 → 02 → 03
2. **Speicher verstehen** — die Grundlage für alles Weitere: 04 → 05 → 05b
3. **Parallelität auf der CPU** (Schwerpunkt): 06 → 07 → 08 → 09
4. **Performance-Modelle** — wird in CUDA sofort gebraucht: 10
5. **GPU** (Schwerpunkt): 11 → 12
6. **Theorie/Algorithmen**: 13 → 14

Kapitel 10 lohnt sich auch schon vor 06, wenn man Speedup-Aufgaben früh üben will.

---

## Praktische Hinweise

- **OpenMP** läuft lokal: `gcc -O2 -fopenmp prog.c -o prog`, 8 Kerne verfügbar.
- **CUDA** läuft **nicht** lokal (kein `nvcc`, keine NVIDIA-GPU). Der Code in den
  CUDA-Kapiteln ist für den **Ara-Cluster** ausgelegt und kommt mit `job.sbatch`.
  Die Papieraufgaben sind ohne Cluster lösbar — sie sind der klausurrelevante Teil.
- Plots/Benchmarks: `source ../.effcom-venv/bin/activate` (matplotlib, numpy, pandas).
