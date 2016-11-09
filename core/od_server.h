#ifndef OD_SERVER_H_
#define OD_SERVER_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odserver_t odserver_t;

typedef enum {
	OD_SUNDEF,
	OD_SIDLE,
	OD_SCONNECT,
	OD_SACTIVE
} odserver_state_t;

struct odserver_t {
	odserver_state_t   state;
	odscheme_server_t *scheme;
	ftio_t             io;
	odlist_t           link;
};

static inline void
od_serverinit(odserver_t *s)
{
	s->state  = OD_SUNDEF;
	s->scheme = NULL;
	s->io     = NULL;
	od_listinit(&s->link);
}

static inline odserver_t*
od_serveralloc(void)
{
	odserver_t *s = malloc(sizeof(*s));
	if (s == NULL)
		return NULL;
	od_serverinit(s);
	return s;
}

static inline void
od_serverfree(odserver_t *s)
{
	free(s);
}

#endif
