#include <sketch_list.h>


// TODO methods implementation for managing list LIFO
void create_and_push_new_node(_Atomic(union tagged_pointer*) *head_sl, uint64_t *version_sketch, uint64_t size) {

	/*sketch_record *new_node = malloc(sizeof(sketch_record));
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
	
	
	record->versioned = *head_sl;*/
	
	// --- Push sr1 onto the list ---
	// This push operation will involve a 64-bit CAS on the head pointer itself.
	// Loop for retries (typical for lock-free operations)
/*	union tagged_pointer* current_head_tp_ptr; // Pointer to the current head's tagged_pointer
	union tagged_pointer* new_head_tp_ptr;     // Pointer to the new head's tagged_pointer

	do {
	// 1. Atomically load the current value of the head pointer (64-bit atomic read)
	current_head_tp_ptr = __atomic_load_n(&my_fcds_instance.sketch_list_head_ptr, __ATOMIC_ACQUIRE);

	// 2. Prepare the new node's 'next' pointer
	//    sr1->next needs to point to a dynamically allocated tagged_pointer
	//    representing the *old* head's value.
	//    If current_head_tp_ptr is NULL (list was empty), sr1->next will point to a tagged_pointer
	//    that represents NULL and tag 0.
	if (current_head_tp_ptr == NULL) {
	    sr1->next = alloc_aligned_tagged_pointer(NULL, 0);
	} else {
	    sr1->next = alloc_aligned_tagged_pointer(current_head_tp_ptr->ptr, current_head_tp_ptr->tag);
	}
	if (sr1->next == NULL) { // Handle allocation failure
	    perror("Failed to allocate sr1->next");
	    free(sr1); return 1;
	}

	// 3. Prepare the new head tagged_pointer.
	//    This new_head_tp_ptr will point to the sr1 record.
	//    Its tag will be incremented from the *previous* head's tag (if any).
	uint64_t next_tag = (current_head_tp_ptr != NULL) ? current_head_tp_ptr->tag + 1 : 1;
	new_head_tp_ptr = alloc_aligned_tagged_pointer(sr1, next_tag);
	if (new_head_tp_ptr == NULL) { // Handle allocation failure
	    perror("Failed to allocate new_head_tp_ptr for sr1");
	    free(sr1->next); free(sr1); return 1;
	}

	// 4. Atomically update the `sketch_list_head_ptr` (64-bit CAS)
	//    We are trying to swap `current_head_tp_ptr` with `new_head_tp_ptr`.
	//    If this CAS fails, `current_head_tp_ptr` will be updated to the actual current value,
	//    and we retry the loop.
	} while (!atomic_compare_exchange_tagged_pointer_ptr(
		 &my_fcds_instance.sketch_list_head_ptr,
		 &current_head_tp_ptr,
		 new_head_tp_ptr)); // This CAS operates on the pointer itself

	printf("Pushed sr1 (data %d). New fcds_sketch.sketch_list_head_ptr points to a tagged_pointer: ptr=%p, tag=%llu\n",
	   sr1->data_payload, (void*)new_head_tp_ptr->ptr, new_head_tp_ptr->tag);


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





// Helper to allocate a 16-byte aligned tagged_pointer on the heap
union tagged_pointer* alloc_aligned_tagged_pointer(struct sketch_record* ptr_val, uint64_t counter_val) {
    union tagged_pointer* new_tp;
    // posix_memalign is a standard way to get aligned memory on POSIX systems
    if (posix_memalign((void**)&new_tp, _Alignof(union tagged_pointer), sizeof(union tagged_pointer)) != 0) {
        perror("posix_memalign failed for tagged_pointer");
        return NULL;
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
        return NULL;
    }
    rec->next = NULL; // Initialize to NULL pointer (will point to tagged_pointer later)
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
