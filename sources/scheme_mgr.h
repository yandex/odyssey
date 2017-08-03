#ifndef SCHEME_MGR_H
#define SCHEME_MGR_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_schememgr od_schememgr_t;

struct od_schememgr
{
	uint64_t version;
};

static inline void
od_schememgr_init(od_schememgr_t *mgr)
{
	mgr->version = 0;
}

static inline uint64_t
od_schememgr_version(od_schememgr_t *mgr)
{
	return mgr->version;
}

static inline uint64_t
od_schememgr_version_next(od_schememgr_t *mgr)
{
	return ++mgr->version;
}

#endif /* SCHEME_MGR_H */
