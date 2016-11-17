#ifndef FT_OP_H_
#define FT_OP_H_

/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftop ftop;

struct ftop {
	int   active;
	void (*cancel)(ftop*, void*);
	void *arg;
};

#endif
