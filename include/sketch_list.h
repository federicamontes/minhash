/**
* Set of functions for memory reclamation list
*/

#ifndef SKETCH_LIST_H
#define SKETCH_LIST_H

#include <utils.h>

// TODO struct of list
typedef struct sketch_record {
	_Atomic unsigned long *versioned;   // versioned[0] is the address of next record in the list
										// versioned[1] is the counter of ongoing readers
	uint64_t *sketch_version;

} sketch_record;

// TODO methods for managing list
void create_and_push_new_node(_Atomic unsigned long** head_sl, uint64_t *version_sketch, uint64_t size);

	
#endif
