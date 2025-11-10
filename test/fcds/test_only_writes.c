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
    double elapsed;
    unsigned int core_id;
} thread_arg_t;


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


void local_insert(uint64_t *sketch, void *hash_functions, uint32_t hash_type, uint64_t size, _Atomic uint32_t *prop, long n_inserts, uint64_t startsize,  uint32_t b) {

    long i;
    uint32_t insertion_counter = 0;
    for (i = 0; i < n_inserts; i++) {
        insert_fcds(sketch, hash_functions, hash_type, size, &insertion_counter, prop, b, i+startsize);

    }
}

void *propagator_routine(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    fcds_sketch *t_sketch = targ->sketch;
    propagator(t_sketch);

    return NULL;
}


void *thread_insert(void *arg) {

    struct timeval t1, t2;
    thread_arg_t *targ = (thread_arg_t *)arg;

    fcds_sketch *t_sketch = targ->sketch;
    uint64_t *local_sketch = t_sketch->local_sketches[targ->tid];
    _Atomic uint32_t *propi = &(t_sketch->prop[targ->tid]);

    pin_thread_to_core(targ->core_id);

    pthread_barrier_wait(&barrier);

    gettimeofday(&t1, NULL);
    local_insert(local_sketch, t_sketch->hash_functions, t_sketch->hash_type, t_sketch->size,
        propi, targ->n_inserts, targ->startsize, t_sketch->b);
        
        
    uint32_t expected_prop = 0; // Only transition from 0 to 1
    while (! __atomic_compare_exchange_n(propi, &expected_prop, 1, 0, // Not weak CAS (strong CAS)
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            // DO NOTHING
            ;                                
    } // Since only two threads uses this prop, the busy-wait should be ok
       
    
    
    gettimeofday(&t2, NULL);
    targ->elapsed = elapsed_ms(t1, t2);
    //fprintf(stderr, "[thread_insert] %u has finished \n", gettid()%t_sketch->N);
    return NULL;
}




int main(int argc, const char*argv[]) {

    if (argc < 6) {
        fprintf(stderr,
                "Usage: %s <number of insertions> <sketch_size> <initial size> <num_threads> <threshold insertion> <hash coefficient> \n",
                argv[0]);
        return 1;
    }

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    long n_inserts = parse_arg(argv[1], "n_inserts", 1);
    long ssize = parse_arg(argv[2], "sketch_size", 1);
    long startsize = parse_arg(argv[3], "start_size", 0);
    long num_threads = parse_arg(argv[4], "num_threads", 1);
    long threshold = parse_arg(argv[5], "threshold", 1);

    if (argc > 6) {
        long k_cofficient = parse_arg(argv[6], "hash coefficient", 1);
        conf.k = k_cofficient;
    }


    // when finished debugging remove comment
    //srand(time(NULL)); 

    struct timeval global_start, global_end;
    struct timeval writer_start, writer_end;
    double insert_sum = 0.0, insert_min = 1e12, insert_max = 0.0;
    
    conf.sketch_size = (uint64_t) ssize;
    if (startsize > 0) conf.init_size = (uint64_t) startsize;

    conf.N = num_threads; 
    conf.b = threshold;

    print_params(n_inserts, conf.sketch_size, conf.init_size, conf.N, 0, conf.b);
    read_configuration(conf);


    fcds_sketch *sketch;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);
    init_fcds(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type, conf.N, conf.b);

    pthread_barrier_init(&barrier, NULL, num_threads-1);


    pthread_t threads[conf.N]; // consider #writers + propagator
    thread_arg_t targs[conf.N]; 

    uint64_t chunk_size = n_inserts / conf.N;
    uint64_t remainder = n_inserts % conf.N;
    uint64_t current_start = startsize;
    uint64_t inserts_for_thread = chunk_size;

    printf("Number of inserts %lu, inserts for threads %lu\n", n_inserts, inserts_for_thread);


    gettimeofday(&global_start, NULL);  // GLOBAL TIME



    /** launch writer threads */
    long i = 0;

    targs[i].tid = i;
    targs[i].sketch = sketch;
    int rc = pthread_create(&threads[i], NULL, propagator_routine, &targs[i]);
    if (rc) {
        fprintf(stderr, "Error creating propagator thread %lu\n", i);
        exit(1);
    }

    for (i = 1; i < conf.N-1; i++) {

        
        targs[i].tid = i;
        targs[i].n_inserts = inserts_for_thread;
        targs[i].startsize = current_start;
        targs[i].sketch = sketch;

        targs[i].core_id = i % num_cores;  

        current_start += inserts_for_thread;

        int rc = pthread_create(&threads[i], NULL, thread_insert, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread %lu\n", i);
            exit(1);
        }
    }
    
    
    targs[i].tid = i;
    targs[i].n_inserts = remainder;
    targs[i].startsize = current_start;
    targs[i].sketch = sketch;

    targs[i].core_id = i % num_cores;

    // Get the start time
    gettimeofday(&writer_start, NULL);

    thread_insert(&targs[i]);
   

    long j;
    for (j = 1; j < conf.N-1; j++) {
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
    
    pthread_barrier_destroy(&barrier);

    //minhash_print(sketch->sketches[1]->sketch, sketch->size);

    return 0;
}
