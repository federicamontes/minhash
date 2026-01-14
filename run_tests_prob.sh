#!/bin/bash

OUTPUT_DIR="test_prob"
TEST_DIR="./test"
NUM_RUNS=10
MAX_THREADS=$(nproc)

# Generate thread counts
THREAD_COUNTS=()
t=2
while [ $t -lt $MAX_THREADS ]; do
    THREAD_COUNTS+=($t)
    t=$((t*2))
done
[[ ! " ${THREAD_COUNTS[@]} " =~ " ${MAX_THREADS} " ]] && THREAD_COUNTS+=($MAX_THREADS)

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=100000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1

# Defining the specific hardware events for cache analysis
# -x, outputs CSV format for easy parsing
PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

./compile.sh fcds
./compile.sh concurrent
cd build
mkdir -p "$OUTPUT_DIR"

for THREADS in "${THREAD_COUNTS[@]}"; do
    for WP in "${WRITE_PROBS[@]}"; do
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            
            # --- FCDS TEST ---
            if [ "$THREADS" -gt 2 ]; then
                BASE_FCDS="fcds_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_wp${WP}_threads${THREADS}_run${RUN}"
                
                # Run test and capture program output
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
                
                # Run again with perf to capture cache data in a separate .perf file
                perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            fi

            # --- CONCURRENT TEST ---
            BASE_CONC="conc_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_wp${WP}_threads${THREADS}_run${RUN}"
            
            # Run test and capture program output
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
            # Run again with perf to capture cache data in a separate .perf file
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            
        done
    done
done

echo "All tests completed. Output files (.txt) and cache data (.perf) are in $OUTPUT_DIR"