#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

#include <minhash.h>
#include <configuration.h>

struct minhash_configuration conf = {
    .sketch_size = 128,          /// Number of hash functions / sketch size
    .prime_modulus = (1ULL << 31) - 1,       /// Large prime for hashing (M)
    .hash_type = 1,        /// ID for hash function pointer
    .init_size = 0,                 /// Initial elements to insert (optional)
    .k = 2,
};


int main(int argc, const char*argv[]) {

    if (argc < 7) {
        fprintf(stderr, 
            "Usage: %s <set size> <sketch_size> <threshold b> <threads N> <hash coefficient> <Jaccard sim target> <number of repetitions>\n",
                argv[0]);
        exit(1);
    }

    long set_size = parse_arg(argv[1], "set_size", 1);
    long ssize = parse_arg(argv[2], "sketch_size", 1);
    long b = parse_arg(argv[3], "threshold b", 0);
    long N = parse_arg(argv[4], "threads N", 1);
    long k_coefficient = parse_arg(argv[5], "hash coefficient", 1);
    double J = parse_double(argv[6], "Jaccard similarity target", 0);
    long rep = parse_arg(argv[7], "repetitions", 1);
    
    conf.sketch_size = (uint64_t) ssize;
    conf.init_size = 0;
    conf.k = k_coefficient;

    read_configuration(conf);


    long shift = (set_size - J*set_size)/(1 + J), start_pos;

    float j_serial, j_par, err_serial = 0, err_par = 0;
    
    void *hash_functions;
    
    struct timeval start, end;
    srand(time(NULL));
    // Get the start time
    gettimeofday(&start, NULL);
    printf("n_test %lu\n", rep);
    printf("serial, parallel\n");
    volatile uint32_t i, j;    
    for (i = 0; i < rep; i++){
        minhash_sketch *serial_sketch_A, *serial_sketch_B, *par_sketch_A, *par_sketch_B;
        hash_functions = hash_functions_init(conf.hash_type, conf.sketch_size, conf.prime_modulus, conf.k);  

        start_pos = rand() % (RAND_MAX - (set_size + shift));  // avoid overflow
        minhash_init(&serial_sketch_A, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);
        minhash_init(&par_sketch_A, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);
           
        for (j = start_pos; j < start_pos + set_size; j++) {
            insert(serial_sketch_A, j);
        }
        for (j = start_pos; j < start_pos + set_size - b*N; j++) {
            insert(par_sketch_A, j);
        }

        minhash_init(&serial_sketch_B, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);
        minhash_init(&par_sketch_B, hash_functions, conf.sketch_size, conf.init_size, conf.hash_type);
        
        for (j = start_pos+shift; j < start_pos + shift + set_size; j++) {
            insert(serial_sketch_B, j);
        }
        for (j = start_pos+shift; j < start_pos + shift + set_size - b*N; j++) {
            insert(par_sketch_B, j);
        }
        
        j_serial = query(serial_sketch_A, serial_sketch_B);
        j_par = query(par_sketch_A, par_sketch_B);
        
        err_serial = (j_serial - J)*(j_serial - J);
        err_par = (j_par - J)*(j_par - J);
        //print the square error for serial and for (simulated) parallel version, separated by a comma (,)
        printf("%.4f,%.4f\n", err_serial, err_par);
        
        minhash_free(serial_sketch_A);
        minhash_free(par_sketch_A);        
        minhash_free(serial_sketch_B);
        minhash_free(par_sketch_B);
    }

    // Get the end time
    gettimeofday(&end, NULL);

    // Compute elapsed time in milliseconds
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;      // seconds to ms
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;          // us to ms

    printf("Elapsed time: %.3f ms\n", elapsed);
    
    //printf("RMSE serial: %.4f vs par: %.4f \n", err_serial / (float)rep, err_par/rep);



    return 0;
}
