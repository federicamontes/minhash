#include <configuration.h>
#include <minhash.h>

#include <math.h>
#include <inttypes.h>



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
                k_hash_functions[i].coefficients = malloc(k_hash_functions[i].k + 1 * sizeof(uint32_t));
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
    free(sketch->hash_functions);
    free(sketch->sketch);
}
