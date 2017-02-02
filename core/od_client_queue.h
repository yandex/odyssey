#ifndef OD_CLIENT_QUEUE_H_
#define OD_CLIENT_QUEUE_H_

/*
 * odissey.
 *
 * PostgreSQL connection queueer and request router.
*/

typedef struct od_clientqueue_t od_clientqueue_t;

struct od_clientqueue_t {
	od_list_t list;
	int       count;
};

static inline void
od_clientqueue_init(od_clientqueue_t *q)
{
	od_listinit(&q->list);
	q->count = 0;
}

static inline void void
od_clientqueue_add(od_clientqueue_t *q, od_client_t *client)
{
	od_listappend(&q->list, &client->link_queue);
	q->count++;
}

static inline od_client_t*
od_clientqueue_next(od_clientqueue_t *q)
{
	if (q->count == 0)
		return NULL;
	q->count--;
	od_list_t *first = od_listpop(&q->list);
	return od_container_of(first, od_client_t, link_queue);
}

#endif
