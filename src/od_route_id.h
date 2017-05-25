#ifndef OD_ROUTE_ID_H
#define OD_ROUTE_ID_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_routeid od_routeid_t;

struct od_routeid
{
	char *user;
	int   user_len;
	char *database;
	int   database_len;
};

static inline void
od_routeid_init(od_routeid_t *id)
{
	id->user         = NULL;
	id->user_len     = 0;
	id->database     = NULL;
	id->database_len = 0;
}

static inline void
od_routeid_free(od_routeid_t *id)
{
	if (id->database)
		free(id->database);
	if (id->user)
		free(id->user);
}

static inline int
od_routeid_copy(od_routeid_t *dest, od_routeid_t *id)
{
	dest->database = malloc(id->database_len);
	if (dest->database == NULL)
		return -1;
	memcpy(dest->database, id->database, id->database_len);
	dest->database_len = id->database_len;
	dest->user = malloc(id->user_len);
	if (dest->user == NULL) {
		free(dest->database);
		dest->database = NULL;
		return -1;
	}
	memcpy(dest->user, id->user, id->user_len);
	dest->user_len = id->user_len;
	return 0;
}

static inline int
od_routeid_compare(od_routeid_t *a, od_routeid_t *b)
{
	if (a->database_len == b->database_len &&
	    a->user_len == b->user_len) {
		if (memcmp(a->database, b->database, a->database_len) == 0 &&
		    memcmp(a->user, b->user, a->user_len) == 0)
			return 1;
	}
	return 0;
}

#endif /* OD_ROUTE_ID_H */
