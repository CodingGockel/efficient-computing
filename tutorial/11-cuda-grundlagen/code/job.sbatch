#!/bin/bash
# Kapitel 11 - alle Beispiele auf einer GPU des Ara-Clusters ausfuehren.
#
#   sbatch job.sbatch
#   squeue -u $USER
#   scancel <jobid>
#
# Die Ausgabe landet in out.txt (siehe --output).

#SBATCH --job-name=kap11_cuda
#SBATCH --output=out.txt
#SBATCH --time=00:30:00
#SBATCH --gres=gpu:1          # OHNE diese Zeile bekommt der Job KEINE GPU
#SBATCH --cpus-per-task=4
#SBATCH --mem=8G

set -e

module load nvidia/cuda/12.4

echo "=== Welche Karte haben wir bekommen? ==="
nvidia-smi
echo

# sm_80 = A100 (Ampere), sm_70 = V100 (Volta), sm_60 = P100 (Pascal).
# Passt das Flag nicht zur Karte, scheitert der Start mit
#   "no kernel image is available for execution on the device".
ARCH=${ARCH:-sm_80}
make ARCH="$ARCH"

echo "=== Aufgabe 11.3c - korrigierte Fassung ==="
./bin/fehlersuche
echo

echo "=== Aufgabe 11.4 - axpy, Transfer und Kernel getrennt ==="
./bin/axpy
echo

echo "=== Aufgabe 11.8 - Grid-Stride-Loop, vier Konfigurationen ==="
./bin/axpy config 10000000
echo

echo "=== Aufgabe 11.5 - 2D-Indexierung, .x als Spalte vs. als Zeile ==="
./bin/bild2d
