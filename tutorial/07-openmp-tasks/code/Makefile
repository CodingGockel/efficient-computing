# Kapitel 07 - OpenMP Tasks
#
#   make            alle Beispiele bauen
#   make run        alle Beispiele nacheinander ausfuehren
#   make graph      Task-Graph aus Aufgabe 7.3 mit p = 1,2,3,4,8 messen
#   make tree       unbalancierten Baum mit p = 1,2,4,8 messen
#   make granular   Aufgabe 7.6f: drei Arbeitsmengen pro Knoten vergleichen
#   make sort       Quicksort mit p = 1,2,4,8 messen
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -fopenmp -I../../_common

BIN      := bin
PROGS    := $(BIN)/fib_tasks $(BIN)/tree_tasks $(BIN)/taskwait_vs_taskgroup \
            $(BIN)/taskgraph $(BIN)/qsort_tasks
COMMON   := ../../_common/bench.hpp

N        ?= 32

.PHONY: all run graph tree granular sort clean

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%: %.cpp $(COMMON) | $(BIN)
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	@echo "=== taskwait vs. taskgroup ==="; $(BIN)/taskwait_vs_taskgroup
	@echo "=== Task-Graph (Aufgabe 7.3) ==="; $(BIN)/taskgraph
	@echo "=== Fibonacci (Aufgabe 7.5) ==="; $(BIN)/fib_tasks $(N)
	@echo "=== Baum (Aufgabe 7.6) ==="; $(BIN)/tree_tasks
	@echo "=== Quicksort (Aufgabe 7.9) ==="; $(BIN)/qsort_tasks

# Makespan gegen die Prozessorzahl: muss bei p>=3 gegen T_inf = 14 gehen
graph: $(BIN)/taskgraph
	@for p in 1 2 3 4 8; do OMP_NUM_THREADS=$$p $(BIN)/taskgraph | tail -4; echo; done

tree: $(BIN)/tree_tasks
	@for p in 1 2 4 8; do OMP_NUM_THREADS=$$p $(BIN)/tree_tasks; echo; done

# Aufgabe 7.6f: ab wann entscheidet der Cutoff?
granular: $(BIN)/tree_tasks
	@for w in 2000 100 20; do $(BIN)/tree_tasks 30 $$w; echo; done

sort: $(BIN)/qsort_tasks
	@for p in 1 2 4 8; do OMP_NUM_THREADS=$$p $(BIN)/qsort_tasks; echo; done

clean:
	rm -rf $(BIN)
