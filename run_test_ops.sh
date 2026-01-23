#!/bin/bash

OUTPUT_DIR="test_ops_scaling"
TEST_DIR="./test"
NUM_RUNS=5
MAX_THREADS=$(nproc)

# Fixed threshold as requested
THRESH=5

# Workload sizes to iterate over
OPS_LIST=(10000 100000 1000000 10000000)

WRITE_PROBS=(0.1 0.5 0.9)
SKETCH_SIZE=100
INITIAL_SIZE=0
HASH_COEFF=2
ALGORITHM=1

# Compile only what's necessary
./compile.sh fcds
./compile.sh concurrent

cd build
mkdir -p "$OUTPUT_DIR"

echo "Running scaling tests with MAX_THREADS: $MAX_THREADS"
echo "Workload sizes: ${OPS_LIST[@]}"

for NUM_OPS in "${OPS_LIST[@]}"; do
    echo "--- Testing NUM_OPS: $NUM_OPS ---"
    
    for WP in "${WRITE_PROBS[@]}"; do
        
        # --- 1. SERIAL TEST (Run once per WP/OPS combo) ---
        echo "  [Serial] WP=$WP OPS=$NUM_OPS"
        BASE_SER="test_serial_wp${WP}_ops${NUM_OPS}"
        "${TEST_DIR}/test_serial_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_SER}.txt" 2>&1

        # --- 2. CONCURRENT RUNS ---
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            
            # FCDS TEST
            if [ "$MAX_THREADS" -gt 2 ]; then
                BASE_FCDS="fcds_thresh${THRESH}_ops${NUM_OPS}_wp${WP}_threads${MAX_THREADS}_run${RUN}"
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESH" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
            fi

            # CONCURRENT TEST
            BASE_CONC="conc_thresh${THRESH}_ops${NUM_OPS}_wp${WP}_threads${MAX_THREADS}_run${RUN}"
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESH" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
        done
    done
done

echo "All scaling tests completed. Output files are in build/$OUTPUT_DIR"
