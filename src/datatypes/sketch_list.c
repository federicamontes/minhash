#include <sketch_list.h>


// TODO methods implementation for managing list LIFO
void create_and_push_new_node(_Atomic unsigned long** head_sl, uint64_t *version_sketch, uint64_t size) {

	sketch_record *new_node = malloc(sizeof(sketch_record));
	if (new_node == NULL) {
		fprintf(stderr, "Error in creating new node for sketch list \n");
		exit(1);
	}

	_Atomic unsigned long *new_versioned = malloc(sizeof(unsigned long) * 2);
	if (new_versioned == NULL) {
		fprintf(stderr, "Error in allocating new versioned field of new record in sketch list \n");
		exit(1);
	}

	new_node->sketch_version = version_sketch;

	_Atomic unsigned long *p;
	sketch_record *record = ((sketch_record *) new_versioned[0]); 
	p = head_sl;
	head_sl = DWCAS(head_sl, p, new_versioned);
	record->versioned = *head_sl;
	
}