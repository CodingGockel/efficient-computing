# Makefile for Aufgabe 7.2 - OpenMP-parallelized 3D ray tracer.

CXX      ?= g++
CXXFLAGS ?= -O2 -Wall
OMPFLAGS ?= -fopenmp

SRC    := src/render.cpp
BIN    := src/render
STL    := assets/test/test.stl
PYTHON ?= python3

.PHONY: all run bench png pdf clean

all: $(BIN)

# Build the ray tracer with OpenMP enabled.
$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(SRC) -o $(BIN)

# Render once with the default scene (writes output.ppm in the cwd).
run: $(BIN)
	./$(BIN) --stl $(STL)

# Run the benchmark harness: produces the table, speedup.png and REPORT.md.
bench: $(BIN)
	$(PYTHON) src/util/benchmark.py --render $(BIN) --stl $(STL)

# Convert the most recent output.ppm to PNG (reuses the existing util script).
png:
	$(PYTHON) src/util/ppm_to_png.py output.ppm

# Convert REPORT.md (with the embedded plot) to REPORT.pdf.
pdf:
	$(PYTHON) src/util/md_to_pdf.py REPORT.md

# Remove the binary and generated artifacts.
clean:
	rm -f $(BIN) output.ppm output.png speedup.png results.csv REPORT.md REPORT.pdf
