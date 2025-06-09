#include <stdio.h>
#include <minhash.h>

int main() {
    minhash_sketch *sketch;
    minhash_init(&sketch, 128, 0);

    insert(sketch, 42);
    insert(sketch, 99);

    float sim = query(sketch, sketch);
    printf("Self-similarity: %.2f\n", sim);

    minhash_free(sketch);
    return 0;
}