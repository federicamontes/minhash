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
    conc_minhash *sketch;
    long n_inserts;
    uint64_t startsize;
} thread_arg_t;


pthread_barrier_t barrier;


void minhash_print(uint64_t *sketch, size_t size) {

    uint64_t i;
    printf("Global sketch Value: \n");
    for(i=0; i < size; i++) {
        printf(" %lu, ", sketch[i]);
    }
    printf("\n");
}



void *thread_insert(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    conc_minhash *t_sketch = targ->sketch;
    pthread_barrier_wait(&barrier);

    long i;
    for (i=0; i < targ->n_inserts;i++) {
        insert_conc_minhash(t_sketch, i+targ->startsize);
    }
    
    return NULL;
}

void *thread_query(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    conc_minhash *t_sketch = targ->sketch;

    // Synchronize all threads before starting insertion
    pthread_barrier_wait(&barrier);

    fprintf(stderr, "Query thread %lu done\n", targ->tid);
    return NULL;
}


int main(int argc, const char*argv[]) {

    set_debug_enabled(false);

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
    if (*endptr != '\0') {
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


    // when finished debugging remove comment
    //srand(time(NULL)); 

    printf("=== Parameters ===\n");
    printf("Number of insertions     : %ld\n", n_inserts);
    printf("Sketch size              : %ld\n", ssize);
    printf("Initial size             : %ld\n", startsize);
    printf("Number of writer threads : %ld\n", num_threads);  // conf.N
    printf("Number of query threads  : %ld\n", num_query_threads);
    printf("Threshold (b)            : %ld\n", threshold);
    printf("Hash type                : %lu\n", conf.hash_type);
    printf("Prime modulus            : %lu\n", conf.prime_modulus);
    printf("Coefficient k-wise       : %d\n", conf.k);
    printf("====================\n");

    
    conf.sketch_size = (uint64_t) ssize;
    if (startsize > 0) conf.init_size = (uint64_t) startsize;

    conf.N = num_threads; /// for now all the threads but one are writers
    conf.b = threshold;

    read_configuration(conf);


    conc_minhash *sketch;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);

    init_conc_minhash(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type, conf.N, conf.b);

    pthread_barrier_init(&barrier, NULL, num_threads + num_query_threads + 1);


    pthread_t threads[conf.N + num_query_threads]; // consider #writers + propagator
    thread_arg_t targs[conf.N + num_query_threads]; 

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


    /*minhash_print(sketch->sketches[1]->sketch, sketch->size);


    // start comparison with serial

    minhash_sketch *serial_sketch;


    minhash_init(&serial_sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);



    for (i = 0; i < n_inserts + startsize - remainder; i++) {
        insert(serial_sketch, i);
    }
    int count = 0;
    for (i = 0; i < sketch->size; i++) {
        if(serial_sketch->sketch[i] == sketch->sketches[1]->sketch[i]) count++;
        else  printf("different %d - %d --- %d!\n", i, serial_sketch->sketch[i],  sketch->sketches[1]->sketch[i]);
    }

    if(count == sketch->size)    printf("Test passed eddaje!\n");
    else printf("NOOOOOOOOOOOOOOOOOOOOO!\n");*/


    //free_conc_minhash(sketch);
    return 0;
}
