#ifndef ODYSSEY_ROUTE_ID_H
#define ODYSSEY_ROUTE_ID_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_route_id od_route_id_t;

struct od_route_id
{
	char *user;
	int   user_len;
	char *database;
	int   database_len;
	bool  physical_rep;
};

static inline void
od_route_id_init(od_route_id_t *id)
{
	id->user         = NULL;
	id->user_len     = 0;
	id->database     = NULL;
	id->database_len = 0;
	id->physical_rep = false;
}

static inline int
od_route_id_copy(od_route_id_t *dest, od_route_id_t *id)
{
	dest->database_len = id->database_len;
	dest->database = mcxt_strdup(id->database);
	if (dest->database == NULL)
		return -1;

	dest->user_len = id->user_len;
	dest->user = mcxt_strdup(id->user);

	if (dest->user == NULL) {
		mcxt_free(dest->database);
		dest->database = NULL;
		return -1;
	}
	dest->physical_rep = id->physical_rep;
	return 0;
}

static inline int
od_route_id_compare(od_route_id_t *a, od_route_id_t *b)
{
	if (a->database_len == b->database_len &&
	    a->user_len == b->user_len) {
		if (memcmp(a->database, b->database, a->database_len) == 0 &&
		    memcmp(a->user, b->user, a->user_len) == 0)
		    if (a->physical_rep == b->physical_rep)
			    return 1;
	}
	return 0;
}

#endif /* ODYSSEY_ROUTE_ID_H */
