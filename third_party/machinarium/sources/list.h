#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct machine_list machine_list_t;

struct machine_list {
	machine_list_t *next, *prev;
};

static inline void machine_list_init(machine_list_t *list)
{
	list->next = list->prev = list;
}

static inline void machine_list_append(machine_list_t *list,
				       machine_list_t *node)
{
	node->next = list;
	node->prev = list->prev;
	node->prev->next = node;
	node->next->prev = node;
}

static inline void machine_list_unlink(machine_list_t *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

static inline void machine_list_push(machine_list_t *list, machine_list_t *node)
{
	node->next = list->next;
	node->prev = list;
	node->prev->next = node;
	node->next->prev = node;
}

static inline machine_list_t *machine_list_pop(machine_list_t *list)
{
	register machine_list_t *pop = list->next;
	machine_list_unlink(pop);
	return pop;
}

static inline machine_list_t *machine_list_pop_back(machine_list_t *list)
{
	register machine_list_t *pop = list->prev;
	machine_list_unlink(pop);
	return pop;
}

static inline int machine_list_empty(machine_list_t *list)
{
	return list->next == list && list->prev == list;
}

#define machine_list_foreach(H, I) for (I = (H)->next; I != H; I = (I)->next)

#define machine_list_foreach_safe(H, I, N) \
	for (I = (H)->next; I != H && (N = I->next); I = N)

#define machine_list_peek(H, type) machine_container_of((H).next, type, link)
