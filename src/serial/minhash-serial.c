
#include <minhash.h>
#include <configuration.h>


void insert(minhash_sketch *sketch, uint64_t elem) {


        basic_insert(sketch->sketch, sketch->size, sketch->hash_functions, sketch->hash_type, elem);

	
}



float query(minhash_sketch *sketch, minhash_sketch *otherSketch) {

	uint64_t i;
	int count = 0;
	for (i=0; i < sketch->size; i++) {
        //fprintf(stderr, "[query]  %d ? %d\n", sketch->sketch[i], otherSketch->sketch[i]);
		if (IS_EQUAL(sketch->sketch[i], otherSketch->sketch[i]))
			count++;
	}
   // fprintf(stderr, "[query] actual count %d\n", count);
	return count/(float)sketch->size;
}
