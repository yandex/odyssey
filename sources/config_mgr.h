#ifndef OD_CONFIG_MGR_H
#define OD_CONFIG_MGR_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_configmgr od_configmgr_t;

struct od_configmgr
{
	uint64_t version;
};

static inline void
od_configmgr_init(od_configmgr_t *mgr)
{
	mgr->version = 0;
}

static inline uint64_t
od_configmgr_version(od_configmgr_t *mgr)
{
	return mgr->version;
}

static inline uint64_t
od_configmgr_version_next(od_configmgr_t *mgr)
{
	return ++mgr->version;
}

#endif /* OD_CONFIG_MGR_H */
