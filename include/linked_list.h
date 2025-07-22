#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <utils.h>

union tagged_pointer {
    struct {
        uint64_t* sketch;               // 64-bit pointer to the sketch
        int64_t counter;              	// 64-bit counter
    };
    __int128_t packed_value;           // The 128-bit representation for atomics
} __attribute__((aligned(16)));        // 16-byte alignment is crucial for cmpxchg16b


typedef struct q_list {
    union tagged_pointer *query_sketch;
    struct q_list *next;

} q_list;

int atomic_compare_exchange_tagged_ptr( volatile union tagged_pointer* obj, union tagged_pointer* expected,
    union tagged_pointer desired);

union tagged_pointer* alloc_aligned_tagged_pointer(uint64_t* ptr_val, uint64_t counter_val);




#endif