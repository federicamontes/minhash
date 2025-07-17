#include <sketch_list.h>


// methods implementation for managing list LIFO
void create_and_push_new_node(_Atomic(union tagged_pointer*) *head_sl, uint64_t *version_sketch, uint64_t size) {

	/*sketch_record *new_node = malloc(sizeof(sketch_record));
	OLD VERSION START
	if (new_node == NULL) {
		fprintf(stderr, "Error in creating new node for sketch list \n");
		exit(1);
	}

	_Atomic unsigned long *new_versioned = malloc(sizeof(unsigned long) * 2);
	if (new_versioned == NULL) {
		fprintf(stderr, "Error in allocating new versioned field of new record in sketch list \n");
		exit(1);
	}

	new_node->sketch_version = version_sketch;

	_Atomic unsigned long *p;
	sketch_record *record = ((sketch_record *) new_versioned[0]); 
	p = head_sl;
	
	
	__atomic_compare_exchange_n(    // head_sl = DWCAS(head_sl, p, new_versioned);
        	ptr,
        	expected_val_ptr,
        	desired_val,
        	false, // false for strong (not weak) compare-exchange
	        __ATOMIC_SEQ_CST, // success memory order
        	__ATOMIC_SEQ_CST  // failure memory order
        );
	
	
	record->versioned = *head_sl;
	OLD VERSION END
	*/
	
	
	
	
	// Create a new node as well as an associated tagged pointer
	// This operation will involve a 64-bit CAS on the head pointer itself.
	// Fail-retry loop on the head
	union tagged_pointer* current_head_tp_ptr = *head_sl; // Pointer to the current head's tagged_pointer
	union tagged_pointer* new_head_tp_ptr;     // Pointer to the new head's tagged_pointer
	
	sketch_record *sr = alloc_sketch_record(version_sketch);   // Pointer to the new list record
	// 0. Prepare the new head tagged_pointer.
	//    This new_head_tp_ptr will point to the sr record.
	//    Its tag will be 0 (no reader on it since it was not published yet)
	new_head_tp_ptr = alloc_aligned_tagged_pointer(sr, 0);
	do {
		// 1. Atomically load the current value of the head pointer (64-bit atomic read)
		// sr->next needs to point to the head (i.e., tagged_pointer) representing the *old* head's value.
		sr->next = __atomic_load_n(head_sl, __ATOMIC_ACQUIRE);  // sr now point to the head record. It works since only a single thread (this one) changes the head
		//fprintf(stderr, "create_and_push_new_node sr->next = %p   -- %d\n", sr->next, i++);

		// 2. Atomically update the `sketch_list_head_ptr` (64-bit CAS)
		//    We are trying to swap `current_head_tp_ptr` with `new_head_tp_ptr`.
		//    If this CAS fails, `current_head_tp_ptr` will be updated to the actual current value,
		//    and we retry the loop.
	} while (!atomic_compare_exchange_tagged_pointer_ptr(
		 head_sl,
		 &current_head_tp_ptr,
		 new_head_tp_ptr)); // This CAS operates on the pointer itself

	//fprintf(stderr, "create_and_push_new_node head_sl = %p\n", *head_sl);

	//// ***** Here should end the function ***** ////





/*
	// --- Push sr2 onto the list ---
	union tagged_pointer* current_head_tp_ptr_sr2;
	union tagged_pointer* new_head_tp_ptr_sr2;
	do {
	current_head_tp_ptr_sr2 = __atomic_load_n(&my_fcds_instance.sketch_list_head_ptr, __ATOMIC_ACQUIRE);

	// sr2->next points to the tagged_pointer that current_head_tp_ptr_sr2 points to
	sr2->next = alloc_aligned_tagged_pointer(current_head_tp_ptr_sr2->ptr, current_head_tp_ptr_sr2->tag);
	if (sr2->next == NULL) {
	    perror("Failed to allocate sr2->next");
	    free(sr2); return 1;
	}

	new_head_tp_ptr_sr2 = alloc_aligned_tagged_pointer(sr2, current_head_tp_ptr_sr2->tag + 1);
	if (new_head_tp_ptr_sr2 == NULL) {
	    perror("Failed to allocate new_head_tp_ptr_sr2");
	    free(sr2->next); free(sr2); return 1;
	}

	} while (!atomic_compare_exchange_tagged_pointer_ptr(
		 &my_fcds_instance.sketch_list_head_ptr,
		 &current_head_tp_ptr_sr2,
		 new_head_tp_ptr_sr2));

	printf("Pushed sr2 (data %d). New fcds_sketch.sketch_list_head_ptr points to a tagged_pointer: ptr=%p, tag=%llu\n",
	   sr2->data_payload, (void*)new_head_tp_ptr_sr2->ptr, new_head_tp_ptr_sr2->tag);


	// --- Traverse and print the list (conceptual, not atomic reads for simplicity) ---
	printf("\n--- Current List Content ---\n");
	union tagged_pointer* current_tp_ptr = __atomic_load_n(&my_fcds_instance.sketch_list_head_ptr, __ATOMIC_ACQUIRE);

	while (current_tp_ptr != NULL) {
		sketch_record* current_record = current_tp_ptr->ptr;
		if (current_record == NULL) { // Should not happen if list is well-formed
		    printf("Error: Tagged pointer points to NULL record.\n");
		    break;
		}
		printf("Record: %p, Data: %d, Sketch Version: %llu, Tag: %llu\n",
		       (void*)current_record, current_record->data_payload, *current_record->sketch_version, current_tp_ptr->tag);

		if (current_record->next != NULL) {
		     printf("  -> Next link details: ptr=%p, tag=%llu\n",
			    (void*)current_record->next->ptr, current_record->next->tag);
		     current_tp_ptr = current_record->next; // Move to the next node's tagged_pointer
		} else {
		    printf("  -> End of list.\n");
		    current_tp_ptr = NULL; // Signal end of loop
		}
	}
	
	
	
	

	// --- Cleanup (very simplified, real lock-free free is complex due to reclamation) ---
	printf("\n--- Cleaning up ---\n");
	union tagged_pointer* head_cleanup = __atomic_exchange_n(&my_fcds_instance.sketch_list_head_ptr, NULL, __ATOMIC_ACQ_REL); // Clear head
	union tagged_pointer* current_tp_to_free = head_cleanup;

	while (current_tp_to_free != NULL) {
		sketch_record* record_to_free = current_tp_to_free->ptr;
		union tagged_pointer* next_tp_to_free_later = NULL;

		if (record_to_free != NULL) {
		    next_tp_to_free_later = record_to_free->next; // Get the next tagged_pointer pointer
		    printf("Freeing record at %p (data %d)\n", (void*)record_to_free, record_to_free->data_payload);
		    free(record_to_free); // Free the sketch_record itself
		}

		printf("Freeing tagged_pointer at %p (points to %p, tag %llu)\n",
		       (void*)current_tp_to_free, (void*)current_tp_to_free->ptr, current_tp_to_free->tag);
		free(current_tp_to_free); // Free the current tagged_pointer union

		current_tp_to_free = next_tp_to_free_later; // Move to the next tagged_pointer to free
	}
*/
	
}


// Delete the node after prev. Prev will point to prev->next->next
// the method suppose that no reader is on prev->next and there won't be any. Hence, the node may be safely delete 
void delete_node(sketch_record *prev){

    _Atomic(union tagged_pointer*) tp_to_del_node = __atomic_load_n(&prev->next, __ATOMIC_ACQUIRE);   // tagged_pointer which points to the node to be deleted

    sketch_record* node_to_del = tp_to_del_node->ptr;

    //fprintf(stderr, "delete_node: prev->next = %p  -- prev->next->ptr = %p\t", prev->next, prev->next->ptr);
    prev->next = node_to_del->next;	// link prev to prev->next->next that is keep the list linked 
    //fprintf(stderr, "prev->next = %p\n", prev->next);
    // Now we can safely free the node and the tagged_pointer to that node
    free(tp_to_del_node);	       // free the tagged_pointer that points to node_to_del
    free(node_to_del->sketch);         // free the area of the sketch
    free(node_to_del);		       // free the node


}



// Helper to allocate a 16-byte aligned tagged_pointer on the heap
union tagged_pointer* alloc_aligned_tagged_pointer(struct sketch_record* ptr_val, uint64_t counter_val) {
    union tagged_pointer* new_tp;
    // posix_memalign is a standard way to get aligned memory on POSIX systems
    if (posix_memalign((void**)&new_tp, _Alignof(union tagged_pointer), sizeof(union tagged_pointer)) != 0) {
        perror("posix_memalign failed for tagged_pointer");
        exit(EXIT_FAILURE);
    }
    new_tp->ptr = ptr_val;
    new_tp->counter = counter_val;
    return new_tp;
}

// Helper to allocate a sketch_record
sketch_record* alloc_sketch_record(uint64_t *sketch) {
    sketch_record* rec = (sketch_record*)malloc(sizeof(sketch_record));
    if (rec == NULL) {
        perror("malloc failed for sketch_record");
        exit(EXIT_FAILURE);
    }
    rec->next = NULL;     // Initialize to NULL pointer (will point to tagged_pointer later)
    rec->sketch = sketch; // Initialize or allocate as needed
    return rec;
}





// Atomically compare and swap a tagged_pointer
// Arguments:
//   obj: Pointer to the volatile tagged_pointer union (e.g., &head or &node->next)
//   expected: Pointer to a tagged_pointer union holding the expected value.
//             This will be updated with the actual current value if CAS fails.
//   desired: The new tagged_pointer union value to set if the comparison succeeds.
// Returns true if the exchange was successful, false otherwise.
int atomic_compare_exchange_tagged_ptr(
    volatile union tagged_pointer* obj,
    union tagged_pointer* expected,
    union tagged_pointer desired)
{
    // We operate on the 'packed_value' member using __atomic_compare_exchange_n
    // This allows the compiler to generate cmpxchg16b (on x86-64) for the 128-bit union.
    return __atomic_compare_exchange_n(
        &obj->packed_value,          // Pointer to the 128-bit value to operate on
        &expected->packed_value,     // Pointer to the expected 128-bit value (updated on failure)
        desired.packed_value,        // The desired 128-bit value
        0,                           // 'false' for strong (not weak) compare-exchange
        __ATOMIC_SEQ_CST,            // Success memory order (sequentially consistent)
        __ATOMIC_SEQ_CST             // Failure memory order (sequentially consistent)
    );
}


// Function to perform 64-bit CAS on a POINTER to a tagged_pointer.
// This is used when you want to change *which* tagged_pointer instance a variable points to.
// This would be used for the `fcds_sketch.sketch_list_head_ptr` itself.
int atomic_compare_exchange_tagged_pointer_ptr(
    _Atomic(union tagged_pointer*)* obj_ptr_to_modify, // Pointer to the _Atomic pointer variable
    union tagged_pointer** expected_ptr,                // Pointer to the expected pointer value (updated on failure)
    union tagged_pointer* desired_ptr)                  // The desired pointer value
{
    // Use __atomic_compare_exchange_n for the _Atomic pointer variable
    return __atomic_compare_exchange_n(
        obj_ptr_to_modify,
        expected_ptr,
        desired_ptr,
        0,
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST
    );
}
