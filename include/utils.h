/**
* Set of base functions used in multiple cases
*/

#ifndef UTILS_H
#define UTILS_H
#include <hash.h>

// insert the element in the sketch: if elem's hash value is the actual minimum, the function return true, false otherwise
int basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem);
// Merge other_sketch and sketch. The resulting sketch is written into sketch itself
int merge(uint64_t *sketch, uint64_t *other_sketch, uint64_t size);
#endif
