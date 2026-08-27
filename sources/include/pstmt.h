#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * some helpers for working with prepared statements
 *
 * for pstmt support implementation there must be next hashmaps created
 *
 * for client:
 * 	client pstmt name -> *pstmt description
 *
 * for server:
 * 	server pstmt name -> *pstmt description
 *
 * global:
 * 	pstmt description -> ref counter
 *
 * ex.:
 * client sends parse "P0" with query "Q"
 * the description will be "Q" (+ param bytes)
 * client name will be "P0"
 * hash will be xxh64("Q" + param bytes)
 * server name will be "pstmt_#" where # is global counter
 */

#include <machinarium/ds/hm.h>
#include <machinarium/machinarium.h>

#include <types.h>

#define OD_PSTMT_NAME_PREFIX "odyssey_pstmt_"
#define OD_MAX_PSTMT_NUM (99999999999999999UL)
#define OD_PSTMT_NAME_MAX_LEN \
	(sizeof(OD_PSTMT_NAME_PREFIX) + 17 /* length of OD_MAX_PSTMT_NUM */)

typedef char od_pstmt_name_t[OD_PSTMT_NAME_MAX_LEN];

/* query and param bytes from Parse message */
typedef struct {
	void *data;
	size_t len;
} od_pstmt_desc_t;

typedef struct {
	mm_hashmap_t *hm;
	atomic_uint_fast64_t counter;
} od_global_pstmt_map_t;

struct od_pstmt {
	/* own the desc->data copy */
	od_pstmt_desc_t desc;
	od_pstmt_name_t name;

	/*
	 * holded by:
	 * - client pstmts, portals
	 * - server pstmts
	 * - xplan entries
	 * - global pstmts map
	 *
	 * after reach 1, the stmt must be deleted from global map
	 */
	atomic_uint_fast64_t refs;

	od_global_pstmt_map_t *source;
};

/* "P_0" -> *od_pstmt_t */
mm_hashmap_t *od_client_pstmt_hashmap_create(void);
void od_client_pstmt_hashmap_free(mm_hashmap_t *hm);

int od_client_add_pstmt(od_client_t *client, const char *name,
			od_pstmt_t *pstmt);
int od_client_has_pstmt(od_client_t *client, const char *name);
int od_client_remove_pstmt(od_client_t *client, const char *name);
od_pstmt_t *od_client_get_pstmt(od_client_t *client, const char *name);
void od_client_pstmts_clear(od_client_t *client);

/* "portal_name" -> *od_pstmt_t (portal tracking, extended protocol) */
mm_hashmap_t *od_client_portal_hashmap_create(void);
void od_client_portal_hashmap_free(mm_hashmap_t *hm);

int od_client_add_portal(od_client_t *client, const char *portal_name,
			 od_pstmt_t *pstmt);
od_pstmt_t *od_client_get_portal(od_client_t *client, const char *portal_name);
int od_client_remove_portal(od_client_t *client, const char *portal_name);
void od_client_portals_clear(od_client_t *client);

/* "odyssey_pstmt_0" -> *od_pstmt_t */
mm_hashmap_t *od_server_pstmt_hashmap_create(void);
void od_server_pstmts_free(od_server_t *server);

int od_server_has_pstmt(od_server_t *server, const od_pstmt_t *pstmt);
int od_server_add_pstmt(od_server_t *server, od_pstmt_t *pstmt);
int od_server_remove_pstmt(od_server_t *server, const od_pstmt_t *pstmt);
void od_server_pstmts_clear(od_server_t *server);

/* accessor for the value of the server pstmt hashmap (used by console) */
const od_pstmt_t *od_server_pstmt_kvp_pstmt(mm_hashmap_t *hm,
					    mm_hashmap_kvp_t *kvp);

/* SIEVE eviction of reserved statements in excess of cap; returns count or -1 */
int od_server_pstmt_evict_overflow(od_server_t *server, size_t cap,
				   machine_msg_t *stream);

/* od_pstmt_desc_t -> od_pstmt_t */
od_global_pstmt_map_t *od_global_pstmts_map_create(size_t nlocks);
void od_global_pstmts_map_free(od_global_pstmt_map_t *hm);
od_pstmt_t *od_pstmt_create_or_get(od_global_pstmt_map_t *gm,
				   od_pstmt_desc_t desc);
int od_global_pstmts_has_pstmt(od_global_pstmt_map_t *gm,
			       const od_pstmt_desc_t desc);
void od_global_pstmt_try_remove(od_global_pstmt_map_t *gm, od_pstmt_t *pstmt);

typedef int (*od_global_pstmt_cb)(const od_pstmt_t *pstmt, void *arg);
void od_global_pstmt_foreach(od_global_pstmt_map_t *gm, od_global_pstmt_cb cb,
			     void *arg);

/* helpers */
char *od_pstmt_name_from_parse(machine_msg_t *msg);
od_pstmt_desc_t od_pstmt_desc_from_parse(machine_msg_t *msg);
od_pstmt_desc_t od_pstmt_desc_copy(const od_pstmt_desc_t desc);

machine_msg_t *od_pstmt_parse_of(const od_pstmt_t *pstmt);
machine_msg_t *od_pstmt_describe_of(const od_pstmt_t *pstmt);

/*
 * should be called only with
 * - lock on global map held
 * - or from ref that is already valid
 */
static inline void od_pstmt_ref(od_pstmt_t *pstmt)
{
	atomic_fetch_add_explicit(&pstmt->refs, 1, memory_order_relaxed);
}

static inline void od_pstmt_unref(od_pstmt_t *pstmt)
{
	uint64_t v = atomic_fetch_sub_explicit(&pstmt->refs, 1,
					       memory_order_release);
	od_assert(v > 1);
	if (v == 2) {
		/*
		 * some other thread can ref this pstmt in parallel
		 * this fact will be rechecked in remove fn
		 */
		od_global_pstmt_try_remove(pstmt->source, pstmt);
	}
}
