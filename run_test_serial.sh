#!/bin/bash

OUTPUT_DIR="test_prob"
TEST_DIR="./test"
NUM_RUNS=5  # Changed from 10 to 5

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=1000000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1

# Hardware events for cache analysis
PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

# Ensure binaries are ready
./compile.sh fcds
./compile.sh concurrent
./compile.sh serial
cd build
mkdir -p "$OUTPUT_DIR"

for WP in "${WRITE_PROBS[@]}"; do
    echo "Processing Window Probability: $WP"

    for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
        
        # --- 1. SERIAL BASELINE (Required for the black dotted line in your plot) ---
        # Filename format matched to your previous 'ls' output: test_serial_wp0.1
        # We only run this once per WP (or per run if you want to average them)
        BASE_SER="test_serial_wp${WP}_run${RUN}"
        "${TEST_DIR}/test_serial_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_SER}" 2>&1

        # --- 2. FCDS TEST (Strictly 2 Threads) ---
        THREADS_FCDS=2
        BASE_FCDS="fcds_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_wp${WP}_threads${THREADS_FCDS}_run${RUN}"
        
        # Capture program output
        "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS_FCDS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
        
        # Capture perf stats
        perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
        "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS_FCDS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > /dev/null 2>&1


        # --- 3. CONCURRENT TEST (Strictly 1 Thread) ---
        THREADS_CONC=1
        BASE_CONC="conc_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_wp${WP}_threads${THREADS_CONC}_run${RUN}"
        
        # Capture program output
        "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS_CONC" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
        
        # Capture perf stats
        perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
        "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS_CONC" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
        
    done
done

echo "Tests finished. Results in build/$OUTPUT_DIR"
