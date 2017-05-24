#ifndef OD_LIST_H
#define OD_LIST_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_list od_list_t;

struct od_list
{
	od_list_t *next, *prev;
};

static inline void
od_listinit(od_list_t *list)
{
	list->next = list->prev = list;
}

static inline void
od_listappend(od_list_t *list, od_list_t *node)
{
	node->next = list;
	node->prev = list->prev;
	node->prev->next = node;
	node->next->prev = node;
}

static inline void
od_listunlink(od_list_t *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

static inline void
od_listpush(od_list_t *list, od_list_t *node)
{
	node->next = list->next;
	node->prev = list;
	node->prev->next = node;
	node->next->prev = node;
}

static inline od_list_t*
od_listpop(od_list_t *list)
{
	register od_list_t *pop = list->next;
	od_listunlink(pop);
	return pop;
}

static inline int
od_listempty(od_list_t *list)
{
	return list->next == list && list->prev == list;
}

#define od_listforeach(LIST, I) \
	for (I = (LIST)->next; I != LIST; I = (I)->next)

#define od_listforeach_safe(LIST, I, N) \
	for (I = (LIST)->next; I != LIST && (N = I->next); I = N)

#endif /* OD_LIST_H */
