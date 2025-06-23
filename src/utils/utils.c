#include <utils.h>

int basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem) {

        int insertion = 0;  // boolean function that takes track if at least one element insertion in the min hash has actually occured
	uint64_t i;
	switch (hash_type) {
	case 1: {
		kwise_hash *kwise_h_func = (kwise_hash *) hash_functions;
		for (i = 0; i < size; i++) {
			uint64_t val = kwise_h_func[i].hash_function(&kwise_h_func[i], elem);
			if (val < sketch[i]){
				sketch[i] = val;
				insertion = 1;	
			}
		}
		break;
	    }
	default: {
		pairwise_hash *pairwise_h_func = (pairwise_hash *) hash_functions; /// pairwise_h_func is the pairwise struct
		for (i = 0; i < size; i++) {
			uint64_t val = pairwise_h_func[i].hash_function(&pairwise_h_func[i], elem);
			if (val < sketch[i]){
				sketch[i] = val;
				insertion = 1;	
			}
		}
		break;
	    }
	}
	
	return insertion;
}

void merge(uint64_t *sketch, uint64_t *other_sketch, uint64_t size) {
// Merge other_sketch and sketch. The resulting sketch is written into sketch itself
	uint64_t i;
	for (i = 0; i < size; i++)
	   if (sketch[i] > other_sketch[i]) sketch[i] = other_sketch[i];
}
