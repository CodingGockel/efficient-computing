# ── Configuration ────────────────────────────────────────────────────
PLOTDIR   = plots
OUTPUT    = $(PLOTDIR)/plot.png
OPT       = O0
ARGS      = 1
START_N   = 1000
STEP      = 1000

TMPDIR    = .tmp
CSV       = $(TMPDIR)/results.csv

SCRIPT    = run_benchmark.sh
PLOT      = plot_benchmark.py

# ── Validate required parameters ─────────────────────────────────────
ifndef SOURCE
    $(error SOURCE is required. Usage: make SOURCE=my_program.c)
endif

# ── Targets ──────────────────────────────────────────────────────────
.PHONY: all clean

all: $(OUTPUT)

# Step 2: plot from CSV -> save to plots/ folder, then delete .tmp
$(OUTPUT): $(CSV)
	mkdir -p $(PLOTDIR)
	python3 $(PLOT) -i $(CSV) -o $(OUTPUT)
	rm -rf $(TMPDIR)

# Step 1: create .tmp folder and run benchmark -> produces CSV
$(CSV): $(SOURCE) $(SCRIPT)
	mkdir -p $(TMPDIR)
	chmod +x $(SCRIPT)
	./$(SCRIPT) -c $(SOURCE) -O $(OPT) -o $(CSV) -a $(ARGS) -s $(START_N) -t $(STEP)

clean:
	rm -f ./benchmark_tmp
	rm -rf $(TMPDIR)
	rm -rf $(PLOTDIR)
