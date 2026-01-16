#!/bin/bash

OUTPUT_DIR="test_sketch_size_numa"
TEST_DIR="./test"
NUM_RUNS=10
# Fixed at 20 threads as requested
FIXED_THREADS=20

# Specific sketch sizes up to 2048
SKETCH_SIZES=(64 128 256 512 1024 1536 2048)

WRITE_PROBS=(0.1 0.5 0.9)
NUM_OPS=100000
INITIAL_SIZE=0
THRESHOLD_INSERTION=5
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
        # Node 0 even cores
        for ((i=0; i<20; i++)); do
            list+="$((i*2)),"
        done
        # Node 1 odd cores
        for ((i=0; i<$((n-20)); i++)); do
            list+="$((i*2 + 1)),"
        done
    fi
    echo "${list%,}" # remove trailing comma
}

echo "Running benchmarks with $FIXED_THREADS threads pinned to NUMA Node 0"

for SIZE in "${SKETCH_SIZES[@]}"; do
    CPU_LIST=$(get_cpu_list "$THREADS")
    echo "Running with $THREADS threads on CPUs: $CPU_LIST"

    for WP in "${WRITE_PROBS[@]}"; do
        echo "Size: $SIZE | WP: $WP | Threads: $FIXED_THREADS | NUMA: Node 0"
        
        for ((RUN=1; RUN<=NUM_RUNS; RUN++)); do
            # Base Filenames (added _threads20 for the python parser to keep track)
            BASE_FCDS="fcds_sz${SIZE}_wp${WP}_threads${FIXED_THREADS}_run${RUN}"
            BASE_CONC="conc_sz${SIZE}_wp${WP}_threads${FIXED_THREADS}_run${RUN}"

            # --- FCDS ---
            # --cpunodebind=0 pins to cores on Node 0
            # --localalloc ensures memory is allocated on the same node
            numactl --physcpubind="$CPU_LIST" --localalloc \
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_FCDS}.txt" 2>&1
            
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_FCDS}.perf" \
            numactl --physcpubind="$CPU_LIST" --localalloc \
            "${TEST_DIR}/test_fcds_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESHOLD_INSERTION" "$WP" "$HASH_COEFF" > /dev/null 2>&1

            # --- CONCURRENT ---
            numactl ---physcpubind="$CPU_LIST" --localalloc \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > "${OUTPUT_DIR}/${BASE_CONC}.txt" 2>&1
            
            perf stat -x, -e "$PERF_EVENTS" -o "${OUTPUT_DIR}/${BASE_CONC}.perf" \
            numactl --physcpubind="$CPU_LIST" --localalloc \
            "${TEST_DIR}/test_conc_prob" "$NUM_OPS" "$SIZE" "$INITIAL_SIZE" "$FIXED_THREADS" "$THRESHOLD_INSERTION" "$ALGORITHM" "$WP" "$HASH_COEFF" > /dev/null 2>&1
        done
    done
done

echo "Tests complete. Data in build/$OUTPUT_DIR"
