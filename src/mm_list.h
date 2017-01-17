#ifndef MM_LIST_H_
#define MM_LIST_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmlist mmlist;

struct mmlist {
	mmlist *next, *prev;
};

static inline void
mm_listinit(mmlist *h)
{
	h->next = h->prev = h;
}

static inline void
mm_listappend(mmlist *h, mmlist *n)
{
	n->next = h;
	n->prev = h->prev;
	n->prev->next = n;
	n->next->prev = n;
}

static inline void
mm_listunlink(mmlist *n)
{
	n->prev->next = n->next;
	n->next->prev = n->prev;
}

static inline void
mm_listpush(mmlist *h, mmlist *n)
{
	n->next = h->next;
	n->prev = h;
	n->prev->next = n;
	n->next->prev = n;
}

static inline mmlist*
mm_listpop(mmlist *h)
{
	register mmlist *pop = h->next;
	mm_listunlink(pop);
	return pop;
}

#define mm_listforeach(H, I) \
	for (I = (H)->next; I != H; I = (I)->next)

#define mm_listforeach_safe(H, I, N) \
	for (I = (H)->next; I != H && (N = I->next); I = N)

#endif
