#include <minhash.h>
#include <configuration.h>
#include <stdarg.h>
#include <unistd.h>

static bool debug_enabled = false;

void set_debug_enabled(bool enabled) {
    debug_enabled = enabled;
}

static inline void trace(int fd,const char *fmt, ...) {

	if (!debug_enabled) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        write(fd, buf, len);
    }
}

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

    trace("[init_conc_minhash] insert tagged pointer sketch = %p query tagged pointer sketch = %p\n \t\t insert sketch = %p query sketch = %p \n", 
    	(*sketch)->sketches[1], (*sketch)->sketches[0], (*sketch)->sketches[1]->sketch, (*sketch)->sketches[0]->sketch);

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
    int c = 0;

	do {
        // Atomically load the current value of the tagged_pointer at head_ptr.
        // We need to use __atomic_load_n on its packed_value because it's a 128-bit atomic object.
        ptr = __atomic_load_n(ins_sketch, __ATOMIC_SEQ_CST);
        // Note that head_ptr is never NULL
        current_value.packed_value = __atomic_load_n(&(ptr->packed_value), __ATOMIC_SEQ_CST);  // current_head_value is just a copy of *heat_ptr
        //trace("[FetchAndInc128] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld\n", c++, pthread_self(), ptr, ptr->sketch, ptr->counter);
        trace("[FetchAndInc128] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld \n \t\t current_value = %p\n", 
        	c++, pthread_self(), ptr, ptr->sketch, ptr->counter, current_value.sketch);


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


void sketch_values_update(conc_minhash *sketch) {


	union tagged_pointer *query_sketch = FetchAndInc128(&(sketch->sketches[0]), 1);
	union tagged_pointer *insert_sketch = FetchAndInc128(&(sketch->sketches[1]), 0);

	trace("[sketch_values_update] query = %p \t insert = %p \n", query_sketch, insert_sketch);

	uint64_t i, dest;
	for (i = 0; i < sketch->size; i++) {
		uint64_t src = query_sketch->sketch[i];
		do {
			dest = insert_sketch->sketch[i];
		} while (src < dest && !__atomic_compare_exchange_n(&(insert_sketch->sketch[i]), &dest, src, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	}

	query_sketch = FetchAndInc128(&(sketch->sketches[0]), -1);
	insert_sketch = FetchAndInc128(&(sketch->sketches[1]), -1);


}


void concurrent_merge(conc_minhash *sketch) {

	trace("Thread %ld - MERGE START\n", pthread_self());
	// creation of new insert sketch
	union tagged_pointer *insert_sketch, *query_sketch;
	uint64_t *new_insert_sketch = malloc(sketch->size * sizeof(uint64_t));
	if (new_insert_sketch == NULL) {
		fprintf(stderr, "Error in malloc() for allocation of new insert sketch in merge \n");
		exit(1);
	}
	trace("[concurrent_merge] BEFORE alloc aligned insert sketch = %p sketch = %p \n",sketch->sketches[1], sketch->sketches[1]->sketch);
	
	init_empty_sketch_conc_minhash(new_insert_sketch, sketch->size);
	_Atomic (union tagged_pointer *)new_tp = alloc_aligned_tagged_pointer(new_insert_sketch, 1); 
	trace("[concurrent_merge] alloc aligned ptr new insert = %p sketch = %p \n",new_tp, new_tp->sketch);
	trace("[concurrent_merge] old insert = %p sketch = %p \n",sketch->sketches[1], sketch->sketches[1]->sketch);

	int c = 0;
	do { // fail retry to publish new insert sketch 
		insert_sketch = __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST);//FetchAndInc128(&(sketch->sketches[1]), 0);
		trace("[concurrent_merge] fail retry #%d orig sketch %p old insert = %p sketch = %p \n",c, __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST), insert_sketch, insert_sketch->sketch);
			
		c++;
	} while (!__atomic_compare_exchange_n(&(sketch->sketches[1]), &insert_sketch, new_tp, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	
    trace("[concurrent_merge] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld\n", 
    	c, pthread_self(), insert_sketch, insert_sketch->sketch, insert_sketch->counter);
	// wait until ongoing insertions have completed
	while (__atomic_load_n(&(insert_sketch->counter), __ATOMIC_ACQUIRE) > 0);
	
	// creation of query sketch → insert sketch must become the new query sketch
	do { // fail retry to publish new query sketch (which is pointed by insert_sketch)
		query_sketch =  __atomic_load_n(&(sketch->sketches[0]), __ATOMIC_SEQ_CST);//FetchAndInc128(&(sketch->sketches[0]), 0); // acquire query sketch
	} while (!__atomic_compare_exchange_n(&(sketch->sketches[0]), &query_sketch, insert_sketch, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	trace("[concurrent_merge] query = %p \t insert = %p\n\t\t  sketch q = %p sketch ins = %p \n", 
		sketch->sketches[0], sketch->sketches[1], sketch->sketches[0]->sketch, sketch->sketches[1]->sketch);
	
	sketch_values_update(sketch); 

	__atomic_store_n(&(sketch->insert_counter), 0, __ATOMIC_RELEASE);
	trace("MERGE DONE\n");
	free(query_sketch->sketch);
	free(query_sketch);
	cache_query_sketch(); // TODO

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

	
	int64_t old_cntr = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST);
	int64_t threshold = (sketch->b - 1) * sketch->N;
	trace("[insert_conc_minhash] Thread %ld: insert_counter = %ld\n", pthread_self(), old_cntr);
	while (old_cntr > threshold) {
		int64_t expected = old_cntr;
        int64_t desired = -((int64_t) sketch->N);
		if ( __atomic_compare_exchange_n(&(sketch->insert_counter), &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
			trace("Thread %ld: triggering merge (setting insert_counter = -%u)\n", pthread_self(), sketch->N);
			concurrent_merge(sketch);
		}
		old_cntr = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST);
	}


	while (__atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST) < 0); //wait for merge to complete
	trace("[insert_conc_minhash] Thread %ld: merge complete, counter reset\n", pthread_self());

	
	_Atomic(union tagged_pointer *) insert_sketch = FetchAndInc128(&(sketch->sketches[1]), 1);
	trace("[insert_conc_minhash] Thread %ld: insert sketch ptr %p \n", pthread_self(), insert_sketch);

	__atomic_fetch_add(&(sketch->insert_counter), 1, __ATOMIC_ACQ_REL);

	concurrent_basic_insert(insert_sketch->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, val);

	//insert_sketch = FetchAndInc128(&(sketch->sketches[1]), -1); 
	
	insert_sketch = FetchAndInc128(&insert_sketch, -1);
	trace("[insert_conc_minhash] Thread %ld: second check ptrs %p \n", pthread_self(), insert_sketch);

}
