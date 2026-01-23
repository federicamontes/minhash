#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>

#include <minhash.h>
#include <configuration.h>


struct minhash_configuration conf = {
    .sketch_size = 128,          /// Number of hash functions / sketch size
    .prime_modulus = (1ULL << 31) - 1,       /// Large prime for hashing (M)
    .hash_type = 1,        /// ID for hash function pointer
    .init_size = 0,                 /// Initial elements to insert (optional)
    .k = 5,
    .N = 0,
    .b = 0,
    .n_sketches = 1,
};



typedef struct {
    pthread_t tid;
    conc_minhash *sketch;
    long n_inserts;
    uint64_t startsize;
    long algorithm;
    double elapsed;
    unsigned int core_id;
    long sketch_id;
} thread_arg_t;

unsigned long count_queries;
pthread_barrier_t barrier;

static void print_params(long n_inserts, long ssize, long startsize,
                         long num_threads_total, long num_query_threads, long threshold)
{
    printf("=== Parameters ===\n");
    printf("Number of insertions     : %ld\n", n_inserts);
    printf("Sketch size              : %ld\n", ssize);
    printf("Initial size             : %ld\n", startsize);
    printf("Number of writer threads : %ld\n", num_threads_total); /* user-facing writers count */
    printf("Number of query threads  : %ld\n", num_query_threads);
    printf("Threshold (b)            : %ld\n", threshold);
    printf("    Merge Threshold (Nb) : %ld\n", threshold*num_threads_total);
    printf("Hash type                : %lu\n", conf.hash_type);
    printf("Prime modulus            : %lu\n", conf.prime_modulus);
    printf("Coefficient k-wise       : %d\n", conf.k);
    printf("====================\n");
}


static inline double elapsed_ms(struct timeval start, struct timeval end) {
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;
    return elapsed;
}

void minhash_print(uint64_t *sketch, size_t size) {

    uint64_t i;
    printf("Global sketch Value: \n");
    for(i=0; i < size; i++) {
        printf(" %lu, ", sketch[i]);
    }
    printf("\n");
}

void do_compare_with_serial(conc_minhash *sketch,
                         void *hash_functions,
                         uint64_t sketch_size,
                         uint64_t init_size,
                         long n_inserts,
                         uint64_t remainder,
                         int hash_type) 
{
    minhash_sketch *serial_sketch;

    // Initialize serial version
    minhash_init(&serial_sketch, hash_functions, sketch_size, init_size, hash_type);

    // Perform serial insertions
    for (uint64_t i = 0; i < n_inserts + init_size - remainder; i++) {
        insert(serial_sketch, i);
    }

    // Compare serial vs concurrent sketch results
    int count = 0;
    for (uint64_t i = 0; i < sketch->size; i++) {
        if (serial_sketch->sketch[i] == sketch->sketches[1]->sketch[i])
            count++;
        else
            printf("different %lu - %u --- %u!\n",
                   i, serial_sketch->sketch[i], sketch->sketches[1]->sketch[i]);
    }

    if (count == sketch->size)
        printf("✅ Test passato eddaje!\n");
    else
        printf("❌ NOOOO Test failed: %d/%lu elements match.\n", count, sketch->size);

    // Optional cleanup
    free(serial_sketch);
}

void *thread_query(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    conc_minhash *t_sketch = targ->sketch;

    pin_thread_to_core(targ->tid, targ->core_id);


    // Synchronize all threads before starting insertion
    pthread_barrier_wait(&barrier);

    int i;
    for (;;) {
       concurrent_query(t_sketch, t_sketch->sketches[0]->sketch);
       __sync_fetch_and_add(&count_queries, 1);
    }


    return NULL;
}


void *thread_insert(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    struct timeval t1, t2;
    conc_minhash *t_sketch = targ->sketch;
    
    pin_thread_to_core(targ->tid, targ->core_id);

    pthread_barrier_wait(&barrier);

    gettimeofday(&t1, NULL);
    long i;
    for (i=0; i < targ->n_inserts;i++) {
        //printf("[%lu] insertion number %ld\n", targ->tid, i);
        if (!targ->algorithm) {
            insert_conc_minhash_0(t_sketch, i+targ->startsize);
        } else {
            insert_conc_minhash(t_sketch, i+targ->startsize, targ->sketch_id);
        }
    }
    
    gettimeofday(&t2, NULL);
    targ->elapsed = elapsed_ms(t1, t2);
    //fprintf(stderr, "[thread_insert] %u has finished \n", gettid()%t_sketch->N);
    return NULL;
}




int main(int argc, const char*argv[]) {

    set_debug_enabled(false);
    bool compare_with_serial = false;

    if (argc < 9) {
        fprintf(stderr,
                "Usage: %s <number of insertions> <sketch_size> <initial size> <num_threads> <threshold insertion> <num_query_threads> <algorithm> <hash coefficient> <num sketches: default 1>\n",
                argv[0]);
        return 1;
    }

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    unsigned long node;

    long n_inserts = parse_arg(argv[1], "n_inserts", 1);
    long ssize = parse_arg(argv[2], "sketch_size", 1);
    long startsize = parse_arg(argv[3], "start_size", 0);
    long num_threads = parse_arg(argv[4], "num_threads", 1);
    long threshold = parse_arg(argv[5], "threshold", 1);
    long num_query_threads = parse_arg(argv[6], "num_query_threads", 0);
    long algorithm = parse_arg(argv[7], "algorithm", 0); //0 is baseline version, 1 is paper version


    if (argc > 8) {
        long k_cofficient = parse_arg(argv[8], "hash coefficient", 1);
        conf.k = k_cofficient;
    }

    long n_sketches = (argc == 10) ? parse_arg(argv[9], "threshold", 1) : 1;



    // when finished debugging remove comment
    //srand(time(NULL)); 

    struct timeval global_start, global_end;
    struct timeval writer_start, writer_end;
    double insert_sum = 0.0, insert_min = 1e12, insert_max = 0.0;
    
    conf.sketch_size = (uint64_t) ssize;
    if (startsize > 0) conf.init_size = (uint64_t) startsize;

    conf.N = num_threads; 
    conf.b = threshold;
    conf.n_sketches = n_sketches;

    print_params(n_inserts, conf.sketch_size, conf.init_size, conf.N, num_query_threads, conf.b);
    read_configuration(conf);


    conc_minhash *sketch;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);
    init_conc_minhash(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type, conf.N, conf.b, conf.n_sketches);

    pthread_barrier_init(&barrier, NULL, num_threads + num_query_threads);


    pthread_t threads[conf.N + num_query_threads]; // consider #writers + propagator
    thread_arg_t targs[conf.N + num_query_threads]; 

    uint64_t chunk_size = n_inserts / conf.N;
    uint64_t remainder = n_inserts % conf.N;
    uint64_t current_start = startsize;
    uint64_t inserts_for_thread = chunk_size;

    uint32_t threads_per_sketch = conf.N / n_sketches;


    printf("Number of inserts %lu, inserts for threads %lu\n", n_inserts, inserts_for_thread);


    gettimeofday(&global_start, NULL);  // GLOBAL TIME


    /** launch query threads */
    long i;
    for (i=0; i < num_query_threads; i++){
        targs[i].tid = i;
        targs[i].sketch = sketch;
        targs[i].core_id = i % num_cores;
        int rc = pthread_create(&threads[i], NULL, thread_query, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread query %lu\n", i);
            exit(1);
        }
    }

    int current_sketch_id = 1, n_thread_per_sketch = 0;


    /** launch writer threads */
    for (; i < num_query_threads+conf.N-1; i++) {

    	if (i < num_query_threads+ remainder) { // takes into account that the first num_query_threads threads are for query
    		// First 'remainder' threads get the base chunk plus one
    	    inserts_for_thread = chunk_size + 1;
    		
    		//  start position is offset by the work done by previous threads.
    		// The previous 'i' threads each got (chunk_size + 1).
    	    current_start = startsize + ((i - num_query_threads) * (chunk_size + 1));
    	} else {
    		// Remaining threads get only the base chunk size
    	    inserts_for_thread = chunk_size;
    		
    		//  start position is calculated as:
    		// (Work of the 'remainder' threads) + (Work of the preceding 'chunk_size' threads)
    	    current_start = startsize + 
    			(remainder * (chunk_size + 1)) + 
    			(((i - num_query_threads) - remainder) * chunk_size);
    	}
        targs[i].tid = i;
        targs[i].n_inserts = inserts_for_thread;
        targs[i].startsize = current_start;
        targs[i].sketch = sketch;
        targs[i].algorithm = algorithm;
        targs[i].core_id = i % num_cores;  
        node = numa_node_of_cpu(targs[i].core_id); 
        targs[i].sketch_id = node + 1;  
        //targs[i].sketch_id = (i % n_sketches) + 1;  
        n_thread_per_sketch++;
        if(n_thread_per_sketch == threads_per_sketch) {n_thread_per_sketch = 0; current_sketch_id++;}



        current_start += inserts_for_thread;

        int rc = pthread_create(&threads[i], NULL, thread_insert, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread %lu\n", i);
            exit(1);
        }
    }
    
    if (i < remainder) {
        inserts_for_thread = chunk_size + 1;
    
        current_start = startsize + ((i-num_query_threads) * (chunk_size + 1));
    } else {
        inserts_for_thread = chunk_size;
        current_start = startsize + 
                        (remainder * (chunk_size + 1)) + // Total work of the first remainder threads
                        (((i - num_query_threads) - remainder) * chunk_size);  // total work of the preceding chunk_size threads
    }
    
        printf("Number of operations %lu, total operations %lu\n", inserts_for_thread, inserts_for_thread + current_start);
    targs[i].tid = i;
    targs[i].n_inserts = inserts_for_thread;
    targs[i].startsize = current_start;
    targs[i].sketch = sketch;
    targs[i].algorithm = algorithm;
    targs[i].core_id = i % num_cores;
    node = numa_node_of_cpu(targs[i].core_id); 
    targs[i].sketch_id = node + 1;  
    //targs[i].sketch_id = (i % n_sketches) + 1;  
    n_thread_per_sketch++;
    if(n_thread_per_sketch == threads_per_sketch) {n_thread_per_sketch = 0; current_sketch_id++;}



    // Get the start time
    gettimeofday(&writer_start, NULL);

    thread_insert(&targs[i]);
   

    long j;
    for (j = num_query_threads; j < num_query_threads+conf.N-1; j++) {
        pthread_join(threads[j], NULL);
        double t = targs[j].elapsed;
        insert_sum += t;
        if (t < insert_min) insert_min = t;
        if (t > insert_max) insert_max = t;
    }

    insert_sum += targs[j].elapsed;
    if (targs[j].elapsed < insert_min) insert_min = targs[j].elapsed;
    if (targs[j].elapsed > insert_max) insert_max = targs[j].elapsed;
    gettimeofday(&writer_end, NULL);
    // Get the end time

    gettimeofday(&global_end, NULL);


    printf("Writer threads finished. Elapsed wall-clock time: %.3f ms\n",
       elapsed_ms(writer_start, writer_end));
    printf("Writer thread times: avg %.3f ms, min %.3f ms, max %.3f ms\n",
       insert_sum / conf.N, insert_min, insert_max);


    printf("Total program elapsed time: %.3f ms\n",
           elapsed_ms(global_start, global_end));

    printf("Number of queries %lu\n", count_queries);
    
    pthread_barrier_destroy(&barrier);

    //minhash_print(sketch->sketches[1]->sketch, sketch->size);


    if (compare_with_serial)
        do_compare_with_serial(sketch, hash_functions, sketch->size, conf.init_size, n_inserts, remainder, conf.hash_type);
    return 0;
}
