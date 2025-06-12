#include <configuration.h>
#include <minhash.h>
#include <math.h>

void hash_functions_init(pairwise_hash *hf, uint64_t size) {
    uint64_t i;
    for (i = 0; i < size; i++) {
        hf[i].a = random();
        hf[i].b = random();
        hf[i].M = UINT64_MAX - 1;
        hf[i].hash_function = pairwise_func;
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

void minhash_init(minhash_sketch **sketch, pairwise_hash *hash_functions, uint64_t sketch_size, int init_size) {
    
    *sketch = malloc(sizeof(minhash_sketch));
    if (*sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating minhash_sketch\n");
        exit(1);
    }

    (*sketch)->size = sketch_size;

    
    (*sketch)->sketch = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating sketch array\n");
        exit(1);
    }

    /*(*sketch)->hash_functions = malloc(sketch_size * sizeof(pairwise_hash));
    if ((*sketch)->hash_functions == NULL) {
        fprintf(stderr, "Error in malloc() when allocating hash functions\n");
        exit(1);
    }

    
    hash_functions_init((*sketch)->hash_functions, sketch_size);*/
	(*sketch)->hash_functions = hash_functions;


    init_empty_values(*sketch);


    
    if (init_size > 0)
        init_values(*sketch, init_size);


}


void minhash_free(minhash_sketch *sketch) {
    free(sketch->hash_functions);
    free(sketch->sketch);
}