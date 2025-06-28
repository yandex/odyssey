#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct mm_signalrd mm_signalrd_t;
typedef struct mm_signalmgr mm_signalmgr_t;

struct mm_signalrd {
	mm_call_t call;
	int signal;
	machine_list_t link;
};

struct mm_signalmgr {
	mm_fd_t fd;
	machine_list_t readers;
	int readers_count;
};

int mm_signalmgr_init(mm_signalmgr_t *, mm_loop_t *);
void mm_signalmgr_free(mm_signalmgr_t *, mm_loop_t *);
int mm_signalmgr_set(mm_signalmgr_t *, sigset_t *, sigset_t *);
int mm_signalmgr_wait(mm_signalmgr_t *, uint32_t);
