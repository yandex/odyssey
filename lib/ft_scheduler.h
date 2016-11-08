#ifndef FT_SCHEDULER_H_
#define FT_SCHEDULER_H_

/*
 * libfluent.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftscheduler ftscheduler;

struct ftscheduler {
	ftlist    list_ready;
	ftlist    list_active;
	ftlist    list_free;
	int       count_ready;
	int       count_active;
	int       count_free;
	int       size_stack;
	ftfiber   main;
	ftfiber  *current;
	uint64_t  seq;
	void     *data;
};

static inline ftfiber*
ft_scheduler_current(ftscheduler *s) {
	return s->current;
}

static inline ftfiber*
ft_scheduler_next_ready(ftscheduler *s)
{
	if (s->count_ready == 0)
		return NULL;
	return ft_container_of(s->list_ready.next, ftfiber, link);
}

static inline int
ft_scheduler_online(ftscheduler *s) {
	return s->count_active + s->count_ready;
}

void ft_scheduler_init(ftscheduler*, int, void*);
void ft_scheduler_free(ftscheduler*);

ftfiber*
ft_scheduler_new(ftscheduler*, ftfiberf, void*);

void ft_scheduler_set(ftfiber*, ftfiberstate);
void ft_scheduler_call(ftfiber*);
void ft_scheduler_yield(ftscheduler*);

#endif
