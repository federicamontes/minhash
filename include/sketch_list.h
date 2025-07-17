/**
* Set of functions for memory reclamation list
*/

#ifndef SKETCH_LIST_H
#define SKETCH_LIST_H

#include <utils.h>


// Forward declaration of sketch_record for tagged_pointer
struct sketch_record;

// Union to pack the pointer and counter into a __int128_t
// This MUST be exactly 16 bytes (2 * 64-bit) for cmpxchg16b to work
// and should be 16-byte aligned.
//
// On x86-64, __int128_t typically interprets the first 64 bits as the "low" part
// and the second 64 bits as the "high" part. So, `ptr` would be the low part.
union tagged_pointer {
    struct {
        struct sketch_record* ptr;     // 64-bit pointer to the next record
        uint64_t counter;              // 64-bit counter/version number
    };
    __int128_t packed_value;           // The 128-bit representation for atomics
} __attribute__((aligned(16)));        // 16-byte alignment is crucial for cmpxchg16b



// List of old sketches
typedef struct sketch_record {
    // The pointer to the next record is `next.ptr`
    // The counter for the next record is `next.counter`
    union tagged_pointer *next;

    // min hash sketch:
    uint64_t *sketch;

} sketch_record;

// --- Static Assertions for correctness and alignment ---
//TODO: check the following
// Static assertion to ensure the size is 16 bytes
//_Static_assert(sizeof(union tagged_pointer) == 16, "tagged_pointer union must be 16 bytes long.");
// Static assertion to ensure alignment is 16 bytes
//_Static_assert(offsetof(struct sketch_record, next) % _Alignof(union tagged_pointer) == 0,
//               "sketch_record.next must be 16-byte aligned within sketch_record.");

// Ensure sketch_record.next is properly aligned within sketch_record.
// This implicitly checks if sketch_record itself is large enough and aligned correctly
// to contain a 16-byte aligned tagged_pointer.
//_Static_assert(offsetof(struct sketch_record, next) % _Alignof(union tagged_pointer) == 0,
//               "sketch_record.next must be 16-byte aligned within sketch_record.");






// Helper to allocate a 16-byte aligned tagged_pointer on the heap
union tagged_pointer* alloc_aligned_tagged_pointer(struct sketch_record* ptr_val, uint64_t counter_val);
// Helper to allocate a sketch_record
sketch_record* alloc_sketch_record(uint64_t *sketch);




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
    union tagged_pointer desired);
// Function to perform 64-bit CAS on a POINTER to a tagged_pointer.
// This is used when you want to change *which* tagged_pointer instance a variable points to.
// This would be used for the `fcds_sketch.sketch_list_head_ptr` itself.
int atomic_compare_exchange_tagged_pointer_ptr(
    _Atomic(union tagged_pointer*)* obj_ptr_to_modify, // Pointer to the _Atomic pointer variable
    union tagged_pointer** expected_ptr,                // Pointer to the expected pointer value (updated on failure)
    union tagged_pointer* desired_ptr);                  // The desired pointer value
    
    
    
    
    

// TODO methods for managing list
void create_and_push_new_node(_Atomic(union tagged_pointer*) *head_sl, uint64_t *version_sketch, uint64_t size);
void delete_node(sketch_record *prev);



#endif
