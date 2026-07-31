# Kapitel 06 - OpenMP-Grundlagen
#
#   make            alle Beispiele bauen
#   make run        alle Beispiele nacheinander ausfuehren
#   make scaling    Messreihe zu Aufgabe 6.9 (p = 1,2,4,8)
#   make race       scoping.cpp mit ThreadSanitizer -> findet die Race Conditions
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -fopenmp -I../../_common

BIN      := bin
PROGS    := $(BIN)/matvec $(BIN)/false_sharing $(BIN)/scoping
COMMON   := ../../_common/bench.hpp

M        ?= 4096
N        ?= 4096

.PHONY: all run scaling race clean

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%: %.cpp $(COMMON) | $(BIN)
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	@for p in $(PROGS); do echo "=== $$p ==="; $$p; echo; done

# Aufgabe 6.9: Speedup ueber die Threadzahl
scaling: $(BIN)/matvec
	@echo "Matrix-Vektor-Produkt, m=$(M) n=$(N)"
	@for p in 1 2 4 8; do OMP_NUM_THREADS=$$p $(BIN)/matvec $(M) $(N); echo; done

# Race Conditions in scoping.cpp nachweisen (Teil 1 und Teil 4).
# setarch -R schaltet ASLR ab -- ohne das bricht ThreadSanitizer auf aktuellen
# Kerneln mit "unexpected memory mapping" ab.
race: scoping.cpp | $(BIN)
	$(CXX) -std=c++17 -O1 -g -fopenmp -fsanitize=thread scoping.cpp -o $(BIN)/scoping_tsan
	@echo "--- ThreadSanitizer meldet die Races in Teil 1 und Teil 4 ---"
	-OMP_NUM_THREADS=4 setarch -R $(BIN)/scoping_tsan 2>&1 | grep -E 'WARNING|SUMMARY'

clean:
	rm -rf $(BIN)
