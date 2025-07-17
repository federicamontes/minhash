
#include <minhash.h>
#include <configuration.h>





void init_empty_sketch_fcds(fcds_sketch *sketch) {

    uint64_t i, j;
    for (i=0; i < sketch->size; i++){
	   sketch->global_sketch[i] = INFTY;
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

    // init double collect list for versioned sketch
    /*(*sketch)->sketch_list = malloc(sizeof(unsigned long) * 2);
    if ((*sketch)->sketch_list == NULL) {
        fprintf(stderr, "Error in allocating sketch list for double collect \n");
        exit(1);  
    }
    (*sketch)->sketch_list[0] = NULL;
    (*sketch)->sketch_list[1] = 0;*/
    
    

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
        
        
    // Initialize the head of the list to point to a copy of global_sketch
    uint64_t *copy = copy_sketch((*sketch)->global_sketch, sketch_size);
    sketch_record *sr = alloc_sketch_record(copy);   // Pointer to the new list record
    union tagged_pointer *tp = alloc_aligned_tagged_pointer(sr, 0);
    sr->next = alloc_aligned_tagged_pointer(NULL, 0);
    __atomic_store_n(&((*sketch)->sketch_list), tp, __ATOMIC_RELEASE);
    // fprintf(stderr, "init: head_sl = %p, head_sl->ptr = %p, sr->nexst = %p\n", (*sketch)->sketch_list, (*sketch)->sketch_list->ptr, sr->next);



}



void free_fcds(fcds_sketch *sketch){

    //TODO free the version list of sketches
    free(sketch->sketch_list);

    free(sketch->global_sketch);
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
        // printf("Done %u inserts\n", b);
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
        while (__atomic_load_n(prop, __ATOMIC_RELAXED) != 0) {
                // Optional: yield CPU or sleep briefly to avoid busy-waiting.
                // sched_yield(); // or usleep(1);
        }     
        
    }
    

}





uint64_t *get_global_sketch(fcds_sketch *sketch){
/** return a copy of global_sketch in sketch.
* The actual algorithm is an impementation of a double collect mechanism in which a copy of global_sketch is generated, then it is compared with global_sketch to check
* if a modification occured in the middle. If so, the last sketch in the shared list is returned.
* This algorithm should guarantee overall correctness since the returned sketch is a valid state in the query's time interval
*/

  uint64_t *copy = copy_sketch(sketch->global_sketch, sketch->size);
  
  // Check if global was changed while copy was generated
  uint64_t i;
  int valid = 1;  // a boolean variable, it is true if copy is a valid sketch (i.e., it can be returned)
  for (i = 0; i < sketch->size; i++){
      if (copy[i] != sketch->global_sketch[i]){
          valid = 0;  // global_sketch has been changed during the copy_sketch function. It is not safe to return it
          break;
      }
  }
  
  if (!valid){
     // Go to the shared list and take a copy of the first element (i.e., the one pointed by the head
     _Atomic(union tagged_pointer*) head = get_head(sketch);
      for (i = 0; i < sketch->size; i++){ // copy the sketch in the list
          copy[i] = head->ptr->sketch[i];
      }
      decrement_counter(head);  // no more work on the record pointed by head, it can be released
  }
  
  
  return copy;


}

float query_fcds(fcds_sketch *sketch, uint64_t *otherSketch) { // TODO: change the signature: do we need to compare two fcds sketch? Can the latter be just a simple sketch (array)?

    uint64_t *actual_sketch = get_global_sketch(sketch);   // here we have a a deep copy
    //uint64_t *second = get_global_sketch(otherSketch);
    
    uint64_t i;
    int count = 0;
    for (i=0; i < sketch->size; i++) {
	if (IS_EQUAL(actual_sketch[i], otherSketch[i]))
	    count++;
    }
    //fprintf(stderr, "[query] actual count %d\n", count);
    //if(count != sketch->size)fprintf(stderr, "[query] actual count %d\n", count);
    
    free(actual_sketch); // they are just array, standard free suffices
    //free(second);
    return count/(float)sketch->size;

}









void *propagator(fcds_sketch *sketch) {
    //fcds_sketch *sketch = (fcds_sketch *)arg;

    // printf("Propagator enter!\n");

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
                    // printf("Propagator: Processing propagation request for thread %u\n", i);

                    // --- Perform the actual propagation logic here ---
                    // This would typically involve:
                    // 1. Merging sketch->local_sketches[i] into sketch->global_sketch
                    // 2. Notify thread T_i the propagation is ended by setting prop[i] to 0
                    // TODO Ensure any shared data accessed here is handled with appropriate synchronization (e.g., if global_sketch is lock-free or protected by its own means).

                    if (merge(sketch->global_sketch, sketch->local_sketches[i], sketch->size)) { // TODO: it does not take into account reader threads concurrent to this one
                        /// global sketch print
                        // if(0) minhash_print(sketch);

                        //TODO create new node in list of sketch_list
                        uint64_t *version_sketch = copy_sketch(sketch->global_sketch, sketch->size);
                        create_and_push_new_node(&sketch->sketch_list, version_sketch, sketch->size);
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
        garbage_collector_list(sketch);

    }
    return NULL;
}






_Atomic(union tagged_pointer*) get_head(fcds_sketch *sketch){


    union tagged_pointer current_head_value;
    union tagged_pointer new_head_value;
    union tagged_pointer* head_ptr; // Pointer to the tagged_pointer at the head of the list



//do {
    do {
        // Atomically load the current value of the tagged_pointer at head_ptr.
        // We need to use __atomic_load_n on its packed_value because it's a 128-bit atomic object.
        head_ptr = __atomic_load_n(&sketch->sketch_list, __ATOMIC_SEQ_CST);
        // Note that head_ptr is never NULL
        current_head_value.packed_value = __atomic_load_n(&head_ptr->packed_value, __ATOMIC_SEQ_CST);  // current_head_value is just a copy of *heat_ptr



        // Prepare the desired new value: same pointer, incremented counter.
        new_head_value = current_head_value;
        new_head_value.counter++;

        // Attempt to atomically compare and swap.
        // If successful, current_head_value still holds the value *before* the swap.
        // If unsuccessful, current_head_value is updated with the *current* value, and the loop retries.
    } while (!atomic_compare_exchange_tagged_ptr(
                 //head_ptr,              // The target tagged_pointer to modify
	         sketch->sketch_list,   
                 &current_head_value,   // Expected value (will be updated on failure)
                 new_head_value         // Desired new value
             ));                        // If the CAS fails it means either another query thread has changed the counter or the sketch list's head changes. 
	                                // In the latter case we have to take the new head. NOtice that if CAS fails no modification occurs to *head_ptr



//} while (head_ptr != sketch->sketch_list);



    // Return the pointer to the tagged_pointer just incremented
    return head_ptr;

}

void decrement_counter(_Atomic(union tagged_pointer*) tp){


    union tagged_pointer current_tp_value;
    union tagged_pointer new_tp_value;




    do {
        // Note that head_ptr is never NULL
        current_tp_value.packed_value = __atomic_load_n(&tp->packed_value, __ATOMIC_SEQ_CST);



        // Prepare the desired new value: same pointer, incremented counter.
        new_tp_value = current_tp_value;
        new_tp_value.counter--;

        // Attempt to atomically compare and swap.
        // If successful, current_head_value still holds the value *before* the swap.
        // If unsuccessful, current_head_value is updated with the *current* value, and the loop retries.
    } while (!atomic_compare_exchange_tagged_ptr(
                 tp,                  // The target tagged_pointer to modify
                 &current_tp_value,   // Expected value (will be updated on failure)
                 new_tp_value         // Desired new value
             ));


}



void garbage_collector_list(fcds_sketch *sketch){


    sketch_record *node, *prev;
    _Atomic(union tagged_pointer*) tp = __atomic_load_n(&sketch->sketch_list, __ATOMIC_ACQUIRE);   // tagged_pointer which points to the head
    
    // prev is initially the first record that always exists
    prev =  __atomic_load_n(&tp->ptr, __ATOMIC_RELAXED);
    // Skip the head of the list: do not delete that node, not matter what
    tp = __atomic_load_n(&prev->next, __ATOMIC_RELAXED);  // prev is always not NULL
    
    
    while (tp->ptr != NULL) {
    // if tp->ptr is NULL head is the last tagger_pointer of the list
        node = tp->ptr;
        //fprintf(stderr, "garbage_collector_list prev = %p -- prev->next = %p  -- tp->ptr = %p\n", prev, prev->next, tp->ptr);
        // check if we can safely delete node. We can do that if head->counter is 0. TODO: do we need atomic load of that field? Can we use a 64 version of it?
        if (__atomic_load_n(&tp->counter, __ATOMIC_RELAXED) == 0){  // No need of stronger memory order, if it is zero it won't be 1 and if it is >0 we skip it and clear it later
       

            //We can safely remove the record pointed by tp->ptr; tp is stored in prev
            delete_node(prev);
            
            node = prev->next->ptr;
        } else {
            // else serves because if we delete the node, prev does not change	
            prev = node;
        }
        tp = prev->next;    
    }
    

}
