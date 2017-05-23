#ifndef MM_MSG_H_
#define MM_MSG_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_msg_t mm_msg_t;

struct mm_msg_t {
	uint16_t  refs;
	int       type;
	mm_buf_t  data;
	mm_list_t link;
};

#endif
