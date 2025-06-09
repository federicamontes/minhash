#ifndef MINHASH_H
#define MINHASH_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#include <hash.h>


#define INFTY UINT64_MAX
#define IS_EQUAL(x, y) ((x) == (y))

typedef struct minhash_sketch {

	uint64_t size;
	uint64_t *sketch;
	pairwise_hash *hash_functions;
} minhash_sketch;

extern minhash_sketch *sketch;


/** INIT AND CLEAR OPERATIONS */
void minhash_init(minhash_sketch **mh, uint64_t sketch_size, int empty);
void hash_functions_init(pairwise_hash *hf, uint64_t size);
void init_empty_values(minhash_sketch *sketch);
void init_values(minhash_sketch **sketch, uint64_t size);
void minhash_free(minhash_sketch *mh);


/** SKETCH OPERATIONS */
void insert(minhash_sketch *sketch, uint64_t elem);
float query(minhash_sketch *sketch, minhash_sketch *otherSketch);

#endif