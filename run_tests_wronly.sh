#!/bin/bash

OUTPUT_DIR="test_wronly"
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

# Default parameters
NUM_INSERTIONS=100000
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
for THREADS in "${THREAD_COUNTS[@]}"; do

    # fcds write-only test
    for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
        if [ "$THREADS" -eq 2 ]; then
            echo "Skipping FCDS test for thread count: 2"
            continue
        fi
        OUT_FILE="${OUTPUT_DIR}/fcds_wronly_ins${NUM_INSERTIONS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_threads${THREADS}_run${RUN}.txt"
        "${TEST_DIR}/test_fcds_wronly" \
            "$NUM_INSERTIONS" \
            "$SKETCH_SIZE" \
            "$INITIAL_SIZE" \
            "$THREADS" \
            "$THRESHOLD_INSERTION" \
            "$HASH_COEFF" \
            > "$OUT_FILE" 2>&1
    done

    # concurrent write-only test
    for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
        OUT_FILE="${OUTPUT_DIR}/conc_wronly_ins${NUM_INSERTIONS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_threads${THREADS}_run${RUN}.txt"
        "${TEST_DIR}/test_conc_wronly" \
            "$NUM_INSERTIONS" \
            "$SKETCH_SIZE" \
            "$INITIAL_SIZE" \
            "$THREADS" \
            "$THRESHOLD_INSERTION" \
            "$ALGORITHM" \
            "$HASH_COEFF" \
            > "$OUT_FILE" 2>&1
    done

done

echo "All write-only tests completed."
