#!/bin/bash

set -euo pipefail

OUTPUT_DIR="test_fix_qr"
TEST_DIR="./test"

# Number of runs per configuration
NUM_RUNS=10

# Total available CPU cores
MAX_THREADS=$(nproc)

# Generate symmetric (workers, queries) configurations
CONFIGS=()
declare -A SEEN

t=1
while [ $t -lt $MAX_THREADS ]; do
    # (MAX - t, t)
    w1=$((MAX_THREADS - t))
    q1=$t
    if [ $w1 -gt 0 ]; then
        key="${w1},${q1}"
        if [[ -z "${SEEN[$key]:-}" ]]; then
            CONFIGS+=("$w1 $q1")
            SEEN[$key]=1
        fi
    fi

    # (t, MAX - t)
    w2=$t
    q2=$((MAX_THREADS - t))
    if [ $q2 -gt 0 ]; then
        key="${w2},${q2}"
        if [[ -z "${SEEN[$key]:-}" ]]; then
            CONFIGS+=("$w2 $q2")
            SEEN[$key]=1
        fi
    fi

    t=$((t * 2))
done

echo "Total threads: $MAX_THREADS"
echo "Worker / Query configurations:"
for cfg in "${CONFIGS[@]}"; do
    echo "  workers=${cfg%% *}, queries=${cfg##* }"
done

# Default parameters
NUM_QUERIES=100000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=10
ALGORITHM=1   # only for concurrent test

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
                "${TEST_DIR}/test_fcds_fix_qr"
                "$NUM_QUERIES"
                "$SKETCH_SIZE"
                "$INITIAL_SIZE"
                "$THREADS"
                "$THRESHOLD_INSERTION"
                "$NUM_QUERY_THREADS"
                "$HASH_COEFF"
            )

            OUT_FILE="${OUTPUT_DIR}/fcds_fixqr_q${NUM_QUERIES}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_workers${THREADS}_queries${NUM_QUERY_THREADS}_run${RUN}.txt"

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
            "${TEST_DIR}/test_conc_fix_qr"
            "$NUM_QUERIES"
            "$SKETCH_SIZE"
            "$INITIAL_SIZE"
            "$THREADS"
            "$THRESHOLD_INSERTION"
            "$NUM_QUERY_THREADS"
            "$ALGORITHM"
            "$HASH_COEFF"
        )

        OUT_FILE="${OUTPUT_DIR}/conc_fixqr_q${NUM_QUERIES}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_workers${THREADS}_queries${NUM_QUERY_THREADS}_run${RUN}.txt"

        echo "Running: ${CMD[*]}"
        "${CMD[@]}" > "$OUT_FILE" 2>&1
    done
done

echo "All fixed-query-rate tests completed."
