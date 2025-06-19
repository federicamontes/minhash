
#include <minhash.h>
#include <configuration.h>


void insert(minhash_sketch *sketch, uint64_t elem) {

	uint64_t i;
	switch (sketch->hash_type) {
	case 1: {
		kwise_hash *kwise_h_func = (kwise_hash *) sketch->hash_functions;
		for (i = 0; i < sketch->size; i++) {
			uint64_t val = kwise_h_func[i].hash_function(&kwise_h_func[i], elem);
			if (val < sketch->sketch[i])
				sketch->sketch[i] = val;
		}
		break;
	    }
	default: {
		pairwise_hash *pairwise_h_func = (pairwise_hash *) sketch->hash_functions; /// pairwise_h_func is the pairwise struct
		for (i = 0; i < sketch->size; i++) {
			uint64_t val = pairwise_h_func[i].hash_function(&pairwise_h_func[i], elem);
			if (val < sketch->sketch[i])
				sketch->sketch[i] = val;
		}
		break;
	    }
	}

}



float query(minhash_sketch *sketch, minhash_sketch *otherSketch) {

	uint64_t i;
	int count = 0;
	for (i=0; i < sketch->size; i++) {
		if (IS_EQUAL(sketch->sketch[i], otherSketch->sketch[i]))
			count++;
	}
    fprintf(stderr, "[query] actual count %d\n", count);
	return count/(float)sketch->size;
}
