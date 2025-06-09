
#include <minhash.h>
#include <configuration.h>


void insert(minhash_sketch *minhashsketch, uint64_t elem) {

	uint64_t i;
	for (i = 0; i < minhashsketch->size; i++) {
		uint64_t val = minhashsketch->hash_functions[i].hash_function(&minhashsketch->hash_functions[i], elem);
		if (val < minhashsketch->sketch[i])
			minhashsketch->sketch[i] = val;
	}

}



float query(minhash_sketch *sketch, minhash_sketch *otherSketch) {

	uint64_t i;
	uint64_t count = 0;
	for (i=0; i < sketch->size; i++) {
		if (IS_EQUAL(sketch->sketch[i], otherSketch->sketch[i]))
			count++;
	}

	return count/sketch->size;
}