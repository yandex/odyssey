#ifndef MM_H_
#define MM_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_t mm_t;

struct mm_t {
	mm_machinemgr_t machine_mgr;
	mm_msgpool_t    msg_pool;
};

extern mm_t machinarium;

#endif
