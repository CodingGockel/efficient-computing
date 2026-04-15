#!/bin/bash

# Defaults
OUTPUT="results.csv"
SOURCE=""
BINARY="./benchmark_tmp"
START_N=1000
STEP=1000
ARG_COUNT=1
OPT_FLAG="O0"  # default: no optimization

usage() {
    echo "Usage: $0 [-o output.csv] [-c source.c] [-O O0|O1|O2|O3] [-s start_n] [-t step] [-a 1|2|3]"
    echo "  -c  Path to the C source file to compile and benchmark"
    echo "  -O  GCC optimization flag (O0, O1, O2, O3)"
    echo "  -a  Number of arguments to pass to binary (1=n, 2=n m, 3=n m l)"
    exit 1
}

while getopts "o:c:O:s:t:a:" opt; do
    case $opt in
        o) OUTPUT="$OPTARG" ;;
        c) SOURCE="$OPTARG" ;;
        O) OPT_FLAG="$OPTARG" ;;
        s) START_N="$OPTARG" ;;
        t) STEP="$OPTARG" ;;
        a) ARG_COUNT="$OPTARG" ;;
        *) usage ;;
    esac
done

# Validate source file
if [ -z "$SOURCE" ]; then
    echo "Error: No source file specified. Use -c source.c"
    usage
fi

if [ ! -f "$SOURCE" ]; then
    echo "Error: Source file '$SOURCE' not found."
    exit 1
fi

# Validate -a flag
if [[ "$ARG_COUNT" != "1" && "$ARG_COUNT" != "2" && "$ARG_COUNT" != "3" ]]; then
    echo "Error: -a must be 1, 2 or 3."
    usage
fi

# Compile with given optimization flag
echo "Compiling $SOURCE with -$OPT_FLAG ..."
gcc "-$OPT_FLAG" "$SOURCE" -o "$BINARY" -lm
if [ $? -ne 0 ]; then
    echo "Error: Compilation failed."
    exit 1
fi
echo "Compilation successful -> $BINARY"

# Build CSV header based on arg count
if [ "$ARG_COUNT" -eq 1 ]; then
    echo "n,time_spent" > "$OUTPUT"
elif [ "$ARG_COUNT" -eq 2 ]; then
    echo "n,m,time_spent" > "$OUTPUT"
else
    echo "n,m,l,time_spent" > "$OUTPUT"
fi

echo "Starting benchmark: source=$SOURCE, opt=-$OPT_FLAG, args=$ARG_COUNT, start=$START_N, step=$STEP"
echo "Output: $OUTPUT"

n=$START_N

while true; do
    if [ "$ARG_COUNT" -eq 1 ]; then
        ARGS="$n"
    elif [ "$ARG_COUNT" -eq 2 ]; then
        ARGS="$n $n"
    else
        ARGS="$n $n $n"
    fi

    result=$(timeout 30s $BINARY $ARGS 2>/dev/null)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 139 ]; then
        echo "Segmentation fault at n=$n. Stopping."
        break
    elif [ $EXIT_CODE -eq 124 ]; then
        echo "Timeout at n=$n. Stopping."
        break
    elif [ $EXIT_CODE -ne 0 ]; then
        echo "Unexpected exit code $EXIT_CODE at n=$n. Stopping."
        break
    fi

    time_val=$(echo "$result" | grep -oP '[0-9]+\.[0-9]+' | head -1)

    if [ -z "$time_val" ]; then
        echo "Could not parse output at n=$n, skipping."
    else
        if [ "$ARG_COUNT" -eq 1 ]; then
            echo "$n,$time_val" >> "$OUTPUT"
        elif [ "$ARG_COUNT" -eq 2 ]; then
            echo "$n,$n,$time_val" >> "$OUTPUT"
        else
            echo "$n,$n,$n,$time_val" >> "$OUTPUT"
        fi
        echo "n=$n -> $time_val s"
    fi

    n=$((n + STEP))
done

# Cleanup tmp binary
rm -f "$BINARY"
echo "Done. Results saved to $OUTPUT"
