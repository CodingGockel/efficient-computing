# Assignment 2

Benchmarkes the c-files and plots the results.

---

## Usage

### Run with Make (recommended)

```bash
make SOURCE=my_program.c
```

This will:
1. Compile the C source with the -O0 optimization flag
2. Run the benchmark and save results to `.tmp/results.csv`
3. Generate a plot and save it in `plots/plot.png`

### Makefile Parameters

| Parameter | Default         | Description                              |
|-----------|-----------------|------------------------------------------|
| `SOURCE`  | `my_program.c`  | C source file to compile and benchmark   |
| `OUTPUT`  | `plot.png`      | Output plot file                         |
| `OPT`     | `O0`            | GCC optimization flag (O0, O1, O2, O3)  |
| `ARGS`    | `1`             | Number of arguments passed to binary (1=n, 2=n m, 3=n m l) |
| `START_N` | `1000`          | Starting value of n                      |
| `STEP`    | `1000`          | Step size for each benchmark iteration   |

Override parameters inline:
```bash
make SOURCE=daxpy.c OUTPUT=plot_O2.png OPT=O2 ARGS=1 STEP=500
```

### Cleanup
```bash
make clean
```

---

## Benchmark Script

The script `run_benchmark.sh` compiles the C source and runs it for increasing `n` until a segmentation fault or timeout occurs.

```bash
./run_benchmark.sh -c my_program.c -O O2 -o results.csv -a 1 -s 1000 -t 1000
```

| Flag | Description                                      |
|------|--------------------------------------------------|
| `-c` | Path to the C source file                        |
| `-O` | GCC optimization level (O0, O1, O2, O3)          |
| `-o` | Output CSV file                                  |
| `-a` | Number of arguments for the binary (1, 2 or 3)   |
| `-s` | Start value of n                                 |
| `-t` | Step size                                        |

---

## Plot Script

The script `plot_benchmark.py` reads the CSV and generates a plot.

```bash
python3 plot_benchmark.py -i results.csv -o plot.png
```

| Flag | Description                        |
|------|------------------------------------|
| `-i` | Input CSV file                     |
| `-o` | Output plot file (optional, if omitted the plot is shown directly) |

---

## Example: Compare Optimization Levels

```bash
make OPT=O0 OUTPUT=plot_O0.png
make OPT=O1 OUTPUT=plot_O1.png
make OPT=O2 OUTPUT=plot_O2.png
make OPT=O3 OUTPUT=plot_O3.png
```

---

## Notes

- The benchmark stops automatically on a **segmentation fault** (stack overflow for large `n` due to VLA allocation) or **timeout** (30s per run).
- The C program uses **Variable Length Arrays (VLA)** on the stack, which limits the maximum `n` before a segfault occurs.
- `randInRange(DBL_MIN, DBL_MAX)` generates random doubles across the full positive double range.
