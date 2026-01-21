#define _GNU_SOURCE

#include <configuration.h>
#include <minhash.h>

#include <math.h>
#include <inttypes.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/** --- Core pinning --- */

int pin_thread_to_core(unsigned long tid, unsigned int core_id) {

    cpu_set_t cpuset;
    pthread_t thread;
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (core_id < 0 || core_id >= num_cores) {
        fprintf(stderr, "Error: core_id %d is out of range (0-%d)\n",
                core_id, num_cores - 1);
        return -1;
    }

    // Get the calling thread
    thread = pthread_self();

    // Initialize and set CPU set
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    // Set affinity
    int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        perror("pthread_setaffinity_np");
        return ret;
    }

    // Optional: verify
    CPU_ZERO(&cpuset);
    ret = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        perror("pthread_getaffinity_np");
        return ret;
    }

    if (CPU_ISSET(core_id, &cpuset))
        printf("Thread %lu successfully pinned to core %d\n",tid, core_id);
    else
        fprintf(stderr, "Failed to pin thread to core %d\n", core_id);

    return 0;

}



/** --- Argument parsing --- */
long parse_arg(const char *arg, const char *name, long min) {
    char *endptr;
    long val = strtol(arg, &endptr, 10);
    if (*endptr != '\0' || val < min) {
        fprintf(stderr, "%s must be >= %ld\n", name, min);
        exit(EXIT_FAILURE);
    }
    return val;
}

double parse_double(const char *arg, const char *name, long min) {
    char *endptr;
    double val = strtod(arg, &endptr);
    if (*endptr != '\0' || val < min) {
        fprintf(stderr, "%s must be >= %ld\n", name, min);
        exit(EXIT_FAILURE);
    }
    return val;
}

static double elapsed_ms(struct timeval a, struct timeval b) {
    double ms = (b.tv_sec - a.tv_sec) * 1000.0;
    ms += (b.tv_usec - a.tv_usec) / 1000.0;
    return ms;
}



void read_configuration(struct minhash_configuration global_config) {


    // Example: print config
    printf("Config: sketch_size=%" PRIu64 ", prime_modulus=%" PRIu64 "\n",
           global_config.sketch_size, global_config.prime_modulus);
}

void * hash_functions_init(uint64_t hf_id, uint64_t size, uint32_t prime_modulus, uint32_t k) {
    uint64_t i;
    switch (hf_id) {
        case 1:
            printf("Kwise hash\n");
            kwise_hash *k_hash_functions;
            k_hash_functions = malloc(size * sizeof(kwise_hash));
            if (k_hash_functions == NULL) {
                fprintf(stderr, "Error in malloc() when allocating kwise hash functions\n");
                exit(1);
            }
            for (i=0; i < size; i++) {
                k_hash_functions[i].M = prime_modulus;
                k_hash_functions[i].k = k;
                k_hash_functions[i].coefficients = malloc((k_hash_functions[i].k + 1) * sizeof(uint32_t));
                if (k_hash_functions[i].coefficients == NULL) {
                    fprintf(stderr, "Error in malloc() when allocating kwise hash functions coefficients\n");
                    exit(1);
                }
                uint32_t j;
                for (j = 0; j <= k; j++) {
                    k_hash_functions[i].coefficients[j] = random();
                }
                k_hash_functions[i].hash_function = kwise_func;
            }
            return k_hash_functions;
        default:
            printf("Pairwise hash\n");
            pairwise_hash *p_hash_functions;
            p_hash_functions = malloc(size * sizeof(pairwise_hash));
            if (p_hash_functions == NULL) {
                fprintf(stderr, "Error in malloc() when allocating pairwise hash functions\n");
                exit(1);
            }
            for (i = 0; i < size; i++) {
                p_hash_functions[i].a = random();
                p_hash_functions[i].b = random();
                p_hash_functions[i].M = prime_modulus; // 2^31 - 1 biggest prime number in 32 bits. Using more bits may cause overflow
                p_hash_functions[i].hash_function = pairwise_func;
            }
            return p_hash_functions;
            break;
    }
}


void init_empty_values(minhash_sketch *sketch) {

	uint64_t i;
	for (i=0; i < sketch->size; i++)
		sketch->sketch[i] = INFTY;
}

void init_values(minhash_sketch *sketch, uint64_t size) {

	uint64_t i;
	for (i=0; i < size; i++) {
		insert(sketch, i);
	}
}

void minhash_init(minhash_sketch **sketch, void *hash_functions, uint64_t sketch_size, int init_size, uint32_t hash_type) {
    
    *sketch = malloc(sizeof(minhash_sketch));
    if (*sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating minhash_sketch\n");
        exit(1);
    }

    (*sketch)->size = sketch_size;
    (*sketch)->hash_type = hash_type;

    
    (*sketch)->sketch = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating sketch array\n");
        exit(1);
    }


#ifdef LOCKS
    pthread_mutex_init(&((*sketch)->lock), NULL);
#endif

#ifdef RW_LOCKS
    pthread_rwlock_init(&((*sketch)->rw_lock), NULL);
#endif


	
    (*sketch)->hash_functions = hash_functions;

    init_empty_values(*sketch);


    
    if (init_size > 0)
        init_values(*sketch, init_size);


}


void minhash_free(minhash_sketch *sketch) {

#ifdef LOCKS
    pthread_mutex_destroy(&sketch->lock);
#endif

#ifdef RW_LOCKS
    pthread_rwlock_destroy(&sketch->rw_lock);
#endif
    // free(sketch->hash_functions); TODO: I removed it here since if we have two or more sketches it will cause double free
    free(sketch->sketch);
    free(sketch);
}
