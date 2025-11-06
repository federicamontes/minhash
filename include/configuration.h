#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define SKETCH_SIZE 10 /// TODO: this has to be a parameter in the configuration directory
#define PENDING_OFFSET 32 //this allows manipulating certain bits of the 46-bit counter in the tagger pointer
#define MASK 0xFFFFFFFFULL

#include <stdint.h>
#include <stdbool.h>


// Configuration struct for minhash parameters
struct minhash_configuration {
    uint64_t sketch_size;          /// Number of hash functions / sketch size
    uint64_t prime_modulus;        /// Large prime for hashing (M)
    uint64_t hash_type;   	   /// ID for hash function pointer
    int init_size;                 /// Initial elements to insert (optional)
    uint32_t k;                    /// Coefficient of k-wise hashing
#if defined(FCDS) || defined(CONC_MINHASH)
    uint32_t N;                    // number of writing threads
    uint32_t b;                    // threshold for propagation
#endif
};


void set_debug_enabled(bool enabled);

int pin_thread_to_core(unsigned int core_id);
long parse_arg(const char *arg, const char *name, long min);
void read_configuration(struct minhash_configuration conf);

#endif