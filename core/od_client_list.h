#ifndef OD_CLIENT_LIST_H_
#define OD_CLIENT_LIST_H_

/*
 * odissey.
 *
 * PostgreSQL connection lister and request router.
*/

typedef struct od_clientlist_t od_clientlist_t;

struct od_clientlist_t {
	od_list_t list;
	int       count;
};

static inline void
od_clientlist_init(od_clientlist_t *list)
{
	od_listinit(&list->list);
	list->count = 0;
}

static inline void
od_clientlist_free(od_clientlist_t *list)
{
	od_client_t *client;
	od_list_t *i, *n;
	od_listforeach_safe(&list->list, i, n) {
		client = od_container_of(i, od_client_t, link);
		/* ... */
		od_clientfree(client);
	}
}

static inline void
od_clientlist_add(od_clientlist_t *list, od_client_t *client)
{
	od_listappend(&list->list, &client->link);
	list->count++;
}

static inline void
od_clientlist_unlink(od_clientlist_t *list, od_client_t *client)
{
	assert(list->count > 0);
	od_listunlink(&client->link);
	list->count--;
	od_clientfree(client);
}

#endif
