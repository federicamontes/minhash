
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

    //TODO rivedere
    (*sketch)->collect_sketch = malloc(sketch_size * sizeof(uint64_t));
    if ((*sketch)->collect_sketch == NULL) {
        fprintf(stderr, "Error in malloc() when allocating collect_sketch array\n");
        exit(1);
    }


    (*sketch)->prop = malloc(N * sizeof(_Atomic uint32_t));
    if ((*sketch)->prop == NULL) {
        fprintf(stderr, "Error in malloc() when allocating prop array\n");
        exit(1);
    }
    
    uint32_t i;
    for (i = 0; i < (*sketch)->N; i++) {
        __atomic_store_n(&(*sketch)->prop[i], 0, __ATOMIC_RELAXED); // TODO: check if atomic_relaxed is correct
    }
    
    (*sketch)->local_sketches = malloc(N * sizeof(uint64_t *));
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














void insert_fcds(uint64_t *local_sketch, void *hash_functions, uint32_t hash_type, uint64_t sketch_size, uint32_t *insertion_counter, _Atomic uint32_t *prop, uint32_t b, uint64_t elem) {
/**
* The function inserts a new element n the local sketch. If the threshold b is reached, the thread wait for the propagation
* The insertion is first done through basic_insert. Insert_counter is passed as pointer and takes track of the number of successfull insertions. prop is a pointer to an atomic variable
*/
    int insertion = basic_insert(local_sketch, sketch_size, hash_functions, hash_type, elem); // no need for synchronization here
    *insertion_counter += insertion;
    
    if(*insertion_counter == b){
        // start the propagation proceedure 
        *insertion_counter = 0;
        printf("Done %u inserts\n", b);
        // We've reached or exceeded the threshold.
        // Try to atomically set the prop_flag to 1 (Propagation Needed).
        // Use CAS to ensure only one "request" is made at a time.
        // If the flag is already 1 or 2, it means a propagation is already pending or in progress.
        uint32_t expected_prop = 0; // Only transition from 0 to 1
        while (! __atomic_compare_exchange_n(prop, &expected_prop, 1, 0, // Not weak CAS (strong CAS)
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            // DO NOTHING
            ;                                
        } // Since only two threads uses this prop, the busy-wait should be ok
        
        // Successfully set the flag to 1. Now we are waiting for the propagator.


        // Spin-wait until the propagator sets it back to 0.
        // The __ATOMIC_ACQUIRE ensures that once we see 0, all changes made  by the propagator are visible.
        while (__atomic_load_n(prop, __ATOMIC_ACQUIRE) != 0) {
                // Optional: yield CPU or sleep briefly to avoid busy-waiting.
                // sched_yield(); // or usleep(1);
        }     
        
    }
    

}



float query_fcds(fcds_sketch *sketch, fcds_sketch *otherSketch) {

    sketch = otherSketch;
    otherSketch = sketch;
    return 0.0;
}









void *propagator(fcds_sketch *sketch) {
    //fcds_sketch *sketch = (fcds_sketch *)arg;

    printf("Propagator enter!\n");

    while (1) { // TODO; Loop indefinitely or until a termination condition

        
        // Iterate through all worker threads to check their prop
        for (uint32_t i = 0; i < sketch->N; i++) {
            _Atomic uint32_t *current_prop = &(sketch->prop[i]);

            uint32_t expected_flag = 1; // We are looking for a flag that is set to 1 (Propagation Needed)
            uint32_t desired_flag = 2;  // We want to set it to 2 (Executing Propagation)

            // Atomically try to change the flag from 1 to 2.
            // This Compare-And-Swap (CAS) ensures:
            // 1. We only process if the flag is exactly 1.
            // 2. No other propagator (if multiple exist) can process this specific flag at the same time.
            // 3. The worker thread, if it checks, will see state 2 and know its request is being handled.
            if (__atomic_compare_exchange_n(current_prop, // Pointer to the atomic variable
                                            &expected_flag,    // Pointer to the expected value (1)
                                            desired_flag,      // The new value to set if comparison succeeds (2)
                                            0,                 // False for "weak" CAS (use strong CAS)
                                            __ATOMIC_ACQ_REL,  // Memory order for success: Acquire (for subsequent reads) + Release (for preceding writes)
                                            __ATOMIC_RELAXED)) { // Memory order for failure: Relaxed (no special ordering needed)
                    // If we successfully set the flag to 2, it means we "claimed" this propagation request.
                    printf("Propagator: Processing propagation request for thread %u\n", i);

                    // --- Perform the actual propagation logic here ---
                    // This would typically involve:
                    // 1. Merging sketch->local_sketches[i] into sketch->global_sketch
                    // 2. Notify thread T_i the propagation is ended by setting prop[i] to 0
                    // TODO Ensure any shared data accessed here is handled with appropriate synchronization (e.g., if global_sketch is lock-free or protected by its own means).

                    if (merge(sketch->global_sketch, sketch->local_sketches[i], sketch->size)) { // TODO: it does not take into account reader threads concurrent to this one
                        /// global sketch print
                        if(0) minhash_print(sketch);

                        //TODO create new node in list of sketch_list
                    }

                    // After propagation is complete, atomically set the flag back to 0.
                    // This signals to the worker thread that the propagation is finished and it can proceed.
                    // __ATOMIC_RELEASE ensures all memory effects of the propagation (e.g., updates to global_sketch)
                    // are visible to other threads that later acquire (read) this flag.
                    __atomic_store_n(current_prop, 0, __ATOMIC_RELEASE);
            }
            // If the CAS fails, it means:
            //   a) The flag was not 1 (e.g., it was 0, meaning no propagation needed), or 2, meaning another propagator is handling it (Impossible in our setting).
            //   b) In either case, we don't proceed with propagation for this 'i' in this iteration.

        }

        // TODO here propagator thread must check if 
        // memory reclamation is possible in sketch_list

    }
    return NULL;
}
