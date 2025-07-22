#include <linked_list.h>

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


// Helper to allocate a 16-byte aligned tagged_pointer on the heap
union tagged_pointer* alloc_aligned_tagged_pointer(uint64_t* ptr_val, uint64_t counter_val) {
    union tagged_pointer* new_tp;
    // posix_memalign is a standard way to get aligned memory on POSIX systems
    if (posix_memalign((void**)&new_tp, _Alignof(union tagged_pointer), sizeof(union tagged_pointer)) != 0) {
        perror("posix_memalign failed for tagged_pointer");
        exit(EXIT_FAILURE);
    }
    new_tp->sketch = ptr_val;
    new_tp->counter = counter_val;
    return new_tp;
}


void cache_query_sketch(void) {

    
}