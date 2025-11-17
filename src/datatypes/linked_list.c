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



int atomic_compare_exchange_tagged_ptr_machine(
    volatile union tagged_pointer* obj,
    union tagged_pointer* expected,
    union tagged_pointer desired)
{
    // The desired value is the new value to write (RCX:RBX)
    __int128_t desired_128 = desired.packed_value;

    // Use a temporary char (1 byte) for the success result
    char success_byte;

    // Pointer to the low 64-bits of the desired value (for RBX)
    uint64_t* desired_low  = (uint64_t*)&desired_128;
    // Pointer to the high 64-bits of the desired value (for RCX)
    uint64_t* desired_high = (uint64_t*)&desired_128 + 1;

    __asm__ __volatile__ (
        "lock cmpxchg16b %1\n"
        "setz %0\n" // Set %0 (success_byte) to 1 if ZF is set (compare succeeded)

        // Output Constraints:
        // %0: success_byte (result)
        // %1: Memory operand (clobbered)
        // %A: RDX:RAX (Expected value is input, Actual value is output if fail)
        // %b: RBX (Desired Low 64-bit, input only)
        // %c: RCX (Desired High 64-bit, input only)
        : "=q" (success_byte),
          "+m" (obj->packed_value),
          "+A" (expected->packed_value)
        
        // Input Constraints:
        // 'g' constraint allows passing via register or memory
        // We cast the pointers to access the 64-bit parts of the __int128_t
        : "b" (*desired_low),   // RBX (Input): The low 64 bits of desired_128
          "c" (*desired_high)   // RCX (Input): The high 64 bits of desired_128
        
        // Clobbers:
        // "cc": condition codes (modified by cmpxchg16b)
        // "memory": volatile access
        : "cc", "memory"
    );

    // Convert the 1-byte result (0 or 1) back to the 4-byte int return value.
    return (int)success_byte;
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
