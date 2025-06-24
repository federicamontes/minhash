#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define SKETCH_SIZE 10 /// TODO: this has to be a parameter in the configuration directory

#include <stdint.h>

// Configuration struct for minhash parameters
struct minhash_configuration {
    uint64_t sketch_size;          /// Number of hash functions / sketch size
    uint64_t prime_modulus;        /// Large prime for hashing (M)
    uint64_t hash_type;   	   /// ID for hash function pointer
    int init_size;                 /// Initial elements to insert (optional)
    uint32_t k;                    /// Coefficient of k-wise hashing
#if FCDS
    uint32_t N;                    // number of writing threads
    uint32_t b;                    // threshold for propagation
#endif
};




void read_configuration(struct minhash_configuration conf);

#endif