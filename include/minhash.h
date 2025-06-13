#ifndef MINHASH_H
#define MINHASH_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#if defined(LOCKS) || defined(RW_LOCKS)
    #include <pthread.h>
#endif

#include <hash.h>


#define INFTY UINT64_MAX
#define IS_EQUAL(x, y) ((x) == (y))

typedef struct minhash_sketch {

	uint64_t size;
	uint64_t *sketch;
	uint32_t hash_type;
	void *hash_functions;
#ifdef LOCKS
	pthread_mutex_t lock;
#endif
#ifdef RW_LOCKS
/** rw_locks guarantee that while a write_lock is acquired, no readers or writers can access the locked sketch
 *  when one or more readers acquire the read_lock, no writer can acquire the write lock until ALL the reader
 *  have released the lock
*/
	pthread_rwlock_t rw_lock;
#endif
} minhash_sketch;

extern minhash_sketch *sketch;


/** INIT AND CLEAR OPERATIONS */
void minhash_init(minhash_sketch **mh, void *hash_functions, uint64_t sketch_size, int empty, uint32_t hash_type);
void* hash_functions_init(uint64_t hf_id, uint64_t size, uint32_t modulus, uint32_t k);
void init_empty_values(minhash_sketch *sketch);
void init_values(minhash_sketch *sketch, uint64_t size);
void minhash_free(minhash_sketch *mh);


/** SKETCH OPERATIONS */
void insert(minhash_sketch *sketch, uint64_t elem);
float query(minhash_sketch *sketch, minhash_sketch *otherSketch);


void insert_parallel(minhash_sketch *sketch, uint64_t elem);
float query_parallel(minhash_sketch *sketch, minhash_sketch *otherSketch);

#endif