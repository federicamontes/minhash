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
   
   
   
    (*sketch)->sketches_ptr[0] = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->sketches_ptr[0] == NULL) {
        fprintf(stderr, "Error in malloc() when allocating sketch\n");
        exit(1);
    }
    
    (*sketch)->sketches_ptr[1] = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->sketches_ptr[1] == NULL) {
        fprintf(stderr, "Error in malloc() when allocating sketch\n");
        exit(1);
    }
    (*sketch)->insert_counter = 0;
    (*sketch)->pending_cnt = 0;
    (*sketch)->reclaiming = 0;
    
    init_empty_sketch_conc_minhash((*sketch)->sketches_ptr[1], (*sketch)->size);
    uint64_t i;
    for (i = 0; i < init_size; i++) {
        basic_insert((*sketch)->sketches_ptr[1], (*sketch)->size, (*sketch)->hash_functions, (*sketch)->hash_type, i);
    }
    for (i = 0; i < (*sketch)->size; i++)
        (*sketch)->sketches_ptr[0][i] = (*sketch)->sketches_ptr[1][i];
        

    
    
    init_empty_sketch_conc_minhash((*sketch)->sketches[0]->sketch, (*sketch)->size);
    
    if (init_size > 0)
        init_values_conc_minhash(*sketch, init_size);

    // uint64_t i;
    for (i = 0; i < (*sketch)->size; i++)
 		(*sketch)->sketches[1]->sketch[i] = (*sketch)->sketches[0]->sketch[i];
        
        
    (*sketch)->head = NULL;


    trace(STDOUT_FILENO ,"[init_conc_minhash] insert tagged pointer sketch = %p query tagged pointer sketch = %p\n \t\t insert sketch = %p query sketch = %p \n", 
    	(*sketch)->sketches[1], (*sketch)->sketches[0], (*sketch)->sketches[1]->sketch, (*sketch)->sketches[0]->sketch);

}



void free_conc_minhash(conc_minhash *sketch){

    //TODO free the version list of sketches
    if (sketch->head != NULL) free(sketch->head);

    free(sketch->sketches[0]->sketch);
    free(sketch->sketches[1]->sketch);

    free(sketch);
}


/**
 * Atomically fetch and increment the 128-bit tagged pointer
 *
 * This function implements an update of a 128-bit structure
 * that encodes both:
 *   - a pointer to the current sketch (`sketch`)
 *   - an signed 64-bit counter field (`counter`)
 *
 * The `counter` itself packs two subfields — e.g. the insertion count
 * and the pending reader count (tracked via PENDING_OFFSET).
 *
 * FetchAndInc128() atomically increments a portion of that counter while
 * keeping the pointer field consistent. The operation returns the pointer
 * to the previously loaded tagged pointer (before the increment), giving the caller
 * a stable snapshot to work with.
 *
 * @param ins_sketch Pointer to the atomic tagged pointer 
 * @param increment Signed increment value applied to the counter.
 *               
 * @return A pointer to the union tagged_pointer structure representing
 *         the version *before* the increment (a safe snapshot for the caller).
 */
union tagged_pointer *FetchAndInc128(_Atomic (union tagged_pointer *) *ins_sketch, int64_t increment) {

	union tagged_pointer current_value; // local copy of the tagged pointer
    union tagged_pointer new_value;		// updated tagged pointer
    union tagged_pointer* ptr; 			// Pointer to the tagged_pointer at the head of the list
    int c = 0;

	do {
        // Atomically load the current value of the tagged_pointer
        // We need to use __atomic_load_n on its packed_value because it's a 128-bit atomic object.
        ptr = __atomic_load_n(ins_sketch, __ATOMIC_SEQ_CST);

        // atomic unmarshaling of packed value inside tagged pointer
        current_value.packed_value = __atomic_load_n(&(ptr->packed_value), __ATOMIC_SEQ_CST);  // current_head_value is just a copy of *heat_ptr
        //trace("[FetchAndInc128] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld\n", c++, pthread_self(), ptr, ptr->sketch, ptr->counter);
        trace(STDOUT_FILENO,"[FetchAndInc128] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld \n \t\t current_value = %p\n", 
        	c++, pthread_self(), ptr, ptr->sketch, ptr->counter, current_value.sketch);


        // Prepare the desired new value: same pointer, updated counter.
        new_value = current_value;
        new_value.counter += increment;

        // Attempt to atomically compare and swap.
        // If successful, ins_sketch still holds the value *before* the swap.
        // If unsuccessful, ins_sketch is updated with the *current* value, and the loop retries.
    } while (!atomic_compare_exchange_tagged_ptr(
	         	*ins_sketch,   	   // tagged pointer to be updated
                 &current_value,   // Expected value (will be updated on failure)
                 new_value         // Desired new value
             ));                        
             // If the CAS fails it means either another query thread has changed the counter or the sketch list's head changes. 
	         // In the latter case we have to take the new head. NOtice that if CAS fails no modification occurs to *head_ptr

	return ptr;
}



/* OPERATIONS IMPLEMENTATION */


void sketch_values_update(conc_minhash *sketch) {


	union tagged_pointer *query_sketch = FetchAndInc128(&(sketch->sketches[0]), 1);
	union tagged_pointer *insert_sketch = FetchAndInc128(&(sketch->sketches[1]), 0);

	trace(STDOUT_FILENO,"[sketch_values_update] query = %p \t insert = %p \n", query_sketch, insert_sketch);

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


/**
 * This function performs the query on the MinHash sketch.
 *
 * This function computes the similarity between the current
 * query sketch and another given sketch.
 *
 * @param sketch Pointer to the concurrent MinHash structure.
 * @param otherSketch Pointer to another MinHash sketch to compare against.
 * @return float Similarity between the two sketches
 */
float concurrent_query(conc_minhash *sketch, uint64_t *otherSketch) {

	// acquire a consistent view of the query sketch
	// <sketch_ptr, pending_cnt, insert_cnt> where insert_cnt is a don't-care value
	// while pending_cnt represents the number of ongoing queries on that sketch, and it
	// must be used for garbage collection in the future
	union tagged_pointer *query_sketch = sketch->sketches[0];//= FetchAndInc128(&(sketch->sketches[0]), (1ULL<<PENDING_OFFSET));
	uint64_t i;
	int count = 0;

	// comparison of the sketches values
	for (i = 0; i < sketch->size; i++) {
		if (IS_EQUAL(query_sketch->sketch[i], otherSketch[i])) {
    	    count++;
		}
	}

	// decrement pending counter to allow future garbage collection
	//FetchAndInc128(&query_sketch, -((int64_t)1<<PENDING_OFFSET)); 

    return count/(float)sketch->size;
}


void concurrent_merge_0_old(conc_minhash *sketch) {

	trace(STDOUT_FILENO,"Thread %ld - MERGE START\n", pthread_self());
	// creation of new insert sketch
	union tagged_pointer *insert_sketch, *query_sketch;
	uint64_t *new_insert_sketch = malloc(sketch->size * sizeof(uint64_t));
	if (new_insert_sketch == NULL) {
		fprintf(stderr, "Error in malloc() for allocation of new insert sketch in merge \n");
		exit(1);
	}
	trace(STDOUT_FILENO,"[concurrent_merge] BEFORE alloc aligned insert sketch = %p sketch = %p \n",sketch->sketches[1], sketch->sketches[1]->sketch);
	
	init_empty_sketch_conc_minhash(new_insert_sketch, sketch->size);
	_Atomic (union tagged_pointer *)new_tp = alloc_aligned_tagged_pointer(new_insert_sketch, 1); 
	trace("[concurrent_merge] alloc aligned ptr new insert = %p sketch = %p \n",new_tp, new_tp->sketch);
	trace("[concurrent_merge] old insert = %p sketch = %p \n",sketch->sketches[1], sketch->sketches[1]->sketch);

	int c = 0;
	do { // fail retry to publish new insert sketch 
		insert_sketch = __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST);//FetchAndInc128(&(sketch->sketches[1]), 0);
		trace(STDOUT_FILENO,"[concurrent_merge] fail retry #%d orig sketch %p old insert = %p sketch = %p \n",c, __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST), insert_sketch, insert_sketch->sketch);
			
		c++;
	} while (!__atomic_compare_exchange_n(&(sketch->sketches[1]), &insert_sketch, new_tp, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	
    trace(STDOUT_FILENO,"[concurrent_merge] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld\n", 
    	c, pthread_self(), insert_sketch, insert_sketch->sketch, insert_sketch->counter);
	// wait until ongoing insertions have completed
	while (__atomic_load_n(&(insert_sketch->counter), __ATOMIC_ACQUIRE) > 0);
	
	// creation of query sketch → insert sketch must become the new query sketch
	do { // fail retry to publish new query sketch (which is pointed by insert_sketch)
		query_sketch =  __atomic_load_n(&(sketch->sketches[0]), __ATOMIC_SEQ_CST);//FetchAndInc128(&(sketch->sketches[0]), 0); // acquire query sketch
	} while (!__atomic_compare_exchange_n(&(sketch->sketches[0]), &query_sketch, insert_sketch, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	trace(STDOUT_FILENO,"[concurrent_merge] query = %p \t insert = %p\n\t\t  sketch q = %p sketch ins = %p \n", 
		sketch->sketches[0], sketch->sketches[1], sketch->sketches[0]->sketch, sketch->sketches[1]->sketch);
	
	sketch_values_update(sketch); 

	__atomic_store_n(&(sketch->insert_counter), 0, __ATOMIC_RELEASE);
	trace(STDOUT_FILENO,"MERGE DONE\n");
	//free(query_sketch->sketch);
	//free(query_sketch);
	cache_query_sketch(); // TODO

}


void concurrent_merge_0(conc_minhash *sketch) {

	trace(STDOUT_FILENO,"Thread %ld - MERGE START\n", pthread_self());
	// creation of new insert sketch
	//union tagged_pointer *insert_sketch, *query_sketch;
	uint64_t *insert_sketch, *query_sketch;
	uint64_t *new_insert_sketch = malloc(sketch->size * sizeof(uint64_t));
	if (new_insert_sketch == NULL) {
		fprintf(stderr, "Error in malloc() for allocation of new insert sketch in merge \n");
		exit(1);
	}
	trace(STDOUT_FILENO,"[concurrent_merge] BEFORE alloc aligned insert sketch = %p sketch = %p \n",sketch->sketches[1], sketch->sketches[1]->sketch);
	
	// Step 1: wait ongoing writers by checking pending counter
	while((uint32_t) ((sketch->pending_cnt != 0)) )
	    trace(STDOUT_FILENO,"[merge] pending_cnt (uint32_t)  = 0x%08X (%u)\n", 
    	    (uint32_t) ((sketch->sketches[1]->counter >> PENDING_OFFSET) & MASK), (uint32_t) ((sketch->sketches[1]->counter >> PENDING_OFFSET) & MASK));

	int i;
	for (i = 0; i < sketch->size; i++) 
		new_insert_sketch[i] = sketch->sketches_ptr[1][i];
	
	
	int c = 0;
	do { // fail retry to publish new insert sketch 
		insert_sketch = __atomic_load_n(&(sketch->sketches_ptr[1]), __ATOMIC_SEQ_CST);
		//trace(STDOUT_FILENO,"[concurrent_merge] fail retry #%d orig sketch %p old insert = %p sketch = %p \n",c, __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST), insert_sketch, insert_sketch->sketch);
			
		c++;
	} while (!__atomic_compare_exchange_n(&(sketch->sketches_ptr[1]), &insert_sketch, new_insert_sketch, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	
        //trace(STDOUT_FILENO,"[concurrent_merge] %d Thread %ld: tagged pointer = %p  sketch = %p counter = %ld\n", 
    	//    c, pthread_self(), insert_sketch, insert_sketch->sketch, insert_sketch->counter);
	
	
	// creation of query sketch → insert sketch must become the new query sketch
	do { // fail retry to publish new query sketch (which is pointed by insert_sketch)
		query_sketch =  __atomic_load_n(&(sketch->sketches_ptr[0]), __ATOMIC_SEQ_CST);
	} while (!__atomic_compare_exchange_n(&(sketch->sketches_ptr[0]), &query_sketch, insert_sketch, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	//trace(STDOUT_FILENO,"[concurrent_merge] query = %p \t insert = %p\n\t\t  sketch q = %p sketch ins = %p \n", 
	//	sketch->sketches[0], sketch->sketches[1], sketch->sketches[0]->sketch, sketch->sketches[1]->sketch);
	

	__atomic_store_n(&(sketch->insert_counter), 0, __ATOMIC_RELEASE);
	trace(STDOUT_FILENO,"MERGE DONE\n");
	//free(query_sketch->sketch);
	//free(query_sketch);
	//cache_query_sketch(); // TODO

}


/**
 * This function performs the merge of the MinHash sketch to allow fresh queries
 *
 * This function is executed by the first writer that wins the race for modifying
 * the insert_counter to -N
 * It alignes the state between the current insertion sketch 
 * and query sketch using atomic pointer swaps
 *
 * Merge Operations:
 *   1. Wait until all ongoing writers complete their pending insertions
 *   2. Publish the current insertion sketch as the new query sketch (for fresh queries)
 *   3. Create a new insertion sketch, initialized as a copy of the old one
 *   4. Atomically publish this new sketch for subsequent insertions
 * 	 5. TODO garbage collection logic
 *
 * The merge guarantees that readers always see a consistent snapshot, and writers
 * never modify the same sketch concurrently with readers.
 *
 * @param sketch Pointer to the concurrent MinHash structure.
 */
void concurrent_merge(conc_minhash *sketch) {

	trace(STDERR_FILENO, "Thread %ld - MERGE START\n", gettid()%sketch->N);

	union tagged_pointer *insert_sketch, *query_sketch;
	uint64_t i;

	// Step 1: wait ongoing writers by checking pending counter
	/*while((uint32_t) ((sketch->sketches[1]->counter >> PENDING_OFFSET) & MASK) != 0)
		trace(STDOUT_FILENO,"[merge] pending_cnt (uint32_t)  = 0x%08X (%u)\n", 
    		(uint32_t) ((sketch->sketches[1]->counter >> PENDING_OFFSET) & MASK), (uint32_t) ((sketch->sketches[1]->counter >> PENDING_OFFSET) & MASK));*/
    		


        while (1) {
            int32_t total_active = 0;
            for (int i = 0; i < STRIPE_width; i++) {
                total_active += atomic_load(&sketch->active_writers[i].count);
            }
        
            if (total_active == 0) break;
        
        // Optional: cpu_relax() or usleep() to prevent burning CPU while waiting
        }

	trace(STDERR_FILENO, "Ongoing writers have finished\n");
	
	//Step 2: publish the current insertion sketch as the new query sketch
	insert_sketch = __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST);
	do { // fail retry to publish new query sketch (which is pointed by insert_sketch)
		query_sketch =  __atomic_load_n(&(sketch->sketches[0]), __ATOMIC_SEQ_CST);
	} while (!__atomic_compare_exchange_n(&(sketch->sketches[0]), &query_sketch, insert_sketch, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	
	// TODO: enqueue the old query sketch for garbage collection (safe reclamation later)

	trace(STDERR_FILENO, "Query sketch is now fresh\n");

	//Step 3: create new insert sketch and initialize its content
	uint64_t *new_insert_sketch = malloc(sketch->size * sizeof(uint64_t));
	if (new_insert_sketch == NULL) {
		fprintf(stderr, "Error in malloc() for allocation of new insert sketch in merge \n");
		exit(1);
	}
	
	for (i=0; i < sketch->size; i++) 
		new_insert_sketch[i] = insert_sketch->sketch[i];

	// Step 4: Publish insertion sketch and reset all counters 
	// insertion counter being 0 allows new insertion thread to progress
	_Atomic (union tagged_pointer *)new_tp = alloc_aligned_tagged_pointer(new_insert_sketch, 0); 
	int c;
	do { // fail retry to publish new insert sketch 
		insert_sketch = __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST);
		trace(STDOUT_FILENO,"[concurrent_merge] fail retry #%d orig sketch %p old insert = %p sketch = %p \n",c, __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST), insert_sketch, insert_sketch->sketch);
		c++;
	} while (!__atomic_compare_exchange_n(&(sketch->sketches[1]), &insert_sketch, new_tp, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));


	// TODO: garbage collection 
}


/**
 * This function performs a thread-safe concurrent insertion into a MinHash sketch.
 *
 * This function updates each cell of the MinHash sketch with the hash value of the
 * provided element using either *pairwise-independent* or *k-wise-independent* hash
 * functions, depending on the hash type.
 *
 * Each cell is updated atomically via a compare-and-swap (CAS)
 * operation that ensures only smaller hash values replace existing ones, maintaining
 * the MinHash property.
 *
 * @param sketch          Pointer to the array of MinHash sketch
 * @param size            Number of sketch entries
 * @param hash_functions  Pointer to an array of initialized hash function 
 * @param hash_type       Type of hash functions (1 = k-wise, otherwise = pairwise).
 * @param elem            Element to be inserted into the sketch.
 */
void concurrent_basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem){

	uint64_t i, old;
	switch (hash_type) {
	case 1: {
		// k-wise hash function
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
		// pairwise hash function
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
 * This follows Function Insert(x) from old version of Algorithm 1 in paper Concurrent Minhash Sketch
 * insert_counter is separated from the pending counter of insertion sketch
 * */
void insert_conc_minhash_0_old(conc_minhash *sketch, uint64_t val) {


	
	int64_t old_cntr = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST);
	int64_t threshold = (sketch->b - 1) * sketch->N;
	trace(STDOUT_FILENO,"[insert_conc_minhash] Thread %ld: insert_counter = %ld\n", pthread_self()%sketch->N, old_cntr);
	while (old_cntr > threshold) {
		int64_t expected = old_cntr;
        int64_t desired = -((int64_t) sketch->N);
		if ( __atomic_compare_exchange_n(&(sketch->insert_counter), &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
			trace(STDOUT_FILENO,"Thread %ld: triggering merge (setting insert_counter = -%u)\n", pthread_self(), sketch->N);
			concurrent_merge_0(sketch);
		}
		old_cntr = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST);
	}


	while (__atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST) < 0); //wait for merge to complete
	trace(STDOUT_FILENO,"[insert_conc_minhash] Thread %ld: merge complete, counter reset\n", pthread_self());

	
	_Atomic(union tagged_pointer *) insert_sketch = FetchAndInc128(&(sketch->sketches[1]), 1);
	trace(STDOUT_FILENO,"[insert_conc_minhash] Thread %ld: insert sketch ptr %p \n", pthread_self(), insert_sketch);

	__atomic_fetch_add(&(sketch->insert_counter), 1, __ATOMIC_ACQ_REL);

	concurrent_basic_insert(insert_sketch->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, val);

	//insert_sketch = FetchAndInc128(&(sketch->sketches[1]), -1); 
	
	insert_sketch = FetchAndInc128(&insert_sketch, -1);
	trace(STDOUT_FILENO,"[insert_conc_minhash] Thread %ld: second check ptrs %p \n", pthread_self(), insert_sketch);

}

/** 
 * This follows an implementation with separated counter and no use of 128-bit atomic instruction, just classical 64bit atomic ones
 * insert_counter is separated from the pending counter of insertion sketch
 * */
void insert_conc_minhash_0(conc_minhash *sketch, uint64_t val, pthread_t tid) {

        _Atomic uint64_t *insert_sketch;
	//union tagged_pointer new_val, old_val;
	uint64_t *Icur, *Inew;
	uint32_t pending_cnt; // pending insertion of each thread
	int32_t insert_cnt; // completed insertions
	unsigned long res_cas = 0;
	
	//int tid = get_thread_id() % STRIPE_width;

        // First, register the thread as a ongoning one
        while(1){
           //__atomic_fetch_add(&(sketch->pending_cnt), 1, __ATOMIC_ACQ_REL);
           atomic_fetch_add_explicit(&sketch->active_writers[tid].count, 1, memory_order_acquire);
           //atomic_fetch_add_explicit(&sketch->lazy_update[tid].count, 1, memory_order_acquire);
           //if(sketch->lazy_update[tid].count >= sketch->b) insert_cnt = __atomic_fetch_add(&(sketch->insert_counter), sketch->lazy_update[tid].count, __ATOMIC_ACQ_REL);  // increase the insert_counter
            __atomic_fetch_add(&(sketch->insert_counter), 1, __ATOMIC_ACQ_REL);
           Icur =  __atomic_load_n(&(sketch->sketches_ptr[1]), __ATOMIC_SEQ_CST);
           
           // Check if we have reached the threshold, if so we can continue with the normal insertion
           if (insert_cnt >= 0 && insert_cnt <= (int32_t)((sketch->b-1)*sketch->N)) break; 
           
           // Unregister the current thread as an ongoing thread since it is going to either perform the merge operation or what for merge to finish
           //__atomic_fetch_add(&(sketch->pending_cnt), -1, __ATOMIC_ACQ_REL);
           atomic_fetch_add_explicit(&sketch->active_writers[tid].count, -1, memory_order_acquire);
           
           // if above threshold try to execute a merge
	   if (insert_cnt > (int32_t)((sketch->b-1)*sketch->N)) { 

		while(1) {

		    // Set the sketch->insert_counter to -N, if the cas succeeds the thread executes the merge operation
		    // try to modify the insertion counter to -N to trigger a merge
		    int64_t desired = -((int64_t) sketch->N);
	            res_cas = __atomic_compare_exchange_n(&(sketch->insert_counter), &insert_cnt, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	            

		    trace(STDOUT_FILENO,"res cas %d \n", res_cas);

		    // the thread won the race to do the merge
		    if (res_cas) break;
		    Inew =  __atomic_load_n(&(sketch->sketches_ptr[1]), __ATOMIC_SEQ_CST);

		    // otherwise, reload the latest version to know why the previous CAS failed
		    insert_cnt = __atomic_load_n(&(sketch->insert_counter), __ATOMIC_SEQ_CST);

			//trace(STDERR_FILENO, "[%u] INSERT COUNTER %d\n", gettid()%sketch->N, insert_cnt, (int32_t) insert_sketch->counter & MASK);

			// if merge already terminated OR merge is in progress then break and wait
		    if (Inew != Icur || insert_cnt < 0) { // TODO: check if only the condition on insert_cnt is required or we need one about the pointer
				//trace(STDERR_FILENO, "[%u] Inew %p \t Icur %p insert_cnt %d \n",
				//	gettid()%sketch->N, Inew, Icur, insert_cnt);
			break; 
		    }

                    
			

		} //end while true above threshold
	

	   } //end if above threshold
           // If CAS succeeded, perform the actual merge operation
	   if (res_cas) {
		concurrent_merge_0(sketch);
		// trace(STDERR_FILENO, "[%u] Merge finished Icur %p \t sketch->sketches[1]->sketch %p\n", gettid()%sketch->N, Icur, sketch->sketches[1]->sketch);
   	   }

	   //trace(STDERR_FILENO, " [%u] Icur %p \t sketch->sketches[1]->sketch %p\n", gettid()%sketch->N, Icur, sketch->sketches[1]->sketch);

	   // Wait until the merge completes and sketch ptr changes
	   while (Icur == sketch->sketches[1]->sketch);  // TODO: check if it can also be accomplished with insert_count < 0 instead
        
        
        }
	

	




	concurrent_basic_insert(Icur, sketch->size, sketch->hash_functions, sketch->hash_type, val);

	//insert_sketch = FetchAndInc128(&(sketch->sketches[1]), -1); 
	
        //__atomic_fetch_add(&(sketch->pending_cnt), -1, __ATOMIC_ACQ_REL);  // Unregister the thread, indicates the insertion has finished
        atomic_fetch_add_explicit(&sketch->active_writers[tid].count, -1, memory_order_acquire);
	trace(STDOUT_FILENO,"[insert_conc_minhash] Thread %ld: second check ptrs %p \n", pthread_self(), insert_sketch);

}



/**
 * This is the version of insert operation described inby Algorithm 1
 * Performs a concurrent insertion into a MinHash sketch.
 *
 * Each insertion operates on a shared sketch structure protected
 * by a 128-bit atomic word combining a pointer (to the current insertion sketch)
 * and two counters:
 *   - pending_cnt (upper 32 bits): tracks concurrent pending insertions
 *   - insert_cnt  (lower 32 bits): tracks number of completed insertions
 *
 * When the total number of insertions reaches a threshold, a merge
 * operation is triggered to align sketches for queries.
 *
 * @param sketch The concurrent MinHash data structure.
 * @param val The value to be inserted 
 */
void insert_conc_minhash(conc_minhash *sketch, uint64_t val) {

	_Atomic(union tagged_pointer *) insert_sketch; //128-bit ptr → <sketch_ptr, pending_cnt, insert_cnt>
	union tagged_pointer new_val, old_val;
	uint64_t *Icur, *Inew;
	uint32_t pending_cnt; // pending insertion of each thread
	int32_t insert_cnt; // completed insertions
	unsigned long res_cas = 0;
	trace(STDERR_FILENO,"[%u ]START Icur %p \t sketch->sketches[1]->sketch %p\n", gettid()%sketch->N,Icur, sketch->sketches[1]->sketch);

	while(1) {

		
		/**
         * Atomically fetch and increment the composite counter.
         *
         * We incrementing both the insertion and pending counters:
         *    +1 to insert_cnt
         *    +1 << PENDING_OFFSET to pending_cnt
         */
		insert_sketch = FetchAndInc128(&(sketch->sketches[1]), 1 + (1ULL<<PENDING_OFFSET));

		//unmarshaling of each field
		Icur = insert_sketch->sketch;
		insert_cnt = (int32_t) insert_sketch->counter & MASK;
		pending_cnt = (uint32_t) ((insert_sketch->counter >> PENDING_OFFSET) & MASK);

		trace(STDOUT_FILENO,"BEFORE INSERTION \n");
		trace(STDOUT_FILENO,"counter = 0x%016llX\n", (unsigned long long)insert_sketch->counter);
    	trace(STDOUT_FILENO,"pending_cnt (uint32_t)  = 0x%08X (%u)\n", pending_cnt, pending_cnt);
    	trace(STDOUT_FILENO,"insert_cnt (int32_t)  = 0x%08X (%d) \t threshold %d \n", (int32_t)insert_cnt, insert_cnt, (int32_t)((sketch->b-1)*sketch->N));

    	//threshold not reached, do the insertion
		if (insert_cnt >= 0 && insert_cnt <= (int32_t)((sketch->b-1)*sketch->N)) break; 

		// otherwise an insertions or a merge might happen, decrement pending counter
		FetchAndInc128(&insert_sketch, -((int64_t)1<<PENDING_OFFSET));
    	
    	// if above threshold check if a merge is needed
		if (insert_cnt > (int32_t)((sketch->b-1)*sketch->N)) { 

			while(1) {

				// create new sketch content for triggering a merge
				new_val.sketch = Icur;
				new_val.counter = (uint64_t)(pending_cnt & MASK) << PENDING_OFFSET | ((int64_t)(-(sketch->N)));
				
				old_val.sketch = Icur;
				old_val.counter = (uint64_t)(pending_cnt & MASK) << PENDING_OFFSET | insert_cnt;
				trace(STDOUT_FILENO,"new val  %u \t %d\n", (uint32_t) ((new_val.counter >> PENDING_OFFSET) & MASK), (int32_t)new_val.counter & MASK);
				trace(STDOUT_FILENO,"old val  %u \t %d\n", (uint32_t) ((old_val.counter >> PENDING_OFFSET) & MASK), (int32_t)old_val.counter & MASK);

				// try to modify the insertion counter to -N to trigger a merge
				res_cas = atomic_compare_exchange_tagged_ptr(sketch->sketches[1], &old_val, new_val);

				trace(STDOUT_FILENO,"res cas %d \n", res_cas);

				// the thread won the race to do the merge
				if (res_cas) break;

				// otherwise, reload the latest version to know why the previous CAS failed
				insert_sketch = __atomic_load_n(&(sketch->sketches[1]), __ATOMIC_SEQ_CST);
				Inew = insert_sketch->sketch;
				insert_cnt = (int32_t) insert_sketch->counter & MASK;
				trace(STDERR_FILENO, "[%u] INSERT COUNTER %d\n", gettid()%sketch->N, insert_cnt, (int32_t) insert_sketch->counter & MASK);

				// if merge already terminated OR merge is in progress then break and wait
				if (Inew != Icur || insert_cnt < 0) {
					trace(STDERR_FILENO, "[%u] Inew %p \t Icur %p insert_cnt %d \n",
						gettid()%sketch->N, Inew, Icur, insert_cnt);
					break; 
				}

				pending_cnt = (uint32_t) ((insert_sketch->counter >> PENDING_OFFSET) & MASK);

				

			} //end while true above threshold
		

		} //end if above threshold

		// If CAS succeeded, perform the actual merge operation
		if (res_cas) {
			concurrent_merge(sketch);
			trace(STDERR_FILENO, "[%u] Merge finished Icur %p \t sketch->sketches[1]->sketch %p\n", gettid()%sketch->N, Icur, sketch->sketches[1]->sketch);
		}

		trace(STDERR_FILENO, " [%u] Icur %p \t sketch->sketches[1]->sketch %p\n", gettid()%sketch->N, Icur, sketch->sketches[1]->sketch);

		// Wait until the merge completes and sketch ptr changes
		while (Icur == sketch->sketches[1]->sketch);

	} //end outer while true


    /**
     * Perform the actual MinHash insertion on the current sketch.
     * This updates local sketch state concurrently via CAS.
     */
	concurrent_basic_insert(insert_sketch->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, val);

	// insertion completed, decrement pending counter
	FetchAndInc128(&insert_sketch, -((int64_t)1<<PENDING_OFFSET)); // TODO check if this is correct

   	trace(STDERR_FILENO, "[%u] has finished insertion! \n", gettid()%sketch->N);
	trace(STDOUT_FILENO,"AFTER INSERTION \n");
	trace(STDOUT_FILENO,"counter = 0x%016llX\n", (unsigned long long)insert_sketch->counter);
    trace(STDOUT_FILENO,"pending_cnt (uint32_t)  = 0x%08X (%u)\n", pending_cnt, pending_cnt);
    trace(STDOUT_FILENO,"insert_cnt (int32_t)  = 0x%08X (%d)\n", (int32_t)insert_cnt, insert_cnt);
}
