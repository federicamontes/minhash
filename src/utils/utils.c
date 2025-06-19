#include <utils.h>

void basic_insert(uint64_t *sketch, uint64_t size, void *hash_functions, uint32_t hash_type, uint64_t elem) {

	uint64_t i;
	switch (hash_type) {
	case 1: {
		kwise_hash *kwise_h_func = (kwise_hash *) hash_functions;
		for (i = 0; i < size; i++) {
			uint64_t val = kwise_h_func[i].hash_function(&kwise_h_func[i], elem);
			if (val < sketch[i])
				sketch[i] = val;
		}
		break;
	    }
	default: {
		pairwise_hash *pairwise_h_func = (pairwise_hash *) hash_functions; /// pairwise_h_func is the pairwise struct
		for (i = 0; i < size; i++) {
			uint64_t val = pairwise_h_func[i].hash_function(&pairwise_h_func[i], elem);
			if (val < sketch[i])
				sketch[i] = val;
		}
		break;
	    }
	}

}
