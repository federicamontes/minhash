/**
* Set of base functions used in multiple cases
*/

#ifndef UTILS_H
#define UTILS_H
#include <hash.h>

void basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem);

#endif
