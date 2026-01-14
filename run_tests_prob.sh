#!/bin/bash

OUTPUT_DIR="test_prob"
TEST_DIR="./test"

# Number of runs per test
NUM_RUNS=10

# Get the number of available CPU cores
MAX_THREADS=$(nproc)

# Generate thread counts as powers of 2 up to MAX_THREADS
THREAD_COUNTS=()
t=2
while [ $t -lt $MAX_THREADS ]; do
    THREAD_COUNTS+=($t)
    t=$((t*2))
done

# Ensure we include MAX_THREADS itself if not already in the list
if [[ ! " ${THREAD_COUNTS[@]} " =~ " ${MAX_THREADS} " ]]; then
    THREAD_COUNTS+=($MAX_THREADS)
fi

echo "Thread counts to test: ${THREAD_COUNTS[@]}"


# Write probabilities to test
WRITE_PROBS=(0.1 0.5 0.9)

# Default parameters
NUM_OPS=100000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1  # will only be used for concurrent test

# 1. Compile both versions
./compile.sh fcds
./compile.sh concurrent

cd build

# Create base output directory
mkdir -p "$OUTPUT_DIR"

# 2. Run tests
for THREADS in "${THREAD_COUNTS[@]}"; do
    # fcds test
    if [ "$THREADS" -eq 2 ]; then
        echo "Skipping FCDS test for thread count: 2"
        continue
    fi
    for WP in "${WRITE_PROBS[@]}"; do
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            OUT_FILE="${OUTPUT_DIR}/fcds_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_wp${WP}_threads${THREADS}_run${RUN}.txt"
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "$OUT_FILE" 2>&1
        done
    done

    # concurrent test
    for WP in "${WRITE_PROBS[@]}"; do
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            OUT_FILE="${OUTPUT_DIR}/conc_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_wp${WP}_threads${THREADS}_run${RUN}.txt"
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "$OUT_FILE" 2>&1
        done
    done
done

echo "All tests completed."
