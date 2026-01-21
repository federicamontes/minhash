#!/bin/bash

OUTPUT_DIR="test_threshold"
TEST_DIR="./test"
NUM_RUNS=10
MAX_THREADS=$(nproc)

# Iterate over these thresholds as requested
THRESHOLDS=(5 100 500)

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=1000000
SKETCH_SIZE=100
INITIAL_SIZE=0
HASH_COEFF=2
ALGORITHM=1

# Kept exactly as your original script
PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

./compile.sh fcds
./compile.sh concurrent
cd build
mkdir -p "$OUTPUT_DIR"

echo "Running with fixed threads: $MAX_THREADS"
echo "Thresholds: ${THRESHOLDS[@]}"

for THRESH in "${THRESHOLDS[@]}"; do
    for WP in "${WRITE_PROBS[@]}"; do
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            
            # --- FCDS TEST ---
            if [ "$MAX_THREADS" -gt 2 ]; then
                BASE_FCDS="fcds_thresh${THRESH}_ops${NUM_OPS}_wp${WP}_threads${MAX_THREADS}_run${RUN}"
                
                # Run test and capture program output
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESH" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
                
                # Run again with perf
                perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESH" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            fi

            # --- CONCURRENT TEST ---
            BASE_CONC="conc_thresh${THRESH}_ops${NUM_OPS}_wp${WP}_threads${MAX_THREADS}_run${RUN}"
            
            # Run test and capture program output
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESH" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
            # Run again with perf
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESH" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            
        done
    done
done

echo "All tests completed. Output files (.txt) and cache data (.perf) are in build/$OUTPUT_DIR"