#ifndef MM_MACHINE_H
#define MM_MACHINE_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_machine mm_machine_t;

struct mm_machine
{
	int                  online;
	uint64_t             id;
	char                *name;
	machine_coroutine_t  main;
	void                *main_arg;
	mm_thread_t          thread;
	mm_scheduler_t       scheduler;
	mm_signalmgr_t       signal_mgr;
	mm_eventmgr_t        event_mgr;
	mm_loop_t            loop;
	mm_list_t            link;
};

extern __thread mm_machine_t *mm_self;

static inline void
mm_errno_set(int value)
{
	mm_scheduler_current(&mm_self->scheduler)->errno_ = value;

	/* update system errno as well */
	errno = value;
}

static inline int
mm_errno_get(void)
{
	return mm_scheduler_current(&mm_self->scheduler)->errno_;
}

#endif /* MM_MACHINE_H */
