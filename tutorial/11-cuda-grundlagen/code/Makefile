# Kapitel 11 - CUDA I
#
# Laeuft NICHT lokal - nvcc und NVIDIA-GPU gibt es nur auf dem Ara-Cluster:
#
#   module load nvidia/cuda/12.4
#   make                  alles bauen (Standard: sm_80 = A100)
#   make ARCH=sm_70       fuer eine V100 uebersetzen (P100: sm_60)
#   make regs             Register- und Shared-Memory-Verbrauch je Kernel
#   make clean
#
# Ausgefuehrt wird ueber Slurm:  sbatch job.sbatch

NVCC  ?= nvcc
ARCH  ?= sm_80
FLAGS ?= -std=c++17 -O2 -arch=$(ARCH) -I../../_common -Xcompiler -Wall,-Wextra

BIN    := bin
PROGS  := $(BIN)/axpy $(BIN)/bild2d $(BIN)/fehlersuche
COMMON := ../../_common/cuda_check.hpp

.PHONY: all regs clean

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%: %.cu $(COMMON) | $(BIN)
	$(NVCC) $(FLAGS) $< -o $@

# -Xptxas -v zeigt pro Kernel: Register je Thread, Shared Memory, Spilling.
# Die Zahlen braucht man fuer die Occupancy-Rechnung in Kapitel 12.
regs: $(COMMON) | $(BIN)
	@for f in axpy bild2d fehlersuche; do \
		echo "=== $$f ==="; \
		$(NVCC) $(FLAGS) -Xptxas -v -c $$f.cu -o $(BIN)/$$f.o 2>&1 | grep -E "ptxas|registers|smem" ; \
	done

clean:
	rm -rf $(BIN)
