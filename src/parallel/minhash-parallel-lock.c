
#include <minhash.h>
#include <configuration.h>


void insert_parallel(minhash_sketch *sketch, uint64_t elem) {

#ifdef LOCKS
    pthread_mutex_lock(&(sketch->lock));
#elif defined(RW_LOCKS)
    pthread_rwlock_wrlock(&(sketch->rw_lock));
#endif

        basic_insert(sketch->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, elem);

	

#ifdef LOCKS
    pthread_mutex_unlock(&(sketch->lock));
#elif defined(RW_LOCKS)
    pthread_rwlock_unlock(&(sketch->rw_lock));
#endif

}



float query_parallel(minhash_sketch *sketch, minhash_sketch *otherSketch) {

	uint64_t i;
	int count = 0;

#ifdef LOCKS
    pthread_mutex_lock(&(sketch->lock));
    pthread_mutex_lock(&(otherSketch->lock));
#elif defined(RW_LOCKS)
    pthread_rwlock_rdlock(&(sketch->rw_lock));
    pthread_rwlock_rdlock(&(otherSketch->rw_lock));
#endif

	for (i = 0; i < sketch->size; i++) {
		if (IS_EQUAL(sketch->sketch[i], otherSketch->sketch[i]))
			count++;
	}

#ifdef LOCKS
    pthread_mutex_unlock(&(otherSketch->lock));
    pthread_mutex_unlock(&(sketch->lock));
#elif defined(RW_LOCKS)
    pthread_rwlock_unlock(&(otherSketch->rw_lock));
    pthread_rwlock_unlock(&(sketch->rw_lock));
#endif

    fprintf(stderr, "[query] actual count %d\n", count);
	return count/(float)sketch->size;
}
