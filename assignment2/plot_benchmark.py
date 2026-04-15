import pandas as pd
import matplotlib.pyplot as plt
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-i", "--input", default="results.csv", help="CSV input file")
parser.add_argument("-o", "--output", default=None, help="Save plot to file (optional)")
args = parser.parse_args()

df = pd.read_csv(args.input)

plt.figure(figsize=(10, 6))
plt.plot(df["n"], df["time_spent"], marker="o", linewidth=1.5, markersize=3)
plt.xlabel("n")
plt.ylabel("Time (s)")
plt.title("DAXPY Benchmark: Runtime vs. n")
plt.grid(True)
plt.tight_layout()

if args.output:
    plt.savefig(args.output, dpi=150)
    print(f"Plot saved to {args.output}")
else:
    plt.show()
