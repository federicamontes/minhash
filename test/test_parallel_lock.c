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
    .k = 3,
};



typedef struct {
    pthread_t tid;
    minhash_sketch *sketch;
    long n_inserts;
    uint64_t startsize;
} thread_arg_t;

pthread_barrier_t barrier;

void minhash_print(minhash_sketch *sketch) {

    uint64_t i;
    printf("Value: \n");
    for(i=0; i < sketch->size; i++) {
        printf(" %lu, ", sketch->sketch[i]);
    }
    printf("\n");
}

void do_insert(minhash_sketch *sketch, long n_inserts, uint64_t startsize) {

    long i;
    for (i = 0; i < n_inserts; i++) {
        insert_parallel(sketch, i+startsize);
    }
}

void *thread_insert(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    // Synchronize all threads before starting insertion
    pthread_barrier_wait(&barrier);

    do_insert(targ->sketch, targ->n_inserts, targ->startsize);
    //minhash_print(targ->sketch);
    return NULL;
}


int main(int argc, const char*argv[]) {

    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <N: number of insertions> <sketch_size> <start_size> <num_threads>\n",
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
    
    conf.sketch_size = (uint64_t) ssize;
    if (startsize > 0) conf.init_size = (uint64_t) startsize;


    read_configuration(conf);


    minhash_sketch *sketch;
    minhash_sketch *sketch2;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);

    minhash_init(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);
    minhash_init(&sketch2, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);

    long i;
    for (i = 0; i < n_inserts; i++) {
        insert(sketch2, i+startsize);
    }

    /** THREAD DEFINITION */

    pthread_barrier_init(&barrier, NULL, num_threads);


    pthread_t threads[num_threads];
    thread_arg_t targs[num_threads];

    uint64_t chunk_size = n_inserts / num_threads;
    uint64_t remainder = n_inserts % num_threads;
    uint64_t current_start = startsize;
    uint64_t inserts_for_thread = chunk_size;


    for (i = 0; i < num_threads - 1; i++) {

        

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

    if (i == num_threads - 1) {
        inserts_for_thread += remainder;  // Last thread takes the leftover
    }
    pthread_barrier_wait(&barrier);

    struct timeval start, end;

    // Get the start time
    gettimeofday(&start, NULL);
    
    do_insert(sketch, n_inserts, startsize);

    long j;
    for (j = 0; j < num_threads - 1; j++) {
        pthread_join(threads[j], NULL);
    }

    // Get the end time
    gettimeofday(&end, NULL);

    // Compute elapsed time in milliseconds
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;      // seconds to ms
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;          // us to ms

    printf("Elapsed time: %.3f ms\n", elapsed);

    pthread_barrier_destroy(&barrier);


    //minhash_print(sketch);
    //minhash_print(sketch2);

    long k;
    for (k = 0; k < sketch->size; k++) {
        if (sketch->sketch[k] != sketch2->sketch[k]) {
            fprintf(stderr, "Sketches are not equivalent! Error at position %ld, values are %lu -- %lu \n", 
                i, sketch->sketch[k], sketch2->sketch[k]);
        } 
    }
    printf("Test passed!\n");


    minhash_free(sketch);
    return 0;
}
