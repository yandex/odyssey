#ifndef FT_LIST_H_
#define FT_LIST_H_

/*
 * fluent.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftlist ftlist;

struct ftlist {
	ftlist *next, *prev;
};

static inline void
ft_listinit(ftlist *h)
{
	h->next = h->prev = h;
}

static inline void
ft_listappend(ftlist *h, ftlist *n)
{
	n->next = h;
	n->prev = h->prev;
	n->prev->next = n;
	n->next->prev = n;
}

static inline void
ft_listunlink(ftlist *n)
{
	n->prev->next = n->next;
	n->next->prev = n->prev;
}

static inline void
ft_listpush(ftlist *h, ftlist *n)
{
	n->next = h->next;
	n->prev = h;
	n->prev->next = n;
	n->next->prev = n;
}

static inline ftlist*
ft_listpop(ftlist *h)
{
	register ftlist *pop = h->next;
	ft_listunlink(pop);
	return pop;
}

#define ft_listforeach(H, I) \
	for (I = (H)->next; I != H; I = (I)->next)

#define ft_listforeach_safe(H, I, N) \
	for (I = (H)->next; I != H && (N = I->next); I = N)

#endif
