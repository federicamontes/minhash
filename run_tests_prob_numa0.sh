#!/bin/bash

OUTPUT_DIR="test_prob_numa_pinning"
TEST_DIR="./test"
NUM_RUNS=10
MAX_THREADS=$(nproc)

# --- Generate thread counts based on your logic ---
QUARTER=$((MAX_THREADS / 4))
RAW_LIST="3 $((QUARTER/2)) $QUARTER $((QUARTER*2)) $((QUARTER*3)) $MAX_THREADS"

THREAD_COUNTS=()
for t in $RAW_LIST; do
    if [ "$t" -gt 1 ]; then
        THREAD_COUNTS+=("$t")
    fi
done

# Deduplicate and sort the list
THREAD_COUNTS=($(echo "${THREAD_COUNTS[@]}" | tr ' ' '\n' | sort -nu | tr '\n' ' '))

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=1000000
SKETCH_SIZE=100
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
HASH_COEFF=2
ALGORITHM=1

PERF_EVENTS="cache-references,cache-misses,L1-dcache-load-misses,LLC-load-misses"

./compile.sh fcds
./compile.sh concurrent
cd build
mkdir -p "$OUTPUT_DIR"

# Function to generate the CPU list
get_cpu_list() {
    local n=$1
    local list=""
    if [ "$n" -le 20 ]; then
        # Stay on NUMA 0 (even cores: 0, 2, 4...)
        for ((i=0; i<n; i++)); do
            list+="$((i*2)),"
        done
    else
        # Fill NUMA 0 (20 even cores), then use NUMA 1 (odd cores: 1, 3, 5...)
        for ((i=0; i<20; i++)); do
            list+="$((i*2)),"
        done
        for ((i=0; i<$((n-20)); i++)); do
            list+="$((i*2 + 1)),"
        done
    fi
    echo "${list%,}" # remove trailing comma
}

for THREADS in "${THREAD_COUNTS[@]}"; do
    CPU_LIST=$(get_cpu_list "$THREADS")
    echo "Running with $THREADS threads on CPUs: $CPU_LIST"

    for WP in "${WRITE_PROBS[@]}"; do
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            
            # --- FCDS TEST ---
            if [ "$THREADS" -gt 2 ]; then
                BASE_FCDS="fcds_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_wp${WP}_threads${THREADS}_run${RUN}"
                
                numactl --physcpubind="$CPU_LIST" --localalloc \
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
                
                perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
                numactl --physcpubind="$CPU_LIST" --localalloc \
                "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            fi

            # --- CONCURRENT TEST ---
            BASE_CONC="conc_prob_ops${NUM_OPS}_size${SKETCH_SIZE}_init${INITIAL_SIZE}_b${THRESHOLD_INSERTION}_alg${ALGORITHM}_wp${WP}_threads${THREADS}_run${RUN}"
            
            numactl --physcpubind="$CPU_LIST" --localalloc \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            numactl --physcpubind="$CPU_LIST" --localalloc \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SKETCH_SIZE" "$INITIAL_SIZE" "$THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
            
        done
    done
done

echo "Tests complete. Data in build/$OUTPUT_DIR"