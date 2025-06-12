#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define SKETCH_SIZE 10 /// TODO: this has to be a parameter in the configuration directory

#include <stdint.h>

// Configuration struct for minhash parameters
struct minhash_configuration {
    uint64_t sketch_size;          /// Number of hash functions / sketch size
    uint64_t prime_modulus;        /// Large prime for hashing (M)
    uint64_t hash_function;   	   /// ID for hash function pointer
    int init_size;                 /// Initial elements to insert (optional)
};



void read_configuration(struct minhash_configuration conf);

#endif