
#include <hash.h>


// Pairwise hash function implementation
uint64_t pairwise_func(pairwise_hash *self, uint64_t x) {
    assert(self->M > 0);
    return ((self->a * x % self->M) + self->b) % self->M;
}


/// Kwise hash function implementation
uint64_t kwise_func(kwise_hash *self, uint64_t x) {

    uint64_t pow_x = 1;
    uint64_t sum = 0;
    uint32_t i = 0;

    for (i = 0; i <= self->k; i++) {
        sum = (sum + (pow_x * self->coefficients[i] % self->M)) % self->M;
        pow_x = (pow_x * x) % self->M;
    }

    return sum % self->M;
}
