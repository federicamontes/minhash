#include <stdio.h>
#include <minhash.h>

int main() {
    minhash_sketch *sketch;
    minhash_sketch *sketch2;



    pairwise_hash *hash_functions = malloc(128 * sizeof(pairwise_hash));
    if (hash_functions == NULL) {
        fprintf(stderr, "Error in malloc() when allocating hash functions\n");
        exit(1);
    }

    
    hash_functions_init(hash_functions, 128);


    minhash_init(&sketch, hash_functions, 128, 50);
    minhash_init(&sketch2, hash_functions, 128, 100);

    insert(sketch, 42);
    insert(sketch, 99);

    uint64_t i;
    for (i = 0; i < 128; i++) {
        fprintf(stderr, "sketch 1 values: %lu \t", sketch->sketch[i]);
    }
    fprintf(stderr, "\n");

    insert(sketch2, 42);
    insert(sketch2, 100);  // different element to differ the sketches

    for (i = 0; i < 128; i++) {
        fprintf(stderr, "sketch 2 values: %lu \t", sketch2->sketch[i]);
    }
    fprintf(stderr, "\n");

    float sim = query(sketch, sketch2);
    printf("Similarity: %.2f\n", sim);

    minhash_free(sketch);
    return 0;
}