# Code zu Kapitel 11 — CUDA I

> **Dieser Code ist auf diesem Rechner nicht übersetzt worden.** Es gibt hier weder `nvcc`
> noch eine NVIDIA-GPU (siehe `CLAUDE.md`). Er ist für den **Ara-Cluster** geschrieben und
> muss dort übersetzt und ausgeführt werden. Anders als bei den OpenMP-Kapiteln stehen in den
> Lösungen deshalb **keine gemessenen Zahlen**, sondern hergeleitete Erwartungswerte — die
> tatsächlichen Messwerte trägt man nach dem ersten Lauf selbst nach.

## Dateien

| Datei | Aufgabe | Inhalt |
|---|---|---|
| [`../../_common/cuda_check.hpp`](../../_common/cuda_check.hpp) | — | gemeinsam mit Kapitel 12: `CUDA_CHECK`-Makro, `CUDA_CHECK_KERNEL()`, `ec::GpuTimer` (Events), `ec::nahe`, Geräteinfo — eingebunden mit `-I../../_common` |
| `fehlersuche.cu` | 11.3c | die korrigierte Fassung des fehlerhaften Programms, alle neun Fehler markiert |
| `axpy.cu` | 11.4, 11.8 | `y ← αx + y`; H→D / Kernel / D→H getrennt gemessen, Größen-Sweep, Grid-Stride-Vergleich |
| `bild2d.cu` | 11.5 | 2D-Indexierung; derselbe Kernel mit `.x` als Spalte und als Zeile |
| `Makefile` | — | Build mit `nvcc`, `ARCH`-Variable, `make regs` für `-Xptxas -v` |
| `job.sbatch` | — | Slurm-Job, der alles nacheinander laufen lässt |

## Ablauf auf dem Cluster

```bash
scp -r code/ <kuerzel>@login1.ara.uni-jena.de:~/kap11/
ssh <kuerzel>@login1.ara.uni-jena.de
cd kap11

module load nvidia/cuda/12.4
make                       # Standard: ARCH=sm_80 (A100)

sbatch job.sbatch          # Job einreichen
squeue -u $USER            # Warteschlange ansehen
cat out.txt                # Ergebnis
```

**Auf dem Login-Knoten wird nur übersetzt, nicht gerechnet.** Für interaktives Arbeiten:

```bash
salloc --gres=gpu:1 --time=00:30:00
srun ./bin/axpy 10000000
```

## Die richtige Architektur

| Karte | Compute Capability | Flag |
|---|---|---|
| Tesla P100 (Pascal) | 6.0 | `make ARCH=sm_60` |
| Tesla V100 (Volta) | 7.0 | `make ARCH=sm_70` |
| Tesla A100 (Ampere) | 8.0 | `make ARCH=sm_80` (Standard) |

Passt das Flag nicht zur zugeteilten Karte, scheitert der **Start** — nicht die Übersetzung —
mit `no kernel image is available for execution on the device`. `nvidia-smi` steht deshalb als
erstes im Job-Skript.

## Einzelaufrufe

```bash
./bin/fehlersuche                 # Aufgabe 11.3c
./bin/axpy                        # Aufgabe 11.4e: Sweep n = 2^10 .. 2^26
./bin/axpy 10000000               # eine Größe ausführlich
./bin/axpy config 10000000        # Aufgabe 11.8b: vier Grid-Konfigurationen
./bin/bild2d                      # Aufgabe 11.5: 1080 x 1920
./bin/bild2d 4096 4096            # quadratisch — hier fällt r/c-Vertauschung NICHT auf
```

Zwei Hinweise zur Laufzeit:

- `./bin/axpy config` startet den Grid-Stride-Kernel bewusst auch mit `<<<1,1>>>`. Das ist
  **ein** Thread für 10⁷ Elemente und dauert entsprechend zweistellige Sekunden — genau das
  ist der Punkt der Aufgabe. Deshalb steht im Job-Skript `--time=00:30:00`.
- Der Sweep in `./bin/axpy` geht bis `2²⁶`; das sind vier Host-Vektoren à 268 MB. Wer weiter
  hinauf will, muss `--mem` im Job-Skript anheben.

## Was man sich beim Lesen ansehen sollte

- **`axpy.cu`:** der Aufwärmlauf vor der Messung (der erste CUDA-Aufruf im Prozess
  initialisiert den Kontext und kostet einmalig hunderte Millisekunden) und dass Transfer und
  Kernel **getrennt** gemessen werden. Wer nur die Kernel-Zeit zeigt, macht eine Aussage über
  die Hardware, nicht über sein Programm.
- **`bild2d.cu`:** die beiden Kernels unterscheiden sich in genau zwei Zeilen. Beide sind
  korrekt. Der Zeitunterschied kommt allein daraus, welche Adressen die 32 Lanes eines Warps
  gleichzeitig anfassen.
- **`cuda_check.hpp`:** warum `cudaGetLastError()` **und** `cudaDeviceSynchronize()` nötig
  sind — der eine fängt Startfehler, der andere Laufzeitfehler.
