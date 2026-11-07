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
#include <config.h>
#include <alloc/linear.h>
#include <sql/minimal/ast.h>

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
	/* desc.data points to desc_data */
	od_pstmt_desc_t desc;
	od_pstmt_name_t name;

	/*
	 * cached hashes, computed once at creation:
	 * - desc_hash: xxh64 over desc bytes (xxh_pstmt_desc)
	 * - name_hash: xxh64 over the name string (strlen-based, the
	 *   same as the server map khash), so both lookup styles -
	 *   via this cache and via mm_hashmap_lock_key - agree
	 */
	mm_hash_t desc_hash;
	mm_hash_t name_hash;

	/*
	 * Query context parsed once from desc.data at pstmt creation time.
	 * Remember its grammar: clients sharing this statement can select
	 * a different parser for the reserved-name check.
	 * deallocate_name is stored inplace (no heap allocation).
	 */
	od_query_ctx_t query_ctx;
	od_config_query_parsing_mode_t query_parser_mode;

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

	/*
	 * pins keep the pstmt alive while the pointer may still be
	 * dereferenced:
	 *
	 * - one pin is held by the global pstmts map entry, released
	 *   by the value dtor when the entry is removed
	 * - one transient pin is held by od_pstmt_unref between the
	 *   refs decrement and od_global_pstmt_try_remove completion
	 *
	 * the last unpin frees the pstmt itself
	 */
	atomic_uint_fast64_t pins;

	od_global_pstmt_map_t *source;

	_Alignas(max_align_t) uint8_t desc_data[];
};

/*
 * client-side "name" -> *od_pstmt_t map (named prepared statements
 * and portals share the shape). the unnamed ("") entry is served
 * from a dedicated slot without hashing or a bucket lookup: the hot
 * relay path touches the unnamed stmt/portal on every simple Query
 * cycle (relay.c), so it must not pay for the hashmap roundtrip.
 */
typedef struct {
	/* "name" -> *od_pstmt_t (the named entries) */
	mm_hashmap_t hm;
	/* unnamed ("") entry, NULL while not registered */
	od_pstmt_t *unnamed;
} od_client_stmt_map_t;

/* "P_0" -> *od_pstmt_t */
od_client_stmt_map_t *od_client_pstmt_map_create(void);
void od_client_pstmt_map_free(od_client_stmt_map_t *map);

/*
 * canonical hash of a stmt/portal name as used by the client maps.
 *
 * name_len includes the trailing NUL - the convention of the
 * kiwi_be_read_* helpers - so callers holding a wire message name
 * never need strlen.  the value equals the strlen-based
 * xxh_str_ptr hash, so both APIs share one map.
 */
static inline mm_hash_t od_hm_name_hash(const char *name, uint32_t name_len)
{
	/* name_len includes the NUL, so it is never zero */
	od_assert(name_len >= 1);

	return mm_xxh64_hash(name, name_len - 1, 0 /* seed */);
}

int od_client_add_pstmt(od_client_t *client, const char *name,
			od_pstmt_t *pstmt);
int od_client_has_pstmt(od_client_t *client, const char *name);
int od_client_remove_pstmt(od_client_t *client, const char *name);
od_pstmt_t *od_client_get_pstmt(od_client_t *client, const char *name);

/*
 * as the plain counterparts, but with the name length (including
 * NUL) and the precomputed od_hm_name_hash supplied by the caller;
 * the map operations then never strlen or re-hash the name.
 */
od_pstmt_t *od_client_get_pstmt_hash(od_client_t *client, const char *name,
				     mm_hash_t hash);
int od_client_add_pstmt_hash(od_client_t *client, const char *name,
			     mm_hash_t hash, od_pstmt_t *pstmt);
int od_client_remove_pstmt_hash(od_client_t *client, const char *name,
				mm_hash_t hash);

void od_client_pstmts_clear(od_client_t *client);

/* "portal_name" -> *od_pstmt_t (portal tracking, extended protocol) */
od_client_stmt_map_t *od_client_portal_map_create(void);
void od_client_portal_map_free(od_client_stmt_map_t *map);

int od_client_add_portal(od_client_t *client, const char *portal_name,
			 od_pstmt_t *pstmt);
od_pstmt_t *od_client_get_portal(od_client_t *client, const char *portal_name);
int od_client_remove_portal(od_client_t *client, const char *portal_name);

/* as the plain counterparts, see od_client_get_pstmt_hash */
od_pstmt_t *od_client_get_portal_hash(od_client_t *client,
				      const char *portal_name, mm_hash_t hash);
int od_client_add_portal_hash(od_client_t *client, const char *portal_name,
			      mm_hash_t hash, od_pstmt_t *pstmt);
int od_client_remove_portal_hash(od_client_t *client, const char *portal_name,
				 mm_hash_t hash);

void od_client_portals_clear(od_client_t *client);

/*
 * server-side reserved prepared statements: a "odyssey_pstmt_<n>"
 * (inplace) -> slot map, paired with the SIEVE eviction queue over
 * the same entries. the queue lives with the map so the eviction
 * invariants (hand never points at an unlinked node, count mirrors
 * live map entries) are maintained in one place.
 */
typedef struct {
	/* generated names as inplace keys: no unnamed statements */
	mm_hashmap_t hm;
	/* SIEVE queue of the slots, oldest first */
	od_list_t fifo;
	/* rotating SIEVE hand, points at a fifo slot or the head */
	od_list_t *hand;
	/* live entries, mirrors hm size */
	size_t count;
} od_server_pstmt_map_t;

od_server_pstmt_map_t *od_server_pstmt_map_create(void);
/* NULL-safe live entry count of the server stmt map */
size_t od_server_pstmt_count(od_server_t *server);
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
				   od_pstmt_desc_t desc,
				   od_linear_alloc_t *arena,
				   const od_config_query_parsing_t *parsing);
int od_global_pstmts_has_pstmt(od_global_pstmt_map_t *gm,
			       const od_pstmt_desc_t desc);
void od_global_pstmt_try_remove(od_global_pstmt_map_t *gm, od_pstmt_t *pstmt);

typedef int (*od_global_pstmt_cb)(const od_pstmt_t *pstmt, void *arg);
void od_global_pstmt_foreach(od_global_pstmt_map_t *gm, od_global_pstmt_cb cb,
			     void *arg);

/* helpers */
/* name of the Parse msg, with its length (including the NUL)
 * for od_hm_name_hash */
char *od_pstmt_name_from_parse_len(machine_msg_t *msg, uint32_t *name_len);
od_pstmt_desc_t od_pstmt_desc_from_parse(machine_msg_t *msg);

machine_msg_t *od_pstmt_parse_of(const od_pstmt_t *pstmt);
machine_msg_t *od_pstmt_describe_of(const od_pstmt_t *pstmt);

static inline void od_pstmt_pin(od_pstmt_t *pstmt)
{
	atomic_fetch_add_explicit(&pstmt->pins, 1, memory_order_relaxed);
}

void od_pstmt_unpin(od_pstmt_t *pstmt);

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
	/*
	 * keep the pstmt (and its desc bytes) alive while this unref
	 * is in flight: another thread can take a ref, use the pstmt
	 * and remove it from the global map while we go from the refs
	 * decrement to od_global_pstmt_try_remove
	 */
	od_pstmt_pin(pstmt);

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

	od_pstmt_unpin(pstmt);
}
