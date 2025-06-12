
#include <minhash.h>
#include <configuration.h>


void insert(minhash_sketch *sketch, uint64_t elem) {

	uint64_t i;
	for (i = 0; i < sketch->size; i++) {
		uint64_t val = sketch->hash_functions[i].hash_function(&sketch->hash_functions[i], elem);
		if (val < sketch->sketch[i])
			sketch->sketch[i] = val;
	}

}



float query(minhash_sketch *sketch, minhash_sketch *otherSketch) {

	uint64_t i;
	float count = 0.f;
	for (i=0; i < sketch->size; i++) {
		if (IS_EQUAL(sketch->sketch[i], otherSketch->sketch[i]))
			count++;
	}

	return count/sketch->size;
}