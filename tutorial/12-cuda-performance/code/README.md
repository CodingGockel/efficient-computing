# Code zu Kapitel 12 — CUDA II (Performance)

> **Dieser Code ist auf diesem Rechner nicht übersetzt worden** — es gibt hier weder `nvcc`
> noch eine NVIDIA-GPU. Er ist für den **Ara-Cluster** geschrieben. In den Lösungen stehen
> deshalb **hergeleitete Erwartungswerte statt Messwerten**; die echten Zahlen trägt man nach
> dem ersten Lauf selbst nach.

## Dateien

| Datei | Aufgabe | Inhalt |
|---|---|---|
| [`../../_common/cuda_check.hpp`](../../_common/cuda_check.hpp) | — | gemeinsam mit Kapitel 11: `CUDA_CHECK`, `ec::GpuTimer` (Events), `ec::gb_pro_s`, Geräteinfo |
| `reduktion.cu` | 12.5 | $s = \sum (x_i - y_i)^2$ in vier Fassungen: Atomic pro Thread, Shared Memory, Shared Memory **divergent** (12.5h), Warp-Shuffle |
| `matmul.cu` | 12.7 | Matrixprodukt naiv vs. gekachelt (`TILE = 32`), mit Wächtern für `N % TILE != 0` |
| `Makefile` | — | `make`, `make regs` (`-Xptxas -v`), `make sanitize` (`compute-sanitizer`) |
| `job.sbatch` | — | Slurm-Job, der alles nacheinander laufen lässt |

## Ablauf auf dem Cluster

```bash
scp -r ../../_common ../code <kuerzel>@login1.ara.uni-jena.de:~/kap12/
ssh <kuerzel>@login1.ara.uni-jena.de
cd kap12/code

module load nvidia/cuda/12.4
make                       # Standard: ARCH=sm_80 (A100)

sbatch job.sbatch
squeue -u $USER
cat out.txt
```

Beim Kopieren muss `_common/` mit — die Makefiles binden es über `-I../../_common` ein. Am
einfachsten kopiert man den ganzen `tutorial/`-Ordner.

## Einzelaufrufe

```bash
./bin/reduktion                    # Aufgabe 12.5, n = 10^8
./bin/reduktion 1000000            # kleiner, für schnelle Durchläufe
./bin/matmul                       # Aufgabe 12.7: N = 1024, 2048, 4096 + Gegenprobe N = 1000
./bin/matmul 2048                  # eine Größe

make regs                          # Aufgabe 12.7e: Register/Thread und Shared/Block
make sanitize                      # Aufgabe 12.8e: memcheck, racecheck, synccheck
```

## Was man sich beim Lesen ansehen sollte

- **`reduktion.cu`:** Die vier Fassungen unterscheiden sich nur darin, *wie zusammengeführt
  wird*. Fassung 1 → 2 beseitigt einen strukturellen Engpass und bringt Faktoren; 2 → 3
  optimiert innerhalb eines Kernels, der schon am Speicherbus hängt, und bringt fast nichts.
  Das Programm rechnet die Roofline-Schranke selbst aus und druckt sie über die Tabelle —
  daran misst man das Ergebnis, nicht an GFLOP/s.
- **`reduktion.cu`, Fassung 2 vs. 2b:** dieselbe Rechnung, dieselbe Zahl an Additionen, nur
  `if (tid < k)` gegen `if (tid % (2k) == 0)`. Der Unterschied ist reine Warp-Divergenz.
- **`matmul.cu`:** die beiden `__syncthreads()` und die drei Wächter, die der
  Vorlesungsfassung für `N % TILE != 0` fehlen. Die Gegenprobe mit `N = 1000` läuft am Ende
  automatisch mit.
- **`make sanitize`:** `compute-sanitizer` findet Shared-Memory-Races und divergente
  Barrieren, die ein `CUDA_CHECK` prinzipiell nicht sehen kann.

## Laufzeit

`./bin/reduktion` startet Fassung 1 mit 10⁸ `atomicAdd` auf **eine** Adresse — das dauert
absichtlich lange (der Punkt der Aufgabe) und wird deshalb nur einmal gemessen.
`./bin/matmul` mit `N = 4096` braucht für den naiven Kernel ebenfalls spürbar Zeit. Das
Job-Skript setzt daher `--time=00:40:00` und `--mem=16G` (drei 4096²-Matrizen à 64 MB auf
Host und Device).
