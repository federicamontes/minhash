#!/bin/bash

set -euo pipefail

OUTPUT_DIR="test_fix_wr"
TEST_DIR="./test"

# Number of runs per configuration
NUM_RUNS=10

# Total available CPU cores
MAX_THREADS=$(nproc)

# --- THREAD LIST LOGIC ---
QUARTER=$((MAX_THREADS / 4))
# Your logic points: 1, Q/2, Q, 2Q, 3Q, MAX
RAW_POINTS="3 $((QUARTER/2)) $QUARTER $((QUARTER*2)) $((QUARTER*3)) $MAX_THREADS"

# Generate symmetric (workers, queries) configurations
CONFIGS=()
declare -A SEEN

for t in $RAW_POINTS; do
    # Configuration 1: (MAX - t, t)
    w1=$((MAX_THREADS - t))
    q1=$t
    if [ "$w1" -gt 0 ] && [ "$q1" -gt 0 ]; then
        key="${w1},${q1}"
        if [[ -z "${SEEN[$key]:-}" ]]; then
            CONFIGS+=("$w1 $q1")
            SEEN[$key]=1
        fi
    fi

    # Configuration 2: (t, MAX - t)
    w2=$t
    q2=$((MAX_THREADS - t))
    if [ "$w2" -gt 0 ] && [ "$q2" -gt 0 ]; then
        key="${w2},${q2}"
        if [[ -z "${SEEN[$key]:-}" ]]; then
            CONFIGS+=("$w2 $q2")
            SEEN[$key]=1
        fi
    fi
done

echo "Total threads: $MAX_THREADS"
echo "Worker / Query configurations (using Quarter-Step logic):"
for cfg in "${CONFIGS[@]}"; do
    echo "  workers=${cfg%% *}, queries=${cfg##* }"
done

# Default parameters
NUM_INSERTIONS=1000000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1   # only for concurrent test

NUMA=$(numactl --hardware | grep "available:" | awk '{print $2}')
echo "NUMA=$NUMA"


# Compile both versions
./compile.sh fcds
./compile.sh concurrent

cd build || exit 1

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run tests
for cfg in "${CONFIGS[@]}"; do
    THREADS=${cfg%% *}
    NUM_QUERY_THREADS=${cfg##* }

    echo "=== Configuration: workers=$THREADS, queries=$NUM_QUERY_THREADS ==="

    # --------------------
    # FCDS (requires >= 2 workers)
    # --------------------
    if [ "$THREADS" -ge 2 ]; then
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            CMD=(
                "${TEST_DIR}/test_fcds_fix_wr"
                "$NUM_INSERTIONS"
                "$SKETCH_SIZE"
                "$INITIAL_SIZE"
                "$THREADS"
                "$THRESHOLD_INSERTION"
                "$NUM_QUERY_THREADS"
                "$HASH_COEFF"
            )

            OUT_FILE="${OUTPUT_DIR}/fcds_fixwr_ins${NUM_INSERTIONS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_workers${THREADS}_queries${NUM_QUERY_THREADS}_run${RUN}.txt"

            echo "Running: ${CMD[*]}"
            "${CMD[@]}" > "$OUT_FILE" 2>&1
        done
    else
        echo "Skipping FCDS (workers < 2)"
    fi

    # --------------------
    # Concurrent (no constraint)
    # --------------------
    for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
        CMD=(
            "${TEST_DIR}/test_conc_fix_wr"
            "$NUM_INSERTIONS"
            "$SKETCH_SIZE"
            "$INITIAL_SIZE"
            "$THREADS"
            "$THRESHOLD_INSERTION"
            "$NUM_QUERY_THREADS"
            "$ALGORITHM"
            "$HASH_COEFF"
            "$NUMA"
        )

        OUT_FILE="${OUTPUT_DIR}/conc_fixwr_numa_ins${NUM_INSERTIONS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_workers${THREADS}_queries${NUM_QUERY_THREADS}_run${RUN}.txt"

        echo "Running: ${CMD[*]}"
        "${CMD[@]}" > "$OUT_FILE" 2>&1
    done
done

echo "All fixed-write-rate tests completed."
