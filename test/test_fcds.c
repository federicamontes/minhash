#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>

#include <minhash.h>
#include <configuration.h>

struct minhash_configuration conf = {
    .sketch_size = 128,          /// Number of hash functions / sketch size
    .prime_modulus = (1ULL << 31) - 1,       /// Large prime for hashing (M)
    .hash_type = 0,        /// ID for hash function pointer
    .init_size = 0,                 /// Initial elements to insert (optional)
    .k = 5,
    .N = 0,
    .b = 0,
};



typedef struct {
    pthread_t tid;
    fcds_sketch *sketch;
    long n_inserts;
    uint64_t startsize;
} thread_arg_t;


pthread_barrier_t barrier;

void minhash_print(fcds_sketch *sketch) {

    uint64_t i;
    printf("Global sketch Value: \n");
    for(i=0; i < sketch->size; i++) {
        printf(" %lu, ", sketch->global_sketch[i]);
    }
    printf("\n");
}

void local_insert(uint64_t *sketch, void *hash_functions, uint32_t hash_type, uint64_t size, _Atomic uint32_t *prop, long n_inserts, uint64_t startsize,  uint32_t b) {

    long i;
    uint32_t insertion_counter = 0;
    for (i = 0; i < n_inserts; i++) {
        insert_fcds(sketch, hash_functions, hash_type, size, &insertion_counter, prop, b, i+startsize);

    }
}

void *thread_insert(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    fcds_sketch *t_sketch = targ->sketch;
    uint64_t *local_sketch = t_sketch->local_sketches[targ->tid];
    _Atomic uint32_t *propi = &(t_sketch->prop[targ->tid]);

    // Synchronize all threads before starting insertion
    pthread_barrier_wait(&barrier);

    local_insert(local_sketch, t_sketch->hash_functions, t_sketch->hash_type, t_sketch->size,
        propi, targ->n_inserts, targ->startsize, t_sketch->b);
        
        
    uint32_t expected_prop = 0; // Only transition from 0 to 1
    while (! __atomic_compare_exchange_n(propi, &expected_prop, 1, 0, // Not weak CAS (strong CAS)
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            // DO NOTHING
            ;                                
    } // Since only two threads uses this prop, the busy-wait should be ok
       
    
if (0) {
    uint64_t i;
    printf("Local sketch of %lu : \n", targ->tid);
    for(i=0; i < t_sketch->size; i++) {
        printf(" %lu, ", local_sketch[i]);
    }
    printf("\n");
}
    return NULL;
}


void *thread_query(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    fcds_sketch *t_sketch = targ->sketch;

    // Synchronize all threads before starting insertion
    pthread_barrier_wait(&barrier);

    for (int i = 0; i < 1000000; i++)
	   query_fcds(t_sketch, t_sketch->global_sketch);
	
    fprintf(stderr, "Query thread %lu done\n", targ->tid);
    return NULL;
}

void *propagator_routine(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    fcds_sketch *t_sketch = targ->sketch;
    propagator(t_sketch);

    return NULL;
}


int main(int argc, const char*argv[]) {

    if (argc < 7) {
        fprintf(stderr,
                "Usage: %s <number of insertions> <sketch_size> <initial size> <num_threads> <threshold insertion> <num_query_threads>\n",
                argv[0]);
        return 1;
    }

    char *endptr;
    long n_inserts = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || n_inserts <= 0){
        fprintf(stderr, "n_inserts must be greater than zero!\n");
        exit(1);
    }

    long ssize = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || ssize <= 0) {
        fprintf(stderr, "Size of sketch must be greater than zero!\n");
        exit(1);
    }

    long startsize = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0' || startsize < 0) {
        fprintf(stderr, "Starting size of set must be greater (or equal to) than zero!\n");
        exit(1);
    }

    long num_threads = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0' || num_threads <= 1) {
        fprintf(stderr, "num_threads must be greater than one!\n");
        return 1;
    }

    long threshold = strtol(argv[5], &endptr, 10);
    if (*endptr != '\0' || threshold < 1) {
        fprintf(stderr, "threshold must be greater than zero!\n");
        return 1;
    }
    long num_query_threads = strtol(argv[6], &endptr, 10);
    if (*endptr != '\0' || num_query_threads < 0) {
        fprintf(stderr, "num_query_threads must be greater than or equal to zero!\n");
        return 1;
    }

    printf("=== Parameters ===\n");
    printf("Number of insertions     : %ld\n", n_inserts);
    printf("Sketch size              : %ld\n", ssize);
    printf("Initial size             : %ld\n", startsize);
    printf("Number of writer threads : %ld\n", num_threads - 1);  // conf.N
    printf("Number of query threads  : %ld\n", num_query_threads);
    printf("Threshold (b)            : %ld\n", threshold);
    printf("Hash type                : %lu\n", conf.hash_type);
    printf("Prime modulus            : %lu\n", conf.prime_modulus);
    printf("Coefficient k-wise       : %d\n", conf.k);
    printf("====================\n");

    
    conf.sketch_size = (uint64_t) ssize;
    if (startsize > 0) conf.init_size = (uint64_t) startsize;

    conf.N = num_threads-1; /// for now all the threads but one are writers
    conf.b = threshold;

    read_configuration(conf);


    fcds_sketch *sketch;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);

    init_fcds(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type, conf.N, conf.b);

    pthread_barrier_init(&barrier, NULL, num_threads + num_query_threads);


    pthread_t threads[conf.N+1 + num_query_threads]; // consider #writers + propagator
    thread_arg_t targs[conf.N+1 + num_query_threads]; 

    uint64_t chunk_size = n_inserts / conf.N;
    uint64_t remainder = n_inserts % conf.N;
    uint64_t current_start = startsize;
    uint64_t inserts_for_thread = chunk_size;

    long i;
    for (i = 0; i < conf.N; i++) {

        
        targs[i].tid = i;
        targs[i].n_inserts = inserts_for_thread;
        targs[i].startsize = current_start;
        targs[i].sketch = sketch;

        current_start += inserts_for_thread;

        int rc = pthread_create(&threads[i], NULL, thread_insert, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread %lu\n", i);
            exit(1);
        }
    }
    
    
    for (; i < conf.N + num_query_threads; i++){
        targs[i].tid = i;
        targs[i].sketch = sketch;
        int rc = pthread_create(&threads[i], NULL, thread_query, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread query %lu\n", i);
            exit(1);
        }
    }
    
    pthread_barrier_wait(&barrier);


    if (i >= conf.N) {
        targs[i].tid = i;
        targs[i].sketch = sketch;
        int rc = pthread_create(&threads[i], NULL, propagator_routine, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread %lu\n", i);
            exit(1);
        }
    }

    struct timeval start, end;

    // Get the start time
    gettimeofday(&start, NULL);


    long j;
    for (j = 0; j < conf.N; j++) {
        pthread_join(threads[j], NULL);
    }

    // Get the end time
    gettimeofday(&end, NULL);

    // Compute elapsed time in milliseconds
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;      // seconds to ms
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;          // us to ms

    printf("Elapsed time: %.3f ms\n", elapsed);
    gettimeofday(&start, NULL);
    // Join query threads
    long q;
    for (q = 0; q < num_query_threads; q++) {
        //pthread_cancel(threads[q]);
        pthread_join(threads[conf.N + q], NULL); // or conf.N + q
    }


    pthread_barrier_destroy(&barrier);
    // Get the end time
    gettimeofday(&end, NULL);

    // Compute elapsed time in milliseconds
    elapsed = (end.tv_sec - start.tv_sec) * 1000.0;      // seconds to ms
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;          // us to ms

    printf("Elapsed time: %.3f ms\n", elapsed);



    /*minhash_sketch *serial_sketch;


    minhash_init(&serial_sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);



    for (i = 0; i < n_inserts - remainder; i++) {
        insert(serial_sketch, i+startsize);
    }
    int count = 0;
    for (i = 0; i < sketch->size; i++) {
        if(serial_sketch->sketch[i] == sketch->global_sketch[i]) count++;
        else  printf("different %d - %d --- %d!\n", i, serial_sketch->sketch[i],  sketch->global_sketch[i]);
    }

    if(count == sketch->size)    printf("Test passed eddaje!\n");
    else printf("NOOOOOOOOOOOOOOOOOOOOO!\n");*/


    //free_fcds(sketch);
    return 0;
}
