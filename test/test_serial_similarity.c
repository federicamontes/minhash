#include <stdio.h>
#include <assert.h>

#include <minhash.h>
#include <configuration.h>

struct minhash_configuration conf = {
    .sketch_size = 128,          /// Number of hash functions / sketch size
    .prime_modulus = (1ULL << 31) - 1,       /// Large prime for hashing (M)
    .hash_type = 0,        /// ID for hash function pointer
    .init_size = 0,                 /// Initial elements to insert (optional)
    .k = 3,
};


int main(int argc, const char*argv[]) {

    if (argc < 4) {
        fprintf(stderr, 
            "Parameter error! Make sure to pass <N: (int) number of insertions ,sketch_size: (int) size of the sketch, init_size: (int) starting size of set \n");
        exit(1);
    }

    char *endptr;
    long n_inserts = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || n_inserts <= 0){
        fprintf(stderr, "n_inserts must be greater than zero!\n");
        exit(1);
    }

    long ssize = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || ssize <= 0) {
        fprintf(stderr, "Size of sketch must be greater than zero!\n");
        exit(1);
    }

    long startsize = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0' || startsize <= 0) {
        fprintf(stderr, "Starting size of set must be greater than zero!\n");
        exit(1);
    }
    
    printf("n, size, init size %ld, %ld, %ld \n", n_inserts, ssize, startsize);
    conf.sketch_size = (uint64_t) ssize;
    if (startsize > 0) conf.init_size = (uint64_t) startsize;


    read_configuration(conf);



    minhash_sketch *sketch;
    minhash_sketch *sketch2;

    void *hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);

    minhash_init(&sketch, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);
    minhash_init(&sketch2, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);

    long i;
    for (i = 0; i < n_inserts; i++) {
        insert(sketch, i+startsize);
    }

    for (i = 0; i < n_inserts; i++) {
        insert(sketch2, i+startsize);
    }


    minhash_free(sketch);
    return 0;
}