#ifndef HASH_H
#define HASH_H

#include <stdint.h>

typedef struct pairwise_hash { ///TODO change struct name to allow different hash functions by configuration
	uint64_t a; /// first coefficient
	uint64_t b; /// second coefficient
	uint64_t M; /// this is a large prime number
	uint64_t (*hash_function)(struct pairwise_hash *self, uint64_t x);
} pairwise_hash;


extern pairwise_hash *hash_funcs;
uint64_t pairwise_func(pairwise_hash *self, uint64_t x);


#endif