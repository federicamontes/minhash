#!/bin/bash

OUTPUT_DIR="test_ops_scaling"
TEST_DIR="./test"
NUM_RUNS=5
MAX_THREADS=$(nproc)

# Fixed settings
THRESHOLD_INSERTION=5
WRITE_PROBS=(0.1 0.5 0.9)
OPS_LIST=(10000 100000 1000000 10000000)
SKETCH_SIZE=100
INITIAL_SIZE=0
HASH_COEFF=2
ALGORITHM=1

./compile.sh fcds
./compile.sh concurrent

cd build
mkdir -p "$OUTPUT_DIR"

echo "Running scaling tests with MAX_THREADS: $MAX_THREADS"
echo "Workload sizes: ${OPS_LIST[@]}"

for NUM_OPS in "${OPS_LIST[@]}"; do
    echo "Processing NUM_OPS: $NUM_OPS"
    
    for WP in "${WRITE_PROBS[@]}"; do
        
        # --- 1. SERIAL TEST (Run once per configuration to save time) ---
        # Filename pattern maintained for Python script compatibility
        BASE_SER="test_serial_wp${WP}_ops${NUM_OPS}"
        echo "  [Serial] Running WP $WP..."
        "${TEST_DIR}/test_serial_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_SER}.txt" 2>&1

        # --- 2. CONCURRENT RUNS ---
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            
            # FCDS TEST (Same filename pattern as original script)
            if [ "$MAX_THREADS" -gt 2 ]; then
                BASE_FCDS="fcds_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_wp${WP}_threads${MAX_THREADS}_run${RUN}"
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
            fi

            # CONCURRENT TEST (Same filename pattern as original script)
            BASE_CONC="conc_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_wp${WP}_threads${MAX_THREADS}_run${RUN}"
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$MAX_THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
        done
    done
done

echo "All tests completed. Files are in build/$OUTPUT_DIR"