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
    double prob;
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


void *propagator_routine(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    fcds_sketch *t_sketch = targ->sketch;
    propagator(t_sketch);

    return NULL;
}


void *thread_routine(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    struct timeval t1, t2;
    fcds_sketch *t_sketch = targ->sketch;
    double prob = targ->prob;
    uint64_t *local_sketch = t_sketch->local_sketches[targ->tid];
    _Atomic uint32_t *propi = &(t_sketch->prop[targ->tid]);
    uint32_t insertion_counter = 0;

    pin_thread_to_core(targ->core_id);

    pthread_barrier_wait(&barrier);

    gettimeofday(&t1, NULL);
    long i;
    unsigned int state = targ->tid;
    for (i=0; i < targ->n_inserts;i++) {
        //printf("[%lu] insertion number %ld\n", targ->tid, i);
        if (rand_r(&state) < prob*RAND_MAX) {
            insert_fcds(local_sketch, t_sketch->hash_functions, t_sketch->hash_type, 
                t_sketch->size, &insertion_counter, propi, t_sketch->b, i+targ->startsize);
        } else {
            query_fcds(t_sketch, t_sketch->global_sketch);
        }
    }
    
    gettimeofday(&t2, NULL);
    targ->elapsed = elapsed_ms(t1, t2);
    //fprintf(stderr, "[thread_insert] %u has finished \n", gettid()%t_sketch->N);
    return NULL;
}




int main(int argc, const char*argv[]) {


    if (argc < 7) {
        fprintf(stderr,
                "Usage: %s <number of operations> <sketch_size> <initial size> <num_threads> <threshold insertion>  <write probability> <hash coefficient>\n",
                argv[0]);
        return 1;
    }

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    long n_ops = parse_arg(argv[1], "n_ops", 1);
    long ssize = parse_arg(argv[2], "sketch_size", 1);
    long startsize = parse_arg(argv[3], "start_size", 0);
    long num_threads = parse_arg(argv[4], "num_threads", 1);
    long threshold = parse_arg(argv[5], "threshold", 1);
    double prob = parse_double(argv[6], "probability", 0);

    if (argc > 7) {
        long k_cofficient = parse_arg(argv[7], "hash coefficient", 1);
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

    print_params(n_ops, conf.sketch_size, conf.init_size, conf.N, 0, conf.b);
    read_configuration(conf);


    fcds_sketch *sketch;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);
    init_fcds(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type, conf.N, conf.b);

    pthread_barrier_init(&barrier, NULL, num_threads - 1);


    pthread_t threads[conf.N]; // consider #writers + propagator
    thread_arg_t targs[conf.N]; 

    uint64_t chunk_size = n_ops / conf.N;
    uint64_t remainder = n_ops % conf.N;
    uint64_t current_start = startsize;
    uint64_t inserts_for_thread = chunk_size;

    printf("Number of operations %lu, operations for threads %lu\n", n_ops, inserts_for_thread);


    gettimeofday(&global_start, NULL);  // GLOBAL TIME

    long i = 0;

    /** launch propagator */
    targs[i].tid = i;
    targs[i].sketch = sketch;
    int rc = pthread_create(&threads[i], NULL, propagator_routine, &targs[i]);
    if (rc) {
        fprintf(stderr, "Error creating propagator thread %lu\n", i);
        exit(1);
    }

    /** launch writer threads */
    for (i = 1; i < conf.N-1; i++) {

        
        targs[i].tid = i;
        targs[i].n_inserts = inserts_for_thread;
        targs[i].startsize = current_start;
        targs[i].sketch = sketch;
        targs[i].prob      = prob;
        targs[i].core_id = i % num_cores;  

        current_start += inserts_for_thread;

        int rc = pthread_create(&threads[i], NULL, thread_routine, &targs[i]);
        if (rc) {
            fprintf(stderr, "Error creating thread %lu\n", i);
            exit(1);
        }
    }
    
    
    targs[i].tid = i;
    targs[i].n_inserts = remainder;
    targs[i].startsize = current_start;
    targs[i].sketch = sketch;
    targs[i].prob      = prob;


    targs[i].core_id = i % num_cores;

    // Get the start time
    gettimeofday(&writer_start, NULL);

    thread_routine(&targs[i]);
   

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


    printf("Threads finished. Elapsed wall-clock time: %.3f ms\n",
       elapsed_ms(writer_start, writer_end));
    printf("Thread times: avg %.3f ms, min %.3f ms, max %.3f ms\n",
       insert_sum / conf.N, insert_min, insert_max);


    printf("Total program elapsed time: %.3f ms\n",
           elapsed_ms(global_start, global_end));
    
    pthread_barrier_destroy(&barrier);

    //minhash_print(sketch->sketches[1]->sketch, sketch->size);

    return 0;
}
