#ifndef MM_MSG_H
#define MM_MSG_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_msg mm_msg_t;

struct mm_msg
{
	uint16_t  refs;
	int       type;
	mm_buf_t  data;
	mm_list_t link;
};

#endif /* MM_MSG_H */
