#ifndef MINHASH_H
#define MINHASH_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#if defined(LOCKS) || defined(RW_LOCKS) || defined(FCDS)
    #include <pthread.h>
#endif
#if defined(FCDS)
	#include <sketch_list.h>
#endif
#include <hash.h>
#include <utils.h>



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



#ifdef FCDS 

/** FAST CONCURRENT DATA STRUCTURES */

/* unoptimized version with a single local sketch */
typedef struct fcds_sketch {
    uint32_t N;		   // number of writing threads
	uint32_t b;		   // threshold for propagation
	
	uint64_t size;
	uint64_t *global_sketch;   // accessed by query threads in read only fashion, T_N+1 only writer threads
	
	// hash functions
	uint32_t hash_type;
	void *hash_functions;
	
	uint64_t **local_sketches; // position i is a sketch accessed by T_i and T_N+1 only
	_Atomic uint32_t *prop;    // synchronize access to local_sketches: array of N atomic variables. TODO: check actual data type, it takes boolean values only

	// TODO change type, it must be a list node from sketch_list
	_Atomic unsigned long *sketch_list;  // use for double collect mechanism. TODO: check how it works since we have a single writers who writes multiple locations


} fcds_sketch;

// extern fcds_sketch *sketch;

void init_fcds(fcds_sketch **sketch, void *hash_functions, uint64_t sketch_size, int init_size, uint32_t hash_type, uint32_t N, uint32_t b);
void init_empty_sketch_fcds(fcds_sketch *sketch);
void init_values_fcds(fcds_sketch *sketch, uint64_t size);
void free_fcds(fcds_sketch *sketch);

void insert_fcds(uint64_t *local_sketch, void *hash_functions, uint32_t hash_type, uint64_t sketch_size, uint32_t *insertion_counter, _Atomic uint32_t *prop, uint32_t b, uint64_t elem);
void *propagator(fcds_sketch *arg);

uint64_t *get_global_sketch(fcds_sketch *sketch);
float query_fcds(fcds_sketch *sketch, fcds_sketch *otherSketch);

#endif




#endif
