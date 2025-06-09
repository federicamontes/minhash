#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define SKETCH_SIZE 10 /// TODO: this has to be a parameter in the configuration directory

#include <stdint.h>

struct global_configuration {

	uint64_t sketch_size;

};




void read_configuration(void);

#endif