#ifndef OD_LIST_H_
#define OD_LIST_H_

/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

typedef struct odlist_t odlist_t;

struct odlist_t {
	odlist_t *next, *prev;
};

static inline void
od_listinit(odlist_t *list)
{
	list->next = list->prev = list;
}

static inline void
od_listappend(odlist_t *list, odlist_t *node)
{
	node->next = list;
	node->prev = list->prev;
	node->prev->next = node;
	node->next->prev = node;
}

static inline void
od_listunlink(odlist_t *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

static inline void
od_listpush(odlist_t *list, odlist_t *node)
{
	node->next = list->next;
	node->prev = list;
	node->prev->next = node;
	node->next->prev = node;
}

static inline odlist_t*
od_listpop(odlist_t *list)
{
	register odlist_t *pop = list->next;
	od_listunlink(pop);
	return pop;
}

static inline int
od_listempty(odlist_t *list)
{
	return list->next == list && list->prev == list;
}

#define od_listforeach(LIST, I) \
	for (I = (LIST)->next; I != LIST; I = (I)->next)

#define od_listforeach_safe(LIST, I, N) \
	for (I = (LIST)->next; I != LIST && (N = I->next); I = N)

#endif
