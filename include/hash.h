#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <assert.h>

typedef struct pairwise_hash { ///TODO change struct name to allow different hash functions by configuration
	uint32_t a; /// first coefficient
	uint32_t b; /// second coefficient
	uint64_t M; /// this is a large prime number
	uint64_t (*hash_function)(struct pairwise_hash *self, uint64_t x);
} pairwise_hash;


typedef struct kwise_hash {
	uint32_t k; /// polynomial degree
	uint64_t M; /// modulus
	uint32_t *coefficients; /// array of coefficients
	uint64_t (*hash_function) (struct kwise_hash *self, uint64_t x);
} kwise_hash;


extern pairwise_hash *p_hash_funcs;
extern kwise_hash *k_hash_funcs;
uint64_t pairwise_func(pairwise_hash *self, uint64_t x);
uint64_t kwise_func(kwise_hash *self, uint64_t x);

#endif