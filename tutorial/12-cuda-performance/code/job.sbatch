#!/bin/bash
# Kapitel 12 - Performance-Beispiele auf einer GPU des Ara-Clusters.
#
#   sbatch job.sbatch
#   squeue -u $USER
#   cat out.txt

#SBATCH --job-name=kap12_cuda
#SBATCH --output=out.txt
#SBATCH --time=00:40:00
#SBATCH --gres=gpu:1          # OHNE diese Zeile bekommt der Job KEINE GPU
#SBATCH --cpus-per-task=4
#SBATCH --mem=16G

set -e

module load nvidia/cuda/12.4

echo "=== Welche Karte haben wir bekommen? ==="
nvidia-smi
echo

ARCH=${ARCH:-sm_80}
make ARCH="$ARCH"

echo "=== Aufgabe 12.7e - Register und Shared Memory je Kernel ==="
make ARCH="$ARCH" regs
echo

echo "=== Aufgabe 12.5 - Reduktion auf drei Wegen ==="
# Fassung 1 macht 10^8 Atomics auf eine Adresse und braucht entsprechend lange.
./bin/reduktion 100000000
echo

echo "=== Aufgabe 12.7 - Matrixprodukt naiv vs. gekachelt ==="
./bin/matmul
