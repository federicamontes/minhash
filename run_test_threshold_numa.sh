#!/bin/bash

OUTPUT_DIR="test_threshold_numa"
TEST_DIR="./test"
NUM_RUNS=10
# Fixed at 20 threads as requested
FIXED_THREADS=20

# Threshold Insertion values to iterate over
THRESHOLDS=(5 100 500)

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=1000000
SKETCH_SIZE=100
INITIAL_SIZE=0
HASH_COEFF=2
ALGORITHM=1

# Perf events for hardware analysis
PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

./compile.sh fcds
./compile.sh concurrent
cd build
mkdir -p "$OUTPUT_DIR"

# Function to generate the CPU list based on your NUMA topology
get_cpu_list() {
    local n=$1
    local list=""
    if [ "$n" -le 20 ]; then
        # Stay on Node 0 (even cores: 0, 2, 4...)
        for ((i=0; i<n; i++)); do
            list+="$((i*2)),"
        done
    else
        # Fill Node 0 completely (20 cores), then Node 1 (odd cores: 1, 3, 5...)
        for ((i=0; i<20; i++)); do
            list+="$((i*2)),"
        done
        for ((i=0; i<$((n-20)); i++)); do
            list+="$((i*2 + 1)),"
        done
    fi
    echo "${list%,}" # remove trailing comma
}

# Generate the list once since FIXED_THREADS doesn't change
CPU_LIST=$(get_cpu_list "$FIXED_THREADS")
echo "Enforcing NUMA 0 pinning on Even Cores: $CPU_LIST"

for THRESH in "${THRESHOLDS[@]}"; do
    for WP in "${WRITE_PROBS[@]}"; do
        echo "Threshold: $THRESH | WP: $WP | Threads: $FIXED_THREADS | CPUs: $CPU_LIST"
        
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            # Base Filenames
            BASE_FCDS="fcds_thresh${THRESH}_ops${NUM_OPS}_wp${WP}_threads${FIXED_THREADS}_run${RUN}"
            BASE_CONC="conc_thresh${THRESH}_ops${NUM_OPS}_wp${WP}_threads${FIXED_THREADS}_run${RUN}"

            # --- FCDS ---
            # taskset -c ensures internal thread pinning doesn't use odd cores
            taskset -c "$CPU_LIST" numactl --localalloc \
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESH" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
            
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
            taskset -c "$CPU_LIST" numactl --localalloc \
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESH" "$WP" "$HASH_COEFF" > /dev/null 2>&1

            # --- CONCURRENT ---
            taskset -c "$CPU_LIST" numactl --localalloc \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESH" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            taskset -c "$CPU_LIST" numactl --localalloc \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESH" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
        done
    done
done

echo "Tests complete. Data in build/$OUTPUT_DIR"