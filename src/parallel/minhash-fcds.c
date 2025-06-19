
#include <minhash.h>
#include <configuration.h>





void init_empty_sketch_fcds(fcds_sketch *sketch) {

    uint64_t i, j;
    for (i=0; i < sketch->size; i++){
	sketch->global_sketch[i] = INFTY;
	sketch->collect_sketch[i] = INFTY; // should not be needed since the values of the global are copied here
    }
	
    for(i = 0; i < sketch->N; i++){
        for (j = 0; j < sketch->size; j++){
	    sketch->local_sketches[i][j] = INFTY;
        }
    }
}

void init_values_fcds(fcds_sketch *sketch, uint64_t size) {
/**
* Insert the elements from 0 to size into the sketch that is, generate a sketch for the set composed by the elements in [0, size]
*/

    uint64_t i, j;
    for (i = 0; i < size; i++) {
        basic_insert(sketch->global_sketch, sketch->size, sketch->hash_functions, sketch->hash_type, i);
    }
    // It is enough to copy the global sketch into the local one for te initialization. Avoids further insert in the sketches
    for (i = 0; i < sketch->N; i++){
        for (j = 0; j < sketch->size; j++) // Be carefull, since we are copying, the cycle scans the whole sketch (that is, sketch->size elements)
            sketch->local_sketches[i][j] = sketch->global_sketch[j];
    }
}

//void minhash_init(minhash_sketch **sketch, void *hash_functions, uint64_t sketch_size, int init_size, uint32_t hash_type) {

void init_fcds(fcds_sketch **sketch, void *hash_functions, uint64_t sketch_size, int init_size, uint32_t hash_type, uint32_t N, uint32_t b){
    
    *sketch = malloc(sizeof(fcds_sketch));
    if (*sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating fcds_sketch\n");
        exit(1);
    }

    (*sketch)->N = N;
    (*sketch)->b = b;
    
    (*sketch)->size = sketch_size;
    
    (*sketch)->hash_type = hash_type;
    (*sketch)->hash_functions = hash_functions;

    
    (*sketch)->global_sketch = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->global_sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating global_sketch array\n");
        exit(1);
    }

    (*sketch)->collect_sketch = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->collect_sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating collect_sketch array\n");
        exit(1);
    }


    (*sketch)->prop = malloc(N * sizeof(uint32_t));
    if ((*sketch)->prop == NULL) {
        fprintf(stderr, "Error in malloc() when allocating prop array\n");
        exit(1);
    }
    
    uint32_t i;
    for(i = 0; i < N; i++)
        (*sketch)->prop[i] = 0;
    
    (*sketch)->local_sketches = malloc(N * sizeof(void *));
    if ((*sketch)->local_sketches == NULL) {
        fprintf(stderr, "Error in malloc() when allocating local_sketches array\n");
        exit(1);
    }    		
    
    for(i = 0; i < N; i++){
        (*sketch)->local_sketches[i] = malloc(sketch_size * sizeof(uint64_t));
        if ((*sketch)->local_sketches[i] == NULL) {
            fprintf(stderr, "Error in malloc() when allocating local_sketches[%d] array\n", i);
            exit(1);
        }
    }
    
    
    init_empty_sketch_fcds(*sketch);
    
    if (init_size > 0)
        init_values_fcds(*sketch, init_size);


}



void free_fcds(fcds_sketch *sketch){


    free(sketch->global_sketch);
    free(sketch->collect_sketch);
    free(sketch->prop);
    
    uint32_t i;
    for (i = 0; i < sketch->N; i++)
        free(sketch->local_sketches[i]);
    free(sketch->local_sketches);
    
    free(sketch);


}














void insert_fcds(minhash_sketch *sketch, uint64_t elem) {

    sketch = sketch;
    elem = elem;

}



float query_fcds(minhash_sketch *sketch, minhash_sketch *otherSketch) {

    sketch = otherSketch;
    otherSketch = sketch;
    return 0.0;
}
