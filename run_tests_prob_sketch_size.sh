#!/bin/bash

OUTPUT_DIR="test_sketch_size"
TEST_DIR="./test"
NUM_RUNS=10
MAX_THREADS=$(nproc)

# Specific sketch sizes up to 2048
# These cover small-fit to L1-saturation scenarios
SKETCH_SIZES=(64 128 256 512 1024 1536 2048)

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=1000000
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1
NUMA=$(numactl --hardware | grep "available:" | awk '{print $2}')
echo "NUMA=$NUMA"

PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

./compile.sh fcds
./compile.sh concurrent
cd build
mkdir -p "$OUTPUT_DIR"

echo "Running benchmarks with Max Threads ($MAX_THREADS) and Sketch Sizes up to 2048"

for SIZE in "${SKETCH_SIZES[@]}"; do
    for WP in "${WRITE_PROBS[@]}"; do
        echo "Size: $SIZE | WP: $WP | Threads: $MAX_THREADS"
        
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            # Base Filenames
            BASE_FCDS="fcds_sz${SIZE}_wp${WP}_run${RUN}"
            BASE_CONC="conc_sz_numa${SIZE}_wp${WP}_run${RUN}"

            # --- FCDS ---
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > /dev/null 2>&1

            # --- CONCURRENT ---
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" "$NUMA" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" "$NUMA" > /dev/null 2>&1
        done
    done
done

echo "Tests complete. Data in build/$OUTPUT_DIR"
