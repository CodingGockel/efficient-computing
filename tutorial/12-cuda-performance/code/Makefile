# Kapitel 12 - CUDA II (Performance)
#
# Laeuft NICHT lokal - nvcc und NVIDIA-GPU gibt es nur auf dem Ara-Cluster:
#
#   module load nvidia/cuda/12.4
#   make                  alles bauen (Standard: sm_80 = A100)
#   make ARCH=sm_70       fuer eine V100 uebersetzen (P100: sm_60)
#   make regs             Register und Shared Memory je Kernel (Aufgabe 12.7e)
#   make sanitize         compute-sanitizer ueber beide Programme (Aufgabe 12.8e)
#   make clean
#
# Ausgefuehrt wird ueber Slurm:  sbatch job.sbatch

NVCC  ?= nvcc
ARCH  ?= sm_80
FLAGS ?= -std=c++17 -O2 -arch=$(ARCH) -I../../_common -Xcompiler -Wall,-Wextra

BIN    := bin
PROGS  := $(BIN)/reduktion $(BIN)/matmul
COMMON := ../../_common/cuda_check.hpp

.PHONY: all regs sanitize clean

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%: %.cu $(COMMON) | $(BIN)
	$(NVCC) $(FLAGS) $< -o $@

# Die beiden Zahlen fuer die Occupancy-Rechnung aus Aufgabe 12.4.
regs: $(COMMON) | $(BIN)
	@for f in reduktion matmul; do \
		echo "=== $$f ==="; \
		$(NVCC) $(FLAGS) -Xptxas -v -c $$f.cu -o $(BIN)/$$f.o 2>&1 | grep -E "ptxas|registers|smem" ; \
	done

# Findet genau die Fehler, die CUDA_CHECK nicht findet (Aufgabe 12.8e):
# Zugriffe ausserhalb der Grenzen, Shared-Memory-Races, divergente Barrieren.
sanitize: all
	compute-sanitizer --tool memcheck  $(BIN)/reduktion 1000000
	compute-sanitizer --tool racecheck $(BIN)/reduktion 1000000
	compute-sanitizer --tool synccheck $(BIN)/matmul 512

clean:
	rm -rf $(BIN)
