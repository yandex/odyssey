#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct mm_msg mm_msg_t;

struct mm_msg {
	uint16_t refs;
	uint64_t machine_id;
	int type;
	mm_buf_t data;
	machine_list_t link;
};

static inline void mm_msg_init(mm_msg_t *msg, int type)
{
	msg->refs = 0;
	msg->type = type;
	msg->machine_id = 0;
	mm_buf_init(&msg->data);
	machine_list_init(&msg->link);
}
