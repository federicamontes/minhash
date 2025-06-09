
#include <hash.h>


// Actual hash function definition
uint64_t pairwise_func(pairwise_hash *self, uint64_t x) {
    return (self->a * x + self->b) % self->M;
}

