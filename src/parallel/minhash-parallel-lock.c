
#include <minhash.h>
#include <configuration.h>


void insert_parallel(minhash_sketch *sketch, uint64_t elem) {

	uint64_t i;

#ifdef LOCKS
    pthread_mutex_lock(&(sketch->lock));
#elif defined(RW_LOCKS)
    pthread_rwlock_wrlock(&(sketch->rw_lock));
#endif

	switch (sketch->hash_type) {
	case 1:
		kwise_hash *kwise_h_func = (kwise_hash *) sketch->hash_functions;
		for (i = 0; i < sketch->size; i++) {
			uint64_t val = kwise_h_func[i].hash_function(&kwise_h_func[i], elem);
			if (val < sketch->sketch[i])
				sketch->sketch[i] = val;

		}

		break;
		
	default:
		pairwise_hash *pairwise_h_func = (pairwise_hash *) sketch->hash_functions; /// pairwise_h_func is the pairwise struct
		for (i = 0; i < sketch->size; i++) {
			uint64_t val = pairwise_h_func[i].hash_function(&pairwise_h_func[i], elem);
			//printf("val %lu  --- sketch %lu\n",  val , sketch->sketch[i]);
		  	
			if (val < sketch->sketch[i])
				sketch->sketch[i] = val;

		}
		break;
	}

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

	for (i=0; i < sketch->size; i++) {
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
