/**
* Set of base functions used in multiple cases
*/

#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <hash.h>

#if defined(FCDS)
	#include <stdatomic.h>
	typedef __int128_t aligned_int128 __attribute__((aligned(16)));

#endif

// insert the element in the sketch: if elem's hash value is the actual minimum, the function return true, false otherwise
int basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem);
// Merge other_sketch and sketch. The resulting sketch is written into sketch itself

// Copy the sketch
uint64_t *copy_sketch(const uint64_t *sketch, uint64_t size);

int merge(uint64_t *sketch, uint64_t *other_sketch, uint64_t size);








#endif
