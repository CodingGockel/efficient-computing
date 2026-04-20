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

.PHONY: all exercises clean

exercises:
	$(MAKE) all SOURCE=src/excercise1.c OUTPUT=plots/excercise1_O0.png OPT=O0 ARGS=1 START_N=1 STEP=10000
	$(MAKE) all SOURCE=src/excercise1.c OUTPUT=plots/excercise1_O1.png OPT=O1 ARGS=1 START_N=1 STEP=10000
	$(MAKE) all SOURCE=src/excercise1.c OUTPUT=plots/excercise1_O2.png OPT=O2 ARGS=1 START_N=1 STEP=10000
	$(MAKE) all SOURCE=src/excercise1.c OUTPUT=plots/excercise1_O3.png OPT=O3 ARGS=1 START_N=1 STEP=10000
	$(MAKE) all SOURCE=src/excercise2.c OUTPUT=plots/excercise2_O0.png OPT=O0 ARGS=2 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise2.c OUTPUT=plots/excercise2_O1.png OPT=O1 ARGS=2 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise2.c OUTPUT=plots/excercise2_O2.png OPT=O2 ARGS=2 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise2.c OUTPUT=plots/excercise2_O3.png OPT=O3 ARGS=2 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise3.c OUTPUT=plots/excercise3_O0.png OPT=O0 ARGS=3 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise3.c OUTPUT=plots/excercise3_O1.png OPT=O1 ARGS=3 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise3.c OUTPUT=plots/excercise3_O2.png OPT=O2 ARGS=3 START_N=1 STEP=10
	$(MAKE) all SOURCE=src/excercise3.c OUTPUT=plots/excercise3_O3.png OPT=O3 ARGS=3 START_N=1 STEP=10

all: $(OUTPUT)

$(OUTPUT): $(CSV)
	mkdir -p $(PLOTDIR)
	python3 $(PLOT) -i $(CSV) -o $(OUTPUT) -a $(ARGS) -O $(OPT)
	rm -rf $(TMPDIR)

$(CSV): $(SOURCE) $(SCRIPT)
	mkdir -p $(TMPDIR)
	chmod +x $(SCRIPT)
	./$(SCRIPT) -c $(SOURCE) -O $(OPT) -o $(CSV) -a $(ARGS) -s $(START_N) -t $(STEP)

clean:
	rm -f ./benchmark_tmp
	rm -rf $(TMPDIR)
	rm -rf $(PLOTDIR)
