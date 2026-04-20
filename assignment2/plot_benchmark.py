import pandas as pd
import matplotlib.pyplot as plt
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-i", "--input", default="results.csv", help="CSV input file")
parser.add_argument("-o", "--output", default=None, help="Save plot to file (optional)")
parser.add_argument("-a", "--args", default="1", help="how many args are passed to the c file")
parser.add_argument("-O", "--optimization", default="O0", help="what optimization flag was used")
args = parser.parse_args()

match args.args:
    case "1":
        titlePrefix = "DAXPY"
    case "2":
        titlePrefix = "VectrorXMatrix"
    case "3":
        titlePrefix = "MatrixXMatrix"
    case _:
        titlePrefix = ""
        
match args.optimization:
    case "O0":
        optInfo = "with optimization flag: O0"
    case "O1":
        optInfo = "with optimization flag: O1"
    case "O2":
        optInfo = "with optimization flag: O2"
    case "O3":
        optInfo = "with optimization flag: O3"
    case _:
        optInfo = ""
        
df = pd.read_csv(args.input)

plt.figure(figsize=(10, 6))
plt.plot(df["n"], df["time_spent"], marker="o", linewidth=1.5, markersize=3)
plt.xlabel("n")
plt.ylabel("Time (s)")
plt.title(f"{titlePrefix} Runtime vs. n {optInfo}")
plt.grid(True)
plt.tight_layout()

if args.output:
    plt.savefig(args.output, dpi=150)
    print(f"Plot saved to {args.output}")
else:
    plt.show()
