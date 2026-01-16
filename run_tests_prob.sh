#!/bin/bash

OUTPUT_DIR="test_prob"
TEST_DIR="./test"
NUM_RUNS=10
MAX_THREADS=$(nproc)

# --- Generate thread counts based on your logic ---
QUARTER=$((MAX_THREADS / 4))
# Your logic: 1, Q/2, Q, Q*2, Q*3, MAX
RAW_LIST="3 $((QUARTER/2)) $QUARTER $((QUARTER*2)) $((QUARTER*3)) $MAX_THREADS"

THREAD_COUNTS=()
for t in $RAW_LIST; do
    # Skip thread 1 as requested
    if [ "$t" -gt 1 ]; then
        THREAD_COUNTS+=("$t")
    fi
done

# Deduplicate and sort the list
THREAD_COUNTS=($(echo "${THREAD_COUNTS[@]}" | tr ' ' '\n' | sort -nu | tr '\n' ' '))

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=100000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1

# Defining the specific hardware events for cache analysis
PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

./compile.sh fcds
./compile.sh concurrent
cd build
mkdir -p "$OUTPUT_DIR"

echo "Running with threads: ${THREAD_COUNTS[@]}"

for THREADS in "${THREAD_COUNTS[@]}"; do
    for WP in "${WRITE_PROBS[@]}"; do
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            
            # --- FCDS TEST ---
            # FCDS logic from baseline (only runs if threads > 2)
            if [ "$THREADS" -gt 2 ]; then
                BASE_FCDS="fcds_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_wp${WP}_threads${THREADS}_run${RUN}"
                
                # Run test and capture program output
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
                
                # Run again with perf
                perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            fi

            # --- CONCURRENT TEST ---
            BASE_CONC="conc_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_wp${WP}_threads${THREADS}_run${RUN}"
            
            # Run test and capture program output
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
            # Run again with perf
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            
        done
    done
done

echo "All tests completed. Output files (.txt) and cache data (.perf) are in build/$OUTPUT_DIR"