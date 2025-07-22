#include <minhash.h>
#include <configuration.h>


void init_empty_sketch_conc_minhash(uint64_t *sketch, uint64_t size) {

    uint64_t i;
    for (i=0; i < size; i++){
	   sketch[i] = INFTY;
    }
}


void init_values_conc_minhash(conc_minhash *sketch, uint64_t size) {
/**
* Insert the elements from 0 to size into the sketch that is, generate a sketch for the set composed by the elements in [0, size]
*/

    uint64_t i;
    for (i = 0; i < size; i++) {
        basic_insert(sketch->sketches[0]->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, i);
    }

}

void init_conc_minhash(conc_minhash **sketch, void *hash_functions, uint64_t sketch_size, int init_size, uint32_t hash_type, uint32_t N, uint32_t b){
    
    *sketch = malloc(sizeof(conc_minhash));
    if (*sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating fcds_sketch\n");
        exit(1);
    }

    (*sketch)->N = N;
    (*sketch)->b = b;
    
    (*sketch)->size = sketch_size;
    
    (*sketch)->hash_type = hash_type;
    (*sketch)->hash_functions = hash_functions;

    
    
    uint64_t *s = malloc(sketch_size * sizeof(uint64_t));
    if (s == NULL) {
        fprintf(stderr, "Error in malloc() when allocating sketch\n");
        exit(1);
    }

	(*sketch)->sketches[0] = alloc_aligned_tagged_pointer(s, 0); 
	
	s = malloc(sketch_size * sizeof(uint64_t));
    if (s == NULL) {
        fprintf(stderr, "Error in malloc() when allocating second sketch\n");
        exit(1);
    }
	(*sketch)->sketches[1] = alloc_aligned_tagged_pointer(s, 0); 
   
    (*sketch)->insert_counter = 0;
    (*sketch)->reclaiming = 0;
    
    
    init_empty_sketch_conc_minhash((*sketch)->sketches[0]->sketch, (*sketch)->size);
    
    if (init_size > 0)
        init_values_conc_minhash(*sketch, init_size);

    uint64_t i;
    for (i = 0; i < (*sketch)->size; i++)
 		(*sketch)->sketches[1]->sketch[i] = (*sketch)->sketches[0]->sketch[i];
        
        
    (*sketch)->head = NULL;

}



void free_conc_minhash(conc_minhash *sketch){

    //TODO free the version list of sketches
    if (sketch->head != NULL) free(sketch->head);

    free(sketch->sketches[0]->sketch);
    free(sketch->sketches[1]->sketch);

    free(sketch);
}



union tagged_pointer *FetchAndInc128(_Atomic (union tagged_pointer *) *ins_sketch, uint64_t increment) {

	union tagged_pointer current_value;
    union tagged_pointer new_value;
    union tagged_pointer* ptr; // Pointer to the tagged_pointer at the head of the list


	do {
        // Atomically load the current value of the tagged_pointer at head_ptr.
        // We need to use __atomic_load_n on its packed_value because it's a 128-bit atomic object.
        ptr = __atomic_load_n(ins_sketch, __ATOMIC_SEQ_CST);
        // Note that head_ptr is never NULL
        current_value.packed_value = __atomic_load_n(&ptr->packed_value, __ATOMIC_SEQ_CST);  // current_head_value is just a copy of *heat_ptr



        // Prepare the desired new value: same pointer, incremented counter.
        new_value = current_value;
        new_value.counter += increment;

        // Attempt to atomically compare and swap.
        // If successful, current_head_value still holds the value *before* the swap.
        // If unsuccessful, current_head_value is updated with the *current* value, and the loop retries.
    } while (!atomic_compare_exchange_tagged_ptr(
                 //head_ptr,              // The target tagged_pointer to modify
	         	*ins_sketch,   
                 &current_value,   // Expected value (will be updated on failure)
                 new_value         // Desired new value
             ));                        // If the CAS fails it means either another query thread has changed the counter or the sketch list's head changes. 
	                                // In the latter case we have to take the new head. NOtice that if CAS fails no modification occurs to *head_ptr

	return ptr;
}



/* OPERATIONS IMPLEMENTATION */

void conc_merge(conc_minhash *sketch) {

	sketch = sketch;

}

void concurrent_basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem){

	uint64_t i, old;
	switch (hash_type) {
	case 1: {
		kwise_hash *kwise_h_func = (kwise_hash *) hash_functions;
		for (i = 0; i < size; i++) {
			uint64_t val = kwise_h_func[i].hash_function(&kwise_h_func[i], elem);
			do {
				old = sketch[i];
			} while (val < old && !__atomic_compare_exchange_n(&(sketch[i]), &old, val, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
		}
		break;
	    }
	default: {
		pairwise_hash *pairwise_h_func = (pairwise_hash *) hash_functions; /// pairwise_h_func is the pairwise struct
		for (i = 0; i < size; i++) {
			uint64_t val = pairwise_h_func[i].hash_function(&pairwise_h_func[i], elem);
			do {
				old = sketch[i];
			} while (val < old && !__atomic_compare_exchange_n(&(sketch[i]), &old, val, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
		}
		break;
	    }
	}
	

}


/** 
 * This follows Function Insert(x) from Algorithm 1 in paper Concurrent Minhash Sketch
 * */
void insert_conc_minhash(conc_minhash *sketch, uint64_t val) {

	uint64_t old_cntr = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_RELAXED);
	while (old_cntr > (sketch->b - 1)*sketch->N) {
		if (! __atomic_compare_exchange_n(&(sketch->insert_counter), &old_cntr, -(sketch->N), 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
			conc_merge(sketch);
		}

		old_cntr = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_RELAXED);
	}


	while (sketch->insert_counter < 0); //wait for merge to complete


	union tagged_pointer *insert_sketch = FetchAndInc128(&(sketch->sketches[1]), 1);

	__atomic_fetch_add(&(sketch->insert_counter), 1, __ATOMIC_RELEASE);

	concurrent_basic_insert(insert_sketch->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, val);

	__atomic_fetch_add(&(sketch->insert_counter), -1, __ATOMIC_RELEASE);

	insert_sketch = FetchAndInc128(&(sketch->sketches[1]), -1);

}